#include <pthread.h>
#include "task.h"
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

enum THREAD_POOL_STATE { T_P_RUNNING, T_P_PAUSED, T_P_STOPPED};





struct pool_task
{
    struct task *task;
    struct thread_pool *thread_pool;
    pthread_t thread_id;
    enum RUNNING_TASK_STATE state;
};


struct thread_pool
{
    //task queue
    struct task_entry *head;
    struct task_entry *tail;
    //running tasks
    struct pool_task *running;
    size_t num_running;
    //synchronization mechanisms
    pthread_mutex_t lock;
    pthread_cond_t signal;
    //pool state
    enum THREAD_POOL_STATE state;
};
//TODO: Do we need control over the running thread tasks ?
//SOL: Partially solved by also holding a pointer to the task alongside 



void thread_pool_init(struct thread_pool *thread_pool, int num_running);
void thread_pool_destroy(struct thread_pool *thread_pool);
void thread_pool_add_task(struct thread_pool *thread_pool, struct task *task);

#endif
