#include "target_utility.h"

#include "mape_k.h"
#include "knowledge_base.h"
#include "analysis.h"
#include "task_engine.h"

#include <pthread.h>
#include <stdlib.h>
#include "arc_model.h"

#include <stdio.h>
#include <string.h>

#include "attack_repertoire.h"
#include "timestamp.h"


static void enum_to_arc_value(int enum_value, struct arc_value *value)
{
	value->type = V_INT;
	value->v_int = enum_value;
}

void attack_utility_init(struct knowledge_base *k_b)
{
	struct utility_decision *attack_utility = malloc(sizeof(struct utility_decision));
	utility_decision_init(attack_utility,5);
	utility_decision_add_concern(attack_utility, "AttackVector", 0.3, attack_vector_utility);
	utility_decision_add_concern(attack_utility, "AttackComplexity",0.2, attack_complexity_utility);
	utility_decision_add_concern(attack_utility, "PrivilegesRequired", 0.2, privileges_required_utility);
	utility_decision_add_concern(attack_utility, "UserInteraction", 0.1, user_interaction_utility);
	utility_decision_add_concern(attack_utility, "RunningTime", 0.2, running_time_utility);

	struct named_res *t_u_res = malloc(sizeof(struct named_res));
    named_res_init(t_u_res, "AttackUtility", 1, attack_utility);
    knowledge_base_add_res(k_b, t_u_res);
	
}

void attack_utility(struct analysis *a)
{
	timestamp("[","] ");
    printf("[Utility] Attacks [Start]\n");
    
    struct knowledge_base *k_b = &a->az->mape_k->k_b;
    struct attack_repertoire *a_r= knowledge_base_get_res(k_b, "AttackRepertoire");

    struct utility_decision *attack_utility = knowledge_base_get_res(k_b,"AttackUtility");;


	//Attack utility does not change over time, so we only need to do this once really.
	if(attack_utility->num_alternatives == 0)
	{
		
	
		
		for(size_t i=0; i < a_r->num_attack_patterns; i++)
		{
			struct attack_pattern *a_p = &a_r->a_patterns[i];

			struct arc_value attack_values[attack_utility->num_concerns];

			enum_to_arc_value(a_p->a_v, &attack_values[0]);
			enum_to_arc_value(a_p->a_c, &attack_values[1]);
			enum_to_arc_value(a_p->p_r, &attack_values[2]);
			enum_to_arc_value(a_p->u_i, &attack_values[3]);
			enum_to_arc_value(a_p->r_t, &attack_values[4]);

			/*
			double a_v = attack_vector_score(a_p->a_v);
			double a_c = attack_complexity_score(a_p->a_c);
			double p_r = privileges_required_score(a_p->p_r);
			double u_i = user_interaction_score(a_p->u_i);
			double r_t = running_time_score(a_p->r_t);
			*/
			utility_decision_add_option(attack_utility,a_p->name,attack_values,attack_utility->num_concerns);
			//double u = 0.3*a_v+0.2*a_c+0.2*p_r+0.1*u_i+0.2*r_t;
			//utility_score_init(&attack_utility[i],a_p->name,u);
	
		}
		utility_decision_rank(attack_utility);
	}


    for(int i=0;i<attack_utility->num_alternatives;i++)
	{
		printf("%s:%lg\n",attack_utility->options[i].name, attack_utility->options[i].utility);
	}


	timestamp("[","] ");
    printf("[Utility] Attacks [Complete]\n");
	
    analysis_set_status(a, ANALYSIS_DONE);
    a->done(a);
}


void attack_utility_z(struct analysis *a)
{
	timestamp("[","] ");
    printf("[Utility] Attacks [Start]\n");
    
    struct knowledge_base *k_b = &a->az->mape_k->k_b;
    struct attack_repertoire *a_r= knowledge_base_get_res(k_b, "AttackRepertoire");

    struct utility_score *attack_utility;
    attack_utility = malloc(a_r->num_attack_patterns *sizeof(struct utility_score));
    
    for(size_t i=0; i < a_r->num_attack_patterns; i++)
    {
		struct attack_pattern *a_p = &a_r->a_patterns[i];

		double a_v = attack_vector_score(a_p->a_v);
		double a_c = attack_complexity_score(a_p->a_c);
		double p_r = privileges_required_score(a_p->p_r);
		double u_i = user_interaction_score(a_p->u_i);
		double r_t = running_time_score(a_p->r_t);

		double u = 0.3*a_v+0.2*a_c+0.2*p_r+0.1*u_i+0.2*r_t;
		utility_score_init(&attack_utility[i],a_p->name,u);

		printf("[Utility] %s:%g\n",a_p->name,u);
	
    }

    qsort(attack_utility,a_r->num_attack_patterns,sizeof(struct utility_score),compare_utility_scores);


    
    //Cleanup previous loop's stuff
    struct utility_score  *old_utility = knowledge_base_get_res(k_b,"AttackUtilityRanking");
    size_t num_utility_entries = knowledge_base_num_res(k_b,"AttackUtilityRanking");
    if(old_utility)
    {
		for(size_t i = 0; i < num_utility_entries;i++)
		{
			free(old_utility[i].option);
		}
		free(old_utility);
		knowledge_base_rem_res(k_b,"AttackUtilityRanking");
	
    }
    

    struct named_res *a_u_res = malloc(sizeof(struct named_res));
    named_res_init(a_u_res, "AttackUtilityRanking", a_r->num_attack_patterns, attack_utility);
    knowledge_base_add_res(k_b, a_u_res);

	timestamp("[","] ");
    printf("[Utility] Attacks [Complete]\n");
	
    analysis_set_status(a, ANALYSIS_DONE);
    a->done(a);
}
