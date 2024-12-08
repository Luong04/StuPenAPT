#include <stddef.h>
#include <stdbool.h>


#ifndef INI_PARSER_H
#define INI_PARSER_H


enum INI_TOKEN_TYPE
{
    INI_TOK_L_BRACKET,
    INI_TOK_R_BRACKET,
    INI_TOK_DOT,
    INI_TOK_EQUALS,
    INI_TOK_NUMBER,
    INI_TOK_ID,
    INI_TOK_STRING,
    INI_TOK_WSPACE,
    INI_TOK_END,
    INI_TOK_UNKNOWN,
    INI_NUM_TOKENS
};




struct ini_token
{
    char *str;
    enum INI_TOKEN_TYPE t_type;
};

void ini_token_init(struct ini_token *token, char *str, enum INI_TOKEN_TYPE t_type);


struct ini_token_stream
{
    struct ini_token *tokens;
    size_t num_tokens;
    size_t max_tokens;
};


#define INI_TOKEN_STREAM_INIT_SIZE 32

void ini_token_stream_init(struct ini_token_stream *t_s);
void ini_token_stream_add_token(struct ini_token_stream *t_s,
				struct ini_token *token);
void ini_token_stream_destroy(struct ini_token_stream *t_s);




//let's actually write a character based recognizer this time.
enum INI_TOKEN_TYPE ini_tok_recognize(char *text, int *pos);

//assumes the text memory will not be freed while the token stream is in use.
bool build_ini_token_stream(char *text, struct ini_token_stream *t_s);


enum INI_LAST_SEL{INI_S_INI=0, INI_S_SECTION, INI_S_SUBSECTION};


struct ini_elem
{
    enum INI_LAST_SEL l_s;
    union
    {
	struct ini *ini;
	struct ini_section *i_s;
	struct ini_subsection *i_ss;
    };
};


    
//forward declare it for now implement it later ; and back patch the parser
struct ini;
bool ini_parse(char *ini_text, struct ini *ini);

bool parse_ini(struct ini_token_stream *t_s, int *pos, struct ini *ini,struct ini_elem *last);
bool parse_ini_sections(struct ini_token_stream *t_s, int *pos, struct ini *ini,struct ini_elem *last);
bool parse_ini_properties(struct ini_token_stream *t_s, int *pos, struct ini *ini,struct ini_elem *last);
bool parse_ini_property(struct ini_token_stream *t_s, int *pos, struct ini *ini,struct ini_elem *last);
bool parse_ini_propvalue(struct ini_token_stream *t_s, int *pos, struct ini *ini,struct ini_elem *last);
#endif
