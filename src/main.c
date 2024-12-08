#define _XOPEN_SOURCE  600

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "tool_runner.h"
#include "mape_k.h"
#include "arc_model.h"
#include "arc_system.h"
#include "arc_parse.h"
#include "file_utils.h"
#include "arc_utils.h"
#include "view.h"

#include <unistd.h>
#include <sys/types.h>
#include "loaders.h"
#include "attack_repertoire.h"
#include "tool_plugin.h"
#include "scan_probe.h"
#include "brain.h"

#include "target_utility.h"
#include "scan_utility.h"
#include "attack_utility.h"
#include "attack_plan_generator.h"
#include "execution_command.h"
#include "pentest_state.h"
#include "string_buffer.h"
#include "arc_json_export.h"
#include "connection_cache.h"


void cleanup_utilities(struct knowledge_base *k_b)
{

    struct utility_score *target_utility = knowledge_base_get_res(k_b,"TargetUtilityRanking");
    size_t num_utility_entries = knowledge_base_num_res(k_b,"TargetUtilityRanking");
    for( size_t i = 0 ; i < num_utility_entries ; i++)
    {
	utility_score_destroy(&target_utility[i]);
    }
    free(target_utility);

    struct utility_score *attack_utility = knowledge_base_get_res(k_b,"AttackUtilityRanking");
    num_utility_entries = knowledge_base_num_res(k_b,"AttackUtilityRanking");
    for( size_t i = 0 ; i < num_utility_entries ; i++)
    {
	utility_score_destroy(&attack_utility[i]);
    }
    free(attack_utility);

    /*
    //This is ugly so maybe store the utility some other place later but for now...
    for(size_t i = 0 ; i < k_b->num_res;i++)
    {
	if(k_b->named_res[i] &&
	   (strcmp(k_b->named_res[i]->name,TargetUtilityRanking) == 0
	    || strcmp(k_b->named_res[i]->name,TargetUtilityRanking) == 0 ))
	{
	    free(k_b->named_res[i]);
	}
    }
    */
    knowledge_base_rem_res(k_b, "TargetUtilityRanking");
    knowledge_base_rem_res(k_b, "AttackUtilityRanking");
    
    
}

void create_system_view(struct system_view *s_v, struct arc_system *sys )
{
    uint8_t v_f= VIEW_FLAG_INTERFACES | VIEW_FLAG_PROPERTIES | VIEW_FLAG_PROP_VALUE;// VIEW_FLAG_PROPERTIES | VIEW_FLAG_PROP_VALUE;//VIEW_FLAG_NAME_ONLY;
    system_view_init(s_v,sys,v_f);
}


void xdot_draw(char *filename)
{
    pid_t pid = fork();
    
    if(pid==0)
    {
	execl ("/usr/bin/xdot", "xdot", filename, (char *)0);
    }
    else
    {
	return;
    }
}

