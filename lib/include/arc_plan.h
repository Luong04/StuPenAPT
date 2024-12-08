#include <stddef.h>

#include "arc_system.h"

#ifndef ARC_PLAN
#define ARC_PLAN


enum ARC_OP
{
    ADD_COMP=0, REM_COMP,
    ADD_INVO, REM_INVO,
    ADD_IFACE, REM_IFACE,
    ADD_SYS_PROP, REM_SYS_PROP,
    ADD_COMP_PROP, REM_COMP_PROP,
    ADD_IFACE_PROP, REM_IFACE_PROP,
};


enum PLAN_RET_CODES
{
    PLAN_OK =0,               //0 
    PLAN_WRONG_ARGS,	      //1 
    PLAN_COMP_EXISTS,	      //2 
    PLAN_COMP_NOT_FOUND,      //3 
    PLAN_INVO_EXISTS,	      //4 
    PLAN_INVO_NOT_FOUND,      //5 
    PLAN_IFACE_EXISTS,	      //6 
    PLAN_IFACE_NOT_FOUND,     //7 
    PLAN_PROP_EXISTS,	      //8 
    PLAN_PROP_NOT_FOUND,      //9 
    PLAN_PROP_INVALID	      //10
};



//All the parser has to do is pretty much fill in an arc_plan structure like the one below
struct arc_plan_action
{
    enum ARC_OP op;
    char **args;
    size_t num_args;
};

//Here I will do a bit of memory copying to keep the args separate than the original text but I don't need more memory than that :)
struct arc_plan
{
    char *plan_name;
    struct arc_plan_action **actions;
    size_t num_actions;
    //ugly hack for parsing, clean up later
    size_t parse_idx;
};

void plan_init(struct arc_plan *plan, char *plan_name);
void plan_destroy(struct arc_plan *plan);
void plan_add_action(struct arc_plan *plan, struct arc_plan_action *action);
void plan_parse_old_dsl(struct arc_plan *plan, char *txt);
int plan_execute(struct arc_plan *plan, struct arc_system *system);

void plan_action_cleanup(struct arc_plan_action *plan_action);

struct arc_plan_action* plan_parse_comp_add(char *args);
struct arc_plan_action* plan_parse_comp_rem(char *args);
struct arc_plan_action* plan_parse_invo_add(char *args);
struct arc_plan_action* plan_parse_invo_rem(char *args);
struct arc_plan_action* plan_parse_iface_add(char *args);
struct arc_plan_action* plan_parse_iface_rem(char *args);
struct arc_plan_action* plan_parse_prop_add(char *args);
struct arc_plan_action* plan_parse_prop_rem(char *args);

int plan_comp_add(struct arc_plan_action *action, struct arc_system *sys);
int plan_comp_rem(struct arc_plan_action *action, struct arc_system *sys);
int plan_invo_add(struct arc_plan_action *action, struct arc_system *sys);
int plan_invo_rem(struct arc_plan_action *action, struct arc_system *sys);
int plan_iface_add(struct arc_plan_action *action, struct arc_system *sys);
int plan_iface_rem(struct arc_plan_action *action, struct arc_system *sys);
int plan_comp_prop_add(struct arc_plan_action *action, struct arc_system *sys);
int plan_comp_prop_rem(struct arc_plan_action *action, struct arc_system *sys);
int plan_sys_prop_add(struct arc_plan_action *action, struct arc_system *sys);
int plan_sys_prop_rem(struct arc_plan_action *action, struct arc_system *sys);
int plan_iface_prop_add(struct arc_plan_action *action, struct arc_system *sys);
int plan_iface_prop_rem(struct arc_plan_action *action, struct arc_system *sys);


const char *plan_return_status(enum PLAN_RET_CODES ret);
void cleanup_plan(struct arc_plan *plan);
#endif
