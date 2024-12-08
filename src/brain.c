#define _XOPEN_SOURCE 600


#include "brain.h"
#include <stdio.h>
#include "arc_model.h"
#include "utility.h"
#include "pentest_state.h"
#include "attack_step_execution.h"
#include "report.h"
#include <stdlib.h>
#include <string.h>
#include "scan_probe.h"
/*
  This is a bit ugly but is probably better than adding locks to the mape_k stuff.
  Well at least until a thread sanitizer proves me wrong.
*/

static struct pentest_brain *brain = NULL;


void pentest_brain_init(char *target_dir)
{

    if(!brain)
    {	
		brain = malloc(sizeof(struct pentest_brain));
    }

	
	
	
    pthread_mutex_init(&brain->lock,NULL);
    //This is getting to be a bit too much state, maybe we want to make a struct out of it.
    brain->terminate = false;
    brain->loop = 0;
    size_t report_buff_size = 4*1024*1024;
    //Just allocate some memory for the reporting and continue;
    brain->pentest_report_buff = malloc(report_buff_size);
    brain->sb_pentest_report = malloc(sizeof(struct string_buffer));
    string_buffer_init(brain->sb_pentest_report,brain->pentest_report_buff,report_buff_size);
    brain->target_dir = target_dir;
}

void pentest_brain_cleanup()
{
    free(brain->pentest_report_buff);
    free(brain->sb_pentest_report);
    pthread_mutex_destroy(&brain->lock);
    free(brain);
	
}



bool viable_attacks_exist(struct knowledge_base *k_b)
{

    //UGLY HACK: Fetch targets from the target utility to avoid the architectural model lookup.
    struct utility_score *target_utility = knowledge_base_get_res(k_b,"TargetUtilityRanking");
    size_t num_utility_entries = knowledge_base_num_res(k_b,"TargetUtilityRanking");
    struct pentest_state *pt_s = knowledge_base_get_res(k_b,"PentestState");
    struct attack_repertoire *a_r = knowledge_base_get_res(k_b,"AttackRepertoire");
    
    //For every attack 
    for(size_t i = 0 ; i < a_r->num_attack_patterns;i++)
    {
		struct attack_pattern *a_p = &a_r->a_patterns[i];
		char *attack = a_p->name;
		//For every target
		for(size_t j = 0 ; j < num_utility_entries ; j++)
		{
			char *target = target_utility[j].option;

			//If the attack has not failed, or worked or is active try to see if it could execute.
			bool failed = pentest_attack_failed(pt_s,attack,target);
			bool worked = pentest_attack_worked(pt_s,attack,target);
			bool active = pentest_attack_active(pt_s,attack,target);
			if(!failed && !worked && !active)
			{
				/* I believe this avoids a data race
				   since the attack step will only be accessed if the attack is not active
				*/
				struct attack_step *a_s = attack_repertoire_get_attack_step(a_r, a_p->a_steps[0]);
				/*TODO: This also is kind ugly see if I can maybe wrap it around a function */
				if(a_s && a_s->guard && a_s->guard(k_b->model,target))
				{
					return true;
				}
		
			}
		}
	
    }

    return false;
}

bool pentest_loop_done()
{
    bool res = false;
    pthread_mutex_lock(&brain->lock);
    res = brain->terminate;
    pthread_mutex_unlock(&brain->lock);

    return res;
    
}

void refresh_active_targets(struct mape_k *mape_k)
{
	//For every known component, if it has not been scanned for new hosts add a scan for new hosts.
    struct knowledge_base *k_b = &mape_k->k_b;
    struct arc_model *a_m = k_b->model;
    struct arc_system *sys = a_m->sys;
    struct scan_repertoire  *s_r =  knowledge_base_get_res(k_b,"ScanRepertoire");
    struct pentest_state *pt_s = knowledge_base_get_res(&mape_k->k_b,"PentestState");
    pthread_mutex_lock(&a_m->sys_lock);
	
    for(size_t i = 0; i < sys->num_components; i++)
    {
		struct arc_component *c = sys->components[i];
		if(c)
		{
			struct arc_property *e_s = comp_find_property(c, "EXPLOITATION_STATE");
			if(!e_s)
			{
				fprintf(stderr, "[MAPE] Panic exiting no exploitation state property for %s\n",c->comp_name);
				exit(-1);
			}
			else
			{
				if(strcmp(e_s->v_str, "Escalated") == 0 || strcmp(e_s->v_str, "CnC") == 0)
				{
					pentest_state_rem_target(pt_s,c->comp_name);
				}
			}
		}
    }
    pthread_mutex_lock(&a_m->sys_lock);
}

