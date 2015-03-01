//	VirtualDub - Video processing and capture application
//	Audio processing library
//	Copyright (C) 1998-2004 Avery Lee
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

#include <vd2/system/cpuaccel.h>
#include <vd2/Priss/convert.h>

///////////////////////////////////////////////////////////////////////////
//
//	scalar implementations
//
///////////////////////////////////////////////////////////////////////////

void VDAPIENTRY VDConvertPCM32FToPCM8(void *dst0, const void *src0, uint32 samples) {
	if (!samples)
		return;

	uint8 *dst = (uint8 *)dst0;
	const float *src = (const float *)src0;

	do {
		const float ftmp = 98304.0f + *src++;
		sint32 v = reinterpret_cast<const sint32&>(ftmp) - 0x47bfff80;

		if ((uint32)v >= 256)
			v = (~v) >> 31;

		*dst++ = (uint8)v;
	} while(--samples);
}

void VDAPIENTRY VDConvertPCM32FToPCM16(void *dst0, const void *src0, uint32 samples) {
	if (!samples)
		return;

	sint16 *dst = (sint16 *)dst0;
	const float *src = (const float *)src0;

	do {
		const float ftmp = 384.0f + *src++;
		sint32 v = reinterpret_cast<const sint32&>(ftmp) - 0x43bf8000;

		if ((uint32)v >= 0x10000)
			v = (~v) >> 31;

		*dst++ = (sint16)(v - 0x8000);
	} while(--samples);
}

void VDAPIENTRY VDConvertPCM16ToPCM8(void *dst0, const void *src0, uint32 samples) {
	if (!samples)
		return;

	uint8 *dst = (uint8 *)dst0;
	const sint16 *src = (const sint16 *)src0;

	do {
		*dst++ = (uint8)((*src++ >> 8)^0x80);
	} while(--samples);
}

void VDAPIENTRY VDConvertPCM16ToPCM32F(void *dst0, const void *src0, uint32 samples) {
	if (!samples)
		return;

	float *dst = (float *)dst0;
	const sint16 *src = (const sint16 *)src0;

	do {
		*dst++ = (float)*src++ * (1.0f / 32768.0f);
	} while(--samples);
}

void VDAPIENTRY VDConvertPCM8ToPCM16(void *dst0, const void *src0, uint32 samples) {
	if (!samples)
		return;

	sint16 *dst = (sint16 *)dst0;
	const uint8 *src = (const uint8 *)src0;

	do {
		*dst++ = (sint16)(((int)*src++ - 0x80) << 8);
	} while(--samples);
}

void VDAPIENTRY VDConvertPCM8ToPCM32F(void *dst0, const void *src0, uint32 samples) {
	if (!samples)
		return;

	float *dst = (float *)dst0;
	const uint8 *src = (const uint8 *)src0;

	do {
		*dst++ = (float)((int)*src++ - 0x80) * (1.0f / 128.0f);
	} while(--samples);
}

sint16 VDAPIENTRY VDAudioFilterPCM16(const sint16 *src, const sint16 *filter, uint32 filterquadsize) {
	sint32 v = 0x2000;

	const uint32 n = filterquadsize*4;
	for(uint32 j=0; j<n; j+=4) {
		v += (sint32)filter[j  ] * (sint32)src[j  ];
		v += (sint32)filter[j+1] * (sint32)src[j+1];
		v += (sint32)filter[j+2] * (sint32)src[j+2];
		v += (sint32)filter[j+3] * (sint32)src[j+3];
	}

	v = (v>>14) + 0x8000;

	if ((uint32)v >= 0x10000)
		v = ~v >> 31;

	return (sint16)(v - 0x8000);
}

void VDAPIENTRY VDAudioFilterPCM16End() {
}

void VDAPIENTRY VDAudioFilterPCM16SymmetricArray(sint16 *dst, ptrdiff_t dst_stride, const sint16 *src, uint32 count, const sint16 *filter, uint32 filterquadsizeminus1) {
	uint32 filterelsizeminus4 = filterquadsizeminus1*4;

	filter += filterquadsizeminus1*4;
	src += filterquadsizeminus1*4;

	for(uint32 i=0; i<count; ++i) {
		sint32 v = 0x2000 + (sint32)filter[0] * src[i];

		for(uint32 j=1; j<=filterelsizeminus4; j+=4) {
			int k = -(int)j;
			v += (sint32)filter[j  ] * ((sint32)src[i+j  ] + (sint32)src[i+k  ]);
			v += (sint32)filter[j+1] * ((sint32)src[i+j+1] + (sint32)src[i+k-1]);
			v += (sint32)filter[j+2] * ((sint32)src[i+j+2] + (sint32)src[i+k-2]);
			v += (sint32)filter[j+3] * ((sint32)src[i+j+3] + (sint32)src[i+k-3]);
		}

		v = (v>>14) + 0x8000;

		if ((uint32)v >= 0x10000)
			v = ~v >> 31;

		*dst = (sint16)(v - 0x8000);
		dst += dst_stride;
	}
}


