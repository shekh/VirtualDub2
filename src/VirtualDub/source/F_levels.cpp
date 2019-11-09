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

#include <windows.h>
#include <commctrl.h>

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"
#include "LevelControl.h"

#include "resource.h"
#include "filter.h"
#include "gui.h"
#include "VBitmap.h"
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/system/cpuaccel.h>

/////////////////////////////////////////////////////////////////////

extern HINSTANCE g_hInst;

struct LevelsFilterData {
	unsigned char xtblmono[256];
	unsigned char xtblmono2[256];
	int xtblluma[256];
	const uint8 *xtblluma2[256];
	
	uint8		cliptab[768];

	int			iInputLo, iInputHi;
	int			iOutputLo, iOutputHi;
	double		rHalfPt, rGammaCorr;

	IFilterPreview *ifp;
	RECT		rHisto;
	uint32		*mpHisto;
	sint32		mHistoMax;
	bool		fInhibitUpdate;
	bool		bLuma;
	bool		bFullRange;
};

/////////////////////////////////////////////////////////////////////

static int levels_init(FilterActivation *fa, const FilterFunctions *ff) {
	LevelsFilterData *mfd = (LevelsFilterData *)fa->filter_data;

	mfd->iInputLo = 0x0000;
	mfd->rHalfPt = 0.5;
	mfd->rGammaCorr = 1.0;
	mfd->iInputHi = 0xFFFF;
	mfd->iOutputHi = 0xFFFF;
	mfd->bLuma = true;

	return 0;
}

/////////////////////////////////////////////////////////////////////

static int bright_table_R[256];
static int bright_table_G[256];
static int bright_table_B[256];
extern "C" unsigned char YUV_clip_table[];

#ifdef _M_IX86
static void __declspec(naked) AsmLevelsRunScalar(uint32 *dst, ptrdiff_t dstpitch, sint32 w, sint32 h, const int *xtblptr) {
	__asm {
			push	ebp
			push	edi
			push	esi
			push	ebx
			mov		edi,[esp+4+16]
			mov		ebp,[esp+12+16]
			shl		ebp,2
			sub		dword ptr [esp+8+16],ebp
yloop:
			mov		ebp,[esp+12+16]
xloop:
			mov		eax,[edi]					;load source pixel
			xor		ebx,ebx

			mov		bl,al
			xor		ecx,ecx

			mov		cl,ah
			and		eax,00ff0000h

			shr		eax,16
			mov		edx,[bright_table_R+ebx*4]

			mov		esi,[bright_table_G+ecx*4]
			add		edx,00008000h

			add		edx,esi
			mov		esi,[bright_table_B+eax*4]

			add		edx,esi						;edx = bright*65536
			mov		esi,[esp+20+16]					;load brightness translation table

			shr		edx,16						;edx = brightness
			add		edi,4

			mov		edx,[esi+edx*4]				;get brightness delta [AGI]

			mov		al,[YUV_clip_table+eax+edx+256]	;[AGI]
			mov		bl,[YUV_clip_table+ebx+edx+256]

			mov		cl,[YUV_clip_table+ecx+edx+256]
			mov		[edi-2],al

			mov		[edi-4],bl
			dec		ebp

			mov		[edi-3],cl
			jne		xloop

			add		edi,[esp+8+16]

			dec		dword ptr [esp+16+16]
			jne		yloop

			pop		ebx
			pop		esi
			pop		edi
			pop		ebp
			ret
	}
}

static void __declspec(naked) AsmLevelsRunMMX(uint32 *dst, ptrdiff_t dstpitch, sint32 w, sint32 h, const int *xtblptr) {
	static const __int64 bright_coeff=0x000026464b220e98i64;
	static const __int64 round = 0x0000000000004000i64;
	__asm {
			push	ebp
			push	edi
			push	esi
			push	ebx
			mov		edi,[esp+4+16]
			mov		ebp,[esp+12+16]
			and		ebp,0fffffffeh
			shl		ebp,2
			sub		dword ptr [esp+8+16],ebp
			mov		esi,[esp+20+16]

			movq		mm6,bright_coeff
yloop:
			mov			ebp,[esp+12+16]
			dec			ebp
			jz			do_single
xloop:
			movd		mm2,[edi]
			pxor		mm7,mm7

			movd		mm3,[edi+4]
			movq		mm0,mm6

			movq		mm1,round
			punpcklbw	mm2,mm7

			pmaddwd		mm0,mm2
			punpcklbw	mm3,mm7

			movq		mm4,mm3
			pmaddwd		mm3,mm6

			movq		mm5,mm1
			;

			paddd		mm1,mm0
			psrlq		mm0,32

			paddd		mm5,mm3
			psrlq		mm3,32

			paddd		mm0,mm1
			paddd		mm3,mm5

			psrld		mm0,15

			psrld		mm3,15

			movd		eax,mm0

			movd		ebx,mm3

			movd		mm1,[esi+eax*4]

			movd		mm5,[esi+ebx*4]
			punpckldq	mm1,mm1

			paddw		mm2,mm1
			punpckldq	mm5,mm5

			paddw		mm4,mm5
			add			edi,8

			packuswb	mm2,mm4
			sub			ebp,2

			;
			;

			movq		[edi-8],mm2
			ja			xloop
			jnz			no_single

			;----------

do_single:
			movd		mm2,[edi]
			movq		mm0,mm6
			movq		mm1,round
			punpcklbw	mm2,mm7
			pmaddwd		mm0,mm2
			paddd		mm1,mm0
			psrlq		mm0,32
			paddd		mm0,mm1
			psrld		mm0,15
			movd		eax,mm0
			movd		mm1,[esi+eax*4]
			punpckldq	mm1,mm1
			paddw		mm2,mm1
			packuswb	mm2,mm2
			movd		[edi],mm2
no_single:

			;----------

			add		edi,[esp+8+16]

			dec		dword ptr [esp+16+16]
			jne		yloop

			pop		ebx
			pop		esi
			pop		edi
			pop		ebp
			emms
			ret
	}
}

