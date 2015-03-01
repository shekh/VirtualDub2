//	VirtualDub - Video processing and capture application
//	Internal filter library
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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include <stdafx.h>
#include <emmintrin.h>

void VDDSPBlend8_LerpConst_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16, uint8 factor) {
	__m128i *dst = (__m128i *)dst0;
	const __m128i *srca = (const __m128i *)src0;
	const __m128i *srcb = (const __m128i *)src1;

	__m128i zero = _mm_setzero_si128();
	__m128i vfac = _mm_set1_epi16(factor);
	__m128i vbias = _mm_set1_epi16(128);
	do {
		__m128i c = *srca++;
		__m128i d = *srcb++;
		__m128i diff1 = _mm_subs_epu8(c, d);
		__m128i diff2 = _mm_subs_epu8(d, c);
		__m128i ad = _mm_or_si128(diff1, diff2);
		__m128i adlo = _mm_unpacklo_epi8(ad, zero);
		__m128i adhi = _mm_unpackhi_epi8(ad, zero);
		__m128i multlo = _mm_add_epi16(_mm_mullo_epi16(adlo, vfac), vbias);
		__m128i multhi = _mm_add_epi16(_mm_mullo_epi16(adhi, vfac), vbias);
		__m128i deltalo = _mm_srli_epi16(_mm_add_epi16(multlo, _mm_srli_epi16(multlo, 8)), 8);
		__m128i deltahi = _mm_srli_epi16(_mm_add_epi16(multhi, _mm_srli_epi16(multhi, 8)), 8);
		__m128i delta = _mm_packus_epi16(deltalo, deltahi);
		__m128i sign = _mm_cmpeq_epi8(diff1, zero);
		__m128i add = _mm_and_si128(sign, delta);
		__m128i sub = _mm_andnot_si128(sign, delta);

		*dst++ = _mm_subs_epu8(_mm_adds_epu8(c, add), sub);
	} while(--n16);
}

void VDDSPBlend8_Min_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16) {
	__m128i *dst = (__m128i *)dst0;
	const __m128i *srca = (const __m128i *)src0;
	const __m128i *srcb = (const __m128i *)src1;

	do {
		__m128i c = *srca++;
		__m128i d = *srcb++;

		*dst++ = _mm_min_epu8(c, d);
	} while(--n16);
}

void VDDSPBlend8_Max_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16) {
	__m128i *dst = (__m128i *)dst0;
	const __m128i *srca = (const __m128i *)src0;
	const __m128i *srcb = (const __m128i *)src1;

	do {
		__m128i c = *srca++;
		__m128i d = *srcb++;

		*dst++ = _mm_max_epu8(c, d);
	} while(--n16);
}

void VDDSPBlend8_Add_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16) {
	__m128i *dst = (__m128i *)dst0;
	const __m128i *srca = (const __m128i *)src0;
	const __m128i *srcb = (const __m128i *)src1;

	do {
		__m128i c = *srca++;
		__m128i d = *srcb++;

		*dst++ = _mm_adds_epu8(c, d);
	} while(--n16);
}

void VDDSPBlend8_Multiply_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16) {
	__m128i *dst = (__m128i *)dst0;
	const __m128i *srca = (const __m128i *)src0;
	const __m128i *srcb = (const __m128i *)src1;

	__m128i zero = _mm_setzero_si128();
	__m128i vbias = _mm_set1_epi16(128);
	do {
		__m128i c = *srca++;
		__m128i d = *srcb++;

		__m128i clo = _mm_unpacklo_epi8(c, zero);
		__m128i chi = _mm_unpackhi_epi8(c, zero);
		__m128i dlo = _mm_unpacklo_epi8(d, zero);
		__m128i dhi = _mm_unpackhi_epi8(d, zero);
		__m128i vlo = _mm_add_epi16(_mm_mullo_epi16(clo, dlo), vbias);
		__m128i vhi = _mm_add_epi16(_mm_mullo_epi16(chi, dhi), vbias);

		vlo = _mm_srli_epi16(_mm_add_epi16(vlo, _mm_srli_epi16(vlo, 8)), 8);
		vhi = _mm_srli_epi16(_mm_add_epi16(vhi, _mm_srli_epi16(vhi, 8)), 8);

		*dst++ = _mm_packus_epi16(vlo, vhi);
	} while(--n16);
}

