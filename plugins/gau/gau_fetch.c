#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tool_runner.h"
#include "arc_plugin_interface.h"
#include "gau_fetch.h"

static char *make_component_name(void)
{
    char *name = malloc(64);
    snprintf(name, 64, "Gau_%ld", time(NULL));
    return name;
}

static char *make_iface_name(int idx)
{
    char *name = malloc(32);
    snprintf(name, 32, "AggregatedURL_%d", idx);
    return name;
}

static void parse_gau_stdout(char *output, struct arc_model *a_m, const char *outfile_path)
{
    if (!output) return;

    char *component = make_component_name();
    arc_model_add_component(a_m, component);

    char prop[256];
    snprintf(prop, sizeof(prop), "%s:Source", component);
    arc_model_assign_property_str(a_m, prop, "Gau");

    snprintf(prop, sizeof(prop), "%s:OutputFile", component);
    arc_model_assign_property_str(a_m, prop, outfile_path);

    int count = 0;
    int filtered = 0;
    
    char *line = output;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        if (*line && (strncmp(line, "http://", 7) == 0 || strncmp(line, "https://", 8) == 0)) {
            count++;
            
            if (strstr(line, ".png") || strstr(line, ".jpg") || 
                strstr(line, ".ttf") || strstr(line, ".woff") || 
                strstr(line, ".gif")) {
                filtered++;
            } else {
                char *iface = make_iface_name(count - filtered);
                arc_model_add_interface(a_m, component, iface);

                snprintf(prop, sizeof(prop), "%s:%s:URL", component, iface);
                arc_model_assign_property_str(a_m, prop, line);

                free(iface);
            }
        }

        if (!nl) break;
        line = nl + 1;
    }

    snprintf(prop, sizeof(prop), "%s:TotalURLs", component);
    char cnt[16];
    snprintf(cnt, sizeof(cnt), "%d", count - filtered);
    arc_model_assign_property_str(a_m, prop, cnt);

    snprintf(prop, sizeof(prop), "%s:FilteredOut", component);
    snprintf(cnt, sizeof(cnt), "%d", filtered);
    arc_model_assign_property_str(a_m, prop, cnt);

    free(component);
}

bool fetch_all_urls_filtered(struct arc_model *a_m, char *args)
{
    struct arc_value target = arc_model_get_property(a_m, "Target");
    if (!target.v_str) {
        return false;
    }

    char out_path[256];
    snprintf(out_path, sizeof(out_path), "/tmp/gau_%ld.txt", time(NULL));

    struct tool_runner tr;
    tool_runner_init(&tr, "gau", 12);
    
    tool_runner_add_arg(&tr, "--blacklist");
    tool_runner_add_arg(&tr, "png,jpg,ttf,woff,gif");
    tool_runner_add_arg(&tr, "--mc");
    tool_runner_add_arg(&tr, "200,201,302,401,403");
    tool_runner_add_arg(&tr, "--o");
    tool_runner_add_arg(&tr, out_path);
    tool_runner_add_arg(&tr, target.v_str);

    tool_runner_run(&tr);

    parse_gau_stdout(tr.output, a_m, out_path);

    arc_model_assign_property_str(a_m, "GauOutput", out_path);

    tool_runner_destroy(&tr);
    free(target.v_str);
    return true;
}