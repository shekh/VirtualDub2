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
;
;--------------
;
;	We include three versions of the resize filter here.  The first
;	version is the integer point-sampling filter.  There's no sense
;	in optimizing a point-sampled filter with the FPU since the
;	bottlenecks are address computation and memory bandwidth.  MMX
;	might help on Pentium-MMXs with qword-length writes, but the
;	point-sampling filter runs so fast already that there's no
;	reason to try to speed it up.  AMD K6's and PPro/PIIs will
;	write-allocate cache lines, so there's absolutely no win there.
;
;	The next 3 versions of the filter are bilinearly filtered.  The
;	integer version is included for systems with a slow FPU or no
;	FPU at all (386).  It is *very* slow at 10 multiplies per pixel.
;	It could be significantly sped up by computing all four
;	corner coefficients and then using those; with one red/blue
;	multiply and one green multiply per sampled pixel, the routine
;	could probably be sped up by 20% or so, but it's not worth the
;	work.  The MMX version works the same way; it could be modified
;	similarly, but MMX multiplies are so much faster that it would
;	slow the routine down.
;
;	The last version is the most interesting.  It's an FPU version.
;	It works by kicking the FPU into 80-bit mode and then processing
;	pixels as packed 64-bit integers: r/b in one 32-bit half, and
;	green in the other.  FPU multiplies are almost as fast as MMX
;	ones, so we gain a lot of speed.  The FPU version is about 3x
;	faster than integer on a Pentium, and still a lot faster on a
;	486.  It's the first ASM float code I've ever written, so it's
;	really messy; kind of weird that my first FPU code would have
;	nothing to do with numerics, eh?
;
;	Approximate ideal cycle counts per pixel (Pentium):
;
;		Point sampled:	3
;		Integer:	130-150
;		FPU:		40-50
;		MMX:		10-20

	segment	.rdata, align=16

x0002000200020002	dq	0002000200020002h
x0004000400040004	dq	0004000400040004h
x0008000800080008	dq	0008000800080008h
x0000200000002000	dq	0000200000002000h

	align 16
MMX_roundval		dq	0000200000002000h, 0000200000002000h


	segment	.text

	extern _MMX_enabled : byte
	extern _FPU_enabled : byte


;**************************************************************************

	global	_asm_resize_interp_row_run	

;asm_resize_interp_row_run(
;	[esp+ 4] void *dst,
;	[esp+ 8] void *src,
;	[esp+12] ulong width,
;	[esp+16] __int64 xaccum,
;	[esp+24] __int64 x_inc);


_asm_resize_interp_row_run:
	test	byte [_MMX_enabled], 1
	jnz	_asm_resize_interp_row_run_MMX

	push	ebp
	push	edi
	push	esi
	push	ebx

	sub	esp,8

	mov	ebp,[esp+12+16+8]
	mov	edi,[esp+4+16+8]
	mov	esi,[esp+8+16+8]

	lea	edi,[edi+ebp*4]
	neg	ebp

;******************************************************
;
;	[esp+ 4] x_frac2
;	[esp+ 0] x_frac

	mov	eax,[esp+16+16+8]
	mov	ebx,256
	shr	eax,24
	sub	ebx,eax
	mov	[esp],eax
	mov	[esp+4],ebx
	shr	esi,2
	mov	ecx,[esp+20+16+8]
	add	esi,ecx

colloop_interp_row:
	mov	eax,[esi*4]
	mov	ecx,[esi*4+4]
	mov	ebx,eax
	mov	edx,ecx
	and	eax,00ff00ffh
	and	ebx,0000ff00h
	and	ecx,00ff00ffh
	and	edx,0000ff00h
	imul	eax,[esp+4]		;x_frac2
	imul	ebx,[esp+4]		;x_frac2
	imul	ecx,[esp]		;x_frac
	imul	edx,[esp]		;x_frac
	add	eax,ecx
	add	ebx,edx

	shr	eax,8
	and	ebx,00ff0000h
	shr	ebx,8
	and	eax,00ff00ffh

	or	eax,ebx			;[data write ] u
	mov	edx,[esp+28+16+8]	;[frac update] v x_inc high

	mov	ebx,[esp+16+16+8]	;[frac update] u x_accum low
	mov	ecx,[esp+24+16+8]	;[frac update] v x_inc low

	mov	[edi+ebp*4],eax		;[data write ] u
	add	ebx,ecx			;[frac update] v

	adc	esi,edx			;[frac update] u
	mov	[esp+16+16+8],ebx	;[frac update] v x_accum low

	shr	ebx,24			;[frac update] u
	mov	eax,256			;[frac update] v

	mov	[esp],ebx		;[frac update] u
	sub	eax,ebx			;[frac update] v

	mov	[esp+4],eax		;[frac update] u
	inc	ebp

	jne	colloop_interp_row

	add	esp,8

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

	segment	.rdata

x0000FFFF0000FFFF	dq	0000FFFF0000FFFFh
x0000010100000101	dq	0000010100000101h
x0100010001000100	dq	0100010001000100h

	segment	.text

	align 16
_asm_resize_interp_row_run_MMX:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	esi,[esp+8+16]
	mov	edi,[esp+4+16]
	mov	ebp,[esp+12+16]

	movd	mm4,dword [esp+16+16]
	pxor	mm7,mm7
	movd	mm6,dword [esp+24+16]
	punpckldq mm4,mm4
	punpckldq mm6,mm6

	shr	esi,2

	mov	eax,[esp+16+16]
	mov	ebx,[esp+20+16]
	add	esi,ebx
	mov	ebx,[esp+24+16]
	mov	ecx,[esp+28+16]

	shl	ebp,2
	add	edi,ebp
	neg	ebp


colloop_interp_row_MMX:
	movd		mm1,dword [esi*4+4]
	movq		mm5,mm4

	movd		mm0,dword [esi*4]
	punpcklbw	mm1,mm7

	punpcklbw	mm0,mm7
	psrld		mm5,24

	movq		mm3,[x0100010001000100]
	packssdw	mm5,mm5

	pmullw		mm1,mm5
	psubw		mm3,mm5

	pmullw		mm0,mm3
	paddd		mm4,mm6

	;stall
	;stall

	;stall
	;stall

	paddw		mm0,mm1

	psrlw		mm0,8
	add		eax,ebx

	adc		esi,ecx
	packuswb	mm0,mm0

	movd		dword [edi+ebp],mm0

	add		ebp,4
	jnz		colloop_interp_row_MMX

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

;**************************************************************************

	global	_asm_resize_interp_col_run	

;asm_resize_interp_col_run(
;	[esp+ 4] void *dst,
;	[esp+ 8] void *src1,
;	[esp+12] void *src2,
;	[esp+16] ulong width,
;	[esp+20] ulong yaccum);


_asm_resize_interp_col_run:
	test	byte [_MMX_enabled], 1
	jnz	_asm_resize_interp_col_run_MMX

	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	sub	esp,8

	mov	esi,[esp+8+28+8]
	mov	edi,[esp+12+28+8]
	mov	ebp,[esp+16+28+8]
	sub	edi,esi

;******************************************************
;
;	[esp+ 4] x_frac2
;	[esp+ 0] x_frac

	mov	eax,[esp+20+28+8]
	shr	eax,8
	and	eax,255
	mov	[esp],eax
	xor	eax,255
	inc	eax
	mov	[esp+4],eax

colloop_interp_col:
	mov	eax,[esi]
	mov	ecx,[esi+edi]
	mov	ebx,eax
	mov	edx,ecx
	and	eax,00ff00ffh
	and	ebx,0000ff00h
	and	ecx,00ff00ffh
	and	edx,0000ff00h
	imul	eax,[esp+4]		;x_frac2
	imul	ebx,[esp+4]		;x_frac2
	imul	ecx,[esp]		;x_frac
	imul	edx,[esp]		;x_frac
	add	eax,ecx
	add	ebx,edx

	shr	eax,8
	and	ebx,00ff0000h
	shr	ebx,8
	and	eax,00ff00ffh

	or	eax,ebx			;[data write ] u
	mov	edx,[esp+4+28+8]	;[data write ] v

	mov	[edx],eax		;[data write ] u
	add	edx,4			;[data write ] v

	mov	[esp+4+28+8],edx	;[data write ] v
	add	esi,4

	sub	ebp,1
	jne	colloop_interp_col

	add	esp,8

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

_asm_resize_interp_col_run_MMX:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+8+28]
	mov	edx,[esp+12+28]
	mov	edi,[esp+4+28]
	mov	ebp,[esp+16+28]

	movd	mm4,dword [esp+20+28]
	pxor	mm7,mm7
	punpcklwd mm4,mm4
	punpckldq mm4,mm4
	psrlw	mm4,8
	pxor	mm4,[x0000FFFF0000FFFF]
	paddw	mm4,[x0000010100000101]

	shl	ebp,2
	add	edi,ebp
	add	esi,ebp
	add	edx,ebp
	neg	ebp

colloop_interp_col_MMX:
	movd	mm0,dword [esi+ebp]
	movd	mm2,dword [edx+ebp]

	punpcklbw mm0,mm7
	punpcklbw mm2,mm7

	movq	mm1,mm0
	punpcklwd mm0,mm2
	punpckhwd mm1,mm2

	pmaddwd	mm0,mm4
	pmaddwd	mm1,mm4

	psrad	mm0,8
	psrad	mm1,8

	packssdw mm0,mm1
	packuswb mm0,mm0

	movd	dword [edi+ebp],mm0

	add	ebp,4
	jnz	colloop_interp_col_MMX

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

