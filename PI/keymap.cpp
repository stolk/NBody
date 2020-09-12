#include "keymap.h"
#include "nfy.h"
#include "logx.h"

#include <stdio.h>
#include <string.h>

static char data[1024];


int keymap[ KEYMAP_NUMFUNCS ] =
{
	// TANK FWD
	0x40000052,
	// TANK BWD
	0x40000051,
	// TANK LEFT
	0x40000050,
	// TANK RIGHT
	0x4000004F,
	// FIRE
	32,
};


const char* keyfunctions[ KEYMAP_NUMFUNCS ] =
{
	"tank_forward",
	"tank_backward",
	"tank_left",
	"tank_right",
	"fire",
};


int keymap_store( const char* filespath )
{
	char fname[256];
	snprintf( fname, sizeof( fname ), "%s/keymap.txt", filespath );
	FILE* f = fopen( fname, "w" );
	if ( !f )
		return 0;
	fprintf( f, "#keymap.txt\n" );
	for ( int i=0; i<KEYMAP_NUMFUNCS; ++i )
	{
		fprintf( f, "%s=0x%02x\n", keyfunctions[ i ], keymap[ i ] );
	}
	fclose( f );
	return KEYMAP_NUMFUNCS;
}


int keymap_load( const char* filespath )
{
	char fname[256];
	snprintf( fname, sizeof( fname ), "%s/keymap.txt", filespath );
	FILE* f = fopen( fname, "r" );
	if ( !f )
		return 0;
	int numread = (int)fread( data, 1, sizeof(data)-1, f );
	if ( numread <= 1 )
		return 0;

	int numremapped = 0;
	for ( int i=0; i<KEYMAP_NUMFUNCS; ++i )
	{
		char s[80];
		nfy_str( data, keyfunctions[i], s, sizeof(s)-1 );
		LOGI( "function '%s' set to '%s'", keyfunctions[ i ], s );
		if ( strlen( s ) == 1 )
		{
			const int newsym = (int) s[0];
			if ( newsym != keymap[ i ] )
			{
				keymap[ i ] = newsym;
				LOGI( "%s remapped to %c (0x%02x)", keyfunctions[ i ], s[0], newsym );
				numremapped++;
			}
		}
		if ( strlen( s ) > 2 && s[0] == '0' && s[1] == 'x' )
		{
			int newsym=0;
			const int rv = sscanf( s, "%x", &newsym );
			if (rv != 1)
				LOGE( "failed to extract hex nr from string." );
			if ( rv == 1 && newsym != keymap[ i ] )
			{
				keymap[ i ] = newsym;
				LOGI( "%s remapped to 0x%02x", keyfunctions[ i ], keymap[ i ] );
				numremapped++;
			}
		}
	}
	return numremapped;
}

