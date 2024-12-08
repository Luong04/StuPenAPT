#include "attack_plan_generator.h"
#include "task_engine.h"
#include "planner.h"
#include "mape_k.h"
#include "arc_model.h"
#include "pentest_state.h"
#include "utility.h"
#include "timestamp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


//Forwarde declare callbacks
static void target_select(struct task *t);
static void scan_select(struct task *t);
static void attack_select(struct task *t);
static void attack_plan_gen_ctrl(struct task *t);


void attack_plan_gen(struct plan *p)
{
	timestamp("[","] ");
    printf("[Planning] [Initialized]\n");
    struct task *t_target_sel = task_engine_create_task(target_select, NULL,
														&p->pl->mape_k->k_b, p);
    task_engine_add_task(t_target_sel,0);
}


bool target_exploited(struct arc_model *a_m, char *target)
{
    bool res = false;
    
    size_t nbytes = snprintf(NULL,0,"%s:EXPLOITATION_STATE",target);
    char es_buff[nbytes+1];
    snprintf(es_buff,nbytes+1,"%s:EXPLOITATION_STATE",target);
    
	
    struct arc_value v = arc_model_get_property(a_m,es_buff);
    if(v.type != V_STRING)
    {
		res = false;
    }
    else
    {
		if(strcmp(v.v_str, "Escalated") == 0 || strcmp(v.v_str, "Cnd") == 0)
		{
			res = true;
		}
		free(v.v_str);
    }
	
    return res;
}

void target_select(struct task *t)
{
    struct knowledge_base *k_b = task_get_ctx(t);
    struct plan *p = task_get_cb_ctx(t);

    //fetch pentest state
    struct pentest_state *p_s = knowledge_base_get_res(k_b,"PentestState");
    //fetch utility scores
    struct utility_decision *target_utility = knowledge_base_get_res(k_b,"TargetUtility");
    
    

    struct arc_model *a_m = k_b->model;
    
    printf("Pentest Num Active Targes:%d\n",pentest_state_num_targets(p_s));
    size_t num_slots = pentest_state_max_targets(p_s) - pentest_state_num_targets(p_s);
    size_t idx = 0;
    
    
    while(num_slots > 0 && idx < target_utility->num_alternatives)
    {
		char *target = target_utility->options[idx].name;

		printf("Target: %s:%d:%d\n",target, pentest_active_target(p_s,target), target_exploited(a_m,target));
		if(!pentest_active_target(p_s,target) && !target_exploited(a_m,target))
		{
			printf("[Planning] adding %s to active targets\n",target);
			pentest_state_add_target(p_s,target);
			num_slots--;
		}
		idx++;
    }
	
	struct task *t_scan_sel = task_engine_create_task(scan_select, NULL,
														&p->pl->mape_k->k_b, p);
    task_engine_add_task(t_scan_sel,0);    
}

