#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>


#ifndef CONNECTION_H
#define CONNECTION_H

struct connection;

typedef    bool  (*c_init) (struct connection *conn, void *args);    
typedef  ssize_t (*c_send) (struct connection *conn, void *buff, size_t len);
typedef    bool  (*c_dial) (struct connection *conn);
typedef    bool  (*c_listen) (struct connection *conn);
typedef  ssize_t (*c_recv) (struct connection *conn, void *buff, size_t len);
typedef    bool  (*c_destroy) (struct connection *conn, void *args);    

enum CONNECTION_TYPE
{
    CONN_T_TCP=0,
    CONN_T_UDP,
    CONN_T_UNIX,
    CONN_T_TELNET,
    CONN_T_NUM_KNOWN_TYPES
};

enum CONNECTION_ROLE
{
    CONN_R_LISTEN=0,
    CONN_R_DIAL
};

struct connection
{
    uint8_t c_t;
    c_init init_fn;
    void *init_args;
    c_dial dial_fn;
    c_listen listen_fn;
    c_send send_fn;
    c_recv recv_fn;
    c_destroy destroy_fn;
    void *destroy_args;
    //stores connection specific info
    void *ctx;
    
};


bool connection_init(struct connection *conn, enum CONNECTION_TYPE c_t, char *init_args);
bool connection_destroy(struct connection *conn);

//In case we want to deal with with a connection we don't know how to handle
bool connection_init_with_params(struct connection *conn, uint8_t c_t,
				 c_init cinit, void *c_init_args,
				 c_destroy cdestroy, void *c_destroy_args,
				 c_dial cdial, c_listen clisten,
				 c_send csend, c_recv crecv);
bool connection_dial(struct connection *conn);
bool connection_listen(struct connection *conn);
ssize_t connection_send(struct connection *conn, void *buff, size_t num_bytes);
ssize_t connection_recv(struct connection *conn, void *buff, size_t num_bytes); 


#endif

