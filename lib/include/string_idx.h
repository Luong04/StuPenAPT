#include <stdint.h>
#include <stddef.h>

#ifndef STRING_IDX_H
#define STRING_IDX_H


struct string_idx
{
    size_t *idx;
    char   *buffer;
    size_t buffer_len;
    size_t num_elements;
    size_t max_elements;
};


void string_idx_init(struct string_idx *s_i, size_t max_elems, size_t buff_len);
void string_idx_put(struct string_idx *s_i, char *str);
char *string_idx_get(struct string_idx *s_i, size_t idx);
void string_idx_destroy(struct string_idx *s_i);

#endif
