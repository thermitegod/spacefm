#!/usr/bin/fish

#CC=gcc CXX=g++ meson setup --buildtype=debug --prefix=$(pwd)/build ./build
#CC=gcc CXX=g++ meson setup --buildtype=release --prefix=$(pwd)/build ./build

CC=clang CXX=clang++ meson setup --buildtype=debug --prefix=$(pwd)/build ./build
#CC=clang CXX=clang++ meson setup --buildtype=release --prefix=$(pwd)/build ./build

#CC=clang CXX=clang++ meson setup -Db_sanitize=address,undefined -Db_lundef=false --buildtype=debug --prefix=$(pwd)/build ./build
