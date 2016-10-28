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

#include <vd2/system/cpuaccel.h>
#include <vd2/Kasumi/pixmap.h>
#include "Blur.h"

///////////////////////////////////////////////////////////////////////////

#ifdef _M_IX86
	#define USE_ASM
#endif

extern HINSTANCE g_hInst;

#ifdef _MSC_VER
#pragma warning(disable: 4799)		// function has no EMMS instruction
#endif

///////////////////////////////////////////////////////////////////////////

typedef unsigned char byte;

struct MyFilterData {
	IVDXFilterPreview *ifp;
	int		grad_threshold;
	byte	*sum_row[5];
	uint32	*avg_row[5];
	VDPixmap	vbBlur;
	VEffect	*effBlur;
	void	*pBlurBitmap;
	bool	fBlurPass;
	int		*square_table;
};

	// codes[i] = yyy000xx
	//
	// y = # of bits set in bits 0-4 of i
	// x = # of bits set in bits 1-3 of i

static const byte codes[]={
	0x00, 0x20, 0x21, 0x41, 0x21, 0x41, 0x42, 0x62,
	0x21, 0x41, 0x42, 0x62, 0x42, 0x62, 0x63, 0x83,
	0x20, 0x40, 0x41, 0x61, 0x41, 0x61, 0x62, 0x82,
	0x41, 0x61, 0x62, 0x82, 0x62, 0x82, 0x83, 0xa3,
};

#ifdef USE_ASM
static void __declspec(naked) filtrow_1_mmx(byte *sum, uint32 *src, long width, const long pitch, const long thresh) {
	__asm {
		push	ebx
		push	esi
		push	edi
		push	ebp

		mov			edi,[esp+16+4]
		mov			esi,[esp+16+8]
		mov			ebp,[esp+16+12]
		mov			edx,[esp+16+16]
		mov			ecx,[esp+16+20]

		sub			esi,edx

		neg			ecx
		add			edi,ebp
		sub			ecx, 1
		neg			ebp
		pxor		mm7,mm7
pixelloop:
		movd		mm0,[esi]

		movd		mm2,[esi+edx*2]
		punpcklbw	mm0,mm7

		movd		mm1,[esi+edx-4]
		punpcklbw	mm2,mm7

		movd		mm3,[esi+edx+4]
		punpcklbw	mm1,mm7

		psubw		mm0,mm2
		punpcklbw	mm3,mm7

		psubw		mm1,mm3
		movq		mm2,mm0

		movq		mm3,mm1
		psraw		mm0,15

		psraw		mm1,15
		pxor		mm2,mm0

		pxor		mm3,mm1
		psubw		mm0,mm2

		psubw		mm1,mm3
		movq		mm2,mm0

		psllq		mm2,16
		movq		mm3,mm1

		movq		mm4,mm0
		psllq		mm3,16

		movq		mm5,mm1
		psllq		mm4,32

		psllq		mm5,32
		paddw		mm0,mm2

		paddw		mm1,mm3
		paddw		mm0,mm4

		paddw		mm1,mm5
		add			esi,4

		punpckhwd	mm0,mm1

		pmaddwd		mm0,mm0

		movd		eax,mm0
		add			eax,ecx
		adc			ebx,ebx
		and			ebx,31
		mov			al,[codes + ebx]
		mov			[edi+ebp],al

		inc			ebp
		jne			pixelloop

		add			ebx, ebx
		and			ebx, 31
		mov			al,[codes + ebx]
		mov			[edi],al
		add			ebx, ebx
		and			ebx, 31
		mov			al,[codes + ebx]
		mov			[edi+1],al

		pop		ebp
		pop		edi
		pop		esi
		pop		ebx

		ret
	}
}
#endif

