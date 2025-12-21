#ifndef FUZZING_H
#define FUZZING_H

#include "arc_plugin_interface.h"
#include <stdbool.h>

bool light_fuzz(struct arc_model *a_m, char *args);
bool medium_fuzz(struct arc_model *a_m, char *args);
bool heavy_fuzz(struct arc_model *a_m, char *args);

#endif