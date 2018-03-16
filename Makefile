
CXX ?= g++

CXXFLAGS ?= -Wall -std=c++11 -O3

.PHONY: all clean

PROG = benchmark_pool

all: $(PROG)

$(PROG): benchmark_pool.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< -I libs -lpthread -lboost_thread -lboost_system

clean: 
	rm -f $(PROG)

