#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "arc_plugin_interface.h"
#include "vuln_scan.h"

static char *mapping[] = {
    "CVE_SCAN:cve_scan",
    "VULN_ENDPOINT_SCAN:vuln_endpoint_scan",
    "VULN_NETWORK_SCAN:vuln_network_scan",
    "EXPOSURE_SCAN:exposure_scan"
};

const char *tool_name(void)
{
    return "Nuclei";
}

char **tool_interface(size_t *num_caps)
{
    *num_caps = sizeof(mapping) / sizeof(char*);
    return mapping;
}

// Guards
bool can_cve_scan(struct arc_model *a_m, char *args)
{
    struct arc_value f = arc_model_get_property(a_m, "AliveOutput");
    bool ok = (f.v_str != NULL);
    if (ok) free(f.v_str);
    return ok;
}

bool can_vuln_endpoint_scan(struct arc_model *a_m, char *args)
{
    struct arc_value f = arc_model_get_property(a_m, "HTTPxOutput");
    bool ok = (f.v_str != NULL);
    if (ok) free(f.v_str);
    return ok;
}

bool can_vuln_network_scan(struct arc_model *a_m, char *args)
{
    struct arc_value f = arc_model_get_property(a_m, "TargetsFile");
    bool ok = (f.v_str != NULL);
    if (ok) free(f.v_str);
    return ok;
}

bool can_exposure_scan(struct arc_model *a_m, char *args)
{
    struct arc_value f = arc_model_get_property(a_m, "HTTPxOutput");
    bool ok = (f.v_str != NULL);
    if (ok) free(f.v_str);
    return ok;
}