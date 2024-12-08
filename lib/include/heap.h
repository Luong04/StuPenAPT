#include <stddef.h>

#ifndef HEAP_H
#define HEAP_H

struct heap_entry
{
    int priority;
    void *payload;
};

struct heap
{
    struct heap_entry *heap_array;
    size_t max_size;
    size_t cur_size;    
};


void heap_init(struct heap *heap,size_t initial_size);
void heap_insert(struct heap *heap, int priority, void *payload);
void *heap_peek(struct heap *heap);
int heap_peek_priority(struct heap *heap);
void *heap_top(struct heap *heap);
void heap_destroy(struct heap *heap);

//inline 
static inline int parent(int idx);
static inline int leftchild(int idx);
static inline int rightchild(int idx);
#endif
