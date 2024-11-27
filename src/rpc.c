#include <endian.h>
#include <stdarg.h>
#include <stdio.h>

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

    const rpc_program_t *program = NULL;

    /* Search the local data base initially */
    for (uint32_t n = 0; n < module->nof_programs; n++) {
        if (module->programs[n].program_id == id) {
            program = &module->programs[n];
            break;
        }
    }

    /* Search the remote data base if we did not find a match locally */
    if (!program) {
        rpc_program_t *iter = SLIST_FIRST( &module->remote );
        while (iter) {
            if (iter->program_id == id) {
                program = iter;
                break;
            }
            iter = SLIST_NEXT( iter, list );
        }
    }

    return program;
}

static const rpc_procedure_t * lookup_procedure_from_id(const rpc_program_t *program, uint32_t id) {

    const rpc_procedure_t *procedure = NULL;
    const rpc_procedure_t *iter = NULL;
    
    if (program->remote) {
        iter = SLIST_FIRST( &program->remote_proc );
        while (iter) {
            if (iter->id == id) {
                procedure = iter;
                break;
            }
            iter = SLIST_NEXT( iter, list );
        }
    } else {
        iter = program->procedures;
        while (iter && iter->id != 0xFFFFFFFF) {
            if (iter->id == id) {
                procedure = iter;
                break;
            }
            iter++;
        }
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
                    case 'p':
                    {
                        /* Special case with two-args; one for length and one with a pointer */
                        uint16_t bytes_len = (uint16_t)va_arg(*args, unsigned int);
                        const char *bytes = (const char *)va_arg(*args, const char *);
                        RPC_DBG("PACK: data[%"PRIu16"] -> ...\n", bytes_len);
                        /* Write the data buffer length into the packet */
                        rpc_data_type_t value;
                        value.u16 = htobe16(bytes_len);
                        memcpy(&(call->data[data_len]), &value, sizeof(value.u16));
                        data_len += sizeof(value.u16);
                        /* Write the actual data buffer content into the packet */
                        memcpy(&(call->data[data_len]), bytes, bytes_len);
                        data_len += bytes_len;
                    }
                    break;
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
                case 'p':
                {
                    /* Special case of 'p', which is a pointer to a buffer of bytes of a certain length */
                    uint32_t bytes_len = (uint32_t)va_arg(args, uint32_t);
                    uint8_t *bytes = (uint8_t *)va_arg(args, uint8_t *);
                    /* Grab the length of the bytes buffer from the packet */
                    rpc_data_type_t _len;
                    memcpy(&_len, &data[offset], sizeof(_len.u16));
                    _len.u16 = be16toh(_len.u16);
                    RPC_DBG("UNPACK: bytes[%"PRIu16"] -> ...\n", _len.u16);
                    /* Advance past the <length> field in the packet */
                    offset += sizeof(_len.u16);
                    uint16_t copylen = _len.u16;
                    /* Truncate to the receiving buffer capability */
                    if (copylen > bytes_len) {
                        copylen = bytes_len;
                    }
                    memcpy(bytes, &data[offset], copylen);
                    offset += copylen;
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

void rpc_result_push_buffer(rpc_server_t *me, const uint8_t *value, uint16_t len, rpc_msg_t *msg) {

    rpc_reply_t *reply = &msg->reply;
    uint16_t data_len = be16toh(reply->data_len);

    RPC_DBG("RESPUSH: buffer[%"PRIu16"] -> ...\n", len);
    /* Put the buffer length into the packet */
    rpc_data_type_t v;
    v.u16 = len;
    v.u16 = be16toh(v.u16);
    memcpy(&reply->data[data_len], &v.u16, sizeof(uint16_t));
    data_len += sizeof(uint16_t);
    /* Put the data into the packet - if any */
    if (len > 0) {
        memcpy(&reply->data[data_len], value, len);
        data_len += len;
    }

    reply->data_len = htobe16(data_len);
}

int rpc_connect(rpc_client_t *me, uint16_t node) {

    if (!me->conn) {
        /* Create an RDP connection with the particular RPC server node */
        RPC_DBG("RPC-C: Connecting the RPC service on %"PRIu16"\n", node);
        me->conn = csp_connect(CSP_PRIO_HIGH, node, CSP_PORT_RPC_SERVER, 0, CSP_O_NONE);
        if (me->conn == NULL) {
            RPC_DBG("RPC-C: Could not connect to RPC service\n");
            return -1;
        }
        RPC_DBG("RPC-C: Connected to service\n");
    }

    return 0;
}

int rpc_disconnect(rpc_client_t *me) {

    if (me && me->conn) {
        RPC_DBG("RPC-C: Close connection\n");
        csp_close(me->conn);
        me->conn = NULL;
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
    if (!request) {
        RPC_ERR("RPC-C: Could not get CSP buffer\n");
        return -1;
    }

    /* Setup the argument list */
    va_list args;
    va_start(args, procedure);

    /* Grab any possible call back to use if multiple replies comes along */
    void (*multires_cb)(uint32_t, va_list) = (void (*)(uint32_t, va_list))va_arg(args, void *);

    /* Find the associated program object */
    const rpc_program_t *prg = lookup_program_from_id(&me->module, program);
    if (!prg) {
        RPC_ERR("RPC-C: Error, could not find program: 0x%08"PRIX32"\n", program);
        csp_buffer_free(request);
        return -1;
    }

    /* Create the RPC call protocol message to send to the RPC server */
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

    /* Start processing the possible replies from the RPC server */
    bool keep_replying = false;
    do {
        /* Wait for the reply from the client - this might take up to 10 seconds */
        csp_packet_t *reply = csp_read(me->conn, 10000);
        if (reply) {
            rpc_msg_t *msg = (rpc_msg_t *)reply->data;
            if (msg->type != RPC_MSG_REPLY) {
                /* We discard any other thing than REPLY messages here */
                csp_buffer_free(reply);
                continue;
            }
            const rpc_procedure_t *rpc = lookup_procedure_from_id(prg, procedure);
            uint16_t data_len = be16toh(msg->reply.data_len);
            uint32_t amount = be32toh(msg->reply.amount);
            uint32_t idx = be32toh(msg->reply.idx);

            /* There are still more to come */
            keep_replying = (idx < amount) ? true : false;

            RPC_DBG("RPC-C: rpc_reply: (%" PRIu32 " of %" PRIu32 ") data_len=%"PRId16", data=%p\n", idx, amount, data_len, &msg->reply.data[0]);

            if (rpc) {
                va_list __args;
                /* Grab a copy of the argument list */
                va_copy(__args, args);
                rpc_unpack(&msg->reply.data[0], data_len, rpc->res_fmt, __args);
                va_end(__args);
                if (multires_cb) {
                    /* Grab a copy of the argument list */
                    va_copy(__args, args);
                    (*multires_cb)(procedure, __args);
                    va_end(__args);
                }
            }

            csp_buffer_free(reply);
        } else {
            RPC_DBG("RPC-C: Timeout waiting for reply\n");
            return -1;
        }
    } while (keep_replying);

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
        reply_msg->reply.data_len = htobe32(0);
        reply_msg->reply.amount = htobe32(0);
        reply_msg->reply.idx = htobe32(0);
        packet->length = sizeof(reply_msg->type) + sizeof(reply_msg->version) + sizeof(reply_msg->xid) + sizeof(reply_msg->reply);
    }

    return packet;
}

static csp_packet_t * rpc_handle_msg(rpc_server_t *me, csp_packet_t *packet) {

    rpc_msg_t *req_msg = (rpc_msg_t *)packet->data;
    csp_packet_t *reply = NULL;

    if (req_msg->version != RPC_VERSION) {
        RPC_ERR("RPC-S: Error, wrong RPC version in message.\n");
        return NULL;
    }

    switch (req_msg->type) {
        case RPC_MSG_CALL:
        {
            uint16_t data_len = be16toh(req_msg->call.data_len);
            uint32_t program = be32toh(req_msg->call.program);
            uint32_t procedure = be32toh(req_msg->call.procedure);

            RPC_DBG("RPC-S: rpc_msg_call: data_len=%"PRId16",program=0x%"PRIX32",procedure=%"PRIu32"\n", data_len, program, procedure);

            bool more = false;
            do {
                reply = rpc_result_prepare(me, req_msg);
                if (reply) {
                    /* Find a possibly matching program handler */
                    const rpc_program_t *prg = lookup_program_from_id(&me->module, program);
                    if (prg) {
                        /* We found a match, call the associated handler */
                        rpc_msg_t *reply_msg = (rpc_msg_t *)reply->data;
                        more = (*prg->handler)(me, program, procedure, req_msg, reply_msg, prg->data);
                        reply->length += be16toh(reply_msg->reply.data_len);
                        RPC_DBG("RPC-S: rpc_msg_reply: data_len=%"PRId16"\n", be16toh(reply_msg->reply.data_len));
                    }

                    RPC_DBG("RPC-S: Send reply\n");
                    csp_send(me->conn, reply);
                    reply = NULL;
                }
            } while (more);
        }
        break;
    }

    return reply;

}

void rpc_set_reply_header(rpc_reply_t *reply, uint32_t amount, uint32_t idx) {

    reply->amount = htobe32(amount);
    reply->idx = htobe32(idx);
}

rpc_program_t *rpc_register_remote_program(rpc_client_t *me, uint16_t node, uint32_t program_id) {

    /* Iterate thru the list of remote programs to see if we already have it */
    rpc_program_t *iter = SLIST_FIRST( &me->module.remote );
    while (iter) {
        if ((iter->program_id == program_id) && (iter->node == node)) {
            break;
        }
        iter = SLIST_NEXT( iter, list );
    }

    if (!iter) {
        /* We have not found a program by that id, grab a new one from the list */
        iter = SLIST_FIRST( &me->program_slot );
        if (iter) {
            SLIST_REMOVE( &me->program_slot, iter, rpc_program_s, list );
            iter->program_id = program_id;
            iter->node = node;
            SLIST_INIT( &iter->remote_proc );
            SLIST_INSERT_HEAD( &me->module.remote, iter, list );
        }
    }

    return iter;
}

rpc_procedure_t *rpc_register_remote_procedure(rpc_client_t *me, rpc_program_t *program, uint32_t procedure_id) {

    /* Iterate through the procedure list to see if we already have it */
    rpc_procedure_t *iter = SLIST_FIRST( &program->remote_proc );
    while (iter) {
        if (iter->id == procedure_id) {
            break;
        }
        iter = SLIST_NEXT( iter, list );
    }

    if (!iter) {
        iter = SLIST_FIRST( &me->procedure_slot );
        if (iter) {
            SLIST_REMOVE( &me->procedure_slot, iter, rpc_procedure_s, list );
            iter->id = procedure_id;
            SLIST_INSERT_HEAD( &program->remote_proc, iter, list );
        }
    }

    return iter;
}

void rpc_list_remote_programs(rpc_client_t *me, uint16_t node) {

    rpc_program_t *prg_iter = SLIST_FIRST( &me->module.remote );
    while(prg_iter) {
        if (prg_iter->node == node) {
            rpc_procedure_t *pro_iter = SLIST_FIRST( &prg_iter->remote_proc );
            while (pro_iter) {
                printf("PRG: %" PRIu16 ":0x%08" PRIX32 ", %s:%" PRIu32 "('%s') => '%s'\n", prg_iter->node, prg_iter->program_id, pro_iter->name, pro_iter->id, pro_iter->arg_fmt, pro_iter->res_fmt);
                pro_iter = SLIST_NEXT( pro_iter, list );
            }
        }
        prg_iter  = SLIST_NEXT( prg_iter, list );
    }
}
void rpc_remove_remote_programs(rpc_client_t *me, uint16_t node) {

    rpc_program_t *prg_iter = SLIST_FIRST( &me->module.remote );
    while(prg_iter) {
        rpc_program_t *prg_next = SLIST_NEXT( prg_iter, list );
        if (prg_iter->node == node) {
            rpc_procedure_t *pro_iter = SLIST_FIRST( &prg_iter->remote_proc );
            while (pro_iter) {
                rpc_procedure_t *pro_next = SLIST_NEXT( pro_iter, list );
                SLIST_REMOVE( &prg_iter->remote_proc, pro_iter, rpc_procedure_s, list );
                SLIST_INSERT_HEAD( &me->procedure_slot, pro_iter, list );
                pro_iter = pro_next;
            }
            SLIST_REMOVE( &me->module.remote, prg_iter, rpc_program_s, list );
            SLIST_INSERT_HEAD( &me->program_slot, prg_iter, list );
        }
        prg_iter  = prg_next;
    }
}

int rpc_init_client(rpc_client_t *me, uint32_t nof_programs, rpc_program_t *programs, uint32_t nof_procedures, rpc_procedure_t *procedures) {

    /* Initializing the client, also means parsing the programs registered */
    extern const rpc_program_t __start_rpc_programs;
    extern const rpc_program_t __stop_rpc_programs;
    const rpc_program_t *iter = &__start_rpc_programs;
    me->module.nof_programs = 0;
    me->module.programs = &__start_rpc_programs;
    RPC_DBG("RPC-C: Registering programs from address: %p\n", me->module.programs);
    while (iter != &__stop_rpc_programs) {
        RPC_DBG("  0x%08" PRIX32 ", '%s'\n", iter->program_id, iter->name);
        me->module.nof_programs++;
        iter++;
    }
    RPC_DBG("RPC-C: Registered %"PRId32" programs\n", me->module.nof_programs);

    /* Initialize the remote program lists */
    SLIST_INIT( &me->module.remote );

    /* Initialize the remote programs and procedures place holders */
    SLIST_INIT( &me->program_slot );
    SLIST_INIT( &me->procedure_slot );

    /* Fill up the program slots */
    if (nof_programs && programs) {
        while (nof_programs) {
            programs->remote = true;
            programs->node = 0; /* TODO: Currently not used */
            SLIST_INIT( &programs->remote_proc );
            SLIST_INSERT_HEAD( &me->program_slot, programs, list );
            programs++;
            nof_programs--;
        }
    }

    /* Fill up the procedure slots */
    if (nof_procedures && procedures) {
        while (nof_procedures) {
            SLIST_INSERT_HEAD( &me->procedure_slot, procedures, list );
            procedures++;
            nof_procedures--;
        }
    }

    global_rpc_client = me;

    me->conn = NULL;

    return 0;
}

int rpc_start_server(rpc_server_t *me) {

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

    RPC_DBG("RPC-S: Incoming connection from: %"PRIu16":%"PRIu16"\n", src, sport);

    return true;
}

bool rpc_handle_connection(rpc_server_t *me) {

    /* Read request packets on connection, timeout is 10 s */
    csp_packet_t *request = csp_read(me->conn, 100000);
    if (NULL == request) {
        /* The connection is lost, tell the caller */
        RPC_DBG("RPC-S: Client disconnected or timeout.\n");
        return false;
    }

    RPC_DBG("RPC-S: Handle message\n");
    /* Handle the RPC request (call) and send the reply */
    csp_packet_t *reply = NULL;
    reply = rpc_handle_msg(me, request);
    if (reply) {
        RPC_DBG("RPC-S: Send reply\n");
        csp_send(me->conn, reply);
    }

    /* Free the request packet */
    csp_buffer_free(request);

    /* We still have a valid connection */
    return true;
}

int rpc_fetch_first(uint16_t node, rpc_fetch_result_t *result) {

    int res = -1;

    res = rpc_connect(global_rpc_client, node);
    if (!res) {
        res = rpc_call_invoke(global_rpc_client, RPC_PROGRAM_RPC, RPC_PROCEDURE_FETCH_FIRST,
            /* CALLBACK */
                NULL,
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
            /* CALLBACK */
                NULL,
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

int rpc_fetch_all(uint16_t node, rpc_fetch_result_t *result, void (*result_cb)(uint32_t index, va_list args)) {

    int res = -1;

    res = rpc_connect(global_rpc_client, node);
    if (!res) {
        res = rpc_call_invoke(global_rpc_client, RPC_PROGRAM_RPC, RPC_PROCEDURE_FETCH_ALL,
            /* CALLBACK */
                result_cb,
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
            RPC_DBG("DSUC: Could not call RPC_PROCEDURE_FETCH_ALL - %i\n", res);
        }
        rpc_disconnect(global_rpc_client);
    }

    return res;
}

typedef struct rpc_prg_data_s {
    const rpc_program_t *iter_prg;
    const rpc_procedure_t *iter_proc;
    uint32_t current_procedure;
    int32_t nof_procedures;
} rpc_prg_data_t;

static void rpc_fetch_result_push(rpc_server_t *me, rpc_msg_t *reply, const rpc_program_t *prg, const rpc_procedure_t *proc) {

    if (prg && proc) {
        rpc_result_push_int32(me, 0, reply);
        rpc_result_push_uint32(me, prg->program_id, reply);
        rpc_result_push_string(me, prg->name, reply);
        rpc_result_push_uint32(me, proc->id, reply);
        rpc_result_push_string(me, proc->name, reply);
        rpc_result_push_string(me, proc->arg_fmt, reply);
        rpc_result_push_string(me, proc->res_fmt, reply);
    } else {
        /* Push an empty result to indicate the end */
        rpc_result_push_int32(me, -1, reply);
        rpc_result_push_uint32(me, 0, reply);
        rpc_result_push_string(me, "", reply);
        rpc_result_push_uint32(me, 0, reply);
        rpc_result_push_string(me, "", reply);
        rpc_result_push_string(me, "", reply);
        rpc_result_push_string(me, "", reply);
    }
}

static uint32_t count_number_of_procedures(void) {

    extern const rpc_program_t __start_rpc_programs;
    extern const rpc_program_t __stop_rpc_programs;
    uint32_t nof_procedures = 0;

    /* Count the total number of procedures */
    const rpc_program_t *iter_prg = &__start_rpc_programs;
    while (iter_prg < &__stop_rpc_programs) {
        const rpc_procedure_t *iter_proc = iter_prg->procedures;
        while (iter_proc && iter_proc->id != 0xFFFFFFFFUL) {
            nof_procedures++;
            iter_proc++;
        }
        iter_prg++;
    }

    return nof_procedures;
}

static bool find_first_procedure(rpc_prg_data_t *data) {
    
    extern const rpc_program_t __start_rpc_programs;
    extern const rpc_program_t __stop_rpc_programs;
    bool found = false;

    data->iter_prg = &__start_rpc_programs;
    while (data->iter_prg < &__stop_rpc_programs) {
        data->iter_proc = data->iter_prg->procedures;
        if (data->iter_proc && data->iter_proc->id != 0xFFFFFFFFUL) {
            found = true;
            break;
        }
        data->iter_prg++;
    }

    return found;
}

static bool find_next_procedure(rpc_prg_data_t *data) {

    extern const rpc_program_t __start_rpc_programs;
    extern const rpc_program_t __stop_rpc_programs;
    bool found = false;

    /* Advance to the next procedure in the list */
    data->iter_proc++;
    if (data->iter_proc->id != 0xFFFFFFFFUL) {
        found = true;
    } else {
        /* Advance the program iterator until we find a valid procedure */
        data->iter_prg++;
        while (data->iter_prg < &__stop_rpc_programs) {
            data->iter_proc = data->iter_prg->procedures;
            if (data->iter_proc && data->iter_proc->id != 0xFFFFFFFFUL) {
                found = true;
                break;
            }
            data->iter_prg++;
        }
    }

    return found;
}

static bool rpc_program_handler(rpc_server_t *me, uint32_t program, uint32_t procedure, rpc_msg_t *call, rpc_msg_t *reply, void *data) {

    extern const rpc_program_t __start_rpc_programs;
    extern const rpc_program_t __stop_rpc_programs;
    rpc_prg_data_t *prg_data = (rpc_prg_data_t *)data;

    bool more = false;

    switch (procedure) {
        case RPC_PROCEDURE_FETCH_FIRST:
        {
            RPC_DBG("RPC: rpc_fetch_first()\n");

            if (find_first_procedure(prg_data)) {
                /* Valid program and procedure list */
                rpc_fetch_result_push(me, reply, prg_data->iter_prg, prg_data->iter_proc);
            } else {
                /* The program list is empty */
                rpc_fetch_result_push(me, reply, NULL, NULL);
            }
        }
        break;
        case RPC_PROCEDURE_FETCH_NEXT:
        {
            RPC_DBG("RPC: rpc_fetch_next()\n");

            if (find_next_procedure(prg_data)) {
                /* Valid program and procedure list */
                rpc_fetch_result_push(me, reply, prg_data->iter_prg, prg_data->iter_proc);
            } else {
                /* The program list is empty */
                rpc_fetch_result_push(me, reply, NULL, NULL);
            }
        }
        break;
        case RPC_PROCEDURE_FETCH_ALL:
        {
            RPC_DBG("RPC: rpc_fetch_all()\n");

            /* This can be executed multiple times during an RPC request from a client */
            if (prg_data->nof_procedures < 0) {
                /* Initially, start by counting the total amount of procedures */
                prg_data->current_procedure = 0;
                prg_data->nof_procedures = count_number_of_procedures();
                /* Setup initial iterator */
                find_first_procedure(prg_data);
            } else {
                /* Find the next valid procedure */
                find_next_procedure(prg_data);
            }

            /* Setup the reply header information regarding the number of replys */
            rpc_set_reply_header(&reply->reply, prg_data->nof_procedures, prg_data->current_procedure);

            if (prg_data->current_procedure < prg_data->nof_procedures) {
                rpc_fetch_result_push(me, reply,  prg_data->iter_prg, prg_data->iter_proc);
                prg_data->current_procedure++;
                more = true;
            } else {
                rpc_fetch_result_push(me, reply, NULL, NULL);
                prg_data->nof_procedures = -1;
                more = false;
            }
        }
        break;
        default:
            RPC_DBG("RPC: Unhandled RPC procedure call: 0x%"PRIX32"\n", procedure);
            break;
    }

    return more;
}

static rpc_prg_data_t g_prg_data = {
    .iter_prg = NULL,
    .iter_proc = NULL,
    .nof_procedures = -1,
};

static const rpc_procedure_t g_rpc_procedures[] = {
    { .id = RPC_PROCEDURE_FETCH_FIRST, .name = "rpc_fetch_first", .arg_fmt = "", .res_fmt = "lLsLsss",
        .descr = "This method is used to fetch the first entry of RPC programs and it's associated procedures located on a module.",
        .args = NULL, /* void */
        .result = (const rpc_proc_arg_t [])
        {
            {
                .descr = "Validity of the entry. 0=valid, -1=invalid/empty",
                .name = "status", .type = RPC_INT32,
            },
            {
                .descr = "The program identifier uniquely identifying the program",
                .name = "program_id", .type = RPC_UINT32
            },
            {
                .descr = "The program name",
                .name = "program_name", .type = RPC_STRING
            },
            {
                .descr = "The procedure ID uniquely identifying the procedure for the particular program",
                .name = "procedure_id", .type = RPC_UINT32
            },
            {
                .descr = "The procedure name",
                .name = "procedure_name", .type = RPC_STRING
            },
            {
                .descr = "Argument format string",
                .name = "arg_fmt", .type = RPC_STRING
            },
            {
                .descr = "Result format string",
                .name = "res_fmt", .type = RPC_STRING
            },
            RPC_PROC_ARG_NULL_INIT,
        },
    },
    { .id = RPC_PROCEDURE_FETCH_NEXT, .name = "rpc_fetch_next", .arg_fmt = "", .res_fmt = "lLsLsss",
        .args = NULL, /* void */
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
    { .id = RPC_PROCEDURE_FETCH_ALL, .name = "rpc_fetch_all", .arg_fmt = "", .res_fmt = "lLsLsss",
        .args = NULL, /* void */
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

RPC_DECLARE_PROGRAM( rpc_server, RPC_PROGRAM_RPC, rpc_program_handler, &g_rpc_procedures[0], &g_prg_data );
