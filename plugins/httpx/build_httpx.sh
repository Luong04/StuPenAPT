#!/bin/bash
gcc -fPIC -c -I../../include httpx.c httpx_filter.c tool_runner.c string_buffer.c tokenize.c
gcc -shared -o libhttpx.so httpx.o httpx_filter.o tool_runner.o string_buffer.o tokenize.o
rm -f *.o
echo "Built libhttpx.so"