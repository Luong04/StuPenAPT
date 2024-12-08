#include "arc_system.h"
#include "arc_condition.h"
#include "arc_value.h"
#include <stddef.h>

struct arc_system* arc_parse(char *txt);

/*
int parse_system(char **tokens, size_t num_tokens, size_t *cur_token, struct arc_system *system);
int parse_components(char **tokens, size_t num_tokens, size_t *cur_token, struct arc_system *system);
int parse_invocations(char **tokens, size_t num_tokens, size_t *cur_token, struct arc_system *system);
int parse_properties(char **tokens, size_t num_tokens, size_t *cur_token, void *entity, enum ARC_ENTITY_TYPE type );
int parse_interfaces(char **tokens, size_t num_tokens, size_t *cur_token, struct arc_component *comp);
struct arc_component* parse_component(char **tokens, size_t num_tokens, size_t *cur_token);
struct arc_invocation* parse_invocation(char **tokens, size_t num_tokens, size_t *cur_token,struct arc_system *system);
struct arc_interface* parse_interface(char **tokens, size_t num_tokens, size_t *cur_token);
struct arc_property* parse_property(char **tokens, size_t num_tokens, size_t *cur_token);
*/

struct arc_property* parse_txt_property(char *txt,struct arc_system *sys);
struct arc_value parse_txt_value(char *txt);
enum ARC_COMPARISON_OPERATOR parse_cond_value(char *txt);

struct arc_simple_condition* parse_simple_cond(char **tokens, size_t num_tokens, size_t *cur_token, struct arc_system *sys);
struct arc_complex_condition* parse_complex_cond(char **tokens, size_t num_tokens, size_t *cur_token, struct arc_system *sys);

struct arc_simple_condition* parse_txt_condition(char *txt, struct arc_system *sys);
struct arc_complex_condition* parse_txt_complex_condition(char *txt,
							  struct arc_system *sys);
