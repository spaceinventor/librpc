#pragma once

#include <inttypes.h>
#include <sys/queue.h>
#include <stdarg.h>

#include <csp/csp.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#define RPC_DBG(...) printf(__VA_ARGS__)
#define RPC_WRN(...) printf(__VA_ARGS__)
#else
#define RPC_DBG(...) do {} while(0)
#define RPC_WRN(...) do {} while(0)
#endif

#define RPC_ERR(...) printf(__VA_ARGS__)

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

#ifdef __cplusplus
}
#endif
