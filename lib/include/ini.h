#include <stddef.h>
#include <stdlib.h>

#ifndef INI_H
#define INI_H


#define INI_INIT_NUM_PROPERTIES 4
#define INI_INIT_NUM_SUBSECTIONS 4
#define INI_INIT_NUM_SECTIONS 4
enum INI_PROP_TYPE {INI_P_STR=0, INI_P_NUM, INI_P_INVALID };

struct ini_property
{
    char *key;
    enum INI_PROP_TYPE p_type;
    union
    {
	char *str;
	double num;
    }value;
};

void ini_str_property_init(struct ini_property *i_p, char *key,  char *str);
void ini_num_property_init(struct ini_property *i_p, char *key,  double num);
void ini_property_destroy(struct ini_property *i_p);



struct ini_subsection
{
    char *name;
    struct ini_property *properties;
    size_t num_properties;
    size_t max_properties;
};

void ini_subsection_init(struct  ini_subsection *i_ss, char *name);
void ini_subsection_add(struct ini_subsection *i_ss,
			struct ini_property *p);
void ini_subsection_destroy(struct ini_subsection *i_ss);

/*
  note how the section name and properties could
  literally be replaced by a subsection if we wanted to
*/

struct ini_section
{

    char *name;
    struct ini_property *properties;
    size_t num_properties;
    size_t max_properties;

    struct ini_subsection *subsections;
    size_t num_subsections;
    size_t max_subsections;
};

void ini_section_init(struct ini_section *i_s, char *name);
void ini_section_add_property(struct ini_section *i_s,
			      struct ini_property *p);
void ini_section_add_subsection(struct ini_section *i_s,
				struct ini_subsection *i_ss);
void ini_section_destroy(struct ini_section *i_s);
struct ini_subsection *ini_section_find_subsection(struct ini_section *i_s,
						   char *name);

struct ini 
{
    struct ini_property *global_props;
    size_t num_global_props;
    size_t max_global_props;

    struct ini_section *sections;
    size_t num_sections;
    size_t max_sections;

};
#endif

void ini_init(struct ini *ini);
void ini_add_property(struct ini *ini, struct ini_property *i_p);
void ini_add_section(struct ini *ini, struct ini_section *i_s);
void ini_destroy(struct ini *ini);
void ini_dump(struct ini *ini);
struct ini_section *ini_find_section(struct ini *ini, char *name);
