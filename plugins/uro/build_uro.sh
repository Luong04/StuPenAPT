#!/bin/bash
gcc -fPIC -c -I../../include uro.c uro_dedupe.c tool_runner.c string_buffer.c tokenize.c
gcc -shared -o liburo.so uro.o uro_dedupe.o tool_runner.o string_buffer.o tokenize.o
rm -f *.o
echo "Built liburo.so"