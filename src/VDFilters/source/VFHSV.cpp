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

#include "resource.h"

extern HINSTANCE g_hInst;

///////////////////////////////////

struct HSVFilterData {
	int hue;
	int sat;
	int val;
	int	_pad;

	int divtab[256];
	int	graytab[256];
	int valtab[512][2];
	int sattab[256];

	IVDXFilterPreview *ifp;

	// Don't make these functions virtual.  Bad things happen.
	void RunScalar(uint32 *dst, ptrdiff_t dstpitch, unsigned w, unsigned h, int subshift, int sectors);
	void RebuildSVTables();
};

template<class T>
void ptr_increment(T *&ptr, ptrdiff_t delta) {
	ptr = (T *)((char *)ptr + delta);
}

// XXX: These assembly routines are pretty awful -- the algorithm turns out to be slower
//      than the scalar code, at least on P3/P4.  Must redo.

#if 0
static void __declspec(naked) __stdcall hsv_run_ISSE(uint32 *dst, ptrdiff_t dstpitch, unsigned w, unsigned h, int subshift, int sectors, int sat, int val, const unsigned short (*divtab)[4]) {
	static const __int64 x0000FFFFFFFFFFFF = 0x0000FFFFFFFFFFFF;
	static const __int64 shifts[2][3]={
		{48,32,16}, {0,16,32}
	};

	static const __int64 rangefloor = 0;
	static const __int64 rangeceil = 0x000000FF00FF00FF;
	static const __int64 x0001w = 0x0001000100010001;
	static const __int64 x0100w = 0x0100010001000100;
	static const __int64 x0101w = 0x0101010101010101;
	static const __int64 x01ffw = 0x01ff01ff01ff01ff;
	static const __int64 x00000101d = 0x0000010100000101;

	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			esi, [esp+12+16]				;esi = w
		mov			ecx, [esp+ 4+16]				;ecx = dst
		shl			esi, 2
		mov			edi, [esp+16+16]				;edi = h
		add			ecx, esi
		mov			edx, [esp+24+16]				;edx = sectors
		neg			esi
		mov			ebx, [esp+ 8+16]				;ebx = pitch
		mov			ebp, [esp+36+16]				;ebp = divtab
		lea			edx, [edx*8+shifts]				;edx = ptr to shift table entry
		pshufw		mm1, [esp+28+16], 0			;mm1 = sat | sat | sat | sat
		pshufw		mm2, [esp+32+16], 0			;mm2 = val | val | val | val
		pshufw		mm7, [esp+20+16], 0			;mm6 = subshift | subshift | subshift | subshift
		psllw		mm1, 6
		mov			ebx, [esp+8+16]
		mov			eax, esp
		sub			esp, 40
		and			esp, -8
		mov			[esp+36], eax
		mov			[esp+32], ebx
		movq		mm3, mm2
		pcmpeqb		mm5, mm5
		psraw		mm3, 15
		pxor		mm2, mm3
		psubw		mm2, mm3
		pxor		mm3, mm5
		psrlw		mm3, 7
		movq		mm4, x0100w
		psubw		mm4, mm2
		psllw		mm4, 5

		movq		[esp+24], mm3
		movq		[esp+0], mm4
		movq		[esp+16], mm1
		movd		eax, mm7
		or			eax, eax
		js			negative_shift

		movq		[esp+8], mm7

yloop_positive_shift:
		mov			eax, esi
xloop_positive_shift:
		movd		mm0, [ecx+eax]
		pxor		mm2, mm2

		punpcklbw	mm0, mm2
		pshufw		mm1, mm0, 11001001b				;mm1 = A | B | R | G

		movq		mm3, mm0						;mm3 = A | R | G | B
		pshufw		mm2, mm0, 11010010b				;mm2 = A | G | B | R

		pminsw		mm0, mm1						;mm0 = A |<RB|<RG|<GB
		pmaxsw		mm1, mm2						;mm1 = A |>GB|>RB|>RG

		pminsw		mm0, mm2						;mm0 = A |<RGB|<RGB|<RGB
		movq		mm2, mm3						;mm3 = A | R | G | B

		pmaxsw		mm1, mm3						;mm1 = A |>RGB|>RGB|>RGB
		pcmpeqw		mm2, mm1						;mm2 = 1 | R=Max | G=Max | B=Max

		movq		mm6, mm1
		pshufw		mm4, mm2, 11010010b				;mm4 = 1 | G=Max | B=Max | R=Max

		psubw		mm6, mm0						;mm6 = range
		movd		ebx, mm6
		movq		mm7, mm0
		movzx		ebx, bx
		paddw		mm7, mm1						;mm7 = rangecen2x
		pxor		mm7, [esp+24]					;mm7 ^= invertmask
		psllw		mm7, 3
		pmulhuw		mm7, [esp]						;mm7 *= valscale
		pxor		mm7, [esp+24]					;mm7 = rangecen2x'

		movq		mm5, x01ffw
		psubw		mm5, mm7
		pminsw		mm5, mm7

		psllw		mm6, 2
		pmulhuw		mm6, [esp+16]					;mm6 = (range * sat) >> 16
		pminsw		mm6, mm5						;mm6 = newrange2x

		movq		mm5, mm7
		paddw		mm5, mm6						;mm5 = newtop2x
		psubw		mm7, mm6						;mm7 = newbottom2x

		psrlw		mm5, 1							;mm5 = newtop
		psrlw		mm7, 1							;mm7 = newbottom

		psubw		mm3, mm0						;mm3 = rgb - bottom
		movq		mm0, mm7
		movq		mm1, mm5

		psubw		mm5, mm7						;mm7 = newrange

		pmullw		mm3, mm5						;mm3 = (rgb - bottom) * newrange
		movq		mm6, mm3

		pmullw		mm3, [ebp + ebx*8]
		pmulhuw		mm6, [ebp + ebx*8]
		psrlw		mm3, 15
		paddw		mm3, mm6						;mm6 = ((rgb - bottom) * newrange * divtab[range] + 0x8000) >> 16
		paddw		mm3, mm0

		pmullw		mm5, [esp+8]					;mm6 = subshift (scale) newrange
		psrlw		mm5, 8

		pand		mm2, mm5

		pandn		mm4, mm2						;mm4 = 0 | (R=Max && G<Max)?delta | (G=Max && B<Max)?delta | (B=Max && R<Max)?delta

		pshufw		mm4, mm4, 11010010b				;mm4 = 0 | (G=Max && B<Max)?delta | (B=Max && R<Max)?delta | (R=Max && G<Max)?delta

		psubw		mm3, mm4						;decrease 1st axis

		movq		mm2, mm3
		pmaxsw		mm3, mm0						;clamp minimum

		psubw		mm2, mm3						;mm2 = underflow

		pshufw		mm2, mm2, 11010010b				;mm2 = 0 | G->R | B->G | R->B

		psubw		mm3, mm2						;shift underflow to 2nd axis

		movq		mm2, mm3
		pminsw		mm3, mm1						;clamp maximum

		psubw		mm2, mm3						;mm2 = overflow

		pshufw		mm2, mm2, 11010010b				;mm2 = 0 | G->R | B->G | R->B

		movq		mm0, x0000FFFFFFFFFFFF
		psubw		mm3, mm2						;shift overflow to 3rd axis

		pand		mm0, mm3
		psllq		mm3, [edx+0]					;apply left shift (dammit, why doesn't pshufw take a third register)

		psrlq		mm0, [edx+24]					;apply right shift

		paddw		mm0, mm3						;mm0 = final pixel!

		packuswb	mm0, mm0

		movd		[ecx+eax], mm0

		add			eax, 4
		jne			xloop_positive_shift

		add			ecx, [esp+32]
		dec			edi
		jne			yloop_positive_shift

		mov			esp, [esp+36]
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		emms
		ret			36

negative_shift:
		pxor		mm0, mm0
		psubw		mm0, mm7
		movq		[esp+8], mm0

yloop_negative_shift:
		mov			eax, esi
xloop_negative_shift:
		movd		mm0, [ecx+eax]
		pxor		mm2, mm2

		punpcklbw	mm0, mm2
		pshufw		mm1, mm0, 11001001b				;mm1 = A | B | R | G

		movq		mm3, mm0						;mm3 = A | R | G | B
		pshufw		mm2, mm0, 11010010b				;mm2 = A | G | B | R

		pminsw		mm0, mm1						;mm0 = A |<RB|<RG|<GB
		pmaxsw		mm1, mm2						;mm1 = A |>GB|>RB|>RG

		pminsw		mm0, mm2						;mm0 = A |<RGB|<RGB|<RGB
		movq		mm2, mm3						;mm3 = A | R | G | B

		pmaxsw		mm1, mm3						;mm1 = A |>RGB|>RGB|>RGB
		pcmpeqw		mm2, mm1						;mm2 = 1 | R=Max | G=Max | B=Max

		movq		mm6, mm1
		pshufw		mm4, mm2, 11001001b				;mm4 = 1 | B=Max | R=Max | G=Max

		psubw		mm6, mm0						;mm6 = range
		movd		ebx, mm6
		movq		mm7, mm0
		movzx		ebx, bx
		paddw		mm7, mm1						;mm7 = rangecen2x
		pxor		mm7, [esp+24]					;mm7 ^= invertmask
		psllw		mm7, 3
		pmulhuw		mm7, [esp]						;mm7 *= valscale
		pxor		mm7, [esp+24]					;mm7 = rangecen2x'

		movq		mm5, x01ffw
		psubw		mm5, mm7
		pminsw		mm5, mm7

		psllw		mm6, 2
		pmulhuw		mm6, [esp+16]					;mm6 = (range * sat) >> 16
		pminsw		mm6, mm5						;mm6 = newrange2x

		movq		mm5, mm7
		paddw		mm5, mm6						;mm5 = newtop2x
		psubw		mm7, mm6						;mm7 = newbottom2x

		psrlw		mm5, 1							;mm5 = newtop
		psrlw		mm7, 1							;mm7 = newbottom

		psubw		mm3, mm0						;mm3 = rgb - bottom
		movq		mm0, mm7
		movq		mm1, mm5

		psubw		mm5, mm7						;mm7 = newrange

		pmullw		mm3, mm5						;mm3 = (rgb - bottom) * newrange
		movq		mm6, mm3

		pmullw		mm3, [ebp + ebx*8]
		pmulhuw		mm6, [ebp + ebx*8]
		psrlw		mm3, 15
		paddw		mm3, mm6						;mm6 = ((rgb - bottom) * newrange * divtab[range] + 0x8000) >> 16
		paddw		mm3, mm0

		pmullw		mm5, [esp+8]					;mm6 = subshift (scale) newrange
		psrlw		mm5, 8

		pand		mm2, mm5

		pandn		mm4, mm2						;mm4 = 0 | (R=Max && B<Max)?delta | (G=Max && R<Max)?delta | (B=Max && G<Max)?delta
		pshufw		mm4, mm4, 11001001b				;mm4 = 0 | (B=Max && G<Max)?delta | (R=Max && B<Max)?delta | (G=Max && R<Max)?delta
		psubw		mm3, mm4						;decrease 1st axis

		movq		mm2, mm3
		pmaxsw		mm3, mm0						;clamp minimum
		psubw		mm2, mm3						;mm2 = underflow
		pshufw		mm2, mm2, 11001001b				;mm2 = 0 | B->R | R->G | G->B 
		psubw		mm3, mm2						;shift underflow to 2nd axis

		movq		mm2, mm3
		pminsw		mm3, mm1						;clamp maximum
		psubw		mm2, mm3						;mm2 = overflow
		pshufw		mm2, mm2, 11001001b				;mm2 = 0 | B->R | R->G | G->B 

		movq		mm0, x0000FFFFFFFFFFFF
		psubw		mm3, mm2						;shift overflow to 3rd axis

		pand		mm0, mm3
		psllq		mm3, [edx+0]					;apply left shift (dammit, why doesn't pshufw take a third register)

		psrlq		mm0, [edx+24]					;apply right shift

		paddw		mm0, mm3						;mm0 = final pixel!

		packuswb	mm0, mm0

		movd		[ecx+eax], mm0

		add			eax, 4
		jne			xloop_negative_shift

		add			ecx, [esp+32]
		dec			edi
		jne			yloop_negative_shift


		mov			esp, [esp+36]
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		emms
		ret			36
	}
}
#elif 0
static void __declspec(naked) __stdcall hsv_run_ISSE(uint32 *dst, ptrdiff_t dstpitch, unsigned w, unsigned h, int subshift, int sectors, int sat, int val, const unsigned short (*divtab)[4]) {
	static const __int64 x0000FFFFFFFFFFFF = 0x0000FFFFFFFFFFFF;
	static const __int64 rangefloor = 0;
	static const __int64 rangeceil = 0x000000FF00FF00FF;
	static const __int64 x0001w = 0x0001000100010001;
	static const __int64 x00ffw = 0x00ff00ff00ff00ff;
	static const __int64 x0100w = 0x0100010001000100;
	static const __int64 x0101w = 0x0101010101010101;
	static const __int64 x01ffw = 0x01ff01ff01ff01ff;
	static const __int64 x05faw = 0x00000000000005fa;
	static const __int64 x00000101d = 0x0000010100000101;

	static const __int64 x = 0x000001FE00FF0000;		// ramps up
	static const __int64 y = 0x000005FA04FB03FC;		// ramps down
	static const __int64 z = 0x0000000000FF00FF;

	static const __int64 huetab[]={
		0x00000000,		// impossible
		0x0000FF00,		// B
		0x00FF0000,		// G
		0x00000000,		// GB
		0x000000FF,		// R
		0x00000000,		// RB
		0x00000000,		// RG
		0x00000000,		// impossible
		0,			// impossible
		765,			// B
		255,			// G
		510,			// GB = 765 
		-255,			// R
		1020,			// RB = 1275
		0,			// RG = 255
		0,			// impossible
	};

	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			esi, [esp+12+16]				;esi = w
		mov			ecx, [esp+ 4+16]				;ecx = dst
		shl			esi, 2
		mov			edi, [esp+16+16]				;edi = h
		add			ecx, esi
		mov			edx, [esp+24+16]				;edx = sectors
		imul		edx, 510
		add			edx, [esp+20+16]
		sub			edx, 1530
		neg			esi
		mov			ebx, [esp+ 8+16]				;ebx = pitch
		mov			ebp, [esp+36+16]				;ebp = divtab
		pshufw		mm1, [esp+28+16], 0			;mm1 = sat | sat | sat | sat
		pshufw		mm2, [esp+32+16], 0			;mm2 = val | val | val | val
		movd		mm7, edx					;mm7 = subshift
		psllw		mm1, 6
		mov			ebx, [esp+8+16]
		mov			eax, esp
		sub			esp, 40
		and			esp, -8
		mov			[esp+36], eax
		mov			[esp+32], ebx
		movq		mm3, mm2
		pcmpeqb		mm5, mm5
		psraw		mm3, 15
		pxor		mm2, mm3
		psubw		mm2, mm3
		pxor		mm3, mm5
		psrlw		mm3, 7
		movq		mm4, x0100w
		psubw		mm4, mm2
		psllw		mm4, 5

		movq		[esp+24], mm3
		movq		[esp+0], mm4
		movq		[esp+16], mm1
		movd		eax, mm7
		cmp			eax, 1
		js			negative_shift
		sub			eax, 1530
negative_shift:
		cmp			eax, -1530
		jg			positive_shift
		add			eax, 1530
positive_shift:
		movd		mm7, eax
		movq		[esp+8], mm7

yloop:
		mov			eax, esi
xloop:
		movd		mm0, [ecx+eax]
		pxor		mm2, mm2

		punpcklbw	mm0, mm2
		pshufw		mm1, mm0, 11001001b				;mm1 = A | B | R | G

		movq		mm3, mm0						;mm3 = A | R | G | B
		pshufw		mm2, mm0, 11010010b				;mm2 = A | G | B | R

		pminsw		mm0, mm1						;mm0 = A |<RB|<RG|<GB
		pmaxsw		mm1, mm2						;mm1 = A |>GB|>RB|>RG

		pminsw		mm0, mm2						;mm0 = A |<RGB|<RGB|<RGB
		movq		mm2, mm3						;mm3 = A | R | G | B

		pmaxsw		mm1, mm3						;mm1 = A |>RGB|>RGB|>RGB

		movq		mm6, mm1

		psubw		mm6, mm0						;mm6 = range
		movd		ebx, mm6
		movq		mm7, mm0
		movzx		ebx, bx
		paddw		mm7, mm1						;mm7 = rangecen2x
		pxor		mm7, [esp+24]					;mm7 ^= invertmask
		psllw		mm7, 3
		pmulhuw		mm7, [esp]						;mm7 *= valscale
		pxor		mm7, [esp+24]					;mm7 = rangecen2x'

		movq		mm5, x01ffw
		psubw		mm5, mm7
		pminsw		mm5, mm7

		psllw		mm6, 2
		pmulhuw		mm6, [esp+16]					;mm6 = (range * sat) >> 16
		pminsw		mm6, mm5						;mm6 = newrange2x

		movq		mm5, mm7
		paddw		mm5, mm6						;mm5 = newtop2x
		psubw		mm7, mm6						;mm7 = newbottom2x

		psrlw		mm5, 1							;mm5 = newtop
		psrlw		mm7, 1							;mm7 = newbottom

		psubw		mm3, mm0						;mm3 = rgb - bottom
		movq		mm0, mm7
		movq		mm1, mm5

		psubw		mm5, mm7						;mm7 = newrange

		pmullw		mm3, x00ffw						;mm3 = (rgb - bottom) * 255
		movq		mm6, mm3

		pmullw		mm3, [ebp + ebx*8]
		pmulhuw		mm6, [ebp + ebx*8]
		psrlw		mm3, 15
		paddw		mm3, mm6						;mm3 = ((rgb - bottom) * 255 * divtab[range] + 0x8000) >> 16

		;convert RGB to hue angle
		pxor		mm7,mm7
		packuswb	mm3,mm7
		pcmpeqb		mm4,mm4
		movq		mm2,mm4
		pcmpeqb		mm4,mm3
		pmovmskb	ebx,mm4
		pxor		mm3,[huetab+ebx*8]
		psadbw		mm3,mm7
		paddw		mm3,[huetab+ebx*8+64]

		;rotate hue angle and wrap into 0-1529
		paddw		mm3,[esp+8]
		pcmpgtw		mm2,mm3
		pand		mm2,x05faw
		paddw		mm3,mm2

		;convert hue angle back to RGB
		pshufw		mm3, mm3, 0
		movq		mm4, y
		psubw		mm4, mm3
		psubw		mm3, x
		packuswb	mm3, mm3
		packuswb	mm4, mm4
		pand		mm3, mm4
		pxor		mm7, mm7
		pxor		mm3, z
		punpcklbw	mm3, mm7

		;scale and write out
		paddw		mm5, x0001w
		psllw		mm5, 8
		pmulhuw		mm3, mm5
		paddw		mm3, mm0
		packuswb	mm3, mm3
		movd		[ecx+eax], mm3

		add			eax, 4
		jne			xloop

		add			ecx, [esp+32]
		dec			edi
		jne			yloop

		mov			esp, [esp+36]
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		emms
		ret			36
	}
}
#endif

