#pragma once

#include <inttypes.h>

#include <csp/csp.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The RPC server port to use
 * 
 */
#define CSP_PORT_RPC_SERVER 9

/**
 * @brief RPC message type
 * 
 * The message type could either be a Call or a Reply.
 * 
 */
typedef enum rpc_msg_type_e {
    RPC_MSG_CALL = 0,
    RPC_MSG_REPLY = 1,
} rpc_msg_type_t;

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
 * @param data_len Length of the data field
 * @param data Procedure arguments packed as Big Endian according to out-of-band format
 * @param reply Pointer to a rpc_msg_t packet which will receive the procedure return objects
 */
typedef void rpc_server_callback_t(rpc_server_t *me, uint32_t program, uint32_t procedure, uint16_t data_len, uint8_t *data, rpc_msg_t *reply);

/**
 * @brief RPC procedure lookup method prototype
 * 
 * @param id 
 * @return rpc_procedure_t *
 */
typedef rpc_procedure_t * rpc_lookup_procedure_t(uint32_t id);

typedef struct rpc_api_s {
    rpc_lookup_procedure_t *lookup;
} rpc_api_t;

typedef struct client_s {
    csp_conn_t *conn;
    const rpc_api_t *api;
} rpc_client_t;

typedef struct rpc_server_s {
    csp_socket_t sock;
    const rpc_api_t *api;
} rpc_server_t;

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
} rpc_data_type_t;

extern int rpc_connect(rpc_client_t *me, uint16_t node);
extern int rpc_disconnect(rpc_client_t *me);
extern int rpc_call_invoke(rpc_client_t *me, uint32_t program, uint32_t procedure, void *ret, ...);

extern int rpc_start_server(rpc_server_t *me);
extern int rpc_stop_server(rpc_server_t *me);
extern csp_conn_t * rpc_waitfor_connections(rpc_server_t *me);

extern csp_packet_t * rpc_handle_msg(rpc_server_t *me, csp_packet_t *packet, rpc_server_callback_t *cb);
extern csp_packet_t * rpc_result_prepare(rpc_server_t *me, rpc_msg_t *msg);

extern void rpc_result_push_uint16(rpc_server_t *me, uint16_t value, rpc_msg_t *msg);
extern void rpc_result_push_int16(rpc_server_t *me, int16_t value, rpc_msg_t *msg);
extern void rpc_result_push_uint32(rpc_server_t *me, uint32_t value, rpc_msg_t *msg);
extern void rpc_result_push_int32(rpc_server_t *me, int32_t value, rpc_msg_t *msg);
extern void rpc_result_push_float(rpc_server_t *me, float value, rpc_msg_t *msg);
extern void rpc_result_push_double(rpc_server_t *me, double value, rpc_msg_t *msg);

#ifdef __cplusplus
}
#endif
