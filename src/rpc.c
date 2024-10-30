#include <endian.h>
#include <stdarg.h>
#include <stdio.h>

#include <csp/csp.h>

#include "rpc.h"

#if 0
#define RPC_DBG(...) printf(__VA_ARGS__)
#else
#define RPC_DBG(...) do {} while(0)
#endif

typedef union rpc_data_type_u {
    int8_t      i8;
    uint8_t     u8;
    int16_t     i16;
    uint16_t    u16;
    int32_t     i32;
    uint32_t    u32;
    int64_t     i64;
    uint64_t    u64;
    float       flt;
    double      dbl;
    const char *str;
} rpc_data_type_t;

rpc_client_t *global_rpc_client = NULL;
rpc_server_t *global_rpc_server = NULL;

static const rpc_program_t * lookup_program_from_id(rpc_module_t *module, uint32_t id) {

    for (uint32_t n = 0; n < module->nof_programs; n++) {
        if (module->programs[n].program_id == id) {
            return &module->programs[n];
        }
    }

    return NULL;
}

static const rpc_procedure_t * lookup_procedure_from_id(const rpc_program_t *program, uint32_t id) {

    const rpc_procedure_t *procedure = NULL;
    const rpc_procedure_t *iter = program->procedures;

    while (iter && iter->id != 0xFFFFFFFF) {
        if (iter->id == id) {
            procedure = iter;
            break;
        }
        iter++;
    }

    return procedure;
}

static uint16_t rpc_pack_call_request(const rpc_program_t *prg, rpc_call_t *call, va_list *args) {

    uint16_t length = 0;
    uint16_t data_len = 0;

    /* Lookup the RPC procedure format */
    const rpc_procedure_t *rpc = lookup_procedure_from_id(prg, be32toh(call->procedure));

    if (rpc) {
        /* Scan the format string and grab arguments from the args list */
        const char *pfmt = rpc->arg_fmt;
        if (pfmt) {
            while (*pfmt != '\0') {
                switch (*pfmt) {
                    case 'f':
                    {
                        rpc_data_type_t value;
                        value.flt = (float)va_arg(*args, double);
                        RPC_DBG("PACK: float > %f\n", value.flt);
                        value.u32 = htobe32(value.u32);
                        memcpy(&(call->data[data_len]), &value, sizeof(float));
                        data_len += sizeof(float);
                    }
                    break;
                    case 'd':
                    {
                        rpc_data_type_t value;
                        value.dbl = (double)va_arg(*args, double);
                        RPC_DBG("PACK: double > %f\n", value.dbl);
                        value.u64 = htobe64(value.u64);
                        memcpy(&(call->data[data_len]), &value, sizeof(double));
                        data_len += sizeof(double);
                    }
                    break;
                    case 'b':
                    {
                        int8_t value = (int8_t)va_arg(*args, int);
                        RPC_DBG("PACK: int8_t -> %"PRIi8"\n", value);
                        memcpy(&(call->data[data_len]), &value, sizeof(int8_t));
                        data_len += sizeof(int8_t);
                    } break;
                    case 'B':
                    {
                        uint8_t value = (uint8_t)va_arg(*args, int);
                        RPC_DBG("PACK: uint8_t -> %"PRIu8"\n", value);
                        memcpy(&(call->data[data_len]), &value, sizeof(uint8_t));
                        data_len += sizeof(uint8_t);
                    } break;
                    case 'h':
                    {
                        int16_t value = (int16_t)va_arg(*args, int);
                        RPC_DBG("PACK: int16_t -> %"PRIi16"\n", value);
                        value = htobe16(value);
                        memcpy(&(call->data[data_len]), &value, sizeof(int16_t));
                        data_len += sizeof(int16_t);
                    } break;
                    case 'H':
                    {
                        uint16_t value = (uint16_t)va_arg(*args, int);
                        RPC_DBG("PACK: uint16_t -> %"PRIu16"\n", value);
                        value = htobe16(value);
                        memcpy(&(call->data[data_len]), &value, sizeof(uint16_t));
                        data_len += sizeof(uint16_t);
                    } break;
                    case 'l':
                    {
                        int32_t value = (int32_t)va_arg(*args, int32_t);
                        RPC_DBG("PACK: int32_t -> %"PRIi32"\n", value);
                        value = htobe32(value);
                        memcpy(&(call->data[data_len]), &value, sizeof(int32_t));
                        data_len += sizeof(int32_t);
                    } break;
                    case 'L':
                    {
                        uint32_t value = (uint32_t)va_arg(*args, uint32_t);
                        RPC_DBG("PACK: uint32_t -> %"PRIu32"\n", value);
                        value = htobe32(value);
                        memcpy(&(call->data[data_len]), &value, sizeof(uint32_t));
                        data_len += sizeof(uint32_t);
                    } break;
                    case 'q':
                    {
                        int64_t value = (int64_t)va_arg(*args, int64_t);
                        RPC_DBG("PACK: int64_t -> %"PRIi64"\n", value);
                        value = htobe64(value);
                        memcpy(&(call->data[data_len]), &value, sizeof(int64_t));
                        data_len += sizeof(int64_t);
                    } break;
                    case 'Q':
                    {
                        uint64_t value = (uint64_t)va_arg(*args, uint64_t);
                        RPC_DBG("PACK: uint64_t -> %"PRIu64"\n", value);
                        value = htobe64(value);
                        memcpy(&(call->data[data_len]), &value, sizeof(uint64_t));
                        data_len += sizeof(uint64_t);
                    } break;
                    case 's':
                    {
                        const char *value = (const char *)va_arg(*args, const char *);
                        RPC_DBG("PACK: string -> '%s'\n", value);
                        size_t size = strlen(value) + 1;
                        memcpy(&(call->data[data_len]), value, size);
                        data_len += size;
                    } break;
                }

                pfmt++;
            }
        }
    
        length = sizeof(*call) + data_len;
        call->data_len = htobe16(data_len);

        RPC_DBG("pack: data_len=%"PRId16",program=0x%"PRIX32",procedure=%"PRIu32"\n", data_len, be32toh(call->program), be32toh(call->procedure));
    }

    return length;
}

