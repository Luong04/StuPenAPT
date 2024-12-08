#include "knowledge_base.h"
#include "analysis.h"

#ifndef ANALYZER_H
#define ANALYZER_H

struct mape_k;

typedef void *(*strategy_sel_fn)(struct knowledge_base *k_b);
#define NUM_ANALYSES_INIT 4

struct analyzer
{
    //analysis handles
    struct analysis **analyses;
    size_t num_analyses;
    size_t max_analyses;

    //Strategy Selector
    strategy_sel_fn strategy_sel;

    /* Only make the pointer known */
    struct mape_k *mape_k;
};

void init_analyzer(struct analyzer *ana, struct mape_k *mape_k, strategy_sel_fn strategy_sel);
void analyzer_add_analysis(struct analyzer *ana, struct analysis *a);
void analyzer_rem_analysis(struct analyzer *ana, struct analysis *a);
bool analyzer_all_analyses_done(struct analyzer *ana);
void run_analyzer(struct analyzer *ana);
void destroy_analyzer(struct analyzer *ana);

#endif