void pentest_loop_start(struct mape_k *mape_k)
{

    //This technically should never run twice but just to be "safe"
    pthread_mutex_lock(&brain->lock);
    
    struct pentest_state *pt_s = knowledge_base_get_res(&mape_k->k_b,"PentestState");
    //A bit ugly but here's our reporting.
    if(brain->loop >= 1)
    {
		pentest_update_report(brain->sb_pentest_report,&mape_k->k_b,brain->loop);
    }
    
	brain->loop++;
    
    
    size_t num_active_attacks = pentest_state_num_attacks(pt_s);
	size_t num_active_scans = pentest_state_num_scans(pt_s);
	printf("Scans: %zu, attacks: %zu\n",num_active_scans, num_active_attacks);

    /*TODO:Determine actual condition to return to monitoring.
      Possibly has to do with active probes, scans and attacks
      Also possibly check if any attacks are possible that we did not try.
    */
    
    /*
      This is a bit of a hack here too:
      If there are probes remaining,
      we do not check if we have attacks we have not tried against  all targets,
      since targets might be added by the scan.
      Note this allows the hack in viable_attacks_exist that checks targets by utility
      to avoid locking. //That hack is pretty ugly...
    */

    /*
      This will loop even though we have gained root on everything.
      Take in a strategy function pointer later, definitely need a struct for all of this.
    */

    //refresh_active_targets(mape_k);
    //schedule_scans(mape_k);
	
	
    if(brain->loop == 4)
    {
		fprintf(stderr,"[MAPE] Terminating!\n");
		size_t report_path_len = strlen(brain->target_dir)+strlen("Report.json")+2;
		char *report_path = malloc(report_path_len);
		snprintf(report_path,report_path_len,"%s/Report.json",brain->target_dir);
		pentest_write_report(brain->sb_pentest_report, &mape_k->k_b, report_path);
		free(report_path);
		brain->terminate = true;
    }
    else if(monitor_has_probes_waiting(&mape_k->m))
    {
		printf("[MAPE] Loop %zu Start...\n",brain->loop);
		monitor_run(&mape_k->m);
    }
    else if(num_active_attacks > 0 || viable_attacks_exist(&mape_k->k_b) || brain->loop==1) 
    {
		//This is duplicated but maintains structure and avoids having an extra if
		printf("[MAPE] Loop %zu Start...\n",brain->loop);
		// Check if viable scans exist too?
		run_analyzer(&mape_k->a);
    }
    
    pthread_mutex_unlock(&brain->lock);
}

bool analysis_needed(struct knowledge_base *k_b)
{

    struct arc_model *a_m = k_b->model;

    bool adapt = false;
    pthread_mutex_lock(&a_m->sys_lock);

    if(a_m->sys->num_components > 0)
    {
		adapt = true;
    }
    pthread_mutex_unlock(&a_m->sys_lock);
    
    return adapt;
}

void *utility_update_done(struct knowledge_base *k_b)
{
 
    void *a_u_r =  knowledge_base_get_res(k_b,"AttackUtility");
    void *t_u_r =  knowledge_base_get_res(k_b,"TargetUtility");

    if(a_u_r && t_u_r)
    {
		return t_u_r;
    }
    
    return NULL;
}

void monitor_control(struct probe *p)
{

    if(p)
    {
	
		struct monitor *m = probe_get_monitor(p);
		struct mape_k *a_c = m->mape_k;
		struct knowledge_base * k_b = &a_c->k_b;
		struct analyzer * a = &a_c->a;

		struct pentest_state *p_s = knowledge_base_get_res(k_b,"PentestState");
		
		//struct attack_step_exec_ctx *ase_c = cmd_get_ctx(cmd);
		//pentest_state_active_attack_update(p_s,ase_c->a_a);
		struct scan_probe *s_p =  probe_get_ctx(p);
		struct active_scan *a_s = s_p->a_s;
		pentest_state_active_scan_update(p_s,a_s);
		

		pthread_mutex_lock(&brain->lock);
		monitor_rem_probe(m,p);
		pthread_mutex_unlock(&brain->lock);
	
		//TODO: Determine conditions to move to analysis, most probable condition: any probe has finished and we have progress in some metric
		if(monitor_all_probes_done(m))
		{
			bool adapt = m->goal_eval(k_b);
			if(adapt)
			{
				run_analyzer(a);
			}
			else
			{
				printf("[MAPE] No adaptation needed, starting next loop...\n");
				pentest_loop_start(a_c);
			}
		}
    }
}


