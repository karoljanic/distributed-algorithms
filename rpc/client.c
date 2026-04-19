#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/random.h>
#include <fcntl.h>
#include <time.h>

#include "client.h"


struct RFile {
    int server_fd;
};

static int client_sock = -1;
static struct sockaddr_in server_addr;
static uint64_t client_auth = 0;


static void ensure_client_initialized(void) {
    if (client_sock != -1) {
        return;
    }

    const char *env = getenv("RPC_SERVER");
    const char *host = "127.0.0.1";
    int port = RPC_PORT_DEFAULT;
    if (env) {
        char *tmp = strdup(env);
        char *c = strchr(tmp, ':');
        if (c) {
            *c = '\0';
            host = tmp;
            port = atoi(c+1);
        } 
        else {
            host = tmp;
        }
    }

    client_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_sock < 0) {
        perror("socket");
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &server_addr.sin_addr) != 1) {
        struct hostent *h = gethostbyname(host);
        if (!h) {
            fprintf(stderr, "Could not resolve RPC_SERVER host %s\n", host);
            exit(1);
        }
        server_addr.sin_addr = *(struct in_addr*)h->h_addr_list[0];
    }

    const char *envtok = getenv("RPC_AUTH_TOKEN");
    if (envtok && *envtok) {
        uint64_t v = strtoull(envtok, NULL, 0);
        client_auth = v ? v : 1;
    } 
    else {
        uint64_t t = 0;
        if (getrandom(&t, sizeof(t), 0) != sizeof(t)) {
            t = (uint64_t)time(NULL) ^ (uint64_t)getpid();
        }

        client_auth = t ? t : 1;
    }
}

static int send_request_wait(const void *req, size_t req_len, void *resp, size_t *resp_len) {
    struct timeval tv = {2, 0};

    for (int attempt = 0; attempt < 2; ++attempt) {
        ssize_t s = sendto(client_sock, req, req_len, 0,
                           (struct sockaddr*)&server_addr, sizeof(server_addr));
        if (s < 0) {
            return -1;
        }

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(client_sock, &fds);
        int rv = select(client_sock+1, &fds, NULL, NULL, &tv);
        if (rv == 0) {
            if (attempt == 0) {
                continue;
            }
            return -1;
        } 
        else if (rv < 0) {
            return -1;
        }

        ssize_t rr = recvfrom(client_sock, resp, *resp_len, 0, NULL, NULL);
        if (rr < 0) {
            return -1;
        }
        *resp_len = (size_t)rr;
        return 0;
    }
    return -1;
}

static uint64_t make_seq(void) {
    uint64_t s = 0;
    if (getrandom(&s, sizeof(s), 0) != sizeof(s)) {
        s = ((uint64_t)rand() << 32) ^ (uint64_t)time(NULL);
    }

    return s ? s : 1;
}

File *rfs_open(const char *pathname, const char *mode) {
    ensure_client_initialized();

    size_t pathlen = strlen(pathname) + 1;
    size_t modlen = strlen(mode) + 1;
    size_t payload = pathlen + modlen;
    size_t bufsz = sizeof(struct rpc_request_header) + payload;
    uint8_t *buf = malloc(bufsz);

    struct rpc_request_header hdr;
    hdr.opcode = htonl(RFS_OPEN);
    uint64_t seq = make_seq();
    hdr.request_id = htonll(seq);
    hdr.authentication_token = htonll(client_auth);
    hdr.payload_length = htonl((uint32_t)payload);
    
    memcpy(buf, &hdr, sizeof(hdr));
    memcpy(buf + sizeof(hdr), pathname, pathlen);
    memcpy(buf + sizeof(hdr) + pathlen, mode, modlen);

    uint8_t respbuf[RPC_MAX_MSG_LEN];
    size_t resp_len = sizeof(respbuf);
    int rv = send_request_wait(buf, bufsz, respbuf, &resp_len);
    free(buf);

    if (rv < 0) {
        return NULL;
    }

    if (resp_len < sizeof(struct rpc_response_header)) {
        return NULL;
    }

    struct rpc_response_header rh;
    memcpy(&rh, respbuf, sizeof(rh));
    rh.request_id = ntohll(rh.request_id);
    rh.status = ntohl(rh.status);
    rh.error_code = ntohl(rh.error_code);
    rh.payload_length = ntohl(rh.payload_length);

    if (rh.request_id != seq) {
        return NULL;
    }

    if (rh.status != 0) {
        errno = rh.error_code;
        return NULL;
    }
    
    if (rh.payload_length < sizeof(int32_t)) {
        return NULL;
    }
    
    int32_t srvfd;
    memcpy(&srvfd, respbuf + sizeof(rh), sizeof(int32_t));
    srvfd = ntohl(srvfd);
    File *f = malloc(sizeof(File));
    f->server_fd = srvfd;

    return f;
}

