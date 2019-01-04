#include "ctrl.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

extern "C"
{
#include "elapsed.h"
}

// From GBase
#include "logx.h"
#include "kv.h" 
#include "nfy.h"
#include "checkogl.h"
#include "glpr.h"
#include "txdb.h"
#include "quad.h"
#include "sticksignal.h"

// From PI
#include "view.h"
#include "keymap.h"
#include "stars.h"
#include "cam.h"
#include "debugdraw.h"

#include "threadtracer.h"


const char* ctrl_filesPath = ".";
const char* ctrl_configPath = ".";
int ctrl_fullScreen = 0;

int surfaceW = 0;
int surfaceH = 0;

int  ctrl_level = -1;
bool ctrl_signedin = false;
bool ctrl_buyRequested = false;
bool ctrl_achievementRequested = false;
bool ctrl_leaderboardRequested = false;
bool ctrl_signinoutRequested = false;

static bool virgin = true;

// Serial Nr Hash
static int snh = -1;
static const char* snf = "snf";

float csf=1.0f; // no retina display scale for android.

float invaspect=1.0f;


static void setMenuActivation( bool a )
{
	view_enabled[ VIEWLVLS ] = a;
	view_enabled[ VIEWMAIN ] = !a;
	view_enabled[ VIEWBBAC ] = !a;
	view_enabled[ VIEWPAUS ] = !a;

#if defined( PLAY ) // On Amazon app store, we don't do google play services.
	view_enabled[ VIEWBGPL ] = a;
#endif
	
#if defined( PLAY ) || defined( OSX ) || defined( IPHN )
	view_enabled[ VIEWBLEA ] = a && ctrl_signedin;
	view_enabled[ VIEWBACH ] = a && ctrl_signedin;
#endif

	//view_enabled[ VIEWBBUY ] = a && menu_buyEnabled;

	view_enabled[ VIEWSETT ] = false;
}


static void onStartgame( const char* cmd )
{
	setMenuActivation( false );
}


static void onMenu( const char* cmd )
{
	setMenuActivation( true );
}


static void onSigninout( const char* )
{
	ctrl_signinoutRequested = true;
}


static void onBuy( const char* )
{
	ctrl_buyRequested = true;
	view_enabled[ VIEWBBUY ] = false;
}


static void onLeaderboard( const char* )
{
	ctrl_leaderboardRequested = true;
}


static void onAchievement( const char* )
{
	ctrl_achievementRequested = true;
}


static void onOutcome( const char* cmd )
{
	const int win  = nfy_int( cmd, "win"  );
	const int lose = nfy_int( cmd, "lose" );
	if ( !win && !lose )
		LOGE( "Strange. Did we have a draw?" );
}


static void onSettings( const char* cmd )
{
	const int show = nfy_int( cmd, "show" );
	if ( show >= 0 )
	{
		view_enabled[ VIEWSETT ] = show;
		view_enabled[ VIEWLVLS ] = !show;
	}
}


static void onKeymapdlg( const char* m )
{
	const int show = nfy_int( m, "show" );
	if ( show >= 0 )
	{
		//keymapdlg_reset();
		view_enabled[ VIEWRMAP ] = show;
		view_enabled[ VIEWSETT ] = !show;
	}
	if ( show == 0 )
	{
		const int numstored = keymap_store( ctrl_configPath );
		if (!numstored)
		{
			LOGE( "Failed to store keymapping." );
		}
		else
		{
			LOGI( "Stored %d keymappings", numstored );
		}
	}
}


static void onAspect( const char* m )
{
	const float ratio = nfy_flt( m, "ratio" );
	invaspect = 1.0f / ratio;
}


