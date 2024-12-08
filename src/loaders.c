#include "loaders.h"
#include "attack_repertoire.h"
#include "arc_system.h"
#include "arc_utils.h"

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tokenize.h"

struct tool_plugin *extract_toolset(struct arc_model *toolset_arch, size_t *num_tools)
{

    //Noone can touch the model while we load tools.
    pthread_mutex_lock(&toolset_arch->sys_lock);
    //This is a bit slow but it should hopefully only happen a few times, at most.
    struct tool_plugin *toolset = malloc(sizeof(struct tool_plugin)*toolset_arch->sys->num_components);
    
    size_t num_loaded = 0;
    for(size_t i=0;i<toolset_arch->sys->num_components;i++)
    {
	struct arc_component *c = toolset_arch->sys->components[i];
	struct arc_property *p = comp_find_property(c,"PluginPath");
	if(p && p->type == P_STRING)
	{
	    load_tool_plugin(&toolset[i],p->v_str);
	    num_loaded++;
	}

	if(toolset[i].num_caps > 1)
	{
	    printf("[Init] Loaded %s with %zu capabilities\n",toolset[i].name,toolset[i].num_caps);
	}
	else
	{
	    printf("[Init] Loaded %s with %zu capability\n",toolset[i].name,toolset[i].num_caps);
	}
    }
    
    *num_tools = num_loaded;
    pthread_mutex_unlock(&toolset_arch->sys_lock);
    
    
    return toolset;
    
}


struct tool_capability *find_capability(struct tool_plugin *toolset, size_t num_tools, char *capability)
{
    for(size_t i=0;i<num_tools;i++)
    {
	for(size_t j=0;j<toolset[i].num_caps;j++)
	{
	    if(strcmp(toolset[i].caps[j].name,capability) == 0)
	    {
		return &toolset[i].caps[j];
	    }
	}
    }
    fprintf(stderr, "[Warn] Could not find tool to provide %s\n", capability);
    return NULL;
}

struct attack_step *extract_attack_steps(struct arc_model *attacks_arch, size_t *num_steps,
					 struct tool_plugin *toolset, size_t num_tools)
{
    pthread_mutex_lock(&attacks_arch->sys_lock);
    struct arc_component *c_attack_steps = sys_find_component(attacks_arch->sys,"AttackSteps");

    if(!c_attack_steps)
    {
	fprintf(stderr, "[Err] Could not find the AttackSteps component panic exiting\n");
    }
    
	
    //This is a bit slow but it should hopefully only happen a few times, at most.
    struct attack_step *attack_steps = malloc(sizeof(struct attack_step)*c_attack_steps->num_interfaces);

    for(size_t i = 0; i < c_attack_steps->num_interfaces; i++)
    {

	struct arc_interface *as_if = c_attack_steps->interfaces[i];

	char *a_s_name = as_if->if_name;
	struct arc_property *p_MATTACK = iface_find_property(as_if,"MitreAttck");
	if(!p_MATTACK)
	{
	    fprintf(stderr,"[Warn] Could not find the MitreAttck property for attack step:%s\n",a_s_name);
	    exit(-1);
	}
	struct tool_capability *cap = find_capability(toolset,num_tools, a_s_name);
	if(cap)
	{
	    cap_execute run = cap->run;
	    cap_guard guard = cap->guard;
	    attack_step_init(&attack_steps[i],a_s_name,p_MATTACK->v_str,run,guard);    
	}
	else
	{
	    fprintf(stderr, "[Warn] Could not find attack step capability for %s\n",a_s_name);
	    attack_step_init(&attack_steps[i],a_s_name,p_MATTACK->v_str,NULL,NULL);    
	}
	
    }
    *num_steps = c_attack_steps->num_interfaces;
    pthread_mutex_unlock(&attacks_arch->sys_lock);

    return attack_steps;
}


static void parse_add_attack_steps(struct attack_pattern *a_p, char *a_s_str)
{

    struct tokenizer a_s_tok;
    tokenizer_init(&a_s_tok, a_s_str, "[ , ]");
    char *token = next_token(&a_s_tok);
    while(token)
    {

	attack_pattern_add_step(a_p,token);
	token = next_token(&a_s_tok);
    }
    tokenizer_cleanup(&a_s_tok);
}

