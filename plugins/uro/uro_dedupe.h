#ifndef URO_DEDUPE_H
#define URO_DEDUPE_H

#include "arc_plugin_interface.h"
#include <stdbool.h>

bool deduplicate_urls(struct arc_model *a_m, char *args);

#endif