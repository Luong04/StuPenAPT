#include <stddef.h>


#include "scan_repertoire.h"

#ifndef ACTIVE_SCAN_H
#define ACTIVE_SCAN_H

enum ACTIVE_SCAN_STATE {AS_WAIT = 0 , AS_RUN, AS_DONE};


//Looks like this doesn't need a lock. Well maybe it does, recheck after implementing reporting
struct active_scan
{
	struct scan *scan;
    char *target;
    enum ACTIVE_SCAN_STATE scan_state;
    
};
void active_scan_init(struct active_scan *a_s, struct scan *scan, char *target);
void active_scan_cleanup(struct active_scan *a_s);
bool active_scan_done(struct active_scan *a_s);
bool active_scan_eq(struct active_scan *a_s, struct active_scan *o_s);


#define PENTEST_STATE_INIT_NUM_PAST_SCANS 8

struct past_scan
{
    char *scan;
    char *target;
};

void past_scan_init(struct past_scan *p_s, char *scan, char *target);
void past_scan_cleanup(struct past_scan *p_s);

#endif
