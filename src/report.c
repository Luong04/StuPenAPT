#include "report.h"
#include <pthread.h>
#include "utility.h"
#include "mape_k.h"
#include "arc_model.h"
#include <stdio.h>
#include <stdlib.h>

void pentest_write_report(struct string_buffer *s_b, struct knowledge_base *k_b, char *filename)
{
    FILE *fp = fopen(filename, "w");
    fprintf(fp,"{\n\"Iterations\" : [");
    fprintf(fp,"%s\n",s_b->b_buff);
    fprintf(fp,"\n]}");
    fclose(fp);
}


void pentest_update_report(struct string_buffer *s_b, struct knowledge_base *k_b, size_t loop)
{

    struct pentest_state *pt_s = knowledge_base_get_res(k_b,"PentestState");
    //TODO: Decide if we should lock here or on every function individually to let attacks succeed in the meantime.
    pthread_mutex_lock(&pt_s->lock);
    if(loop > 1)
    {
	string_buffer_add_str(s_b,",");
    }
    string_buffer_add_fmt(s_b,"\n{\n\t\"PentestStep\" : \"%zu\",",loop);
    target_utility_json_dump(s_b,k_b);
    attack_utility_json_dump(s_b,k_b);
    pentest_state_json_dump_targets(s_b,pt_s,k_b->model);
    pentest_state_json_dump(s_b, pt_s);
    
    pthread_mutex_unlock(&pt_s->lock);

    string_buffer_add_str(s_b,"\n}");
}


void pentest_state_json_dump(struct string_buffer *s_b, struct pentest_state *pt_s)
{ 
    pentest_state_json_dump_attacks_worked(s_b, pt_s);
    pentest_state_json_dump_attacks_failed(s_b, pt_s);
    pentest_state_json_dump_attacks_active(s_b, pt_s);
    pentest_state_json_dump_scans_active(s_b, pt_s);
}


void pentest_state_json_dump_attacks_failed(struct string_buffer *s_b, struct pentest_state *pt_s)
{
    string_buffer_add_str(s_b,"\n\t\"AttacksFailed\" : [");

    for(size_t i = 0 ; i < pt_s->num_failed ; i++)
    {
	struct past_attack *p_a = &pt_s->failed[i];
	string_buffer_add_fmt(s_b,"\n\t\t{\"%s\" : \"%s\"}",p_a->a_p, p_a->target);
	if(i < pt_s->num_failed -1)
	{
	    string_buffer_add_str(s_b,",");
	}
	
	
    }
    
    string_buffer_add_str(s_b,"\n\t],\n");
}

void pentest_state_json_dump_attacks_worked(struct string_buffer *s_b, struct pentest_state *pt_s)
{
    string_buffer_add_str(s_b,"\n\t\"AttacksWorked\" : [");


    for(size_t i = 0 ; i < pt_s->num_worked ; i++)
    {
	struct past_attack *p_a = &pt_s->worked[i];
	string_buffer_add_fmt(s_b,"\n\t\t{\"%s\" : \"%s\"}",p_a->a_p, p_a->target);
	if(i < pt_s->num_worked -1)
	{
	    string_buffer_add_str(s_b,",");
	}
	
    }
    
    string_buffer_add_str(s_b,"\n\t],\n");
}

void pentest_state_json_dump_attacks_active(struct string_buffer *s_b, struct pentest_state *pt_s)
{
    string_buffer_add_str(s_b,"\n\t\"AttacksActive\" : [");

    for(size_t i = 0 ; i < pt_s->num_attacks ; i++)
    {
	struct active_attack *a_a = pt_s->attacks[i];
	string_buffer_add_str(s_b,"\n\t\t\t{");
	string_buffer_add_fmt(s_b,"\n\t\t\t\t\"target\" : \"%s\",",a_a->target);
	string_buffer_add_fmt(s_b,"\n\t\t\t\t\"attack\" : \"%s\",",a_a->attack->name);
	string_buffer_add_fmt(s_b,"\n\t\t\t\t\"progress\" : \"%d%%\"",a_a->cur_step*100/a_a->attack->num_steps);
	if(i < pt_s->num_attacks -1)
	{
	    string_buffer_add_str(s_b,",");
	}
	string_buffer_add_str(s_b,"\n\t\t\t}");
    }
    
    string_buffer_add_str(s_b,"\n\t]\n");
}

void pentest_state_json_dump_targets(struct string_buffer *s_b, struct pentest_state *pt_s, struct arc_model *a_m)
{
    string_buffer_add_str(s_b,"\n\t\"TargetsActive\" : [");

    for(size_t i = 0 ; i < pt_s->num_targets ; i++)
    {
	char *target = pt_s->targets[i];
	string_buffer_add_str(s_b,"\n\t\t\t{");
	string_buffer_add_fmt(s_b,"\n\t\t\t\t\"name\" : \"%s\",",target);
	//TODO: only print exploitation state, utility is already printed, do we care about other properties?
	char es_p_buff[128];
	sprintf(es_p_buff,"%s:EXPLOITATION_STATE",target);
	struct arc_value es_v = arc_model_get_property(a_m, es_p_buff);

	if(es_v.type == V_STRING)
	{
	   string_buffer_add_fmt(s_b,"\n\t\t\t\t\"exploitation_state\" : \"%s\"",es_v.v_str);
	   free(es_v.v_str);
	}
	string_buffer_add_str(s_b,"\n\t\t\t}");
	if(i < pt_s->num_targets -1)
	{
	    string_buffer_add_str(s_b,",");
	}
	
    }
    
    string_buffer_add_str(s_b,"\n\t],\n");
}

void pentest_state_json_dump_scans_active(struct string_buffer *s_b, struct pentest_state *pt_s)
{
    
}

void target_utility_json_dump(struct string_buffer *s_b, struct knowledge_base *k_b)
{
    struct utility_score *target_utility = knowledge_base_get_res(k_b,"TargetUtilityRanking");
    size_t num_t_utility_entries = knowledge_base_num_res(k_b,"TargetUtilityRanking");

    string_buffer_add_str(s_b,"\n\t\"TargetUtility\" : [");

    for(size_t i = 0 ; i < num_t_utility_entries ; i++)
    {
	struct utility_score *u_s = &target_utility[i];
	string_buffer_add_fmt(s_b,"\n\t\t{\"%s\" : %g}",u_s->option,u_s->value);
	if(i < num_t_utility_entries -1)
	{
	    string_buffer_add_str(s_b,",");
	}
	
    }

    string_buffer_add_str(s_b,"\n\t],\n");

}


void attack_utility_json_dump(struct string_buffer *s_b, struct knowledge_base *k_b)
{
    struct utility_score *attack_utility = knowledge_base_get_res(k_b,"AttackUtilityRanking");
    size_t num_a_utility_entries = knowledge_base_num_res(k_b,"AttackUtilityRanking");


    string_buffer_add_str(s_b,"\n\t\"AttackUtility\" : [");

    for(size_t i = 0 ; i < num_a_utility_entries ; i++)
    {
	struct utility_score *u_s = &attack_utility[i];
	string_buffer_add_fmt(s_b,"\n\t\t{\"%s\" : %g}",u_s->option,u_s->value);
	if(i < num_a_utility_entries -1)
	{
	    string_buffer_add_str(s_b,",");
	}
    }
    string_buffer_add_str(s_b,"\n\t],\n");
}
