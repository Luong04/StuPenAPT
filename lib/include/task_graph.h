#include <stdint.h>
#include "task.h"

#ifndef TASK_GRAPH_H
#define TASK_GRAPH_H


#define INIT_TASK_NODE_EDGES 8

struct task_dep_node
{
    struct task *task;
    uint32_t *in_edges, *out_edges;
    size_t num_in_edges, num_out_edges;
    size_t max_in_edges, max_out_edges;
    struct task_dep_node *next;
};


void task_dep_node_init(struct task_dep_node *d_n, struct task *t);
//sets the max equal to num and allocates that much memory instead.
void task_dep_node_init_explicit(struct task_dep_node *d_n, struct task *t,
				 size_t max_in_edges, size_t max_out_edges);
void task_dep_node_add_in(struct task_dep_node *d_n, uint32_t t_id);
void task_dep_node_rem_in(struct task_dep_node *d_n, uint32_t t_id);
void task_dep_node_add_out(struct task_dep_node *d_n, uint32_t t_id);
void task_dep_node_rem_out(struct task_dep_node *d_n, uint32_t t_id);
void task_dep_node_destroy(struct task_dep_node *d_n);
void task_dep_node_print(struct task_dep_node *d_n);

struct task_graph
{
    //This is a hash table
    // list of primes: 17, 37, 47, 67, 97
    struct task_dep_node **dep_nodes_hash;
    //how large the hash table should be.
    uint32_t num_cells;
};

void task_graph_init(struct task_graph *d_g, uint32_t init_size);
void task_graph_add(struct task_graph *d_g, struct task_dep_node *d_n);
void task_graph_rem(struct task_graph *d_g, struct task_dep_node *d_n);
void task_graph_destroy(struct task_graph *d_g);
void task_graph_print(struct task_graph *d_g);
struct task_dep_node *task_graph_get(struct task_graph *d_g, uint32_t t_id);


#endif

