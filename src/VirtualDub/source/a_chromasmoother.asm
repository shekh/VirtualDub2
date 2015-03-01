;	VirtualDub - Video processing and capture application
;	Copyright (C) 1998-2001 Avery Lee
;
;	This program is free software; you can redistribute it and/or modify
;	it under the terms of the GNU General Public License as published by
;	the Free Software Foundation; either version 2 of the License, or
;	(at your option) any later version.
;
;	This program is distributed in the hope that it will be useful,
;	but WITHOUT ANY WARRANTY; without even the implied warranty of
;	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;	GNU General Public License for more details.
;
;	You should have received a copy of the GNU General Public License
;	along with this program; if not, write to the Free Software
;	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	segment	.rdata, align=16
luma_coeff2	dw	29*4, 150*4, 77*4, -1024
luma_coeff4	dw	29*4, 150*4, 77*4, -2048

	segment	.text

	; avg(c1, c2, c3) - luma(c1, c2, c3) + luma(c2)
	; avg(c1, c2, c3) - luma(avg(c1, c2, c3) - c2)
	; avg(c1, c2, c3) + luma(c2 - avg(c1, c2, c3))

	global	_asm_chromasmoother_FilterHorizontalMPEG1_MMX
_asm_chromasmoother_FilterHorizontalMPEG1_MMX:
	push		ebp
	push		edi
	push		esi
	push		ebx

	mov		eax, [esp+12+16]
	mov		ecx, [esp+8+16]
	add		eax, eax
	mov		edx, [esp+4+16]
	add		eax, eax

	movq		mm6, qword [luma_coeff4]
	pcmpeqb		mm7, mm7
	pxor		mm3, mm3
	psllq		mm7, 48

	lea		ecx, [ecx+eax]
	lea		edx, [edx+eax-4]
	xor		eax, -4

	movd		mm1, dword [ecx+eax+4]
	punpcklbw	mm1, mm3
	movq		mm0, mm1
	paddw		mm1, mm1
	movq		mm4, mm0

	mov		ebx, 2
	add		eax, 8
	jz		.skiploop

.xloop:
	movq		mm2, mm1
	movq		mm1, mm0

	movd		mm0, dword [ecx+eax]
	pxor		mm3, mm3

	punpcklbw	mm0, mm3
	movq		mm5, mm4		;mm5 = x[i-1]

	movq		mm4, mm0		;mm4 = x[i+0]
	paddw		mm5, mm5		;mm5 = 2*x[i-1]

	paddw		mm5, mm5		;mm5 = 4*x[i-1]
	paddw		mm1, mm0		;mm1 = x[i-1] + x[i]
	paddw		mm2, mm1		;mm2 = x[i-2] + 2*x[i-1] + x[i]

	psubw		mm5, mm2		;mm5 = x[i-2] - 2*x[i-1] + x[i]

	por		mm5, mm7		;mm5 = -1 | x[i-2] - 2*x[i-1] + x[i]

	psllw		mm5, 6

	pmaddwd		mm5, mm6

	punpckldq	mm3, mm5

	paddd		mm3, mm5

	punpckhwd	mm3, mm3

	punpckhdq	mm3, mm3

	paddw		mm2, mm3

	psraw		mm2, 2

	packuswb	mm2, mm2

	movd		dword [edx+eax], mm2

	add		eax, 4
	jne		.xloop
.skiploop:
	mov		eax, -4
	add		edx, 4
	sub		ebx, 1
	jne		.xloop

	pop		ebx
	pop		esi
	pop		edi
	pop		ebp
	ret


	global	_asm_chromasmoother_FilterHorizontalMPEG2_MMX
_asm_chromasmoother_FilterHorizontalMPEG2_MMX:
	push		ebp
	push		edi
	push		esi
	push		ebx

	mov		eax, [esp+12+16]
	mov		ecx, [esp+8+16]
	add		eax, eax
	mov		edx, [esp+4+16]
	add		eax, eax

	movq		mm6, qword [luma_coeff2]
	pcmpeqb		mm7, mm7
	pxor		mm3, mm3
	psllq		mm7, 48

	lea		ecx, [ecx+eax]
	lea		edx, [edx+eax-4]
	xor		eax, -4

	movd		mm4, dword [ecx+eax+4]
	punpcklbw	mm4, mm3

	mov		ebx, 2
	add		eax, 8
	jz		.skiploop

.xloop:
	movd		mm0, dword [ecx+eax]
	pxor		mm3, mm3

	punpcklbw	mm0, mm3
	movq		mm1, mm4

	movq		mm2, mm4
	psubw		mm1, mm0		;mm1 = x[i-1] - x[i]

	movq		mm4, mm0
	por			mm1, mm7		;mm1 = -1 | x[i-1] - x[i]

	paddw		mm2, mm0		;mm2 = x[i-1] + x[i]
	psllw		mm1, 6

	pmaddwd		mm1, mm6

	punpckldq	mm3, mm1

	paddd		mm3, mm1		;mm3 = luma(x[i-1] - x[i]) | ? | ? | ?

	punpckhwd	mm3, mm3

	punpckhdq	mm3, mm3

	paddw		mm2, mm3

	psraw		mm2, 1

	packuswb	mm2, mm2

	movd		dword [edx+eax], mm2

	add		eax, 4
	jne		.xloop
.skiploop:
	mov		eax, -4
	add		edx, 4
	sub		ebx, 1
	jne		.xloop

	pop		ebx
	pop		esi
	pop		edi
	pop		ebp
	ret


	global	_asm_chromasmoother_FilterVerticalMPEG1_MMX
_asm_chromasmoother_FilterVerticalMPEG1_MMX:
	push		ebp
	push		edi
	push		esi
	push		ebx

	mov			ebp, [esp+8+16]
	mov			eax, [esp+12+16]
	mov			ebx, [ebp-12]
	add			eax, eax
	mov			edi, [esp+4+16]
	add			eax, eax
	mov			ecx, [ebp-8]
	add			ebx, eax
	mov			edx, [ebp-4]
	add			ecx, eax
	add			edx, eax
	add			edi, eax
	neg			eax

	movq		mm6, qword [luma_coeff4]
	pcmpeqb		mm7, mm7
	pxor		mm3, mm3
	psllq		mm7, 48

.xloop:
	movd		mm1, dword [ecx+eax]
	pxor		mm3, mm3
	movd		mm0, dword [ebx+eax]
	punpcklbw	mm1, mm3
	movd		mm2, dword [edx+eax]
	punpcklbw	mm0, mm3
	movq		mm4, mm1
	punpcklbw	mm2, mm3
	paddw		mm0, mm2
	paddw		mm1, mm1
	paddw		mm1, mm0
	psllw		mm4, 2
	psubw		mm4, mm1
	por			mm4, mm7
	psllw		mm4, 6
	pmaddwd		mm4, mm6
	punpckldq	mm5, mm4
	paddd		mm4, mm5
	punpckhwd	mm4, mm4
	punpckhdq	mm4, mm4
	paddw		mm1, mm4
	psraw		mm1, 2
	packuswb	mm1, mm1
	movd		dword [edi+eax], mm1
	add			eax, 4
	jne			.xloop

	pop		ebx
	pop		esi
	pop		edi
	pop		ebp
	ret


	end
