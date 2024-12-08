#include <stdbool.h>
#include <stddef.h>
#include "arc_value.h"


#ifndef UTILITY_H
#define UTILITY_H

struct utility_score
{
    char *option;
    double value;
};

void utility_score_init(struct utility_score *u_s, char *option, double value);
void utility_score_destroy(struct utility_score *u_s);

int compare_utility_scores(const void *e1, const void *e2);


typedef double (*value_fn)(struct arc_value *a_v);

struct utility_concern
{
	char *name;
	double weight;
	value_fn u_f;
};

struct decision_alternative
{
	char *name;
	struct arc_value *values;
	size_t num_values;
	double utility;
};



#define MAX_INIT_ALTERNATIVES 8

struct utility_decision
{
	struct utility_concern *concerns;
	size_t max_concerns;
	size_t num_concerns;

	struct decision_alternative *options;
	size_t max_alternatives;
	size_t num_alternatives;
};



void decision_alternative_init(struct decision_alternative *a, char *name,
							   struct arc_value *values, size_t n_v);
void decision_alternative_cleanup(struct decision_alternative *a);
void decision_alternative_eval(struct decision_alternative *a, struct utility_decision *d);
//If we choose to copy the values we need a destroy function

void utility_concern_init(struct utility_concern *u_c, char *name, double weight, value_fn u_f);
void utility_concern_destroy(struct utility_concern *u_c);


void utility_decision_init(struct utility_decision *u_d, size_t num_concerns);
void utility_decision_destroy(struct utility_decision *u_d);
bool utility_decision_add_concern(struct utility_decision *u_d, char *name,
								  double weight, value_fn fn);

bool utility_decision_add_option(struct utility_decision *u_d, char *name,
								 struct arc_value *vals, size_t n_v);

void utility_decision_clear_options(struct utility_decision *u_d);
void utility_decision_rank(struct utility_decision *u_d);



#endif



