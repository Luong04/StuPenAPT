#include "arc_property.h"
#include "arc_system.h"
#include "arc_value.h"

#include <stdbool.h>

#ifndef ARC_CONDITION
#define ARC_CONDITION

enum ARC_COMPARISON_OPERATOR{ARC_COND_LT=0,ARC_COND_LE,ARC_COND_GT,ARC_COND_GE,ARC_COND_EQ,ARC_COND_INVALID};
enum ARC_BOOLEAN_OPERATOR{ARC_COND_OR=0, ARC_COND_AND};

struct arc_simple_condition
{
    struct arc_property *prop;
    enum ARC_COMPARISON_OPERATOR op;
    union
    {
	int v_int;
	double v_dbl;
	char *v_str;
    };
};

struct arc_complex_condition
{
    struct arc_simple_condition *simple_conds;
    //N-1 operators
    enum ARC_BOOLEAN_OPERATOR *ops;
    size_t num_simple_conditions;
};


void init_simple_condition(struct arc_simple_condition *cond,
			   struct arc_property *prop,
			   enum ARC_COMPARISON_OPERATOR op,
			   struct arc_value value);

void destroy_simple_condition(struct arc_simple_condition *cond);

bool arc_simple_cond_eval(struct arc_simple_condition *cond, struct arc_system *sys);

void init_complext_condition(struct arc_complex_condition *cond,
			     struct arc_simple_condition *simple_conds,
			     enum ARC_BOOLEAN_OPERATOR *ops,
			     size_t num_simple_conditions);

bool arc_complex_cond_eval(struct arc_complex_condition *cond, struct arc_system *sys);
void destroy_complex_condition(struct arc_complex_condition *cond);


#endif
