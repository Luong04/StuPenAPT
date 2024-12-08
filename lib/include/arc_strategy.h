#include "arc_plan.h"
#include "arc_condition.h"

#ifndef ARC_STRATEGY
#define ARC_STRATEGY

#define ARC_CONDPLAN_INITIAL_CAP 8

struct arc_condplan
{
    struct arc_complex_condition *condition;
    struct arc_plan *plan;
};

struct arc_strategy
{
    char *name;
    struct arc_complex_condition *strat_cond;
    struct arc_condplan **cond_plans;
    size_t num_cond_plans;
    size_t max_cond_plans;
    struct arc_system *sys;
    struct knowledge_base *kb;
};

void init_strategy(struct arc_strategy *strat, char *name, struct arc_system *sys,struct knowledge_base *kb);
void destroy_strategy(struct arc_strategy *strat);

void strat_add_plan(struct arc_strategy *strategy , struct arc_condplan *condplan);


//parsing
struct arc_strategy* strategy_parse(char *txt, struct arc_system *sys,struct knowledge_base *kb);


int strategy_execute(struct arc_strategy *strat);


#endif
