//	Priss (NekoAmp 2.0) - MPEG-1/2 audio decoding library
//	Copyright (C) 2003 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
///////////////////////////////////////////////////////////////////////////
//
// MPEG-1/2 layer III decoding
//
// Layer III is fugly compared to layer I/II.  The general stages of decoding
// are:
//
// 1) Sliding window (primitive VBR)
//
//    Data regions of each frame are interpreted as a continuous stream and
//    the encoded values for a frame start 0-511 bytes behind the data region
//    for that frame.
//
// 2) Huffman decoding
//
//    Layer III granules encode 576 frequency lines, composed of 32 subbands
//    of 18 samples each.  The 576 values are broken into four regions: two
//    encoded in pairs using two huffman tables, a third composed of only
//    -1/0/+1 in quadruples, and the highest part of the spectrum all zeroes.
//    When this stage fails, the whole frame is trashed.
//
// 3) Dequantization
//
//    Decoded values are pushed through a nonlinear power ramp and then
//    modulated by scalefactors.  Scalefactors group frequency lines -- not
//    necessarily dividing cleanly between subbands -- differently depending
//    on the block type:
//
//    A) long blocks (usual case) - 21 scalefactor bands
//    B) short blocks (transients) - 12 scalefactor bands for three time
//                                   windows (6 samples each)
//    C) mixed blocks - 2 long bands and 3 windows of 9 bands each.
//
//    Short blocks seldom occur and are not really worth optimizing (one
//    problem is that they only encode one-third of the frequency range).
//    I've never seen mixed blocks.
//
// 4) Stereo processing
//
//    Layer III supports two types of joint stereo, mid/side (MS) and
//    intensity stereo (IS).  In MS, a simple butterfly is used to convert
//    sum and difference signals into stereo; in IS, a mono channel is
//    cast to stereo with different amplitudes on the left and right.
//    When both MS and IS are active, the switch from MS to IS occurs when
//    the data for the right channel ends.
//
//    Priss currently has a small bug in MPEG-2 IS processing: unlike
//    MPEG-1, short blocks in MPEG-2 have the MS-to-IS switch point tracked
//    per window.  MPEG-2 audio with intensity stereo is a bit hard to
//    come by (the LAME build I have doesn't do it), and even harder to get
//    into an MPEG-1 video file, so I don't have a test case for this.
//
// 5) Antialiasing
//
//    I don't really understand this, but rotations are performed between
//    frequency lines in adjacent subbands.  Without it, a sort of "halo"
//    echo effect is heard.  Next.
//
// 6) IMDCT
//
//    The IMDCT, like other DCT transforms, has infinite basis functions and
//    thus performs poorly with transients.  Short blocks combat this by
//    slicing the IMDCT into thirds, thus reducing the length of preecho.
//    Slower transients can also be modeled with the regular 18-point IMDCT
//    by using the attack and release windows (types 1 and 3) instead of
//    the regular sinusoidal window (type 0).
//
// 7) Polyphase filter
//
//    As with layer I and II, it all ends here.  MPEG-1 produces two
//    granules for 1152 samples and MPEG-2 produces one granule of 576
//    samples.

#include <math.h>
#include <float.h>
#include "engine.h"
#include "bitreader.h"

//#define RDTSC_PROFILE

#ifdef RDTSC_PROFILE

	#include <windows.h>

	static long p_lasttime;
	static long p_frames=0;
	static __int64 p_total=0;
	static __int64 p_scalefac=0;
	static __int64 p_huffdec=0;
	static __int64 p_huffdec1=0;
	static __int64 p_dequan=0;
	static __int64 p_stereo=0;
	static __int64 p_hybrid=0;
	static __int64 p_polyphase=0;

	static void __inline profile_set(int) {
		__asm {
			rdtsc
			mov		p_lasttime,eax
		};
	}

	static void __inline profile_add(__int64& counter) {
		long diff;

		__asm {
			rdtsc
			sub		eax,p_lasttime
			mov		diff,eax
		}

		counter += diff;
		p_total += diff;

		__asm {
			rdtsc
			mov		p_lasttime,eax

		}
	}
#else

	#define profile_set(x)
	#define profile_add(x)

#endif


namespace {
	struct LayerIIIRegionSideInfo {
		unsigned	part2_3_length;
		unsigned	big_values;
		unsigned	global_gain;
		unsigned	scalefac_compress;
		bool		window_switching_flag;
		uint8	table_select[3];	// 3 for unswitched, 2 for switched
		union {
			struct {
				uint8	block_type;
				bool	mixed_block_flag;
				uint8	subblock_gain[3];
			} switched;
			struct {
				uint8	region0_count;
				uint8	region1_count;
			} unswitched;
		};
		bool		preflag;
		bool		scalefac_scale;
		bool		count1table_select;
	};
	struct LayerIIISideInfo {
		unsigned	main_data_begin;
		bool		scfsi[2][4];
		LayerIIIRegionSideInfo gr[2][2];
	};

	void DecodeSideInfoMPEG1(LayerIIISideInfo& si, const uint8 *src, unsigned nch) {
		VDMPEGAudioBitReader bits(src, 33);
		unsigned gr, ch, i;

		si.main_data_begin	= bits.get(9);

		bits.get(nch>1 ? 3 : 5);		// skip private bits

		// read scale factor selectors (ch x 4 bits)
		for(ch=0; ch<nch; ++ch)
			for(i=0; i<4; ++i)
				si.scfsi[ch][i] = bits.getbool();

		// read global regions (2 x ch x 56 bits)
		for(gr=0; gr<2; ++gr) {
			for(ch=0; ch<nch; ++ch) {
				LayerIIIRegionSideInfo& rsi = si.gr[gr][ch];

				rsi.part2_3_length			= bits.get(12);
				rsi.big_values				= bits.get(9);
				rsi.global_gain				= bits.get(8);
				rsi.scalefac_compress		= bits.get(4);
				rsi.window_switching_flag	= bits.getbool();

				if (rsi.window_switching_flag) {
					rsi.switched.block_type			= bits.get(2);
					rsi.switched.mixed_block_flag	= bits.getbool();
					for(unsigned r=0; r<2; ++r)
						rsi.table_select[r] = bits.get(5);
					rsi.table_select[2] = 0;
					for(unsigned w=0; w<3; ++w)
						rsi.switched.subblock_gain[w] = bits.get(3);
				} else {
					for(unsigned r=0; r<3; ++r)
						rsi.table_select[r] = bits.get(5);
					rsi.unswitched.region0_count = bits.get(4);
					rsi.unswitched.region1_count = bits.get(3);
				}

				rsi.preflag = bits.getbool();
				rsi.scalefac_scale	= bits.getbool();
				rsi.count1table_select = bits.getbool();
			}
		}
	}
	
