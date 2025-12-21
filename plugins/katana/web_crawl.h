#ifndef WEB_CRAWL_H
#define WEB_CRAWL_H

#include "arc_plugin_interface.h"
#include <stdbool.h>

bool full_active_crawl(struct arc_model *a_m, char *args);
bool light_crawl(struct arc_model *a_m, char *args);
bool default_crawl(struct arc_model *a_m, char *args);

#endif