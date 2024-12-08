
#include <stdio.h>
#include <stdlib.h>


#include "tool_runner.h"
#include "mape_k.h"
#include "arc_model.h"
#include "arc_system.h"
#include "arc_parse.h"
#include "file_utils.h"

void cleanup_sys(struct arc_system *sys)
{
    
    if(!sys)
    {
		return;
    }

    
    for(size_t i=0;i<sys->num_components;i++)
    {
		if(sys->components[i])
		{
		
			for(size_t j=0;j<sys->components[i]->num_interfaces;j++)
			{
				if(sys->components[i]->interfaces[j])
				{
					for(size_t k=0;k<sys->components[i]->interfaces[j]->num_properties;k++)
					{
						if(sys->components[i]->interfaces[j]->properties[k])
						{
							destroy_property(sys->components[i]->interfaces[j]->properties[k]);
							free(sys->components[i]->interfaces[j]->properties[k]);
						}
					}
					destroy_interface(sys->components[i]->interfaces[j]);
					free(sys->components[i]->interfaces[j]);
				}
			}

			for(size_t j=0;j<sys->components[i]->num_properties;j++)
			{
				if(sys->components[i]->properties[j])
				{
					destroy_property(sys->components[i]->properties[j]);
					free(sys->components[i]->properties[j]);
				}
			}
		    
			destroy_component(sys->components[i]);
			free(sys->components[i]);
		}
    }

    for(size_t i=0;i<sys->num_invocations;i++)
    {
		if(sys->invocations[i])
		{
			free(sys->invocations[i]);
		}
    }

    for(size_t i=0;i<sys->num_properties;i++)
    {
		if(sys->properties[i])
		{
			destroy_property(sys->properties[i]);
			free(sys->properties[i]);
		}
    }
    
    destroy_system(sys);
    free(sys);
}


void load_arc_model(char *arc_file, struct arc_model *a_m)
{
    char *text = read_file(arc_file);
    struct arc_system *system=arc_parse(text);
    free(text);

    if(!system)
    {
		fprintf(stderr,"Could not parse architecture file, exiting\n");
		exit(-1);
    }
    init_model(a_m, system);
}
