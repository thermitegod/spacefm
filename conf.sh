#!/bin/bash

mkdir -p ./build
cd ./build

#CC=gcc CXX=g++ meson setup --buildtype=debug ..
#CC=gcc CXX=g++ meson setup --buildtype=release ..

CC=/usr/lib/llvm/16/bin/clang CXX=/usr/lib/llvm/16/bin/clang++ meson setup --buildtype=debug ..
#CC=/usr/lib/llvm/16/bin/clang CXX=/usr/lib/llvm/16/bin/clang++ meson setup --buildtype=release ..

#CC=clang CXX=clang++ CC_LD=mold CXX_LD=mold meson setup --buildtype=debug ..
#CC=clang CXX=clang++ CC_LD=mold CXX_LD=mold meson setup --buildtype=release ..
