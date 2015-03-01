//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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
#include <vd2/system/math.h>

#include <windows.h>

#include "resource.h"

extern HINSTANCE g_hInst;

///////////////////////////////////

#define YIQMODE_Y		(0)
#define YIQMODE_I		(1)
#define YIQMODE_Q		(2)
#define	YIQMODE_IQAVERAGE3 (3)
#define	YIQMODE_IQAVERAGE5 (4)
#define	YIQMODE_IQAV5TMP (5)
#define	YIQMODE_CHROMAUP	(6)
#define YIQMODE_CHROMADOWN	(7)

typedef signed long IQPixel;
typedef unsigned char YPixel;

typedef struct YIQFilterData {
	int			mode;
	IQPixel		*rows;
	unsigned char *tsramp;
	YPixel		*lummap;
	IQPixel		*iqmap;
} YIQFilterData;

#if 0
static void iq_average_row3(uint32 *_src, IQPixel *_dst, int _count) {
	uint32 *src = _src;
	IQPixel *dst = _dst;
	int count = 1-_count;

	uint32 c;
	long r, g, b;
	long	i1, q1,
			i2=0, q2=0,
			i3=0, q3=0;

	// dst[0]: [0] [0] [1]
	// dst[2]: [0] [1] [2]

	c = *src++;
	r = (c>>16) & 255;
	g = (c>> 8) & 255;
	b = c & 255;

	i2 = i1 = 54 * r - 133 * g + 79 * b; // I
	q2 = q1 = 154 * r - 72 * g - 82 * b; // Q

	do {
		c = *src++;
		r = (c>>16) & 255;
		g = (c>> 8) & 255;
		b = c & 255;

		i3 = i2; q3 = q2;
		i2 = i1; q2 = q1;
		i1 = 54 * r - 133 * g + 79 * b; // I
		q1 = 154 * r - 72 * g - 82 * b; // Q

		dst[0] = i1 + i2*2 + i3;
		dst[1] = q1 + q2*2 + q3;

		dst+=2;

	} while(++count);

	dst[0] = i2 + i1*3;
	dst[1] = q2 + q1*3;
}
#endif

static void iq_average_row5(uint32 *_src, IQPixel *_dst, int _count) {
	uint32 *src = _src;
	IQPixel *dst = _dst;
	int count = 2-_count;

	uint32 c;
	long r, g, b;
	long	i1, q1,
			i2=0, q2=0,
			i3=0, q3=0,
			i4=0, q4=0,
			i5=0, q5=0;

	// dst[0]: [0] [0] [0] [1] [2]
	// dst[2]: [0] [0] [1] [2] [3]
	// dst[4]: [0] [1] [2] [3] [4]

	c = *src++;
	r = (c>>16) & 255;
	g = (c>> 8) & 255;
	b = c & 255;

	i4 = i3 = i2 = 54 * r - 133 * g + 79 * b; // I
	q4 = q3 = q2 = 154 * r - 72 * g - 82 * b; // Q

	c = *src++;
	r = (c>>16) & 255;
	g = (c>> 8) & 255;
	b = c & 255;

	i1 = 54 * r - 133 * g + 79 * b; // I
	q1 = 154 * r - 72 * g - 82 * b; // Q

	do {
		c = *src++;
		r = (c>>16) & 255;
		g = (c>> 8) & 255;
		b = c & 255;

		i5 = i4; q5 = q4;
		i4 = i3; q4 = q3;
		i3 = i2; q3 = q2;
		i2 = i1; q2 = q1;
		i1 = 54 * r - 133 * g + 79 * b; // I
		q1 = 154 * r - 72 * g - 82 * b; // Q

		dst[0] = i1 + i2*2 + i3*2 + i4*2 + i5;
		dst[1] = q1 + q2*2 + q3*2 + q4*2 + q5;

		dst+=2;

	} while(++count);

	// dst[n-6]: [5] [4] [3] [2] [1]
	// dst[n-4]: [4] [3] [2] [1] [1]
	// dst[n-2]: [3] [2] [1] [1] [1]

	dst[0] = i1*3 + i2*2 + i3*2 + i4;
	dst[1] = q1*3 + q2*2 + q3*2 + q4;

	dst[2] = i1*5 + i2*2 + i3;
	dst[3] = q1*5 + q2*2 + q3;
}

