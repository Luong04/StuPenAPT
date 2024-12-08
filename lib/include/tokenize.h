
#include <stddef.h>

#ifndef TOKENIZER_H

#define TOKENIZER_H

struct tokenizer
{
    char *str;
    char *delims;
    char *pos;
    char *tokens;
    size_t tpos;
};

void tokenizer_init(struct tokenizer *tok, char *str,char *delims);
void tokenizer_cleanup(struct tokenizer *tok);

char* next_token(struct tokenizer *tok);
char next_delim(struct tokenizer *tok, char *delims);
int   count_tokens(struct tokenizer *tok);
#endif