static void filtrow_1(byte *sum, uint32 *src, long width, const long pitch, const long thresh, const int *square_table) {
#ifdef _M_IX86
	if (MMX_enabled) {
		filtrow_1_mmx(sum, src, width, pitch, thresh);
		return;
	}
#endif

	long bitarray=0;

	src = (uint32 *)((char *)src - pitch);

	do {
		uint32 pu, pd, pl, pr;
		int grad_x, grad_y;

		// Fetch surrounding pixels.
		pl = *(uint32 *)((char *)(src-1) + pitch);
		pr = *(uint32 *)((char *)(src+1) + pitch);
		pu = *(uint32 *)((char *)src);
		pd = *(uint32 *)((char *)src + pitch*2);

		// Compute gradient at pixel.
		int rx = ((pr&0xff0000)>>16) - ((pl>>16)&0xff);
		int gx = ((pr&0x00ff00)>> 8) - ((pl>> 8)&0xff);
		int bx = ((pr&0x0000ff)    ) - ((pl    )&0xff);
		int ry = ((pd&0xff0000)>>16) - ((pu>>16)&0xff);
		int gy = ((pd&0x00ff00)>> 8) - ((pu>> 8)&0xff);
		int by = ((pd&0x0000ff)    ) - ((pu    )&0xff);

		grad_x = abs(rx) + abs(gx) + abs(bx);
		grad_y = abs(ry) + abs(gy) + abs(by);

		bitarray >>= 1;

		if (square_table[765+grad_x] + square_table[765+grad_y] > thresh) {
			bitarray |= 16;
		}

		*sum++ = codes[bitarray];

		++src;
	} while(--width>0);

	*sum++ = codes[bitarray>>1];
	*sum++ = codes[bitarray>>2];
}

#ifdef USE_ASM
static void __declspec(naked) avgrow_asm(uint32 *dst, uint32 *src, long width) {
	__asm {
		push	ebx
		push	esi
		push	edi
		push	ebp

		mov		esi,[esp+8+16]
		mov		edi,[esp+4+16]

		mov		eax,[esi]
		mov		ebx,0000ff00h
		and		ebx,eax
		and		eax,00ff00ffh

		mov		ecx,[esi+4]
		mov		edx,0000ff00h

		lea		eax,[eax+eax*4]
		lea		ebx,[ebx+ebx*4]

		and		edx,ecx
		and		ecx,00ff00ffh
		add		edx,edx
		add		ecx,ecx
		add		ebx,edx
		add		eax,ecx

		mov		ecx,[esi+8]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		ebx,edx
		add		eax,ecx

		add		eax,00040004h
		add		ebx,00000400h

		shr		eax,3
		shr		ebx,3

		and		eax,00ff00ffh
		and		ebx,0000ff00h

		add		eax,ebx

		mov		[edi],eax




		mov		eax,[esi+4]
		mov		ebx,0000ff00h
		and		ebx,eax
		and		eax,00ff00ffh

		mov		ecx,[esi+8]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		ebx,edx
		add		eax,ecx

		mov		ecx,[esi]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh

		add		eax,eax
		add		ebx,ebx

		lea		edx,[edx+edx*2]
		lea		ecx,[ecx+ecx*2]

		add		ebx,edx
		add		eax,ecx

		mov		ecx,[esi+12]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		ebx,edx
		add		eax,ecx

		add		eax,00040004h
		add		ebx,00000400h

		shr		eax,3
		shr		ebx,3

		and		eax,00ff00ffh
		and		ebx,0000ff00h

		add		eax,ebx

		mov		[edi+4],eax

		;prepare for loop!

		mov		ebp,[esp+12+16]
		sub		ebp,4
		mov		eax,ebp
		shl		eax,2
		add		esi,eax
		add		edi,eax
		neg		ebp

pixelloop:
		mov		eax,[esi+ebp*4+4]
		mov		ebx,0000ff00h
		and		ebx,eax
		and		eax,00ff00ffh

		mov		ecx,[esi+ebp*4+8]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		ebx,edx
		add		eax,ecx

		mov		ecx,[esi+ebp*4+12]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		ebx,edx
		add		eax,ecx

		add		eax,eax
		add		ebx,ebx

		mov		ecx,[esi+ebp*4]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		ebx,edx
		add		eax,ecx

		mov		ecx,[esi+ebp*4+16]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		ebx,edx
		add		eax,ecx

		add		eax,00040004h
		add		ebx,00000400h

		shr		eax,3
		shr		ebx,3

		and		eax,00ff00ffh
		and		ebx,0000ff00h

		add		eax,ebx

		mov		[edi+ebp*4+8],eax

		inc		ebp
		jne		pixelloop

		;finish up

		mov		eax,[esi+4]
		mov		ebx,0000ff00h
		and		ebx,eax
		and		eax,00ff00ffh

		mov		ecx,[esi+8]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		ebx,edx
		add		eax,ecx

		mov		ecx,[esi+12]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		eax,eax
		add		ebx,ebx
		lea		edx,[edx+edx*2]
		lea		ecx,[ecx+ecx*2]
		add		ebx,edx
		add		eax,ecx

		mov		ecx,[esi]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		ebx,edx
		add		eax,ecx

		add		eax,00040004h
		add		ebx,00000400h

		shr		eax,3
		shr		ebx,3

		and		eax,00ff00ffh
		and		ebx,0000ff00h

		add		eax,ebx

		mov		[edi+8],eax



		mov		eax,[esi+12]
		mov		ebx,0000ff00h
		and		ebx,eax
		and		eax,00ff00ffh

		mov		ecx,[esi+8]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		lea		eax,[eax+eax*4]
		lea		ebx,[ebx+ebx*4]
		add		ecx,ecx
		add		edx,edx
		add		ebx,edx
		add		eax,ecx

		mov		ecx,[esi+4]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		ebx,edx
		add		eax,ecx

		add		eax,00040004h
		add		ebx,00000400h

		shr		eax,3
		shr		ebx,3

		and		eax,00ff00ffh
		and		ebx,0000ff00h

		add		eax,ebx

		mov		[edi+12],eax

		pop		ebp
		pop		edi
		pop		esi
		pop		ebx

		ret
	}
}

