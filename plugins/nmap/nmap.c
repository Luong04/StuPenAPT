#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include "tool_runner.h"
#include "tokenize.h"
#include "arc_plugin_interface.h"

static char *mapping[] =
{
    "FullScan:full_scan",
    "QuickScan:quick_scan",
};

const char *tool_name(void)
{
    return "Nmap";
}

char **tool_interface(size_t *num_caps)
{
    *num_caps = sizeof(mapping)/sizeof(char*);
    return mapping;
}