;--------------------------------------------------------------------------

	global	_asm_resize_ccint_MMX	

;asm_resize_ccint_MMX(dst, src1, src2, src3, src4, count, xaccum, xinc, tbl);

_asm_resize_ccint_MMX:
	push	ebx
	push	esi
	push	edi
	push	ebp

	mov	ebx,[esp + 4 + 16]	;ebx = dest addr
	mov	ecx,[esp + 24 + 16]	;ecx = count

	mov	ebp,[esp + 32 + 16]	;ebp = increment
	mov	edi,ebp			;edi = increment
	shl	ebp,16			;ebp = fractional increment
	mov	esi,[esp + 28 + 16]	;esi = 16:16 position
	shr	edi,16			;edi = integer increment
	mov	[esp + 32 + 16],ebp	;xinc = fractional increment
	mov	ebp,esi			;ebp = 16:16 position
	shr	esi,16			;esi = integer position
	shl	ebp,16			;ebp = fraction
	mov	[esp + 28 + 16], ebp	;xaccum = fraction

	mov	eax,[esp + 8 + 16]

	shr	ebp,24			;ebp = fraction (0...255)
	mov	[esp + 20 + 16],edi	;src4 = integer increment
	shl	ebp,4			;ebp = fraction*16
	mov	edi,ebp
	mov	ebp,[esp + 4 + 16]	;ebp = destination

	shr	eax, 2
	add	eax, esi
	shl	ecx, 2			;ecx = count*4
	lea	ebp, [ebp+ecx-4]
	neg	ecx			;ecx = -count*4

	movq		mm6,[x0000200000002000]
	pxor		mm7,mm7

	mov		edx,[esp+28+16]		;edx = fractional accumulator
	mov		esi,[esp+32+16]		;esi = fractional increment

	mov			ebx,[esp+36+16]		;ebx = coefficient pointer

	movd		mm1,dword [eax*4+4]
	punpcklbw	mm0,mm7				;mm0 = [a1][r1][g1][b1]

	;borrow stack pointer
	push		0				;don't crash
	push		dword [fs:0]
	mov			dword [fs:0], esp
	mov			esp,[esp+20+16+8]	;esp = integer increment
	jmp			short ccint_loop_MMX_start

	;EAX	source pointer / 4
	;EBX	coefficient pointer
	;ECX	count
	;EDX	fractional accumulator
	;ESI	fractional increment
	;EDI	coefficient offset
	;ESP	integer increment
	;EBP	destination pointer

	align		16
ccint_loop_MMX:
	movd		mm0,dword [eax*4]
	packuswb	mm2,mm2				;mm0 = [a][r][g][b][a][r][g][b]

	movd		mm1,dword [eax*4+4]
	punpcklbw	mm0,mm7				;mm0 = [a1][r1][g1][b1]

	movd		dword [ebp+ecx],mm2
ccint_loop_MMX_start:
	movq		mm4,mm0				;mm0 = [a1][r1][g1][b1]

	movd		mm2,dword [eax*4+8]
	punpcklbw	mm1,mm7				;mm1 = [a2][r2][g2][b2]

	movd		mm3,dword [eax*4+12]
	punpcklbw	mm2,mm7				;mm2 = [a3][r3][g3][b3]

	punpcklbw	mm3,mm7				;mm3 = [a4][r4][g4][b4]
	movq		mm5,mm2				;mm2 = [a3][r3][g3][b3]

	add		edx,esi				;add fractional incrmeent
	punpcklwd	mm0,mm1				;mm0 = [g2][g1][b2][b1]

	pmaddwd		mm0,[ebx+edi]
	punpcklwd	mm2,mm3				;mm2 = [g4][g3][b4][b3]

	pmaddwd		mm2,[ebx+edi+8]
	punpckhwd	mm4,mm1				;mm4 = [a2][a1][r2][r1]

	pmaddwd		mm4,[ebx+edi]
	punpckhwd	mm5,mm3				;mm5 = [a4][a3][b4][b3]

	pmaddwd		mm5,[ebx+edi+8]
	paddd		mm0,mm6

	adc		eax,esp				;add integer increment and fractional bump to offset
	mov		edi,0ff000000h

	paddd		mm2,mm0				;mm0 = [ g ][ b ]
	paddd		mm4,mm6

	psrad		mm2,14
	paddd		mm4,mm5				;mm4 = [ a ][ r ]

	and		edi,edx
	psrad		mm4,14

	shr		edi,20				;edi = fraction (0...255)*16
	add		ecx,4

	packssdw	mm2,mm4				;mm0 = [ a ][ r ][ g ][  b ]
	jnc		ccint_loop_MMX

	packuswb	mm2,mm2				;mm0 = [a][r][g][b][a][r][g][b]
	movd		dword [ebp],mm2

	mov		esp, dword [fs:0]
	pop		dword [fs:0]
	pop		eax

	pop		ebp
	pop		edi
	pop		esi
	pop		ebx
	ret

;------------------------------------------------------
;
; void asm_resize_ccint_col(
;	[esp+ 4] Pixel32 *dst,
;	[esp+ 8] Pixel32 *src1,
;	[esp+12] Pixel32 *src2,
;	[esp+16] Pixel32 *src3,
;	[esp+20] Pixel32 *src4,
;	[esp+24] unsigned long count,
;	[esp+28] int *tbl);
;

	global	_asm_resize_ccint_col
_asm_resize_ccint_col:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	eax,[esp+4+16]
	mov	edx,[esp+24+16]
	shl	edx,2
	neg	edx
	sub	eax,edx
	mov	[esp+24+16],edx
	mov	[esp+4+16],eax

	mov	eax,[esp+20+16]
	mov	ebx,[esp+16+16]
	mov	ecx,[esp+12+16]
	mov	edx,[esp+8+16]

	push	eax
	push	ebx
	push	ecx
	push	edx

	mov	edx,[esp+28+32]	;eax = coefficient pointer
	push	dword [edx+12]
	push	dword [edx+8]
	push	dword [edx+4]
	push	dword [edx+0]

.pixelloop:
	mov	esi,00002000h	;red accum = rounder
	mov	edx,[esp+16]	;load pointer to row 1
	mov	edi,00002000h	;green accum = rounder
	mov	ebp,00002000h	;blue accum = rounder

	mov	eax,[edx]	;fetch pixel 1
	mov	ebx,0000ff00h
	add	edx,4
	and	ebx,eax
	mov	[esp+16],edx
	mov	ecx,eax
	shr	ebx,8
	mov	edx,[esp+20]	;load pointer to row 2
	imul	ebx,[esp+0]	;scale green
	shr	eax,16
	and	ecx,000000ffh
	imul	ecx,[esp+0]	;scale blue
	add	edi,ebx
	and	eax,000000ffh
	imul	eax,[esp+0]	;scale red
	add	esi,eax
	add	ebp,ecx

	mov	eax,[edx]	;fetch pixel 2
	mov	ebx,0000ff00h
	add	edx,4
	and	ebx,eax
	mov	[esp+20],edx
	mov	ecx,eax
	shr	ebx,8
	mov	edx,[esp+24]	;load pointer to row 3
	imul	ebx,[esp+4]	;scale green
	shr	eax,16
	and	ecx,000000ffh
	imul	ecx,[esp+4]	;scale blue
	add	edi,ebx
	and	eax,000000ffh
	imul	eax,[esp+4]	;scale red
	add	esi,eax
	add	ebp,ecx

	mov	eax,[edx]	;fetch pixel 3
	mov	ebx,0000ff00h
	add	edx,4
	and	ebx,eax
	mov	[esp+24],edx
	mov	ecx,eax
	shr	ebx,8
	mov	edx,[esp+28]	;load pointer to row 4
	imul	ebx,[esp+8]	;scale green
	shr	eax,16
	and	ecx,000000ffh
	imul	ecx,[esp+8]	;scale blue
	add	edi,ebx
	and	eax,000000ffh
	imul	eax,[esp+8]	;scale red
	add	esi,eax
	add	ebp,ecx

	mov	eax,[edx]	;fetch pixel 4
	mov	ebx,0000ff00h
	add	edx,4
	and	ebx,eax
	mov	[esp+28],edx
	mov	ecx,eax
	shr	ebx,8
	mov	edx,003fffffh
	imul	ebx,[esp+12]	;scale green
	shr	eax,16
	and	ecx,000000ffh
	imul	ecx,[esp+12]	;scale blue
	add	edi,ebx
	and	eax,000000ffh
	imul	eax,[esp+12]	;scale red
	add	esi,eax
	add	ebp,ecx

	;channels are done, let's clip them!
	;
	;esi = R
	;edi = G
	;ebp = B

	mov	ecx,[esp+4+48]	;fetch destination pointer
	cmp	esi,80000000h

	sbb	eax,eax		;eax=-1 if r>=0
	cmp	edi,80000000h

	sbb	ebx,ebx		;ebx=-1 if g>=0
	and	esi,eax		;clip low red

	and	edi,ebx		;clip low green
	cmp	ebp,80000000h

	sbb	eax,eax		;eax=-1 if b>=0
	cmp	edx,esi

	sbb	ebx,ebx		;eax=-1 if r<3fffffh
	and	ebp,eax		;clip low blue

	or	esi,ebx		;clip high red
	cmp	edx,edi

	sbb	eax,eax		;ebx=-1 if g<3fffffh
	cmp	edx,ebp

	sbb	ebx,ebx		;ecx=-1 if b<ffh
	or	edi,eax		;clip high green

	;combine channels into a pixel

	shl	esi,2
	or	ebp,ebx		;clip high blue

	shr	edi,6
	and	esi,00ff0000h

	shr	ebp,14
	and	edi,0000ff00h

	and	ebp,0ffh
	add	esi,edi

	mov	eax,[esp+24+48]	;fetch row counter
	add	esi,ebp

	add	eax,4
	nop

	mov	[esp+24+48],eax
	mov	[ecx+eax-4],esi	;write pixel

	jne	.pixelloop

	add	esp,32

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

