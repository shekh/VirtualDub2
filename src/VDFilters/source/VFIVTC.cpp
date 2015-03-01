//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2009 Avery Lee
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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "stdafx.h"

#include <vd2/system/cpuaccel.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/fraction.h>
#include <vd2/system/memory.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/system/win32/intrin.h>
#include <emmintrin.h>

#include "resource.h"
#include "filter.h"

////////////////////////////////////////////////////////////

namespace {
	struct IVTCScore {
		sint64 mVar[2];
		sint64 mVarShift[2];
	};

	IVTCScore operator+(const IVTCScore& x, const IVTCScore& y) {
		IVTCScore r;

		r.mVar[0] = x.mVar[0] + y.mVar[0];
		r.mVar[1] = x.mVar[1] + y.mVar[1];
		r.mVarShift[0] = x.mVarShift[0] + y.mVarShift[0];
		r.mVarShift[1] = x.mVarShift[1] + y.mVarShift[1];
		return r;
	}

#ifdef VD_CPU_X86
	#pragma warning(disable: 4799)	// warning C4799: function '`anonymous namespace'::ComputeScanLineImprovement_XRGB8888_MMX' has no EMMS instruction

	void __declspec(naked) ComputeScanLineImprovement_X8R8G8B8_MMX(const void *src1, const void *src2, ptrdiff_t pitch, uint32 w, sint64 *var, sint64 *varshift) {
		__asm {
			push		ebx

			mov			eax,[esp+8]
			mov			ecx,[esp+16]
			mov			edx,[esp+12]
			mov			ebx,[esp+20]

			pxor		mm5,mm5
			pxor		mm6,mm6
	xloop:
			movd		mm0,[eax]
			pxor		mm7,mm7

			movd		mm1,[eax+ecx*2]
			punpcklbw	mm0,mm7			;mm0 = pA

			movd		mm2,[eax+ecx]
			punpcklbw	mm1,mm7			;mm1 = pC

			movd		mm3,[edx+ecx]
			punpcklbw	mm2,mm7			;mm2 = pB

			punpcklbw	mm3,mm7			;mm3 = pE
			paddw		mm0,mm1			;mm0 = pA + pC

			paddw		mm3,mm3			;mm3 = 2*pE
			paddw		mm2,mm2			;mm2 = 2*pB

			psubw		mm3,mm0			;mm3 = 2*pE - (pA + pC)
			psubw		mm0,mm2			;mm0 = pA + pC - 2*pB

			psllq		mm3,16
			add			eax,4

			pmaddwd		mm3,mm3			;mm3 = sq(pA + pC - 2*pE)	[mm5 mm3 ---]
			psllq		mm0,16

			pmaddwd		mm0,mm0			;mm0 = sq(pA + pC - 2*pB)	[mm0 mm5 mm3]
			add			edx,4

			paddd		mm5,mm3
			dec			ebx

			paddd		mm6,mm0
			jne			xloop

			movq		mm0, mm6
			psrlq		mm0, 32
			movq		mm1, mm5
			psrlq		mm1, 32
			paddd		mm0, mm6
			movd		eax, mm0
			paddd		mm1, mm5
			movd		edx, mm1

			mov			ecx, [esp+24]
			add			dword ptr [ecx], eax
			adc			dword ptr [ecx+4], 0

			mov			ecx, [esp+28]
			add			dword ptr [ecx], edx
			adc			dword ptr [ecx+4], 0

			pop			ebx
			ret
		}
	}
#endif

#if defined(VD_CPU_X86) || defined(VD_CPU_AMD64)
	IVTCScore ComputeScanImprovement_X8R8G8B8_SSE2(const void *src1, const void *src2, ptrdiff_t srcpitch, uint32 w, uint32 h) {
		IVTCScore score = {0};

		__m128i zero = _mm_setzero_si128();

		uint32 w2 = w >> 1;

		static const __m128i mask = { -1, -1, -1, -1, -1, -1, 0, 0, -1, -1, -1, -1, -1, -1, 0, 0 };

		bool firstfield = true;
		do {
			__m128i var = zero;
			__m128i varshift = zero;

			const uint8 *src1r0 = (const uint8 *)src1;
			const uint8 *src1r1 = src1r0 + srcpitch;
			const uint8 *src1r2 = src1r1 + srcpitch;
			const uint8 *src2r = (const uint8 *)src2 + srcpitch;
			for(uint32 x=0; x<w2; ++x) {
				__m128i rA = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r0), zero);
				__m128i rB = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r1), zero);
				__m128i rC = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r2), zero);
				__m128i rE = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src2r), zero);
				__m128i rAC = _mm_add_epi16(rA, rC);
				__m128i d1 = _mm_sub_epi16(rAC, _mm_add_epi16(rB, rB));		// combing in current frame
				__m128i d3 = _mm_sub_epi16(rAC, _mm_add_epi16(rE, rE));		// combing in merged frame

				d1 = _mm_and_si128(d1, mask);
				d3 = _mm_and_si128(d3, mask);

				var = _mm_add_epi32(var, _mm_madd_epi16(d1, d1));
				varshift = _mm_add_epi32(varshift, _mm_madd_epi16(d3, d3));

				src1r0 += 8;
				src1r1 += 8;
				src1r2 += 8;
				src2r += 8;
			}

			if (w & 1) {
				__m128i rA = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)src1r0), zero);
				__m128i rB = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)src1r1), zero);
				__m128i rC = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)src1r2), zero);
				__m128i rE = _mm_unpacklo_epi8(_mm_cvtsi32_si128(*(const int *)src2r), zero);
				__m128i rAC = _mm_add_epi16(rA, rC);
				__m128i d1 = _mm_sub_epi16(rAC, _mm_add_epi16(rB, rB));		// combing in current frame
				__m128i d3 = _mm_sub_epi16(rAC, _mm_add_epi16(rE, rE));		// combing in merged frame

				d1 = _mm_and_si128(d1, mask);
				d3 = _mm_and_si128(d3, mask);

				var = _mm_add_epi32(var, _mm_madd_epi16(d1, d1));
				varshift = _mm_add_epi32(varshift, _mm_madd_epi16(d3, d3));
			}

			src1 = (const uint8 *)src1 + srcpitch;
			src2 = (const uint8 *)src2 + srcpitch;

			var = _mm_add_epi32(var, _mm_shuffle_epi32(var, 0xee));
			varshift = _mm_add_epi32(varshift, _mm_shuffle_epi32(varshift, 0xee));
			var = _mm_add_epi32(var, _mm_shuffle_epi32(var, 0x55));
			varshift = _mm_add_epi32(varshift, _mm_shuffle_epi32(varshift, 0x55));

			uint32 ivar = _mm_cvtsi128_si32(var);
			uint32 ivarshift = _mm_cvtsi128_si32(varshift);

			if (firstfield) {
				score.mVar[0] += ivar;
				score.mVarShift[0] += ivarshift;
			} else {
				score.mVar[1] += ivar;
				score.mVarShift[1] += ivarshift;
			}

			firstfield = !firstfield;
		} while(--h);

		return score;
	}
#endif

	IVTCScore ComputeScanImprovement_XRGB8888(const void *src1, const void *src2, ptrdiff_t srcpitch, uint32 w, uint32 h) {
		IVTCScore score = {0};

#if defined(VD_CPU_X86) || defined(VD_CPU_AMD64)
		if (SSE2_enabled)
			return ComputeScanImprovement_X8R8G8B8_SSE2(src1, src2, srcpitch, w, h);
#endif

#ifdef VD_CPU_X86
		if (MMX_enabled) {
			int phase = 0;
			do {
				ComputeScanLineImprovement_X8R8G8B8_MMX(src1, src2, srcpitch, w, &score.mVar[phase], &score.mVarShift[phase]);

				phase ^= 1;
				src1 = (const char *)src1 + srcpitch;
				src2 = (const char *)src2 + srcpitch;
			} while(--h);

			__asm emms
			return score;
		}
#endif

		bool firstfield = true;
		do {
			uint32 var = 0;
			uint32 varshift = 0;

			const uint8 *src1r0 = (const uint8 *)src1;
			const uint8 *src1r1 = src1r0 + srcpitch;
			const uint8 *src1r2 = src1r1 + srcpitch;
			const uint8 *src2r = (const uint8 *)src2 + srcpitch;
			for(uint32 x=0; x<w; ++x) {
				int bA = src1r0[0];
				int gA = src1r0[1];
				int rA = src1r0[2];
				int bB = src1r1[0];
				int gB = src1r1[1];
				int rB = src1r1[2];
				int bC = src1r2[0];
				int gC = src1r2[1];
				int rC = src1r2[2];
				int bE = src2r[0];
				int gE = src2r[1];
				int rE = src2r[2];
				int rd1 = rA + rC - 2*rB;		// combing in current frame
				int gd1 = gA + gC - 2*gB;
				int bd1 = bA + bC - 2*bB;
				int rd3 = rA + rC - 2*rE;		// combing in merged frame
				int gd3 = gA + gC - 2*gE;
				int bd3 = bA + bC - 2*bE;

				var += rd1*rd1;
				var += gd1*gd1;
				var += bd1*bd1;
				varshift += rd3*rd3;
				varshift += gd3*gd3;
				varshift += bd3*bd3;

				src1r0 += 4;
				src1r1 += 4;
				src1r2 += 4;
				src2r += 4;
			}

			src1 = (const uint8 *)src1 + srcpitch;
			src2 = (const uint8 *)src2 + srcpitch;

			if (firstfield) {
				score.mVar[0] += var;
				score.mVarShift[0] += varshift;
			} else {
				score.mVar[1] += var;
				score.mVarShift[1] += varshift;
			}

			firstfield = !firstfield;
		} while(--h);

		return score;
	}

