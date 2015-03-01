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

	extern _MMX_enabled : byte

	global	_asm_sharpen_run	

;asm_sharpen_run(
;	[esp+ 4] void *dst,
;	[esp+ 8] void *src,
;	[esp+12] ulong width,
;	[esp+16] ulong height,
;	[esp+20] ulong srcstride,
;	[esp+24] ulong dststride,
;	[esp+28] long a_mult,
;	[esp+32] long b_mult);

_asm_sharpen_run:

	test	byte [_MMX_enabled],1
	jnz	_asm_sharpen_run_MMX

	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+ 8+28]
	mov	edi,[esp+20+28]
	mov	edx,[esp+ 4+28]

	mov	ebp,[esp+16+28]

.rowloop:
	push	ebp
	mov	ebp,[esp+12+32]
	mov	eax,ebp
	shl	eax,2
	add	esi,eax
.colloop:
	push	ebp
	push	edx

	mov	eax,[esi-4]
	mov	ebx,[esi+4-4]
	mov	ecx,eax
	mov	edx,ebx

	and	eax,00ff00ffh
	and	ebx,00ff00ffh
	and	ecx,0000ff00h
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi+edi+4-4]
	mov	edx,ebx
	and	ebx,00ff00ffh
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi+edi*2-4]
	mov	edx,ebx
	and	ebx,00ff00ffh
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi+edi*2+4-4]
	mov	edx,ebx
	and	ebx,00ff00ffh
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi-4-4]
	mov	edx,ebx
	and	ebx,00ff00ffh
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi+edi-4-4]
	mov	edx,ebx
	and	ebx,00ff00ffh
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi+edi*2-4-4]
	mov	edx,ebx
	and	ebx,00ff00ffh
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	;EAX now holds the neighbor sum of red and blue
	;ECX now holds the neighbor sum of green

	mov	ebx,eax
	mov	ebp,[esi+edi-4]
	shr	ebx,16
	and	eax,0000ffffh
	imul	eax,[esp+28+40]
	imul	ebx,[esp+28+40]
	imul	ecx,[esp+28+40]

	push	esi
	push	edi

	mov	edx,ebp
	mov	esi,ebp
	shr	ebp,16
	and	edx,000000ffh
	and	ebp,000000ffh
	and	esi,0000ff00h
	imul	edx,[esp+32+48]
	imul	esi,[esp+32+48]
	imul	ebp,[esp+32+48]
	add	eax,edx
	add	ecx,esi
	add	ebx,ebp

	mov	edx,edx

	;(ESI,EDX):
	;	00010000-7FFFFFFF	11
	;	00000000-0000FFFF	10
	;	80000000-FFFFFFFF	00

	mov	esi,eax		;b1 u
	mov	edi,ebx		;r1 v
	xor	esi,-1		;b2 u
	xor	edi,-1		;r2 v
	mov	edx,esi		;b3 u
	mov	ebp,edi		;r3 v
	sar	esi,31		;b4 u
	add	edx,00010000h	;b5 v
	sar	edi,31		;r4 u
	add	ebp,00010000h	;r5 v
	sar	edx,31		;b6 u
	and	eax,esi		;b7 v
	sar	ebp,31		;r6 u
	mov	esi,ecx		;g1 v
	and	ebx,edi		;r7 u
	xor	esi,-1		;g2 v
	or	eax,edx		;b8 u
	mov	edx,esi		;g3 v
	sar	esi,31		;g4 u
	add	edx,01000000h	;g5 v
	and	ecx,esi		;g6 u
	or	ebx,ebp		;r8 v
	sar	edx,31		;g7 u
	and	ebx,0000ff00h	;   v
	shl	ebx,8		;   u
	or	ecx,edx		;g8 v
	shr	eax,8		;   u
	and	ecx,00ff0000h	;   v
	shr	ecx,8		;   u
	pop	edi		;   v
	pop	esi		;   u
	and	eax,000000ffh	;   v
	pop	edx		;   u
	or	eax,ecx		;   v
	pop	ebp		;   u
	or	eax,ebx		;   v
	sub	esi,4		;   u
	mov	[edx+ebp*4-4],eax	;v
	dec	ebp
	jne	.colloop

	pop	ebp

	add	esi,edi
	add	edx,[esp+24+28]

	dec	ebp
	jne	.rowloop

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

