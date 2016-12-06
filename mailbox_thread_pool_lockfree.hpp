#include <thread>
#include <boost/lockfree/queue.hpp>

template<typename Arg, void (*F)(const Arg&)>
class mailbox_thread_pool_lockfree
{
private:
	std::size_t m_size;
    bool m_done;
    boost::lockfree::queue<Arg, boost::lockfree::fixed_sized<true>, boost::lockfree::capacity<512>> m_mailbox;
    std::vector<std::thread> m_workers;

public:

    // destructor clean the threads
    ~mailbox_thread_pool_lockfree()
    {
        // just detach the threads
        if (!m_done) stop(false);
    }

    // constructor spawn the threads
	mailbox_thread_pool_lockfree(std::size_t size) : 
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
                    while (!m_mailbox.pop(arg) && !m_done) {} // spin lock
                    F(arg);
                }
            }));
        }
	}

    // submit new argument to be processed by the threads
	void submit(const Arg& arg)
	{
        if (m_done) return;
        if(!m_mailbox.bounded_push(arg))
        {
            // just do it on the main thread
            F(arg);
        }
	}

    // stop all threads, may or may not wait to finish
    void stop(bool wait=false)
    {
        m_done = true;
        
        for (auto i = 0; i < m_size; ++i)
        {
            if (wait)
            {
                Arg arg;
                // finish the remaining work
                while (m_mailbox.pop(arg))
                    F(arg);
                // join on threads until they actually finish
                m_workers[i].join();
            }
            else
            {
                // lets threads continue on their own
                m_workers[i].detach();
            }
        }
    }
};
 
