#include "arc_recognizer.h"
#include "arc_plan.h"

#ifndef PLAN_PARSER_H
#define PLAN_PARSER_H


//for now just do the parsing, do the plan_action identification in the next pass
bool plan_parse(struct token_stream *t_s, struct arc_plan *plan, char *name,
		size_t how_many);

bool parse_lines(struct token_stream *t_s, size_t *pos, struct arc_plan *plan);
bool parse_line(struct token_stream *t_s,   size_t *pos, struct arc_plan *plan);
bool parse_genop(struct token_stream *t_s,  size_t *pos, struct arc_plan *plan);
bool parse_propop(struct token_stream *t_s, size_t *pos, struct arc_plan *plan);
bool parse_pupdate(struct token_stream *t_s, size_t *pos, struct arc_plan *plan);
bool parse_pvalue(struct token_stream *t_s, size_t *pos, struct arc_plan *plan);
bool parse_genop1(struct token_stream *t_s, size_t *pos, struct arc_plan *plan);
bool parse_genop2(struct token_stream *t_s, size_t *pos, struct arc_plan *plan);
bool parse_ffi(struct token_stream *t_s, size_t *pos, struct arc_plan *plan);
bool parse_op(struct token_stream *t_s, size_t *pos, struct arc_plan *plan);





int run_parser(int argc, char *argv[]);

#endif