#endif

#ifdef _M_IX86
static void translate_rgba_luma(uint8 * VDRESTRICT dst, ptrdiff_t pitch, uint32 w, uint32 h, const LevelsFilterData *mfd) {
	if (CPUGetEnabledExtensions() & CPUF_SUPPORTS_MMX)
		AsmLevelsRunMMX((uint32 *)dst, pitch, w, h, mfd->xtblluma);
	else
		AsmLevelsRunScalar((uint32 *)dst, pitch, w, h, mfd->xtblluma);
}
#else
static void translate_rgba_luma(uint8 * VDRESTRICT dst, ptrdiff_t pitch, uint32 w0, uint32 h, const LevelsFilterData *mfd) {
	uint8 *p = dst;
	ptrdiff_t modulo = pitch - 4*w0;

	do {
		uint32 w = w0;
		do {
			uint32 r = p[2];
			uint32 g = p[1];
			uint32 b = p[0];
			const uint8 *yp = mfd->xtblluma2[(bright_table_R[r] + bright_table_G[g] + bright_table_B[b] + 0x8000) >> 16];

			p[0] = yp[b];
			p[1] = yp[g];
			p[2] = yp[r];
			p += 4;
		} while(--w);

		p += modulo;
	} while(--h);
}
#endif

static void translate_rgba(uint8 * VDRESTRICT dst, ptrdiff_t pitch, uint32 w, uint32 h, const uint8 * VDRESTRICT tbl) {
	uint8 *p = dst;
	ptrdiff_t modulo = pitch - 4*w;
	for(uint32 y=0; y<h; ++y) {
		for(uint32 x=0; x<w; ++x){
			p[0] = tbl[p[0]];
			p[1] = tbl[p[1]];
			p[2] = tbl[p[2]];
			p += 4;
		}

		p += modulo;
	}
}

static void translate_plane(uint8 * VDRESTRICT dst, ptrdiff_t pitch, uint32 w, uint32 h, const uint8 * VDRESTRICT tbl) {
	for(uint32 y=0; y<h; ++y) {
		for(uint32 x=0; x<w; ++x)
			dst[x] = tbl[dst[x]];

		dst += pitch;
	}
}

static void translate_rgba_luma_32F(const VDXPixmap& px, const LevelsFilterData *mfd) {
	const long	y_base	= mfd->iOutputLo;
	const long	y_range	= mfd->iOutputHi - mfd->iOutputLo;
	const double	x_lo	= mfd->iInputLo / (double)0xffff;
	const double	x_hi	= mfd->iInputHi / (double)0xffff;
	double gammapower = 1.0 / mfd->rGammaCorr;
	float vrange = y_range / (double)0xffff;
	float vmin = y_base / (double)0xffff;
	float vmax = (y_base + y_range) / (double)0xffff;
	float vavg = (y_base + y_range * 0.5) / (double)0xffff;

	uint32 w = px.w;
	uint32 h = px.h;

	float* dr = (float*)px.data;
	float* dg = (float*)px.data2;
	float* db = (float*)px.data3;

	for(uint32 y=0; y<h; ++y) {
		if (x_lo == x_hi) for(uint32 x=0; x<w; ++x) {
			dr[x] = vavg;
			dg[x] = vavg;
			db[x] = vavg;
		} else for(uint32 x=0; x<w; ++x) {
			float vx = dr[x]*0.299 + dg[x]*0.587 + db[x]*0.114;
			float vy;

			if (vx < x_lo)
				vy = vmin;
			else if (vx > x_hi)
				vy = vmax;
			else {
				vy = pow((vx - x_lo) / (x_hi - x_lo), gammapower);
				vy = vmin + vrange * vy;
			}

			float r = vy-vx+dr[x];
			float g = vy-vx+dg[x];
			float b = vy-vx+db[x];
			if(r<0) r=0; if(r>1) r=1;
			if(g<0) g=0; if(g>1) g=1;
			if(b<0) b=0; if(b>1) b=1;

			dr[x] = r;
			dg[x] = g;
			db[x] = b;
		}

		dr += px.pitch/4;
		dg += px.pitch2/4;
		db += px.pitch3/4;
	}
}

