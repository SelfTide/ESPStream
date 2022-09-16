// ArduCAM Mini demo (C)2018 Lee
// Web: http://www.ArduCAM.com
// This program is a demo of how to use most of the functions
// of the library with ArduCAM Mini camera, and can run on any Arduino platform.
// This demo was made for ArduCAM_Mini_2MP_Plus.
// It needs to be used in combination with PC software.
// It can take photo continuously as video streaming.


#include <WiFi.h>
#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include "memorysaver.h"

#ifndef STASSID
#define STASSID "YOUR_SSID"
#define STAPSK  "YOUR_SSID_PASSWORD"
#endif

#define MAXPACKETSIZE 2500

unsigned int localPort = 89;      // local port to listen on

// buffers for receiving and sending data
const char* ssid     = STASSID;
const char* password = STAPSK;

//This demo can only work on OV2640_MINI_2MP or OV5642_MINI_5MP or OV5642_MINI_5MP_BIT_ROTATION_FIXED platform.
#if !( defined OV2640_MINI_2MP_PLUS)
#error Please select the hardware platform and camera module in the ../libraries/ArduCAM/memorysaver.h file
#endif
#define BMPIMAGEOFFSET 66

typedef struct {
    unsigned short type;                 /* Magic identifier            */
    unsigned int size;                   /* File size in bytes          */
    unsigned int reserved;
    unsigned int offset;                 /* Offset to image data, bytes */
} HEADER;
typedef struct {
    unsigned int size;               	/* Header size in bytes      */
    int width,height;                	/* Width and height of image */
    unsigned short planes;       		/* Number of colour planes   */
    unsigned short bits;         		/* Bits per pixel            */
    unsigned int compression;        	/* Compression type          */
    unsigned int imagesize;          	/* Image size in bytes       */
    int xresolution,yresolution;     	/* Pixels per meter          */
    unsigned int ncolours;           	/* Number of colours         */
    unsigned int importantcolours;   	/* Important colours         */
} INFOHEADER;
typedef struct {
    unsigned char r,g,b,junk;
} COLOURINDEX;

const char bmp_header[BMPIMAGEOFFSET] PROGMEM =
{
  0x42, 0x4D, 0x36, 0x58, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x28, 0x00,
  0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x03, 0x00,
  0x00, 0x00, 0x00, 0x58, 0x02, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0xC4, 0x0E, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0xE0, 0x07, 0x00, 0x00, 0x1F, 0x00,
  0x00, 0x00
};

// set pin 5 as the slave select for the digital pot:
const int CS = 5;
bool is_header = false;

#if defined (OV2640_MINI_2MP_PLUS)
ArduCAM myCAM( OV2640, CS );
#endif

uint8_t read_fifo_burst(ArduCAM myCAM);

WiFiServer server(localPort);

void setup() {
	// put your setup code here, to run once:
	uint8_t vid, pid;
	uint8_t temp;
#if defined(__SAM3X8E__)
	Wire1.begin();
	Serial.begin(115200);
#else
	Wire.begin();
	Serial.begin(921600);
#endif

	Serial.println(F("ACK CMD ArduCAM Start! END"));
	// set the CS as an output:
	pinMode(CS, OUTPUT);
	digitalWrite(CS, HIGH);
	// initialize SPI:
	SPI.begin();
	  //Reset the CPLD
	myCAM.write_reg(0x07, 0x80);
	delay(100);
	myCAM.write_reg(0x07, 0x00);
	delay(100);
	
	while (1) {
		//Check if the ArduCAM SPI bus is OK
		myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
		temp = myCAM.read_reg(ARDUCHIP_TEST1);
		if (temp != 0x55) {
			Serial.println(F("ACK CMD SPI interface Error!END"));
			delay(1000); continue;
		} else {
			Serial.println(F("ACK CMD SPI interface OK.END")); break;
		}
	}

#if defined (OV2640_MINI_2MP_PLUS)
	while (1) {
		//Check if the camera module type is OV2640
		myCAM.wrSensorReg8_8(0xff, 0x01);
		myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
		myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
		if ((vid != 0x26 ) && (( pid != 0x41 ) || ( pid != 0x42 ))) {
			Serial.println(F("ACK CMD Can't find OV2640 module!"));
			delay(1000); continue;
		}else{
			Serial.println(F("ACK CMD OV2640 detected.END")); break;
		}
	}
#endif
	//Change to JPEG capture mode and initialize the OV5642 module
	myCAM.set_format(JPEG);
	myCAM.InitCAM();
#if defined (OV2640_MINI_2MP_PLUS)
	myCAM.OV2640_set_JPEG_size(OV2640_320x240);
#endif
	delay(1000);
	myCAM.clear_fifo_flag();


	delay(1000);
	myCAM.clear_fifo_flag();

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
	}
	
	Serial.println("");
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
	
	server.begin();
}

