#include "arc_interface.h"
#include <stddef.h>

#ifndef ARC_COMPONENT
#define ARC_COMPONENT

#define NUM_INTERFACES_INIT 16

struct arc_component
{
    char *comp_name;
    struct arc_property **properties;
    size_t num_properties;
    size_t max_properties;
    struct arc_interface **interfaces;
    size_t num_interfaces;
    size_t max_interfaces;
};

void init_component(struct arc_component *comp, char *comp_name);
void destroy_component(struct arc_component *comp);
struct arc_component *clone_component(struct arc_component *comp);

void comp_add_iface(struct arc_component *comp, struct arc_interface *iface);
void comp_add_property(struct arc_component *comp, struct arc_property *prop);

struct arc_interface* comp_rem_iface(struct arc_component *comp, char *iface_name);
struct arc_property* comp_rem_property(struct arc_component *comp, char *prop_name);

struct arc_interface* comp_find_iface(struct arc_component *comp, char *iface_name);
struct arc_property* comp_find_property(struct arc_component *comp, char *prop_name);


void print_component(struct arc_component *comp);

#endif
