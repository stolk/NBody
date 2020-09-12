#define KEYMAP_NUMFUNCS	5


#define KEY_FW		keymap[0]
#define KEY_BW		keymap[1]
#define KEY_LEFT	keymap[2]
#define KEY_RIGHT	keymap[3]
#define KEY_FIRE	keymap[4]

extern int keymap[ KEYMAP_NUMFUNCS ];

extern int keymap_load( const char* filespath );

extern int keymap_store( const char* filespath );

extern const char* keyfunctions[ KEYMAP_NUMFUNCS ];

