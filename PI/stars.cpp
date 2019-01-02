#include "stars.h"

// From GBase
#include "logx.h"
#include "math.h"
#include "checkogl.h"
#include "vmath.h"
#include "rendercontext.h"
#include "glpr.h"

// PI
#include "cam.h"
#include "debugdraw.h"


#include "threadtracer.h"

#include <float.h>
#include <immintrin.h>

#define VECTORIZE	1

#if defined( MSWIN )
#       define ALIGNEDPRE __declspec(align(32))
#       define ALIGNEDPST
#else
#       define ALIGNEDPRE
#       define ALIGNEDPST __attribute__ ((aligned (32)))
#endif



static cell_t cells[ GRIDRES ][ GRIDRES ];

static aggregate_t* aggregates[ 1+NUMDIMS ] = { 0,0,0,0,0 };


static const int circle_sz = 12;
static const float circle_scl = 0.02f;

typedef struct
{
	float circle[ circle_sz ][ 3 ][ 2 ];
	float displacements[ NUMSTARS ][ 2 ];
} vdata_t;

static vdata_t vdata;


//! Convenience struct, so we can return two floats from a function that sums forces.
typedef struct
{
	float x;
	float y;
} force_t;


#define MAXCONTRIBS	500

typedef struct
{
	int totalcount;
	int mixedcoords[ MAXCONTRIBS ];

	int counts[ NUMDIMS+1 ];
	int sortedcoords[ MAXCONTRIBS ];
} contribinfo_t;

contribinfo_t contribs[ GRIDRES ][ GRIDRES ];


static const float G = 0.0002f;

#define ENCODECONTRIB( LEVEL, X, Y ) \
	( ( X << 0 ) | ( Y << 8 ) | ( LEVEL << 16 ) )



static float halton( int idx, int base )
{
	float result = 0;
	float f = 1.0f / base;
	int i = idx;
	while ( i > 0 )
	{
		result += f * ( i % base );
		i = i / base;
		f = f / base;
	}
	return result;
}


void remove_from_cell( int idx, int cx, int cy )
{
	ASSERT( cx >= 0 && cx < GRIDRES && cy >= 0 && cy < GRIDRES );
	cell_t& cell = cells[ cx ][ cy ];
	ASSERTM( idx >= 0 && idx < cell.cnt, "idx %d not in range 0..%d", idx, cell.cnt );
	const int last = cell.cnt-1;
	if ( last != idx )
	{
		cell.px[idx] = cell.px[last];
		cell.py[idx] = cell.py[last];
		cell.vx[idx] = cell.vx[last];
		cell.vy[idx] = cell.vy[last];
		cell.st[idx] = cell.st[last];
	}
	cell.cnt--;
}


int add_to_cell( int cx, int cy, float px, float py, float vx, float vy )
{
	cell_t& cell = cells[ cx ][ cy ];
	const float EPS = 10e-6;
	ASSERTM
	(
		px >= cell.xrng[0] - EPS && px <= cell.xrng[1] + EPS,
		"px %f not in range %f..%f of cx %d vx,vy=%f,%f",
		px, cell.xrng[0], cell.xrng[1], cx, vx, vy
	);
	ASSERTM
	(
		py >= cell.yrng[0] - EPS && py <= cell.yrng[1] + EPS,
		"py %f not in range %f..%f of cy %d vx,vy=%f,%f",
		py, cell.yrng[0], cell.yrng[1], cy, vx, vy
	);
	const int i = cell.cnt++;
	ASSERT( i < CELLCAP );
	cell.px[ i ] = px;
	cell.py[ i ] = py;
	cell.vx[ i ] = vx;
	cell.vy[ i ] = vy;
	cell.st[ i ] = 0;
	return i;
}


int add_star( float px, float py, float vx, float vy )
{
	const int cx = POS2CELL(px);
	const int cy = POS2CELL(py);
	if ( cx < 0 || cx >= GRIDRES ) return -1;
	if ( cy < 0 || cy >= GRIDRES ) return -1;
	ASSERTM( cx >= 0 && cx < GRIDRES && cy >= 0 && cy < GRIDRES, "Cell coordinate %d,%d is out of grid bounds for star position %f,%f", cx, cy, px, py );
	return add_to_cell( cx, cy, px, py, vx, vy );
}


