#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "tool_runner.h"
#include "arc_plugin_interface.h"
#include "fuzzing.h"

// Wordlist detection and selection
static char *get_wordlist_path(const char *size)
{
    // Priority order with SecLists paths
    char *light_candidates[] = {
        "/usr/share/seclists/Discovery/Web-Content/common.txt",
        "/usr/share/wordlists/dirb/common.txt",
        "/opt/seclists/Discovery/Web-Content/common.txt",
        "./wordlists/common.txt"
    };
    
    char *medium_candidates[] = {
        "/usr/share/seclists/Discovery/Web-Content/raft-medium-directories.txt",
        "/usr/share/seclists/Discovery/Web-Content/directory-list-2.3-medium.txt",
        "/usr/share/wordlists/dirb/big.txt",
        "/opt/seclists/Discovery/Web-Content/raft-medium-directories.txt",
        "./wordlists/medium.txt"
    };
    
    char *heavy_candidates[] = {
        "/usr/share/seclists/Discovery/Web-Content/raft-large-directories.txt",
        "/usr/share/seclists/Discovery/Web-Content/directory-list-lowercase-2.3-big.txt",
        "/usr/share/wordlists/dirbuster/directory-list-2.3-big.txt",
        "/opt/seclists/Discovery/Web-Content/raft-large-directories.txt",
        "./wordlists/large.txt"
    };
    
    char **candidates = NULL;
    size_t count = 0;
    
    if (strcmp(size, "light") == 0) {
        candidates = light_candidates;
        count = sizeof(light_candidates) / sizeof(char*);
    } else if (strcmp(size, "medium") == 0) {
        candidates = medium_candidates;
        count = sizeof(medium_candidates) / sizeof(char*);
    } else if (strcmp(size, "heavy") == 0) {
        candidates = heavy_candidates;
        count = sizeof(heavy_candidates) / sizeof(char*);
    }
    
    // Check which exists
    for (size_t i = 0; i < count; i++) {
        struct stat buffer;
        if (stat(candidates[i], &buffer) == 0) {
            char *result = malloc(strlen(candidates[i]) + 1);
            strcpy(result, candidates[i]);
            return result;
        }
    }
    
    // Fallback error message
    fprintf(stderr, "[ffuf] No wordlist found for size: %s\n", size);
    return NULL;
}

static void parse_ffuf_stdout(char *output, struct arc_model *a_m, const char *fuzz_level, const char *url)
{
    if (!output) return;

    char component_name[64];
    snprintf(component_name, sizeof(component_name), "FFUF_%ld", time(NULL));
    arc_model_add_component(a_m, component_name);

    char prop_path[256];
    snprintf(prop_path, sizeof(prop_path), "%s:FuzzLevel", component_name);
    arc_model_assign_property_str(a_m, prop_path, fuzz_level);

    snprintf(prop_path, sizeof(prop_path), "%s:TargetURL", component_name);
    arc_model_assign_property_str(a_m, prop_path, url);

    int idx = 0;
    char *line = output;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        // FFUF output format: URL [Status:200, Size:1024, Words:42, Lines:10]
        if (*line && strstr(line, "http")) {
            idx++;
            char iface[32];
            snprintf(iface, sizeof(iface), "Found_%d", idx);
            arc_model_add_interface(a_m, component_name, iface);

            snprintf(prop_path, sizeof(prop_path), "%s:%s:URL", component_name, iface);
            arc_model_assign_property_str(a_m, prop_path, line);

            // Parse status code
            char *status_start = strstr(line, "Status:");
            if (status_start) {
                char status[8];
                sscanf(status_start, "Status:%7s", status);
                snprintf(prop_path, sizeof(prop_path), "%s:%s:StatusCode", component_name, iface);
                arc_model_assign_property_str(a_m, prop_path, status);
            }

            // Classify by status
            if (strstr(line, "Status:200") || strstr(line, "Status:301") || strstr(line, "Status:302")) {
                snprintf(prop_path, sizeof(prop_path), "%s:%s:Relevance", component_name, iface);
                arc_model_assign_property_str(a_m, prop_path, "HIGH");
            } else {
                snprintf(prop_path, sizeof(prop_path), "%s:%s:Relevance", component_name, iface);
                arc_model_assign_property_str(a_m, prop_path, "MEDIUM");
            }
        }

        if (!nl) break;
        line = nl + 1;
    }

    snprintf(prop_path, sizeof(prop_path), "%s:DiscoveredCount", component_name);
    char count_str[16];
    snprintf(count_str, sizeof(count_str), "%d", idx);
    arc_model_assign_property_str(a_m, prop_path, count_str);
}

