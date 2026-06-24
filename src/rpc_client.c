#include <endian.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <csp/csp.h>

#include "rpc.h"

#define be8toh(x) (x)
#define htobe8(x) (x)

#define RPC_HANDLE_CLIENT_IMPL(type, name, size) \
    void rpc_request_push_##name(type value, rpc_msg_t *msg) { \
        rpc_call_t *call = &msg->call; \
        value = htobe##size(value); \
        memcpy(&call->data[call->data_len], &value, sizeof(type)); \
        call->data_len += sizeof(type); \
    } \
    type rpc_result_pop_##name(rpc_msg_t *msg) { \
        rpc_reply_t *reply = &msg->reply; \
        type value; \
        memcpy(&value, &reply->data[reply->data_len], sizeof(type)); \
        value = be##size##toh(value); \
        reply->data_len += sizeof(type); \
        return value; \
    }

RPC_HANDLE_CLIENT_IMPL(uint8_t, uint8, 8)
RPC_HANDLE_CLIENT_IMPL(int8_t, int8, 8)
RPC_HANDLE_CLIENT_IMPL(uint16_t, uint16, 16)
RPC_HANDLE_CLIENT_IMPL(int16_t, int16, 16)
RPC_HANDLE_CLIENT_IMPL(uint32_t, uint32, 32)
RPC_HANDLE_CLIENT_IMPL(int32_t, int32, 32)
RPC_HANDLE_CLIENT_IMPL(uint64_t, uint64, 64)
RPC_HANDLE_CLIENT_IMPL(int64_t, int64, 64)
RPC_HANDLE_CLIENT_IMPL(float, float, 32)
RPC_HANDLE_CLIENT_IMPL(double, double, 64)
#undef RPC_HANDLE_CLIENT_IMPL

void rpc_request_push_string(const char *value, rpc_msg_t *msg) {

    rpc_call_t *call = &msg->call;

    const char *empty = "";
    if (!value) {
        value = empty;
    }

    size_t size = strlen(value) + 1; /* Including the NULL termination character */
    memcpy(&call->data[call->data_len], value, size);

    call->data_len += size;
}

void rpc_request_push_buffer(const uint8_t *value, uint16_t len, rpc_msg_t *msg) {

    rpc_call_t *call = &msg->call;

    /* Put the buffer length into the packet */
    uint16_t len_be = htobe16(len);
    memcpy(&call->data[call->data_len], &len_be, sizeof(uint16_t));
    call->data_len += sizeof(uint16_t);

    /* Put the data into the packet - if any */
    memcpy(&call->data[call->data_len], value, len);
    call->data_len += len;
}

void rpc_result_pop_string(char *value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;

    size_t len = strlen((char *)&reply->data[reply->data_len]);
    strcpy(value, (char *)&reply->data[reply->data_len]);

    reply->data_len += len + 1;
}

void rpc_result_pop_buffer(uint8_t *value, uint16_t *len, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;

    *len = rpc_result_pop_uint16(msg);
    memcpy(value, &reply->data[reply->data_len], *len);

    reply->data_len += *len;
}

int rpc_disconnect(csp_conn_t *conn) {

    return csp_close(conn) == CSP_ERR_NONE ? RPC_STATUS_OK : RPC_STATUS_ERR_INVALID;
}

int rpc_build_request(uint16_t node, uint32_t program, uint32_t procedure, csp_conn_t ** conn, rpc_msg_t **req_msg) {

    *conn = csp_connect(CSP_PRIO_NORM, node, CSP_PORT_RPC_SERVER, 0, CSP_O_NONE);
    if (*conn == NULL) {
        return RPC_STATUS_ERR_COULD_NOT_CONNECT;
    }

    csp_packet_t *request = csp_buffer_get(0);
    if (!request) {
        return RPC_STATUS_ERR_NO_MEMORY;
    }

    *req_msg = (rpc_msg_t *)request->data;
    (*req_msg)->type = RPC_MSG_CALL;
    (*req_msg)->version = RPC_VERSION;
    (*req_msg)->xid = htobe32(0x12341234); /* TODO: We need to have a sequence number at this place */
    request->length = sizeof((*req_msg)->type) + sizeof((*req_msg)->version) + sizeof((*req_msg)->xid) + sizeof((*req_msg)->call);
    (*req_msg)->call.data_len = 0;
    (*req_msg)->call.program = htobe32(program);
    (*req_msg)->call.procedure = htobe32(procedure);

    return RPC_STATUS_OK;
}

void rpc_send(csp_conn_t *conn, rpc_msg_t *msg) {

    csp_packet_t *packet = (csp_packet_t *)((uint8_t*)msg - offsetof(csp_packet_t, data));

    packet->length += msg->call.data_len;
    msg->call.data_len = htobe16(msg->call.data_len);

    csp_send(conn, packet);
}

int rpc_get_reply(csp_conn_t *conn, rpc_msg_t **msg, uint32_t maxresponses, uint32_t expected_idx, uint32_t timeout) {

    csp_packet_t *reply = csp_read(conn, timeout);
    if (!reply) {
        return RPC_STATUS_ERR_TIMEOUT;
    }

    *msg = (rpc_msg_t *)reply->data;
    (*msg)->reply.amount = be32toh((*msg)->reply.amount);
    (*msg)->reply.idx = be32toh((*msg)->reply.idx);
    (*msg)->reply.data_len = 0;

    if ((*msg)->reply.amount == 0) {
        csp_buffer_free(reply);
        return RPC_STATUS_EMPTYRESPONSE;
    }

    if ((*msg)->reply.idx >= maxresponses
        || (*msg)->reply.idx >= (*msg)->reply.amount
        || (*msg)->reply.idx != expected_idx) {
        csp_buffer_free(reply);
        return RPC_STATUS_ERR_INVALID;
    }

    return RPC_STATUS_OK;
}

void rpc_buffer_free(rpc_msg_t *msg) {

    csp_packet_t *packet = (csp_packet_t *)((uint8_t*)msg - offsetof(csp_packet_t, data));

    csp_buffer_free(packet);
}
