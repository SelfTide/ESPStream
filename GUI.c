// Most of this code is taken from https://github.com/cnlohr/rawdrawandroid
// Added a top hat control function

#include "GUI.h"

#define CNFG3D
#define CNFG_IMPLEMENTATION
#include "CNFG.h"


Control_data controller = { .roll = 0.0, .pitch = 0.0, .yaw = 0.0, .throttle = 0.0};

cd2char write_cd;

extern struct android_app * gapp;

volatile int suspended;

uint32_t randomtexturedata[256*256];

bool BUTTON_PRESSED[2] = {false, false};
bool SHOW_MENU = false;

unsigned frames = 0;
unsigned long iframeno = 0;

int lastbuttonx[2] = {0, 0};
int lastbuttony[2] = {0, 0};
int lastmotionx[2] = {0, 0};
int lastmotiony[2] = {0, 0};
int lastbid[2] = {0, 0};
int lastmask[2] = {0, 0};
int lastkey[2] = {0, 0}, lastkeydown[2] = {0, 0};

static int keyboard_up;

float accx, accy, accz;
int accs;

float mountainangle;
float mountainoffsetx;
float mountainoffsety;

ASensorManager * sm;
const ASensor * as;
bool no_sensor_for_gyro = false;
ASensorEventQueue* aeq;
ALooper * l;

#define PI 3.141592654


void SetupIMU()
{
	sm = ASensorManager_getInstance();
	as = ASensorManager_getDefaultSensor( sm, ASENSOR_TYPE_GYROSCOPE );
	no_sensor_for_gyro = as == NULL;
	l = ALooper_prepare( ALOOPER_PREPARE_ALLOW_NON_CALLBACKS );
	aeq = ASensorManager_createEventQueue( sm, (ALooper*)&l, 0, 0, 0 ); //XXX??!?! This looks wrong.
	if(!no_sensor_for_gyro) {
		ASensorEventQueue_enableSensor( aeq, as);
		printf( "setEvent Rate: %d\n", ASensorEventQueue_setEventRate( aeq, as, 10000 ) );
	}

}

void AccCheck()
{
	if(no_sensor_for_gyro) {
		return;
	}

	ASensorEvent evt;
	do
	{
		ssize_t s = ASensorEventQueue_getEvents( aeq, &evt, 1 );
		if( s <= 0 ) break;
		accx = evt.vector.v[0];
		accy = evt.vector.v[1];
		accz = evt.vector.v[2];
		mountainangle /*degrees*/ -= accz;// * 3.1415 / 360.0;// / 100.0;
		mountainoffsety += accy;
		mountainoffsetx += accx;
		accs++;
	} while( 1 );
}

void HandleKey( int keycode, int bDown )
{
	lastkey[bDown] = keycode;
	lastkeydown[bDown] = bDown;
	if( keycode == 10 && !bDown ) 
	{ 
		keyboard_up = 0; 
		AndroidDisplayKeyboard( keyboard_up ); 
		printf("Enter pressed\n");  
	}

	if( keycode == 4 ) { AndroidSendToBack( 1 ); } //Handle Physical Back Button.

	printf("Keycode: %d %c\n", keycode, keycode);
}

void HandleButton( int x, int y, int button, int bDown )
{
	lastbid[button] = button;

	lastbuttonx[button] = x;
	lastbuttony[button] = y;

	printf("Last button ID: %d\n", button);

	if( bDown )
	{
		BUTTON_PRESSED[button] = true;
	}else{
		BUTTON_PRESSED[button] = false;
	}
	
	if(bDown && x > 60 && x < 160 && y > 60 && y < 160)
		SHOW_MENU = true;
	else if(bDown && !(x > 60 && x < 160 && y > 60 && y < 160))
		SHOW_MENU = false;
	
	//if( bDown ) { keyboard_up = !keyboard_up; AndroidDisplayKeyboard( keyboard_up ); }
}

void HandleMotion( int x, int y, int mask )
{
	lastmask[mask] = mask;
	lastmotionx[mask] = x;
	lastmotiony[mask] = y;
}

void HandleDestroy()
{
	printf( "Destroying\n" );
	exit(10);
}

void HandleSuspend()
{
	suspended = 1;
}

void HandleResume()
{
	suspended = 0;
}

void draw_circle(Point pC, GLfloat radius, int Color) {
	GLfloat step = acos(1 - 1/radius) * 0.3;
	GLfloat x, y;

	//Change color to white.
	CNFGColor( Color );

	for(GLfloat theta = 0; theta <= 360; theta += step) {
		x = pC.x + (radius * cos(theta));
		y = pC.y + (radius * sin(theta));
		CNFGTackPixel(x, y);
	}
}

float find_hyp_distance(float x, float y)
{
	// the world is full of triangles
	float hypotenuse = 0;

	// Pythagorean Theorem
	hypotenuse = sqrt((pow(x, 2)) + (pow(y, 2)));

	return hypotenuse;
}

