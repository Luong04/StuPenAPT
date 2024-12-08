#include "knowledge_base.h"
#include "pentest_state.h"
#include "string_buffer.h"
#include "arc_model.h"

void pentest_state_json_dump(struct string_buffer *s_b, struct pentest_state *pt_s);
void pentest_state_json_dump_attacks_failed(struct string_buffer *s_b, struct pentest_state *pt_s);
void pentest_state_json_dump_attacks_worked(struct string_buffer *s_b, struct pentest_state *pt_s);
void pentest_state_json_dump_attacks_active(struct string_buffer *s_b, struct pentest_state *pt_s);
void pentest_state_json_dump_targets(struct string_buffer *s_b, struct pentest_state *pt_s, struct arc_model *a_m);
void pentest_state_json_dump_scans_active(struct string_buffer *s_b, struct pentest_state *pt_s);
void target_utility_json_dump(struct string_buffer *s_b, struct knowledge_base *k_b);
void attack_utility_json_dump(struct string_buffer *s_b, struct knowledge_base *k_b);

void pentest_update_report(struct string_buffer *s_b, struct knowledge_base *k_b, size_t loop);
void pentest_write_report(struct string_buffer *s_b, struct knowledge_base *k_b, char *filename);
