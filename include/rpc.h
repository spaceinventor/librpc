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

#define RPC_STATUS_OK 0
#define RPC_STATUS_ERR_COULD_NOT_CONNECT -1
#define RPC_STATUS_ERR_TIMEOUT -2
#define RPC_STATUS_ERR_NO_MEMORY -3
#define RPC_STATUS_ERR_INVALID -4
#define RPC_STATUS_ERR_NOT_FOUND -5
#define RPC_STATUS_ERR_NOT_SUPPORTED -6
#define RPC_STATUS_ERR_INUSE -7

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
 * @param conn The connection on which the call was received
 * @param call Pointer to the rpc_msg_t packet containing the call request being processed
 * @param reply Pointer to a rpc_msg_t packet which will receive the procedure return objects
 * @param ctx Call back context pointer, which will be passed along
 * @return true: call me again, false: done
 */
typedef int rpc_server_callback_t(uint32_t procedure, csp_conn_t *conn, rpc_msg_t *call);

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

/**
 * @brief RPC Program declaration macro
 * 
 * Use this macro to declare/instantiate a RPC program statically with the
 * RPC framework. The user must supply a name which will be used as a textual
 * representation of the program to the outside world alongside a unique id.
 * 
 * All instances of the rpc_program_t type will be placed in a special section
 * in memory, which will be placed by the linker, and handled by the RPC module
 * as a whole.
 * 
 * @param _nAME The name of the "program" to declare (not a string)
 * @param _pROGAMiD A globally unique ID which will represent the program
 * @param _hANDLER Pointer to an RPC server side handler (rpc_server_callback_t)
 * @param _pROCEDURES Pointer to a list of rpc_procedure_t objects
 * @param _dATA Pointer to some contextual data which will be passed to the handler
 * 
 */
#define RPC_DECLARE_PROGRAM(_nAME, _pROGRAMiD, _hANDLER) \
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
        };

typedef struct rpc_module_s {
    uint32_t nof_programs;
    const rpc_program_t *programs;
} rpc_module_t;

typedef struct client_s {
    uint32_t timeout;
} rpc_client_t;

typedef struct rpc_server_s {
    csp_socket_t sock;
    rpc_module_t module;
} rpc_server_t;

/* Client side methods */
extern rpc_client_t *global_rpc_client;
extern int rpc_client_init(rpc_client_t *me);

extern csp_conn_t *rpc_connect(uint16_t node);
extern int rpc_build_request(uint16_t node, uint32_t program, uint32_t procedure, csp_conn_t ** conn, rpc_msg_t **req_msg);
extern void rpc_send(csp_conn_t *conn, rpc_msg_t *msg);
extern void rpc_buffer_free(rpc_msg_t *msg);
extern int rpc_disconnect(csp_conn_t *conn);
extern int rpc_get_reply(csp_conn_t *conn, rpc_msg_t **msg, uint32_t maxresponses, uint32_t extected_idx, uint32_t timeout);

#define RPC_HANDLE_CLIENT_HDR(type, name) \
    void rpc_request_push_##name(type value, rpc_msg_t *msg); \
    type rpc_result_pop_##name(rpc_msg_t *msg);

RPC_HANDLE_CLIENT_HDR(uint8_t, uint8)
RPC_HANDLE_CLIENT_HDR(int8_t, int8)
RPC_HANDLE_CLIENT_HDR(uint16_t, uint16)
RPC_HANDLE_CLIENT_HDR(int16_t, int16)
RPC_HANDLE_CLIENT_HDR(uint32_t, uint32)
RPC_HANDLE_CLIENT_HDR(int32_t, int32)
RPC_HANDLE_CLIENT_HDR(uint64_t, uint64)
RPC_HANDLE_CLIENT_HDR(int64_t, int64)
RPC_HANDLE_CLIENT_HDR(float, float)
RPC_HANDLE_CLIENT_HDR(double, double)
#undef RPC_HANDLE_CLIENT_HDR

extern void rpc_request_push_string(const char *value, rpc_msg_t *msg);
extern void rpc_request_push_buffer(const uint8_t *value, uint16_t len, rpc_msg_t *msg);
extern void rpc_result_pop_string(char *value, rpc_msg_t *msg);
extern void rpc_result_pop_buffer(uint8_t *value, uint16_t *len, rpc_msg_t *msg);

/* Main loop server side methods */
extern rpc_server_t *global_rpc_server;
extern void rpc_server_main(rpc_server_t *me);

/* Server side call handler methods */
extern rpc_msg_t *rpc_result_prepare(rpc_msg_t *call, uint32_t amount, uint32_t idx);
extern void rpc_send_reply(csp_conn_t *conn, rpc_msg_t *reply);

#define RPC_HANDLE_SERVER_HDR(type, name) \
    void rpc_result_push_##name(type value, rpc_msg_t *msg); \
    type rpc_request_pop_##name(rpc_msg_t *msg);

RPC_HANDLE_SERVER_HDR(uint8_t, uint8)
RPC_HANDLE_SERVER_HDR(int8_t, int8)
RPC_HANDLE_SERVER_HDR(uint16_t, uint16)
RPC_HANDLE_SERVER_HDR(int16_t, int16)
RPC_HANDLE_SERVER_HDR(uint32_t, uint32)
RPC_HANDLE_SERVER_HDR(int32_t, int32)
RPC_HANDLE_SERVER_HDR(uint64_t, uint64)
RPC_HANDLE_SERVER_HDR(int64_t, int64)
RPC_HANDLE_SERVER_HDR(float, float)
RPC_HANDLE_SERVER_HDR(double, double)
#undef RPC_HANDLE_SERVER_HDR

extern void rpc_request_pop_string(char *value, rpc_msg_t *msg);
extern void rpc_request_pop_buffer(uint8_t *value, uint16_t *len, rpc_msg_t *msg);
extern void rpc_result_push_string(const char *value, rpc_msg_t *msg);
extern void rpc_result_push_buffer(const uint8_t *value, uint16_t len, rpc_msg_t *msg);

#ifdef __cplusplus
}
#endif
