/*
 *		MIT License
 *
 *	Copyright (c) 2022 SelfTide
 *
 *	Permission is hereby granted, free of charge, to any person obtaining a copy of this
 *	software and associated documentation files (the "Software"), to deal in the Software
 *	without restriction, including without limitation the rights to use, copy, modify, merge,
 *	publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
 *	to whom the Software is furnished to do so, subject to the following conditions:
 *
 *	The above copyright notice and this permission notice shall be included in all copies or
 *	substantial portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 *	BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 *	DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * 	I'd like to give a shout out to:
 *
 * 		Sergey Lyubka aka "Dr. Wolf"
 *
 * 	for help with code structure and trouble shooting.
 */

#include "tcp_server.h"

static const char *TAG = "TCPIP";
int maxfds;

int tcp_server_init (server *sc)
{
	tcpip_adapter_ip_info_t ipInfo;
	int opt = 1;

	ESP_LOGI(TAG, "in tcp_server_init()\n");

	memset(sc->ip, 0, 16);

	sc->client_s[0] = 0;
	sc->client_s[1] = 0;
	sc->nclients = 0;

	sc->port = 89;

	// IP address.
	tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipInfo);

	//printf("TCP_SERVER IP:" IPSTR "\n", IP2STR(&ipInfo.ip));
	esp_ip4addr_ntoa((const struct esp_ip4_addr *)&ipInfo.ip, sc->ip, 16);
	ESP_LOGI(TAG, "TCP_SERVER ip: %s\n", sc->ip);

	ESP_LOGI(TAG, "TCP_SERVER PORT: %d\n", sc->port);

	sc->server.sin_addr.s_addr = htonl(INADDR_ANY);	// this might be causing an issue.
	// sc->server.sin_addr.s_addr = ipInfo.ip.addr;
	sc->server.sin_family = AF_INET;
	sc->server.sin_port = htons(sc->port);

	for(size_t i = 0; i < NUMELEMS(sc->client_s); i++) sc->client_s[i] = 0;

	// Create a socket, bind, listen, then add to fd_set
	sc->listen_s = socket(AF_INET , SOCK_STREAM, IPPROTO_IP);
	setsockopt(sc->listen_s, SOL_SOCKET,  SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
	// set socket options to make socket go BRRRRRRRR
	setsockopt(sc->listen_s, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
	setsockopt(sc->listen_s, IPPROTO_TCP, TCP_QUICKACK, &opt, sizeof(opt));

	if(sc->listen_s < 0)
	{
		ESP_LOGE(TAG, "Could not create socket! Error: %d", sc->listen_s);
		perror("socket error");
		return -1;
	}
	if(bind(sc->listen_s, (struct sockaddr *)&sc->server, sizeof(sc->server)) < 0)
	{
		perror("bind error");
		return -1;
	}
	if(listen(sc->listen_s, 5) < 0)
	{
		perror("listen error");
		return -1;

	}
	
	ESP_LOGI(TAG, "listening socket in INIT: %d\n", sc->listen_s);

	return 1; // server listening
}

void *tcp_server_read(void *arg)
{
	args *thread_args = (args *)arg;
	int action, ret, new_s, opt = 1;
	char *recvbuff = (char *)malloc(200 * sizeof(char ));
	fd_set read_fds;


	maxfds = 0;
	memset (recvbuff, 0, 200);

	// ESP_LOGI(TAG, "listening socket in RUN: %d\n", thread_args->sc.listen_s);
	// ESP_LOGI(TAG, "client_s[0] socket in RUN: %d\n", thread_args->sc.client_s[0]);
	// ESP_LOGI(TAG, "client_s[1] socket in RUN: %d\n", thread_args->sc.client_s[1]);

	FD_ZERO(&read_fds);
	FD_SET(thread_args->sc.listen_s, &read_fds);  // Always add listener
	if(thread_args->sc.listen_s > maxfds) maxfds = thread_args->sc.listen_s;

	for (size_t i = 0; i < NUMELEMS(thread_args->sc.client_s); i++) {
		if (thread_args->sc.client_s[i] == 0) continue; // Not connected

		FD_SET(thread_args->sc.client_s[i], &read_fds);

		if (thread_args->sc.client_s[i] > maxfds) maxfds = thread_args->sc.client_s[i];
	}

	thread_args->sc.tv.tv_sec = 0;
	thread_args->sc.tv.tv_usec = 1000; // set timeout to 10ms

	// ESP_LOGI(TAG, "maxfds = %d\n", maxfds);
	action = select(maxfds + 1, &read_fds, NULL, NULL, &thread_args->sc.tv);
	// ESP_LOGI(TAG, "action: %d\n", action);

	if(action < 0)
	{
		perror("Error in select()");
	}else if(action == 0){
		//ESP_LOGI(TAG, "timed out...\n");
	}else{
		if(FD_ISSET(thread_args->sc.listen_s, &read_fds))
		{
			if((new_s = accept(thread_args->sc.listen_s, NULL, NULL)) < 0)
			{
				perror("accept()");
			}else{
				ESP_LOGI(TAG, "In accept()\n");
				setsockopt(new_s, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
				setsockopt(new_s, IPPROTO_TCP, TCP_QUICKACK, &opt, sizeof(opt));

				if(new_s > maxfds) maxfds = new_s;

				if(thread_args->sc.nclients < 2) // max of 2 clients allowed
				{
					for(size_t i = 0;i < NUMELEMS(thread_args->sc.client_s); i++)
						if(thread_args->sc.client_s[i] == 0)
						{
							thread_args->sc.client_s[i] = new_s;
							break;
						}
					thread_args->sc.nclients++;

					ESP_LOGI(TAG, "New connection: %d\n", new_s);
					//send(new_s, "Hello!!\n\r", 9, MSG_DONTWAIT);
				}else{
					ESP_LOGE(TAG, "max clients reached!!");
					send(new_s, "Max clients reached, closing connection!!!\n", 43, MSG_DONTWAIT);
				}
			}
			ESP_LOGI(TAG, "new_s = %d\n", new_s);
		}else{
			for(size_t i = 0; i < NUMELEMS(thread_args->sc.client_s); i++) // only allow 2 clients
			{
				if(thread_args->sc.client_s[i] == 0) continue;
				if(FD_ISSET(thread_args->sc.client_s[i], &read_fds))
				{
					ESP_LOGI(TAG, "reading from client...\n");
					if((ret = recv(thread_args->sc.client_s[i], recvbuff, 200, MSG_DONTWAIT)) < 0)
					{
						perror("recv error");
						close(thread_args->sc.client_s[i]); 				// close socket form client
						FD_CLR(thread_args->sc.client_s[i], &read_fds);		// remove socket from FD set
						thread_args->sc.client_s[i] = 0;
						thread_args->sc.nclients--;
						ESP_LOGI(TAG, "client closed connection!\n");
					}else if(ret == 0){
						close(thread_args->sc.client_s[i]); 				// close socket form client
						FD_CLR(thread_args->sc.client_s[i], &read_fds);		// remove socket from FD set
						thread_args->sc.client_s[i] = 0;
						thread_args->sc.nclients--;
						ESP_LOGI(TAG, "client closed connection!\n");
					}else{
						return recvbuff;
					}
				}
			}
		}
	}
	
	free(recvbuff);
	return NULL;
}

void tcp_server_write(void *arg)
{
	video_stream_packet_state vsps;
	args *thread_args = (args *)arg;
	data_pack *dpack;

	// ESP_LOGI(TAG, "start_capture = %s\t", start_capture ? "true" : "false");
	// ESP_LOGI(TAG, "stack_count = %d\t", stack_count);
	
	// we are only writing to the first client
	if(thread_args->sc.client_s[0] != 0 
		&& start_capture
		&& stack_count > 0)
	{
		while(stack_count > 0)
		{
			ESP_LOGI(TAG, "popping stack");
			
			while(pthread_mutex_lock(&lock) != 0) taskYIELD();
			dpack = pop_stack();
			stack_count--;
			pthread_mutex_unlock(&lock);
			
			ESP_LOGI(TAG, "data pack size: %d", dpack->length);
			send(thread_args->sc.client_s[0], (const uint8_t *)dpack->data, dpack->length, 0);
			
			if(dpack != NULL)
			{
				if(dpack->data)
					free(dpack->data);
				free(dpack);
			}
		}
	}
}

void tcp_server_run(void *arg)
{
	char *recvbuff;
	
	ESP_LOGI(TAG, "In tcp_server_run()");
	
	while(1)
	{
		// pthread_mutex_lock(&lock);
		
		recvbuff = tcp_server_read(arg);
		
		if(recvbuff)
		{
			ESP_LOGE(TAG, "Receive buffer: %s",(char *)recvbuff);
			if(strncmp("start", recvbuff, 5) == 0)
			{
				start_capture = true;
			}else if(strncmp("stop", recvbuff, 4) == 0){
				start_capture = false;
			}
			free(recvbuff);
		}
		
		if(start_capture)
		{
			// ESP_LOGI(TAG, "inside mutex\n");
			tcp_server_write(arg);
		}else{
			vTaskDelay(pdMS_TO_TICKS(10));
		}
		
		// pthread_cond_signal(&cond);
		// pthread_cond_wait(&cond, &lock);
		// pthread_mutex_unlock(&lock);
	}
}