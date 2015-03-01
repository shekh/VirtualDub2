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
#include "DSP.h"

void VDDSPBlend8_LerpConst(void *dst0, const void *src0, const void *src1, uint32 n16, uint8 factor) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		sint32 c = *srca++;
		sint32 d = *srcb++;
		sint32 r = ((c << 8) - c) + (d - c) * factor + 0x80;

		r += r >> 8;

		*dst++ = (uint8)(r >> 8);
	} while(--n);
}

void VDDSPBlend8_Min(void *dst0, const void *src0, const void *src1, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		uint8 c = *srca++;
		uint8 d = *srcb++;

		*dst++ = c < d ? c : d;
	} while(--n);
}

void VDDSPBlend8_Max(void *dst0, const void *src0, const void *src1, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		uint8 c = *srca++;
		uint8 d = *srcb++;

		*dst++ = c > d ? c : d;
	} while(--n);
}

void VDDSPBlend8_Add(void *dst0, const void *src0, const void *src1, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		uint8 c = *srca++;
		uint8 d = *srcb++;

		sint32 r = (sint32)c + (sint32)d;

		r |= (255 - r) >> 31;

		*dst++ = (uint8)r;
	} while(--n);
}

void VDDSPBlend8_Multiply(void *dst0, const void *src0, const void *src1, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		sint32 c = *srca++;
		sint32 d = *srcb++;
		sint32 r = c * d + 0x80;

		r += r >> 8;

		*dst++ = (uint8)(r >> 8);
	} while(--n);
}

void VDDSPBlend8_LinearBurn(void *dst0, const void *src0, const void *src1, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		sint32 c = *srca++;
		sint32 d = *srcb++;
		sint32 r = c + d - 255;

		r &= ~(r >> 31);

		*dst++ = (uint8)r;
	} while(--n);
}

void VDDSPBlend8_ColorBurn(void *dst0, const void *src0, const void *src1, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		// r = 1 - (1 - c)/d
		sint32 c = *srca++;
		sint32 d = *srcb++;
		sint32 r = 0;
		
		if (d) {
			r = 255 - (255*(255 - c) + (d >> 1)) / d;

			r &= (~r >> 31);
		}

		*dst++ = (uint8)r;
	} while(--n);
}

void VDDSPBlend8_Screen(void *dst0, const void *src0, const void *src1, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		// r = 1 - (1-a)*(1-b)
		//   = 1 - (1 - a - b + a*b)
		//   = a + b - a*b
		sint32 c = *srca++;
		sint32 d = *srcb++;
		sint32 m = c * d + 128;
		sint32 r = c + d - ((m + (m >> 8)) >> 8);
		
		*dst++ = (uint8)r;
	} while(--n);
}

void VDDSPBlend8_ColorDodge(void *dst0, const void *src0, const void *src1, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		// r = c / (1 - d)
		sint32 c = *srca++;
		sint32 d = *srcb++;
		sint32 e = 255 - d;
		sint32 r = e ? (255 * c + (e >> 1)) / e : c ? 255 : 0;

		if (r > 255)
			r = 255;
		
		*dst++ = (uint8)r;
	} while(--n);
}

void VDDSPBlend8_Overlay(void *dst0, const void *src0, const void *src1, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		// r = c < 0.5 ? c*d*2 : 1-(1-a)*(2-2*b)
		//   = c < 0.5 ? c*d*2 : 1-(2-2*a-2*b+2*a*b)
		//   = c < 0.5 ? c*d*2 : 2*(a + b - a * b)-1
		sint32 c = *srca++;
		sint32 d = *srcb++;
		sint32 r;

		if (c < 128) {
			r = c * d * 2 + 0x80;
			r += r >> 8;
			r >>= 8;
		} else {
			r = c * d + 128;
			r += r >> 8;
			r >>= 8;
			r = c + d - r;
			r += r;
			r -= 255;

			if (r < 0)
				r = 0;
		}

		*dst++ = (uint8)r;
	} while(--n);
}

