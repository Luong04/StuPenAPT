#include <stdbool.h>

#include "arc_property.h"
#include "arc_interface.h"
#include "arc_component.h"
#include "arc_invocation.h"
#include "arc_system.h"

#include "string_buffer.h"


#ifndef ARC_JSON_EXPORT_H
#define ARC_JSON_EXPORT_H


bool arc_json_dump_property(struct string_buffer *s_b, struct arc_property *a_p);
bool arc_json_dump_interface(struct string_buffer *s_b, struct arc_interface *a_i);
bool arc_json_dump_component(struct string_buffer *s_b, struct arc_component *a_c);
bool arc_json_dump_invocation(struct string_buffer *s_b, struct arc_invocation *a_i);
bool arc_json_dump_system(struct string_buffer *s_b, struct arc_system *a_s);

#endif
