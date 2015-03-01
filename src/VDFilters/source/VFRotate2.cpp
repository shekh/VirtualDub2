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

#include <math.h>
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include "resource.h"
#include <vd2/system/cpuaccel.h>
#include <vd2/system/math.h>

/////////////////////////////////////////////////////////////////////

#ifdef VD_CPU_X86
	#define USE_ASM
#endif

#ifdef _MSC_VER
	#pragma warning(disable: 4799)		// function has no EMMS instruction
#endif

/////////////////////////////////////////////////////////////////////

namespace {
	static COLORREF g_crCustomColors[16];

	bool guiChooseColor(HWND hwnd, COLORREF& rgbOld) {
		CHOOSECOLOR cc;

		memset(&cc, 0, sizeof(CHOOSECOLOR));
		cc.lStructSize	= sizeof(CHOOSECOLOR);
		cc.hwndOwner	= hwnd;
		cc.lpCustColors	= (LPDWORD)g_crCustomColors;
		cc.rgbResult	= rgbOld;
		cc.Flags		= CC_FULLOPEN | CC_RGBINIT;

		if (ChooseColor(&cc)==TRUE) {;
			rgbOld = cc.rgbResult;
			return true;
		}

		return false;
	}
}

/////////////////////////////////////////////////////////////////////

extern HINSTANCE g_hInst;

namespace {
	//	void MakeCubic4Table(
	//		int *table,			pointer to 256x4 int array
	//		double A,			'A' value - determines characteristics
	//		mmx_table);			generate interleaved table
	//
	//	Generates a table suitable for cubic 4-point interpolation.
	//
	//	Each 4-int entry is a set of four coefficients for a point
	//	(n/256) past y[1].  They are in /16384 units.
	//
	//	A = -1.0 is the original VirtualDub bicubic filter, but it tends
	//	to oversharpen video, especially on rotates.  Use A = -0.75
	//	for a filter that resembles Photoshop's.


	void MakeCubic4Table(int *table, double A, bool mmx_table) {
		int i;

		for(i=0; i<256; i++) {
			double d = (double)i / 256.0;
			int y1, y2, y3, y4, ydiff;

			// Coefficients for all four pixels *must* add up to 1.0 for
			// consistent unity gain.
			//
			// Two good values for A are -1.0 (original VirtualDub bicubic filter)
			// and -0.75 (closely matches Photoshop).

			y1 = (int)floor(0.5 + (        +     A*d -       2.0*A*d*d +       A*d*d*d) * 16384.0);
			y2 = (int)floor(0.5 + (+ 1.0             -     (A+3.0)*d*d + (A+2.0)*d*d*d) * 16384.0);
			y3 = (int)floor(0.5 + (        -     A*d + (2.0*A+3.0)*d*d - (A+2.0)*d*d*d) * 16384.0);
			y4 = (int)floor(0.5 + (                  +           A*d*d -       A*d*d*d) * 16384.0);

			// Normalize y's so they add up to 16384.

			ydiff = (16384 - y1 - y2 - y3 - y4)/4;
			_ASSERT(ydiff > -16 && ydiff < 16);

			y1 += ydiff;
			y2 += ydiff;
			y3 += ydiff;
			y4 += ydiff;

			if (mmx_table) {
				table[i*4 + 0] = table[i*4 + 1] = (y2<<16) | (y1 & 0xffff);
				table[i*4 + 2] = table[i*4 + 3] = (y4<<16) | (y3 & 0xffff);
			} else {
				table[i*4 + 0] = y1;
				table[i*4 + 1] = y2;
				table[i*4 + 2] = y3;
				table[i*4 + 3] = y4;
			}
		}
	}

	struct RotateRow {
		int leftzero, left, right, rightzero;
		sint64 xaccum_left, yaccum_left;
	};
}


struct VDRotate2FilterData {
	int angle;
	int filtmode;

	// working variables

	IVDXFilterPreview *ifp;

	sint64	u_step;
	sint64	v_step;

	RotateRow *rows;
	int *coeff_tbl;

	COLORREF	rgbColor;
	HBRUSH		hbrColor;

	bool	fExpandBounds;
	
};

static const char *const szModeStrings[]={
	"point",
	"bilinear",
	"bicubic"
};

enum {
	FILTMODE_POINT		= 0,
	FILTMODE_BILINEAR,
	FILTMODE_BICUBIC,
	FILTMODE_COUNT
};

/////////////////////////////////////////////////////////////////////

