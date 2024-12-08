TOOL_SOURCE=say_hello
${CC} $TOOL_SOURCE.c tool_runner.c string_buffer.c -O0 -fPIC -shared -o lib${TOOL_SOURCE}.so
