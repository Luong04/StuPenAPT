#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "tool_runner.h"
#include "tokenize.h"
#include "arc_plugin_interface.h"

static char *parse_nmap_host_field(char *host_field, struct arc_model *a_m);
static void parse_nmap_status_field(char *status_field, struct arc_model *a_m, char *component_name);
static void parse_nmap_ports_field(char *ports_field, struct arc_model *a_m, char *component_name);
static void parse_nmap_port_field(char *port_field, struct arc_model *a_m, char *component_name);


static char *next_line(char *output, size_t *num_skip)
{
    char *line = output;
    size_t skip = 0;
    while(output[skip]!='\0')
    {
		skip++;
		if(output[skip]=='\n')
		{
			output[skip]='\0';
			skip++;
			break;
		}
    }
    *num_skip = skip;
    return line;
}

static void parse_nmap_grepable(char *line,struct arc_model *a_m)
{

    char *component_name = NULL;
    
    
    struct tokenizer tab_tokenizer;
    tokenizer_init(&tab_tokenizer, line, "\t");
    char *field_token = next_token(&tab_tokenizer);
    while(field_token)
    {

		char *field_type_pos = strchr(field_token, ':');
		char *field_type = strndup(field_token, field_type_pos-field_token);

		if(strcmp(field_type,"Host")==0)
		{
			if(component_name)
			{
				free(component_name);
			}
			component_name = parse_nmap_host_field(field_type_pos+1,a_m);
		}
		else if(strcmp(field_type,"Status")==0)
		{
			parse_nmap_status_field(field_type_pos+1,a_m,component_name);
		}
		else if(strcmp(field_type,"Ports")==0)
		{
			parse_nmap_ports_field(field_type_pos+1,a_m, component_name);
		}
		free(field_type);
		field_token = next_token(&tab_tokenizer);
    }
    tokenizer_cleanup(&tab_tokenizer);

    if(component_name)
    {
		free(component_name);
    }
}

static char *make_identifier(char *ip, char prefix)
{
    char *component_name = calloc(strlen(ip)+4,1);
    sprintf(component_name, "%c_%s",prefix,ip);
    for(size_t i=0;i<strlen(component_name);i++)
    {
		if(component_name[i] == '.')
		{
			component_name[i] = '_';
		}
    }

    return component_name;
}

static char *parse_nmap_host_field(char *host_field, struct arc_model *a_m)
{

    struct tokenizer whitespace_tok;
    tokenizer_init(&whitespace_tok,host_field, " ");
    char *ip = next_token(&whitespace_tok);
    if(!ip)
    {
		fprintf(stderr,"[nmap] No IP in nmap Host field!\n");
		return NULL;
    }

    char *component_name = make_identifier(ip,'T');
    arc_model_add_component(a_m,component_name);
    
    int nbytes = snprintf(NULL,0,"%s:IP",component_name);
    char ip_buff[nbytes+1];
    snprintf(ip_buff,nbytes+1,"%s:IP",component_name);
    arc_model_assign_property_str(a_m,ip_buff,ip);
    
    char *r_dns_name = next_token(&whitespace_tok);
    if(!r_dns_name  || strlen(r_dns_name) <= 2)
    {
		fprintf(stderr,"[nmap] No reverse DNS name or reverse DNS name is empty in nmap Host field!\n");
	
    }
    else
    {
		//do stuff with r_dns_name
		int nbytes = snprintf(NULL,0,"%s:RDNS",component_name);
		char rdns_buff[nbytes+1];
		snprintf(rdns_buff,nbytes+1,"%s:RDNS",component_name);
		arc_model_assign_property_str(a_m,rdns_buff,r_dns_name);
	
    }


    tokenizer_cleanup(&whitespace_tok);
    return component_name;
}


static void parse_nmap_status_field(char *status_field, struct arc_model *a_m, char *component_name)
{
    char *status = status_field+1;
    int nbytes = snprintf(NULL,0,"%s:Status",component_name);
    char buff[nbytes+1];
    snprintf(buff,nbytes+1,"%s:Status",component_name);
    arc_model_assign_property_str(a_m,buff,status);
}


static void parse_nmap_ports_field(char *ports_field, struct arc_model *a_m, char *component_name)
{
    struct tokenizer commaspace_tok;
    tokenizer_init(&commaspace_tok, ports_field,",");
    char *port_info = next_token(&commaspace_tok);

    int num_ports = 0;
    while(port_info)
    {
		parse_nmap_port_field(port_info+1, a_m, component_name);
		port_info = next_token(&commaspace_tok);
		num_ports++;
    }

    int nbytes = snprintf(NULL,0,"%s:NUM_SERVICES",component_name);
    char ns_buff[nbytes+1];
    snprintf(ns_buff,nbytes+1,"%s:NUM_SERVICES",component_name);
    arc_model_assign_property_int(a_m,ns_buff,num_ports);

    nbytes = snprintf(NULL,0,"%s:NUM_VULNERABILITIES",component_name);
    char nv_buff[nbytes+1];
    snprintf(nv_buff,nbytes+1,"%s:NUM_VULNERABILITIES",component_name);
    arc_model_assign_property_int(a_m,nv_buff,0);

    nbytes = snprintf(NULL,0,"%s:NUM_CONNECTIONS",component_name);
    char nc_buff[nbytes+1];
    snprintf(nc_buff,nbytes+1,"%s:NUM_CONNECTIONS",component_name);
    arc_model_assign_property_int(a_m,nc_buff,1);

    nbytes = snprintf(NULL,0,"%s:EXPLOITATION_STATE",component_name);
    char es_buff[nbytes+1];
    snprintf(es_buff,nbytes+1,"%s:EXPLOITATION_STATE",component_name);
    arc_model_assign_property_str(a_m,es_buff,"None");
    tokenizer_cleanup(&commaspace_tok);
}