#if 0
static void __declspec(naked) __stdcall hsv_run_MMX(uint32 *dst, ptrdiff_t dstpitch, unsigned w, unsigned h, int subshift, int sectors, int sat, int val, const unsigned short (*divtab)[4]) {
	static const __int64 x0000FFFFFFFFFFFF = 0x0000FFFFFFFFFFFF;
	static const __int64 shifts[2][3]={
		{48,32,16}, {0,16,32}
	};

	static const __int64 rangefloor = 0;
	static const __int64 rangeceil = 0x000000FF00FF00FF;
	static const __int64 x0001w = 0x0001000100010001;
	static const __int64 x0100w = 0x0100010001000100;
	static const __int64 x0101w = 0x0101010101010101;
	static const __int64 x01ffw = 0x01ff01ff01ff01ff;
	static const __int64 x00000101d = 0x0000010100000101;
	static const __int64 x00008000d = 0x0000800000008000;

	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			esi, [esp+12+16]				;esi = w
		mov			ecx, [esp+ 4+16]				;ecx = dst
		shl			esi, 2
		mov			edi, [esp+16+16]				;edi = h
		add			ecx, esi
		mov			edx, [esp+24+16]				;edx = sectors
		neg			esi
		mov			ebx, [esp+ 8+16]				;ebx = pitch
		mov			ebp, [esp+36+16]				;ebp = divtab
		lea			edx, [edx*8+shifts]				;edx = ptr to shift table entry
		pshufw		mm1, [esp+28+16], 0			;mm1 = sat | sat | sat | sat
		pshufw		mm2, [esp+32+16], 0			;mm2 = val | val | val | val
		pshufw		mm7, [esp+20+16], 0			;mm6 = subshift | subshift | subshift | subshift
		psllw		mm1, 5
		mov			ebx, [esp+8+16]
		mov			eax, esp
		sub			esp, 40
		and			esp, -8
		mov			[esp+36], eax
		mov			[esp+32], ebx
		movq		mm3, mm2
		pcmpeqb		mm5, mm5
		psraw		mm3, 15
		pxor		mm2, mm3
		psubw		mm2, mm3
		pxor		mm3, mm5
		psrlw		mm3, 7
		movq		mm4, x0100w
		psubw		mm4, mm2
		psllw		mm4, 5

		movq		[esp+24], mm3
		movq		[esp+0], mm4
		movq		[esp+16], mm1
		movd		eax, mm7
		or			eax, eax
		js			negative_shift

		movq		[esp+8], mm7

yloop_positive_shift:
		mov			eax, esi
xloop_positive_shift:
		movd		mm0, [ecx+eax]
		pxor		mm2, mm2

		punpcklbw	mm0, mm2
		pshufw		mm1, mm0, 11001001b				;mm1 = A | B | R | G

		movq		mm3, mm0						;mm3 = A | R | G | B
		pshufw		mm2, mm0, 11010010b				;mm2 = A | G | B | R

		pminsw		mm0, mm1						;mm0 = A |<RB|<RG|<GB
		pmaxsw		mm1, mm2						;mm1 = A |>GB|>RB|>RG

		pminsw		mm0, mm2						;mm0 = A |<RGB|<RGB|<RGB
		movq		mm2, mm3						;mm3 = A | R | G | B

		pmaxsw		mm1, mm3						;mm1 = A |>RGB|>RGB|>RGB
		pcmpeqw		mm2, mm1						;mm2 = 1 | R=Max | G=Max | B=Max

		movq		mm6, mm1
		pshufw		mm4, mm2, 11010010b				;mm4 = 1 | G=Max | B=Max | R=Max

		psubw		mm6, mm0						;mm6 = range
		movd		ebx, mm6
		movq		mm7, mm0
		movzx		ebx, bx
		paddw		mm7, mm1						;mm7 = rangecen2x
		pxor		mm7, [esp+24]					;mm7 ^= invertmask
		psllw		mm7, 3
		pmulhuw		mm7, [esp]						;mm7 *= valscale
		pxor		mm7, [esp+24]					;mm7 = rangecen2x'

		movq		mm5, x01ffw
		psubw		mm5, mm7
		pminsw		mm5, mm7

		psllw		mm6, 3
		pmulhuw		mm6, [esp+16]					;mm6 = (range * sat) >> 16
		pminsw		mm6, mm5						;mm6 = newrange2x

		movq		mm5, mm7
		paddw		mm5, mm6						;mm5 = newtop2x
		psubw		mm7, mm6						;mm7 = newbottom2x

		psrlw		mm5, 1							;mm5 = newtop
		psrlw		mm7, 1							;mm7 = newbottom

		psubw		mm3, mm0						;mm3 = rgb - bottom
		movq		mm0, mm7
		movq		mm1, mm5

		psubw		mm5, mm7						;mm7 = newrange

		pmullw		mm3, mm5						;mm3 = (rgb - bottom) * newrange
		movq		mm6, mm3

		pmaddwd		mm3, [ebp + ebx*8]
		paddd		mm3, x00008000d
		psrld		mm3, 16
		movq		mm2, mm3
		psrlq		mm3, 16
		por			mm3, mm2						;mm3 = ((rgb - bottom) * newrange * divtab[range] + 0x8000) >> 16
		paddw		mm3, mm0

		pmullw		mm5, [esp+8]					;mm6 = subshift (scale) newrange
		psrlw		mm5, 8

		pand		mm2, mm5

		pandn		mm4, mm2						;mm4 = 0 | (R=Max && G<Max)?delta | (G=Max && B<Max)?delta | (B=Max && R<Max)?delta

		pshufw		mm4, mm4, 11010010b				;mm4 = 0 | (G=Max && B<Max)?delta | (B=Max && R<Max)?delta | (R=Max && G<Max)?delta

		psubw		mm3, mm4						;decrease 1st axis

		movq		mm2, mm3
		pmaxsw		mm3, mm0						;clamp minimum

		psubw		mm2, mm3						;mm2 = underflow

		pshufw		mm2, mm2, 11010010b				;mm2 = 0 | G->R | B->G | R->B

		psubw		mm3, mm2						;shift underflow to 2nd axis

		movq		mm2, mm3
		pminsw		mm3, mm1						;clamp maximum

		psubw		mm2, mm3						;mm2 = overflow

		pshufw		mm2, mm2, 11010010b				;mm2 = 0 | G->R | B->G | R->B

		movq		mm0, x0000FFFFFFFFFFFF
		psubw		mm3, mm2						;shift overflow to 3rd axis

		pand		mm0, mm3
		psllq		mm3, [edx+0]					;apply left shift (dammit, why doesn't pshufw take a third register)

		psrlq		mm0, [edx+24]					;apply right shift

		paddw		mm0, mm3						;mm0 = final pixel!

		packuswb	mm0, mm0

		movd		[ecx+eax], mm0

		add			eax, 4
		jne			xloop_positive_shift

		add			ecx, [esp+32]
		dec			edi
		jne			yloop_positive_shift

		mov			esp, [esp+36]
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		emms
		ret			36

negative_shift:
		pxor		mm0, mm0
		psubw		mm0, mm7
		movq		[esp+8], mm0

yloop_negative_shift:
		mov			eax, esi
xloop_negative_shift:
		movd		mm0, [ecx+eax]
		pxor		mm2, mm2

		punpcklbw	mm0, mm2

		pand		mm0, x0000FFFFFFFFFFFF			;mm0 = ? | R | G | B

		movq		mm1, mm0
		movq		mm2, mm0
		movq		mm3, mm0
		movq		mm4, mm0
		movq		mm5, mm0
		psrlq		mm2, 16
		psllq		mm3, 32
		psrlq		mm4, 32
		psllq		mm5, 16
		paddw		mm2, mm3						;mm2 = ? | B | R | G
		paddw		mm4, mm5						;mm4 = ? | G | B | R

		movq		mm3, mm0

		;sort	mm0<->mm2
		movq		mm1, mm0
		psubusw		mm1, mm2
		psubw		mm0, mm1
		paddw		mm2, mm1
		
		;sort	mm2<->mm4
		movq		mm1, mm2
		psubusw		mm1, mm4
		psubw		mm2, mm1
		paddw		mm4, mm1

		;sort	mm0<->mm2
		movq		mm1, mm0
		psubusw		mm1, mm2
		psubw		mm0, mm1
		paddw		mm2, mm1

		movq		mm1, mm4

		movq		mm2, mm3						;mm3 = A | R | G | B
		pcmpeqw		mm2, mm1						;mm2 = 1 | R=Max | G=Max | B=Max

		movq		mm6, mm1

		psubw		mm6, mm0						;mm6 = range
		movd		ebx, mm6
		movq		mm7, mm0
		movzx		ebx, bx
		paddw		mm7, mm1						;mm7 = rangecen2x
		pxor		mm7, [esp+24]					;mm7 ^= invertmask
		psllw		mm7, 3
		pmulhw		mm7, [esp]						;mm7 *= valscale
		pxor		mm7, [esp+24]					;mm7 = rangecen2x'

		movq		mm5, mm7
		psllw		mm5, 7
		psraw		mm5, 15
		pand		mm5, x01ffw
		pxor		mm5, mm7

		psllw		mm6, 3
		pmulhw		mm6, [esp+16]					;mm6 = (range * sat) >> 16
		movq		mm4, mm6
		psubusw		mm4, mm5
		psubw		mm6, mm4						;mm6 = newrange2x

		movq		mm5, mm7
		paddw		mm5, mm6						;mm5 = newtop2x
		psubw		mm7, mm6						;mm7 = newbottom2x

		psrlw		mm5, 1							;mm5 = newtop
		psrlw		mm7, 1							;mm7 = newbottom

		psubw		mm3, mm0						;mm3 = rgb - bottom
		movq		mm0, mm7
		movq		mm1, mm5

		psubw		mm5, mm7						;mm7 = newrange

		pmullw		mm3, mm5						;mm3 = (rgb - bottom) * newrange

		pmullw		mm5, [esp+8]					;mm6 = subshift (scale) newrange
		psrlw		mm5, 8

		pand		mm2, x0000FFFFFFFFFFFF
		movq		mm6, mm2
		movq		mm4, mm2
		psllq		mm6, 32
		psrlq		mm4, 16
		paddw		mm4, mm6						;mm4 = 1 | B=Max | R=Max | G=Max

		pand		mm2, mm5
		pandn		mm4, mm2						;mm4 = 0 | (R=Max && B<Max)?delta | (G=Max && R<Max)?delta | (B=Max && G<Max)?delta
		pand		mm4, x0000FFFFFFFFFFFF

		movq		mm6, [ebp + ebx*8]
		movq		mm7, mm3
		pmullw		mm7, mm6
		movq		mm2, mm3
		pmulhw		mm6, mm3
		movq		mm5, [ebp + ebx*8]
		psraw		mm2, 15
		psraw		mm5, 15
		pand		mm2, [ebp + ebx*8]
		pand		mm3, mm5
		paddw		mm3, mm2
		psrlw		mm7, 15
		paddw		mm3, mm7
		paddw		mm3, mm6						;mm3 = ((rgb - bottom) * newrange * divtab[range] + 0x8000) >> 16

		paddw		mm3, mm0

		movq		mm2, mm4
		psllq		mm4, 32
		psrlq		mm2, 16
		paddw		mm4, mm2						;mm4 = 0 | (B=Max && G<Max)?delta | (R=Max && B<Max)?delta | (G=Max && R<Max)?delta
		pand		mm4, x0000FFFFFFFFFFFF

		;round-robin stage 1
		movq		mm2, mm3
		psubusw		mm3, mm4						;decrease 1st axis
		psubusw		mm3, mm0
		psubw		mm2, mm4
		paddw		mm3, mm0
		psubw		mm2, mm3						;mm2 = underflow (0 to -n)
		movq		mm4, mm2
		psllq		mm2, 32
		psrlq		mm4, 16
		psubw		mm3, mm2
		psubw		mm3, mm4
		pand		mm3, x0000FFFFFFFFFFFF

		;round-robin stage 2
		movq		mm2, mm3
		psubusw		mm2, mm1						;mm2 = overflow (0 to n)
		movq		mm4, mm2
		psubw		mm3, mm2
		psllq		mm2, 32
		psrlq		mm4, 16
		psubw		mm3, mm2
		psubw		mm3, mm4
		pand		mm3, x0000FFFFFFFFFFFF

		movq		mm0, mm3

		psllq		mm3, [edx+0]					;apply left shift (dammit, why doesn't pshufw take a third register)

		psrlq		mm0, [edx+24]					;apply right shift

		paddw		mm0, mm3						;mm0 = final pixel!

		packuswb	mm0, mm0

		movd		[ecx+eax], mm0

		add			eax, 4
		jne			xloop_negative_shift

		add			ecx, [esp+32]
		dec			edi
		jne			yloop_negative_shift


		mov			esp, [esp+36]
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		emms
		ret			36
	}
}
#elif 0
static void __declspec(naked) __stdcall hsv_run_MMX(uint32 *dst, ptrdiff_t dstpitch, unsigned w, unsigned h, int subshift, int sectors, int sat, int val, const unsigned short (*divtab)[4]) {
	static const __int64 x0000FFFFFFFFFFFF = 0x0000FFFFFFFFFFFF;
	static const __int64 rangefloor = 0;
	static const __int64 rangeceil = 0x000000FF00FF00FF;
	static const __int64 x0001w = 0x0001000100010001;
	static const __int64 x00ffw = 0x00ff00ff00ff00ff;
	static const __int64 x0100w = 0x0100010001000100;
	static const __int64 x0101w = 0x0101010101010101;
	static const __int64 x01ffw = 0x01ff01ff01ff01ff;
	static const __int64 x05faw = 0x00000000000005fa;
	static const __int64 x00000101d = 0x0000010100000101;

	static const __int64 x = 0x000001FE00FF0000;		// ramps up
	static const __int64 y = 0x000005FA04FB03FC;		// ramps down
	static const __int64 z = 0x0000000000FF00FF;
	static const __int64 hdiff_correct = 0x000000ff02fd04fb;

	static const __int64 huetab[]={
		0x00000000,		// impossible
		0x0000FF00,		// B
		0x00FF0000,		// G
		0x00000000,		// GB
		0x000000FF,		// R
		0x00000000,		// RB
		0x00000000,		// RG
		0x00000000,		// impossible
		0,			// impossible
		765,			// B
		255,			// G
		510,			// GB = 765 
		-255,			// R
		1020,			// RB = 1275
		0,			// RG = 255
		0,			// impossible
	};

	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			esi, [esp+12+16]				;esi = w
		mov			ecx, [esp+ 4+16]				;ecx = dst
		shl			esi, 2
		mov			edi, [esp+16+16]				;edi = h
		add			ecx, esi
		mov			edx, [esp+24+16]				;edx = sectors
		imul		edx, 510
		add			edx, [esp+20+16]
		sub			edx, 1530
		neg			esi
		mov			ebx, [esp+ 8+16]				;ebx = pitch
		mov			ebp, [esp+36+16]				;ebp = divtab
		movd		mm1, [esp+28+16]			;mm1 = sat
		punpcklwd	mm1, mm1
		punpckldq	mm1, mm1
		movd		mm2, [esp+32+16]			;mm2 = val
		punpcklwd	mm2, mm2
		punpckldq	mm2, mm2
		movd		mm7, edx					;mm7 = subshift
		psllw		mm1, 5
		mov			ebx, [esp+8+16]
		mov			eax, esp
		sub			esp, 40
		and			esp, -8
		mov			[esp+36], eax
		mov			[esp+32], ebx
		movq		mm3, mm2
		pcmpeqb		mm5, mm5
		psraw		mm3, 15
		pxor		mm2, mm3
		psubw		mm2, mm3
		pxor		mm3, mm5
		psrlw		mm3, 7
		movq		mm4, x0100w
		psubw		mm4, mm2
		psllw		mm4, 5

		movq		[esp+24], mm3
		movq		[esp+0], mm4
		movq		[esp+16], mm1
		movd		eax, mm7
		cmp			eax, 1
		js			negative_shift
		sub			eax, 1530
negative_shift:
		cmp			eax, -1530
		jg			positive_shift
		add			eax, 1530
positive_shift:
		movd		mm7, eax
		movq		[esp+8], mm7

yloop:
		mov			eax, esi
xloop:
		movd		mm0, [ecx+eax]
		pxor		mm2, mm2

		punpcklbw	mm0, mm2

		pand		mm0, x0000FFFFFFFFFFFF			;mm0 = ? | R | G | B

		movq		mm1, mm0
		movq		mm2, mm0
		movq		mm3, mm0
		movq		mm4, mm0
		movq		mm5, mm0
		psrlq		mm2, 16
		psllq		mm3, 32
		psrlq		mm4, 32
		psllq		mm5, 16
		paddw		mm2, mm3						;mm2 = ? | B | R | G
		paddw		mm4, mm5						;mm4 = ? | G | B | R

		movq		mm3, mm0

		;sort	mm0<->mm2
		movq		mm1, mm0
		psubusw		mm1, mm2
		psubw		mm0, mm1
		paddw		mm2, mm1
		
		;sort	mm2<->mm4
		movq		mm1, mm2
		psubusw		mm1, mm4
		psubw		mm2, mm1
		paddw		mm4, mm1

		;sort	mm0<->mm2
		movq		mm1, mm0
		psubusw		mm1, mm2
		psubw		mm0, mm1
		paddw		mm2, mm1

		movq		mm1, mm4

		movq		mm6, mm1

		psubw		mm6, mm0						;mm6 = range
		movd		ebx, mm6
		movq		mm7, mm0
		movzx		ebx, bx
		paddw		mm7, mm1						;mm7 = rangecen2x
		pxor		mm7, [esp+24]					;mm7 ^= invertmask
		psllw		mm7, 3
		pmulhw		mm7, [esp]						;mm7 *= valscale
		pxor		mm7, [esp+24]					;mm7 = rangecen2x'

		movq		mm5, mm7
		psllw		mm5, 7
		psraw		mm5, 15
		pand		mm5, x01ffw
		pxor		mm5, mm7

		psllw		mm6, 3
		pmulhw		mm6, [esp+16]					;mm6 = (range * sat) >> 16
		movq		mm4, mm6
		psubusw		mm4, mm5
		psubw		mm6, mm4						;mm6 = newrange2x

		movq		mm5, mm7
		paddw		mm5, mm6						;mm5 = newtop2x
		psubw		mm7, mm6						;mm7 = newbottom2x

		psrlw		mm5, 1							;mm5 = newtop
		psrlw		mm7, 1							;mm7 = newbottom

		psubw		mm3, mm0						;mm3 = rgb - bottom
		movq		mm0, mm7
		movq		mm1, mm5

		pmullw		mm3, x00ffw
		movq		mm6, [ebp + ebx*8]
		movq		mm7, mm3
		pmullw		mm7, mm6
		movq		mm2, mm3
		pmulhw		mm6, mm3
		movq		mm5, [ebp + ebx*8]
		psraw		mm2, 15
		psraw		mm5, 15
		pand		mm2, [ebp + ebx*8]
		pand		mm3, mm5
		paddw		mm3, mm2
		psrlw		mm7, 15
		paddw		mm3, mm7
		paddw		mm3, mm6						;mm3 = ((rgb - bottom) * newrange * divtab[range] + 0x8000) >> 16

		movq		mm4, mm3
		movq		mm5, mm3
		psllq		mm4, 16
		psrlq		mm5, 32
		paddw		mm4, mm5						;mm4 = ? | G | B | R
		movq		mm5, mm3
		movq		mm6, mm3
		psllq		mm5, 32
		psrlq		mm6, 16
		paddw		mm5, mm6						;mm5 = ? | B | R | G

		pcmpeqw		mm3, x00ffw						;mm3 = ? | Rmax | Gmax | Bmax
		movq		mm2, mm4
		pcmpeqw		mm4, x00ffw						;mm4 = ? | Gmax | Bmax | Rmax
		pandn		mm4, mm3						;mm4 = ? | Rdom | Gdom | Bdom
		psubw		mm2, mm5						;mm2 = ? | G-B | B-R | R-G
		paddw		mm2, hdiff_correct
		pand		mm2, mm4
		movq		mm3, mm2
		psrlq		mm2, 16
		paddw		mm3, mm2
		psrlq		mm2, 16
		paddw		mm3, mm2
		
		;rotate hue angle and wrap into 0-1529
		pcmpeqb		mm2, mm2
		paddw		mm3,[esp+8]
		pcmpgtw		mm2,mm3
		pand		mm2,x05faw
		paddw		mm3,mm2

		;convert hue angle back to RGB
		pshufw		mm3, mm3, 0
		movq		mm4, y
		psubw		mm4, mm3
		psubw		mm3, x
		packuswb	mm3, mm3
		packuswb	mm4, mm4
		pand		mm3, mm4
		pxor		mm7, mm7
		pxor		mm3, z
		punpcklbw	mm3, mm7

		;scale and write out
		psubw		mm1, mm0
		paddw		mm1, x0001w
		psllw		mm1, 6
		psllw		mm3, 2
		pmulhw		mm3, mm1
		paddw		mm3, mm0
		packuswb	mm3, mm3
		movd		[ecx+eax], mm3

		add			eax, 4
		jne			xloop

		add			ecx, [esp+32]
		dec			edi
		jne			yloop


		mov			esp, [esp+36]
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		emms
		ret			36
	}
}
#endif

