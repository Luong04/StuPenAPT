#include "tool_plugin.h"
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


void tool_plugin_init(struct tool_plugin *t_plugin, char *tool_name, size_t num_caps)
{
    t_plugin->name = tool_name;
    t_plugin->num_caps = num_caps;
    t_plugin->caps = malloc(t_plugin->num_caps*sizeof(struct tool_capability));
}


void tool_plugin_destroy(struct tool_plugin *t_plugin)
{
    for(size_t i=0;i<t_plugin->num_caps;i++)
    {
	free(t_plugin->caps[i].name);
    }

    free(t_plugin->caps);
    dlclose(t_plugin->handle);
}


void load_tool_plugin(struct tool_plugin *t_plugin, char *plugin_file)
{
    void *handle;
    tool_name_fn name_fn;
    tool_interface_fn interface_fn;

    char *tool_name = NULL;
    size_t num_caps = 0;

    dlerror();
    handle = dlopen(plugin_file, RTLD_NOW);
    

    if (!handle )
    {
	fprintf(stderr, "Couldn't not dlopen %s exiting\n",plugin_file);
	printf("%s\n",dlerror());
	return;
    }
    t_plugin->handle = handle;

    
    name_fn = dlsym(handle, "tool_name");   
    if(!name_fn)
    {
	fprintf(stderr, "Couldn't not resolve tool_name symbol\n");
	return;
    }
    tool_name = name_fn();

    
    interface_fn = dlsym(handle, "tool_interface");
    if (!interface_fn)
    {
	fprintf(stderr, "Couldn't not resolve tool_interface symbol\n");
	return;
    }
	
    char **interface = interface_fn(&num_caps);
    tool_plugin_init(t_plugin, tool_name, num_caps);
    for(int i=0;i<t_plugin->num_caps;i++)
    {
	char *fn_name = strchr(interface[i],':');
	if(fn_name && fn_name[1]!='\0')
	{
	    t_plugin->caps[i].name = strndup(interface[i],fn_name+1-interface[i]);
	    t_plugin->caps[i].run = dlsym(handle,fn_name+1);
	    if(!t_plugin->caps[i].run)
	    {
		fprintf(stderr,"Could not resolve function: %s\n",fn_name);
		return;
	    }
	    
	}
    }
}
