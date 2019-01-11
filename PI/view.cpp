#include "view.h"

// GBase
#include "logx.h"
#include "nfy.h"
#include "vmath.h"
#include "kv.h"

// PI
#include "keymap.h"
#if 0
#include "keymapdlg.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

bool view_enabled[ VIEWCOUNT ];

bool view_gamepadActive = false;
float view_deadzone = 0.24f;

static bool view_cameraMode = false;
static float valOrbSpeed = 0.0f;
static float valElvSpeed = 0.0f;
static float valSclSpeed = 0.0f;

static rect_t rects[ VIEWCOUNT ];

typedef struct
{
	int pointerId;
	float x;
	float y;
	float movex;
	float movey;
	float age;
	int moved;
} touch_t;


static touch_t touches[ VIEWCOUNT ];

static touch_t camTouches[16];
static int numCamTouches=0;


rect_t view_rect( int nr ) 
{ 
	ASSERTM( nr >= 0 && nr < VIEWCOUNT, "nr=%d", nr );
	return rects[ nr ]; 
}


static rect_t rectMake( int x, int y, int w, int h )
{
	rect_t r;
	r.x = x;
	r.y = y;
	r.w = w;
	r.h = h;
	return r;
}


void view_init( void )
{
	for ( int i=0; i<VIEWCOUNT; ++i )
		view_enabled[ i ] = false;

	for ( int i=0; i<VIEWCOUNT; ++i )
	{
		touches[ i ].x = touches[ i ].y = 0.0f;
		touches[ i ].pointerId = -1;
	}
}


void view_setup( int backingWidth, int backingHeight, bool smallFormat )
{
	int mainh = backingHeight;
	int mainw = backingWidth;
	int mainx = 0;
	int mainy = 0;

	int lvlsh = 9 * backingHeight / 10;
	int lvlsw = 5 * lvlsh / 4;

	int lvlsx = ( backingWidth - lvlsw ) / 2;
	int lvlsy = 1 * backingHeight / 10;

	rects[ VIEWLVLS ] = rectMake( lvlsx, lvlsy, lvlsw, lvlsh );
	rects[ VIEWMAIN ] = rectMake( mainx, mainy, mainw, mainh );

	int retiw = mainh / 4;
	int retix = ( mainw - retiw ) / 2;
	int retiy = ( 3 * mainh ) / 4 - retiw / 2;
	rects[ VIEWRETI ] = rectMake( retix, retiy, retiw, retiw );

	int helpx = 0; // ( mainw - mainh ) / 2;
	rects[ VIEWHELP ] = rectMake( helpx, 0, mainh, mainh );

	view_gamepadActive = false;
}


static bool removeCamTouch( int pointerId )
{
	for ( int j=0; j<numCamTouches; ++j )
		if ( camTouches[ j ].pointerId == pointerId )
		{
			if ( numCamTouches == 1 )
			{
				numCamTouches = 0;
				return true;
			}
			camTouches[ j ] = camTouches[ numCamTouches-1 ];
			numCamTouches--;
			return true;
		}
	return false;
}


static bool addCamTouch( int pointerId, float x, float y )
{
	for ( int j=0; j<numCamTouches; ++j )
		if ( camTouches[ j ].pointerId == pointerId )
			return false;
	camTouches[ numCamTouches ].pointerId = pointerId;
	camTouches[ numCamTouches ].x = x;
	camTouches[ numCamTouches ].y = y;
	numCamTouches++;
	return true;
}


static int viewHit( int x, int y )
{
	for ( int i=0; i<VIEWCOUNT; ++i )
	{
		if ( view_enabled[ i ] )
		{
			if ( rects[ i ].x <= x && rects[ i ].y <= y && (rects[ i ].x+rects[ i ].w) >= x && (rects[ i ].y+rects[ i ].h) >= y )
				return i;
		}
	}
	return -1;
}