extern "C" void asm_rotate_point(
		uint32 *src,
		uint32 *dst,
		long width,
		long Ufrac,
		long Vfrac,
		long UVintstepV,
		long UVintstepnoV,
		long Ustep,
		long Vstep);

extern "C" void asm_rotate_bilinear(
		uint32 *src,
		uint32 *dst,
		long width,
		long pitch,
		long Ufrac,
		long Vfrac,
		long UVintstepV,
		long UVintstepnoV,
		long Ustep,
		long Vstep);

/////////////////////////////////////////////////////////////////////

static uint32 bilinear_interp(uint32 c1, uint32 c2, uint32 c3, uint32 c4, unsigned long cox, unsigned long coy) {
	int co1, co2, co3, co4;

	co4 = (cox * coy) >> 8;
	co3 = coy - co4;
	co2 = cox - co4;
	co1 = 0x100 - coy - co2;

	return   ((((c1 & 0x00FF00FF)*co1 + (c2 & 0x00FF00FF)*co2 + (c3 & 0x00FF00FF)*co3 + (c4 & 0x00FF00FF)*co4)>>8)&0x00FF00FF)
		   + ((((c1 & 0x0000FF00)*co1 + (c2 & 0x0000FF00)*co2 + (c3 & 0x0000FF00)*co3 + (c4 & 0x0000FF00)*co4)&0x00FF0000)>>8);
}

#define RED(x) ((signed long)((x)>>16)&255)
#define GRN(x) ((signed long)((x)>> 8)&255)
#define BLU(x) ((signed long)(x)&255)

static inline uint32 cc(const uint32 *yptr, const int *tbl) {
	const uint32 y1 = yptr[0];
	const uint32 y2 = yptr[1];
	const uint32 y3 = yptr[2];
	const uint32 y4 = yptr[3];
	long red, grn, blu;

	red = RED(y1)*tbl[0] + RED(y2)*tbl[1] + RED(y3)*tbl[2] + RED(y4)*tbl[3];
	grn = GRN(y1)*tbl[0] + GRN(y2)*tbl[1] + GRN(y3)*tbl[2] + GRN(y4)*tbl[3];
	blu = BLU(y1)*tbl[0] + BLU(y2)*tbl[1] + BLU(y3)*tbl[2] + BLU(y4)*tbl[3];

	if (red<0) red=0; else if (red>4194303) red=4194303;
	if (grn<0) grn=0; else if (grn>4194303) grn=4194303;
	if (blu<0) blu=0; else if (blu>4194303) blu=4194303;

	return ((red<<2) & 0xFF0000) | ((grn>>6) & 0x00FF00) | (blu>>14);
}

#undef RED
#undef GRN
#undef BLU

#ifdef _M_IX86
static uint32 __declspec(naked) cc_MMX(const uint32 *src, const int *table) {

	static const sint64 x0000200000002000 = 0x0000200000002000i64;

	//	[esp + 4]	src
	//	[esp + 8]	table

	_asm {
		mov			ecx,[esp+4]
		mov			eax,[esp+8]

		movd		mm0,[ecx]
		pxor		mm7,mm7

		movd		mm1,[ecx+4]
		punpcklbw	mm0,mm7				;mm0 = [a1][r1][g1][b1]

		movd		mm2,[ecx+8]
		punpcklbw	mm1,mm7				;mm1 = [a2][r2][g2][b2]

		movd		mm3,[ecx+12]
		punpcklbw	mm2,mm7				;mm2 = [a3][r3][g3][b3]

		punpcklbw	mm3,mm7				;mm3 = [a4][r4][g4][b4]
		movq		mm4,mm0				;mm0 = [a1][r1][g1][b1]

		punpcklwd	mm0,mm1				;mm0 = [g2][g1][b2][b1]
		movq		mm5,mm2				;mm2 = [a3][r3][g3][b3]

		pmaddwd		mm0,[eax]
		punpcklwd	mm2,mm3				;mm2 = [g4][g3][b4][b3]

		pmaddwd		mm2,[eax+8]
		punpckhwd	mm4,mm1				;mm4 = [a2][a1][r2][r1]

		pmaddwd		mm4,[eax]
		punpckhwd	mm5,mm3				;mm5 = [a4][a3][b4][b3]

		pmaddwd		mm5,[eax+8]
		paddd		mm0,mm2				;mm0 = [ g ][ b ]

		paddd		mm0,x0000200000002000
		;idle V

		paddd		mm4,x0000200000002000
		psrad		mm0,14

		paddd		mm4,mm5				;mm4 = [ a ][ r ]

		psrad		mm4,14

		packssdw	mm0,mm4				;mm0 = [ a ][ r ][ g ][  b ]
		packuswb	mm0,mm0				;mm0 = [a][r][g][b][a][r][g][b]

		movd		eax,mm0
		ret
	}
}

