#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "arc_plugin_interface.h"
#include "fuzzing.h"

static char *mapping[] = {
    "LightFuzz:light_fuzz",
    "MediumFuzz:medium_fuzz",
    "HeavyFuzz:heavy_fuzz"
};

const char *tool_name(void)
{
    return "FFUF";
}

char **tool_interface(size_t *num_caps)
{
    *num_caps = sizeof(mapping) / sizeof(char*);
    return mapping;
}

// Guards
bool can_light_fuzz(struct arc_model *a_m, char *args)
{
    struct arc_value f = arc_model_get_property(a_m, "HTTPxOutput");
    bool ok = (f.v_str != NULL);
    if (ok) free(f.v_str);
    return ok;
}

bool can_medium_fuzz(struct arc_model *a_m, char *args)
{
    struct arc_value f = arc_model_get_property(a_m, "HTTPxOutput");
    bool ok = (f.v_str != NULL);
    if (ok) free(f.v_str);
    return ok;
}

bool can_heavy_fuzz(struct arc_model *a_m, char *args)
{
    struct arc_value f = arc_model_get_property(a_m, "HTTPxOutput");
    bool ok = (f.v_str != NULL);
    if (ok) free(f.v_str);
    return ok;
}