#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"


int main(int argc, char **argv) {
    if (argc > 1) {
        setenv("RPC_AUTH_TOKEN", argv[1], 1);
    }

    File *f = rfs_open("rpc_test.txt", "w");
    if (!f) { 
        perror("rfs_open"); 
        return 1; 
    }

    const char *msg = "Hello RPC!\n";
    if (rfs_write(f, msg, strlen(msg)) < 0) { 
        perror("rfs_write"); 
        rfs_close(f); 
        return 1; 
    }
    
    rfs_close(f);

    
    File *g = rfs_open("rpc_test.txt", "r");
    if (!g) { 
        perror("rfs_open read"); 
        return 1; 
    }
    
    char buf[128];
    ssize_t n = rfs_read(g, buf, sizeof(buf) - 1);
    if (n < 0) { 
        perror("rfs_read"); 
        rfs_close(g); 
        return 1; 
    }
    
    buf[n] = '\0';
    printf("Read (%zd bytes):\n%s", n, buf);
    rfs_close(g);

    return 0;
}
