#ifndef VULN_SCAN_H
#define VULN_SCAN_H

#include "arc_plugin_interface.h"
#include <stdbool.h>

bool cve_scan(struct arc_model *a_m, char *args);
bool vuln_endpoint_scan(struct arc_model *a_m, char *args);
bool vuln_network_scan(struct arc_model *a_m, char *args);
bool exposure_scan(struct arc_model *a_m, char *args);

#endif