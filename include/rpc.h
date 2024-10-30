#pragma once

#include <inttypes.h>

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

typedef struct rpc_proc_arg_s {
    const char * const name;
    const uint8_t type;
} rpc_proc_arg_t;

#define RPC_PROC_ARG_NULL_INIT { .name = NULL, .type = 0 }

/**
 * @brief RPC procedure object type
 * 
 * This type specifies a RPC function which is bound
 * into the RPC system.
 * 
 */
typedef struct rpc_procedure_s {
    uint32_t id;
    const char *name;
    const char *arg_fmt;
    const char *res_fmt;
    const rpc_proc_arg_t *args;
    const rpc_proc_arg_t *result;
} rpc_procedure_t;

#define RPC_PROCEDURE_NULL_INIT {.id = 0xFFFFFFFFUL, .name = NULL, .arg_fmt = NULL, .res_fmt = NULL }

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
 */
typedef void rpc_server_callback_t(rpc_server_t *me, uint32_t program, uint32_t procedure, rpc_msg_t *call, rpc_msg_t *reply, void *ctx);

/**
 * @brief RPC procedure lookup method prototype
 * 
 * @param id 
 * @return rpc_procedure_t *
 */
typedef const rpc_procedure_t * rpc_lookup_procedure_t(uint32_t id);

typedef struct rpc_api_s {
    rpc_lookup_procedure_t *lookup;
} rpc_api_t;

typedef struct rpc_program_s {
    const uint32_t program_id;
    const char *name;
    rpc_server_callback_t *handler;
    const rpc_api_t *api;
    const rpc_procedure_t *procedures;
    void *data;
} rpc_program_t;

#define RPC_STRINGIFY(_x) #_x
#define RPC_DECLARE_PROGRAM(_nAME, _pROGRAMiD, _hANDLER, _aPI, _pROCEDURES, _dATA) \
    __attribute__((used,aligned(8),section("rpc_programs"))) \
    static const rpc_program_t __rpc_program_##_nAME##_instance = { \
        .name = RPC_STRINGIFY(_nAME), \
        .program_id = (_pROGRAMiD), \
        .handler = _hANDLER, \
        .api = _aPI, \
        .procedures = _pROCEDURES, \
        .data = _dATA, \
    };

typedef struct rpc_module_s {
    uint32_t nof_programs;
    const rpc_program_t *programs;
} rpc_module_t;

typedef struct client_s {
    csp_conn_t *conn;
    rpc_module_t module;
} rpc_client_t;

typedef struct rpc_server_s {
    csp_conn_t *conn;
    csp_socket_t sock;
    rpc_module_t module;
} rpc_server_t;

/* Client side methods */
extern rpc_client_t *global_rpc_client;
extern int rpc_init_client(rpc_client_t *me);
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

/* RPC client program procedures */
extern int rpc_fetch_first(uint16_t node, rpc_fetch_result_t *result);
extern int rpc_fetch_next(uint16_t node, rpc_fetch_result_t *result);

#ifdef __cplusplus
}
#endif
