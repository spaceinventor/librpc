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

static void rpc_unpack_call_reply(rpc_client_t *me, rpc_reply_t *reply, uint32_t procedure, void *ret) {

    /* Lookup the RPC procedure format */
    rpc_procedure_t *rpc = me->api->lookup(procedure);
    
    /* Grab the data length */
    uint16_t data_len = be16toh(reply->data_len);
    uint16_t offset = 0;

    if (rpc) {
        printf("unpack: %s - '%s'\n", rpc->name, rpc->res_fmt);
        const char *pfmt = rpc->res_fmt;
        while (*pfmt != '\0' && offset < data_len) {
            rpc_data_type_t value;
            switch (*pfmt) {
                case 'Q':
                {
                    memcpy(&value, &reply->data[offset], sizeof(value));
                    value.u64 = be64toh(value.u64);
                    printf("UNPACK: uint64_t -> %"PRIu64"\n", value.u64);
                    *(uint64_t *)ret = value.u64;
                    offset += sizeof(value.u64);
                }
                break;
                case 'q':
                {
                    memcpy(&value, &reply->data[offset], sizeof(value.i64));
                    value.i64 = be64toh(value.i64);
                    printf("UNPACK: int64_t -> %"PRIi64"\n", value.i64);
                    *(int64_t *)ret = value.i64;
                    offset += sizeof(value.i64);
                }
                break;
                case 'L':
                {
                    memcpy(&value, &reply->data[offset], sizeof(value));
                    value.u32 = be32toh(value.u32);
                    printf("UNPACK: uint32_t -> %"PRIu32"\n", value.u32);
                    *(uint32_t *)ret = value.u32;
                    offset += sizeof(value.u32);
                }
                break;
                case 'l':
                {
                    memcpy(&value, &reply->data[offset], sizeof(value.i32));
                    value.i32 = be32toh(value.i32);
                    printf("UNPACK: int32_t -> %"PRIi32"\n", value.i32);
                    *(int32_t *)ret = value.i32;
                    offset += sizeof(value.i32);
                }
                break;
                case 'H':
                {
                    memcpy(&value, &reply->data[offset], sizeof(value.u16));
                    value.u16 = be16toh(value.u16);
                    printf("UNPACK: uint16_t -> %"PRIu16"\n", value.u16);
                    *(uint16_t *)ret = value.u16;
                    offset += sizeof(value.u16);
                }
                break;
                case 'h':
                {
                    memcpy(&value, &reply->data[offset], sizeof(value.i16));
                    value.i16 = be16toh(value.i16);
                    printf("UNPACK: int16_t -> %"PRIi16"\n", value.i16);
                    *(int16_t *)ret = value.i16;
                    offset += sizeof(value.i16);
                }
                break;
                case 'B':
                {
                    memcpy(&value, &reply->data[offset], sizeof(value.u8));
                    printf("UNPACK: uint8_t -> %"PRIu8"\n", value.u8);
                    *(uint8_t *)ret = value.u8;
                    offset += sizeof(value.u8);
                }
                break;
                case 'b':
                {
                    memcpy(&value, &reply->data[offset], sizeof(value.i8));
                    printf("UNPACK: int8_t -> %"PRIi8"\n", value.i8);
                    *(int8_t *)ret = value.i8;
                    offset += sizeof(value.i8);
                }
                break;
            }

            pfmt++;
        }
    }
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

int rpc_call_invoke(rpc_client_t *me, uint32_t program, uint32_t procedure, void *ret, ...) {

    /* Allocate the CSP packet for the RPC object */
    csp_packet_t *request = csp_buffer_get(0);

    /* Create the RPC call protocol message to send to the RPC server */
    va_list args;
    va_start(args, ret);
    rpc_msg_t *req_msg = (rpc_msg_t *)request->data;
    req_msg->type = RPC_MSG_CALL;
    req_msg->xid = htobe32(0x12341234); /* TODO: We need to have a sequence number at this place */
    req_msg->call.data_len = 0;
    req_msg->call.program = htobe32(program);
    req_msg->call.procedure = htobe32(procedure);
    request->length = sizeof(req_msg->type) + sizeof(req_msg->xid);
    request->length += rpc_pack_call_request(me, &req_msg->call, args);
    va_end(args);

    /* Send the RPC call to the RPC server */
    csp_send(me->conn, request);

    /* Wait for the reply from the client */
    csp_packet_t *reply = csp_read(me->conn, 100);
    if (reply) {
        rpc_msg_t *msg = (rpc_msg_t *)reply->data;
        rpc_unpack_call_reply(me, &msg->reply, procedure, ret);
        csp_buffer_free(reply);
    } else {
        printf("RPC: Timeout waiting for reply\n");
    }

    return 0;
}

csp_packet_t * rpc_result_prepare(rpc_server_t *me, rpc_msg_t *msg) {

    csp_packet_t *packet;

    packet = csp_buffer_get(0);
    if (packet) {
        rpc_msg_t *reply_msg = (rpc_msg_t *)packet->data;
        reply_msg->type = RPC_MSG_REPLY;
        reply_msg->xid = msg->xid;
        reply_msg->reply.data_len = 0;
        packet->length = sizeof(reply_msg->type) + sizeof(reply_msg->xid) + sizeof(reply_msg->reply);
    }

    return packet;
}

void rpc_result_push_uint32(rpc_server_t *me, uint32_t value, csp_packet_t *result) {

    rpc_msg_t *msg = (rpc_msg_t *)result->data;
    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    value = htobe32(value);
    memcpy(&reply->data[data_len], &value, sizeof(uint32_t));
    data_len += sizeof(uint32_t);
    reply->data_len = htobe16(data_len);

    result->length += sizeof(uint32_t);
}

void rpc_result_push_int32(rpc_server_t *me, int32_t value, csp_packet_t *result) {

    rpc_msg_t *msg = (rpc_msg_t *)result->data;
    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    value = htobe32(value);
    memcpy(&reply->data[data_len], &value, sizeof(int32_t));
    data_len += sizeof(int32_t);
    reply->data_len = htobe16(data_len);

    result->length += sizeof(int32_t);
}

csp_packet_t * rpc_handle_msg(rpc_server_t *me, csp_packet_t *packet, rpc_server_callback_t *cb) {

    rpc_msg_t *req_msg = (rpc_msg_t *)packet->data;
    csp_packet_t *reply = NULL;

    switch (req_msg->type) {
        case RPC_MSG_CALL:
        {
            uint16_t data_len = be16toh(req_msg->call.data_len);
            uint32_t program = be32toh(req_msg->call.program);
            uint32_t procedure = be32toh(req_msg->call.procedure);

            printf("rpc_msg: data_len=%"PRId16",program=0x%"PRIX32",procedure=%"PRIu32"\n", data_len, program, procedure);

            reply = rpc_result_prepare(me, req_msg);

            if (cb) {
                (*cb)(me, program, procedure, data_len, &req_msg->call.data[0], reply);
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
