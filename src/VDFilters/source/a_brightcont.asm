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

	global _asm_brightcont1_run	
	global _asm_brightcont2_run	

;asm_brightcont(x)_run(
;	[esp+ 4] void *dst,
;	[esp+ 8] ulong width,
;	[esp+12] ulong height,
;	[esp+16] ulong stride
;	[esp+20] ulong multiplier,
;	[esp+24] ulong adder1,
;	[esp+28] ulong adder2);

_asm_brightcont1_run:
	test	byte [_MMX_enabled], 1
	jnz	_asm_brightcont1_run_MMX

	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+ 4+28]
	mov	edi,[esp+20+28]
	mov	ebp,[esp+12+28]

brightcont1.rowloop:
	push	ebp
	mov	ebp,[esp+ 8+32]
brightcont1.colloop:
	mov	eax,[esi+ebp*4-4]
	mov	ecx,eax

	and	eax,00ff00ffh
	and	ecx,0000ff00h
	mul	edi
	mov	ebx,eax
	mov	eax,ecx
	mul	edi
	or	eax,00800000h
	or	ebx,80008000h
	sub	eax,[esp+28+32]		;normal range is 00400000-005fe000
	sub	ebx,[esp+24+32]		;normal range is 40004000-5fe05fe0
	mov	ecx,eax
	mov	edx,ebx
	and	ecx,00800000h		;will be zero if there was no carry
	and	edx,80008000h
	shr	ecx,15
	shr	edx,15
	or	ecx,00800000h
	or	edx,80008000h
	sub	ecx,00000100h
	sub	edx,00010001h
	xor	ecx,0ff7fffffh
	xor	edx,7fff7fffh
	and	eax,ecx
	and	ebx,edx

	mov	ecx,eax
	mov	edx,ebx
	shr	ecx,1
	add	edx,edx
	or	ecx,eax
	or	edx,ebx
	shr	ecx,12
	and	edx,20002000h
	shr	edx,13
	and	ecx,00000100h		;will be zero if there was no carry
	or	ecx,00800000h
	or	edx,80008000h
	sub	ecx,00000100h
	sub	edx,00010001h
	xor	ecx,000ff000h
	xor	edx,0ff00ff0h
	or	eax,ecx
	or	ebx,edx

	shr	eax,4
	and	ebx,0ff00ff0h
	shr	ebx,4
	and	eax,0000ff00h
	or	eax,ebx

	mov	[esi+ebp*4-4],eax
	dec	ebp
	jne	brightcont1.colloop
	pop	ebp

	add	esi,[esp+16+28]

	dec	ebp
	jne	brightcont1.rowloop

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

_asm_brightcont2_run:
	test	byte [_MMX_enabled], 1
	jnz	_asm_brightcont2_run_MMX

	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+ 4+28]
	mov	edi,[esp+20+28]
	mov	ebp,[esp+12+28]

brightcont2.rowloop:
	push	ebp
	mov	ebp,[esp+ 8+32]
brightcont2.colloop:
	mov	eax,[esi+ebp*4-4]
	mov	ecx,eax

	and	eax,00ff00ffh
	and	ecx,0000ff00h
	mul	edi
	mov	ebx,eax
	mov	eax,ecx
	mul	edi
	add	eax,[esp+28+32]		;normal range is 00000000-001fe000
	add	ebx,[esp+24+32]		;normal range is 00000000-1fe01fe0

	;carry bits are at 0x00300000 and 0x30003000

	%if 1

;	mov	ecx,00100000h
;	mov	edx,10001000h

	mov	ecx,00100000h		;[g]
	mov	edx,eax			;[g]
	shr	edx,10			;[g]
	and	edx,00000c00h

	sub	ecx,edx			;[g   u]
	mov	edx,10001000h		;[r/b v]
	or	eax,ecx			;[g   u]
	mov	ecx,ebx			;[r/b v]
	shr	ecx,10			;[r/b u]
	and	ecx,000c000ch
	sub	edx,ecx			;[r/b v]
	and	eax,000ff000h		;[c   u]
	or	ebx,edx			;[r/b v]
	shr	eax,4			;[c   u]
	and	ebx,0ff00ff0h		;[c   v]
	shr	ebx,4			;[c   u]
	or	eax,ebx			;[c   u]

	%else


	mov	ecx,eax
	mov	edx,ebx
	shr	ecx,1
	add	edx,edx
	or	ecx,eax
	or	edx,ebx
	shr	ecx,12
	and	edx,20002000h
	shr	edx,13
	and	ecx,00000100h		;will be zero if there was no carry
	or	ecx,00800000h
	or	edx,80008000h
	sub	ecx,00000100h
	sub	edx,00010001h
	xor	ecx,000ff000h
	xor	edx,0ff00ff0h
	or	eax,ecx
	or	ebx,edx
	shr	eax,4
	and	ebx,0ff00ff0h
	shr	ebx,4
	and	eax,0000ff00h
	or	eax,ebx

	%endif

	mov	[esi+ebp*4-4],eax
	dec	ebp
	jne	brightcont2.colloop
	pop	ebp

	add	esi,[esp+16+28]

	dec	ebp
	jne	brightcont2.rowloop

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

;********************************************************************

;asm_brightcont(x)_run(
;	[esp+ 4] void *dst,
;	[esp+ 8] ulong width,
;	[esp+12] ulong height,
;	[esp+16] ulong stride
;	[esp+20] ulong multiplier,
;	[esp+24] ulong adder1,
;	[esp+28] ulong adder2);
;
;	mm5:	zero
;	mm6:	multiplier
;	mm7:	subtractor

