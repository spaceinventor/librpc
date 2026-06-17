#include <endian.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <csp/csp.h>

#include "rpc.h"

#if 0
#define RPC_DBG(...) printf(__VA_ARGS__)
#define RPC_WRN(...) printf(__VA_ARGS__)
#else
#define RPC_DBG(...) do {} while(0)
#define RPC_WRN(...) do {} while(0)
#endif

#define RPC_ERR(...) printf(__VA_ARGS__)

rpc_client_t *global_rpc_client = NULL;
rpc_server_t *global_rpc_server = NULL;

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

#define RPC_HANDLE_SERVER_IMPL(type, name, size) \
    void rpc_result_push_##name(type value, rpc_msg_t *msg) { \
        rpc_reply_t *reply = &msg->reply; \
        value = htobe##size(value); \
        memcpy(&reply->data[reply->data_len], &value, sizeof(type)); \
        reply->data_len += sizeof(type); \
    } \
    type rpc_request_pop_##name(rpc_msg_t *msg) { \
        rpc_call_t *call = &msg->call; \
        type value; \
        memcpy(&value, &call->data[call->data_len], sizeof(type)); \
        value = be##size##toh(value); \
        call->data_len += sizeof(type); \
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

void rpc_request_pop_string(char *value, rpc_msg_t *msg) {

    rpc_call_t *request = &msg->call;

    size_t len = strlen((char *)&request->data[request->data_len]);
    strcpy(value, (char *)&request->data[request->data_len]);

    request->data_len += len + 1;
}

