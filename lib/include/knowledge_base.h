#include <pthread.h>
#include "arc_model.h"
#include "arc_strategy.h"
#include "named_resource.h"

//to be replaced with an adaptation controller
#include "task_engine.h"

#ifndef KNOWLEDGE_BASE
#define KNOWLEDGE_BASE

#define ARC_KB_PLANS_INIT_MAX 16
#define ARC_KB_STRATS_INIT_MAX 16
#define ARC_KB_RES_INIT_MAX 32


/*TODO: Decide if we want to free res or not in the end.
  Right now calling knowledge_base_rem_res on something not allocated with malloc will corrupt memory
*/

struct knowledge_base
{
    /* The architectural model */
    struct arc_model *model;

    /* Resources */
    struct named_res **named_res;
    size_t num_res;
    size_t max_res; 

    /*Adaptation specifics*/
    struct arc_plan **known_plans;
    size_t num_plans;
    size_t max_plans;
    struct arc_strategy **known_strats;
    size_t num_strats;
    size_t max_strats;

};

void init_knowledge_base(struct knowledge_base *k_b, struct arc_model *model);
void destroy_knowledge_base(struct knowledge_base *k_b);

void knowledge_base_add_plan(struct knowledge_base *k_b, struct arc_plan *plan);
struct arc_plan* knowledge_base_get_plan(struct knowledge_base *k_b, char *plan_name);

void knowledge_base_add_strat(struct knowledge_base *k_b, struct arc_strategy *strat);
struct arc_strategy* knowledge_base_get_strat(struct knowledge_base *k_b, char *strat_name);

void knowledge_base_add_res(struct knowledge_base *k_b, struct named_res *named_res);
void* knowledge_base_get_res(struct knowledge_base *k_b, char *name);
void knowledge_base_rem_res(struct knowledge_base *k_b, char *name);
size_t knowledge_base_num_res(struct knowledge_base *k_b, char *name);

void knowledge_base_list_res(struct knowledge_base *k_b);

#endif
