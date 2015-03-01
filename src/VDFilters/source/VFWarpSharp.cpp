//	warpsharp - edge sharpening filter for VirtualDub
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
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <new>
#include <commctrl.h>

#include <vd2/system/cpuaccel.h>
#include <vd2/system/vdstl.h>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideoaccel.h>
#include <vd2/vdlib/Dialog.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/Kasumi/pixmaputils.h>

#include "Blur.h"
#include "resource.h"

#include "VFWarpSharp.inl"

#if defined(_MSC_VER) && defined(_M_IX86)
	#pragma warning(disable: 4799)
	#pragma warning(disable: 4324)
#endif

static void MakeCubic4Table(int *table, double A, bool mmx_table) {
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

///////////////////////////////////////////////////////////////////////////////

typedef unsigned char Pixel8;

class VDVFilterConfigWarpSharp {
public:
	int mDepth;
	int mBlurLevel;
};

///////////////////////////////////////////////////////////////////////////////

class __declspec(align(8)) VDVFilterWarpSharp : public VDXVideoFilter {
public:
	VDVFilterWarpSharp();
	~VDVFilterWarpSharp();

	uint32 GetParams();
	void Start();
	void Run();
	void End();

	void StartAccel(IVDXAContext *vdxa);
	void RunAccel(IVDXAContext *vdxa);
	void StopAccel(IVDXAContext *vdxa);

	bool Configure(VDXHWND hwnd);
	void GetSettingString(char *buf, int maxlen);
	void GetScriptString(char *buf, int maxlen);

	void ScriptConfig(IVDXScriptInterpreter *interp, const VDXScriptValue *argv, int argc);

	VDXVF_DECLARE_SCRIPT_METHODS();

protected:
	ptrdiff_t mBumpPitch;
	unsigned char *mpBumpBuf;
	VEffect *veffBlur;
	VDPixmap vbmBump;

	uint32 mVDXAPSGradient;
	uint32 mVDXAPSBlur;
	uint32 mVDXAPSFinal;
	uint32 mVDXARTGradientBuffer1;
	uint32 mVDXARTGradientBuffer2;

	vdfastvector<int>	mDisplacementRowMap;

	VDVFilterConfigWarpSharp mConfig;

	int mCubicTab[1024];
	__declspec(align(8)) int mCubicTabMMX[1024];
};

VDXVF_BEGIN_SCRIPT_METHODS(VDVFilterWarpSharp)
	VDXVF_DEFINE_SCRIPT_METHOD(VDVFilterWarpSharp, ScriptConfig, "ii")
VDXVF_END_SCRIPT_METHODS()

VDVFilterWarpSharp::VDVFilterWarpSharp()
	: mVDXAPSGradient(0)
	, mVDXAPSBlur(0)
	, mVDXAPSFinal(0)
	, mVDXARTGradientBuffer1(0)
	, mVDXARTGradientBuffer2(0)
{
}

VDVFilterWarpSharp::~VDVFilterWarpSharp() {
}

uint32 VDVFilterWarpSharp::GetParams() {
	const VDXPixmapLayout& pxsrc = *fa->src.mpPixmapLayout;

	fa->src.mBorderWidth = 1;
	fa->src.mBorderHeight = 1;

	switch(pxsrc.format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
		case nsVDXPixmap::kPixFormat_Y8:
		case nsVDXPixmap::kPixFormat_Y8_FR:
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_VDXA_RGB:
		case nsVDXPixmap::kPixFormat_VDXA_YUV:
			return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM;
	}

	return FILTERPARAM_NOT_SUPPORTED;
}

void VDVFilterWarpSharp::Start() {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;

	mBumpPitch = (pxlsrc.w+7)&~7;

	if (!(mpBumpBuf = new_nothrow unsigned char[mBumpPitch * pxlsrc.h]))
		ff->ExceptOutOfMemory();

	memset(mpBumpBuf, 0, mBumpPitch*pxlsrc.h);

	mDisplacementRowMap.resize(pxlsrc.w*2, 0);

	vbmBump.data = mpBumpBuf;
	vbmBump.pitch = mBumpPitch;
	vbmBump.w = pxlsrc.w;
	vbmBump.h = pxlsrc.h;
	vbmBump.format = nsVDPixmap::kPixFormat_Y8;

	if (!(veffBlur = VCreateEffectBlurHi(VDPixmapToLayoutFromBase(vbmBump, mpBumpBuf))))
		ff->ExceptOutOfMemory();

	MakeCubic4Table(mCubicTab, -0.60, false);
	MakeCubic4Table(mCubicTabMMX, -0.60, true);
}

namespace {
	static void WarpSharpComputeGradientRow8(uint8 *dst, const uint8 *src, ptrdiff_t pitch, uint32 count) {
		const uint8 *src0 = src;
		const uint8 *src1 = (const uint8 *)src0 + pitch;
		const uint8 *src2 = (const uint8 *)src1 + pitch;

		for(uint32 x=0; x<count; ++x) {
			const int p00 = src0[0];
			const int p10 = src0[1];
			const int p20 = src0[2];
			const int p01 = src1[0];
			const int p21 = src1[2];
			const int p02 = src2[0];
			const int p12 = src2[1];
			const int p22 = src2[2];
			++src0;
			++src1;
			++src2;

			int a = p22 - p00;
			int b = p20 - p02;
			int h = p21 - p01;
			int v = p12 - p10;

			int dx = a + b + 2*h;
			int dy = a - b + 2*v;

			int odx = abs(dx);
			int ody = abs(dy);

			// min(dx,dy) = dx + 0.5 * (fabs(dy-dx) - (dy-dx))
			// max(dx,dy) = dx + 0.5 * (fabs(dx-dy) - (dx-dy))

			// h ~= dx + dy - 0.5*min(dx,dy)
			//	  = dx + dy - 0.5*(dx + 0.5 * (fabs(dy-dx) - (dy-dx)))
			//	  = dx + dy - 0.5*dx + 0.25 * (fabs(dy-dx) - (dy-dx))
			//	  = 0.5*dx + dy + 0.25 * (fabs(dy-dx) - (dy-dx))
			//	  = 0.5*dx + dy + 0.25 * fabs(dy-dx) - 0.25*dy + 0.25*dx
			//	  = 0.75*(dx + dy) + 0.25 * fabs(dy-dx)

			int level = (3*(odx+ody) + abs(ody-odx) + 2) >> 2;

			if (level > 255)
				level = 255;

			*dst++ = (unsigned char)level;
		}
	}

	static void WarpSharpComputeGradientRow32(uint8 *dst, const uint32 *src, ptrdiff_t pitch, uint32 count) {
		const uint32 *src0 = src;
		const uint32 *src1 = (const uint32 *)((const char *)src0 + pitch);
		const uint32 *src2 = (const uint32 *)((const char *)src1 + pitch);

		for(uint32 x=0; x<count; ++x) {
			const uint32 p00 = src0[0];
			const uint32 p10 = src0[1];
			const uint32 p20 = src0[2];
			const uint32 p01 = src1[0];
			const uint32 p21 = src1[2];
			const uint32 p02 = src2[0];
			const uint32 p12 = src2[1];
			const uint32 p22 = src2[2];
			++src0;
			++src1;
			++src2;

			const uint32 rb00 = p00 & 0xff00ff;
			const uint32 rb10 = p10 & 0xff00ff;
			const uint32 rb20 = p20 & 0xff00ff;
			const uint32 rb01 = p01 & 0xff00ff;
			const uint32 rb21 = p21 & 0xff00ff;
			const uint32 rb02 = p02 & 0xff00ff;
			const uint32 rb12 = p12 & 0xff00ff;
			const uint32 rb22 = p22 & 0xff00ff;

			const uint32 g00 = p00 & 0xff00;
			const uint32 g10 = p10 & 0xff00;
			const uint32 g20 = p20 & 0xff00;
			const uint32 g01 = p01 & 0xff00;
			const uint32 g21 = p21 & 0xff00;
			const uint32 g02 = p02 & 0xff00;
			const uint32 g12 = p12 & 0xff00;
			const uint32 g22 = p22 & 0xff00;

			uint32 rb_a = rb22 - rb00 + 0x40004000;
			uint32 rb_b = rb20 - rb02;
			uint32 rb_h = rb21 - rb01;
			uint32 rb_v = rb12 - rb10;
			uint32 g_a = g22 - g00;
			uint32 g_b = g20 - g02;
			uint32 g_h = g21 - g01;
			uint32 g_v = g12 - g10;

			uint32 rb_dx = rb_a + rb_b + 2*rb_h;
			uint32 rb_dy = rb_a - rb_b + 2*rb_v;
			int r_dx = (rb_dx >> 16) - 0x4000;
			int r_dy = (rb_dy >> 16) - 0x4000;
			int b_dx = (rb_dx & 0xffff) - 0x4000;
			int b_dy = (rb_dy & 0xffff) - 0x4000;
			int g_dx = (int)(g_a + g_b + 2*g_h) >> 8;
			int g_dy = (int)(g_a - g_b + 2*g_v) >> 8;

			int odx = abs(r_dx)*59 + abs(g_dx)*183 + abs(b_dx)*14;
			int ody = abs(r_dy)*59 + abs(g_dy)*183 + abs(b_dy)*14;

			// min(dx,dy) = dx + 0.5 * (fabs(dy-dx) - (dy-dx))
			// max(dx,dy) = dx + 0.5 * (fabs(dx-dy) - (dx-dy))

			// h ~= dx + dy - 0.5*min(dx,dy)
			//	  = dx + dy - 0.5*(dx + 0.5 * (fabs(dy-dx) - (dy-dx)))
			//	  = dx + dy - 0.5*dx + 0.25 * (fabs(dy-dx) - (dy-dx))
			//	  = 0.5*dx + dy + 0.25 * (fabs(dy-dx) - (dy-dx))
			//	  = 0.5*dx + dy + 0.25 * fabs(dy-dx) - 0.25*dy + 0.25*dx
			//	  = 0.75*(dx + dy) + 0.25 * fabs(dy-dx)

			int level = (3*(odx+ody) + abs(ody-odx) + 512) >> 10;

			if (level > 255)
				level = 255;

			*dst++ = (unsigned char)level;
		}
	}

#ifdef _M_IX86
	static void __declspec(naked) __cdecl WarpSharpComputeGradientRow32MMX(uint8 *dst, const uint32 *src, ptrdiff_t pitch, uint32 count) {
		static const __declspec(align(8)) __int64 coeff = 0x0000003b00b7000ei64;

		__asm {
			push		ebp
			push		edi
			push		esi
			push		ebx

			mov			esi, [esp+4+16]		;esi = dest
			mov			edi, [esp+8+16]		;edi = source
			mov			ebx, [esp+12+16]	;ebx = pitch
			add			[esp+16+16], esi	;arg4 = dest+count = limit
			pxor		mm7,mm7

xloop:
			movd		mm0, [edi+ebx*2+8]

			movd		mm1, [edi]
			punpcklbw	mm0, mm7			;mm0 = br

			movd		mm2, [edi+ebx*2+0]
			punpcklbw	mm1, mm7			;mm1 = tl

			movd		mm3, [edi+8]
			punpcklbw	mm2, mm7			;mm2 = bl
			psubw		mm0, mm1			;mm0 = br - tl

			movd		mm4, [edi+ebx*2+4]
			punpcklbw	mm3, mm7			;mm3 = tr
			movq		mm1, mm0

			movd		mm5, [edi+4]
			punpcklbw	mm4, mm7			;mm4 = bc
			psubw		mm2, mm3			;mm2 = bl - tr

			movd		mm3, [edi+ebx+8]
			punpcklbw	mm5, mm7			;mm1 = tc
			paddw		mm0, mm2			;mm0 = br - tl + bl - tr

			movd		mm6, [edi+ebx]
			punpcklbw	mm3, mm7			;mm3 = mr
			psubw		mm1, mm2			;mm1 = br - tl - bl + tr

			punpcklbw	mm6, mm7			;mm6 = ml

			psubw		mm4, mm5			;mm4 = bc - tc
			psubw		mm3, mm6			;mm3 = mr - ml

			paddw		mm4, mm4			;mm4 = (bc - tc)*2
			paddw		mm3, mm3			;mm3 = (mr - ml)*2

			paddw		mm0, mm4			;mm0 = br - tl + bl - tr + (bc - tc)*2
			paddw		mm1, mm3			;mm1 = br - tl - bl + tr + (mr - ml)*2

			movq		mm2, mm0
			movq		mm3, mm1
			psraw		mm2, 15
			psraw		mm3, 15

			pxor		mm0, mm2
			pxor		mm1, mm3
			psubw		mm0, mm2			;mm0 = |dy|
			psubw		mm1, mm3			;mm1 = |dx|

			pmaddwd		mm0,coeff
			pmaddwd		mm1,coeff

			movq		mm5,mm0
			psrlq		mm0,32
			movq		mm6,mm1
			psrlq		mm1,32
			paddd		mm0,mm5				;mm0 = dy
			paddd		mm1,mm6				;mm1 = dx

			movd		eax,mm1				;eax = dx
			movd		edx,mm0				;edx = dy

			lea			ecx, [eax + edx]	;edx = dx+dy
			sub			eax, edx			;eax = dx-dy

			cdq								;edx = (dx-dy) < 0 ? -1 : 0

			xor			eax, edx
			lea			ecx, [ecx*2 + ecx + 512 + 80000000h - 1024*256]	;ecx = 3*(dx+dy) + magic

			sub			eax, edx			;eax = |dx-dy|

			add			eax, ecx			;eax = level = 3*(dx+dy) + |dx-dy| + magic
			sar			eax, 10
			cdq
			or			eax, edx

			mov			[esi], al
			add			esi, 1
			add			edi, 4
			cmp			esi, [esp+16+16]	;compare against limit
			jne			xloop
			
			pop			ebx
			pop			esi
			pop			edi
			pop			ebp
			ret
		}
	}
#endif

	static void InterpolateBicubicRowDisplacement32(uint32 *dst, const uint32 *src, ptrdiff_t pitch, const int *disprow, uint32 w, const int *cubicTab) {
		for(uint32 i=0; i<w; ++i) {
			int dispx = disprow[0];
			int dispy = disprow[1];
			disprow += 2;

			int ox = dispx&255;
			int oy = dispy&255;

			const uint32 *src0 = (const uint32 *)((const char *)src + pitch*((dispy>>8)-1)) + (dispx>>8);
			++src;

			const int *cubic_horiz = cubicTab + ox*4;
			const int *cubic_vert = cubicTab + oy*4;

			const uint32 *src1 = (const uint32 *)((const char *)src0 + pitch);
			const uint32 *src2 = (const uint32 *)((const char *)src1 + pitch);
			const uint32 *src3 = (const uint32 *)((const char *)src2 + pitch);

			const uint32 p00 = src0[-1];
			const uint32 p01 = src0[ 0];
			const uint32 p02 = src0[+1];
			const uint32 p03 = src0[+2];
			const uint32 p10 = src1[-1];
			const uint32 p11 = src1[ 0];
			const uint32 p12 = src1[+1];
			const uint32 p13 = src1[+2];
			const uint32 p20 = src2[-1];
			const uint32 p21 = src2[ 0];
			const uint32 p22 = src2[+1];
			const uint32 p23 = src2[+2];
			const uint32 p30 = src3[-1];
			const uint32 p31 = src3[ 0];
			const uint32 p32 = src3[+1];
			const uint32 p33 = src3[+2];

			int ch0 = cubic_horiz[0];
			int ch1 = cubic_horiz[1];
			int ch2 = cubic_horiz[2];
			int ch3 = cubic_horiz[3];
			int cv0 = cubic_vert[0];
			int cv1 = cubic_vert[1];
			int cv2 = cubic_vert[2];
			int cv3 = cubic_vert[3];

			int r0 = ((int)((p00>>16)&0xff) * ch0 + (int)((p01>>16)&0xff) * ch1 + (int)((p02>>16)&0xff) * ch2 + (int)((p03>>16)&0xff) * ch3 + 128) >> 8;
			int g0 = ((int)((p00>> 8)&0xff) * ch0 + (int)((p01>> 8)&0xff) * ch1 + (int)((p02>> 8)&0xff) * ch2 + (int)((p03>> 8)&0xff) * ch3 + 128) >> 8;
			int b0 = ((int)((p00    )&0xff) * ch0 + (int)((p01    )&0xff) * ch1 + (int)((p02    )&0xff) * ch2 + (int)((p03    )&0xff) * ch3 + 128) >> 8;
			int r1 = ((int)((p10>>16)&0xff) * ch0 + (int)((p11>>16)&0xff) * ch1 + (int)((p12>>16)&0xff) * ch2 + (int)((p13>>16)&0xff) * ch3 + 128) >> 8;
			int g1 = ((int)((p10>> 8)&0xff) * ch0 + (int)((p11>> 8)&0xff) * ch1 + (int)((p12>> 8)&0xff) * ch2 + (int)((p13>> 8)&0xff) * ch3 + 128) >> 8;
			int b1 = ((int)((p10    )&0xff) * ch0 + (int)((p11    )&0xff) * ch1 + (int)((p12    )&0xff) * ch2 + (int)((p13    )&0xff) * ch3 + 128) >> 8;
			int r2 = ((int)((p20>>16)&0xff) * ch0 + (int)((p21>>16)&0xff) * ch1 + (int)((p22>>16)&0xff) * ch2 + (int)((p23>>16)&0xff) * ch3 + 128) >> 8;
			int g2 = ((int)((p20>> 8)&0xff) * ch0 + (int)((p21>> 8)&0xff) * ch1 + (int)((p22>> 8)&0xff) * ch2 + (int)((p23>> 8)&0xff) * ch3 + 128) >> 8;
			int b2 = ((int)((p20    )&0xff) * ch0 + (int)((p21    )&0xff) * ch1 + (int)((p22    )&0xff) * ch2 + (int)((p23    )&0xff) * ch3 + 128) >> 8;
			int r3 = ((int)((p30>>16)&0xff) * ch0 + (int)((p31>>16)&0xff) * ch1 + (int)((p32>>16)&0xff) * ch2 + (int)((p33>>16)&0xff) * ch3 + 128) >> 8;
			int g3 = ((int)((p30>> 8)&0xff) * ch0 + (int)((p31>> 8)&0xff) * ch1 + (int)((p32>> 8)&0xff) * ch2 + (int)((p33>> 8)&0xff) * ch3 + 128) >> 8;
			int b3 = ((int)((p30    )&0xff) * ch0 + (int)((p31    )&0xff) * ch1 + (int)((p32    )&0xff) * ch2 + (int)((p33    )&0xff) * ch3 + 128) >> 8;

			int r = (r0 * cv0 + r1 * cv1 + r2 * cv2 + r3 * cv3 + (1<<19)) >> 20;
			int g = (g0 * cv0 + g1 * cv1 + g2 * cv2 + g3 * cv3 + (1<<19)) >> 20;
			int b = (b0 * cv0 + b1 * cv1 + b2 * cv2 + b3 * cv3 + (1<<19)) >> 20;

			if (r<0) r=0; else if (r>255) r=255;
			if (g<0) g=0; else if (g>255) g=255;
			if (b<0) b=0; else if (b>255) b=255;

			dst[i] = (r<<16) + (g<<8) + b;
		}
	}

	static void InterpolateBicubicRowDisplacement8(uint8 *dst, const uint8 *src, ptrdiff_t pitch, const int *disprow, uint32 w, const int *cubicTab) {
		for(uint32 i=0; i<w; ++i) {
			int dispx = disprow[0];
			int dispy = disprow[1];
			disprow += 2;

			int ox = dispx&255;
			int oy = dispy&255;

			const uint8 *src0 = (const uint8 *)src + pitch*((dispy>>8)-1) + (dispx>>8);
			++src;

			const int *cubic_horiz = cubicTab + ox*4;
			const int *cubic_vert = cubicTab + oy*4;

			const uint8 *src1 = (const uint8 *)src0 + pitch;
			const uint8 *src2 = (const uint8 *)src1 + pitch;
			const uint8 *src3 = (const uint8 *)src2 + pitch;

			const int p00 = src0[-1];
			const int p01 = src0[ 0];
			const int p02 = src0[+1];
			const int p03 = src0[+2];
			const int p10 = src1[-1];
			const int p11 = src1[ 0];
			const int p12 = src1[+1];
			const int p13 = src1[+2];
			const int p20 = src2[-1];
			const int p21 = src2[ 0];
			const int p22 = src2[+1];
			const int p23 = src2[+2];
			const int p30 = src3[-1];
			const int p31 = src3[ 0];
			const int p32 = src3[+1];
			const int p33 = src3[+2];

			int ch0 = cubic_horiz[0];
			int ch1 = cubic_horiz[1];
			int ch2 = cubic_horiz[2];
			int ch3 = cubic_horiz[3];
			int cv0 = cubic_vert[0];
			int cv1 = cubic_vert[1];
			int cv2 = cubic_vert[2];
			int cv3 = cubic_vert[3];

			int p0 = (p00 * ch0 + p01 * ch1 + p02 * ch2 + p03 * ch3 + 128) >> 8;
			int p1 = (p10 * ch0 + p11 * ch1 + p12 * ch2 + p13 * ch3 + 128) >> 8;
			int p2 = (p20 * ch0 + p21 * ch1 + p22 * ch2 + p23 * ch3 + 128) >> 8;
			int p3 = (p30 * ch0 + p31 * ch1 + p32 * ch2 + p33 * ch3 + 128) >> 8;

			int p = (p0 * cv0 + p1 * cv1 + p2 * cv2 + p3 * cv3 + (1<<19)) >> 20;

			if (p<0) p=0; else if (p>255) p=255;

			dst[i] = (uint8)p;
		}
	}

#ifdef _M_IX86
	uint32 __declspec(naked) __cdecl InterpolateBicubicMMX(const uint32 *src, ptrdiff_t pitch, const int *horiz, const int *vert) {
		static const __declspec(align(8)) __int64 rounder = 0x0000200000002000i64;

		__asm {
			mov		eax,[esp+4]
			mov		ecx,[esp+8]
			mov		edx,[esp+12]

			pxor		mm7,mm7

			movd		mm0,[eax]				;fetch p00
			movd		mm6,[eax+4]				;fetch p01
			punpcklbw	mm0,mm7					;mm0 = [a00][r00][g00][b00]
			movq		mm1,mm0
			punpcklbw	mm6,mm7
			punpcklwd	mm0,mm6					;mm0 = [g01][g00][b01][b00]
			punpckhwd	mm1,mm6					;mm1 = [a01][a00][r01][r00]
			pmaddwd		mm0,[edx]				;mm0 = [a00-01][r00-01]
			pmaddwd		mm1,[edx]				;mm1 = [g00-01][b00-01]

			movd		mm2,[eax+8]				;fetch p00
			movd		mm6,[eax+12]			;fetch p01
			punpcklbw	mm2,mm7					;mm2 = [a02][r02][g02][b02]
			movq		mm3,mm2
			punpcklbw	mm6,mm7
			punpcklwd	mm2,mm6					;mm2 = [g03][g02][b03][b02]
			punpckhwd	mm3,mm6					;mm3 = [a03][a02][r03][r02]
			pmaddwd		mm2,[edx+8]				;mm2 = [a02-03][r02-03]
			pmaddwd		mm3,[edx+8]				;mm3 = [g02-03][b02-03]

			paddd		mm0,mm2
			paddd		mm1,mm3

			paddd		mm0,rounder
			paddd		mm1,rounder

			psrad		mm0,14
			psrad		mm1,14

			packssdw	mm0,mm1					;mm0 = intermediate pixel 0
			add			eax,ecx

			;---------------------------------

			movd		mm1,[eax]				;fetch p00
			movd		mm6,[eax+4]				;fetch p01
			punpcklbw	mm1,mm7					;mm0 = [a00][r00][g00][b00]
			movq		mm2,mm1
			punpcklbw	mm6,mm7
			punpcklwd	mm1,mm6					;mm0 = [g01][g00][b01][b00]
			punpckhwd	mm2,mm6					;mm1 = [a01][a00][r01][r00]
			pmaddwd		mm1,[edx]				;mm0 = [a00-01][r00-01]
			pmaddwd		mm2,[edx]				;mm1 = [g00-01][b00-01]

			movd		mm3,[eax+8]				;fetch p00
			movd		mm6,[eax+12]			;fetch p01
			punpcklbw	mm3,mm7					;mm2 = [a02][r02][g02][b02]
			movq		mm4,mm3
			punpcklbw	mm6,mm7
			punpcklwd	mm3,mm6					;mm2 = [g03][g02][b03][b02]
			punpckhwd	mm4,mm6					;mm3 = [a03][a02][r03][r02]
			pmaddwd		mm3,[edx+8]				;mm2 = [a02-03][r02-03]
			pmaddwd		mm4,[edx+8]				;mm3 = [g02-03][b02-03]

			paddd		mm1,mm3
			paddd		mm2,mm4

			paddd		mm1,rounder
			paddd		mm2,rounder

			psrad		mm1,14
			psrad		mm2,14

			packssdw	mm1,mm2					;mm1 = intermediate pixel 1
			add			eax,ecx

			;---------------------------------

			movd		mm2,[eax]				;fetch p00
			movd		mm6,[eax+4]				;fetch p01
			punpcklbw	mm2,mm7					;mm0 = [a00][r00][g00][b00]
			movq		mm3,mm2
			punpcklbw	mm6,mm7
			punpcklwd	mm2,mm6					;mm0 = [g01][g00][b01][b00]
			punpckhwd	mm3,mm6					;mm1 = [a01][a00][r01][r00]
			pmaddwd		mm2,[edx]				;mm0 = [a00-01][r00-01]
			pmaddwd		mm3,[edx]				;mm1 = [g00-01][b00-01]

			movd		mm4,[eax+8]				;fetch p00
			movd		mm6,[eax+12]			;fetch p01
			punpcklbw	mm4,mm7					;mm2 = [a02][r02][g02][b02]
			movq		mm5,mm4
			punpcklbw	mm6,mm7
			punpcklwd	mm4,mm6					;mm2 = [g03][g02][b03][b02]
			punpckhwd	mm5,mm6					;mm3 = [a03][a02][r03][r02]
			pmaddwd		mm4,[edx+8]				;mm2 = [a02-03][r02-03]
			pmaddwd		mm5,[edx+8]				;mm3 = [g02-03][b02-03]

			paddd		mm2,mm4
			paddd		mm3,mm5

			paddd		mm2,rounder
			paddd		mm3,rounder

			psrad		mm2,14
			psrad		mm3,14

			packssdw	mm2,mm3 				;mm2 = intermediate pixel 2
			add			eax,ecx

			;---------------------------------

			movd		mm3,[eax]				;fetch p00
			movd		mm6,[eax+4]				;fetch p01
			punpcklbw	mm3,mm7					;mm0 = [a00][r00][g00][b00]
			movq		mm4,mm3
			punpcklbw	mm6,mm7
			punpcklwd	mm3,mm6					;mm0 = [g01][g00][b01][b00]
			punpckhwd	mm4,mm6					;mm1 = [a01][a00][r01][r00]
			pmaddwd		mm3,[edx]				;mm0 = [a00-01][r00-01]
			pmaddwd		mm4,[edx]				;mm1 = [g00-01][b00-01]

			movd		mm5,[eax+8]				;fetch p00
			movd		mm6,[eax+12]			;fetch p01
			punpcklbw	mm5,mm7					;mm2 = [a02][r02][g02][b02]
			punpcklbw	mm6,mm7
			movq		mm7,mm5
			punpcklwd	mm5,mm6					;mm2 = [g03][g02][b03][b02]
			punpckhwd	mm7,mm6					;mm3 = [a03][a02][r03][r02]
			pmaddwd		mm5,[edx+8]				;mm2 = [a02-03][r02-03]
			pmaddwd		mm7,[edx+8]				;mm3 = [g02-03][b02-03]

			paddd		mm3,mm5
			paddd		mm4,mm7

			paddd		mm3,rounder
			paddd		mm4,rounder

			psrad		mm3,14
			psrad		mm4,14

			packssdw	mm3,mm4 				;mm2 = intermediate pixel 2

			;---------------------------------

			mov			edx,[esp+16]

			movq		mm4,mm0
			movq		mm5,mm2
			punpcklwd	mm0,mm1
			punpckhwd	mm4,mm1
			punpcklwd	mm2,mm3
			punpckhwd	mm5,mm3

			pmaddwd		mm0,[edx]
			pmaddwd		mm4,[edx]
			pmaddwd		mm2,[edx+8]
			pmaddwd		mm5,[edx+8]

			paddd		mm0,mm2
			paddd		mm4,mm5

			paddd		mm0,rounder
			paddd		mm4,rounder

			psrad		mm0,14
			psrad		mm4,14

			packssdw	mm0,mm4
			packuswb	mm0,mm0

			movd		eax,mm0
			ret
		};
	}

	static void InterpolateBicubicRowDisplacement32MMX(uint32 *dst, const uint32 *src, ptrdiff_t pitch, const int *disprow, uint32 w, const int *cubicTabMMX) {
		--src;
		for(uint32 i=0; i<w; ++i) {
			int dispx = disprow[0];
			int dispy = disprow[1];
			disprow += 2;

			int ox = dispx&255;
			int oy = dispy&255;

			const uint32 *src2 = (const uint32 *)((const char *)src + pitch*((dispy>>8)-1)) + (dispx>>8);
			const int *cubic_horiz = cubicTabMMX + ox*4;
			const int *cubic_vert = cubicTabMMX + oy*4;

			dst[i] = InterpolateBicubicMMX(src2, pitch, cubic_horiz, cubic_vert);
			++src;
		}
	}
#endif

	static void ShowDisplacement(uint32 *dst, const uint32 *src, ptrdiff_t pitch, const int *disprow, uint32 w) {
		--src;
		for(uint32 i=0; i<w; ++i) {
			int dispx = disprow[0];
			int dispy = disprow[1];
			disprow += 2;

			dispx = ((dispx + 4) >> 3) + 128;
			dispy = ((dispy + 4) >> 3) + 128;

			if ((unsigned)dispx >= 256)
				dispx = ~dispx >> 31;

			if ((unsigned)dispy >= 256)
				dispy = ~dispy >> 31;

			dst[i] = ((dispx & 0xff) << 16) + ((dispy & 0xff) << 8) + 0x80;
			++src;
		}
	}
}

void VDVFilterWarpSharp::Run() {
	const char *src;
	char *dst;
	const VDXPixmap& pxdst = *fa->dst.mpPixmap;
	const VDXPixmap& pxsrc = *fa->src.mpPixmap;
	const ptrdiff_t srcpitch = pxsrc.pitch;
	const ptrdiff_t dstpitch = pxdst.pitch;
	const ptrdiff_t bumppitch = mBumpPitch;
	unsigned char *bump;
	int x, y;

	src = (char *)pxsrc.data;
	for(y=1; y<pxsrc.h-1; y++) {
		bump = mpBumpBuf + bumppitch*y + 1;

		if (pxsrc.format == nsVDXPixmap::kPixFormat_XRGB8888) {
#ifdef _M_IX86
			if (MMX_enabled)
				WarpSharpComputeGradientRow32MMX(bump, (const uint32 *)src, srcpitch, pxsrc.w - 2);
			else
#endif
				WarpSharpComputeGradientRow32(bump, (const uint32 *)src, srcpitch, pxsrc.w - 2);
		} else {
			WarpSharpComputeGradientRow8(bump, (const uint8 *)src, srcpitch, pxsrc.w - 2);
		}

		src += srcpitch;
	}

	for(int pass=0; pass<=mConfig.mBlurLevel; ++pass) {
		veffBlur->run(vbmBump);
	}

	VDCPUCleanupExtensions();

	src = (const char *)pxsrc.data + pxsrc.pitch*4;
	dst = (char *)pxdst.data + pxdst.pitch*4;

	const char *src2;
	const char *src3;
	char *dst2;
	char *dst3;
	ptrdiff_t srcpitch2;
	ptrdiff_t srcpitch3;
	ptrdiff_t dstpitch2;
	ptrdiff_t dstpitch3;

	if (pxdst.format == nsVDXPixmap::kPixFormat_XRGB8888)
		VDMemcpyRect(pxdst.data, pxdst.pitch, pxsrc.data, pxsrc.pitch, pxsrc.w*4, 4);
	else {
		VDMemcpyRect(pxdst.data, pxdst.pitch, pxsrc.data, pxsrc.pitch, pxsrc.w, 4);

		switch(pxdst.format) {
		case nsVDXPixmap::kPixFormat_Y8:
		case nsVDXPixmap::kPixFormat_Y8_FR:
			break;

		default:
			VDMemcpyRect(pxdst.data2, pxdst.pitch2, pxsrc.data2, pxsrc.pitch2, pxsrc.w, 4);
			VDMemcpyRect(pxdst.data3, pxdst.pitch3, pxsrc.data3, pxsrc.pitch3, pxsrc.w, 4);

			src2 = (const char *)pxsrc.data2 + pxsrc.pitch2*4;
			src3 = (const char *)pxsrc.data3 + pxsrc.pitch3*4;
			dst2 = (char *)pxdst.data2 + pxdst.pitch2*4;
			dst3 = (char *)pxdst.data3 + pxdst.pitch3*4;
			srcpitch2 = pxsrc.pitch2;
			srcpitch3 = pxsrc.pitch3;
			dstpitch2 = pxdst.pitch2;
			dstpitch3 = pxdst.pitch3;
		}
	}

	int lo_dispy, hi_dispy;

	const int height = pxsrc.h;
	const int width = pxsrc.w;
	const int depth = mConfig.mDepth*(mConfig.mBlurLevel + 1);

	int *const disprow = mDisplacementRowMap.data();
	for(y=0; y<height-8; y++) {
		lo_dispy = -(3+y)*256;
		hi_dispy = (height-6-y)*256 - 1;
		bump = mpBumpBuf + mBumpPitch * (3+y) + 3;

		int lo_dispx = -3*256;
		int hi_dispx = (width-6)*256 - 1;

		int *dispdst = disprow;
		for(x=0; x<width-8; x++) {
			int dispx, dispy;

			// Vector points away from edge

			dispx = (int)bump[bumppitch] - (int)bump[2+bumppitch];
			dispy = (int)bump[1] - (int)bump[1+bumppitch*2];

			dispx = ((dispx*depth+128)>>8);
			dispy = ((dispy*depth+128)>>8);

			if (dispx < lo_dispx)
				dispx = lo_dispx;
			else if (dispx > hi_dispx)
				dispx = hi_dispx;

			if (dispy < lo_dispy)
				dispy = lo_dispy;
			else if (dispy > hi_dispy)
				dispy = hi_dispy;

			dispdst[0] = dispx;
			dispdst[1] = dispy;
			dispdst += 2;

			++bump;
			lo_dispx -= 256;
			hi_dispx -= 256;
		}

		if (pxdst.format == nsVDXPixmap::kPixFormat_XRGB8888) {
			uint32 *dst32 = (uint32 *)dst + 4;
			const uint32 *src32 = (const uint32 *)src + 4;

			dst32[-4] = src32[-4];
			dst32[-3] = src32[-3];
			dst32[-2] = src32[-2];
			dst32[-1] = src32[-1];

#ifdef _M_IX86
			if (MMX_enabled)
				InterpolateBicubicRowDisplacement32MMX(dst32, src32, srcpitch, disprow, width-8, mCubicTabMMX);
			else
#endif
				InterpolateBicubicRowDisplacement32(dst32, src32, srcpitch, disprow, width-8, mCubicTab);

			dst32[x] = src32[x];
			dst32[x+1] = src32[x+1];
			dst32[x+2] = src32[x+2];
			dst32[x+3] = src32[x+3];
		} else {
			uint8 *dst8 = (uint8 *)dst + 4;
			const uint8 *src8 = (const uint8 *)src + 4;

			dst8[-4] = src8[-4];
			dst8[-3] = src8[-3];
			dst8[-2] = src8[-2];
			dst8[-1] = src8[-1];

			InterpolateBicubicRowDisplacement8(dst8, src8, srcpitch, disprow, width-8, mCubicTab);

			dst8[x] = src8[x];
			dst8[x+1] = src8[x+1];
			dst8[x+2] = src8[x+2];
			dst8[x+3] = src8[x+3];

			switch(pxdst.format) {
			case nsVDXPixmap::kPixFormat_Y8:
			case nsVDXPixmap::kPixFormat_Y8_FR:
				break;

			default:
				dst8 = (uint8 *)dst2 + 4;
				src8 = (const uint8 *)src2 + 4;

				for(int i=0; i<2; ++i) {
					dst8[-4] = src8[-4];
					dst8[-3] = src8[-3];
					dst8[-2] = src8[-2];
					dst8[-1] = src8[-1];

					InterpolateBicubicRowDisplacement8(dst8, src8, srcpitch, disprow, width-8, mCubicTab);

					dst8[x] = src8[x];
					dst8[x+1] = src8[x+1];
					dst8[x+2] = src8[x+2];
					dst8[x+3] = src8[x+3];

					dst8 = (uint8 *)dst3 + 4;
					src8 = (const uint8 *)src3 + 4;
				}

				src2 += srcpitch2;
				src3 += srcpitch3;
				dst2 += dstpitch2;
				dst3 += dstpitch3;
			}
		}

		src += srcpitch;
		dst += dstpitch;
	}

	VDCPUCleanupExtensions();

	if (pxdst.format == nsVDXPixmap::kPixFormat_XRGB8888)
		VDMemcpyRect((char *)pxdst.data + pxdst.pitch*(pxdst.h - 4), pxdst.pitch, (const char *)pxsrc.data + pxsrc.pitch*(pxsrc.h - 4), pxsrc.pitch, pxsrc.w*4, 4);
	else {
		VDMemcpyRect((char *)pxdst.data + pxdst.pitch*(pxdst.h - 4), pxdst.pitch, (const char *)pxsrc.data + pxsrc.pitch*(pxsrc.h - 4), pxsrc.pitch, pxsrc.w, 4);

		switch(pxdst.format) {
		case nsVDXPixmap::kPixFormat_Y8:
		case nsVDXPixmap::kPixFormat_Y8_FR:
			break;

		default:
			VDMemcpyRect((char *)pxdst.data2 + pxdst.pitch2*(pxdst.h - 4), pxdst.pitch2, (const char *)pxsrc.data2 + pxsrc.pitch2*(pxsrc.h - 4), pxsrc.pitch2, pxsrc.w, 4);
			VDMemcpyRect((char *)pxdst.data3 + pxdst.pitch3*(pxdst.h - 4), pxdst.pitch3, (const char *)pxsrc.data3 + pxsrc.pitch3*(pxsrc.h - 4), pxsrc.pitch3, pxsrc.w, 4);
		}
	}
}

void VDVFilterWarpSharp::End() {
	delete[] mpBumpBuf;
	mpBumpBuf = NULL;

	if (veffBlur) {
		delete veffBlur;
		veffBlur = NULL;
	}
}

void VDVFilterWarpSharp::StartAccel(IVDXAContext *vdxa) {
	if (fa->src.mpPixmap->format == nsVDXPixmap::kPixFormat_VDXA_YUV)
		mVDXAPSGradient = vdxa->CreateFragmentProgram(kVDXAPF_D3D9ByteCodePS20, kVDFilterWarpSharpPS_GradientYUV, sizeof kVDFilterWarpSharpPS_GradientYUV);
	else
		mVDXAPSGradient = vdxa->CreateFragmentProgram(kVDXAPF_D3D9ByteCodePS20, kVDFilterWarpSharpPS_GradientRGB, sizeof kVDFilterWarpSharpPS_GradientRGB);

	mVDXAPSBlur = vdxa->CreateFragmentProgram(kVDXAPF_D3D9ByteCodePS20, kVDFilterWarpSharpPS_Blur, sizeof kVDFilterWarpSharpPS_Blur);
	mVDXAPSFinal = vdxa->CreateFragmentProgram(kVDXAPF_D3D9ByteCodePS20, kVDFilterWarpSharpPS_Final, sizeof kVDFilterWarpSharpPS_Final);
	mVDXARTGradientBuffer1 = vdxa->CreateRenderTexture(fa->dst.w, fa->dst.h, 1, 1, kVDXAF_Unknown, false);
	mVDXARTGradientBuffer2 = vdxa->CreateRenderTexture(fa->dst.w, fa->dst.h, 1, 1, kVDXAF_Unknown, false);
}

void VDVFilterWarpSharp::RunAccel(IVDXAContext *vdxa) {
	vdxa->SetSampler(0, fa->src.mVDXAHandle, kVDXAFilt_Bilinear);
	vdxa->SetTextureMatrix(0, fa->src.mVDXAHandle, -0.5f, -0.5f, NULL);
	vdxa->SetTextureMatrix(1, fa->src.mVDXAHandle, +0.5f, -0.5f, NULL);
	vdxa->SetTextureMatrix(2, fa->src.mVDXAHandle, -0.5f, +0.5f, NULL);
	vdxa->SetTextureMatrix(3, fa->src.mVDXAHandle, +0.5f, +0.5f, NULL);
	vdxa->DrawRect(mVDXARTGradientBuffer1, mVDXAPSGradient, NULL);

	for(int pass=0; pass<=mConfig.mBlurLevel; ++pass) {
		vdxa->SetSampler(0, mVDXARTGradientBuffer1, kVDXAFilt_Bilinear);
		vdxa->SetTextureMatrix(0, mVDXARTGradientBuffer1, -0.5f, -0.5f, NULL);
		vdxa->SetTextureMatrix(1, mVDXARTGradientBuffer1, +0.5f, -0.5f, NULL);
		vdxa->SetTextureMatrix(2, mVDXARTGradientBuffer1, -0.5f, +0.5f, NULL);
		vdxa->SetTextureMatrix(3, mVDXARTGradientBuffer1, +0.5f, +0.5f, NULL);
		vdxa->DrawRect(mVDXARTGradientBuffer2, mVDXAPSBlur, NULL);

		std::swap(mVDXARTGradientBuffer1, mVDXARTGradientBuffer2);
	}

	vdxa->SetSampler(0, mVDXARTGradientBuffer1, kVDXAFilt_Bilinear);
	vdxa->SetSampler(1, fa->src.mVDXAHandle, kVDXAFilt_Bilinear);
	vdxa->SetTextureMatrix(0, mVDXARTGradientBuffer1, +1.0f,  0.0f, NULL);
	vdxa->SetTextureMatrix(1, mVDXARTGradientBuffer1, -1.0f,  0.0f, NULL);
	vdxa->SetTextureMatrix(2, mVDXARTGradientBuffer1,  0.0f, +1.0f, NULL);
	vdxa->SetTextureMatrix(3, mVDXARTGradientBuffer1,  0.0f, -1.0f, NULL);
	vdxa->SetTextureMatrix(4, fa->src.mVDXAHandle, 0, 0, NULL);

	VDXATextureDesc desc;
	vdxa->GetTextureDesc(fa->src.mVDXAHandle, desc);

	const float depth = mConfig.mDepth*(mConfig.mBlurLevel + 1) / 256.0f;
	const float v[4] = {
		depth * desc.mInvTexWidth,
		depth * desc.mInvTexHeight,
		0,
		0
	};

	vdxa->SetFragmentProgramConstF(0, 1, v);

	vdxa->DrawRect(fa->dst.mVDXAHandle, mVDXAPSFinal, NULL);
}

void VDVFilterWarpSharp::StopAccel(IVDXAContext *vdxa) {
	vdxa->DestroyObject(mVDXAPSGradient);
	mVDXAPSGradient = 0;

	vdxa->DestroyObject(mVDXAPSBlur);
	mVDXAPSBlur = 0;

	vdxa->DestroyObject(mVDXAPSFinal);
	mVDXAPSFinal = 0;

	vdxa->DestroyObject(mVDXARTGradientBuffer1);
	mVDXARTGradientBuffer1 = 0;

	vdxa->DestroyObject(mVDXARTGradientBuffer2);
	mVDXARTGradientBuffer2 = 0;
}

class VDVFilterDialogWarpSharp : public VDDialogFrameW32 {
public:
	VDVFilterDialogWarpSharp(VDVFilterConfigWarpSharp& config, IVDXFilterPreview *ifp)
		: VDDialogFrameW32(IDD_FILTER_WARPSHARP)
		, mifp(ifp)
		, mConfig(config)
		, mOldConfig(config)
	{}

	const VDVFilterConfigWarpSharp& GetConfig() const { return mConfig; }

protected:
	virtual bool OnLoaded();
	virtual bool OnOK();
	virtual bool OnCancel();
	virtual bool OnCommand(uint32 id, uint32 extcode);

	virtual VDZINT_PTR DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);

	IVDXFilterPreview *const mifp;

	VDVFilterConfigWarpSharp& mConfig;
	VDVFilterConfigWarpSharp mOldConfig;
};