void rpc_request_pop_buffer(uint8_t *value, uint16_t *len, rpc_msg_t *msg) {

    rpc_call_t *request = &msg->call;

    *len = rpc_request_pop_uint16(msg);
    memcpy(value, &request->data[request->data_len], *len);

    request->data_len += *len;
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


csp_conn_t *rpc_connect(uint16_t node) {

    csp_conn_t *conn = csp_connect(CSP_PRIO_NORM, node, CSP_PORT_RPC_SERVER, 0, CSP_O_NONE);
    if (!conn) {
        RPC_ERR("RPC-C: Could not connect to RPC service on node %"PRIu16"\n", node);
        return NULL;
    }

    return conn;
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

int rpc_get_reply(csp_conn_t *conn, rpc_msg_t **msg, uint32_t maxresponses, uint32_t extected_idx, uint32_t timeout) {

    csp_packet_t *reply = csp_read(conn, timeout);
    if (!reply) {
        return RPC_STATUS_ERR_TIMEOUT;
    }

    *msg = (rpc_msg_t *)reply->data;
    (*msg)->reply.amount = be32toh((*msg)->reply.amount);
    (*msg)->reply.idx = be32toh((*msg)->reply.idx);
    (*msg)->reply.data_len = 0;

    if ((*msg)->reply.idx >= maxresponses 
        || (*msg)->reply.idx > (*msg)->reply.amount 
        || (*msg)->reply.idx != extected_idx) {
        csp_buffer_free(reply);
        return RPC_STATUS_ERR_INVALID;
    }

    return RPC_STATUS_OK;
}

void rpc_buffer_free(rpc_msg_t *msg) {

    csp_packet_t *packet = (csp_packet_t *)((uint8_t*)msg - offsetof(csp_packet_t, data));

    csp_buffer_free(packet);
}

int rpc_client_init(rpc_client_t *me) {

    if (global_rpc_client) {
        return RPC_STATUS_ERR_INUSE;
    }

    global_rpc_client = me;

    return RPC_STATUS_OK;
}

rpc_msg_t * rpc_result_prepare(rpc_msg_t *call, uint32_t amount, uint32_t idx) {

    csp_packet_t *packet = csp_buffer_get(0);
    rpc_msg_t *reply = (rpc_msg_t *)packet->data;
    if (packet) {
        reply->type = RPC_MSG_REPLY;
        reply->version = RPC_VERSION;
        reply->xid = call->xid;
        reply->reply.data_len = htobe32(0);
        reply->reply.amount = htobe32(0);
        reply->reply.idx = htobe32(0);
        packet->length = sizeof(reply->type) + sizeof(reply->version) + sizeof(reply->xid) + sizeof(reply->reply);
    }

    reply->reply.amount = htobe32(amount);
    reply->reply.idx = htobe32(idx);

    return reply;
}

static const rpc_program_t * lookup_program_from_id(rpc_module_t *module, uint32_t id) {

    const rpc_program_t *program = NULL;

    /* Search the local data base initially */
    for (uint32_t n = 0; n < module->nof_programs; n++) {
        if (module->programs[n].program_id == id) {
            program = &module->programs[n];
            break;
        }
    }

    return program;
}

static void rpc_handle_msg(csp_packet_t *packet) {

    rpc_msg_t *req_msg = (rpc_msg_t *)packet->data;

    if (req_msg->version != RPC_VERSION) {
        RPC_ERR("RPC-S: Error, wrong RPC version in message.\n");
        return;
    }

    switch (req_msg->type) {
        case RPC_MSG_CALL:
        {
            uint32_t program = be32toh(req_msg->call.program);
            uint32_t procedure = be32toh(req_msg->call.procedure);

            RPC_DBG("RPC-S: rpc_msg_call: data_len=%"PRId16",program=0x%"PRIX32",procedure=%"PRIu32"\n", be16toh(req_msg->call.data_len), program, procedure);

            /* Reset data length as it is used to unpack the content */
            req_msg->call.data_len = 0;

            /* Find a possibly matching program handler */
            const rpc_program_t *prg = lookup_program_from_id(&global_rpc_server->module, program);
            if (prg) {
                /* We found a match, call the associated handler */
                int missingdatasize = (*prg->handler)(procedure, req_msg);
                if (missingdatasize > 0) {
                    /* The response array size is 0, send empty response as ACK */
                    rpc_msg_t *reply = rpc_result_prepare(req_msg, 0, 0);
                    memset(reply->reply.data, 0, missingdatasize);
                    reply->reply.data_len = missingdatasize;
                    rpc_send_reply(global_rpc_server->conn, reply);
                }
            } else {
                /* No match found, we can not handle this request */
                RPC_ERR("RPC-S: No handler found for program 0x%"PRIX32"\n", program);
            }
        }
        break;
    }
}

void rpc_send_reply(csp_conn_t *conn, rpc_msg_t *reply) {

    csp_packet_t *packet = (csp_packet_t *)((uint8_t*)reply - offsetof(csp_packet_t, data));

    packet->length += reply->reply.data_len;
    reply->reply.data_len = htobe16(reply->reply.data_len);

    csp_send(conn, packet);
}

static int rpc_start_server(rpc_server_t *me) {

    /* Starting the server, also means parsing the programs registered */
    extern const rpc_program_t __start_rpc_programs;
    extern const rpc_program_t __stop_rpc_programs;
    const rpc_program_t *iter = &__start_rpc_programs;
    me->module.nof_programs = 0;
    me->module.programs = &__start_rpc_programs;
    RPC_DBG("RPC-S: Registering programs from address: %p\n", me->module.programs);
    while (iter != &__stop_rpc_programs) {
        RPC_DBG("  0x%08"PRIX32", '%s'\n", iter->program_id, iter->name);
        me->module.nof_programs++;
        iter++;
    }
    RPC_DBG("RPC-S: Registered %"PRId32" programs\n", me->module.nof_programs);

    memset(&me->sock, 0, sizeof(me->sock));
    int res;
    me->sock.opts = CSP_O_NONE;
    res = csp_bind(&me->sock, CSP_PORT_RPC_SERVER);
    if (res != CSP_ERR_NONE) {
        if (res == CSP_ERR_INVAL) return RPC_STATUS_ERR_INVALID;
        else if (res == CSP_ERR_USED) return RPC_STATUS_ERR_INUSE;
        else return RPC_STATUS_ERR_INVALID;
    }

    /* Create a backlog of 1 connection */
    csp_listen(&me->sock, 1);

    global_rpc_server = me;

    return RPC_STATUS_OK;
}

static bool rpc_waitfor_connections(rpc_server_t *me) {

    if ((me->conn = csp_accept(&me->sock, CSP_MAX_TIMEOUT)) == NULL)
    {
        /* timeout */
        return false;
    }

    RPC_DBG("RPC-S: Incoming connection from: %"PRIu16":%"PRIu16"\n", csp_conn_src(me->conn), csp_conn_sport(me->conn));

    return true;
}

static bool rpc_handle_connection(rpc_server_t *me) {

    /* Read request packets on connection */
    csp_packet_t *request = csp_read(me->conn, me->timeout);
    if (NULL == request) {
        /* The connection is lost, tell the caller */
        RPC_DBG("RPC-S: Client disconnected or timeout.\n");
        return false;
    }

    RPC_DBG("RPC-S: Handle message\n");
    /* Handle the RPC request (call) and send the reply */
    rpc_handle_msg(request);

    /* Free the request packet */
    csp_buffer_free(request);

    /* We still have a valid connection */
    return true;
}

void rpc_server_main(rpc_server_t *me) {

    if (rpc_start_server(me) != CSP_ERR_NONE) {
        printf("RPC: Could not start the RPC server\n");
        return;
    }

    /* Wait for connections and then process packets on the connection */
    printf("RPC: Starting the server loop...\n");

    while(true) {
        /* Wait for a new connection, 10 s timeout */
        bool connected = rpc_waitfor_connections(me);
        if (!connected) {
            /* Timeout, try again */
            continue;
        }

        /* Keep calling the RPC connection handler, until we dont have a connection */
        rpc_handle_connection(me);

        /* Close our end of the connection and loop */
        csp_close(me->conn);
    }
}
