#include <stddef.h>
#include <stdbool.h>

#ifndef TOOL_INFO_H
#define TOOL_INFO_H


struct arc_model;
typedef bool (*cap_execute)(struct arc_model *a_m, char *args);
typedef bool (*cap_guard)(struct arc_model *a_m, char *args);

struct tool_capability
{
    char *name;
    cap_execute run;
    cap_guard guard;
};


typedef char *(*tool_name_fn)(void);
typedef char **(*tool_interface_fn)(size_t *num_caps);



struct tool_plugin
{
    void *handle;
    char *name;
    struct tool_capability *caps;
    size_t num_caps;
};

void tool_plugin_init(struct tool_plugin *t_plugin, char *tool_name, size_t num_caps);
void tool_plugin_destroy(struct tool_plugin *t_plugin);
void load_tool_plugin(struct tool_plugin *t_plugin, char *plugin_file);
#endif