void stars_spawn( int num, float centrex, float centrey, float vx, float vy, float radius )
{
	static int idx=0;

	for ( int i=0; i<num; ++i )
	{
		float dsqr = 0.0f;
		float px,py;
		do
		{
			px = -1 + 2 * halton( idx, 2 );
			py = -1 + 2 * halton( idx, 3 );
			idx++;
			dsqr = px*px + py*py;
		} while ( dsqr >= 1.0f );
#if 0
		float vx = -1 + 2 * halton( idx, 5 );
		float vy = -1 + 2 * halton( idx, 7 );
		float l = sqrtf( vx*vx + vy*vy );
		vx = 0.04f * vx * (1/l);
		vy = 0.04f * vy * (1/l);
#endif
		//const float scl = GRIDRES/2.4f;
		add_star( centrex + radius*px, centrey + radius*py, vx, vy );
	}
}


void stars_create( void )
{
	for ( int cx=0; cx<GRIDRES; ++cx )
		for ( int cy=0; cy<GRIDRES; ++cy )
		{
			cell_t& cell = cells[ cx ][ cy ];
			cell.cnt = 0;
			cell.xrng[0] = CELL2POS(cx) - 0.5f;
			cell.xrng[1] = CELL2POS(cx) + 0.5f;
			cell.yrng[0] = CELL2POS(cy) - 0.5f;
			cell.yrng[1] = CELL2POS(cy) + 0.5f;
		}

	{
		const cell_t& cell = cells[GRIDRES/2][GRIDRES/2];
		LOGI( "center cell has x range %f,%f", cell.xrng[0], cell.xrng[1] );
		LOGI( "px 0.0 falls in cx %d", POS2CELL(0.0f) );
	}

	const int num = NUMSTARS/2;
	const float off = GRIDRES/4.8f;
	const float rad = GRIDRES/5.0f;
	stars_spawn( num, -off/2, -off,  0.05f,  0.4f, rad );
	stars_spawn( num,  off/2,  off, -0.05f, -0.4f, rad );

	float maxcnt=0;
	for ( int cx=0; cx<GRIDRES; ++cx )
		for ( int cy=0; cy<GRIDRES; ++cy )
		{
			cell_t& cell = cells[ cx ][ cy ];
			if ( maxcnt < cell.cnt )
				maxcnt = cell.cnt;
		}
	LOGI( "Created %d stars in %d cells, with max cell load at %f", NUMSTARS, GRIDRES*GRIDRES, maxcnt / (float) CELLCAP );


	for ( int lvl=1; lvl<=NUMDIMS; ++lvl )
	{
		const int res = grid_resolutions[ lvl ];
		const size_t sz = res * res * sizeof( aggregate_t );
		aggregates[ lvl ] = (aggregate_t*) malloc( sz );
		memset( aggregates[ lvl ], 0, sz );
	}
	LOGI( "Allocated %d aggregation levels.", NUMDIMS );

	stars_calculate_contribution_info();
	LOGI( "Calculated contribution info." );

	for ( int i=0; i<circle_sz; ++i )
	{
		const float a0 = M_PI * 2 / circle_sz * ( i+0 );
		const float a1 = M_PI * 2 / circle_sz * ( i+1 );
		const float x0 = circle_scl * cosf( a0 );
		const float y0 = circle_scl * sinf( a0 );
		const float x1 = circle_scl * cosf( a1 );
		const float y1 = circle_scl * sinf( a1 );
		float* writer = vdata.circle[i][0];
		*writer++ =  0; *writer++ =  0;
		*writer++ = x0; *writer++ = y0;
		*writer++ = x1; *writer++ = y1;
	}
}


