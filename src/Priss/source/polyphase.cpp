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

#include <string.h>
#include <math.h>
#include <malloc.h>
#include <vd2/system/cpuaccel.h>
#include "polyphase.h"

#ifdef _M_AMD64
extern "C" void vdasm_mpegaudio_polyphase_dct4x8(float *f);
extern "C" void vdasm_mpegaudio_polyphase_dctinputbutterflies(float *out, const float *in);
extern "C" void vdasm_mpegaudio_polyphase_matrixout_stereo(const float (*pSrc)[16], const float *pWinFwd, const float *pWinRev, int inc, const uint32 *pSampleInv, const sint16 *pDst, const float (*pSrcFinal)[16], const uint32 *pFinalMask);
#endif

extern "C" {
	extern const float __declspec(align(16)) leecoef1[16]={		// 1/(2 cos (pi/64*(2i+1)))
		// first eight (forward)
		0.50060299823520f,
		0.50547095989754f,
		0.51544730992262f,
		0.53104259108978f,
		0.55310389603444f,
		0.58293496820613f,
		0.62250412303566f,
		0.67480834145501f,

		// last eight (backward)
		10.19000812354803f,
		3.40760841846872f,
		2.05778100995341f,
		1.48416461631417f,
		1.16943993343288f,
		0.97256823786196f,
		0.83934964541553f,
		0.74453627100230f,
	};

	extern const float __declspec(align(16)) leecoef2[8]={		// 1/(2 cos (pi/32*(2i+1)))
		0.50241928618816f,
		0.52249861493969f,
		0.56694403481636f,
		0.64682178335999f,
		0.78815462345125f,
		1.06067768599035f,
		1.72244709823833f,
		5.10114861868916f,
	};
}

namespace {
	// Dewindowing coefficients (synthesis filter).
	//
	// There are two significant features of ISO 11172-3 table B.3:
	//
	//   1) All the values are multiples of 1/65536.
	//   2) The FIR filter is antisymmetric about D[256].
	//
	// We store the first half of the coefficients scaled up by 64K.

	static const int sFilter[17][16] = {
	{  0, -29, 213, -459, 2037, -5153,  6574,-37489,75038,37489, 6574, 5153, 2037, 459,213,29},
	{ -1, -31, 218, -519, 2000, -5517,  5959,-39336,74992,35640, 7134, 4788, 2063, 401,208,26},
	{ -1, -35, 222, -581, 1952, -5879,  5288,-41176,74856,33791, 7640, 4425, 2080, 347,202,24},
	{ -1, -38, 225, -645, 1893, -6237,  4561,-43006,74630,31947, 8092, 4063, 2087, 294,196,21},
	{ -1, -41, 227, -711, 1822, -6589,  3776,-44821,74313,30112, 8492, 3705, 2085, 244,190,19},
	{ -1, -45, 228, -779, 1739, -6935,  2935,-46617,73908,28289, 8840, 3351, 2075, 197,183,17},
	{ -1, -49, 228, -848, 1644, -7271,  2037,-48390,73415,26482, 9139, 3004, 2057, 153,176,16},
	{ -2, -53, 227, -919, 1535, -7597,  1082,-50137,72835,24694, 9389, 2663, 2032, 111,169,14},
	{ -2, -58, 224, -991, 1414, -7910,    70,-51853,72169,22929, 9592, 2330, 2001,  72,161,13},
	{ -2, -63, 221,-1064, 1280, -8209,  -998,-53534,71420,21189, 9750, 2006, 1962,  36,154,11},
	{ -2, -68, 215,-1137, 1131, -8491, -2122,-55178,70590,19478, 9863, 1692, 1919,   2,147,10},
	{ -3, -73, 208,-1210,  970, -8755, -3300,-56778,69679,17799, 9935, 1388, 1870, -29,139, 9},
	{ -3, -79, 200,-1283,  794, -8998, -4533,-58333,68692,16155, 9966, 1095, 1817, -57,132, 8},
	{ -4, -85, 189,-1356,  605, -9219, -5818,-59838,67629,14548, 9959,  814, 1759, -83,125, 7},
	{ -4, -91, 177,-1428,  402, -9416, -7154,-61289,66494,12980, 9916,  545, 1698,-106,117, 7},
	{ -5, -97, 163,-1498,  185, -9585, -8540,-62684,65290,11455, 9838,  288, 1634,-127,111, 6},
	{ -5,-104, 146,-1567,  -45, -9727, -9975,-64019,64019, 9975, 9727,   45, 1567,-146,104, 5},
	};
}

void *VDMPEGAudioPolyphaseFilter::operator new(size_t s) {
	return _aligned_malloc(s, 32);
}

void VDMPEGAudioPolyphaseFilter::operator delete(void *p) {
	_aligned_free(p);
}

VDMPEGAudioPolyphaseFilter *VDMPEGAudioPolyphaseFilter::Create() {
	switch(GetOptMode()) {
	case kOptModeSSE:
		return new VDMPEGAudioPolyphaseFilterSSE;
	default:
		return new VDMPEGAudioPolyphaseFilterFPU;
	}
}

VDMPEGAudioPolyphaseFilter::VDMPEGAudioPolyphaseFilter() {
	unsigned i, j;

	for(i=0; i<17; ++i) {
		for(j=0; j<16; ++j)
			mFilter[i][j] = mFilter[i][j+16] = sFilter[i][j] * 0.5f; // * (1.0 / 65536.0);
	}

	Reset();
}