static void __declspec(naked) avgrow_mmx(uint32 *dst, uint32 *src, long width) {
	static const __declspec(align(8)) __int64 add4=0x0004000400040004i64;
	__asm {
		push	ebx
		push	esi
		push	edi
		push	ebp

		mov		esi,[esp+8+16]
		mov		edi,[esp+4+16]

		mov		eax,[esi]
		mov		ebx,0000ff00h
		and		ebx,eax
		and		eax,00ff00ffh

		mov		ecx,[esi+4]
		mov		edx,0000ff00h

		lea		eax,[eax+eax*4]
		lea		ebx,[ebx+ebx*4]

		and		edx,ecx
		and		ecx,00ff00ffh
		add		edx,edx
		add		ecx,ecx
		add		ebx,edx
		add		eax,ecx

		mov		ecx,[esi+8]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		ebx,edx
		add		eax,ecx

		add		eax,00040004h
		add		ebx,00000400h

		shr		eax,3
		shr		ebx,3

		and		eax,00ff00ffh
		and		ebx,0000ff00h

		add		eax,ebx

		mov		[edi],eax




		mov		eax,[esi+4]
		mov		ebx,0000ff00h
		and		ebx,eax
		and		eax,00ff00ffh

		mov		ecx,[esi+8]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		ebx,edx
		add		eax,ecx

		mov		ecx,[esi]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh

		add		eax,eax
		add		ebx,ebx

		lea		edx,[edx+edx*2]
		lea		ecx,[ecx+ecx*2]

		add		ebx,edx
		add		eax,ecx

		mov		ecx,[esi+12]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		ebx,edx
		add		eax,ecx

		add		eax,00040004h
		add		ebx,00000400h

		shr		eax,3
		shr		ebx,3

		and		eax,00ff00ffh
		and		ebx,0000ff00h

		add		eax,ebx

		mov		[edi+4],eax

		;prepare for loop!

		mov		ebp,[esp+12+16]
		sub		ebp,4
		mov		eax,ebp
		shl		eax,2
		add		esi,eax
		add		edi,eax
		neg		ebp

		pxor		mm7,mm7
		movq		mm6,add4
pixelloop:
		movd		mm0,[esi+ebp*4+4]
		punpcklbw	mm0,mm7

		movd		mm1,[esi+ebp*4+8]
		punpcklbw	mm1,mm7

		movd		mm2,[esi+ebp*4+12]
		punpcklbw	mm2,mm7

		paddw		mm0,mm1
		paddw		mm0,mm2
		paddw		mm0,mm0

		movd		mm3,[esi+ebp*4]
		punpcklbw	mm3,mm7

		movd		mm4,[esi+ebp*4+16]
		punpcklbw	mm4,mm7

		paddw		mm3,mm4
		paddw		mm0,mm3

		paddw		mm0,mm6
		psraw		mm0,3
		packuswb	mm0,mm0

		movd		[edi+ebp*4+8],mm0

		inc		ebp
		jne		pixelloop

		;finish up

		mov		eax,[esi+4]
		mov		ebx,0000ff00h
		and		ebx,eax
		and		eax,00ff00ffh

		mov		ecx,[esi+8]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		ebx,edx
		add		eax,ecx

		mov		ecx,[esi+12]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		eax,eax
		add		ebx,ebx
		lea		edx,[edx+edx*2]
		lea		ecx,[ecx+ecx*2]
		add		ebx,edx
		add		eax,ecx

		mov		ecx,[esi]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		ebx,edx
		add		eax,ecx

		add		eax,00040004h
		add		ebx,00000400h

		shr		eax,3
		shr		ebx,3

		and		eax,00ff00ffh
		and		ebx,0000ff00h

		add		eax,ebx

		mov		[edi+8],eax



		mov		eax,[esi+12]
		mov		ebx,0000ff00h
		and		ebx,eax
		and		eax,00ff00ffh

		mov		ecx,[esi+8]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		lea		eax,[eax+eax*4]
		lea		ebx,[ebx+ebx*4]
		add		ecx,ecx
		add		edx,edx
		add		ebx,edx
		add		eax,ecx

		mov		ecx,[esi+4]
		mov		edx,0000ff00h
		and		edx,ecx
		and		ecx,00ff00ffh
		add		ebx,edx
		add		eax,ecx

		add		eax,00040004h
		add		ebx,00000400h

		shr		eax,3
		shr		ebx,3

		and		eax,00ff00ffh
		and		ebx,0000ff00h

		add		eax,ebx

		mov		[edi+12],eax

		pop		ebp
		pop		edi
		pop		esi
		pop		ebx

		ret
	}
}
#endif

