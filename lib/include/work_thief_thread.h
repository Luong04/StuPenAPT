#include <pthread.h>
#include "task.h"

#ifndef WORK_THIEF_H
#define WORK_THIEF_H


enum WORK_THIEF_STATE {W_T_NOT_STARTED,W_T_RUNNING, W_T_PAUSED, W_T_STOPPED};

struct task_engine;

struct thief_task
{
    struct task *task;
    struct work_thief *thief;
    enum RUNNING_TASK_STATE state;
};


struct work_thief
{
    //task queue
    struct task_entry *head;
    struct task_entry *tail;
    size_t t_q_size;
    //running task
    struct thief_task *running;
    //synchronization mechanisms
    pthread_mutex_t lock;
    pthread_cond_t signal;
    pthread_t thread_id;
    //thief thread state
    enum WORK_THIEF_STATE state;
};


void work_thief_init(struct work_thief *w_t);
void work_thief_destroy(struct work_thief *w_t);
void work_thief_add_task(struct work_thief *w_t, struct task *t);
struct task *work_thief_tail_steal(struct work_thief *w_t);
size_t work_thief_queue_size(struct work_thief *w_t);
void work_thief_state_print(struct work_thief *w_t);

#endif
