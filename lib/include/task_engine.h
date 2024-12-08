#include "task_graph.h"
#include "work_thief_thread.h"
#include "mem_pool.h"

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifndef TASK_ENGINE_H
#define TASK_ENGINE_H

struct task_engine
{
    //synchronization mechanisms
    pthread_mutex_t lock;
    pthread_cond_t signal;

    //dependency graph
    struct task_graph dep_graph;
    //worker threads
    struct work_thief *workers;
    size_t num_workers;

    struct mem_pool task_pool;
    pthread_mutex_t task_pool_lock;
    uint32_t next_task_id;
    
};

void task_engine_init(size_t num_workers,uint32_t hash_size, uint32_t pool_size);
void task_engine_add_task(struct task *task, size_t num_deps, ...);
void task_engine_sched(struct task *task);
void task_engine_done(struct task *task);
bool task_engine_try_steal(struct work_thief *w_t);
void task_engine_destroy();

struct task *task_engine_create_task(task_run_fn run,task_done_fn done, void *ctx, void *cb_ctx);
void task_engine_free_task(struct task *t);


#endif
