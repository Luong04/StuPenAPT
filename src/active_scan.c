#define _XOPEN_SOURCE 600
#define _POSIX_C_SOURCE  200809L
#include "active_scan.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

void active_scan_init(struct active_scan *a_s, struct scan *scan, char *target)
{
	a_s->scan = scan;
	if(target)
	{
		a_s->target = strdup(target);
	}
	else
	{
		a_s->target = NULL;
	}
	a_s->scan_state = AS_WAIT;
}
void active_scan_cleanup(struct active_scan *a_s)
{
	if(a_s->target)
	{
		free(a_s->target);
	}
}

bool active_scan_done(struct active_scan *a_s)
{
	return (a_s->scan_state == AS_DONE);
}

bool active_scan_eq(struct active_scan *a_s, struct active_scan *o_s)
{
	

	if(strcmp(a_s->scan->name,o_s->scan->name) != 0)
	{
		return false;
	}
	
	if(a_s->target && o_s->target)
	{
		if(strcmp(a_s->target,o_s->target)!=0)
		{
			return false;
		}
	}
	if( (!a_s->target && o_s->target) || (a_s->target && !o_s->target))
	{
		return false;
	}

	return true;
}


void past_scan_init(struct past_scan *p_s, char *scan, char *target)
{
	p_s->scan = strdup(scan);
	if(target)
	{
		p_s->target = strdup(target);
	}
	else
	{
		p_s->target = NULL;
	}
}

void past_scan_cleanup(struct past_scan *p_s)
{
	free(p_s->scan);
	if(p_s->target)
	{
		free(p_s->target);
	}
}

