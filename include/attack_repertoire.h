#include "attack_scoring.h"
#include "tool_plugin.h"
#include <stddef.h>

#ifndef ATTACK_REPERTOIRE_H
#define ATTACK_REPERTOIRE_H

struct attack_step
{
    char name[64];
    char MATTACK[12];
    cap_execute run;
    cap_guard guard;
};

void attack_step_init(struct attack_step *a_s, char *name,  char *MATTACK, cap_execute run, cap_guard guard);
void attack_step_print(struct attack_step *a_s);

#define ATTACK_PATTERN_INIT_NUM_STEPS 2

struct attack_pattern
{
    char name[64];
    char **a_steps;
    size_t num_steps;
    size_t max_steps;
    enum ATTACK_VECTOR a_v;
    enum ATTACK_COMPLEXITY a_c;
    enum PRIVILEGES_REQUIRED p_r;
    enum USER_INTERACTION u_i;
    enum RUNNING_TIME r_t;
};

void attack_pattern_init(struct attack_pattern *a_p, char *name,
			 enum ATTACK_VECTOR a_v, enum ATTACK_COMPLEXITY a_c,
			 enum PRIVILEGES_REQUIRED p_r,  enum USER_INTERACTION u_i,
			 enum RUNNING_TIME r_t);
void attack_pattern_add_step(struct attack_pattern *a_p, char *a_s);
void attack_pattern_destroy(struct attack_pattern *a_p);
void attack_pattern_print(struct attack_pattern *a_p);

struct attack_repertoire
{   
    struct attack_pattern *a_patterns;
    size_t num_attack_patterns;
    size_t max_attack_patterns;

    struct attack_step *a_steps;
    size_t num_attack_steps;
    size_t max_attack_steps;
    
};

#define ATTACK_REPERTOIRE_INIT_NUM_PATTERNS 16
#define ATTACK_REPERTOIRE_INIT_NUM_STEPS 64

void attack_repertoire_init(struct attack_repertoire *a_r);
void attack_repertoire_add_pattern(struct attack_repertoire *a_r, struct attack_pattern *a_p);
void attack_repertoire_add_step(struct attack_repertoire *a_r, struct attack_step *a_s);
void attack_repertoire_destroy(struct attack_repertoire *a_r);
void attack_repertoire_print(struct attack_repertoire *a_r);

struct attack_pattern *attack_repertoire_get_pattern(struct attack_repertoire *a_r, char *attack);
struct attack_step *attack_repertoire_get_attack_step(struct attack_repertoire *a_r, char *step);


#endif
