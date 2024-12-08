#include <pthread.h>

#ifndef ANALYSIS_H
#define ANALYSIS_H


enum ANALYSIS_STATUS {ANALYSIS_STARTED,ANALYSIS_RUNNING, ANALYSIS_DONE, ANALYSIS_STATUS_UNDEF};

struct analysis;
struct analyzer;

typedef void (*analysis_run)(struct analysis *a);
typedef void (*analysis_done)(struct analysis *a);

struct analysis
{
    pthread_mutex_t lock;
    void *ctx;
    void *res;
    enum ANALYSIS_STATUS status;
    
    //pointer to the analyzer
    struct analyzer *az;

    //function pointer interface
    analysis_run run;
    analysis_done done;
};

//high level interface
void init_analysis(struct analysis *ana, analysis_run run, analysis_done done, void *ctx);
void run_analysis(struct analysis *ana);
void destroy_analysis(struct analysis *ana);

//safe accessors
void *analysis_get_ctx(struct analysis *ana);
void analysis_set_status(struct analysis *ana, enum ANALYSIS_STATUS status);
enum ANALYSIS_STATUS analysis_get_status(struct analysis *ana);
struct analyzer *analysis_get_analyzer(struct analysis *ana);
void analysis_set_analyzer(struct analysis *ana, struct analyzer *az);
void *analysis_get_result(struct analysis *ana);


#endif