;--------------------------------------------------------------------------
;asm_resize_ccint_col_MMX(dst, src1, src2, src3, src4, count, tbl);

	global	_asm_resize_ccint_col_MMX	

_asm_resize_ccint_col_MMX:
	push	ebx
	push	esi
	push	edi
	push	ebp

	mov	ebp,[esp + 4 + 16]	;ebp = dest addr
	mov	esi,[esp + 24 + 16]	;esi = count
	add	esi,esi
	add	esi,esi

	mov	eax,[esp + 8 + 16]	;eax = row 1
	mov	ebx,[esp + 12 + 16]	;ebx = row 2
	mov	ecx,[esp + 16 + 16]	;ecx = row 3
	mov	edx,[esp + 20 + 16]	;edx = row 4
	mov	edi,[esp + 28 + 16]	;edi = coefficient ptr
	
	add	eax,esi
	add	ebx,esi
	add	ecx,esi
	add	edx,esi
	add	ebp,esi
	neg	esi

	movq		mm4,[edi]
	movq		mm5,[edi+8]
	movq		mm6,[x0000200000002000]
	pxor		mm7,mm7

	movd		mm2,dword [eax+esi]
	movd		mm1,dword [ebx+esi]		;mm1 = pixel1
	punpcklbw	mm2,mm7
	jmp		short ccint_col_loop_MMX.entry

	align		16
ccint_col_loop_MMX:
	movd		mm2,dword [eax+esi]		;mm2 = pixel0
	packuswb	mm0,mm0
	
	movd		mm1,dword [ebx+esi]		;mm1 = pixel1
	pxor		mm7,mm7

	movd		dword [ebp+esi-4],mm0
	punpcklbw	mm2,mm7
	
ccint_col_loop_MMX.entry:	
	punpcklbw	mm1,mm7
	movq		mm0,mm2
	
	movd		mm3,dword [edx+esi]		;mm3 = pixel3
	punpcklwd	mm0,mm1			;mm0 = [g1][g0][b1][b0]
	
	pmaddwd		mm0,mm4
	punpckhwd	mm2,mm1			;mm2 = [a1][a0][r1][r0]
	
	movd		mm1,dword [ecx+esi]		;mm1 = pixel2
	punpcklbw	mm3,mm7
		
	pmaddwd		mm2,mm4
	punpcklbw	mm1,mm7
	
	movq		mm7,mm1
	punpcklwd	mm1,mm3			;mm1 = [g3][g2][b3][b2]
	
	punpckhwd	mm7,mm3			;mm7 = [a3][a2][r3][r2]
	pmaddwd		mm1,mm5
	
	pmaddwd		mm7,mm5
	paddd		mm0,mm6
	
	paddd		mm2,mm6
	paddd		mm0,mm1
	
	paddd		mm2,mm7
	psrad		mm0,14
	
	psrad		mm2,14
	add		esi,4
	
	packssdw	mm0,mm2
	jne		ccint_col_loop_MMX
	
	packuswb	mm0,mm0
	movd		dword [ebp-4],mm0

	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	ret
	

;--------------------------------------------------------------------------
;asm_resize_ccint_col_SSE2(dst, src1, src2, src3, src4, count, tbl);

	global	_asm_resize_ccint_col_SSE2	

_asm_resize_ccint_col_SSE2:
	push	ebx
	push	esi
	push	edi
	push	ebp

	mov	ebp,[esp + 4 + 16]	;ebp = dest addr
	mov	esi,[esp + 24 + 16]	;esi = count
	add	esi,esi
	add	esi,esi

	mov	eax,[esp + 8 + 16]	;eax = row 1
	mov	ebx,[esp + 12 + 16]	;ebx = row 2
	mov	ecx,[esp + 16 + 16]	;ecx = row 3
	mov	edx,[esp + 20 + 16]	;edx = row 4
	mov	edi,[esp + 28 + 16]	;edi = coefficient ptr
	
;	lea	eax,[eax+esi-4]
;	lea	ebx,[ebx+esi-4]
;	lea	ecx,[ecx+esi-4]
;	lea	edx,[edx+esi-4]
;	lea	ebp,[ebp+esi-4]
	neg	esi

	add	esi,4
	jz	ccint_col_SSE2_odd

	movq		xmm4,qword [edi]
	movq		xmm5,qword [edi+8]
	punpcklqdq	xmm4,xmm4
	punpcklqdq	xmm5,xmm5
	movq		xmm6,[x0000200000002000]
	punpcklqdq	xmm6,xmm6
	pxor		xmm7,xmm7

;	jmp		short ccint_col_loop_SSE2.entry

;	align		16
ccint_col_loop_SSE2:
	movq		xmm0, qword [eax]
	add			eax, 8
	movq		xmm1, qword [ebx]
	add			ebx, 8
	movq		xmm2, qword [ecx]
	add			ecx, 8
	movq		xmm3, qword [edx]
	add			edx, 8
	punpcklbw	xmm0,xmm1
	punpcklbw	xmm2,xmm3
	movdqa		xmm1,xmm0
	movdqa		xmm3,xmm2
	punpcklbw	xmm0,xmm7
	punpckhbw	xmm1,xmm7
	punpcklbw	xmm2,xmm7
	punpckhbw	xmm3,xmm7
	pmaddwd		xmm0,xmm4
	pmaddwd		xmm1,xmm4
	pmaddwd		xmm2,xmm5
	pmaddwd		xmm3,xmm5
	paddd		xmm0,xmm6
	paddd		xmm1,xmm6
	paddd		xmm0,xmm2
	paddd		xmm1,xmm3
	psrad		xmm0,14
	psrad		xmm1,14
	packssdw	xmm0,xmm1
	packuswb	xmm0,xmm0
	movdq2q		mm0,xmm0	
	movntq		[ebp],mm0
	add		ebp,8
	add		esi,8
	jnc		ccint_col_loop_SSE2
	jnz		ccint_col_SSE2_noodd
ccint_col_SSE2_odd:
	movd		mm0, dword [eax]
	pxor		mm7,mm7
	movd		mm1, dword [ebx]
	movdq2q		mm4,xmm4
	movd		mm2, dword [ecx]
	movdq2q		mm5,xmm5
	movd		mm3, dword [edx]
	movdq2q		mm6,xmm6
	punpcklbw	mm0,mm1
	punpcklbw	mm2,mm3
	movq		mm1,mm0
	movq		mm3,mm2
	punpcklbw	mm0,mm7
	punpckhbw	mm1,mm7
	punpcklbw	mm2,mm7
	punpckhbw	mm3,mm7
	pmaddwd		mm0,mm4
	pmaddwd		mm1,mm4
	pmaddwd		mm2,mm5
	pmaddwd		mm3,mm5
	paddd		mm0,mm6
	paddd		mm2,mm6
	paddd		mm0,mm2
	paddd		mm1,mm3
	psrad		mm0,14
	psrad		mm1,14
	packssdw	mm0,mm1
	packuswb	mm0,mm0
	movd		eax,mm0
	movnti		[ebp],eax

ccint_col_SSE2_noodd:
	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	ret


; long resize_table_row_by2linear_MMX(Pixel *out, Pixel *in, PixDim w);

	global	_resize_table_row_by2linear_MMX	

_resize_table_row_by2linear_MMX:
	push		ebp
	push		esi
	push		edi
	push		ebx

	mov		ebp,[esp + 12 + 16]		;ebp = pixel counter
	mov		eax,[esp + 16 + 16]		;eax = accumulator
	shl		ebp,2
	neg		ebp
	mov		edi,[esp +  4 + 16]		;edi = destination pointer
	mov		esi,[esp + 8 + 16]		;esi = source
	movq		mm6,[x0002000200020002]
	pxor		mm7,mm7

	;load row pointers!

	and		ebp,0fffffff8h
	jz		.oddpixeltest

	sub		esi,ebp
	sub		edi,ebp
	sub		esi,ebp

	test		dword [esp+8+16],4
	jnz		.pixelloop_unaligned

.pixelloop:
	movd		mm0,dword [esi+ebp*2+4]

	movq		mm1,qword [esi+ebp*2+8]
	punpcklbw	mm0,mm7

	movq		mm2,mm1
	punpcklbw	mm1,mm7

	movq		mm3,qword [esi+ebp*2+16]
	punpckhbw	mm2,mm7

	movq		mm4,mm3
	punpcklbw	mm3,mm7

	punpckhbw	mm4,mm7
	paddw		mm1,mm1

	paddw		mm3,mm3
	paddw		mm0,mm2

	paddw		mm2,mm4
	paddw		mm0,mm1

	paddw		mm2,mm3
	paddw		mm0,mm6

	paddw		mm2,mm6
	psraw		mm0,2

	psraw		mm2,2
	packuswb	mm0,mm0

	packuswb	mm2,mm2
	add		ebp,8

	movd		dword [edi+ebp-8],mm0

	movd		dword [edi+ebp-4],mm2
	jne		.pixelloop
	jmp		short .oddpixeltest