bool VDVFilterDialogWarpSharp::OnLoaded() {
	HWND hwndItem = GetDlgItem(mhdlg, IDC_DEPTH);

	SendMessage(hwndItem, TBM_SETRANGE, TRUE, MAKELONG(0,1024));
	SendMessage(hwndItem, TBM_SETPOS, (WPARAM)TRUE, mConfig.mDepth);
	SetDlgItemInt(mhdlg, IDC_STATIC_DEPTH, MulDiv(mConfig.mDepth, 100, 256), FALSE);

	hwndItem = GetDlgItem(mhdlg, IDC_BLUR);

	SendMessage(hwndItem, TBM_SETRANGE, TRUE, MAKELONG(0,11));
	SendMessage(hwndItem, TBM_SETPOS, (WPARAM)TRUE, mConfig.mBlurLevel);
	SetDlgItemInt(mhdlg, IDC_STATIC_BLUR, mConfig.mBlurLevel+1, FALSE);

	mifp->InitButton((VDXHWND)GetDlgItem(mhdlg, IDC_PREVIEW));

	return VDDialogFrameW32::OnLoaded();
}

bool VDVFilterDialogWarpSharp::OnOK() {
	mifp->Close();
	End(true);
	return true;
}

bool VDVFilterDialogWarpSharp::OnCancel() {
	mifp->Close();
    End(false);
	mConfig = mOldConfig;
    return true;
}

