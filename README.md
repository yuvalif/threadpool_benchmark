# threadpool_benchmark
Benchmark of some thread pool implementations in C++

The following implementations are tested:
* Single threaded baseline
* https://github.com/vit-vit/CTPL
* https://github.com/henkel/threadpool (also from here: http://threadpool.sourceforge.net/index.html)
* https://github.com/progschj/ThreadPool
* http://www.boost.org/doc/libs/1_62_0/doc/html/thread/synchronization.html#thread.synchronization.executors (no need to clone, but exists only from boost 1.56 and up)
* `thread_pool.hpp` using `boos::thread_group` and asio. Based on: http://stackoverflow.com/questions/12215395/thread-pool-using-boost-asio code from Tanner Sansbury (no need to clone as code is copied into this project)

To build: `clang++ -std=c++11 -O3 -Wall -o benchmark_pool benchmark_pool.cpp -lpthread -lboost_thread -lboost_system`
(also works with g++).

To run: `./benchmark_pool <size of input> <number of procs>`

To fetch all implementations needed for the benchmark, call `./fetch_dependencies.sh`. Tested implementations will be cloned one directory above.

To run multiple times for different different number of threads (up to max procs) use: `./run_benchmark.sh <interation> <max processors>`
