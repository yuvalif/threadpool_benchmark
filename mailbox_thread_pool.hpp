#include <queue>
#include <condition_variable>
#include <vector>
#include <thread>
#include <functional>

//! thread pool with queue of inputs to a function
//! using a queue synchronized with condition variables
template<typename Arg, std::size_t QSIZE=512>
class thread_pool
{
private:
    //! number of worker threads
    const std::size_t m_number_of_workers;
    //! internal state indicating that processing of existing work should finish
    //! and no new work should be submitted
    volatile bool m_done;
    //! queue of work items
    std::queue<Arg> m_queue;
    //! mutex used by the empty/full conditions and related variables
    std::mutex m_mutex;
    //! condition used to signal that queue is empty or not empty
    std::condition_variable m_empty_cond;
    //! condition used to signal that queue is full or not full
    std::condition_variable m_full_cond;
    //! the actual worker threads
    std::vector<std::thread> m_workers;

    //! this is the function executed by the worker threads
    //! it pull items ot of the queue until signaled to stop
    void worker(std::function<void(const Arg&)> F)
    {
        while (true) 
        {
            Arg arg;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                // if queue is not empty we continue regardless or "done" indication
                if (m_queue.empty())
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
                        while (!m_done && m_queue.empty())
                        {
                            m_empty_cond.wait(lock);
                        }
                        if (m_queue.empty())
                        {
                            // done and empty
                            return;
                        }
                    }
               }

               // there is work to do
               arg = m_queue.front();
               m_queue.pop();
            }

            // notify that queue is not full
            m_full_cond.notify_one();
            // execute the work when the mutex is not locked
            F(arg);
        }
    }

public:

    //! don't allow copying
    thread_pool& operator=(const thread_pool&) = delete;
    thread_pool(const thread_pool&) = delete;

    //! destructor clean the threads
    ~thread_pool()
    {
        // TODO: may be an issue if "stop" is called from a different thread than the destructor
        // dont wait for the threads
        if (!m_done) stop(false);
    }

    //! constructor spawn the threads
    thread_pool(std::size_t number_of_workers, std::function<void(const Arg&)> F) : 
        m_number_of_workers(number_of_workers),
        m_done(false)
    {
        for (auto i = 0; i < m_number_of_workers; ++i)
        {
            // start all worker threads
            m_workers.push_back(std::thread(&thread_pool::worker, this, F));
        }
    }

    //! submit new argument to be processed by the threads
    //! blocking call
    void submit(const Arg& arg)
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            while (!m_done && m_queue.size() == QSIZE) 
            {
                m_full_cond.wait(lock);
            }
            if (m_done)
            {
                return;
            }
            m_queue.push(arg);
        }
        m_empty_cond.notify_one();
    }

    //! submit new argument to be processed by the threads if queue has space
    //! non-blocking call
    bool try_submit(const Arg& arg)
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_done || m_queue.size() == QSIZE) 
            {
                return false;
            }
            m_queue.push(arg);
        }
        m_empty_cond.notify_one();
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
                // drain the queue without running F on it
                while (!m_queue.empty()) 
                {
                    m_queue.pop();
                }
            }
        }

        // notify all that we are done
        m_empty_cond.notify_all();
        m_full_cond.notify_all();

        for (auto i = 0; i < m_number_of_workers; ++i)
        {
            m_workers[i].join();
        }
    }
};
 