void VDDSPBlend8_LinearBurn_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16) {
	__m128i *dst = (__m128i *)dst0;
	const __m128i *srca = (const __m128i *)src0;
	const __m128i *srcb = (const __m128i *)src1;

	__m128i ones = _mm_set1_epi8((char)255);
	do {
		__m128i c = *srca++;
		__m128i d = *srcb++;

		*dst++ = _mm_subs_epu8(c, _mm_xor_si128(d, ones));
	} while(--n16);
}

void VDDSPBlend8_Screen_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16) {
	__m128i *dst = (__m128i *)dst0;
	const __m128i *srca = (const __m128i *)src0;
	const __m128i *srcb = (const __m128i *)src1;

	__m128i zero = _mm_setzero_si128();
	__m128i vbias = _mm_set1_epi16(128);
	do {
		__m128i c = *srca++;
		__m128i d = *srcb++;

		__m128i clo = _mm_unpacklo_epi8(c, zero);
		__m128i chi = _mm_unpackhi_epi8(c, zero);
		__m128i dlo = _mm_unpacklo_epi8(d, zero);
		__m128i dhi = _mm_unpackhi_epi8(d, zero);
		__m128i vlo = _mm_add_epi16(_mm_mullo_epi16(clo, dlo), vbias);
		__m128i vhi = _mm_add_epi16(_mm_mullo_epi16(chi, dhi), vbias);

		vlo = _mm_srli_epi16(_mm_add_epi16(vlo, _mm_srli_epi16(vlo, 8)), 8);
		vhi = _mm_srli_epi16(_mm_add_epi16(vhi, _mm_srli_epi16(vhi, 8)), 8);

		*dst++ = _mm_adds_epu8(c, _mm_subs_epu8(d, _mm_packus_epi16(vlo, vhi)));
	} while(--n16);
}

void VDDSPBlend8_Overlay_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16) {
	__m128i *dst = (__m128i *)dst0;
	const __m128i *srca = (const __m128i *)src0;
	const __m128i *srcb = (const __m128i *)src1;

	__m128i zero = _mm_setzero_si128();
	__m128i vbias = _mm_set1_epi16(128);
	do {
		// r = c < 0.5 ? c*d*2 : 1-(1-c)*(2-2*d)
		//   = c < 0.5 ? c*d*2 : 1-2*(1-c)*(1-d)
		__m128i c = *srca++;
		__m128i d = *srcb++;

		__m128i highmask = _mm_cmplt_epi8(c, zero);
		__m128i ahalf = _mm_xor_si128(c, highmask);
		__m128i a = _mm_adds_epu8(ahalf, ahalf);
		__m128i b = _mm_xor_si128(d, highmask);

		__m128i alo = _mm_unpacklo_epi8(a, zero);
		__m128i ahi = _mm_unpackhi_epi8(a, zero);
		__m128i blo = _mm_unpacklo_epi8(b, zero);
		__m128i bhi = _mm_unpackhi_epi8(b, zero);
		__m128i vlo = _mm_add_epi16(_mm_mullo_epi16(alo, blo), vbias);
		__m128i vhi = _mm_add_epi16(_mm_mullo_epi16(ahi, bhi), vbias);

		vlo = _mm_srli_epi16(_mm_add_epi16(vlo, _mm_srli_epi16(vlo, 8)), 8);
		vhi = _mm_srli_epi16(_mm_add_epi16(vhi, _mm_srli_epi16(vhi, 8)), 8);

		*dst++ = _mm_xor_si128(_mm_packus_epi16(vlo, vhi), highmask);
	} while(--n16);
}

