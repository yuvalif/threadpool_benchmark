// based on code taken from here: https://github.com/juanchopanza/cppblog
//
// Copyright (c) 2013 Juan Palacios juan.palacios.puyana@gmail.com
// Subject to the BSD 2-Clause License
// - see < http://opensource.org/licenses/BSD-2-Clause>
//

#ifndef CONCURRENT_QUEUE_
#define CONCURRENT_QUEUE_

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

template <typename T, std::size_t N>
class concurrent_queue
{
    public:

        bool empty()
        {
            std::unique_lock<std::mutex> mlock(mutex_);
            return (size_==0);
        }

        bool try_pop(T& item)
        {
            std::unique_lock<std::mutex> mlock(mutex_);
            while (size_==0)
            {
                return false;
            }
            item = queue_.front();
            queue_.pop();
            --size_;
            mlock.unlock();
            full_cond_.notify_one();
            return true;
        }

        void pop(T& item)
        {
            std::unique_lock<std::mutex> mlock(mutex_);
            while (size_==0)
            {
                empty_cond_.wait(mlock);
            }
            item = queue_.front();
            queue_.pop();
            --size_;
            mlock.unlock();
            full_cond_.notify_one();
        }

        void push(const T& item)
        {
            std::unique_lock<std::mutex> mlock(mutex_);
            while (size_==N)
            {
                full_cond_.wait(mlock);
            }
            queue_.push(item);
            ++size_;
            mlock.unlock();
            empty_cond_.notify_one();
        }

        void unblock()
        {
            std::unique_lock<std::mutex> mlock(mutex_);
            size_ = N+1;
            full_cond_.notify_all();
            empty_cond_.notify_all();
        }

        concurrent_queue() : size_(0) { static_assert(N < SIZE_MAX, "max queue size must be smaller than max size_t"); }
        concurrent_queue(const concurrent_queue&) = delete;            // disable copying
        concurrent_queue& operator=(const concurrent_queue&) = delete; // disable assignment

    private:
        std::size_t size_;
        std::queue<T> queue_;
        std::mutex mutex_;
        std::condition_variable empty_cond_;
        std::condition_variable full_cond_;
};

#endif

