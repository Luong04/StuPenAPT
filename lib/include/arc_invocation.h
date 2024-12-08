#include "arc_component.h"
#include "arc_interface.h"


#ifndef ARC_INVOCATION
#define ARC_INVOCATION


struct arc_invocation
{
    struct arc_component *from, *to;
    struct arc_interface *iface;
};


void init_invocation(struct arc_invocation *invo, struct arc_interface *iface,
		     struct arc_component *from, struct arc_component *to);

void print_invocation(struct arc_invocation *invo);

//no copying makes sense as the copied pointers belong to the system or to the caller.


#endif
