#include <stddef.h>
#include <stdint.h>

#ifndef RBTREE_H
#define RBTREE_H


/*
NOTE: We only hold pointers to data, it is the user's responsibility to ensure
the data pointed by the tree pointers is not invalidated while tree operations
are running.
*/

/*
NOTE: We assume data values are unique, i.e., abort insertions with duplicates 
*/

//Comparison function pointer
typedef int (*rb_cmp_func)(const void*, const void*);

#define BLACK 0
#define RED 1





//Tree node struct
struct rb_node
{
    // set to RED or BLACK for red or black
    uint8_t color;
    //We don't hold data just pointers so data can be whatever.
    void *data;
    //left and right subtrees
    struct rb_node *left, *right;
    //parent pointer
    struct rb_node *parent;
};


//Tree struct
struct rb_tree
{
    //root node
    struct rb_node *root;
    //comparison function
    rb_cmp_func cmp;
    size_t size;
};


//Exposed functions to manipulate the tree
void        rb_tree_init(struct rb_tree *tree, rb_cmp_func cmp);
//returns 0 on success a negative value on failure
int        rb_tree_insert(struct rb_tree *tree, void *data);
//returns a pointer to the data or NULL if no data with that value was found
void*      rb_tree_find(struct rb_tree *tree, void *data);
//returns 0 on success or a negative value on failure
int        rb_tree_delete(struct rb_tree *tree, void *data);
void       rb_tree_destroy(struct rb_tree *tree);


#endif
