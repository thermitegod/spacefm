#!/bin/bash

find src/ -iname '*.cxx' -o -iname '*.hxx' | xargs clang-format -i
