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

	int idx=0;
	for ( int i=0; i<NUMSTARS; ++i )
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
#if 1
		const float vx = 0.0f;
		const float vy = 0.0f;
#else
		float vx = -1 + 2 * halton( idx, 5 );
		float vy = -1 + 2 * halton( idx, 7 );
		float l = sqrtf( vx*vx + vy*vy );
		vx = 0.04f * vx * (1/l);
		vy = 0.04f * vy * (1/l);
#endif
		const float scl = GRIDRES/2.4f;
		add_star( scl*px, scl*py, vx, vy );
	}

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


void cell_update_centre_of_mass( int cx, int cy )
{
	cell_t& cell = cells[ cx ][ cy ];
	const int cnt = cell.cnt;
	if ( !cnt )
	{
		cell.cx = ( cell.xrng[0] + cell.xrng[1] ) / 2;
		cell.cy = ( cell.yrng[0] + cell.yrng[1] ) / 2;
		cell.lx = cell.hx = cell.cx;
		cell.ly = cell.hy = cell.cy;
		return;
	}
	cell.cx = 0;
	cell.cy = 0;
	cell.lx = cell.hx = cell.px[0];
	cell.ly = cell.hy = cell.py[0];
	for ( int i=0; i<cnt; ++i )
	{
		cell.cx += cell.px[i];
		cell.cy += cell.py[i];
		cell.lx = cell.px[i] < cell.lx ? cell.px[i] : cell.lx;
		cell.hx = cell.px[i] > cell.hx ? cell.px[i] : cell.hx;
		cell.ly = cell.py[i] < cell.ly ? cell.py[i] : cell.ly;
		cell.hy = cell.py[i] > cell.hy ? cell.py[i] : cell.hy;
	}
	const float EPS = 10e-6;
	ASSERTM( cell.lx >= cell.xrng[0]-EPS && cell.lx <= cell.xrng[1]+EPS, "lx %f xrng %f..%f", cell.lx, cell.xrng[0], cell.xrng[1] );
	ASSERTM( cell.hx >= cell.xrng[0]-EPS && cell.hx <= cell.xrng[1]+EPS, "hx %f xrng %f..%f", cell.hx, cell.xrng[0], cell.xrng[1] );
	ASSERTM( cell.ly >= cell.yrng[0]-EPS && cell.ly <= cell.yrng[1]+EPS, "ly %f yrng %f..%f", cell.ly, cell.yrng[0], cell.yrng[1] );
	ASSERTM( cell.hy >= cell.yrng[0]-EPS && cell.hy <= cell.yrng[1]+EPS, "hy %f yrng %f..%f", cell.hy, cell.yrng[0], cell.yrng[1] );
	cell.cx *= ( 1.0 / cell.cnt );
	cell.cy *= ( 1.0 / cell.cnt );
}

static const float G = 0.0001f;

force_t sum_forces_of_single_cell( int cx, int cy, float curx, float cury )
{
	const cell_t& cell = cells[ cx ][ cy ];
	force_t force = { 0, 0 };
	const int cnt = cell.cnt;
	for ( int i=0; i<cnt; ++i )
	{
		const float dx = cell.px[ i ] - curx;
		const float dy = cell.py[ i ] - cury;
		const float dsqr = dx*dx + dy*dy;
		if ( dsqr > 0.0f )
		{
			const float dist = sqrtf( dsqr );
			//ASSERTM( dist > 0.0f, "dx %f dy %f dsqr %f dist %f", dx, dy, dsqr, dist );
			//LOGI( "dx %f dy %f dsqr %f dist %f", dx, dy, dsqr, dist );
			const float dirx = dx / dist;
			const float diry = dy / dist;
			float f = G * 1.0f / dsqr;
			f = CLAMPED( f, 0, 1 );
			force.x += dirx * f;
			force.y += diry * f;
		}
	}
	return force;
}