#if 0
static void hsv_run_scalar(uint32 *dst, ptrdiff_t dstpitch, unsigned w, unsigned h, int subshift, int sectors, int sat, int val, const unsigned short (*divtab)[4]) {
	const int rshift = sectors * 8;
	const int lshift = 24 - sectors * 8;

	for(int y=0; y<h; ++y) {
		for(int x=0; x<w; ++x) {
			uint32 px = dst[x];
			int r = (px>>16)&0xff;
			int g = (px>> 8)&0xff;
			int b = (px    )&0xff;

			int top			= r;
			int bottom		= g;

			if (top < bottom) {
				top = g;
				bottom = r;
			}

			if (b > top)
				top = b;

			if (b < bottom)
				bottom = b;

			int range		= top - bottom;
			int rangecen2x	= top + bottom;

			if (val > 0)
				rangecen2x = (((rangecen2x ^ 0x1ff) * (256 - val) + 128)>>8) ^ 0x1ff;
			else
				rangecen2x = (rangecen2x * (256 + val) + 128)>>8;

			int newrangemax2x	= min(rangecen2x, 511 - rangecen2x);
			int newrange2x		= min((range * sat + 128)>>8, newrangemax2x);
			int newbottom	= (rangecen2x - newrange2x)>>1;
			int newtop		= (rangecen2x + newrange2x)>>1;
			int newrange	= newtop - newbottom;

			if (!range || newbottom >= newtop) {
				px = newbottom * 0x010101;
			} else {
				r = ((unsigned)(r-bottom) * newrange * divtab[range][0] + 0x8000) >> 16;
				g = ((unsigned)(g-bottom) * newrange * divtab[range][0] + 0x8000) >> 16;
				b = ((unsigned)(b-bottom) * newrange * divtab[range][0] + 0x8000) >> 16;

				if (subshift<0) {
					int rangeinc = (128 - newrange * subshift) >> 8;

						 if (r == newrange && b<newrange) { g -= rangeinc; if (g<0) { b -= g; g=0; if (b>newrange) { r=newrange*2-b; b=newrange; }} }
					else if (g == newrange && r<newrange) { b -= rangeinc; if (b<0) { r -= b; b=0; if (r>newrange) { g=newrange*2-r; r=newrange; }} }
					else if (b == newrange && g<newrange) { r -= rangeinc; if (r<0) { g -= r; r=0; if (g>newrange) { b=newrange*2-g; g=newrange; }} }
				} else {
					int rangeinc = (newrange * subshift + 128) >> 8;

						 if (r == newrange && g<newrange) { b -= rangeinc; if (b<0) { g -= b; b=0; if (g>newrange) { r=newrange*2-g; g=newrange; }} }
					else if (g == newrange && b<newrange) { r -= rangeinc; if (r<0) { b -= r; r=0; if (b>newrange) { g=newrange*2-b; b=newrange; }} }
					else if (b == newrange && r<newrange) { g -= rangeinc; if (g<0) { r -= g; g=0; if (r>newrange) { b=newrange*2-r; r=newrange; }} }
				}
				px = ((r<<16) + (g<<8) + b) + 0x010101*newbottom;
			}

			dst[x] = (px<<lshift) | (px>>rshift);
		}

		ptr_increment(dst, dstpitch);
	}
}
#endif

