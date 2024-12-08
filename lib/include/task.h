#include <pthread.h>
#include <stdint.h>

#ifndef TASK_H
#define TASK_H

/* 
 This is a tiny task implementation
*/


enum TASK_STATUS
{
    TASK_CREATED,
    TASK_RUNNING,
    TASK_COMPLETE,
    TASK_ABORTED,
    TASK_STATUS_UNKNOWN
};

//forward declare to make it a bit easier to read
struct task;

typedef void (*task_run_fn)(struct task *task);
typedef void  (*task_done_fn)(struct task *task);




void create_task(struct task *task,
		 uint32_t task_id,
		 task_run_fn run,
		 task_done_fn done,
		 void *task_ctx,
		 void *cb_ctx);
//void create_task_defaults(struct task *task, uint32_t task_id, task_run run, void *task_ctx);
void destroy_task(struct task *task);


//thread_safe functions - all of these are inline functions acquiring and releasing the lock as needed
uint32_t task_get_id(struct task *task);
enum TASK_STATUS task_get_status(struct task *task);
void task_set_status(struct task *task, enum TASK_STATUS status);
//returns the task_ctx
void *task_get_ctx(struct task *task);
//returns the cb_ctx
void *task_get_cb_ctx(struct task *task);

void task_done(struct task *task);
void task_run(struct task *task);

//convenience function if we actually decide to make the struct opaque
size_t get_task_struct_size();

//shared among different execution engines
struct task_entry
{
    struct task *task;
    struct task_entry *next;
};

enum RUNNING_TASK_STATE {R_T_NONE,R_T_RUNNING, R_T_PAUSED, R_T_ABORTED};


//defaults
void default_task_init(struct task *task);
void default_task_done(struct task *task);

#endif
