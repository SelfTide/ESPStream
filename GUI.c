// Most of this code is taken from https://github.com/cnlohr/rawdrawandroid
// Added a top hat control function

#include "GUI.h"

#define CNFG3D
#define CNFG_IMPLEMENTATION
#include "CNFG.h"


Control_data controler;

cd2char write_cd;

extern struct android_app * gapp;

volatile int suspended;

uint32_t randomtexturedata[256*256];

bool BUTTON_PRESSED[2] = {false, false};

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
	GLfloat step = 1/radius;
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
	hypotenuse = sqrt((pow(x, 2))+(pow(y, 2)));

	return hypotenuse;
}

void draw_tophat_control( int cn )
{
	int screenx = 0, screeny = 0;
	CNFGGetDimensions( (short *)&screenx, (short *)&screeny );

	CNFGPenX = lastmotionx[cn]; CNFGPenY = lastmotiony[cn];

	CNFGSetLineWidth( 4 );

	Point p;
	p.x = lastmotionx[cn];
	p.y = lastmotiony[cn];

	double lx = (p.x - lastbuttonx[cn]), ly = (p.y - lastbuttony[cn]);
	//printf("Length lx: %f ly: %f\n", lx, ly);

	// Restrict where the small circle can go inside the big circle
	if(find_hyp_distance(lx, ly) >= 300)
	{
		double angle;

		// compute origanl angle to find proper x:y cord in bounds using angle
		// make sure we are not dividing by Zero
		// if there is then we have to stop it
		angle = (lx == 0 ) ? fabs(atan(ly/1)) : fabs(atan(ly/lx));

		double clxm = (300 * cos(angle)), clym = (300 * sin(angle));
		//printf("Calculated Length max: X %f Y %f\n Angle: %f\n", clxm, clym, angle);

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

		printf("Cords: X %d Y %d\n", p.x, p.y);
		lx = clxm;
		ly = clym;
	}

	draw_circle(p, 100.0, 0xffffffff);

	char st[200];
	memset(st, 0, 200);
	sprintf(st, "Cord: X %d, Y %d\n", lastmotionx[cn], lastmotiony[cn]);
	CNFGDrawText( st, 10 );

	if(BUTTON_PRESSED[cn])
	{
		if(p.x < (screenx/2))
		{
			controler.roll = lx;
			controler.pitch = ly;
			printf("Roll: %f Pitch: %f\n", controler.roll, controler.pitch);
		}else{
			controler.throttle = ly;
			printf("Throttle: %f\n", controler.throttle);
		}
	}else{
		if(p.x < (screenx/2))
		{
			controler.roll = 0.00;
			controler.pitch = 0.00;
			printf("Roll: %f Pitch: %f\n", controler.roll, controler.pitch);
		}else{
			controler.throttle = 0.00;
			printf("Throttle: %f\n", controler.throttle);
		}
	}

	// Draw big circle where initial button press accured
	p.x = lastbuttonx[cn];
	p.y = lastbuttony[cn];

	draw_circle(p, 300.0, 0xffffffff);

}
