#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "tool_runner.h"
#include "tokenize.h"
#include "arc_plugin_interface.h"
#include "web_crawl.h"

static char *make_component_name(char *url)
{
    char *comp_name = malloc(strlen(url) + 3);
    sprintf(comp_name, "W_%s", url);
    
    for (char *p = comp_name + 2; *p; p++) {
        if (*p == ':' || *p == '/' || *p == '.' || *p == '-') {
            *p = '_';
        }
    }
    
    return comp_name;
}

static char *make_interface_name(int index)
{
    char *iface_name = malloc(32);
    sprintf(iface_name, "URL_%d", index);
    return iface_name;
}

static void parse_katana_line(char *line, struct arc_model *a_m, char *component_name, int *url_index)
{
    if (!line || strlen(line) == 0) return;
    
    if (strncmp(line, "http://", 7) != 0 && strncmp(line, "https://", 8) != 0) return;
    
    (*url_index)++;
    
    char *iface_name = make_interface_name(*url_index);
    arc_model_add_interface(a_m, component_name, iface_name);
    
    char *prop_path = malloc(strlen(component_name) + strlen(iface_name) + 32);
    
    sprintf(prop_path, "%s:%s:URL", component_name, iface_name);
    arc_model_assign_property_str(a_m, prop_path, line);
    
    if (strstr(line, ".js")) {
        sprintf(prop_path, "%s:%s:Type", component_name, iface_name);
        arc_model_assign_property_str(a_m, prop_path, "JavaScript");
    } else if (strstr(line, ".css")) {
        sprintf(prop_path, "%s:%s:Type", component_name, iface_name);
        arc_model_assign_property_str(a_m, prop_path, "Stylesheet");
    } else if (strstr(line, "?") || strstr(line, "&")) {
        sprintf(prop_path, "%s:%s:Type", component_name, iface_name);
        arc_model_assign_property_str(a_m, prop_path, "Dynamic");
        
        char *query_start = strchr(line, '?');
        if (query_start) {
            sprintf(prop_path, "%s:%s:HasParameters", component_name, iface_name);
            arc_model_assign_property_str(a_m, prop_path, "true");
        }
    } else if (strstr(line, "/api/")) {
        sprintf(prop_path, "%s:%s:Type", component_name, iface_name);
        arc_model_assign_property_str(a_m, prop_path, "API");
    } else {
        sprintf(prop_path, "%s:%s:Type", component_name, iface_name);
        arc_model_assign_property_str(a_m, prop_path, "Static");
    }
    
    if (strstr(line, ".json")) {
        sprintf(prop_path, "%s:%s:Format", component_name, iface_name);
        arc_model_assign_property_str(a_m, prop_path, "JSON");
    } else if (strstr(line, ".xml")) {
        sprintf(prop_path, "%s:%s:Format", component_name, iface_name);
        arc_model_assign_property_str(a_m, prop_path, "XML");
    }
    
    free(prop_path);
    free(iface_name);
}

static void parse_katana_output(char *output, struct arc_model *a_m, char *target_url, char *crawl_type)
{
    if (!output) return;
    
    char *component_name = make_component_name(target_url);
    arc_model_add_component(a_m, component_name);
    
    char *prop_path = malloc(strlen(component_name) + 64);
    
    sprintf(prop_path, "%s:Target", component_name);
    arc_model_assign_property_str(a_m, prop_path, target_url);
    
    sprintf(prop_path, "%s:CrawlType", component_name);
    arc_model_assign_property_str(a_m, prop_path, crawl_type);
    
    int url_count = 0;
    char *line_start = output;
    char *line_end;
    
    while ((line_end = strchr(line_start, '\n')) != NULL) {
        *line_end = '\0';
        parse_katana_line(line_start, a_m, component_name, &url_count);
        line_start = line_end + 1;
    }
    
    if (strlen(line_start) > 0) {
        parse_katana_line(line_start, a_m, component_name, &url_count);
    }
    
    sprintf(prop_path, "%s:URLCount", component_name);
    char count_str[32];
    sprintf(count_str, "%d", url_count);
    arc_model_assign_property_str(a_m, prop_path, count_str);
    
    sprintf(prop_path, "%s:Status", component_name);
    arc_model_assign_property_str(a_m, prop_path, url_count > 0 ? "Crawled" : "Empty");
    
    free(prop_path);
    free(component_name);
}

