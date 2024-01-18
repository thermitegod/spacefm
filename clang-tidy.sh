#!/bin/bash

# runs all checks in .clang-tidy
run-clang-tidy -config-file=".clang-tidy" -quiet -p ./build

# use this to oneshot a check
# run-clang-tidy -quiet -fix -checks="-*," -p ./build
