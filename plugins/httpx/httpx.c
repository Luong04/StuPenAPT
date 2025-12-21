#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "arc_plugin_interface.h"
#include "httpx_filter.h"

static char *mapping[] = {
    "FilterLiveURLs:filter_live_urls",
    "FilterSuccess200_201:filter_success_only"
};

const char *tool_name(void)
{
    return "HTTPx";
}

char **tool_interface(size_t *num_caps)
{
    *num_caps = sizeof(mapping) / sizeof(char*);
    return mapping;
}

bool can_filter_live_urls(struct arc_model *a_m, char *args)
{
    struct arc_value f = arc_model_get_property(a_m, "KatanaOutput");
    bool ok = (f.v_str != NULL);
    if (ok) free(f.v_str);
    return ok;
}

bool can_filter_success_only(struct arc_model *a_m, char *args)
{
    struct arc_value f = arc_model_get_property(a_m, "KatanaOutput");
    bool ok = (f.v_str != NULL);
    if (ok) free(f.v_str);
    return ok;
}