void aggregate_cells( void )
{
	TT_SCOPE( "aggregate_cells" );
	// note aggregates[0] is unused, we count aggregate levels from 1 to NUMDIMS
	aggregate_t* writer = aggregates[ 1 ];
	for ( int x=0; x<GRIDRES; ++x )
		for ( int y=0; y<GRIDRES; ++y )
		{
			const cell_t& cell = cells[ x ][ y ];
			const int cnt = cell.cnt;
			writer->cnt = cnt;
			if ( !cnt )
			{
				writer->cx = ( cell.xrng[0] + cell.xrng[1] ) / 2;
				writer->cy = ( cell.yrng[0] + cell.yrng[1] ) / 2;
			}
			else
			{
				writer->cx = 0;
				writer->cy = 0;
				for ( int i=0; i<cnt; ++i )
				{
					writer->cx += cell.px[i];
					writer->cy += cell.py[i];
				}
				writer->cx *= ( 1.0f / cnt );
				writer->cy *= ( 1.0f / cnt );
			}
			writer++;
		}
}


int aggregate_level( int lvl )
{
	TT_SCOPE( "aggregate_level" );
	ASSERT( lvl >= 2 && lvl <= NUMDIMS );
	const aggregate_t* reader = aggregates[ lvl-1 ];
	aggregate_t* writer = aggregates[ lvl ];
	const int res = grid_resolutions[ lvl ];
	const int dres = res*2;
	int highcnt = 0;
	for ( int x=0; x<res; ++x )
	{
		for ( int y=0; y<res; ++y )
		{
			const aggregate_t* s0 = reader+0;
			const aggregate_t* s1 = reader+1;
			const aggregate_t* s2 = reader+dres+0;
			const aggregate_t* s3 = reader+dres+1;
			writer->cnt = s0->cnt + s1->cnt + s2->cnt + s3->cnt;
			writer->cx =  s0->cnt * s0->cx;
			writer->cx += s1->cnt * s1->cx;
			writer->cx += s2->cnt * s2->cx;
			writer->cx += s3->cnt * s3->cx;
			writer->cy =  s0->cnt * s0->cy;
			writer->cy += s1->cnt * s1->cy;
			writer->cy += s2->cnt * s2->cy;
			writer->cy += s3->cnt * s3->cy;
			const float scl = writer->cnt ? 1.0f / writer->cnt : 1.0f;
			writer->cx *= scl;
			writer->cy *= scl;
			highcnt = writer->cnt > highcnt ? writer->cnt : highcnt;
			reader += 2;
			writer += 1;
		}
		reader += dres;
	}
	return highcnt;
}


static void make_aggregates( void )
{
	TT_SCOPE( "make_aggregates" );
	for ( int i=1; i<=NUMDIMS; ++i )
		if ( i==1 )
			aggregate_cells();
		else
		{
			const int hi = aggregate_level( i );
			(void) hi;
			//LOGI( "Highest star count at level %d: %d", i, hi );
		}
}


void enumerate_contributors( int level, int x, int y, int cx, int cy )
{
	const int cell_size = cell_sizes[ level ];

	const int req_dist = req_distances[ level ];

	const int xx = cell_size * x;
	const int yy = cell_size * y;

	const bool skip_refining =
		xx >= cx + req_dist ||
		xx + cell_size - 1 <= cx - req_dist ||
		yy >= cy + req_dist ||
		yy + cell_size - 1 <= cy - req_dist;

	contribinfo_t& contrib = contribs[ cx ][ cy ];
	if ( skip_refining )
	{
		int& totalcount = contrib.totalcount;
		contrib.mixedcoords[ totalcount++ ] = ENCODECONTRIB( level, x, y );
	}
	else
	{
		// Grid cells at level 2 and higher may require breaking up into four.
		if ( level > 1 )
		{
			enumerate_contributors( level-1, 2*x+0, 2*y+0, cx, cy );
			enumerate_contributors( level-1, 2*x+1, 2*y+0, cx, cy );
			enumerate_contributors( level-1, 2*x+1, 2*y+1, cx, cy );
			enumerate_contributors( level-1, 2*x+0, 2*y+1, cx, cy );
		}
		else
		{
			// we are at single-cell level, we need to do the full solution, computing per-star.
			int& totalcount = contrib.totalcount;
			contrib.mixedcoords[ totalcount++ ] = ENCODECONTRIB( 0, x, y );
		}
	}
}