int main(int argc, char *argv[])
{


    char *target_dir = NULL;
    if(argc < 2)
    {
		char cwd[1024];
		if (getcwd(cwd, sizeof(cwd)) != NULL)
		{
			printf("[Warn] No directory provided, assuming .(%s)\n",cwd);
			target_dir = ".";
		}
    }
	else
	{
		target_dir = argv[1];
	}
    
	pentest_brain_init(target_dir);
	task_engine_init(16,1024+256,1024);
	connection_cache_init();

	size_t tools_arch_len = strlen(target_dir)+strlen("Tooling.arch")+2;
	char *tools_arch = malloc(tools_arch_len);
	snprintf(tools_arch, tools_arch_len, "%s/Tooling.arch",target_dir);
	
	size_t attacks_arch_len = strlen(target_dir)+strlen("AttackRepertoire.arch")+2;
	char *attacks_arch = malloc(attacks_arch_len);
	snprintf(attacks_arch, attacks_arch_len, "%s/AttackRepertoire.arch",target_dir);

	size_t scans_arch_len = strlen(target_dir)+strlen("ScanRepertoire.arch")+2;
	char *scans_arch = malloc(scans_arch_len);
	snprintf(scans_arch, scans_arch_len, "%s/ScanRepertoire.arch",target_dir);

	size_t target_arch_len = strlen(target_dir)+strlen("Target.arch")+2;
	char *target_arch = malloc(target_arch_len);
	snprintf(target_arch, target_arch_len, "%s/Target.arch",target_dir);

    //initialize the monitor
    struct mape_k apt_brain = { 0 };
    

    struct arc_model tools_m;
    load_arc_model(tools_arch,&tools_m);
	
    size_t num_tools = 0;
    struct tool_plugin *toolset = extract_toolset(&tools_m, &num_tools);

    struct arc_model attacks_m;
    load_arc_model(attacks_arch,&attacks_m);

    struct attack_repertoire a_r;
    attack_repertoire_init(&a_r);
    load_attack_repertoire(&a_r, &attacks_m,toolset,num_tools);
    //attack_repertoire_print(&a_r);

    struct arc_model scans_m;
    load_arc_model(scans_arch,&scans_m);

    struct scan_repertoire s_r;
    scan_repertoire_init(&s_r);
    load_scan_repertoire(&s_r, &scans_m,toolset, num_tools);
    //scan_repertoire_print(&s_r);

    struct arc_model target_m;
    load_arc_model(target_arch,&target_m);
    init_knowledge_base(&apt_brain.k_b, &target_m);
	
    //Store the scanning and attack repertoires in the knowledge base.
    struct named_res a_r_res;
    named_res_init(&a_r_res, "AttackRepertoire", 1, &a_r);
    knowledge_base_add_res(&apt_brain.k_b, &a_r_res);

    struct named_res s_r_res;
    named_res_init(&s_r_res, "ScanRepertoire", 1, &s_r);
    knowledge_base_add_res(&apt_brain.k_b, &s_r_res);
    
    
    
    struct arc_value v_max_targets = arc_model_get_property(&target_m, "TARGET_THRESHOLD");
    struct arc_value v_max_attacks = arc_model_get_property(&target_m, "ATTACK_THRESHOLD");


	//Set utlity models, move someplace else later.
	target_utility_init(&apt_brain.k_b);
	scan_utility_init(&apt_brain.k_b);
	attack_utility_init(&apt_brain.k_b);
    
    struct pentest_state p_s;
    pentest_state_init(&p_s,v_max_targets.v_int, v_max_attacks.v_int,v_max_targets.v_int);

    struct named_res p_s_res;
    named_res_init(&p_s_res, "PentestState", 1, &p_s);
    knowledge_base_add_res(&apt_brain.k_b, &p_s_res);
        
    init_monitor(&apt_brain.m,&apt_brain, analysis_needed);
	

    //initialize analyzer
    struct analysis a_target_utility;
	struct analysis a_scan_utility;
    struct analysis a_attack_utility;
    
    init_analysis(&a_target_utility, target_utility, analysis_control, &apt_brain.a);
	init_analysis(&a_scan_utility, scan_utility, analysis_control, &apt_brain.a);
    init_analysis(&a_attack_utility, attack_utility, analysis_control, &apt_brain.a);
    
    init_analyzer(&apt_brain.a,&apt_brain,utility_update_done);
    analyzer_add_analysis(&apt_brain.a, &a_target_utility);
	analyzer_add_analysis(&apt_brain.a, &a_scan_utility);
    analyzer_add_analysis(&apt_brain.a, &a_attack_utility);

    struct plan attack_plan;
    init_plan(&attack_plan,attack_plan_gen, planning_control, &apt_brain.p);
    init_planner(&apt_brain.p,&apt_brain);
    planner_add_plan(&apt_brain.p,&attack_plan);

    executor_init(&apt_brain.e,&apt_brain);
    

    pentest_loop_start(&apt_brain);
    
    struct system_view target_view;
    create_system_view(&target_view,  target_m.sys);
    //xdot_draw("target.dot");
    while(!pentest_loop_done())
    {
	//sleep to not consume too much cpu time
	usleep(1000000);
	system_view_update(&target_view, target_m.sys);
	//system_view_to_dot(&target_view,"target.dot");
	
    }

    /*
    system_view_update(&target_view, target_m.sys);
    system_view_to_dot(&target_view,target_dot);
    xdot_draw(target_dot);
    */



    

	size_t config_used_path_len = strlen(target_dir)+strlen("Config.json")+2;
	char *config_used_path = malloc(config_used_path_len);
	snprintf(config_used_path,config_used_path_len,"%s/Config.json",target_dir);

    char *str_tools_repo = malloc(10*1024*1024);
    struct string_buffer json_sb_tr;
    string_buffer_init(&json_sb_tr, str_tools_repo, 10*1024*1024);
	string_buffer_add_str(&json_sb_tr,"[\n");
    arc_json_dump_system(&json_sb_tr,tools_m.sys);
	string_buffer_add_str(&json_sb_tr,",\n");
    dump_buffer_to_file(config_used_path, str_tools_repo,"w");
    
    char *str_attack_repo = malloc(10*1024*1024);
    struct string_buffer json_sb_ar;
    string_buffer_init(&json_sb_ar, str_attack_repo, 10*1024*1024);
    arc_json_dump_system(&json_sb_ar,attacks_m.sys);
	string_buffer_add_str(&json_sb_ar,",\n");
    dump_buffer_to_file(config_used_path, str_attack_repo,"a");
	
    char *str_scan_repo = malloc(10*1024*1024);
    struct string_buffer json_sb_sr;
    string_buffer_init(&json_sb_sr, str_scan_repo, 10*1024*1024);
    arc_json_dump_system(&json_sb_sr,scans_m.sys);
	string_buffer_add_str(&json_sb_sr,"\n]");
    dump_buffer_to_file(config_used_path, str_scan_repo,"a");


	
    size_t target_sys_path_len = strlen(target_dir)+strlen("TargetSystem.json")+2;
	char *target_sys_path = malloc(target_sys_path_len);
	snprintf(target_sys_path,target_sys_path_len,"%s/TargetSystem.json",target_dir);
    char *str_target_m = malloc(10*1024*1024);
    struct string_buffer json_sb_ta;
    string_buffer_init(&json_sb_ta, str_target_m, 10*1024*1024);
    arc_json_dump_system(&json_sb_ta,target_m.sys);
    dump_buffer_to_file(target_sys_path, str_target_m,"w");

	
	system_view_destroy(&target_view);
    cleanup_utilities(&apt_brain.k_b);
    pentest_state_cleanup(&p_s);
    destroy_monitor(&apt_brain.m);
    destroy_analyzer(&apt_brain.a);
    destroy_planner(&apt_brain.p);
    destroy_knowledge_base(&apt_brain.k_b);
    executor_destroy(&apt_brain.e);
    destroy_model(&tools_m);
    cleanup_sys(tools_m.sys);
    cleanup_sys(attacks_m.sys);
    cleanup_sys(scans_m.sys);
    cleanup_sys(target_m.sys);

    
    for(size_t i=0;i<num_tools;i++)
    {
	tool_plugin_destroy(&toolset[i]);
    }
    free(toolset);
    
    attack_repertoire_destroy(&a_r);
    scan_repertoire_destroy(&s_r);

    free(str_tools_repo);
    free(str_attack_repo);
    free(str_scan_repo);
    free(str_target_m);

    connection_cache_cleanup();

	free(tools_arch);
	free(attacks_arch);
	free(scans_arch);
	free(target_arch);
	free(target_sys_path);
    free(config_used_path);

	pentest_brain_cleanup();
	
    return 0;    
    
}