	void DecodeSideInfoMPEG2(LayerIIISideInfo& si, const uint8 *src, unsigned nch) {
		VDMPEGAudioBitReader bits(src, 33);

		si.main_data_begin	= bits.get(8);

		bits.get(nch>1 ? 2 : 1);		// skip private bits

		for(unsigned ch=0; ch<nch; ++ch) {		// 63 bits per channel
			LayerIIIRegionSideInfo& rsi = si.gr[0][ch];

			rsi.part2_3_length			= bits.get(12);
			rsi.big_values				= bits.get(9);
			rsi.global_gain				= bits.get(8);
			rsi.scalefac_compress		= bits.get(9);
			rsi.window_switching_flag	= bits.getbool();

			if (rsi.window_switching_flag) {
				rsi.switched.block_type			= bits.get(2);
				rsi.switched.mixed_block_flag	= bits.getbool();
				for(unsigned r=0; r<2; ++r)
					rsi.table_select[r] = bits.get(5);
				rsi.table_select[2] = 0;
				for(unsigned w=0; w<3; ++w)
					rsi.switched.subblock_gain[w] = bits.get(3);
			} else {
				for(unsigned r=0; r<3; ++r)
					rsi.table_select[r] = bits.get(5);
				rsi.unswitched.region0_count = bits.get(4);
				rsi.unswitched.region1_count = bits.get(3);
			}

			rsi.scalefac_scale	= bits.getbool();
			rsi.count1table_select = bits.getbool();
		}
	}

	static struct costable_t {
		costable_t() {
			for(unsigned i=0; i<36; ++i) {
				for(unsigned j=0; j<18; ++j) {
					v[i][j] = (float)cos((3.1415926535897932 / 72) * (i+i+1+18) * (j+j+1));
				}
			}
		}

		float v[36][18];
	} costable;

	////////////////////////////////////

#if 0
	void IMDCT_18(const float *in, float (*out)[2][32], float *overlap, const float *window) {
		float t[36];

		for(unsigned i=0; i<36; ++i) {
			double s=0;
			for(unsigned j=0; j<18; ++j) {
				s += in[j] * costable.v[i][j]; //cos((3.1415926535897932 / 72) * (i+i+1+18) * (j+j+1));
			}

			t[i] = (float)s;
		}

		for(unsigned k=0; k<18; ++k) {
			out[k][0][0] = overlap[k] + t[k]*window[k];
			overlap[k] = t[k+18]*window[k+18];
		}
	}
#else

	// The algorithm for this IMDCT comes from HP Laboratories report
	// HPL-2000-66, "Faster MPEG-1 Layer III Audio Decoding" by
	// Scott B. Marovich.  It first uses a technique from the Lee
	// decomposition to shift the IMDCT into an IDCT, then factors
	// the 18-point IDCT down to 4-point and 5-point IDCTs.  Profiling
	// seems to indicate that this is not as fast as FreeAmp's IMDCT,
	// which appears to use a direct IMDCT factorization instead, but
	// layer III decoding is rather rare in MPEG-1 video files and
	// this runs fast enough.

	void IMDCT_9(const float (*b)[2], float *d) {
		static const float c1 = 0.93969262078591f;	// cos(1*(pi/9))
		static const float c2 = 0.76604444311898f;	// cos(2*(pi/9))
		static const float c4 = 0.17364817766693f;	// cos(4*(pi/9))

		// 'g' stage (5-point IDCT)
		const float G0 = b[0][0];
		const float G1 = b[2][0];
		const float G2 = b[4][0];
		const float G3 = b[6][0];
		const float G4 = b[8][0];
		const float x0 = G3*0.5f + G0;
		const float x1 = G0 - G3;
		const float x2 = G1 - G2 - G4;
		const float g0 = x0 + c1*G1 + c2*G2 + c4*G4;
		const float g1 = x2*0.5f + x1;
		const float g2 = x0 - c4*G1 - c1*G2 + c2*G4;
		const float g3 = x0 - c2*G1 + c4*G2 - c1*G4;
		const float g4 = x1 - x2;

		// 'h prime' stage (4-point IDCT)
		static const float odd[4]={
			0.50771330594287f,
			0.57735026918963f,
			0.77786191343021f,
			1.46190220008154f
		};

		static const float covals[4]={	// odd[i] * sin((pi/18)*(2i+1))*(-1)^i
			0.08816349035423f,
			-0.28867513459481f,
			0.59587679629710f,
			-1.37373870972731f,
		};

		const float H0 = b[1][0];
		const float H1 = b[1][0] + b[3][0];
		const float H2 = b[3][0] + b[5][0];
		const float H3 = b[5][0] + b[7][0];
		const float y0 = H3*0.5f + H0;
		const float y1 = H1 - H2;
		const float h0 = odd[0]*(y0 + c1*H1 + c2*H2) + b[7][0]*covals[0];
		const float h1 = odd[1]*(0.5f*y1 + H0 - H3)  + b[7][0]*covals[1];
		const float h2 = odd[2]*(y0 - c4*H1 - c1*H2) + b[7][0]*covals[2];
		const float h3 = odd[3]*(y0 - c2*H1 + c4*H2) + b[7][0]*covals[3];

		d[0] = g0 + h0;
		d[1] = g1 + h1;
		d[2] = g2 + h2;
		d[3] = g3 + h3;
		d[4] = g4;
		d[5] = g3 - h3;
		d[6] = g2 - h2;
		d[7] = g1 - h1;
		d[8] = g0 - h0;
	}

