#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tool_runner.h"
#include "arc_plugin_interface.h"
#include "vuln_scan.h"

static void parse_nuclei_stdout(char *output, struct arc_model *a_m, const char *scan_label, const char *input_file)
{
    if (!output) return;

    char component_name[64];
    snprintf(component_name, sizeof(component_name), "Nuclei_%ld", time(NULL));
    arc_model_add_component(a_m, component_name);

    char prop_path[256];
    snprintf(prop_path, sizeof(prop_path), "%s:ScanLabel", component_name);
    arc_model_assign_property_str(a_m, prop_path, scan_label);

    snprintf(prop_path, sizeof(prop_path), "%s:InputFile", component_name);
    arc_model_assign_property_str(a_m, prop_path, input_file ? input_file : "");

    int idx = 0;
    char *line = output;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        if (*line) {
            idx++;
            char iface[32];
            snprintf(iface, sizeof(iface), "Finding_%d", idx);
            arc_model_add_interface(a_m, component_name, iface);

            snprintf(prop_path, sizeof(prop_path), "%s:%s:Raw", component_name, iface);
            arc_model_assign_property_str(a_m, prop_path, line);

            const char *sev = "LOW";
            if (strstr(line, "[critical]") || strstr(line, "[high]")) sev = "HIGH";
            else if (strstr(line, "[medium]")) sev = "MEDIUM";

            snprintf(prop_path, sizeof(prop_path), "%s:%s:Severity", component_name, iface);
            arc_model_assign_property_str(a_m, prop_path, sev);
        }

        if (!nl) break;
        line = nl + 1;
    }

    snprintf(prop_path, sizeof(prop_path), "%s:FindingCount", component_name);
    char count_str[16];
    snprintf(count_str, sizeof(count_str), "%d", idx);
    arc_model_assign_property_str(a_m, prop_path, count_str);
}

bool cve_scan(struct arc_model *a_m, char *args)
{
    struct arc_value alive_file = arc_model_get_property(a_m, "AliveOutput");

    struct tool_runner tr;
    tool_runner_init(&tr, "nuclei", 8);
    tool_runner_add_arg(&tr, "-l");
    tool_runner_add_arg(&tr, alive_file.v_str);
    tool_runner_add_arg(&tr, "-t");
    tool_runner_add_arg(&tr, "http/cves/");
    tool_runner_add_arg(&tr, "-severity");
    tool_runner_add_arg(&tr, "critical,high,medium,low");
    tool_runner_add_arg(&tr, "-silent");

    tool_runner_run(&tr);
    parse_nuclei_stdout(tr.output, a_m, "CVE_SCAN", alive_file.v_str);

    tool_runner_destroy(&tr);
    free(alive_file.v_str);
    return true;
}

bool vuln_endpoint_scan(struct arc_model *a_m, char *args)
{
    struct arc_value httpx_file = arc_model_get_property(a_m, "HTTPxOutput");

    struct tool_runner tr;
    tool_runner_init(&tr, "nuclei", 8);
    tool_runner_add_arg(&tr, "-l");
    tool_runner_add_arg(&tr, httpx_file.v_str);
    tool_runner_add_arg(&tr, "-t");
    tool_runner_add_arg(&tr, "http/");
    tool_runner_add_arg(&tr, "-severity");
    tool_runner_add_arg(&tr, "medium,high,critical,low");
    tool_runner_add_arg(&tr, "-silent");

    tool_runner_run(&tr);
    parse_nuclei_stdout(tr.output, a_m, "VULN_ENDPOINT_SCAN", httpx_file.v_str);

    tool_runner_destroy(&tr);
    free(httpx_file.v_str);
    return true;
}

bool vuln_network_scan(struct arc_model *a_m, char *args)
{
    struct arc_value targets_file = arc_model_get_property(a_m, "TargetsFile");

    struct tool_runner tr;
    tool_runner_init(&tr, "nuclei", 8);
    tool_runner_add_arg(&tr, "-l");
    tool_runner_add_arg(&tr, targets_file.v_str);
    tool_runner_add_arg(&tr, "-t");
    tool_runner_add_arg(&tr, "network/");
    tool_runner_add_arg(&tr, "-severity");
    tool_runner_add_arg(&tr, "critical,high,medium,low");
    tool_runner_add_arg(&tr, "-silent");

    tool_runner_run(&tr);
    parse_nuclei_stdout(tr.output, a_m, "VULN_NETWORK_SCAN", targets_file.v_str);

    tool_runner_destroy(&tr);
    free(targets_file.v_str);
    return true;
}

bool exposure_scan(struct arc_model *a_m, char *args)
{
    struct arc_value httpx_file = arc_model_get_property(a_m, "HTTPxOutput");

    struct tool_runner tr;
    tool_runner_init(&tr, "nuclei", 8);
    tool_runner_add_arg(&tr, "-l");
    tool_runner_add_arg(&tr, httpx_file.v_str);
    tool_runner_add_arg(&tr, "-t");
    tool_runner_add_arg(&tr, "http/exposures/");
    tool_runner_add_arg(&tr, "-severity");
    tool_runner_add_arg(&tr, "medium,high,critical,low");
    tool_runner_add_arg(&tr, "-silent");

    tool_runner_run(&tr);
    parse_nuclei_stdout(tr.output, a_m, "EXPOSURE_SCAN", httpx_file.v_str);

    tool_runner_destroy(&tr);
    free(httpx_file.v_str);
    return true;
}