#ifdef _M_IX86

///////////////////////////////////////////////////////////////////////////
//
//	MMX implementations
//
///////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
	#pragma warning(disable: 4799)		// warning C4799: function has no MMX instruction
#endif

namespace {
	const __int64 x80b = 0x8080808080808080;
}

void __declspec(naked) VDAPIENTRY VDConvertPCM16ToPCM8_MMX(void *dst0, const void *src0, uint32 samples) {

	__asm {
		mov			eax, [esp+12]
		mov			ecx, [esp+8]
		mov			edx, [esp+4]
		or			eax, eax
		jz			xit

		movq		mm7, x80b
		neg			eax
		add			eax, 7
		jc			nodq

		;process quads (8n samples)
dqloop:
		movq		mm0, [ecx]
		movq		mm1, [ecx+8]
		psrlw		mm0, 8
		add			edx, 8
		psrlw		mm1, 8
		add			ecx, 16
		packuswb	mm0, mm1
		pxor		mm0, mm7
		add			eax, 8
		movq		[edx-8], mm0
		ja			dqloop
nodq:
		cmp			eax, 3
		jg			noq
		add			eax, 4

		;process leftover quad (4 samples)
		movq		mm0, [ecx]
		add			edx, 4
		psrlw		mm0, 8
		packuswb	mm0, mm0
		add			ecx, 8
		pxor		mm0, mm7
		movd		[edx-4], mm0
noq:
		sub			eax, 7
		jz			xit2

		;process leftover samples
		movd		mm0, ebx
singleloop:
		mov			bl, byte ptr [ecx+1]
		add			ecx, 2
		xor			bl, 80h
		mov			byte ptr [edx], bl
		inc			edx
		inc			eax
		jne			singleloop
		movd		ebx, mm0
xit2:
		emms
xit:
		ret
	}
}

void __declspec(naked) VDAPIENTRY VDConvertPCM8ToPCM16_MMX(void *dst0, const void *src0, uint32 samples) {
	__asm {
		mov		eax, [esp+12]
		mov		ecx, [esp+8]
		mov		edx, [esp+4]
		or		eax, eax
		jz		xit

		movq		mm7, x80b
		neg			eax
		movq		mm0, mm7
		add			eax, 7
		jc			nodq

		;process quads (8n samples)
dqloop:
		pxor		mm0, [ecx]
		pxor		mm1, mm1
		add			edx, 16
		punpcklbw	mm1, mm0
		add			ecx, 8
		pxor		mm2, mm2
		punpckhbw	mm2, mm0
		add			eax, 8
		movq		[edx-16], mm1
		movq		mm0, mm7
		movq		[edx-8], mm2
		ja			dqloop
nodq:
		cmp			eax, 3
		jg			noq
		add			eax, 4

		;process leftover quad (4 samples)
		movd		mm0, [ecx]
		pxor		mm1, mm1
		pxor		mm0, mm7
		add			edx, 8
		punpcklbw	mm1, mm0
		add			ecx, 4
		movq		[edx-8], mm1
noq:
		sub			eax, 7
		jz			xit2

		;process leftover samples
		movd		mm0, ebx
singleloop:
		movzx		ebx, byte ptr [ecx]
		add			edx, 2
		shl			ebx, 8
		inc			ecx
		xor			ebx, 8000h
		inc			eax
		mov			word ptr [edx-2], bx
		jne			singleloop
		movd		ebx, mm0
xit2:
		emms
xit:
		ret
	}
}


