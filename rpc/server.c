#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>

#include "server.h"


struct auth_entry { uint64_t token; uint32_t perms; };
static struct auth_entry *auth_table = NULL;
static size_t auth_count = 0;

void load_authorized_tokens(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "[rpc server] error: auth file '%s' not found — tokens are required, refusing to start\n", path);
        exit(1);
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }

        if (!*p || *p == '#') {
            continue;
        }

        char *tok_s = strtok(p, " \t\n");
        char *perm_s = strtok(NULL, " \t\n");
        if (!tok_s) {
            continue;
        }

        uint64_t tok = strtoull(tok_s, NULL, 0);
        uint32_t perms = RFS_ALL_PERMISSIONS;
        if (perm_s) {
            perms = (uint32_t)strtoul(perm_s, NULL, 0);
        }
        
        struct auth_entry *newt = realloc(auth_table, (auth_count + 1) * sizeof(*newt));
        if (!newt) {
            break;
        }

        auth_table = newt;
        auth_table[auth_count].token = tok;
        auth_table[auth_count].perms = perms;
        auth_count++;
    }

    fclose(f);
    if (auth_count == 0) {
        fprintf(stderr, "[rpc server] error: auth file '%s' contains no tokens — tokens are required, refusing to start\n", path);
        exit(1);
    }

    fprintf(stderr, "[rpc server] loaded %zu authorized token(s) from %s\n", auth_count, path);
}

uint32_t find_perms_for(uint64_t token) {
    for (size_t i = 0; i < auth_count; ++i) {
        if (auth_table[i].token == token) return auth_table[i].perms;
    }
    return 0;
}

uint32_t required_perm_for_opcode(uint32_t opcode) {
    switch (opcode) {
        case RFS_OPEN: return RFS_OPEN_PERMISSION;
        case RFS_READ: return RFS_READ_PERMISSION;
        case RFS_WRITE: return RFS_WRITE_PERMISSION;
        case RFS_LSEEK: return RFS_LSEEK_PERMISSION;
        case RFS_CHMOD: return RFS_CHMOD_PERMISSION;
        case RFS_UNLINK: return RFS_UNLINK_PERMISSION;
        case RFS_RENAME: return RFS_RENAME_PERMISSION;
        case RFS_CLOSE: return RFS_CLOSE_PERMISSION;
        default: return 0;
    }
}

int authorize_request(uint64_t token, uint32_t opcode) {
    if (token == 0) return -1;
    uint32_t needed = required_perm_for_opcode(opcode);
    uint32_t perms = find_perms_for(token);
    if (perms == 0) return -1;
    if (needed && !(perms & needed)) return -1;
    return 0;
}

static struct response resp_error(int errnum) {
    struct response r = { .payload = NULL, .payload_len = 0, .status = -1, .error_code = errnum };
    return r;
}

static struct response resp_ok_empty(void) {
    struct response r = { .payload = NULL, .payload_len = 0, .status = 0, .error_code = 0 };
    return r;
}

static struct response handle_open(const uint8_t *payload, uint32_t payload_len) {
    const char *pathname = (const char*)payload;
    size_t remain = payload_len;
    size_t plen = strnlen(pathname, remain);
    if (plen == remain) {
        return resp_error(EINVAL);
    }

    const char *mode = (const char*)(payload + plen + 1);
    int flags = O_RDONLY;
    if (strchr(mode, 'w')) {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    }
    else if (strchr(mode, 'a')) {
        flags = O_WRONLY | O_CREAT | O_APPEND;
    }

    int fd = open(pathname, flags, 0644);
    if (fd < 0) {
        return resp_error(errno);
    }

    int32_t sfd_net = htonl(fd);
    int32_t *out = malloc(sizeof(int32_t));
    memcpy(out, &sfd_net, sizeof(sfd_net));
    struct response r = { .payload = out, .payload_len = sizeof(int32_t), .status = 0, .error_code = 0 };
    return r;
}

static struct response handle_read(const uint8_t *payload, uint32_t payload_len) {
    if (payload_len < sizeof(int32_t) + sizeof(uint32_t)) {
        return resp_error(EINVAL);
    }

    int32_t sfd_net; uint32_t cnt_net;
    memcpy(&sfd_net, payload, sizeof(sfd_net));
    memcpy(&cnt_net, payload + sizeof(sfd_net), sizeof(cnt_net));

    int sfd = ntohl(sfd_net);
    uint32_t cnt = ntohl(cnt_net);
    uint8_t *data = malloc(cnt + sizeof(int32_t));
    ssize_t br = read(sfd, data + sizeof(int32_t), cnt);
    if (br < 0) { 
        free(data); 
        return resp_error(errno); 
    }

    int32_t br_net = htonl((int32_t)br);
    memcpy(data, &br_net, sizeof(br_net));
    struct response r = { .payload = data, .payload_len = sizeof(br_net) + (uint32_t)br, .status = 0, .error_code = 0 };
    return r;
}

static struct response handle_write(const uint8_t *payload, uint32_t payload_len) {
    if (payload_len < sizeof(int32_t)) {
        return resp_error(EINVAL);
    }