static int viewForPointerId( int pointerId )
{
	for ( int i=0; i<VIEWCOUNT; ++i )
		if ( touches[ i ].pointerId == pointerId )
			return i;
	return -1;
}


int view_touchDown( int pointerCount, int pointerIdx, int* pointerIds, float* pointerX, float* pointerY )
{
		int i = pointerIdx;
		int pointerId = pointerIds[ i ];
		float x = pointerX[ i ];
		float y = pointerY[ i ];
		int v = viewHit( x, y );
		//LOGI( "pointerId 0x%x hit view %d at %.1f,%.1f", pointerId, v, x, y );
		if ( v != -1 )
		{
			touches[ v ].x = x;
			touches[ v ].y = y;
			touches[ v ].pointerId = pointerId;
			touches[ v ].movex = 0.0f;
			touches[ v ].movey = 0.0f;
			touches[ v ].age   = 0.0f;
			touches[ v ].moved = 0;
			x -= rects[ v ].x;
			y -= rects[ v ].y;
			//const float rx = x / rects[ v ].w;
			//const float ry = y / rects[ v ].h;
			switch( v )
			{
				case VIEWPAUS:
					nfy_msg( "menu" );
					break;
				case VIEWMAIN :
#if 1
					if ( !addCamTouch( pointerId, x, y ) )
						LOGE( "Failed to add camera touch for pointerId %d. numCamTouches = %d", pointerId, numCamTouches );
					ASSERT( numCamTouches <= 16 );
#endif
					break;
				case VIEWLVLS:
					break;
				case VIEWBSET:
					break;
				case VIEWBBAC:
					nfy_msg( "menu" );
					break;
				case VIEWBACH:
					nfy_msg( "achievement" );
					break;
				case VIEWBLEA:
					nfy_msg( "leaderboard" );
					break;
				case VIEWBGPL:
					nfy_msg( "signinout" );
					break;
				case VIEWBBUY:
					nfy_msg( "buy" );
					break;
			}
		}
		return v;
}


void view_touchUp( int pointerCount, int pointerIdx, int* pointerIds, float* pointerX, float* pointerY )
{
	{
		int i = pointerIdx;
		float x = pointerX[ i ];
		float y = pointerY[ i ];
		int pointerId = pointerIds[ i ];
		removeCamTouch( pointerId );
		int v;
		do
		{
			v = viewForPointerId( pointerId );
			if ( v != -1 )
			{
				x -= rects[ v ].x;
				y -= rects[ v ].y;
				const float rx = x / rects[ v ].w;
				const float ry = y / rects[ v ].h;
				touches[ v ].pointerId = -1;
				const float th = rects[ VIEWMAIN ].w / 50.0f;	// 20 pixels on 1024 wide screen. 50 pixels on a 2560 pixel wide screen.
				const bool moved = ( fabsf( touches[ v ].movex ) > th || fabsf( touches[ v ].movey ) > th );
				//const float rdx = touches[v].movex / rects[ v ].w;
				//const float rdy = touches[v].movey / rects[ v ].h;
				//const bool quick = touches[ v ].age < 0.43f;
				static char m[ 128 ];
				switch( v )
				{
					case VIEWMAIN:
						if ( !moved )
						{
							snprintf( m, sizeof(m), "select x=%f y=%f button=%d", rx*2-1.0f, ry*2-1.0f, pointerId );
							nfy_queue_msg( m );
						}
						break;
					case VIEWLVLS:
						if ( !moved && !view_enabled[ VIEWRESU] )
						{
							snprintf( m, sizeof(m), "levelsel touchx=%f touchy=%f", rx, ry );
							nfy_msg( m );
						}
						break;
					case VIEWSETT:
						if ( !moved )
						{
							snprintf( m, sizeof(m), "settings touchy=%f", 1-ry );
							nfy_msg( m );
						}
						break;
					case VIEWRESU:
						snprintf( m, sizeof(m), "resumedlg selected=%d close=1", ry>0.5 ? 0 : 1 );
						nfy_msg( m );
						nfy_queue_msg( "levelsel start=1 confirmed=1" );
						break;
				}
			}
		} while ( v != -1 );
	}
}