WiFiClient client0;

typedef struct {
	bool start_image, data_image, end_image;
	int image_size, data_size;
}video_stream_packet_state;

typedef union {
	uint8_t		in[4];
	uint32_t	out;
}b2dw; // byte to double word ie. 8bit to 32bit hence, 1000bytes * 8 = 8000bits / 32 = 250 "double word" uint32_t

void CheckForConnections();
bool get_arducam_image(video_stream_packet_state *vsp, uint32_t *image_data);

int mode = 0;
bool start_capture = true;
bool GOT_IMAGE = false;

void loop() {

	uint8_t temp = 0xff, temp_last = 0;
	video_stream_packet_state vsps;
	uint32_t *image_data;

	vsps.start_image = false, vsps.data_image = false, vsps.end_image = false;
	vsps.image_size = 0, vsps.data_size = 0;
	image_data = (uint32_t *)malloc( sizeof(uint32_t ) * MAXPACKETSIZE);

	temp = 0xff;

	CheckForConnections();
	
	if(client0.connected())
	{	
		if(client0.available())
		{
			temp = client0.read();
		}
		
		if (temp == 0x21)
		{
			start_capture = false;
			mode = 3;
		}
		
		if (start_capture)
		{
			printf("Start Cap\n");
			myCAM.flush_fifo();
			myCAM.clear_fifo_flag();
			//Start capture
			myCAM.start_capture();
			start_capture = false;
		}
		
		// two modes; single shot and continuous
		switch(mode)
		{
			// Continuous
			case 0:	if(!get_arducam_image(&vsps, image_data))
					{
						printf("Error getting image!\n");
						GOT_IMAGE = false;
					}else{
						printf("Got image!\n");
						GOT_IMAGE = true;
						start_capture = true;
					}
					break;
					
			// Single shot		
			case 1:	if(!GOT_IMAGE)	
					{
						if(!get_arducam_image(&vsps, image_data))
						{
							printf("Error getting image!\n");
							GOT_IMAGE = false;
						}else{
							printf("Got image!\n");
							GOT_IMAGE = true;
							mode = 3;
						}
					}
					
					break;
					
			// Reset single shot
			case 2: mode = 1;
					GOT_IMAGE = false;
					break;
					
			case 3: break;
		}
		
		if(GOT_IMAGE)	// got an image? send it.
		{
			// send start state
			vsps.start_image = true, vsps.data_image = false, vsps.end_image = false;
		
			client0.write((const uint8_t *)&vsps, sizeof(video_stream_packet_state));
			
			// send data state
			vsps.start_image = false, vsps.data_image = true, vsps.end_image = false;
			
			//printf("Size of full packet w/ structure header: %d\n\r", sizeof(video_stream_packet_state) + (sizeof(uint32_t ) * vsps.data_size));
			client0.write((const uint8_t *)&vsps, sizeof(video_stream_packet_state));
			client0.write((const uint8_t *)image_data, sizeof(uint32_t ) * vsps.data_size);
				
			// send end state
			vsps.start_image = false, vsps.data_image = false, vsps.end_image = true;
			vsps.image_size = 0, vsps.data_size = 0;
			memset(image_data, 0, sizeof(uint32_t ) * MAXPACKETSIZE);
			
			client0.write((const uint8_t *)&vsps, sizeof(video_stream_packet_state));
			
			GOT_IMAGE = false;
		}
	}
	
	free(image_data);
}

