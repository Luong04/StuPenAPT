
#include <stddef.h>
#include "arc_value.h"

#ifndef TARGET_SCORING_H
#define TARGET_SCORING_H

enum EXPLOITATION_STATE
{
    ES_None = 0,
    ES_Initial ,
    ES_Escalated,
    ES_CnC,
    ES_Size
};



double num_services_score(size_t num_services);
double num_vulnerabilities_score(size_t num_vulnerabilities);
double num_connections_score(size_t num_connections);
double exploitation_state_score(enum EXPLOITATION_STATE e_s);


enum EXPLOITATION_STATE str_to_exploitation_state(char *e_s_str);


double num_vulnerabilities_utility(struct arc_value *a_v);
double num_services_utility(struct arc_value *a_v);
double num_connections_utility(struct arc_value *a_v);
double exploitation_state_utility(struct arc_value *a_v);


#endif
