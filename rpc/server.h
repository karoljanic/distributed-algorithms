#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include <stdint.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "common.h"


struct response {
    void *payload;
    uint32_t payload_len;
    int status;
    int error_code;
};

void load_authorized_tokens(const char *path);

uint32_t find_perms_for(uint64_t token);
uint32_t required_perm_for_opcode(uint32_t opcode);
int authorize_request(uint64_t token, uint32_t opcode);

struct response dispatch_request(uint32_t opcode, const uint8_t *payload, uint32_t payload_len);

void send_response(int sock, struct sockaddr_in *client, socklen_t client_len,
                   uint64_t request_id, uint32_t opcode, const struct response *r);

#endif // RPC_SERVER_H
