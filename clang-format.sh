#!/usr/bin/env bash

find \
    ./src \
    ./tools \
    ./tests \
    -iname '*.cxx' -o -iname '*.hxx' | \
    clang-format -i --files=/dev/stdin
