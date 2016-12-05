#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <boost/optional.hpp>

template<typename Arg, void (*F)(const Arg&)>
class mailbox_thread_pool
{
private:
	std::size_t m_size;
    bool m_done;
    boost::optional<Arg> m_mailbox;
    std::vector<std::thread> m_workers;
    std::mutex m_mutex;
    std::condition_variable m_worker_condition;
    std::condition_variable m_submitter_condition;

public:

    // destructor clean the threads
    ~mailbox_thread_pool()
    {
        // just detach the threads
        if (!m_done) stop(false);
    }

    // constructor spawn the threads
	mailbox_thread_pool(std::size_t size) : 
        m_size(size),
        m_done(false),
        m_mailbox(boost::none)
	{
        for (auto i = 0; i < m_size; ++i)
        {
            // start all worker threads
            m_workers.push_back(std::thread([&]() 
            {
                while (!m_done) 
                {
                    Arg arg;
                    {
                        std::unique_lock<std::mutex> lock(m_mutex);
                        while (m_mailbox == boost::none) m_worker_condition.wait(lock);
                        if (m_done) break;
                        arg = *m_mailbox;
                        m_mailbox = boost::none;
                    }
                    // notify mailbox is empty, submitter can send the next item
                    m_submitter_condition.notify_one();
                    // make sure that the heavy lifting is done when the mutex is not locked
                    F(arg);
                }
            }));
        }
	}

    // submit new argument to be processed by the threads
	void submit(const Arg& arg)
	{
	    {
            std::unique_lock<std::mutex> lock(m_mutex);
            while (m_mailbox != boost::none) m_submitter_condition.wait(lock);
            m_mailbox = arg;
        }
        // notify that mailbox is full, one of the threads should wake up
        m_worker_condition.notify_one();
	}

    // stop all threads, may or may not wait to finish
    void stop(bool wait=false)
    {
        m_done = true;
        // release all waiting threads
        for (auto i = 0; i < m_size; ++i)
        {
            m_mailbox = Arg();
            m_worker_condition.notify_one();
        }
        
        for (auto i = 0; i < m_size; ++i)
        {
            if (wait)
                // join on them until they actually finish
                m_workers[i].join();
            else
                // lets them continue on their own
                m_workers[i].detach();
        }
    }
};
 