	void IMDCT_18(const float *in, float (*out)[2][32], float *overlap, const float *window) {
		static const float pi = 3.1415926535897932384626433832795f;

		float a[18];
		float t[18];

		a[0] = in[0];
		a[1] = in[0]+in[1];
		a[2] = in[1]+in[2];
		a[3] = in[2]+in[3];
		a[4] = in[3]+in[4];
		a[5] = in[4]+in[5];
		a[6] = in[5]+in[6];
		a[7] = in[6]+in[7];
		a[8] = in[7]+in[8];
		a[9] = in[8]+in[9];
		a[10] = in[9]+in[10];
		a[11] = in[10]+in[11];
		a[12] = in[11]+in[12];
		a[13] = in[12]+in[13];
		a[14] = in[13]+in[14];
		a[15] = in[14]+in[15];
		a[16] = in[15]+in[16];
		a[17] = in[16]+in[17];

		a[17] += a[15];
		a[15] += a[13];
		a[13] += a[11];
		a[11] += a[9];
		a[9] += a[7];
		a[7] += a[5];
		a[5] += a[3];
		a[3] += a[1];

		float d[18];

		IMDCT_9((float(*)[2])&a[0], d);
		IMDCT_9((float(*)[2])&a[1], d+9);
		static const float coeff_9_to_18[9]={		// 1 / (2 cos((pi/36)(2i+1)))
			0.50190991877167f,
			0.51763809020504f,
			0.55168895948125f,
			0.61038729438073f,
			0.70710678118655f,
			0.87172339781055f,
			1.18310079157625f,
			1.93185165257814f,
			5.73685662283492f,
		};

		const float y[9]={
			d[9+0] * coeff_9_to_18[0],
			d[9+1] * coeff_9_to_18[1],
			d[9+2] * coeff_9_to_18[2],
			d[9+3] * coeff_9_to_18[3],
			d[9+4] * coeff_9_to_18[4],
			d[9+5] * coeff_9_to_18[5],
			d[9+6] * coeff_9_to_18[6],
			d[9+7] * coeff_9_to_18[7],
			d[9+8] * coeff_9_to_18[8],
		};

		// Note: The second half of this butterfly has been reversed.
		t[ 0] = d[0] + y[0];
		t[ 9] = d[0] - y[0];
		t[ 1] = d[1] + y[1];
		t[10] = d[1] - y[1];
		t[ 2] = d[2] + y[2];
		t[11] = d[2] - y[2];
		t[ 3] = d[3] + y[3];
		t[12] = d[3] - y[3];
		t[ 4] = d[4] + y[4];
		t[13] = d[4] - y[4];
		t[ 5] = d[5] + y[5];
		t[14] = d[5] - y[5];
		t[ 6] = d[6] + y[6];
		t[15] = d[6] - y[6];
		t[ 7] = d[7] + y[7];
		t[16] = d[7] - y[7];
		t[ 8] = d[8] + y[8];
		t[17] = d[8] - y[8];

		// multiplication to convert idct to imdct has already been folded
		// into the windows

		// y[0..8]   =  x[9..17]  = t[17..9]
		// y[9..17]  = -x[17..9]  = -t[9..17]
		// y[18..26] = -x[8..0]   = -t[8..0]		(negation folded into window)
		// y[27..35] = -x[0..8]   = -t[0..8]		(negation folded into window)

#if 0
		for(unsigned k=0; k<9; ++k) {
			out[k][0][0] = overlap[k] + t[17-k]*window[k];
			out[k+9][0][0] = overlap[k+9] - t[k+9]*window[k+9];
			overlap[k] = t[8-k]*window[k+18];
			overlap[k+9] = t[k]*window[k+27];
		}
#else
		out[0][0][0] = overlap[0] + t[17]*window[0];
		out[1][0][0] = overlap[1] + t[16]*window[1];
		out[2][0][0] = overlap[2] + t[15]*window[2];
		out[3][0][0] = overlap[3] + t[14]*window[3];
		out[4][0][0] = overlap[4] + t[13]*window[4];
		out[5][0][0] = overlap[5] + t[12]*window[5];
		out[6][0][0] = overlap[6] + t[11]*window[6];
		out[7][0][0] = overlap[7] + t[10]*window[7];
		out[8][0][0] = overlap[8] + t[ 9]*window[8];
		out[9][0][0] = overlap[9] - t[ 9]*window[9];
		out[10][0][0] = overlap[10] - t[10]*window[10];
		out[11][0][0] = overlap[11] - t[11]*window[11];
		out[12][0][0] = overlap[12] - t[12]*window[12];
		out[13][0][0] = overlap[13] - t[13]*window[13];
		out[14][0][0] = overlap[14] - t[14]*window[14];
		out[15][0][0] = overlap[15] - t[15]*window[15];
		out[16][0][0] = overlap[16] - t[16]*window[16];
		out[17][0][0] = overlap[17] - t[17]*window[17];
		overlap[0] = t[8]*window[18];
		overlap[1] = t[7]*window[19];
		overlap[2] = t[6]*window[20];
		overlap[3] = t[5]*window[21];
		overlap[4] = t[4]*window[22];
		overlap[5] = t[3]*window[23];
		overlap[6] = t[2]*window[24];
		overlap[7] = t[1]*window[25];
		overlap[8] = t[0]*window[26];
		overlap[9] = t[0]*window[27];
		overlap[10] = t[1]*window[28];
		overlap[11] = t[2]*window[29];
		overlap[12] = t[3]*window[30];
		overlap[13] = t[4]*window[31];
		overlap[14] = t[5]*window[32];
		overlap[15] = t[6]*window[33];
		overlap[16] = t[7]*window[34];
		overlap[17] = t[8]*window[35];
#endif
	}

	void IMDCT_18_Null(float (*out)[2][32], float *overlap) {
		for(unsigned k=0; k<18; ++k) {
			out[k][0][0] = overlap[k];
			overlap[k] = 0;
		}
	}
#endif

	////////////////////////////////////////////////////////////

#if 0
	void IMDCT_6_3(const float *in, float (*out)[2][32], float *overlap, const float *window) {
		float t[24]={0};

		unsigned k;

		for(unsigned w=0; w<3; ++w) {
			for(unsigned i=0; i<12; ++i) {
				double s=0;
				for(unsigned j=0; j<6; ++j) {
					s += in[j*3] * cos((3.1415926535897932 / 24) * (i+i+1+6) * (j+j+1));
				}

				t[i+w*6] += (float)s * window[i];
			}

			++in;
		}

		for(k=0; k<6; ++k)
			out[k][0][0] = overlap[k];

		for(k=0; k<12; ++k) {
			out[k+6][0][0] = overlap[k+6] + t[k];
			overlap[k] = t[k+12];
		}

		for(k=0; k<6; ++k)
			overlap[k+12] = 0;
	}
#else

	void IMDCT_6(float dst[12], const float src[18]) {
#if 0
		{
			for(unsigned i=0; i<12; ++i) {
				double s=0;
				for(unsigned j=0; j<6; ++j) {
					s += src[j*3] * cos((3.1415926535897932 / 24) * (i+i+1+6) * (j+j+1));
				}

				dst[i] = (float)s;
			}
		}
#else
		//////////////

		static const float root3div2 = 0.86602540378443864676372317075294f;
		static const float pi = 3.1415926535897932384626433832795f;

		static const float oddscale[3]={		// 1/(2*cos(pi/12*[1 3 5])) - part of Lee decomposition of 6pt IDCT
			0.51763809020504f,
			0.70710678118655f,
			1.93185165257814f,
		};

		static const float finalscale[6]={		// 1/(2*cos(pi/24*[1:2:11])) - part of conversion from IDCT to IMDCT
			0.504314480290076f,
			0.541196100146197f,
			0.630236207005132f,
			0.821339815852291f,
			1.30656296487638f,
			3.83064878777019f,
		};

		float a[6]={src[0], src[0]+src[3], src[3]+src[6], src[6]+src[9], src[9]+src[12], src[12]+src[15]};
		float c1[6] = { a[0], a[2], a[4], a[1], a[1]+a[3], a[3]+a[5] };
		float c2[6]={
			c1[0] + c1[1]*root3div2 + c1[2]*0.5f,
			c1[0] - c1[2],
			c1[0] - c1[1]*root3div2 + c1[2]*0.5f,

			(c1[3] + c1[4]*root3div2 + c1[5]*0.5f) * oddscale[0],
			(c1[3]					 - c1[5]     ) * oddscale[1],
			(c1[3] - c1[4]*root3div2 + c1[5]*0.5f) * oddscale[2],
		};
		float b[6]={
			(c2[0]+c2[3]) * finalscale[0],
			(c2[1]+c2[4]) * finalscale[1],
			(c2[2]+c2[5]) * finalscale[2],
			(c2[2]-c2[5]) * finalscale[3],
			(c2[1]-c2[4]) * finalscale[4],
			(c2[0]-c2[3]) * finalscale[5],
		};

		dst[0] = b[3];
		dst[1] = b[4];
		dst[2] = b[5];
		dst[3] = -b[5];
		dst[4] = -b[4];
		dst[5] = -b[3];
		dst[6] = -b[2];
		dst[7] = -b[1];
		dst[8] = -b[0];
		dst[9] = -b[0];
		dst[10] = -b[1];
		dst[11] = -b[2];
#endif
	}

