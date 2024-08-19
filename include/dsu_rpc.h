#pragma once

#include <inttypes.h>

#include <csp/csp.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum dsu_rpc_procedure_e {
    DSU_PROCEDURE_OPEN_FIFO = 0,
    DSU_PROCEDURE_FOO = 1,
} dsu_rpc_procedure_t;

typedef enum rpc_msg_type_e {
    RPC_MSG_CALL = 0,
    RPC_MSG_REPLY = 1,
} rpc_msg_type_t;

typedef enum rpc_program_e {
    RPC_PROGRAM_DSM = 0x10000000,
} rpc_program_t;

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

typedef struct dsu_func_open_fifo_res_s {
    int res;
} dsu_func_open_fifo_res_t;

typedef struct dsu_rpc_client_s {
    csp_conn_t *conn;
} dsu_rpc_client_t;

typedef struct dsu_rpc_server_s {
    csp_socket_t sock;
} dsu_rpc_server_t;

typedef void dsu_rpc_server_callback_t(uint32_t program, uint32_t procedure, uint16_t data_len, uint8_t *data, csp_packet_t *result);

typedef union rpc_data_type_u {
    int8_t      i8;
    uint8_t     u8;
    int16_t     i16;
    uint16_t    u16;
    int32_t     i32;
    uint32_t    u32;
} rpc_data_type_t;

extern int dsu_rpc_connect(dsu_rpc_client_t *me, uint16_t node);
extern int dsu_rpc_disconnect(dsu_rpc_client_t *me);
extern int dsu_rpc_call_invoke(dsu_rpc_client_t *me, dsu_rpc_procedure_t procedure, void *ret, ...);
extern int dsu_rpc_start_server(dsu_rpc_server_t *me);
extern int dsu_rpc_stop_server(dsu_rpc_server_t *me);
extern csp_conn_t * dsu_rpc_waitfor_connections(dsu_rpc_server_t *me);
extern csp_packet_t * dsu_rpc_handle_msg(dsu_rpc_server_t *me, csp_packet_t *packet, dsu_rpc_server_callback_t *cb);
extern csp_packet_t * dsu_rpc_result_prepare(rpc_msg_t *msg);
extern void dsu_rpc_result_push_int(int value, csp_packet_t *result);

#ifdef __cplusplus
}
#endif
