#include <cmath>    // for std::sqrt
#include <cstdlib>  // for std::rand
#include <thread>   // for std::thread
#include <atomic>   // for std::atomic
#include <string>
#include <algorithm>
#include <vector>	// for std:vector
#include <iostream>
#include <chrono>   // for time calculations
// thread libraries to be tested
#include "CTPL/ctpl.h" // for testing CPTL thread pool
#ifndef TIME_UTC
#define TIME_UTC TIME_UTC_
#endif
#include "threadpool/boost/threadpool.hpp" // for testing old boost thread pool (by Philippe Henkel)
#include "ThreadPool/ThreadPool.h" // for testing thread pool using Jakob Progsch's thread pool
#if BOOST_VERSION > 105600
#include <boost/thread/executors/basic_thread_pool.hpp> // for testing new boost thread pool. available from ver 1.56 and up
#endif
#include "thread_pool.hpp" // for asio based pool

// the work being done is calculating if a number is prime or no and accumulating the result
bool IsPrime(unsigned long n)
{
    // special handle for 0,1,2
    if (n < 3)
    {
	if (n == 2) return true;
    }

    // no need to check above sqrt(n)
    const auto N = std::ceil(std::sqrt(n) + 1);

    for (auto i = 2; i < N; ++i)
    {
        if (n%i == 0)
        {
			return false;
        }
    }

	return true;
}

// check if a number is prime and accumulate into a counter
void CountIfPrime(unsigned long n, unsigned long& count)
{
	if (IsPrime(n)) ++count;
}

// check if a number is prime and accumulate into a counter - thred safe with thread id
void CountIfPrimeCTPL(int id, unsigned long n, std::atomic<unsigned long>& count)
{
	if (IsPrime(n)) ++count;
}

// check if a number is prime and accumulate into a counter - thred safe
void CountIfPrimeBoost(unsigned long n, std::atomic<unsigned long>& count)
{
	if (IsPrime(n)) ++count;
}

unsigned long count_primes_asio(const std::vector<unsigned long>& random_inputs, unsigned int NUMBER_OF_PROCS)
{
    std::atomic<unsigned long> number_of_primes(0);
	{
		asio_thread_pool pool(NUMBER_OF_PROCS);

		// loop over input to accumulate how many primes are there
    	std::for_each(random_inputs.begin(), random_inputs.end(), 
            [&](unsigned long n) 
    	{
    		while (!pool.run_task(boost::bind(CountIfPrimeBoost, n, std::ref(number_of_primes)))) {}
    	});

        // as pool go out of scope, it will wait for all threads to finish
	}

    return number_of_primes;
}

#if BOOST_VERSION > 105600
// using boost's experimental thread pool
unsigned long count_primes_boost(const std::vector<unsigned long>& random_inputs, unsigned int NUMBER_OF_PROCS)
{
    // here the main thread also does work
	boost::basic_thread_pool tp(NUMBER_OF_PROCS-1);
    std::atomic<unsigned long> number_of_primes(0);

	// loop over input to accumulate how many primes are there
    std::for_each(random_inputs.begin(), random_inputs.end(), 
            [&](unsigned long n) 
    {
    	tp.submit(boost::bind(CountIfPrimeBoost, n, std::ref(number_of_primes)));
    });

    // wait for all threads to finish and do work at the same time
	while(tp.try_executing_one()) {}

    tp.close();

    return number_of_primes;
}
#endif

// using Jakob Progsch's thread pool
unsigned long count_primes_JP(const std::vector<unsigned long>& random_inputs, unsigned int NUMBER_OF_PROCS)
{
    std::atomic<unsigned long> number_of_primes(0);
    {
        ThreadPool pool(NUMBER_OF_PROCS);

	    // loop over input to accumulate how many primes are there
        std::for_each(random_inputs.begin(), random_inputs.end(), 
                [&](unsigned long n) 
        {
            pool.enqueue(CountIfPrimeBoost, n, std::ref(number_of_primes));
        });

        // as pool go out of scope, it will wait for all threads to finish
    }

    return number_of_primes;
}

// using old boost thread pool
unsigned long count_primes_boost_old(const std::vector<unsigned long>& random_inputs, unsigned int NUMBER_OF_PROCS)
{
    std::atomic<unsigned long> number_of_primes(0);
    {
        boost::threadpool::pool tp(NUMBER_OF_PROCS);

	    // loop over input to accumulate how many primes are there
        std::for_each(random_inputs.begin(), random_inputs.end(), 
                [&](unsigned long n) 
        {
            boost::threadpool::schedule(tp, boost::bind(CountIfPrimeBoost, n , std::ref(number_of_primes)));
        });

        // as pool go out of scope, it will wait for all threads to finish
    }

    return number_of_primes;
}