ssize_t rfs_read(File *f, void *buf_out, size_t count) {
    if (!f) { 
        errno = EBADF; return -1; 
    }
    
    ensure_client_initialized();

    size_t payload = sizeof(int32_t) + sizeof(uint32_t);
    size_t bufsz = sizeof(struct rpc_request_header) + payload;
    uint8_t *buf = malloc(bufsz);

    struct rpc_request_header hdr;
    hdr.opcode = htonl(RFS_READ);
    uint64_t seq = make_seq();
    hdr.request_id = htonll(seq);
    hdr.authentication_token = htonll(client_auth);
    hdr.payload_length = htonl((uint32_t)payload);

    memcpy(buf, &hdr, sizeof(hdr));
    int32_t sfd = htonl(f->server_fd);
    memcpy(buf + sizeof(hdr), &sfd, sizeof(sfd));
    uint32_t cnt = htonl((uint32_t)count);
    memcpy(buf + sizeof(hdr) + sizeof(sfd), &cnt, sizeof(cnt));

    uint8_t respbuf[RPC_MAX_MSG_LEN];
    size_t resp_len = sizeof(respbuf);
    int rv = send_request_wait(buf, bufsz, respbuf, &resp_len);
    free(buf);

    if (rv < 0) { 
        errno = ETIMEDOUT; 
        return -1; 
    }

    if (resp_len < sizeof(struct rpc_response_header)) {
        return -1;
    }

    struct rpc_response_header rh;
    memcpy(&rh, respbuf, sizeof(rh));
    rh.request_id = ntohll(rh.request_id);
    rh.status = ntohl(rh.status);
    rh.error_code = ntohl(rh.error_code);
    rh.payload_length = ntohl(rh.payload_length);
    
    if (rh.request_id != seq) {
        return -1;
    }
    
    if (rh.status != 0) { 
        errno = rh.error_code; 
        return -1; 
    }
    
    if (rh.payload_length < sizeof(int32_t)) {
        return -1;
    }

    int32_t bytes_read_net;
    memcpy(&bytes_read_net, respbuf + sizeof(rh), sizeof(int32_t));
    int32_t bytes_read = ntohl(bytes_read_net);
    if ((size_t)bytes_read > count) {
        bytes_read = count;
    }

    if (bytes_read > 0) {
        memcpy(buf_out, respbuf + sizeof(rh) + sizeof(int32_t), bytes_read);
    }

    return bytes_read;
}

ssize_t rfs_write(File *f, const void *buf_in, size_t count) {
    if (!f) { 
        errno = EBADF; 
        return -1; 
    }
    
    ensure_client_initialized();

    size_t payload = sizeof(int32_t) + count;
    size_t bufsz = sizeof(struct rpc_request_header) + payload;
    if (bufsz > RPC_MAX_MSG_LEN) { 
        errno = EINVAL; 
        return -1; 
    }

    uint8_t *buf = malloc(bufsz);
    struct rpc_request_header hdr;
    hdr.opcode = htonl(RFS_WRITE);
    uint64_t seq = make_seq();
    hdr.request_id = htonll(seq);
    hdr.authentication_token = htonll(client_auth);
    hdr.payload_length = htonl((uint32_t)payload);

    memcpy(buf, &hdr, sizeof(hdr));
    int32_t sfd = htonl(f->server_fd);
    memcpy(buf + sizeof(hdr), &sfd, sizeof(sfd));
    memcpy(buf + sizeof(hdr) + sizeof(sfd), buf_in, count);

    uint8_t respbuf[RPC_MAX_MSG_LEN];
    size_t resp_len = sizeof(respbuf);
    int rv = send_request_wait(buf, bufsz, respbuf, &resp_len);
    free(buf);
    
    if (rv < 0) { 
        errno = ETIMEDOUT; 
        return -1; 
    }

    if (resp_len < sizeof(struct rpc_response_header)) {
        return -1;
    }

    struct rpc_response_header rh;
    memcpy(&rh, respbuf, sizeof(rh));
    rh.request_id = ntohll(rh.request_id);
    rh.status = ntohl(rh.status);
    rh.error_code = ntohl(rh.error_code);
    rh.payload_length = ntohl(rh.payload_length);
    
    if (rh.request_id != seq) {
        return -1;
    }
    
    if (rh.status != 0) { 
        errno = rh.error_code; 
        return -1; 
    }
    
    if (rh.payload_length < sizeof(int32_t)) {
        return -1;
    }

    int32_t written_net;
    memcpy(&written_net, respbuf + sizeof(rh), sizeof(int32_t));
    int32_t written = ntohl(written_net);

    return written;
}