void VDDSPBlend8_SoftLight(void *dst0, const void *src0, const void *src1, uint32 n16) {
	static const uint8 kDarkenCurve[256]={
		 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,11,12,13,14,
		15,16,17,18,18,19,20,21,22,23,23,24,25,26,26,27,
		28,29,29,30,31,32,32,33,34,34,35,36,36,37,38,38,
		39,40,40,41,41,42,43,43,44,44,45,45,46,46,47,47,
		48,48,49,49,50,50,51,51,52,52,53,53,53,54,54,55,
		55,55,56,56,56,57,57,57,58,58,58,59,59,59,59,60,
		60,60,60,61,61,61,61,61,62,62,62,62,62,62,63,63,
		63,63,63,63,63,63,63,63,64,64,64,64,64,64,64,64,
		64,64,64,64,64,64,64,64,63,63,63,63,63,63,63,63,
		63,63,62,62,62,62,62,62,61,61,61,61,61,60,60,60,
		60,59,59,59,59,58,58,58,57,57,57,56,56,56,55,55,
		55,54,54,53,53,53,52,52,51,51,50,50,49,49,48,48,
		47,47,46,46,45,45,44,44,43,43,42,41,41,40,40,39,
		38,38,37,36,36,35,34,34,33,32,32,31,30,29,29,28,
		27,26,26,25,24,23,23,22,21,20,19,18,18,17,16,15,
		14,13,12,11,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
	};

	static const uint8 kLightenCurve[256]={
		 0, 3, 6, 9,11,14,16,19,21,23,26,28,30,32,33,35,
		37,39,40,42,43,45,46,47,48,49,51,52,53,53,54,55,
		56,57,57,58,58,59,60,60,60,61,61,62,62,62,62,63,
		63,63,63,63,63,63,64,64,64,64,64,64,64,64,64,64,
		64,64,64,64,64,64,64,64,63,63,63,63,63,63,63,63,
		63,63,63,62,62,62,62,62,62,62,61,61,61,61,61,61,
		60,60,60,60,60,59,59,59,59,59,58,58,58,58,57,57,
		57,57,56,56,56,56,55,55,55,55,54,54,54,54,53,53,
		53,52,52,52,51,51,51,51,50,50,50,49,49,49,48,48,
		48,47,47,47,46,46,46,45,45,45,44,44,43,43,43,42,
		42,42,41,41,40,40,40,39,39,39,38,38,37,37,37,36,
		36,35,35,35,34,34,33,33,33,32,32,31,31,31,30,30,
		29,29,28,28,28,27,27,26,26,25,25,25,24,24,23,23,
		22,22,21,21,21,20,20,19,19,18,18,17,17,16,16,15,
		15,15,14,14,13,13,12,12,11,11,10,10, 9, 9, 8, 8,
		 7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0,
	};

	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		// r = (d <= 0.5 ) ? c + (2*d - 1) * c*(1-c)
		//   : (c <= 0.25) ? c + (2*d - 1) * c*((16*c-12)*c+3)
		//	 :               c + (2*d - 1) * (sqrt(c) - c)
		//
		// r = c + (2*d - 1) * (  (d <= 0.5 ) ? c*(1-c)
		//                      : (c <= 0.25) ? c*((16*c-12)*c+3)
		//	                    :               (sqrt(c) - c)

		sint32 c = *srca++;
		sint32 d = *srcb++;
		sint32 r;

		if (d < 128) {
			sint32 darken = (255 - (d + d)) * kDarkenCurve[c] + 128;

			darken += darken >> 8;
			darken >>= 8;

			r = c - darken;

			if (r < 0)
				r = 0;
		} else {
			sint32 lighten = ((d + d) - 255) * kLightenCurve[c] + 128;

			lighten += lighten >> 8;
			lighten >>= 8;

			r = c + lighten;

			if (r > 255)
				r = 255;
		}

		*dst++ = (uint8)r;
	} while(--n);
}

void VDDSPBlend8_HardLight(void *dst0, const void *src0, const void *src1, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		// r = d < 0.5 ? c*d*2 : 1-(1-c)*(2-d*2);
		//   = d < 0.5 ? c*d*2 : 1-2*(1-c-d+c*d);
		//   = d < 0.5 ? c*d*2 : 2*(c+d)-2*c*d-1;

		sint32 c = *srca++;
		sint32 d = *srcb++;
		sint32 r = c * d * 2 + 128;
		r += r >> 8;
		r >>= 8;

		if (d >= 128)
			r = 2*(c + d) - r - 255;

		*dst++ = (uint8)r;
	} while(--n);
}

