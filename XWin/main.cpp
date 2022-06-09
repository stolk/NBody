#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#if defined(linux)
#	include <fenv.h>
#	include <unistd.h>
#endif

#include "checkogl.h"

#include <SDL.h>

#include "logx.h"
#include "nfy.h"
#include "assertreport.h"

#include "ctrl.h"
#include "view.h"

#if defined(linux)
#	include "threadtracer.h"
#endif


//#include "sengine.h"

#include "icon128pixels.h"


static SDL_Window* window=0;
static SDL_GameController *controller = NULL;
static int fbw=1280,fbh=720; 
static bool should_quit = false;


static void present_assert(const char* condition, const char* file, int line)
{
	const char* platform="unknown";
#if defined(MSWIN)
	platform = "mswin";
#elif defined(ANDROID)
	platform = "android";
#elif defined(IPHN)
	platform = "ios";
#elif defined(OSX)
	platform = "macos";
#elif defined(XWIN)
	platform = "xwin";
#endif

	char text[512];
	snprintf
	(
		text, sizeof(text),
		"ASSERT FAILED %s v%s-%s: %s (%s:%d)", TOSTRING(LOGTAG), TOSTRING(APPVER), platform, condition, file, line
	);

	if ( !strstr( condition, "[NOREP]" ) )
	{
		const char* server = "45.79.100.67";	// stolk.org
		assertreport_init( server, 2323 );
		assertreport_send( text, (int)strlen( text )+1 );
		assertreport_exit();
	}

	SDL_ShowSimpleMessageBox
	(
		SDL_MESSAGEBOX_ERROR,
		"Assertion Failure",
		text,
		NULL
	);
}


static bool add_joystick( int idx )
{
	const int numjoy = SDL_NumJoysticks();
	LOGI("Number of joysticks: %d", numjoy);
	ASSERT( idx < numjoy );
	if ( SDL_IsGameController( idx ) )
	{
		controller = SDL_GameControllerOpen( idx );
		if (!controller)
		{
			LOGE("Could not open gamecontroller %i: %s", idx, SDL_GetError());
			return false;
		}
		LOGI( "Using controller %s", SDL_GameControllerName(controller) );
		return true;
	}
	else
	{
		LOGI( "Joystick %d is not a gamecontroller. Not using it.", idx );
		return false;
	}
}


static bool rmv_joystick( int idx )
{
	if ( SDL_IsGameController( idx ) )
	{
		LOGI( "Game Controller was removed." );
		return true;
	}
	else
	{
		return false;
	}
}


#if 0
static float read_depth( int scrx, int scry )
{
	float depth;
	glReadPixels( scrx, scry, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth );
	CHECK_OGL
	return depth;
}
#endif


static void draw_screen( void )
{
	const char* returnCode = ctrl_drawFrame();
	(void) returnCode;

	TT_BEGIN( "SwapWindow" );
	SDL_GL_SwapWindow( window );
	TT_END  ( "SwapWindow" );
}


static void handle_key( SDL_KeyboardEvent keyev )
{
	const bool down = keyev.state == SDL_PRESSED;
        const bool altdown = keyev.keysym.mod & KMOD_ALT;
	const bool repeat = keyev.repeat;

	const SDL_Keysym* keysym = &(keyev.keysym);

	switch( keysym->sym ) 
	{
		case SDLK_F11:
		case SDLK_RETURN:
			if (repeat) return;
			if (keyev.keysym.sym == SDLK_RETURN && !altdown) 
				return view_setKeyStatus( keysym->sym, down, repeat );
			if ( down )
			{
				const bool was_fs = ( SDL_GetWindowFlags( window ) & SDL_WINDOW_FULLSCREEN_DESKTOP ) != 0;
				SDL_SetWindowFullscreen( window, was_fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP );
				ctrl_fullScreen = !was_fs;
			}
			break;
		case SDLK_ESCAPE:
			{
				if ( down )
				{
					const bool handled = ctrl_onBack();
					if ( !handled )
						should_quit = true;
				}
			}
			break;
		default:
			view_setKeyStatus( keysym->sym, down, repeat );
			break;

	}
}


