#include <endian.h>
#include <stdarg.h>
#include <stdio.h>

#include <csp/csp.h>

#include "rpc.h"

static uint16_t rpc_pack_call_request(rpc_client_t *me, rpc_call_t *call, va_list args) {

    uint16_t length = 0;
    uint16_t data_len = 0;

    /* Lookup the RPC procedure format */
    rpc_procedure_t *rpc = me->api->lookup(be32toh(call->procedure));

    if (rpc) {
        /* Scan the format string and grab arguments from the args list */
        const char *pfmt = rpc->arg_fmt;
        while (*pfmt != '\0') {
            switch (*pfmt) {
                case 'f':
                {
                    rpc_data_type_t value;
                    value.flt = (float)va_arg(args, double);
                    printf("PACK: float > %f\n", value.flt);
                    value.u32 = htobe32(value.u32);
                    memcpy(&(call->data[data_len]), &value, sizeof(float));
                    data_len += sizeof(float);
                }
                break;
                case 'd':
                {
                    rpc_data_type_t value;
                    value.dbl = (double)va_arg(args, double);
                    printf("PACK: double > %f\n", value.dbl);
                    value.u64 = htobe64(value.u64);
                    memcpy(&(call->data[data_len]), &value, sizeof(double));
                    data_len += sizeof(double);
                }
                break;
                case 'b':
                {
                    int8_t value = (int8_t)va_arg(args, int);
                    printf("PACK: int8_t -> %"PRIi8"\n", value);
                    memcpy(&(call->data[data_len]), &value, sizeof(int8_t));
                    data_len += sizeof(int8_t);
                } break;
                case 'B':
                {
                    uint8_t value = (uint8_t)va_arg(args, int);
                    printf("PACK: uint8_t -> %"PRIu8"\n", value);
                    memcpy(&(call->data[data_len]), &value, sizeof(uint8_t));
                    data_len += sizeof(uint8_t);
                } break;
                case 'h':
                {
                    int16_t value = (int16_t)va_arg(args, int);
                    printf("PACK: int16_t -> %"PRIi16"\n", value);
                    value = htobe16(value);
                    memcpy(&(call->data[data_len]), &value, sizeof(int16_t));
                    data_len += sizeof(int16_t);
                } break;
                case 'H':
                {
                    uint16_t value = (uint16_t)va_arg(args, int);
                    printf("PACK: uint16_t -> %"PRIu16"\n", value);
                    value = htobe16(value);
                    memcpy(&(call->data[data_len]), &value, sizeof(uint16_t));
                    data_len += sizeof(uint16_t);
                } break;
                case 'l':
                {
                    int32_t value = (int32_t)va_arg(args, int32_t);
                    printf("PACK: int32_t -> %"PRIi32"\n", value);
                    value = htobe32(value);
                    memcpy(&(call->data[data_len]), &value, sizeof(int32_t));
                    data_len += sizeof(int32_t);
                } break;
                case 'L':
                {
                    uint32_t value = (uint32_t)va_arg(args, uint32_t);
                    printf("PACK: uint32_t -> %"PRIu32"\n", value);
                    value = htobe32(value);
                    memcpy(&(call->data[data_len]), &value, sizeof(uint32_t));
                    data_len += sizeof(uint32_t);
                } break;
                case 'q':
                {
                    int64_t value = (int64_t)va_arg(args, int64_t);
                    printf("PACK: int64_t -> %"PRIi64"\n", value);
                    value = htobe64(value);
                    memcpy(&(call->data[data_len]), &value, sizeof(int64_t));
                    data_len += sizeof(int64_t);
                } break;
                case 'Q':
                {
                    uint64_t value = (uint64_t)va_arg(args, uint64_t);
                    printf("PACK: uint64_t -> %"PRIu64"\n", value);
                    value = htobe64(value);
                    memcpy(&(call->data[data_len]), &value, sizeof(uint64_t));
                    data_len += sizeof(uint64_t);
                } break;
            }

            pfmt++;
        }
    
        length = sizeof(*call) + data_len;
        call->data_len = htobe16(data_len);

        printf("pack: data_len=%"PRId16",program=0x%"PRIX32",procedure=%"PRIu32"\n", data_len, be32toh(call->program), be32toh(call->procedure));
    }

    return length;
}

