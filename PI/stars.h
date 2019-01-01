#define NUMSTARS	15000	//! Total number of stars.
#define	GRIDRES		64	//! Grid resolution.
#define CELLCAP		1200	//! Max stars per cell.

#define ST_CROSSED_LO_X		(1<<0)
#define ST_CROSSED_HI_X		(1<<1)
#define ST_CROSSED_LO_Y		(1<<2)
#define ST_CROSSED_HI_Y		(1<<3)

#define ST_IS_SET( ST, B ) \
	( ( ST & B ) != 0 )

#define ST_IS_CROSSED_LO_X( ST )	ST_IS_SET( ST, ST_CROSSED_LO_X )
#define ST_IS_CROSSED_HI_X( ST )	ST_IS_SET( ST, ST_CROSSED_HI_X )
#define ST_IS_CROSSED_LO_Y( ST )	ST_IS_SET( ST, ST_CROSSED_LO_Y )
#define ST_IS_CROSSED_HI_Y( ST )	ST_IS_SET( ST, ST_CROSSED_HI_Y )

#define ST_SET_CROSSED_LO_X( ST )	ST |= ST_CROSSED_LO_X
#define ST_SET_CROSSED_HI_X( ST )	ST |= ST_CROSSED_HI_X
#define ST_SET_CROSSED_LO_Y( ST )	ST |= ST_CROSSED_LO_Y
#define ST_SET_CROSSED_HI_Y( ST )	ST |= ST_CROSSED_HI_Y

#define ST_CLR_CROSSED_LO_X( ST )	ST &= ~ST_CROSSED_LO_X
#define ST_CLR_CROSSED_HI_X( ST )	ST &= ~ST_CROSSED_HI_X
#define ST_CLR_CROSSED_LO_Y( ST )	ST &= ~ST_CROSSED_LO_Y
#define ST_CLR_CROSSED_HI_Y( ST )	ST &= ~ST_CROSSED_HI_Y

typedef struct
{
	float px[ CELLCAP ];	//! x coordinates of all the stars in this cell.
	float py[ CELLCAP ];	//! y coordinates of all the stars in this cell.
	float vx[ CELLCAP ];	//! velocities, x component.
	float vy[ CELLCAP ];	//! velocities, y component.
	int   st[ CELLCAP ];	//! status bits for each star.
	float xrng[2];		//! cell's low and high x.
	float yrng[2];		//! cell's low and high y;
	int cnt;		//! number of stars in this cell.
	float cx;		//! center of mass for cell, x component.
	float cy;		//! center of mass for cell, y component.
} cell_t;


typedef struct
{
	int cnt;
	float cx;
	float cy;
	float rngx[2];
	float rngy[2];
} aggregate_t;

#define NUMDIMS 5

#define AGG0RES	GRIDRES
#define AGG1RES (AGG0RES/2)
#define AGG2RES (AGG1RES/2)
#define AGG3RES (AGG2RES/2)
#define AGG4RES (AGG3RES/2)

const int grid_resolutions[ 1+NUMDIMS ] =
{
	GRIDRES,	// cell level: individual stars.
	AGG0RES,	// 1 cell aggregate.
	AGG1RES,	// 2x2 cell aggregate.
	AGG2RES,	// 4x4 cell aggregate.
	AGG3RES,	// 8x8 cell aggregate.
	AGG4RES,	// 16x16 cell aggregate.
};

const int cell_sizes[ 1+NUMDIMS ] =
{
	1,	// level 0: individual stars.
	1,	// level 1: cell aggregate.
	2,	// level 2: 2x2
	4,	// level 3: 4x4
	8,	// level 4: 8x8
	16,	// level 5: 16x16
};

const int req_distances[ 1+NUMDIMS ] =
{
	0,
	2,
	2*2,
	2*2*2,
	2*2*2*2,
	2*2*2*2*2,
};

#define	CELL2POS( C )	( (float) ( C - (GRIDRES-1)/2.0f ) )

#define POS2CELL( P )	( (int) floorf( P + GRIDRES/2 ) )


extern void stars_create( void );

extern void stars_calculate_contribution_info( void );

extern void stars_update( float dt );

extern void stars_draw_grid( void );

extern void stars_draw_field( void );