// The IQ system sucks for what we do, so we're going to follow
// the lead of modern TVs and use (Y, R-Y, B-Y) instead of (Y, I, Q).
//
//	Forward matrix:
//
//	Y	= 0.30R + 0.59G + 0.11B
//	R-Y	= 0.70R - 0.59G - 0.11B (179, -151, -28)
//	B-Y	=-0.30R - 0.59G + 0.89B (-77, -151, 228)
//
//	Inverse matrix:
//
//	R = Y + (R-Y)
//	G = Y - 0.50847(R-Y) - 0.18644(B-Y)
//	B = Y + (B-Y)

static void rb_average_row3(uint32 *_src, IQPixel *_dst, int _count) {
	uint32 *src = _src;
	IQPixel *dst = _dst;
	int count = 2-_count;

	// dst[0]: [0] [0] [1]
	// dst[2]: [0] [1] [2]

	*dst	= ((((src[0]&0x00ff00ff)*3 + (src[1]&0x00ff00ff))>>2)&0x00ff00ff)
			| ((((src[0]&0x0000ff00)*3 + (src[1]&0x0000ff00))>>2)&0x0000ff00);
	src++;
	dst++;

	do {
		*dst	= ((((src[-1]&0x00ff00ff) + (src[0]&0x00ff00ff)*2 + (src[1]&0x00ff00ff))>>2)&0x00ff00ff)
				| ((((src[-1]&0x0000ff00) + (src[0]&0x0000ff00)*2 + (src[1]&0x0000ff00))>>2)&0x0000ff00);
		++src;
		++dst;

	} while(++count);

	*dst	= ((((src[-1]&0x00ff00ff) + (src[0]&0x00ff00ff)*3)>>2)&0x00ff00ff)
			| ((((src[-1]&0x0000ff00) + (src[0]&0x0000ff00)*3)>>2)&0x0000ff00);
}

static void rb_average_row5(uint32 *_src, IQPixel *_dst, int _count) {
	uint32 *src = _src;
	IQPixel *dst = _dst;
	int count = 4-_count;

	// dst[0]: [0] [0] [1]
	// dst[2]: [0] [1] [2]

	*dst	= ((((src[0]&0x00ff00ff)*5 + (src[1]&0x00ff00ff)*2 + (src[2]&0x00ff00ff))>>3)&0x00ff00ff)
			| ((((src[0]&0x0000ff00)*5 + (src[1]&0x0000ff00)*2 + (src[2]&0x0000ff00))>>3)&0x0000ff00);
	src++;
	dst++;

	*dst	= ((((src[-1]&0x00ff00ff)*3 + (src[0]&0x00ff00ff)*2 + (src[1]&0x00ff00ff)*2 + (src[2]&0x00ff00ff))>>3)&0x00ff00ff)
			| ((((src[-1]&0x0000ff00)*3 + (src[0]&0x0000ff00)*2 + (src[1]&0x0000ff00)*2 + (src[2]&0x0000ff00))>>3)&0x0000ff00);
	src++;
	dst++;

	do {
		*dst	= ((((src[-2]&0x00ff00ff) + (src[-1]&0x00ff00ff)*2 + (src[0]&0x00ff00ff)*2 + (src[1]&0x00ff00ff)*2 + (src[2]&0x00ff00ff))>>3)&0x00ff00ff)
				| ((((src[-2]&0x0000ff00) + (src[-1]&0x0000ff00)*2 + (src[0]&0x0000ff00)*2 + (src[1]&0x0000ff00)*2 + (src[2]&0x0000ff00))>>3)&0x0000ff00);
		++src;

		dst++;

	} while(++count);

	*dst	= ((((src[-2]&0x00ff00ff) + (src[-1]&0x00ff00ff)*2 + (src[0]&0x00ff00ff)*2 + (src[1]&0x00ff00ff)*3)>>3)&0x00ff00ff)
			| ((((src[-2]&0x0000ff00) + (src[-1]&0x0000ff00)*2 + (src[0]&0x0000ff00)*2 + (src[1]&0x0000ff00)*3)>>3)&0x0000ff00);
	++src;
	dst++;

	*dst	= ((((src[-2]&0x00ff00ff) + (src[-1]&0x00ff00ff)*2 + (src[0]&0x00ff00ff)*5)>>3)&0x00ff00ff)
			| ((((src[-2]&0x0000ff00) + (src[-1]&0x0000ff00)*2 + (src[0]&0x0000ff00)*5)>>3)&0x0000ff00);
}

