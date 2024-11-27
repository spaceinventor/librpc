#pragma once

#include <inttypes.h>
#include <sys/queue.h>
#include <stdarg.h>

#include <csp/csp.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The current RPC protocol version
 * 
 */
#define RPC_VERSION 1

/**
 * @brief The RPC server port to use
 * 
 */
#define CSP_PORT_RPC_SERVER 9

/**
 * @brief The RPC program id for the RPC server
 * 
 * This program id is used exclusively by the RPC
 * server for its "program". The RPC server program
 * implements the supporting procedures for "talking"
 * to the RPC server on a particular client node.
 * 
 */
#define RPC_PROGRAM_RPC 0xFFFFFFFF

/**
 * @brief RPC program procedure ID list
 * 
 */
typedef enum rpc_program_procedures_e {
    RPC_PROCEDURE_FETCH_FIRST = 0,
    RPC_PROCEDURE_FETCH_NEXT = 1,
    RPC_PROCEDURE_FETCH_ALL = 2,
} rpc_program_procedures_t;

typedef struct rpc_fetch_result_s {
    int32_t result;
    uint32_t program_id;
    char program_name[64];
    uint32_t procedure_id;
    char procedure_name[64];
    char arg_fmt[64];
    char res_fmt[64];
} rpc_fetch_result_t;

typedef enum rpc_msg_type_e {
    RPC_MSG_CALL = 0,
    RPC_MSG_REPLY = 1,
} rpc_msg_type_t;

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

typedef struct rpc_proc_arg_s {
    const char * const name;
    const uint8_t type;
    const char * const descr;
} rpc_proc_arg_t;

#define RPC_PROC_ARG_NULL_INIT { .name = "", .type = 0 }

/**
 * @brief RPC procedure object type
 * 
 * This type specifies a RPC function which is bound
 * into the RPC system.
 * 
 */
SLIST_HEAD( rpc_procedure_list_s, rpc_procedure_s );
typedef struct rpc_procedure_list_s rpc_procedure_list_t;

typedef struct rpc_procedure_s {
    uint32_t id;
    char name[64];
    char arg_fmt[64];
    char res_fmt[64];
    const char * const descr;
    const rpc_proc_arg_t *args;
    const rpc_proc_arg_t *result;
    SLIST_ENTRY( rpc_procedure_s ) list;
} rpc_procedure_t;

#define RPC_PROCEDURE_NULL_INIT { .id = 0xFFFFFFFFUL, .name = "", .arg_fmt = "", .res_fmt = "", .args = NULL, .result = NULL }

/**
 * @brief RPC call message type
 * 
 * When the message type is an RPC_MSG_CALL, then the
 * data payload is a rpc_call_t. It contains the program
 * to call a specific procedure on. The data_len and data
 * fields contains the arguments to the procedure.
 * 
 */
typedef struct rpc_call_s {
    uint32_t   program;
    uint32_t   procedure;
    uint16_t   data_len;
    uint8_t    data[];
} __attribute__((packed)) rpc_call_t;

/**
 * @brief RPC reply message type
 * 
 * When the message type is an RPC_MSG_REPLY, then the
 * data payload is a rpc_reply_t. The data_len and data
 * fields contains the reply from the procedure call.
 * 
 */
typedef struct rpc_reply_s {
    uint32_t   amount;
    uint32_t   idx;
    uint16_t   data_len;
    uint8_t    data[];
} __attribute__((packed)) rpc_reply_t;

/**
 * @brief RPC message type
 * 
 * This is the main RPC message type encapsulation structure.
 * The type filed contains either a RPC_MSG_CALL or a RPC_MSG_REPLY
 * and the union fields must be interpreted as such.
 * 
 * The xid field contains a eXecution ID, which is simply a serial
 * number of the particular procedure call request. This is used
 * to balance the call and reply messages.
 * 
 */
typedef struct rpc_msg_s {
    uint8_t     type;
    uint8_t     version;
    uint32_t    xid;
    union {
        rpc_call_t  call;
        rpc_reply_t reply;
    };
} __attribute__((packed)) rpc_msg_t;

struct rpc_server_s;
typedef struct rpc_server_s rpc_server_t;

/**
 * @brief RPC Server call back method prototype
 * 
 * A method of this type can be used as a call back from the RPC server
 * to handle specific procedure calls on the server side.
 * 
 * @param me A pointer to a RPC server object instance
 * @param program The program to execute a specific procedure on
 * @param procedure The procedure to execute on the specified program
 * @param call Pointer to the rpc_msg_t packet containing the call request being processed
 * @param reply Pointer to a rpc_msg_t packet which will receive the procedure return objects
 * @param ctx Call back context pointer, which will be passed along
 * @return true: call me again, false: done
 */
typedef bool rpc_server_callback_t(rpc_server_t *me, uint32_t program, uint32_t procedure, rpc_msg_t *call, rpc_msg_t *reply, void *ctx);

SLIST_HEAD( rpc_program_list_s, rpc_program_s );
typedef struct rpc_program_list_s rpc_program_list_t;

