#pragma once

#include <inttypes.h>

#include <csp/csp.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CSP_PORT_RPC_SERVER 9

typedef enum rpc_msg_type_e {
    RPC_MSG_CALL = 0,
    RPC_MSG_REPLY = 1,
} rpc_msg_type_t;

typedef struct rpc_procedure_s {
    uint32_t id;
    const char *name;
    const char *arg_fmt;
    const char *res_fmt;
} rpc_procedure_t;

typedef struct rpc_call_s {
    uint32_t   program;
    uint32_t   procedure;
    uint16_t   data_len;
    uint8_t    data[];
} __attribute__((packed)) rpc_call_t;

typedef struct rpc_reply_s {
    uint16_t   data_len;
    uint8_t    data[];
} __attribute__((packed)) rpc_reply_t;

typedef struct rpc_msg_s {
    uint8_t     type;
    uint32_t    xid;
    union {
        rpc_call_t  call;
        rpc_reply_t reply;
    };
} __attribute__((packed)) rpc_msg_t;

struct rpc_server_s;
typedef struct rpc_server_s rpc_server_t;

typedef void rpc_server_callback_t(rpc_server_t *me, uint32_t program, uint32_t procedure, uint16_t data_len, uint8_t *data, csp_packet_t *result);

typedef rpc_procedure_t * rpc_lookup_procedure_t(uint32_t id);

typedef struct rpc_api_s {
    rpc_lookup_procedure_t *lookup;
} rpc_api_t;

typedef struct client_s {
    csp_conn_t *conn;
    const rpc_api_t *api;
} rpc_client_t;

typedef struct rpc_server_s {
    csp_socket_t sock;
    const rpc_api_t *api;
} rpc_server_t;

typedef union rpc_data_type_u {
    int8_t      i8;
    uint8_t     u8;
    int16_t     i16;
    uint16_t    u16;
    int32_t     i32;
    uint32_t    u32;
    int64_t     i64;
    uint64_t    u64;
} rpc_data_type_t;

extern int rpc_connect(rpc_client_t *me, uint16_t node);
extern int rpc_disconnect(rpc_client_t *me);
extern int rpc_call_invoke(rpc_client_t *me, uint32_t program, uint32_t procedure, void *ret, ...);

extern int rpc_start_server(rpc_server_t *me);
extern int rpc_stop_server(rpc_server_t *me);
extern csp_conn_t * rpc_waitfor_connections(rpc_server_t *me);

extern csp_packet_t * rpc_handle_msg(rpc_server_t *me, csp_packet_t *packet, rpc_server_callback_t *cb);
extern csp_packet_t * rpc_result_prepare(rpc_server_t *me, rpc_msg_t *msg);
extern void rpc_result_push_uint32(rpc_server_t *me, uint32_t value, csp_packet_t *result);
extern void rpc_result_push_int32(rpc_server_t *me, int32_t value, csp_packet_t *result);

#ifdef __cplusplus
}
#endif