bool light_fuzz(struct arc_model *a_m, char *args)
{
    struct arc_value httpx_file = arc_model_get_property(a_m, "HTTPxOutput");

    char *wordlist = get_wordlist_path("light");
    if (!wordlist) {
        fprintf(stderr, "[ffuf] Light wordlist not found\n");
        free(httpx_file.v_str);
        return false;
    }

    struct tool_runner tr;
    tool_runner_init(&tr, "ffuf", 12);
    tool_runner_add_arg(&tr, "-l");
    tool_runner_add_arg(&tr, httpx_file.v_str);
    tool_runner_add_arg(&tr, "-w");
    tool_runner_add_arg(&tr, wordlist);
    tool_runner_add_arg(&tr, "-u");
    tool_runner_add_arg(&tr, "FUZZ");
    tool_runner_add_arg(&tr, "-silent");
    tool_runner_add_arg(&tr, "-t");
    tool_runner_add_arg(&tr, "40");
    tool_runner_add_arg(&tr, "-rate");
    tool_runner_add_arg(&tr, "100");

    tool_runner_run(&tr);
    parse_ffuf_stdout(tr.output, a_m, "LIGHT", httpx_file.v_str);

    tool_runner_destroy(&tr);
    free(httpx_file.v_str);
    free(wordlist);
    return true;
}

bool medium_fuzz(struct arc_model *a_m, char *args)
{
    struct arc_value httpx_file = arc_model_get_property(a_m, "HTTPxOutput");

    char *wordlist = get_wordlist_path("medium");
    if (!wordlist) {
        fprintf(stderr, "[ffuf] Medium wordlist not found\n");
        free(httpx_file.v_str);
        return false;
    }

    struct tool_runner tr;
    tool_runner_init(&tr, "ffuf", 12);
    tool_runner_add_arg(&tr, "-l");
    tool_runner_add_arg(&tr, httpx_file.v_str);
    tool_runner_add_arg(&tr, "-w");
    tool_runner_add_arg(&tr, wordlist);
    tool_runner_add_arg(&tr, "-u");
    tool_runner_add_arg(&tr, "FUZZ");
    tool_runner_add_arg(&tr, "-silent");
    tool_runner_add_arg(&tr, "-t");
    tool_runner_add_arg(&tr, "50");
    tool_runner_add_arg(&tr, "-rate");
    tool_runner_add_arg(&tr, "200");

    tool_runner_run(&tr);
    parse_ffuf_stdout(tr.output, a_m, "MEDIUM", httpx_file.v_str);

    tool_runner_destroy(&tr);
    free(httpx_file.v_str);
    free(wordlist);
    return true;
}

bool heavy_fuzz(struct arc_model *a_m, char *args)
{
    struct arc_value httpx_file = arc_model_get_property(a_m, "HTTPxOutput");

    char *wordlist = get_wordlist_path("heavy");
    if (!wordlist) {
        fprintf(stderr, "[ffuf] Heavy wordlist not found\n");
        free(httpx_file.v_str);
        return false;
    }

    struct tool_runner tr;
    tool_runner_init(&tr, "ffuf", 12);
    tool_runner_add_arg(&tr, "-l");
    tool_runner_add_arg(&tr, httpx_file.v_str);
    tool_runner_add_arg(&tr, "-w");
    tool_runner_add_arg(&tr, wordlist);
    tool_runner_add_arg(&tr, "-u");
    tool_runner_add_arg(&tr, "FUZZ");
    tool_runner_add_arg(&tr, "-silent");
    tool_runner_add_arg(&tr, "-t");
    tool_runner_add_arg(&tr, "100");
    tool_runner_add_arg(&tr, "-rate");
    tool_runner_add_arg(&tr, "500");

    tool_runner_run(&tr);
    parse_ffuf_stdout(tr.output, a_m, "HEAVY", httpx_file.v_str);

    tool_runner_destroy(&tr);
    free(httpx_file.v_str);
    free(wordlist);
    return true;
}