// Draw top hat control, then send control data to remote host
void draw_tophat_control(int cn, server_con *sc)
{
	int screenx = 0, screeny = 0;
	int control_range = 300, max_angle = 90, max_throttle = 255;
	char buffer[200];

	CNFGGetDimensions( (short *)&screenx, (short *)&screeny );

	CNFGPenX = lastmotionx[cn]; CNFGPenY = lastmotiony[cn];

	CNFGSetLineWidth( 4 );

	Point p;
	p.x = lastmotionx[cn];
	p.y = lastmotiony[cn];

	double lx = (p.x - lastbuttonx[cn]), ly = (p.y - lastbuttony[cn]);	// (lx, ly) last x and last y
	//printf("Length lx: %f ly: %f\n", lx, ly);

	// Restrict where the small circle can go inside the big circle
	if(find_hyp_distance(lx, ly) >= 300)
	{
		double angle;

		// compute origanl angle to find proper x:y cord in bounds using angle
		// make sure we are not dividing by Zero
		// if there is then we have to stop it
		angle = (lx == 0 ) ? fabs(atan(ly/1)) : fabs(atan(ly/lx));

		double clxm = (300 * cos(angle)), clym = (300 * sin(angle));	// circle last x/y motion
		// printf("Calculated Length max: X %f Y %f\n Angle: %f\n", clxm, clym, angle);

		// with angle and hyp/radius compute new x:y cord
		if(lx < 0) // we are in a negative quad
		{
			p.x = (clxm * -1.0) + lastbuttonx[cn];
		}else{
			p.x = clxm + lastbuttonx[cn];
		}

		if(ly < 0) // we are in a negative quad
		{
			p.y = (clym * -1.0) + lastbuttony[cn];
		}else{
			p.y = clym + lastbuttony[cn];
		}

		// printf("Cords: X %d Y %d\n", p.x, p.y);
		lx = (lx < 0) ? -(clxm) : clxm;
		ly = (ly < 0) ? -(clym) : clym;
	}

	draw_circle(p, 100.0, 0xffffffff);

	memset(buffer, 0, 200);
	sprintf(buffer, "Cord: X %d, Y %d\n", lastmotionx[cn], lastmotiony[cn]);
	CNFGDrawText(buffer, 10);

	memset(buffer, 0, 200);
	if(BUTTON_PRESSED[cn])
	{
		if(p.x < (screenx/2))
		{
			controller.yaw = (lx * 100 / control_range) * max_angle / 100;
			controller.pitch = (ly * 100 / control_range) * max_angle / 100;
		}else{
			controller.roll = (lx * 100 / control_range) * max_angle / 100;
			controller.throttle = (ly * 100 / control_range) * max_throttle / 100;
		}
	}else{
		if(p.x < (screenx/2))
		{
			controller.yaw = 0.00;
			controller.pitch = 0.00;

		}else{
			controller.roll = 0.00;
			controller.throttle = 0.00;
		}
	}

	// Draw big circle where initial button press accured
	p.x = lastbuttonx[cn];
	p.y = lastbuttony[cn];

	draw_circle(p, 300.0, 0xffffffff);
	
	// send control data to remote host
	memset(buffer, 0, 200);
	if(sc->connected)
	{
		strncpy(buffer, "control\0", 8);
		memcpy(&buffer[8], &controller, sizeof(Control_data));
		send(sc->s, buffer, (8 + sizeof(Control_data)), MSG_DONTWAIT);
	}
}

void display_control_data(void)
{
	char buffer[200];
	
	memset(buffer, 0, 200);
	// Draw control data on the upper left handside of he screen, will probably make this a debug option
	sprintf(buffer, "Yaw: %3.3f, Pitch: %3.3f\n", controller.yaw, controller.pitch);
	CNFGPenX = 300, CNFGPenY = 100;		
	CNFGDrawText(buffer, 5);	
	
	// this portion of control data is to sit below yaw and pitch data
	sprintf(buffer, "Roll: %3.3f, Throttle: %3.3f\n", controller.roll, controller.throttle);
	CNFGPenX = 300, CNFGPenY = 130;
	CNFGDrawText(buffer, 5);
}

void draw_menu()
{
	int screenx = 0, screeny = 0;
	CNFGGetDimensions( (short *)&screenx, (short *)&screeny );

	CNFGColor( 0x85FF00FF );

	// draw box
	CNFGDrawBox(60, 60, 160, 160);

	// draw three line segments in box
	CNFGTackSegment(80, 75, 140, 75);

	CNFGTackSegment(80, 110, 140, 110);	

	CNFGTackSegment(80, 145, 140, 145);
}

char *menu_items[] = {
	"Connect",
	"Disconnect",
	"Video Fullscreen"
};

bool show_items[] = { true, false, true };
	

void draw_submenu()
{
	int screenx = 0, screeny = 0;
	int h = 0, w = 0, menu_h = 0, menu_w = 0, menu_count = 0;
	
	CNFGGetDimensions( (short *)&screenx, (short *)&screeny );

	for(int c = 0; c < 3; c++)
	{
		// Get the height and width of text from menu_items and sort for longest
		CNFGGetTextExtents(menu_items[c], &w, &h, 10);
		
		if(w > menu_w) menu_w = w;

		// Add height of all menu_items
		if(show_items[c]) 
		{
			menu_h += h;
			menu_count++;
		}
	}
	
	menu_count++;
	
	CNFGColor( 0x85FF00FF );
	
	CNFGDrawBox(60, 160, 60 + (menu_w + (menu_count * 20)), 160 + (menu_h + (menu_count * 20)));
	
	CNFGPenX = 100, CNFGPenY = 180;
	
	// Draw all currently enabled menu_items with offsets for display
	for(int c = 0; c < 3; c++) 
	{
		if(show_items[c])
		{
			CNFGDrawText(menu_items[c], 10);
			CNFGPenY += h + 20;
		}
	}
}
