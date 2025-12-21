#!/bin/bash
gcc -fPIC -c -I../../include gau.c gau_fetch.c tool_runner.c string_buffer.c tokenize.c
gcc -shared -o libgau.so gau.o gau_fetch.o tool_runner.o string_buffer.o tokenize.o
rm -f *.o
echo "Built libgau.so"