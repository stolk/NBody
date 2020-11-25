#include "stars.h"
#include "threadtracer.h"

#include <stdlib.h>

int main( int argc, char* argv[]  )
{
	tt_signin( -1, "mainthread" );
	const int num = atoi( argv[1] );
	const bool multithreaded = false;
	stars_init( multithreaded );
	stars_create();
	stars_spawn( 30000, 0,0,  0,0,  GRIDRES/2.3, true, true );
	for ( int i=0; i<num; ++i )
		stars_update( 1/120.0f );

#if defined(linux)
	tt_report( "bench.json" );
#endif

	return 0;
}

