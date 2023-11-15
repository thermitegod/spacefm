#!/bin/bash

find src/ -iname '*.cxx' -o -iname '*.hxx' | xargs --max-args=$(nproc) clang-format -i
