#include <stddef.h>
#include <stdbool.h>

#include "attack_repertoire.h"

#ifndef ACTIVE_ATTACK_H
#define ACTIVE_ATTACK_H

enum ACTIVE_ATTACK_STEP_STATE {AAS_WAIT = 0 , AAS_RUN, AAS_DONE};


//Looks like this doesn't need a lock. Well maybe it does, recheck after implementing reporting
struct active_attack
{
    char *target;
    struct attack_pattern *attack;
    size_t cur_step;
    enum ACTIVE_ATTACK_STEP_STATE cur_step_state;
    bool cur_res;
};
void active_attack_init(struct active_attack *a_a, struct attack_pattern *a_p, char *target);
void active_attack_cleanup(struct active_attack *a_a);
void active_attack_next_step(struct active_attack *a_a);
bool active_attack_done(struct active_attack *a_a);


#define PENTEST_STATE_INIT_NUM_PAST_ATTACKS 8

struct past_attack
{
    char *a_p;
    char *target;
};

void past_attack_init(struct past_attack *p_a, char *a_p, char *target);
void past_attack_cleanup(struct past_attack *p_a);

#endif
