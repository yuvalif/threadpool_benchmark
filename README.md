# threadpool_benchmark
Benchmark of some thread pool implementations in C++

The following implementations are tested:
(1) single threaded baseline

(2) https://github.com/vit-vit/CTPL (please clone one directory above)

(3) https://github.com/henkel/threadpool (please clone one directory above). Also from here: http://threadpool.sourceforge.net/index.html

(4) https://github.com/progschj/ThreadPool (please clone one directory above)

(5) http://www.boost.org/doc/libs/1_62_0/doc/html/thread/synchronization.html#thread.synchronization.executors (no need to clone, but exists only from boost 1.56)

(6) thread_pool.hpp using boost thread_group and asio. Based on: http://stackoverflow.com/questions/12215395/thread-pool-using-boost-asio code from Tanner Sansbury (no need to clone copied into this project)

(7) Thread pool based on one producer multiple comsumers using a "mailbox" queue synchronized with condition variables. Code for concurrent queue is based on Juan Palacios code from here: https://github.com/juanchopanza/cppblog (no need to clone, modified version added to this project)

(8) Thread pool based on one producer multiple comsumers using a lockfree "mailbox" queue. Using boost lockfree queue: http://www.boost.org/doc/libs/1_62_0/doc/html/boost/lockfree/queue.html (no need to clone, part of boost)

To build:

```clang++ -std=c++11 -O3 -Wall -o benchmark_pool benchmark_pool.cpp -lpthread -lboost_thread -lboost_system```

(also works with g++).

To run:

```./benchmark_pool <size of input> <number of procs>```
