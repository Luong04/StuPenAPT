#include <stdbool.h>
#include <stddef.h>

#include "arc_recognizer.h"

#ifndef PARSE_COMMON
#define PARSE_COMMON

//convinience functions
int consume(char *token, char *expected,size_t *cur_token);
bool consume_p(struct token_stream *t_s, enum ARC_TOKEN_TYPE t_type, size_t *pos);
bool eat_until(struct token_stream *t_s, enum ARC_TOKEN_TYPE t_type, size_t *pos);
bool expect(struct token_stream *t_s, char *expected, size_t pos);
bool peek_type(struct token_stream *t_s, enum ARC_TOKEN_TYPE t_type, size_t *pos);
bool peek_str(struct token_stream *t_s, enum ARC_TOKEN_TYPE t_type, char *str,  size_t *pos);
void strip_quotes(char *str);

char* parse_name(char **tokens, size_t num_tokens, size_t *cur_token);
int parse_int(char **tokens, size_t num_tokens, size_t *cur_token);
double parse_double(char **tokens, size_t num_tokens, size_t *cur_token);
char* parse_string(char **tokens, size_t num_tokens, size_t *cur_token);


#endif