void view_touchCancel( int pointerCount, int pointerIdx, int* pointerIds, float* pointerX, float* pointerY )
{
	view_touchUp( pointerCount, pointerIdx, pointerIds, pointerX, pointerY );
}


void view_mouseMove( float dx, float dy )
{
	LOGI( "mouseMove %f %f", dx, dy );
	//const float deltaX = dx / (float)rects[ VIEWMAIN ].w;
	//const float deltaY = dy / (float)rects[ VIEWMAIN ].h;
}


void view_touchMove( int pointerCount, int pointerIdx, int* pointerIds, float* pointerX, float* pointerY, int mb_down, bool ctlPressed, bool altPressed )
{
	for ( int i=0; i<pointerCount; ++i )
	{
		//int pointerId = pointerIds[ i ];
		float x = pointerX[ i ];
		float y = pointerY[ i ];
		char msg[128];

		int v = -1;
		if ( mb_down & 1 ) v = viewForPointerId( 0 );
		if ( mb_down & 2 ) v = viewForPointerId( 1 );
		if ( mb_down & 4 ) v = viewForPointerId( 2 );
		if ( v != -1 )
		{
			//LOGI( "pointer %d(%d) moved for view %d", i, pointerId, v );
			float dx = x - touches[ v ].x;
			float dy = y - touches[ v ].y;
			touches[ v ].movex += dx;
			touches[ v ].movey += dy;
			touches[ v ].moved = true;
			const float rdx = dx / rects[ v ].w;
			const float rdy = dy / rects[ v ].h;
			touches[ v ].x = x;
			touches[ v ].y = y;
			x -= rects[ v ].x;
			y -= rects[ v ].y;
			const float rx = -1 + 2 * x / rects[ v ].w;
			const float ry = -1 + 2 * y / rects[ v ].h;

			switch( v )
			{
				case VIEWMAIN:
					if ( mb_down & 1 )
					{
						snprintf( msg, sizeof(msg), "sprinkle start=0 x=%f y=%f addrot=%d", rx, ry, altPressed );
						nfy_msg( msg );
					}
					if ( mb_down & 2 )
					{
						break;
					}
					if ( mb_down & 4 )
					{
        					snprintf( msg, sizeof(msg), "camera movex=%f movey=%f", -2.0f * rdx, -2.0f * rdy );
					        nfy_msg( msg );
						break;
					}
			}
		}
	}
}


static float applyDeadZone( float value, float factor=1.0f )
{
	const float deadzone = factor * view_deadzone;
	const float workzone = ( 1.0f - deadzone );
	if ( fabsf( value ) < deadzone )
		value=0.0f;
	else
		value = ( ( value < 0.0f ) ? ( value+deadzone) : (value-deadzone) ) / workzone;
	return value;
}


