//  debugdraw.cpp
//
//  (c)2012-2017 Abraham Stolk


#include "debugdraw.h"

#include "logx.h"
#include "checkogl.h"
#include "glpr.h"


static const int maxv = 32768;
static int numv = 0;
static float vdata[ maxv ][ 2 ];

void debugdraw_init(void)
{
	debugdraw_clear();
}


void debugdraw_clear(void)
{
	numv = 0;
}


void debugdraw_line( float frx, float fry, float tox, float toy )
{
	if ( numv < maxv )
	{
		vdata[ numv ][ 0 ] = frx;
		vdata[ numv ][ 1 ] = fry;
		++numv;
		vdata[ numv ][ 0 ] = tox;
		vdata[ numv ][ 1 ] = toy;
		++numv;
	}
}


void debugdraw_rect( float x0, float y0, float x1, float y1 )
{
	debugdraw_line( x0, y0, x1, y0 );
	debugdraw_line( x1, y0, x1, y1 );
	debugdraw_line( x1, y1, x0, y1 );
	debugdraw_line( x0, y1, x0, y0 );
}


void debugdraw_crosshairs( float px, float py, float sz )
{
	debugdraw_line( px-sz, py, px+sz, py );
	debugdraw_line( px, py-sz, px, py+sz );
}


void debugdraw_diamond( float x, float y, float sz )
{
	const float offs[4][2] =
	{
		{ sz,0 },
		{ 0,sz },
		{ -sz,0 },
		{ 0,-sz },
	};
	for ( int i=0; i<4; ++i )
	{
		const float* off0 = offs[i];
		const float* off1 = offs[(i+1)%4];
		debugdraw_line( x + off0[0], y + off0[1], x + off1[0], y + off1[1] );
	}
}


void debugdraw_arrow( float frx, float fry, float tox, float toy )
{
	const float d[2]  = { tox-frx, toy-fry };
	const float t[2]  = { d[1], d[0] };
	const float q[2]  = { frx + d[0]*0.9f, fry + d[1]*0.9f };
	const float p0[2] = { q[0] + t[0]*0.1f, q[1] + t[1]*0.1f };
	const float p1[2] = { q[0] + t[0]*-0.1f, q[1] + t[1]*-0.1f };
	debugdraw_line( frx, fry, tox, toy );
	debugdraw_line( p0[0], p0[1], tox, toy );
	debugdraw_line( p1[0], p1[1], tox, toy );
}


void debugdraw_draw( void )
{
	if ( !numv ) return;

	GLuint vao=0;
	GLuint vbo=0;
	glGenVertexArrays( 1, &vao );
	ASSERT( vao > 0 );
	glBindVertexArray( vao );

	glGenBuffers( 1, &vbo );
	ASSERT( vbo > 0 );
	CHECK_OGL

	glBindBuffer( GL_ARRAY_BUFFER, vbo );
	glBufferData( GL_ARRAY_BUFFER, numv*2*sizeof(float), (void*)vdata, GL_STREAM_DRAW );
	CHECK_OGL
	glVertexAttribPointer( ATTRIB_VERTEX, 2, GL_FLOAT, 0, 2 * sizeof(float), (void*) 0 /* offset in vbo */ );
	CHECK_OGL
	glEnableVertexAttribArray( ATTRIB_VERTEX );
	CHECK_OGL

	glDrawArrays( GL_LINES, 0, numv );

	glBindVertexArray( 0 );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	CHECK_OGL

	glDeleteBuffers( 1, &vbo );
	glDeleteVertexArrays( 1, &vao );
	CHECK_OGL
}