void scan_select(struct task *t)
{
	struct knowledge_base *k_b = task_get_ctx(t);
    struct plan *p = task_get_cb_ctx(t);

	struct arc_model *a_m = k_b->model;
    struct pentest_state *p_s = knowledge_base_get_res(k_b,"PentestState");
    struct scan_repertoire *s_r= knowledge_base_get_res(k_b, "ScanRepertoire");
    //fetch utility scores
    struct utility_decision *scan_utility = knowledge_base_get_res(k_b,"ScanUtility");
    struct utility_decision *target_utility = knowledge_base_get_res(k_b,"TargetUtility");
    size_t num_slots = pentest_state_max_scans(p_s) - pentest_state_num_scans(p_s);


    printf("Pentest num active Scans:%d\n",pentest_state_num_scans(p_s));

	//Try scans with no targets
	for(size_t i = 0 ; i < scan_utility->num_alternatives;i++)
	{
		if(num_slots == 0)
		{
			break;
		}
		char *target = NULL;
		char *scan = scan_utility->options[i].name;
		struct scan *sc = scan_repertoire_get_scan(s_r,scan);
		if(sc && sc->s_t == ST_Network)
		{
		
			bool active = pentest_scan_active(p_s,scan,target);
			bool guard = sc->guard && sc->guard(a_m,target);
			bool done = pentest_scan_done(p_s,scan,target);
			pthread_t ptid = pthread_self();
			uint32_t tid = task_get_id(t);
			printf("%u:%u:%s:%s:%u:%u:%u:\n",tid,ptid,scan, target, active, guard, done);
		
			if(!active && !done && guard && num_slots > 0 )  
			{
				printf("[Planning] adding %s:%s to active scans\n",scan,target);
				struct active_scan *a_s = malloc(sizeof(struct active_scan));;
				active_scan_init(a_s,sc,target);
				pentest_state_add_scan(p_s, a_s);
				num_slots--;	
			}
		}
		
	}


	for(size_t i = 0; i < target_utility->num_alternatives; i++)
	{
		if(num_slots > 0)
		{
			char *target = target_utility->options[i].name;
			for(size_t j = 0 ; j < scan_utility->num_alternatives; j++)
			{
				char *scan = scan_utility->options[j].name;
				struct scan *sc = scan_repertoire_get_scan(s_r,scan);

				if(sc && num_slots > 0 && sc->s_t != ST_Network)
				{
					bool active = pentest_scan_active(p_s,scan,target);
					bool guard = sc->guard && sc->guard(a_m,target);
					bool done = pentest_scan_done(p_s,scan,target);
					pthread_t ptid = pthread_self();
					uint32_t tid = task_get_id(t);
					printf("%u:%u:%s:%s:%u:%u:%u:\n",tid,ptid,scan, target, active, guard, done);
					//Test guard last since it's calling into the plugin.
					if(!active && !done && guard  )  
					{
						
						printf("[Planning] adding %s:%s to active scans\n",scan,target);
						struct active_scan *a_s = malloc(sizeof(struct active_scan));;
						active_scan_init(a_s,sc,target);
						pentest_state_add_scan(p_s, a_s);
						num_slots--;
					}
				}

				if(num_slots == 0)
				{
					break;
				}
				
			}
		}

		if(num_slots == 0)
		{
			break;
		}
	}
	
	
    /*
    //While we still are below the concurrency threshold and have things we haven't tested:
    while(num_slots > 0 && s_idx < scan_utility->num_alternatives && t_idx < target_utility->num_alternatives)
    {
		
		char *scan = scan_utility->options[s_idx].name;
		char *target = target_utility->options[t_idx].name;
		//struct attack_pattern *a_p = attack_repertoire_get_pattern(a_r, attack);
		struct scan *sc = scan_repertoire_get_scan(s_r,scan);
		
		if(sc)
		{
			
			bool active = pentest_scan_active(p_s,scan,target);
			bool guard = sc->guard && sc->guard(a_m,target);
			bool done = pentest_scan_done(p_s,scan,target);
			printf("%s:%s:%u:%u:%u:\n",scan, target, active, guard, done);
			//Test guard last since it's calling into the plugin.
			if(!active && !done && guard )  
			{
				
				printf("[Planning] adding %s:%s to active scans\n",scan,target);
				struct active_scan *a_s = malloc(sizeof(struct active_scan));;
				active_scan_init(a_s,sc,target);
				pentest_state_add_scan(p_s, a_s);
				num_slots--;
				//What is the condition to move to the next target?
				if(target_exploited(a_m,target))
				{
					s_idx++;
				}
			}
			s_idx++;
		}
		
	}
	*/
	

	struct task *t_attack_sel = task_engine_create_task(attack_select, attack_plan_gen_ctrl,
														&p->pl->mape_k->k_b, p);
    task_engine_add_task(t_attack_sel,0);
}


