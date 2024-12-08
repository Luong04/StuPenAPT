#include <pthread.h>

#ifndef PROBE_H
#define PROBE_H


enum PROBE_STATUS{PROBE_CREATED=0,PROBE_RUNNING,PROBE_STOPPED,PROBE_DONE, PROBE_ST_UNDEF};

struct monitor;

struct probe
{
    //state
    pthread_mutex_t lock;
    void *ctx;
    enum PROBE_STATUS status;
    struct monitor *mon;

    //function pointer interface
    void (*probe_init)(struct probe *probe);
    void* (*probe_run)(struct probe *probe);
    void (*probe_destroy)(struct probe *probe);
    void (*probe_done)(struct probe *probe);
};

typedef void (*probe_init)(struct probe *probe);
typedef void *(*probe_run)(struct probe *probe);
typedef void (*probe_destroy)(struct probe *probe);
typedef void (*probe_done)(struct probe *probe);

//high level interface
void init_probe(struct probe *probe, probe_init init, probe_run run,
		probe_destroy destroy, probe_done done, void *ctx);
void start_probe(struct probe *probe);
void destroy_probe(struct probe *probe);

//safe accessors
void *probe_get_ctx(struct probe *probe);
void probe_set_status(struct probe *probe, enum PROBE_STATUS status);
enum PROBE_STATUS probe_get_status(struct probe *probe);
struct monitor *probe_get_monitor(struct probe *probe);
void probe_set_monitor(struct probe *probe, struct monitor *mon);

#endif
