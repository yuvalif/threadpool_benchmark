#include <queue>
#include <condition_variable>
#include <vector>
#include <thread>

//! thread pool with queue of inputs to a function
//! using a queue synchronized with condition variables
template<typename Arg, void (*F)(const Arg&), std::size_t QSIZE=512>
class thread_pool
{
private:
	std::size_t m_size;
    bool m_done;
    std::queue<Arg> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_empty_cond;
    std::condition_variable m_full_cond;
    std::vector<std::thread> m_workers;

public:
    //! destructor clean the threads
    thread_pool()
    {
        // just detach the threads
        if (!m_done) stop(false);
    }

    //! constructor spawn the threads
	thread_pool(std::size_t size) : 
        m_size(size),
        m_done(false)
	{
        for (auto i = 0; i < m_size; ++i)
        {
            // start all worker threads
            m_workers.push_back(std::thread([&]() 
            {
                while (true) 
                {
                    Arg arg;
                    {
                        std::unique_lock<std::mutex> mlock(m_mutex);
                        while (m_queue.empty())
                        {
                            if (m_done)
                            {
                                // queue is empty and we are done
                                return;
                            }
                            else
                            {
                                // queue is empty and we are not done - wait on condition
                                m_empty_cond.wait(mlock);
                            }
                        }
                        arg = m_queue.front();
                        m_queue.pop();
                    }
                    // notify that queue is not full
                    m_full_cond.notify_one();
                    // execute the work when the queue is not locked
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
        {
            std::unique_lock<std::mutex> mlock(m_mutex);
            while (m_queue.size() == QSIZE)
            {
                m_full_cond.wait(mlock);
            }
            m_queue.push(arg);
        }
        m_empty_cond.notify_one();
	}

    //! submit new argument to be processed by the threads if queue has space
    //! non-blocking call
	bool try_submit(const Arg& arg)
	{
        if (m_done) return false;
        {
            std::unique_lock<std::mutex> mlock(m_mutex);
            if (m_queue.size() == QSIZE) return false;
            m_queue.push(arg);
        }
        m_empty_cond.notify_one();
        return true;
	}

    //! stop all threads, may or may not wait to finish
    void stop(bool wait=false)
    {
        // dont allow new submitions
        m_done = true;

        // notify all that we are done       
        m_empty_cond.notify_all();
        
        if (!wait)
        {
            // drain the queue
            std::unique_lock<std::mutex> mlock(m_mutex);
            while (!m_queue.empty()) m_queue.pop();
        }
        
        for (auto i = 0; i < m_size; ++i)
        {
            // join on threads until they actually finish
            m_workers[i].join();
        }
    }
};
 
