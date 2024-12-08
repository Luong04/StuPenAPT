#include <stdlib.h>

#ifndef QUEUE_H
#define QUEUE_H

struct queue_entry
{
    void *elem;
    struct queue_entry *next,*prev;
};

//a queue of queue_entries
struct generic_queue
{
    size_t size;
    struct queue_entry *head,*tail;
};


void  queue_init(struct generic_queue *queue);
void  queue_push_front(struct generic_queue *queue,void *elem);
void  queue_push_back(struct  generic_queue  *queue,void *elem);
//ownership transfers 
void* queue_pop_front(struct generic_queue *queue);
//ownership transfers 
void* queue_pop_back(struct generic_queue *queue);
void* queue_remove(struct generic_queue *queue, struct queue_entry *entry);
//for complex objects
int   queue_find(struct generic_queue *queue, void *elem,
		 int (*cmpfunc)(void *elem1,void *elem2));
void   queue_clear(struct generic_queue *queue);
//pretty much queue clear
void   queue_destroy(struct generic_queue *queue);

//copy one queue to another
void queue_cpy(struct generic_queue *dst,struct generic_queue *src);
void queue_push_back_entry(struct generic_queue *queue, struct queue_entry *entry);

//get a specific element
void* queue_get_idx(struct generic_queue *queue, size_t idx);


//convenience comparators
int str_cmp(void *str_a,void *str_b);


#endif