void attack_select(struct task *t)
{
    struct knowledge_base *k_b = task_get_ctx(t);
    struct arc_model *a_m = k_b->model;
    struct pentest_state *p_s = knowledge_base_get_res(k_b,"PentestState");
    struct attack_repertoire *a_r= knowledge_base_get_res(k_b, "AttackRepertoire");
    //fetch utility scores
    struct utility_decision *attack_utility = knowledge_base_get_res(k_b,"AttackUtility");
    struct utility_decision *target_utility = knowledge_base_get_res(k_b,"TargetUtility");
    size_t num_slots = pentest_state_max_attacks(p_s) - pentest_state_num_attacks(p_s);

    printf("Pentest num active Attacks:%d\n",pentest_state_num_attacks(p_s));
	

	/*
    //While we still are below the concurrency threshold and have things we haven't tested:
    while(num_slots > 0 && a_idx <attack_utility->num_alternatives && t_idx < target_utility->num_alternatives)
    {
		
	char *attack = attack_utility->options[a_idx].name;
		char *target = target_utility->options[t_idx].name;
		struct attack_pattern *a_p = attack_repertoire_get_pattern(a_r, attack);
		
		if(a_p)
		{
			struct attack_step *a_s = attack_repertoire_get_attack_step(a_r,a_p->a_steps[0]);
			if(a_s)
			{
				bool active = pentest_attack_active(p_s,attack,target);
				bool guard = a_s->guard && a_s->guard(a_m,target);
				bool failed = pentest_attack_failed(p_s,attack,target);
				bool worked = pentest_attack_worked(p_s,attack,target);
				printf("%s:%s:%u:%u:%u:%u\n",attack, target, active, guard, failed, worked);
				//Test guard last since it's calling into the plugin.
				if(!active &&  !failed && !worked && guard )  
				{
		    
					printf("[Planning] adding %s:%s to active attacks\n",attack,target);
					struct active_attack *a_a = malloc(sizeof(struct active_attack));
					active_attack_init(a_a,a_p,target);
					pentest_state_add_attack(p_s, a_a);
					num_slots--;
					//What is the condition to move to the next target?
					if(target_exploited(a_m,target))
					{
						t_idx++;
					}
				}
			}
		}
		a_idx++;
    }
	*/
	for(size_t i = 0 ; i < target_utility->num_alternatives;i++)
	{
		
		if(num_slots == 0)
		{
			break;
		}
		
		char *target = target_utility->options[i].name;
		for(size_t j = 0 ; j < attack_utility->num_alternatives;j++)
		{
			char *attack = attack_utility->options[j].name;
			struct attack_pattern *a_p = attack_repertoire_get_pattern(a_r, attack);
			if(a_p && num_slots > 0)
			{
				struct attack_step *a_s = attack_repertoire_get_attack_step(a_r,a_p->a_steps[0]);
				if(a_s)
				{
					bool active = pentest_attack_active(p_s,attack,target);
					bool guard = a_s->guard && a_s->guard(a_m,target);
					bool failed = pentest_attack_failed(p_s,attack,target);
					bool worked = pentest_attack_worked(p_s,attack,target);
					pthread_t ptid = pthread_self();
					uint32_t tid = task_get_id(t);
					printf("%u:%u:%s:%s:%u:%u:%u:%u\n",tid,ptid,attack, target, active, guard, failed, worked);
					//Test guard last since it's calling into the plugin.
					if(!active &&  !failed && !worked && guard )  
					{
						
						printf("[Planning] adding %s:%s to active attacks\n",attack,target);
						struct active_attack *a_a = malloc(sizeof(struct active_attack));
						active_attack_init(a_a,a_p,target);
						pentest_state_add_attack(p_s, a_a);
						num_slots--;
					}
				}
			}
		}

		if(num_slots == 0)
		{
			break;
		}
		
	}
}

static void attack_plan_gen_ctrl(struct task *t)
{
    struct plan *p = task_get_cb_ctx(t);
	timestamp("[","] ");
    printf("[Planning] [Complete]\n");
    plan_set_status(p, PLAN_DONE);
    p->done(p);
}