static void handle_controllerbutton(SDL_ControllerButtonEvent cbev)
{
	const bool pressed = cbev.state == SDL_PRESSED;
	switch (cbev.button)
	{
	case SDL_CONTROLLER_BUTTON_A:
		view_setControllerButtonValue("BUT-A", pressed);
		break;
	case SDL_CONTROLLER_BUTTON_B:
		view_setControllerButtonValue("BUT-B", pressed);
		break;
	case SDL_CONTROLLER_BUTTON_START:
		view_setControllerButtonValue("START", pressed);
		break;
	case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
		view_setControllerButtonValue("BUT-L1", pressed);
		break;
	case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
		view_setControllerButtonValue("BUT-R1", pressed);
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_UP:
		view_setControllerButtonValue("DPAD-U", pressed);
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
		view_setControllerButtonValue("DPAD-D", pressed);
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
		view_setControllerButtonValue("DPAD-L", pressed);
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
		view_setControllerButtonValue("DPAD-R", pressed);
		break;
	case SDL_CONTROLLER_BUTTON_BACK:
		if (pressed)
			if (!ctrl_onBack())
				should_quit = true;
		break;
	}
}


static void handle_controlleraxismotion(SDL_ControllerAxisEvent axev )
{
	const float v = axev.value > 0 ? axev.value / 32767.0f : axev.value / 32768.0f;
	switch (axev.axis)
	{
	case SDL_CONTROLLER_AXIS_LEFTX:
		view_setControllerAxisValue("LX", v);
		break;
	case SDL_CONTROLLER_AXIS_LEFTY:
		view_setControllerAxisValue("LY", v);
		break;
	case SDL_CONTROLLER_AXIS_RIGHTX:
		view_setControllerAxisValue("RX", v);
		break;
	case SDL_CONTROLLER_AXIS_RIGHTY:
		view_setControllerAxisValue("RY", v);
		break;
	case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
		view_setControllerAxisValue("LT", v);
		break;
	case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
		view_setControllerAxisValue("RT", v);
		break;
	}
}

static int pointerIds[3] = { 0,1,2 };
static int lmb_down = 0;
static int mmb_down = 0;
static int rmb_down = 0;
static float mousex = 0;
static float mousey = 0;
static int mb_click = 0;
static int mb_released = 0;


static void handle_mousebutton(SDL_MouseButtonEvent mbev)
{
	const bool down = mbev.state == SDL_PRESSED;
	mousex = mbev.x * csf;
	mousey = ( fbh - 1 - mbev.y ) * csf;
	int* pointerId = pointerIds+0;
	if ( mbev.button == SDL_BUTTON_MIDDLE ) pointerId = pointerIds+1;
	if ( mbev.button == SDL_BUTTON_RIGHT  ) pointerId = pointerIds+2;

	const SDL_Keymod km = SDL_GetModState();
	const bool altPressed = km & KMOD_ALT;
	const bool ctlPressed = km & KMOD_CTRL;


	if ( down )
	{
		const int v = view_touchDown( 1, 0, pointerId, &mousex, &mousey );
		if ( v == VIEWMAIN )
		{
			if (mbev.button == SDL_BUTTON_LEFT)
			{
				lmb_down = down;
				mb_click |= 1;
			}
			if (mbev.button == SDL_BUTTON_RIGHT)
			{
				rmb_down = down;
				mb_click |= 4;
			}
			if (mbev.button == SDL_BUTTON_MIDDLE)
			{
				mmb_down = down;
				mb_click |= 2;
			}
			if ( altPressed )
			{
			}
			if ( ctlPressed )
			{
				const float rx = -1 + 2 * mousex / view_rect( VIEWMAIN ).w;
				const float ry = -1 + 2 * mousey / view_rect( VIEWMAIN ).h;
				char m[80];
				snprintf( m, sizeof(m), "clearcell x=%f y=%f", rx, ry );
				nfy_msg( m );
			}
		}
	}
	else
	{
		view_touchUp  ( 1, 0, pointerId, &mousex, &mousey );
		if ( mbev.button == SDL_BUTTON_LEFT  )
			lmb_down = down;
		if ( mbev.button == SDL_BUTTON_RIGHT )
			rmb_down = down;
		if ( mbev.button == SDL_BUTTON_MIDDLE )
			mmb_down = down;
		mb_released = 1;
	}

#if 0
	if ( !lmb_down && !mmb_down && !rmb_down )
		SDL_SetRelativeMouseMode( ( lmb_down || mmb_down || rmb_down ) ? SDL_TRUE : SDL_FALSE );
#endif
}


