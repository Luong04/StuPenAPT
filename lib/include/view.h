#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "arc_system.h"
#include "arc_component.h"

#ifndef VIEW_H

#define VIEW_FLAG_NAME_ONLY 0
#define VIEW_FLAG_INTERFACES 1 
#define VIEW_FLAG_PROPERTIES 2
#define VIEW_FLAG_PROP_VALUE 4

struct component_view
{
    struct arc_component *component;
    uint8_t view_flags;
    //not happy about this, consider if a better solution exists
    char *comp_name;
};

void component_view_init(struct component_view *c_v, struct arc_component *c,
			 uint8_t v_f);

void component_view_to_dot(struct component_view *c_v, FILE *fp);
void component_view_destroy(struct component_view *c_v);


#define MAX_INIT_COMPONENT_VIEWS 8

struct system_view
{
    struct arc_system *system;
    struct component_view *component_views;
    size_t num_component_views;
    size_t max_component_views;
    uint8_t default_flags;
};



void system_view_init(struct system_view *s_v, struct arc_system *s,uint8_t vf);
void system_view_destroy(struct system_view *s_v);
void system_view_add(struct system_view *s_v,struct component_view *c_v);
void system_view_rem(struct system_view *s_v,struct component_view *c_v);
void system_view_update(struct system_view *s_v, struct arc_system *system);
void system_view_to_dot(struct system_view *s_v, char *filename);


struct component_view *component_view_for(struct system_view *s_v, char *name);

#endif 