VDMPEGAudioPolyphaseFilter::OptMode VDMPEGAudioPolyphaseFilter::GetOptMode() {
	long optflags = CPUGetEnabledExtensions();

	if (optflags & CPUF_SUPPORTS_SSE)
		return kOptModeSSE;
	else
		return kOptModeFPU;
}

void VDMPEGAudioPolyphaseFilter::Reset() {
	mWindowPos = 0;
	memset(&mWindow, 0, sizeof mWindow);
}

// Matrixing phase
//
// The matrixing phase of 11172-3 is a 32-point DCT transform that produces
// 64 output values.  As there are only 32 input values, 32 of the output
// values are redundant and need not be generated.  To compute the DCT32,
// we apply two iterations of Byeong Gi Lee's decomposition to break it down
// into four 8-point DCTs, then apply the 8-point AAN DCT.
//
// The Lee decomposition consists of five steps:
//
// 1) Butterfly (x+y, x-y) between low and high halves to create even and
//    odd parts.
// 2) Multiply odd part by 1/(2 cos(pi/2N*(2i+1)))
// 3) DCT on both parts.
// 4) Shift-and-add elements in odd half.
// 5) Interleave.
//
// Lee can be used to break down the 8-point DCT as well in 29a12m, but
// it requires three layers of multiplications as well as a high number
// of butterflies, whereas LLM and AAN require less data movement. We do
// not have a quantization stage so AAN is 29a13m instead of 29a5m, but
// LLM has a (2 sqrt 2) scaling factor makes it also 29a13m.  The LLM
// has more rotators which are expensive latency-wise, so we use AAN.

void VDMPEGAudioPolyphaseFilter::DCT4x8(float *x) {
	static const float cosvals[8]={		// cos(x*pi/16)
		1.f,
		0.98078528040323043f,
		0.92387953251128674f,
		0.83146961230254524f,
		0.70710678118654757f,
		0.55557023301960229f,
		0.38268343236508984f,
		0.19509032201612833f,
	};

	// This is essentially the AAN algorithm, except that leave the rotator
	// as 2a4m, giving us a total of 28a6m.  The operations have been
	// finessed a bit as well to reduce data movement.

	// perform B3

	float (*const s)[4] = (float (*)[4])x;
	
	for(unsigned i=0; i<4; ++i) {
		// perform B3 (8a)
		const float b3[8] = {
			s[0][i]+s[7][i],
			s[1][i]+s[6][i],
			s[2][i]+s[5][i],
			s[3][i]+s[4][i],
			s[0][i]-s[7][i],
			s[1][i]-s[6][i],
			s[2][i]-s[5][i],
			s[3][i]-s[4][i],
		};

		// perform B2, part of ~B1 (7a)

		const float b2[8] = {
			b3[0]+b3[3],
			b3[1]+b3[2],
			b3[0]-b3[3],
			b3[1]-b3[2],
			b3[4]-b3[6],
			b3[5]+b3[7],
			b3[6],
			b3[7]-b3[4],
		};

		// perform part of ~B1, M (5a6m)

		const float m[8] = {
			b2[0]+b2[1],
			b2[0]-b2[1],
			(b2[2]-b2[3])*cosvals[4],
			b2[3],
			b2[4]*cosvals[6] + b2[5]*cosvals[2],
			b2[4]*cosvals[2] - b2[5]*cosvals[6],
			b2[6],
			b2[7]*cosvals[4],
		};

		// perform R1 (pass 1) (4a)

		const float r1a[8]={
			m[0],
			m[1],
			m[2]+m[3],
			m[2]-m[3],
			m[4],
			m[5],
			m[6]+m[7],
			m[6]-m[7],
		};

		// perform R1 (pass 2) (4a)

		const float r1[8]={
			r1a[0],
			r1a[1],
			r1a[2],
			r1a[3],
			r1a[7]+r1a[4],
			r1a[6]+r1a[5],
			r1a[6]-r1a[5],
			r1a[7]-r1a[4],
		};

		// perform D

		static const float d[8]={		// [1 .5 .5 -.5 .5 .5 .5 .5] ./ cos(pi*[0 5 6 1 4 7 2 3]/16)
			1.00000000000000f,
			0.89997622313642f,
			1.30656296487638f,
			-0.50979557910416f,
			0.70710678118655f,
			2.56291544774151f,
			0.54119610014620f,
			0.60134488693505f,
		};

		// perform P

		s[0][i] = r1[0] * d[0];
		s[1][i] = r1[4] * d[1];
		s[2][i] = r1[2] * d[2];
		s[3][i] = r1[6] * d[3];
		s[4][i] = r1[1] * d[4];
		s[5][i] = r1[5] * d[5];
		s[6][i] = r1[3] * d[6];
		s[7][i] = r1[7] * d[7];
	}
}

#ifdef _M_AMD64
	void VDMPEGAudioPolyphaseFilterSSE::DCT4x8(float *x) {
		vdasm_mpegaudio_polyphase_dct4x8(x);
	}