static uint16_t rpc_unpack(uint8_t *data, uint16_t len, const char *fmt, va_list args) {

    uint16_t offset = 0;

    if (fmt) {
        while (*fmt != '\0' && offset < len) {
            rpc_data_type_t *val;
            switch (*fmt) {
                case 'Q':
                {
                    val = (rpc_data_type_t *)va_arg(args, uint64_t *);
                    memcpy(val, &data[offset], sizeof(val->u64));
                    val->u64 = be64toh(val->u64);
                    RPC_DBG("UNPACK: uint64_t -> %"PRIu64"\n", val->u64);
                    offset += sizeof(val->u64);
                }
                break;
                case 'q':
                {
                    val = (rpc_data_type_t *)va_arg(args, int64_t *);
                    memcpy(val, &data[offset], sizeof(val->i64));
                    val->i64 = be64toh(val->i64);
                    RPC_DBG("UNPACK: int64_t -> %"PRIi64"\n", val->i64);
                    offset += sizeof(val->i64);
                }
                break;
                case 'L':
                {
                    val = (rpc_data_type_t *)va_arg(args, uint32_t *);
                    memcpy(val, &data[offset], sizeof(val->u32));
                    val->u32 = be32toh(val->u32);
                    RPC_DBG("UNPACK: uint32_t -> %"PRIu32"\n", val->u32);
                    offset += sizeof(val->u32);
                }
                break;
                case 'l':
                {
                    val = (rpc_data_type_t *)va_arg(args, int32_t *);
                    memcpy(val, &data[offset], sizeof(val->i32));
                    val->i32 = be32toh(val->i32);
                    RPC_DBG("UNPACK: int32_t -> %"PRIi32"\n", val->i32);
                    offset += sizeof(val->i32);
                }
                break;
                case 'H':
                {
                    val = (rpc_data_type_t *)va_arg(args, uint16_t *);
                    memcpy(val, &data[offset], sizeof(val->u16));
                    val->u16 = be16toh(val->u16);
                    RPC_DBG("UNPACK: uint16_t -> %"PRIu16"\n", val->u16);
                    offset += sizeof(val->u16);
                }
                break;
                case 'h':
                {
                    val = (rpc_data_type_t *)va_arg(args, int16_t *);
                    memcpy(val, &data[offset], sizeof(val->i16));
                    val->i16 = be16toh(val->i16);
                    RPC_DBG("UNPACK: int16_t -> %"PRIi16"\n", val->i16);
                    offset += sizeof(val->i16);
                }
                break;
                case 'B':
                {
                    val = (rpc_data_type_t *)va_arg(args, uint8_t *);
                    memcpy(val, &data[offset], sizeof(val->u8));
                    val->u8 = val->u8;
                    RPC_DBG("UNPACK: uint8_t -> %"PRIu8"\n", val->u8);
                    offset += sizeof(val->u8);
                }
                break;
                case 'b':
                {
                    val = (rpc_data_type_t *)va_arg(args, int8_t *);
                    memcpy(val, &data[offset], sizeof(val->i8));
                    val->i8 = val->i8;
                    RPC_DBG("UNPACK: int8_t -> %"PRIi8"\n", val->i8);
                    offset += sizeof(val->i8);
                }
                break;
                case 'f':
                {
                    val = (rpc_data_type_t *)va_arg(args, float *);
                    memcpy(val, &data[offset], sizeof(val->flt));
                    val->u32 = be32toh(val->u32);
                    RPC_DBG("UNPACK: float -> %f\n", val->flt);
                    offset += sizeof(val->flt);
                }
                break;
                case 'd':
                {
                    val = (rpc_data_type_t *)va_arg(args, double *);
                    memcpy(val, &data[offset], sizeof(val->dbl));
                    val->u64 = be64toh(val->u64);
                    RPC_DBG("UNPACK: double -> %0.15f\n", val->dbl);
                    offset += sizeof(val->dbl);
                }
                break;
                case 's':
                {
                    char *str = (char *)va_arg(args, char *);
                    size_t size = strlen((const char *)&data[offset]) + 1;
                    memcpy(str, &data[offset], size);
                    RPC_DBG("UNPACK: string -> %s\n", str);
                    offset += size;
                }
                break;
            }

            fmt++;
        }
    }

    return offset;
}

