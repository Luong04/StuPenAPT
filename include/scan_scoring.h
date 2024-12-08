#include "arc_value.h"

#ifndef SCAN_SCORING_H
#define SCAN_SCORING_H


enum SCAN_RANGE
{
	SR_Network = 0,
	SR_Host,
	SR_Local,
	SR_Size
};

enum SCAN_TARGETS
{
	ST_Network =0,
	ST_Multiple,
	ST_One,
	ST_Size
};

enum SCAN_DURATION
{
	SD_Quick = 0,
	SD_Medium,
	SD_Long,
	SD_Guessing,
	SD_Size
};

enum SCAN_COMPLEXITY
{
	SC_Low,
	SC_High,
	SC_Size
};


double scan_range_score(enum SCAN_RANGE s_r);
double scan_targets_score(enum SCAN_TARGETS s_t);
double scan_duration_score(enum SCAN_DURATION s_d);
double scan_complexity_score(enum SCAN_COMPLEXITY s_c);


enum SCAN_RANGE str_to_scan_range(char *s_r_str);
enum SCAN_TARGETS str_to_scan_targets(char *s_t_str);
enum SCAN_DURATION str_to_scan_duration(char *s_d_str);
enum SCAN_COMPLEXITY str_to_scan_complexity(char *s_c_str);

double scan_range_utility(struct arc_value *a_v);
double scan_targets_utility(struct arc_value *a_v);
double scan_duration_utility(struct arc_value *a_v);
double scan_complexity_utility(struct arc_value *a_v);


#endif
