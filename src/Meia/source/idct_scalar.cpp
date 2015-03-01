#include <string.h>
#include <math.h>

#include <crtdbg.h>

#include <vd2/system/vdtypes.h>
#include <vd2/Meia/MPEGIDCT.h>
#include "tables.h"

//#define VERIFY_PRUNING

using namespace nsVDMPEGTables;

namespace nsVDMPEGIDCTScalar {

	#define PRESCALE_CHEAT_BITS (8)
	#define PRESCALE_BITS (8)
	#define SCALE_BITS (14)

	#define FIXED_SCALE(x) ((int)(((x)<0 ? (x)*(double)(1<<SCALE_BITS)-0.5 : (x)*(double)(1<<SCALE_BITS) + 0.5) ))

	static const double sqrt2 = 1.4142135623730950488016887242097;
	static const double invsqrt2 = 0.70710678118654752440084436210485;

	// lambda(x) = cos(pi*k/16)

	static const double lambda[8]={
		1,
		0.98078528040323043,
		0.92387953251128674,
		0.83146961230254524,
		0.70710678118654757,
		0.55557023301960229,
		0.38268343236508984,
		0.19509032201612833,
	};


	// these are not the same but are very similar in their use

	static const int inv_a1 = 21407;	//FIXED_SCALE(lambda[6] + lambda[2]);
	static const int inv_a2 = -15137;	//FIXED_SCALE(-lambda[2]);
	static const int inv_a3 = -8868;	//FIXED_SCALE(lambda[6] - lambda[2]) - 1;
	static const int inv_b1 = 30274;	//FIXED_SCALE((lambda[6] + lambda[2]) * sqrt2);
	static const int inv_b2 = -21407;	//FIXED_SCALE(-lambda[2] * sqrt2);
	static const int inv_b3 = -12541;	//FIXED_SCALE((lambda[6] - lambda[2]) * sqrt2) - 1;

	// these are inverses

	static const int inv_a0			= 11585;	//FIXED_SCALE(invsqrt2);
	static const int inv_c0			= 11585;	//FIXED_SCALE(invsqrt2);
	static const int inv_rot1		= 21407;	//FIXED_SCALE(lambda[6] + lambda[2]);
	static const int inv_rot2		= -8867;	//FIXED_SCALE(lambda[6] - lambda[2]);
	static const int inv_rot3		= 15137;	//FIXED_SCALE(lambda[2]);
	static const int inv_rot1sq2	= 30274;	//FIXED_SCALE((lambda[6] + lambda[2]) * sqrt2);
	static const int inv_rot2sq2	= -12540;	//FIXED_SCALE((lambda[6] - lambda[2]) * sqrt2);
	static const int inv_rot3sq2	= 21407;	//FIXED_SCALE(lambda[2] * sqrt2);

	#define FIXED_PRESCALE(x) ((int)(((x)<0 ? (x)*(double)(1<<(PRESCALE_BITS+PRESCALE_CHEAT_BITS))-0.5 : (x)*(double)(1<<(PRESCALE_BITS + PRESCALE_CHEAT_BITS)) + 0.5) ))

	static const double prescale_values_1D[8]={
		lambda[0] * 0.5,
		lambda[4] * invsqrt2,
		lambda[6] * invsqrt2,
		lambda[2] * invsqrt2,
		lambda[1] * invsqrt2,
		lambda[5] * invsqrt2,
		lambda[7] * invsqrt2,
		lambda[3] * invsqrt2,
	};

	#define ONE(i,j) FIXED_PRESCALE(prescale_values_1D[i] * prescale_values_1D[j])
	#define ROW(j) ONE(0,j),ONE(1,j),ONE(2,j),ONE(3,j),ONE(4,j),ONE(5,j),ONE(6,j),ONE(7,j)

	static const int prescale_values_2D[64]={
		ROW(0),
		ROW(1),
		ROW(2),
		ROW(3),
		ROW(4),
		ROW(5),
		ROW(6),
		ROW(7),
	};

