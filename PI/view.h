#ifndef VIEW_H
#define VIEW_H

#include "baseconfig.h"


enum
{
	VIEWSETT,	// settings screen.
	VIEWRESU,	// resume/restart dialog.
	VIEWBGPL,	// button google play sign in/out.
	VIEWBSET,	// button settings
    	VIEWBACH,	// button achievements
    	VIEWBLEA,	// button leaderboards
	VIEWBBUY,	// button buy
	VIEWLVLS,	// menu screen
	VIEWBBAC,	// button back
	VIEWPAUS,	// pause button

	VIEWMAIN,	// main
	VIEWRETI,	// reticule

	VIEWRMAP,	// key remapping (desktop version only.)

	VIEWCOUNT
};

typedef struct 
{
	int x;
	int y;
	int w;
	int h;
} rect_t;


extern rect_t view_rect( int nr );

extern bool view_enabled[ VIEWCOUNT ];

extern bool view_gamepadActive;

extern void view_init( void );

extern void view_setup( int backingW, int backingH, bool smallFormat=false );

extern int  view_touchDown( int pointerCount, int pointerIdx, int* pointerIds, float* pointerX, float* pointerY );

extern void view_touchUp( int pointerCount, int pointerIdx, int* pointerIds, float* pointerX, float* pointerY );

extern void view_touchCancel( int pointerCount, int pointerIdx, int* pointerIds, float* pointerX, float* pointerY );

extern void view_touchMove( int pointerCount, int pointerIdx, int* pointerIds, float* pointerX, float* pointerY, bool mb_down );

extern void view_mouseMove( float dx, float dy );

extern void view_setControllerAxisValue( const char* axisName, float value );

extern void view_setControllerButtonValue( const char* butName, bool value );

extern void view_setKeyStatus( int keysym, bool down, bool repeat );

extern void view_update( float dt );


#endif

