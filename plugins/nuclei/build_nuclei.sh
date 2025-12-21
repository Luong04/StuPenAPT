#!/bin/bash
gcc -fPIC -c -I../../include nuclei.c vuln_scan.c tool_runner.c string_buffer.c tokenize.c
gcc -shared -o libnuclei.so nuclei.o vuln_scan.o tool_runner.o string_buffer.o tokenize.o
rm -f *.o
echo "Built libnuclei.so"