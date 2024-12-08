#include <stddef.h>

#ifndef NAMED_RESOURCE
#define NAMED_RESOURCE


struct named_res
{
    char *name;
    void *res;
    size_t n_items;
    
};


void named_res_init(struct named_res *named_res, char *name,size_t n_items, void *res);
void named_res_destroy(struct named_res *named_res);    
#endif