static inline uint32 bicubic_interp_MMX(const uint32 *src, ptrdiff_t pitch, unsigned long cox, unsigned long coy, const int *table) {
	uint32 x[4];

	cox >>= 24;
	coy >>= 24;

	src = (uint32 *)((char *)src - pitch - 4);

	x[0] = cc_MMX(src, table+cox*4); src = (uint32 *)((char *)src + pitch);
	x[1] = cc_MMX(src, table+cox*4); src = (uint32 *)((char *)src + pitch);
	x[2] = cc_MMX(src, table+cox*4); src = (uint32 *)((char *)src + pitch);
	x[3] = cc_MMX(src, table+cox*4);

	return cc_MMX(x, table + coy*4);
}
#endif

static inline uint32 bicubic_interp(const uint32 *src, ptrdiff_t pitch, unsigned long cox, unsigned long coy, const int *table) {
	uint32 x[4];

	cox >>= 24;
	coy >>= 24;

	src = (uint32 *)((char *)src - pitch - 4);

	x[0] = cc(src, table+cox*4); src = (uint32 *)((char *)src + pitch);
	x[1] = cc(src, table+cox*4); src = (uint32 *)((char *)src + pitch);
	x[2] = cc(src, table+cox*4); src = (uint32 *)((char *)src + pitch);
	x[3] = cc(src, table+cox*4);

	return cc(x, table + coy*4);
}

static uint32 ColorRefToPixel32(COLORREF rgb) {
	return (uint32)(((rgb>>16)&0xff) | ((rgb<<16)&0xff0000) | (rgb&0xff00));
}

