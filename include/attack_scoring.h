#include "arc_value.h"

#ifndef ATTACK_SCORING_H
#define ATTACK_SCORING_H


enum ATTACK_VECTOR
{
    AV_Network = 0,  
    AV_Adjacent,     
    AV_Local,        
    AV_Physical,     
    AV_Size
};

enum ATTACK_COMPLEXITY
{
    AC_Low = 0,   
    AC_Medium,    
    AC_High,      
    AC_Size
};

enum PRIVILEGES_REQUIRED
{
    PR_None = 0,     
    PR_Low,         
    PR_High,        
    PR_Size
};

enum USER_INTERACTION
{
    UI_None = 0, 
    UI_Required, 
    UI_Size
};

enum RUNNING_TIME
{
    RT_Quick = 0,
    RT_Medium ,
    RT_Long,
    RT_Guessing,
    RT_Size
};


double attack_vector_score(enum ATTACK_VECTOR a_v);
double attack_complexity_score(enum ATTACK_COMPLEXITY a_c);
double privileges_required_score(enum PRIVILEGES_REQUIRED p_r);
double user_interaction_score(enum USER_INTERACTION u_i);
double running_time_score(enum RUNNING_TIME r_t);

enum ATTACK_VECTOR str_to_attack_vector(char *a_v_str);
enum ATTACK_COMPLEXITY str_to_attack_complexity(char *a_c_str);
enum PRIVILEGES_REQUIRED str_to_privileges_required(char *p_r_str);
enum USER_INTERACTION str_to_user_interaction(char *u_i_str);
enum RUNNING_TIME str_to_running_time(char *r_t_str);


double attack_vector_utility(struct arc_value *a_v);
double attack_complexity_utility(struct arc_value *a_v);
double privileges_required_utility(struct arc_value *a_v);
double user_interaction_utility(struct arc_value *a_v);
double running_time_utility(struct arc_value *a_v);

#endif
