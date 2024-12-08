#include <stdbool.h>



//This is now a unified recognizer for all of YAADL
#ifndef ARC_RECOGNIZER_H
#define ARC_RECOGNIZER_H

/*'[', ']', '=', 'NIL', '->', ':', [0-9], [a-z] */

enum ARC_TOKEN_TYPE
{
    TOKEN_L_BRACKET = 0,
    TOKEN_R_BRACKET,
    TOKEN_L_CBRACE,
    TOKEN_R_CBRACE,
    TOKEN_EQUALS,
    TOKEN_NIL,
    TOKEN_CALLS,
    TOKEN_COLON,
    TOKEN_NUMBER,
    TOKEN_ID,
    TOKEN_STRING,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_QCOMP,
    TOKEN_QIFACE,
    TOKEN_UNKNOWN,
	TOKEN_EOF,
    NUM_TOKENS
};


static char *TOKEN_NAME[] =
{
    "TOKEN_L_BRACKET",
    "TOKEN_R_BRACKET",
    "TOKEN_L_CBRACE",
    "TOKEN_R_CBRACE",
    "TOKEN_EQUALS",
    "TOKEN_NIL",
    "TOKEN_CALLS",
    "TOKEN_COLON",
    "TOKEN_NUMBER",
    "TOKEN_ID",
    "TOKEN_STRING",
    "TOKEN_PLUS",
    "TOKEN_MINUS",
    "TOKEN_QCOMP",
    "TOKEN_QIFACE",
    "TOKEN_UNKNOWN",
	"TOKEN_EOF"
};


struct token
{
    char *str;
    enum ARC_TOKEN_TYPE t_type;
};

void token_init(struct token *token, char *str, enum ARC_TOKEN_TYPE t_type);
void token_cleanup(struct token *token);

struct token_stream
{
    struct token *tokens;
    size_t num_tokens;
    size_t max_tokens;
};


#define TOKEN_STREAM_INIT_SIZE 32

void token_stream_init(struct token_stream *t_s);
void token_stream_add_token(struct token_stream *t_s, struct token *token);
void token_stream_destroy(struct token_stream *t_s);

//Allocates memory for the str part of the token
struct token recognize(char *text, size_t *pos);

//assumes the text memory will not be freed while the token stream is in use.
bool build_token_stream(char *text, struct token_stream *t_s);



#endif
