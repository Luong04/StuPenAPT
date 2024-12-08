#include <stdio.h>

#include "attack_step_execution.h"
#include "executor.h"
#include "mape_k.h"
#include "pentest_state.h"



void attack_step_exec_init(struct attack_step_exec_ctx *ase_c,
			   struct attack_step *a_s,
			   struct active_attack *a_a)
{
    ase_c->a_s = a_s;
    ase_c->a_a = a_a;
}

static void attack_step_run(struct task *t);
static void attack_step_ctrl(struct task *t);

void attack_step_exec(struct exec_cmd *cmd)
{
    struct task *t_as_run = task_engine_create_task(attack_step_run,attack_step_ctrl,cmd,cmd);   
    task_engine_add_task(t_as_run,0);
}

static void attack_step_run(struct task *t)    
{

    struct exec_cmd *cmd = task_get_ctx(t);
    struct attack_step_exec_ctx *ase_c= cmd_get_ctx(cmd);
    struct executor *ex = cmd_get_executor(cmd);
    struct arc_model *a_m = ex->mape_k->k_b.model;
    
    struct attack_step *a_s = ase_c->a_s;
	struct active_attack *a_a = ase_c->a_a;
    char *target = ase_c->a_a->target;

    bool res = false;
    printf("[%s:%s] Initializing\n",ase_c->a_s->name,ase_c->a_a->target); 
    if(a_s->guard(a_m,target))
    {
		pthread_mutex_lock(&cmd->lock);
		//set the state to running
		a_a->cur_step_state = AAS_RUN;
		pthread_mutex_unlock(&cmd->lock);
		res = a_s->run(a_m,target);
		if(res)
		{
			printf("[%s:%s] Success\n",ase_c->a_s->name,ase_c->a_a->target);
		}
		else
		{
			printf("[%s:%s] Failed\n",ase_c->a_s->name,ase_c->a_a->target);
		}
		
    }
	pthread_mutex_lock(&cmd->lock);
    a_a->cur_res = res;
	pthread_mutex_unlock(&cmd->lock);
    
}

static void attack_step_ctrl(struct task *t)
{
    struct exec_cmd *cmd = task_get_ctx(t);
    struct attack_step_exec_ctx *ase_c= cmd_get_ctx(cmd);
    struct active_attack *a_a = ase_c->a_a;
    a_a->cur_step_state = AAS_DONE;    
    cmd_set_status(cmd,CMD_DONE);
    cmd->done(cmd);
}


