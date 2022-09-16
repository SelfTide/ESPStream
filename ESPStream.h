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

typedef struct {
	bool start_image, data_image, end_image;
	int image_size, data_size;
}video_stream_packet_state;

typedef struct {
    int last_pos;
    void *context;
} custom_stbi_mem_context;

int build_fd_sets(int socket, fd_set *read_fds, fd_set *write_fds);
int connect_serv (server_con *sc);
server_con espstream_init(char *ip_address, uint8_t port);
uint32_t *espstream_get_image(server_con *sc, int *imgWidth, int *imgHeight, int *size);
void espstream_cleanup();

#endif