//#define USE_YIQ_SPACE

#ifdef VD_CPU_X86
	extern "C" void asm_tv_average5row(uint32 *src, IQPixel *dst, int count);
	extern "C" void asm_tv_average5col(uint32 *, IQPixel *, IQPixel *, IQPixel *, IQPixel *, IQPixel *, long);
#else
#endif

static void ChromaShift(uint32 *dst0, uint32 *src0, ptrdiff_t dstpitch, ptrdiff_t srcpitch, sint32 w, sint32 h) {
	sint32 wt;

	do {
		uint32 *src = src0;
		uint32 *dst = dst0;

		wt = w;
		do {
			uint32 cs = *src;
			uint32 cd = *dst;
			int ydiff;
			int r, g, b;

			r = (cs>>16)&255;
			g = (cs>>8)&255;
			b = cs&255;

			ydiff = (54*((int)((cd>>16)&255)-r)+183*((int)((cd&0xff00)>>8)-g)+19*((int)(cd&255) - b) + 128) >> 8;

			r += ydiff;
			g += ydiff;
			b += ydiff;

			if (r<0) r=0; else if (r>255) r=255;
			if (g<0) g=0; else if (g>255) g=255;
			if (b<0) b=0; else if (b>255) b=255;

			*dst = (r<<16) + (g<<8) + b;

			++src, ++dst;
		} while(--wt);

		src0 = (uint32 *)((char *)src0 + srcpitch);
		dst0 = (uint32 *)((char *)dst0 + dstpitch);
	} while(--h);
}

