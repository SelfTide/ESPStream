// Drone Control App for android : this is part of a project using esp32 to control a drone. 
#include "GUI.h"
#include "net_com.h"
#include "timer_sleep.h"

// Make sure to only do one CNFG_IMPLEMENTATION or you will have mulitiple definitions which will cause a linker error, you've been warned
// ^-- https://stackoverflow.com/questions/40567041/multiple-definitions-of-linker-error
#define CNFG3D
#include "CNFG.h"
#include "CNFGAndroid.h"
#ifdef __clang__
#define STBIDEF static inline
#endif

#define STBI_FAILURE_USERMSG
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MAXPACKETSIZE 57600

#define DEBUG

#include <android/data_space.h>
#include <android/imagedecoder.h>

short screenx, screeny;
float Heightmap[HMX*HMY];

uint32_t *current_image = NULL, *incoming_image = NULL;
fd_set read_fds, write_fds;
int current_size = 0, incoming_size = 0, incoming_pos = 0, maxfds = 2;
extern unsigned long iframeno;
extern unsigned frames;
extern volatile int suspended;
extern bool BUTTON_PRESSED[2];

typedef struct {
	bool start_image, data_image, end_image;
	int image_size, data_size;
}video_stream_packet_state;

typedef struct {
    int last_pos;
    void *context;
} custom_stbi_mem_context;

static void write2memory(void *context, void *data, int size) {
   custom_stbi_mem_context *c = (custom_stbi_mem_context*)context; 
   char *dst = (char *)c->context;
   char *src = (char *)data;
   int cur_pos = c->last_pos;
   
   for (int i = 0; i < size; i++) {
       dst[cur_pos++] = src[i];
   }
   c->last_pos = cur_pos;
}