static char *next_port_info_chunk(char *str, char until_c)
{
    char *walk_str = str;
    char *chunk = NULL;
    size_t num_chars = 0;
    while(*walk_str!='\0' && *walk_str!=until_c)
    {
		num_chars++;
		walk_str++;
    }

    if(num_chars > 0)
    {
		chunk = strndup(str,num_chars);
	
    }
    return chunk;
}


static char *qualified_iface_property(char *component, char *interface, char *property)
{
    char *qual_name = NULL;
    size_t num_bytes = snprintf(NULL,0,"%s:%s:%s",component,interface,property);
    if(num_bytes > 3)
    {
		qual_name = malloc(num_bytes+1);
		snprintf(qual_name,num_bytes+1,"%s:%s:%s",component,interface,property);
    }

    return qual_name;
}

static void parse_nmap_port_field(char *port_field, struct arc_model *a_m, char *component_name)
{

    char *interface = NULL;
    char *port = next_port_info_chunk(port_field,'/');
    if(port)
    {
		interface = make_identifier(port, 'I');
		arc_model_add_interface(a_m, component_name, interface);
		char *qual_name = qualified_iface_property(component_name, interface, "Port");
		arc_model_assign_property_str(a_m, qual_name, port);
		port_field+=strlen(port);
	
		free(qual_name);
		free(port);
	
    }
    port_field++;
    char *state = next_port_info_chunk(port_field,'/');
    if(state)
    {
		char *qual_name = qualified_iface_property(component_name, interface, "State");
		arc_model_assign_property_str(a_m, qual_name, state);
		port_field+=strlen(state);
	
		free(qual_name);
		free(state);
    }
    port_field++;
    char *protocol = next_port_info_chunk(port_field,'/');
    if(protocol)
    {
		char *qual_name = qualified_iface_property(component_name, interface, "Protocol");
		arc_model_assign_property_str(a_m, qual_name, protocol);
		port_field+=strlen(protocol);
	
		free(qual_name);
		free(protocol);
    }
    port_field++;
    char *owner = next_port_info_chunk(port_field,'/');
    if(owner)
    {

		char *qual_name = qualified_iface_property(component_name, interface, "Owner");
		arc_model_assign_property_str(a_m, qual_name, owner);
		port_field+=strlen(owner);
	
		free(qual_name);
		free(owner);
    }
    port_field++;
    char *service = next_port_info_chunk(port_field,'/');
    if(service)
    {
		char *qual_name = qualified_iface_property(component_name, interface, "Service");
		arc_model_assign_property_str(a_m, qual_name, service);
		port_field+=strlen(service);
		free(qual_name);
		free(service);
    }
    port_field++;
    char *sun_rpc_info = next_port_info_chunk(port_field,'/');
    if(sun_rpc_info)
    {
		port_field+=strlen(sun_rpc_info);
		free(sun_rpc_info);
    }
    port_field++;
    char *version_info = next_port_info_chunk(port_field,'/');
    if(version_info)
    {
		char *qual_name = qualified_iface_property(component_name, interface, "Version");
		arc_model_assign_property_str(a_m, qual_name, version_info);
		port_field+=strlen(version_info);
	
		free(qual_name);
		free(version_info);
    }

    if(interface)
    {
		free(interface);
    }
}

bool net_scan_guard(struct arc_model *a_m, char *args)
{
    bool res = true;
    struct arc_value target_ipr = arc_model_get_property(a_m,"IPR");
    if(target_ipr.type != V_STRING)
    {
		fprintf(stderr,"[nmap] String Property:IPR is required to peform a NetWorkScan\n");
		res = false;
    }

    free(target_ipr.v_str);
    return res;
}

bool net_scan(struct arc_model *a_m, char *args)
{
    /* nmap TargetIPRange -oG - */

    bool res = true;
    struct arc_value target_ipr = arc_model_get_property(a_m,"IPR");
    if(target_ipr.type != V_STRING)
    {
		fprintf(stderr,"[nmap] String Property:IPR is required to peform a NetWorkScan\n");
		res = false;
    }
    else
    {
		

		
		struct tool_runner tool_runner;
		tool_runner_init(&tool_runner, "nmap", 4);
		
		tool_runner_add_arg(&tool_runner,target_ipr.v_str);
		tool_runner_add_arg(&tool_runner,"-A");
		tool_runner_add_arg(&tool_runner,"-oG");
		tool_runner_add_arg(&tool_runner,"-");
		
		tool_runner_run(&tool_runner);
		//tool_runner_print_summary(&tool_runner);;
		char *output = tool_runner.output;
		size_t num_skip = 0;
		char *line = next_line(output, &num_skip);
		output+=num_skip;
		while(line && num_skip > 0)
		{
	    
			if(line[0]!='#')
			{
				parse_nmap_grepable(line, a_m);
			}
	    
			num_skip = 0;
			line = next_line(output, &num_skip);
			output+=num_skip;
		}
		tool_runner_destroy(&tool_runner);
		free(target_ipr.v_str);
    }
	

    
    return res;
}