#else
	void __declspec(naked) VDMPEGAudioPolyphaseFilterSSE::DCT4x8(float *x) {
		static const __declspec(align(16)) float c2[4]={0.92387953251128674f,0.92387953251128674f,0.92387953251128674f,0.92387953251128674f};
		static const __declspec(align(16)) float c4[4]={0.70710678118654757f,0.70710678118654757f,0.70710678118654757f,0.70710678118654757f};
		static const __declspec(align(16)) float c6[4]={0.38268343236508984f,0.38268343236508984f,0.38268343236508984f,0.38268343236508984f};

		static const __declspec(align(16)) float d[8][4]={		// [1 .5 .5 -.5 .5 .5 .5 .5] ./ cos(pi*[0 5 6 1 4 7 2 3]/16)
	#define TE(x) x,x,x,x
			TE(1.00000000000000f),
			TE(0.89997622313642f),
			TE(1.30656296487638f),
			TE(-0.50979557910416f),
			TE(0.70710678118655f),
			TE(2.56291544774151f),
			TE(0.54119610014620f),
			TE(0.60134488693505f),
	#undef TE
		};

		// See the FPU version to get an idea of the flow of this AAN
		// implementation.  Note that we do all four DCTs in parallel!

		__asm {
			mov		ecx, [esp+4]

			;even part - B3 (4a)
			movaps	xmm0, [ecx+0*16]		;xmm0 = s[0]
			movaps	xmm1, [ecx+1*16]		;xmm1 = s[1]
			movaps	xmm2, [ecx+2*16]		;xmm2 = s[2]
			movaps	xmm3, [ecx+3*16]		;xmm3 = s[3]
			addps	xmm0, [ecx+7*16]		;xmm0 = s[0]+s[7]
			addps	xmm1, [ecx+6*16]		;xmm1 = s[1]+s[6]
			addps	xmm2, [ecx+5*16]		;xmm2 = s[2]+s[5]
			addps	xmm3, [ecx+4*16]		;xmm3 = s[3]+s[4]

			;even part - B2/~B1a (4a)
			movaps	xmm4, xmm0
			addps	xmm0, xmm3				;xmm0 = b2[0] = b3[0]+b3[3]
			movaps	xmm5, xmm1
			addps	xmm1, xmm2				;xmm1 = b2[1] = b3[1]+b2[2]
			subps	xmm4, xmm3				;xmm4 = b2[2] = b3[0]-b3[3]
			subps	xmm5, xmm2				;xmm5 = b2[3] = b3[1]-b3[2]

			;even part - ~B1b/M (3a1m)
			movaps	xmm2, xmm0
			subps	xmm4, xmm5
			addps	xmm0, xmm1				;xmm0 = m[0] = b2[0] + b2[1]
			mulps	xmm4, c4				;xmm4 = m[2] = (b2[2] - b2[3])*c4
			subps	xmm2, xmm1				;xmm2 = m[1] = b2[0] - b2[1]

			;even part - R1 (2a)
			movaps	xmm3, xmm4
			subps	xmm4, xmm5				;xmm4 = r1[3] = m[2]-m[3]
			addps	xmm3, xmm5				;xmm3 = r1[2] = m[2]+m[3]

			;even part - d (4m)
			mulps	xmm0, [d+0*16]			;xmm0 = out[0] = r1[0]*d[0]
			mulps	xmm2, [d+4*16]			;xmm2 = out[4] = r1[1]*d[4]
			mulps	xmm3, [d+2*16]			;xmm3 = out[2] = r1[2]*d[2]
			mulps	xmm4, [d+6*16]			;xmm4 = out[6] = r1[3]*d[6]

			;odd part - B3 (4a)
			movaps	xmm1, [ecx+0*16]
			movaps	xmm5, [ecx+1*16]
			movaps	xmm6, [ecx+2*16]
			movaps	xmm7, [ecx+3*16]
			subps	xmm1, [ecx+7*16]		;xmm1 = b3[4] = s[0]-s[7]
			subps	xmm5, [ecx+6*16]		;xmm5 = b3[5] = s[1]-s[6]
			subps	xmm6, [ecx+5*16]		;xmm6 = b3[6] = s[2]-s[5]
			subps	xmm7, [ecx+4*16]		;xmm7 = b3[7] = s[3]-s[4]

			;even part - writeout
			movaps	[ecx+0*16], xmm0
			movaps	[ecx+4*16], xmm2
			movaps	[ecx+2*16], xmm3
			movaps	[ecx+6*16], xmm4

			;odd part - B2/~B1a (3a)
			addps	xmm5, xmm7				;xmm5 = b2[5] = b3[5]+b3[7]
			subps	xmm7, xmm1				;xmm7 = b2[7] = b3[7]-b3[4]
			subps	xmm1, xmm6				;xmm1 = b2[4] = b3[4]-b3[6]

			;odd part - ~B1b/M (2a5m)
			movaps	xmm0, xmm1
			mulps	xmm7, c4				;xmm7 = m[7] = c4*b2[7]
			movaps	xmm2, xmm5
			mulps	xmm0, c6
			mulps	xmm1, c2
			mulps	xmm2, c2
			mulps	xmm5, c6
			addps	xmm0, xmm2				;xmm0 = m[4] = c6*b2[4] + c2*b2[5]
			subps	xmm1, xmm5				;xmm1 = m[5] = c2*b2[4] - c6*b2[5]

			;odd part - R1a (2a)
			movaps	xmm5, xmm6
			addps	xmm6, xmm7				;xmm6 = r1a[6] = m[6]+m[7]
			subps	xmm5, xmm7				;xmm5 = r1a[7] = m[6]-m[7]

			;odd part - R1b (4a)
			movaps	xmm3, xmm5
			movaps	xmm4, xmm6
			subps	xmm5, xmm0				;xmm5 = r1b[7] = r1a[7]-r1a[4]
			subps	xmm6, xmm1				;xmm6 = r1b[6] = r1a[6]-r1a[5]
			addps	xmm4, xmm1				;xmm4 = r1b[5] = r1a[6]+r1a[5]
			addps	xmm3, xmm0				;xmm3 = r1b[4] = r1a[7]+r1a[4]

			;odd part - D (4a)
			mulps	xmm3, [d+1*16]
			mulps	xmm4, [d+5*16]
			mulps	xmm6, [d+3*16]
			mulps	xmm5, [d+7*16]

			;odd part - writeout
			movaps	[ecx+1*16], xmm3
			movaps	[ecx+5*16], xmm4
			movaps	[ecx+3*16], xmm6
			movaps	[ecx+7*16], xmm5

			ret		4
		}
	}