static void handle_mousemotion(SDL_MouseMotionEvent mmev)
{
	mousex = mmev.x * csf;
	mousey = ( fbh - 1 - mmev.y ) * csf;
	//float dx = mmev.xrel * csf / (float)fbh;
	const float dy = mmev.yrel * csf / (float)fbh;
	char msg[ 80 ];

#if 0
	if ( lmb_down || mmb_down || rmb_down )
		if ( !SDL_GetRelativeMouseMode() )
			if ( !SDL_SetRelativeMouseMode(SDL_TRUE) )
			{
				//LOGE( "Failed to set relative mouse mode." );
			}
#endif

	if ( lmb_down && view_enabled[ VIEWMAIN ] )
	{
	}
	if ( mmb_down && view_enabled[ VIEWMAIN ] )
	{
		float scl = ( 1.0f + dy );
		scl = scl < 0.1f ? 0.1f : scl;
		scl = scl > 4.0f ? 4.0f : scl;
		snprintf( msg, sizeof( msg ), "camera trackscale=%f", scl );
		nfy_msg( msg );
	}
	if ( rmb_down && view_enabled[ VIEWMAIN ] )
	{
	}

	int* pointerId = pointerIds+0;

	const int mb_down =
		lmb_down ? 1 : 0 +
		mmb_down ? 2 : 0 +
		rmb_down ? 4 : 0;
	const SDL_Keymod km = SDL_GetModState();
	const bool ctlPressed = km & KMOD_CTRL;
	const bool altPressed = km & KMOD_ALT;
	view_touchMove( 1, 0, pointerId, &mousex, &mousey, mb_down, ctlPressed, altPressed );
}


static void handle_mousewheel(SDL_MouseWheelEvent mwev)
{
	if ( view_enabled[ VIEWMAIN ] )
	{
		const float scl = 1.0f - 0.05f * mwev.y;
		char m[80];
		snprintf(m, sizeof(m), "camera trackscale=%f", scl);
		nfy_msg(m);
	}
}


static int process_events( void )
{
	SDL_Event event;
	while( SDL_PollEvent( &event ) ) 
	{
		switch( event.type ) 
		{
			case SDL_WINDOWEVENT:
				if ( event.window.event == SDL_WINDOWEVENT_RESIZED )
				{
					fbw = event.window.data1;
					fbh = event.window.data2;
					LOGI( "resizing to %d,%d", fbw, fbh );
					ctrl_resize( fbw, fbh );
				}
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				handle_mousebutton( event.button );
       				break;

			case SDL_MOUSEMOTION: 
				handle_mousemotion( event.motion );
				break;

			case SDL_MOUSEWHEEL:
				handle_mousewheel( event.wheel );
				break;

			case SDL_JOYDEVICEADDED:
				LOGI( "Joystick device (id %d) was added.", event.cdevice.which );
				add_joystick( event.cdevice.which );
				break;
			case SDL_JOYDEVICEREMOVED:
				LOGI( "Joystick (index %d) was removed.", event.cdevice.which );
				rmv_joystick( event.cdevice.which );
				break;

			case SDL_CONTROLLERBUTTONDOWN:
			case SDL_CONTROLLERBUTTONUP:
				handle_controllerbutton( event.cbutton );
				break;
			case SDL_CONTROLLERAXISMOTION:
				handle_controlleraxismotion( event.caxis );
				break;
			case SDL_KEYDOWN:
				handle_key( event.key );
				break;
			case SDL_KEYUP:
				handle_key( event.key );
				break;
			case SDL_QUIT:
       				/* Handle quit requests (like Ctrl-c). */
				should_quit = true;
				break;
		}
	}
	return 0;
}


void iterate( void )
{
	process_events();	// Process the SDL events.
	ctrl_simulate();	// Run the game simulation.
	draw_screen();		// Draws, and swaps window.

#if 0
	// After drawing, see what's in depth buffer under the mouse cursor.
	float pickdepth = read_depth( mousex, mousey );
	ctrl_pick( mousex, mousey, pickdepth, mb_click );
	//if ( mb_released ) ctrl_mousebuttonreleased();
#endif

	mb_click = 0;
	mb_released = 0;
}


static bool make_window( bool use_aa )
{
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, use_aa ? 1 : 0 );
	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, use_aa ? 4 : 0 );
	SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	window = SDL_CreateWindow
	(
		"Sprinkle, Sprinkle, Little Star.",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		fbw, fbh,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
	);
	if ( !window ) 
	{
		LOGE( "Unable to create %s window: %s\n", use_aa ? "MSAA":"non AA", SDL_GetError() );
		return false;
	}
	else
		LOGI( "Created %s window.", use_aa ? "MSAA" : "Non AA" );
	return true;
}


