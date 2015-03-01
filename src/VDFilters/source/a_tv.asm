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

	segment	.text

	extern	_MMX_enabled:byte

;	asm_tv_average5col(Pixel *dst, Pixel *r1, Pixel *r2, Pixel *r3, Pixel *r4, Pixel *r5, long w);

	global	_asm_tv_average5col	

%define p_dst	[esp+4+28]
%define p_r1	[esp+8+28]
%define p_r2	[esp+12+28]
%define p_r3	[esp+16+28]
%define p_r4	[esp+20+28]
%define p_r5	[esp+24+28]
%define p_width	[esp+28+28]

_asm_tv_average5col:
	test	byte [_MMX_enabled],1
	jnz	_asm_tv_average5col_MMX

	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	ebp,p_width
	neg	ebp
	shl	ebp,2

	sub	p_dst,ebp
	sub	p_r1,ebp
	sub	p_r2,ebp
	sub	p_r3,ebp
	sub	p_r4,ebp
	sub	p_r5,ebp

tv5col_loop:
	mov	esi,p_r2
	mov	edi,p_r3

	mov	eax,[esi+ebp]
	mov	ecx,[edi+ebp]
	mov	ebx,eax
	mov	edx,ecx
	and	eax,00ff00ffh
	and	ecx,00ff00ffh
	and	ebx,0000ff00h
	and	edx,0000ff00h
	add	eax,ecx
	add	ebx,edx

	mov	esi,p_r4
	mov	edi,p_r1

	mov	ecx,[esi+ebp]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	add	eax,ecx
	add	ebx,edx

	add	eax,eax
	add	ebx,ebx

	mov	ecx,[edi+ebp]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	add	eax,ecx
	add	ebx,edx

	mov	esi,p_r5
	mov	edi,p_dst

	mov	ecx,[esi+ebp]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	add	eax,ecx
	add	ebx,edx

	mov	edx,[edi+ebp]		;get old pixel
	mov	esi,000000ffh
	mov	edi,0000ff00h
	and	esi,edx			;esi = c<blu>
	and	edi,edx			;edi = c<grn>
	and	edx,00ff0000h		;edx = c<red>

	shr	eax,3
	and	ebx,0007f800h
	shr	ebx,3			;ebx = d<grn>
	mov	ecx,eax
	and	eax,000000ffh		;eax = d<blu>
	and	ecx,00ff0000h		;ecx = d<red>

	sub	edx,ecx			;edx = (c-d)<red>
	sub	edi,ebx			;edi = (c-d)<grn>
	sub	esi,eax			;esi = (c-d)<blu>

	sar	edi,8
	push	eax
	sar	edx,16
	push	ebx

	lea	edx,[edx+edx*2]		;edx = r*3
	lea	eax,[edi+edi*4]		;eax = g*5

	shl	eax,2			;eax = g*20
	add	edx,esi			;edx = r*3 + b

	lea	eax,[eax+eax*2]		;eax = g*60
	lea	edx,[edx+edx*2]		;edx = r*9 + b*3

	add	edx,edx			;edx = r*18 + b*6
	add	eax,edi			;eax = g*61

	add	eax,edx			;eax = r*18 + g*61 + b*6

	lea	edx,[eax+eax*2]		;edx = r*54 + g*183 + b*18

	add	edx,esi			;edx = r*54 + g*183 + b*19

;	imul	edx,54
;	imul	edi,183
;	imul	esi,19

	sar	edx,8
	pop	ebx
	shr	ebx,8
	pop	eax
	shr	ecx,16
	add	ebx,edx			;ebx = grn
	add	eax,edx			;eax = blu
	add	ecx,edx			;ecx = red

;CLIP	macro	x
;	local	@1,@2
;	cmp	x,0
;	jge	@1
;	mov	x,0
;@1:
;	cmp	x,256
;	jl	@2
;	mov	x,255
;@2:
;%endmacro

%macro CLIP	1
	mov	edx,000000ffh
	mov	esi,%1

	shr	esi,24
	sub	edx,%1

	shr	edx,24
	xor	esi,000000ffh

	or	%1,edx

	and	%1,esi