;*********

_asm_sharpen_run_MMX:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+ 8+28]
	mov	edi,[esp+20+28]
	mov	edx,[esp+ 4+28]

	mov	ebp,[esp+16+28]

	;mm5: neighbor multiplier
	;mm6: center multiplier
	;mm7: zero

	pxor	mm7,mm7

	mov	eax,[esp+28+28]
	neg	eax
	shr	eax,2
	mov	ebx,eax
	shl	ebx,16
	or	eax,ebx
	movd	mm0,eax
	movd	mm5,eax
	psllq	mm5,32
	por	mm5,mm0

	mov	eax,[esp+32+28]
	shr	eax,2
	and	eax,0000fff8h
	mov	ebx,eax
	shl	ebx,16
	or	eax,ebx
	movd	mm0,eax
	movd	mm6,eax
	psllq	mm6,32
	por	mm6,mm0

.rowloop_MMX:
	push	ebp
	push	edx
	push	esi
	mov	ebp,[esp+12+40]

	movd	mm0,dword [esi-4]
	punpcklbw mm0,mm7

	movq	mm1,[esi]
	movq	mm2,mm1
	punpcklbw mm1,mm7
	punpckhbw mm2,mm7
	paddw	mm0,mm1
	paddw	mm0,mm2

	movd	mm1,dword [esi+edi*2-4]
	punpcklbw mm1,mm7
	paddw	mm0,mm1

	movq	mm1,[esi+edi*2]
	movq	mm2,mm1
	punpcklbw mm1,mm7
	punpckhbw mm2,mm7
	paddw	mm0,mm1
	paddw	mm0,mm2

	movd	mm1,dword [esi+edi-4]
	punpcklbw mm1,mm7
	movd	mm2,dword [esi+edi+4]
	punpcklbw mm2,mm7
	paddw	mm1,mm2
	paddw	mm1,mm0
	pmullw	mm1,mm5

	movd	mm2,dword [esi+edi]
	punpcklbw mm2,mm7
	pmullw	mm2,mm6
	psubusw	mm2,mm1
	psrlw	mm2,6
	packuswb mm2,mm2
	movd	dword [edx],mm2

	add	esi,4
	dec	ebp

	movd	mm1,dword [esi+4]
	add	edx,4
	movd	mm4,dword [esi+edi*2+4]
	punpcklbw mm1,mm7
	punpcklbw mm4,mm7
	paddw	mm0,mm1
	movd	mm1,dword [esi-8]
	movd	mm2,dword [esi+edi*2-8]
	punpcklbw mm1,mm7
	paddw	mm0,mm4

	jmp	.colloop_MMX_entry

.colloop_MMX:
	movd	mm1,dword [esi+4]
	add	edx,4

	movd	mm4,dword [esi+edi*2+4]
	punpcklbw mm1,mm7

	psubusw	mm3,mm2
	punpcklbw mm4,mm7

	psrlw	mm3,6
	paddw	mm0,mm1

	movd	mm1,dword [esi-8]
	packuswb mm3,mm3

	movd	mm2,dword [esi+edi*2-8]
	punpcklbw mm1,mm7

	movd	dword [edx-4],mm3
	paddw	mm0,mm4

.colloop_MMX_entry:

	punpcklbw mm2,mm7
	psubw	mm0,mm1

	movd	mm1,dword [esi+edi-4]
	psubw	mm0,mm2

	movd	mm2,dword [esi+edi+4]
	punpcklbw mm1,mm7

	movd	mm3,dword [esi+edi]
	punpcklbw mm2,mm7

	punpcklbw mm3,mm7
	paddw	mm1,mm0

	pmullw	mm3,mm6
	paddw	mm2,mm1

	pmullw	mm2,mm5
	add	esi,4

	dec	ebp
	jne	.colloop_MMX

	psubusw	mm3,mm2
	psrlw	mm3,6
	packuswb mm3,mm3
	movd	dword [edx],mm3

	pop	esi
	pop	edx
	pop	ebp

	add	esi,edi
	add	edx,[esp+24+28]

	dec	ebp
	jne	.rowloop_MMX

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	emms
	ret

	end
