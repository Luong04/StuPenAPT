#include <stddef.h>


#ifndef LIST_H
#define LIST_H

struct list_node
{
    void *elem;
    struct list_node *next;
};

void list_node_init(struct list_node *l_n, void *elem);

struct list
{
    struct list_node *head, *tail;
    size_t len;
};

void list_init(struct list *l);
void list_tail_add(struct list *l, struct list_node *l_n);
void list_head_add(struct list *l, struct list_node *l_n);
struct list_node *list_rem(struct list *l);

#endif



