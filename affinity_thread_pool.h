#include <queue>
#include <condition_variable>
#include <vector>
#include <thread>
#include <functional>
#include <cassert>

//! thread pool with multiple queue of inputs to a function
//! using a queue synchronized with condition variables
//! each queue belongs to a specific worker thread
template<typename Arg, std::size_t QSIZE=512>
class affinity_thread_pool
{
private:
    //! number of worker threads
    const std::size_t m_number_of_workers;
    //! internal state indicating that processing of existing work should finish
    //! and no new work should be submitted
    volatile bool m_done;
    //! queues of work items, one for each worker
    std::vector<std::queue<Arg> > m_queues;
    //! mutex used by the empty/full conditions and related variables
    std::mutex m_mutex;
    //! condition used to signal if a queue is empty or not empty
    std::vector<std::condition_variable> m_empty_conds;
    //! condition used to signal if a queue is full or not full
    std::vector<std::condition_variable> m_full_conds;
    //! the actual worker threads
    std::vector<std::thread> m_workers;
    //! round robin index of the worker thread to use in case of no 
    //! affinity work submitted (blocking) when all queues are full
    std::size_t m_rr_worker_id;

    //! this is the function executed by the worker threads
    //! it pull items ot of the queue until signaled to stop
    void worker(std::function<void(const Arg&)> F, std::size_t worker_id)
    {
        assert(worker_id < m_number_of_workers);
        std::queue<Arg>& q = m_queues[worker_id];
        std::condition_variable& empty_cond = m_empty_conds[worker_id];
        std::condition_variable& full_cond = m_full_conds[worker_id];

        while (true) 
        {
            Arg arg;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                // if queue is not empty we continue regardless or "done" indication
                if (q.empty())
                {
                    // queue is empty
                    if (m_done)
                    {
                        // queue is empty and we are done
                        return;
                    }
                    else
                    {
                        // queue is empty but we are not done yet
                        // wait to get notified, either on queue not empty of being done
                        while (!m_done && q.empty())
                        {
                            empty_cond.wait(lock);
                        }
                        if (q.empty())
                        {
                            // done and empty
                            return;
                        }
                    }
               }

               // there is work to do
               arg = q.front();
               q.pop();
            }

            // notify that queue is not full
            full_cond.notify_one();
            // execute the work when the mutex is not locked
            F(arg);
        }
    }

public:

    //! indicating that any worker thread may execute the work
    static const int NoAffinity = -1;

    //! don't allow copying
    affinity_thread_pool& operator=(const affinity_thread_pool&) = delete;
    affinity_thread_pool(const affinity_thread_pool&) = delete;

    //! destructor clean the threads
    ~affinity_thread_pool()
    {
        // TODO: may be an issue if "stop" is called from a different thread than the destructor
        // dont wait for the threads
        if (!m_done) stop(false);
    }

    //! constructor spawn the threads
    affinity_thread_pool(std::size_t number_of_workers, std::function<void(const Arg&)> F) : 
        m_number_of_workers(number_of_workers),
        m_done(false),
        m_queues(number_of_workers),
        m_empty_conds(number_of_workers),
        m_full_conds(number_of_workers),
        m_rr_worker_id(0)
    {
        for (auto i = 0; i < m_number_of_workers; ++i)
        {
            // start all worker threads
            m_workers.push_back(std::thread(&affinity_thread_pool::worker, this, F, i));
        }
    }

    //! submit new argument to be processed by the threads
    //! blocking call
    void submit(const Arg& arg, int worker_id = NoAffinity)
    {
        std::size_t actual_q = static_cast<std::size_t>(worker_id);
        assert(worker_id < static_cast<int>(m_number_of_workers) && worker_id >= NoAffinity);
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_done)
            {
                return;
            }
            else if (worker_id == NoAffinity)
            {
                // no affinity, find a free queue
                bool pushed = false;
                for (auto i = 0; i < m_number_of_workers; ++i)
                {
                    // dont start from firts q
                    // start fron an arbitrary queue in round robin manner
                    const std::size_t q_idx = (m_rr_worker_id+i)%m_number_of_workers;
                    if (m_queues[q_idx].size() < QSIZE)
                    {
                        m_queues[q_idx].push(arg);
                        pushed = true;
                        actual_q = q_idx;

                        break;
                    }
                }
                if (!pushed)
                {
                    // all queues were busy
                    // wait on arbitrary queue in round robin manner
                    while (!m_done && m_queues[m_rr_worker_id].size() == QSIZE) 
                    {
                        m_full_conds[m_rr_worker_id].wait(lock);
                    }
                    if (m_done)
                    {
                        // marked as done while we were waiting
                        return;
                    }
                    m_queues[m_rr_worker_id].push(arg);
                    actual_q = m_rr_worker_id;
                }
                // increment the round robin index
                m_rr_worker_id = (m_rr_worker_id+1)%m_number_of_workers;
            }
            else
            {
                // has affinity, try using a specific worker
                while (!m_done && m_queues[worker_id].size() == QSIZE) 
                {
                    m_full_conds[worker_id].wait(lock);
                }
                if (m_done)
                {
                    // marked as done while we were waiting
                    return;
                }
                m_queues[worker_id].push(arg);
            }
        }
        assert(actual_q < m_number_of_workers);
        m_empty_conds[actual_q].notify_one();
    }

    //! submit new argument to be processed by the threads if queue has space
    //! non-blocking call
    bool try_submit(const Arg& arg, int worker_id = NoAffinity)
    {
        std::size_t actual_q = static_cast<std::size_t>(worker_id);
        assert(worker_id < static_cast<int>(m_number_of_workers) && worker_id >= NoAffinity);
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_done)
            {
                return false;
            }
            else if (worker_id == NoAffinity)
            {
                // no affinity, find a free queue
                bool pushed = false;
                for (auto i = 0; i < m_number_of_workers; ++i)
                {
                    if (m_queues[i].size() < QSIZE)
                    {
                        m_queues[i].push(arg);
                        pushed = true;
                        break;
                    }
                }
                if (!pushed)
                {
                    // all queues were busy
                    return false;
                }
            }
            else
            {
                // has affinity, try using a specific worker
                if (m_queues[worker_id].size() == QSIZE)
                {
                    return false;
                }
                else
                {
                    m_queues[worker_id].push(arg);
                }
            }
        }
        assert(actual_q < m_number_of_workers);
        m_empty_conds[actual_q].notify_one();
        return true;
    }

    //! stop all threads, may or may not wait to finish
    void stop(bool wait)
    {
        {
            // take lock to make sure worker threads are either waiting
            // or processing, and will check on m_done before next iteration
            std::unique_lock<std::mutex> lock(m_mutex);
            // dont allow new submitions
            m_done = true;
            if (!wait)
            {
                // drain the queues without running F on it
                for (auto i = 0; i < m_number_of_workers; ++i)
                {
                    while (!m_queues[i].empty()) 
                    {
                        m_queues[i].pop();
                    }
                }
            }
        }

        // notify all that we are done
        for (auto i = 0; i < m_number_of_workers; ++i)
        {
            m_empty_conds[i].notify_all();
            m_full_conds[i].notify_all();
        }

        for (auto i = 0; i < m_number_of_workers; ++i)
        {
            m_workers[i].join();
        }
    }
};
 
