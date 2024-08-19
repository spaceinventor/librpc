#include <endian.h>
#include <stdarg.h>
#include <stdio.h>

#include <csp/csp.h>

#include "dsu_server.h"
#include "dsu_rpc.h"

static const rpc_procedure_t g_rpc_procedures[] = {

    { .id = DSU_PROCEDURE_OPEN_FIFO, .name = "open_fifo", .arg_fmt = "L", .res_fmt = "l" },
};

static rpc_procedure_t * lookup_rpc_procedure(dsu_rpc_procedure_t procedure) {

    uint32_t i;
    rpc_procedure_t *rpc = NULL;

    for (i = 0; i < sizeof(g_rpc_procedures)/sizeof(rpc_procedure_t); i++) {
        if (g_rpc_procedures[i].id == procedure) {
            rpc = &g_rpc_procedures[i];
            break;
        }
    }

    return rpc;
}

static uint16_t rpc_pack_call_request(rpc_call_t *call, dsu_rpc_procedure_t procedure, va_list args) {

    uint16_t length = 0;
    uint16_t data_len = 0;

    call->program = htobe32(RPC_PROGRAM_DSM); /* Currently the only program we support */
    call->procedure = htobe32(procedure);

    /* Lookup the RPC procedure format */
    rpc_procedure_t *rpc = lookup_rpc_procedure(procedure);

    if (rpc) {
        /* Scan the format string and grab arguments from the args list */
        char *pfmt = rpc->arg_fmt;
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
                    int32_t value = (int32_t)va_arg(args, int);
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
            }

            pfmt++;
        }
    
        length = sizeof(*call) + data_len;
        call->data_len = htobe16(data_len);

        printf("pack: data_len=%"PRId16",program=0x%"PRIX32",procedure=%"PRIu32"\n", data_len, (uint32_t)RPC_PROGRAM_DSM, (uint32_t)procedure);
    }

    return length;
}

static void rpc_unpack_call_reply(dsu_rpc_procedure_t procedure, rpc_reply_t *reply, void *ret) {

    /* Lookup the RPC procedure format */
    rpc_procedure_t *rpc = lookup_rpc_procedure(procedure);
    
    /* Grab the length of the result buffer */
    uint16_t data_len = be16toh(reply->data_len);

    if (rpc) {
        printf("unpack: %s - '%s'\n", rpc->name, rpc->res_fmt);
        char *pfmt = rpc->res_fmt;
        while (*pfmt != '\0' && data_len > 0) {
            rpc_data_type_t value;
            switch (*pfmt) {
                case 'L':
                {
                    data_len -= sizeof(value.u32);
                    memcpy(&value, &reply->data[data_len], sizeof(value));
                    value.u32 = be32toh(value.u32);
                    printf("UNPACK: uint32_t -> %"PRIu32"\n", value.u32);
                    *(uint32_t *)ret = value.u32;
                }
                break;
                case 'l':
                {
                    data_len -= sizeof(value.i32);
                    memcpy(&value, &reply->data[data_len], sizeof(value.i32));
                    value.i32 = be32toh(value.i32);
                    printf("UNPACK: int32_t -> %"PRIi32"\n", value.i32);
                    *(int32_t *)ret = value.i32;
                }
                break;
                case 'H':
                {
                    data_len -= sizeof(value.u16);
                    memcpy(&value, &reply->data[data_len], sizeof(value.u16));
                    value.u16 = be16toh(value.u16);
                    printf("UNPACK: uint16_t -> %"PRIu16"\n", value.u16);
                    *(uint16_t *)ret = value.u16;
                }
                break;
                case 'h':
                {
                    data_len -= sizeof(value.i16);
                    memcpy(&value, &reply->data[data_len], sizeof(value.i16));
                    value.i16 = be16toh(value.i16);
                    printf("UNPACK: int16_t -> %"PRIi16"\n", value.i16);
                    *(int16_t *)ret = value.i16;
                }
                break;
                case 'B':
                {
                    data_len -= sizeof(value.u8);
                    memcpy(&value, &reply->data[data_len], sizeof(value.u8));
                    printf("UNPACK: uint8_t -> %"PRIu8"\n", value.u8);
                    *(uint8_t *)ret = value.u8;
                }
                break;
                case 'b':
                {
                    data_len -= sizeof(value.i8);
                    memcpy(&value, &reply->data[data_len], sizeof(value.i8));
                    printf("UNPACK: int8_t -> %"PRIi8"\n", value.i8);
                    *(int8_t *)ret = value.i8;
                }
                break;
            }

            pfmt++;
        }
    }
}

