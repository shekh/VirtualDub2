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

	global	_asm_reduceby2_32	

;asm_reduceby2_32(
;	[esp+ 4] void *dst,
;	[esp+ 8] void *src,
;	[esp+12] ulong width,
;	[esp+16] ulong height,
;	[esp+20] ulong srcstride,
;	[esp+24] ulong dststride);

_asm_reduceby2_32:
	test	byte [_MMX_enabled], 1
	jnz	_asm_reduceby2_32_MMX

	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+ 8+28]
	mov	edi,[esp+20+28]
	add	edi,esi
	mov	edx,[esp+ 4+28]

	mov	ebp,[esp+16+28]

rowloop:
	push	ebp
	mov	ebp,[esp+12+32]
colloop:
	push	edx

	mov	eax,[esi+ebp*8-8]
	mov	ebx,[esi+ebp*8-4]
	mov	ecx,eax
	mov	edx,ebx

	and	eax,00ff00ffh
	and	ebx,00ff00ffh
	and	ecx,0000ff00h
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[edi+ebp*8-8]
	mov	edx,ebx
	and	ebx,00ff00ffh
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[edi+ebp*8-4]
	mov	edx,ebx
	and	ebx,00ff00ffh
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	shr	eax,2
	and	ecx,0003fc00h
	shr	ecx,2
	and	eax,00ff00ffh
	pop	edx
	or	eax,ecx

	mov	[edx+ebp*4-4],eax
	dec	ebp
	jne	colloop
	pop	ebp

	add	esi,[esp+20+28]
	add	edx,[esp+24+28]
	add	edi,[esp+20+28]
	add	esi,[esp+20+28]
	add	edi,[esp+20+28]

	dec	ebp
	jne	rowloop

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

_asm_reduceby2_32_MMX:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+ 8+28]
	mov	edi,[esp+20+28]
	add	edi,esi
	mov	edx,[esp+ 4+28]

	mov	ebp,[esp+16+28]

	pxor	mm7,mm7

rowloop_MMX:
	push	ebp
	mov	ebp,[esp+12+32]
colloop_MMX:
	movq	mm0,[esi+ebp*8-8]

	movq	mm1,[edi+ebp*8-8]
	movq	mm2,mm0

	punpcklbw mm0,mm7
	movq	mm3,mm1

	punpcklbw mm1,mm7

	punpckhbw mm2,mm7
	paddw	mm0,mm1

	punpckhbw mm3,mm7
	paddw	mm0,mm2

	paddw	mm0,mm3

	psrlw	mm0,2

	packuswb mm0,mm0
	dec	ebp

	movd	dword [edx+ebp*4],mm0
	jne	colloop_MMX

	pop	ebp

	add	esi,[esp+20+28]
	add	edx,[esp+24+28]
	add	edi,[esp+20+28]
	add	esi,[esp+20+28]
	add	edi,[esp+20+28]

	dec	ebp
	jne	rowloop_MMX

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	emms
	ret

;**********************************************************

	global	_asm_reduce2hq_run	

;asm_reduce2hq_run(
;	[esp+ 4] void *dst,
;	[esp+ 8] void *src,
;	[esp+12] ulong width,
;	[esp+16] ulong height,
;	[esp+20] ulong srcstride,
;	[esp+24] ulong dststride);

_asm_reduce2hq_run:
	test	byte [_MMX_enabled], 1
	jnz	_asm_reduce2hq_run_MMX

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

rowloop2:
	push	ebp
	mov	ebp,[esp+12+32]
	mov	eax,ebp
	shl	eax,3
	add	esi,eax