void stars_calculate_contribution_info( void )
{
	TT_SCOPE( "calc contribution" );
	for ( int cx=0; cx<GRIDRES; ++cx )
		for ( int cy=0; cy<GRIDRES; ++cy )
		{
			contribinfo_t& contrib = contribs[ cx ][ cy ];
			const int toplvl = NUMDIMS;
			const int topres = grid_resolutions[ toplvl ];
			for ( int x=0; x<topres; ++x )
				for ( int y=0; y<topres; ++y )
					enumerate_contributors( toplvl, x, y, cx, cy );
			//LOGI( "Total nr of contributions for cell %d,%d: %d", cx, cy, contrib.totalcount );
			int numwritten = 0;
			int sumcount = 0;
			for ( int l=0; l<=NUMDIMS; ++l )
			{
				const int res = grid_resolutions[ l ];
				int& count = contrib.counts[ l ];
				for ( int i=0; i<contrib.totalcount; ++i )
				{
					const int code = contrib.mixedcoords[ i ];
					const int x   = ( code >>  0 ) & 0xff;
					const int y   = ( code >>  8 ) & 0xff;
					const int lvl = ( code >> 16 ) & 0xff;
					ASSERT( lvl >= 0 && lvl <= NUMDIMS );
					if ( lvl == l )
					{
						ASSERTM( x >= 0 && x < res, "x,y %d,%d not in range 0..%d", x, y, res );
						ASSERTM( y >= 0 && y < res, "x,y %d,%d not in range 0..%d", x, y, res );
						contrib.sortedcoords[ numwritten++ ] = code;
						count++;
					}
				}
				sumcount += count;
			}
			ASSERT( numwritten == contrib.totalcount );
			ASSERT( sumcount == contrib.totalcount );
		}
}