int main()
{
	int c, x, y;
	double ThisTime;
	double LastFPSTime = OGGetAbsoluteTime();
	int linesegs = 0;

	
	volatile int i = 0, a = 0; // as related to https://stackoverflow.com/questions/22309028/gdb-left-operand-of-assignment-is-not-an-lvalue

#ifdef DEBUG	
	while(!i)
	{
		a++;
		sleep_ms(100);
	}
#endif
	// Setup our network connection
	// 1) connect to video stream server 2) connect to control server
	
	int action;
	
	video_stream_packet_state vsp;
	uint32_t *image_data;
	int32_t imgWidth, imgHeight, n;
	uint32_t *image_buffer;
	
	vsp.start_image = false, vsp.data_image = false, vsp.end_image = false;
	vsp.image_size = 0, vsp.data_size = 0;
	image_data = (uint32_t *)malloc( sizeof(uint32_t ) * MAXPACKETSIZE);
	memset(image_data, 0, sizeof(uint32_t ) * MAXPACKETSIZE);
	
	server_con sc[2];
	memset(sc[0].ip, 0, 16);
	memset(sc[1].ip, 0, 16);
	strcpy(sc[0].ip, "192.168.0.9");
	strcpy(sc[1].ip, "192.168.0.9");
	sc[0].port = 89;
	sc[1].port = 88;
	sc[0].connected = false;
	sc[1].connected = false;
	
	// we need some different permissions ;)
	int hasperm;

/*	hasperm = AndroidHasPermissions( "ACCESS" );
	if( !hasperm )
	{
		AndroidRequestAppPermissions( "ACCESS" );
	}
*/
	hasperm = AndroidHasPermissions( "WRITE_MEDIA_STORAGE" );
	if( !hasperm )
	{
		AndroidRequestAppPermissions( "WRITE_MEDIA_STORAGE" );
	}
/*	hasperm = AndroidHasPermissions( "READ_MEDIA_STORAGE" );
	if( !hasperm )
	{
		AndroidRequestAppPermissions( "READ_MEDIA_STORAGE" );
	}
*/
	hasperm = AndroidHasPermissions( "INTERNET" );
	if( !hasperm )
	{
		AndroidRequestAppPermissions( "INTERNET" );
	}

	hasperm = AndroidHasPermissions( "ACCESS_NETWORK_STATE" );
	if( !hasperm )
	{
		AndroidRequestAppPermissions( "ACCESS_NETWORK_STATE" );
	}

	CNFGBGColor = 0x969696ff;
	CNFGSetupFullscreen( "Test Bench", 0 );
	//CNFGSetup( "Test Bench", 0, 0 );

	for( x = 0; x < HMX; x++ )
	for( y = 0; y < HMY; y++ )
	{
		Heightmap[x+y*HMX] = tdPerlin2D( x, y )*8.;
	}


	const char * assettext = "Not Found";
	AAsset * file = AAssetManager_open( gapp->activity->assetManager, "asset.txt", AASSET_MODE_BUFFER );
	if( file )
	{
		size_t fileLength = AAsset_getLength(file);
		char * temp = malloc( fileLength + 1);
		memcpy( temp, AAsset_getBuffer( file ), fileLength );
		temp[fileLength] = 0;
		assettext = temp;
	}
	SetupIMU();

	bool first_run = true;
	
	while(1)
	{
			
		// net code here
		if(first_run)
		{

			for(c = 0; c < 2; c++)
			{
				if(connect_serv(&sc[c]))
				{

					printf("Connected to server: %d\n", c); // 0: video stream connection 1: control stream
					sc[c].connected = true;
				
				}else{
					
					printf("Failed to connect to server: %d\n", c);
					sc[c].connected = false;
				
				}
			}
			
			first_run = false;
			
		}else{
			
			if(sc[0].connected || sc[1].connected)
			{
				
				printf("Listening...\n");
				
				// listen for incoming data
				build_fd_sets(sc[0].s, &read_fds, NULL);
				
				sc[0].tv.tv_sec = 0;
				sc[0].tv.tv_usec = 10;
				
				action = select(FD_SETSIZE, &read_fds, NULL, NULL, &sc[0].tv);

				if(action == -1)
				{
					printf("Error in select()\n");
				}else if(action){
					printf("We got data!\n");
				}else{
					printf("No data yet...\n");
				}

				if(action > 0)
				{

					if(FD_ISSET(sc[0].s, &read_fds))	// got data on video connection
					{
						memset((void *)&vsp, 0, sizeof( video_stream_packet_state));
						if((action = recv(sc[0].s, &vsp, sizeof( video_stream_packet_state), 0)) > 0)
						{
							printf("Size of recv: %d\n", action);
							
							if(vsp.end_image)	// are we done with image?
							{
								/*	Test image write code
								FILE *image_file;
								
								image_file = fopen("/data/data/org.yourorg.Drone_control/testimage.jpg", "wb");
								if(image_file)
								{
									if(fwrite(image_data, current_size * 4, 1, image_file))
									{
										printf("Wrote to image file.\n");
									}else{
										printf("Failed to write to image file!\n");
									}
									
								}else{
									printf("Failed to create file!\n");
									perror("/data/data/org.yourorg.Drone_control/testimage.jpg");
								}
								*/								
								
								if(stbi_info_from_memory((stbi_uc	const *)image_data, current_size * 4, &imgWidth, &imgHeight, &n))
								{
									int comp;
									
									stbi_set_flip_vertically_on_load(true);
									printf("Comp level: %d\n", n);
									
									image_buffer = (uint32_t *)stbi_load_from_memory((stbi_uc	const *)image_data, current_size * 4, &imgWidth, &imgHeight, &comp, 4);
									
									if(image_buffer != NULL)
									{
										// if(stbi_write_bmp("/data/data/org.yourorg.Drone_control/Myimage.bmp", imgWidth, imgHeight, 4, image_buffer) == 0)
										//	 printf("Failed to write Bitmap file! %s\n", stbi_failure_reason());
										
										custom_stbi_mem_context context;
										context.last_pos = 0;
										context.context = (void *)image_data;
										memset(image_data, 0, sizeof(uint32_t ) * MAXIMAGESIZE);
										
										if(stbi_write_bmp_to_func((stbi_write_func *)write2memory, &context, imgWidth, imgHeight, 4, image_buffer) > 0)
										{
								
											printf("Wrote image to memory...\n");
											current_size = context.last_pos / 4;
									
											if(current_image == NULL)
											{
												current_image = (uint32_t *)malloc(sizeof(uint32_t) * current_size);
												memset(current_image, 0, sizeof(uint32_t) * current_size);
												memcpy(current_image, image_data, sizeof(uint32_t) * current_size);
											}else{
												
												current_image = (uint32_t *)realloc(current_image, sizeof(uint32_t) * current_size);
												memset(current_image, 0, sizeof(uint32_t) * current_size);
												memcpy(current_image, image_data, sizeof(uint32_t) * current_size);
											}
										}else{
											printf("Failed to write image to memory! %s\n", stbi_failure_reason());
										}
										
									}else{
										printf("Failed to load image! %s\n", stbi_failure_reason());
									}
									
									free(image_buffer);
									
								}else{
									printf("Failed to fetch image info! %s\n", stbi_failure_reason());
								}
								
								incoming_size = 0;
								
								//memset(image_data, 0, sizeof(uint32_t) * MAXPACKETSIZE);
								printf("finished image...\n");
							}
								
							if(vsp.start_image)	// start new image incoming?
							{
								printf("Starting new image...\n");
								current_size = vsp.data_size;
								
								memset(image_data, 0, sizeof(uint32_t ) * MAXPACKETSIZE);		
							}
							
							if(vsp.data_image)	// is this image data?
							{
								printf("Getting image data...\n");
								if(FD_ISSET(sc[0].s, &read_fds))	// got data on video connection
								{
									if((action = recv(sc[0].s, image_data, sizeof( uint32_t) * vsp.data_size, MSG_WAITALL)) < 0) // recive image data
									{
										printf("Error reciving image_data...\n");
									}else{
										printf("Recived image data! Size: %d\n", action);
									}
								}
							}
						}
					}
				}
			}
		}
/*
		if(!(sc[0].s < 0) && !(sc[1].s < 0)) // we are only writing to the first connection
		{
			char buff[100];

			sc[0].tv.tv_sec = 0;
			sc[0].tv.tv_usec = 10;

			memset(buff, 0, 100);

			build_fd_sets(sc[0].s, NULL, &write_fds);

			maxfds =  sc[0].s;

			//sprintf(buff, "%3.3f:%3.3f:%3.3f\n", ((roll * 1.0)/300.0), ((pitch * 1.0)/300.0), ((fabs(throttle) * 100)/300));

			write_cd.roll = 0.00;
			write_cd.pitch = 0.00;
			write_cd.throttle = 0.00;

			write_cd.roll = ((controler.roll * 1.0)/300.0);
			write_cd.pitch = ((controler.pitch * 1.0)/300.0);
			write_cd.throttle = ((controler.throttle * 100.0)/300.0);

			memset(buff, 0, 100);
			buff[0] = 'P';
			memcpy((&buff[1]), write_cd.data, sizeof(Control_data));

			send(sc[0].s, buff, sizeof(Control_data) + 1, 0);
			// printf("Size of Control_data: %d\n", sizeof(Control_data));
			// printf("Size of double: %d\n", sizeof(float));
		}
*/

		iframeno++;

		CNFGHandleInput();
		AccCheck();

		if( suspended ) { usleep(50000); continue; }

		CNFGClearFrame();
		CNFGColor( 0xFFFFFFFF );
		CNFGGetDimensions( &screenx, &screeny );

		CNFGDrawBox(screenx * 0.01, screeny * 0.01, screenx * 0.99, screeny * 0.99);

		if(BUTTON_PRESSED[0])
		{
			draw_tophat_control(0);
		}

		if(BUTTON_PRESSED[1])
		{
			draw_tophat_control(1);
		}

		frames++;
		//On Android, CNFGSwapBuffers must be called, and CNFGUpdateScreenWithBitmap does not have an implied framebuffer swap.
		if(current_image)
		{
			// unsigned char simpleImage[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
			CNFGBlitImage(current_image, (screenx/2) - (imgWidth/2), (screeny/2) - (imgHeight/2), imgWidth, imgHeight, 4);
			// UpdateScreenWithBitmapOffsetX = (screenx/2) - (imgWidth/2);
			// UpdateScreenWithBitmapOffsetY = (screeny/2) - (imgHeight/2);
			// CNFGUpdateScreenWithBitmap(image_data, imgWidth, imgHeight);
			// GLuint tex =(GLuint ) CNFGTexImage(image_data, imgWidth, imgHeight);
			// CNFGBlitTex((unsigned int)tex, (screenx/2) - (imgWidth/2), (screeny/2) - (imgHeight/2),imgWidth, imgHeight);
			// free(image_buffer);
		}

		CNFGSwapBuffers();

		ThisTime = OGGetAbsoluteTime();
		if( ThisTime > LastFPSTime + 1 )
		{
			printf( "FPS: %d\n", frames );
			frames = 0;
			linesegs = 0;
			LastFPSTime+=1;
		}
		
		
	}

	return(0);
}

