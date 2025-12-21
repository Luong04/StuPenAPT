#include "tokenize.h"
#include <stdlib.h>
#include <string.h>


#include <stdio.h>

static inline int in_string(char c, char *str)
{
    char *start=str;
    while(*start!='\0')
    {
	if(c == *start)
	{
	    return 1;
	}
	start++;
    }
    return 0;
}



void tokenizer_init(struct tokenizer *tok, char *str,char *delims)
{
    tok->pos=str;
    tok->str=str;
    tok->delims=delims;
    tok->tokens=malloc(strlen(str)+1);
    tok->tpos=0;
    
}

void tokenizer_cleanup(struct tokenizer *tok)
{
    free(tok->tokens);
    tok->tpos=0;
}

char* next_token(struct tokenizer *tok)
{
    size_t size=0;
    while(*tok->pos!='\0' && in_string(*tok->pos,tok->delims))
    {
	tok->pos++;
    }
    char *start=tok->pos;
    while(*tok->pos!='\0' && !in_string(*tok->pos,tok->delims))
    {
	size++;
	tok->pos++;
    }
    if(size>0)
    {
	char *ret = &tok->tokens[tok->tpos];
	strncpy(&tok->tokens[tok->tpos],start,size);
	tok->tokens[tok->tpos+size]='\0';
	tok->tpos+=(size+1);
	//skip trailing delimeters
	while(*tok->pos!='\0' && in_string(*tok->pos,tok->delims))
	{
	    tok->pos++;
	}

	return ret;
    }
   
    return NULL;
}

int count_tokens(struct tokenizer *tok)
{
    if(!tok->str)
    {
	return 0;
    }
    else 
    {
	int num_tokens=1;
	char *start=tok->str;
	while(start && *start!='\0')
	{
	    if(in_string(*start,tok->delims))
	    {
		num_tokens++;
		while(start && *start!='\0' && in_string(*start,tok->delims))
		{
		    start++;
		}
	    }
	    else
	    {
	    start++;
	    }
	    
	}
	return num_tokens;
    }
    
}


char next_delim(struct tokenizer *tok, char *delims)
{
    size_t size=0;
    while(*tok->pos!='\0' && !in_string(*tok->pos,delims))
    {
	tok->pos++;
    }
    return *tok->pos++;
}

