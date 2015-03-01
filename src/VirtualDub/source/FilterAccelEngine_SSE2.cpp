//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2011 Avery Lee
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

#include "stdafx.h"
#include <emmintrin.h>

void VDFilterAccelInterleaveYUV_SSE2(
		void *dst, ptrdiff_t dstpitch,
		const void *srcY, ptrdiff_t srcYPitch, 
		const void *srcCb, ptrdiff_t srcCbPitch, 
		const void *srcCr, ptrdiff_t srcCrPitch,
		uint32 w,
		uint32 h)
{
	__m128i xFFb = _mm_set1_epi32(0xFFFFFFFFU);

	do {
		__m128i *VDRESTRICT vdst = (__m128i *)dst;
		const uint8 *VDRESTRICT vsrcY = (const uint8 *)srcY;
		const uint8 *VDRESTRICT vsrcCb = (const uint8 *)srcCb;
		const uint8 *VDRESTRICT vsrcCr = (const uint8 *)srcCr;

		uint32 x8 = w >> 3;
		if (x8) {
			if (!((int)dst & 15)) {
				do {
					__m128i y = _mm_loadl_epi64((const __m128i *)vsrcY);
					__m128i cb = _mm_loadl_epi64((const __m128i *)vsrcCb);
					__m128i cr = _mm_loadl_epi64((const __m128i *)vsrcCr);

					__m128i a = _mm_unpacklo_epi8(cb, cr);
					__m128i b = _mm_unpacklo_epi8(y, xFFb);

					vdst[0] = _mm_unpacklo_epi8(a, b);
					vdst[1] = _mm_unpackhi_epi8(a, b);
					vdst += 2;
					vsrcY += 8;
					vsrcCb += 8;
					vsrcCr += 8;
				} while(--x8);
			} else {
				do {
					__m128i y = _mm_loadl_epi64((const __m128i *)vsrcY);
					__m128i cb = _mm_loadl_epi64((const __m128i *)vsrcCb);
					__m128i cr = _mm_loadl_epi64((const __m128i *)vsrcCr);

					__m128i a = _mm_unpacklo_epi8(cb, cr);
					__m128i b = _mm_unpacklo_epi8(y, xFFb);

					__m128i ahi = _mm_shuffle_epi32(a, 0xee);
					__m128i bhi = _mm_shuffle_epi32(b, 0xee);

					_mm_storel_epi64(vdst, a);
					_mm_storel_epi64((__m128i *)((char *)vdst + 8), ahi);
					_mm_storel_epi64(vdst + 1, b);
					_mm_storel_epi64((__m128i *)((char *)vdst + 24), bhi);

					vdst += 2;
					vsrcY += 8;
					vsrcCb += 8;
					vsrcCr += 8;
				} while(--x8);
			}
		}

		uint32 xr = w & 7;

		if (xr) {
			uint8 *dst8 = (uint8 *)vdst;

			do {
				dst8[0] = *vsrcCb++;
				dst8[1] = *vsrcY++;
				dst8[2] = *vsrcCr++;
				dst8[3] = 0xFF;
				dst8 += 4;
			} while(--xr);
		}

		dst = (char *)dst + dstpitch;
		srcY = (char *)srcY + srcYPitch;
		srcCb = (char *)srcCb + srcCbPitch;
		srcCr = (char *)srcCr + srcCrPitch;
	} while(--h);
}