static void avgrow(uint32 *dst, uint32 *src, long width) {
#ifdef USE_ASM

	if (MMX_enabled)
		avgrow_mmx(dst, src, width);
	else
		avgrow_asm(dst, src, width);

#else

	int rsum, gsum, bsum, r0, g0, b0;
	long w;

	uint32 c = *src++;

	r0 = ((int)(c>>16)&0xff);
	g0 = ((int)(c>> 8)&0xff);
	b0 = ((int)(c    )&0xff);

	rsum = r0*4;
	gsum = g0*4;
	bsum = b0*4;

	w=1;
	do {
		uint32 c = src[0];

		rsum = rsum + ((int)(c>>16)&0xff);
		gsum = gsum + ((int)(c>> 8)&0xff);
		bsum = bsum + ((int)(c    )&0xff);

		++src;
	} while(--w);

	w=3;
	do {
		uint32 c = src[0];

		rsum = rsum + ((int)(c>>16)&0xff) - r0;
		gsum = gsum + ((int)(c>> 8)&0xff) - g0;
		bsum = bsum + ((int)(c    )&0xff) - b0;

		*dst++ = ((rsum/5) << 16) | ((gsum/5)<<8) | (bsum/5);

		++src;
	} while(--w);

	w = width-6;
	do {
		uint32 c = src[0], d = src[-5];

		rsum = rsum + ((int)(c>>16)&0xff) - ((int)(d>>16)&0xff);
		gsum = gsum + ((int)(c>> 8)&0xff) - ((int)(d>> 8)&0xff);
		bsum = bsum + ((int)(c    )&0xff) - ((int)(d    )&0xff);

		*dst++ = ((rsum/5) << 16) | ((gsum/5)<<8) | (bsum/5);

		++src;
	} while(--w);

	c = *src++;

	r0 = ((int)(c>>16)&0xff);
	g0 = ((int)(c>> 8)&0xff);
	b0 = ((int)(c    )&0xff);

	w=3;
	do {
		uint32 d = src[-5];

		rsum = rsum - ((int)(d>>16)&0xff) + r0;
		gsum = gsum - ((int)(d>> 8)&0xff) + g0;
		bsum = bsum - ((int)(d    )&0xff) + b0;

		*dst++ = ((rsum/5) << 16) | ((gsum/5)<<8) | (bsum/5);

		++src;
	} while(--w);
#endif
}