_asm_brightcont1_run_MMX:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+ 4+28]
	mov	edi,[esp+20+28]
	mov	ebp,[esp+12+28]

	pxor	mm5,mm5

	mov	eax,[esp+20+28]		;replicate multiplier into all four bytes
	mov	ah,al
	rol	eax,8
	mov	al,ah
	rol	eax,8
	mov	al,ah
	movd	mm6,eax			;mm6 = multiplier replicated into 4 words
	punpcklbw mm6,mm5

;	mov	eax,[esp+24+28]		;construct subtractor value
;	or	eax,[esp+28+28]
;	movd	mm7,eax
;	psrlq	mm7,4
;	punpcklbw mm7,mm5

	movd	mm7,dword [esp+24+28]
	psrlq	mm7,4
	movq	mm0,mm7
	psllq	mm0,32
	por	mm7,mm0

brightcont1MMX.rowloop:
	push	ebp
	push	esi
	mov	ebp,[esp+ 8+36]
	shr	ebp,2
	jz	brightcont1MMX.noby4
brightcont1MMX.colloop:
	movq	mm0,[esi]
;	movq	mm7,mm7

	movq	mm2,[esi+8]
	movq	mm1,mm0

	movq	mm3,mm2
	punpcklbw mm0,mm5

	punpckhbw mm1,mm5
	pmullw	mm0,mm6

	punpcklbw mm2,mm5
	pmullw	mm1,mm6

	punpckhbw mm3,mm5
	pmullw	mm2,mm6

	psrlw	mm0,4
	pmullw	mm3,mm6

	psrlw	mm1,4
	psubusw	mm0,mm7

	psrlw	mm2,4
	psubusw	mm1,mm7

	psrlw	mm3,4
	psubusw	mm2,mm7

	packuswb mm0,mm1
	psubusw	mm3,mm7

	packuswb mm2,mm3

	movq	[esi],mm0

	movq	[esi+8],mm2

	add	esi,16

	dec	ebp
	jne	brightcont1MMX.colloop
brightcont1MMX.noby4:

	mov	ebp,[esp+8+36]
	and	ebp,3
	jz	brightcont1MMX.nosingles
brightcont1MMX.colloopsingle:
	movd	mm0,dword [esi]
	punpcklbw mm0,mm5
	pmullw	mm0,mm6
	add	esi,4
	psrlw	mm0,4
	psubusw	mm0,mm7
	packuswb mm0,mm0
	movd	dword [esi-4],mm0
	dec	ebp
	jne	brightcont1MMX.colloopsingle

brightcont1MMX.nosingles:
	pop	esi
	pop	ebp

	add	esi,[esp+16+28]

	dec	ebp
	jne	brightcont1MMX.rowloop

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	emms
	ret

_asm_brightcont2_run_MMX:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+ 4+28]
	mov	edi,[esp+20+28]
	mov	ebp,[esp+12+28]

	pxor	mm5,mm5

	mov	eax,[esp+20+28]		;replicate multiplier into all four bytes
	mov	ah,al
	rol	eax,8
	mov	al,ah
	rol	eax,8
	mov	al,ah
	movd	mm6,eax			;mm6 = multiplier replicated into 4 words
	punpcklbw mm6,mm5

;	mov	eax,[esp+24+28]		;construct subtractor value
;	or	eax,[esp+28+28]
;	movd	mm7,eax
;	psrlq	mm7,4
;	punpcklbw mm7,mm5

	movd	mm7,dword [esp+24+28]
	psrlq	mm7,4
	movq	mm0,mm7
	psllq	mm0,32
	por	mm7,mm0

brightcont2MMX.rowloop:
	push	ebp
	push	esi
	mov	ebp,[esp+ 8+36]
	shr	ebp,2
	jz	brightcont2MMX.noby4
brightcont2MMX.colloop:
	movq	mm0,[esi]
;	movq	mm7,mm7

	movq	mm2,[esi+8]
	movq	mm1,mm0

	movq	mm3,mm2
	punpcklbw mm0,mm5

	punpckhbw mm1,mm5
	pmullw	mm0,mm6

	punpcklbw mm2,mm5
	pmullw	mm1,mm6

	punpckhbw mm3,mm5
	pmullw	mm2,mm6

	psrlw	mm0,4
	pmullw	mm3,mm6

	psrlw	mm1,4
	paddusw	mm0,mm7

	psrlw	mm2,4
	paddusw	mm1,mm7

	psrlw	mm3,4
	paddusw	mm2,mm7

	packuswb mm0,mm1
	paddusw	mm3,mm7

	packuswb mm2,mm3

	movq	[esi],mm0

	movq	[esi+8],mm2

	add	esi,16

	dec	ebp
	jne	brightcont2MMX.colloop
brightcont2MMX.noby4:

	mov	ebp,[esp+8+36]
	and	ebp,3
	jz	brightcont2MMX.nosingles
brightcont2MMX.colloopsingle:
	movd	mm0,dword [esi]
	punpcklbw mm0,mm5
	pmullw	mm0,mm6
	add	esi,4
	psrlw	mm0,4
	paddusw	mm0,mm7
	packuswb mm0,mm0
	movd	dword [esi-4],mm0
	dec	ebp
	jne	brightcont2MMX.colloopsingle

brightcont2MMX.nosingles:
	pop	esi
	pop	ebp

	add	esi,[esp+16+28]

	dec	ebp
	jne	brightcont2MMX.rowloop

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
