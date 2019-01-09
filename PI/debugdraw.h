// Debug Drawing

#ifndef DEBUGDRAW_H
#define DEBUGDRAW_H

extern void debugdraw_init( void );

extern void debugdraw_clear( void );

extern void debugdraw_line( float x0, float y0, float x1, float y1 );

extern void debugdraw_rect( float x0, float y0, float x1, float y1 );

extern void debugdraw_diamond( float x, float y, float sz );

extern void debugdraw_draw( void );

extern void debugdraw_crosshairs( float px, float py, float sz );

extern void debugdraw_triangle( float px, float py, float sz );

extern void debugdraw_arrow( float x0, float y0, float x1, float y1 );

extern void debugdraw_angle( float px, float py, float dx, float dy, float ang, float sz );

extern bool debugdraw_fat_arrow( float frx, float fry, float tox, float toy, float width );

#endif