#ifdef USE_ASM
static void __declspec(naked) final_mmx(uint32 *dst, int width, byte **row_array, uint32 **avg_array) {
	static const __declspec(align(8)) __int64 add4=0x0004000400040004i64;
	static const __declspec(align(8)) __int64 add8=0x0008000800080008i64;
	static const __declspec(align(8)) __int64 add16=0x0010001000100010i64;

	__asm {
		push		ebx
		push		esi
		push		edi
		push		ebp

		mov			esi,[esp+4+16]
		mov			ebp,[esp+8+16]
		mov			edi,[esp+12+16]
		mov			edx,[esp+16+16]
		mov			eax,ebp
		neg			ebp
		shl			eax,2
		add			esi,eax
		pxor		mm7,mm7

pixelloop:
		mov			eax,[edx+4]
		movd		mm0,[eax+ebp*4]
		punpcklbw	mm0,mm7
		mov			eax,[edx+8]
		movd		mm2,[eax+ebp*4]
		punpcklbw	mm2,mm7
		paddw		mm0,mm2
		mov			eax,[edx+12]
		movd		mm2,[eax+ebp*4]
		punpcklbw	mm2,mm7
		paddw		mm0,mm2
		paddw		mm0,mm0
		mov			eax,[edx+0]
		movd		mm1,[eax+ebp*4]
		punpcklbw	mm1,mm7
		mov			eax,[edx+16]
		movd		mm3,[eax+ebp*4]
		punpcklbw	mm3,mm7
		paddw		mm1,mm3
		paddw		mm0,mm1

		xor			eax,eax
		xor			ebx,ebx

		mov			ecx,[edi+0]
		mov			al,[ecx+ebp]
		mov			ecx,[edi+16]
		mov			bl,[ecx+ebp]
		and			al,0e0h
		and			bl,0e0h
		add			eax,ebx

		mov			ecx,[edi+4]
		mov			bl,[ecx+ebp]
		add			eax,ebx
		mov			ecx,[edi+8]
		mov			bl,[ecx+ebp]
		add			eax,ebx
		mov			ecx,[edi+12]
		mov			bl,[ecx+ebp]
		add			eax,ebx

		mov			ebx,eax
		and			al,31

		cmp			al,3
		ja			high_detail
		cmp			al,1
		ja			medium_detail
		cmp			ebx,00000080h
		jae			low_detail

		paddw		mm0,add4
		psraw		mm0,3
		packuswb	mm0,mm0
		movd		[esi+ebp*4],mm0
		add			ebp, 1
		jne			pixelloop
		jmp			xit

		align		16
low_detail:
		movd		mm1,[esi+ebp*4]
		punpcklbw	mm1,mm7
		psllw		mm1,3
		paddw		mm0,mm1
		paddw		mm1,mm1
		paddw		mm0,mm1
		paddw		mm0,add16
		psraw		mm0,5
		packuswb	mm0,mm0
		movd		[esi+ebp*4],mm0
		add			ebp, 1
		jne			pixelloop
		jmp			xit

		align		16
medium_detail:
		movd		mm1,[esi+ebp*4]
		punpcklbw	mm1,mm7
		psllw		mm1,3
		paddw		mm0,mm1
		paddw		mm0,add8
		psraw		mm0,4
		packuswb	mm0,mm0
		movd		[esi+ebp*4],mm0
		add			ebp, 1
		jne			pixelloop
		jmp			xit

		align		16
high_detail:
		add			ebp, 1
		jne			pixelloop
xit:
		pop			ebp
		pop			edi
		pop			esi
		pop			ebx

		ret
	}
}
#endif

