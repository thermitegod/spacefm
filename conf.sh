#!/bin/bash

#CC=gcc CXX=g++ meson setup --buildtype=debug --prefix=$(pwd)/build ./build
#CC=gcc CXX=g++ meson setup --buildtype=release --prefix=$(pwd)/build ./build

CC=clang CXX=clang++ meson setup --buildtype=debug --prefix=$(pwd)/build ./build
#CC=clang CXX=clang++ meson setup --buildtype=release --prefix=$(pwd)/build ./build

#CC=clang CXX=clang++ meson setup --buildtype=debug --prefix=$(pwd)/build -Db_sanitize=undefined -Db_lundef=false ./build
#CC=clang CXX=clang++ meson setup --buildtype=debug --prefix=$(pwd)/build -Db_sanitize=address -Db_lundef=false ./build
#CC=clang CXX=clang++ meson setup --buildtype=debug --prefix=$(pwd)/build -Db_sanitize=address,undefined -Db_lundef=false ./build