#endif

namespace {
#ifdef _M_AMD64
	static void __stdcall DCTInputButterfliesSSE(float x[32], const float in[32]) {
		vdasm_mpegaudio_polyphase_dctinputbutterflies(x, in);
	}
#else
	static void __declspec(naked) __stdcall DCTInputButterfliesSSE(float x[32], const float in[32]) {
		__asm {
			push	ebx
			mov		eax, [esp+8+4]
			mov		ebx, [esp+4+4]
			xor		ecx, ecx
			mov		edx, 48
xloop:
			movups	xmm0, [eax+ecx]			;xmm0 = in[i]
			movups	xmm1, [eax+edx]			;xmm1 = in[15-i]
			movups	xmm2, [eax+ecx+64]		;xmm2 = in[i+16]
			movups	xmm3, [eax+edx+64]		;xmm3 = in[31-i]
			shufps	xmm1, xmm1, 00011011b
			shufps	xmm3, xmm3, 00011011b

			;butterfly for first decomposition
			movaps	xmm4, xmm0
			movaps	xmm5, xmm1
			addps	xmm0, xmm3				;xmm0 = y0 = x0+x3
			addps	xmm1, xmm2				;xmm1 = y1 = x1+x2
			subps	xmm4, xmm3				;xmm4 = y2 = x0-x3
			subps	xmm5, xmm2				;xmm5 = y3 = x1-x2
			mulps	xmm4, [leecoef1+ecx]
			mulps	xmm5, [leecoef1+ecx+32]

			;butterfly for second decomposition
			movaps	xmm2, xmm0
			movaps	xmm3, xmm4
			addps	xmm0, xmm1				;xmm0 = z0 = y0+y1
			subps	xmm2, xmm1				;xmm2 = z1 = y0-y1
			addps	xmm3, xmm5				;xmm3 = z2 = y2+y3
			subps	xmm4, xmm5				;xmm4 = z3 = y2-y3
			mulps	xmm2, [leecoef2+ecx]
			mulps	xmm4, [leecoef2+ecx]

			;interleave in 0-2-1-3 order
			movaps		xmm1, xmm0
			unpcklps	xmm0, xmm3			;xmm0 = z2B | z0B | z2A | z0A
			unpckhps	xmm1, xmm3			;xmm1 = z2D | z0D | z2C | z0C
			movaps		xmm6, xmm2
			unpcklps	xmm2, xmm4			;xmm2 = z3B | z1B | z3A | z1A
			unpckhps	xmm6, xmm4			;xmm6 = z3D | z1D | z3C | z1C

			movlps	[ebx   ], xmm0
			movlps	[ebx+ 8], xmm2
			movhps	[ebx+16], xmm0
			movhps	[ebx+24], xmm2
			movlps	[ebx+32], xmm1
			movlps	[ebx+40], xmm6
			movhps	[ebx+48], xmm1
			movhps	[ebx+56], xmm6

			add		ebx, 64
			add		ecx, 16
			sub		edx, 16
			cmp		ecx, edx
			jb		xloop

			pop		ebx
			ret		8
		}
	}
#endif
}

void VDMPEGAudioPolyphaseFilterSSE::DCTInputButterflies(float x[32], const float in[32]) {
	DCTInputButterfliesSSE(x, in);
}

void VDMPEGAudioPolyphaseFilter::DCTInputButterflies(float x[32], const float in[32]) {
	for(unsigned i=0; i<8; ++i) {
		const float x0 = in[i];
		const float x1 = in[15-i];
		const float x2 = in[i+16];
		const float x3 = in[31-i];

		// butterfly for first decomposition
		const float y0 =  x0+x3;
		const float y1 =  x1+x2;
		const float y2 = (x0-x3)*leecoef1[i];
		const float y3 = (x1-x2)*leecoef1[i+8];		// last 8 are reflected in table

		// butterfly for second decomposition
		const float z0 =  y0+y1;
		const float z1 = (y0-y1)*leecoef2[i];
		const float z2 =  y2+y3;
		const float z3 = (y2-y3)*leecoef2[i];

		// store with output reordering
		x[i*4+0] = z0;
		x[i*4+1] = z2;
		x[i*4+2] = z1;
		x[i*4+3] = z3;
	}
}

