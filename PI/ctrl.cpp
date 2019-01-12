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
#include "stars.h"
#include "cam.h"
#include "debugdraw.h"

#if defined(linux)
#	include "threadtracer.h"
#endif


const char* ctrl_filesPath = ".";
const char* ctrl_configPath = ".";
int ctrl_fullScreen = 1;

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


static void onStartgame( const char* cmd )
{
}


static void onMenu( const char* cmd )
{
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


static void onAspect( const char* m )
{
	const float ratio = nfy_flt( m, "ratio" );
	invaspect = 1.0f / ratio;
}


static float sprinkle_radius = 0.02f;
static void onSprinkle( const char* m )
{
	const float x = nfy_flt( m, "x" );
	const float y = nfy_flt( m, "y" );
	const int addrot = nfy_int( m, "addrot" );
	//const int start = nfy_flt( m, "start" );
	const float px = cam_pos[0] + x / cam_scl / invaspect;
	const float py = cam_pos[1] + y / cam_scl;
	const int cnt = 12;
	const float rad = sprinkle_radius / cam_scl;
	stars_sprinkle( cnt, px, py, rad, addrot );
}


static void onSelect( const char* m )
{
	const float x = nfy_flt( m, "x" );
	const float y = nfy_flt( m, "y" );
	const int button = nfy_int( m, "button" );
	const float px = cam_pos[0] + x / cam_scl / invaspect;
	const float py = cam_pos[1] + y / cam_scl;
	if ( button == 0 )
	{
		stars_select( px, py );
	}
	if ( button == 2 )
	{
		stars_sprinkle( 1, px, py, 0.02f, false );
	}
}


static void onClearfield( const char* m )
{
	stars_clear();
}


static void onClearcell( const char* m )
{
	const float x = nfy_flt( m, "x" );
	const float y = nfy_flt( m, "y" );
	const float px = cam_pos[0] + x / cam_scl / invaspect;
	const float py = cam_pos[1] + y / cam_scl;
	stars_clear_cell( px, py );
}


static void onBrush( const char* m )
{
	const int radius = nfy_int( m, "radius" );
	sprinkle_radius = radius * 0.02f;
}


static void onShow( const char* m )
{
	const int toggle_grid = nfy_int( m, "toggle_grid" );
	const int toggle_aggr = nfy_int( m, "toggle_aggr" );
	const int toggle_help = nfy_int( m, "toggle_help" );
	if ( toggle_grid > 0 ) stars_show_grid = !stars_show_grid;
	if ( toggle_aggr > 0 ) stars_show_aggr = !stars_show_aggr;
	if ( toggle_help > 0 ) view_enabled[ VIEWHELP ] = ! view_enabled[ VIEWHELP ];
}


static void onBlackhole( const char* m )
{
	const int toggle = nfy_int( m, "toggle" );
	if ( toggle > 0 )
	{
		stars_add_blackhole = !stars_add_blackhole;
	}
}


static void onSpawndemo( const char* m )
{
	const int nr = nfy_int( m, "nr" );
	stars_clear();
	if ( nr == 0)
	{
		stars_add_blackhole = true;
		stars_spawn( 30000, 0,0,  0,0,  GRIDRES/2.3, true, true );
	}
}


static void onSplatradius( const char* m )
{
	const float delta = nfy_flt( m, "delta" );
	if ( delta > -FLT_MAX )
		stars_change_splat_radius( delta );
}


static void onPause( const char* m )
{
	const int toggle = nfy_int( m, "toggle" );
	if ( toggle > 0 )
		ctrl_paused = !ctrl_paused;
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
	nfy_obs_add( "aspect", onAspect );
	nfy_obs_add( "sprinkle", onSprinkle );
	nfy_obs_add( "select", onSelect );
	nfy_obs_add( "clearfield", onClearfield );
	nfy_obs_add( "clearcell", onClearcell );
	nfy_obs_add( "brush", onBrush );
	nfy_obs_add( "show", onShow );
	nfy_obs_add( "blackhole", onBlackhole );
	nfy_obs_add( "spawndemo", onSpawndemo );
	nfy_obs_add( "splatradius", onSplatradius );
	nfy_obs_add( "pause", onPause );

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

#if defined(linux)
	tt_signin( -1, "mainthread" );
#endif

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

	view_enabled[ VIEWMAIN ] = true;
	view_enabled[ VIEWHELP ] = true;

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
	if ( view_enabled[ VIEWHELP ] )
	{
		nfy_msg( "show toggle_help=1" );
		return true;
	}

	return false;
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
#if defined(linux)
	tt_report( "threadtracer.json" );
#endif
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



