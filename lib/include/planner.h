#include <stdint.h>
#include <stddef.h>
#include "plan.h"

#ifndef PLANNER_H
#define PLANNER_H

struct mape_k;


#define NUM_PLANS_INIT 4

struct planner
{
    //plan handles
    struct plan **plans;
    size_t num_plans;
    size_t max_plans;


    /* Only make the pointer known */
    struct mape_k *mape_k;
};


void init_planner(struct planner *pl, struct mape_k *mape_k);
void planner_add_plan(struct planner *pl, struct plan *p);
void planner_rem_plan(struct planner *pl, struct plan *p);
bool planner_all_plans_created(struct planner *pl);
void run_planner(struct planner *pl);
void destroy_planner(struct planner *pl);



#endif
