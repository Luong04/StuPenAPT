#include "arc_property.h"
#include <stddef.h>

#ifndef ARC_INTERFACE
#define ARC_INTERFACE


#define NUM_PROPERTIES_INIT 32

struct arc_interface
{
    char *if_name;
    //No ownership
    struct arc_property **properties;
    size_t num_properties;
    size_t max_properties;
    
};

void init_interface(struct arc_interface *interface, char *if_name);
void iface_add_property(struct arc_interface *interface,
			struct arc_property *prop);
struct arc_property* iface_rem_property(struct arc_interface *iface,
					char *prop_name);
void destroy_interface(struct arc_interface *interface);
struct arc_interface *clone_interface(struct arc_interface *iface);

struct arc_property* iface_find_property(struct arc_interface *iface,
			   char *prop_name);

void print_interface(struct arc_interface *interface);





#endif
