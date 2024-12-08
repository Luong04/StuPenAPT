#include "target_utility.h"
#include "target_scoring.h"

#include "mape_k.h"
#include "knowledge_base.h"
#include "analysis.h"
#include "task_engine.h"

#include <pthread.h>
#include <stdlib.h>
#include "arc_model.h"

#include <stdio.h>
#include <string.h>



//Doesn't malloc memory instead it passes ownership if necessary
static void property_to_value(struct arc_property *p, struct arc_value *v)
{
	if(p->type == P_INT)
	{
		v->type = V_INT;
		v->v_int = p->v_int;
	}
	else if(p->type == P_DOUBLE)
	{
		v->type = V_DOUBLE;
		v->v_dbl = p->v_dbl;
	}
	else if(p->type == P_STRING)
	{
		v->type = V_STRING;
		v->v_str = p->v_str;
	}
}

void target_utility_init(struct knowledge_base *k_b)
{
	struct utility_decision *target_utility = malloc(sizeof(struct utility_decision));
	utility_decision_init(target_utility,4);
	utility_decision_add_concern(target_utility, "NumServices", 0.2, num_services_utility);
	utility_decision_add_concern(target_utility, "NumVulnerabilities",0.2, num_vulnerabilities_utility);
	utility_decision_add_concern(target_utility, "NumConnections", 0.2, num_connections_utility);
	utility_decision_add_concern(target_utility, "ExploitationState", 0.4, exploitation_state_utility);

	struct named_res *t_u_res = malloc(sizeof(struct named_res));
    named_res_init(t_u_res, "TargetUtility", 1, target_utility);
    knowledge_base_add_res(k_b, t_u_res);
	
}



void target_utility(struct analysis *a)
{
	struct knowledge_base *k_b = &a->az->mape_k->k_b;
    struct arc_model *a_m = k_b->model;
    struct arc_system *sys = a_m->sys;

	printf("[Utility] Targets [Start]\n");
    
    struct utility_decision *target_utility = knowledge_base_get_res(k_b,"TargetUtility");;

	if(target_utility->num_alternatives > 0)
	{
		utility_decision_clear_options(target_utility);
	}
	
	pthread_mutex_lock(&a_m->sys_lock);
	for(size_t  i=0; i < sys->num_components;i++)
    {

		struct arc_value target_values[target_utility->num_concerns];
		
		struct arc_component *c = sys->components[i];
		struct arc_property* n_s_p = comp_find_property(c,"NUM_SERVICES");
		property_to_value(n_s_p,&target_values[0]);
		struct arc_property* n_v_p = comp_find_property(c,"NUM_VULNERABILITIES");
		property_to_value(n_v_p,&target_values[1]);
		struct arc_property* n_c_p = comp_find_property(c,"NUM_CONNECTIONS");
		property_to_value(n_c_p,&target_values[2]);
		struct arc_property* e_s_p = comp_find_property(c,"EXPLOITATION_STATE");
		property_to_value(e_s_p,&target_values[3]);

		
	
		utility_decision_add_option(target_utility,c->comp_name,target_values,target_utility->num_concerns);
	
    }
	pthread_mutex_unlock(&a_m->sys_lock);

	utility_decision_rank(target_utility);

	for(int i=0;i<target_utility->num_alternatives;i++)
	{
		printf("%s:%lg\n",target_utility->options[i].name, target_utility->options[i].utility);
	}


    printf("[Utility] Targets [Complete]\n");
    analysis_set_status(a, ANALYSIS_DONE);

	
	
}

void target_utility_z(struct analysis *a)
{
   
    struct knowledge_base *k_b = &a->az->mape_k->k_b;
    struct arc_model *a_m = k_b->model;
    struct arc_system *sys = a_m->sys;

    printf("[Utility] Targets [Start]\n");
    pthread_mutex_lock(&a_m->sys_lock);
    struct utility_score *target_utility;
    target_utility = malloc(sys->num_components *sizeof(struct utility_score));
    for(size_t  i=0; i < sys->num_components;i++)
    {
		struct arc_component *c = sys->components[i];
		struct arc_property* n_s_p = comp_find_property(c,"NUM_SERVICES");
		struct arc_property* n_v_p = comp_find_property(c,"NUM_VULNERABILITIES");
		struct arc_property* n_c_p = comp_find_property(c,"NUM_CONNECTIONS");
		struct arc_property* e_s_p = comp_find_property(c,"EXPLOITATION_STATE");
	
		double n_s = num_services_score(n_s_p->v_int);
		double n_v = num_vulnerabilities_score(n_v_p->v_int);
		double n_c = num_connections_score(n_c_p->v_int);
		double e_s = exploitation_state_score(str_to_exploitation_state(e_s_p->v_str));
		double u = 0.2*n_s + 0.2*n_v + 0.2*n_c+ 0.4*e_s;
		utility_score_init(&target_utility[i],c->comp_name,u);
		printf("[Utility] %s:%g\n",c->comp_name,u);
	
    }
    pthread_mutex_unlock(&a_m->sys_lock);
    qsort(target_utility,sys->num_components,sizeof(struct utility_score),compare_utility_scores);
    
    //Cleanup previous loop's utility
    struct utility_score  *old_utility = knowledge_base_get_res(k_b,"TargetUtilityRanking");
    size_t num_utility_entries = knowledge_base_num_res(k_b,"TargetUtilityRanking");
    if(old_utility)
    {
		for(size_t i = 0; i < num_utility_entries;i++)
		{
			free(old_utility[i].option);
		}
		free(old_utility);
	
		knowledge_base_rem_res(k_b,"TargetUtilityRanking");
	
    }
    
    
    struct named_res *t_u_res = malloc(sizeof(struct named_res));
    named_res_init(t_u_res, "TargetUtilityRanking", sys->num_components, target_utility);
    knowledge_base_add_res(k_b, t_u_res);
    printf("[Utility] Targets [Complete]\n");
    analysis_set_status(a, ANALYSIS_DONE);
}