	// This also comes from the Marovich paper.  It's not as optimized, but
	// short blocks are comparatively rare.

	void IMDCT_6_3(const float *in, float (*out)[2][32], float *overlap, const float *window) {
		float t[24]={0};

		unsigned k;

		for(unsigned w=0; w<3; ++w) {
			float u[12];

			IMDCT_6(u, in+w);

			for(unsigned i=0; i<12; ++i)
				t[i+w*6] += (float)(u[i] * window[i]);
		}

		for(k=0; k<6; ++k)
			out[k][0][0] = overlap[k];

		for(k=0; k<12; ++k) {
			out[k+6][0][0] = overlap[k+6] + t[k];
			overlap[k] = t[k+12];
		}

		for(k=0; k<6; ++k)
			overlap[k+12] = 0;
	}
#endif

	////////////////////////////////////

	static void DecodeScalefactorsMPEG1(const LayerIIISideInfo& sideinfo, VDMPEGAudioHuffBitReader& bits, uint8 *scalefac_l, uint8 *scalefac_s, unsigned gr, unsigned ch) {
		const LayerIIIRegionSideInfo& rsi = sideinfo.gr[gr][ch];

		// read in scalefactors
		static const uint8 slen1_table[16]={0,0,0,0,3,1,1,1,2,2,2,3,3,3,4,4};
		static const uint8 slen2_table[16]={0,1,2,3,0,1,2,3,1,2,3,1,2,3,2,3};
		unsigned i;
		const unsigned slen1 = slen1_table[rsi.scalefac_compress];
		const unsigned slen2 = slen2_table[rsi.scalefac_compress];

		if (rsi.window_switching_flag && rsi.switched.block_type == 2) {	// short blocks
			// Scalefactors for short blocks are ordered as
			// scalefac_s[sfb][window].  We store them as one big array for
			// convenience.

			if (rsi.switched.mixed_block_flag) {
				for(i=0; i<8; ++i)						// long bands 0-7
					*scalefac_l++ = bits.get(slen1);

				scalefac_s += 3*3;
				for(i=0; i<9; ++i)						// short bands 3-5
					*scalefac_s++ = slen1?bits.get(slen1):0;

				for(i=0; i<18; ++i)						// short bands 6-11
					*scalefac_s++ = slen2?bits.get(slen2):0;
			} else {
				for(i=0; i<18; ++i)						// short bands 0-5
					*scalefac_s++ = slen1?bits.get(slen1):0;

				for(i=0; i<18; ++i)
					*scalefac_s++ = slen2?bits.get(slen2):0;
			}
		} else {		// long blocks
			// Long blocks are split into four bands.  Depending on
			// the scalefactor selectors, some bands will share
			// scalefactors between regions, and some won't.

			unsigned sfb = 0;

			static const unsigned sfblimit[4]={6,11,16,21};
			unsigned slen[4]={slen1,slen1,slen2,slen2};

			for(unsigned r=0; r<4; ++r) {
				if (!gr || !sideinfo.scfsi[ch][r]) {
					if (unsigned slen_bits = slen[r]) {
						for(; sfb<sfblimit[r]; ++sfb)
							scalefac_l[sfb] = bits.get(slen_bits);
					} else {
						for(; sfb<sfblimit[r]; ++sfb)
							scalefac_l[sfb] = 0;
					}
				} else
					sfb = sfblimit[r];
			}
		}
	}

	static void DecodeScalefactorsMPEG2(LayerIIIRegionSideInfo& rsi, VDMPEGAudioHuffBitReader& bits, uint8 *scalefac_l, uint8 *scalefac_s, unsigned mode_ext, unsigned ch) {
		// Tables of bit allocations per scalefactor breakdown type,
		// three cases per type (long, short, mixed).  Note that for
		// the mixed case the first six scalefactor bands (long bands)
		// have been subtracted from the table.

		static const uint8 is_on_566_tab[3][4] = {{7,7,7,0}, {12,12,12,0}, {0,15,12,0}};
		static const uint8 is_on_444_tab[3][4] = {{6,6,6,3}, {12,9,9,6}, {0,12,9,6}};
		static const uint8 is_on_430_tab[3][4] = {{8,8,5,0}, {15,12,9,0}, {0,18,9,0}};
		static const uint8 is_off_5544_tab[3][4] = {{6,5,5,5}, {9,9,9,9}, {0,9,9,9}};
		static const uint8 is_off_5540_tab[3][4] = {{6,5,7,3}, {9,9,12,6}, {0,9,12,6}};
		static const uint8 is_off_4300_tab[3][4] = {{11,10,0,0}, {18,18,0,0}, {9,18,0,0}};

		unsigned v = rsi.scalefac_compress;
		unsigned set_bits[4];

		rsi.preflag = 0;

		const uint8 *set_count;
		const unsigned windowtype = rsi.window_switching_flag && rsi.switched.block_type==2 ? rsi.switched.mixed_block_flag ? 2 : 1 : 0;

		if ((mode_ext & 1) && ch) {	// second channel w/ intensity stereo enabled
			v >>= 1;			// LSB is used for intensity scale.

			if (v < 180) {			// 5 x 6 x 6 = 180 types
				set_bits[0] = v / 36;
				set_bits[1] = (v % 36) / 6;
				set_bits[2] = v % 6;
				set_bits[3] = 0;
				set_count = is_on_566_tab[windowtype];
			} else if (v < 244) {	// 4 x 4 x 4 = 64 types
				v -= 180;
				set_bits[0] = (v>>4) & 3;
				set_bits[1] = (v>>2) & 3;
				set_bits[2] = v & 3;
				set_bits[3] = 0;
				set_count = is_on_444_tab[windowtype];
			} else {				// 4 x 3 = 12 types
				v -= 244;
				set_bits[0] = v / 3;
				set_bits[1] = v % 3;
				set_bits[2] = 0;
				set_bits[3] = 0;
				set_count = is_on_430_tab[windowtype];
			}
		} else {					// first channel or intensity stereo disabled
			if (v < 400) {			// 5 x 5 x 4 x 4 = 400 types
				set_bits[0] = (v>>4) / 5;
				set_bits[1] = (v>>4) % 5;
				set_bits[2] = (v>>2) & 3;
				set_bits[3] = v & 3;
				set_count = is_off_5544_tab[windowtype];
			} else if (v < 500) {	// 5 x 5 x 4 = 100 types
				v -= 400;
				set_bits[0] = (v>>2) / 5;
				set_bits[1] = (v>>2) % 5;
				set_bits[2] = v & 3;
				set_bits[3] = 0;
				set_count = is_off_5540_tab[windowtype];
			} else {				// 4 x 3 = 12 types
				v -= 500;
				set_bits[0] = v / 3;
				set_bits[1] = v % 3;
				set_bits[2] = 0;
				set_bits[3] = 0;
				set_count = is_off_4300_tab[windowtype];
				rsi.preflag = 1;
			}
		}

		uint8 *dst = scalefac_l;

		if (rsi.window_switching_flag && rsi.switched.block_type == 2) {
			if (rsi.switched.mixed_block_flag) {
				// The first six scalefactors, which are in the long band of
				// mixed blocks, are always in the first set of scalefactors
				// so we just read them now.

				for(unsigned i=0; i<6; ++i)
					scalefac_l[i] = set_bits[0] ? bits.get(set_bits[0]) : 0;

				scalefac_s += 9;	// skip first three short bands
			}

			dst = scalefac_s;
		}

		for(unsigned set=0; set<4; ++set) {
			for(unsigned i=0; i<set_count[set]; ++i)
				*dst++ = set_bits[set] ? bits.get(set_bits[set]) : 0;
		}
	}