static int smoother_run(const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	long w, h, x;
	int g_thresh = mfd->grad_threshold;
	int next_row = 0, next_avg_row = 0;
	uint32 *avg_rows[5];
	byte *row[5];
	int i;

	uint32 *src, *srcf, *dst, *avg;
	ptrdiff_t srcf_pitch;
	void (*avgrow_ptr)(uint32 *dst, uint32 *src, long width);

	const VDXPixmap& pxsrc = *fa->src.mpPixmap;
	const VDXPixmap& pxdst = *fa->dst.mpPixmap;

	///////////

	if (!g_thresh)
		g_thresh = 1;

	if (mfd->pBlurBitmap) {
		mfd->effBlur->run(mfd->vbBlur, (VDPixmap&)pxsrc);
		srcf = (uint32 *)mfd->vbBlur.data;
		srcf_pitch = mfd->vbBlur.pitch;

	} else {
		srcf = (uint32 *)pxsrc.data;
		srcf_pitch = pxsrc.pitch;
	}

	avgrow_ptr = avgrow;

	avgrow_ptr(mfd->avg_row[1], (uint32 *)((char *)pxsrc.data + 0*pxsrc.pitch), pxsrc.w);
	memcpy(mfd->avg_row[2], mfd->avg_row[1], pxsrc.w*4);
	memcpy(mfd->avg_row[3], mfd->avg_row[1], pxsrc.w*4);
	avgrow_ptr(mfd->avg_row[4], (uint32 *)((char *)pxsrc.data + 1*pxsrc.pitch), pxsrc.w);

	src = (uint32 *)pxsrc.data;
	dst = (uint32 *)pxdst.data;

	memset(mfd->sum_row[1], 0, pxdst.w+2);
	memset(mfd->sum_row[2], 0, pxdst.w+2);
	memset(mfd->sum_row[3], 0, pxdst.w+2);
	filtrow_1(mfd->sum_row[4], (uint32 *)((char *)(srcf + 1) + srcf_pitch), pxdst.w-2, srcf_pitch, g_thresh, mfd->square_table);

	h = pxsrc.h;
	do {
		if (h>3)
			filtrow_1(mfd->sum_row[next_row], (uint32 *)((char *)(srcf + 1) + srcf_pitch*2), pxdst.w-2, srcf_pitch, g_thresh, mfd->square_table);
		else
			memset(mfd->sum_row[next_row], 0, pxdst.w + 2);

		avg = mfd->avg_row[next_avg_row];

		if (h>2)
			avgrow_ptr(avg, (uint32 *)((char *)src + 2*pxsrc.pitch), pxsrc.w);
		else
			memcpy(avg, mfd->avg_row[(next_avg_row+4)%5], pxsrc.w*4);

		if (++next_row>=5)
			next_row = 0;

		if (++next_avg_row>=5)
			next_avg_row = 0;

#ifdef USE_ASM
		if (MMX_enabled) {
			for(i=0; i<5; i++)
				row[i] = mfd->sum_row[(next_row+i) % 5] + pxsrc.w;

			for(i=0; i<5; i++)
				avg_rows[i] = mfd->avg_row[(next_avg_row+i) % 5] + pxsrc.w;

			final_mmx(dst, pxdst.w, row, avg_rows);

			src = (uint32 *)((char *)src + pxsrc.pitch);
			dst = (uint32 *)((char *)dst + pxdst.pitch);
			srcf = (uint32 *)((char *)srcf + srcf_pitch);
		} else
#endif
		{
			for(i=0; i<5; i++)
				row[i] = mfd->sum_row[(next_row+i) % 5];

			for(i=0; i<5; i++)
				avg_rows[i] = mfd->avg_row[(next_avg_row+i) % 5];

			w = pxsrc.w;
			x=0;
			do {
				uint32 p0, p1, p2, p3, p4;
				int r, g, b;
				int s, A, B;

				// bbb000aa
				//
				// B = 5-neighbor count (0-5)
				// A = 3-neighbor count (0-3)

				s = (int)(row[0][x]&0xe0) + (int)row[1][x] + (int)row[2][x] + (int)row[3][x] + (int)(row[4][x] & 0xe0);
				A = s & 31;
				B = s>>5;

	#if 0
				if (A>3)
					*dst = 0x00ff00;
				else if (A>1)
					*dst = 0xff0000;
				else if (B>3)
					*dst = 0x0000ff;
				else
					*dst = 0;
	#else
				if (A>3)
					*dst = *src;
				else {
					p0 = avg_rows[0][x];
					p1 = avg_rows[1][x];
					p2 = avg_rows[2][x];
					p3 = avg_rows[3][x];
					p4 = avg_rows[4][x];

					r = (p0&0xff0000)
					  + (p1&0xff0000)*2
					  + (p2&0xff0000)*2
					  + (p3&0xff0000)*2
					  + (p4&0xff0000); 
					g = (p0&0xff00)
					  + (p1&0xff00)*2
					  + (p2&0xff00)*2
					  + (p3&0xff00)*2
					  + (p4&0xff00);
					b = (p0&0xff)
					  + (p1&0xff)*2
					  + (p2&0xff)*2
					  + (p3&0xff)*2
					  + (p4&0xff);

					if (A>1) {
						uint32 d = *src;

						r += (d<<3)&0x7f80000;
						g += (d<<3)&0x7f800;
						b += (d<<3)&0x7f8;

						*dst = ((r>>4)&0xff0000) | ((g>>4)&0xff00) | (b>>4);
					} else if (B>3) {
						uint32 d = *src;

						r = r*3 + ((d<<3)&0x7f80000);
						g = g*3 + ((d<<3)&0x7f800);
						b = b*3 + ((d<<3)&0x7f8);

						*dst = ((r>>5)&0xff0000) | ((g>>5)&0xff00) | (b>>5);
					} else {
						*dst = ((r>>3)&0xff0000) | ((g>>3)&0xff00) | (b>>3);
					}
				}
	#endif

				++src;
				++dst;

			} while(++x<w);

			src = (uint32 *)((char *)(src - w) + pxsrc.pitch);
			dst = (uint32 *)((char *)(dst - w) + pxdst.pitch);
			srcf = (uint32 *)((char *)srcf + srcf_pitch);
		}
	} while(--h>0);

#ifdef USE_ASM
	if (MMX_enabled)
		__asm emms
#endif

	return 0;
}

