#include "connection.h"

#ifndef CONNECTION_CACHE
#define CONNECTION_CACHE

struct conn_entry
{
    struct connection *conn;
    char *ip;
    char *port;	
};

void conn_entry_init(struct conn_entry *c_e, struct connection *c, char *ip, char *port);
void conn_entry_cleanup(struct conn_entry *c_e);

#define CONN_CACHE_INIT_MAX_CONNECTIONS 8

struct conn_cache
{
    struct conn_entry  *entries;
    size_t num_connections;
    size_t max_connections;
};


void connection_cache_init();
struct connection *connection_cache_add(uint8_t c_t, char *args, char *ip, char *port);
void connection_cache_rem(struct connection *c);
struct connection *connection_cache_lookup(char *ip, char *port);
void connection_cache_cleanup();

#endif