.pixelloop_unaligned:
	movq		mm0,[esi+ebp*2+4]

	movq		mm1,mm0
	punpcklbw	mm0,mm7

	movq		mm2,[esi+ebp*2+12]
	punpckhbw	mm1,mm7

	movq		mm3,mm2
	punpcklbw	mm2,mm7

	movd		mm4,dword [esi+ebp*2+20]
	punpckhbw	mm3,mm7

	punpcklbw	mm4,mm7
	paddw		mm1,mm1

	paddw		mm3,mm3
	paddw		mm0,mm2

	paddw		mm2,mm4
	paddw		mm0,mm1

	paddw		mm2,mm3
	paddw		mm0,mm6

	paddw		mm2,mm6
	psraw		mm0,2

	psraw		mm2,2
	packuswb	mm0,mm0

	packuswb	mm2,mm2
	add			ebp,8

	movd		dword [edi+ebp-8],mm0

	movd		dword [edi+ebp-4],mm2
	jne			.pixelloop_unaligned

	;odd pixel?

.oddpixeltest:
	test		dword [esp+12+16],1
	jz			.xit

	movd		mm0,dword [esi+4]

	movd		mm1,dword [esi+8]
	punpcklbw	mm0,mm7
	movd		mm2,dword [esi+12]
	punpcklbw	mm1,mm7

	punpcklbw	mm2,mm7
	paddw		mm1,mm1

	paddw		mm0,mm2
	paddw		mm1,mm6

	paddw		mm1,mm0

	psraw		mm1,2

	packuswb	mm1,mm1

	movd		dword [edi],mm1

.xit:
	pop		ebx
	pop		edi
	pop		esi
	pop		ebp
	ret

; long resize_table_row_by2cubic_MMX(Pixel *out, Pixel *in, PixDim w, unsigned long accum, unsigned long fstep, unsigned long istep);

_resize_table_row_by2cubic_MMX:
	push	ebp
	push	esi
	push	edi
	push	edx
	push	ecx
	push	ebx

	mov	ebp,[esp + 12 + 24]		;ebp = pixel counter
	mov	eax,[esp + 16 + 24]		;eax = accumulator
	shl	ebp,2
	mov	ebx,[esp + 20 + 24]		;ebx = fractional step
	neg	ebp
	mov	edi,[esp + 4 + 24]		;edi = destination pointer
	mov	ecx,[esp + 24 + 24]		;ecx = integer step

	;load row pointers!

	mov	esi,[esp + 8 + 24]		;esi = row pointer table

	sub	esi,ebp
	sub	esi,ebp

	sub	edi,ebp
	movq	mm6,[x0004000400040004]
	pxor	mm7,mm7

.pixelloop:
	movd		mm0,dword [esi+ebp*2+4]

	movd		mm1,dword [esi+ebp*2+12]
	punpcklbw	mm0,mm7
	movd		mm2,dword [esi+ebp*2+16]
	punpcklbw	mm1,mm7
	movd		mm3,dword [esi+ebp*2+20]
	punpcklbw	mm2,mm7
	movd		mm4,dword [esi+ebp*2+28]
	punpcklbw	mm3,mm7

	punpcklbw	mm4,mm7
	psllw		mm2,3

	paddw		mm1,mm3
	paddw		mm0,mm4

	movq		mm3,mm1
	paddw		mm2,mm6

	psllw		mm1,2
	psubw		mm2,mm0

	paddw		mm1,mm3
	add			ebp,4

	paddw		mm2,mm1

	psraw		mm2,4

	packuswb	mm2,mm2

	movd		dword [edi+ebp-4],mm2
	jne		.pixelloop

	pop	ebx
	pop	ecx
	pop	edx
	pop	edi
	pop	esi
	pop	ebp

	ret


;-------------------------------------------------------------------------
;
;	long resize_table_row_protected_MMX(
;		[esp+ 4]  Pixel *out,
;		[esp+ 8]  Pixel *in,
;		[esp+12]  int *filter,
;		[esp+16]  int filter_width,
;		[esp+20]  PixDim w,
;		[esp+24]  long accum,
;		[esp+28]  long frac,
;		[esp+32]  long limit);

_resize_table_row_protected_MMX:
	push	ebp
	push	esi
	push	edi
	push	ebx

	mov	ecx,[esp + 20 + 16]
	mov	eax,[esp + 24 + 16]
	shl	ecx,2
	mov	edi,[esp + 8 + 16]
	xor	ecx,-1
	mov	ebp,[esp + 32 + 16]
	inc	ecx
	mov	esi,eax
	mov	[esp + 20 + 16],ecx
	mov	edx,eax
	lea	edi,[edi+ebp*4]
	mov	ebx,[esp + 4 + 16]
	sub	ebx,ecx
	sub	ebx,4
	mov	[esp + 4 + 16],ebx

.pixelloop:
	sar	esi,16
	and	edx,0000ff00h
	mov	ecx,[esp + 16 + 16]		;filter width

	shr	ecx,1
	shr	edx,5

	imul	edx,ecx

	add	eax,[esp + 28 + 16]
	add	edx,[esp + 12 + 16]

	movq	mm6,[MMX_roundval]
	pxor	mm0,mm0
	movq	mm7,mm6
	pxor	mm1,mm1
	mov	[esp + 24 + 16],eax


.coeff_loop:
	mov		eax,esi
	cmp		esi,80000000h
	sbb		ebx,ebx
	inc		esi
	and		eax,ebx
	paddd		mm7,mm0			;alpha/red from last iteration
	sub		eax,ebp
	paddd		mm6,mm1			;green/blue from last iteration
	sbb		ebx,ebx
	and		eax,ebx
	movd		mm0, dword [edi+eax*4]
	mov		eax,esi
	cmp		esi,80000000h
	sbb		ebx,ebx
	inc		esi
	and		eax,ebx
	sub		eax,ebp
	sbb		ebx,ebx
	and		eax,ebx
	movd		mm1, dword [edi+eax*4]
	pxor		mm5,mm5
	punpcklbw	mm0,mm1		;[a0][a1][r0][r1][g0][g1][b0][b1]
	movq		mm1,mm0
	punpckhbw	mm0,mm5		;[ a0 ][ a1 ][ r0 ][ r1 ]
	punpcklbw	mm1,mm5		;[ g0 ][ g1 ][ b0 ][ b1 ]
	pmaddwd		mm0,[edx]
	pmaddwd		mm1,[edx]
	add		edx,8
	sub		ecx,1
	jne		.coeff_loop

	paddd		mm7,mm0			;accumulate alpha/red (pixels 2/3)

	paddd		mm6,mm1			;accumulate green/blue (pixels 2/3)
	mov		ecx,[esp+20+16]

	psrad		mm7,14
	mov		eax,[esp+24+16]

	psrad		mm6,14
	mov		ebx,[esp+4+16]

	packssdw	mm6,mm7
	add		ecx,4

	packuswb	mm6,mm6
	mov		[esp+20+16],ecx

	mov		esi,eax
	mov		edx,eax

	movd		dword [ebx+ecx],mm6
	jne		.pixelloop

	pop	ebx
	pop	edi
	pop	esi
	pop	ebp

	ret

;-------------------------------------------------------------------------
;
;	long resize_table_row_MMX(Pixel *out, Pixel *in, int *filter, int filter_width, PixDim w, long accum, long frac);

	segment	.text

_resize_table_row_MMX:
	push	ebp
	push	esi
	push	edi
	push	ebx

	cmp		dword [esp+16+16], 4
	jz		.accel_4coeff
	cmp		dword [esp+16+16], 6
	jz		.accel_6coeff
	cmp		dword [esp+16+16], 8
	jz		.accel_8coeff

	mov	eax,[esp + 24 + 16]
	mov	ebp,[esp + 20 + 16]
	mov	ebx,[esp + 8 + 16]
	mov	edi,[esp + 4 + 16]

	mov	esi,eax
	mov	edx,eax

	pxor		mm5,mm5

	mov		ecx,[esp + 16 + 16]
	shr		ecx,1
	mov		[esp+16+16],ecx
	test	ecx,1
	jnz		.pixelloop_odd_pairs

.pixelloop_even_pairs:
	shr		esi,14
	and		edx,0000ff00h
	and		esi,0fffffffch

	mov		ecx,[esp + 16 + 16]
	shr		edx,5
	add		esi,ebx
	imul	edx,ecx
	add		eax,[esp + 28 + 16]
	add		edx,[esp + 12 + 16]

	movq	mm6,[MMX_roundval]
	pxor	mm3,mm3
	movq	mm7,mm6
	pxor	mm2,mm2

