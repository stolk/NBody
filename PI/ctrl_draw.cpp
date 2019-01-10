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
#include "checkogl.h"
#include "glpr.h"
#include "view.h"
#include "quad.h"
#include "nfy.h"
#include "shdw.h"

#include "sticksignal.h"
#include "num_steps.h"
#include "debugdraw.h"

#if defined( USEGLDEBUGPROC )
#	include "gldebugproc.inl"
#endif

//From  PI
#include "text.h"
#include "stars.h"
#include "cam.h"

#if defined( USEES3 )
#	include "shadersources_glsl_es300.h"
#elif defined( USECOREPROFILE )
#	include "shadersources_glsl_150.h"
#endif

static unsigned int fontProgram=0;
static unsigned int starProgram=0;
static unsigned int mainProgram=0;


static bool supportsDebugOutput=false;

static float low_pass_fps = 60.0f;

static rendercontext_t renderContext;

#define USEVIEW( V ) \
	{ \
		rect_t r = view_rect( V ); \
		glViewport( csf*r.x, csf*r.y, csf*r.w, csf*r.h ); \
	}


static bool checkForExtension( const char* searchName )
{
#if defined( USECOREPROFILE )
	GLint n=0;
	glGetIntegerv( GL_NUM_EXTENSIONS, &n );
	for ( int i = 0; i < n; i++ )
	{
		const GLubyte* name = glGetStringi( GL_EXTENSIONS, i );
		//LOGI( "name %s", name );
		if ( strstr( (const char*) name, searchName ) ) return true;
	}
	return false;
#else
	const unsigned char* extensions = glGetString( GL_EXTENSIONS );
	return ( strstr( (char*)extensions, searchName ) != 0 );
#endif
}


//! Done repeatedly during lifetime of process, each time Android pushes our app to the foreground.
bool ctrl_draw_create( void )
{
	supportsDebugOutput = checkForExtension( "_debug_output" );
	CHECK_OGL
	LOGI( "Does %s debug output.", supportsDebugOutput ? "support" : "not support" );
#if defined( USEGLDEBUGPROC )
	if ( supportsDebugOutput )
	{
		glDebugMessageCallbackARB( (GLDEBUGPROCARB) gl_debug_proc, NULL );
                glEnable( GL_DEBUG_OUTPUT );
		CHECK_OGL
	}
#endif

	glpr_init();
	CHECK_OGL

	const char* attribs_font = "position";
	const char* attribs_star = "position,opacity,hue,displacement";
	const char* attribs_main = "position";

	const char* uniforms_font = "rotx,roty,translation,colour";
	const char* uniforms_star = "rotx,roty,translation,invaspect,splatscale,gain";
	const char* uniforms_main = "rotx,roty,translation,colour,invaspect";

	bool ok;

	ok = glpr_load( "Font",  fontProgram, source_vsh_Font, source_fsh_Font, attribs_font, uniforms_font );
	ASSERT(ok);
	CHECK_OGL

	ok = glpr_load( "Star",  starProgram, source_vsh_Star, source_fsh_Star, attribs_star, uniforms_star );
	ASSERT(ok);
	CHECK_OGL

	ok = glpr_load( "Main",  mainProgram, source_vsh_Main, source_fsh_Main, attribs_main, uniforms_main );
	ASSERT(ok);
	CHECK_OGL

	LOGI( "Font program loaded as %d", fontProgram );
	LOGI( "Star program loaded as %d", starProgram );
	LOGI( "Main program loaded as %d", mainProgram );

#if !defined( OSX ) && !defined( IPHN )
	glpr_validate( fontProgram );
	glpr_validate( starProgram );
	glpr_validate( mainProgram );
#endif

	{
		glpr_use( fontProgram );
		static int translationUniform = glpr_uniform("translation");
		static int rotxUniform = glpr_uniform("rotx");
		static int rotyUniform = glpr_uniform("roty");
		static int colourUniform = glpr_uniform("colour");
		glUniform2f( translationUniform, 0, 0 );
		glUniform2f( rotxUniform, 1, 0 );
		glUniform2f( rotyUniform, 0, 1 );
		glUniform4f( colourUniform, 1,1,0,1 );
		CHECK_OGL
	}

	{
		glpr_use( starProgram );
		static int translationUniform = glpr_uniform("translation");
		static int rotxUniform = glpr_uniform("rotx");
		static int rotyUniform = glpr_uniform("roty");
		static int invaspectUniform = glpr_uniform("invaspect");
		static int gainUniform = glpr_uniform("gain");
		glUniform2f( translationUniform, 0, 0 );
		glUniform2f( rotxUniform, 0.042, 0.0 );
		glUniform2f( rotyUniform, 0.0, 0.042 );
		glUniform1f( invaspectUniform, 1.0 );
		glUniform1f( gainUniform, 0.5f );
		CHECK_OGL
	}

	{
		glpr_use( mainProgram );
		static int translationUniform = glpr_uniform("translation");
		static int rotxUniform = glpr_uniform("rotx");
		static int rotyUniform = glpr_uniform("roty");
		static int colourUniform = glpr_uniform("colour");
		static int invaspectUniform = glpr_uniform("invaspect");
		glUniform2f( translationUniform, 0, 0 );
		glUniform2f( rotxUniform, 0.042, 0.0 );
		glUniform2f( rotyUniform, 0.0, 0.042 );
		glUniform4f( colourUniform, 1,1,0,1 );
		glUniform1f( invaspectUniform, 1.0 );
		CHECK_OGL
	}

	glBlendFunc( GL_ONE, GL_ONE );
	CHECK_OGL

#if defined( USEES3 )
	//glLineWidth( 1.0f );
#endif

	glDisable( GL_DEPTH_TEST );
	glDisable( GL_CULL_FACE );

#if defined( USECOREPROFILE )
	glEnable( GL_MULTISAMPLE );
	CHECK_OGL
	glEnable( GL_LINE_SMOOTH );
	CHECK_OGL
#endif

#if defined(USECOREPROFILE)
	glEnable(GL_FRAMEBUFFER_SRGB);
#endif

	return true;
}


