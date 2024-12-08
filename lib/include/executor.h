#include "execution_command.h"
#include <stdbool.h>


#ifndef EXECUTOR_H
#define EXECUTOR_H

#define NUM_EXECUTION_CMDS_INIT 8

struct mape_k;

struct executor
{    
    struct exec_cmd **cmds;
    size_t num_cmds;
    size_t max_cmds;
    /* Only make the pointer known */
    struct mape_k *mape_k;
};


void executor_init(struct executor *exec, struct mape_k *mape_k);
void executor_destroy(struct executor *exec);

void executor_add_command(struct executor *exec, struct exec_cmd *cmd);
void executor_rem_command_idx(struct executor *exec, size_t idx);
void executor_rem_command(struct executor *exec, struct exec_cmd *cmd);
void executor_run(struct executor *exec); //best name ever :D
bool executor_all_cmds_done(struct executor *exec);

#endif

