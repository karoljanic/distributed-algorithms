#include "common.h"
#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>


int main(int argc, char **argv) {
    int port = RPC_PORT_DEFAULT;
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&srv, sizeof(srv)) < 0) {
        perror("bind"); return 1;
    }

    printf("RPC server listening on UDP port %d\n", port);

    uint8_t buf[RPC_MAX_MSG_LEN];
    load_authorized_tokens("authorized_tokens.txt");

    while (1) {
        struct sockaddr_in client;
        socklen_t cli_length = sizeof(client);
        ssize_t r = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&client, &cli_length);
        if (r <= 0) {
            continue;
        }

        if ((size_t)r < sizeof(struct rpc_request_header)) {
            continue;
        }

        struct rpc_request_header rh;
        memcpy(&rh, buf, sizeof(rh));
        uint32_t opcode = ntohl(rh.opcode);
        uint64_t request_id = ntohll(rh.request_id);
        uint64_t auth = ntohll(rh.authentication_token);
        uint32_t payload_len = ntohl(rh.payload_length);

        if (authorize_request(auth, opcode) != 0) {
            struct response er = { .payload = NULL, .payload_len = 0, .status = -1, .error_code = EACCES };
            send_response(sock, &client, cli_length, request_id, opcode, &er);
            continue;
        }

        const uint8_t *payload = buf + sizeof(rh);
        struct response rpl = dispatch_request(opcode, payload, payload_len);

        send_response(sock, &client, cli_length, request_id, opcode, &rpl);
        if (rpl.payload) {
            free(rpl.payload);
        }
    }

    close(sock);
    return 0;
}