void VDMPEGAudioPolyphaseFilter::Matrix(const float *in, bool stereo, int ch) {
	unsigned i;
	float __declspec(align(16)) x[32];

	// do input butterflies to split down to four 8-point DCTs
	DCTInputButterflies(x, in);

	// do four 8-point AAN DCTs
	DCT4x8(x);

	// offset addition for odd 8-point matrices
	x[ 2] += x[ 6];
	x[ 3] += x[ 7];

	x[ 6] += x[10];
	x[ 7] += x[11];

	x[10] += x[14];
	x[11] += x[15];

	x[14] += x[18];
	x[15] += x[19];

	x[18] += x[22];
	x[19] += x[23];

	x[22] += x[26];
	x[23] += x[27];

	x[26] += x[30];
	x[27] += x[31];

	// offset addition for odd 16-point matrix, as well as reflection of odd
	// sample sets to aid the synthesis filter bank

	if (stereo) {
		float (*out)[2][16] = (float (*)[2][16])&mWindow.stereo[0][ch][mWindowPos];

		if (mWindowPos & 1) {
			out[0 ][0][0] = x[0];
			out[31][0][0] = x[1] + x[3];

			for (i = 2; i < 30; i+=2) {
				out[32-i][0][0] = x[i];
				out[31-i][0][0] = x[i+1] + x[i+3];
			}

			out[2][0][0] = x[30];
			out[1][0][0] = x[31];
		} else {
			for (i = 0; i < 30; i+=2) {
				out[i+0][0][0] = x[i];
				out[i+1][0][0] = x[i+1] + x[i+3];
			}

			out[30][0][0] = x[30];
			out[31][0][0] = x[31];
		}
	} else {
		float (*out)[16] = (float (*)[16])&mWindow.mono[0][mWindowPos];

		if (mWindowPos & 1) {
			out[0 ][0] = x[0];
			out[31][0] = x[1] + x[3];

			for (i = 2; i < 30; i+=2) {
				out[32-i][0] = x[i];
				out[31-i][0] = x[i+1] + x[i+3];
			}

			out[2][0] = x[30];
			out[1][0] = x[31];
		} else {
			for (i = 0; i < 30; i+=2) {
				out[i+0][0] = x[i];
				out[i+1][0] = x[i+1] + x[i+3];
			}

			out[30][0] = x[30];
			out[31][0] = x[31];
		}
	}
}

// Subband synthesis filter strategy
//
// Since we only stored half of the 64-point DCT output in the synthesis
// window, we must emulate the 32>64 conversion. From Intel's AP-533:
//
//		x[ 0..15] =  x'[16..31]
//      x[16]     = 0
//      x[17..31] = -x'[31..17]
//      x[32..48] = -x'[16.. 0]
//		x[48..63] = -x'[ 0..15]
//
// Even offsets use x[0..31], while odd offsets use x[32..63].  There is
// symmetry in the sample generation:
//
// even x[ 0]     =  x'[16]
//		x[ 1..15] =  x'[16 + 1..15]
//      x[16]     = 0
//      x[17..31] = -x'[16 + 15..1]
//
// odd  x[32]     = -x'[16]
//      x[33..47] = -x'[16 - (1..15)]
//      x[48]     = -x'[ 0]
//		x[49..63] = -x'[16 - (15..1)]
//
// So we can split the reconstruction into three phases: s[0], s[16], and
// a phase that generates s[1..15] and s[31..17] at the same time, in
// opposite directions.  As it turns out, the subband synthesis window is
// antisymmetric around D[256], meaning that row 17 is the negative
// reverse of row 15, row 18 is the negative reverse of row 14, etc. and
// we can reuse window rows in reverse as well.  We now have only 17
// window rows to deal with.
//
// Getting rid of the sliding window is easy: use a circular array and
// double up all the window rows so we can choose a contiguous array of
// 16 window elements for any offset of the sliding window.  Next.
//
// That leaves the question of how to deal with the annoying flip/flop
// of the reconstruction filter between the first and second halves of
// the matrixing stage output.  The V[] matrix looks like this in
// the standard:
//
//		A1
//			A2
//		B1
//			B2
//		C1
//			C2
//		D1
//			D2
//
// On the first run we would process A1-B2-C1-D2, on the second run
// B1-C2-D1-E2, and so on.  However, if we swap the halves on every
// other block:
//
//		A1
//			A2
//		B2
//			B1
//		C1
//			C2
//		D2
//			D1
//
// Then the hopping is easy -- we just process all primary halves on even
// passes and all secondary halves on odd passes.
//
// Finally, to eliminate the redundant 32 elements per set: notice that
// our traversal over odd elements is mirrored from even ones -- the first
// 16 samples draw from x[16..31] on even sets and x[16..1] on odd sets,
// mirrored around x[16].  So we just mirror every other set on write and
// mirror direction for every other set on read.  We will unfortunately
// have to have two loops for sliding window offset polarity since
// x[0..15] and x[33..48] are opposite sign but x[17..31] and x[48..63]
// are not.

namespace {
#if 1
	int clip(float f) {
		static const float bias = 8388608.0f + 4194304.0f + 32768.0f; // 2^23 + 2^22
		float tmp = f + bias;
		int v = reinterpret_cast<sint32&>(tmp) - 0x4b400000;

		if ((unsigned)v >= 65536)
			v = (~v >> 31)&0xffff;

		return v - 0x8000;
	}
#else
	int clip(float f) {
		int v = (int)f;

		if (v < -32768)
			v = -32768;
		else if (v > 32767)
			v = 32767;

		return v;
	}
#endif
}

void VDMPEGAudioPolyphaseFilter::SynthesizeMono(sint16 *dst) {
	bool odd = (mWindowPos & 1) != 0;
	int sample;

	// first sample is special case
	{
		const float *window_fwd = &mFilter[0][(-(int)mWindowPos)&15];
		const float *src = &mWindow.mono[16][0];
		float left = 0;

		for(int i=0; i<16; i+=2) {
			left  += src[i+0] * window_fwd[i];
			left  -= src[i+1] * window_fwd[i+1];
		}

		if (odd) {
			left = -left;
		}

		dst[ 0] = clip(left);
	}

	// reflected samples
	for(sample=1; sample<16; ++sample) {
		const float *window_fwd = &mFilter[sample][(-(int)mWindowPos)&15];
		const float *window_rev = &mFilter[sample][mWindowPos&15];
		const float *src = mWindow.mono[odd ? 16-sample : 16+sample];
		float left1 = 0, left2 = 0;

		for(int i=0; i<16; i+=2) {
			left1  += src[i+0] * window_fwd[i];
			left1  -= src[i+1] * window_fwd[i+1];

			left2  += src[i+0] * window_rev[15-i];
			left2  += src[i+1] * window_rev[14-i];
		}

		if (odd) {
			left1 = -left1;
		}

		dst[ 0+sample] = clip(left1);
		dst[32-sample] = clip(left2);
	}

	// center sample is special case
	{
		const float *window_fwd = &mFilter[16][(-(int)mWindowPos)&15];
		const float *src = mWindow.mono[0];
		float left = 0;

		for(int i=!odd; i<16; i+=2) {
			left  -= src[i] * window_fwd[i];
		}

		dst[16] = clip(left);
	}
}