	void MidSideButterfly(float *left, float *right, uint32 n) {
		VDASSERT(!(n & 1));

		static const float invsqrt2 = 0.70710678118654752440084436210485f;

		for(unsigned i=0; i<n; i += 2) {
			const float x0 = left [i+0] * invsqrt2;
			const float y0 = right[i+0] * invsqrt2;
			const float x1 = left [i+1] * invsqrt2;
			const float y1 = right[i+1] * invsqrt2;

			left [i+0] = (float)(x0+y0);
			right[i+0] = (float)(x0-y0);
			left [i+1] = (float)(x1+y1);
			right[i+1] = (float)(x1-y1);
		}
	}
}

void VDMPEGAudioDecoder::PrereadLayerIII() {
	const bool is_mpeg2 = mHeader.nMPEGVer > 1;
	const unsigned nch = mMode != 3 ? 2 : 1;

	// fill Huffman buffer

	unsigned sidelen = is_mpeg2 ? (nch>1 ? 17 : 9) : (nch>1 ? 32 : 17);
	unsigned left = mFrameDataSize - sidelen;
	const uint8 *src = mFrameBuffer + sidelen;

	while(left > 0) {
		unsigned tc = kL3BufferSize - mL3BufferPos;
		if (tc > left)
			tc = left;
		memcpy(mL3Buffer+mL3BufferPos, src, tc);
		mL3BufferPos = (mL3BufferPos + tc) & (kL3BufferSize-1);
		mL3BufferLevel += tc;
		src += tc;
		left -= tc;
	}

	if (mL3BufferLevel > kL3BufferSize)
		mL3BufferLevel = kL3BufferSize;
}

