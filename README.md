# threadpool_benchmark
Benchmark of some thread pool implementations in C++

The following implementations are tested:

(1) https://github.com/vit-vit/CTPL

(2) https://github.com/henkel/threadpool

(3) https://github.com/progschj/ThreadPool

(4) http://www.boost.org/doc/libs/1_62_0/doc/html/thread/synchronization.html#thread.synchronization.executors (no need to clone, but exists only from boost 1.56)

To build:

```clang++ -std=c++11 -O3 -Wall -o benchmark_pool benchmark_pool.cpp -lpthread -lboost_thread -lboost_system```

(also works with g++).

To run:

```./benchmark_pool <size of input> <number of procs>```