.coeffloop_unaligned_even_pairs:
	movd		mm0,dword [esi+0]
	paddd		mm7,mm2			;accumulate alpha/red (pixels 2/3)

	punpcklbw	mm0,[esi+4]		;mm1=[a0][a1][r0][r1][g0][g1][b0][b1]
	paddd		mm6,mm3			;accumulate green/blue (pixels 2/3)

	movd		mm2,dword [esi+8]
	movq		mm1,mm0			;mm0=[a0][a1][r0][r1][g0][g1][b0][b1]

	punpcklbw	mm2,[esi+12]	;mm2=[a2][a3][r2][r3][g2][g3][b2][b3]

	punpckhbw	mm0,mm5			;mm0=[ a0 ][ a1 ][ r0 ][ r1 ]
	movq		mm3,mm2			;mm3=[a2][a3][r2][r3][g2][g3][b2][b3]

	pmaddwd		mm0,[edx]		;mm0=[a0*f0+a1*f1][r0*f0+r1*f1]
	punpcklbw	mm1,mm5			;mm1=[ g0 ][ g1 ][ b0 ][ b1 ]

	pmaddwd		mm1,[edx]		;mm1=[g0*f0+g1*f1][b0*f0+b1*f1]
	punpckhbw	mm2,mm5			;mm2=[ a2 ][ a3 ][ r0 ][ r1 ]

	pmaddwd		mm2,[edx+8]		;mm2=[a2*f2+a3*f3][r2*f2+r3*f3]
	punpcklbw	mm3,mm5			;mm3=[ g2 ][ g3 ][ b2 ][ b3 ]

	pmaddwd		mm3,[edx+8]		;mm3=[g2*f2+g3*f3][b2*f2+b3*f3]
	paddd		mm7,mm0			;accumulate alpha/red (pixels 0/1)

	paddd		mm6,mm1			;accumulate green/blue (pixels 0/1)
	add		edx,16

	add		esi,16
	sub		ecx,2

	jne		.coeffloop_unaligned_even_pairs

	paddd		mm7,mm2			;accumulate alpha/red (pixels 2/3)
	paddd		mm6,mm3			;accumulate green/blue (pixels 2/3)

	psrad		mm7,14
	psrad		mm6,14

	packssdw	mm6,mm7
	add		edi,4

	packuswb	mm6,mm6
	sub		ebp,1

	mov	esi,eax
	mov	edx,eax

	movd	dword [edi-4],mm6
	jne	.pixelloop_even_pairs

	pop	ebx
	pop	edi
	pop	esi
	pop	ebp

	ret

;----------------------------------------------------------------

.pixelloop_odd_pairs:
	shr		esi,14
	and		edx,0000ff00h
	and		esi,0fffffffch

	mov		ecx,[esp + 16 + 16]
	shr		edx,5
	add		esi,ebx
	imul	edx,ecx
	add		eax,[esp + 28 + 16]
	sub		ecx,1
	add		edx,[esp + 12 + 16]

	movq	mm6,[MMX_roundval]
	pxor	mm3,mm3
	pxor	mm2,mm2
	movq	mm7,mm6

.coeffloop_unaligned_odd_pairs:
	movd		mm0,dword [esi+0]
	paddd		mm7,mm2			;accumulate alpha/red (pixels 2/3)

	punpcklbw	mm0,[esi+4]		;mm1=[a0][a1][r0][r1][g0][g1][b0][b1]
	paddd		mm6,mm3			;accumulate green/blue (pixels 2/3)

	movd		mm2,dword [esi+8]
	movq		mm1,mm0			;mm0=[a0][a1][r0][r1][g0][g1][b0][b1]

	punpcklbw	mm2,[esi+12]	;mm2=[a2][a3][r2][r3][g2][g3][b2][b3]

	punpckhbw	mm0,mm5			;mm0=[ a0 ][ a1 ][ r0 ][ r1 ]
	movq		mm3,mm2			;mm3=[a2][a3][r2][r3][g2][g3][b2][b3]

	pmaddwd		mm0,[edx]		;mm0=[a0*f0+a1*f1][r0*f0+r1*f1]
	punpcklbw	mm1,mm5			;mm1=[ g0 ][ g1 ][ b0 ][ b1 ]

	pmaddwd		mm1,[edx]		;mm1=[g0*f0+g1*f1][b0*f0+b1*f1]
	punpckhbw	mm2,mm5			;mm2=[ a2 ][ a3 ][ r0 ][ r1 ]

	pmaddwd		mm2,[edx+8]		;mm2=[a2*f2+a3*f3][r2*f2+r3*f3]
	punpcklbw	mm3,mm5			;mm3=[ g2 ][ g3 ][ b2 ][ b3 ]

	pmaddwd		mm3,[edx+8]		;mm3=[g2*f2+g3*f3][b2*f2+b3*f3]
	paddd		mm7,mm0			;accumulate alpha/red (pixels 0/1)

	paddd		mm6,mm1			;accumulate green/blue (pixels 0/1)
	add		edx,16

	add		esi,16
	sub		ecx,2

	jne		.coeffloop_unaligned_odd_pairs

	paddd		mm7,mm2			;accumulate alpha/red (pixels 2/3)
	paddd		mm6,mm3			;accumulate green/blue (pixels 2/3)

	;finish up odd pair

	movd		mm0,dword [esi]		;mm0 = [x1][r1][g1][b1]
	punpcklbw	mm0,[esi+4]		;mm2 = [x0][x1][r0][r1][g0][g1][b0][b1]
	movq		mm1,mm0
	punpcklbw	mm0,mm5			;mm0 = [g0][g1][b0][b1]
	punpckhbw	mm1,mm5			;mm1 = [x0][x1][r0][r1]

	pmaddwd		mm0,[edx]
	pmaddwd		mm1,[edx]

	paddd		mm6,mm0
	paddd		mm7,mm1

	;combine into pixel

	psrad		mm6,14

	psrad		mm7,14

	packssdw	mm6,mm7
	add		edi,4

	packuswb	mm6,mm6
	sub		ebp,1

	mov		esi,eax
	mov		edx,eax

	movd		dword [edi-4],mm6
	jne		.pixelloop_odd_pairs

	pop	ebx
	pop	edi
	pop	esi
	pop	ebp

	ret

;----------------------------------------------------------------

.accel_4coeff:
	mov	eax,[esp + 24 + 16]
	mov	ebp,[esp + 20 + 16]
	add	ebp,ebp
	add	ebp,ebp
	mov	ebx,[esp + 8 + 16]
	mov	edi,[esp + 4 + 16]
	add	edi,ebp
	neg	ebp

	mov	esi,eax
	mov	edx,eax

	movq		mm4,[MMX_roundval]
	pxor		mm5,mm5

	mov		ecx,[esp+12+16]

.pixelloop_4coeff:
	shr		esi,14
	and		edx,0000ff00h
	and		esi,0fffffffch

	shr		edx,4
	add		esi,ebx
	add		eax,[esp+28+16]
	add		edx,ecx

	movd		mm0,dword [esi+0]
	movd		mm2,dword [esi+8]
	punpcklbw	mm0,[esi+4]		;mm0=[a0][a1][r0][r1][g0][g1][b0][b1]

	movq		mm1,mm0			;mm1=[a0][a1][r0][r1][g0][g1][b0][b1]

	punpckhbw	mm0,mm5			;mm0=[ a0 ][ a1 ][ r0 ][ r1 ]

	pmaddwd		mm0,[edx]		;mm0=[a0*f0+a1*f1][r0*f0+r1*f1]
	punpcklbw	mm2,[esi+12]	;mm2=[a2][a3][r2][r3][g2][g3][b2][b3]

	movq		mm3,mm2			;mm3=[a2][a3][r2][r3][g2][g3][b2][b3]
	punpcklbw	mm1,mm5			;mm1=[ g0 ][ g1 ][ b0 ][ b1 ]

	pmaddwd		mm1,[edx]		;mm1=[g0*f0+g1*f1][b0*f0+b1*f1]
	punpckhbw	mm2,mm5			;mm2=[ a2 ][ a3 ][ r0 ][ r1 ]

	pmaddwd		mm2,[edx+8]		;mm2=[a2*f2+a3*f3][r2*f2+r3*f3]
	punpcklbw	mm3,mm5			;mm3=[ g2 ][ g3 ][ b2 ][ b3 ]

	pmaddwd		mm3,[edx+8]		;mm3=[g2*f2+g3*f3][b2*f2+b3*f3]
	paddd		mm0,mm4			;accumulate alpha/red (pixels 0/1)

	paddd		mm1,mm4			;accumulate green/blue (pixels 0/1)

	paddd		mm0,mm2			;accumulate alpha/red (pixels 2/3)
	paddd		mm1,mm3			;accumulate green/blue (pixels 2/3)

	psrad		mm0,14
	psrad		mm1,14

	packssdw	mm1,mm0
	mov	esi,eax

	packuswb	mm1,mm1
	mov	edx,eax

	movd	dword [edi+ebp],mm1
	add		ebp,4
	jne		.pixelloop_4coeff

	pop	ebx
	pop	edi
	pop	esi
	pop	ebp

	ret


;----------------------------------------------------------------

.accel_6coeff:
	mov	eax,[esp + 24 + 16]
	mov	ebp,[esp + 20 + 16]
	add	ebp,ebp
	add	ebp,ebp
	mov	ebx,[esp + 8 + 16]
	mov	edi,[esp + 4 + 16]
	add	edi,ebp
	neg	ebp

	mov	esi,eax
	mov	edx,eax

	movq		mm4,[MMX_roundval]
	pxor		mm5,mm5

	mov		ecx,[esp+12+16]