off_t rfs_lseek(File *f, off_t offset, int whence) {
    if (!f) { 
        errno = EBADF; 
        return -1; 
    }

    ensure_client_initialized();

    size_t payload = sizeof(int32_t) + sizeof(int64_t) + sizeof(int32_t);
    size_t bufsz = sizeof(struct rpc_request_header) + payload;
    uint8_t *buf = malloc(bufsz);

    struct rpc_request_header hdr;
    hdr.opcode = htonl(RFS_LSEEK);
    uint64_t seq = make_seq();
    hdr.request_id = htonll(seq);
    hdr.authentication_token = htonll(client_auth);
    hdr.payload_length = htonl((uint32_t)payload);

    memcpy(buf, &hdr, sizeof(hdr));
    int32_t sfd = htonl(f->server_fd);
    memcpy(buf + sizeof(hdr), &sfd, sizeof(sfd));
    int64_t offnet = htonll((uint64_t)offset);
    memcpy(buf + sizeof(hdr) + sizeof(sfd), &offnet, sizeof(offnet));
    int32_t wn = htonl(whence);
    memcpy(buf + sizeof(hdr) + sizeof(sfd) + sizeof(offnet), &wn, sizeof(wn));

    uint8_t respbuf[RPC_MAX_MSG_LEN];
    size_t resp_len = sizeof(respbuf);
    int rv = send_request_wait(buf, bufsz, respbuf, &resp_len);
    free(buf);

    if (rv < 0) { 
        errno = ETIMEDOUT; 
        return -1; 
    }

    if (resp_len < sizeof(struct rpc_response_header)) {
        return -1;
    }

    struct rpc_response_header rh;
    memcpy(&rh, respbuf, sizeof(rh));
    rh.request_id = ntohll(rh.request_id);
    rh.status = ntohl(rh.status);
    rh.error_code = ntohl(rh.error_code);
    rh.payload_length = ntohl(rh.payload_length);
    
    if (rh.request_id != seq) {
        return -1;
    }
    
    if (rh.status != 0) { 
        errno = rh.error_code; 
        return -1; 
    }
    
    if (rh.payload_length < sizeof(int64_t)) {
        return -1;
    }

    int64_t newoff_net;
    memcpy(&newoff_net, respbuf + sizeof(rh), sizeof(newoff_net));
    int64_t newoff = (int64_t)ntohll((uint64_t)newoff_net);

    return (off_t)newoff;
}

int rfs_chmod(const char *pathname, mode_t mode) {
    ensure_client_initialized();

    size_t pathlen = strlen(pathname)+1;
    size_t payload = pathlen + sizeof(uint32_t);
    size_t bufsz = sizeof(struct rpc_request_header) + payload;
    uint8_t *buf = malloc(bufsz);

    struct rpc_request_header hdr;
    hdr.opcode = htonl(RFS_CHMOD);
    uint64_t seq = make_seq();
    hdr.request_id = htonll(seq);
    hdr.authentication_token = htonll(client_auth);
    hdr.payload_length = htonl((uint32_t)payload);

    memcpy(buf, &hdr, sizeof(hdr));
    memcpy(buf + sizeof(hdr), pathname, pathlen);
    uint32_t m = htonl((uint32_t)mode);
    memcpy(buf + sizeof(hdr) + pathlen, &m, sizeof(m));

    uint8_t respbuf[RPC_MAX_MSG_LEN];
    size_t resp_len = sizeof(respbuf);
    int rv = send_request_wait(buf, bufsz, respbuf, &resp_len);
    free(buf);
    
    if (rv < 0) { 
        errno = ETIMEDOUT; 
        return -1; 
    }
    
    if (resp_len < sizeof(struct rpc_response_header)) {
        return -1;
    }

    struct rpc_response_header rh;
    memcpy(&rh, respbuf, sizeof(rh));
    rh.request_id = ntohll(rh.request_id);
    rh.status = ntohl(rh.status);
    rh.error_code = ntohl(rh.error_code);
    
    if (rh.request_id != seq) {
        return -1;
    }
    
    if (rh.status != 0) { 
        errno = rh.error_code; 
        return -1; 
    }
    
    return 0;
}