struct attack_pattern *extract_attack_patterns(struct arc_model *attacks_arch, size_t *num_patterns)
{
    pthread_mutex_lock(&attacks_arch->sys_lock);
    struct arc_component *c_attack_patterns = sys_find_component(attacks_arch->sys,"AttackPatterns");

    if(!c_attack_patterns)
    {
	fprintf(stderr, "[Err] Could not find the AttackSteps component panic exiting\n");
    }
    
	
    //This is a bit slow but it should hopefully only happen a few times, at most.
    struct attack_pattern *attack_patterns = malloc(sizeof(struct attack_pattern)*c_attack_patterns->num_interfaces);

    for(size_t i = 0; i < c_attack_patterns->num_interfaces; i++)
    {

	struct arc_interface *ap_if = c_attack_patterns->interfaces[i];

	char *a_p_name = ap_if->if_name;
	
	struct arc_property *p_attack_vector = iface_find_property(ap_if, "AttackVector");
	if(!p_attack_vector)
	{
	    fprintf(stderr,"[Err] Could not find the AttackVector property for attack pattern:%s\n",a_p_name);
	    exit(-1);
	}

	struct arc_property *p_attack_complexity = iface_find_property(ap_if, "AttackComplexity");
	if(!p_attack_complexity)
	{
	    fprintf(stderr,"[Err] Could not find the AttackComplexity property for attack pattern:%s\n",a_p_name);
	    exit(-1);
	}

	struct arc_property *p_required_privileges = iface_find_property(ap_if, "RequiredPrivileges");
	if(!p_required_privileges)
	{
	    fprintf(stderr,"[Err] Could not find the RequiredPrivileges property for attack pattern:%s\n",a_p_name);
	    exit(-1);
	}

	struct arc_property *p_user_interaction = iface_find_property(ap_if, "UserInteraction");
	if(!p_user_interaction)
	{
	    fprintf(stderr,"[Err] Could not find the UserInteraction property for attack pattern:%s\n",a_p_name);
	    exit(-1);
	}

	struct arc_property *p_running_time = iface_find_property(ap_if, "RunningTime");
	if(!p_running_time)
	{
	    fprintf(stderr,"[Err] Could not find the RunningTime property for attack pattern:%s\n",a_p_name);
	    exit(-1);
	}

	struct arc_property *p_attack_steps = iface_find_property(ap_if, "Steps");
	if(!p_attack_steps)
	{
	    fprintf(stderr,"[Err] Could not find the Steps property for attack pattern:%s\n",a_p_name);
	    exit(-1);
	}
	

	
	enum ATTACK_VECTOR a_v = str_to_attack_vector(p_attack_vector->v_str);
	enum ATTACK_COMPLEXITY a_c = str_to_attack_complexity(p_attack_complexity->v_str);
	enum PRIVILEGES_REQUIRED p_r = str_to_privileges_required(p_required_privileges->v_str);
	enum USER_INTERACTION u_i = str_to_user_interaction(p_user_interaction->v_str);
	enum RUNNING_TIME r_t = str_to_running_time(p_running_time->v_str);

	if(a_v == AV_Size || a_c == AC_Size || p_r == PR_Size || u_i == UI_Size || r_t == RT_Size)
	{
	    fprintf(stderr, "[Err] One of the utility metrics for the attack pattern %s is invalid, panic exiting\n",a_p_name);
	    exit(-1);
	}

       

	attack_pattern_init(&attack_patterns[i],a_p_name,a_v,a_c,p_r,u_i,r_t);
	parse_add_attack_steps(&attack_patterns[i], p_attack_steps->v_str);

	

    }
    *num_patterns = c_attack_patterns->num_interfaces;
    pthread_mutex_unlock(&attacks_arch->sys_lock);

    return attack_patterns;
}

void load_attack_repertoire(struct attack_repertoire *a_r, struct arc_model *attacks_m,
			    struct tool_plugin *toolset, size_t num_tools)
{
    size_t num_attack_steps = 0;
    struct attack_step *attack_steps = extract_attack_steps(attacks_m,&num_attack_steps,toolset,num_tools);

    //This is a bit slow and wastes a bit of memory, redesign if this becomes a bottleneck;
    for(size_t i=0;i<num_attack_steps;i++)
    {
	attack_repertoire_add_step(a_r, &attack_steps[i]);
    }
    free(attack_steps);