static float period = 1/60.0f;


void ctrl_simulate( void )
{
	debugdraw_clear();

	// Handle the queued notification messages.
	nfy_process_queue();

	// calculate dt
	static double last = 0.001 * elapsed_ms_since_start();
	double now = 0.001 * elapsed_ms_since_start();
	period = (float) ( now - last );
	last = now;

	if ( period < 1 && period > 0 )
	{
		static float lowPassPeriod = 1/60.0f;
		lowPassPeriod = 0.8f * lowPassPeriod + 0.2f * period;
		low_pass_fps = 1 / lowPassPeriod;
	}
	else
		period = 1/60.0f;

	int numsteps = (int) num_120Hz_steps( period );
	const float dt = 1 / 120.0f;
	numsteps = numsteps > 6 ? 6 : numsteps;	// stop compensating below 20 fps.

	sticksignal_update( dt, numsteps );

	for (  int i=0; i<numsteps; ++i )
	{
		stars_update( dt );
	}
}


const char* ctrl_drawFrame( void )
{
	view_update( period );

	glFrontFace( GL_CCW );
	glCullFace( GL_BACK );
	glEnable( GL_CULL_FACE );
	glDepthFunc( GL_LEQUAL );
	glDisable( GL_BLEND );
	glActiveTexture ( GL_TEXTURE0 );
	CHECK_OGL

	if ( view_enabled[ VIEWMAIN ] )
	{
		USEVIEW( VIEWMAIN );
		glClearColor( 0.01,0.02,0.08, 1.0 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		glEnable( GL_BLEND );

		{
			glpr_use( mainProgram );
			static int invaspectUniform = glpr_uniform("invaspect");
			static int rotxUniform = glpr_uniform("rotx");
			static int rotyUniform = glpr_uniform("roty");
			static int translationUniform = glpr_uniform("translation");
			glUniform1f( invaspectUniform, invaspect );
			glUniform2f( rotxUniform, cam_scl, 0 );
			glUniform2f( rotyUniform, 0, cam_scl );
			glUniform2f( translationUniform, -cam_scl * cam_pos[0], -cam_scl * cam_pos[1] );
			stars_draw_grid();
		}

		{
			glpr_use( starProgram );
			static int invaspectUniform = glpr_uniform("invaspect");
			static int rotxUniform = glpr_uniform("rotx");
			static int rotyUniform = glpr_uniform("roty");
			static int translationUniform = glpr_uniform("translation");
			glUniform1f( invaspectUniform, invaspect );
			glUniform2f( rotxUniform, cam_scl, 0 );
			glUniform2f( rotyUniform, 0, cam_scl );
			glUniform2f( translationUniform, -cam_scl * cam_pos[0], -cam_scl * cam_pos[1] );
			stars_draw_field();
		}

		{
			glpr_use( mainProgram );
			static int colourUniform = glpr_uniform("colour");
			glUniform4f( colourUniform, 1,1,0,1 );
			debugdraw_draw();
		}
	}

	glpr_use( fontProgram );

	static int font_colourUniform = glpr_uniform( "colour" );

	// Draw FPS

	USEVIEW( VIEWMAIN );
	char str[32];
	snprintf( str, sizeof(str), "%03d", (int) roundf( low_pass_fps ) );
	glUniform4f( font_colourUniform, 0.6f, 0.3f, 0.02f, 1.0f );
	glDisable( GL_DEPTH_TEST );
	text_draw_string( str, vec3_t(1,-1,0), vec3_t(0.023, 0.04, 0.0 ), "right", "bottom", -1 );
	snprintf( str, sizeof(str), "%d", stars_total_count() );
	text_draw_string( str, vec3_t(-1,-1,0), vec3_t(0.024, 0.04, 0.0 ), "left", "bottom", -1 );
	CHECK_OGL
	return 0;
}


void ctrl_draw_destroy( void )
{
	glDeleteProgram( fontProgram );
	fontProgram = 0;

	CHECK_OGL
	LOGI( "Shader programs deleted." );
}