void VDMPEGAudioPolyphaseFilter::SynthesizeStereo(sint16 *dst) {
	bool odd = (mWindowPos & 1) != 0;
	int sample;

	// first sample is special case
	{
		const float *window_fwd = &mFilter[0][(-(int)mWindowPos)&15];
		const float (*src)[16] = &mWindow.stereo[16][0];
		float left = 0, right = 0;

		for(int i=0; i<16; i+=2) {
			left  += src[0][i+0] * window_fwd[i];
			right += src[1][i+0] * window_fwd[i];
			left  -= src[0][i+1] * window_fwd[i+1];
			right -= src[1][i+1] * window_fwd[i+1];
		}

		if (odd) {
			left = -left;
			right = -right;
		}

		dst[ 0] = clip(left);
		dst[ 1] = clip(right);
	}

	// reflected samples
	for(sample=1; sample<16; ++sample) {
		const float *window_fwd = &mFilter[sample][(-(int)mWindowPos)&15];
		const float *window_rev = &mFilter[sample][mWindowPos&15];
		const float (*src)[16] = mWindow.stereo[odd ? 16-sample : 16+sample];
		float left1 = 0, right1 = 0, left2 = 0, right2 = 0;

		for(int i=0; i<16; i+=2) {
			left1  += src[0][i+0] * window_fwd[i];
			right1 += src[1][i+0] * window_fwd[i];
			left1  -= src[0][i+1] * window_fwd[i+1];
			right1 -= src[1][i+1] * window_fwd[i+1];

			left2  += src[0][i+0] * window_rev[15-i];
			right2 += src[1][i+0] * window_rev[15-i];
			left2  += src[0][i+1] * window_rev[14-i];
			right2 += src[1][i+1] * window_rev[14-i];
		}

		if (odd) {
			left1 = -left1;
			right1 = -right1;
		}

		dst[ 0+sample*2] = clip(left1);
		dst[ 1+sample*2] = clip(right1);
		dst[64-sample*2] = clip(left2);
		dst[65-sample*2] = clip(right2);
	}

	// center sample is special case
	{
		const float *window_fwd = &mFilter[16][(-(int)mWindowPos)&15];
		const float (*src)[16] = &mWindow.stereo[0][0];
		float left = 0, right = 0;

		for(int i=!odd; i<16; i+=2) {
			left  -= src[0][i] * window_fwd[i];
			right -= src[1][i] * window_fwd[i];
		}

		dst[32] = clip(left);
		dst[33] = clip(right);
	}
}