long smoother_param(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	pxldst.data = pxlsrc.data;
	pxldst.pitch = pxlsrc.pitch;

	return FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

static int smoother_start(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	int i;

	if (!mfd->square_table) {
		if (!(mfd->square_table = new int[765*2+1]))
			return 1;

		for(i=0; i<=765; i++)
			mfd->square_table[765+i] = mfd->square_table[765-i] = i*i;
	}

	for(i=0; i<5; i++)
		if (!(mfd->sum_row[i] = new byte[fa->src.w+2]))
			return 1;

	for(i=0; i<5; i++)
		if (!(mfd->avg_row[i] = new uint32[fa->src.w]))
			return 1;

	if (mfd->fBlurPass) {
		uint32 blurPitch = ((fa->src.w+1)&-2) * 4;

		if (!(mfd->pBlurBitmap = VirtualAlloc(NULL, blurPitch*fa->src.h, MEM_COMMIT, PAGE_READWRITE)))
			return 1;

		mfd->vbBlur.data = mfd->pBlurBitmap;
		mfd->vbBlur.pitch = blurPitch;
		mfd->vbBlur.w = fa->src.w;
		mfd->vbBlur.h = fa->src.h;
		mfd->vbBlur.format = nsVDPixmap::kPixFormat_XRGB8888;

		if (!(mfd->effBlur = VCreateEffectBlur((VDPixmapLayout&)*fa->src.mpPixmapLayout)))
			return 1;
	}

	return 0;
}

static int smoother_end(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	int i;

	delete mfd->square_table;
	mfd->square_table = NULL;

	for(i=0; i<5; i++) {
		delete mfd->sum_row[i];
		mfd->sum_row[i] = NULL;
	}

	for(i=0; i<5; i++) {
		delete mfd->avg_row[i];
		mfd->avg_row[i] = NULL;
	}

	if (mfd->pBlurBitmap) {
		VirtualFree(mfd->pBlurBitmap, 0, MEM_RELEASE);
		mfd->pBlurBitmap = NULL;
	}

	delete mfd->effBlur; mfd->effBlur = NULL;

	return 0;
}

static INT_PTR CALLBACK FilterValueDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	MyFilterData *mfd = (MyFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			mfd = (MyFilterData *)lParam;
			SendMessage(GetDlgItem(hDlg, IDC_SLIDER), TBM_SETRANGE, (WPARAM)FALSE, MAKELONG(0, 100));
			SendMessage(GetDlgItem(hDlg, IDC_SLIDER), TBM_SETPOS, (WPARAM)TRUE, (mfd->grad_threshold+100)/200); 
			CheckDlgButton(hDlg, IDC_PREFILTER, mfd->fBlurPass?BST_CHECKED:BST_UNCHECKED);
			SetWindowLongPtr(hDlg, DWLP_USER, (LPARAM)mfd);
			mfd->ifp->InitButton((VDXHWND)GetDlgItem(hDlg, IDC_PREVIEW));
            return (TRUE);

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				EndDialog(hDlg, 0);
				return TRUE;
			case IDCANCEL:
	            EndDialog(hDlg, 1);  
		        return TRUE;
			case IDC_PREVIEW:
				mfd->ifp->Toggle((VDXHWND)hDlg);
				break;
			case IDC_PREFILTER:
				if (HIWORD(wParam) == BN_CLICKED) {
					mfd->fBlurPass = !!IsDlgButtonChecked(hDlg, IDC_PREFILTER);
					mfd->ifp->UndoSystem();
					mfd->ifp->RedoSystem();
				}
				break;
			}
            break;

		case WM_NOTIFY:
			{
				HWND hwndItem = GetDlgItem(hDlg, IDC_SLIDER);
				SetDlgItemInt(hDlg, IDC_VALUE, SendMessage(hwndItem, TBM_GETPOS, 0,0), FALSE);
				mfd->grad_threshold = SendMessage(GetDlgItem(hDlg, IDC_SLIDER), TBM_GETPOS, 0,0)*200;
				mfd->ifp->RedoFrame();
			}
			return TRUE;
    }
    return FALSE;
}