void view_setControllerAxisValue( const char* axisName, float value )
{
	view_gamepadActive = true;
	static float lx = 0.0f;
	static float ly = 0.0f;
	static float rx = 0.0f;
	static float ry = 0.0f;
	char m[80];
	m[0] = 0;

	if ( view_cameraMode )
	{
		if ( !strcmp( axisName, "RX" ) )
		{
			value = applyDeadZone( value );
			valOrbSpeed = -2.0f * value;
		}
		if ( !strcmp( axisName, "RY" ) )
		{
			value = applyDeadZone( value );
			valElvSpeed = 0.75f * value;
		}
		if ( !strcmp( axisName, "LY" ) )
		{
			value = applyDeadZone( value );
			valSclSpeed = value;
		}
		return;
	}

	if ( !strcmp( axisName, "LX" ) )
	{
		lx = applyDeadZone( value );
		snprintf( m, sizeof(m), "joystick left=1 x=%f y=%f", lx, -ly );
	}
	if ( !strcmp( axisName, "LY" ) )
	{
		ly = applyDeadZone( value );
		snprintf( m, sizeof(m), "joystick left=1 x=%f y=%f", lx, -ly );
	}
	if ( !strcmp( axisName, "RX" ) )
	{
		rx = applyDeadZone( value );
		snprintf( m, sizeof(m), "joystick left=0 x=%f y=%f", rx, -ry );
	}
	if ( !strcmp( axisName, "RY" ) )
	{
		ry = applyDeadZone( value );
		ry = value;
		snprintf( m, sizeof(m), "joystick left=0 x=%f y=%f", rx, -ry );
	}
	if (!strcmp(axisName, "RT"))
	{
		static bool state = false;
		if (!state && value > 0.9f)
		{
			state = true;
			nfy_msg("fire gamepad=1 keyboard=0");
		}
		else if (state && value < 0.5f)
		{
			state = false;
		}
	}
	if (!strcmp(axisName, "LT"))
	{
		static bool state = false;
		if (!state && value > 0.9f)
		{
			state = true;
			nfy_msg("fire gamepad=1 keyboard=0");
		}
		else if (state && value < 0.5f)
		{
			state = false;
		}
	}

	if ( m[0] )
	{
		nfy_msg( m );
	}

	if ( !view_enabled[ VIEWMAIN ] )
	{
		static int statex = 0;
		static int statey = 0;
		if ( !strcmp( axisName, "LX" ) )
		{
			if ( statex !=  1 && lx > 0.9f )
			{
				statex = 1;
				view_setControllerButtonValue( "DPAD-R", true );
			}
			if ( statex != -1 && lx < -0.9f )
			{
				statex = -1;
				view_setControllerButtonValue( "DPAD-L", true );
			}
			if ( statex != 0 && lx > -0.5f && lx < 0.5f )
			{
				statex = 0;
			}
			if ( statey !=  1 && ly > 0.9f )
			{
				statey = 1;
				view_setControllerButtonValue( "DPAD-D", true );
			}
			if ( statey != -1 && ly < -0.9f )
			{
				statey = -1;
				view_setControllerButtonValue( "DPAD-U", true );
			}
			if ( statey != 0 && ly > -0.5f && ly < 0.5f )
			{
				statey = 0;
			}
		}
	}
}


