#pragma once

#include "rpc.h"

typedef struct rpc_proc_arg_s {
    const char * const name;
    const uint8_t type;
    const char * const descr;
} rpc_proc_arg_t;

/**
 * @brief RPC procedure object type
 * 
 * This type specifies a RPC function which is bound
 * into the RPC system.
 * 
 */
SLIST_HEAD( rpc_procedure_list_s, rpc_procedure_s );
typedef struct rpc_procedure_list_s rpc_procedure_list_t;

struct rpc_server_s;
typedef struct rpc_server_s rpc_server_t;

typedef struct rpc_server_s {
    csp_socket_t sock;
} rpc_server_t;

/* Main loop server side methods */
extern rpc_server_t *global_rpc_server;
extern void rpc_server_main(rpc_server_t *me);
extern int rpc_dispatch_program(uint32_t program, uint32_t procedure, csp_conn_t *conn, rpc_msg_t *call);

/* Server side call handler methods */
extern rpc_msg_t *rpc_result_prepare(rpc_msg_t *call, uint32_t amount, uint32_t idx);
extern void rpc_send_reply(csp_conn_t *conn, rpc_msg_t *reply);

#define RPC_HANDLE_SERVER_HDR(type, name) \
    void rpc_result_push_##name(type value, rpc_msg_t *msg); \
    type rpc_request_pop_##name(rpc_msg_t *msg, uint16_t *offset);

RPC_HANDLE_SERVER_HDR(uint8_t, uint8)
RPC_HANDLE_SERVER_HDR(int8_t, int8)
RPC_HANDLE_SERVER_HDR(uint16_t, uint16)
RPC_HANDLE_SERVER_HDR(int16_t, int16)
RPC_HANDLE_SERVER_HDR(uint32_t, uint32)
RPC_HANDLE_SERVER_HDR(int32_t, int32)
RPC_HANDLE_SERVER_HDR(uint64_t, uint64)
RPC_HANDLE_SERVER_HDR(int64_t, int64)
RPC_HANDLE_SERVER_HDR(float, float)
RPC_HANDLE_SERVER_HDR(double, double)
#undef RPC_HANDLE_SERVER_HDR

extern void rpc_request_pop_string(char *value, rpc_msg_t *msg, uint16_t *offset);
extern void rpc_request_pop_buffer(uint8_t *value, uint16_t *len, rpc_msg_t *msg, uint16_t *offset);
extern void rpc_result_push_string(const char *value, rpc_msg_t *msg);
extern void rpc_result_push_buffer(const uint8_t *value, uint16_t len, rpc_msg_t *msg);
