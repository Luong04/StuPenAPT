#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tool_runner.h"
#include "arc_plugin_interface.h"
#include "wayback_archive.h"

static char *make_component_name(void)
{
    char *name = malloc(64);
    snprintf(name, 64, "Wayback_%ld", time(NULL));
    return name;
}

static char *make_iface_name(int idx)
{
    char *name = malloc(32);
    snprintf(name, 32, "ArchiveURL_%d", idx);
    return name;
}

static void parse_wayback_stdout(char *output, struct arc_model *a_m, const char *outfile_path)
{
    if (!output) return;

    char *component = make_component_name();
    arc_model_add_component(a_m, component);

    char prop[256];
    snprintf(prop, sizeof(prop), "%s:Source", component);
    arc_model_assign_property_str(a_m, prop, "Wayback");

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

            free(iface);
        }

        if (!nl) break;
        line = nl + 1;
    }

    snprintf(prop, sizeof(prop), "%s:HistoricalCount", component);
    char cnt[16];
    snprintf(cnt, sizeof(cnt), "%d", count);
    arc_model_assign_property_str(a_m, prop, cnt);

    free(component);
}

bool fetch_wayback_urls(struct arc_model *a_m, char *args)
{
    struct arc_value subfinder_file = arc_model_get_property(a_m, "AliveOutput");
    if (!subfinder_file.v_str) {
        return false;
    }

    char out_path[256];
    snprintf(out_path, sizeof(out_path), "/tmp/wayback_%ld.txt", time(NULL));

    struct tool_runner tr;
    tool_runner_init(&tr, "bash", 8);
    tool_runner_add_arg(&tr, "-c");
    
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cat %s | waybackurls > %s", subfinder_file.v_str, out_path);
    tool_runner_add_arg(&tr, cmd);

    bool success = tool_runner_execute(&tr);
    char *output = tool_runner_get_output(&tr);

    parse_wayback_stdout(output, a_m, out_path);

    arc_model_assign_property_str(a_m, "WaybackOutput", out_path);

    free(subfinder_file.v_str);
    tool_runner_destroy(&tr);

    return success;
}