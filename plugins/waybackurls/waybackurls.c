#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "arc_plugin_interface.h"
#include "wayback_archive.h"

static char *mapping[] = {
    "FetchWaybackURLs:fetch_wayback_urls"
};

const char *tool_name(void)
{
    return "Wayback";
}

char **tool_interface(size_t *num_caps)
{
    *num_caps = sizeof(mapping) / sizeof(char*);
    return mapping;
}

bool can_fetch_wayback_urls(struct arc_model *a_m, char *args)
{
    const struct arc_value alive = arc_model_get_property(a_m, "AliveOutput");
    if (!alive.v_str) return false;
    
    free(alive.v_str);
    return true;
}