typedef struct rpc_program_s {
    uint32_t program_id;
    char *name;
    bool remote;
    union {
        struct { /* LOCAL */
            rpc_server_callback_t *handler;
            const rpc_procedure_t *procedures;
        };
        struct { /* REMOTE */
            uint16_t node;
            rpc_procedure_list_t remote_proc;
            SLIST_ENTRY( rpc_program_s ) list;
        };
    };
    void *data;
} rpc_program_t;

#define RPC_STRINGIFY(_x) #_x
#define RPC_DECLARE_PROGRAM(_nAME, _pROGRAMiD, _hANDLER, _pROCEDURES, _dATA) \
    static const \
        __attribute__((__aligned__(__alignof__(rpc_program_t)))) \
        rpc_program_t \
        __attribute__((__used__)) \
        __attribute__((section("rpc_programs"))) \
        __rpc_program_##_nAME##_instance = { \
            .name = RPC_STRINGIFY(_nAME), \
            .program_id = (_pROGRAMiD), \
            .remote = false, \
            .handler = _hANDLER, \
            .procedures = _pROCEDURES, \
            .data = _dATA, \
        };

typedef struct rpc_module_s {
    uint32_t nof_programs;
    const rpc_program_t *programs;
    rpc_program_list_t remote;
} rpc_module_t;

typedef struct client_s {
    csp_conn_t *conn;
    rpc_module_t module;
    /* Free lists */
    rpc_program_list_t program_slot;
    rpc_procedure_list_t procedure_slot;
} rpc_client_t;

typedef struct rpc_server_s {
    csp_conn_t *conn;
    csp_socket_t sock;
    rpc_module_t module;
    uint8_t spad_buffer[2000];
} rpc_server_t;

/* Client side methods */
extern rpc_client_t *global_rpc_client;
extern int rpc_init_client(rpc_client_t *me, uint32_t nof_programs, rpc_program_t *programs, uint32_t nof_procedures, rpc_procedure_t *procedures);
extern void rpc_remove_remote_programs(rpc_client_t *me, uint16_t node);
extern rpc_program_t *rpc_register_remote_program(rpc_client_t *me, uint16_t node, uint32_t program_id);
extern rpc_procedure_t *rpc_register_remote_procedure(rpc_client_t *me, rpc_program_t *program, uint32_t procedure_id);
extern void rpc_list_remote_programs(rpc_client_t *me, uint16_t node);
extern int rpc_connect(rpc_client_t *me, uint16_t node);
extern int rpc_disconnect(rpc_client_t *me);
extern int rpc_call_invoke(rpc_client_t *me, uint32_t program, uint32_t procedure, ...);

/* Main loop server side methods */
extern rpc_server_t *global_rpc_server;
extern int rpc_start_server(rpc_server_t *me);
extern int rpc_stop_server(rpc_server_t *me);
extern bool rpc_waitfor_connections(rpc_server_t *me);
extern bool rpc_handle_connection(rpc_server_t *me);

/* Server side call handler methods */
extern void rpc_set_reply_header(rpc_reply_t *reply, uint32_t amount, uint32_t idx);
extern int rpc_call_deserialize(rpc_server_t *me, rpc_msg_t *msg, ...);
extern void rpc_result_push_uint8(rpc_server_t *me, uint8_t value, rpc_msg_t *msg);
extern void rpc_result_push_int8(rpc_server_t *me, int8_t value, rpc_msg_t *msg);
extern void rpc_result_push_uint16(rpc_server_t *me, uint16_t value, rpc_msg_t *msg);
extern void rpc_result_push_int16(rpc_server_t *me, int16_t value, rpc_msg_t *msg);
extern void rpc_result_push_uint32(rpc_server_t *me, uint32_t value, rpc_msg_t *msg);
extern void rpc_result_push_int32(rpc_server_t *me, int32_t value, rpc_msg_t *msg);
extern void rpc_result_push_uint64(rpc_server_t *me, uint64_t value, rpc_msg_t *msg);
extern void rpc_result_push_int64(rpc_server_t *me, int64_t value, rpc_msg_t *msg);
extern void rpc_result_push_float(rpc_server_t *me, float value, rpc_msg_t *msg);
extern void rpc_result_push_double(rpc_server_t *me, double value, rpc_msg_t *msg);
extern void rpc_result_push_string(rpc_server_t *me, const char *value, rpc_msg_t *msg);
extern void rpc_result_push_buffer(rpc_server_t *me, const uint8_t *value, uint16_t len, rpc_msg_t *msg);

/* RPC client program procedures */
extern int rpc_fetch_first(uint16_t node, rpc_fetch_result_t *result);
extern int rpc_fetch_next(uint16_t node, rpc_fetch_result_t *result);
extern int rpc_fetch_all(uint16_t node, rpc_fetch_result_t *result, void (*result_cb)(uint32_t, void *, va_list), void *ctx);

#ifdef __cplusplus
}
#endif
