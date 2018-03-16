
CXX ?= g++

CXXFLAGS ?= -Wall -O3

.PHONY: all clean

PROG = benchmark_pool

all: $(PROG)

$(PROG): benchmark_pool.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< --std=c++11 -I /usr/local/include -I libs -L /usr/local/lib -lpthread -lboost_thread -lboost_system

clean: 
	rm -f $(PROG)

