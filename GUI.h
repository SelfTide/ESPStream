#ifndef GUI
#define GUI

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "os_generic.h"
#include <GLES3/gl3.h>
#include <asset_manager.h>
#include <asset_manager_jni.h>
#include <android_native_app_glue.h>
#include <android/sensor.h>
#include "CNFGAndroid.h"
#include "ESPStream.h"

#define GL_BGRA_EXT                       0x80E1
#define GL_BGRA8_EXT                      0x93A1

#define HMX 162
#define HMY 162

typedef struct point {
	GLint x;
	GLint y;
}Point;

typedef struct {
	double roll;
	double pitch;
	double yaw;
	double throttle;
}Control_data;

Control_data controller;

typedef union {

	struct {

		float roll;
		float pitch;
		float throttle;
	};

	char data[sizeof(Control_data)];

}cd2char;


void SetupIMU();

void AccCheck();

void draw_circle(Point pC, GLfloat radius, int Color);

float find_hyp_distance(float x, float y);

void HandleKey( int keycode, int bDown );

void draw_tophat_control(int cn, server_con *sc);

void HandleButton( int x, int y, int button, int bDown );

void HandleMotion( int x, int y, int mask );

void HandleDestroy();

void HandleSuspend();

void HandleResume();

void draw_menu();

void draw_submenu();

void display_control_data(void);

#endif