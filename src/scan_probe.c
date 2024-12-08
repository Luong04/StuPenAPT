#include "scan_probe.h"
#include <stdio.h>
#include "task_engine.h"
#include "timestamp.h"
#include "mape_k.h"

void scan_probe_scan(struct task *t);
void scan_probe_control(struct task *t);

void scan_probe_init(struct probe *probe)
{
    //struct scan_probe *s_c = probe_get_ctx(probe);
	
    timestamp("[Probe Init:","] ");
}


void scan_probe_setup(struct scan_probe *s_p, struct active_scan *a_s)
{
	s_p->a_s = a_s;
}


void scan_probe_scan(struct task *t)
{
    //HOLD HERE AND GET THIS PART RIGHT:
	/*
	  1. Get access to the monitor
	  2. Get access to the knowledge_base
	  3. Get access to the model that way
	  4. Get access to the active scan
	  5. Get access to the scan inside active scan
	  6. Get access to the target inside active scan
	 */
    struct scan_probe *s_c = task_get_ctx(t);
	struct active_scan *a_s = s_c->a_s;
	struct scan *scan = s_c->a_s->scan;
	
	
	struct probe *p = task_get_cb_ctx(t);
	struct monitor *m = probe_get_monitor(p);
	struct mape_k *a_c = m->mape_k;
	struct arc_model *a_m = a_c->k_b.model;
	
    if(!scan->run || !a_m)
    {
		fprintf(stderr, "[Warn] Either not capability function or no target model for %s\n", scan->name);
    }

    if(scan->guard && scan->guard(a_m,s_c->a_s->target))
    {
		scan->run(a_m,a_s->target);
    }
    
}

void *scan_probe_run(struct probe *probe)
{
    struct scan_probe *s_c = probe_get_ctx(probe);
	
    printf("[Scan] %s [Initialized] : \n",s_c->a_s->scan->name);
	s_c->a_s->scan_state = AS_RUN;
    struct task *t_scan = task_engine_create_task(scan_probe_scan,
						  scan_probe_control,s_c,probe);
    task_engine_add_task(t_scan,0);
	timestamp("[Probe Started:","] ");
    return NULL;

}

void scan_probe_destroy(struct probe *probe)
{
    
}

void scan_probe_control(struct task *t)
{
    struct probe *p = task_get_cb_ctx(t);
    struct scan_probe *s_c = task_get_ctx(t);
	timestamp("[","] ");
    printf("[Scan] %s [Complete]\n",s_c->a_s->scan->name);
	s_c->a_s->scan_state = AS_DONE;
    probe_set_status(p, PROBE_DONE);
    p->probe_done(p);
}