void analysis_control(struct analysis *ana)
{

    struct analyzer *a = analysis_get_analyzer(ana);
    struct mape_k *a_c = a->mape_k;
    struct monitor *m = &a_c->m;
    struct knowledge_base * k_b = &a_c->k_b;
    struct planner *p = &a_c->p;
    //struct arc_model *a_m = k_b->model;


    if(analyzer_all_analyses_done(a))
    {

		void *strat = a->strategy_sel(k_b);
	
		if(strat)
		{

			//struct utility_score *b_t = strat;
			if(p)
			{
				run_planner(p);
			}
	    
		}
		else
		{
			printf("[MAPE] Could not find an adaptation strategy that improves state, starting new loop\n");
			pentest_loop_start(a_c);
			/*
			  if(m->goal_eval(k_b))
			  {
			  monitor_run(m);
			  }
			*/
		}
    }
}

void planning_control(struct plan *p)
{

    struct planner *pl = plan_get_planner(p);
    struct executor *ex = &pl->mape_k->e;
    struct monitor  *m = &pl->mape_k->m;
    struct knowledge_base *k_b = &pl->mape_k->k_b;
    struct mape_k *a_c = pl->mape_k;
	struct arc_model *a_m = k_b->model;
    
    if(planner_all_plans_created(pl))
    {
		if(ex)
		{

			struct pentest_state *p_s = knowledge_base_get_res(k_b,"PentestState");
			
			//For every active scan create a probe
			struct scan_repertoire *s_r = knowledge_base_get_res(k_b,"ScanRepertoire");
			for(size_t i = 0; i < pentest_state_num_scans(p_s); i++)
			{
				struct active_scan *a_s = pentest_state_get_scan(p_s,i);
				struct scan *scan = scan_repertoire_get_scan(s_r,a_s->scan->name);
				if(a_s->scan_state == AS_WAIT)
				{
					struct scan_probe *scan_probe = malloc(sizeof(struct scan_probe));
					scan_probe_setup(scan_probe,a_s);
				
					struct probe *p_scan = malloc(sizeof(struct probe));
					init_probe(p_scan, scan_probe_init, scan_probe_run,
							   scan_probe_destroy, monitor_control, scan_probe);
					
					pthread_mutex_lock(&brain->lock);
					monitor_add_probe(m, p_scan);
					pthread_mutex_unlock(&brain->lock);
					
				}
			}


			
			//For every active attack create an execution command			
			struct attack_repertoire *a_r = knowledge_base_get_res(k_b,"AttackRepertoire");
			for(size_t i = 0; i < pentest_state_num_attacks(p_s) ;i++)
			{
				struct active_attack *a_a = pentest_state_get_attack(p_s,i);
				struct attack_step *a_s = attack_repertoire_get_attack_step(a_r, a_a->attack->a_steps[a_a->cur_step]);
				//If the current step is ready for execution		
				if(a_a->cur_step_state == AAS_WAIT)
				{
					struct attack_step_exec_ctx *ase_c = malloc(sizeof(struct attack_step_exec_ctx));
					attack_step_exec_init(ase_c,a_s,a_a);
		    
					struct exec_cmd *cmd = malloc(sizeof(struct exec_cmd));
					exec_cmd_init(cmd, attack_step_exec, execution_control, ase_c);

					//This should only happen from one thread at a time but just to be safe.
					pthread_mutex_lock(&brain->lock);
					executor_add_command(ex, cmd);
					pthread_mutex_unlock(&brain->lock);
				}
			}
			
			
			//Do these need to be monitor functions?
			if(ex->num_cmds == 0)
			{
				fprintf(stderr,"[MAPE] No commands queued for execution, starting new loop!\n");
		
				pentest_loop_start(a_c);
			}
			else
			{
				if(ex->num_cmds > 0)
				{
					pthread_mutex_lock(&brain->lock);
					executor_run(ex);
					pthread_mutex_unlock(&brain->lock);
				}
				/*
				if(m->num_probes > 0)
				{
					pthread_mutex_lock(&brain->lock);
					monitor_run(m);
					pthread_mutex_unlock(&brain->lock);
					}*/
			}

	    
		}
    }
	else
	{
		printf("Thread dies here!\n");
	}
	    
}

void execution_control(struct exec_cmd *cmd)
{
    struct executor *ex = cmd_get_executor(cmd);
    struct monitor *m = &ex->mape_k->m;
    struct analyzer *a = &ex->mape_k->a;
    struct knowledge_base *k_b = &ex->mape_k->k_b;
    struct mape_k *a_c = ex->mape_k;

    struct pentest_state *p_s = knowledge_base_get_res(k_b,"PentestState");
    struct attack_step_exec_ctx *ase_c = cmd_get_ctx(cmd);
    pentest_state_active_attack_update(p_s,ase_c->a_a);
    
    /*Fix this */
    pthread_mutex_lock(&brain->lock);
    //Only allowed to remove things from the executor in here under the lock
    executor_rem_command(ex,cmd);
    //also cleanup ase_c later
    pthread_mutex_unlock(&brain->lock);

    if(executor_all_cmds_done(ex))
    {
		pentest_loop_start(a_c);
    }
}