int main( int argc, char* argv[] )
{
	int vsync=0;
	for ( int i=1; i<argc; ++i )
	{
		if ( !strncmp( argv[ i ], "fs=", 3 ) ) ctrl_fullScreen=atoi(argv[i]+3);
		if ( !strncmp( argv[ i ], "vsync=", 6 ) ) vsync = atoi(argv[i]+6);
		if ( !strncmp( argv[ i ], "w=", 2 ) ) fbw = atoi(argv[i]+2);
		if ( !strncmp( argv[ i ], "h=", 2 ) ) fbh = atoi(argv[i]+2);
	}

	const uint32_t subsystems = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER;
	if ( SDL_Init( 0 ) )
	{
		LOGE( "Unable to initialize SDL: %s", SDL_GetError() );
		return 1;
	}

	if ( SDL_InitSubSystem( subsystems ) )
	{
		LOGE( "Unable to initialize SDL Subsystems: %s", SDL_GetError() );
		return 1;
	}

	if ( SDL_IsTextInputActive() )
		SDL_StopTextInput();

	asserthook = present_assert;

	fprintf( stderr, "Sprinkle, Sprinkle, Little Star. v1.0\n" );
	fprintf( stderr, "(c) Game Studio Abraham Stolk Inc.\n" );

	const int numjoy = SDL_NumJoysticks();
	LOGI("Number of joysticks: %d", numjoy);
	const int mappings_added = SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt");
	LOGI("Added %d gamecontroller mappings.", mappings_added);

	const int got_msaa_window = make_window( true );
	if ( !got_msaa_window )
	{
		const int got_normal_window = make_window( false );
		if ( !got_normal_window )
		{
			LOGE( "Could not create a window. Neither MSAA or standard window." );
			return 1;
		}
	}

	// Hook in the application icon.
	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom( (void*) icon128pixels, 128, 128, 32, 128 * 4, 0x00ff0000, 0x0000ff00, 0x0000ff, 0xff000000 );
	if (surface)
	{
		SDL_SetWindowIcon( window, surface );
		SDL_FreeSurface( surface );
	}

	SDL_GLContext glContext = SDL_GL_CreateContext( window );

	int db=-1;
	const int getattr_result = SDL_GL_GetAttribute( SDL_GL_DOUBLEBUFFER, &db );
	if ( getattr_result != 0 )
	{
		LOGE( "Failed to get attribute SDL_GL_DOUBLEBUFFER." );
	}
	else
	{
		LOGI( "DOUBLEBUFFER: %d", db );
	}

#if defined( MSWIN )
	const int version = gl3wInit();
	LOGI("Version of gl3w: %d", version);
#endif

	ctrl_filesPath = ".";
	ctrl_configPath = ".";

#if defined(xlinux)
	// Sigh... libopenal1 divides by zero!
	// Sigh again... Intel Graphics Performance Analyzer also does bad math!
	// Hard stop on NaN.
	LOGI( "Enabling FP exceptions." );
	feenableexcept( FE_DIVBYZERO | FE_INVALID | FE_OVERFLOW );
	fprintf(stderr,"To examine cpu load: top -H -p %d\n", getpid());
#endif

	int w,h;
	SDL_GL_GetDrawableSize( window, &w, &h );
	csf = w / (float)fbw;

	ctrl_create( fbw, fbh, csf );

	ctrl_enablePremium( true );

	if ( ctrl_fullScreen )
		SDL_SetWindowFullscreen( window, SDL_WINDOW_FULLSCREEN_DESKTOP );

	//sengine_create( argc, (const char**) argv );

	if ( vsync )
	{
		int rv = SDL_GL_SetSwapInterval( -1 );
		if ( rv < 0 ) 
		{
			LOGI( "Late swap tearing not available. Using hard v-sync with display." );
			rv = SDL_GL_SetSwapInterval( 1 );
			if ( rv < 0 ) LOGE( "SDL_GL_SetSwapInterval() failed." );
		}
		else
		{
			LOGI( "Can use late vsync swap." );
		}
	}
	else
	{
		SDL_GL_SetSwapInterval( 0 );
	}

	if ( controller )
		LOGI( "Using controller %s", SDL_GameControllerName( controller ) );

	while ( !should_quit )
	{
		iterate();
	}

	// Tear down the game.
	ctrl_destroy();
	ctrl_exit();

	// Tear down sound engine.
	//sengine_destroy();

	SDL_GL_DeleteContext( glContext );

	LOGI( "Shutting down SDL subsystems." );
	SDL_QuitSubSystem( subsystems );
	SDL_Quit();

	return 0; 
}