int dsu_rpc_connect(dsu_rpc_client_t *me, uint16_t node) {

    /* Create an RDP connection with the particular RPC server node */
    printf("RPC: Connecting the RPC service on %"PRIu16"\n", node);
    me->conn = csp_connect(CSP_PRIO_HIGH, node, CSP_PORT_DSU_SERVER, 0, CSP_O_RDP);
    if (me->conn == NULL) {
        printf("RPC: Could not connect to RPC service\n");
        return -1;
    }

    return 0;
}

int dsu_rpc_disconnect(dsu_rpc_client_t *me) {

    if (me && me->conn) {
        csp_close(me->conn);
    }

    return 0;
}

int dsu_rpc_call_invoke(dsu_rpc_client_t *me, dsu_rpc_procedure_t procedure, void *ret, ...) {

    /* Allocate the CSP packet for the RPC object */
    csp_packet_t *request = csp_buffer_get(0);

    /* Create the RPC call protocol message to send to the RPC server */
    va_list args;
    va_start(args, ret);
    rpc_msg_t *req_msg = (rpc_msg_t *)request->data;
    req_msg->type = RPC_MSG_CALL;
    req_msg->xid = htobe32(0x12341234); /* TODO: We need to have a sequence number at this place */
    req_msg->call.data_len = 0;
    request->length = sizeof(req_msg->type) + sizeof(req_msg->xid);
    request->length += rpc_pack_call_request(&req_msg->call, procedure, args);
    va_end(args);

    /* Send the RPC call to the RPC server */
    csp_send(me->conn, request);

    /* Wait for the reply from the client */
    csp_packet_t *reply = csp_read(me->conn, 100);
    if (reply) {
        rpc_msg_t *msg = (rpc_msg_t *)reply->data;
        rpc_unpack_call_reply(procedure, &msg->reply, ret);
        csp_buffer_free(reply);
    } else {
        printf("RPC: Timeout waiting for reply\n");
    }

    return 0;
}

csp_packet_t * dsu_rpc_result_prepare(rpc_msg_t *msg) {

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

void dsu_rpc_result_push_int(int value, csp_packet_t *result) {

    rpc_msg_t *msg = (rpc_msg_t *)result->data;
    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    value = htobe32(value);
    memcpy(&reply->data[data_len], &value, sizeof(int));
    data_len += sizeof(int);
    reply->data_len = htobe16(data_len);

    result->length += sizeof(int);
}

csp_packet_t * dsu_rpc_handle_msg(dsu_rpc_server_t *me, csp_packet_t *packet, dsu_rpc_server_callback_t *cb) {

    rpc_msg_t *req_msg = (rpc_msg_t *)packet->data;
    csp_packet_t *reply = NULL;

    switch (req_msg->type) {
        case RPC_MSG_CALL:
        {
            uint16_t data_len = be16toh(req_msg->call.data_len);
            uint32_t program = be32toh(req_msg->call.program);
            uint32_t procedure = be32toh(req_msg->call.procedure);

            printf("rpc_msg: data_len=%"PRId16",program=0x%"PRIX32",procedure=%"PRIu32"\n", data_len, program, procedure);

            reply = dsu_rpc_result_prepare(req_msg);

            if (cb) {
                (*cb)(program, procedure, data_len, &req_msg->call.data[0], reply);
            }

        }
        break;
    }

    return reply;

}

int dsu_rpc_start_server(dsu_rpc_server_t *me) {

    int res;
    me->sock.opts = CSP_O_RDP /*CSP_O_NONE*/;
    res = csp_bind(&me->sock, CSP_PORT_DSU_SERVER);
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

int dsu_rpc_stop_server(dsu_rpc_server_t *me) {

    int res;

    /* Close the CSP socket */
    res = csp_socket_close(&me->sock);
    if (res != CSP_ERR_NONE) {
        return -1;
    }

    return 0;
}

csp_conn_t * dsu_rpc_waitfor_connections(dsu_rpc_server_t *me) {

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
