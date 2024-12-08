#include <pthread.h>
#include "monitor.h"
#include "analyzer.h"
#include "planner.h"
#include "executor.h"

#ifndef MAPE_K_H
#define MAPE_K_H


struct mape_k
{
    //mape-k ptrs
    struct knowledge_base k_b;
    struct monitor m;
    struct analyzer a;
    struct planner p;
    struct executor e;
    //state
    bool initialized;
    bool adaptation_active;
    //lock
    
};

#endif


