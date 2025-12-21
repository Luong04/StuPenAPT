#!/bin/bash
# filepath: plugins/katana/build_katana.sh

gcc -fPIC -c -I../../include katana.c web_crawl.c tool_runner.c string_buffer.c tokenize.c

gcc -shared -o libkatana.so katana.o web_crawl.o tool_runner.o string_buffer.o tokenize.o

rm -f *.o

echo "Built libkatana.so"