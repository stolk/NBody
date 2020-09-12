
#define	GRIDRES		64	//! Grid resolution.

#define	CELL2POS( C )	( (float) ( C - (GRIDRES-1)/2.0f ) )

#define POS2CELL( P )	( (int) floorf( P + GRIDRES/2 ) )