static int yiq_run(const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	YIQFilterData *mfd = (YIQFilterData *)fa->filter_data;
	uint32 *dst = (uint32 *)fa->dst.data;
	long w, h;
	uint32 c;

	if (mfd->mode == YIQMODE_IQAVERAGE3) {
		IQPixel *row_p = mfd->rows,
				*row_c = mfd->rows + 1*fa->dst.w,
				*row_n = mfd->rows + 2*fa->dst.w,
				*row_t;

		IQPixel *r1, *r2, *r3;

		rb_average_row3(dst, row_p, fa->dst.w);

		memcpy(row_c, row_p, sizeof(IQPixel) * 1 * fa->dst.w);

		h = fa->dst.h;
		do {
			if (h>1)
				rb_average_row3((uint32 *)((char *)dst + fa->dst.pitch), row_n, fa->dst.w);
			else
				row_n = row_c;

			r1 = row_p;
			r2 = row_c;
			r3 = row_n;

			w = fa->dst.w;
			do {
				uint32 c, d;
				long y, r, g, b;
	
				c = *dst;
				d	= ((((r1[0] & 0x00ff00ff) + 2*(r2[0] & 0x00ff00ff) + (r3[0] & 0x00ff00ff))>>2) & 0x00ff00ff)
					| ((((r1[0] & 0x0000ff00) + 2*(r2[0] & 0x0000ff00) + (r3[0] & 0x0000ff00))>>2) & 0x0000ff00);

				y	= ((54*((c>>16)&255) + 183*((c>>8)&255) + 19*(c&255))>>8)
					- ((54*((d>>16)&255) + 183*((d>>8)&255) + 19*(d&255))>>8);

				r = ((d>>16)&255) + y;
				g = ((d>> 8)&255) + y;
				b = ((d    )&255) + y;

				r1++;
				r2++;
				r3++;

				// [r]   [ 256  160  243][y]
				// [g] = [ 256 -164  -71][i]
				// [b]   [ 256  443 -283][q]

				if (r<0) r=0; else if (r>255) r=255;
				if (g<0) g=0; else if (g>255) g=255;
				if (b<0) b=0; else if (b>255) b=255;

				*dst++ = (r<<16) + (g<<8) + b;
			} while(--w);

			row_t = row_p; row_p = row_c; row_c = row_n; row_n = row_t;

			dst = (uint32 *)((char *)dst + fa->dst.modulo);
		} while(--h);
	} else if (mfd->mode == YIQMODE_IQAVERAGE5) {
		IQPixel *row_f = mfd->rows,
				*row_p = mfd->rows + 1*fa->dst.w,
				*row_c = mfd->rows + 2*fa->dst.w,
				*row_n = mfd->rows + 3*fa->dst.w,
				*row_l = mfd->rows + 4*fa->dst.w,
				*row_t;

		IQPixel *r1, *r2, *r3, *r4, *r5;

		rb_average_row5(dst, row_f, fa->dst.w);

		memcpy(row_p, row_f, sizeof(IQPixel) * 2 * fa->dst.w);
		memcpy(row_c, row_f, sizeof(IQPixel) * 2 * fa->dst.w);

#ifdef VD_CPU_X86
		asm_tv_average5row((uint32 *)((char *)dst + fa->dst.pitch), row_n, fa->dst.w);
#else
		rb_average_row5((uint32 *)((char *)dst + fa->dst.pitch), row_n, fa->dst.w);
#endif

		h = fa->dst.h;
		do {
			if (h>2)
#ifdef VD_CPU_X86
				asm_tv_average5row((uint32 *)((char *)dst + fa->dst.pitch*2), row_l, fa->dst.w);
#else
				rb_average_row5((uint32 *)((char *)dst + fa->dst.pitch*2), row_l, fa->dst.w);
#endif
			else
				row_l = row_n;

			r1 = row_f;
			r2 = row_p;
			r3 = row_c;
			r4 = row_n;
			r5 = row_l;

#ifdef VD_CPU_X86
			asm_tv_average5col(dst, r1, r2, r3, r4, r5, fa->dst.w);
			dst += fa->dst.w;
#else
			w = fa->dst.w;
			do {
				uint32 c, d;
				long y, r, g, b;
	
				c = *dst;
				d	= ((((r1[0] & 0x00ff00ff) + 2*(r2[0] & 0x00ff00ff) + 2*(r3[0] & 0x00ff00ff) + 2*(r4[0] & 0x00ff00ff) + (r5[0] & 0x00ff00ff))>>3) & 0x00ff00ff)
					| ((((r1[0] & 0x0000ff00) + 2*(r2[0] & 0x0000ff00) + 2*(r3[0] & 0x0000ff00) + 2*(r4[0] & 0x0000ff00) + (r5[0] & 0x0000ff00))>>3) & 0x0000ff00);

#if 0
				y	= ((54*((c>>16)&255) + 183*((c>>8)&255) + 19*(c&255))>>8)
					- ((54*((d>>16)&255) + 183*((d>>8)&255) + 19*(d&255))>>8);
#else
				y	=( 54*((long)((c>>16)&255) - (long)((d>>16)&255))
					+ 183*((long)((c>> 8)&255) - (long)((d>> 8)&255))
					+  19*((long)((c    )&255) - (long)((d    )&255)))>>8;
#endif
				r = ((d>>16)&255) + y;
				g = ((d>> 8)&255) + y;
				b = ((d    )&255) + y;

				r1++;
				r2++;
				r3++;
				r4++;
				r5++;

				// [r]   [ 256  160  243][y]
				// [g] = [ 256 -164  -71][i]
				// [b]   [ 256  443 -283][q]

				if (r<0) r=0; else if (r>255) r=255;
				if (g<0) g=0; else if (g>255) g=255;
				if (b<0) b=0; else if (b>255) b=255;

				*dst++ = (r<<16) + (g<<8) + b;
			} while(--w);
#endif

			row_t = row_f;
			row_f = row_p;
			row_p = row_c;
			row_c = row_n;
			row_n = row_l;
			row_l = row_t;

			dst = (uint32 *)((char *)dst + fa->dst.modulo);
		} while(--h);
	} else if (mfd->mode == YIQMODE_IQAV5TMP) {
		IQPixel *row_f = mfd->rows,
				*row_p = mfd->rows + 2*fa->dst.w,
				*row_c = mfd->rows + 4*fa->dst.w,
				*row_n = mfd->rows + 6*fa->dst.w,
				*row_l = mfd->rows + 8*fa->dst.w,
				*row_t;

		IQPixel *r1, *r2, *r3, *r4, *r5;
		YPixel *ly = mfd->lummap;
		IQPixel *liq = mfd->iqmap;

		iq_average_row5(dst, row_f, fa->dst.w);
		memcpy(row_p, row_f, sizeof(IQPixel) * 2 * fa->dst.w);
		memcpy(row_c, row_f, sizeof(IQPixel) * 2 * fa->dst.w);

		iq_average_row5((uint32 *)((char *)dst + fa->dst.pitch), row_n, fa->dst.w);

		h = fa->dst.h;
		do {
			if (h>2)
				iq_average_row5((uint32 *)((char *)dst + fa->dst.pitch*2), row_l, fa->dst.w);
			else
				row_l = row_n;

			r1 = row_f;
			r2 = row_p;
			r3 = row_c;
			r4 = row_n;
			r5 = row_l;

			w = fa->dst.w;
			do {
				uint32 c;
				long y, i, q, r, g, b;
				long m;
	
				c = *dst;

				y = 4915 * ((c>>16) & 255) + 9667 * ((c>>8) & 255) + 1802 * (c & 255);
				i = ((long)r1[0] + 2*(long)r2[0] + 2*(long)r3[0] + 2*(long)r4[0] + (long)r5[0] + 128)>>8;
				q = ((long)r1[1] + 2*(long)r2[1] + 2*(long)r3[1] + 2*(long)r4[1] + (long)r5[1] + 128)>>8;

				r1 += 2;
				r2 += 2;
				r3 += 2;
				r4 += 2;
				r5 += 2;

				// [r]   [ 256  160  243][y]
				// [g] = [ 256 -164  -71][i]
				// [b]   [ 256  443 -283][q]

				m = mfd->tsramp[255 + ((y+8192)>>14) - *ly];
				*ly++ = (YPixel)((y+8192)>>14);

				liq[0] = i = (IQPixel)((liq[0]*m + i*(256-m))>>8);
				liq[1] = q = (IQPixel)((liq[1]*m + q*(256-m))>>8);

				liq += 2;

				r = (y + 160*i + 243*q + 8192) >> 14;
				g = (y - 164*i -  71*q + 8192) >> 14;
				b = (y + 443*i - 283*q + 8192) >> 14;

				if (r<0) r=0; else if (r>255) r=255;
				if (g<0) g=0; else if (g>255) g=255;
				if (b<0) b=0; else if (b>255) b=255;

				*dst++ = (r<<16) + (g<<8) + b;
			} while(--w);

			row_t = row_f;
			row_f = row_p;
			row_p = row_c;
			row_c = row_n;
			row_n = row_l;
			row_l = row_t;

			dst = (uint32 *)((char *)dst + fa->dst.modulo);
		} while(--h);
	} else if (mfd->mode == YIQMODE_CHROMAUP) {
		ChromaShift(fa->src.Address32(0,0), fa->src.Address32(0,1), -fa->src.pitch, -fa->src.pitch, fa->src.w, fa->src.h-1);
	} else if (mfd->mode == YIQMODE_CHROMADOWN) {
		ChromaShift(fa->src.Address32i(0,0), fa->src.Address32i(0,1), fa->src.pitch, fa->src.pitch, fa->src.w, fa->src.h-1);
	} else {
		int cr, cg, cb, bias;
		int v;

		switch(mfd->mode) {
		case YIQMODE_Y:
			cr = VDRoundToInt(+0.30 * 256);
			cg = VDRoundToInt(+0.59 * 256);
			cb = VDRoundToInt(+0.11 * 256);
			bias = 0;
			break;

		case YIQMODE_I:
			cr = VDRoundToInt(+0.21 * 256);
			cg = VDRoundToInt(-0.52 * 256);
			cb = VDRoundToInt(+0.31 * 256);
			bias = 128 * 256;
			break;

		case YIQMODE_Q:
			cr = VDRoundToInt(+0.60 * 256);
			cg = VDRoundToInt(-0.28 * 256);
			cb = VDRoundToInt(-0.32 * 256);
			bias = 128 * 256;
			break;

		}

		h = fa->dst.h;
		do {
			w = fa->dst.w;
			do {
				c = *dst;
				v = (cr * ((c>>16)&255) + cg * ((c>>8)&255) + cb * (c&255) + bias)>>8;

				if (v<0) v=0; else if (v>255) v=255;

				*dst++ = v + (v<<8) + (v<<16);
			} while(--w);

			dst = (uint32 *)((char *)dst + fa->dst.modulo);
		} while(--h);
	}

	return 0;
}

