#include "scan_repertoire.h"


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void scan_init(struct scan *s, char *name, char *MATTACK, cap_execute run, cap_guard guard,
			   enum SCAN_RANGE s_r, enum SCAN_TARGETS s_t,
			   enum SCAN_DURATION s_d, enum SCAN_COMPLEXITY s_c)
{
    strncpy(s->name, name, sizeof(s->name));
    strncpy(s->MATTACK, MATTACK, sizeof(s->MATTACK));
    s->run = run;
    s->guard = guard;
	s->s_r = s_r;
	s->s_t = s_t;
	s->s_d = s_d;
	s->s_c = s_c;
}

void scan_print(struct scan *s)
{
    printf("%s:\n", s->name);
    printf(" - MITRE ATTCK: %s\n", s->MATTACK);
}



void scan_repertoire_init(struct scan_repertoire *s_r)
{
    s_r->num_scans = 0;
    s_r->max_scans = SCAN_REPERTOIRE_INIT_NUM_SCANS;
    s_r->scans = malloc(s_r->max_scans * sizeof(struct scan));
}

void scan_repertoire_destroy(struct scan_repertoire *s_r)
{
    free(s_r->scans);
}
void scan_repertoire_add_scan(struct scan_repertoire *s_r, struct scan *s)
{
    if(s_r->num_scans >= s_r->max_scans)
    {
	s_r->max_scans *= 2;
	s_r->scans = realloc(s_r->scans, s_r->max_scans * sizeof(struct scan));
    }

    s_r->scans[s_r->num_scans] = *s;
    s_r->num_scans++;
}


void scan_repertoire_print(struct scan_repertoire *s_r)
{
    printf("[Scans]\n");
    for(size_t i=0;i<s_r->num_scans;i++)
    {
	scan_print(&s_r->scans[i]);
    }
}


struct scan *scan_repertoire_get_scan(struct scan_repertoire *s_r, char *scan)
{
	for(size_t i=0;i < s_r->num_scans;i++)
    {
		if(strcmp(s_r->scans[i].name, scan) == 0)
		{
			return &s_r->scans[i];
		}
    }
	
    return NULL;
}