namespace {
#ifndef _M_AMD64
	void __declspec(naked) __stdcall ComputeSamplesStereoSSE(const float (*pSrc)[16], const float *pWinFwd, const float *pWinRev, int inc, const uint32 *pSampleInv, const sint16 *pDst, const float (*pSrcFinal)[16], const uint32 *pFinalMask) {
		static const __declspec(align(16)) uint32 invother[4]={ 0, 0x80000000, 0, 0x80000000 };
		static const __declspec(align(16)) uint32 invall[4]={ 0x80000000, 0x80000000, 0x80000000, 0x80000000 };
		__asm {
			push	ebp
			push	edi
			push	esi
			push	ebx

			mov		eax, [esp+4+16]			;eax = pointer to subband samples
			mov		ebx, [esp+20+16]		;ebx = pointer to sample inversion value
			mov		ecx, [esp+8+16]			;ecx = pointer to forward window
			mov		edx, [esp+12+16]		;edx = pointer to reverse window
			mov		esi, [esp+24+16]		;esi = pointer to first two forward destination samples
			mov		ebp, [esp+16+16]		;ebp = source increment
			lea		edi, [esi+120]			;edi = pointer to first two reverse destination samples

			;compute first sample (0)

			movaps	xmm5, invother
			movups	xmm0, [ecx]				;load window samples 0-3
			xorps	xmm0, xmm5				;toggle signs on odd window samples
			movaps	xmm1, xmm0
			mulps	xmm0, [eax]				;multiply by left subband samples
			mulps	xmm1, [eax+64]			;multiply by right subband samples
			movups	xmm2, [ecx+16]			;load window samples 4-7
			xorps	xmm2, xmm5				;toggle signs on odd window samples
			movaps	xmm3, xmm2
			mulps	xmm2, [eax+16]			;multiply by left subband samples
			mulps	xmm3, [eax+80]			;multiply by right subband samples
			addps	xmm0, xmm2
			addps	xmm1, xmm3
			movups	xmm2, [ecx+32]			;load window samples 8-11
			xorps	xmm2, xmm5				;toggle signs on odd window samples
			movaps	xmm3, xmm2
			mulps	xmm2, [eax+32]			;multiply by left subband samples
			mulps	xmm3, [eax+96]			;multiply by right subband samples
			addps	xmm0, xmm2
			addps	xmm1, xmm3
			movups	xmm2, [ecx+48]			;load window samples 12-15
			xorps	xmm2, xmm5				;toggle signs on odd window samples
			movaps	xmm3, xmm2
			mulps	xmm2, [eax+48]			;multiply by left subband samples
			mulps	xmm3, [eax+112]			;multiply by right subband samples
			addps	xmm0, xmm2
			addps	xmm1, xmm3

			movaps	xmm2, xmm0				;xmm2 = l3 | l2 | l1 | l0
			movlhps	xmm0, xmm1				;xmm0 = r1 | r0 | l1 | l0
			movhlps	xmm1, xmm2				;xmm1 = r3 | r2 | l3 | l2
			addps	xmm0, xmm1				;xmm0 = r1+r3 | r0+r2 | l1+l3 | l0+l2
			shufps	xmm0, xmm0, 11011000b	;xmm0 = r1+r3 | l1+l3 | r0+r2 | l0+l2
			movhlps	xmm3, xmm0				;xmm3 =   ?   |   ?   | r1+r3 | l1+l3
			movaps	xmm4, [ebx]
			movhlps	xmm4, xmm4
			addps	xmm0, xmm3				;xmm0 = ? | ? | r | l
			xorps	xmm0, xmm4
			cvtps2pi	mm0, xmm0
			packssdw	mm0, mm0
			movd	[esi-4], mm0

			add		ecx, 128
			add		edx, 128
			add		eax, ebp

			;compute reflected samples (1-15, 17-31)
xloop:
			movups	xmm2, [edx+48]
			shufps	xmm2, xmm2, 00011011b	;xmm2 = reverse window
			movups	xmm3, [ecx]				;xmm3 = forward window
			xorps	xmm3, invother			;negate every other sample in forward window
			movaps	xmm0, [eax]				;xmm0 = left source
			movaps	xmm1, xmm0
			mulps	xmm0, xmm2
			mulps	xmm1, xmm3
			movaps	xmm4, xmm0				;xmm4 = left forward
			movaps	xmm5, xmm1				;xmm5 = left reverse
			movaps	xmm0, [eax+64]			;xmm0 = left source
			movaps	xmm1, xmm0
			mulps	xmm0, xmm2
			mulps	xmm1, xmm3
			movaps	xmm6, xmm0				;xmm6 = right forward
			movaps	xmm7, xmm1				;xmm7 = right reverse

			movups	xmm2, [edx+32]
			shufps	xmm2, xmm2, 00011011b	;xmm2 = reverse window
			movups	xmm3, [ecx+16]			;xmm3 = forward window
			xorps	xmm3, invother			;negate every other sample in forward window
			movaps	xmm0, [eax+16]			;xmm0 = left source
			movaps	xmm1, xmm0
			mulps	xmm0, xmm2
			mulps	xmm1, xmm3
			addps	xmm4, xmm0				;xmm4 += left forward
			addps	xmm5, xmm1				;xmm5 += left reverse
			movaps	xmm0, [eax+80]			;xmm0 = left source
			movaps	xmm1, xmm0
			mulps	xmm0, xmm2
			mulps	xmm1, xmm3
			addps	xmm6, xmm0				;xmm6 += right forward
			addps	xmm7, xmm1				;xmm7 += right reverse

			movups	xmm2, [edx+16]
			shufps	xmm2, xmm2, 00011011b	;xmm2 = reverse window
			movups	xmm3, [ecx+32]			;xmm3 = forward window
			xorps	xmm3, invother			;negate every other sample in forward window
			movaps	xmm0, [eax+32]			;xmm0 = left source
			movaps	xmm1, xmm0
			mulps	xmm0, xmm2
			mulps	xmm1, xmm3
			addps	xmm4, xmm0				;xmm4 += left forward
			addps	xmm5, xmm1				;xmm5 += left reverse
			movaps	xmm0, [eax+96]			;xmm0 = left source
			movaps	xmm1, xmm0
			mulps	xmm0, xmm2
			mulps	xmm1, xmm3
			addps	xmm6, xmm0				;xmm6 += right forward
			addps	xmm7, xmm1				;xmm7 += right reverse

			movups	xmm2, [edx]
			shufps	xmm2, xmm2, 00011011b	;xmm2 = reverse window
			movups	xmm3, [ecx+48]			;xmm3 = forward window
			xorps	xmm3, invother			;negate every other sample in forward window
			movaps	xmm0, [eax+48]			;xmm0 = left source
			movaps	xmm1, xmm0
			mulps	xmm0, xmm2
			mulps	xmm1, xmm3
			addps	xmm4, xmm0				;xmm4 += left forward
			addps	xmm5, xmm1				;xmm5 += left reverse
			movaps	xmm0, [eax+112]			;xmm0 = left source
			movaps	xmm1, xmm0
			mulps	xmm0, xmm2
			mulps	xmm1, xmm3
			addps	xmm6, xmm0				;xmm6 += right forward
			addps	xmm7, xmm1				;xmm7 += right reverse

			movaps	xmm0, xmm4				;xmm0 = lf3 | lf2 | lf1 | lf0
			movaps	xmm1, xmm5
			movlhps	xmm0, xmm6				;xmm0 = rf0 | rf1 | lf1 | lf0
			movlhps	xmm1, xmm7
			movhlps	xmm6, xmm4				;xmm6 = rf3 | rf2 | lf3 | lf2
			movhlps	xmm7, xmm5
			addps	xmm0, xmm6				;xmm0 = rf0+rf3 | rf1+rf2 | lf1+lf3 | lf0+lf2
			addps	xmm1, xmm7
			movaps	xmm2, xmm0
			movaps	xmm3, xmm1
			shufps	xmm0, xmm0, 10110001b	;xmm0 = rf1+rf2 | rf0+rf3 | lf0+lf2 | lf1+lf3
			shufps	xmm1, xmm1, 10110001b
			addps	xmm0, xmm2				;xmm0 = rf | rf | lf | lf
			addps	xmm1, xmm3				;xmm1 = rb | rb | lb | lb
			shufps	xmm0, xmm1, 10001000b	;xmm0 = rf | lf | rb | lb
			xorps	xmm0, [ebx]
			cvtps2pi	mm0, xmm0
			movhlps	xmm0, xmm0
			packssdw	mm0, mm0
			cvtps2pi	mm1, xmm0
			packssdw	mm1, mm1
			movd	[esi], mm1
			movd	[edi], mm0

			add		esi,4
			sub		edi,4
			add		eax,ebp
			add		ecx,128
			add		edx,128
			cmp		esi,edi
			jne		xloop

			;do last sample (16)
			mov		eax, [esp+28+16]
			mov		edi, [esp+32+16]
			movaps	xmm5, [edi]				;load final mask (masks out every other sample)

			movups	xmm0, [ecx]				;load window samples 0-3
			andps	xmm0, xmm5				;mask out every other sample
			movaps	xmm1, xmm0
			mulps	xmm0, [eax]				;multiply by left subband samples
			mulps	xmm1, [eax+64]			;multiply by right subband samples
			movups	xmm2, [ecx+16]			;load window samples 4-7
			andps	xmm2, xmm5				;mask out every other sample
			movaps	xmm3, xmm2
			mulps	xmm2, [eax+16]			;multiply by left subband samples
			mulps	xmm3, [eax+80]			;multiply by right subband samples
			addps	xmm0, xmm2
			addps	xmm1, xmm3
			movups	xmm2, [ecx+32]			;load window samples 8-11
			andps	xmm2, xmm5				;mask out every other sample
			movaps	xmm3, xmm2
			mulps	xmm2, [eax+32]			;multiply by left subband samples
			mulps	xmm3, [eax+96]			;multiply by right subband samples
			addps	xmm0, xmm2
			addps	xmm1, xmm3
			movups	xmm2, [ecx+48]			;load window samples 12-15
			andps	xmm2, xmm5				;mask out every other sample
			movaps	xmm3, xmm2
			mulps	xmm2, [eax+48]			;multiply by left subband samples
			mulps	xmm3, [eax+112]			;multiply by right subband samples
			addps	xmm0, xmm2
			addps	xmm1, xmm3

			movaps	xmm2, xmm0				;xmm2 = l3 | l2 | l1 | l0
			movlhps	xmm0, xmm1				;xmm0 = r1 | r0 | l1 | l0
			movhlps	xmm1, xmm2				;xmm1 = r3 | r2 | l3 | l2
			addps	xmm0, xmm1				;xmm0 = r1+r3 | r0+r2 | l1+l3 | l0+l2
			shufps	xmm0, xmm0, 11011000b	;xmm0 = r1+r3 | l1+l3 | r0+r2 | l0+l2
			movhlps	xmm3, xmm0				;xmm3 =   ?   |   ?   | r1+r3 | l1+l3
			addps	xmm0, xmm3				;xmm0 = ? | ? | r | l
			xorps	xmm0, invall
			cvtps2pi	mm0, xmm0
			packssdw	mm0, mm0
			movd	[esi], mm0

			emms
			pop		ebx
			pop		esi
			pop		edi
			pop		ebp
			ret		32
		}
	}
#endif
};