void rpc_result_push_uint8(rpc_server_t *me, uint8_t value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    RPC_DBG("RESPUSH: uint8_t -> %"PRIu8"\n", value);
    value = value;
    memcpy(&reply->data[data_len], &value, sizeof(uint8_t));
    data_len += sizeof(uint8_t);
    reply->data_len = htobe16(data_len);
}

void rpc_result_push_int8(rpc_server_t *me, int8_t value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    RPC_DBG("RESPUSH: int8_t -> %"PRIi8"\n", value);
    value = value;
    memcpy(&reply->data[data_len], &value, sizeof(int8_t));
    data_len += sizeof(int8_t);
    reply->data_len = htobe16(data_len);
}


void rpc_result_push_uint16(rpc_server_t *me, uint16_t value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    RPC_DBG("RESPUSH: uint16_t -> %"PRIu16"\n", value);
    value = htobe16(value);
    memcpy(&reply->data[data_len], &value, sizeof(uint16_t));
    data_len += sizeof(uint16_t);
    reply->data_len = htobe16(data_len);
}

void rpc_result_push_int16(rpc_server_t *me, int16_t value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    RPC_DBG("RESPUSH: int16_t -> %"PRIi16"\n", value);
    value = htobe16(value);
    memcpy(&reply->data[data_len], &value, sizeof(int16_t));
    data_len += sizeof(int16_t);
    reply->data_len = htobe16(data_len);
}

void rpc_result_push_uint32(rpc_server_t *me, uint32_t value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    RPC_DBG("RESPUSH: uint32_t -> %"PRIu32"\n", value);
    value = htobe32(value);
    memcpy(&reply->data[data_len], &value, sizeof(uint32_t));
    data_len += sizeof(uint32_t);
    reply->data_len = htobe16(data_len);
}

void rpc_result_push_int32(rpc_server_t *me, int32_t value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    RPC_DBG("RESPUSH: int32_t -> %"PRIi32"\n", value);
    value = htobe32(value);
    memcpy(&reply->data[data_len], &value, sizeof(int32_t));
    data_len += sizeof(int32_t);
    reply->data_len = htobe16(data_len);
}

void rpc_result_push_uint64(rpc_server_t *me, uint64_t value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    RPC_DBG("RESPUSH: uint64_t -> %"PRIu64"\n", value);
    value = htobe64(value);
    memcpy(&reply->data[data_len], &value, sizeof(uint64_t));
    data_len += sizeof(uint64_t);
    reply->data_len = htobe16(data_len);
}