static long yiq_param(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	pxldst.data = pxlsrc.data;
	pxldst.pitch = pxlsrc.pitch;

	return FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

/////////////////////////////////////////////////////////////

static INT_PTR CALLBACK YIQConfigDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
        case WM_INITDIALOG:
			SetWindowLongPtr(hDlg, DWLP_USER, lParam);
			CheckDlgButton(hDlg, IDC_MODE_Y, ((YIQFilterData *)lParam)->mode == YIQMODE_Y);
			CheckDlgButton(hDlg, IDC_MODE_I, ((YIQFilterData *)lParam)->mode == YIQMODE_I);
			CheckDlgButton(hDlg, IDC_MODE_Q, ((YIQFilterData *)lParam)->mode == YIQMODE_Q);
			CheckDlgButton(hDlg, IDC_MODE_IQAVERAGE3, ((YIQFilterData *)lParam)->mode == YIQMODE_IQAVERAGE3);
			CheckDlgButton(hDlg, IDC_MODE_IQAVERAGE5, ((YIQFilterData *)lParam)->mode == YIQMODE_IQAVERAGE5);
			CheckDlgButton(hDlg, IDC_MODE_IQAV5TMP, ((YIQFilterData *)lParam)->mode == YIQMODE_IQAV5TMP);
			CheckDlgButton(hDlg, IDC_MODE_CHROMAUP, ((YIQFilterData *)lParam)->mode == YIQMODE_CHROMAUP);
			CheckDlgButton(hDlg, IDC_MODE_CHROMADOWN, ((YIQFilterData *)lParam)->mode == YIQMODE_CHROMADOWN);
            return (TRUE);

        case WM_COMMAND:                 
			switch(LOWORD(wParam)) {

            case IDOK:
				{
					YIQFilterData *mfd = (YIQFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);

					if (IsDlgButtonChecked(hDlg, IDC_MODE_Y)) mfd->mode = YIQMODE_Y;
					if (IsDlgButtonChecked(hDlg, IDC_MODE_I)) mfd->mode = YIQMODE_I;
					if (IsDlgButtonChecked(hDlg, IDC_MODE_Q)) mfd->mode = YIQMODE_Q;
					if (IsDlgButtonChecked(hDlg, IDC_MODE_IQAVERAGE3)) mfd->mode = YIQMODE_IQAVERAGE3;
					if (IsDlgButtonChecked(hDlg, IDC_MODE_IQAVERAGE5)) mfd->mode = YIQMODE_IQAVERAGE5;
					if (IsDlgButtonChecked(hDlg, IDC_MODE_IQAV5TMP)) mfd->mode = YIQMODE_IQAV5TMP;
					if (IsDlgButtonChecked(hDlg, IDC_MODE_CHROMAUP)) mfd->mode = YIQMODE_CHROMAUP;
					if (IsDlgButtonChecked(hDlg, IDC_MODE_CHROMADOWN)) mfd->mode = YIQMODE_CHROMADOWN;
				}
				EndDialog(hDlg, 0);
				return TRUE;

			case IDCANCEL:
                EndDialog(hDlg, 1);
                return TRUE;

            }
            break;
    }
    return FALSE;
}