	#undef ROW
	#undef ONE

	#define ROW(x) (x*8)+0,(x*8)+5,(x*8)+2,(x*8)+4,(x*8)+1,(x*8)+6,(x*8)+3,(x*8)+7

	static const int reorder[64]={
		ROW(0),ROW(5),ROW(2),ROW(4),ROW(1),ROW(6),ROW(3),ROW(7)
	};

	#undef ROW


#if 1
	#include "idct_scalar_asm.inl"
#else
	static void scalar_idct_feig_winograd_8x8(int *d) {
		int i;

		d[0] += 1<<(PRESCALE_BITS);
		
		//---------- full 8x8 IDCT [462a54m22s]

		// apply horizontal R2 (R18) transform [64a]
		
		for(i=0; i<8*8; i+=8) {
			int x2, x3, x4, x5, x6, x7;
			int y4, y5, y6, y7;

			x2 = d[i+2];
			x3 = d[i+3];

			d[i+2] = x2-x3;
			d[i+3] = x2+x3;

			x4 = d[i+4];
			x5 = d[i+5];
			x6 = d[i+6];
			x7 = d[i+7];

			y4 = x4-x6;
			y5 = x7+x5;
			y6 = x4+x6;
			y7 = x7-x5;
					
			d[i+4] = y5+y4;
			d[i+5] = y5-y4;
			d[i+6] = y6;
			d[i+7] = y7;
		}

		// apply vertical R2 transform [64a]

		for(i=0; i<8; ++i) {
			int x2, x3, x4, x5, x6, x7;
			int y4, y5, y6, y7;

			x2 = d[i+16];
			x3 = d[i+24];

			d[i+16] = x2-x3;
			d[i+24] = x2+x3;

			x4 = d[i+32];
			x5 = d[i+40];
			x6 = d[i+48];
			x7 = d[i+56];

			y4 = x4-x6;
			y5 = x7+x5;
			y6 = x4+x6;
			y7 = x7-x5;
					
			d[i+32] = y5+y4;
			d[i+40] = y5-y4;
			d[i+48] = y6;
			d[i+56] = y7;
		}

		// apply M1/M2 and R1 to first 6 rows

		for(i=0; i<6*8; i+=8) {
			int x0, x1, x2, x3, x4, x5, x6, x7;
			int y0, y1, y2, y3, y4, y5, y6, y7;
			int z0, z1, z2, z3, z4, z5, z6, z7;

			x0 = d[i+0];
			x1 = d[i+1];
			x2 = d[i+2];
			x3 = d[i+3];
			x4 = d[i+4];
			x5 = d[i+5];
			x6 = d[i+6];
			x7 = d[i+7];

			// M1 or M2 [18a34m20s]

			if (i==3*8 || i==4*8) {	// M2
				x0 *= inv_a0;
				x1 *= inv_a0;
				x2 *= inv_a0;
				x3 <<= SCALE_BITS;
				x4 <<= SCALE_BITS;
				x5 *= inv_a0;
				
				int tmp = (x6+x7) * inv_b2;

				x6 = x6 * inv_b3 - tmp;
				x7 = x7 * inv_b1 + tmp;
			} else { // M1

				x0 <<= SCALE_BITS - 1;
				x1 <<= SCALE_BITS - 1;
				x2 <<= SCALE_BITS - 1;
				x3 *= inv_a0;
				x4 *= inv_a0;
				x5 <<= SCALE_BITS - 1;

				int tmp = (x6+x7) * inv_a2;

				x6 = x6 * inv_a3 - tmp;
				x7 = x7 * inv_a1 + tmp;
			}

			d[i+0] = x0;
			d[i+1] = x1;
			d[i+2] = x2;
			d[i+3] = x3;
			d[i+4] = x4;
			d[i+5] = x5;
			d[i+6] = x6;
			d[i+7] = x7;
		}

		// Apply M3 and R2 to last two rows together

		{
			int x60, x61, x62, x63, x64, x65, x66, x67;
			int x70, x71, x72, x73, x74, x75, x76, x77;
			int y60, y61, y62, y63, y64, y65, y66, y67;
			int y70, y71, y72, y73, y74, y75, y76, y77;
			int tmp;

			x60 = d[48];
			x61 = d[49];
			x62 = d[50];
			x63 = d[51];
			x64 = d[52];
			x65 = d[53];
			x66 = d[54];
			x67 = d[55];

			x70 = d[56];
			x71 = d[57];
			x72 = d[58];
			x73 = d[59];
			x74 = d[60];
			x75 = d[61];
			x76 = d[62];
			x77 = d[63];

			// apply inverse rotator (1/G2) [18a18m]
			//
			// x0 =  y0*g6 - y1*g2
			// x1 =  y0*g2 + y1*g6
			//
			// let: t = g2*(y0+y1)
			//
			// x0 =  y0*(g6+g2) - t
			// x1 =  y1*(g6-g2) + t

			tmp = inv_rot3 * (x60 + x70);
			y60 = x60*inv_rot2 + tmp;
			y70 = x70*inv_rot1 - tmp;
			
			tmp = inv_rot3 * (x61 + x71);
			y61 = x61*inv_rot2 + tmp;
			y71 = x71*inv_rot1 - tmp;
			
			tmp = inv_rot3 * (x62 + x72);
			y62 = x62*inv_rot2 + tmp;
			y72 = x72*inv_rot1 - tmp;
			
			tmp = inv_rot3sq2 * (x63 + x73);
			y63 = x63*inv_rot2sq2 + tmp;
			y73 = x73*inv_rot1sq2 - tmp;
			
			tmp = inv_rot3sq2 * (x64 + x74);
			y64 = x64*inv_rot2sq2 + tmp;
			y74 = x74*inv_rot1sq2 - tmp;
			
			tmp = inv_rot3 * (x65 + x75);
			y65 = x65*inv_rot2 + tmp;
			y75 = x75*inv_rot1 - tmp;

			// last two rows (Q3) are a little annoying [10a2m2s].
			//
			// order:	66
			//			76
			//			67
			//			77

			y77 = x77-x66;
			y67 = x67+x76;
			y76 = x67-x76;
			y66 = x77+x66;

			x77 = (y77+y67) * inv_c0;
			x67 = (y77-y67) * inv_c0;
			x76 = y76 << SCALE_BITS;
			x66 = y66 << SCALE_BITS;

			y77 = x66-x77;
			y67 = x67+x76;
			y76 = x67-x76;
			y66 = x66+x77;

			d[48] = y60;
			d[49] = y61;
			d[50] = y62;
			d[51] = y63;
			d[52] = y64;
			d[53] = y65;
			d[54] = y66;
			d[55] = y67;

			d[56] = y70;
			d[57] = y71;
			d[58] = y72;
			d[59] = y73;
			d[60] = y74;
			d[61] = y75;
			d[62] = y76;
			d[63] = y77;
		}

		// Apply horizontal R1 (B1t * B2 * B3) to last two rows [144a]

		for(i=0*8; i<8*8; i+=8) {
			int x0, x1, x2, x3, x4, x5, x6, x7;
			int y0, y1, y2, y3, y4, y5, y6, y7;
			int z0, z1, z2, z3, z4, z5, z6, z7;

			x0 = d[i+0];
			x1 = d[i+1];
			x2 = d[i+2];
			x3 = d[i+3];
			x4 = d[i+4];
			x5 = d[i+5];
			x6 = d[i+6];
			x7 = d[i+7];
			
			y0 = x0+x1;
			y1 = x0-x1;
			y2 = x2;
			y3 = x2+x3;

			y5 = x5;
			y7 = x5-x7;
			y4 = y7-x4;
			y6 = x6+y4;

			z0 = y0+y3;
			z1 = y1+y2;
			z2 = y1-y2;
			z3 = y0-y3;
			z4 = y4;
			z5 = y5;
			z6 = y6;
			z7 = y7;

			d[i+0] = z0+z7;
			d[i+1] = z1-z6;
			d[i+2] = z2+z5;
			d[i+3] = z3+z4;
			d[i+4] = z3-z4;
			d[i+5] = z2-z5;
			d[i+6] = z1+z6;
			d[i+7] = z0-z7;
		}

		// Apply vertical R1 [144a].

		for(i=0; i<8; i++) {
			int x0, x1, x2, x3, x4, x5, x6, x7;
			int y0, y1, y2, y3, y4, y5, y6, y7;
			int z0, z1, z2, z3, z4, z5, z6, z7;

			x0 = d[i+0];
			x1 = d[i+8];
			x2 = d[i+16];
			x3 = d[i+24];
			x4 = d[i+32];
			x5 = d[i+40];
			x6 = d[i+48];
			x7 = d[i+56];
			
			y0 = x0+x1;
			y1 = x0-x1;
			y2 = x2;
			y3 = x2+x3;

			y5 = x5;
			y7 = x5-x7;
			y4 = y7-x4;
			y6 = y4+x6;

			z0 = y0+y3;
			z1 = y1+y2;
			z2 = y1-y2;
			z3 = y0-y3;
			z4 = y4;
			z5 = y5;
			z6 = y6;
			z7 = y7;

			d[i+ 0] = (z0+z7)>>(PRESCALE_BITS + SCALE_BITS);
			d[i+ 8] = (z1-z6)>>(PRESCALE_BITS + SCALE_BITS);
			d[i+16] = (z2+z5)>>(PRESCALE_BITS + SCALE_BITS);
			d[i+24] = (z3+z4)>>(PRESCALE_BITS + SCALE_BITS);
			d[i+32] = (z3-z4)>>(PRESCALE_BITS + SCALE_BITS);
			d[i+40] = (z2-z5)>>(PRESCALE_BITS + SCALE_BITS);
			d[i+48] = (z1+z6)>>(PRESCALE_BITS + SCALE_BITS);
			d[i+56] = (z0-z7)>>(PRESCALE_BITS + SCALE_BITS);
		}
	}