colloop2:
	
	; middle center is 4x
	; top center, middle left, middle right, and bottom center are 2x
	; corners are 1x

	push	edx
	mov	eax,[esi-8]		;top center	(2x)
	mov	ebx,[esi+edi+4-8]	;middle right	(2x)
	sub	esi,8
	mov	ecx,eax
	mov	edx,ebx

	and	eax,00ff00ffh
	and	ebx,00ff00ffh
	and	ecx,0000ff00h
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi+edi-4]		;middle left	(2x)
	mov	edx,ebx
	and	ebx,00ff00ffh
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi+edi*2]		;bottom center	(2x)
	mov	edx,ebx
	and	ebx,00ff00ffh
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	add	eax,eax			;double the ortho edges
	add	ecx,ecx

	mov	ebx,[esi-4]		;top left	(1x)
	mov	edx,ebx
	and	ebx,00ff00ffh
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi+4]		;top right	(1x)
	mov	edx,ebx
	and	ebx,00ff00ffh
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi+edi*2-4]	;bottom left	(1x)
	mov	edx,ebx
	and	ebx,00ff00ffh
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi+edi*2+4]	;bottom right	(1x)
	mov	edx,ebx
	and	ebx,00ff00ffh
	and	edx,0000ff00h
	add	eax,ebx
	add	ecx,edx

	mov	ebx,[esi+edi]		;center		(4x)
	mov	edx,ebx
	and	ebx,00ff00ffh
	and	edx,0000ff00h
	shl	ebx,2
	shl	edx,2
	add	eax,ebx
	add	ecx,edx

	shr	eax,4
	and	ecx,000ff000h
	shr	ecx,4
	and	eax,00ff00ffh
	pop	edx
	or	eax,ecx

	mov	[edx+ebp*4-4],eax
	dec	ebp
	jne	colloop2

	pop	ebp

	add	esi,edi
	add	esi,edi
	add	edx,[esp+24+28]

	dec	ebp
	jne	rowloop2

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

_asm_reduce2hq_run_MMX:
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

	pxor	mm7,mm7

rowloop2_MMX:
	push	ebp
	push	esi
	push	edx
	mov	ebp,[esp+12+40]
colloop2_MMX:
	
	; middle center is 4x
	; top center, middle left, middle right, and bottom center are 2x
	; corners are 1x

	%if 0
	movd	mm0,[esi]
	movd	mm1,[esi+edi-4]
	movd	mm2,[esi+edi+4]
	movd	mm3,[esi+edi*2]
	punpcklbw mm0,mm7
	punpcklbw mm1,mm7
	punpcklbw mm2,mm7
	punpcklbw mm3,mm7
	paddw	mm0,mm1
	paddw	mm0,mm2
	paddw	mm0,mm3
	paddw	mm0,mm0

	movd	mm1,[esi-4]
	movd	mm2,[esi+4]
	movd	mm3,[esi+edi*2-4]
	movd	mm4,[esi+edi*2+4]
	punpcklbw mm1,mm7
	punpcklbw mm2,mm7
	punpcklbw mm3,mm7
	punpcklbw mm4,mm7
	paddw	mm0,mm1
	paddw	mm0,mm2
	paddw	mm0,mm3
	paddw	mm0,mm4

	movd	mm1,[esi+edi]
	punpcklbw mm1,mm7
	psllw	mm1,2
	paddw	mm0,mm1
	psrlw	mm0,4
	packuswb mm0,mm0
	movd	[edx],mm0

	add	esi,8
	add	edx,4

	dec	ebp
	jne	colloop2_MMX

	%else

	movd	mm0,dword [esi]

	movd	mm1,dword [esi+edi-4]
	punpcklbw mm0,mm7

	movd	mm2,dword [esi+edi+4]
	punpcklbw mm1,mm7

	movd	mm3,dword [esi+edi*2]
	punpcklbw mm2,mm7

	punpcklbw mm3,mm7
	paddw	mm0,mm1

	movd	mm1,dword [esi-4]
	paddw	mm0,mm2

	movd	mm2,dword [esi+4]
	paddw	mm0,mm3

	movd	mm3,dword [esi+edi*2-4]
	paddw	mm0,mm0

	movd	mm4,dword [esi+edi*2+4]
	punpcklbw mm1,mm7

	punpcklbw mm2,mm7
	paddw	mm0,mm2

	punpcklbw mm3,mm7
	paddw	mm0,mm1

	movd	mm1,dword [esi+edi]
	punpcklbw mm4,mm7

	punpcklbw mm1,mm7
	paddw	mm0,mm3

	psllw	mm1,2
	paddw	mm0,mm4

	paddw	mm0,mm1
	add	esi,8

	psrlw	mm0,4
	add	edx,4

	packuswb mm0,mm0
	dec	ebp

	movd	dword [edx-4],mm0
	jne	colloop2_MMX

	%endif

	pop	edx
	pop	esi
	pop	ebp

	add	esi,edi
	add	esi,edi
	add	edx,[esp+24+28]

	dec	ebp
	jne	rowloop2_MMX

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
