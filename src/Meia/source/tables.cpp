#include "tables.h"

namespace nsVDMPEGTables {
	// red:		[-223, 481]
	// green:	[-172, 432]
	// blue:	[-277, 534]
	//
	// So we need a clip table: [-277, 534].  Let's make it [-288, 543].
	//
	//	This is why Java sucks: it doesn't have a preprocessor.

	#define CLIP_TABLE_16R(v) v,v,v,v,v,v,v,v,v,v,v,v,v,v,v,v
	#define CLIP_TABLE_64R(v) CLIP_TABLE_16R(v),CLIP_TABLE_16R(v),CLIP_TABLE_16R(v),CLIP_TABLE_16R(v)
	#define CLIP_TABLE_16U(x) (x+0),(x+1),(x+2),(x+3),(x+4),(x+5),(x+6),(x+7),(x+8),(x+9),(x+10),(x+11),(x+12),(x+13),(x+14),(x+15)

	const unsigned char clip_table[832]={
		CLIP_TABLE_16R(0),		//	[-288, -273]
		CLIP_TABLE_16R(0),		//	[-272, -257]
		CLIP_TABLE_64R(0),		//	[-256, -193]
		CLIP_TABLE_64R(0),		//	[-192, -129]
		CLIP_TABLE_64R(0),		//	[-128,  -65]
		CLIP_TABLE_64R(0),		//	[- 64,   -1]
		CLIP_TABLE_16U(0x00),
		CLIP_TABLE_16U(0x10),
		CLIP_TABLE_16U(0x20),
		CLIP_TABLE_16U(0x30),
		CLIP_TABLE_16U(0x40),
		CLIP_TABLE_16U(0x50),
		CLIP_TABLE_16U(0x60),
		CLIP_TABLE_16U(0x70),
		CLIP_TABLE_16U(0x80),
		CLIP_TABLE_16U(0x90),
		CLIP_TABLE_16U(0xA0),
		CLIP_TABLE_16U(0xB0),
		CLIP_TABLE_16U(0xC0),
		CLIP_TABLE_16U(0xD0),
		CLIP_TABLE_16U(0xE0),
		CLIP_TABLE_16U(0xF0),
		CLIP_TABLE_64R(255),	//	[256, 319]
		CLIP_TABLE_64R(255),	//	[320, 383]
		CLIP_TABLE_64R(255),	//	[384, 447]
		CLIP_TABLE_64R(255),	//	[448, 511]
		CLIP_TABLE_16R(255),	//	[512, 527]
		CLIP_TABLE_16R(255),	//	[528, 543]
	};

	#undef CLIP_TABLE_16R
	#undef CLIP_TABLE_64R
	#undef CLIP_TABLE_16U
};
