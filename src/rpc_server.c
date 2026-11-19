#include "rpc_server.h"

#include <endian.h>
#include <stdio.h>
#include <string.h>

#define be8toh(x) (x)
#define htobe8(x) (x)

#define RPC_HANDLE_SERVER_IMPL(type, name, size) \
    void rpc_result_push_##name(type value, rpc_msg_t *msg) { \
        rpc_reply_t *reply = &msg->reply; \
        value = htobe##size(value); \
        memcpy(&reply->data[reply->data_len], &value, sizeof(type)); \
        reply->data_len += sizeof(type); \
    } \
    type rpc_request_pop_##name(rpc_msg_t *msg, uint16_t *offset) { \
        rpc_call_t *call = &msg->call; \
        type value; \
        if (call->data_len < sizeof(type)) { \
            return 0; \
        } \
        memcpy(&value, &call->data[*offset], sizeof(type)); \
        value = be##size##toh(value); \
        call->data_len -= sizeof(type); \
        *offset += sizeof(type); \
        return value; \
    }

RPC_HANDLE_SERVER_IMPL(uint8_t, uint8, 8)
RPC_HANDLE_SERVER_IMPL(int8_t, int8, 8)
RPC_HANDLE_SERVER_IMPL(uint16_t, uint16, 16)
RPC_HANDLE_SERVER_IMPL(int16_t, int16, 16)
RPC_HANDLE_SERVER_IMPL(uint32_t, uint32, 32)
RPC_HANDLE_SERVER_IMPL(int32_t, int32, 32)
RPC_HANDLE_SERVER_IMPL(uint64_t, uint64, 64)
RPC_HANDLE_SERVER_IMPL(int64_t, int64, 64)
RPC_HANDLE_SERVER_IMPL(float, float, 32)
RPC_HANDLE_SERVER_IMPL(double, double, 64)
#undef RPC_HANDLE_SERVER_IMPL

void rpc_request_pop_string(char *value, rpc_msg_t *msg, uint16_t *offset) {

    rpc_call_t *request = &msg->call;

    if (request->data_len == 0) {
        value[0] = '\0';
        return;
    }

    size_t len = strnlen((char *)&request->data[*offset], request->data_len);
    if (len == request->data_len) {
        /* Missing NULL termination */
        value[0] = '\0';
        request->data_len = 0;
        return;
    }

    strncpy(value, (char *)&request->data[*offset], len);
    value[len] = '\0';

    *offset += (len + 1);
    request->data_len -= (len + 1);
}

void rpc_request_pop_buffer(uint8_t *value, uint16_t *len, rpc_msg_t *msg, uint16_t *offset) {

    rpc_call_t *request = &msg->call;

    *len = rpc_request_pop_uint16(msg, offset);
    if (request->data_len < *len) {
        request->data_len = 0;
        *len = 0;
        return;
    }
    memcpy(value, &request->data[*offset], *len);

    request->data_len -= *len;
    *offset += *len;
}

void rpc_result_push_string(const char *value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;

    const char *empty = "";
    if (!value) {
        value = empty;
    }

    size_t size = strlen(value) + 1; /* Including the NULL termination character */
    memcpy(&reply->data[reply->data_len], value, size);

    reply->data_len += size;
}

void rpc_result_push_buffer(const uint8_t *value, uint16_t len, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;

    /* Put the buffer length into the packet */
    uint16_t len_be = htobe16(len);
    memcpy(&reply->data[reply->data_len], &len_be, sizeof(uint16_t));
    reply->data_len += sizeof(uint16_t);

    /* Put the data into the packet - if any */
    memcpy(&reply->data[reply->data_len], value, len);
    reply->data_len += len;
}

rpc_msg_t * rpc_result_prepare(rpc_msg_t *call, uint32_t amount, uint32_t idx) {

    csp_packet_t *packet = csp_buffer_get(0);
    if (packet == NULL) {
        return NULL;
    }

    rpc_msg_t *reply = (rpc_msg_t *)packet->data;
    reply->type = RPC_MSG_REPLY;
    reply->version = RPC_VERSION;
    reply->xid = call->xid;
    reply->reply.data_len = htobe32(0);
    reply->reply.amount = htobe32(0);
    reply->reply.idx = htobe32(0);
    packet->length = sizeof(reply->type) + sizeof(reply->version) + sizeof(reply->xid) + sizeof(reply->reply);

    reply->reply.amount = htobe32(amount);
    reply->reply.idx = htobe32(idx);

    return reply;
}

static void rpc_handle_msg(csp_packet_t *packet) {

    rpc_msg_t *req_msg = (rpc_msg_t *)packet->data;

    if (req_msg->version != RPC_VERSION) {
        RPC_ERR("RPC-S: Error, wrong RPC version in message.\n");
        return;
    }

    if (sizeof(*req_msg) > packet->length) {
        RPC_ERR("RPC-S: Error, invalid packet length.\n");
        return;
    }

    switch (req_msg->type) {
        case RPC_MSG_CALL:
        {
            uint32_t program = be32toh(req_msg->call.program);
            uint32_t procedure = be32toh(req_msg->call.procedure);
            req_msg->call.data_len = be16toh(req_msg->call.data_len);

            /* Sanity check of data_len in msg */
            if (sizeof(*req_msg) + req_msg->call.data_len > packet->length) {
                RPC_ERR("RPC-S: Error, invalid packet length.\n");
                return;
            }

            RPC_DBG("RPC-S: rpc_msg_call: data_len=%"PRId16",program=0x%"PRIX32",procedure=%"PRIu32"\n", req_msg->call.data_len, program, procedure);

            int result = rpc_dispatch_program(program, procedure, packet, req_msg);
            if (result == RPC_STATUS_ERR_NOT_FOUND) {
                RPC_ERR("RPC-S: No handler found for program 0x%"PRIX32"\n", program);
            } else if (result != 0) {
                RPC_ERR("RPC-S: Error handling program 0x%"PRIX32", procedure %"PRIu32"\n", program, procedure);
            }
        }
        break;
    }
}

void rpc_send_reply(const csp_packet_t *request, rpc_msg_t *reply) {
    csp_packet_t *packet = (csp_packet_t *)((uint8_t*)reply - offsetof(csp_packet_t, data));

    packet->length += reply->reply.data_len;
    reply->reply.data_len = htobe16(reply->reply.data_len);

    csp_sendto_reply(request, packet, CSP_O_SAME);
}

void rpc_server_callback(csp_packet_t *packet) {
    if (packet == NULL) {
        return;
    }

    rpc_handle_msg(packet);
    csp_buffer_free(packet);
}

int rpc_server_register(void) {
    return csp_bind_callback(rpc_server_callback, CSP_PORT_RPC_SERVER);
}

int rpc_server_main(void) {

    csp_socket_t sock = {};
    sock.opts = CSP_O_NONE;
    int res = csp_bind(&sock, CSP_PORT_RPC_SERVER);
    if (res == CSP_ERR_INVAL) return RPC_STATUS_ERR_INVALID;
    else if (res == CSP_ERR_USED) return RPC_STATUS_ERR_INUSE;
    else if (res != CSP_ERR_NONE) return RPC_STATUS_ERR_INVALID;

    /* Create a backlog of 1 connection */
    csp_listen(&sock, 1);

    while(true) {
        csp_conn_t *conn = csp_accept(&sock, CSP_MAX_TIMEOUT);
        csp_packet_t *packet;
        while ((packet = csp_read(conn, 0)) != NULL) {
            rpc_server_callback(packet);
        }
        csp_close(conn);
    }
}