static void translate_plane_32F(float * VDRESTRICT dst, ptrdiff_t pitch, uint32 w, uint32 h, const LevelsFilterData *mfd) {
	const long	y_base	= mfd->iOutputLo;
	const long	y_range	= mfd->iOutputHi - mfd->iOutputLo;
	const double	x_lo	= mfd->iInputLo / (double)0xffff;
	const double	x_hi	= mfd->iInputHi / (double)0xffff;
	double gammapower = 1.0 / mfd->rGammaCorr;
	float vrange = y_range / (double)0xffff;
	float vmin = y_base / (double)0xffff;
	float vmax = (y_base + y_range) / (double)0xffff;
	float vavg = (y_base + y_range * 0.5) / (double)0xffff;

	for(uint32 y=0; y<h; ++y) {
		if (x_lo == x_hi) for(uint32 x=0; x<w; ++x) {
			dst[x] = vavg;
		} else for(uint32 x=0; x<w; ++x) {
			float vx = dst[x];
			float vy;

			if (vx < x_lo)
				vy = vmin;
			else if (vx > x_hi)
				vy = vmax;
			else {
				vy = pow((vx - x_lo) / (x_hi - x_lo), gammapower);
				vy = vmin + vrange * vy;
			}

			dst[x] = vy;
		}

		dst += pitch/4;
	}
}

