#include "arc_component.h"
#include "arc_property.h"
#include "arc_invocation.h"
#include <pthread.h>

#ifndef ARCH_SYSTEM
#define ARCH_SYSTEM

#define NUM_COMPONENTS_INIT 32
#define NUM_INVOCATIONS_INIT 128

enum ARC_ENTITY_TYPE { ARC_ENT_SYSTEM=0, ARC_ENT_COMPONENT, ARC_ENT_INVOCATION, ARC_ENT_INTERFACE };


struct arc_system
{
    char *sys_name;
    struct arc_component **components;
    size_t num_components;
    size_t max_components;

    struct arc_invocation **invocations;
    size_t num_invocations;
    size_t max_invocations;
    
    struct arc_property **properties;
    size_t num_properties;
    size_t max_properties;
};

void init_system(struct arc_system *sys, char *sys_name);
void destroy_system(struct arc_system *sys);
struct arc_system *clone_system(struct arc_system *sys);
//assumes everything is allocated in the heap via malloc
void cleanup_sys_clone(struct arc_system *sys);


void sys_add_property(struct arc_system *sys, struct arc_property *prop);
struct arc_property*  sys_rem_property(struct arc_system *sys, char *prop_name);
void sys_add_component(struct arc_system *sys, struct arc_component *comp);
struct arc_component*  sys_rem_component(struct arc_system *sys, char *comp_name);
void sys_add_invocation(struct arc_system *sys, struct arc_invocation *invo);
struct arc_invocation* sys_rem_invocation(struct arc_system *sys, char *from,
					  char *to, char *if_name);

struct arc_component* sys_find_component(struct arc_system *sys, char *comp_name);
struct arc_invocation* sys_find_invocation(struct arc_system *sys, char *from, char *to, char *if_name);
struct arc_property* sys_find_property(struct arc_system *sys, char *prop_name);

void print_system(struct arc_system *sys);
void sys_dump_to_file(struct arc_system *sys, char *fname);


#endif