static void *rpc_unpack(uint8_t *data, uint16_t len, const char *fmt, void *out, va_list args) {

    uint16_t offset = 0;

    while (*fmt != '\0' && offset < len) {
        rpc_data_type_t *res_val = (rpc_data_type_t *)((uintptr_t)out + offset);
        rpc_data_type_t *val;
        switch (*fmt) {
            case 'Q':
            {
                val = (rpc_data_type_t *)va_arg(args, uint64_t *);
                memcpy(val, &data[offset], sizeof(val->u64));
                val->u64 = be64toh(val->u64);

                memcpy(res_val, &data[offset], sizeof(res_val->u64));
                res_val->u64 = be64toh(res_val->u64);

                printf("UNPACK: uint64_t -> %"PRIu64",%"PRIu64"\n", res_val->u64, val->u64);
                offset += sizeof(val->u64);
            }
            break;
            case 'q':
            {
                val = (rpc_data_type_t *)va_arg(args, int64_t *);
                memcpy(val, &data[offset], sizeof(val->i64));
                val->i64 = be64toh(val->i64);

                memcpy(res_val, &data[offset], sizeof(res_val->i64));
                res_val->i64 = be64toh(res_val->i64);

                printf("UNPACK: int64_t -> %"PRIi64",%"PRIi64"\n", res_val->i64, val->i64);
                offset += sizeof(val->i64);
            }
            break;
            case 'L':
            {
                val = (rpc_data_type_t *)va_arg(args, uint32_t *);
                memcpy(val, &data[offset], sizeof(val->u32));
                val->u32 = be32toh(val->u32);

                memcpy(res_val, &data[offset], sizeof(res_val->u32));
                res_val->u32 = be32toh(res_val->u32);

                printf("UNPACK: uint32_t -> %"PRIu32",%"PRIu32"\n", res_val->u32, val->u32);
                offset += sizeof(val->u32);
            }
            break;
            case 'l':
            {
                val = (rpc_data_type_t *)va_arg(args, int32_t *);
                memcpy(val, &data[offset], sizeof(val->i32));
                val->i32 = be32toh(val->i32);

                memcpy(res_val, &data[offset], sizeof(res_val->i32));
                res_val->i32 = be32toh(res_val->i32);

                printf("UNPACK: int32_t -> %"PRIi32",%"PRIi32"\n", res_val->i32, val->i32);
                offset += sizeof(val->i32);
            }
            break;
            case 'H':
            {
                val = (rpc_data_type_t *)va_arg(args, uint16_t *);
                memcpy(val, &data[offset], sizeof(val->u16));
                val->u16 = be16toh(val->u16);

                memcpy(res_val, &data[offset], sizeof(res_val->u16));
                res_val->u16 = be16toh(res_val->u16);

                printf("UNPACK: uint16_t -> %"PRIu16",%"PRIu16"\n", res_val->u16, val->u16);
                offset += sizeof(val->u16);
            }
            break;
            case 'h':
            {
                val = (rpc_data_type_t *)va_arg(args, int16_t *);
                memcpy(val, &data[offset], sizeof(val->i16));
                val->i16 = be16toh(val->i16);

                memcpy(res_val, &data[offset], sizeof(res_val->i16));
                res_val->i16 = be16toh(res_val->i16);

                printf("UNPACK: int16_t -> %"PRIi16",%"PRIi16"\n", res_val->i16, val->i16);
                offset += sizeof(val->i16);
            }
            break;
            case 'B':
            {
                val = (rpc_data_type_t *)va_arg(args, uint8_t *);
                memcpy(val, &data[offset], sizeof(val->u8));
                val->u8 = val->u8;

                memcpy(res_val, &data[offset], sizeof(res_val->u8));
                res_val->u8 = res_val->u8;

                printf("UNPACK: uint8_t -> %"PRIu8",%"PRIu8"\n", res_val->u8, val->u8);
                offset += sizeof(val->u8);
            }
            break;
            case 'b':
            {
                val = (rpc_data_type_t *)va_arg(args, int8_t *);
                memcpy(val, &data[offset], sizeof(val->i8));
                val->i8 = val->i8;

                memcpy(res_val, &data[offset], sizeof(res_val->i8));
                res_val->i8 = res_val->i8;

                printf("UNPACK: int8_t -> %"PRIi8",%"PRIi8"\n", res_val->i8, val->i8);
                offset += sizeof(val->i8);
            }
            break;
            case 'f':
            {
                val = (rpc_data_type_t *)va_arg(args, float *);
                memcpy(val, &data[offset], sizeof(val->flt));
                val->u32 = be32toh(val->u32);

                memcpy(res_val, &data[offset], sizeof(res_val->flt));
                res_val->u32 = be32toh(res_val->u32);

                printf("UNPACK: float -> %f,%f\n", res_val->flt, val->flt);
                offset += sizeof(val->flt);
            }
            break;
            case 'd':
            {
                val = (rpc_data_type_t *)va_arg(args, double *);
                memcpy(val, &data[offset], sizeof(val->dbl));
                val->u64 = be64toh(val->u64);

                memcpy(res_val, &data[offset], sizeof(res_val->dbl));
                res_val->u64 = be64toh(res_val->u64);

                printf("UNPACK: double -> %0.15f,%0.15f\n", res_val->dbl, val->dbl);
                offset += sizeof(val->dbl);
            }
            break;
        }

        fmt++;
    }

    return out;
}

