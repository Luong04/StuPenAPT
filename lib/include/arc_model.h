#include <pthread.h>
#include "arc_value.h"
#include "list.h"
#include <stdbool.h>

#ifndef ARC_MODEL_H
#define ARC_MODEL_H

//To share this file with plugins without much baggage
struct arc_system;
struct arc_model
{
    struct arc_system *sys;
    pthread_mutex_t sys_lock;
};

void init_model(struct arc_model *model, struct arc_system *sys);
void destroy_model(struct arc_model *model);

//modify components/interfaces/invocations
bool arc_model_add_component(struct arc_model *a_m, char *name);
bool arc_model_add_interface(struct arc_model *a_m, char *component, char *name);
bool arc_model_add_invocation(struct arc_model *a_m, char *qual_from, char *qual_to);
bool arc_model_rem_component(struct arc_model *a_m, char *name);
bool arc_model_rem_interface(struct arc_model *a_m, char *component, char *name);
bool arc_model_rem_invocation(struct arc_model *a_m, char *qual_from, char *qual_to);

//The qualified name stuff is implemented a bit crudely, see if we can make it any better at some point.
bool arc_model_assign_property_int(struct arc_model *a_m, char *qual_name, int val);
bool arc_model_assign_property_dbl(struct arc_model *a_m, char *qual_name, double val);
bool arc_model_assign_property_str(struct arc_model *a_m, char *qual_name, char *val);
bool arc_model_remove_property(struct arc_model *a_m, char *qual_name);
const struct arc_value arc_model_get_property(struct arc_model *a_m, char *qual_name);

struct list arc_model_find_components(struct arc_model *a_m, char *p, struct arc_value v);
struct list arc_model_find_interfaces(struct arc_model *a_m, char *c, char *p, struct arc_value v);

#endif
