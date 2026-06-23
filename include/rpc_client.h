#pragma once

#include "rpc.h"

typedef enum {
    RPC_PROTOCOL_RDP = 0,
    RPC_PROTOCOL_DTP = 1,
    RPC_PROTOCOL_XTX = 2,
} rpc_protocol_t;

extern int rpc_build_request(uint16_t node, uint32_t program, uint32_t procedure, csp_conn_t ** conn, rpc_msg_t **req_msg);
extern void rpc_send(csp_conn_t *conn, rpc_msg_t *msg);
extern void rpc_buffer_free(rpc_msg_t *msg);
extern int rpc_disconnect(csp_conn_t *conn);
extern int rpc_get_reply(csp_conn_t *conn, rpc_msg_t **msg, uint32_t maxresponses, uint32_t extected_idx, uint32_t timeout);

#define RPC_HANDLE_CLIENT_HDR(type, name) \
    void rpc_request_push_##name(type value, rpc_msg_t *msg); \
    type rpc_result_pop_##name(rpc_msg_t *msg);

RPC_HANDLE_CLIENT_HDR(uint8_t, uint8)
RPC_HANDLE_CLIENT_HDR(int8_t, int8)
RPC_HANDLE_CLIENT_HDR(uint16_t, uint16)
RPC_HANDLE_CLIENT_HDR(int16_t, int16)
RPC_HANDLE_CLIENT_HDR(uint32_t, uint32)
RPC_HANDLE_CLIENT_HDR(int32_t, int32)
RPC_HANDLE_CLIENT_HDR(uint64_t, uint64)
RPC_HANDLE_CLIENT_HDR(int64_t, int64)
RPC_HANDLE_CLIENT_HDR(float, float)
RPC_HANDLE_CLIENT_HDR(double, double)
#undef RPC_HANDLE_CLIENT_HDR

extern void rpc_request_push_string(const char *value, rpc_msg_t *msg);
extern void rpc_request_push_buffer(const uint8_t *value, uint16_t len, rpc_msg_t *msg);
extern void rpc_result_pop_string(char *value, rpc_msg_t *msg);
extern void rpc_result_pop_buffer(uint8_t *value, uint16_t *len, rpc_msg_t *msg);