void view_setControllerButtonValue( const char* butName, bool value )
{
	view_gamepadActive = true;
	char m[80];
	m[0] = 0;

	if ( view_enabled[ VIEWMAIN ] )
	{
		if ( !strcmp( butName, "BUT-L1" ) )
		{
			view_cameraMode = value;
			snprintf( m, sizeof(m), "camcontrol active=%d", value );
			nfy_msg( m );
		}
		if ( !strcmp( butName, "BUT-A" ) && value )
		{
		}
		if ( !strcmp( butName, "DPAD-U" ) && value )
			nfy_msg( "movepoi dx=1 dy=0" );
		if ( !strcmp( butName, "DPAD-D" ) && value )
			nfy_msg( "movepoi dx=-1 dy=0" );
		if ( !strcmp( butName, "DPAD-L" ) && value )
			nfy_msg( "movepoi dx=0 dy=1" );
		if ( !strcmp( butName, "DPAD-R" ) && value )
			nfy_msg( "movepoi dx=0 dy=-1" );
		return;
	}
	else if ( view_enabled[ VIEWSETT ] )
	{
		if ( !strcmp( butName, "BUT-A" ) && value )
			nfy_msg( "settings selected=1" );
		if ( !strcmp( butName, "DPAD-U" ) && value )
			nfy_msg( "settings dy=-1" );
		if ( !strcmp( butName, "DPAD-D" ) && value )
			nfy_msg( "settings dy=1" );
		if ( !strcmp( butName, "DPAD-L" ) && value )
			nfy_msg( "settings dx=-1" );
		if ( !strcmp( butName, "DPAD-R" ) && value )
			nfy_msg( "settings dx=1" );
	}
	else if ( view_enabled[ VIEWRESU ] )
	{
		if ( !strcmp( butName, "BUT-A" ) && value )
		{
			nfy_msg( "resumedlg close=1" );
			nfy_queue_msg( "levelsel start=1 confirmed=1 up=0" );
		}
		if ( !strcmp( butName, "DPAD-U" ) && value )
			nfy_msg( "resumedlg dy=-1" );
		if ( !strcmp( butName, "DPAD-D" ) && value )
			nfy_msg( "resumedlg dy=1" );
	}
	else if ( view_enabled[ VIEWLVLS ] )
	{
		if ( !strcmp( butName, "BUT-A" ) )
			snprintf( m, sizeof(m), "levelsel start=1 up=%d", !value );
		if ( !strcmp( butName, "DPAD-U" ) && value )
			snprintf( m, sizeof(m), "levelsel dx=0 dy=-1" );
		if ( !strcmp( butName, "DPAD-D" ) && value )
			snprintf( m, sizeof(m), "levelsel dx=0 dy=1" );
		if ( !strcmp( butName, "DPAD-L" ) && value )
			snprintf( m, sizeof(m), "levelsel dx=-1 dy=0" );
		if ( !strcmp( butName, "DPAD-R" ) && value )
			snprintf( m, sizeof(m), "levelsel dx=1 dy=0" );
		if ( m[0] )
			nfy_msg( m );
		return;
	}
}


