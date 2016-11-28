# threadpool_benchmark
benchmark of some thread pool implementations in C++
the following implementations are tested:

(1) https://github.com/vit-vit/CTPL

(2) https://github.com/henkel/threadpool

(3) https://github.com/progschj/ThreadPool

(4) http://www.boost.org/doc/libs/1_62_0/doc/html/thread/synchronization.html#thread.synchronization.executors (no need to clone)

to build:

clang++ -std=c++11 -O3 -Wall -o benchmark_pool benchmark_pool.cpp -lpthread -lboost_thread -lboost_system

(also works with g++).

to run:

./benchmark_pool \<size of input\> \<number of procs\>
