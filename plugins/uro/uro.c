#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "arc_plugin_interface.h"
#include "uro_dedupe.h"

static char *mapping[] = {
    "DeduplicateURLs:deduplicate_urls"
};

const char *tool_name(void)
{
    return "URO";
}

char **tool_interface(size_t *num_caps)
{
    *num_caps = sizeof(mapping) / sizeof(char*);
    return mapping;
}

bool can_deduplicate_urls(struct arc_model *a_m, char *args)
{
    struct arc_value f = arc_model_get_property(a_m, "HTTPxOutput");
    bool ok = (f.v_str != NULL);
    if (ok) free(f.v_str);
    return ok;
}