#define MAXSOURCES	( MAXCONTRIBS + 8 * CELLCAP )
void cell_update( int cx, int cy, float dt )
{
	TT_SCOPE( "cell_update" );
	cell_t& cell = cells[ cx ][ cy ];
	const int cnt = cell.cnt;
	if ( !cnt ) return;
	float qx[ CELLCAP ];	// new x-coords.
	float qy[ CELLCAP ];	// new y-coords.

	ALIGNEDPRE float src_x  [ MAXSOURCES ] ALIGNEDPST;
	ALIGNEDPRE float src_y  [ MAXSOURCES ] ALIGNEDPST;
	ALIGNEDPRE float src_scl[ MAXSOURCES ] ALIGNEDPST;

	TT_BEGIN( "gather contribs" );
	// Find all the sources that generate gravity for this cell (individual stars, and aggregates.)
	const contribinfo_t& contrib = contribs[ cx ][ cy ];
	int reader = 0;
	int numsrc = 0;
	// level 0: individual stars
	const int count0 = contrib.counts[0];
	for ( int i=0; i<count0; ++i )
	{
		const int code = contrib.sortedcoords[ reader++ ];
		const int x = ( code >> 0 ) & 0xff;
		const int y = ( code >> 8 ) & 0xff;
		const cell_t& other = cells[ x ][ y ];
		for ( int j=0; j<other.cnt; ++j )
		{
			src_x  [ numsrc ] = other.px[ j ];
			src_y  [ numsrc ] = other.py[ j ];
			src_scl[ numsrc ] = 1;
			numsrc++;
		}
	}
	// level [1..NUMDIMS] (inclusive) are aggregates.
	for ( int level=1; level<=NUMDIMS; ++level )
	{
		const int countn = contrib.counts[ level ];
		const int res = grid_resolutions[ level ];
		for ( int i=0; i<countn; ++i )
		{
			const int code = contrib.sortedcoords[ reader++ ];
			const int x = ( code >> 0 ) & 0xff;
			const int y = ( code >> 8 ) & 0xff;
			aggregate_t& ag = aggregates[ level ][ x * res + y ];
			src_x  [ numsrc ] = ag.cx;
			src_y  [ numsrc ] = ag.cy;
			src_scl[ numsrc ] = ag.cnt;
			if ( ag.cnt )
				numsrc++;
			ASSERT( numsrc <= MAXSOURCES );
		}
	}
	//LOGI( "For cx,cy %d,%d: numsrc: %d", cx, cy, numsrc );
#if VECTORIZE
	// Make it an even nr of batches.
	while ( numsrc & 0xf )
		src_scl[ numsrc++ ] = 0;
	const int numbatches = numsrc/8;
#endif
	TT_END( "gather contribs" );

	// Traverse the stars in this cell, and sum all forces on it.

	TT_BEGIN( "Compute forces" );

	for ( int i=0; i<cnt; ++i )
	{
		float ax = 0.0f;
		float ay = 0.0f;

		const float curx = cell.px[i];
		const float cury = cell.py[i];

		const __m256 curx8 = _mm256_set1_ps( curx );
		const __m256 cury8 = _mm256_set1_ps( cury );
		const __m256 G8    = _mm256_set1_ps( G );

#if VECTORIZE
		__m256 forcex8 = _mm256_setzero_ps();	// all batches accumulate in these.
		__m256 forcey8 = _mm256_setzero_ps();
		for ( int batch=0; batch<numbatches; ++batch )
		{
			const __m256 x8   = _mm256_load_ps( src_x + 8*batch );
			const __m256 y8   = _mm256_load_ps( src_y + 8*batch );
			const __m256 scl8 = _mm256_load_ps( src_scl + 8*batch );
			const __m256 dx8  = _mm256_sub_ps ( x8, curx8 );
			const __m256 dy8  = _mm256_sub_ps ( y8, cury8 );
			const __m256 dsqr8 = _mm256_add_ps
			(
			 	_mm256_mul_ps( dx8, dx8 ),
				_mm256_mul_ps( dy8, dy8 )
			);
			__m256 idist8  = _mm256_rsqrt_ps( dsqr8 );
			idist8 = _mm256_min_ps( idist8, _mm256_set1_ps( 100.0 ) );
			__m256 denom8 = _mm256_mul_ps( _mm256_mul_ps( idist8, idist8 ), idist8 );
			__m256 numer8 = _mm256_mul_ps( scl8, G8 );
			__m256 magn8  = _mm256_mul_ps( numer8, denom8 );
			__m256 addx8  = _mm256_mul_ps( magn8, dx8 );
			__m256 addy8  = _mm256_mul_ps( magn8, dy8 );
			forcex8 = _mm256_add_ps( forcex8, addx8 );
			forcey8 = _mm256_add_ps( forcey8, addy8 );
		}
		// Now we need to sum all 8 lanes in the force vector.
		__m256 sumx8 = _mm256_hadd_ps( forcex8, forcex8 );
		__m256 sumy8 = _mm256_hadd_ps( forcey8, forcey8 );
		sumx8 = _mm256_hadd_ps( sumx8, sumx8 );
		sumy8 = _mm256_hadd_ps( sumy8, sumy8 );
		const __m128 lox4 = _mm256_extractf128_ps( sumx8, 0x00 );
		const __m128 hix4 = _mm256_extractf128_ps( sumx8, 0xff );
		const __m128 loy4 = _mm256_extractf128_ps( sumy8, 0x00 );
		const __m128 hiy4 = _mm256_extractf128_ps( sumy8, 0xff );
		ax += _mm_cvtss_f32( _mm_add_ps( lox4, hix4 ) );	// finally use the scalar float for x.
		ay += _mm_cvtss_f32( _mm_add_ps( loy4, hiy4 ) );	// finally use the scalar float for y.
#else
		for ( int s=0; s<numsrc; ++s )
		{
			const float dx =  src_x[s] - curx;
			const float dy =  src_y[s] - cury;
			const float scl = src_scl[s];
			const float dsqr = dx*dx + dy*dy;
			float dist = sqrtf( dsqr );
			dist = dist < 1e-2 ? 1e-2 : dist;
			const float magn = ( src_scl[s] * G ) / ( dist*dist*dist );
			ax += magn * dx;
			ay += magn * dy;
		}
#endif

		// apply forces to change velocity.
		cell.vx[i] += ax * dt;
		cell.vy[i] += ay * dt;
		// apply velocity to change position.
		qx[i] = cell.px[i] + cell.vx[i] * dt;
		qy[i] = cell.py[i] + cell.vy[i] * dt;
		// see if we transitioned into another cell.
		if ( qx[i] < cell.xrng[0] ) ST_SET_CROSSED_LO_X( cell.st[i] );
		if ( qx[i] > cell.xrng[1] ) ST_SET_CROSSED_HI_X( cell.st[i] );
		if ( qy[i] < cell.yrng[0] ) ST_SET_CROSSED_LO_Y( cell.st[i] );
		if ( qy[i] > cell.yrng[1] ) ST_SET_CROSSED_HI_Y( cell.st[i] );
	}
	TT_END( "Compute forces" );

	memcpy( cell.px, qx, cnt*sizeof(float) );
	memcpy( cell.py, qy, cnt*sizeof(float) );
}


