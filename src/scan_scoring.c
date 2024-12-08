#include "scan_scoring.h"

#include <string.h>



double scan_range_score(enum SCAN_RANGE s_r)
{
	static const double s_r_score_tbl[SR_Size] = { 1.0, 0.73, 0.32 };
	if(s_r >= SR_Size || s_r < 0)
    {
		return 0;
    }
    return s_r_score_tbl[s_r];
}

double scan_targets_score(enum SCAN_TARGETS s_t)
{
	static const double s_t_score_tbl[ST_Size] = { 1.0, 0.73, 0.32 };
	if(s_t >= ST_Size || s_t < 0)
    {
		return 0;
    }
    return s_t_score_tbl[s_t];
}
	
double scan_duration_score(enum SCAN_DURATION s_d)
{
	static const double s_d_score_tbl[SD_Size] = { 1.0, 0.73, 0.31, 0.12 };
	if(s_d >= SD_Size || s_d < 0)
    {
		return 0;
    }
    return s_d_score_tbl[s_d];
}


double scan_complexity_score(enum SCAN_COMPLEXITY s_c)
{
	static const double s_c_score_tbl[SC_Size] = { 1.0, 0.73 };
	if(s_c >= SC_Size || s_c < 0)
    {
		return 0;
    }
    return s_c_score_tbl[s_c];
}

enum SCAN_RANGE str_to_scan_range(char *s_r_str)
{
	if(strcmp(s_r_str,"Network") == 0)
    {
		return SR_Network;
    }
    else if(strcmp(s_r_str,"Host") == 0)
    {
		return SR_Host;
    }
    else if(strcmp(s_r_str,"Local") == 0)
    {
		return SR_Local;
    }
    else
    {
		return SR_Size;
    }
}

enum SCAN_TARGETS str_to_scan_targets(char *s_t_str)
{
	if(strcmp(s_t_str,"Network") == 0)
    {
		return ST_Network;
    }
    else if(strcmp(s_t_str,"Multiple") == 0)
    {
		return ST_Multiple;
    }
    else if(strcmp(s_t_str,"One") == 0)
    {
		return ST_One;
    }
    else
    {
		return ST_Size;
    }
}
enum SCAN_DURATION str_to_scan_duration(char *s_d_str)
{
	if(strcmp(s_d_str,"Quick") == 0)
    {
		return SD_Quick;
    }
    else if(strcmp(s_d_str,"Medium") == 0)
    {
		return SD_Medium;
    }
    else if(strcmp(s_d_str,"Long") == 0)
    {
		return SD_Long;
    }
    else if(strcmp(s_d_str,"Guessing") == 0)
    {
		return SD_Guessing;
    }
    else
    {
		return SD_Size;
    }
}
enum SCAN_COMPLEXITY str_to_scan_complexity(char *s_c_str)
{
	if(strcmp(s_c_str,"Low") == 0)
    {
		return SC_Low;
    }
    else if(strcmp(s_c_str,"High") == 0)
    {
		return SC_High;
	}
    else
    {
		return SC_Size;
    }
}


double scan_range_utility(struct arc_value *a_v)
{
	return scan_range_score(a_v->v_int);
}
double scan_targets_utility(struct arc_value *a_v)
{
	return scan_targets_score(a_v->v_int);
}
double scan_duration_utility(struct arc_value *a_v)
{
	return scan_duration_score(a_v->v_int);
}
double scan_complexity_utility(struct arc_value *a_v)
{
	return scan_complexity_score(a_v->v_int);
}
