#include "tool_plugin.h"
#include "arc_model.h"
#include "attack_repertoire.h"
#include "scan_repertoire.h"

#include <stddef.h>
#include <stdint.h>

#ifndef LOADERS_H
#define LOADERS_H

//Tool loaders 
struct tool_plugin *extract_toolset(struct arc_model *toolset_arch, size_t *num_tools);
//capability search
struct tool_capability *find_capability(struct tool_plugin *toolset, size_t num_tools, char *capability);

//Attack repertoire loaders
//Maybe find nicer name for extract
struct attack_step *extract_attack_steps(struct arc_model *attacks_arch, size_t *num_steps,
					 struct tool_plugin *toolset, size_t num_tools);
struct attack_pattern *extract_attack_patterns(struct arc_model *attacks_arch, size_t *num_patterns);
void load_attack_repertoire(struct attack_repertoire *a_r, struct arc_model *attacks_m,
			    struct tool_plugin *toolset, size_t num_tools);

//Scan repertoire loaders
struct scan *extract_scans(struct arc_model *scans_arc, size_t *num_scans,
			   struct tool_plugin *toolset, size_t num_tools);
void load_scan_repertoire(struct scan_repertoire *s_r, struct arc_model *scans_m,
			  struct tool_plugin *toolset, size_t num_tools);



#endif
