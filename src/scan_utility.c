#include "scan_utility.h"



#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mape_k.h"
#include "arc_model.h"
#include "scan_scoring.h"
#include "scan_repertoire.h"
#include "timestamp.h"



static void enum_to_arc_value(int enum_value, struct arc_value *value)
{
	value->type = V_INT;
	value->v_int = enum_value;
}


void scan_utility_init(struct knowledge_base *k_b)
{
	struct utility_decision *scan_utility = malloc(sizeof(struct utility_decision));
	utility_decision_init(scan_utility,4);
	utility_decision_add_concern(scan_utility, "ScanRange", 0.25, scan_range_utility);
	utility_decision_add_concern(scan_utility, "ScanTargets",0.25, scan_targets_utility);
	utility_decision_add_concern(scan_utility, "ScanDuration", 0.25, scan_duration_utility);
	utility_decision_add_concern(scan_utility, "ScanComplexity", 0.25, scan_complexity_utility);

	struct named_res *t_u_res = malloc(sizeof(struct named_res));
    named_res_init(t_u_res, "ScanUtility", 1, scan_utility);
    knowledge_base_add_res(k_b, t_u_res);
	
}

void scan_utility(struct analysis *a)
{
	timestamp("[","] ");
    printf("[Utility] Scans [Start]\n");
    
    struct knowledge_base *k_b = &a->az->mape_k->k_b;
    struct scan_repertoire *s_r= knowledge_base_get_res(k_b, "ScanRepertoire");

    struct utility_decision *scan_utility = knowledge_base_get_res(k_b,"ScanUtility");;


	//Scan utility does not change over time, so we only need to do this once really.
	if(scan_utility->num_alternatives == 0)
	{
		
	
		
		for(size_t i=0; i < s_r->num_scans; i++)
		{
			struct scan *sc = &s_r->scans[i];

			struct arc_value scan_values[scan_utility->num_concerns];

			enum_to_arc_value(sc->s_r, &scan_values[0]);
			enum_to_arc_value(sc->s_t, &scan_values[1]);
			enum_to_arc_value(sc->s_d, &scan_values[2]);
			enum_to_arc_value(sc->s_c, &scan_values[3]);

	    

			utility_decision_add_option(scan_utility,sc->name,scan_values,scan_utility->num_concerns);
		
		}
		utility_decision_rank(scan_utility);
	}


    for(int i=0;i<scan_utility->num_alternatives;i++)
	{
		printf("%s:%lg\n",scan_utility->options[i].name, scan_utility->options[i].utility);
	}


	timestamp("[","] ");
    printf("[Utility] Scans [Complete]\n");
	
    analysis_set_status(a, ANALYSIS_DONE);
    a->done(a);
}

