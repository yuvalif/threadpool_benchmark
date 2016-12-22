#include <thread>
#include <chrono>
#include <boost/lockfree/queue.hpp>

//! thread pool with queue of inputs to a function
//! using a lockfree queue
template<typename Arg, void (*F)(const Arg&), std::size_t QSIZE=512>
class thread_pool_lockfree
{
private:
    //! number of threads
	const std::size_t m_size;
    //! time to wait when idle (useconds)
    const unsigned long m_wait_time;
    //! indication that the pool should not receive new Args
    std::atomic<bool> m_done;
    //! queue holding the Args
    boost::lockfree::queue<Arg, boost::lockfree::fixed_sized<true>, boost::lockfree::capacity<QSIZE> > m_queue;
    //! array of worker threads
    std::vector<std::thread> m_workers;

public:
    static const unsigned long NoWait = 0;

    //! destructor clean the threads but dont wait for them to finish
    ~thread_pool_lockfree()
    {
        // just detach the threads
        if (!m_done) stop(false);
    }

    //! constructor spawn the threads
	thread_pool_lockfree(std::size_t size, unsigned long wait_time) : 
        m_size(size),
        m_wait_time(wait_time),
        m_done(false)
	{
        for (auto i = 0; i < m_size; ++i)
        {
            // start all worker threads
            m_workers.push_back(std::thread([this]() 
            {
                while (true) 
                {
                    Arg arg;
                    while (!m_queue.pop(arg)) 
                    {
                        // if queue is empty and work is done - the thread exits
                        if (m_done) 
                        {
                            return;
                        }
                        // wait/busy wait until there is something in queue
                        else if (m_wait_time)
                        {
                             std::this_thread::sleep_for(std::chrono::microseconds(m_wait_time));
                        }
                    }
                    F(arg);
                }
            }));
        }
	}

    //! submit new argument to be processed by the threads
    //! blocking call
	void submit(const Arg& arg)
	{
        if (m_done) return;
        while(!m_queue.bounded_push(arg))
        {
            // wait/busy wait until queue has space
            if (m_wait_time)
            {
                std::this_thread::sleep_for(std::chrono::microseconds(m_wait_time));
            }
        }
	}

    //! submit new argument to be processed by the threads if queue has space
    //! non-blocking call
	bool try_submit(const Arg& arg)
	{
        if (m_done) return false;
        return m_queue.bounded_push(arg);
    }

    //! stop all threads, may or may not wait to finish
    void stop(bool wait=false)
    {
        // dont allow new submitions
        m_done = true;
        
        if (!wait)
        {
            // drain the queue
            Arg arg;
            while (!m_queue.empty()) m_queue.pop(arg);
        }

        for (auto i = 0; i < m_size; ++i)
        {
            // join on threads until they actually finish
            m_workers[i].join();
        }
    }
};
 