sint16 __declspec(naked) VDAPIENTRY VDAudioFilterPCM16_MMX(const sint16 *src, const sint16 *filter, uint32 filterquadsize) {
	static const uint64 roundconst = 0x0000200000002000;

	__asm {
		mov		eax,[esp+12]
		mov		ecx,[esp+4]
		shl		eax,3
		mov		edx,[esp+8]
		movq	mm0,roundconst
		add		ecx, eax
		add		edx, eax
		neg		eax
xloop:
		movq	mm1,[ecx+eax]
		pmaddwd	mm1,[edx+eax]
		add		eax,8
		paddd	mm0,mm1
		jne		xloop

		punpckldq	mm1,mm0
		paddd		mm0,mm1
		psrad		mm0,14
		packssdw	mm0,mm0
		psrlq		mm0,48
		movd	eax, mm0
		ret
	}
}

sint16 __declspec(naked) VDAPIENTRY VDAudioFilterPCM16_128_MMX(const sint16 *src, const sint16 *filter, uint32 filterquadsize) {
	static const uint64 roundconst = 0x0000200000002000;

	__asm {
		mov		eax,[esp+12]
		mov		ecx,[esp+4]
		mov		edx,[esp+8]

		movq	mm4,roundconst		;1
		movq	mm0,[ecx+  0]		;1
		pmaddwd	mm0,[edx+  0]		;2
		movq	mm1,[ecx+  8]		;1
		pmaddwd	mm1,[edx+  8]		;1
		movq	mm2,[ecx+ 16]		;2
		pmaddwd	mm2,[edx+ 16]		;1
		movq	mm3,[ecx+ 24]		;1
		pmaddwd	mm3,[edx+ 24]		;2

		paddd	mm0,mm4				;1
		movq	mm4,[ecx+ 32]		;1
		pmaddwd	mm4,[edx+ 32]		;2
		movq	mm5,[ecx+ 40]		;1
		pmaddwd	mm5,[edx+ 40]		;2
		movq	mm6,[ecx+ 48]		;1
		pmaddwd	mm6,[edx+ 48]		;2
		movq	mm7,[ecx+ 56]		;1
		pmaddwd	mm7,[edx+ 56]		;2

		paddd	mm0,mm4				;1
		movq	mm4,[ecx+ 64]		;1
		pmaddwd	mm4,[edx+ 64]		;2
		paddd	mm1,mm5				;1
		movq	mm5,[ecx+ 72]		;1
		pmaddwd	mm5,[edx+ 72]		;2
		paddd	mm2,mm6				;1
		movq	mm6,[ecx+ 80]		;1
		pmaddwd	mm6,[edx+ 80]		;2
		paddd	mm3,mm7				;1
		movq	mm7,[ecx+ 88]		;1
		pmaddwd	mm7,[edx+ 88]		;2

		paddd	mm0,mm4				;1
		movq	mm4,[ecx+ 96]		;1
		pmaddwd	mm4,[edx+ 96]		;2
		paddd	mm1,mm5				;1
		movq	mm5,[ecx+104]		;1
		pmaddwd	mm5,[edx+104]		;2
		paddd	mm2,mm6				;1
		movq	mm6,[ecx+112]		;1
		pmaddwd	mm6,[edx+112]		;2
		paddd	mm3,mm7				;1
		movq	mm7,[ecx+120]		;1
		pmaddwd	mm7,[edx+120]		;2

		paddd	mm0,mm4				;1
		movq	mm4,[ecx+128]		;1
		pmaddwd	mm4,[edx+128]		;2
		paddd	mm1,mm5				;1
		movq	mm5,[ecx+136]		;1
		pmaddwd	mm5,[edx+136]		;2
		paddd	mm2,mm6				;1
		movq	mm6,[ecx+144]		;1
		pmaddwd	mm6,[edx+144]		;2
		paddd	mm3,mm7				;1
		movq	mm7,[ecx+152]		;1
		pmaddwd	mm7,[edx+152]		;2

		paddd	mm0,mm4				;1
		movq	mm4,[ecx+160]		;1
		pmaddwd	mm4,[edx+160]		;2
		paddd	mm1,mm5				;1
		movq	mm5,[ecx+168]		;1
		pmaddwd	mm5,[edx+168]		;2
		paddd	mm2,mm6				;1
		movq	mm6,[ecx+176]		;1
		pmaddwd	mm6,[edx+176]		;2
		paddd	mm3,mm7				;1
		movq	mm7,[ecx+184]		;1
		pmaddwd	mm7,[edx+184]		;2

		paddd	mm0,mm4				;1
		movq	mm4,[ecx+192]		;1
		pmaddwd	mm4,[edx+192]		;2
		paddd	mm1,mm5				;1
		movq	mm5,[ecx+200]		;1
		pmaddwd	mm5,[edx+200]		;2
		paddd	mm2,mm6				;1
		movq	mm6,[ecx+208]		;1
		pmaddwd	mm6,[edx+208]		;2
		paddd	mm3,mm7				;1
		movq	mm7,[ecx+216]		;1
		pmaddwd	mm7,[edx+216]		;2

		paddd	mm0,mm4				;1
		movq	mm4,[ecx+224]		;1
		pmaddwd	mm4,[edx+224]		;2
		paddd	mm1,mm5				;1
		movq	mm5,[ecx+232]		;1
		pmaddwd	mm5,[edx+232]		;2
		paddd	mm2,mm6				;1
		movq	mm6,[ecx+240]		;1
		pmaddwd	mm6,[edx+240]		;2
		paddd	mm3,mm7				;1
		movq	mm7,[ecx+248]		;1
		pmaddwd	mm7,[edx+248]		;2

		paddd		mm0, mm4		;1
		paddd		mm1, mm5		;1
		paddd		mm2, mm6		;1
		paddd		mm3, mm7		;1

		paddd		mm0, mm2		;1
		paddd		mm1, mm3		;1
		paddd		mm0, mm1		;1
		punpckldq	mm1, mm0		;1
		paddd		mm0, mm1		;1
		psrad		mm0, 14			;1
		packssdw	mm0, mm0		;1
		psrld		mm0, 16
		movd		eax, mm0
		ret
	}
}