	static void scalar_idct_feig_winograd_4x4(int *d) {
		int i;

		//	reordering performed by zigzag matrix:
		//
		//	normal indices	0 1 2 3 4 5 6 7
		//	dest indices	0 5 2 4 1 6 3 7
		//
		//	Thus if last_pos < 10, we can consider columns/rows 1, 3, 6, and 7
		//	to be zero.

		d[0] += 1<<(PRESCALE_BITS);
		
		//---------- VR-pruned 4x4 IDCT [309a44m22s] [VR-PRUNE:156a10m7s]

		// move row 0 into row 1

		d[8] = d[0];
		d[10] = d[2];
		d[12] = d[4];
		d[13] = d[5];

		// apply horizontal R2 (R18) transform [10a] [VR-PRUNE: 54a]
		
		for(i=1*8; i<6*8; i+=8) {
			int x2, x4, x5, x6, x7;
			int y4, y5, y6, y7;

			x4 = d[i+4];
			x5 = d[i+5];
			x6 = d[i+6];
			x7 = d[i+7];

			y4 = x4;
			y5 = x5;
			y6 = x4;
			y7 = x5;
					
			d[i+4] = y5+y4;
			d[i+5] = y5-y4;
			d[i+6] = y6;
			d[i+7] = y7;
		}

		// apply vertical R2 transform [16a] [VR-PRUNE: 48a]

		for(i=0; i<8; ++i) {
			int x2, x4, x5, x6, x7;
			int y4, y5, y6, y7;

			x2 = d[i+16];

			d[i+16] = x2;
			d[i+24] = x2;

			x4 = d[i+32];
			x5 = d[i+40];
			x6 = d[i+48];
			x7 = d[i+56];

			y4 = x4;
			y5 = x5;
			y6 = x4;
			y7 = x5;
					
			d[i+32] = y5+y4;
			d[i+40] = y5-y4;
			d[i+48] = y6;
			d[i+56] = y7;
		}

		// apply M1/M2 to first 6 rows

		for(i=1*8; i<6*8; i+=8) {
			int x0, x2, x3, x4, x5, x6, x7;

			x0 = d[i+0];
			x2 = d[i+2];
			x4 = d[i+4];
			x5 = d[i+5];
			x6 = d[i+6];
			x7 = d[i+7];

			// M1 or M2 [18a27m20s] [VR-PRUNE: 3a7m7s]

			if (i==3*8 || i==4*8) {	// M2
				x0 *= inv_a0;
				x3 = x2 << SCALE_BITS;
				x2 *= inv_a0;
				x4 <<= SCALE_BITS;
				x5 *= inv_a0;
				
				int tmp = (x6-x7) * inv_b2;

				x6 = x6 * inv_b3 - tmp;
				x7 = x7 * -inv_b1 + tmp;
			} else { // M1

				x0 <<= SCALE_BITS - 1;
				x3 = x2 * inv_a0;
				x2 <<= SCALE_BITS - 1;
				x4 *= inv_a0;
				x5 <<= SCALE_BITS - 1;

				int tmp = (x6-x7) * inv_a2;

				x6 = x6 * inv_a3 - tmp;
				x7 = x7 * -inv_a1 + tmp;
			}

			d[i+0] = x0;
			d[i+2] = x2;
			d[i+3] = x3;
			d[i+4] = x4;
			d[i+5] = x5;
			d[i+6] = x6;
			d[i+7] = x7;
		}

		// Apply M3 and R2 to last two rows together

		{
			int x60, x61, x62, x64, x65, x66, x67;
			int x70, x71, x72, x74, x75, x76, x77;
			int y60, y61, y62, y63, y64, y65, y66, y67;
			int y70, y71, y72, y73, y74, y75, y76, y77;
			int tmp;

			x60 = d[48];
			x61 = d[49];
			x62 = d[50];
			x64 = d[52];
			x65 = d[53];
			x66 = d[54];
			x67 = d[55];

			x70 = d[56];
			x71 = d[57];
			x72 = d[58];
			x74 = d[60];
			x75 = d[61];
			x76 = d[62];
			x77 = d[63];

			// apply inverse rotator (1/G2) [15a15m] [VR-PRUNE: 3a3m]
			//
			// x0 =  y0*g6 - y1*g2
			// x1 =  y0*g2 + y1*g6
			//
			// let: t = g2*(y0+y1)
			//
			// x0 =  y0*(g6+g2) - t
			// x1 =  y1*(g6-g2) + t

			tmp = inv_rot3 * (x60 - x70);
			y60 = x60*inv_rot2 + tmp;
			y70 = x70*-inv_rot1 - tmp;
			
			tmp = inv_rot3 * (x62 - x72);
			y62 = x62*inv_rot2 + tmp;
			y72 = x72*-inv_rot1 - tmp;
			
			tmp = inv_rot3sq2 * (x62 - x72);
			y63 = x62*inv_rot2sq2 + tmp;
			y73 = x72*-inv_rot1sq2 - tmp;
			
			tmp = inv_rot3sq2 * (x64 - x74);
			y64 = x64*inv_rot2sq2 + tmp;
			y74 = x74*-inv_rot1sq2 - tmp;
			
			tmp = inv_rot3 * (x65 - x75);
			y65 = x65*inv_rot2 + tmp;
			y75 = x75*-inv_rot1 - tmp;

			// last two rows (Q3) are a little annoying [10a2m2s].
			//
			// order:	66
			//			76
			//			67
			//			77

			y77 = x77-x66;
			y67 = x67+x76;
			y76 = x67-x76;
			y66 = x77+x66;

			x77 = (y77-y67) * inv_c0;
			x67 = (y77+y67) * inv_c0;
			x76 = y76 << SCALE_BITS;
			x66 = y66 << SCALE_BITS;

			y77 = x66-x77;
			y67 = x67-x76;
			y76 = x67+x76;
			y66 = x66+x77;

			d[48] = y60;
			d[50] = y62;
			d[51] = y63;
			d[52] = y64;
			d[53] = y65;
			d[54] = y66;
			d[55] = y67;

			d[56] = y70;
			d[58] = y72;
			d[59] = y73;
			d[60] = y74;
			d[61] = y75;
			d[62] = y76;
			d[63] = y77;
		}

		// Apply horizontal R1 (B1t * B2 * B3) to last two rows [112a] [VR-PRUNE: 32a]

		for(i=1*8; i<8*8; i+=8) {
			int x0, x2, x3, x4, x5, x6, x7;
			int y0, y2, y3, y4, y5, y6, y7;
			int z0, z1, z2, z3, z4, z5, z6, z7;

			x0 = d[i+0];
			x2 = d[i+2];
			x3 = d[i+3];
			x4 = d[i+4];
			x5 = d[i+5];
			x6 = d[i+6];
			x7 = d[i+7];
			
			y0 = x0;
			y2 = x2;
			y3 = x2+x3;

			y5 = x5;
			y7 = x5-x7;
			y4 = y7-x4;
			y6 = x6+y4;

			z0 = y0+y3;
			z1 = y0+y2;
			z2 = y0-y2;
			z3 = y0-y3;
			z4 = y4;
			z5 = y5;
			z6 = y6;
			z7 = y7;

			d[i+0] = z0+z7;
			d[i+1] = z1-z6;
			d[i+2] = z2+z5;
			d[i+3] = z3+z4;
			d[i+4] = z3-z4;
			d[i+5] = z2-z5;
			d[i+6] = z1+z6;
			d[i+7] = z0-z7;
		}

		// Apply vertical R1 [128a] [VR-PRUNE: 16a]

		for(i=0; i<8; i++) {
			int x0, x2, x3, x4, x5, x6, x7;
			int y0, y2, y3, y4, y5, y6, y7;
			int z0, z1, z2, z3, z4, z5, z6, z7;

			x0 = d[i+8];
			x2 = d[i+16];
			x3 = d[i+24];
			x4 = d[i+32];
			x5 = d[i+40];
			x6 = d[i+48];
			x7 = d[i+56];
			
			y0 = x0;
			y2 = x2;
			y3 = x2+x3;

			y5 = x5;
			y7 = x5-x7;
			y4 = y7-x4;
			y6 = y4+x6;

			z0 = y0+y3;
			z1 = y0+y2;
			z2 = y0-y2;
			z3 = y0-y3;
			z4 = y4;
			z5 = y5;
			z6 = y6;
			z7 = y7;

			d[i+ 0] = (z0+z7)>>(PRESCALE_BITS + SCALE_BITS);
			d[i+ 8] = (z1-z6)>>(PRESCALE_BITS + SCALE_BITS);
			d[i+16] = (z2+z5)>>(PRESCALE_BITS + SCALE_BITS);
			d[i+24] = (z3+z4)>>(PRESCALE_BITS + SCALE_BITS);
			d[i+32] = (z3-z4)>>(PRESCALE_BITS + SCALE_BITS);
			d[i+40] = (z2-z5)>>(PRESCALE_BITS + SCALE_BITS);
			d[i+48] = (z1+z6)>>(PRESCALE_BITS + SCALE_BITS);
			d[i+56] = (z0-z7)>>(PRESCALE_BITS + SCALE_BITS);
		}
	}
#endif