static int smoother_config(VDXFilterActivation *fa, const VDXFilterFunctions *ff, VDXHWND hWnd) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	MyFilterData mfd_old = *mfd;

	mfd->ifp = fa->ifp;

	if (DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_SMOOTHER), (HWND)hWnd, FilterValueDlgProc, (LPARAM)mfd)) {
		*mfd = mfd_old;
		return 1;
	}

	return 0;
}

static void smoother_string2(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int maxlen) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	_snprintf(buf, maxlen, " (g:%ld)", mfd->grad_threshold/4);
}

static void smoother_script_config(IVDXScriptInterpreter *isi, void *lpVoid, VDXScriptValue *argv, int argc) {
	VDXFilterActivation *fa = (VDXFilterActivation *)lpVoid;
	int lv = argv[0].asInt();

	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	mfd->grad_threshold = lv*4;
	mfd->fBlurPass = !!argv[1].asInt();
}

static VDXScriptFunctionDef smoother_func_defs[]={
	{ (VDXScriptFunctionPtr)smoother_script_config, "Config", "0ii" },
	{ NULL },
};

static VDXScriptObject smoother_obj={
	NULL, smoother_func_defs
};

static bool smoother_script_line(VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int buflen) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d,%d)", mfd->grad_threshold/4, mfd->fBlurPass);

	return true;
}

extern const VDXFilterDefinition g_VDVFSmoother = {
	0,0,NULL,
	"smoother",
	"Dynamically smooths an image while trying not to smear edges.\n\n[Assembly optimized][MMX optimized]",
	NULL,NULL,
	sizeof(MyFilterData),
	NULL,NULL,
	smoother_run,
	smoother_param,
	smoother_config,
	NULL,
	smoother_start,
	smoother_end,

	&smoother_obj,
	smoother_script_line,
	smoother_string2,
};