#include "target_scoring.h"

#include <string.h>
#include <math.h>



double num_vulnerabilities_utility(struct arc_value *a_v)
{

	if(a_v->type != V_INT || a_v->v_int < 0)
	{
		return 0.0;
	}
	else 
	{
		double value = a_v->v_int * a_v->v_int;
		if(value <= 10)
		{
			return (double)value/10.0;
		}
		return 1.0;
	}
}

double num_services_utility(struct arc_value *a_v)
{

	if(a_v->type != V_INT || a_v->v_int < 0)
	{
		return 0.0;
	}
	else 
	{
		double value = a_v->v_int;
		if(value <= 10)
		{
			return (double)value/10.0;
		}
		return 1.0;
	}
}

double num_connections_utility(struct arc_value *a_v)
{

	if(a_v->type != V_INT || a_v->v_int < 0)
	{
		return 0.0;
	}
	else 
	{
		double value = pow((double)a_v->v_int,1.5);
		if(value <= 10)
		{
			return (double)value/10.0;
		}
		return 1.0;
	}
}

double exploitation_state_utility(struct arc_value *a_v)
{
	if(a_v->type != V_STRING)
	{
		return 0.0;
	}
	else 
	{
		if(strcmp(a_v->v_str,"None" ) == 0)
		{
			return 0.73;
		}
		else if(strcmp(a_v->v_str, "Initial" ) == 0)
		{
			return 0.9;
		}
		else if(strcmp(a_v->v_str, "Escalated") == 0)
		{
			return 1.0;
		}
		else if(strcmp(a_v->v_str, "CnC") == 0)
		{
			return 0.23;
		}
		return 0.0;
	}
}



double num_services_score(size_t num_services)
{
    if(num_services >= 10)
    {
		return 1.0;
    }
    else
    {
		return (double)num_services/10.0;
    }
}

double num_vulnerabilities_score(size_t num_vulnerabilities)
{
    double nv_sq = num_vulnerabilities*num_vulnerabilities;
    if(nv_sq > 100)
    {
		return 1.0;
    }
    else
    {
		return (double)nv_sq/100;
    }
    
}
double num_connections_score(size_t num_connections)
{
    double nc_sc = num_connections+(double)num_connections*0.5;
    if(nc_sc > 15)
    {
		return 1.0;
    }
    else
    {
		return (double)nc_sc/15.0;
    }
}

double exploitation_state_score(enum EXPLOITATION_STATE e_s)
{
    static const double e_s_score_tbl[ES_Size] = {0.4, 0.85, 0.62, 0.2};
    if(e_s >= ES_Size || e_s < 0)
    {
		return 0;
    }
    return e_s_score_tbl[e_s];
}


enum EXPLOITATION_STATE str_to_exploitation_state(char *e_s_str)
{
    if(strcmp(e_s_str,"None") == 0)
    {
		return ES_None;
    }
    else if(strcmp(e_s_str,"Initial") == 0)
    {
		return ES_Initial;
    }
    else if(strcmp(e_s_str,"Escalated") == 0)
    {
		return ES_Escalated;
    }
    else if(strcmp(e_s_str,"CnC") == 0)
    {
		return ES_CnC;
    }
    else
    {
		return ES_Size;
    }

}