int rpc_connect(rpc_client_t *me, uint16_t node) {

    /* Create an RDP connection with the particular RPC server node */
    printf("RPC: Connecting the RPC service on %"PRIu16"\n", node);
    me->conn = csp_connect(CSP_PRIO_HIGH, node, CSP_PORT_RPC_SERVER, 0, CSP_O_RDP);
    if (me->conn == NULL) {
        printf("RPC: Could not connect to RPC service\n");
        return -1;
    }

    return 0;
}

int rpc_disconnect(rpc_client_t *me) {

    if (me && me->conn) {
        csp_close(me->conn);
    }

    return 0;
}

int rpc_call_deserialize(rpc_server_t *me, rpc_msg_t *msg, ...) {


    /* Lookup the RPC procedure format */
    rpc_procedure_t *rpc = me->api->lookup(be32toh(msg->call.procedure));
    uint16_t data_len = be16toh(msg->call.data_len);

    printf("deserializing_call: data_len=%"PRId16",procedure:%"PRIu32"\n", data_len, be32toh(msg->call.procedure));

    uint8_t dummy[1024];

    if (rpc) {
        va_list(args);
        va_start(args, msg);

        rpc_unpack(&msg->call.data[0], data_len, rpc->arg_fmt, &dummy[0], args);

        va_end(args);
    }

    return 0;
}

int rpc_call_invoke(rpc_client_t *me, uint32_t program, uint32_t procedure, void *ret, ...) {

    /* Allocate the CSP packet for the RPC object */
    csp_packet_t *request = csp_buffer_get(0);

    /* Create the RPC call protocol message to send to the RPC server */
    va_list args;
    va_start(args, ret);

    rpc_msg_t *req_msg = (rpc_msg_t *)request->data;
    req_msg->type = RPC_MSG_CALL;
    req_msg->version = RPC_VERSION;
    req_msg->xid = htobe32(0x12341234); /* TODO: We need to have a sequence number at this place */
    req_msg->call.data_len = 0;
    req_msg->call.program = htobe32(program);
    req_msg->call.procedure = htobe32(procedure);
    request->length = sizeof(req_msg->type) + sizeof(req_msg->version) + sizeof(req_msg->xid);
    request->length += rpc_pack_call_request(me, &req_msg->call, args);

    /* Send the RPC call to the RPC server */
    csp_send(me->conn, request);

    /* Wait for the reply from the client */
    csp_packet_t *reply = csp_read(me->conn, 100);
    if (reply) {
        rpc_procedure_t *rpc = me->api->lookup(procedure);
        rpc_msg_t *msg = (rpc_msg_t *)reply->data;
        uint16_t data_len = be16toh(msg->reply.data_len);

        printf("rpc_reply: data_len=%"PRId16"\n", data_len);

        if (rpc) {
            rpc_unpack(&msg->reply.data[0], data_len, rpc->res_fmt, ret, args);
        }

        csp_buffer_free(reply);
    } else {
        printf("RPC: Timeout waiting for reply\n");
    }

    va_end(args);

    return 0;
}

csp_packet_t * rpc_result_prepare(rpc_server_t *me, rpc_msg_t *msg) {

    csp_packet_t *packet;

    packet = csp_buffer_get(0);
    if (packet) {
        rpc_msg_t *reply_msg = (rpc_msg_t *)packet->data;
        reply_msg->type = RPC_MSG_REPLY;
        reply_msg->version = RPC_VERSION;
        reply_msg->xid = msg->xid;
        reply_msg->reply.data_len = 0;
        packet->length = sizeof(reply_msg->type) + sizeof(reply_msg->version) + sizeof(reply_msg->xid) + sizeof(reply_msg->reply);
    }

    return packet;
}

void rpc_result_push_uint32(rpc_server_t *me, uint32_t value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    value = htobe32(value);
    memcpy(&reply->data[data_len], &value, sizeof(uint32_t));
    data_len += sizeof(uint32_t);
    reply->data_len = htobe16(data_len);
}

