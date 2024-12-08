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

bool oracle_telnet_guard(struct arc_model *a_m, char *args)
{
    bool res = true;
    char *target = args;
    
    int nbytes = snprintf(NULL,0,"%s:IP",target);
    char ip_buff[nbytes+1];
    snprintf(ip_buff,nbytes+1,"%s:IP",target);
    
    struct arc_value target_ip = arc_model_get_property(a_m,ip_buff);
    if(target_ip.type != V_STRING)
    {
	fprintf(stderr,"[OracleTelnet] Property: %s:IP is required to peform TELNETCrack\n", target);
	res = false;
    }
    else
    {
	struct arc_value v = {.type = V_STRING, .v_str = strdup("telnet")};
	struct list telnet_ifaces = arc_model_find_interfaces(a_m, target, "Service", v);
	free(v.v_str);
	cleanup_str_list(&telnet_ifaces);

    }
    free(target_ip.v_str);

    return res;
}


bool oracle_telnet(struct arc_model *a_m, char *args)
{
    bool res = true;
    char *target = args;
    int nbytes = snprintf(NULL,0,"%s:IP",target);
    char ip_buff[nbytes+1];
    snprintf(ip_buff,nbytes+1,"%s:IP",target);

    struct arc_value v = {.type = V_STRING, .v_str = strdup("telnet")};
    struct list telnet_ifaces = arc_model_find_interfaces(a_m, target, "Service", v);
    //TODO:Later check if this is already known and stuff
    struct list_node *l_n = telnet_ifaces.head;

    if(telnet_ifaces.len == 0)
    {
	res = false;
    }
    else
    {
    
    
	while(l_n)
	{
	    char *i_face = l_n->elem;
	    char qual_name_buff[256];
	    sprintf(qual_name_buff,"%s:%s:user_creds",target,i_face);
	    arc_model_assign_property_str(a_m,qual_name_buff,"[msfadmin:msfadmin]");
	    l_n = l_n->next;
	}
	
    
	//Set the target property to initial access.
	nbytes = snprintf(NULL,0,"%s:EXPLOITATION_STATE",target);
	char es_buff[nbytes+1];
	snprintf(es_buff,nbytes+1,"%s:EXPLOITATION_STATE",target);
	arc_model_assign_property_str(a_m,es_buff,"Initial");	
	
    }
    free(v.v_str);
    cleanup_str_list(&telnet_ifaces);
    
    return res;
    
}


