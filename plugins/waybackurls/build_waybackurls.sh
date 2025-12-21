#!/bin/bash
gcc -fPIC -c -I../../include waybackurls.c wayback_archive.c tool_runner.c string_buffer.c tokenize.c
gcc -shared -o libwaybackurls.so waybackurls.o wayback_archive.o tool_runner.o string_buffer.o tokenize.o
rm -f *.o
echo "Built libwaybackurls.so"