static int rotate2_run(const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	const VDRotate2FilterData *mfd = (VDRotate2FilterData *)fa->filter_data;
	sint64 xaccum, yaccum;
	uint32 *src, *dst, pixFill;
	sint32 w, h;
	const RotateRow *rr = mfd->rows;
	const sint64 du = mfd->u_step;
	const sint64 dv = mfd->v_step;

	unsigned long Ustep, Vstep;
	ptrdiff_t UVintstepV, UVintstepnoV;

	// initialize Abrash texmapping variables :)

	Ustep = (unsigned long)du;
	Vstep = (unsigned long)dv;

	UVintstepnoV = (long)(du>>32) + (long)(dv>>32)*(fa->src.pitch>>2);
	UVintstepV = UVintstepnoV + (fa->src.pitch>>2);

	dst = fa->dst.data;
	pixFill = ColorRefToPixel32(mfd->rgbColor);

	h = fa->dst.h;
	do {
		// texmap!

		w = rr->leftzero;
		if (w) do {
			*dst++ = pixFill;
		} while(--w);

		xaccum = rr->xaccum_left;
		yaccum = rr->yaccum_left;

		switch(mfd->filtmode) {
		case FILTMODE_POINT:
			w = fa->dst.w - (rr->leftzero + rr->left + rr->right + rr->rightzero);
			if (w) {

#ifdef USE_ASM
				asm_rotate_point(
					(uint32*)((char *)fa->src.data + (int)(xaccum>>32)*4 + (int)(yaccum>>32)*fa->src.pitch),
					dst,
					w,
					(unsigned long)xaccum,
					(unsigned long)yaccum,
					UVintstepV,
					UVintstepnoV,
					Ustep,
					Vstep);

				dst += w;

#else
				do {
					*dst++ = *(uint32*)((char *)fa->src.data + (int)(xaccum>>32)*4 + (int)(yaccum>>32)*fa->src.pitch);

					xaccum += du;
					yaccum += dv;
				} while(--w);
#endif
				break;
			}

		case FILTMODE_BILINEAR:
			w = rr->left;
			if (w) {
				do {
					uint32 *src = (uint32*)((char *)fa->src.data + (int)(xaccum>>32)*4 + (int)(yaccum>>32)*fa->src.pitch);
					uint32 c1, c2, c3, c4;

					int px = (int)(xaccum >> 32);
					int py = (int)(yaccum >> 32);

					if (px<0 || py<0 || px>=fa->src.w || py>=fa->src.h)
						c1 = pixFill;
					else
						c1 = *(uint32 *)((char *)src + 0);

					++px;

					if (px<0 || py<0 || px>=fa->src.w || py>=fa->src.h)
						c2 = pixFill;
					else
						c2 = *(uint32 *)((char *)src + 4);

					++py;

					if (px<0 || py<0 || px>=fa->src.w || py>=fa->src.h)
						c4 = pixFill;
					else
						c4 = *(uint32 *)((char *)src + 4 + fa->src.pitch);

					--px;

					if (px<0 || py<0 || px>=fa->src.w || py>=fa->src.h)
						c3 = pixFill;
					else
						c3 = *(uint32 *)((char *)src + 0 + fa->src.pitch);

					*dst++ = bilinear_interp(c1, c2, c3, c4, (unsigned long)xaccum >> 24, (unsigned long)yaccum >> 24);

					xaccum += du;
					yaccum += dv;
				} while(--w);
			}

			w = fa->dst.w - (rr->leftzero + rr->left + rr->right + rr->rightzero);
			if (w) {
#ifdef USE_ASM
				asm_rotate_bilinear(
						(uint32*)((char *)fa->src.data + (int)(xaccum>>32)*4 + (int)(yaccum>>32)*fa->src.pitch),
						dst,
						w,
						fa->src.pitch,
						(unsigned long)xaccum,
						(unsigned long)yaccum,
						UVintstepV,
						UVintstepnoV,
						Ustep,
						Vstep);

				xaccum += du*w;
				yaccum += dv*w;
				dst += w;
#else
				do {
					uint32 *src = (uint32*)((char *)fa->src.data + (int)(xaccum>>32)*4 + (int)(yaccum>>32)*fa->src.pitch);
					uint32 c1, c2, c3, c4, cY;
					int co1, co2, co3, co4, cox, coy;

					c1 = *(uint32 *)((char *)src + 0);
					c2 = *(uint32 *)((char *)src + 4);
					c3 = *(uint32 *)((char *)src + 0 + fa->src.pitch);
					c4 = *(uint32 *)((char *)src + 4 + fa->src.pitch);

					cox = ((unsigned long)xaccum >> 24);
					coy = ((unsigned long)yaccum >> 24);

					co4 = (cox * coy) >> 8;
					co3 = coy - co4;
					co2 = cox - co4;
					co1 = 0x100 - coy - co2;

					cY = ((((c1 & 0x00FF00FF)*co1 + (c2 & 0x00FF00FF)*co2 + (c3 & 0x00FF00FF)*co3 + (c4 & 0x00FF00FF)*co4)>>8)&0x00FF00FF)
					   + ((((c1 & 0x0000FF00)*co1 + (c2 & 0x0000FF00)*co2 + (c3 & 0x0000FF00)*co3 + (c4 & 0x0000FF00)*co4)&0x00FF0000)>>8);

					*dst++ = cY;

					xaccum += du;
					yaccum += dv;
				} while(--w);
#endif
			}

			w = rr->right;
			if (w) {
				do {
					uint32 *src = (uint32*)((char *)fa->src.data + (int)(xaccum>>32)*4 + (int)(yaccum>>32)*fa->src.pitch);
					uint32 c1, c2, c3, c4;

					int px = (int)(xaccum >> 32);
					int py = (int)(yaccum >> 32);

					if (px<0 || py<0 || px>=fa->src.w || py>=fa->src.h)
						c1 = pixFill;
					else
						c1 = *(uint32 *)((char *)src + 0);

					++px;

					if (px<0 || py<0 || px>=fa->src.w || py>=fa->src.h)
						c2 = pixFill;
					else
						c2 = *(uint32 *)((char *)src + 4);

					++py;

					if (px<0 || py<0 || px>=fa->src.w || py>=fa->src.h)
						c4 = pixFill;
					else
						c4 = *(uint32 *)((char *)src + 4 + fa->src.pitch);

					--px;

					if (px<0 || py<0 || px>=fa->src.w || py>=fa->src.h)
						c3 = pixFill;
					else
						c3 = *(uint32 *)((char *)src + 0 + fa->src.pitch);

					*dst++ = bilinear_interp(c1, c2, c3, c4, (unsigned long)xaccum >> 24, (unsigned long)yaccum >> 24);

					xaccum += du;
					yaccum += dv;
				} while(--w);
			}
			break;

		case FILTMODE_BICUBIC:
			w = rr->left;
			if (w) {
				do {
					uint32 *src = (uint32*)((char *)fa->src.data + (int)(xaccum>>32)*4 + (int)(yaccum>>32)*fa->src.pitch);
					uint32 c1, c2, c3, c4;

					int px = (int)(xaccum >> 32);
					int py = (int)(yaccum >> 32);

					if (px<0 || py<0 || px>=fa->src.w || py>=fa->src.h)
						c1 = pixFill;
					else
						c1 = *(uint32 *)((char *)src + 0);

					++px;

					if (px<0 || py<0 || px>=fa->src.w || py>=fa->src.h)
						c2 = pixFill;
					else
						c2 = *(uint32 *)((char *)src + 4);

					++py;

					if (px<0 || py<0 || px>=fa->src.w || py>=fa->src.h)
						c4 = pixFill;
					else
						c4 = *(uint32 *)((char *)src + 4 + fa->src.pitch);

					--px;

					if (px<0 || py<0 || px>=fa->src.w || py>=fa->src.h)
						c3 = pixFill;
					else
						c3 = *(uint32 *)((char *)src + 0 + fa->src.pitch);

					*dst++ = bilinear_interp(c1, c2, c3, c4, (unsigned long)xaccum >> 24, (unsigned long)yaccum >> 24);

					xaccum += du;
					yaccum += dv;
				} while(--w);
			}

			w = fa->dst.w - (rr->leftzero + rr->left + rr->right + rr->rightzero);
			if (w) {
				sint64 xa = xaccum;
				sint64 ya = yaccum;

				src = (uint32*)((char *)fa->src.data + (int)(xa>>32)*4 + (int)(ya>>32)*fa->src.pitch);

				xaccum += du * w;
				yaccum += dv * w;

#ifdef _M_IX86
				if (MMX_enabled)
					do {
						*dst++ = bicubic_interp_MMX(src, fa->src.pitch, (unsigned long)xa, (unsigned long)ya, mfd->coeff_tbl);

						xa = (sint64)(unsigned long)xa + Ustep;
						ya = (sint64)(unsigned long)ya + Vstep;

						src += (xa>>32) + (ya>>32 ? UVintstepV : UVintstepnoV);

						xa = (unsigned long)xa;
						ya = (unsigned long)ya;
					} while(--w);
				else
#endif
					do {
						*dst++ = bicubic_interp(src, fa->src.pitch, (unsigned long)xa, (unsigned long)ya, mfd->coeff_tbl);

						xa = (sint64)(unsigned long)xa + Ustep;
						ya = (sint64)(unsigned long)ya + Vstep;

						src += (xa>>32) + (ya>>32 ? UVintstepV : UVintstepnoV);

						xa = (unsigned long)xa;
						ya = (unsigned long)ya;
					} while(--w);

			}

			w = rr->right;
			if (w) {
				do {
					uint32 *src = (uint32*)((char *)fa->src.data + (int)(xaccum>>32)*4 + (int)(yaccum>>32)*fa->src.pitch);
					uint32 c1, c2, c3, c4;

					int px = (int)(xaccum >> 32);
					int py = (int)(yaccum >> 32);

					if (px<0 || py<0 || px>=fa->src.w || py>=fa->src.h)
						c1 = pixFill;
					else
						c1 = *(uint32 *)((char *)src + 0);

					++px;

					if (px<0 || py<0 || px>=fa->src.w || py>=fa->src.h)
						c2 = pixFill;
					else
						c2 = *(uint32 *)((char *)src + 4);

					++py;

					if (px<0 || py<0 || px>=fa->src.w || py>=fa->src.h)
						c4 = pixFill;
					else
						c4 = *(uint32 *)((char *)src + 4 + fa->src.pitch);

					--px;

					if (px<0 || py<0 || px>=fa->src.w || py>=fa->src.h)
						c3 = pixFill;
					else
						c3 = *(uint32 *)((char *)src + 0 + fa->src.pitch);

					*dst++ = bilinear_interp(c1, c2, c3, c4, (unsigned long)xaccum >> 24, (unsigned long)yaccum >> 24);

					xaccum += du;
					yaccum += dv;
				} while(--w);
			}
			break;
		}

		w = rr->rightzero;
		if (w) do {
			*dst++ = pixFill;
		} while(--w);

		dst = (uint32 *)((char *)dst + fa->dst.modulo);

		++rr;

	} while(--h);

#ifndef _M_AMD64
	if (MMX_enabled)
		__asm emms
#endif

	return 0;
}

