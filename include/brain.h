#include <stdbool.h>

#include "mape_k.h"
#include "probe.h"
#include "analysis.h"


#ifndef BRAIN_H
#define BRAIN_H


struct pentest_brain
{
	pthread_mutex_t lock;
    
	bool terminate;
	size_t loop;
	char *pentest_report_buff;
	struct string_buffer *sb_pentest_report;
	char *target_dir;
};

void pentest_brain_init(char *target_dir);
void pentest_brain_cleanup();

//evaluation functions
bool analysis_needed(struct knowledge_base *k_b);
void *utility_update_done(struct knowledge_base *k_b);

//controllers
void monitor_control(struct probe *probe);
void analysis_control(struct analysis *ana);
void planning_control(struct plan *p);
void execution_control(struct exec_cmd *cmd);

//pentest_loop_control
bool pentest_loop_done();
void pentest_loop_start(struct mape_k *mape_k);

#endif
