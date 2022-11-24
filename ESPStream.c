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
 
#include "ESPStream.h"

#ifdef __clang__
#define STBIDEF static inline
#endif

#define STBI_FAILURE_USERMSG
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MAXIMAGESIZE 57600

fd_set read_fds, write_fds;
int current_size = 0, incoming_size = 0, incoming_pos = 0, maxfds = 2;

video_stream_packet_state vsps;
uint32_t *image_data;

int build_fd_sets(int socket, fd_set *read_fds, fd_set *write_fds)
{
	
	if(read_fds)
	{
		FD_ZERO(read_fds);
		FD_SET(socket, read_fds);
	}
	
	if(write_fds)
		FD_ZERO(write_fds);
	
	/* there is something to send, set up write_fd for server socket
	if (server->send_buffer.current > 0)
		FD_SET(server->socket, write_fds);
	*/
   
  return 0;
}

/* ???
typedef struct {
	int client_s, port;
	char ip[16];
	struct timeval tv;
	struct sockaddr_in server;
	bool connected;
}client; 
*/

int connect_serv (server_con *sc)
{
	
	sc->tv.tv_sec = 0;
	sc->tv.tv_usec = 10;
	//Create a socket
	sc->server.sin_addr.s_addr = inet_addr(sc->ip);
	sc->server.sin_family = AF_INET;
	//inet_pton(AF_INET, "192.168.0.252", &server.sin_addr);
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


static void write2memory(void *context, void *data, int size) 
{
   custom_stbi_mem_context *c = (custom_stbi_mem_context*)context; 
   char *dst = (char *)c->context;
   char *src = (char *)data;
   int cur_pos = c->last_pos;
   
   for (int i = 0; i < size; i++) {
       dst[cur_pos++] = src[i];
   }
   c->last_pos = cur_pos;
}

// initialize server_con structure and allocate memory for recive buffer of image.
server_con espstream_init(char *ip_address, uint8_t port)
{
	vsps.start_image = false, vsps.data_image = false, vsps.end_image = false;
	vsps.image_size = 0;
	image_data = (uint32_t *)malloc( sizeof(uint32_t ) * MAXIMAGESIZE);
	memset(image_data, 0, sizeof(uint32_t ) * MAXIMAGESIZE);
	
	server_con sc;
	memset(sc.ip, 0, 16);
	strcpy(sc.ip, ip_address);
	
	sc.port = port;
	
	sc.connected = false;
	
	sc.tv.tv_sec = 0;
	sc.tv.tv_usec = 10;
	
	return sc;
}

// returns image data from stream or NULL on error, width, height, and size are passed in to be populated.
uint32_t *espstream_get_image(server_con *sc, int *imgWidth, int *imgHeight, int *size)
{
	int n, action;
	uint32_t *image_buffer, *current_image = NULL;
	
	
	// net code
	if(!sc->connected)
	{

		if(connect_serv(sc))
		{

			printf("Connected to video stream server.\n"); // 0: video stream connection 1: control stream
			send(sc->s, "start", 5, MSG_DONTWAIT);
			sc->connected = true;
		
		}else{
			
			printf("Failed to connect to video stream server.\n");
			sc->connected = false;
		
		}
		
	}else{
		
		if(sc->connected)
		{
			
			//printf("Listening...\n");
			
			// listen for incoming data
			build_fd_sets(sc->s, &read_fds, NULL);
			
			action = select(FD_SETSIZE, &read_fds, NULL, NULL, &sc->tv);

			if(action == -1)
			{
				printf("Error in select()\n");
				return NULL;
			}else if(action){
				printf("We got data!\n");
			}else{
				printf("No data yet...\n");
			}

			if(action > 0)
			{

				if(FD_ISSET(sc->s, &read_fds))	// got data on video connection
				{
					memset((void *)&vsps, 0, sizeof( video_stream_packet_state));
					if((action = read(sc->s, &vsps, sizeof( video_stream_packet_state))) > 0)
					{
						printf("Size of recv: %d %d\n", action, (int )sizeof( video_stream_packet_state));
						
						if(vsps.end_image)	// are we done with image?
						{

							/*	Test image write code - just leaving this here for debug reasons
							FILE *image_file;
							
							image_file = fopen("/data/data/org.yourorg.ESPStream/testimage.jpg", "w+b");
							if(image_file)
							{
								
								if(fwrite(image_data, current_size * 4, 1, image_file))
								{
									printf("Wrote to image file.\n");
								}else{
									printf("Failed to write to image file!\n");
								}
								
								int size;
								
								fseek(image_file, 0, SEEK_END);
								size = ftell(image_file);
								
								if(fread(image_data, 1, size, image_file) != size)
									printf("failed to read image file!\n");
								
							}else{
								printf("Failed to create file!\n");
								perror("testimage.jpg");
							}
							*/
							
							if(stbi_info_from_memory((stbi_uc	const *)image_data, current_size * 4, imgWidth, imgHeight, &n))
							{
								int comp;
								
								stbi_set_flip_vertically_on_load(true);
								printf("Comp level: %d\n", n);
								
								image_buffer = (uint32_t *)stbi_load_from_memory((stbi_uc	const *)image_data, current_size * 4, imgWidth, imgHeight, &comp, 4);
								
								if(image_buffer != NULL)
								{
									// if(stbi_write_bmp("/data/data/org.yourorg.Drone_control/Myimage.bmp", imgWidth, imgHeight, 4, image_buffer) == 0)
									//	 printf("Failed to write Bitmap file! %s\n", stbi_failure_reason());
									
									custom_stbi_mem_context context;
									context.last_pos = 0;
									context.context = (void *)image_data;
									memset(image_data, 0, sizeof(uint32_t ) * MAXIMAGESIZE);
									
									if(stbi_write_bmp_to_func((stbi_write_func *)write2memory, &context, *imgWidth, *imgHeight, 4, image_buffer) > 0)
									{
							
										printf("Wrote image to memory...\n");
										current_size = (context.last_pos / sizeof(uint32_t)) - 35; // this is supposed to compinsate for junk data
								
										if(current_image == NULL)
										{
											current_image = (uint32_t *)malloc(sizeof(uint32_t) * current_size);
											memset(current_image, 0, sizeof(uint32_t) * current_size);
											memcpy(current_image, &image_data[35], sizeof(uint32_t) * current_size); // start where junk data ends?
											*size = sizeof(uint32_t) * current_size; // number of bytes
										}
									}else{
										printf("Failed to write image to memory! %s\n", stbi_failure_reason());
										return NULL;
									}
									
								}else{
									printf("Failed to load image! %s\n", stbi_failure_reason());
									return NULL;
								}
								
								free(image_buffer);
								
							}else{
								printf("Failed to fetch image info! %s\n", stbi_failure_reason());
							}
							
							incoming_size = 0;
							
							//memset(image_data, 0, sizeof(uint32_t) * MAXIMAGESIZE);
							printf("finished image...\n");
						}
							
						if(vsps.start_image)	// start new image incoming?
						{
							printf("Starting new image... size: %d\n", vsps.image_size);
							current_size = vsps.image_size / 4;
							
							memset(image_data, 0, sizeof(uint32_t ) * MAXIMAGESIZE);		
						}
						
						if(vsps.data_image)	// is this image data?
						{
							printf("Getting image data...\n");
							if(FD_ISSET(sc->s, &read_fds))	// got data on video connection
							{
								if((action = recv(sc->s, image_data, current_size * 4, MSG_WAITALL)) < 0) // recive image data
								{
									printf("Error reciving image_data...\n");
									return NULL;
								}else{
									printf("Recived image data! expected size: %d recived Size: %d\n", current_size * 4, action);
								}
							}
						}
					}
				}
			}
		}
	}
	
	return current_image;
}

void espstream_cleanup()
{
	free(image_data);
}