static long rotate2_param(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	VDRotate2FilterData *mfd = (VDRotate2FilterData *)fa->filter_data;
	int destw, desth;

	if (mfd->fExpandBounds) {
		double ang = mfd->angle * (3.14159265358979323846 / 2147483648.0);
		double xcos = cos(ang);
		double xsin = sin(ang);
		int destw1, destw2, desth1, desth2;

		// Because the rectangle is symmetric, we only
		// need to rotate two corners and pick the farthest.

		destw1 = VDRoundToInt(fabs(pxldst.w * xcos - pxldst.h * xsin));
		desth1 = VDRoundToInt(fabs(pxldst.h * xcos + pxldst.w * xsin));
		destw2 = VDRoundToInt(fabs(pxldst.w * xcos + pxldst.h * xsin));
		desth2 = VDRoundToInt(fabs(pxldst.h * xcos - pxldst.w * xsin));

		destw = std::max<int>(destw1, destw2);
		desth = std::max<int>(desth1, desth2);
	} else {
		destw = pxldst.w;
		desth = pxldst.h;
	}

	pxldst.w = destw;
	pxldst.h = desth;

	return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

static INT_PTR CALLBACK rotate2DlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	static const char * const szModes[]={
		"Point sampling",
		"Bilinear - 2x2",
		"Bicubic - 4x4",
	};

	VDRotate2FilterData *mfd = (struct VDRotate2FilterData *)GetWindowLongPtr(hDlg, DWLP_USER);
	HWND hwndItem;

    switch (message)
    {
        case WM_INITDIALOG:
			{
				char buf[32];
				int i;

				mfd = (struct VDRotate2FilterData *)lParam;
				SetWindowLongPtr(hDlg, DWLP_USER, lParam);

				sprintf(buf, "%.3f", (double)mfd->angle * (360.0 / 4294967296.0));
				SetDlgItemText(hDlg, IDC_ANGLE, buf);

				hwndItem = GetDlgItem(hDlg, IDC_FILTERMODE);

				for(i=0; i<sizeof szModes/sizeof szModes[0]; i++)
					SendMessage(hwndItem, CB_ADDSTRING, 0, (LPARAM)szModes[i]);

				SendMessage(hwndItem, CB_SETCURSEL, mfd->filtmode, 0);

				CheckDlgButton(hDlg, IDC_EXPANDBOUNDS, mfd->fExpandBounds);

				mfd->hbrColor = CreateSolidBrush(mfd->rgbColor);

				mfd->ifp->InitButton((VDXHWND)GetDlgItem(hDlg, IDC_PREVIEW));

			}
            return (TRUE);

        case WM_COMMAND:     
			switch(LOWORD(wParam)) {
            case IDOK:
				mfd->ifp->Close();
				EndDialog(hDlg, 0);
				return TRUE;

			case IDCANCEL:
				mfd->ifp->Close();
                EndDialog(hDlg, 1);
                return TRUE;

			case IDC_PREVIEW:
				mfd->ifp->Toggle((VDXHWND)hDlg);
				return TRUE;

			case IDC_ANGLE:
				if (HIWORD(wParam) == EN_KILLFOCUS) {
					char buf[32];
					double ang;

					if (!GetDlgItemText(hDlg, IDC_ANGLE, buf, sizeof buf)
						|| 1!=sscanf(buf, "%lf", &ang)) {

						MessageBeep(MB_ICONEXCLAMATION);
						SetFocus(GetDlgItem(hDlg, IDC_ANGLE));
						return TRUE;
					}

					ang *= (1.0 / 360.0);
					ang -= floor(ang);

					mfd->ifp->UndoSystem();
					mfd->angle = VDRoundToInt((ang - 0.5) * 4294967296.0) ^ 0x80000000;
					mfd->ifp->RedoSystem();
				}
				return TRUE;

			case IDC_FILTERMODE:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					mfd->ifp->UndoSystem();
					mfd->filtmode = SendDlgItemMessage(hDlg, IDC_FILTERMODE, CB_GETCURSEL, 0, 0);
					mfd->ifp->RedoSystem();
				}
				return TRUE;

			case IDC_EXPANDBOUNDS:
				mfd->ifp->UndoSystem();
				mfd->fExpandBounds = !!IsDlgButtonChecked(hDlg, IDC_EXPANDBOUNDS);
				mfd->ifp->RedoSystem();
				return TRUE;


			case IDC_PICKCOLOR:
				mfd->ifp->UndoSystem();
				if (guiChooseColor(hDlg, mfd->rgbColor)) {
					DeleteObject(mfd->hbrColor);
					mfd->hbrColor = CreateSolidBrush(mfd->rgbColor);
					RedrawWindow(GetDlgItem(hDlg, IDC_COLOR), NULL, NULL, RDW_ERASE|RDW_INVALIDATE|RDW_UPDATENOW);
				}
				mfd->ifp->RedoSystem();
				break;
            }
            break;

		case WM_CTLCOLORSTATIC:
			if (GetWindowLong((HWND)lParam, GWL_ID) == IDC_COLOR)
				return (BOOL)mfd->hbrColor;
			break;
    }
    return FALSE;
}