// using CPTL thread pool
unsigned long count_primes_ctpl(const std::vector<unsigned long>& random_inputs, unsigned int NUMBER_OF_PROCS)
{
    ctpl::thread_pool pool(NUMBER_OF_PROCS);
    std::atomic<unsigned long> number_of_primes(0);

	// loop over input to accumulate how many primes are there
    std::for_each(random_inputs.begin(), random_inputs.end(), 
            [&](unsigned long n) 
    {
        pool.push(CountIfPrimeCTPL, n, std::ref(number_of_primes));
    });

    // wait for all threads to finish
    pool.stop(true);

    return number_of_primes;
}

// single threaded calculation of primes
unsigned long count_primes(const std::vector<unsigned long>& random_inputs)
{
    unsigned long number_of_primes = 0;
	// loop over input to accumulate how many primes are there
    std::for_each(random_inputs.begin(), random_inputs.end(), 
            [&](unsigned long n) 
			{ 
				CountIfPrime(n, number_of_primes);
			});

    return number_of_primes;
}


int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cout << "Usage: " << argv[0] << " <size of input> <number of procs>" << std::endl;
        return 1;
    }

	const auto MAX_PROCS = std::thread::hardware_concurrency();
    const auto INPUT_SIZE = std::stol(argv[1]);
    const auto NUMBER_OF_PROC = std::stol(argv[2]);

    if (MAX_PROCS < NUMBER_OF_PROC)
    {
        std::cout << "maximum " << MAX_PROCS  << " concurrent threads are supported. use less threads" << std::endl;
        return 1;
    }

    std::vector<unsigned long> random_inputs;

    for (auto i = 0; i < INPUT_SIZE; ++i)
    {
        random_inputs.push_back(std::rand());
    }
    
    std::chrono::time_point<std::chrono::system_clock> start, end;

    start = std::chrono::system_clock::now();
    auto number_of_primes = count_primes(random_inputs);
    end = std::chrono::system_clock::now();
    std::cout << "count_primes:" << number_of_primes << " prime numbers were found. computation took " << 
        std::chrono::duration_cast<std::chrono::nanoseconds> (end - start).count()/INPUT_SIZE  << " nanosec per iteration" << std::endl;

    start = std::chrono::system_clock::now();
    number_of_primes = count_primes_ctpl(random_inputs, NUMBER_OF_PROC);
    end = std::chrono::system_clock::now();
    std::cout << "count_primes_ctpl:" << number_of_primes << " prime numbers were found. computation took " << 
        std::chrono::duration_cast<std::chrono::nanoseconds> (end - start).count()/INPUT_SIZE  << " nanosec per iteration" << std::endl;

    start = std::chrono::system_clock::now();
    number_of_primes = count_primes_boost_old(random_inputs, NUMBER_OF_PROC);
    end = std::chrono::system_clock::now();
    std::cout << "count_primes_boost_old:" << number_of_primes << " prime numbers were found. computation took " << 
        std::chrono::duration_cast<std::chrono::nanoseconds> (end - start).count()/INPUT_SIZE  << " nanosec per iteration" << std::endl;

    start = std::chrono::system_clock::now();
    number_of_primes = count_primes_JP(random_inputs, NUMBER_OF_PROC);
    end = std::chrono::system_clock::now();
    std::cout << "count_primes_JP:" << number_of_primes << " prime numbers were found. computation took " << 
        std::chrono::duration_cast<std::chrono::nanoseconds> (end - start).count()/INPUT_SIZE  << " nanosec per iteration" << std::endl;

#if BOOST_VERSION > 105600
    start = std::chrono::system_clock::now();
    number_of_primes = count_primes_boost(random_inputs, NUMBER_OF_PROC);
    end = std::chrono::system_clock::now();
    std::cout << "count_primes_boost:" << number_of_primes << " prime numbers were found. computation took " << 
        std::chrono::duration_cast<std::chrono::nanoseconds> (end - start).count()/INPUT_SIZE  << " nanosec per iteration" << std::endl;
#else
    std::cout << "count_primes_boost cannot be tested. boost version is: " << BOOST_VERSION << std::endl;
#endif

    start = std::chrono::system_clock::now();
    number_of_primes = count_primes_asio(random_inputs, NUMBER_OF_PROC);
    end = std::chrono::system_clock::now();
    std::cout << "count_primes_asio:" << number_of_primes << " prime numbers were found. computation took " << 
        std::chrono::duration_cast<std::chrono::nanoseconds> (end - start).count()/INPUT_SIZE  << " nanosec per iteration" << std::endl;

    return 0;
}