bool VDMPEGAudioDecoder::DecodeLayerIII() {
	LayerIIISideInfo sideinfo;
	const unsigned nch = mMode != 3 ? 2 : 1;
	const bool is_mpeg2 = mHeader.nMPEGVer > 1;

	if (is_mpeg2)
		DecodeSideInfoMPEG2(sideinfo, mFrameBuffer, nch);
	else
		DecodeSideInfoMPEG1(sideinfo, mFrameBuffer, nch);

	const unsigned sidelen = is_mpeg2 ? (nch>1 ? 17 : 9) : (nch>1 ? 32 : 17);

	profile_set(0);

	uint32 reservoirOffset = sideinfo.main_data_begin + (mFrameDataSize - sidelen);

	if (mL3BufferLevel < reservoirOffset)
		return false;

	VDMPEGAudioHuffBitReader bits(mL3Buffer, mL3BufferPos - reservoirOffset);

	uint8 scalefac_l[2][22]={0};
	uint8 scalefac_s[2][13][3]={0};

	// There are only 21 long bands and 12 short bands in MPEG audio, but
	// data above the tables is considered to have a scalefactor of zero.
	// The standard doesn't say whether short block reordering occurs --
	// reference appears to do it, so we'll just pretend there is an
	// extra band when dequantizing.

	static const unsigned sLongScalefactorBands[3][3][23]={
		{
			{0,4,8,12,16,20,24,30,36,44,52,62,74,90,110,134,162,196,238,288,342,418,576},	// 44.1KHz
			{0,4,8,12,16,20,24,30,36,42,50,60,72,88,106,128,156,190,230,276,330,384,576},	// 48KHz
			{0,4,8,12,16,20,24,30,36,44,54,66,82,102,126,156,194,240,296,364,448,550,576},	// 32KHz
		},
		{
			{0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},	// 22KHz
			{0,6,12,18,24,30,36,44,54,66,80,96,114,136,162,194,232,278,332,394,464,540,576},	// 24KHz
			{0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},	// 16KHz
		},
		{
			{0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},		// 11KHz
			{0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},		// 12KHz
			{0,12,24,36,48,60,72,88,108,132,160,192,232,280,336,400,476,566,568,570,572,574,576},	// 8KHz
		}
	};

	static const unsigned sShortScalefactorBands[3][3][14]={
		{
			{0,4,8,12,16,22,30,40,52,66,84,106,136,192},	// 44.1KHz
			{0,4,8,12,16,22,28,38,50,64,80,100,126,192},	// 48KHz
			{0,4,8,12,16,22,30,42,58,78,104,138,180,192},	// 32KHz
		},
		{
			{0,4,8,12,18,24,32,42,56,74,100,132,174,192},	// 22KHz
			{0,4,8,12,18,26,36,48,62,80,104,136,180,192},	// 24KHz
			{0,4,8,12,18,26,36,48,62,80,104,134,174,192},	// 16KHz
		},
		{
			{0,4,8,12,18,26,36,48,62,80,104,136,174,192},	// 11KHz
			{0,4,8,12,18,26,36,48,62,80,104,136,174,192},	// 12KHz
			{0,8,16,24,36,52,72,96,124,160,162,164,166,192},	// 8KHz
		}
	};

	unsigned mpegtype = mHeader.lSamplingFreq < 16000 ? 2 : is_mpeg2 ? 1 : 0;

	const unsigned *const pLongBands = sLongScalefactorBands[mpegtype][mSamplingRateIndex];
	const unsigned *const pShortBands = sShortScalefactorBands[mpegtype][mSamplingRateIndex];

	for(unsigned gr=0; gr<(is_mpeg2 ? 1U : 2U); ++gr) {
		float (&recon)[2][576] = mL3Data.recon;

		unsigned ch;
		unsigned ms_bound;
		unsigned is_band;
		unsigned gr_zero_bound = 0;

		for(ch=0; ch<nch; ++ch) {
			const LayerIIIRegionSideInfo& rsi = sideinfo.gr[gr][ch];
			unsigned region_end = bits.pos() + rsi.part2_3_length;
			float (&reconch)[576] = recon[ch];

			if (is_mpeg2)
				DecodeScalefactorsMPEG2(sideinfo.gr[gr][ch], bits, &scalefac_l[ch][0], &scalefac_s[ch][0][0], mModeExtension, ch);
			else
				DecodeScalefactorsMPEG1(sideinfo, bits, &scalefac_l[ch][0], &scalefac_s[ch][0][0], gr, ch);

			VDASSERT(scalefac_l[ch][21] == 0);
			VDASSERT(scalefac_s[ch][12][0] == 0);

			profile_add(p_scalefac);

			// decode big huffman-encoded samples -- three regions
			sint32 freq[576+4];
			unsigned region1_start;
			unsigned region2_start;
			unsigned count1_start	= 2*rsi.big_values;

			if (count1_start > 576)
				throw (int)ERR_INVALIDDATA;

			if (rsi.window_switching_flag) {
				const unsigned block_type = rsi.switched.block_type;

				// Sadly, although the breakpoint is conveniently at frequency
				// line 36 in both cases in MPEG-1, in MPEG-2 the long window
				// has band 8 at line 56 instead, and in MPEG-2.5 the short
				// window breakpoint is at line 72.

				region1_start = block_type == 2 ? pShortBands[3]*3 : pLongBands[8];
				region2_start = count1_start;
			} else {
				// check for invalid regions
				if (rsi.unswitched.region0_count + rsi.unswitched.region1_count + 2 >= 23)
					throw (int)ERR_INVALIDDATA;

				region1_start = pLongBands[rsi.unswitched.region0_count + 1];
				region2_start = pLongBands[rsi.unswitched.region0_count + rsi.unswitched.region1_count + 2];
			}

			if (region1_start > count1_start)
				region1_start = count1_start;
			if (region2_start > count1_start)
				region2_start = count1_start;

			// reject invalid table 14
			if ((rsi.table_select[0] == 4 || rsi.table_select[0] == 14) && region1_start > 0)
				throw (int)ERR_INVALIDDATA;
			if ((rsi.table_select[1] == 4 || rsi.table_select[1] == 14) && region1_start != region2_start)
				throw (int)ERR_INVALIDDATA;
			if ((rsi.table_select[2] == 4 || rsi.table_select[2] == 14) && region2_start != count1_start)
				throw (int)ERR_INVALIDDATA;

			if (bits.pos() > region_end)
				throw (int)ERR_INVALIDDATA;

			DecodeHuffmanValues(bits, freq, rsi.table_select[0], region1_start >> 1);

			if (bits.pos() > region_end)
				throw (int)ERR_INVALIDDATA;

			DecodeHuffmanValues(bits, freq + region1_start, rsi.table_select[1], (region2_start - region1_start) >> 1);

			if (bits.pos() > region_end)
				throw (int)ERR_INVALIDDATA;

			DecodeHuffmanValues(bits, freq + region2_start, rsi.table_select[2], (count1_start - region2_start) >> 1);

			//if (bits.pos() > region_end)
			//	throw (int)ERR_INVALIDDATA;

			profile_add(p_huffdec);

			// decode little huffman-encoded samples
			
			uint32 val = bits.peek(32);

			unsigned zero_bound;
			{
				unsigned i = count1_start;

				if (rsi.count1table_select) {		// inverted 4-bit table
					while(i<=576-4 && bits.pos() < region_end) {
						unsigned c = bits.get(4);
						int w=0, x=0, y=0, z=0;

						if (!(c & 8))
							w = bits.get(1) ? -1 : 1;
						if (!(c & 4))
							x = bits.get(1) ? -1 : 1;
						if (!(c & 2))
							y = bits.get(1) ? -1 : 1;
						if (!(c & 1))
							z = bits.get(1) ? -1 : 1;

						freq[i++] = w;
						freq[i++] = x;
						freq[i++] = y;
						freq[i++] = z;
					}
				} else {
					const L3HuffmanTableDescriptor& tablec1 = sL3HuffmanTables[32];

					while(i<=576-4 && bits.pos() < region_end) {
						unsigned idx = 0;
						
						while(tablec1.table[idx][0])
							idx += tablec1.table[idx][bits.get(1)];

						unsigned c = tablec1.table[idx][1];
						int w=0, x=0, y=0, z=0;

						if (c & 8)
							w = bits.get(1) ? -1 : 1;
						if (c & 4)
							x = bits.get(1) ? -1 : 1;
						if (c & 2)
							y = bits.get(1) ? -1 : 1;
						if (c & 1)
							z = bits.get(1) ? -1 : 1;

						freq[i++] = w;
						freq[i++] = x;
						freq[i++] = y;
						freq[i++] = z;
					}
				}

				zero_bound = i;
				while(zero_bound > 0 && !freq[zero_bound-1])
					--zero_bound;

				if (zero_bound > gr_zero_bound)
					gr_zero_bound = zero_bound;

				if (ch) {
					unsigned sfb;

					// hmm... what band is it in?

					if (rsi.window_switching_flag && rsi.switched.block_type==2) {
						if (rsi.switched.mixed_block_flag && zero_bound <= 36) {	// mixed block - 35 scalefactor bands
							const unsigned long_bands = is_mpeg2 ? 6 : 8;

							for(sfb = 0; sfb < long_bands; ++sfb)
								if (zero_bound <= pLongBands[sfb])
									break;

							ms_bound = pLongBands[sfb];
							is_band = sfb;
						} else {								// short block - 12 scalefactor bands + end band
							for(sfb = 0; sfb < 13; ++sfb)
								if (zero_bound <= pShortBands[sfb]*3)
									break;

							ms_bound = pShortBands[sfb]*3;
							is_band = sfb;
						}
					} else {									// long block - 21 scalefactor bands + end band
						for(sfb = 0; sfb < 22; ++sfb)
							if (zero_bound <= pLongBands[sfb])
								break;

						ms_bound = pLongBands[sfb];
						is_band = sfb;
					}
				}

				if (i < 576)
					memset(freq+i, 0, sizeof(freq[0])*(576-i));

				VDASSERT(i >= 574 || bits.pos() == region_end);

				bits.seek(region_end);
			}

			profile_add(p_huffdec1);

			// requantization

			{
				static const uint8 pretab[22]={0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,2,2,3,3,3,2};
				static const float log2 = 0.693147180559945309417232f;

				static const float inv_pow2[36]={
					1.0f,					0.7071067691f,
					0.5f,					0.3535533845f,
					0.25f,					0.1767766923f,
					0.125f,					0.08838834614f,
					0.0625f,				0.04419417307f,
					0.03125f,				0.02209708653f,
					0.015625f,				0.01104854327f,
					0.0078125f,				0.005524271633f,
					0.00390625f,			0.002762135817f,
					0.001953125f,			0.001381067908f,
					0.0009765625f,			0.0006905339542f,
					0.00048828125f,			0.0003452669771f,
					0.000244140625f,		0.0001726334885f,
					0.0001220703125f,		8.631674427e-005f,
					6.103515625e-005f,		4.315837214e-005f,
					3.051757813e-005f,		2.157918607e-005f,
					1.525878906e-005f,		1.078959303e-005f,
					7.629394531e-006f,		5.394796517e-006f,
				};

				bool short_blocks = rsi.window_switching_flag && rsi.switched.block_type == 2;
				unsigned long_bands = short_blocks ? rsi.switched.mixed_block_flag ? is_mpeg2 ? 6 : 8 : 0 : 21;
				float global_gain = expf(log2 * 0.25f*(float)(int)(rsi.global_gain - 210));
				int scalefac_shift = rsi.scalefac_scale ? 1 : 0;
				float scalefac_scale = rsi.scalefac_scale ? -1.0f : -0.5f;
				float gain;

				// long bands
				unsigned i = 0;

				for(unsigned sfb=0; sfb<long_bands; ++sfb) {
					unsigned band_end = pLongBands[sfb+1];

					if (band_end > zero_bound)
						band_end = zero_bound;

					int sf = scalefac_l[ch][sfb];		// 0-15

					if (rsi.preflag)
						sf += pretab[sfb];

					gain = global_gain * inv_pow2[sf << scalefac_shift];

					if (i >= count1_start) {
						if (i >= zero_bound)
							break;

						const float recon_tab[3]={-gain, 0.f, +gain};

						for(; i < band_end; i += 2) {
							reconch[i  ] = recon_tab[freq[i  ]+1];
							reconch[i+1] = recon_tab[freq[i+1]+1];
						}
					} else {
						for(; i < band_end; ++i) {
							VDASSERT(abs(freq[i]) < 8192);

							sint32 x = freq[i];
							float y;

							if ((unsigned)(x+128) < 256)
								y = mL3Pow43Tab[x+128];
							else {
//								y = powf(fabsf((float)x), 4.0f/3.0f);

								// compute c = |x|^4
								float c = (float)x;
								c = c*c;
								c = c*c;

								// compute initial approximation to c^(-1/3) = x^(-4^3)
								union {
									float f;
									int i;
								} conv = { c };

								conv.i = 0x54a21cf0 - conv.i / 3;

								// refine using two Newton-Raphson iterations -- maximum relative error over
								// [0,8192] is 0.00221%.
								float x = conv.f;
								float x2;

								x2 = x*x;
								x = (4.0f / 3.0f)*x - (c*(1.0f/3.0f))*(x2*x2);
								x2 = x*x;
								x = (4.0f / 3.0f)*x - (c*(1.0f/3.0f))*(x2*x2);

								// compute y ~= x^(4/3)
								y = c * x * x;

								// invert as necessary

								static const float signs[2]={-1,1};
								y *= signs[(freq[i] >> 31) + 1];
							}

							reconch[i] = y * gain;
						}
					}
				}

				if (i < 576)
					memset(reconch + i, 0, sizeof(reconch[0])*(576 - i));

				if (short_blocks) {
					int sfb;

					if (rsi.switched.mixed_block_flag)
						sfb = 3;
					else
						sfb = 0;

					for(; sfb < 13; ++sfb) {
						gain = global_gain;

						unsigned i = pShortBands[sfb]*3;

						for(unsigned window=0; window<3; ++window) {
							int sf = scalefac_s[ch][sfb][window];

							gain = global_gain * powf(2.0f, scalefac_scale * sf - 2*rsi.switched.subblock_gain[window]);

							unsigned j = pShortBands[sfb]*3 + window;

							while(j < pShortBands[sfb+1]*3) {
								float y = (float)pow(fabs((double)freq[i]), 4.0/3.0) * gain;

								VDASSERT(abs(freq[i]) < 8192);

								if (freq[i]<0)
									y = -y;

								reconch[j] = (float)y;
								++i;
								j += 3;
							}
						}
					}
				}
			}

			profile_add(p_dequan);

		}

		// stereo processing
		//
		// We can do this after reordering because short block reordering
		// swaps around samples within a scalefactor band, and the switch
		// from MS to IS only occurs between bands.
		//
		// XXX: There is still a bug here with MPEG-2 intensity stereo
		//		decoding -- specifically, the zero bound that switches
		//		IS on is per-window in MPEG-2, but this code only handles
		//		MPEG-1 mode. MPEG-2 files with intensity stereo are a
		//		bit hard to come by since LAME doesn't use IS....

		if (mMode == 1) {					// joint stereo mode -- mode_ext = MS/IS
			static const float invsqrt2 = 0.70710678118654752440084436210485f;

			if (mModeExtension & 2) {		// mid/side stereo mode enabled
				if (mModeExtension == 2)
					ms_bound = 576;

				MidSideButterfly(recon[0], recon[1], ms_bound);
			}

			if (mModeExtension & 1) {		// intensity stereo mode enabled

				// Unfortunately, 11172-3 doesn't say whether IS positions
				// above 7 are valid; they are reachable in low subbands
				// with a low ms_bound.  Most of them are benign except for
				// 9, which has infinite gain -- presumably this is the
				// "I'm yelling inside your head" position.  Oh well.

				static const float is_left_tab[2][16]={
					{	// MPEG-1
						0.0f,
						0.211324865405187f,
						0.366025403784439f,
						0.5f,
						0.633974596215561f,
						0.788675134594813f,
						1.0f,
						1.36602540378444f,		// 7 (illegal)
						2.36602540378444f,
						0.0f,					// 9 - -1/0.  Ugh.
						-1.36602540378444f,
						-0.366025403784439f,
						0.0f,
						0.211324865405187f,
						0.366025403784439f,
						0.5f,
					},
					{	// MPEG-2			powers of 1/root-root-2
						1.00000000000000,
						0.84089641525371,
						1.0,
						0.70710678118655,
						1.0,
						0.59460355750136,
						1.0,
						0.50000000000000,
						1.0,
						0.42044820762686,
						1.0,
						0.35355339059327,
						1.0,
						0.29730177875068,
						1.0,
						0.21022410381343,
					}
				};

				static const float is_right_tab[2][16]={
					{	// MPEG-1
						1.0f,
						0.788675134594813f,
						0.633974596215561f,
						0.5f,
						0.366025403784439f,
						0.211324865405187f,
						0.0f,
						-0.366025403784438f,		// 7 (illegal)
						-1.36602540378444f,
						0.0f,					// 9 - 1/0.  Ugh.
						2.36602540378444f,
						1.36602540378444f,
						1.0f,
						0.788675134594813f,
						0.633974596215562f,
						0.5f,
					},
					{	// MPEG-2			powers of 1/root-root-2
						1.00000000000000,
						1.0,
						0.84089641525371,
						1.0,
						0.70710678118655,
						1.0,
						0.59460355750136,
						1.0,
						0.50000000000000,
						1.0,
						0.42044820762686,
						1.0,
						0.35355339059327,
						1.0,
						0.29730177875068,
						1.0,
					}
				};

				// PITA: IS can occur in short blocks, and we need to know that
				// since the scalefactors are involved.  The right channel
				// determines the switch.

				const LayerIIIRegionSideInfo& rsi = sideinfo.gr[gr][1];

				unsigned long_band_start = 0;
				unsigned long_band_end = 0;
				unsigned short_band_start = 0;
				unsigned short_band_end = 0;

				// Frequency lines above the last scalefactor band inherit is_pos[]
				// from it.

				if (rsi.window_switching_flag && rsi.switched.block_type == 2) {
					if (rsi.switched.mixed_block_flag) {
						if (ms_bound <= pLongBands[8]) {
							long_band_start		= is_band;
							long_band_end		= 8;
							short_band_start	= 3;
						} else {
							short_band_start	= is_band;
						}
						short_band_end		= 13;
					} else {
						short_band_start	= is_band;
						short_band_end		= 13;
					}
					scalefac_s[1][12][0] = scalefac_s[1][11][0];
					scalefac_s[1][12][1] = scalefac_s[1][11][1];
					scalefac_s[1][12][2] = scalefac_s[1][11][2];
				} else {
					// dist10 appears to have a bug -- long blocks can't have IS in sfband 0.
					if (!is_band)
						++is_band;

					long_band_start = is_band;
					long_band_end	= 22;

					scalefac_l[1][21] = scalefac_l[1][20];
				}

				unsigned sfb;

				for(sfb=long_band_start; sfb<long_band_end; ++sfb) {
					const unsigned is_pos = scalefac_l[1][sfb];

					if (is_pos != 7) {
						const unsigned band_start = pLongBands[sfb];
						const unsigned band_end = pLongBands[sfb+1];
						float coleft = is_left_tab[is_mpeg2][is_pos];
						float coright = is_right_tab[is_mpeg2][is_pos];

						if (is_mpeg2 && (sideinfo.gr[gr][1].scalefac_compress & 1)) {
							coleft *= coleft;
							coright *= coright;
						}

						for(unsigned i = band_start; i<band_end; ++i) {
							const float x = recon[0][i];

							recon[0][i] = x*coleft;
							recon[1][i] = x*coright;
						}
					} else if (mModeExtension & 2) {
						const unsigned band_start = pLongBands[sfb];
						const unsigned band_end = pLongBands[sfb+1];

						for(unsigned i = band_start; i<band_end; ++i) {
							const float x = recon[0][i] * invsqrt2;
							const float y = recon[1][i] * invsqrt2;

							recon[0][i] = x+y;
							recon[1][i] = x-y;
						}
					}
				}

				for(sfb=short_band_start; sfb<short_band_end; ++sfb) {
					for(unsigned window=0; window<3; ++window) {
						const unsigned is_pos = scalefac_s[1][sfb][window];

						if (is_pos != 7) {
							const unsigned band_start = pShortBands[sfb]*3 + window;
							const unsigned band_end = pShortBands[sfb+1]*3 + window;
							float coleft = is_left_tab[is_mpeg2][is_pos];
							float coright = is_right_tab[is_mpeg2][is_pos];

							if (is_mpeg2 && (sideinfo.gr[gr][1].scalefac_compress & 1)) {
								coleft *= coleft;
								coright *= coright;
							}

							for(unsigned i=band_start; i<band_end; i+=3) {
								const float x = recon[0][i];

								recon[0][i] = x*coleft;
								recon[1][i] = x*coright;
							}
						} else if (mModeExtension & 2) {
							const unsigned band_start = pShortBands[sfb]*3 + window;
							const unsigned band_end = pShortBands[sfb+1]*3 + window;

							for(unsigned i=band_start; i<band_end; i+=3) {
								const float x = recon[0][i] * invsqrt2;
								const float y = recon[1][i] * invsqrt2;

								recon[0][i] = (float)(x+y);
								recon[1][i] = (float)(x-y);
							}
						}
					}
				}
			}
		}

		profile_add(p_stereo);

		// alias reduction, IMDCT and polyphase

		float subbands[18][2][32];

		for(ch=0; ch<nch; ++ch) {
			const LayerIIIRegionSideInfo& rsi = sideinfo.gr[gr][ch];
			unsigned wintype = 0;
			
			if (rsi.window_switching_flag)
				wintype = rsi.switched.block_type;

			for(unsigned sb=0; sb<32; ++sb)	{
				if (wintype == 2 && (!rsi.switched.mixed_block_flag || sb >= 2)) {
					IMDCT_6_3(&recon[ch][sb*18], (float(*)[2][32])&subbands[0][ch][sb], mL3OverlapBuffer[ch][sb], mL3Windows[wintype]);
				} else {
					if (sb < 31) {
						static const float cs_tab[8]={
							0.85749292571254f,
							0.88174199731771f,
							0.94962864910273f,
							0.98331459249179f,
							0.99551781606759f,
							0.99916055817815f,
							0.99989919524445f,
							0.99999315507028f
						};

						static const float ca_tab[8]={
							-0.51449575542753f,
							-0.47173196856497f,
							-0.31337745420390f,
							-0.18191319961098f,
							-0.09457419252642f,
							-0.04096558288530f,
							-0.01419856857247f,
							-0.00369997467376f
						};

						float *p = &recon[ch][sb*18+18];

						struct local {
							static inline void antialias(float& x, float& y, float cs, float ca) {
								const float xt = x, yt = y;

								x = xt*cs - yt*ca;
								y = xt*ca + yt*cs;
							}
						};

						local::antialias(p[-1], p[0], cs_tab[0], ca_tab[0]);
						local::antialias(p[-2], p[1], cs_tab[1], ca_tab[1]);
						local::antialias(p[-3], p[2], cs_tab[2], ca_tab[2]);
						local::antialias(p[-4], p[3], cs_tab[3], ca_tab[3]);
						local::antialias(p[-5], p[4], cs_tab[4], ca_tab[4]);
						local::antialias(p[-6], p[5], cs_tab[5], ca_tab[5]);
						local::antialias(p[-7], p[6], cs_tab[6], ca_tab[6]);
						local::antialias(p[-8], p[7], cs_tab[7], ca_tab[7]);
					}

					if (sb*18 >= gr_zero_bound+35) {
						IMDCT_18_Null((float(*)[2][32])&subbands[0][ch][sb], mL3OverlapBuffer[ch][sb]);
					} else {
						IMDCT_18(&recon[ch][sb*18], (float(*)[2][32])&subbands[0][ch][sb], mL3OverlapBuffer[ch][sb], mL3Windows[wintype]);
					}
				}
			}
		}

		profile_add(p_hybrid);

		for(unsigned s=0; s<18; ++s) {
			if (s & 1) {
				for(unsigned sb=1; sb<32; sb+=2) {
					subbands[s][0][sb] = -subbands[s][0][sb];
					subbands[s][1][sb] = -subbands[s][1][sb];
				}
			}

			if (nch>1) {
				mpPolyphaseFilter->Generate(subbands[s][0], subbands[s][1], mpSampleDst);
				mpSampleDst += 64;
			} else {
				mpPolyphaseFilter->Generate(subbands[s][0], NULL, mpSampleDst);
				mpSampleDst += 32;
			}
		}

		profile_add(p_polyphase);
	}

#ifdef RDTSC_PROFILE

	if (!(++p_frames & 127)) {
		static char buf[256];

		sprintf(buf, "%d frames: total %I64d, scalefac %d%%, huffdec %d%%/%d%%, dequan %d%%, stereo %d%%, hybrid %d%% (%lu), poly %d%%\n"
				,p_frames
				,p_total
				,(long)((p_scalefac*100)/p_total)
				,(long)((p_huffdec*100)/p_total)
				,(long)((p_huffdec1*100)/p_total)
				,(long)((p_dequan*100)/p_total)
				,(long)((p_stereo*100)/p_total)
				,(long)((p_hybrid*100)/p_total)
				,(long)p_hybrid
				,(long)((p_polyphase*100)/p_total)
				);
		OutputDebugString(buf);
	}
#endif

	mSamplesDecoded += (is_mpeg2 ? 576 : 1152) * nch;

	return true;
}