///////////////////////////////////////////////////////////////////////////
//
//	scalar implementation
//
///////////////////////////////////////////////////////////////////////////

void HSVFilterData::RunScalar(uint32 *dst, ptrdiff_t dstpitch, uint32 w, uint32 h, int subshift, int sectors) {
	int totalshift = sectors * 510 + subshift;

	if (totalshift < 0)
		totalshift += 1530;

	if (totalshift >= 1530)
		totalshift -= 1530;

	int hexshifts = totalshift / 255;
	int subshifts = totalshift % 255;

	for(uint32 y=0; y<h; ++y) {
		for(uint32 x=0; x<w; ++x) {
			uint32 px;// = dst[x];
			int r = ((unsigned char *)&dst[x])[2];
			int g = ((unsigned char *)&dst[x])[1];
			int b = ((unsigned char *)&dst[x])[0];

			int top			= r;
			int bottom		= g;
			int huesector	= 1;
			int huedelta	= g-b;

			if (top < bottom) {
				top = g;
				bottom = r;
				huesector = 3;
				huedelta = b-r;
			}

			if (b > top) {
				top = b;
				huesector = 5;
				huedelta = r-g;
			}

			if (b < bottom)
				bottom = b;

			int range		= top - bottom;
			int rangecen2x	= top + bottom;

			int huedeltaneg = huedelta>>31;
			huesector += huedeltaneg;
			huedelta += range & huedeltaneg;

			int newrangemax2x	= valtab[rangecen2x][1];
			rangecen2x = valtab[rangecen2x][0];

			int newrange2x		= sattab[range];
			
			if (newrange2x > newrangemax2x)
				newrange2x = newrangemax2x;

			int newbottom	= (rangecen2x - newrange2x)>>1;
			int newtop		= (rangecen2x + newrange2x)>>1;
			int newrange	= newtop - newbottom;

			px = graytab[newbottom];
			if (range) {
				huedelta = (huedelta * divtab[range] + 0x8000) >> 16;
				huesector += hexshifts;
				huedelta += subshifts;
				if (huedelta >= 255) {
					huedelta -= 255;
					++huesector;
				}

				unsigned subval = (huedelta * newrange + 128) >> 8;

				static const unsigned tab[18][2]={
					{0xff00ff,-0x000001},
					{0xff0000,+0x000100},
					{0xffff00,-0x010000},
					{0x00ff00,+0x000001},
					{0x00ffff,-0x000100},
					{0x0000ff,+0x010000},
					{0xff00ff,-0x000001},
					{0xff0000,+0x000100},
					{0xffff00,-0x010000},
					{0x00ff00,+0x000001},
					{0x00ffff,-0x000100},
					{0x0000ff,+0x010000},
					{0xff00ff,-0x000001},
					{0xff0000,+0x000100},
					{0xffff00,-0x010000},
					{0x00ff00,+0x000001},
					{0x00ffff,-0x000100},
					{0x0000ff,+0x010000},
				};

				px += (tab[huesector][0]&graytab[newrange]) + subval*tab[huesector][1];
			}

			dst[x] = px;
		}

		ptr_increment(dst, dstpitch);
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	supporting code
//
///////////////////////////////////////////////////////////////////////////

void HSVFilterData::RebuildSVTables() {
	int val2 = (val+128) >> 8;
	int valinvmask = val2>0 ? 0x1ff : 0;
	int valscale = val2>0 ? (256 - val2) : (256+val2);
	int i;

	for(i=0; i<512; ++i) {
		valtab[i][0] = (((i ^ valinvmask) * valscale + 128)>>8) ^ valinvmask;
		valtab[i][1] = valtab[i][0] >= 256 ? 511 - valtab[i][0] : valtab[i][0];
	}

	for(i=0; i<256; ++i) {
		sattab[i] = (i * sat + 32768) >> 16;
	}
}

static int hsv_start(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	HSVFilterData *mfd = (HSVFilterData *)fa->filter_data;
	int i;

	mfd->divtab[0] = mfd->graytab[0] = 0;
	for(i=1; i<256; ++i) {
		mfd->divtab[i] = 0xFF0000/i;
		mfd->graytab[i] = 0x010101*i;
	}

	mfd->RebuildSVTables();

	return 0;
}

static int hsv_stop(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	return 0;
}

int hsv_run(const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {	
	HSVFilterData *mfd = (HSVFilterData *)fa->filter_data;

	if (!fa->dst.w || !fa->dst.h)
		return 0;

	int phase = ((mfd->hue & 0xffff) * 6) >> 8;
	int sectors = ((phase+256) / 512) % 3;
	int shift = phase%512;
	if (shift >= 256)
		shift -= 512;

//	int sat = (mfd->sat+128) >> 8;
//	int val = (mfd->val+128) >> 8;

	mfd->RunScalar(fa->dst.data, fa->dst.pitch, fa->dst.w, fa->dst.h, shift, sectors);
//	hsv_run_MMX(fa->dst.data, fa->dst.pitch, fa->dst.w, fa->dst.h, shift, sectors, sat, val, mfd->divtab);
//	hsv_run_ISSE(fa->dst.data, fa->dst.pitch, fa->dst.w, fa->dst.h, shift, sectors, sat, val, mfd->divtab);

	return 0;
}

long hsv_param(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	pxldst.pitch = pxlsrc.pitch;
	return FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

//////////////////

static int hsv_init(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	HSVFilterData *mfd = (HSVFilterData *)fa->filter_data;
	mfd->hue = 0;
	mfd->sat = 65536;
	mfd->val = 0;
	return 0;
}

static INT_PTR CALLBACK hsvDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	HSVFilterData *mfd = (HSVFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			{
				HWND hwnd;
				mfd = (HSVFilterData *)lParam;
				SetWindowLongPtr(hDlg, DWLP_USER, (LPARAM)mfd);

				hwnd = GetDlgItem(hDlg, IDC_HUE);
				SendMessage(hwnd, TBM_SETRANGEMIN, (WPARAM)FALSE, 0);
				SendMessage(hwnd, TBM_SETRANGEMAX, (WPARAM)FALSE, 4096);
				SendMessage(hwnd, TBM_SETPOS, (WPARAM)TRUE, ((mfd->hue+0x8008) & 0xffff) >> 4);
				SendMessage(hDlg, WM_HSCROLL, 0, (LPARAM)hwnd);		// force label update

				hwnd = GetDlgItem(hDlg, IDC_SATURATION);
				SendMessage(hwnd, TBM_SETRANGEMIN, (WPARAM)FALSE, 0);
				SendMessage(hwnd, TBM_SETRANGEMAX, (WPARAM)FALSE, 8192);
				SendMessage(hwnd, TBM_SETPOS, (WPARAM)TRUE, (mfd->sat+8) >> 4);
				SendMessage(hDlg, WM_HSCROLL, 0, (LPARAM)hwnd);		// force label update

				hwnd = GetDlgItem(hDlg, IDC_VALUE);
				SendMessage(hwnd, TBM_SETRANGEMIN, (WPARAM)FALSE, 0);
				SendMessage(hwnd, TBM_SETRANGEMAX, (WPARAM)FALSE, 8192);
				SendMessage(hwnd, TBM_SETPOS, (WPARAM)TRUE, ((mfd->val+8)>>4) + 4096);
				SendMessage(hDlg, WM_HSCROLL, 0, (LPARAM)hwnd);		// force label update

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
            }
            break;

		case WM_HSCROLL:
			if (lParam) {
				HWND hwndSlider = (HWND)lParam;
				int val = SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
				int newval;
				char buf[64];
				bool redo = false;

				switch(GetWindowLong(hwndSlider, GWL_ID)) {
				case IDC_HUE:
					sprintf(buf, "%+.1f%s", (val-0x800) * (360.0 / 4096.0), VDTextWToA(L"\u00B0").c_str());
					SetDlgItemText(hDlg, IDC_STATIC_HUE, buf);
					newval = ((val+0x800)<<4) & 0xfff0;
					if (newval != mfd->hue)
						redo = true;
					mfd->hue = newval;
					break;
				case IDC_SATURATION:
					sprintf(buf, "x%.1f%%", val * (100.0 / 4096.0));
					SetDlgItemText(hDlg, IDC_STATIC_SATURATION, buf);
					if (val != mfd->sat) {
						mfd->sat = val << 4;
						redo = true;
					}
					break;
				case IDC_VALUE:
					sprintf(buf, "%+.1f%%", val * (100. / 4096.0) - 100.0);
					SetDlgItemText(hDlg, IDC_STATIC_VALUE, buf);
					newval = (val<<4) - 65536;
					if (newval != mfd->val) {
						mfd->val = newval;
						redo = true;
					}
					break;
				}

				if (redo) {
					mfd->RebuildSVTables();
					mfd->ifp->RedoFrame();
				}
			}
			break;
    }
    return FALSE;
}

static int hsv_config(VDXFilterActivation *fa, const VDXFilterFunctions *ff, VDXHWND hWnd) {
	HSVFilterData *mfd = (HSVFilterData *)fa->filter_data;
	HSVFilterData mfd2 = *mfd;
	int ret;

	mfd->ifp = fa->ifp;

	ret = DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_HSV), (HWND)hWnd, hsvDlgProc, (LPARAM)mfd);

	if (ret)
		*mfd = mfd2;

	return ret;
}

static void hsv_string2(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int maxlen) {
	HSVFilterData *mfd = (HSVFilterData *)fa->filter_data;

	_snprintf(buf, maxlen, " (h%+.0f%s, sx%.0f%%, v%+.0f%%)", (short)mfd->hue * (360.0 / 65536.0), VDTextWToA(L"\u00B0").c_str(), mfd->sat * (100.0 / 65536.0), mfd->val * (100.0 / 65536.0));
}

static void hsv_script_config(IVDXScriptInterpreter *isi, void *lpVoid, VDXScriptValue *argv, int argc) {
	VDXFilterActivation *fa = (VDXFilterActivation *)lpVoid;
	HSVFilterData *mfd = (HSVFilterData *)fa->filter_data;

	mfd->hue	= argv[0].asInt();
	mfd->sat	= argv[1].asInt();
	mfd->val	= argv[2].asInt();
}

static VDXScriptFunctionDef hsv_func_defs[]={
	{ (VDXScriptFunctionPtr)hsv_script_config, "Config", "0iii" },
	{ NULL },
};

static VDXScriptObject hsv_obj={
	NULL, hsv_func_defs
};

static bool hsv_script_line(VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int buflen) {
	HSVFilterData *mfd = (HSVFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d,%d,%d)", mfd->hue, mfd->sat, mfd->val);

	return true;
}

extern const VDXFilterDefinition g_VDVFHSV={
	0,0,NULL,
	"HSV adjust",
	"Adjusts color, saturation, and brightness of video.\n\n",
	NULL,NULL,
	sizeof(HSVFilterData),
	hsv_init,
	NULL,
	hsv_run,
	hsv_param,
	hsv_config,
	NULL,
	hsv_start,
	hsv_stop,
	&hsv_obj,
	hsv_script_line,

	hsv_string2
};