bool VDVFilterDialogWarpSharp::OnCommand(uint32 id, uint32 extcode) {
	if (id == IDC_PREVIEW) {
		mifp->Toggle((VDXHWND)mhdlg);
		return true;
	}

	return false;
}

VDZINT_PTR VDVFilterDialogWarpSharp::DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam) {
	switch (msg) {
		case WM_HSCROLL:
			if (lParam) {
				switch(GetWindowLong((HWND)lParam, GWL_ID)) {
				case IDC_DEPTH:
					if ((int)wParam != mConfig.mDepth) {
						mConfig.mDepth = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
						mifp->RedoFrame();
						SetDlgItemInt(mhdlg, IDC_STATIC_DEPTH, MulDiv(mConfig.mDepth, 100, 256), TRUE);
					}
					break;
				case IDC_BLUR:
					if ((int)wParam != mConfig.mBlurLevel) {
						mConfig.mBlurLevel = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
						mifp->RedoFrame();
						SetDlgItemInt(mhdlg, IDC_STATIC_BLUR, mConfig.mBlurLevel+1, TRUE);
					}
					break;
				}
			}
			return TRUE;
	}

	return VDDialogFrameW32::DlgProc(msg, wParam, lParam);
}

void VDVFilterWarpSharp::GetSettingString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, " (depth: %d, blur %dx)", (mConfig.mDepth * 100 + 128) >> 8, mConfig.mBlurLevel + 1);
}

bool VDVFilterWarpSharp::Configure(VDXHWND hwnd) {
	VDVFilterDialogWarpSharp dlg(mConfig, fa->ifp);

	if (dlg.ShowDialog((VDGUIHandle)hwnd)) {
		mConfig = dlg.GetConfig();
		return true;
	}

	return false;
}

void VDVFilterWarpSharp::GetScriptString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, "Config(%d, %d)", mConfig.mDepth, mConfig.mBlurLevel);
}

void VDVFilterWarpSharp::ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc) {
	mConfig.mDepth		= argv[0].asInt();
	mConfig.mBlurLevel	= 0;

	if (argc > 1)
		mConfig.mBlurLevel = argv[1].asInt();
}

extern const VDXFilterDefinition g_VDVFWarpSharp = VDXVideoFilterDefinition<VDVFilterWarpSharp>(NULL, "warp sharp", "Tightens edges in an image by warping the image toward edge boundaries. (Version 1.2)\n\n[Assembly optimized][Requires MMX]");

// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{24,{flat}}' }'' : unreferenced local function has been removed
#pragma warning(disable: 4505)