static int yiq_config(VDXFilterActivation *fa, const VDXFilterFunctions *ff, VDXHWND hWnd) {
	return DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_TV), (HWND)hWnd, YIQConfigDlgProc, (LPARAM)fa->filter_data);
}

/////////////////////////////////////////////////////////////

static void yiq_string2(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int maxlen) {
	YIQFilterData *mfd = (YIQFilterData *)fa->filter_data;

	_snprintf(buf, maxlen, " (%s)", mfd ? (
							mfd->mode==YIQMODE_Y ? "Y channel" :
							mfd->mode==YIQMODE_I ? "I channel" :
							mfd->mode==YIQMODE_Q ? "Q channel" :
							mfd->mode==YIQMODE_IQAVERAGE3 ? "I/Q 3x3 average" :
							mfd->mode==YIQMODE_IQAVERAGE5 ? "I/Q 5x5 average" :
							mfd->mode==YIQMODE_IQAV5TMP ? "I/Q 5x5 + t/s" :
							mfd->mode==YIQMODE_CHROMAUP ? "chroma up" :
							"chroma down"
						) : "unconfigured");
}

static int yiq_start(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	YIQFilterData *mfd = (YIQFilterData *)fa->filter_data;

	if (mfd->mode == YIQMODE_IQAVERAGE3) {
		if (!(mfd->rows = new IQPixel[fa->dst.w * 3]))
			return 1;
	} else if (mfd->mode == YIQMODE_IQAVERAGE5) {
		if (!(mfd->rows = new IQPixel[fa->dst.w * 5]))
			return 1;
	} else if (mfd->mode == YIQMODE_IQAV5TMP) {
		if (!(mfd->rows = new IQPixel[fa->dst.w * 10]))
			return 1;

		if (!(mfd->lummap = new YPixel[fa->dst.w * fa->dst.h]))
			return 1;

		memset(mfd->lummap, 0x80, sizeof(YPixel)*fa->dst.w*fa->dst.h);

		if (!(mfd->iqmap = new IQPixel[fa->dst.w * fa->dst.h * 2]))
			return 1;

		memset(mfd->iqmap, 0, sizeof(IQPixel)*fa->dst.w*fa->dst.h*2);

		if (!(mfd->tsramp = new unsigned char [511]))
			return 1;

		// initialize tsramp

		int i;

		for(i=0; i<16; i++) {
			mfd->tsramp[255 + i] = 192;
			mfd->tsramp[255 - i] = 192;
		}
		for(; i<112; i++) {
			mfd->tsramp[255 + i] = (unsigned char)((112-i)<<1);
			mfd->tsramp[255 - i] = (unsigned char)((112-i)<<1);
		}
		for(; i<256; i++) {
			mfd->tsramp[255 + i] = 0;
			mfd->tsramp[255 - i] = 0;
		}
	}

	return 0;
}

