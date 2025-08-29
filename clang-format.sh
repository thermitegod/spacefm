#!/bin/bash

find src/ -iname '*.cxx' -o -iname '*.hxx' | xargs --max-args=$(nproc) clang-format -i
find dialog/ -iname '*.cxx' -o -iname '*.hxx' | xargs --max-args=$(nproc) clang-format -i
find tools/ -iname '*.cxx' -o -iname '*.hxx' | xargs --max-args=$(nproc) clang-format -i
find tests/ -iname '*.cxx' -o -iname '*.hxx' | xargs --max-args=$(nproc) clang-format -i
