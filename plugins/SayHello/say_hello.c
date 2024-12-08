#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include "tool_runner.h"
#include "arc_plugin_interface.h"

static char *mapping[] =
{
    "SayHello:say_hello",
    "SayHi:say_hi",
    "SayBonjoir:say_bonjoir",
    "Dirlist:ls",
    "ShowSource:show_source",
    "PrintTarget:list_properties"
};

const char *tool_name(void)
{
    return "Greeter";
}

char **tool_interface(size_t *num_caps)
{
    *num_caps = sizeof(mapping)/sizeof(char*);
    return mapping;
}


void say_hello(struct arc_model *a_m)
{
    printf("Hello:%p\n",a_m);
}


void say_hi(struct arc_model *a_m)
{
    printf("Hi:%p\n",a_m);
}

void say_bonjoir(struct arc_model *a_m)
{
    printf("Bonjoir:%p\n",a_m);
}

void ls(struct arc_model *a_m)
{
    struct tool_runner tool_runner;
    tool_runner_init(&tool_runner, "ls", 0);
    tool_runner_run(&tool_runner);
    tool_runner_print_summary(&tool_runner);;
    tool_runner_destroy(&tool_runner);
}

void show_source(struct arc_model *a_m)
{
    struct tool_runner tool_runner;
    tool_runner_init(&tool_runner, "cat", 1);
    tool_runner_add_arg(&tool_runner,"say_hello.c");
    tool_runner_run(&tool_runner);
    tool_runner_print_summary(&tool_runner);;
    tool_runner_destroy(&tool_runner);
}

void list_properties(struct arc_model *a_m)
{
    printf("Looking for target in model:%p!\n",a_m);
    
    const struct arc_value target = arc_model_get_property(a_m, "Target");
    
    printf("%s\n", target.v_str);
    free(target.v_str);
}