%endmacro

	;------------------

	mov	edx,000000ffh
	mov	esi,ecx

	shr	esi,24
	sub	edx,ecx

	shr	edx,24
	xor	esi,000000ffh

	or	ecx,edx
	mov	edx,000000ffh

	and	ecx,esi
	mov	esi,ebx

	shr	esi,24
	sub	edx,ebx

	shr	edx,24
	xor	esi,000000ffh

	or	ebx,edx
	mov	edx,000000ffh

	and	ebx,esi
	mov	esi,eax

	shr	esi,24
	sub	edx,eax

	shr	edx,24
	xor	esi,000000ffh

	or	eax,edx

	and	eax,esi

	;-----------------

	shl	ecx,16
	mov	edx,p_dst
	shl	ebx,8
	add	eax,ecx

	add	eax,ebx
	mov	[edx+ebp],eax

	add	ebp,4
	jne	tv5col_loop

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

	ret

y_coeff	dq	0000003600b70013h
all_80w	dq	0080008000800080h
all_80b	dq	8080808080808080h

_asm_tv_average5col_MMX:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	ebp,p_width
	neg	ebp
	shl	ebp,2

	movq	mm6,[y_coeff]
	pxor	mm7,mm7

	mov	eax,p_r1
	mov	ebx,p_r2
	mov	ecx,p_r3
	mov	edx,p_r4
	mov	esi,p_r5
	mov	edi,p_dst

	sub	eax,ebp
	sub	ebx,ebp
	sub	ecx,ebp
	sub	edx,ebp
	sub	esi,ebp
	sub	edi,ebp

	movd	mm0,dword [eax+ebp]		;mm0 = [1/byte]

	movd	mm4,dword [esi+ebp]		;mm4 = [5/byte]

	movd	mm1,dword [ebx+ebp]		;mm1 = [2/byte]
	punpcklbw mm0,mm7		;mm0 = [1/word]

	punpcklbw mm4,mm7		;mm4 = [5/word]
	jmp	short tv5col_loop_MMX_start

tv5col_loop_MMX:
	movd	mm0,dword [eax+ebp]		;mm0 = [1/byte]
	packsswb mm5,mm5		;[tail]

	movd	mm4,dword [esi+ebp]		;mm4 = [5/byte]
	pxor	mm5,mm3			;[tail]

	movd	mm1,dword [ebx+ebp]		;mm1 = [2/byte]
	punpcklbw mm0,mm7		;mm0 = [1/word]

	punpcklbw mm4,mm7		;mm4 = [5/word]
	movd	dword [edi+ebp-4],mm5		;[tail]

tv5col_loop_MMX_start:
	movd	mm2,dword [ecx+ebp]		;mm2 = [3/byte]
	punpcklbw mm1,mm7		;mm1 = [2/word]

	movd	mm3,dword [edx+ebp]		;mm3 = [4/byte]
	punpcklbw mm2,mm7		;mm2 = [3/word]

	paddw	mm1,mm2			;mm1 = [2+3]/word
	punpcklbw mm3,mm7		;mm3 = [4/word]

	paddw	mm0,mm4			;mm0 = [1+5]/word
	paddw	mm1,mm3			;mm1 = [2+3+4]/word

	movd	mm5,dword [edi+ebp]		;mm5 = [d/byte]
	paddw	mm1,mm1			;mm1 = [(2+3+4)*2]/word

	punpcklbw mm5,mm7		;mm5 = [d/word]
	paddw	mm0,mm1			;mm0 = [1+(2+3+4)*2+5]/word

	psrlw	mm0,3			;mm0 = 5x5 average (word)

	movq	mm1,mm0			;mm1 = [d/word]
	pmaddwd	mm0,mm6			;mm0 = 5x5 average Y in two parts

	movq	mm2,[all_80w]	;mm2 = [80h/word]
	pmaddwd	mm1,mm6			;mm1 = source Y in two parts

	movq	mm3,[all_80b]	;mm3 = [80h/byte]
	psubw	mm5,mm2

	;(stall)

	psubd	mm1,mm0			;[tail]

	movq	mm0,mm1			;[tail]
	psrlq	mm1,32			;[tail]

	paddd	mm0,mm1			;[tail]

	psrad	mm0,8			;[tail]

	punpcklwd mm0,mm0		;[tail]

	punpcklwd mm0,mm0		;[tail]
	add	ebp,4

	paddw	mm5,mm0			;[tail]
	jne	tv5col_loop_MMX

	packsswb mm5,mm5		;[tail]
	pxor	mm5,mm3			;[tail]
	movd	dword [edi+ebp-4],mm5		;[tail]

	emms
	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

	ret

;	asm_tv_average5row(Pixel *src, Pixel *dst, int count)

	global	_asm_tv_average5row	

