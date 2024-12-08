#include "arc_model.h"
#include "probe.h"
#include "scan_repertoire.h"
#include "active_scan.h"
#include "task.h"

#ifndef SCAN_PROBE_H
#define SCAN_PROBE_H


struct scan_probe
{
    struct active_scan *a_s;
};


void scan_probe_init(struct probe *probe);
void *scan_probe_run(struct probe *probe);
void scan_probe_destroy(struct probe *probe);
void scan_probe_control(struct task *t);

//dedicated functions to call on a probe
void scan_probe_setup(struct scan_probe *s_p, struct active_scan *a_s);

#endif
