#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tool_runner.h"
#include "arc_plugin_interface.h"
#include "uro_dedupe.h"

static char *make_component_name(void)
{
    char *name = malloc(64);
    snprintf(name, 64, "URO_%ld", time(NULL));
    return name;
}

static char *make_iface_name(int idx)
{
    char *name = malloc(32);
    snprintf(name, 32, "DedupeURL_%d", idx);
    return name;
}

static void parse_uro_stdout(char *output, struct arc_model *a_m, const char *outfile_path)
{
    if (!output) return;

    char *component = make_component_name();
    arc_model_add_component(a_m, component);

    char prop[256];
    snprintf(prop, sizeof(prop), "%s:Source", component);
    arc_model_assign_property_str(a_m, prop, "URO");

    snprintf(prop, sizeof(prop), "%s:OutputFile", component);
    arc_model_assign_property_str(a_m, prop, outfile_path);

    int count = 0;
    char *line = output;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        if (*line && (strncmp(line, "http://", 7) == 0 || strncmp(line, "https://", 8) == 0)) {
            count++;
            char *iface = make_iface_name(count);
            arc_model_add_interface(a_m, component, iface);

            snprintf(prop, sizeof(prop), "%s:%s:URL", component, iface);
            arc_model_assign_property_str(a_m, prop, line);

            // Extract and classify URL pattern
            char *query_pos = strchr(line, '?');
            char *path_end = query_pos ? query_pos : strchr(line, '\0');
            
            if (strstr(line, "/api/")) {
                snprintf(prop, sizeof(prop), "%s:%s:Pattern", component, iface);
                arc_model_assign_property_str(a_m, prop, "API");
            } else if (query_pos) {
                snprintf(prop, sizeof(prop), "%s:%s:Pattern", component, iface);
                arc_model_assign_property_str(a_m, prop, "Dynamic");
            } else {
                snprintf(prop, sizeof(prop), "%s:%s:Pattern", component, iface);
                arc_model_assign_property_str(a_m, prop, "Static");
            }

            free(iface);
        }

        if (!nl) break;
        line = nl + 1;
    }

    snprintf(prop, sizeof(prop), "%s:DedupeCount", component);
    char cnt[16];
    snprintf(cnt, sizeof(cnt), "%d", count);
    arc_model_assign_property_str(a_m, prop, cnt);

    free(component);
}

bool deduplicate_urls(struct arc_model *a_m, char *args)
{
    struct arc_value httpx_file = arc_model_get_property(a_m, "HTTPxOutput");

    char out_path[256];
    snprintf(out_path, sizeof(out_path), "/tmp/uro_%ld.txt", time(NULL));

    struct tool_runner tr;
    tool_runner_init(&tr, "uro", 4);
    tool_runner_add_arg(&tr, "-i");
    tool_runner_add_arg(&tr, httpx_file.v_str);
    tool_runner_add_arg(&tr, "-o");
    tool_runner_add_arg(&tr, out_path);

    tool_runner_run(&tr);

    parse_uro_stdout(tr.output, a_m, out_path);

    arc_model_assign_property_str(a_m, "UROOutput", out_path);

    tool_runner_destroy(&tr);
    free(httpx_file.v_str);
    return true;
}