static int yiq_end(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	YIQFilterData *mfd = (YIQFilterData *)fa->filter_data;

	delete[] mfd->rows;		mfd->rows = NULL;
	delete[] mfd->lummap;	mfd->lummap = NULL;
	delete[] mfd->iqmap;	mfd->iqmap = NULL;
	delete[] mfd->tsramp;	mfd->tsramp = NULL;

	return 0;
}

static void yiq_script_config(IVDXScriptInterpreter *isi, void *lpVoid, VDXScriptValue *argv, int argc) {
	VDXFilterActivation *fa = (VDXFilterActivation *)lpVoid;
	YIQFilterData *mfd = (YIQFilterData *)fa->filter_data;

	mfd->mode = argv[0].asInt();
}

static VDXScriptFunctionDef yiq_func_defs[]={
	{ (VDXScriptFunctionPtr)yiq_script_config, "Config", "0i" },
	{ NULL },
};

static VDXScriptObject yiq_obj={
	NULL, yiq_func_defs
};

static bool yiq_script_line(VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int buflen) {
	YIQFilterData *mfd = (YIQFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d)", mfd->mode);

	return true;
}

extern const VDXFilterDefinition g_VDVFTV={
	0,0,NULL,
	"TV",
	"Processes video data in NTSC native YIQ format.",
	NULL,
	NULL,
	sizeof(YIQFilterData),
	NULL,NULL,
	yiq_run,
	yiq_param,
	yiq_config,
	NULL,
	yiq_start,
	yiq_end,
	&yiq_obj,
	yiq_script_line,
	yiq_string2
};
