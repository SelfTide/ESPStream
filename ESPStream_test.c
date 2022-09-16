// ESPStream App for android: this project uses esp32 to stream video to an android device. 
// This project is is based off of Clhonr's rawdrawandroid, all applicable code and copyrights 
// that apply to said project apply here where applicable.

#include "GUI.h"
#include "net_com.h"
#include "timer_sleep.h"

// Make sure to only do one CNFG_IMPLEMENTATION or you will have mulitiple definitions which will cause a linker error, you've been warned
// ^-- https://stackoverflow.com/questions/40567041/multiple-definitions-of-linker-error
#define CNFG3D
#include "CNFG.h"
#include "CNFGAndroid.h"

#define DEBUG

// #include <android/data_space.h>
// #include <android/imagedecoder.h>

short screenx, screeny;
float Heightmap[HMX*HMY];

extern unsigned long iframeno;
extern unsigned frames;
extern volatile int suspended;
extern bool BUTTON_PRESSED[2];

int main()
{
	int x, y, a = 0, size = 0;
	int imgWidth, imgHeight;
	double ThisTime;
	double LastFPSTime = OGGetAbsoluteTime();
	int linesegs = 0;

	// as related to https://stackoverflow.com/questions/22309028/gdb-left-operand-of-assignment-is-not-an-lvalue
	volatile int i = 0; 
	
#ifdef DEBUG	
	while(!i)
	{
		a++;
		sleep_ms(100);
	}
#endif

	uint32_t *current_image = NULL, *image_buffer = NULL;

	// Setup our network connection
	server_con sc;
	sc = espstream_init( "192.168.0.9", 89);
	
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
	
	while(1)
	{
		current_image = espstream_get_image( &sc, &imgWidth, &imgHeight, &size);

		// swap out image to buffer to help with flickering.
		if(current_image)
		{
			if(!image_buffer)
			{
				image_buffer = malloc(size);
			}else{
				image_buffer = (uint32_t *)realloc(image_buffer, size);
			}
			memcpy((uint8_t *) image_buffer,(uint8_t *) current_image, size);
			free(current_image);
		}else{
			printf("Error espstream_get_image!\n");
		}

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
		if(image_buffer)
		{
			CNFGBlitImage(image_buffer, (screenx/2) - (imgWidth/2), (screeny/2) - (imgHeight/2), imgWidth, imgHeight);
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
		
		/*
		if(quit)
		{
			break;
		}
		*/
		
		
	}

	espstream_cleanup();

	return(0);
}

