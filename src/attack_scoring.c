#include "attack_scoring.h"

#include <string.h>


double attack_vector_utility(struct arc_value *a_v)
{
	return attack_vector_score(a_v->v_int);
}

double attack_complexity_utility(struct arc_value *a_v)
{
	return attack_complexity_score(a_v->v_int);
}

double privileges_required_utility(struct arc_value *a_v)
{
	return privileges_required_score(a_v->v_int);
}

double user_interaction_utility(struct arc_value *a_v)
{
	return user_interaction_score(a_v->v_int);
}

double running_time_utility(struct arc_value *a_v)
{
	return running_time_score(a_v->v_int);
}



double attack_vector_score(enum ATTACK_VECTOR a_v)
{
    static const double a_v_score_tbl[AV_Size] = {1.0, 0.73, 0.64, 0.23};
    if(a_v >= AV_Size || a_v < 0)
    {
	return 0;
    }
    return a_v_score_tbl[a_v];
}

double attack_complexity_score(enum ATTACK_COMPLEXITY a_c)
{
    static const double a_c_score_tbl[AC_Size] = {1.0, 0.57};
    if(a_c >= AC_Size || a_c < 0)
    {
	return 0;
    }
    return a_c_score_tbl[a_c];
}

double privileges_required_score(enum PRIVILEGES_REQUIRED p_r)
{
    static const double p_r_score_tbl[PR_Size] = {1.0, 0.73, 0.32};
    if(p_r >= PR_Size || p_r < 0)
    {
	return 0;
    }
    return p_r_score_tbl[p_r];
}

double user_interaction_score(enum USER_INTERACTION u_i)
{
    static const double u_i_score_tbl[UI_Size] = {1.0, 0.73};
    if(u_i >= UI_Size || u_i < 0)
    {
	return 0;
    }
    return u_i_score_tbl[u_i];
}


double running_time_score(enum RUNNING_TIME r_t)
{
    static const double r_t_score_tbl[RT_Size] = {1.0, 0.73, 0.31, 0.12};
    if(r_t >= RT_Size || r_t < 0)
    {
	return 0;
    }
    return r_t_score_tbl[r_t];
}


enum ATTACK_VECTOR str_to_attack_vector(char *a_v_str)
{
    if(strcmp(a_v_str,"Network") == 0)
    {
	return AV_Network;
    }
    else if(strcmp(a_v_str,"Adjacent") == 0)
    {
	return AV_Adjacent;
    }
    else if(strcmp(a_v_str,"Local") == 0)
    {
	return AV_Local;
    }
    else if(strcmp(a_v_str,"Physical") == 0)
    {
	return AV_Physical;
    }
    else
    {
	return AV_Size;
    }
    
}
enum ATTACK_COMPLEXITY str_to_attack_complexity(char *a_c_str)
{
    if(strcmp(a_c_str,"Low") == 0)
    {
	return AC_Low;
    }
    else if(strcmp(a_c_str,"High") == 0)
    {
	return AC_High;
    }
    else
    {
	return AC_Size;
    }
}
enum PRIVILEGES_REQUIRED str_to_privileges_required(char *p_r_str)
{

    if(strcmp(p_r_str,"None") == 0)
    {
	return PR_None;
    }
    else if(strcmp(p_r_str,"Low") == 0)
    {
	return PR_Low;
    }
    else if(strcmp(p_r_str,"High") == 0)
    {
	return PR_High;
    }
    else
    {
	return PR_Size;
    }
    
}

enum USER_INTERACTION str_to_user_interaction(char *u_i_str)
{

    if(strcmp(u_i_str,"None") == 0)
    {
	return UI_None;
    }
    else if(strcmp(u_i_str,"Required") == 0)
    {
	return UI_Required;
    }
    else
    {
	return UI_Size;
    }
    
}

enum RUNNING_TIME str_to_running_time(char *r_t_str)
{

    if(strcmp(r_t_str,"Quick") == 0)
    {
	return RT_Quick;
    }
    else if(strcmp(r_t_str,"Medium") == 0)
    {
	return RT_Medium;
    }
    else if(strcmp(r_t_str,"Long") == 0)
    {
	return RT_Long;
    }
    else if(strcmp(r_t_str,"Guessing") == 0)
    {
	return RT_Guessing;
    }
    else
    {
	return RT_Size;
    }
    
}