void rpc_result_push_int64(rpc_server_t *me, int64_t value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    RPC_DBG("RESPUSH: int64_t -> %"PRIi64"\n", value);
    value = htobe64(value);
    memcpy(&reply->data[data_len], &value, sizeof(int64_t));
    data_len += sizeof(int64_t);
    reply->data_len = htobe16(data_len);
}

void rpc_result_push_float(rpc_server_t *me, float value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    RPC_DBG("RESPUSH: float -> %f\n", value);
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

    RPC_DBG("RESPUSH: double -> %0.15f\n", value);
    rpc_data_type_t v;
    v.dbl = value;
    v.u64 = htobe64(v.u64);
    memcpy(&reply->data[data_len], &v.u64, sizeof(double));
    data_len += sizeof(double);
    reply->data_len = htobe16(data_len);
}

void rpc_result_push_string(rpc_server_t *me, const char *value, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);
    const char *empty = "";

    if (!value) {
        value = empty;
    }

    RPC_DBG("RESPUSH: string -> '%s'\n", value);
    rpc_data_type_t v;
    v.str = value;
    size_t size = strlen(v.str) + 1; /* Including the NULL termination character */
    memcpy(&reply->data[data_len], v.str, size);
    data_len += size;
    reply->data_len = htobe16(data_len);
}

