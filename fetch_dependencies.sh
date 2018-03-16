#!/bin/bash

# get thread pool implementations to be benchmarked

mkdir -p libs
cd libs
git clone https://github.com/vit-vit/CTPL.git
git clone https://github.com/henkel/threadpool.git
git clone https://github.com/progschj/ThreadPool.git
cd -