static int levels_run(const FilterActivation *fa, const FilterFunctions *ff) {
	using namespace vd2;
	const LevelsFilterData *mfd = (LevelsFilterData *)fa->filter_data;
	const VDXPixmap& px = *fa->src.mpPixmap;
	VDPixmapFormatEx format = VDPixmapFormatNormalize(px.format);

	if (mfd->bLuma) {
		switch(format.format) {
		case kPixFormat_XRGB8888:
			translate_rgba_luma((uint8 *)px.data, px.pitch, px.w, px.h, mfd);
			break;

		case kPixFormat_RGB_Planar32F:
		case kPixFormat_RGBA_Planar32F:
			translate_rgba_luma_32F(px, mfd);
			break;

		case kPixFormat_Y8:
		case kPixFormat_YUV444_Planar:
		case kPixFormat_YUV422_Planar:
		case kPixFormat_YUV420_Planar:
		case kPixFormat_YUV411_Planar:
		case kPixFormat_YUV410_Planar:
			translate_plane((uint8 *)px.data, px.pitch, px.w, px.h, mfd->xtblmono2);
			break;
		}
	} else {
		switch(format.format) {
		case kPixFormat_XRGB8888:
			translate_rgba((uint8 *)px.data, px.pitch, px.w, px.h, mfd->xtblmono);
			break;
		case kPixFormat_RGB_Planar32F:
		case kPixFormat_RGBA_Planar32F:
			translate_plane_32F((float *)px.data, px.pitch, px.w, px.h, mfd);
			translate_plane_32F((float *)px.data2, px.pitch2, px.w, px.h, mfd);
			translate_plane_32F((float *)px.data3, px.pitch3, px.w, px.h, mfd);
			break;
		}
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////

static long levels_param(FilterActivation *fa, const FilterFunctions *ff) {
	using namespace vd2;
	LevelsFilterData *mfd = (LevelsFilterData *)fa->filter_data;
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	pxldst.pitch = pxlsrc.pitch;
	VDPixmapFormatEx format = VDPixmapFormatNormalize(pxlsrc.format);
	mfd->bFullRange = format.colorRangeMode==vd2::kColorRangeMode_Full;

	if (mfd->bLuma) {
		switch(format.format) {
		case kPixFormat_XRGB8888:
		case kPixFormat_Y8:
		case kPixFormat_YUV444_Planar:
		case kPixFormat_YUV422_Planar:
		case kPixFormat_YUV420_Planar:
		case kPixFormat_YUV411_Planar:
		case kPixFormat_YUV410_Planar:
		case kPixFormat_RGB_Planar32F:
		case kPixFormat_RGBA_Planar32F:
			break;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
		}

		return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM;
	} else {
		switch(format.format) {
		case kPixFormat_XRGB8888:
		case kPixFormat_RGB_Planar32F:
		case kPixFormat_RGBA_Planar32F:
			break;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
		}

		return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM;
	}
}

static void levelsRedoTables(LevelsFilterData *mfd) {
	int i;
	const long	y_base	= mfd->iOutputLo;
	const long	y_range	= mfd->iOutputHi - mfd->iOutputLo;
	const double	x_lo	= mfd->iInputLo / (double)0xffff;
	const double	x_hi	= mfd->iInputHi / (double)0xffff;

	for(i=0; i<256; i++) {
		bright_table_R[i] = 19595*i;
		bright_table_G[i] = 38470*i;
		bright_table_B[i] =  7471*i;
	}

	if (x_lo == x_hi) {
		for(i=0; i<256; i++)
			mfd->xtblmono[i] = (unsigned char)(VDRoundToInt(y_base + y_range * 0.5) >> 8);
	} else {
		double gammapower = 1.0 / mfd->rGammaCorr;

		for(i=0; i<256; i++) {
			double y, x;

			x = i / 255.0;

			if (x < x_lo)
				mfd->xtblmono[i] = (unsigned char)(mfd->iOutputLo >> 8);
			else if (x > x_hi)
				mfd->xtblmono[i] = (unsigned char)(mfd->iOutputHi >> 8);
			else {
				y = pow((x - x_lo) / (x_hi - x_lo), gammapower);

				mfd->xtblmono[i] = (unsigned char)(VDRoundToInt(y_base + y_range * y) >> 8);
			}
		}

		double u_scale = 1;
		double u_bias = 0;
		double u_scale2 = 1.0f / 255.0f;
		double u_bias2 = 0;

		if (!mfd->bFullRange) {
			u_scale = 219.0f / 255.0f;
			u_bias = 16.0f / 255.0f;

			u_scale2 = 1.0f / 219.0f;
			u_bias2 = -16.0f / 219.0f;
		}

		double u_lo = x_lo;
		double u_hi = x_hi;
		double v_lo = (double)mfd->iOutputLo / 65535.0;
		double v_hi = (double)mfd->iOutputHi / 65535.0;
		double v_scale = (v_hi - v_lo) * u_scale;
		double v_bias = v_lo * u_scale + u_bias;

		for(i=0; i<256; ++i) {
			double u = (double)i * u_scale2 + u_bias2;
			double v;

			if (u < u_lo)
				v = 0.0;
			else if (u > u_hi)
				v = 1.0;
			else
				v = pow((u - u_lo) / (u_hi - u_lo), gammapower);

			mfd->xtblmono2[i] = VDClampedRoundFixedToUint8Fast((float)(v * v_scale + v_bias));
		}
	}

	if (mfd->bLuma) {
		if (CPUGetEnabledExtensions() & CPUF_SUPPORTS_MMX)
			for(i=0; i<256; i++)
				mfd->xtblluma[i] = (((int)mfd->xtblmono[i] - i)&0xffff) * 0x10001;
		else
			for(i=0; i<256; i++)
				mfd->xtblluma[i] = (int)mfd->xtblmono[i] - i;

		for(i=0; i<256; i++) {
			mfd->xtblluma2[i] = mfd->cliptab + 256 + (int)mfd->xtblmono[i] - i;
			mfd->cliptab[i] = 0;
			mfd->cliptab[i+256] = (uint8)i;
			mfd->cliptab[i+512] = 255;
		}
	}
}

namespace {
	void HistogramTallyRGB_float(const VDXPixmap& px, uint32 *histo) {
		uint32 w = px.w;
		uint32 h = px.h;

		float* dr = (float*)px.data;
		float* dg = (float*)px.data2;
		float* db = (float*)px.data3;

		for(uint32 y=0; y<h; ++y) {
			for(uint32 x=0; x<w; ++x) {
				float vx = dr[x]*0.299 + dg[x]*0.587 + db[x]*0.114;
				int luma = vx*255;
				if(luma<0) luma = 0;
				if(luma>255) luma = 255;
				++histo[luma];
			}

			dr += px.pitch/4;
			dg += px.pitch2/4;
			db += px.pitch3/4;
		}
	}

	void HistogramTallyXRGB8888(const void *src0, ptrdiff_t srcpitch, uint32 w, uint32 h, uint32 *histo) {
		const uint32 *src = (const uint32 *)src0;

		do {
			for(uint32 x=0; x<w; ++x) {
				uint32 px = src[x];
				uint32 rb = px & 0xff00ff;
				uint32 g = px & 0x00ff00;

				uint32 luma = (rb * 0x00130036 + g * 0x0000b700 + 0x00800000) >> 24;
				++histo[luma];
			}

			src = (const uint32 *)((const char *)src + srcpitch);
		} while(--h);
	}

	void HistogramTallyY8_FR(const void *src0, ptrdiff_t srcpitch, uint32 w, uint32 h, uint32 *histo) {
		const uint8 *src = (const uint8 *)src0;

		do {
			for(uint32 x=0; x<w; ++x) {
				++histo[src[x]];
			}

			src += srcpitch;
		} while(--h);
	}

	void HistogramTallyY8(const void *src0, ptrdiff_t srcpitch, uint32 w, uint32 h, uint32 *histo) {
		const uint8 *src = (const uint8 *)src0;
		uint32 localHisto[256] = {0};

		do {
			for(uint32 x=0; x<w; ++x) {
				++localHisto[src[x]];
			}

			src += srcpitch;
		} while(--h);

		uint32 accum = 0x108000;
		for(uint32 i=0; i<256; ++i) {
			histo[i] += localHisto[accum >> 16];
			accum += 0xdbdc;
		}
	}
}

static void levelsSampleCallback(VDXFBitmap *src, long pos, long cnt, void *pv) {
	using namespace vd2;
	LevelsFilterData *mfd = (LevelsFilterData *)pv;

	const VDXPixmap& pxsrc = *src->mpPixmap;
	VDPixmapFormatEx format = VDPixmapFormatNormalize(pxsrc.format);

	switch(format.format) {
		case kPixFormat_XRGB8888:
			HistogramTallyXRGB8888(pxsrc.data, pxsrc.pitch, pxsrc.w, pxsrc.h, mfd->mpHisto);
			break;

		case kPixFormat_RGB_Planar32F:
		case kPixFormat_RGBA_Planar32F:
			HistogramTallyRGB_float(pxsrc, mfd->mpHisto);
			break;

		case kPixFormat_Y8:
		case kPixFormat_YUV444_Planar:
		case kPixFormat_YUV422_Planar:
		case kPixFormat_YUV420_Planar:
		case kPixFormat_YUV411_Planar:
		case kPixFormat_YUV410_Planar:
			if (mfd->bFullRange)
				HistogramTallyY8_FR(pxsrc.data, pxsrc.pitch, pxsrc.w, pxsrc.h, mfd->mpHisto);
			else
				HistogramTallyY8(pxsrc.data, pxsrc.pitch, pxsrc.w, pxsrc.h, mfd->mpHisto);
			break;
	}
}

static void levelsSampleDisplay(LevelsFilterData *mfd, HWND hdlg) {
	int i;
	uint32 *pHisto = mfd->mpHisto;

	if (mfd->mHistoMax < 0)
		ShowWindow(GetDlgItem(hdlg, IDC_HISTOGRAM), SW_HIDE);

	mfd->mHistoMax = pHisto[0];

	for(i=1; i<256; i++)
		if ((sint32)pHisto[i] > mfd->mHistoMax)
			mfd->mHistoMax = (sint32)pHisto[i];

	InvalidateRect(hdlg, &mfd->rHisto, FALSE);
	UpdateWindow(hdlg);
}

static INT_PTR APIENTRY levelsDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	LevelsFilterData *mfd = (struct LevelsFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);
	char buf[32];

    switch (message)
    {
        case WM_INITDIALOG:
			{
				HWND hwndItem;

				mfd = (struct LevelsFilterData *)lParam;
				SetWindowLongPtr(hDlg, DWLP_USER, lParam);

				GetWindowRect(GetDlgItem(hDlg, IDC_HISTOGRAM), &mfd->rHisto);
				ScreenToClient(hDlg, (POINT *)&mfd->rHisto + 0);
				ScreenToClient(hDlg, (POINT *)&mfd->rHisto + 1);

				mfd->fInhibitUpdate = false;

				hwndItem = GetDlgItem(hDlg, IDC_INPUT_LEVELS);

				SendMessage(hwndItem, VLCM_SETTABCOUNT, FALSE, 3);
				SendMessage(hwndItem, VLCM_SETTABCOLOR, MAKELONG(0, FALSE), 0x000000);
				SendMessage(hwndItem, VLCM_SETTABCOLOR, MAKELONG(1, FALSE), 0x808080);
				SendMessage(hwndItem, VLCM_SETTABCOLOR, MAKELONG(2, FALSE), 0xFFFFFF);
				SendMessage(hwndItem, VLCM_MOVETABPOS, MAKELONG(0, FALSE), mfd->iInputLo);
				SendMessage(hwndItem, VLCM_MOVETABPOS, MAKELONG(1, FALSE), (int)(mfd->iInputLo + (mfd->iInputHi-mfd->iInputLo)*mfd->rHalfPt));
				SendMessage(hwndItem, VLCM_MOVETABPOS, MAKELONG(2,  TRUE), mfd->iInputHi);
				SendMessage(hwndItem, VLCM_SETGRADIENT, 0x000000, 0xFFFFFF);

				hwndItem = GetDlgItem(hDlg, IDC_OUTPUT_LEVELS);

				SendMessage(hwndItem, VLCM_SETTABCOUNT, FALSE, 2);
				SendMessage(hwndItem, VLCM_SETTABCOLOR, MAKELONG(0, FALSE), 0x000000);
				SendMessage(hwndItem, VLCM_SETTABCOLOR, MAKELONG(1, FALSE), 0xFFFFFF);
				SendMessage(hwndItem, VLCM_MOVETABPOS, MAKELONG(0, FALSE), mfd->iOutputLo);
				SendMessage(hwndItem, VLCM_MOVETABPOS, MAKELONG(1,  TRUE), mfd->iOutputHi);
				SendMessage(hwndItem, VLCM_SETGRADIENT, 0x000000, 0xFFFFFF);

				CheckDlgButton(hDlg, IDC_LUMA, mfd->bLuma ? BST_CHECKED : BST_UNCHECKED);

				EnableWindow(GetDlgItem(hDlg, IDC_SAMPLE), true);
				EnableWindow(GetDlgItem(hDlg, IDC_SAMPLE_MULTIPLE), true);
				mfd->ifp->SetSampleCallback(levelsSampleCallback, (void *)mfd);
				mfd->ifp->InitButton((VDXHWND)GetDlgItem(hDlg, IDC_PREVIEW));

			}
            return (TRUE);

        case WM_COMMAND:
			if (mfd->fInhibitUpdate)
				return TRUE;

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

			case IDC_SAMPLE:
				memset(mfd->mpHisto, 0, sizeof(mfd->mpHisto[0])*256);
				mfd->ifp->SampleCurrentFrame();
				levelsSampleDisplay(mfd, hDlg);
				return TRUE;

			case IDC_SAMPLE_MULTIPLE:
				memset(mfd->mpHisto, 0, sizeof(mfd->mpHisto[0])*256);
				mfd->ifp->SampleFrames();
				levelsSampleDisplay(mfd, hDlg);
				return TRUE;

			case IDC_LUMA:
				{	
					bool bNewState = !!IsDlgButtonChecked(hDlg, IDC_LUMA);

					if (bNewState != mfd->bLuma) {
						mfd->bLuma = bNewState;
						mfd->ifp->RedoSystem();
					}

				}
				return TRUE;

			case IDC_INPUTGAMMA:
				mfd->fInhibitUpdate = true;
				if (HIWORD(wParam) == EN_CHANGE) {
					double rv;

					if (GetWindowText((HWND)lParam, buf, sizeof buf))
						if (1 == sscanf(buf, "%lg", &rv)) {
							// pt^(1/rv) = 0.5
							// pt = 0.5^-(1/rv)

							if (rv < 0.01)
								rv = 0.01;
							else if (rv > 10.0)
								rv = 10.0;

							mfd->rGammaCorr = rv;
							mfd->rHalfPt = pow(0.5, rv);

							_RPT4(0, "%g %g %4X %4X\n", mfd->rHalfPt, mfd->rGammaCorr, mfd->iInputLo, mfd->iInputHi);

							SendDlgItemMessage(hDlg, IDC_INPUT_LEVELS, VLCM_SETTABPOS, MAKELONG(1, TRUE),
									(int)(0.5 + mfd->iInputLo + (mfd->iInputHi - mfd->iInputLo)*mfd->rHalfPt));

							levelsRedoTables(mfd);
							mfd->ifp->RedoFrame();
						}
				} else if (HIWORD(wParam) == EN_KILLFOCUS) {
					sprintf(buf, "%.3f", mfd->rGammaCorr);
					SetWindowText((HWND)lParam, buf);
				}
				mfd->fInhibitUpdate = false;
				return TRUE;

			case IDC_INPUTLO:
				mfd->fInhibitUpdate = true;
				if (HIWORD(wParam) == EN_CHANGE) {
					BOOL f;
					int v;

					v = GetDlgItemInt(hDlg, IDC_INPUTLO, &f, FALSE) * 0x0101;

					if (v<0)
						v=0;
					else if (v>mfd->iInputHi)
						v = mfd->iInputHi;

					mfd->iInputLo = v;

					SendDlgItemMessage(hDlg, IDC_INPUT_LEVELS, VLCM_SETTABPOS, MAKELONG(0, TRUE), v);
					SendDlgItemMessage(hDlg, IDC_INPUT_LEVELS, VLCM_SETTABPOS, MAKELONG(1, TRUE), (int)floor(0.5 + mfd->iInputLo + (mfd->iInputHi-mfd->iInputLo)*mfd->rHalfPt));
					levelsRedoTables(mfd);
					mfd->ifp->RedoFrame();
				} else if (HIWORD(wParam) == EN_KILLFOCUS)
					SetDlgItemInt(hDlg, IDC_INPUTLO, mfd->iInputLo>>8, FALSE);

				mfd->fInhibitUpdate = false;
				return TRUE;
			case IDC_INPUTHI:
				mfd->fInhibitUpdate = true;
				if (HIWORD(wParam) == EN_CHANGE) {
					BOOL f;
					int v;

					v = GetDlgItemInt(hDlg, IDC_INPUTHI, &f, FALSE)*0x0101;

					if (v<mfd->iInputLo)
						v=mfd->iInputLo;
					else if (v>0xffff)
						v = 0xffff;

					mfd->iInputHi = v;

					SendDlgItemMessage(hDlg, IDC_INPUT_LEVELS, VLCM_SETTABPOS, MAKELONG(2, TRUE), v);
					SendDlgItemMessage(hDlg, IDC_INPUT_LEVELS, VLCM_SETTABPOS, MAKELONG(1, TRUE), (int)floor(0.5 + mfd->iInputLo + (mfd->iInputHi-mfd->iInputLo)*mfd->rHalfPt));
					levelsRedoTables(mfd);
					mfd->ifp->RedoFrame();
				} else if (HIWORD(wParam) == EN_KILLFOCUS)
					SetDlgItemInt(hDlg, IDC_INPUTHI, mfd->iInputHi>>8, FALSE);

				mfd->fInhibitUpdate = false;
				return TRUE;
			case IDC_OUTPUTLO:
				mfd->fInhibitUpdate = true;
				if (HIWORD(wParam) == EN_CHANGE) {
					BOOL f;
					int v;

					v = GetDlgItemInt(hDlg, IDC_OUTPUTLO, &f, FALSE)*0x0101;

					if (v<0)
						v=0;
					else if (v>mfd->iOutputHi)
						v = mfd->iOutputHi;

					mfd->iOutputLo = v;

					SendDlgItemMessage(hDlg, IDC_OUTPUT_LEVELS, VLCM_SETTABPOS, MAKELONG(0, TRUE), v);

					levelsRedoTables(mfd);
					mfd->ifp->RedoFrame();

				} else if (HIWORD(wParam) == EN_KILLFOCUS)
					SetDlgItemInt(hDlg, IDC_OUTPUTLO, mfd->iOutputLo>>8, FALSE);

				mfd->fInhibitUpdate = false;
				return TRUE;
			case IDC_OUTPUTHI:
				mfd->fInhibitUpdate = true;
				if (HIWORD(wParam) == EN_CHANGE) {
					BOOL f;
					int v;

					v = GetDlgItemInt(hDlg, IDC_OUTPUTHI, &f, FALSE)*0x0101;

					if (v<mfd->iOutputLo)
						v=mfd->iOutputLo;
					else if (v>0xffff)
						v = 0xffff;

					mfd->iOutputHi = v;

					SendDlgItemMessage(hDlg, IDC_OUTPUT_LEVELS, VLCM_SETTABPOS, MAKELONG(1, TRUE), v);

					levelsRedoTables(mfd);
					mfd->ifp->RedoFrame();
				} else if (HIWORD(wParam) == EN_KILLFOCUS)
					SetDlgItemInt(hDlg, IDC_OUTPUTHI, mfd->iOutputHi>>8, FALSE);

				mfd->fInhibitUpdate = false;
				return TRUE;
			}
			break;

		case WM_PAINT:
			if (mfd->mHistoMax < 0)
				return FALSE;
			{
				HDC hdc;
				PAINTSTRUCT ps;
				RECT rPaint, rClip;
				int i;

				hdc = BeginPaint(hDlg, &ps);

				i = GetClipBox(hdc, &rClip);

				if (i==ERROR || i==NULLREGION || IntersectRect(&rPaint, &mfd->rHisto, &rClip)) {
					int x, xlo, xhi, w;
					long lMax = (long)mfd->mHistoMax;

					w = mfd->rHisto.right - mfd->rHisto.left;

					if (i==NULLREGION || i == ERROR) {
						xlo = 0;
						xhi = w;
					} else {
						xlo = rPaint.left - mfd->rHisto.left;
						xhi = rPaint.right - mfd->rHisto.left;
					}

					FillRect(hdc, &mfd->rHisto, (HBRUSH)GetStockObject(WHITE_BRUSH));

					for(x=xlo; x<xhi; x++) {
						int xp, yp, h;
						long y;

						i = (x * 0xFF00) / (w-1);

						y = (long)mfd->mpHisto[i>>8];

						xp = x+mfd->rHisto.left;
						yp = mfd->rHisto.bottom-1;
						h = MulDiv(y, mfd->rHisto.bottom-mfd->rHisto.top, lMax);

						if (h>0) {
							MoveToEx(hdc, x+mfd->rHisto.left, yp, NULL);
							LineTo(hdc, x+mfd->rHisto.left, yp - h);
						}
					}
				}

				EndPaint(hDlg, &ps);
			}
			break;

		case WM_NOTIFY:
			if (!mfd->fInhibitUpdate) {
				NMHDR *pnmh = (NMHDR *)lParam;
				NMVLTABCHANGE *pnmvltc = (NMVLTABCHANGE *)lParam;
				char buf[32];

				mfd->fInhibitUpdate = true;

				switch(pnmh->idFrom) {
				case IDC_INPUT_LEVELS:
					switch(pnmvltc->iTab) {
					case 0:
						mfd->iInputLo = pnmvltc->iNewPos;
						SetDlgItemInt(hDlg, IDC_INPUTLO, mfd->iInputLo>>8, FALSE);
						UpdateWindow(GetDlgItem(hDlg, IDC_INPUTLO));
						SendDlgItemMessage(hDlg, IDC_INPUT_LEVELS, VLCM_SETTABPOS, MAKELONG(1, TRUE), (int)floor(0.5 + mfd->iInputLo + (mfd->iInputHi-mfd->iInputLo)*mfd->rHalfPt));
						break;
					case 1:
						if (mfd->iInputLo == mfd->iInputHi) {
							mfd->rHalfPt = 0.5;
							mfd->rGammaCorr = 1.0;
						} else {

							// compute halfpoint... if inputlo/hi range is even, drop down by 1/2 to allow for halfpoint

							if ((mfd->iInputLo + mfd->iInputHi) & 1)
								mfd->rHalfPt = ((pnmvltc->iNewPos<=(mfd->iInputLo+mfd->iInputHi)/2 ? +1 : -1) + 2*(pnmvltc->iNewPos  - mfd->iInputLo))
										/ (2.0*(mfd->iInputHi - mfd->iInputLo - 1));
							else
								mfd->rHalfPt = (pnmvltc->iNewPos - mfd->iInputLo) / (double)(mfd->iInputHi - mfd->iInputLo);

							// halfpt ^ (1/gc) = 0.5
							// 1/gc = log_halfpt(0.5)
							// 1/gc = log(0.5) / log(halfpt)
							// gc = log(halfpt) / log(0.5)

							// clamp gc to [0.01...10.0]

							if (mfd->rHalfPt > 0.9930925)
								mfd->rHalfPt = 0.9930925;
							else if (mfd->rHalfPt < 0.0009765625)
								mfd->rHalfPt = 0.0009765625;

							mfd->rGammaCorr = log(mfd->rHalfPt) / -0.693147180559945309417232121458177;	// log(0.5);
						}

						sprintf(buf, "%.3f", mfd->rGammaCorr);
						SetDlgItemText(hDlg, IDC_INPUTGAMMA, buf);
						UpdateWindow(GetDlgItem(hDlg, IDC_INPUTGAMMA));
						break;
					case 2:
						mfd->iInputHi = pnmvltc->iNewPos;
						SetDlgItemInt(hDlg, IDC_INPUTHI, mfd->iInputHi>>8, FALSE);
						UpdateWindow(GetDlgItem(hDlg, IDC_INPUTHI));
						SendDlgItemMessage(hDlg, IDC_INPUT_LEVELS, VLCM_SETTABPOS, MAKELONG(1, TRUE), (int)floor(0.5 + mfd->iInputLo + (mfd->iInputHi-mfd->iInputLo)*mfd->rHalfPt));
						break;
					}
					levelsRedoTables(mfd);
					mfd->ifp->RedoFrame();
					mfd->fInhibitUpdate = false;
					return TRUE;
				case IDC_OUTPUT_LEVELS:
					switch(pnmvltc->iTab) {
					case 0:
						mfd->iOutputLo = pnmvltc->iNewPos;
						SetDlgItemInt(hDlg, IDC_OUTPUTLO, (pnmvltc->iNewPos >> 8), FALSE);
						break;
					case 1:
						mfd->iOutputHi = pnmvltc->iNewPos;
						SetDlgItemInt(hDlg, IDC_OUTPUTHI, (pnmvltc->iNewPos >> 8), FALSE);
						break;
					}
					levelsRedoTables(mfd);
					mfd->ifp->RedoFrame();
					mfd->fInhibitUpdate = false;
					return TRUE;
				}
				mfd->fInhibitUpdate = false;
			}
			break;
    }
    return FALSE;
}