.pixelloop_6coeff:
	shr		esi,14
	and		edx,0000ff00h
	and		esi,0fffffffch

	shr		edx,5
	lea		edx,[edx+edx*2]
	add		esi,ebx
	add		eax,[esp+28+16]
	add		edx,ecx

	movd		mm0,dword [esi+0]
	movd		mm2,dword [esi+8]
	punpcklbw	mm0,[esi+4]		;mm0=[a0][a1][r0][r1][g0][g1][b0][b1]

	movq		mm1,mm0			;mm1=[a0][a1][r0][r1][g0][g1][b0][b1]

	punpckhbw	mm0,mm5			;mm0=[ a0 ][ a1 ][ r0 ][ r1 ]

	pmaddwd		mm0,[edx]		;mm0=[a0*f0+a1*f1][r0*f0+r1*f1]
	punpcklbw	mm2,[esi+12]	;mm2=[a2][a3][r2][r3][g2][g3][b2][b3]

	movq		mm3,mm2			;mm3=[a2][a3][r2][r3][g2][g3][b2][b3]
	punpcklbw	mm1,mm5			;mm1=[ g0 ][ g1 ][ b0 ][ b1 ]

	pmaddwd		mm1,[edx]		;mm1=[g0*f0+g1*f1][b0*f0+b1*f1]
	punpckhbw	mm2,mm5			;mm2=[ a2 ][ a3 ][ r0 ][ r1 ]

	pmaddwd		mm2,[edx+8]		;mm2=[a2*f2+a3*f3][r2*f2+r3*f3]
	punpcklbw	mm3,mm5			;mm3=[ g2 ][ g3 ][ b2 ][ b3 ]

	pmaddwd		mm3,[edx+8]		;mm3=[g2*f2+g3*f3][b2*f2+b3*f3]
	paddd		mm0,mm4			;accumulate alpha/red (pixels 0/1)

	paddd		mm1,mm4			;accumulate green/blue (pixels 0/1)

	paddd		mm0,mm2			;accumulate alpha/red (pixels 2/3)
	paddd		mm1,mm3			;accumulate green/blue (pixels 2/3)

	movd		mm6,dword [esi+16]

	punpcklbw	mm6,[esi+20]	;mm1=[a0][a1][r0][r1][g0][g1][b0][b1]

	movq		mm7,mm6			;mm0=[a0][a1][r0][r1][g0][g1][b0][b1]

	punpckhbw	mm6,mm5			;mm0=[ a0 ][ a1 ][ r0 ][ r1 ]

	pmaddwd		mm6,[edx+16]	;mm0=[a0*f0+a1*f1][r0*f0+r1*f1]
	punpcklbw	mm7,mm5			;mm1=[ g0 ][ g1 ][ b0 ][ b1 ]

	pmaddwd		mm7,[edx+16]	;mm1=[g0*f0+g1*f1][b0*f0+b1*f1]
	paddd		mm0,mm6			;accumulate alpha/red (pixels 0/1)

	paddd		mm1,mm7			;accumulate green/blue (pixels 0/1)


	psrad		mm0,14
	psrad		mm1,14

	packssdw	mm1,mm0
	mov	esi,eax

	packuswb	mm1,mm1
	mov	edx,eax

	movd	dword [edi+ebp],mm1
	add		ebp,4
	jne		.pixelloop_6coeff

	pop	ebx
	pop	edi
	pop	esi
	pop	ebp

	ret

;----------------------------------------------------------------

.accel_8coeff:
	mov	eax,[esp + 24 + 16]
	mov	ebp,[esp + 20 + 16]
	add	ebp,ebp
	add	ebp,ebp
	mov	ebx,[esp + 8 + 16]
	mov	edi,[esp + 4 + 16]
	add	edi,ebp
	neg	ebp

	mov	esi,eax
	mov	edx,eax

	movq		mm4,[MMX_roundval]
	pxor		mm5,mm5

	mov		ecx,[esp+12+16]

.pixelloop_8coeff:
	shr		esi,14
	and		edx,0000ff00h
	and		esi,0fffffffch

	shr		edx,3
	add		esi,ebx
	add		eax,[esp+28+16]
	add		edx,ecx

	movd		mm0,dword [esi+0]
	movd		mm2,dword [esi+8]
	punpcklbw	mm0,[esi+4]		;mm0=[a0][a1][r0][r1][g0][g1][b0][b1]

	movq		mm1,mm0			;mm1=[a0][a1][r0][r1][g0][g1][b0][b1]

	punpckhbw	mm0,mm5			;mm0=[ a0 ][ a1 ][ r0 ][ r1 ]

	pmaddwd		mm0,[edx]		;mm0=[a0*f0+a1*f1][r0*f0+r1*f1]
	punpcklbw	mm2,[esi+12]	;mm2=[a2][a3][r2][r3][g2][g3][b2][b3]

	movq		mm3,mm2			;mm3=[a2][a3][r2][r3][g2][g3][b2][b3]
	punpcklbw	mm1,mm5			;mm1=[ g0 ][ g1 ][ b0 ][ b1 ]

	pmaddwd		mm1,[edx]		;mm1=[g0*f0+g1*f1][b0*f0+b1*f1]
	punpckhbw	mm2,mm5			;mm2=[ a2 ][ a3 ][ r0 ][ r1 ]

	pmaddwd		mm2,[edx+8]		;mm2=[a2*f2+a3*f3][r2*f2+r3*f3]
	punpcklbw	mm3,mm5			;mm3=[ g2 ][ g3 ][ b2 ][ b3 ]

	pmaddwd		mm3,[edx+8]		;mm3=[g2*f2+g3*f3][b2*f2+b3*f3]
	paddd		mm0,mm4			;accumulate alpha/red (pixels 0/1)

	paddd		mm1,mm4			;accumulate green/blue (pixels 0/1)

	paddd		mm0,mm2			;accumulate alpha/red (pixels 2/3)
	paddd		mm1,mm3			;accumulate green/blue (pixels 2/3)


	movd		mm6,dword [esi+16]

	punpcklbw	mm6,[esi+20]	;mm1=[a0][a1][r0][r1][g0][g1][b0][b1]

	movd		mm2,dword [esi+24]

	punpcklbw	mm2,[esi+28]	;mm2=[a2][a3][r2][r3][g2][g3][b2][b3]
	movq		mm7,mm6			;mm0=[a0][a1][r0][r1][g0][g1][b0][b1]

	punpckhbw	mm6,mm5			;mm0=[ a0 ][ a1 ][ r0 ][ r1 ]
	movq		mm3,mm2			;mm3=[a2][a3][r2][r3][g2][g3][b2][b3]

	pmaddwd		mm6,[edx+16]	;mm0=[a0*f0+a1*f1][r0*f0+r1*f1]
	punpcklbw	mm7,mm5			;mm1=[ g0 ][ g1 ][ b0 ][ b1 ]

	pmaddwd		mm7,[edx+16]	;mm1=[g0*f0+g1*f1][b0*f0+b1*f1]
	punpckhbw	mm2,mm5			;mm2=[ a2 ][ a3 ][ r0 ][ r1 ]

	pmaddwd		mm2,[edx+24]	;mm2=[a2*f2+a3*f3][r2*f2+r3*f3]
	punpcklbw	mm3,mm5			;mm3=[ g2 ][ g3 ][ b2 ][ b3 ]

	pmaddwd		mm3,[edx+24]	;mm3=[g2*f2+g3*f3][b2*f2+b3*f3]
	paddd		mm0,mm6			;accumulate alpha/red (pixels 0/1)

	paddd		mm1,mm7			;accumulate green/blue (pixels 0/1)
	paddd		mm0,mm2			;accumulate alpha/red (pixels 0/1)

	paddd		mm1,mm3			;accumulate green/blue (pixels 0/1)


	psrad		mm0,14
	psrad		mm1,14

	packssdw	mm1,mm0
	mov	esi,eax

	packuswb	mm1,mm1
	mov	edx,eax

	movd	dword [edi+ebp],mm1
	add		ebp,4
	jne		.pixelloop_8coeff

	pop	ebx
	pop	edi
	pop	esi
	pop	ebp

	ret







;-------------------------------------------------------------------------
;
;	long resize_table_col_MMX(Pixel *out, Pixel **in_table, int *filter, int filter_width, PixDim w, long frac);

_resize_table_col_MMX:
	push		ebp
	push		esi
	push		edi
	push		ebx

	mov			edx,[esp + 12 + 16]
	mov			eax,[esp + 24 + 16]
	shl			eax,2
	imul		eax,[esp + 16 + 16]
	add			edx,eax
	mov			[esp + 12 + 16], edx	;[esp+12+28] = filter pointer

	mov			ebp,[esp + 20 + 16]		;ebp = pixel counter
	mov			edi,[esp + 4 + 16]		;edi = destination pointer

	pxor		mm5,mm5

	cmp			dword [esp+16+16], 4
	jz			.accel_4coeff
	cmp			dword [esp+16+16], 6
	jz			.accel_6coeff

	mov			ecx,[esp + 16 + 16]
	shr			ecx,1
	mov			[esp + 16 + 16],ecx		;ecx = filter pair count

	xor			ebx,ebx					;ebx = source offset 

	mov			ecx,[esp + 16 + 16]		;ecx = filter width counter
.pixelloop:
	mov			eax,[esp + 8 + 16]		;esi = row pointer table
	movq		mm6,[MMX_roundval]
	movq		mm7,mm6
	pxor		mm0,mm0
	pxor		mm1,mm1
.coeffloop:
	mov			esi,[eax]
	paddd		mm6,mm0

	movd		mm0,dword [esi+ebx]	;mm0 = [0][0][0][0][x0][r0][g0][b0]
	paddd		mm7,mm1

	mov			esi,[eax+4]
	add			eax,8

	movd		mm1,dword [esi+ebx]	;mm1 = [0][0][0][0][x1][r1][g1][b1]
	punpcklbw	mm0,mm1			;mm0 = [x0][x1][r0][r1][g0][g1][b0][b1]

	movq		mm1,mm0
	punpcklbw	mm0,mm5			;mm0 = [g1][g0][b1][b0]

	pmaddwd		mm0,[edx]
	punpckhbw	mm1,mm5			;mm1 = [x1][x0][r1][r0]

	pmaddwd		mm1,[edx]
	add			edx,8

	sub			ecx,1
	jne			.coeffloop

	paddd		mm6,mm0
	paddd		mm7,mm1

	psrad		mm6,14
	psrad		mm7,14
	add			edi,4
	packssdw	mm6,mm7
	add			ebx,4
	packuswb	mm6,mm6
	sub			ebp,1

	mov			ecx,[esp + 16 + 16]		;ecx = filter width counter
	mov			edx,[esp + 12 + 16]		;edx = filter bank pointer

	movd		dword [edi-4],mm6
	jne			.pixelloop

