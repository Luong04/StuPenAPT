#include <stdbool.h>
#include <pthread.h>

#ifndef ARC_PLUGIN_INTERFACE_H
#define ARC_PLUGIN_INTERFACE_H

enum value_type {V_INT=0,V_DOUBLE,V_STRING,V_INVALID};

struct arc_value
{
    enum value_type type;
    union
    {
	int v_int;
	double v_dbl;
	char *v_str;
    };
};


//To share this file with plugins without much baggage
struct arc_system;
struct arc_model
{
    struct arc_system *sys;
    pthread_mutex_t sys_lock;
};



void init_model(struct arc_model *model, struct arc_system *sys);
void destroy_model(struct arc_model *model);

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



#endif