void stars_update( float dt )
{
	make_aggregates();

	// Update position and velocity of stars in cells.
	for ( int cx=0; cx<GRIDRES; ++cx )
		for ( int cy=0; cy<GRIDRES; ++cy )
			cell_update( cx, cy, dt );

	const int MAXTRANSITS = NUMSTARS/20;
	float px[ MAXTRANSITS ];
	float py[ MAXTRANSITS ];
	float vx[ MAXTRANSITS ];
	float vy[ MAXTRANSITS ];
	int numtransits = 0;

	// Remove stars that went out of cell bounds.
	for ( int cx=0; cx<GRIDRES; ++cx )
		for ( int cy=0; cy<GRIDRES; ++cy )
		{
			cell_t& cell = cells[ cx ][ cy ];
			int cnt = cell.cnt;
			for ( int i=cnt-1; i>=0; --i )
			{	
				if ( ( cell.st[i] & 0xf ) != 0 )
				{
					int j = numtransits++;
					ASSERT( j < MAXTRANSITS );
					px[j] = cell.px[i];
					py[j] = cell.py[i];
					vx[j] = cell.vx[i];
					vy[j] = cell.vy[i];
					remove_from_cell( i, cx, cy );
				}
			}
		}

	// Add transitionary stars back into the grid, in a new cell.
	//LOGI( "Num transits: %d", numtransits );
	for ( int i=0; i<numtransits; ++i )
	{
		add_star( px[i], py[i], vx[i], vy[i] );
	}
}


