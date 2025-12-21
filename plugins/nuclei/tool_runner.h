

#ifndef TOOL_RUNNER_H
#define TOOL_RUNNER_H


#define T_R_STATUS_UNKNOWN -1337
struct tool_runner
{
    char *binary;
    char **args;
    size_t num_args;
    size_t max_args;
    size_t cmd_strlen;
    char *output;
    int ret_status;
};

void tool_runner_init(struct tool_runner *t_r, char *binary, size_t max_args);
void tool_runner_destroy(struct tool_runner *t_r);
void tool_runner_add_arg(struct tool_runner *t_r, char *arg);
void tool_runner_run(struct tool_runner *t_r);
void tool_runner_print_summary(struct tool_runner *t_r);

#endif