.xit:
	pop		ebx
	pop		edi
	pop		esi
	pop		ebp
	ret



.accel_4coeff:
	movq		mm2,[edx]
	movq		mm3,[edx+8]

	mov			esi,[esp+8+16]			;esi = row pointer table
	mov			eax,[esi]
	add			ebp,ebp
	mov			ebx,[esi+4]
	add			ebp,ebp
	mov			ecx,[esi+8]
	mov			esi,[esi+12]
	add			eax,ebp
	add			ebx,ebp
	add			ecx,ebp
	add			esi,ebp
	add			edi,ebp
	neg			ebp

	;EAX	source 0
	;EBX	source 1
	;ECX	source 2
	;ESI	source 3
	;EDI	destination
	;EBP	counter

	movq		mm4,[MMX_roundval]

.pixelloop4:
	movd		mm6,dword [eax+ebp]	;mm0 = [0][0][0][0][x0][r0][g0][b0]

	punpcklbw	mm6,[ebx+ebp]	;mm0 = [x0][x1][r0][r1][g0][g1][b0][b1]

	movq		mm7,mm6
	punpcklbw	mm6,mm5			;mm0 = [g1][g0][b1][b0]

	pmaddwd		mm6,mm2
	punpckhbw	mm7,mm5			;mm1 = [x1][x0][r1][r0]

	movd		mm0,dword [ecx+ebp]	;mm0 = [0][0][0][0][x0][r0][g0][b0]
	pmaddwd		mm7,mm2

	punpcklbw	mm0,[esi+ebp]	;mm0 = [x0][x1][r0][r1][g0][g1][b0][b1]
	paddd		mm6,mm4

	movq		mm1,mm0
	punpcklbw	mm0,mm5			;mm0 = [g1][g0][b1][b0]

	pmaddwd		mm0,mm3
	punpckhbw	mm1,mm5			;mm1 = [x1][x0][r1][r0]

	pmaddwd		mm1,mm3
	paddd		mm7,mm4

	paddd		mm6,mm0
	paddd		mm7,mm1

	psrad		mm6,14
	psrad		mm7,14
	packssdw	mm6,mm7
	packuswb	mm6,mm6

	movd		dword [edi+ebp],mm6

	add			ebp,4
	jne			.pixelloop4
	jmp			.xit

.accel_6coeff:
	movq		mm2,[edx]
	movq		mm3,[edx+8]
	movq		mm4,[edx+16]

	push		0
	push		dword [fs:0]
	mov			dword [fs:0],esp

	mov			esp,[esp+8+24]			;esp = row pointer table
	mov			eax,[esp]
	add			ebp,ebp
	mov			ebx,[esp+4]
	add			ebp,ebp
	mov			ecx,[esp+8]
	mov			edx,[esp+12]
	mov			esi,[esp+16]
	mov			esp,[esp+20]
	add			eax,ebp
	add			ebx,ebp
	add			ecx,ebp
	add			edx,ebp
	add			esi,ebp
	add			edi,ebp
	add			esp,ebp
	neg			ebp

	;EAX	source 0
	;EBX	source 1
	;ECX	source 2
	;EDX	source 3
	;ESI	source 4
	;EDI	destination
	;ESP	source 5
	;EBP	counter

.pixelloop6:
	movd		mm6,dword [eax+ebp]	;mm0 = [0][0][0][0][x0][r0][g0][b0]

	punpcklbw	mm6,[ebx+ebp]	;mm0 = [x0][x1][r0][r1][g0][g1][b0][b1]

	movq		mm7,mm6
	punpcklbw	mm6,mm5			;mm0 = [g1][g0][b1][b0]

	movd		mm0,dword [ecx+ebp]	;mm0 = [0][0][0][0][x0][r0][g0][b0]
	punpckhbw	mm7,mm5			;mm1 = [x1][x0][r1][r0]

	punpcklbw	mm0,[edx+ebp]	;mm0 = [x0][x1][r0][r1][g0][g1][b0][b1]
	pmaddwd		mm6,mm2

	movq		mm1,mm0
	punpcklbw	mm0,mm5			;mm0 = [g1][g0][b1][b0]

	pmaddwd		mm7,mm2
	punpckhbw	mm1,mm5			;mm1 = [x1][x0][r1][r0]

	paddd		mm6,[MMX_roundval]
	pmaddwd		mm0,mm3

	paddd		mm7,[MMX_roundval]
	pmaddwd		mm1,mm3

	paddd		mm6,mm0

	movd		mm0,dword [esi+ebp]	;mm0 = [0][0][0][0][x0][r0][g0][b0]
	paddd		mm7,mm1

	punpcklbw	mm0,[esp+ebp]	;mm0 = [x0][x1][r0][r1][g0][g1][b0][b1]
	movq		mm1,mm0
	punpcklbw	mm0,mm5			;mm0 = [g1][g0][b1][b0]
	punpckhbw	mm1,mm5			;mm1 = [x1][x0][r1][r0]
	pmaddwd		mm0,mm4
	pmaddwd		mm1,mm4
	paddd		mm6,mm0
	paddd		mm7,mm1

	psrad		mm6,14
	psrad		mm7,14
	packssdw	mm6,mm7
	packuswb	mm6,mm6

	movd		dword [edi+ebp],mm6

	add			ebp,4
	jne			.pixelloop6

	mov			esp,dword [fs:0]
	pop			dword [fs:0]
	pop			eax

	jmp			.xit


_resize_table_col_SSE2:
	push		ebp
	push		esi
	push		edi
	push		ebx

	mov			edx,[esp+12+16]
	mov			eax,[esp+24+16]
	shl			eax,2
	imul		eax,[esp+16+16]
	add			edx,eax
	mov			[esp+12+16], edx		;[esp+12+16] = filter pointer

	mov			ebp,[esp+20+16]		;ebp = pixel counter
	mov			edi,[esp+4+16]		;edi = destination pointer

	pxor		xmm7, xmm7
	movdqa		xmm6, oword [MMX_roundval]

	cmp			dword [esp+16+16], 4
	jz			.accel_4coeff
	cmp			dword [esp+16+16], 6
	jz			.accel_6coeff

	mov			ecx,[esp+16+16]
	shr			ecx,1
	mov			[esp+16+16],ecx		;ecx = filter pair count

	xor			ebx,ebx					;ebx = source offset 

	mov			ecx,[esp+16+16]		;ecx = filter width counter
.pixelloop:
	mov			eax, [esp+8+16]		;esi = row pointer table
	movdqa		xmm4, xmm6
.coeffloop:
	mov			esi,[eax]

	movd		xmm0, dword [esi+ebx]

	mov			esi,[eax+4]
	add			eax,8

	movd		xmm1, dword [esi+ebx]
	punpcklbw	xmm0, xmm1

	punpcklbw	xmm0, xmm7

	movq		xmm2, qword [edx]
	pshufd		xmm2, xmm2, 01000100b

	pmaddwd		xmm0, xmm2

	paddd		xmm4, xmm0

	add			edx,8

	sub			ecx,1
	jne			.coeffloop

	psrad		xmm4,14
	add			edi,4
	packssdw	xmm4,xmm4
	add			ebx,4
	packuswb	xmm4,xmm4
	sub			ebp,1

	mov			ecx,[esp+16+16]		;ecx = filter width counter
	mov			edx,[esp+12+16]		;edx = filter bank pointer

	movd		dword [edi-4],xmm4
	jne			.pixelloop

.xit:
	pop		ebx
	pop		edi
	pop		esi
	pop		ebp
	ret

.accel_4coeff:
	shl			ebp, 2
	mov			eax, [esp+8+16]			;eax = row pointer table
	mov			esi, [eax+12]
	mov			ecx, [eax+8]
	mov			ebx, [eax+4]
	mov			eax, [eax]
	lea			edi, [edi+ebp-4]
	neg			ebp

	;registers:
	;
	;EAX	source 0
	;EBX	source 1
	;ECX	source 2
	;ESI	source 3
	;EDI	destination
	;EBP	counter
	;
	movq		xmm4, qword [edx]				;xmm4 = coeff 0/1
	movq		xmm5, qword [edx+8]			;xmm5 = coeff 2/3
	punpcklqdq	xmm4, xmm4
	punpcklqdq	xmm5, xmm5

	add			ebp, 4
	jz			.oddpixel_4coeff

.pixelloop_4coeff_dualpel:
	movq		xmm0, qword [eax]
	movq		xmm1, qword [ebx]
	movq		xmm2, qword [ecx]
	movq		xmm3, qword [edx]
	add			eax,8
	add			ebx,8
	add			ecx,8
	add			edx,8
	punpcklbw	xmm0, xmm1
	punpcklbw	xmm2, xmm3
	movdqa		xmm1, xmm0
	movdqa		xmm3, xmm2
	punpcklbw	xmm0, xmm7
	punpckhbw	xmm1, xmm7
	punpcklbw	xmm2, xmm7
	punpckhbw	xmm3, xmm7
	pmaddwd		xmm0, xmm4
	pmaddwd		xmm1, xmm4
	pmaddwd		xmm2, xmm5
	pmaddwd		xmm3, xmm5
	paddd		xmm0, xmm2
	paddd		xmm1, xmm3
	paddd		xmm0, xmm6
	paddd		xmm1, xmm6
	psrad		xmm0, 14
	psrad		xmm1, 14
	packssdw	xmm0, xmm1
	packuswb	xmm0, xmm0
	movq		qword [edi+ebp],xmm0
	add			ebp, 8
	jae			.pixelloop_4coeff_dualpel
	jnz			.xit