force_t sum_forces_recursively( int level, int x, int y, int cx, int cy, float curx, float cury )
{
	const int res = grid_resolutions[ level ];

	const int cell_sizes[ 1+NUMDIMS ] =
	{
		1,	// level 0: individual stars.
		1,	// level 1: cell aggregate.
		2,	// level 2: 2x2
		4,	// level 3: 4x4
		8,	// level 4: 8x8
	};
	const int cell_size = cell_sizes[ level ];

	const int req_distances[ 1+NUMDIMS ] =
	{
		0,
		2,
		2*2,
		2*2*2,
		2*2*2*2,
	};
	const int req_dist = req_distances[ level ];

	const int xx = cell_size * x;
	const int yy = cell_size * y;

	if ( cx == 16 && cy == 16 )
	{
		float cpx = CELL2POS(xx) - 0.5f;
		float cpy = CELL2POS(yy) - 0.5f;
		debugdraw_rect( cpx, cpy, cpx + cell_size, cpy + cell_size );
	}

	const bool skip_refining =
		xx >= cx + req_dist ||
		xx + cell_size - 1 <= cx - req_dist ||
		yy >= cy + req_dist ||
		yy + cell_size - 1 <= cy - req_dist;

//	LOGI( "level %d x,y %d,%d cx,cy %d,%d: cell_size %d, req dist %d, skip refining: %d", level, x,y, cx,cy, cell_size, req_dist, skip_refining );

	if ( skip_refining )
	{
		// we can use the aggregate.
		const aggregate_t& ag = aggregates[ level ][ x * res + y ];
		const float dx = ag.cx - curx;
		const float dy = ag.cy - cury;
		const float dsqr = dx*dx + dy*dy;
		if ( ag.cnt && dsqr > 0 )
		{
			const float dist = sqrtf( dsqr );
			const float dirx = dx / dist;
			const float diry = dy / dist;
			float f = G * 1.0f / dsqr;
			f = CLAMPED( f, 0, 1 );
			f *= ag.cnt;
			force_t force = { dirx * f, diry * f };
			if ( cx == 16 && cy == 16 )
			{
				float mx = CELL2POS(xx) - 0.5f + 0.5f * cell_size;
				float my = CELL2POS(yy) - 0.5f + 0.5f * cell_size;
				debugdraw_arrow( mx, my, mx + 10000*force.x, my + 10000*force.y );
			}

			return force;
		}
		else
		{
			force_t force = {0,0};
			return force;
		}
	}
	else
	{
		// Grid cells at level 2 and higher may require breaking up into four.
		if ( level > 1 )
		{
			const force_t f0 = sum_forces_recursively( level-1, 2*x+0, 2*y+0, cx, cy, curx, cury );
			const force_t f1 = sum_forces_recursively( level-1, 2*x+1, 2*y+0, cx, cy, curx, cury );
			const force_t f2 = sum_forces_recursively( level-1, 2*x+1, 2*y+1, cx, cy, curx, cury );
			const force_t f3 = sum_forces_recursively( level-1, 2*x+0, 2*y+1, cx, cy, curx, cury );
			force_t force = { f0.x + f1.x + f2.x + f3.x, f0.y + f1.y + f2.y + f3.y };
			return force;
		}
		else
		{
			// we are at single-cell level, we need to do the full solution, computing per-star.
			const force_t force = sum_forces_of_single_cell( cx, cy, curx, cury );
			return force;
		}
	}
}


void cell_update( int cx, int cy, float dt )
{
	TT_SCOPE( "cell_update" );
	cell_t& cell = cells[ cx ][ cy ];
	const int cnt = cell.cnt;
	if ( !cnt ) return;
	float qx[ CELLCAP ];	// new x-coords.
	float qy[ CELLCAP ];	// new y-coords.
	for ( int i=0; i<cnt; ++i )
	{
		float ax = 0.0f;
		float ay = 0.0f;

		const float curx = cell.px[i];
		const float cury = cell.py[i];

		// Sum all forces from top level grid.
		const int res = grid_resolutions[ NUMDIMS ];
		for ( int xx=0; xx<res; ++xx )
			for ( int yy=0; yy<res; ++yy )
			{
				force_t force = sum_forces_recursively( NUMDIMS, xx, yy, cx, cy, curx, cury );
				ax += force.x;
				ay += force.y;
			}

		//if ( i==0 ) LOGI( "cx,cy %d,%d star0 has ax,ay %f,%f", cx, cy, ax, ay );

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
	memcpy( cell.px, qx, cnt*sizeof(float) );
	memcpy( cell.py, qy, cnt*sizeof(float) );
}


void stars_update( float dt )
{
	make_aggregates();

	// Calculate centre of mass for each cell.
	for ( int cx=0; cx<GRIDRES; ++cx )
		for ( int cy=0; cy<GRIDRES; ++cy )
			cell_update_centre_of_mass( cx, cy );

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

