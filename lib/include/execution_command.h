#include <stdint.h>
#include <pthread.h>

#ifndef EXECUTION_COMMAND_H
#define EXECUTION_COMMAND_H

enum EXEC_CMD_STATUS {CMD_CREAT=0, CMD_PROC, CMD_DONE, CMD_FAIL, CMD_INVALID }; 

struct executor;
struct exec_cmd;

typedef void (*cmd_run)(struct exec_cmd *cmd);
typedef void (*cmd_done)(struct exec_cmd *cmd);

struct exec_cmd
{

    //lock
    pthread_mutex_t lock;
    //status and context
    enum EXEC_CMD_STATUS status;
    void *ctx;

    //just make pointer available
    struct executor *exec;


    //func ptrs
    cmd_run run;
    cmd_done done;
};

void exec_cmd_init(struct exec_cmd *cmd, cmd_run run, cmd_done done, void *ctx);
void exec_cmd_destroy(struct exec_cmd *cmd);

void exec_cmd_run(struct exec_cmd *cmd);

//safe accessors
void *cmd_get_ctx(struct exec_cmd *cmd);
void cmd_set_status(struct exec_cmd *cmd, enum EXEC_CMD_STATUS status);
enum EXEC_CMD_STATUS cmd_get_status(struct exec_cmd *cmd);
struct executor *cmd_get_executor(struct exec_cmd *cmd);
void cmd_set_executor(struct exec_cmd *cmd, struct executor *exec);


#endif
