#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "arc_plugin_interface.h"
#include "web_crawl.h"

static char *mapping[] = {
    "FullActiveCrawl:full_active_crawl",
    "LightCrawl:light_crawl",
    "DefaultCrawl:default_crawl"
};

const char *tool_name(void)
{
    return "Katana";
}

char **tool_interface(size_t *num_caps)
{
    *num_caps = sizeof(mapping) / sizeof(char*);
    return mapping;
}

bool can_full_active_crawl(struct arc_model *a_m, char *args)
{
    const struct arc_value target = arc_model_get_property(a_m, "Target");
    if (!target.v_str) return false;
    
    bool is_web = (strstr(target.v_str, "http://") == target.v_str || 
                   strstr(target.v_str, "https://") == target.v_str);
    
    free(target.v_str);
    return is_web;
}

bool can_light_crawl(struct arc_model *a_m, char *args)
{
    const struct arc_value target = arc_model_get_property(a_m, "Target");
    if (!target.v_str) return false;
    
    bool is_web = (strstr(target.v_str, "http://") == target.v_str || 
                   strstr(target.v_str, "https://") == target.v_str);
    
    free(target.v_str);
    return is_web;
}

bool can_default_crawl(struct arc_model *a_m, char *args)
{
    const struct arc_value target = arc_model_get_property(a_m, "Target");
    if (!target.v_str) return false;
    
    bool is_web = (strstr(target.v_str, "http://") == target.v_str || 
                   strstr(target.v_str, "https://") == target.v_str);
    
    free(target.v_str);
    return is_web;
}