static int levels_config(FilterActivation *fa, const FilterFunctions *ff, VDXHWND hWnd) {
	LevelsFilterData *mfd = (LevelsFilterData *)fa->filter_data;
	LevelsFilterData mfd2 = *mfd;
	int ret;
	uint32 histo[256];

	mfd->ifp = fa->ifp;
	mfd->mpHisto = histo;
	mfd->mHistoMax = -1;

	ret = DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_LEVELS), (HWND)hWnd, levelsDlgProc, (LPARAM)mfd);

	if (ret)
		*mfd = mfd2;

	return ret;
}

///////////////////////////////////////////

static int levels_start(FilterActivation *fa, const FilterFunctions *ff) {
	LevelsFilterData *mfd = (LevelsFilterData *)fa->filter_data;

	levelsRedoTables(mfd);

	return 0;
}

static void levels_string2(const FilterActivation *fa, const FilterFunctions *ff, char *buf, int maxlen) {
	LevelsFilterData *mfd = (LevelsFilterData *)fa->filter_data;

	_snprintf(buf, maxlen, " ( [%4.2f-%4.2f] > %.2f > [%4.2f-%4.2f] (%s) )",
			mfd->iInputLo/(double)0xffff,
			mfd->iInputHi/(double)0xffff,
			mfd->rGammaCorr,
			mfd->iOutputLo/(double)0xffff,
			mfd->iOutputHi/(double)0xffff,
			mfd->bLuma ? "Y" : "RGB"
			);
}

