#ifndef HTTPX_FILTER_H
#define HTTPX_FILTER_H

#include "arc_plugin_interface.h"
#include <stdbool.h>

bool filter_live_urls(struct arc_model *a_m, char *args);
bool filter_success_only(struct arc_model *a_m, char *args);

#endif