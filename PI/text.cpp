#include "text.h"

extern "C" 
{
#include "dblunt.h"
}

#include "vmath.h"
#include "checkogl.h"
#include "rendercontext.h"
#include "logx.h"


#define SCRATCHBUFSZ	(36*1024)
static float scratchbuf[ SCRATCHBUFSZ ];

void text_draw_string
(
	const char* str,
	vec3_t pos,
	vec3_t sz,
	const char* halign,
	const char* valign,
	float fadestart
)
{
	// Convert the ASCII text to a renderable format (2D triangles.)
	float textw=-1;
	float texth=-1;
	const int numtria = dblunt_string_to_vertices
	(
		str,
		scratchbuf,
		sizeof(scratchbuf),
		pos[0], pos[1],
		sz[0], sz[1],
		fadestart,
		&textw,
		&texth
	);

	const int floats_per_vertex = fadestart >= 0 ? 3 : 2;

	float shiftx = 0.0f;
	float shifty = 0.0f;

	if ( !strcmp( valign, "center" ) )
		shifty = 0.5f * texth;
	if ( !strcmp( valign, "bottom" ) )
		shifty = 1.0f * texth;

	if ( !strcmp( halign, "center" ) )
		shiftx = -0.5f * textw;
	if ( !strcmp( halign, "right" ) )
		shiftx = -1.0f * textw;

	for ( int i=0; i<numtria*3; ++i )
	{
		scratchbuf[floats_per_vertex*i+0] += shiftx;
		scratchbuf[floats_per_vertex*i+1] += shifty;
	}

	// Now we have a whole bunch of triangle vertices that we need to put into a VBO.
	const int numv = numtria * 3;
	GLuint vbo=0;
#ifdef USE_VAO
	GLuint vao=0;
	glGenVertexArrays( 1, &vao );
	glBindVertexArray( vao );
#endif
	glGenBuffers( 1, &vbo );
	glBindBuffer( GL_ARRAY_BUFFER, vbo );
	glBufferData( GL_ARRAY_BUFFER, numv*floats_per_vertex*sizeof(float), (void*)scratchbuf, GL_STREAM_DRAW );
	CHECK_OGL
	const size_t stride = floats_per_vertex * sizeof(float);
	glVertexAttribPointer( ATTRIB_VERTEX, 2, GL_FLOAT, 0, (GLsizei)stride, (void*) 0 /* offset in vbo */ );
	glEnableVertexAttribArray( ATTRIB_VERTEX );
	if ( fadestart >= 0 )
	{
		glVertexAttribPointer( ATTRIB_OPACIT, 1, GL_FLOAT, 0, (GLsizei)stride, (void*) (2*sizeof(float)) /* offset in vbo */ );
		glEnableVertexAttribArray( ATTRIB_OPACIT );
	}
	CHECK_OGL

	// The VBO has been constructed, so we can draw the triangles.
	glDrawArrays( GL_TRIANGLES, 0, numv );
	CHECK_OGL

#ifdef USE_VAO
	glBindVertexArray( 0 );
	glDeleteVertexArrays( 1, &vao );
	CHECK_OGL
#endif
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glDeleteBuffers( 1, &vbo );
	CHECK_OGL
}

