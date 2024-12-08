#include <pthread.h>
#include <stdbool.h>

#ifndef PLAN_H
#define PLAN_H

enum PLAN_STATUS {PLAN_STARTED,PLAN_RUNNING, PLAN_DONE, PLAN_STATUS_UNDEF};

struct plan;
struct planner;

typedef void (*plan_run)(struct plan *p);
typedef void (*plan_done)(struct plan *p);

struct plan
{
    pthread_mutex_t lock;
    void *ctx;
    enum PLAN_STATUS status;
    
    //pointer to the planner
    struct planner *pl;

    //function pointer interface
    plan_run run;
    plan_done done;
};

//high level interface
void init_plan(struct plan *p, plan_run run, plan_done done, void *ctx);
void run_plan(struct plan *p);
void destroy_plan(struct plan *p);

//safe accessors
void *plan_get_ctx(struct plan *p);
void plan_set_status(struct plan *p, enum PLAN_STATUS status);
enum PLAN_STATUS plan_get_status(struct plan *p);
struct planner *plan_get_planner(struct plan *p);
void plan_set_planner(struct plan *p, struct planner *pl);



#endif