	static void scalar_idct_intra(unsigned char *dst, int pitch, int *tmp, int last_pos) {
		if (!last_pos) {
			const unsigned char v = clip_table[288 + ((tmp[0] + (1 << PRESCALE_BITS)) >> (PRESCALE_BITS + 1))];
			for(int j=0; j<8; ++j) {
				for(int i=0; i<8; ++i)
					dst[i] = v;

				dst += pitch;
			}
		} else {
			if (last_pos < 10) {
				#ifdef VERIFY_PRUNING
					int chk[64];
					memcpy(chk, tmp, sizeof(int)*64);
					scalar_idct_feig_winograd_8x8(chk);
				#endif

				scalar_idct_feig_winograd_4x4(tmp);

				#ifdef VERIFY_PRUNING
					VDASSERT(!memcmp(chk, tmp, 64*sizeof(int)));
				#endif
			} else
				scalar_idct_feig_winograd_8x8(tmp);

			for(int j=0; j<8; ++j) {
				for(int i=0; i<8; ++i) {
					int v = tmp[j*8+i];

					dst[i] = clip_table[v + 288];
				}
				dst += pitch;
			}
		}
	}

	static void scalar_idct_nonintra(unsigned char *dst, int pitch, int *tmp, int last_pos) {
		if (last_pos == 0) {
			const unsigned char *p = &clip_table[288 + ((tmp[0] + (1 << PRESCALE_BITS)) >> (PRESCALE_BITS + 1))];
			for(int j=0; j<8; ++j) {
				for(int i=0; i<8; ++i)
					dst[i] = p[dst[i]];

				dst += pitch;
			}
		} else {
			if (last_pos < 10) {
				#ifdef VERIFY_PRUNING
					int chk[64];
					memcpy(chk, tmp, sizeof(int)*64);
					scalar_idct_feig_winograd_8x8(chk);
				#endif

				scalar_idct_feig_winograd_4x4(tmp);

				#ifdef VERIFY_PRUNING
					VDASSERT(!memcmp(chk, tmp, 64*sizeof(int)));
				#endif
			} else
				scalar_idct_feig_winograd_8x8(tmp);

			for(int j=0; j<8; ++j) {
				for(int i=0; i<8; ++i) {
					int v = tmp[j*8+i] + dst[i];

					dst[i] = clip_table[v+288];
				}
				dst += pitch;
			}
		}
	}