.oddpixel_4coeff:
	movd		xmm0, dword [eax]
	movd		xmm1, dword [ebx]
	movd		xmm2, dword [ecx]
	movd		xmm3, dword [esi]
	punpcklbw	xmm0, xmm1
	punpcklbw	xmm2, xmm3
	punpcklbw	xmm0, xmm7
	punpcklbw	xmm2, xmm7
	pmaddwd		xmm0, xmm4
	pmaddwd		xmm2, xmm5
	paddd		xmm0, xmm2
	paddd		xmm0, xmm6
	psrad		xmm0, 14
	packssdw	xmm0, xmm0
	packuswb	xmm0, xmm0
	movd		dword [edi],xmm0
	jmp			.xit


.accel_6coeff:
	movq		xmm4, qword [edx]				;xmm4 = coeff 0/1
	movq		xmm5, qword [edx+8]			;xmm5 = coeff 2/3
	movq		xmm6, qword [edx+16]			;xmm5 = coeff 4/5
	punpcklqdq	xmm4, xmm4
	punpcklqdq	xmm5, xmm5
	punpcklqdq	xmm6, xmm6

	push		0
	push		dword [fs:0]
	mov			dword [fs:0],esp

	shl			ebp, 2
	mov			eax, [esp+8+24]			;eax = row pointer table
	mov			esp, [eax+20]
	mov			esi, [eax+16]
	mov			edx, [eax+12]
	mov			ecx, [eax+8]
	mov			ebx, [eax+4]
	mov			eax, [eax]
	lea			edi, [edi+ebp-4]
	neg			ebp

	;registers:
	;
	;EAX	source 0
	;EBX	source 1
	;ECX	source 2
	;EDX	source 3
	;ESI	source 4
	;EDI	destination
	;ESP	source 5
	;EBP	counter
	;

	add			ebp, 4
	jz			.oddpixel_6coeff

.pixelloop_6coeff_dualpel:
	movq		xmm0, qword [eax]
	movq		xmm1, qword [ebx]
	movq		xmm2, qword [ecx]
	movq		xmm3, qword [edx]
	add			eax,8
	add			ebx,8
	add			ecx,8
	add			edx,8
	punpcklbw	xmm0, xmm1
	punpcklbw	xmm2, xmm3
	movdqa		xmm1, xmm0
	movdqa		xmm3, xmm2
	punpcklbw	xmm0, xmm7
	punpckhbw	xmm1, xmm7
	punpcklbw	xmm2, xmm7
	punpckhbw	xmm3, xmm7
	pmaddwd		xmm0, xmm4
	pmaddwd		xmm1, xmm4
	pmaddwd		xmm2, xmm5
	pmaddwd		xmm3, xmm5
	paddd		xmm0, xmm2
	paddd		xmm1, xmm3

	movq		xmm2, qword [esi]
	movq		xmm3, qword [esp]
	add			esi, 8
	add			esp, 8
	punpcklbw	xmm2, xmm3
	movdqa		xmm3, xmm2
	punpcklbw	xmm2, xmm7
	punpckhbw	xmm3, xmm7
	pmaddwd		xmm2, xmm6
	pmaddwd		xmm3, xmm6
	paddd		xmm0, xmm2
	paddd		xmm1, xmm3
	paddd		xmm0, oword [MMX_roundval]
	paddd		xmm1, oword [MMX_roundval]
	psrad		xmm0, 14
	psrad		xmm1, 14
	packssdw	xmm0, xmm1
	packuswb	xmm0, xmm0
	movq		qword [edi+ebp],xmm0
	add			ebp, 8
	jae			.pixelloop_6coeff_dualpel
	jnz			.xit_6coeff

.oddpixel_6coeff:
	movd		xmm0, dword [eax]
	movd		xmm1, dword [ebx]
	movd		xmm2, dword [ecx]
	movd		xmm3, dword [edx]
	punpcklbw	xmm0, xmm1
	punpcklbw	xmm2, xmm3
	movd		xmm1, dword [esi]
	movd		xmm3, dword [esp]
	punpcklbw	xmm0, xmm7
	punpcklbw	xmm2, xmm7
	pmaddwd		xmm0, xmm4
	punpcklbw	xmm1, xmm3
	pmaddwd		xmm2, xmm5
	punpcklbw	xmm1, xmm7
	pmaddwd		xmm1, xmm6
	paddd		xmm0, xmm2
	paddd		xmm1, oword [MMX_roundval]
	paddd		xmm0, xmm1
	psrad		xmm0, 14
	packssdw	xmm0, xmm0
	packuswb	xmm0, xmm0
	movd		dword [edi],xmm0

.xit_6coeff:
	mov			esp, dword [fs:0]
	pop			dword [fs:0]
	pop			eax
	jmp			.xit


;	long resize_table_col_by2linear_MMX(Pixel *out, Pixel **in_table, PixDim w);

	global	_resize_table_col_by2linear_MMX	

_resize_table_col_by2linear_MMX:
	push		ebp
	push		esi
	push		edi
	push		ebx

	mov		ebp,[esp + 12 + 16]		;ebp = pixel counter
	shl		ebp,2
	neg		ebp
	mov		edi,[esp + 4 + 16]		;edi = destination pointer

	;load row pointers!

	mov		esi,[esp + 8 + 16]		;esi = row pointer table

	mov		eax,[esi+4]
	mov		ebx,[esi+8]
	mov		ecx,[esi+12]

	and		ebp,0fffffff8h
	jz		oddpixelcheck

	sub		eax,ebp
	sub		ebx,ebp
	sub		ecx,ebp
	sub		edi,ebp
	movq		mm6,[x0002000200020002]
	pxor		mm7,mm7

.pixelloop:
	movq		mm0,[eax+ebp]

	movq		mm1,[ebx+ebp]
	movq		mm3,mm0

	movq		mm2,[ecx+ebp]
	punpcklbw	mm0,mm7

	movq		mm4,mm1
	punpckhbw	mm3,mm7

	movq		mm5,mm2
	punpcklbw	mm1,mm7

	punpckhbw	mm4,mm7
	paddw		mm1,mm1

	punpcklbw	mm2,mm7
	paddw		mm4,mm4

	punpckhbw	mm5,mm7
	paddw		mm0,mm2

	paddw		mm3,mm5
	paddw		mm1,mm6

	paddw		mm4,mm6
	paddw		mm1,mm0

	paddw		mm4,mm3
	psraw		mm1,2

	psraw		mm4,2
	add		ebp,8

	packuswb	mm1,mm4

	movq		[edi+ebp-8],mm1
	jne		.pixelloop

oddpixelcheck:
	test		dword [esp+12+16],1
	jz		xit

	movd		mm0,dword [eax]

	movd		mm1,dword [ebx]
	punpcklbw	mm0,mm7
	movd		mm2,dword [ecx]
	punpcklbw	mm1,mm7

	punpcklbw	mm2,mm7
	paddw		mm1,mm1

	paddw		mm0,mm2
	paddw		mm1,mm6

	paddw		mm1,mm0

	psraw		mm1,2

	packuswb	mm1,mm1

	movd		dword [edi],mm1

xit:
	pop		ebx
	pop		edi
	pop		esi
	pop		ebp

	ret

;	long resize_table_col_by2cubic_MMX(Pixel *out, Pixel **in_table, PixDim w);

	global	_resize_table_col_by2cubic_MMX	

_resize_table_col_by2cubic_MMX:
	push	ebp
	push	esi
	push	edi
	push	edx
	push	ecx
	push	ebx

	mov	ebp,[esp + 12 + 24]		;ebp = pixel counter
	shl	ebp,2
	neg	ebp
	mov	edi,[esp + 4 + 24]		;edi = destination pointer

	;load row pointers!

	mov	esi,[esp + 8 + 24]		;esi = row pointer table

	mov	eax,[esi+4]
	mov	ebx,[esi+12]
	mov	ecx,[esi+16]
	mov	edx,[esi+20]
	mov	esi,[esi+28]

	sub	eax,ebp
	sub	ebx,ebp
	sub	ecx,ebp
	sub	edx,ebp
	sub	esi,ebp
	sub	edi,ebp
	movq	mm6,[x0008000800080008]
	pxor	mm7,mm7

.pixelloop:
	movd		mm0,dword [eax+ebp]

	movd		mm1,dword [ebx+ebp]
	punpcklbw	mm0,mm7
	movd		mm2,dword [ecx+ebp]
	punpcklbw	mm1,mm7
	movd		mm3,dword [edx+ebp]
	punpcklbw	mm2,mm7
	movd		mm4,dword [esi+ebp]
	punpcklbw	mm3,mm7

	punpcklbw	mm4,mm7
	paddw		mm1,mm3

	movq		mm3,mm1
	psllw		mm1,2

	paddw		mm0,mm4
	psllw		mm2,3

	paddw		mm1,mm3
	paddw		mm2,mm6

	psubw		mm2,mm0
	paddw		mm2,mm1

	psraw		mm2,4

	packuswb	mm2,mm2
	add		ebp,4
	movd		dword [edi+ebp-4],mm2
	jne		.pixelloop

	pop		ebx
	pop		ecx
	pop		edx
	pop		edi
	pop		esi
	pop		ebp

	ret

	end
