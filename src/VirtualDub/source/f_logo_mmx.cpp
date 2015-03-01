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

#ifdef VD_CPU_X86
void __declspec(naked) __cdecl VDVFLogoAlphaBltMMX(Pixel32 *dst, PixOffset dstoff, const Pixel32 *src, PixOffset srcoff, PixDim w, PixDim h) {
	static const __int64 x80w = 0x0080008000800080;
	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx
		mov			eax, [esp+20+16]
		mov			ecx, [esp+4+16]
		shl			eax, 2
		mov			edx, [esp+12+16]
		movq		mm6, x80w
		pxor		mm7, mm7
		mov			ebx, [esp+24+16]
		add			ecx, eax
		add			edx, eax
		neg			eax
		mov			edi, [esp+8+16]
		mov			[esp+20+16],eax
		mov			esi, [esp+16+16]
yloop:
		mov			eax, [esp+20+16]
xloop:
		movd		mm0, [ecx+eax]

		movd		mm1, [edx+eax]
		punpcklbw	mm0, mm7

		movq		mm2, mm1
		paddw		mm0, mm0

		punpcklbw	mm2, mm2
		paddw		mm0, mm0

		punpckhwd	mm2, mm2
		punpckhdq	mm2, mm2
		movq		mm3, mm2
		psrlw		mm2, 2
		psraw		mm3, 15
		psubw		mm2, mm3

		pmulhw		mm0, mm2
		packuswb	mm0, mm0
		paddusb		mm0, mm1
		movd		[ecx+eax], mm0

		add			eax, 4
		jne			xloop

		add			ecx, edi
		add			edx, esi
		dec			ebx
		jne			yloop

		emms
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
	}
}

void __declspec(naked) __cdecl VDVFLogoCombineAlphaBltMMX(Pixel32 *dst, PixOffset dstoff, const Pixel32 *src, PixOffset srcoff, PixDim w, PixDim h) {
	// 0.30 0.59 0.11
	static const __int64 lumafact = 0x000026664B860E14;
	static const __int64 rounder  = 0x0000800000000000;
	static const __int64 x80w	  = 0x0080008000800080;
	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx
		mov			eax, [esp+20+16]
		mov			ecx, [esp+4+16]
		shl			eax, 2
		mov			edx, [esp+12+16]
		movq		mm5, lumafact
		pcmpeqd		mm4, mm4
		pslld		mm4, 24
		movq		mm3, rounder
		pxor		mm7, mm7
		mov			ebx, [esp+24+16]
		add			ecx, eax
		add			edx, eax
		neg			eax
		mov			edi, [esp+8+16]
		mov			[esp+20+16],eax
		mov			esi, [esp+16+16]
yloop:
		mov			eax, [esp+20+16]
xloop:
		movd		mm1, [edx+eax]
		movq		mm0, mm4
		pandn		mm0, [ecx+eax]
		punpcklbw	mm1, mm7
		paddw		mm1, mm1

		pmaddwd		mm1, mm5				;mm1 = (luma1 << 16) + (luma2 << 48)
		punpckldq	mm2, mm1
		paddd		mm1, mm3
		paddd		mm1, mm2
		packuswb	mm1, mm1
		pand		mm1, mm4
		paddb		mm0, mm1

		movd		[ecx+eax], mm0

		add			eax, 4
		jne			xloop

		add			ecx, edi
		add			edx, esi
		dec			ebx
		jne			yloop

		emms
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
	}
}

void __declspec(naked) __cdecl VDVFLogoPremultiplyAlphaMMX(Pixel32 *dst, PixOffset dstoff, PixDim w, PixDim h) {
	// 0.30 0.59 0.11
	static const __int64 lumafact = 0x000026664B860E14;
	static const __int64 rounder  = 0x0000800000000000;
	static const __int64 x80w	  = 0x0080008000800080;
	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx
		mov			eax, [esp+12+16]
		mov			ecx, [esp+4+16]
		shl			eax, 2
		movq		mm5, lumafact
		pcmpeqd		mm4, mm4
		pslld		mm4, 24
		movq		mm3, rounder
		pxor		mm7, mm7
		mov			ebx, [esp+16+16]
		add			ecx, eax
		add			edx, eax
		neg			eax
		mov			edi, [esp+8+16]
		mov			[esp+12+16],eax
yloop:
		mov			eax, [esp+12+16]
xloop:
		movd		mm1, [ecx+eax]
		movq		mm0, mm4
		por			mm0, mm1
		punpcklbw	mm1, mm7
		punpcklbw	mm0, mm7

		punpckhwd	mm1, mm1
		punpckhdq	mm1, mm1

		pmullw		mm0, mm1
		paddw		mm0, x80w
		movq		mm1, mm0
		psrlw		mm1, 8
		paddw		mm0, mm1
		psrlw		mm0, 8
		packuswb	mm0, mm0

		movd		[ecx+eax], mm0

		add			eax, 4
		jne			xloop

		add			ecx, edi
		dec			ebx
		jne			yloop

		emms
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
	}
}

void __declspec(naked) __cdecl VDVFLogoScalePremultipliedAlphaMMX(Pixel32 *dst, PixOffset dstoff, PixDim w, PixDim h, unsigned alpha) {
	static const __int64 x80w	  = 0x0080008000800080;
	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx
		mov			eax, [esp+12+16]
		mov			ecx, [esp+4+16]
		shl			eax, 2
		movd		mm5, [esp+20+16]
		pxor		mm7, mm7
		movq		mm4, x80w
		punpcklwd	mm5, mm5
		punpckldq	mm5, mm5
		mov			ebx, [esp+16+16]
		add			ecx, eax
		add			edx, eax
		neg			eax
		mov			edi, [esp+8+16]
		mov			[esp+12+16],eax
yloop:
		mov			eax, [esp+12+16]
xloop:
		movd		mm0, [ecx+eax]
		punpcklbw	mm0, mm7
		pmullw		mm0, mm5
		paddw		mm0, mm4
		psrlw		mm0, 8
		packuswb	mm0, mm0
		movd		[ecx+eax], mm0

		add			eax, 4
		jne			xloop

		add			ecx, edi
		dec			ebx
		jne			yloop

		emms
		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret
	}
}
#endif
