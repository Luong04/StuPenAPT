#include "execution_command.h"
#include "attack_repertoire.h"


#ifndef ATTACK_STEP_EXECUTION_H
#define ATTACK_STEP_EXECUTION_H





struct attack_step_exec_ctx
{
    struct attack_step *a_s;
    struct active_attack *a_a;
};


void attack_step_exec_init(struct attack_step_exec_ctx *ase_c,
			   struct attack_step *a_s,
			   struct active_attack *a_a);

void attack_step_exec(struct exec_cmd *cmd);

#endif