void VDDSPBlend8_HardLight_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16) {
	__m128i *dst = (__m128i *)dst0;
	const __m128i *srca = (const __m128i *)src0;
	const __m128i *srcb = (const __m128i *)src1;

	__m128i zero = _mm_setzero_si128();
	__m128i c255 = _mm_set1_epi16(255);
	__m128i b255 = _mm_set1_epi8(-1);
	__m128i vbias = _mm_set1_epi16(64);
	do {
		// r = d < 0.5 ? c*d*2 : 2*(c+d)-c*d*2-1;
		__m128i c = *srca++;
		__m128i d = *srcb++;

		// compute c*d*2
		__m128i clo = _mm_unpacklo_epi8(c, zero);
		__m128i chi = _mm_unpackhi_epi8(c, zero);
		__m128i dlo = _mm_unpacklo_epi8(d, zero);
		__m128i dhi = _mm_unpackhi_epi8(d, zero);
		__m128i vlo = _mm_add_epi16(_mm_mullo_epi16(clo, dlo), vbias);
		__m128i vhi = _mm_add_epi16(_mm_mullo_epi16(chi, dhi), vbias);

		vlo = _mm_srli_epi16(_mm_add_epi16(vlo, _mm_srli_epi16(vlo, 8)), 7);
		vhi = _mm_srli_epi16(_mm_add_epi16(vhi, _mm_srli_epi16(vhi, 8)), 7);

		// compute c*d*2, wrapped
		__m128i m = _mm_packus_epi16(_mm_and_si128(vlo, c255), _mm_and_si128(vhi, c255));

		// compute signed screen (wrapped)
		__m128i sum = _mm_add_epi8(c, d);
		__m128i sum2 = _mm_add_epi8(sum, sum);
		__m128i sscr = _mm_sub_epi8(_mm_sub_epi8(sum2, m), b255);

		// select signed screen if d >= 0.5
		__m128i highmask = _mm_cmplt_epi8(d, zero);

		*dst++ = _mm_or_si128(_mm_and_si128(highmask, sscr), _mm_andnot_si128(highmask, m));
	} while(--n16);
}

void VDDSPBlend8_LinearLight_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16) {
	__m128i *dst = (__m128i *)dst0;
	const __m128i *srca = (const __m128i *)src0;
	const __m128i *srcb = (const __m128i *)src1;

	__m128i c255 = _mm_set1_epi8(-1);
	do {
		// r = c + (2*d - 1);
		//   = c + d - (1 - d);
		__m128i c = *srca++;
		__m128i d = *srcb++;

		__m128i inv = _mm_xor_si128(d, c255);
		__m128i lighten = _mm_subs_epu8(d, inv);
		__m128i darken = _mm_subs_epu8(inv, d);

		*dst++ = _mm_subs_epu8(_mm_adds_epu8(c, lighten), darken);
	} while(--n16);
}

void VDDSPBlend8_PinLight_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16) {
	__m128i *dst = (__m128i *)dst0;
	const __m128i *srca = (const __m128i *)src0;
	const __m128i *srcb = (const __m128i *)src1;

	__m128i zero = _mm_setzero_si128();
	__m128i c255 = _mm_set1_epi8(-1);
	do {
		// r = d < 0.5 ? min(c, d*2) : max(c, d*2-1);
		//   = d < 0.5 ? min(c, d*2) : max(c, d - (1 - d));

		__m128i c = *srca++;
		__m128i d = *srcb++;

		__m128i lo = _mm_adds_epu8(d, d);
		__m128i hi = _mm_subs_epu8(d, _mm_xor_si128(d, c255));
		__m128i darkened = _mm_min_epu8(c, lo);
		__m128i lightened = _mm_max_epu8(c, hi);

		__m128i highmask = _mm_cmplt_epi8(d, zero);

		*dst++ = _mm_or_si128(_mm_and_si128(highmask, lightened), _mm_andnot_si128(highmask, darkened));
	} while(--n16);
}