void target_select_z(struct task *t)
{
    struct knowledge_base *k_b = task_get_ctx(t);
    struct plan *p = task_get_cb_ctx(t);

    //fetch pentest state
    struct pentest_state *p_s = knowledge_base_get_res(k_b,"PentestState");
    //fetch utility scores
    struct utility_score *target_utility = knowledge_base_get_res(k_b,"TargetUtilityRanking");
    size_t num_utility_entries = knowledge_base_num_res(k_b,"TargetUtilityRanking");
    

    struct arc_model *a_m = k_b->model;
    
    printf("Pentest Num Active Targes:%d\n",pentest_state_num_targets(p_s));
    size_t num_slots = pentest_state_max_targets(p_s) - pentest_state_num_targets(p_s);
    size_t idx = 0;
    
    
    while(num_slots > 0 && idx < num_utility_entries)
    {
		char *target = target_utility[idx].option;

		printf("Target: %s:%d:%d\n",target, pentest_active_target(p_s,target), target_exploited(a_m,target));
		if(!pentest_active_target(p_s,target) && !target_exploited(a_m,target))
		{
			printf("[Planning] adding %s to active targets\n",target);
			pentest_state_add_target(p_s,target);
			num_slots--;
		}
		idx++;
    }

    struct task *t_attack_sel = task_engine_create_task(attack_select, attack_plan_gen_ctrl,
														&p->pl->mape_k->k_b, p);
    task_engine_add_task(t_attack_sel,0);
}


void attack_select_z(struct task *t)
{
    struct knowledge_base *k_b = task_get_ctx(t);
    struct arc_model *a_m = k_b->model;
    struct pentest_state *p_s = knowledge_base_get_res(k_b,"PentestState");
    struct attack_repertoire *a_r= knowledge_base_get_res(k_b, "AttackRepertoire");
    //fetch utility scores
    struct utility_score *attack_utility = knowledge_base_get_res(k_b,"AttackUtilityRanking");
    size_t num_a_utility_entries = knowledge_base_num_res(k_b,"AttackUtilityRanking");
    struct utility_score *target_utility = knowledge_base_get_res(k_b,"TargetUtilityRanking");
    size_t num_t_utility_entries = knowledge_base_num_res(k_b,"TargetUtilityRanking");
    size_t num_slots = pentest_state_max_attacks(p_s) - pentest_state_num_attacks(p_s);
    //attack utility index
    size_t a_idx = 0;
    //target utility index
    size_t t_idx = 0;

    printf("Pentest num active Attacks:%d\n",pentest_state_num_attacks(p_s));
    
    //While we still are below the concurrency threshold and have things we haven't tested:
    while(num_slots > 0 && a_idx < num_a_utility_entries && t_idx < num_t_utility_entries)
    {
		
		char *attack = attack_utility[a_idx].option;
		//FIX:Get this from the target utility instead
		char *target = target_utility[t_idx].option;
		struct attack_pattern *a_p = attack_repertoire_get_pattern(a_r, attack);
		

	
	
	
		if(a_p)
		{
			struct attack_step *a_s = attack_repertoire_get_attack_step(a_r,a_p->a_steps[0]);
			if(a_s)
			{
				bool active = pentest_attack_active(p_s,attack,target);
				bool guard = a_s->guard && a_s->guard(a_m,target);
				bool failed = pentest_attack_failed(p_s,attack,target);
				bool worked = pentest_attack_worked(p_s,attack,target);
				printf("%s:%s:%u:%u:%u:%u\n",attack, target, active, guard, failed, worked);
				//Test guard last since it's calling into the plugin.
				if(!active &&  !failed && !worked && guard )  
				{
		    
					printf("[Planning] adding %s:%s to active attacks\n",attack,target);
					struct active_attack *a_a = malloc(sizeof(struct active_attack));;
					active_attack_init(a_a,a_p,target);
					pentest_state_add_attack(p_s, a_a);
					num_slots--;
					//What is the condition to move to the next target?
					if(target_exploited(a_m,target))
					{
						t_idx++;
					}
				}
			}
		}
		a_idx++;
    }
}

