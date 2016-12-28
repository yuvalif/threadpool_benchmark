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
    std::size_t m_size;
    bool m_done;
    std::queue<Arg> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_empty_cond;
    std::condition_variable m_full_cond;
    std::vector<std::thread> m_workers;

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
    thread_pool(std::size_t size, const std::function<void(const Arg&)>& F) :
        m_size(size),
        m_done(false)
    {
        for (auto i = 0; i < m_size; ++i)
        {
            // start all worker threads
            m_workers.push_back(std::thread([this, F]()
            {
                while (true)
                {
                    Arg arg;
                    {
                        std::unique_lock<std::mutex> mlock(m_mutex);

                        if (m_queue.empty())
                        {
                            // queue is empty
                            if (m_done)
                            {
                                // queue is empty and we are done
                                // notify any blocked submitter and exit
                                mlock.unlock();
                                m_full_cond.notify_one();
                                return;
                            }
                            else
                            {
                                // queue is empty but we are done yet
                                // wait to get notified, either on queue not empty of being done
                                m_empty_cond.wait(mlock, []{return true;});
                                // if there was a spurios wake-up, we need to check the conditions again
                                continue;
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
            }));
        }
    }

    //! submit new argument to be processed by the threads
    //! blocking call
    void submit(const Arg& arg)
    {
        {
            std::unique_lock<std::mutex> mlock(m_mutex);
            if (m_done) return;
            while (m_queue.size() == QSIZE) m_full_cond.wait(mlock);
            m_queue.push(arg);
        }
        m_empty_cond.notify_one();
    }

    //! submit new argument to be processed by the threads if queue has space
    //! non-blocking call
    bool try_submit(const Arg& arg)
    {
        {
            std::unique_lock<std::mutex> mlock(m_mutex);
            if (m_done) return false;
            if (m_queue.size() == QSIZE) return false;
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
            std::unique_lock<std::mutex> mlock(m_mutex);
            // dont allow new submitions
            m_done = true;
            if (!wait)
            {
                // drain the queue without running F on it
                while (!m_queue.empty()) m_queue.pop();
            }
        }

        // notify all that we are done
        m_empty_cond.notify_all();

        for (auto i = 0; i < m_size; ++i)
        {
            // join on threads until they actually finish
            // in case we are asked not to wait, we still join
            // but since queue is empty the threads should exit immediatly
            // TODO: check if we can detach in the wait=false case
            m_workers[i].join();
        }
    }
};

