#define _XOPEN_SOURCE 500

#include "utility.h"
#include <string.h>
#include <stdlib.h>

void utility_score_init(struct utility_score *u_s, char *option, double value)
{
    u_s->option = strdup(option);
    u_s->value = value;
}

void utility_score_destroy(struct utility_score *u_s)
{
    free(u_s->option);
}


int compare_utility_scores(const void *e1, const void *e2)
{
    const struct utility_score *us1 = e1;
    const struct utility_score *us2 = e2;

    if(us1->value < us2->value)
    {
		return 1;
    }
    else if(us1->value > us2->value)
    {
		return -1;
    }
    else 
    {
		return 0;
    }
}

//MAKE A ARC_VALUE COPY FUNCTION
static void copy_value(struct arc_value *value, struct arc_value *copy)
{
	copy->type = value->type;
	switch (copy->type)
	{
	case V_INT:
		copy->v_int = value->v_int;
		break;
	case V_DOUBLE:
		copy->v_dbl = value->v_dbl;
		break;
	case V_STRING:
		copy->v_str = strdup(value->v_str);
		break;
	default:
		copy->v_int = value->v_int;
		break;
	}
}

void decision_alternative_init(struct decision_alternative *a, char *name,
							   struct arc_value *values, size_t n_v)
{

	//Do we want to copy the values? Otherwise this cannot be allocated on the stack
	a->name = strdup(name);
	a->values = malloc(n_v*sizeof(struct arc_value));
	for(size_t  i = 0; i < n_v ; i++)
	{
		copy_value(&values[i], &a->values[i]);
	}
	
	a->num_values = n_v;
	a->utility = 0;	
}

void decision_alternative_cleanup(struct decision_alternative *a)
{
	free(a->name);
	for(size_t i = 0 ; i < a->num_values;i++)
	{
		if(a->values[i].type == V_STRING)
		{
			free(a->values[i].v_str);
		}
	}
	free(a->values);
}



void utility_concern_init(struct utility_concern *u_c, char *name, double weight, value_fn u_f)
{
	u_c->name = strdup(name);
	u_c->weight = weight;
	u_c->u_f = u_f;
}

void utility_concern_destroy(struct utility_concern *u_c)
{
	free(u_c->name);
}

void utility_decision_init(struct utility_decision *u_d, size_t max_concerns)
{
	u_d->max_concerns = max_concerns;
	u_d->num_concerns = 0;
	u_d->concerns = malloc(u_d->max_concerns * sizeof(struct utility_concern));
	u_d->max_alternatives = MAX_INIT_ALTERNATIVES;
	u_d->options = malloc(u_d->max_alternatives * sizeof(struct decision_alternative));
	u_d->num_alternatives = 0;
}

void utility_decision_destroy(struct utility_decision *u_d)
{
	for(size_t i = 0 ; i < u_d->num_concerns; i++)
	{
		utility_concern_destroy(&u_d->concerns[i]);
	}
	
	free(u_d->concerns);
	for(size_t i = 0 ; i < u_d->num_alternatives;i++)
	{
		decision_alternative_cleanup(&u_d->options[i]);
	}
	free(u_d->options);
}

bool utility_decision_add_concern(struct utility_decision *u_d, char *name, double weight, value_fn fn)
{
	if(u_d->num_concerns >= u_d->max_concerns)
	{
		//We're full so do nothing
		return false;
	}

	utility_concern_init(&u_d->concerns[u_d->num_concerns], name, weight,fn);
	u_d->num_concerns++;
	return true;
}

bool utility_decision_add_option(struct utility_decision *u_d, char *name,
								 struct arc_value *vals, size_t n_v)
{
	if(n_v != u_d->num_concerns)
	{
		return false;
	}

	if(u_d->num_alternatives >= u_d->max_alternatives)
	{
		
		u_d->max_alternatives *=2;
		size_t nbytes = u_d->max_alternatives*sizeof(struct decision_alternative);
		//Buggy if realloc fails
		u_d->options = realloc(u_d->options, nbytes);
	}

	decision_alternative_init(&u_d->options[u_d->num_alternatives], name, vals, u_d->num_concerns);
	u_d->num_alternatives++;
	return true;
}

void decision_alternative_eval(struct decision_alternative *a, struct utility_decision *d)
{

	if(d->max_concerns != a->num_values)
	{
		return;
	}

	a->utility = 0;
	for(size_t i = 0 ; i < d->max_concerns;i++)
	{
		a->utility += d->concerns[i].weight*d->concerns[i].u_f(&a->values[i]);
	}
	
}


void utility_decision_clear_options(struct utility_decision *u_d)
{
	//Do we need to do any cleanup here?

	for(size_t i = 0; i < u_d->num_alternatives;i++)
	{
		decision_alternative_cleanup(&u_d->options[i]);
	}
	
	u_d->num_alternatives = 0;
	
}

static int cmp_options(const void *o1, const void *o2)
{
	const struct  decision_alternative *a1 = o1;
	const struct  decision_alternative *a2 = o2;
	
	if(a1->utility < a2->utility)
	{
		return 1;
	}
	else if (a1->utility > a2->utility)
	{
		return -1;
	}
	else
	{
		return 0;
	}
	
}


void utility_decision_rank(struct utility_decision *u_d)
{
	for(size_t i = 0 ; i < u_d->max_alternatives; i++)
	{
		decision_alternative_eval(&u_d->options[i], u_d);
	}

	qsort(u_d->options,u_d->max_alternatives, sizeof(struct decision_alternative), cmp_options);
}