void view_setKeyStatus( int keysym, bool down, bool repeat )
{
	char m[80];
	m[0] = 0;

#if 0
	// See if we need to route it to the key remapper.
	if ( view_enabled[ VIEWRMAP ] )
	{
		if ( down )
		{
			bool done = keymapdlg_record( keysym );
			if ( done ) nfy_msg( "keymapdlg show=0" );
		}
		return;
	}
#endif

	// See if we need to control the tank.
	if ( view_enabled[ VIEWMAIN ] )
	{
		if ( keysym == KEY_FIRE && down && !repeat ) snprintf( m, sizeof(m), "fire gamepad=0 keyboard=1" );
		if ( keysym == 13 && down ) nfy_msg( "singlestep" );
	}

	if ( view_enabled[ VIEWSETT ] )
	{
		if ( keysym == KEY_LEFT  && down ) { nfy_msg( "settings dx=-1" ); }
		if ( keysym == KEY_RIGHT && down ) { nfy_msg( "settings dx=1" ); }
		if ( keysym == KEY_FW    && down ) { nfy_msg( "settings dy=-1" ); }
		if ( keysym == KEY_BW    && down ) { nfy_msg( "settings dy=1" ); }
		switch( keysym )
		{
			case 32:		// space
			case 13:		// return
			case 0x40000058:	// kp enter
				if ( down )
					nfy_msg( "settings selected=1" );
				break;
		}
	}
	else if ( view_enabled[ VIEWRESU ] )
	{
		if ( keysym == KEY_FW    && down ) { nfy_msg( "resumedlg dy=-1" ); }
		if ( keysym == KEY_BW    && down ) { nfy_msg( "resumedlg dy=1" ); }
		if ( down )
			switch( keysym )
			{
				case 32:
				case 13:
				case 0x40000058:	// kp enter
					snprintf( m, sizeof(m), "resumedlg close=1" );
					nfy_msg( m );
					nfy_queue_msg( "levelsel start=1 confirmed=1 up=0" );
					break;
			}
	}
	else if ( view_enabled[ VIEWLVLS ] )
	{
		if ( keysym == KEY_LEFT  && down ) { nfy_msg( "levelsel dx=-1" ); }
		if ( keysym == KEY_RIGHT && down ) { nfy_msg( "levelsel dx=1" ); }
		if ( keysym == KEY_FW    && down ) { nfy_msg( "levelsel dy=-1" ); }
		if ( keysym == KEY_BW    && down ) { nfy_msg( "levelsel dy=1" ); }
		switch( keysym )
		{
			case 32:		// space
			case 13:		// return
			case 0x40000058:	// kp enter
				snprintf( m, sizeof(m), "levelsel start=1 up=%d", !down );
				break;
		}
	}
	else if ( view_enabled[ VIEWMAIN ] )
	{
		static int le=0, ri=0, up=0, dn=0;
		bool change=0;
		if ( keysym == KEY_LEFT  ) { le=down; change=true; }
		if ( keysym == KEY_RIGHT ) { ri=down; change=true; }
		if ( keysym == KEY_FW    ) { up=down; change=true; }
		if ( keysym == KEY_BW    ) { dn=down; change=true; }
		if ( change )
		{
			return;
		}
		if ( keysym == 'c' && down )
			nfy_msg( "clearfield" );
		if ( keysym == '1' && down )
			nfy_msg( "brush radius=1" );
		if ( keysym == '2' && down )
			nfy_msg( "brush radius=2" );
		if ( keysym == '3' && down )
			nfy_msg( "brush radius=3" );
		if ( keysym == '4' && down )
			nfy_msg( "brush radius=4" );
		if ( keysym == '5' && down )
			nfy_msg( "brush radius=5" );
		if ( keysym == 0x4000003A && down )	// F1
			nfy_msg( "show toggle_help=1" );
		if ( keysym == 0x4000003B && down )	// F2
			nfy_msg( "spawndemo nr=0" );
	}

	switch( keysym )
	{
		case 0x40000057:	// SDLK_KP_PLUS
		case 0x2B:		// SDLK_PLUS
			if ( down ) snprintf( m, sizeof(m), "camera trackscale=0.9f" );
			break;
		case 0x40000056:	// SDLK_KP_MINUS
		case 0x5F:		// SDLK_UNDERSCORE
			if ( down ) snprintf( m, sizeof(m), "camera trackscale=1.1f" );
			break;
		case 0x60:		// SDLK_BACKQUOTE / TILDE
			if ( down ) snprintf( m, sizeof(m), "show toggle_grid=1" );
			break;
		case 0x40000048:	// SDLK_PAUSE
			if ( down && !repeat ) snprintf( m, sizeof(m), "pause toggle=1" );
			break;
		case 'p':
			if ( down ) snprintf( m, sizeof(m), "show toggle_aggr=1" );
			break;
		case 'b':
			if ( down ) snprintf( m, sizeof(m), "blackhole toggle=1" );
			break;
		case 0x4000004B:	// SDLK_PAGEUP
			if ( down ) snprintf( m, sizeof(m), "splatradius delta=0.01" );
			break;
		case 0x4000004E:	// SDLK_PAGEDN
			if ( down ) snprintf( m, sizeof(m), "splatradius delta=-0.01" );
			break;
	}

	if ( m[0] )
		nfy_msg( m );
}


void view_update( float dt )
{
	for ( int i=0; i<VIEWCOUNT; ++i )
		touches[ i ].age += dt;

	char msg[80];
	if ( view_cameraMode )
	{
		if ( fabsf( valElvSpeed ) > 0.001f || fabsf( valOrbSpeed ) > 0.001f )
		{
			snprintf( msg, sizeof(msg), "cameraControl elevationDelta=%f orbitDelta=%f", dt * valElvSpeed, dt*valOrbSpeed );
			nfy_msg( msg );
		}
		if ( fabsf( valSclSpeed ) > 0.001f )
		{
			snprintf( msg, sizeof(msg), "cameraControl distScale=%f", 1.0 + valSclSpeed * dt );
			nfy_msg( msg );
		}
	}
}