    int32_t sfd_net; 
    memcpy(&sfd_net, payload, sizeof(sfd_net));
    int sfd = ntohl(sfd_net);
    const uint8_t *data = payload + sizeof(sfd_net);
    size_t datalen = payload_len - sizeof(sfd_net);
    ssize_t bw = write(sfd, data, datalen);

    if (bw < 0) {
        return resp_error(errno);
    }
    int32_t bw_net = htonl((int32_t)bw);
    int32_t *out = malloc(sizeof(bw_net)); memcpy(out, &bw_net, sizeof(bw_net));
    struct response r = { .payload = out, .payload_len = sizeof(bw_net), .status = 0, .error_code = 0 };
    return r;
}

static struct response handle_lseek(const uint8_t *payload, uint32_t payload_len) {
    if (payload_len < sizeof(int32_t) + sizeof(int64_t) + sizeof(int32_t)) {
        return resp_error(EINVAL);
    }

    int32_t sfd_net; int64_t off_net; int32_t wh_net;
    memcpy(&sfd_net, payload, sizeof(sfd_net));
    memcpy(&off_net, payload + sizeof(sfd_net), sizeof(off_net));
    memcpy(&wh_net, payload + sizeof(sfd_net) + sizeof(off_net), sizeof(wh_net));

    int sfd = ntohl(sfd_net);
    off_t offset = (off_t)ntohll((uint64_t)off_net);
    int whence = ntohl(wh_net);
    off_t no = lseek(sfd, offset, whence);
    if (no == (off_t)-1) {
        return resp_error(errno);
    }

    int64_t no_net = htonll((uint64_t)no);
    int64_t *out = malloc(sizeof(no_net)); memcpy(out, &no_net, sizeof(no_net));
    struct response r = { .payload = out, .payload_len = sizeof(no_net), .status = 0, .error_code = 0 };
    return r;
}

static struct response handle_chmod(const uint8_t *payload, uint32_t payload_len) {
    const char *pathname = (const char*)payload;
    size_t plen = strnlen(pathname, payload_len);
    if (plen == payload_len) {
        return resp_error(EINVAL);
    }

    uint32_t mode_net; memcpy(&mode_net, payload + plen + 1, sizeof(mode_net));
    mode_t mode = (mode_t)ntohl(mode_net);
    if (chmod(pathname, mode) < 0) {
        return resp_error(errno);
    }
    return resp_ok_empty();
}

static struct response handle_unlink(const uint8_t *payload, uint32_t payload_len) {
    const char *pathname = (const char*)payload;
    (void)payload_len;
    if (unlink(pathname) < 0) {
        return resp_error(errno);
    }
    return resp_ok_empty();
}

static struct response handle_rename(const uint8_t *payload, uint32_t payload_len) {
    const char *oldp = (const char*)payload;
    size_t a = strnlen(oldp, payload_len);
    if (a == payload_len) {
        return resp_error(EINVAL);
    }
    const char *newp = oldp + a + 1;
    if (rename(oldp, newp) < 0) {
        return resp_error(errno);
    }
    return resp_ok_empty();
}

static struct response handle_close(const uint8_t *payload, uint32_t payload_len) {
    if (payload_len < sizeof(int32_t)) {
        return resp_error(EINVAL);
    }
    int32_t sfd_net; 
    memcpy(&sfd_net, payload, sizeof(sfd_net)); 
    int sfd = ntohl(sfd_net);
    close(sfd);
    return resp_ok_empty();
}

struct response dispatch_request(uint32_t opcode, const uint8_t *payload, uint32_t payload_len) {
    switch (opcode) {
        case RFS_OPEN:  return handle_open(payload, payload_len);
        case RFS_READ:  return handle_read(payload, payload_len);
        case RFS_WRITE: return handle_write(payload, payload_len);
        case RFS_LSEEK: return handle_lseek(payload, payload_len);
        case RFS_CHMOD: return handle_chmod(payload, payload_len);
        case RFS_UNLINK: return handle_unlink(payload, payload_len);
        case RFS_RENAME: return handle_rename(payload, payload_len);
        case RFS_CLOSE: return handle_close(payload, payload_len);
        default: return resp_error(EINVAL);
    }
}

void send_response(int sock, struct sockaddr_in *client, socklen_t client_len,
                   uint64_t request_id, uint32_t opcode, const struct response *r) {
    struct rpc_response_header response_header;
    response_header.opcode = htonl(opcode);
    response_header.request_id = htonll(request_id);
    response_header.status = htonl(r->status);
    response_header.error_code = htonl(r->error_code);
    response_header.payload_length = htonl(r->payload_len);

    size_t bufsz = sizeof(response_header) + r->payload_len;
    uint8_t *buf = malloc(bufsz);
    memcpy(buf, &response_header, sizeof(response_header));
    if (r->payload_len && r->payload) {
        memcpy(buf + sizeof(response_header), r->payload, r->payload_len);
    }

    sendto(sock, buf, bufsz, 0, (struct sockaddr*)client, client_len);
    free(buf);
}
