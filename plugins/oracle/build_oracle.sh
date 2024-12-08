CC=gcc
TOOL_NAME=oracle
${CC} *.c -O2 -fPIC  -shared -o lib${TOOL_NAME}.so

# -fsanitize=address -g -ggdb
