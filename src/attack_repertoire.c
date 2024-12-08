#define _XOPEN_SOURCE 600

#include "attack_repertoire.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void attack_step_init(struct attack_step *a_s, char *name,  char *MATTACK, cap_execute run, cap_guard guard)
{
    strncpy(a_s->name, name, sizeof(a_s->name));
    strncpy(a_s->MATTACK, MATTACK, sizeof(a_s->MATTACK));
    a_s->run = run;
    a_s->guard = guard;
}

void attack_step_print(struct attack_step *a_s)
{
    printf("%s:\n", a_s->name);
    printf(" - MITRE ATTCK: %s\n", a_s->MATTACK);
    
}


void attack_pattern_init(struct attack_pattern *a_p, char *name,
			 enum ATTACK_VECTOR a_v, enum ATTACK_COMPLEXITY a_c,
			 enum PRIVILEGES_REQUIRED p_r,  enum USER_INTERACTION u_i,
			 enum RUNNING_TIME r_t)
{
    strncpy(a_p->name, name, sizeof(a_p->name));
    a_p->a_v = a_v;
    a_p-> a_c = a_c;
    a_p->p_r = p_r;
    a_p->u_i = u_i;
    a_p->r_t = r_t;

    a_p->num_steps = 0;
    a_p->max_steps = ATTACK_PATTERN_INIT_NUM_STEPS;
    a_p->a_steps = malloc(a_p->max_steps * sizeof(char*));
	    
}



void attack_pattern_add_step(struct attack_pattern *a_p, char *a_s)
{
    if(a_p->num_steps >= a_p->max_steps)
    {
	a_p->max_steps *= 2;
	a_p->a_steps = realloc(a_p->a_steps,a_p->max_steps * sizeof(char*));
    }

    a_p->a_steps[a_p->num_steps] = strdup(a_s);
    a_p->num_steps++;
}

void attack_pattern_destroy(struct attack_pattern *a_p)
{

    for(size_t i = 0 ; i < a_p->num_steps;i++)
    {
	free(a_p->a_steps[i]);
    }
    
    free(a_p->a_steps);
}


void attack_pattern_print(struct attack_pattern *a_p)
{
    printf("%s:\n",a_p->name);
    printf(" - AV: %g\n",attack_vector_score(a_p->a_v));
    printf(" - AC: %g\n",attack_complexity_score(a_p->a_c));
    printf(" - PR: %g\n",privileges_required_score(a_p->p_r));
    printf(" - UI: %g\n",user_interaction_score(a_p->u_i));
    printf(" - RT: %g\n",running_time_score(a_p->r_t));


    //prettify this if needed, for now let it be
    printf(" -steps:\n");
    for(size_t i=0;i<a_p->num_steps;i++)
    {
	printf("      %zu: %s\n",i,a_p->a_steps[i]);
    }
    
}



void attack_repertoire_init(struct attack_repertoire *a_r)
{
    a_r->num_attack_steps = 0;
    a_r->max_attack_steps = ATTACK_REPERTOIRE_INIT_NUM_STEPS;
    a_r->a_steps = malloc(a_r->max_attack_steps * sizeof(struct attack_step));

    a_r->num_attack_patterns = 0;
    a_r->max_attack_patterns = ATTACK_REPERTOIRE_INIT_NUM_PATTERNS;
    a_r->a_patterns = malloc(a_r->max_attack_patterns * sizeof(struct attack_pattern));
    
}

void attack_repertoire_add_pattern(struct attack_repertoire *a_r, struct attack_pattern *a_p)
{
    if(a_r->num_attack_patterns >= a_r->max_attack_patterns)
    {
	a_r->max_attack_patterns *= 2;
	a_r->a_patterns = realloc(a_r->a_patterns, a_r->max_attack_patterns * sizeof(struct attack_pattern));
    }

    a_r->a_patterns[a_r->num_attack_patterns] = *a_p;
    a_r->num_attack_patterns++;
    
}


void attack_repertoire_add_step(struct attack_repertoire *a_r, struct attack_step *a_s)
{
    if(a_r->num_attack_steps >= a_r->max_attack_steps)
    {
	a_r->max_attack_steps *= 2;
	a_r->a_steps = realloc(a_r->a_steps, a_r->max_attack_steps * sizeof(struct attack_step));
    }

    a_r->a_steps[a_r->num_attack_steps] = *a_s;
    a_r->num_attack_steps++;
    
}

void attack_repertoire_destroy(struct attack_repertoire *a_r)
{
    //cleanup patterns
    for(size_t i=0;i<a_r->num_attack_patterns;i++)
    {
	struct attack_pattern *a_p = &a_r->a_patterns[i];
	attack_pattern_destroy(a_p);
    }
    free(a_r->a_patterns);

    //cleanup steps
    free(a_r->a_steps);
}

void attack_repertoire_print(struct attack_repertoire *a_r)
{
    printf("[Steps]\n");
    for(size_t i=0;i<a_r->num_attack_steps;i++)
    {
	attack_step_print(&a_r->a_steps[i]);
    }
    printf("[Patterns]\n");
    for(size_t i=0;i<a_r->num_attack_patterns;i++)
    {
	attack_pattern_print(&a_r->a_patterns[i]);
    }
}


struct attack_pattern *attack_repertoire_get_pattern(struct attack_repertoire *a_r, char *attack)
{
    for(size_t i=0;i < a_r->num_attack_patterns;i++)
    {
	if(strcmp(a_r->a_patterns[i].name, attack) == 0)
	{
	    return &a_r->a_patterns[i];
	}
    }

    return NULL;
}


struct attack_step *attack_repertoire_get_attack_step(struct attack_repertoire *a_r, char *step)
{
    for(size_t i=0;i < a_r->num_attack_steps;i++)
    {
	if(strcmp(a_r->a_steps[i].name, step) == 0)
	{
	    return &a_r->a_steps[i];
	}
    }

    return NULL;
}
