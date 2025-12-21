#include <stdbool.h>
#include <stddef.h>

#ifndef STRING_BUFFER_H
#define STRING_BUFFER_H


struct string_buffer
{
    char *b_buff;
    size_t b_len;
    size_t b_alloc;
};
    

void string_buffer_init(struct string_buffer *s_b, char *b_buff, size_t b_len);
bool string_buffer_add_str(struct string_buffer *s_b, char *str);
bool string_buffer_add_fmt(struct string_buffer *s_b, char *fmt, ...);
void string_buffer_clear(struct string_buffer *s_b);

void string_buffer_resize(struct string_buffer *s_b, char *n_buff, size_t n_len);

#endif