void rpc_result_push_int32(rpc_server_t *me, int32_t value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    value = htobe32(value);
    memcpy(&reply->data[data_len], &value, sizeof(int32_t));
    data_len += sizeof(int32_t);
    reply->data_len = htobe16(data_len);
}

void rpc_result_push_uint16(rpc_server_t *me, uint16_t value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    value = htobe16(value);
    memcpy(&reply->data[data_len], &value, sizeof(uint16_t));
    data_len += sizeof(uint16_t);
    reply->data_len = htobe16(data_len);
}

void rpc_result_push_int16(rpc_server_t *me, int16_t value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    value = htobe16(value);
    memcpy(&reply->data[data_len], &value, sizeof(int16_t));
    data_len += sizeof(int16_t);
    reply->data_len = htobe16(data_len);
}

void rpc_result_push_float(rpc_server_t *me, float value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    rpc_data_type_t v;
    v.flt = value;
    v.u32 = htobe32(v.u32);
    memcpy(&reply->data[data_len], &v.u32, sizeof(float));
    data_len += sizeof(float);
    reply->data_len = htobe16(data_len);
}

void rpc_result_push_double(rpc_server_t *me, double value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    rpc_data_type_t v;
    v.dbl = value;
    v.u64 = htobe64(v.u64);
    memcpy(&reply->data[data_len], &v.u64, sizeof(double));
    data_len += sizeof(double);
    reply->data_len = htobe16(data_len);
}

static csp_packet_t * rpc_handle_msg(rpc_server_t *me, csp_packet_t *packet, rpc_server_callback_t *handler) {

    rpc_msg_t *req_msg = (rpc_msg_t *)packet->data;
    csp_packet_t *reply = NULL;

    if (req_msg->version != RPC_VERSION) {
        printf("ERROR: Wrong RPC version in message.\n");
        return NULL;
    }

    switch (req_msg->type) {
        case RPC_MSG_CALL:
        {
            uint16_t data_len = be16toh(req_msg->call.data_len);
            uint32_t program = be32toh(req_msg->call.program);
            uint32_t procedure = be32toh(req_msg->call.procedure);

            printf("rpc_msg_call: data_len=%"PRId16",program=0x%"PRIX32",procedure=%"PRIu32"\n", data_len, program, procedure);

            reply = rpc_result_prepare(me, req_msg);

            if (handler) {
                rpc_msg_t *reply_msg = (rpc_msg_t *)reply->data;
                (*handler)(me, program, procedure, req_msg, reply_msg);
                reply->length += be16toh(reply_msg->reply.data_len);
                printf("rpc_msg_reply: data_len=%"PRId16"\n", be16toh(reply_msg->reply.data_len));
            }

        }
        break;
    }

    return reply;

}

int rpc_start_server(rpc_server_t *me) {

    int res;
    me->sock.opts = CSP_O_RDP /*CSP_O_NONE*/;
    res = csp_bind(&me->sock, CSP_PORT_RPC_SERVER);
    if (res != CSP_ERR_NONE) {
        return -1;
    }

    /* Create a backlog of 1 connection */
    res = csp_listen(&me->sock, 1);
    if (res != CSP_ERR_NONE) {
        return -1;
    }

    return 0;
}

int rpc_stop_server(rpc_server_t *me) {

    int res;

    /* Close the CSP socket */
    res = csp_socket_close(&me->sock);
    if (res != CSP_ERR_NONE) {
        return -1;
    }

    return 0;
}

csp_conn_t * rpc_waitfor_connections(rpc_server_t *me) {

    csp_conn_t *conn;
    if ((conn = csp_accept(&me->sock, 10000)) == NULL)
    {
        /* timeout */
        return NULL;
    }

    uint16_t src = csp_conn_src(conn);
    uint16_t sport = csp_conn_sport(conn);

    printf("RPC: Incoming connection from: %"PRIu16":%"PRIu16"\n", src, sport);

    return conn;
}

bool rpc_handle_connection(rpc_server_t *me, csp_conn_t *conn, rpc_server_callback_t *cb) {

    /* Read request packets on connection, timeout is 10 s */
    csp_packet_t *request = csp_read(conn, 10000);
    if (NULL == request) {
        /* The connection is lost, tell the caller */
        printf("RPC: Client disconnected or timeout.\n");
        return false;
    }

    /* Handle the RPC request (call) and send the reply */
    csp_packet_t *reply = NULL;
    reply = rpc_handle_msg(me, request, cb);
    if (reply) {
        csp_send(conn, reply);
    }

    /* We still have a valid connection */
    return true;
}