static void levels_script_config(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterActivation *fa = (FilterActivation *)lpVoid;
	LevelsFilterData *mfd = (LevelsFilterData *)fa->filter_data;

	mfd->iInputLo	= argv[0].asInt();
	mfd->iInputHi	= argv[1].asInt();
	mfd->rGammaCorr	= argv[2].asInt() / 16777216.0;
	mfd->rHalfPt	= pow(0.5, mfd->rGammaCorr);
	mfd->iOutputLo	= argv[3].asInt();
	mfd->iOutputHi	= argv[4].asInt();

	mfd->bLuma = false;
	if (argc > 5)
		mfd->bLuma = !!argv[5].asInt();
}

static ScriptFunctionDef levels_func_defs[]={
	{ (ScriptFunctionPtr)levels_script_config, "Config", "0iiiii" },
	{ (ScriptFunctionPtr)levels_script_config, NULL, "0iiiiii" },
	{ NULL },
};

static CScriptObject levels_obj={
	NULL, levels_func_defs
};

static bool levels_script_line(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	LevelsFilterData *mfd = (LevelsFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(0x%04X,0x%04X,0x%08lX,0x%04X,0x%04X, %d)"
				,mfd->iInputLo
				,mfd->iInputHi
				,(long)(0.5 + mfd->rGammaCorr * 16777216.0)
				,mfd->iOutputLo
				,mfd->iOutputHi
				,mfd->bLuma
				);

	return true;
}

FilterDefinition filterDef_levels={
	0,0,NULL,
	"levels",
	"Applies a levels or levels-correction transform to the image."
			"\n\n[Assembly optimized]"
		,
	NULL,NULL,
	sizeof(LevelsFilterData),
	levels_init,NULL,
	levels_run,
	levels_param,
	levels_config,
	NULL,
	levels_start,
	NULL,					// end

	&levels_obj,
	levels_script_line,
	levels_string2,
};