_asm_tv_average5row:
	test	byte [_MMX_enabled], 1
	jnz	_asm_tv_average5row_MMX

	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+4+28]
	mov	edi,[esp+8+28]

	mov	ebp,4
	sub	ebp,[esp+12+28]
	shl	ebp,2

	;----------
	;
	;	5*[2] + 2*[3] + 1*[4]

	mov	eax,[esi+0]
	mov	ecx,[esi+4]
	mov	ebx,eax
	mov	edx,ecx
	and	eax,00ff00ffh
	and	ebx,0000ff00h
	and	ecx,00ff00ffh
	and	edx,0000ff00h
	lea	eax,[eax+eax*4]
	lea	ebx,[ebx+ebx*4]
	lea	eax,[eax+ecx*2]
	lea	ebx,[ebx+edx*2]

	mov	ecx,[esi+8]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	add	eax,ecx
	add	ebx,edx

	shr	eax,3
	and	ebx,0007f800h
	shr	ebx,3
	and	eax,00ff00ffh

	or	eax,ebx
	mov	[edi+0],eax

	;----------
	;
	;	3*[1] + 2*[2] + 2*[3] + 1*[4]

	mov	eax,[esi+4]
	mov	ecx,[esi+8]
	mov	ebx,eax
	mov	edx,ecx
	and	eax,00ff00ffh
	and	ecx,00ff00ffh
	and	ebx,0000ff00h
	and	edx,0000ff00h
	add	eax,ecx
	add	ebx,edx

	add	eax,eax
	add	ebx,ebx

	mov	ecx,[esi+0]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	lea	ecx,[ecx+ecx*2]
	lea	edx,[edx+edx*2]
	add	eax,ecx
	add	ebx,edx

	mov	ecx,[esi+12]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	add	eax,ecx
	add	ebx,edx

	shr	eax,3
	and	ebx,0007f800h
	shr	ebx,3
	and	eax,00ff00ffh

	or	eax,ebx
	mov	[edi+4],eax

	;----------

	sub	esi,ebp
	sub	edi,ebp

tv5row_loop:
	mov	eax,[esi+ebp+4]
	mov	ecx,[esi+ebp+8]
	mov	ebx,eax
	mov	edx,ecx
	and	eax,00ff00ffh
	and	ecx,00ff00ffh
	and	ebx,0000ff00h
	and	edx,0000ff00h
	add	eax,ecx
	add	ebx,edx

	mov	ecx,[esi+ebp+12]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	add	eax,ecx
	add	ebx,edx

	add	eax,eax
	add	ebx,ebx

	mov	ecx,[esi+ebp+0]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	add	eax,ecx
	add	ebx,edx

	mov	ecx,[esi+ebp+16]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	add	eax,ecx
	add	ebx,edx

	shr	eax,3
	and	ebx,0007f800h
	shr	ebx,3
	and	eax,00ff00ffh

	or	eax,ebx
	mov	[edi+ebp+8],eax
	add	ebp,4
	jne	tv5row_loop

	;----------
	;
	;	3*[1] + 2*[2] + 2*[3] + 1*[4]

	mov	eax,[esi+ebp+4]
	mov	ecx,[esi+ebp+8]
	mov	ebx,eax
	mov	edx,ecx
	and	eax,00ff00ffh
	and	ecx,00ff00ffh
	and	ebx,0000ff00h
	and	edx,0000ff00h
	add	eax,ecx
	add	ebx,edx

	add	eax,eax
	add	ebx,ebx

	mov	ecx,[esi+ebp+12]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	lea	ecx,[ecx+ecx*2]
	lea	edx,[edx+edx*2]
	add	eax,ecx
	add	ebx,edx

	mov	ecx,[esi+ebp+0]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	add	eax,ecx
	add	ebx,edx

	shr	eax,3
	and	ebx,0007f800h
	shr	ebx,3
	and	eax,00ff00ffh

	or	eax,ebx
	mov	[edi+ebp+8],eax

	;----------
	;
	;	5*[2] + 2*[3] + 1*[4]

	mov	eax,[esi+ebp+12]
	mov	ecx,[esi+ebp+8]
	mov	ebx,eax
	mov	edx,ecx
	and	eax,00ff00ffh
	and	ebx,0000ff00h
	and	ecx,00ff00ffh
	and	edx,0000ff00h
	lea	eax,[eax+eax*4]
	lea	ebx,[ebx+ebx*4]
	lea	eax,[eax+ecx*2]
	lea	ebx,[ebx+edx*2]

	mov	ecx,[esi+ebp+4]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	add	eax,ecx
	add	ebx,edx

	shr	eax,3
	and	ebx,0007f800h
	shr	ebx,3
	and	eax,00ff00ffh

	or	eax,ebx
	mov	[edi+ebp+12],eax

	;----------

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

	ret