static int rotate2_config(VDXFilterActivation *fa, const VDXFilterFunctions *ff, VDXHWND hWnd) {
	VDRotate2FilterData *mfd = (VDRotate2FilterData *)fa->filter_data;
	VDRotate2FilterData mfd2 = *mfd;
	int ret;

	mfd->hbrColor = NULL;
	mfd->ifp = fa->ifp;

	ret = DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_ROTATE2), (HWND)hWnd, rotate2DlgProc, (LPARAM)mfd);

	if (mfd->hbrColor)
		DeleteObject(mfd->hbrColor);

	if (ret)
		*mfd = mfd2;

	return ret;
}

///////////////////////////////////////////

static int rotate2_start(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	VDRotate2FilterData *mfd = (VDRotate2FilterData *)fa->filter_data;
	const VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	// Compute step parameters.

	double ang = mfd->angle * (3.14159265358979323846 / 2147483648.0);
	double ustep = cos(ang);
	double vstep = -sin(ang);
	sint64 du, dv;

	mfd->u_step = du = (sint64)floor(ustep*4294967296.0 + 0.5);
	mfd->v_step = dv = (sint64)floor(vstep*4294967296.0 + 0.5);

	if (!(mfd->rows = (RotateRow *)calloc(sizeof(RotateRow), pxldst.h)))
		return 1;

	// It's time for Mr.Bonehead!!

	int x0, x1, x2, x3, y;
	sint64 xaccum, yaccum;
	sint64 xaccum_low, yaccum_low, xaccum_high, yaccum_high, xaccum_base, yaccum_base;
	sint64 xaccum_low2, yaccum_low2, xaccum_high2, yaccum_high2;
	RotateRow *rr = mfd->rows;

	// Compute allowable source bounds.

	xaccum_base = xaccum_low = -0x80000000i64*fa->src.w;
	yaccum_base = yaccum_low = -0x80000000i64*fa->src.h;
	xaccum_high = 0x80000000i64*fa->src.w;
	yaccum_high = 0x80000000i64*fa->src.h;

	// Compute accumulators for top-left destination position.

	xaccum = ( dv*(pxldst.h-1) - du*(pxldst.w-1))/2;
	yaccum = (-du*(pxldst.h-1) - dv*(pxldst.w-1))/2;

	// Compute 'marginal' bounds that require partial clipping.

	switch(mfd->filtmode) {
	case FILTMODE_POINT:
		xaccum_low2 = xaccum_low;
		yaccum_low2 = yaccum_low;
		xaccum_high2 = xaccum_high;
		yaccum_high2 = yaccum_high;
		break;
	case FILTMODE_BILINEAR:
		xaccum_low2 = xaccum_low;
		yaccum_low2 = yaccum_low;
		xaccum_high2 = xaccum_high - 0x100000000i64;
		yaccum_high2 = yaccum_high - 0x100000000i64;
		xaccum_low -= 0x100000000i64;
		yaccum_low -= 0x100000000i64;

		xaccum -= 0x80000000i64;
		yaccum -= 0x80000000i64;
		break;

	case FILTMODE_BICUBIC:
		xaccum_low2  = xaccum_low  + 0x100000000i64;
		yaccum_low2  = yaccum_low  + 0x100000000i64;
		xaccum_high2 = xaccum_high - 0x200000000i64;
		yaccum_high2 = yaccum_high - 0x200000000i64;
		xaccum_low -= 0x100000000i64;
		yaccum_low -= 0x100000000i64;

		xaccum -= 0x80000000i64;
		yaccum -= 0x80000000i64;
		break;
	}

	for(y=0; y<pxldst.h; y++) {
		sint64 xa, ya;

		xa = xaccum;
		ya = yaccum;

		for(x0=0; x0<pxldst.w; x0++) {
			if (xa >= xaccum_low && ya >= yaccum_low && xa < xaccum_high && ya < yaccum_high)
				break;

			xa += du;
			ya += dv;
		}

		rr->xaccum_left = xa - xaccum_base;
		rr->yaccum_left = ya - yaccum_base;

		for(x1=x0; x1<pxldst.w; x1++) {
			if (xa >= xaccum_low2 && ya >= yaccum_low2 && xa < xaccum_high2 && ya < yaccum_high2)
				break;

			xa += du;
			ya += dv;
		}

		for(x2=x1; x2<pxldst.w; x2++) {
			if (xa < xaccum_low2 || ya < yaccum_low2 || xa >= xaccum_high2 || ya >= yaccum_high2)
				break;

			xa += du;
			ya += dv;
		}

		for(x3=x2; x3<pxldst.w; x3++) {
			if (xa < xaccum_low || ya < yaccum_low || xa >= xaccum_high || ya >= yaccum_high)
				break;

			xa += du;
			ya += dv;
		}

		rr->leftzero = x0;
		rr->left = x1-x0;
		rr->right = x3-x2;
		rr->rightzero = pxldst.w - x3;
		++rr;

		xaccum -= dv;
		yaccum += du;
	}

	// Fill out cubic interpolation coeff. table

	if (mfd->filtmode == FILTMODE_BICUBIC) {
		if (!(mfd->coeff_tbl = (int *)malloc(sizeof(int)*256*4)))
			return 1;

#ifdef USE_ASM
		MakeCubic4Table(mfd->coeff_tbl, -0.75, !!MMX_enabled);
#else
		MakeCubic4Table(mfd->coeff_tbl, -0.75, false);
#endif
		
	}

	return 0;
}