void CheckForConnections()
{
	if (server.hasClient())
	{
		// If we are already connected to another computer, 
		// then reject the new connection. Otherwise accept
		// the connection. 
		if (client0.connected())
		{
			printf("Connection rejected\n");
			server.available().stop();
		}
		else
		{
			printf("Connection accepted\n");
			client0 = server.available();
		}
	}
}

bool get_arducam_image(video_stream_packet_state *vsps, uint32_t *image_data)
{
	b2dw in_spi;
	int c, d;
	bool is_header = false;
	uint32_t length = 0, image_size = 0;
	
	printf("in get_image...\n");
	
	// clear image_data
	memset(image_data, 0, sizeof(uint32_t ) * MAXPACKETSIZE);
	
	if (myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK))
	{
		length = myCAM.read_fifo_length();
		image_size = length;
	
		printf("in Cap\n");
	
		if ((length >= MAX_FIFO_SIZE) || (length == 0))
		{
			myCAM.clear_fifo_flag();
			start_capture = false;
			printf("Error MAX_FIFO_SIZE or length == 0\n");
			return false;
		}
		// printf("Past check?\n");

		myCAM.CS_LOW();
		myCAM.set_fifo_burst();//Set fifo burst mode
		// Get first two bytes of JPEG header
		SPI.transfer(in_spi.in, 4);
		// printf("Starting 4 bytes: [0]0x%x [1]0x%x [2]0x%x [3]0x%x\n", in_spi.in[0], in_spi.in[1], in_spi.in[2], in_spi.in[3]);
		// delay(10000);
	
		printf("Length: %d\n", length);
		
		if(length < (MAXPACKETSIZE * 4))
		{
			if ((in_spi.in[0] == 0xFF) && (in_spi.in[1] == 0xD8))
			{
				is_header = true;
				image_data[0] = in_spi.out;
				Serial.print("found header...\n");
			}
			
			
			for(c = 1; c < (length / 4); c++)
			{	
				ets_delay_us(15);
				SPI.transfer(in_spi.in, 4);
				
				if(is_header == true)
				{
					image_data[c] = in_spi.out;
					if(!heap_caps_check_integrity_all(true))
					{
						printf("Heap Error @ %d!!!\n\r", c);
						return false;
					}
				}
			}
			
			if (length % 4 > 0)
			{  
				ets_delay_us(15);
				SPI.transfer(in_spi.in, (length % 4));
				
				image_data[c] = in_spi.out;
				is_header = false;
			}else{
				is_header = false;
			}
			
			// set vsps state header info 
			(*vsps).image_size = image_size, !(length % 4) ? (*vsps).data_size = length / 4: (*vsps).data_size = (length / 4) + 1;
		
			myCAM.CS_HIGH();
			myCAM.clear_fifo_flag();
			is_header = false;
			
			return true;
			
		}else{
			printf("Image to large!\n");
			return false;
		}
	}else{
		return false;
	}
}

uint8_t read_fifo_burst(ArduCAM myCAM)
{
  uint8_t temp = 0, temp_last = 0;
  uint32_t length = 0;
  length = myCAM.read_fifo_length();
  client0.println(length, DEC);
  if (length >= MAX_FIFO_SIZE) //512 kb
  {
    client0.println(F("ACK CMD Over size.END"));
    return 0;
  }
  if (length == 0 ) //0 kb
  {
    client0.println(F("ACK CMD Size is 0.END"));
    return 0;
  }
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();//Set fifo burst mode
  temp =  SPI.transfer(0x00);
  length --;
  while ( length-- )
  {
    temp_last = temp;
    temp =  SPI.transfer(0x00);
    if (is_header == true)
    {
      client0.write(temp);
    }
    else if ((temp == 0xD8) & (temp_last == 0xFF))
    {
      is_header = true;
      client0.println(F("ACK CMD IMG END"));
      client0.write(temp_last);
      client0.write(temp);
    }
    if ( (temp == 0xD9) && (temp_last == 0xFF) ) //If find the end ,break while,
      break;
    delayMicroseconds(15);
  }
  myCAM.CS_HIGH();
  is_header = false;
  return 1;
}
