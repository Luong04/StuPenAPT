#define _X_OPEN_SOURCE 600

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "tool_runner.h"
#include "string_buffer.h"

#define T_R_S TATUS_UNKNOWN -1337

void tool_runner_init(struct tool_runner *t_r, char *binary, size_t max_args)
{
    t_r->binary = strdup(binary);
    t_r->max_args = max_args;
    t_r->num_args = 0;
    t_r->args = malloc(t_r->max_args *sizeof(char*));
    //whitespace plus null terminator
    t_r->cmd_strlen = strlen(binary)+2;
    t_r->output = NULL;
    t_r->ret_status = T_R_STATUS_UNKNOWN;
}

void tool_runner_add_arg(struct tool_runner *t_r, char *arg)
{
    if(t_r->num_args >= t_r->max_args)
    {
	t_r->max_args *= 2;
	t_r->args = realloc(t_r->args, t_r->max_args*sizeof(char*));
    }

    t_r->args[t_r->num_args] = strdup(arg);
    //for the ' ' character
    t_r->cmd_strlen += strlen(arg)+1;
    t_r->num_args++;
}

void tool_runner_destroy(struct tool_runner *t_r)
{
    free(t_r->binary);
    for(size_t i = 0;i<t_r->num_args;i++)
    {
	free(t_r->args[i]);
    }

    free(t_r->args);
    
    if(t_r->output)
    {
	free(t_r->output);
    }
    
}

void tool_runner_run(struct tool_runner *t_r)
{
    FILE *fp;
    size_t chunk_len = 256;
    char chunk[chunk_len];
    char cmd_buff[t_r->cmd_strlen];
    memset(cmd_buff,0,t_r->cmd_strlen);
    
    struct string_buffer sb_cmd_buff;
    string_buffer_init(&sb_cmd_buff,cmd_buff,t_r->cmd_strlen);
    string_buffer_add_fmt(&sb_cmd_buff,"%s ",t_r->binary);
    for(size_t i = 0;i<t_r->num_args;i++)
    {
	string_buffer_add_fmt(&sb_cmd_buff,"%s ",t_r->args[i]);
    }
    
    fp = popen(cmd_buff, "r");
    if (fp == NULL)
    {
	return;
    }

    struct string_buffer sb_out_buff;
    size_t out_buff_len = 128;
    char *out_buff = malloc(out_buff_len);

    string_buffer_init(&sb_out_buff,out_buff,out_buff_len);
    
    while (fgets(chunk, chunk_len, fp) != NULL)
    {
	bool ret = string_buffer_add_str(&sb_out_buff,chunk);
	while(!ret)
	{ 
	    char *old_buff = sb_out_buff.b_buff;
	    out_buff_len *= 2;
	    char *new_buff = malloc(out_buff_len);
	    string_buffer_resize(&sb_out_buff,new_buff,out_buff_len);
	    free(old_buff);
	    ret = string_buffer_add_str(&sb_out_buff,chunk);
	}
	
	
    }
    
    t_r->output = sb_out_buff.b_buff;
    t_r->ret_status = pclose(fp);  
}

void tool_runner_print_summary(struct tool_runner *t_r)
{
    printf("Binary: %s\n",t_r->binary);
    printf("Arguments:\n");
    for(size_t i=0;i<t_r->num_args;i++)
    {
	printf("%zu:%s\n",i,t_r->args[i]);
    }
    printf("output:\n");
    if(t_r->output)
    {
	printf("%s",t_r->output);
    }
}

/*
int main(int argc, char *argv[])
{

    if(argc < 2)
    {
	printf("Usage: tool_runner tool args\n");
	return -1;
    }

    struct tool_runner tool_runner;
    tool_runner_init(&tool_runner, argv[1], argc - 2);
    
    
    for(size_t i = 0; i < argc -2 ; i++)
    {
	tool_runner_add_arg(&tool_runner, argv[2+i]);
    }
    tool_runner_run(&tool_runner);
    tool_runner_print_summary(&tool_runner);;
    tool_runner_destroy(&tool_runner);
    return 0;
    
    
    }*/