    size_t num_attack_patterns = 0;
    struct attack_pattern *attack_patterns = extract_attack_patterns(attacks_m,&num_attack_patterns);
    //The same holds for patterns
    for(size_t i = 0 ; i < num_attack_patterns ; i++)
    {
	attack_repertoire_add_pattern(a_r, &attack_patterns[i]);
	//attack_pattern_print(&attack_patterns[i]);
    }
    //Note this doesn't free the a_steps contents, ownership is passed to the repository for that.
    free(attack_patterns);
}

struct scan *extract_scans(struct arc_model *scans_arc, size_t *num_scans,
			   struct tool_plugin *toolset, size_t num_tools)
{
    pthread_mutex_lock(&scans_arc->sys_lock);
    struct arc_component *c_scans = sys_find_component(scans_arc->sys,"Scans");

    if(!c_scans)
    {
	fprintf(stderr, "[Err] Could not find the Scans component panic exiting\n");
    }
    
	
    //This is a bit slow but it should hopefully only happen a few times, at most.
    struct scan *scans = malloc(sizeof(struct scan)*c_scans->num_interfaces);

    for(size_t i = 0; i < c_scans->num_interfaces; i++)
    {

	struct arc_interface *s_if = c_scans->interfaces[i];

	char *s_name = s_if->if_name;
	struct arc_property *p_MATTACK = iface_find_property(s_if,"MitreAttck");
	if(!p_MATTACK)
	{
	    fprintf(stderr,"[Err] Could not find the MitreAttck property for scan:%s\n",s_name);
	    exit(-1);
	}

	struct arc_property *p_range = iface_find_property(s_if,"ScanRange");
	if(!p_range)
	{
	    fprintf(stderr,"[Err] Could not find the ScanRange property for scan:%s\n",s_name);
	    exit(-1);
	}
	struct arc_property *p_targets = iface_find_property(s_if,"ScanTargets");
	if(!p_targets)
	{
	    fprintf(stderr,"[Err] Could not find the ScanTargets property for scan:%s\n",s_name);
	    exit(-1);
	}
	struct arc_property *p_duration = iface_find_property(s_if,"ScanDuration");
	if(!p_duration)
	{
	    fprintf(stderr,"[Err] Could not find the ScanDuration property for scan:%s\n",s_name);
	    exit(-1);
	}
	struct arc_property *p_complexity = iface_find_property(s_if,"ScanComplexity");
	if(!p_complexity)
	{
	    fprintf(stderr,"[Err] Could not find the ScanComplexity property for scan:%s\n",s_name);
	    exit(-1);
	}

	enum SCAN_RANGE s_r = str_to_scan_range(p_range->v_str);
	enum SCAN_TARGETS s_t = str_to_scan_targets(p_targets->v_str);
	enum SCAN_DURATION s_d = str_to_scan_duration(p_duration->v_str);
	enum SCAN_COMPLEXITY s_c = str_to_scan_complexity(p_complexity->v_str);
	

	
	struct tool_capability *cap = find_capability(toolset, num_tools,s_name);
	if(cap)
	{
	    cap_execute run = cap->run;
	    cap_guard guard = cap->guard;
	    scan_init(&scans[i],s_name,p_MATTACK->v_str,run, guard,s_r,s_t,s_d,s_c);
	}
	else
	{
	    fprintf(stderr,"[Warn] Could not find scan capability %s\n",s_name);
	    scan_init(&scans[i],s_name,p_MATTACK->v_str,NULL,NULL,s_r,s_t,s_d,s_c);
	}
    }
    *num_scans = c_scans->num_interfaces;
    pthread_mutex_unlock(&scans_arc->sys_lock);

    return scans;
}

void load_scan_repertoire(struct scan_repertoire *s_r, struct arc_model *scans_m,
			  struct tool_plugin *toolset, size_t num_tools)
{

    size_t num_scans = 0;
    struct scan *scans = extract_scans(scans_m,&num_scans,toolset, num_tools);

    //This is a bit slow and wastes a bit of memory, redesign if this becomes a bottleneck;
    for(size_t i=0;i<num_scans;i++)
    {
	scan_repertoire_add_scan(s_r, &scans[i]);
    }
    free(scans);
    
}
