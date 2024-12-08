#include <stdint.h>
#include <stddef.h>

#ifndef MEM_ARENA_H
#define MEM_ARENA_H



struct mem_arena
{
    uint8_t *bytes;
    size_t n_bytes;
    size_t n_alloc;
};


void mem_arena_init(struct mem_arena *m_a, size_t n_bytes);
void mem_arena_destroy(struct mem_arena *m_a);
void *mem_arena_alloc(struct mem_arena *m_a, size_t a_size);
void *mem_arena_free(struct mem_arena *m_a, void *p);


#endif