void VDDSPBlend8_HardMix_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16) {
	__m128i *dst = (__m128i *)dst0;
	const __m128i *srca = (const __m128i *)src0;
	const __m128i *srcb = (const __m128i *)src1;

	__m128i c255 = _mm_set1_epi8(-1);
	do {
		// r = c + d > 1
		//   = c > 1 - d

		__m128i c = *srca++;
		__m128i d = *srcb++;

		__m128i wrapped = _mm_add_epi8(c, d);
		__m128i unwrapped = _mm_adds_epu8(c, d);

		*dst++ = _mm_xor_si128(_mm_cmpeq_epi8(wrapped, unwrapped), c255);
	} while(--n16);
}

void VDDSPBlend8_Difference_SSE2(void *dst0, const void *src0, const void *src1, uint32 n16) {
	__m128i *dst = (__m128i *)dst0;
	const __m128i *srca = (const __m128i *)src0;
	const __m128i *srcb = (const __m128i *)src1;

	do {
		__m128i c = *srca++;
		__m128i d = *srcb++;

		*dst++ = _mm_or_si128(_mm_subs_epu8(c, d), _mm_subs_epu8(d, c));
	} while(--n16);
}

void VDDSPBlend8_Select_SSE2(void *dst0, const void *src0, const void *src1, const void *srcm, uint32 n16) {
	__m128i *dst = (__m128i *)dst0;
	const __m128i *srca = (const __m128i *)src0;
	const __m128i *srcb = (const __m128i *)src1;
	const __m128i *srcc = (const __m128i *)srcm;

	__m128i zero = _mm_setzero_si128();
	do {
		__m128i c = *srcc;

		int bitmask = _mm_movemask_epi8(c);

		if (bitmask == 0)
			*dst = *srca;
		else if (bitmask == 0xFFFF)
			*dst = *srcb;
		else {
			__m128i a = *srca;
			__m128i b = *srcb;
			__m128i mask = _mm_cmplt_epi8(c, zero);

			*dst = _mm_or_si128(_mm_and_si128(b, mask), _mm_andnot_si128(mask, a));
		}

		++dst;
		++srca;
		++srcb;
		++srcc;
	} while(--n16);
}

void VDDSPBlend8_Lerp_SSE2(void *dst0, const void *src0, const void *src1, const void *srcm, uint32 n16) {
	__m128i *dst = (__m128i *)dst0;
	const __m128i *srca = (const __m128i *)src0;
	const __m128i *srcb = (const __m128i *)src1;
	const __m128i *srcc = (const __m128i *)srcm;

	__m128i zero = _mm_setzero_si128();
	__m128i vbias = _mm_set1_epi16(128);
	do {
		__m128i a = *srca++;
		__m128i b = *srcb++;
		__m128i c = *srcc++;

		__m128i d1 = _mm_subs_epu8(a, b);
		__m128i d2 = _mm_subs_epu8(b, a);

		__m128i d = _mm_or_si128(d1, d2);

		__m128i clo = _mm_unpacklo_epi8(c, zero);
		__m128i chi = _mm_unpackhi_epi8(c, zero);
		__m128i dlo = _mm_unpacklo_epi8(d, zero);
		__m128i dhi = _mm_unpackhi_epi8(d, zero);
		__m128i vlo = _mm_add_epi16(_mm_mullo_epi16(dlo, clo), vbias);
		__m128i vhi = _mm_add_epi16(_mm_mullo_epi16(dhi, chi), vbias);

		vlo = _mm_srli_epi16(_mm_add_epi16(vlo, _mm_srli_epi16(vlo, 8)), 8);
		vhi = _mm_srli_epi16(_mm_add_epi16(vhi, _mm_srli_epi16(vhi, 8)), 8);

		__m128i v = _mm_packus_epi16(vlo, vhi);

		__m128i negmask = _mm_cmpeq_epi8(zero, d2);

		*dst++ = _mm_add_epi8(a, _mm_sub_epi8(_mm_xor_si128(v, negmask), negmask));
	} while(--n16);
}
