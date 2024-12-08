#ifndef ARC_VALUE_H
#define ARC_VALUE_H


enum value_type {V_INT=0,V_DOUBLE,V_STRING,V_INVALID};

struct arc_value
{
    enum value_type type;
    union
    {
	int v_int;
	double v_dbl;
	char *v_str;
    };
};




#endif