bool full_active_crawl(struct arc_model *a_m, char *args)
{
    const struct arc_value target = arc_model_get_property(a_m, "Target");
    
    char output_file[256];
    snprintf(output_file, sizeof(output_file), "/tmp/katana_%ld.txt", time(NULL));
    
    struct tool_runner tool_runner;
    tool_runner_init(&tool_runner, "katana", 10);
    tool_runner_add_arg(&tool_runner, "-u");
    tool_runner_add_arg(&tool_runner, target.v_str);
    tool_runner_add_arg(&tool_runner, "-jc");
    tool_runner_add_arg(&tool_runner, "-fx");
    tool_runner_add_arg(&tool_runner, "-fs");
    tool_runner_add_arg(&tool_runner, "-silent");
    tool_runner_add_arg(&tool_runner, "-nc");
    tool_runner_add_arg(&tool_runner, "-o");
    tool_runner_add_arg(&tool_runner, output_file);
    
    tool_runner_run(&tool_runner);
    
    parse_katana_output(tool_runner.output, a_m, target.v_str, "FullActive");
    
    arc_model_assign_property_str(a_m, "KatanaOutput", output_file);
    
    tool_runner_destroy(&tool_runner);
    free(target.v_str);
    
    return true;
}

bool light_crawl(struct arc_model *a_m, char *args)
{
    const struct arc_value target = arc_model_get_property(a_m, "Target");
    char output_file[256];
    snprintf(output_file, sizeof(output_file), "/tmp/katana_%ld.txt", time(NULL));
    
    struct tool_runner tool_runner;
    tool_runner_init(&tool_runner, "katana", 6);
    tool_runner_add_arg(&tool_runner, "-u");
    tool_runner_add_arg(&tool_runner, target.v_str);
    tool_runner_add_arg(&tool_runner, "-xhr");
    tool_runner_add_arg(&tool_runner, "-fx");
    tool_runner_add_arg(&tool_runner, "-silent");
    tool_runner_add_arg(&tool_runner, "-nc");
    tool_runner_add_arg(&tool_runner, "-o");
    tool_runner_add_arg(&tool_runner, output_file);
    tool_runner_run(&tool_runner);
    
    parse_katana_output(tool_runner.output, a_m, target.v_str, "Light");
    arc_model_assign_property_str(a_m, "KatanaOutput", output_file);
    tool_runner_destroy(&tool_runner);
    free(target.v_str);
    
    return true;
}

bool default_crawl(struct arc_model *a_m, char *args)
{
    const struct arc_value target = arc_model_get_property(a_m, "Target");
    char output_file[256];
    snprintf(output_file, sizeof(output_file), "/tmp/katana_%ld.txt", time(NULL));
    
    struct tool_runner tool_runner;
    tool_runner_init(&tool_runner, "katana", 10);
    tool_runner_add_arg(&tool_runner, "-u");
    tool_runner_add_arg(&tool_runner, target.v_str);
    tool_runner_add_arg(&tool_runner, "-jc");
    tool_runner_add_arg(&tool_runner, "-xhr");
    tool_runner_add_arg(&tool_runner, "-fx");
    tool_runner_add_arg(&tool_runner, "-silent");
    tool_runner_add_arg(&tool_runner, "-nc");
    tool_runner_add_arg(&tool_runner, "-o");
    tool_runner_add_arg(&tool_runner, output_file);
    tool_runner_run(&tool_runner);
    
    parse_katana_output(tool_runner.output, a_m, target.v_str, "Default");
    arc_model_assign_property_str(a_m, "KatanaOutput", output_file);
    tool_runner_destroy(&tool_runner);
    free(target.v_str);
    
    return true;
}