#ifdef VD_CPU_X86
	#pragma warning(disable: 4799)	// warning C4799: function '`anonymous namespace'::ComputeScanLineImprovement_XRGB8888_MMX' has no EMMS instruction

	void __declspec(naked) ComputeScanLineImprovement_L8_MMX(const void *src1, const void *src2, ptrdiff_t pitch, uint32 w, sint64 *var, sint64 *varshift) {
		__asm {
			push		ebx

			mov			eax,[esp+8]
			mov			ecx,[esp+16]
			mov			edx,[esp+12]
			mov			ebx,[esp+20]

			pxor		mm5,mm5
			pxor		mm6,mm6

			sub			ebx, 3
			jbe			xfin
	xloop:
			movd		mm0,[eax]
			pxor		mm7,mm7

			movd		mm1,[eax+ecx*2]
			punpcklbw	mm0,mm7			;mm0 = pA

			movd		mm2,[eax+ecx]
			punpcklbw	mm1,mm7			;mm1 = pC

			movd		mm3,[edx+ecx]
			punpcklbw	mm2,mm7			;mm2 = pB

			punpcklbw	mm3,mm7			;mm3 = pE
			paddw		mm0,mm1			;mm0 = pA + pC

			paddw		mm3,mm3			;mm3 = 2*pE
			paddw		mm2,mm2			;mm2 = 2*pB

			psubw		mm3,mm0			;mm3 = 2*pE - (pA + pC)
			psubw		mm0,mm2			;mm0 = pA + pC - 2*pB

			pmaddwd		mm3,mm3			;mm3 = sq(pA + pC - 2*pE)	[mm5 mm3 ---]
			add			eax,4

			pmaddwd		mm0,mm0			;mm0 = sq(pA + pC - 2*pB)	[mm0 mm5 mm3]
			add			edx,4

			paddd		mm5,mm3
			sub			ebx,4

			paddd		mm6,mm0
			ja			xloop
			jz			xend
	xfin:
			pcmpeqb		mm4, mm4
			mov			ebx, [esp+20]
			and			ebx, 3
			shl			ebx, 4
			movd		mm0, ebx
			psrlq		mm4, mm0

			movd		mm0,[eax]
			movd		mm1,[eax+ecx*2]
			punpcklbw	mm0,mm7			;mm0 = pA
			movd		mm2,[eax+ecx]
			punpcklbw	mm1,mm7			;mm1 = pC
			movd		mm3,[edx+ecx]
			punpcklbw	mm2,mm7			;mm2 = pB
			punpcklbw	mm3,mm7			;mm3 = pE
			paddw		mm0,mm1			;mm0 = pA + pC
			paddw		mm3,mm3			;mm3 = 2*pE
			paddw		mm2,mm2			;mm2 = 2*pB
			psubw		mm3,mm0			;mm3 = 2*pE - (pA + pC)
			psubw		mm0,mm2			;mm0 = pA + pC - 2*pB
			pand		mm3,mm4
			pand		mm0,mm4
			pmaddwd		mm3,mm3			;mm3 = sq(pA + pC - 2*pE)	[mm5 mm3 ---]
			pmaddwd		mm0,mm0			;mm0 = sq(pA + pC - 2*pB)	[mm0 mm5 mm3]
			paddd		mm5,mm3
			paddd		mm6,mm0
	xend:
			movq		mm0, mm6
			psrlq		mm0, 32
			movq		mm1, mm5
			psrlq		mm1, 32
			paddd		mm0, mm6
			movd		eax, mm0
			paddd		mm1, mm5
			movd		edx, mm1

			mov			ecx, [esp+24]
			add			dword ptr [ecx], eax
			adc			dword ptr [ecx+4], 0

			mov			ecx, [esp+28]
			add			dword ptr [ecx], edx
			adc			dword ptr [ecx+4], 0

			pop			ebx
			ret
		}
	}
#endif

#if defined(VD_CPU_X86) || defined(VD_CPU_AMD64)
	IVTCScore ComputeScanImprovement_L8_SSE2(const void *src1, const void *src2, ptrdiff_t srcpitch, uint32 w, uint32 h) {
		IVTCScore score = {0};

		__m128i zero = _mm_setzero_si128();

		uint32 w8 = w >> 3;

		static const uint8 kMaskArray[32] = {
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		};

		__m128i mask = _mm_loadl_epi64((const __m128i *)(kMaskArray + 2*(w & 7)));

		bool firstfield = true;
		do {
			__m128i var = zero;
			__m128i varshift = zero;

			const uint8 *src1r0 = (const uint8 *)src1;
			const uint8 *src1r1 = src1r0 + srcpitch;
			const uint8 *src1r2 = src1r1 + srcpitch;
			const uint8 *src2r = (const uint8 *)src2 + srcpitch;
			for(uint32 x=0; x<w8; ++x) {
				__m128i rA = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r0), zero);
				__m128i rB = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r1), zero);
				__m128i rC = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r2), zero);
				__m128i rE = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src2r), zero);
				__m128i rAC = _mm_add_epi16(rA, rC);
				__m128i d1 = _mm_sub_epi16(rAC, _mm_add_epi16(rB, rB));		// combing in current frame
				__m128i d3 = _mm_sub_epi16(rAC, _mm_add_epi16(rE, rE));		// combing in merged frame

				var = _mm_add_epi32(var, _mm_madd_epi16(d1, d1));
				varshift = _mm_add_epi32(varshift, _mm_madd_epi16(d3, d3));

				src1r0 += 8;
				src1r1 += 8;
				src1r2 += 8;
				src2r += 8;
			}

			if (w & 7) {
				__m128i rA = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r0), zero);
				__m128i rB = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r1), zero);
				__m128i rC = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src1r2), zero);
				__m128i rE = _mm_unpacklo_epi8(_mm_loadl_epi64((const __m128i *)src2r), zero);
				__m128i rAC = _mm_add_epi16(rA, rC);
				__m128i d1 = _mm_sub_epi16(rAC, _mm_add_epi16(rB, rB));		// combing in current frame
				__m128i d3 = _mm_sub_epi16(rAC, _mm_add_epi16(rE, rE));		// combing in merged frame

				d1 = _mm_and_si128(d1, mask);
				d3 = _mm_and_si128(d3, mask);

				var = _mm_add_epi32(var, _mm_madd_epi16(d1, d1));
				varshift = _mm_add_epi32(varshift, _mm_madd_epi16(d3, d3));
			}

			src1 = (const uint8 *)src1 + srcpitch;
			src2 = (const uint8 *)src2 + srcpitch;

			var = _mm_add_epi32(var, _mm_shuffle_epi32(var, 0xee));
			varshift = _mm_add_epi32(varshift, _mm_shuffle_epi32(varshift, 0xee));
			var = _mm_add_epi32(var, _mm_shuffle_epi32(var, 0x55));
			varshift = _mm_add_epi32(varshift, _mm_shuffle_epi32(varshift, 0x55));

			uint32 ivar = _mm_cvtsi128_si32(var);
			uint32 ivarshift = _mm_cvtsi128_si32(varshift);

			if (firstfield) {
				score.mVar[0] += ivar;
				score.mVarShift[0] += ivarshift;
			} else {
				score.mVar[1] += ivar;
				score.mVarShift[1] += ivarshift;
			}

			firstfield = !firstfield;
		} while(--h);

		return score;
	}
