#ifndef NET_COM
#define NET_COM

#include <stdio.h>
#include <stdbool.h>	// https://www.geeksforgeeks.org/bool-in-c/
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/time.h>	// FD_SET, FD_ISSET, FD_ZERO macros

extern fd_set read_fds, write_fds;
extern int maxfds;

typedef struct {
	int s, port;
	char ip[16];
	struct timeval tv;
	struct sockaddr_in server;
	bool connected;
}server_con;

int build_fd_sets(int socket, fd_set *read_fds, fd_set *write_fds);
int connect_serv (server_con *sc);

#endif