by_3	dq	0003000300030003h
by_5	dq	0005000500050005h

_asm_tv_average5row_MMX:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+4+28]
	mov	edi,[esp+8+28]

	mov	ebp,4
	sub	ebp,[esp+12+28]
	shl	ebp,2

	pxor	mm7,mm7

	;----------
	;
	;	5*[2] + 2*[3] + 1*[4]

	movd	mm0,dword [esi+0]
	movd	mm1,dword [esi+4]
	punpcklbw mm0,mm7
	punpcklbw mm1,mm7
	pmullw	mm0,[by_5]
	paddw	mm1,mm1
	movd	mm2,dword [esi+8]
	punpcklbw mm2,mm7
	paddw	mm0,mm1
	paddw	mm0,mm2
	psrlw	mm0,3
	packuswb mm0,mm0
	movd	dword [edi+0],mm0

	;----------
	;
	;	3*[1] + 2*[2] + 2*[3] + 1*[4]

	movd	mm0,dword [esi+0]
	movd	mm1,dword [esi+4]
	movd	mm2,dword [esi+8]
	movd	mm3,dword [esi+12]
	punpcklbw mm0,mm7
	punpcklbw mm1,mm7
	punpcklbw mm2,mm7
	punpcklbw mm3,mm7
	pmullw	mm0,[by_3]
	paddw	mm1,mm2
	paddw	mm1,mm1
	paddw	mm3,mm0
	paddw	mm1,mm3
	psrlw	mm0,3
	packuswb mm0,mm0
	movd	dword [edi+4],mm0

	;----------

	sub	esi,ebp
	sub	edi,ebp

tv5row_loop_MMX:
	movd	mm0,dword [esi+ebp+0]

	movd	mm1,dword [esi+ebp+4]
	punpcklbw mm0,mm7

	movd	mm2,dword [esi+ebp+8]
	punpcklbw mm1,mm7

	movd	mm3,dword [esi+ebp+12]
	punpcklbw mm2,mm7

	paddw	mm1,mm2
	punpcklbw mm3,mm7

	movd	mm4,dword [esi+ebp+16]
	paddw	mm1,mm3

	paddw	mm1,mm1

	paddw	mm0,mm1
	punpcklbw mm4,mm7

	paddw	mm0,mm4

	psrlw	mm0,3

	packuswb mm0,mm0

	movd	dword [edi+ebp+8],mm0

	add	ebp,4
	jne	tv5row_loop_MMX

	;----------
	;
	;	3*[1] + 2*[2] + 2*[3] + 1*[4]

	mov	eax,[esi+ebp+4]
	mov	ecx,[esi+ebp+8]
	mov	ebx,eax
	mov	edx,ecx
	and	eax,00ff00ffh
	and	ecx,00ff00ffh
	and	ebx,0000ff00h
	and	edx,0000ff00h
	add	eax,ecx
	add	ebx,edx

	add	eax,eax
	add	ebx,ebx

	mov	ecx,[esi+ebp+12]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	lea	ecx,[ecx+ecx*2]
	lea	edx,[edx+edx*2]
	add	eax,ecx
	add	ebx,edx

	mov	ecx,[esi+ebp+0]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	add	eax,ecx
	add	ebx,edx

	shr	eax,3
	and	ebx,0007f800h
	shr	ebx,3
	and	eax,00ff00ffh

	or	eax,ebx
	mov	[edi+ebp+8],eax

	;----------
	;
	;	5*[2] + 2*[3] + 1*[4]

	mov	eax,[esi+ebp+12]
	mov	ecx,[esi+ebp+8]
	mov	ebx,eax
	mov	edx,ecx
	and	eax,00ff00ffh
	and	ebx,0000ff00h
	and	ecx,00ff00ffh
	and	edx,0000ff00h
	lea	eax,[eax+eax*4]
	lea	ebx,[ebx+ebx*4]
	lea	eax,[eax+ecx*2]
	lea	ebx,[ebx+edx*2]

	mov	ecx,[esi+ebp+4]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	add	eax,ecx
	add	ebx,edx

	shr	eax,3
	and	ebx,0007f800h
	shr	ebx,3
	and	eax,00ff00ffh

	or	eax,ebx
	mov	[edi+ebp+12],eax

	;----------

	emms
	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp

	ret

	end