void stars_draw_grid( void )
{
	static int colourUniform = glpr_uniform( "colour" );
	float v = cam_scl / 0.50f;
	glUniform4f( colourUniform, v, v, 0, 1 );

	const int totalv = GRIDRES * GRIDRES * 4 * 2;

	float scratchbuf[ totalv ][ 2 ];
	
	int writer = 0;
	for ( int cx=0; cx<GRIDRES; ++cx )
		for ( int cy=0; cy<GRIDRES; ++cy )
		{
			const cell_t& cell = cells[ cx ][ cy ];
			const float x0 = cell.xrng[0];
			const float x1 = cell.xrng[1];
			const float y0 = cell.yrng[0];
			const float y1 = cell.yrng[1];

			scratchbuf[ writer ][ 0 ] = x0; scratchbuf[ writer ][ 1 ] = y0; ++writer;
			scratchbuf[ writer ][ 0 ] = x1; scratchbuf[ writer ][ 1 ] = y0; ++writer;

			scratchbuf[ writer ][ 0 ] = x1; scratchbuf[ writer ][ 1 ] = y0; ++writer;
			scratchbuf[ writer ][ 0 ] = x1; scratchbuf[ writer ][ 1 ] = y1; ++writer;

			scratchbuf[ writer ][ 0 ] = x1; scratchbuf[ writer ][ 1 ] = y1; ++writer;
			scratchbuf[ writer ][ 0 ] = x0; scratchbuf[ writer ][ 1 ] = y1; ++writer;

			scratchbuf[ writer ][ 0 ] = x0; scratchbuf[ writer ][ 1 ] = y1; ++writer;
			scratchbuf[ writer ][ 0 ] = x0; scratchbuf[ writer ][ 1 ] = y0; ++writer;
		}

	ASSERT( writer == totalv );

	GLuint vbo=0;
	GLuint vao=0;
	glGenVertexArrays( 1, &vao );
	glBindVertexArray( vao );
	glGenBuffers( 1, &vbo );
	glBindBuffer( GL_ARRAY_BUFFER, vbo );
	glBufferData( GL_ARRAY_BUFFER, totalv*2*sizeof(float), (void*)scratchbuf, GL_STREAM_DRAW );
	CHECK_OGL
	glVertexAttribPointer( ATTRIB_VERTEX, 2, GL_FLOAT, 0, 2 * sizeof(float), (void*) 0 );
	CHECK_OGL
	glEnableVertexAttribArray( ATTRIB_VERTEX );
	CHECK_OGL

	glDrawArrays( GL_LINES, 0, totalv );
	CHECK_OGL

	glBindVertexArray( 0 );
	glDeleteVertexArrays( 1, &vao );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glDeleteBuffers( 1, &vbo );
	CHECK_OGL
}


void stars_draw_field( void )
{
	static int colourUniform = glpr_uniform( "colour" );

	float v = cam_scl / 0.20f;
	glUniform4f( colourUniform, v,v,v,v );

	int totalv = 0;
	for ( int cx=0; cx<GRIDRES; ++cx )
		for ( int cy=0; cy<GRIDRES; ++cy )
		{
			cell_t& cell = cells[ cx ][ cy ];
			totalv += cell.cnt;
		}

	if ( !totalv )
		return;

	int writer = 0;
	for ( int cx=0; cx<GRIDRES; ++cx )
		for ( int cy=0; cy<GRIDRES; ++cy )
		{
			cell_t& cell = cells[ cx ][ cy ];
			for ( int i=0; i<cell.cnt; ++i )
			{
				vdata.displacements[ writer ][ 0 ] = cell.px[ i ];
				vdata.displacements[ writer ][ 1 ] = cell.py[ i ];
				writer += 1;
			}
		}

	ASSERT( writer == totalv );

	GLuint vbooff = (GLuint) ( sizeof( vdata.circle ) );
	GLuint vbosz  = (GLuint) ( vbooff + totalv * 2 * sizeof( float ) );

	GLuint vbo=0;
	GLuint vao=0;
	glGenVertexArrays( 1, &vao );
	glBindVertexArray( vao );
	glGenBuffers( 1, &vbo );
	glBindBuffer( GL_ARRAY_BUFFER, vbo );
	glBufferData( GL_ARRAY_BUFFER, vbosz, (void*) &vdata, GL_STREAM_DRAW );
	CHECK_OGL
	glVertexAttribPointer( ATTRIB_VERTEX, 2, GL_FLOAT, 0, 2 * sizeof(float), (void*) 0 );
	CHECK_OGL
	glVertexAttribDivisor( ATTRIB_DISPLACEMENT, 1 );
	glVertexAttribPointer( ATTRIB_DISPLACEMENT, 2, GL_FLOAT, 0, 2 * sizeof(float), (void*) (size_t) vbooff );
	CHECK_OGL
	glEnableVertexAttribArray( ATTRIB_VERTEX );
	CHECK_OGL
	glEnableVertexAttribArray( ATTRIB_DISPLACEMENT );
	CHECK_OGL

	//glDrawArrays( GL_POINTS, 0, totalv );
	glDrawArraysInstanced( GL_TRIANGLES, 0, circle_sz * 3, totalv );
	CHECK_OGL

	glBindVertexArray( 0 );
	glDeleteVertexArrays( 1, &vao );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glDeleteBuffers( 1, &vbo );
	CHECK_OGL
}

