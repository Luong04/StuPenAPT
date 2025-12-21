#ifndef WAYBACK_ARCHIVE_H
#define WAYBACK_ARCHIVE_H

#include "arc_plugin_interface.h"
#include <stdbool.h>

bool fetch_wayback_urls(struct arc_model *a_m, char *args);

#endif