int rpc_connect(rpc_client_t *me, uint16_t node) {

    /* Create an RDP connection with the particular RPC server node */
    RPC_DBG("RPC: Connecting the RPC service on %"PRIu16"\n", node);
    me->conn = csp_connect(CSP_PRIO_HIGH, node, CSP_PORT_RPC_SERVER, 0, CSP_O_RDP);
    if (me->conn == NULL) {
        RPC_DBG("RPC: Could not connect to RPC service\n");
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

    /* Lookup the RPC program and there after the procedure format */
    const rpc_program_t *program = lookup_program_from_id(&me->module, be32toh(msg->call.program));
    const rpc_procedure_t *rpc = lookup_procedure_from_id(program, be32toh(msg->call.procedure));
    uint16_t data_len = be16toh(msg->call.data_len);

    if (rpc) {
        va_list(args);
        va_start(args, msg);

        rpc_unpack(&msg->call.data[0], data_len, rpc->arg_fmt, args);

        va_end(args);
    }

    return 0;
}

int rpc_call_invoke(rpc_client_t *me, uint32_t program, uint32_t procedure, ...) {

    /* Allocate the CSP packet for the RPC object */
    csp_packet_t *request = csp_buffer_get(0);

    /* Create the RPC call protocol message to send to the RPC server */
    va_list args;
    va_start(args, procedure);

    /* Find the associated program object */
    const rpc_program_t *prg = lookup_program_from_id(&me->module, program);

    if (!prg) {
        RPC_DBG("ERROR: RPC client could not find program: 0x%08"PRIX32"\n", program);
        return -1;
    }

    rpc_msg_t *req_msg = (rpc_msg_t *)request->data;
    req_msg->type = RPC_MSG_CALL;
    req_msg->version = RPC_VERSION;
    req_msg->xid = htobe32(0x12341234); /* TODO: We need to have a sequence number at this place */
    req_msg->call.data_len = 0;
    req_msg->call.program = htobe32(program);
    req_msg->call.procedure = htobe32(procedure);
    request->length = sizeof(req_msg->type) + sizeof(req_msg->version) + sizeof(req_msg->xid);
    request->length += rpc_pack_call_request(prg, &req_msg->call, &args);

    /* Send the RPC call to the RPC server */
    csp_send(me->conn, request);

    /* Wait for the reply from the client */
    csp_packet_t *reply = csp_read(me->conn, 100);
    if (reply) {
        const rpc_procedure_t *rpc = lookup_procedure_from_id(prg, procedure);
        rpc_msg_t *msg = (rpc_msg_t *)reply->data;
        uint16_t data_len = be16toh(msg->reply.data_len);

        RPC_DBG("rpc_reply: data_len=%"PRId16", data=%p\n", data_len, &msg->reply.data[0]);

        if (rpc) {
            rpc_unpack(&msg->reply.data[0], data_len, rpc->res_fmt, args);
        }

        csp_buffer_free(reply);
    } else {
        RPC_DBG("RPC: Timeout waiting for reply\n");
    }

    va_end(args);

    return 0;
}

static csp_packet_t * rpc_result_prepare(rpc_server_t *me, rpc_msg_t *msg) {

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

static csp_packet_t * rpc_handle_msg(rpc_server_t *me, csp_packet_t *packet) {

    rpc_msg_t *req_msg = (rpc_msg_t *)packet->data;
    csp_packet_t *reply = NULL;

    if (req_msg->version != RPC_VERSION) {
        RPC_DBG("ERROR: Wrong RPC version in message.\n");
        return NULL;
    }

    switch (req_msg->type) {
        case RPC_MSG_CALL:
        {
            uint16_t data_len = be16toh(req_msg->call.data_len);
            uint32_t program = be32toh(req_msg->call.program);
            uint32_t procedure = be32toh(req_msg->call.procedure);

            RPC_DBG("rpc_msg_call: data_len=%"PRId16",program=0x%"PRIX32",procedure=%"PRIu32"\n", data_len, program, procedure);

            reply = rpc_result_prepare(me, req_msg);

            /* Find a possibly matching program handler */
            const rpc_program_t *prg = lookup_program_from_id(&me->module, program);
            if (prg) {
                /* We found a match, call the associated handler */
                rpc_msg_t *reply_msg = (rpc_msg_t *)reply->data;
                (*prg->handler)(me, program, procedure, req_msg, reply_msg, prg->data);
                reply->length += be16toh(reply_msg->reply.data_len);
                RPC_DBG("rpc_msg_reply: data_len=%"PRId16"\n", be16toh(reply_msg->reply.data_len));
                /* Do not try to find any other matches */
                break;
            }
        }
        break;
    }

    return reply;

}

int rpc_init_client(rpc_client_t *me) {

    /* Initializing the client, also means parsing the programs registered */
    extern const rpc_program_t __start_rpc_programs;
    extern const rpc_program_t __stop_rpc_programs;
    const rpc_program_t *iter = &__start_rpc_programs;
    me->module.nof_programs = 0;
    me->module.programs = &__start_rpc_programs;
    RPC_DBG("RPC Client: Registering programs from address: %p\n", me->module.programs);
    while (iter != &__stop_rpc_programs) {
        RPC_DBG("  0x%08"PRIX32", '%s'\n", iter->program_id, iter->name);
        me->module.nof_programs++;
        iter++;
    }
    RPC_DBG("RPC Client: Registered %"PRId32" programs\n", me->module.nof_programs);

    global_rpc_client = me;

    return 0;
}

int rpc_start_server(rpc_server_t *me) {

    /* Starting the server, also means parsing the programs registered */
    extern const rpc_program_t __start_rpc_programs;
    extern const rpc_program_t __stop_rpc_programs;
    const rpc_program_t *iter = &__start_rpc_programs;
    me->module.nof_programs = 0;
    me->module.programs = &__start_rpc_programs;
    RPC_DBG("RPC Server: Registering programs from address: %p\n", me->module.programs);
    while (iter != &__stop_rpc_programs) {
        RPC_DBG("  0x%08"PRIX32", '%s'\n", iter->program_id, iter->name);
        me->module.nof_programs++;
        iter++;
    }
    RPC_DBG("RPC Server: Registered %"PRId32" programs\n", me->module.nof_programs);

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

    global_rpc_server = me;

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

bool rpc_waitfor_connections(rpc_server_t *me) {

    if ((me->conn = csp_accept(&me->sock, 10000)) == NULL)
    {
        /* timeout */
        return false;
    }

    uint16_t src = csp_conn_src(me->conn);
    uint16_t sport = csp_conn_sport(me->conn);

    RPC_DBG("RPC: Incoming connection from: %"PRIu16":%"PRIu16"\n", src, sport);

    return true;
}

bool rpc_handle_connection(rpc_server_t *me) {

    /* Read request packets on connection, timeout is 10 s */
    csp_packet_t *request = csp_read(me->conn, 10000);
    if (NULL == request) {
        /* The connection is lost, tell the caller */
        RPC_DBG("RPC: Client disconnected or timeout.\n");
        return false;
    }

    /* Handle the RPC request (call) and send the reply */
    csp_packet_t *reply = NULL;
    reply = rpc_handle_msg(me, request);
    if (reply) {
        csp_send(me->conn, reply);
    }

    /* We still have a valid connection */
    return true;
}

int rpc_fetch_first(uint16_t node, rpc_fetch_result_t *result) {

    int res = -1;

    res = rpc_connect(global_rpc_client, node);
    if (!res) {
        res = rpc_call_invoke(global_rpc_client, RPC_PROGRAM_RPC, RPC_PROCEDURE_FETCH_FIRST,
            /*ARGS*/
                /*void*/
            /*RETURN*/
                &result->result,
                &result->program_id,
                &result->program_name[0],
                &result->procedure_id,
                &result->procedure_name[0],
                &result->arg_fmt[0],
                &result->res_fmt[0]
        );
        if (res) {
            RPC_DBG("DSUC: Could not call RPC_PROCEDURE_FETCH - %i\n", res);
        }
        rpc_disconnect(global_rpc_client);
    }

    return res;
}

int rpc_fetch_next(uint16_t node, rpc_fetch_result_t *result) {

    int res = -1;

    res = rpc_connect(global_rpc_client, node);
    if (!res) {
        res = rpc_call_invoke(global_rpc_client, RPC_PROGRAM_RPC, RPC_PROCEDURE_FETCH_NEXT,
            /*ARGS*/
                /*void*/
            /*RETURN*/
                &result->result,
                &result->program_id,
                &result->program_name[0],
                &result->procedure_id,
                &result->procedure_name[0],
                &result->arg_fmt[0],
                &result->res_fmt[0]
        );
        if (res) {
            RPC_DBG("DSUC: Could not call RPC_PROCEDURE_FETCH - %i\n", res);
        }
        rpc_disconnect(global_rpc_client);
    }

    return res;
}

typedef struct rpc_prg_data_s {
    const rpc_program_t *iter_prg;
    const rpc_procedure_t *iter_proc;
} rpc_prg_data_t;

static void rpc_fetch_push_empty(rpc_server_t *me, rpc_msg_t *reply) {

    /* Push an empty result to indicate the end */
    rpc_result_push_int32(me, -1, reply);
    rpc_result_push_uint32(me, 0, reply);
    rpc_result_push_string(me, "", reply);
    rpc_result_push_uint32(me, 0, reply);
    rpc_result_push_string(me, "", reply);
    rpc_result_push_string(me, "", reply);
    rpc_result_push_string(me, "", reply);
}

static void rpc_program_handler(rpc_server_t *me, uint32_t program, uint32_t procedure, rpc_msg_t *call, rpc_msg_t *reply, void *data) {

    extern const rpc_program_t __start_rpc_programs;
    extern const rpc_program_t __stop_rpc_programs;
    rpc_prg_data_t *prg_data = (rpc_prg_data_t *)data;

    switch (procedure) {
        case RPC_PROCEDURE_FETCH_FIRST:
        {
            RPC_DBG("RPC: rpc_fetch_first()\n");

            /* Unconditionally reset the iterators in the get first operation */
            prg_data->iter_prg = NULL;
            prg_data->iter_proc = NULL;

            /* Verify that we have programs at all */
            if (&__stop_rpc_programs > &__start_rpc_programs) {
                /* Setup initial iterator */
                prg_data->iter_prg = &__start_rpc_programs;
                prg_data->iter_proc = prg_data->iter_prg->procedures;
                /* Check to see if the procedure list is empty */
                if (!prg_data->iter_proc || prg_data->iter_proc->id == 0xFFFFFFFFUL) {
                    /* The end termination of the procedure list */
                    rpc_fetch_push_empty(me, reply);
                } else {
                    /* Valid program and procedure list */
                    rpc_result_push_int32(me, 0, reply);
                    rpc_result_push_uint32(me, prg_data->iter_prg->program_id, reply);
                    rpc_result_push_string(me, prg_data->iter_prg->name, reply);
                    rpc_result_push_uint32(me, prg_data->iter_proc->id, reply);
                    rpc_result_push_string(me, prg_data->iter_proc->name, reply);
                    rpc_result_push_string(me, prg_data->iter_proc->arg_fmt, reply);
                    rpc_result_push_string(me, prg_data->iter_proc->res_fmt, reply);
                    /* Advance the procedure iterator */
                    prg_data->iter_proc++;
                }
            } else {
                /* The program list is empty */
                rpc_fetch_push_empty(me, reply);
            }
        }
        break;
        case RPC_PROCEDURE_FETCH_NEXT:
        {
            RPC_DBG("RPC: rpc_fetch_next()\n");
            bool last = true;

            /* Have we reached the end of programs list */
            if (prg_data->iter_prg) {
                /* If not, then we might have more procedures */
                if (!prg_data->iter_proc || prg_data->iter_proc->id == 0xFFFFFFFFUL) {
                    /* Advance the program iterator */
                    prg_data->iter_prg++;
                    /* Verify that we are within the bounds */
                    if (prg_data->iter_prg < &__stop_rpc_programs) {
                        /* Restart the procedure iterator */
                        prg_data->iter_proc = prg_data->iter_prg->procedures;
                    } else {
                        prg_data->iter_prg = NULL;
                        prg_data->iter_proc = NULL;
                    }
                }

                if (prg_data->iter_prg && prg_data->iter_proc && prg_data->iter_proc->id != 0xFFFFFFFFUL) {
                    /* Valid program and procedure list */
                    rpc_result_push_int32(me, 0, reply);
                    rpc_result_push_uint32(me, prg_data->iter_prg->program_id, reply);
                    rpc_result_push_string(me, prg_data->iter_prg->name, reply);
                    rpc_result_push_uint32(me, prg_data->iter_proc->id, reply);
                    rpc_result_push_string(me, prg_data->iter_proc->name, reply);
                    rpc_result_push_string(me, prg_data->iter_proc->arg_fmt, reply);
                    rpc_result_push_string(me, prg_data->iter_proc->res_fmt, reply);
                    /* Advance the procedure iterator */
                    prg_data->iter_proc++;
                    last = false;
                }
            }

            if (last) {
                rpc_fetch_push_empty(me, reply);
            }
        }
        break;
        default:
            RPC_DBG("RPC: Unhandled RPC procedure call: 0x%"PRIX32"\n", procedure);
            break;
    }
}

typedef enum rpc_proc_arg_type_e {
    RPC_SIGNED_CHAR = 0,
    RPC_UNSIGNED_CHAR = 1,
    RPC_INT8 = 2,
    RPC_UINT8 = 3,
    RPC_INT16 = 4,
    RPC_UINT16 = 5,
    RPC_INT32 = 6,
    RPC_UINT32 = 7,
    RPC_INT64 = 8,
    RPC_UINT64 = 9,
    RPC_FLOAT = 10,
    RPC_DOUBLE = 11,
    RPC_STRING = 12,
} rpc_proc_arg_type_t;

static rpc_prg_data_t g_prg_data = {
    .iter_prg = NULL,
    .iter_proc = NULL,
};

static const rpc_procedure_t g_rpc_procedures[] = {
    { .id = RPC_PROCEDURE_FETCH_FIRST, .name = "rpc_fetch_first", .arg_fmt = NULL, .res_fmt = "lLsLsss" },
        .args = NULL,
        .result = (const rpc_proc_arg_t [])
        {
            { .name = "status", .type = RPC_INT32 },
            { .name = "program_id", .type = RPC_UINT32 },
            { .name = "program_name", .type = RPC_STRING },
            { .name = "procedure_id", .type = RPC_UINT32 },
            { .name = "procedure_name", .type = RPC_STRING },
            { .name = "arg_fmt", .type = RPC_STRING },
            { .name = "res_fmt", .type = RPC_STRING },
            RPC_PROC_ARG_NULL_INIT,
        },
    { .id = RPC_PROCEDURE_FETCH_NEXT, .name = "rpc_fetch_next", .arg_fmt = NULL, .res_fmt = "lLsLsss",
        .args = NULL,
        .result = (const rpc_proc_arg_t [])
        {
            { .name = "status", .type = RPC_INT32 },
            { .name = "program_id", .type = RPC_UINT32 },
            { .name = "program_name", .type = RPC_STRING },
            { .name = "procedure_id", .type = RPC_UINT32 },
            { .name = "procedure_name", .type = RPC_STRING },
            { .name = "arg_fmt", .type = RPC_STRING },
            { .name = "res_fmt", .type = RPC_STRING },
            RPC_PROC_ARG_NULL_INIT,
        },
    },
    RPC_PROCEDURE_NULL_INIT,
};

RPC_DECLARE_PROGRAM( rpc_server, RPC_PROGRAM_RPC, rpc_program_handler, NULL, &g_rpc_procedures[0], &g_prg_data );
