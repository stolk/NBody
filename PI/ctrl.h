// ctrl.h
//
// Controls the OpenGL view.
//
// (c)2017 Abraham Stolk

#ifndef CTRL_H
#define CTRL_H

extern const char* ctrl_filesPath;

extern const char* ctrl_configPath;

extern int ctrl_fullScreen;

extern bool ctrl_create( int w, int h, float sf=1.0f, const char* format="desktop" ); // Done repeatedly, whenever Android pushes our app to foreground

extern void ctrl_exit( void ); // At program exit.

extern void ctrl_resize( int w, int h );

extern void ctrl_pick( float x, float y, float z, int clicked );

extern void ctrl_mousebuttonreleased( void );

extern void ctrl_destroy( void );

extern void ctrl_simulate( void );

extern const char* ctrl_drawFrame( void );

extern void ctrl_drawShadow( void );

extern bool ctrl_onBack( void );

extern void ctrl_pause( void );

extern void ctrl_enableBuy( bool enabled );

extern void ctrl_enablePremium( bool enabled );

extern bool ctrl_setSNH( int i );

extern void ctrl_set_googleplay_status( bool signedin );

extern int ctrl_level;

extern bool ctrl_signedin;

extern bool ctrl_buyRequested;
extern bool ctrl_leaderboardRequested;
extern bool ctrl_achievementRequested;
extern bool ctrl_signinoutRequested;
extern bool ctrl_paused;


// internal

extern bool ctrl_draw_create( void );

extern void ctrl_draw_destroy( void );

extern int surfaceW;
extern int surfaceH;

extern float csf;

extern float invaspect;

#endif