void VDDSPBlend8_VividLight(void *dst0, const void *src0, const void *src1, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		// r = d < 0.5 ? 1 - (1 - c)/(2*d) : c/(2-2*d);
		//   = d < 0.5 ? 1 - (1 - c)/(2*d) : c/(2-2*d);

		sint32 c = *srca++;
		sint32 d = *srcb++;
		sint32 r;

		if (d < 128) {
			if (d == 0)
				r = 0;
			else {
				r = 255 - ((255 - c)*255 + d) / (2*d);

				if (r < 0)
					r = 0;
			}
		} else {
			if (d == 255)
				r = 255;
			else {
				r = (c*255 + 255 - d) / (2*(255 - d));

				if (r > 255)
					r = 255;
			}
		}

		*dst++ = (uint8)r;
	} while(--n);
}

void VDDSPBlend8_LinearLight(void *dst0, const void *src0, const void *src1, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		// r = c + (2*d - 1);
		sint32 c = *srca++;
		sint32 d = *srcb++;
		sint32 r = c + d + d - 255;

		if (r < 0)
			r = 0;
		else if (r > 255)
			r = 255;

		*dst++ = (uint8)r;
	} while(--n);
}

void VDDSPBlend8_PinLight(void *dst0, const void *src0, const void *src1, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		// r = d < 0.5 ? min(c, d*2) : max(c, d*2-1);

		sint32 c = *srca++;
		sint32 d = *srcb++;
		sint32 r = c;

		if (d < 128) {
			d += d;

			if (r > d)
				r = d;
		} else {
			d = d + d - 255;

			if (r < d)
				r = d;
		}

		*dst++ = (uint8)r;
	} while(--n);
}

void VDDSPBlend8_HardMix(void *dst0, const void *src0, const void *src1, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		// r = c + d > 1
		sint32 c = *srca++;
		sint32 d = *srcb++;

		if (c + d > 255)
			*dst = 255;
		else
			*dst = 0;

		++dst;
	} while(--n);
}

void VDDSPBlend8_Difference(void *dst0, const void *src0, const void *src1, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		sint32 c = *srca++;
		sint32 d = *srcb++;
		sint32 r = c - d;
		sint32 s = r >> 31;

		r ^= s;
		r -= s;

		*dst++ = (uint8)r;
	} while(--n);
}

void VDDSPBlend8_Exclusion(void *dst0, const void *src0, const void *src1, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;

	do {
		// r = c + d - 2*c*d
		sint32 c = *srca++;
		sint32 d = *srcb++;
		sint32 r = (c << 8) + (d << 8) - c - d - 2*c*d + 128;

		r += r >> 8;
		r >>= 8;

		*dst++ = (uint8)r;
	} while(--n);
}

void VDDSPBlend8_Select(void *dst0, const void *src0, const void *src1, const void *srcm, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;
	const uint8 *srcc = (const uint8 *)srcm;

	do {
		if (*srcc >= 0x80)
			*dst = *srcb;
		else
			*dst = *srca;

		++srca;
		++srcb;
		++srcc;
		++dst;
	} while(--n);
}

void VDDSPBlend8_Lerp(void *dst0, const void *src0, const void *src1, const void *srcm, uint32 n16) {
	uint32 n = n16 << 4;
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srca = (const uint8 *)src0;
	const uint8 *srcb = (const uint8 *)src1;
	const uint8 *srcc = (const uint8 *)srcm;

	do {
		const uint8 c = *srcc;

		if (c == 0)
			*dst = *srca;
		else if (c == 255)
			*dst = *srcb;
		else {
			const uint8 a = *srca;
			const uint8 b = *srcb;
			int d = ((int)b - (int)a) * (int)c + 0x80;

			d += d >> 8;
			d >>= 8;

			*dst = (uint8)(a + d);
		}

		++srca;
		++srcb;
		++srcc;
		++dst;
	} while(--n);
}
