#!/bin/bash

# get thread pool implementations to be benchmarked

if [ ! -d "libs" ]; then
    mkdir -p libs
    cd libs > /dev/null
    git clone https://github.com/vit-vit/CTPL.git
    git clone https://github.com/henkel/threadpool.git
    git clone https://github.com/progschj/ThreadPool.git
    cd - > /dev/null
else
    for dir in ./libs/*
    do
        cd $dir > /dev/null
        echo "Updating $dir..."
        git pull
        cd - > /dev/null
    done
fi