//! Done once per lifetime of process.
static void ctrl_init( void )
{
#ifdef DEBUG
	LOGI( "DEBUG build Minimal V" TOSTRING(APPVER) );
#else
	LOGI( "OPTIMIZED build Minimal V" TOSTRING(APPVER) );
#endif

	const unsigned char* renderer = glGetString( GL_RENDERER );
	LOGI( "GL_RENDERER: %s", (const char*)renderer );
	const unsigned char* glversion = glGetString( GL_VERSION );
	LOGI( "GL_VERSION: %s", glversion );
	const unsigned char* slversion = glGetString( GL_SHADING_LANGUAGE_VERSION );
	LOGI( "GL_SHADING_LANGUAGE_VERSION: %s", slversion?(const char*)slversion:"None" );

	nfy_obs_add( "startgame", onStartgame );
	nfy_obs_add( "menu", onMenu );
	nfy_obs_add( "leaderboard", onLeaderboard );
	nfy_obs_add( "achievement", onAchievement );
	nfy_obs_add( "signinout", onSigninout );
	nfy_obs_add( "buy", onBuy );
	nfy_obs_add( "outcome", onOutcome );
	nfy_obs_add( "settings", onSettings );
	nfy_obs_add( "keymapdlg", onKeymapdlg );
	nfy_obs_add( "aspect", onAspect );

	kv_init( ctrl_configPath );

#if defined( IPHN ) || defined( OSX )
	txdb_path = ctrl_filesPath;
	//wld_path = ctrl_filesPath;
#elif defined( ANDROID )
	txdb_path = "Art";
	//wld_path = "";
#else
	static char tp[256];
	snprintf( tp, sizeof(tp), "%s/Art", ctrl_filesPath );
	txdb_path = tp;
#endif
	
#if defined( ANDROID )
	//sengine_path = "Sounds";
	vbodb_path = "VBOs";
#else
	static char ep[256];
	snprintf( ep, sizeof(ep), "%s/Sounds", ctrl_filesPath );
	// sengine_path = ep;

	static char vp[256];
	snprintf( vp, sizeof(vp), "%s/VBOs", ctrl_filesPath );
	//vbodb_path = vp;
#endif

	txdb_premultiply = true;
	txdb_init();

	view_init();
	//menu_init();
	//settings_init();
	//sengine_init();
	//resumedlg_init();
	sticksignal_init();
	cam_init();
	debugdraw_init();
	CHECK_OGL
	//wld_init();

	tt_signin( -1, "mainthread" );

	stars_init();

	virgin = false;
}


//! Done repeatedly during lifetime of process, each time Android pushes our app to the foreground.
bool ctrl_create( int w, int h, float sf, const char* format )
{
	if ( virgin ) ctrl_init();

	csf = sf;

	bool created = ctrl_draw_create();
	if ( !created ) return false;

	surfaceW = w;
	surfaceH = h;

#if defined(XWIN) || defined(MSWIN)
	kv_set_int( "desktop", 1 );
#endif

	LOGI( "ctrl_create( %d, %d )", w, h );
	ctrl_resize( w, h );

	LOGI( "Setting up view for %dx%d @%fx", w, h, csf );
	const bool smallformat = !strcmp( format, "phone" );
	view_setup( w, h, smallformat );

	quad_init();

	//menu_load_resources();
	//wld_load_resources();
	//hud_load_resources();

	//vbodb_load();

	setMenuActivation( false );

	stars_create();


	return true;
}


void ctrl_resize( int w, int h )
{
	surfaceW = w;
	surfaceH = h;
	view_setup( w, h );
	char msg[80];
	snprintf( msg, sizeof(msg), "aspect w=%d h=%d ratio=%f", w, h, w / (float)h );
	nfy_msg( msg );
}


bool ctrl_onBack( void )
{
	if ( view_enabled[ VIEWSETT ] )
	{
		nfy_msg( "settings show=0" );
		return true;
	}

	if ( view_enabled[ VIEWLVLS ] )
		return false;

	// handle the back key by closing the game view.
	setMenuActivation( true );
	return true;
}


void ctrl_destroy( void )
{
	//if ( wld_levelnr >= 0 ) wld_destroy();

	//vbodb_clear();
	quad_exit();
	txdb_clear();
	CHECK_OGL

	ctrl_draw_destroy();
}


void ctrl_exit( void )
{
	stars_exit();
	tt_report( "threadtracer.json" );
}


void ctrl_enableBuy( bool enabled )
{
#if 0
	menu_buyEnabled = enabled;
	if ( view_enabled[ VIEWLVLS ] )
		view_enabled[ VIEWBBUY ] = menu_buyEnabled;
#endif
}


void ctrl_enablePremium( bool enabled )
{
//	menu_premium = enabled;
	ctrl_enableBuy( false );
#if defined(ANDROID)
	char fname[128];
	snprintf( fname, 128, "%s/.%s", ctrl_filesPath, snf );
	FILE* f = fopen( fname, "w" );
	if ( f )
	{
		fprintf( f, "%d", enabled ? snh : 0 );
		fclose( f );
	}
#endif
}
 
 
bool ctrl_setSNH( int i )
{
	snh = i;
	char fname[128];
	snprintf( fname, 128, "%s/.%s", ctrl_filesPath, snf );
	FILE* f = fopen( fname, "r" );
	if ( f )
	{
		char h[80];
		int cnt = (int)fread( h, 1, 80, f );
		fclose( f );
		int val = cnt ? atoi( h ) : 123;
		if ( val == snh )
		{
			//menu_premium = true;
		}
		else
		{
			LOGI( "serialnr does not match." );
		}
	}
	return 0; // menu_premium;
}


void ctrl_pause( void )
{
}


void ctrl_set_googleplay_status( bool signedin )
{
	ctrl_signedin = signedin;
	if ( view_enabled[ VIEWLVLS ] )
	{
		view_enabled[ VIEWBLEA ] = signedin;
		view_enabled[ VIEWBACH ] = signedin;
	}
}



