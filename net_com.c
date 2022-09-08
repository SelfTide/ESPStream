#include "net_com.h"

int build_fd_sets(int socket, fd_set *read_fds, fd_set *write_fds)
{
	
	if(read_fds)
	{
		FD_ZERO(read_fds);
		FD_SET(socket, read_fds);
	}
	
	if(write_fds)
		FD_ZERO(write_fds);
	
	/* there is smoth to send, set up write_fd for server socket
	if (server->send_buffer.current > 0)
		FD_SET(server->socket, write_fds);
	*/
   
  return 0;
}

int connect_serv (server_con *sc)
{
	
	sc->tv.tv_sec = 0;
	sc->tv.tv_usec = 10;
	//Create a socket
	sc->server.sin_addr.s_addr = inet_addr(sc->ip);
	sc->server.sin_family = AF_INET;
	//inet_pton(AF_INET, "192.168.0.22", &server.sin_addr);
	sc->server.sin_port = htons(sc->port);

	sc->s = socket(AF_INET , SOCK_STREAM , 0 );
	if(sc->s < 0)
	{
		printf("Could not create socket! Error: %d", sc->s);
		return -1;
		
	}else{


		//Connect to remote server
		if (connect(sc->s , (struct sockaddr *)&sc->server , sizeof(sc->server)) < 0)
		{
			printf("Connection error!\n");
			return -1;
		
		}else{

			build_fd_sets(sc->s, &read_fds, NULL);

			maxfds =  sc->s;
		}

 	}
	
	return 1; // connection established
}
