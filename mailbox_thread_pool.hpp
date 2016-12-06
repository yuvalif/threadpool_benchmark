#include "concurrent_queue.hpp"
#include <vector>
#include <thread>

template<typename Arg, void (*F)(const Arg&)>
class mailbox_thread_pool
{
private:
	std::size_t m_size;
    bool m_done;
    concurrent_queue<Arg, 512> m_mailbox;
    std::vector<std::thread> m_workers;

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
        m_done(false)
	{
        for (auto i = 0; i < m_size; ++i)
        {
            // start all worker threads
            m_workers.push_back(std::thread([&]() 
            {
                while (!m_done) 
                {
                    Arg arg;
                    m_mailbox.pop(arg);
                    F(arg);
                }
            }));
        }
	}

    // submit new argument to be processed by the threads
	void submit(const Arg& arg)
	{
        if (m_done) return;
        m_mailbox.push(arg);
	}

    // stop all threads, may or may not wait to finish
    void stop(bool wait=false)
    {
        m_done = true;
        
        if (wait)
        {
            Arg arg;
            while (m_mailbox.try_pop(arg))
            {
                F(arg);
            }
        }
        m_mailbox.unblock();
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
 