static int rotate2_end(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	VDRotate2FilterData *mfd = (VDRotate2FilterData *)fa->filter_data;

	free(mfd->rows); mfd->rows = NULL;
	free(mfd->coeff_tbl); mfd->coeff_tbl = NULL;

	return 0;
}

static void rotate2_string2(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int maxlen) {
	VDRotate2FilterData *mfd = (VDRotate2FilterData *)fa->filter_data;

	VDStringA degree = VDTextWToA(L"\u00B0");

	_snprintf(buf, maxlen, " (%.3f%s, %s, #%06X%s)",
			mfd->angle * (360.0 / 4294967296.0),
			degree.c_str(),
			szModeStrings[mfd->filtmode],
			ColorRefToPixel32(mfd->rgbColor),
			mfd->fExpandBounds ? ", expand" : "");
	buf[maxlen - 1] = 0;
}

static void rotate2_script_config(IVDXScriptInterpreter *isi, void *lpVoid, VDXScriptValue *argv, int argc) {
	VDXFilterActivation *fa = (VDXFilterActivation *)lpVoid;
	VDRotate2FilterData *mfd = (VDRotate2FilterData *)fa->filter_data;

	mfd->angle		= argv[0].asInt()<<8;
	mfd->filtmode	= argv[1].asInt();
	mfd->rgbColor	= ColorRefToPixel32(argv[2].asInt());
	mfd->fExpandBounds = !!argv[3].asInt();

	if (mfd->filtmode < 0)
		mfd->filtmode = 0;
	else if (mfd->filtmode >= FILTMODE_COUNT)
		mfd->filtmode = FILTMODE_COUNT-1;
}

static VDXScriptFunctionDef rotate2_func_defs[]={
	{ (VDXScriptFunctionPtr)rotate2_script_config, "Config", "0iiii" },
	{ NULL },
};

static VDXScriptObject rotate2_obj={
	NULL, rotate2_func_defs
};

static bool rotate2_script_line(VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int buflen) {
	VDRotate2FilterData *mfd = (VDRotate2FilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d, %d, 0x%06X, %d)", (mfd->angle+0x80)>>8, mfd->filtmode,
		ColorRefToPixel32(mfd->rgbColor), mfd->fExpandBounds?1:0);

	return true;
}

extern const VDXFilterDefinition g_VDVFRotate2 = {
	0,0,NULL,
	"rotate2",
	"Rotates an image by an arbitrary angle."
#ifdef USE_ASM
			"\n\n[Assembly optimized] [MMX optimized]"
#endif
		,
	NULL,NULL,
	sizeof(VDRotate2FilterData),
	NULL,NULL,
	rotate2_run,
	rotate2_param,
	rotate2_config,
	NULL,
	rotate2_start,
	rotate2_end,

	&rotate2_obj,
	rotate2_script_line,
	rotate2_string2,
};
