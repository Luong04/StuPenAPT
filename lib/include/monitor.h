#include <stddef.h>
#include "probe.h"
#include "knowledge_base.h"


#ifndef MONITORING_CTX_H
#define MONITORING_CTX_H

#define NUM_PROBES_INIT 4

struct mape_k;
typedef bool (*goal_eval_fn)(struct knowledge_base *k_b);

//This should ideally not know about tasks at all.
struct monitor
{
    /* Probe handles */
    struct probe **probes;
    size_t num_probes;
    size_t max_probes;

    /* Goal Evaluator */
    goal_eval_fn goal_eval;

    /* Only make the pointer known */
    struct mape_k *mape_k;
    
};


void init_monitor(struct monitor *mon, struct mape_k *mape_k,
		  goal_eval_fn goal_eval);

//see this adds an already created probe.
void monitor_add_probe(struct monitor *mon, struct probe *probe);
void monitor_rem_probe(struct monitor *mon, struct probe *probe);

//runs on initialized probes
void monitor_run(struct monitor *mon);
//possibly by idx?
void monitor_stop_probe(struct monitor *mon, struct probe *probe);
void destroy_monitor(struct monitor *mon);

bool monitor_all_probes_done(struct monitor *mon);
bool monitor_has_probes_waiting(struct monitor *mon);

#endif
