#ifndef AFFINITY_THREAD_POOL_LOCKFREE_H
#define AFFINITY_THREAD_POOL_LOCKFREE_H

#include <thread>
#include <chrono>
#include <vector>
#include <boost/lockfree/queue.hpp>

//! thread pool with multiple queues of inputs to a function
//! using a lockfree queue
template<typename Arg, std::size_t QSIZE=512>
class affinity_thread_pool_lockfree
{
private:
    //! number of threads
	const std::size_t m_number_of_workers;
    //! time to wait when idle (useconds)
    const unsigned long m_wait_time;
    //! indication that the pool should not receive new Args
    volatile bool m_done;
    //! queues of work items, one for each worker
    typedef boost::lockfree::queue<Arg, boost::lockfree::fixed_sized<true>, boost::lockfree::capacity<QSIZE> > queue_t;
    std::vector<queue_t> m_queues;
    //! the actual worker threads
    std::vector<std::thread> m_workers;
    //! round robin index of the worker thread to use in case of no 
    //! affinity work submitted (blocking) when all queues are full
    std::atomic<std::size_t> m_rr_worker_id;

    //! this is the function executed by the worker threads
    //! it pull items ot of the queue until signaled to stop
    void worker(std::function<void(const Arg&)> F, std::size_t worker_id, unsigned long wait_time)
    {
        assert(worker_id < m_number_of_workers);
        queue_t& q = m_queues[worker_id];
        while (true) 
        {
            Arg arg;
            while (!q.pop(arg)) 
            {
                // if queue is empty and work is done - the thread exits
                if (m_done) 
                {
                    return;
                }
                // wait/busy wait until there is something in queue
                else if (m_wait_time != BusyWait)
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(m_wait_time));
                }
            }
            F(arg);
        }
    }
public:

    //! indicating that work may be executed on any thread
    static const int NoAffinity = -1;
    //! indicating that the workers should busy wait if queue is empty
    static const unsigned long BusyWait = 0;

    //! don't allow copying
    affinity_thread_pool_lockfree& operator=(const affinity_thread_pool_lockfree&) = delete;
    affinity_thread_pool_lockfree(const affinity_thread_pool_lockfree&) = delete;

    //! destructor clean the threads but dont wait for them to finish
    ~affinity_thread_pool_lockfree()
    {
        // stop all threads without finishing the work
        if (!m_done)
        {
            stop(false);
        }
    }

    //! constructor spawn the threads
	affinity_thread_pool_lockfree(std::size_t number_of_workers, std::function<void(const Arg&)> F, unsigned long wait_time) : 
        m_number_of_workers(number_of_workers),
        m_wait_time(wait_time),
        m_done(false),
        m_queues(number_of_workers)
	{
        for (auto i = 0; i < m_number_of_workers; ++i)
        {
            // start all worker threads
            m_workers.push_back(std::thread(&affinity_thread_pool_lockfree::worker, this, F, i, wait_time));
        }
	}

    //! submit new argument to be processed by the threads
    //! blocking call
	void submit(const Arg& arg, int worker_id = NoAffinity)
	{
        assert(worker_id < static_cast<int>(m_number_of_workers) && worker_id >= NoAffinity);

        if (m_done) 
        {
            return;
        }
        else if (worker_id == NoAffinity)
        {
            // no affinity, find a free queue
            bool pushed = false;
            // increment the round robin index every time we use it
            m_rr_worker_id = (m_rr_worker_id+1)%m_number_of_workers;
            // cache the atomic variable before the busy wait loop
            const std::size_t rr_worker_id = m_rr_worker_id;
            while (!m_done && !pushed)
            {
                for (auto i = 0; i < m_number_of_workers; ++i)
                {
                    // dont always start from firts q
                    // start fron an arbitrary queue in round robin manner
                    const std::size_t q_idx = (rr_worker_id+i)%m_number_of_workers;
                    if (m_queues[q_idx].bounded_push(arg))
                    {
                        pushed = true;
                        break;
                    }
                }

                // wait/busy wait until queue has space
                if (!pushed && m_wait_time != BusyWait)
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(m_wait_time));
                }
            }
        }
        else
        {
            // has affinity, try using a specific worker
            while (!m_queues[worker_id].bounded_push(arg))
            {
                // queue is full, wait/busy wait until queue has space
                if (m_wait_time != BusyWait)
                {
                    std::this_thread::sleep_for(std::chrono::microseconds(m_wait_time));
                }
            }
        }
	}

    //! submit new argument to be processed by the threads if queue has space
    //! non-blocking call
	bool try_submit(const Arg& arg, int worker_id = NoAffinity)
	{
        assert(worker_id < static_cast<int>(m_number_of_workers) && worker_id >= NoAffinity);
        if (m_done) 
        {
            return false;
        }
        else if (worker_id == NoAffinity)
        {
            // no affinity, find a free queue
            bool pushed = false;
            // increment the round robin index every time we use it
            m_rr_worker_id = (m_rr_worker_id+1)%m_number_of_workers;

            for (auto i = 0; i < m_number_of_workers; ++i)
            {
                // dont always start from firts q
                // start fron an arbitrary queue in round robin manner
                const std::size_t q_idx = (m_rr_worker_id+i)%m_number_of_workers;
                if (m_queues[q_idx].bounded_push(arg))
                {
                    pushed = true;
                    break;
                }
            }

            return pushed;
        }
        else
        {
            // has affinity, try using a specific worker
            return m_queues[worker_id].bounded_push(arg);
        }
    }

    //! stop all threads, may or may not wait to finish
    void stop(bool wait=false)
    {
        if (m_done)
        {
            return;
        }

        // dont allow new submitions
        m_done = true;
        
        if (!wait)
        {
            // drain the queues
            Arg arg;
            for (auto i = 0; i < m_number_of_workers; ++i)
            {
                while (!m_queues[i].empty()) 
                {
                    m_queues[i].pop(arg);
                }
            }
        }

        for (auto i = 0; i < m_number_of_workers; ++i)
        {
            // join on threads until they actually finish
            m_workers[i].join();
        }
    }
};

#endif

