#include "cam.h"

// GBase
#include "nfy.h"

float cam_scl = 0.042f;

float cam_pos[2] = { 0,0 };

float cam_aspect = 1.0f;


static void onAspect( const char* m )
{
	const float ratio = nfy_flt( m, "ratio" );
	cam_aspect = ratio;
}


static void onCamera( const char* m )
{
	float trackscale = nfy_flt( m, "trackscale" );
	if ( trackscale > -FLT_MAX )
	{
		cam_scl /= trackscale;
	}
	float movex = nfy_flt( m, "movex" );
	float movey = nfy_flt( m, "movey" );
	if ( movex > -FLT_MAX )
		cam_pos[0] += cam_aspect * movex / cam_scl;
	if ( movey > -FLT_MAX )
		cam_pos[1] += movey / cam_scl;
}


void cam_init( void )
{
	nfy_obs_add( "camera", onCamera );
	nfy_obs_add( "aspect", onAspect );
}