void VDAPIENTRY VDAudioFilterPCM16End_MMX() {
	__asm emms
}

void __declspec(naked) VDAPIENTRY VDAudioFilterPCM16SymmetricArray_MMX(sint16 *dst, ptrdiff_t dst_stride, const sint16 *src_center, uint32 count, const sint16 *filter, uint32 filterquadsizeminus1) {
	static const uint64 roundconst = 0x0000200000002000;

	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx
		mov			ebx,[esp+24+16]
		mov			ecx,[esp+12+16]
		shl			ebx,4
		mov			edx,[esp+20+16]
		lea			ecx, [ecx+ebx+8]
		lea			edx, [edx+ebx+8]
		neg			ebx
		mov			esi, [esp+16+16]
		mov			edi, [esp+4+16]
		mov			ebp, [esp+8+16]
		add			ebp, ebp
yloop:
		mov			eax, ebx
		movq		mm0,roundconst
		movq		mm1,[ecx+eax-8]
		pmaddwd		mm1,[edx+eax-8]
xloop:
		movq		mm2, [ecx+eax]
		movq		mm3, [ecx+eax+8]
		pmaddwd		mm2, [edx+eax]
		pmaddwd		mm3, [edx+eax+8]
		add			eax, 16
		paddd		mm0, mm2
		paddd		mm1, mm3
		jne			xloop

		paddd		mm0, mm1
		add			ecx, 2
		punpckldq	mm1, mm0
		paddd		mm0, mm1
		psrad		mm0, 14
		packssdw	mm0, mm0
		psrlq		mm0, 48
		movd		eax, mm0
		mov			word ptr [edi], ax
		add			edi, ebp
		dec			esi
		jne			yloop
		emms
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	SSE implementations
//
///////////////////////////////////////////////////////////////////////////

static const float __declspec(align(16)) sse_32K[4]={32768.0f, 32768.0f, 32768.0f, 32768.0f};
static const float __declspec(align(16)) sse_128[4]={128.0f, 128.0f, 128.0f, 128.0f};
static const float __declspec(align(16)) sse_inv_32K[4]={1.0f/32768.0f, 1.0f/32768.0f, 1.0f/32768.0f, 1.0f/32768.0f};
static const float __declspec(align(16)) sse_inv_128[4]={1.0f/128.0f, 1.0f/128.0f, 1.0f/128.0f, 1.0f/128.0f};

void __declspec(naked) VDAPIENTRY VDConvertPCM32FToPCM16_SSE(void *dst, const void *src, uint32 samples) {
	__asm {
		push		ebx
		mov			edx, [esp+4+4]
		mov			ecx, [esp+8+4]
		movaps		xmm1, sse_32K
		mov			ebx, [esp+12+4]

		neg			ebx
		jz			xit

		test		ecx, 15
		jz			majorloopstart
prealignloop:
		movss		xmm0, [ecx]
		add			ecx, 4
		mulss		xmm0, xmm1
		cvtps2pi	mm0, xmm0
		packssdw	mm0, mm0
		movd		eax, mm0
		mov			word ptr [edx], ax
		add			edx, 2
		inc			ebx
		jz			xit
		test		ecx, 15
		jnz			prealignloop

majorloopstart:
		add			ebx, 3
		jc			postloopstart

majorloop:
		movaps		xmm0, [ecx]
		add			ecx, 16
		mulps		xmm0, xmm1
		cvtps2pi	mm0, xmm0
		movhlps		xmm0, xmm0
		cvtps2pi	mm1, xmm0
		packssdw	mm0, mm1
		movq		[edx], mm0
		add			edx, 8
		add			ebx, 4
		jnc			majorloop

postloopstart:
		sub			ebx, 3
		jz			xit
postloop:
		movss		xmm0, [ecx]
		add			ecx, 4
		mulss		xmm0, xmm1
		cvtps2pi	mm0, xmm0
		packssdw	mm0, mm0
		movd		eax, mm0
		mov			word ptr [edx], ax
		add			edx, 2
		inc			ebx
		jnz			postloop

xit:
		emms
		pop			ebx
		ret
	}
}

void __declspec(naked) VDAPIENTRY VDConvertPCM32FToPCM8_SSE(void *dst, const void *src, uint32 samples) {
	__asm {
		push		ebx
		mov			edx, [esp+4+4]
		mov			ecx, [esp+8+4]
		movaps		xmm1, sse_128
		mov			ebx, [esp+12+4]

		neg			ebx
		jz			xit

		test		ecx, 15
		jz			majorloopstart
prealignloop:
		movss		xmm0, [ecx]
		add			ecx, 4
		mulss		xmm0, xmm1
		addss		xmm0, xmm1
		cvtps2pi	mm0, xmm0
		packssdw	mm0, mm0
		packuswb	mm0, mm0
		movd		eax, mm0
		mov			byte ptr [edx], al
		inc			edx
		inc			ebx
		jz			xit
		test		ecx, 15
		jnz			prealignloop

majorloopstart:
		add			ebx, 3
		jc			postloopstart

majorloop:
		movaps		xmm0, [ecx]
		add			ecx, 16
		mulps		xmm0, xmm1
		addps		xmm0, xmm1
		cvtps2pi	mm0, xmm0
		movhlps		xmm0, xmm0
		cvtps2pi	mm1, xmm0
		packssdw	mm0, mm1
		packuswb	mm0, mm0
		movd		[edx], mm0
		add			edx, 4
		add			ebx, 4
		jnc			majorloop

postloopstart:
		sub			ebx, 3
		jz			xit
postloop:
		movss		xmm0, [ecx]
		add			ecx, 4
		mulss		xmm0, xmm1
		addss		xmm0, xmm1
		cvtps2pi	mm0, xmm0
		packssdw	mm0, mm0
		packuswb	mm0, mm0
		movd		eax, mm0
		mov			byte ptr [edx], al
		inc			edx
		inc			ebx
		jnz			postloop

xit:
		emms
		pop			ebx
		ret
	}
}

void __declspec(naked) VDAPIENTRY VDConvertPCM16ToPCM32F_SSE(void *dst, const void *src, uint32 samples) {
	__asm {
		movaps		xmm4, sse_inv_32K
		push		ebx
		mov			eax, [esp+12+4]
		neg			eax
		jz			xit
		mov			ecx, [esp+8+4]
		add			eax, eax
		mov			edx, [esp+4+4]
		sub			ecx, eax

		;align destination
alignloop:
		test		edx, 15
		jz			fastloop_start
		movsx		ebx, word ptr [ecx+eax]
		cvtsi2ss	xmm0, ebx
		mulss		xmm0, xmm4
		movss		[edx],xmm0
		add			edx, 4
		add			eax, 2
		jne			alignloop
		pop			ebx
		ret

fastloop_start:
		add			eax, 6
		jns			skip_fastloop
		jmp			short fastloop

		align		16
fastloop:
		movq		mm0, [ecx+eax-6]
		pxor		mm1, mm1
		punpckhwd	mm1, mm0
		punpcklwd	mm0, mm0
		psrad		mm0, 16
		psrad		mm1, 16
		cvtpi2ps	xmm0, mm0
		cvtpi2ps	xmm1, mm1
		movlhps		xmm0, xmm1
		mulps		xmm0, xmm4
		movaps		[edx], xmm0
		add			edx, 16
		add			eax, 8
		jnc			fastloop
		emms
skip_fastloop:
		sub			eax, 6
		jz			xit
tidyloop:
		movsx		ebx, word ptr [ecx+eax]
		cvtsi2ss	xmm0, ebx
		mulss		xmm0, xmm4
		movss		[edx],xmm0
		add			edx, 4
		add			eax, 2
		jne			tidyloop
xit:
		pop			ebx
		ret
	}
}

void __declspec(naked) VDAPIENTRY VDConvertPCM8ToPCM32F_SSE(void *dst, const void *src, uint32 samples) {
	__asm {
		movaps		xmm1, sse_inv_128
		push		ebx
		mov			eax, [esp+12+4]
		neg			eax
		jz			xit
		mov			ecx, [esp+8+4]
		mov			edx, [esp+4+4]
		sub			ecx, eax
lup:
		movzx		ebx, byte ptr [ecx+eax]
		sub			ebx, 80h
		cvtsi2ss	xmm0, ebx
		mulss		xmm0, xmm1
		movss		[edx],xmm0
		add			edx, 4
		inc			eax
		jne			lup
xit:
		pop			ebx
		ret
	}
}
#endif

///////////////////////////////////////////////////////////////////////////
//
//	vtables
//
///////////////////////////////////////////////////////////////////////////

static const tpVDConvertPCM g_VDConvertPCMTable_scalar[3][3]={
	{	0,							VDConvertPCM8ToPCM16,			VDConvertPCM8ToPCM32F		},
	{	VDConvertPCM16ToPCM8,		0,								VDConvertPCM16ToPCM32F		},
	{	VDConvertPCM32FToPCM8,		VDConvertPCM32FToPCM16,			0							},
};

#ifdef _M_IX86
static const tpVDConvertPCM g_VDConvertPCMTable_MMX[3][3]={
	{	0,							VDConvertPCM8ToPCM16_MMX,		VDConvertPCM8ToPCM32F		},
	{	VDConvertPCM16ToPCM8_MMX,	0,								VDConvertPCM16ToPCM32F		},
	{	VDConvertPCM32FToPCM8,		VDConvertPCM32FToPCM16,			0							},
};

static const tpVDConvertPCM g_VDConvertPCMTable_SSE[3][3]={
	{	0,							VDConvertPCM8ToPCM16_MMX,		VDConvertPCM8ToPCM32F_SSE	},
	{	VDConvertPCM16ToPCM8_MMX,	0,								VDConvertPCM16ToPCM32F_SSE	},
	{	VDConvertPCM32FToPCM8_SSE,	VDConvertPCM32FToPCM16_SSE,		0							},
};
#endif

tpVDConvertPCMVtbl VDGetPCMConversionVtable() {
#ifdef _M_IX86
	uint32 exts = CPUGetEnabledExtensions();

	if (exts & CPUF_SUPPORTS_MMX) {
		if (exts & CPUF_SUPPORTS_SSE)
			return g_VDConvertPCMTable_SSE;
		else
			return g_VDConvertPCMTable_MMX;
	}
#endif
	return g_VDConvertPCMTable_scalar;
}

static const VDAudioFilterVtable g_VDAudioFilterVtable_scalar = {
	VDAudioFilterPCM16,
	VDAudioFilterPCM16End,
	VDAudioFilterPCM16SymmetricArray
};

#ifdef _M_IX86
static const VDAudioFilterVtable g_VDAudioFilterVtable_MMX = {
	VDAudioFilterPCM16_MMX,
	VDAudioFilterPCM16End_MMX,
	VDAudioFilterPCM16SymmetricArray_MMX
};

static const VDAudioFilterVtable g_VDAudioFilterVtable_128_MMX = {
	VDAudioFilterPCM16_128_MMX,
	VDAudioFilterPCM16End_MMX,
	VDAudioFilterPCM16SymmetricArray_MMX
};
#endif

const VDAudioFilterVtable *VDGetAudioFilterVtable(uint32 taps) {
#ifdef _M_IX86
	uint32 exts = CPUGetEnabledExtensions();

	if (exts & CPUF_SUPPORTS_MMX) {
		if (taps == 128) {
			return &g_VDAudioFilterVtable_128_MMX;
		}

		return &g_VDAudioFilterVtable_MMX;
	}
#endif

	return &g_VDAudioFilterVtable_scalar;
}
