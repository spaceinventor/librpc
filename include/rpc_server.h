#pragma once

#include "rpc.h"

typedef struct rpc_proc_arg_s {
    const char * const name;
    const uint8_t type;
    const char * const descr;
} rpc_proc_arg_t;

/**
 * @brief RPC procedure object type
 * 
 * This type specifies a RPC function which is bound
 * into the RPC system.
 * 
 */
SLIST_HEAD( rpc_procedure_list_s, rpc_procedure_s );
typedef struct rpc_procedure_list_s rpc_procedure_list_t;

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
    rpc_server_callback_t *handler;
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
            .handler = _hANDLER, \
        };

typedef struct rpc_module_s {
    uint32_t nof_programs;
    const rpc_program_t *programs;
} rpc_module_t;

typedef struct rpc_server_s {
    csp_socket_t sock;
    rpc_module_t module;
} rpc_server_t;

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
