#include <stdbool.h>
#include "arc_value.h"

#ifndef ARC_PROPERTY
#define ARC_PROPERTY


enum property_type {P_INT=0,P_DOUBLE,P_STRING,P_INVALID};

struct arc_property
{
    char *name;
    enum property_type type;
    union
    {
	int v_int;
	double v_dbl;
	char *v_str;
    };
};

void init_int_property(struct arc_property *property, char *name, int value);
void init_dbl_property(struct arc_property *property, char *name, double value);
void init_str_property(struct arc_property *property, char *name, char *value);
struct arc_property *clone_property(struct arc_property *property);

void destroy_property(struct arc_property *property);
void destroy_int_property(struct arc_property *property);
void destroy_dbl_property(struct arc_property *property);
void destroy_str_property(struct arc_property *property);

void print_property(struct arc_property *property);

bool property_eq_value(struct arc_property *p, struct arc_value *v);

#endif
