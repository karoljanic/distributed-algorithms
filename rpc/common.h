#ifndef RPC_COMMON_H
#define RPC_COMMON_H

#include <stdint.h>
#include <sys/types.h>
#include <arpa/inet.h>

#define RPC_PORT_DEFAULT 9000
#define RPC_MAX_MSG_LEN 1024

#define RFS_OPEN  1
#define RFS_READ  2
#define RFS_WRITE 3
#define RFS_LSEEK 4
#define RFS_CHMOD 5
#define RFS_UNLINK 6
#define RFS_RENAME 7
#define RFS_CLOSE 8

#define RFS_OPEN_PERMISSION   (1u << 0)
#define RFS_READ_PERMISSION   (1u << 1)
#define RFS_WRITE_PERMISSION  (1u << 2)
#define RFS_LSEEK_PERMISSION  (1u << 3)
#define RFS_CHMOD_PERMISSION  (1u << 4)
#define RFS_UNLINK_PERMISSION (1u << 5)
#define RFS_RENAME_PERMISSION (1u << 6)
#define RFS_CLOSE_PERMISSION  (1u << 7)
#define RFS_ALL_PERMISSIONS   0xffu


struct rpc_request_header {
    uint32_t opcode;
    uint64_t request_id;
    uint64_t authentication_token;
    uint32_t payload_length;
};

struct rpc_response_header {
    uint32_t opcode;
    uint64_t request_id;
    int32_t status;
    int32_t error_code;
    uint32_t payload_length;
};

static inline uint64_t htonll(uint64_t v) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (((uint64_t)htonl((uint32_t)(v & 0xffffffffULL))) << 32) | htonl((uint32_t)(v >> 32));
#else
    return v;
#endif
}

static inline uint64_t ntohll(uint64_t v) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return (((uint64_t)ntohl((uint32_t)(v & 0xffffffffULL))) << 32) | ntohl((uint32_t)(v >> 32));
#else
    return v;
#endif
}

#endif // RPC_COMMON_H