void VDMPEGAudioPolyphaseFilterSSE::SynthesizeStereo(sint16 *dst) {
	bool odd = (mWindowPos & 1) != 0;

	// reflected samples
	static const __declspec(align(16)) uint32 invert[4]={0,0,0x80000000,0x80000000};
	static const __declspec(align(16)) uint32 noinvert[4]={0,0,0,0};
	static const __declspec(align(16)) uint32 evenmask[4]={~0,0,~0,0};
	static const __declspec(align(16)) uint32 oddmask[4]={0,~0,0,~0};

#ifdef _M_AMD64
	vdasm_mpegaudio_polyphase_matrixout_stereo(
					mWindow.stereo[16],
					&mFilter[0][(-(int)mWindowPos)&15],
					&mFilter[0][mWindowPos&15],
					odd ? -(int)sizeof(mWindow.stereo[0]) : +sizeof(mWindow.stereo[0]),
					odd ? invert : noinvert,
					&dst[2],
					mWindow.stereo[0],
					odd ? evenmask : oddmask);
#else
	ComputeSamplesStereoSSE(
					mWindow.stereo[16],
					&mFilter[0][(-(int)mWindowPos)&15],
					&mFilter[0][mWindowPos&15],
					odd ? -(int)sizeof(mWindow.stereo[0]) : +sizeof(mWindow.stereo[0]),
					odd ? invert : noinvert,
					&dst[2],
					mWindow.stereo[0],
					odd ? evenmask : oddmask);
#endif
}

void VDMPEGAudioPolyphaseFilter::Generate(const float left[32], const float right[32], sint16 *dst) {
	if (right) {
		Matrix(left, true, 0);
		Matrix(right, true, 1);

		SynthesizeStereo(dst);
	} else {
		Matrix(left, false, 0);

		SynthesizeMono(dst);
	}

	mWindowPos = (mWindowPos-1) & 15;
}
