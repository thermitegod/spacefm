#!/bin/sh
find ./src -iname *.h -o -iname *.c -o -iname *.hxx -o -iname *.cxx -o -iname *.ixx | xargs clang-format -i
