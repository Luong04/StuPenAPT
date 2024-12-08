#define _XOPEN_SOURCE 600
#include "active_attack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void active_attack_init(struct active_attack *a_a, struct attack_pattern *a_p, char *target)
{
    a_a->attack = a_p;
    a_a->target = strdup(target); //maybe we want to strdup this?
    a_a->cur_step = 0;
    a_a->cur_step_state = AAS_WAIT;
    a_a->cur_res = false;
}

void active_attack_cleanup(struct active_attack *a_a)
{
    free(a_a->target);
}


void active_attack_next_step(struct active_attack *a_a)
{
	a_a->cur_step++;
	a_a->cur_step_state = AAS_WAIT;
	a_a->cur_res = false;
}

bool active_attack_done(struct active_attack *a_a)
{
    //if a step is not done, we're not done
    if(a_a->cur_step_state != AAS_DONE)
    {
	return false;
    }
    
    if(!a_a->cur_res)
    {
	// if a step failed, we're done
	return true;
    }
    else if(a_a->cur_step >= a_a->attack->num_steps -1)
    {
	//we just finnished the last step so we're done
	return true;
    }
    
    return false;
    
}


void past_attack_init(struct past_attack *p_a, char *a_p, char *target)
{
    p_a->a_p = strdup(a_p);
    p_a->target = strdup(target);
}


void past_attack_cleanup(struct past_attack *p_a)
{
    free(p_a->a_p);
    free(p_a->target);
}
