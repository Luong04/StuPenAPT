#define _X_OPEN_SOURCE 500
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include "arc_plugin_interface.h"
#include <string.h>

static void cleanup_str_list(struct list *l)
{
    while(l->len > 0)
    {
	struct list_node *l_n = list_rem(l);
	free(l_n->elem);
	free(l_n);
    }
}

bool oracle_ssh_guard(struct arc_model *a_m, char *args)
{
    char *target = args;

    int nbytes = snprintf(NULL,0,"%s:IP",target);
    char ip_buff[nbytes+1];
    snprintf(ip_buff,nbytes+1,"%s:IP",target);
    
    struct arc_value target_ip = arc_model_get_property(a_m,ip_buff);
    if(target_ip.type != V_STRING)
    {
	fprintf(stderr,"[OracleSsh] Property: %s:IP is required to peform SSHCrack\n", target);
	return false;
    }

    struct arc_value v = {.type = V_STRING, .v_str = strdup("ssh")};
    
    struct list ssh_ifaces = arc_model_find_interfaces(a_m, target, "Service", v);
    if(ssh_ifaces.len == 0)
    {
	return false;
    }
    free(v.v_str);
    cleanup_str_list(&ssh_ifaces);
    return true;
    
}


bool oracle_ssh(struct arc_model *a_m, char *args)
{
    printf("[OracleSsh] Done\n");
    return false;
}


