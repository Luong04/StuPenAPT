#include <stddef.h>

#ifndef MEM_POOL_H 
#define MEM_POOL_H


/*
  Slightly modified/simplified version of:
  https://www.gingerbill.org/article/2019/02/16/memory-allocation-strategies-004
*/

struct mem_pool_free_node
{
    struct mem_pool_free_node *next;
};

struct mem_pool
{
    void *pool;
    size_t pool_size;
    size_t elem_size;

    struct mem_pool_free_node *head;
};

void mem_pool_init(struct mem_pool *m_p, size_t pool_size, size_t elem_size);
void *mem_pool_alloc(struct mem_pool *m_p);
void mem_pool_free(struct mem_pool *m_p, void *ptr);
void mem_pool_reset(struct mem_pool *m_p);
void mem_pool_destroy(struct mem_pool *m_p);


#endif
