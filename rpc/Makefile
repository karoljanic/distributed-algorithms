CC=gcc
CFLAGS=-Wall -Wextra -O3


all: server client

server: server_main.c server.c common.h server.h
	$(CC) $(CFLAGS) -o rpc_server server_main.c server.c

client: client_main.c client.c common.h client.h
	$(CC) $(CFLAGS) -o rpc_client client_main.c client.c

clean:
	rm -f rpc_server rpc_client