#endif

	IVTCScore ComputeScanImprovement(const void *src1, const void *src2, ptrdiff_t srcpitch, uint32 w, uint32 h) {
#if defined(VD_CPU_X86) || defined(VD_CPU_AMD64)
		if (SSE2_enabled)
			return ComputeScanImprovement_L8_SSE2(src1, src2, srcpitch, w, h);
#endif

		IVTCScore score = {0};

#ifdef VD_CPU_X86
		if (MMX_enabled) {
			int phase = 0;
			do {
				ComputeScanLineImprovement_L8_MMX(src1, src2, srcpitch, w, &score.mVar[phase], &score.mVarShift[phase]);

				phase ^= 1;
				src1 = (const char *)src1 + srcpitch;
				src2 = (const char *)src2 + srcpitch;
			} while(--h);

			__asm emms
			return score;
		}
#endif

		bool firstfield = true;
		do {
			uint32 var = 0;
			uint32 varshift = 0;

			// This is the algorithm comment from the original VideoTelecineRemover code, although
			// it only sort of applies now:
			//
			//	now using original intended algorithm, plus checking if the second frame is also combed
			//	so it doesn't easily take the frame to be dropped as the frame to be decombed. 
			//	Check below to see fix...  This actually works very well! Beats any commercial
			//	software out there!
			//
			//	Without the fix, the reason the broken algorithm sorta worked is because, in the 
			//	sequence [A1/A2] [A1/B2], if A2 sorta looked like B2, it would actually take the 
			//	right offset... but when scene changes a lot, this does not work at all. 
			//
			//	Samuel Audet <guardia@cam.órg>

			const uint8 *src1r0 = (const uint8 *)src1;
			const uint8 *src1r1 = src1r0 + srcpitch;
			const uint8 *src1r2 = src1r1 + srcpitch;
			const uint8 *src2r = (const uint8 *)src2 + srcpitch;
			for(uint32 x=0; x<w; ++x) {
				int rA = src1r0[x];
				int rB = src1r1[x];
				int rC = src1r2[x];
				int rE = src2r[x];
				int d1 = rA + rC - 2*rB;		// combing in current frame
				int d3 = rA + rC - 2*rE;		// combing in merged frame

				var += d1*d1;
				varshift += d3*d3;
			}

			src1 = (const uint8 *)src1 + srcpitch;
			src2 = (const uint8 *)src2 + srcpitch;

			if (firstfield) {
				score.mVar[0] += var;
				score.mVarShift[0] += varshift;
			} else {
				score.mVar[1] += var;
				score.mVarShift[1] += varshift;
			}

			firstfield = !firstfield;
		} while(--h);

		return score;
	}

	IVTCScore ComputeScanImprovement(const VDXPixmap& px1, const VDXPixmap& px2) {
		uint32 w = px1.w;
		uint32 h = px1.h;

		IVTCScore zero = {0};

		if (h < 16)
			return zero;

		h -= 16;

		const void *src1data  = (const char *)px1.data  + 8*px1.pitch;
		const void *src1data2 = (const char *)px1.data2 + 8*px1.pitch2;
		const void *src1data3 = (const char *)px1.data3 + 8*px1.pitch3;
		const void *src2data  = (const char *)px2.data  + 8*px2.pitch;
		const void *src2data2 = (const char *)px2.data2 + 8*px2.pitch2;
		const void *src2data3 = (const char *)px2.data3 + 8*px2.pitch3;

		VDASSERT(px1.pitch == px2.pitch);

		switch(px1.format) {
			case nsVDXPixmap::kPixFormat_XRGB8888:
				return	ComputeScanImprovement_XRGB8888(src1data, src2data, px2.pitch, w, h);

			case nsVDXPixmap::kPixFormat_YUV444_Planar:
			case nsVDXPixmap::kPixFormat_YUV444_Planar_FR:
			case nsVDXPixmap::kPixFormat_YUV444_Planar_709:
			case nsVDXPixmap::kPixFormat_YUV444_Planar_709_FR:
				return	ComputeScanImprovement(src1data,  src2data,  px2.pitch, w, h) +
						ComputeScanImprovement(src1data2, src2data2, px2.pitch2, w, h) +
						ComputeScanImprovement(src1data3, src2data3, px2.pitch3, w, h);

			case nsVDXPixmap::kPixFormat_YUV422_Planar:
			case nsVDXPixmap::kPixFormat_YUV422_Planar_FR:
			case nsVDXPixmap::kPixFormat_YUV422_Planar_709:
			case nsVDXPixmap::kPixFormat_YUV422_Planar_709_FR:
				return	ComputeScanImprovement(src1data,  src2data,  px2.pitch, w, h) +
						ComputeScanImprovement(src1data2, src2data2, px2.pitch2, w >> 1, h) +
						ComputeScanImprovement(src1data3, src2data3, px2.pitch3, w >> 1, h);

			case nsVDXPixmap::kPixFormat_YUV411_Planar:
			case nsVDXPixmap::kPixFormat_YUV411_Planar_FR:
			case nsVDXPixmap::kPixFormat_YUV411_Planar_709:
			case nsVDXPixmap::kPixFormat_YUV411_Planar_709_FR:
				return	ComputeScanImprovement(src1data,  src2data,  px2.pitch, w, h) +
						ComputeScanImprovement(src1data2, src2data2, px2.pitch2, w >> 2, h) +
						ComputeScanImprovement(src1data3, src2data3, px2.pitch3, w >> 2, h);

			case nsVDXPixmap::kPixFormat_YUV422_UYVY:
			case nsVDXPixmap::kPixFormat_YUV422_UYVY_FR:
			case nsVDXPixmap::kPixFormat_YUV422_UYVY_709:
			case nsVDXPixmap::kPixFormat_YUV422_UYVY_709_FR:
			case nsVDXPixmap::kPixFormat_YUV422_YUYV:
			case nsVDXPixmap::kPixFormat_YUV422_YUYV_FR:
			case nsVDXPixmap::kPixFormat_YUV422_YUYV_709:
			case nsVDXPixmap::kPixFormat_YUV422_YUYV_709_FR:
				return ComputeScanImprovement(src1data, src2data, px2.pitch, w*2, h);

			default:
				VDASSERT(false);
		}

		return zero;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////
	//
	//	A = U
	//	B = (U+V)/2
	//	C = (V+W)/2
	//	D = W
	//
	//	V = B + C - (A+D)/2
	//
	//	A = U
	//	B = (1-a)U + aV
	//	C = (1-a)V + aW
	//	D = W
	//	V = B + C - ((1-a)A + aD)

	void DeblurFields_Scalar(void *dst0, ptrdiff_t dstpitch, const void *srca0, const void *srcb0, const void *srcc0, const void *srcd0, ptrdiff_t srcpitch, uint32 w16, uint32 h, int weight100) {
		const uint32 w = w16 * 16;
		const ptrdiff_t srcmodulo = srcpitch - w;
		const ptrdiff_t dstmodulo = dstpitch - w;

		const uint8 *srca = (const uint8 *)srca0;
		const uint8 *srcb = (const uint8 *)srcb0;
		const uint8 *srcc = (const uint8 *)srcc0;
		const uint8 *srcd = (const uint8 *)srcd0;
		uint8 *dst = (uint8 *)dst0;

		const int mixfac = (weight100 * 128 + 50)/100;

		do {
			uint32 x = w;
			do {
				const int a = *srca++;
				const int b = *srcb++;
				const int c = *srcc++;
				const int d = *srcd++;

				const int ad = a + (((d-a)*mixfac + 0x40) >> 7);
				const int r = b + c - ad;

				*dst++ = r < 0 ? 0 : r > 255 ? 255 : (uint8)r;
			} while(--x);

			srca += srcmodulo;
			srcb += srcmodulo;
			srcc += srcmodulo;
			srcd += srcmodulo;
			dst += dstmodulo;
		} while(--h);
	}

	void DeblurFields_SSE2(void *dst0, ptrdiff_t dstpitch, const void *srca0, const void *srcb0, const void *srcc0, const void *srcd0, ptrdiff_t srcpitch, uint32 w16, uint32 h, int weight100) {
		const ptrdiff_t srcmodulo = srcpitch - 16*w16;
		const ptrdiff_t dstmodulo = dstpitch - 16*w16;

		const __m128i *srca = (const __m128i *)srca0;
		const __m128i *srcb = (const __m128i *)srcb0;
		const __m128i *srcc = (const __m128i *)srcc0;
		const __m128i *srcd = (const __m128i *)srcd0;
		__m128i *dst = (__m128i *)dst0;

		const __m128i mixfac = _mm_set1_epi16((short)((weight100 * 128 + 50)/100));
		const __m128i mixround = _mm_set1_epi16(0x0040);
		const __m128i zero = _mm_setzero_si128();

		do {
			uint32 x16 = w16;
			do {
				const __m128i a = *srca++;
				const __m128i b = *srcb++;
				const __m128i c = *srcc++;
				const __m128i d = *srcd++;

				const __m128i a_lo = _mm_unpacklo_epi8(a, zero);
				const __m128i a_hi = _mm_unpackhi_epi8(a, zero);
				const __m128i d_lo = _mm_unpacklo_epi8(d, zero);
				const __m128i d_hi = _mm_unpackhi_epi8(d, zero);

				const __m128i ad_lo = _mm_add_epi16(a_lo, _mm_srai_epi16(_mm_add_epi16(_mm_mullo_epi16(_mm_sub_epi16(d_lo, a_lo), mixfac), mixround), 7));
				const __m128i ad_hi = _mm_add_epi16(a_hi, _mm_srai_epi16(_mm_add_epi16(_mm_mullo_epi16(_mm_sub_epi16(d_hi, a_hi), mixfac), mixround), 7));

				const __m128i ad = _mm_packus_epi16(ad_lo, ad_hi);

				const __m128i b_ad_pos = _mm_subs_epu8(b, ad);
				const __m128i b_ad_neg = _mm_subs_epu8(ad, b);

				*dst++ = _mm_subs_epu8(_mm_adds_epu8(c, b_ad_pos), b_ad_neg);
			} while(--x16);

			srca = (const __m128i *)((const char *)srca + srcmodulo);
			srcb = (const __m128i *)((const char *)srcb + srcmodulo);
			srcc = (const __m128i *)((const char *)srcc + srcmodulo);
			srcd = (const __m128i *)((const char *)srcd + srcmodulo);
			dst = (__m128i *)((char *)dst + dstmodulo);
		} while(--h);
	}

	void DeblurFields(void *dst0, ptrdiff_t dstpitch, const void *srca0, const void *srcb0, const void *srcc0, const void *srcd0, ptrdiff_t srcpitch, uint32 w16, uint32 h, int weight100) {
		if (SSE2_enabled)
			DeblurFields_SSE2(dst0, dstpitch, srca0, srcb0, srcc0, srcd0, srcpitch, w16, h, weight100);
		else
			DeblurFields_Scalar(dst0, dstpitch, srca0, srcb0, srcc0, srcd0, srcpitch, w16, h, weight100);
	}

	void DeblurFrames(const VDXPixmap& pxdst, const VDXPixmap& pxsrc0, const VDXPixmap& pxsrc1, const VDXPixmap& pxsrc2, const VDXPixmap& pxsrc3, int weight) {
		const uint32 w = pxdst.w;
		const uint32 h = pxdst.h;

		switch(pxdst.format) {
			case nsVDPixmap::kPixFormat_XRGB8888:
				DeblurFields(pxdst.data, pxdst.pitch, pxsrc0.data, pxsrc1.data, pxsrc2.data, pxsrc3.data, pxsrc0.pitch, (w+3) >> 2, h, weight);
				break;

			case nsVDPixmap::kPixFormat_YUV422_UYVY:
			case nsVDPixmap::kPixFormat_YUV422_UYVY_709:
			case nsVDPixmap::kPixFormat_YUV422_UYVY_709_FR:
			case nsVDPixmap::kPixFormat_YUV422_UYVY_FR:
			case nsVDPixmap::kPixFormat_YUV422_YUYV:
			case nsVDPixmap::kPixFormat_YUV422_YUYV_709:
			case nsVDPixmap::kPixFormat_YUV422_YUYV_709_FR:
			case nsVDPixmap::kPixFormat_YUV422_YUYV_FR:
				DeblurFields(pxdst.data, pxdst.pitch, pxsrc0.data, pxsrc1.data, pxsrc2.data, pxsrc3.data, pxsrc0.pitch, (w+7) >> 3, h, weight);
				break;

			case nsVDPixmap::kPixFormat_Y8:
			case nsVDPixmap::kPixFormat_Y8_FR:
				DeblurFields(pxdst.data, pxdst.pitch, pxsrc0.data, pxsrc1.data, pxsrc2.data, pxsrc3.data, pxsrc0.pitch, (w+15) >> 4, h, weight);
				break;

			case nsVDPixmap::kPixFormat_YUV444_Planar:
			case nsVDPixmap::kPixFormat_YUV444_Planar_709:
			case nsVDPixmap::kPixFormat_YUV444_Planar_709_FR:
			case nsVDPixmap::kPixFormat_YUV444_Planar_FR:
			case nsVDPixmap::kPixFormat_YUV422_Planar:
			case nsVDPixmap::kPixFormat_YUV422_Planar_709:
			case nsVDPixmap::kPixFormat_YUV422_Planar_709_FR:
			case nsVDPixmap::kPixFormat_YUV422_Planar_FR:
			case nsVDPixmap::kPixFormat_YUV420_Planar:
			case nsVDPixmap::kPixFormat_YUV420_Planar_709:
			case nsVDPixmap::kPixFormat_YUV420_Planar_709_FR:
			case nsVDPixmap::kPixFormat_YUV420_Planar_FR:
			case nsVDPixmap::kPixFormat_YUV411_Planar:
			case nsVDPixmap::kPixFormat_YUV411_Planar_709:
			case nsVDPixmap::kPixFormat_YUV411_Planar_709_FR:
			case nsVDPixmap::kPixFormat_YUV411_Planar_FR:
			case nsVDPixmap::kPixFormat_YUV410_Planar:
			case nsVDPixmap::kPixFormat_YUV410_Planar_709:
			case nsVDPixmap::kPixFormat_YUV410_Planar_709_FR:
			case nsVDPixmap::kPixFormat_YUV410_Planar_FR:
				{
					const VDPixmapFormatInfo& info = VDPixmapGetInfo(pxdst.format);
					uint32 wc16 = ((w - 1) >> (info.auxwbits + 4)) + 1;
					uint32 hc = ((h - 1) >> info.auxhbits) + 1;

					DeblurFields(pxdst.data, pxdst.pitch, pxsrc0.data, pxsrc1.data, pxsrc2.data, pxsrc3.data, pxsrc0.pitch, (w+15) >> 4, hc, weight);
					DeblurFields(pxdst.data2, pxdst.pitch2, pxsrc0.data2, pxsrc1.data2, pxsrc2.data2, pxsrc3.data2, pxsrc0.pitch2, wc16, hc, weight);
					DeblurFields(pxdst.data3, pxdst.pitch3, pxsrc0.data3, pxsrc1.data3, pxsrc2.data3, pxsrc3.data3, pxsrc0.pitch3, wc16, hc, weight);
				}
				break;
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////////////
	//
	// Corr(Y,X) = Cov(X,Y) / SD(X)SD(Y)
	//  = [E(XY)-E(X)E(Y)] / sqrt[Var(X)Var(Y)]
	//  = [E(XY)-E(X)E(Y)] / sqrt[(E(X^2)-E(X)^2)(E(Y^2)-E(Y)^2)]
	//  = [Sum(XY)/N-Sum(X)Sum(Y)/N^2] / sqrt[(Sum(X^2)/N-Sum(X)^2/N^2)(Sum(Y^2)/N-Sum(Y)^2/N^2)]
	//  = [N*Sum(XY)-Sum(X)Sum(Y)]/N^2 / sqrt[(N*Sum(X^2)-Sum(X)^2)(N*Sum(Y^2)-Sum(Y)^2)]/N^2
	//  = [N*Sum(XY)-Sum(X)Sum(Y)] / sqrt[(N*Sum(X^2)-Sum(X)^2)(N*Sum(Y^2)-Sum(Y)^2)]

	double ComputePlaneCorrelation_Scalar(const void *srca0, const void *srcb0, ptrdiff_t srcpitch, uint32 w16, uint32 h) {
		const uint32 w = w16 * 16;
		const ptrdiff_t srcmodulo = srcpitch - w;
		const double N = (double)w16 * (double)h * 16;

		const uint8 *srca = (const uint8 *)srca0;
		const uint8 *srcb = (const uint8 *)srcb0;
		sint64 xsum = 0;
		sint64 ysum = 0;
		sint64 x2sum = 0;
		sint64 y2sum = 0;
		sint64 xysum = 0;

		do {
			sint32 xrowsum = 0;
			sint32 yrowsum = 0;
			sint32 x2rowsum = 0;
			sint32 y2rowsum = 0;
			sint32 xyrowsum = 0;

			uint32 x = w;
			do {
				const int x = *srca++;
				const int y = *srcb++;

				xrowsum += x;
				yrowsum += y;
				x2rowsum += x*x;
				y2rowsum += y*y;
				xyrowsum += x*y;
			} while(--x);

			xsum  += xrowsum;
			ysum  += yrowsum;
			x2sum += x2rowsum;
			y2sum += y2rowsum;
			xysum += xyrowsum;

			srca += srcmodulo;
			srcb += srcmodulo;
		} while(--h);

		// [N*Sum(XY)-Sum(X)Sum(Y)] / sqrt[(N*Sum(X^2)-Sum(X)^2)(N*Sum(Y^2)-Sum(Y)^2)]
		const double xsumf = (double)xsum;
		const double ysumf = (double)ysum;
		const double x2sumf = (double)x2sum;
		const double y2sumf = (double)y2sum;
		const double xysumf = (double)xysum;

		double corr = (N*xysumf - xsumf*ysumf) / sqrt((N*x2sumf - xsumf*xsumf) * (N*y2sumf - ysumf*ysumf));
		return corr;
	}

	double ComputePlaneCorrelation_SSE2(const void *srca0, const void *srcb0, ptrdiff_t srcpitch, uint32 w16, uint32 h) {
		const ptrdiff_t srcmodulo = srcpitch - w16*16;
		const __m128i zero = _mm_setzero_si128();
		const __m128i masklo = _mm_set1_epi32(0x00FF00FF);
		const __m128i maskha = _mm_set1_epi32(0x00010001);
		const __m128i maskdw = _mm_srli_epi64(_mm_cmpeq_epi32(zero, zero), 32);
		const double N = (double)w16 * (double)h * 16;

		const __m128i *srca = (const __m128i *)srca0;
		const __m128i *srcb = (const __m128i *)srcb0;
		__m128i xsum = zero;
		__m128i ysum = zero;
		__m128i x2sum = zero;
		__m128i y2sum = zero;
		__m128i xysum = zero;

		do {
			__m128i xrowsum = zero;
			__m128i yrowsum = zero;
			__m128i x2rowsum = zero;
			__m128i y2rowsum = zero;
			__m128i xyrowsum = zero;

			uint32 x16 = w16;
			do {
				const __m128i x = *srca++;
				const __m128i y = *srcb++;

				const __m128i xlo = _mm_srli_epi16(x, 8);
				const __m128i xhi = _mm_and_si128(x, masklo);
				const __m128i ylo = _mm_srli_epi16(y, 8);
				const __m128i yhi = _mm_and_si128(y, masklo);

				xrowsum = _mm_add_epi32(xrowsum, _mm_madd_epi16(_mm_add_epi32(xlo, xhi), maskha));
				yrowsum = _mm_add_epi32(yrowsum, _mm_madd_epi16(_mm_add_epi32(xlo, xhi), maskha));
				x2rowsum = _mm_add_epi32(x2rowsum, _mm_add_epi32(_mm_madd_epi16(xlo, xlo), _mm_madd_epi16(xhi, xhi)));
				y2rowsum = _mm_add_epi32(y2rowsum, _mm_add_epi32(_mm_madd_epi16(ylo, ylo), _mm_madd_epi16(yhi, yhi)));
				xyrowsum = _mm_add_epi32(xyrowsum, _mm_add_epi32(_mm_madd_epi16(xlo, ylo), _mm_madd_epi16(xhi, yhi)));
			} while(--x16);

			xsum = _mm_add_epi64(xsum, _mm_add_epi64(_mm_srli_epi64(xrowsum, 32), _mm_and_si128(xrowsum, maskdw)));
			ysum = _mm_add_epi64(ysum, _mm_add_epi64(_mm_srli_epi64(yrowsum, 32), _mm_and_si128(yrowsum, maskdw)));
			x2sum = _mm_add_epi64(x2sum, _mm_add_epi64(_mm_srli_epi64(x2rowsum, 32), _mm_and_si128(x2rowsum, maskdw)));
			y2sum = _mm_add_epi64(y2sum, _mm_add_epi64(_mm_srli_epi64(y2rowsum, 32), _mm_and_si128(y2rowsum, maskdw)));
			xysum = _mm_add_epi64(xysum, _mm_add_epi64(_mm_srli_epi64(xyrowsum, 32), _mm_and_si128(xyrowsum, maskdw)));

			srca = (__m128i *)((char *)srca + srcmodulo);
			srcb = (__m128i *)((char *)srcb + srcmodulo);
		} while(--h);

		xsum = _mm_add_epi64(_mm_srli_si128(xsum, 64), xsum);
		ysum = _mm_add_epi64(_mm_srli_si128(ysum, 64), ysum);
		x2sum = _mm_add_epi64(_mm_srli_si128(x2sum, 64), x2sum);
		y2sum = _mm_add_epi64(_mm_srli_si128(y2sum, 64), y2sum);
		xysum = _mm_add_epi64(_mm_srli_si128(xysum, 64), xysum);

		uint64 xsumi;
		uint64 ysumi;
		uint64 x2sumi;
		uint64 y2sumi;
		uint64 xysumi;

		_mm_storel_epi64((__m128i *)&xsumi, xsum);
		_mm_storel_epi64((__m128i *)&ysumi, ysum);
		_mm_storel_epi64((__m128i *)&x2sumi, x2sum);
		_mm_storel_epi64((__m128i *)&y2sumi, y2sum);
		_mm_storel_epi64((__m128i *)&xysumi, xysum);

		// [N*Sum(XY)-Sum(X)Sum(Y)] / sqrt[(N*Sum(X^2)-Sum(X)^2)(N*Sum(Y^2)-Sum(Y)^2)]
		const double xsumf = (double)xsumi;
		const double ysumf = (double)ysumi;
		const double x2sumf = (double)x2sumi;
		const double y2sumf = (double)y2sumi;
		const double xysumf = (double)xysumi;

		double corr = (N*xysumf - xsumf*ysumf) / sqrt((N*x2sumf - xsumf*xsumf) * (N*y2sumf - ysumf*ysumf));
		return corr;
	}

	double ComputePlaneCorrelation(const void *srca0, const void *srcb0, ptrdiff_t srcpitch, uint32 w16, uint32 h) {
		if (SSE2_enabled)
			return ComputePlaneCorrelation_SSE2(srca0, srcb0, srcpitch, w16, h);

		return ComputePlaneCorrelation_Scalar(srca0, srcb0, srcpitch, w16, h);
	}

	double ComputeFrameCorrelation(const VDXPixmap& pxsrc0, const VDXPixmap& pxsrc1) {
		const uint32 w = pxsrc0.w;
		const uint32 h = pxsrc0.h;

		switch(pxsrc0.format) {
			case nsVDPixmap::kPixFormat_XRGB8888:
				return ComputePlaneCorrelation(pxsrc0.data, pxsrc1.data, pxsrc0.pitch, (w+3) >> 2, h);

			case nsVDPixmap::kPixFormat_YUV422_UYVY:
			case nsVDPixmap::kPixFormat_YUV422_UYVY_709:
			case nsVDPixmap::kPixFormat_YUV422_UYVY_709_FR:
			case nsVDPixmap::kPixFormat_YUV422_UYVY_FR:
			case nsVDPixmap::kPixFormat_YUV422_YUYV:
			case nsVDPixmap::kPixFormat_YUV422_YUYV_709:
			case nsVDPixmap::kPixFormat_YUV422_YUYV_709_FR:
			case nsVDPixmap::kPixFormat_YUV422_YUYV_FR:
				return ComputePlaneCorrelation(pxsrc0.data, pxsrc1.data, pxsrc0.pitch, (w+7) >> 3, h);

			case nsVDPixmap::kPixFormat_Y8:
			case nsVDPixmap::kPixFormat_Y8_FR:
				return ComputePlaneCorrelation(pxsrc0.data, pxsrc1.data, pxsrc0.pitch, (w+15) >> 4, h);

			case nsVDPixmap::kPixFormat_YUV444_Planar:
			case nsVDPixmap::kPixFormat_YUV444_Planar_709:
			case nsVDPixmap::kPixFormat_YUV444_Planar_709_FR:
			case nsVDPixmap::kPixFormat_YUV444_Planar_FR:
			case nsVDPixmap::kPixFormat_YUV422_Planar:
			case nsVDPixmap::kPixFormat_YUV422_Planar_709:
			case nsVDPixmap::kPixFormat_YUV422_Planar_709_FR:
			case nsVDPixmap::kPixFormat_YUV422_Planar_FR:
			case nsVDPixmap::kPixFormat_YUV420_Planar:
			case nsVDPixmap::kPixFormat_YUV420_Planar_709:
			case nsVDPixmap::kPixFormat_YUV420_Planar_709_FR:
			case nsVDPixmap::kPixFormat_YUV420_Planar_FR:
			case nsVDPixmap::kPixFormat_YUV411_Planar:
			case nsVDPixmap::kPixFormat_YUV411_Planar_709:
			case nsVDPixmap::kPixFormat_YUV411_Planar_709_FR:
			case nsVDPixmap::kPixFormat_YUV411_Planar_FR:
			case nsVDPixmap::kPixFormat_YUV410_Planar:
			case nsVDPixmap::kPixFormat_YUV410_Planar_709:
			case nsVDPixmap::kPixFormat_YUV410_Planar_709_FR:
			case nsVDPixmap::kPixFormat_YUV410_Planar_FR:
				{
					const VDPixmapFormatInfo& info = VDPixmapGetInfo(pxsrc0.format);
					uint32 wc16 = ((w - 1) >> (info.auxwbits + 4)) + 1;
					uint32 hc = ((h - 1) >> info.auxhbits) + 1;

					double ycorr  = ComputePlaneCorrelation(pxsrc0.data, pxsrc1.data, pxsrc0.pitch, (w+15) >> 4, hc);
					double cbcorr = ComputePlaneCorrelation(pxsrc0.data2, pxsrc1.data2, pxsrc0.pitch2, wc16, hc);
					double crcorr = ComputePlaneCorrelation(pxsrc0.data3, pxsrc1.data3, pxsrc0.pitch3, wc16, hc);

					return ycorr * cbcorr * crcorr;
				}
		}

		return 0;
	}

	///////////////////////////////////////////////////////////////////////////////////////////////

	VDXPixmap SlicePixmap(const VDXPixmap& px, bool field2) {
		VDXPixmap t(px);

		if (field2) {
			t.data = (char *)t.data + t.pitch;

			if (t.data2)
				t.data2 = (char *)t.data2 + t.pitch2;

			if (t.data3)
				t.data3 = (char *)t.data3 + t.pitch3;
		}

		t.pitch += t.pitch;
		t.pitch2 += t.pitch2;
		t.pitch3 += t.pitch3;

		if (!field2)
			++t.h;

		t.h >>= 1;

		return t;
	}

	void CopyPlane(void *dst, ptrdiff_t dstpitch, const void *src, ptrdiff_t srcpitch, uint32 w, uint32 h) {
		for(uint32 y=0; y<h; ++y) {
			memcpy(dst, src, w);

			dst = (char *)dst + dstpitch;
			src = (const char *)src + srcpitch;
		}
	}

	void CopyPixmap(const VDXPixmap& dst, const VDXPixmap& src) {
		const uint32 w = dst.w;
		const uint32 h = dst.h;

		switch(dst.format) {
			case nsVDXPixmap::kPixFormat_XRGB8888:
				CopyPlane(dst.data, dst.pitch, src.data, src.pitch, w*4, h);
				break;

			case nsVDXPixmap::kPixFormat_YUV444_Planar:
			case nsVDXPixmap::kPixFormat_YUV444_Planar_FR:
			case nsVDXPixmap::kPixFormat_YUV444_Planar_709:
			case nsVDXPixmap::kPixFormat_YUV444_Planar_709_FR:
				CopyPlane(dst.data, dst.pitch, src.data, src.pitch, w, h);
				CopyPlane(dst.data2, dst.pitch2, src.data2, src.pitch2, w, h);
				CopyPlane(dst.data3, dst.pitch3, src.data3, src.pitch3, w, h);
				break;

			case nsVDXPixmap::kPixFormat_YUV422_Planar:
			case nsVDXPixmap::kPixFormat_YUV422_Planar_FR:
			case nsVDXPixmap::kPixFormat_YUV422_Planar_709:
			case nsVDXPixmap::kPixFormat_YUV422_Planar_709_FR:
				CopyPlane(dst.data, dst.pitch, src.data, src.pitch, w, h);
				CopyPlane(dst.data2, dst.pitch2, src.data2, src.pitch2, w >> 1, h);
				CopyPlane(dst.data3, dst.pitch3, src.data3, src.pitch3, w >> 1, h);
				break;

			case nsVDXPixmap::kPixFormat_YUV411_Planar:
			case nsVDXPixmap::kPixFormat_YUV411_Planar_FR:
			case nsVDXPixmap::kPixFormat_YUV411_Planar_709:
			case nsVDXPixmap::kPixFormat_YUV411_Planar_709_FR:
				CopyPlane(dst.data, dst.pitch, src.data, src.pitch, w, h);
				CopyPlane(dst.data2, dst.pitch2, src.data2, src.pitch2, w >> 2, h);
				CopyPlane(dst.data3, dst.pitch3, src.data3, src.pitch3, w >> 2, h);
				break;

			case nsVDXPixmap::kPixFormat_YUV422_UYVY:
			case nsVDXPixmap::kPixFormat_YUV422_UYVY_FR:
			case nsVDXPixmap::kPixFormat_YUV422_UYVY_709:
			case nsVDXPixmap::kPixFormat_YUV422_UYVY_709_FR:
			case nsVDXPixmap::kPixFormat_YUV422_YUYV:
			case nsVDXPixmap::kPixFormat_YUV422_YUYV_FR:
			case nsVDXPixmap::kPixFormat_YUV422_YUYV_709:
			case nsVDXPixmap::kPixFormat_YUV422_YUYV_709_FR:
				CopyPlane(dst.data, dst.pitch, src.data, src.pitch, w*2, h);
				break;

			default:
				VDASSERT(false);
				break;
		}
	}
}

////////////////////////////////////////////////////////////

struct VDVideoFilterIVTCConfig {
	enum FieldMode {
		kFieldMode_Autoselect,
		kFieldMode_TFF,
		kFieldMode_BFF,
		kFieldMode_Blurred,
		kFieldMode_Duplicated,
		kFieldModeCount
	};

	bool mbReduceRate;
	int mOffset;
	int mBlendWeight;
	FieldMode mFieldMode;

	VDVideoFilterIVTCConfig()
		: mbReduceRate(false)
		, mOffset(-1)
		, mBlendWeight(50)
		, mFieldMode(kFieldMode_Autoselect)
	{
	}

	bool operator==(const VDVideoFilterIVTCConfig& x) const {
		return mbReduceRate == x.mbReduceRate &&
			mOffset == x.mOffset &&
			mFieldMode == x.mFieldMode &&
			mBlendWeight == x.mBlendWeight;
	}

	bool operator!=(const VDVideoFilterIVTCConfig& x) const {
		return mbReduceRate != x.mbReduceRate ||
			mOffset != x.mOffset ||
			mFieldMode != x.mFieldMode ||
			mBlendWeight != x.mBlendWeight;
	}
};

class VDVideoFilterIVTCDialog : public VDDialogFrameW32 {
public:
	VDVideoFilterIVTCDialog(VDVideoFilterIVTCConfig& config, IVDXFilterPreview2 *ifp2)
		: VDDialogFrameW32(IDD_FILTER_IVTC)
		, mConfig(config)
		, mifp2(ifp2)
	{}

protected:
	bool OnLoaded();
	void OnDataExchange(bool write);
	bool OnCommand(uint32 id, uint32 extcode);
	void OnHScroll(uint32 id, int code);
	void UpdateEnables();

	VDVideoFilterIVTCConfig& mConfig;
	IVDXFilterPreview2 *mifp2;
};

bool VDVideoFilterIVTCDialog::OnLoaded() {
	if (mifp2) {
		VDZHWND hwndPreview = GetControl(IDC_PREVIEW);

		if (hwndPreview)
			mifp2->InitButton((VDXHWND)hwndPreview);
	}

	TBSetRange(IDC_BLEND_WEIGHTING, 0, 100);

	UpdateEnables();

	return VDDialogFrameW32::OnLoaded();
}

void VDVideoFilterIVTCDialog::OnDataExchange(bool write) {
	if (write) {
		mConfig.mbReduceRate = IsButtonChecked(IDC_MODE_REDUCE);

		if (IsButtonChecked(IDC_FIELDORDER_AUTO))
			mConfig.mFieldMode = VDVideoFilterIVTCConfig::kFieldMode_Autoselect;
		else if (IsButtonChecked(IDC_FIELDORDER_TFF))
			mConfig.mFieldMode = VDVideoFilterIVTCConfig::kFieldMode_TFF;
		else if (IsButtonChecked(IDC_FIELDORDER_BFF))
			mConfig.mFieldMode = VDVideoFilterIVTCConfig::kFieldMode_BFF;
		else if (IsButtonChecked(IDC_FIELDORDER_BLURRED))
			mConfig.mFieldMode = VDVideoFilterIVTCConfig::kFieldMode_Blurred;
		else if (IsButtonChecked(IDC_FIELDORDER_DUPLICATED))
			mConfig.mFieldMode = VDVideoFilterIVTCConfig::kFieldMode_Duplicated;

		if (IsButtonChecked(IDC_PHASE_MANUAL)) {
			uint32 v = GetControlValueUint32(IDC_PHASE);
			if (v >= 5)
				FailValidation(IDC_PHASE);
			else
				mConfig.mOffset = v;
		} else {
			mConfig.mOffset = -1;
		}

		mConfig.mBlendWeight = TBGetValue(IDC_BLEND_WEIGHTING);
	} else {
		if (mConfig.mbReduceRate)
			CheckButton(IDC_MODE_REDUCE, true);
		else
			CheckButton(IDC_MODE_DECOMB, true);

		switch(mConfig.mFieldMode) {
			case VDVideoFilterIVTCConfig::kFieldMode_Autoselect:
				CheckButton(IDC_FIELDORDER_AUTO, true);
				break;
			case VDVideoFilterIVTCConfig::kFieldMode_TFF:
				CheckButton(IDC_FIELDORDER_TFF, true);
				break;
			case VDVideoFilterIVTCConfig::kFieldMode_BFF:
				CheckButton(IDC_FIELDORDER_BFF, true);
				break;
			case VDVideoFilterIVTCConfig::kFieldMode_Blurred:
				CheckButton(IDC_FIELDORDER_BLURRED, true);
				break;
			case VDVideoFilterIVTCConfig::kFieldMode_Duplicated:
				CheckButton(IDC_FIELDORDER_DUPLICATED, true);
				break;
		}

		if (mConfig.mOffset >= 0) {
			CheckButton(IDC_PHASE_MANUAL, true);
			SetControlTextF(IDC_PHASE, L"%u", mConfig.mOffset);
		} else {
			CheckButton(IDC_PHASE_ADAPTIVE, true);
		}

		TBSetValue(IDC_BLEND_WEIGHTING, mConfig.mBlendWeight);
	}
}

bool VDVideoFilterIVTCDialog::OnCommand(uint32 id, uint32 extcode) {
	switch(id) {
		case IDC_FIELDORDER_AUTO:
		case IDC_FIELDORDER_TFF:
		case IDC_FIELDORDER_BFF:
		case IDC_FIELDORDER_BLURRED:
		case IDC_FIELDORDER_DUPLICATED:
		case IDC_MODE_REDUCE:
		case IDC_MODE_DECOMB:
		case IDC_PHASE_MANUAL:
		case IDC_PHASE_ADAPTIVE:
		case IDC_PHASE:
			{
				VDVideoFilterIVTCConfig tmp(mConfig);

				OnDataExchange(true);
				UpdateEnables();
				if (mifp2 && mConfig != tmp) {
					if (mConfig.mbReduceRate != tmp.mbReduceRate)
						mifp2->RedoSystem();
					else
						mifp2->RedoFrame();
				}
			}

			return false;

		case IDC_PREVIEW:
			if (mifp2)
				mifp2->Toggle((VDXHWND)mhdlg);
			return true;
	}

	return false;
}

void VDVideoFilterIVTCDialog::OnHScroll(uint32 id, int code) {
	if (id == IDC_BLEND_WEIGHTING) {
		if (mifp2) {
			VDVideoFilterIVTCConfig tmp(mConfig);

			OnDataExchange(true);
			if (mConfig != tmp) {
				if (mConfig.mbReduceRate != tmp.mbReduceRate)
					mifp2->RedoSystem();
				else
					mifp2->RedoFrame();
			}
		}
	}
}

void VDVideoFilterIVTCDialog::UpdateEnables() {
	EnableControl(IDC_PHASE, mConfig.mOffset >= 0);
	EnableControl(IDC_BLEND_WEIGHTING, mConfig.mFieldMode == VDVideoFilterIVTCConfig::kFieldMode_Blurred);
}


class VDVideoFilterIVTC : public VDXVideoFilter {
public:
	uint32 GetParams();
	void Start();
	void Run();
	bool OnInvalidateCaches();
	bool Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher);
	bool Configure(VDXHWND);
	void GetSettingString(char *buf, int maxlen);
	void GetScriptString(char *buf, int maxlen);

	void ScriptConfig(IVDXScriptInterpreter *interp, const VDXScriptValue *argv, int argc);

	VDXVF_DECLARE_SCRIPT_METHODS();

protected:
	VDVideoFilterIVTCConfig mConfig;

	struct CacheEntry {
		sint64	mBaseFrame1;
		sint64	mBaseFrame2;
		IVTCScore	mScore;
	};

	enum { kCacheSize = 64 };
	CacheEntry mCache[kCacheSize];
};

VDXVF_BEGIN_SCRIPT_METHODS(VDVideoFilterIVTC)
	VDXVF_DEFINE_SCRIPT_METHOD(VDVideoFilterIVTC, ScriptConfig, "iii")
	VDXVF_DEFINE_SCRIPT_METHOD2(VDVideoFilterIVTC, ScriptConfig, "iiii")
VDXVF_END_SCRIPT_METHODS()

uint32 VDVideoFilterIVTC::GetParams() {
	const VDXPixmapLayout& srcLayout = *fa->src.mpPixmapLayout;

	switch(srcLayout.format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_YUV411_Planar:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_FR:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_709:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_709_FR:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_FR:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_709:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_709_FR:
			break;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
	}

	if (mConfig.mbReduceRate) {
		VDFraction fr(fa->dst.mFrameRateHi, fa->dst.mFrameRateLo);

		fr *= VDFraction(4, 5);

		fa->dst.mFrameRateHi = fr.getHi();
		fa->dst.mFrameRateLo = fr.getLo();
		fa->dst.mFrameCount = ((fa->dst.mFrameCount + 4)/5) * 4;
	}

	return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_ALIGN_SCANLINES | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

void VDVideoFilterIVTC::Start() {
	for(int i=0; i<kCacheSize; ++i) {
		mCache[i].mBaseFrame1 = -1;
		mCache[i].mBaseFrame2 = -1;
	}
}

void VDVideoFilterIVTC::Run() {
	const VDXPixmap *pxsrc[11]={
			fa->mpSourceFrames[0]->mpPixmap,
			fa->mpSourceFrames[1]->mpPixmap,
			fa->mpSourceFrames[2]->mpPixmap,
			fa->mpSourceFrames[3]->mpPixmap,
			fa->mpSourceFrames[4]->mpPixmap,
			fa->mpSourceFrames[5]->mpPixmap,
			fa->mpSourceFrames[6]->mpPixmap,
			fa->mpSourceFrames[7]->mpPixmap,
			fa->mpSourceFrames[8]->mpPixmap,
			fa->mpSourceFrames[9]->mpPixmap,
			fa->mpSourceFrames[10]->mpPixmap
	};

	const VDXPixmap& pxdst = *fa->mpOutputFrames[0]->mpPixmap;

	if (mConfig.mFieldMode == VDVideoFilterIVTCConfig::kFieldMode_Duplicated) {
		IVTCScore scores[20];
		for(int i=0; i<10; ++i) {
			const VDXPixmap& src1 = *pxsrc[i];
			const VDXPixmap& src2 = *pxsrc[i+1];
			sint64 frame1 = fa->mpSourceFrames[i]->mFrameNumber;
			sint64 frame2 = fa->mpSourceFrames[i+1]->mFrameNumber;
			int hkey = (int)frame1 & (kCacheSize - 1);

			CacheEntry& cent = mCache[hkey];
			if (cent.mBaseFrame1 != frame1 || cent.mBaseFrame2 != frame2) {
				cent.mScore.mVar[0] = (sint64)(fabs(ComputeFrameCorrelation(src1, src2)) * 1e+10);
				cent.mBaseFrame1 = frame1;
				cent.mBaseFrame2 = frame2;
			}

			scores[i] = scores[i+10] = cent.mScore;
	//		VDDEBUG("Scores[%d]: %10lld | %-10lld\n", i, scores[i][0], scores[i][1]);
		}

		// The raw scores we have are the amount of variance between each frame and the next.
		//
		// Polarity == true means TFF field order.

		VDDEBUG("scores: %10lld %10lld %10lld %10lld %10lld %10lld %10lld %10lld %10lld %10lld\n"
			, scores[0].mVar[0]
			, scores[1].mVar[0]
			, scores[2].mVar[0]
			, scores[3].mVar[0]
			, scores[4].mVar[0]
			, scores[5].mVar[0]
			, scores[6].mVar[0]
			, scores[7].mVar[0]
			, scores[8].mVar[0]
			, scores[9].mVar[0]);

		sint64 pscores[5];
		for(int i=0; i<5; ++i) {
			pscores[i]	= scores[i+0].mVar[0] + scores[i+5].mVar[0];

			VDDEBUG("Pscores[%d]: %10lld (%10ld)\n"
				, i
				, pscores[i]
				, pscores[i] > 0 ? 0 : (long)sqrt((double)pscores[i])
			);
		}

		int bestPhase = mConfig.mOffset;

		if (bestPhase >= 0) {
			int offset = mConfig.mOffset - (int)(fa->mpSourceFrames[5]->mFrameNumber % 5) - 1;

			if (offset < 0)
				offset += 5;

			bestPhase= offset;
		} else {
			sint64 bestScore = pscores[0];
			bestPhase = 0;

			for(int i=1; i<5; ++i) {
				sint64 score = pscores[i];

				if (score > bestScore) {
					bestScore = score;
					bestPhase = i;
				}
			}
		}

		VDDEBUG("bestPhase: %d (offset=%d)\n", bestPhase, (int)((fa->src.mFrameNumber + bestPhase + 1) % 5));

		if (mConfig.mbReduceRate) {
			// Compute where the duplicated frame occurs in a repeated 5-frame pattern.
			int dupeOffset = (int)((fa->mpSourceFrames[5]->mFrameNumber + bestPhase) % 5);

			// If we're past that point, then read one frame ahead.
			int localOffset = (int)fa->dst.mFrameNumber & 3;
			if (localOffset >= dupeOffset) {
				CopyPixmap(pxdst, *pxsrc[6]);
				return;
			}
		}

		CopyPixmap(pxdst, *pxsrc[5]);
	} else if (mConfig.mFieldMode == VDVideoFilterIVTCConfig::kFieldMode_Blurred) {
		IVTCScore scores[20];
		for(int i=0; i<10; ++i) {
			const VDXPixmap& src1 = *pxsrc[i];
			const VDXPixmap& src2 = *pxsrc[i+1];
			sint64 frame1 = fa->mpSourceFrames[i]->mFrameNumber;
			sint64 frame2 = fa->mpSourceFrames[i+1]->mFrameNumber;
			int hkey = (int)frame1 & (kCacheSize - 1);

			CacheEntry& cent = mCache[hkey];
			if (cent.mBaseFrame1 != frame1 || cent.mBaseFrame2 != frame2) {
				cent.mScore.mVar[0] = (sint64)(fabs(ComputeFrameCorrelation(src1, src2)) * 1e+10);
				cent.mBaseFrame1 = frame1;
				cent.mBaseFrame2 = frame2;
			}

			scores[i] = scores[i+10] = cent.mScore;
	//		VDDEBUG("Scores[%d]: %10lld | %-10lld\n", i, scores[i][0], scores[i][1]);
		}

		// The raw scores we have are the amount of variance between each frame and the next.
		//
		// Polarity == true means TFF field order.

		VDDEBUG("scores: %10lld %10lld %10lld %10lld %10lld %10lld %10lld %10lld %10lld %10lld\n"
			, scores[0].mVar[0]
			, scores[1].mVar[0]
			, scores[2].mVar[0]
			, scores[3].mVar[0]
			, scores[4].mVar[0]
			, scores[5].mVar[0]
			, scores[6].mVar[0]
			, scores[7].mVar[0]
			, scores[8].mVar[0]
			, scores[9].mVar[0]);

		sint64 pscores[5];
		for(int i=0; i<5; ++i) {
			pscores[i]	= (scores[i+2].mVar[0] + scores[i+3].mVar[0] + scores[i+4].mVar[0] + scores[i+7].mVar[0] + scores[i+8].mVar[0] + scores[i+9].mVar[0]) * 2
						- (scores[i+0].mVar[0] + scores[i+1].mVar[0] + scores[i+5].mVar[0] + scores[i+6].mVar[0]) * 3;

			VDDEBUG("Pscores[%d]: %10lld (%10ld)\n"
				, i
				, pscores[i]
				, pscores[i] > 0 ? 0 : (long)sqrt((double)pscores[i])
			);
		}

		int bestPhase = mConfig.mOffset;

		if (bestPhase >= 0) {
			int offset = mConfig.mOffset - (int)(fa->mpSourceFrames[5]->mFrameNumber % 5) - 1;

			if (offset < 0)
				offset += 5;

			bestPhase= offset;
		} else {
			sint64 bestScore = 0;

			for(int i=0; i<5; ++i) {
				sint64 score = pscores[i];

				if (score < bestScore) {
					bestScore = score;
					bestPhase = i;
				}
			}
		}

		VDDEBUG("bestPhase: %d (offset=%d)\n", bestPhase, (int)((fa->src.mFrameNumber + bestPhase + 1) % 5));

		if (mConfig.mbReduceRate) {
			// Compute where the second duplicate C frame occurs in a repeated 5-frame, phase-0
			// pattern.
			int dupeOffset = (int)((fa->mpSourceFrames[5]->mFrameNumber + bestPhase + 1) % 5);

			// If we're past that point, then read one frame ahead.
			int localOffset = (int)fa->dst.mFrameNumber & 3;
			if (localOffset >= dupeOffset) {
				if (bestPhase == 1)
					DeblurFrames(pxdst, *pxsrc[5], *pxsrc[6], *pxsrc[7], *pxsrc[8], mConfig.mBlendWeight);
				else if (bestPhase == 0)
					DeblurFrames(pxdst, *pxsrc[4], *pxsrc[5], *pxsrc[6], *pxsrc[7], mConfig.mBlendWeight);
				else
					CopyPixmap(pxdst, *pxsrc[6]);

				return;
			}
		}

		if (bestPhase == 0)
			DeblurFrames(pxdst, *pxsrc[4], *pxsrc[5], *pxsrc[6], *pxsrc[7], mConfig.mBlendWeight);
		else if (bestPhase == 4)
			DeblurFrames(pxdst, *pxsrc[3], *pxsrc[4], *pxsrc[5], *pxsrc[6], mConfig.mBlendWeight);
		else
			CopyPixmap(pxdst, *pxsrc[5]);
	} else {
		IVTCScore scores[20];
		for(int i=0; i<10; ++i) {
			const VDXPixmap& src1 = *pxsrc[i];
			const VDXPixmap& src2 = *pxsrc[i+1];
			sint64 frame1 = fa->mpSourceFrames[i]->mFrameNumber;
			sint64 frame2 = fa->mpSourceFrames[i+1]->mFrameNumber;
			int hkey = (int)frame1 & (kCacheSize - 1);

			CacheEntry& cent = mCache[hkey];
			if (cent.mBaseFrame1 != frame1 || cent.mBaseFrame2 != frame2) {
				cent.mScore = ComputeScanImprovement(src1, src2);
				cent.mBaseFrame1 = frame1;
				cent.mBaseFrame2 = frame2;
			}

			scores[i] = scores[i+10] = cent.mScore;
	//		VDDEBUG("Scores[%d]: %10lld | %-10lld\n", i, scores[i][0], scores[i][1]);
		}

		// The raw scores we have are the amount of improvement we get at that frame from
		// shifting the opposite field back one frame.
		//
		// Polarity == true means TFF field order.

		sint64 pscores[5][2];
		for(int i=0; i<5; ++i) {
			pscores[i][0]	= scores[i+1].mVarShift[0] + 0*scores[i+2].mVarShift[0] - scores[i+1].mVar[0] - scores[i+2].mVar[0]
							+ scores[i+6].mVarShift[0] + 0*scores[i+7].mVarShift[0] - scores[i+6].mVar[0] - scores[i+7].mVar[0];
			pscores[i][1]	= scores[i+1].mVarShift[1] + 0*scores[i+2].mVarShift[1] - scores[i+1].mVar[1] - scores[i+2].mVar[1]
							+ scores[i+6].mVarShift[1] + 0*scores[i+7].mVarShift[1] - scores[i+6].mVar[1] - scores[i+7].mVar[1];

			VDDEBUG("Pscores[%d]: %10lld | %-10lld (%10ld | %-10ld)\n"
				, i
				, pscores[i][0]
				, pscores[i][1]
				, pscores[i][0] > 0 ? 0 : -(long)sqrt(-(double)pscores[i][0])
				, pscores[i][1] > 0 ? 0 : -(long)sqrt(-(double)pscores[i][1])
			);
		}

		int bestPhase = -1;
		bool bestPolarity = false;
		sint64 bestScore = 0x7ffffffffffffffll;

		static const uint8 kPolarityMasks[3]={ 0x03, 0x02, 0x01 };
		const uint8 polMask = kPolarityMasks[mConfig.mFieldMode];

		int minOffset = 0;
		int maxOffset = 5;

		if (mConfig.mOffset >= 0) {
			int offset = mConfig.mOffset - (int)(fa->mpSourceFrames[5]->mFrameNumber % 5) - 1;

			if (offset < 0)
				offset += 5;

			minOffset = offset;
			maxOffset = minOffset + 1;
		}

		for(int i=minOffset; i<maxOffset; ++i) {
			for(int pol=0; pol<2; ++pol) {
				if (!(polMask & (1 << pol)))
					continue;

				sint64 score = pscores[i][pol];

				if (score < bestScore) {
					bestScore = score;
					bestPhase = i;
					bestPolarity = pol>0;
				}
			}
		}

		// 3/2/3/2 interleave pattern:
		//	A A B C D	TFF		A B C C D
		//	A B C C D			A B C C D
		//	0 4 3 2 1		->	0 4 3 2 1
		//	A B C C D	BFF		A B C C D
		//	A A B C D			A B C C D
		//
		//  c d r c c (copy, decomb, drop, copy, copy)

		VDDEBUG("bestPhase: %d/%d (offset=%d)\n", bestPhase, bestPolarity, (int)((fa->src.mFrameNumber + bestPhase + 1) % 5));

		if (mConfig.mbReduceRate) {
			// Compute where the first duplicate C frame occurs in a repeated 5-frame, phase-0
			// pattern (second decombed frame). This is the one that we drop. Technically we
			// can also drop the second C frame, which we used to do, except that reacts badly
			// if there isn't actual interlacing (ugh).
			int dupeOffset = (int)((fa->mpSourceFrames[5]->mFrameNumber + bestPhase + 2) % 5);

			// If we're past that point, then read one frame ahead.
			int localOffset = (int)fa->dst.mFrameNumber & 3;
			if (localOffset >= dupeOffset) {
				CopyPixmap(SlicePixmap(pxdst, bestPolarity), SlicePixmap(*pxsrc[6], bestPolarity));
				CopyPixmap(SlicePixmap(pxdst, !bestPolarity), SlicePixmap(*pxsrc[bestPhase == 0 || bestPhase == 4 ? 7 : 6], !bestPolarity));
				return;
			}
		}

		CopyPixmap(SlicePixmap(pxdst, bestPolarity), SlicePixmap(*pxsrc[5], bestPolarity));
		CopyPixmap(SlicePixmap(pxdst, !bestPolarity), SlicePixmap(*pxsrc[bestPhase == 4 || bestPhase == 3 ? 6 : 5], !bestPolarity));
	}
}

bool VDVideoFilterIVTC::OnInvalidateCaches() {
	for(int i=0; i<kCacheSize; ++i) {
		mCache[i].mBaseFrame1 = -1;
		mCache[i].mBaseFrame2 = -1;
	}

	return true;
}

bool VDVideoFilterIVTC::Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher) {
	if (mConfig.mbReduceRate)
		frame = (frame >> 2) * 5 + (frame & 3);

	prefetcher->PrefetchFrame(0, frame-5, 0);
	prefetcher->PrefetchFrame(0, frame-4, 0);
	prefetcher->PrefetchFrame(0, frame-3, 0);
	prefetcher->PrefetchFrame(0, frame-2, 0);
	prefetcher->PrefetchFrame(0, frame-1, 0);
	prefetcher->PrefetchFrame(0, frame  , 0);
	prefetcher->PrefetchFrame(0, frame+1, 0);
	prefetcher->PrefetchFrame(0, frame+2, 0);
	prefetcher->PrefetchFrame(0, frame+3, 0);
	prefetcher->PrefetchFrame(0, frame+4, 0);
	prefetcher->PrefetchFrame(0, frame+5, 0);
	return true;
}

bool VDVideoFilterIVTC::Configure(VDXHWND parent) {
	VDVideoFilterIVTCConfig old(mConfig);
	VDVideoFilterIVTCDialog dlg(mConfig, fa->ifp2);

	if (dlg.ShowDialog((VDGUIHandle)parent))
		return true;

	mConfig = old;
	return false;
}

void VDVideoFilterIVTC::GetSettingString(char *buf, int maxlen) {
	static const char *const kModes[]={
		"auto",
		"tff",
		"bff",
		"deblur",
		"frames"
	};

	SafePrintf(buf, maxlen, " (%s, %s, %s)"
		, mConfig.mbReduceRate ? "reduce" : "decomb"
		, kModes[mConfig.mFieldMode]
		, mConfig.mOffset < 0 ? "auto" : "manual");
}

void VDVideoFilterIVTC::GetScriptString(char *buf, int maxlen) {
	if (mConfig.mFieldMode == VDVideoFilterIVTCConfig::kFieldMode_Blurred)
		SafePrintf(buf, maxlen, "Config(%d,%d,%d,%d)", mConfig.mbReduceRate, mConfig.mFieldMode, mConfig.mOffset, mConfig.mBlendWeight);
	else
		SafePrintf(buf, maxlen, "Config(%d,%d,%d)", mConfig.mbReduceRate, mConfig.mFieldMode, mConfig.mOffset);
}

void VDVideoFilterIVTC::ScriptConfig(IVDXScriptInterpreter *interp, const VDXScriptValue *argv, int argc) {
	mConfig.mbReduceRate = argv[0].asInt() != 0;
	
	int i = argv[1].asInt();
	if (i < 0 || i >= VDVideoFilterIVTCConfig::kFieldModeCount)
		i = VDVideoFilterIVTCConfig::kFieldMode_Autoselect;

	mConfig.mFieldMode = (VDVideoFilterIVTCConfig::FieldMode)i;

	mConfig.mOffset = argv[2].asInt();
	if (mConfig.mOffset < -1 || mConfig.mOffset > 4)
		interp->ScriptError(VDXScriptError::FCALL_OUT_OF_RANGE);

	if (argc >= 4)
		mConfig.mBlendWeight = argv[3].asInt();
}

///////////////////////////////////////////////////////////////////////////

extern const VDXFilterDefinition g_VDVFIVTC =
	VDXVideoFilterDefinition<VDVideoFilterIVTC>(
		NULL,
		"IVTC", 
		"Removes 3:2 pulldown (telecine) from video.");

// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{24,{flat}}' }'' : unreferenced local function has been removed
#pragma warning(disable: 4505)