int rfs_unlink(const char *pathname) {
    ensure_client_initialized();

    size_t pathlen = strlen(pathname)+1;
    size_t payload = pathlen;
    size_t bufsz = sizeof(struct rpc_request_header) + payload;
    uint8_t *buf = malloc(bufsz);
    
    struct rpc_request_header hdr;
    hdr.opcode = htonl(RFS_UNLINK);
    uint64_t seq = make_seq();
    hdr.request_id = htonll(seq);
    hdr.authentication_token = htonll(client_auth);
    hdr.payload_length = htonl((uint32_t)payload);

    memcpy(buf, &hdr, sizeof(hdr));
    memcpy(buf + sizeof(hdr), pathname, pathlen);

    uint8_t respbuf[RPC_MAX_MSG_LEN];
    size_t resp_len = sizeof(respbuf);
    int rv = send_request_wait(buf, bufsz, respbuf, &resp_len);
    free(buf);
    
    if (rv < 0) { 
        errno = ETIMEDOUT; 
        return -1; 
    }
    
    if (resp_len < sizeof(struct rpc_response_header)) {
        return -1;
    }
    
    struct rpc_response_header rh;
    memcpy(&rh, respbuf, sizeof(rh));
    rh.request_id = ntohll(rh.request_id);
    rh.status = ntohl(rh.status);
    rh.error_code = ntohl(rh.error_code);
    
    if (rh.request_id != seq) {
        return -1;
    }
    
    if (rh.status != 0) { 
        errno = rh.error_code; 
        return -1; 
    }
    
    return 0;
}

int rfs_rename(const char *oldpath, const char *newpath) {
    ensure_client_initialized();

    size_t a = strlen(oldpath)+1;
    size_t b = strlen(newpath)+1;
    size_t payload = a + b;
    size_t bufsz = sizeof(struct rpc_request_header) + payload;
    uint8_t *buf = malloc(bufsz);

    struct rpc_request_header hdr;
    hdr.opcode = htonl(RFS_RENAME);
    uint64_t seq = make_seq();
    hdr.request_id = htonll(seq);
    hdr.authentication_token = htonll(client_auth);
    hdr.payload_length = htonl((uint32_t)payload);

    memcpy(buf, &hdr, sizeof(hdr));
    memcpy(buf + sizeof(hdr), oldpath, a);
    memcpy(buf + sizeof(hdr) + a, newpath, b);

    uint8_t respbuf[RPC_MAX_MSG_LEN];
    size_t resp_len = sizeof(respbuf);
    int rv = send_request_wait(buf, bufsz, respbuf, &resp_len);
    free(buf);
    
    if (rv < 0) { 
        errno = ETIMEDOUT; 
        return -1; 
    }
    
    if (resp_len < sizeof(struct rpc_response_header)) {
        return -1;
    }
    
    struct rpc_response_header rh;
    memcpy(&rh, respbuf, sizeof(rh));
    rh.request_id = ntohll(rh.request_id);
    rh.status = ntohl(rh.status);
    rh.error_code = ntohl(rh.error_code);
    
    if (rh.request_id != seq) {
        return -1;
    }
    
    if (rh.status != 0) { 
        errno = rh.error_code; 
        return -1; 
    }
    
    return 0;
}

int rfs_close(File *f) {
    if (!f) {
        return 0;
    }

    ensure_client_initialized();
    
    size_t payload = sizeof(int32_t);
    size_t bufsz = sizeof(struct rpc_request_header) + payload;
    uint8_t *buf = malloc(bufsz);

    struct rpc_request_header hdr;
    hdr.opcode = htonl(RFS_CLOSE);
    uint64_t seq = make_seq();
    hdr.request_id = htonll(seq);
    hdr.authentication_token = htonll(client_auth);
    hdr.payload_length = htonl((uint32_t)payload);

    memcpy(buf, &hdr, sizeof(hdr));
    int32_t sfd = htonl(f->server_fd);
    memcpy(buf + sizeof(hdr), &sfd, sizeof(sfd));

    uint8_t respbuf[RPC_MAX_MSG_LEN];
    size_t resp_len = sizeof(respbuf);
    int rv = send_request_wait(buf, bufsz, respbuf, &resp_len);
    free(buf);
    free(f);
    (void)rv;
    
    return 0;
}
