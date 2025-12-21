#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "arc_plugin_interface.h"
#include "gau_fetch.h"

static char *mapping[] = {
    "FetchAllURLsFiltered:fetch_all_urls_filtered"
};

const char *tool_name(void)
{
    return "Gau";
}

char **tool_interface(size_t *num_caps)
{
    *num_caps = sizeof(mapping) / sizeof(char*);
    return mapping;
}

bool can_fetch_all_urls_filtered(struct arc_model *a_m, char *args)
{
    struct arc_value target = arc_model_get_property(a_m, "Target");
    if (!target.v_str) return false;
    
    bool is_domain = (strchr(target.v_str, '.') != NULL) && 
                     (strstr(target.v_str, "http") != target.v_str);
    
    free(target.v_str);
    return is_domain;
}