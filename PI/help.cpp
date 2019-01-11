#include "help.h"

// GBase
#include "glpr.h"
#include "text.h"

#define NUML	14
const char* keys[NUML][2] =
{
	"F1",		"Toggle Help.",
	"F2",		"Spawn Demo.",
	"L-Drag",	"Spawn Stars.",
	"R-Drag",	"Move Camera.",
	"Scroll",	"Zoom in/out.",
	"ALT+Drag",	"Spawn Orbitting Stars.",
	"B",		"Toggle Black Hole.",
	"L-Click",	"Select Star.",
	"P",		"Show Aggregates.",
	"~",		"Toggle Grid.",
	"PgUp",		"Increase Particle Size.",
	"PgDn",		"Decrease Particle Size.",
	"1..5",		"Set Brush Size.",
	"C",		"Clear Stars.",
};


void help_draw( void )
{
	for ( int i=0; i<NUML; ++i )
	{
		const char* key = keys[i][0];
		const char* fun = keys[i][1];
		const float th = 0.08f;
		const float tw = 0.04f;

		text_draw_string( key, vec3_t(-0.68f,0.9f-i*0.14f,0), vec3_t(tw,th,0), "right", "center", -1 );
		text_draw_string( fun, vec3_t(-0.58f,0.9f-i*0.14f,0), vec3_t(tw,th,0), "left",  "center", -1 );
	}
}