	static void scalar_idct_test(int *tmp, int last_pos) {
		scalar_idct_feig_winograd_8x8(tmp);
	}
	///////////////////////////////////////////////////////////////////////

	static const int zigzag_std[64]={
		 0,  1,  8, 16,  9,  2,  3, 10,
		17, 24, 32, 25, 18, 11,  4,  5,
		12, 19, 26, 33, 40, 48, 41, 34,
		27, 20, 13,  6,  7, 14, 21, 28,
		35, 42, 49, 56, 57, 50, 43, 36,
		29, 22, 15, 23, 30, 37, 44, 51,
		58, 59, 52, 45, 38, 31, 39, 46,
		53, 60, 61, 54, 47, 55, 62, 63,
	};

	#define ROW(x) reorder[zigzag_std[(x*8)+0]],reorder[zigzag_std[(x*8)+1]],reorder[zigzag_std[(x*8)+2]],reorder[zigzag_std[(x*8)+3]],reorder[zigzag_std[(x*8)+4]],reorder[zigzag_std[(x*8)+5]],reorder[zigzag_std[(x*8)+6]],reorder[zigzag_std[(x*8)+7]]

	static const int zigzag_reordered[64]={
		ROW(0),ROW(1),ROW(2),ROW(3),ROW(4),ROW(5),ROW(6),ROW(7)
	};

};

const struct VDMPEGIDCTSet g_VDMPEGIDCT_scalar = {
	(tVDMPEGIDCT)nsVDMPEGIDCTScalar::scalar_idct_intra,
	(tVDMPEGIDCT)nsVDMPEGIDCTScalar::scalar_idct_nonintra,
	(tVDMPEGIDCTTest)nsVDMPEGIDCTScalar::scalar_idct_test,
	nsVDMPEGIDCTScalar::prescale_values_2D,
	nsVDMPEGIDCTScalar::zigzag_reordered,
};
