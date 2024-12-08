#include <stddef.h>
#include "tool_plugin.h"
#include "scan_scoring.h"
#ifndef SCAN_REPERTOIRE_H
#define SCAN_REPERTOIRE_H

struct scan
{
    char name[64];
    char MATTACK[12];
    cap_execute run;
    cap_guard guard;

	//stats
	enum SCAN_RANGE s_r;
	enum SCAN_TARGETS s_t;
	enum SCAN_DURATION s_d;
	enum SCAN_COMPLEXITY s_c;
	
};

void scan_init(struct scan *s, char *name, char *MATTACK, cap_execute run, cap_guard guard,
			   enum SCAN_RANGE s_r, enum SCAN_TARGETS s_t,
			   enum SCAN_DURATION s_d, enum SCAN_COMPLEXITY s_c);
void scan_print(struct scan *s);


#define SCAN_REPERTOIRE_INIT_NUM_SCANS 16

struct scan_repertoire
{
    struct scan *scans;
    size_t num_scans;
    size_t max_scans;
};




void scan_repertoire_init(struct scan_repertoire *s_r);
void scan_repertoire_destroy(struct scan_repertoire *s_r);
void scan_repertoire_add_scan(struct scan_repertoire *s_r, struct scan *s);
void scan_repertoire_print(struct scan_repertoire *s_r);


struct scan *scan_repertoire_get_scan(struct scan_repertoire *s_r, char *scan);


#endif
