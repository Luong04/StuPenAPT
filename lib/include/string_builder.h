/*
 * Copyright 2023 Charilaos Skandylas <Nope>
   Heavily based on:
   https://gitlab.com/herrhotzenplotz/hotzenbot-2/-/blob/trunk/src/sb.h
   by:  Nico Sonack <nsonack@herrhotzenplotz.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef STRING_BUILDER_H
#define STRING_BUILDER_H

#include <stdbool.h>
#include "mem_arena.h"


/*
 * String builder
 */

struct string_chunk
{
    struct string_chunk *next;
    char             *str;
    size_t           len;
};

struct string_builder
{
    struct string_chunk *head, *tail;
    struct mem_arena m_a;
    size_t len;
    size_t num_chunks;
};


void string_builder_init(struct string_builder *s_b, size_t n_bytes);
void string_builder_append_str(struct string_builder *s_b, char *str);
struct string_builder *string_builder_chain(struct string_builder *l,
					    struct string_builder *r);
void string_builder_prepend_str(struct string_builder *s_b, char *str);
bool string_builder_to_str(struct string_builder *s_b, char *b_buff,
			   size_t b_buff_len);
void string_builder_destroy(struct string_builder *s_b);
//size_t  string_uilder_len(struct string_builder *s_b);

#endif 
