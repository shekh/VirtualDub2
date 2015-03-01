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

	global	_asm_convolute_run	
	global	_asm_dynamic_convolute_run	

;asm_convolute_run(
;	[esp+ 4] void *dst,
;	[esp+ 8] void *src,
;	[esp+12] ulong width,
;	[esp+16] ulong height,
;	[esp+20] ulong srcstride,
;	[esp+24] ulong dststride,
;	[esp+28] long *convol_matrix);

_asm_convolute_run:
;	test	_MMX_enabled, 1
;	jnz	_asm_average_run_MMX

	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	sub	esp,40

	cld
	mov	esi,[esp+28+28+40]
	mov	edi,esp
	mov	ecx,10
	rep	movsd

	mov	esi,[esp+ 8+28+40]
	mov	edi,[esp+20+28+40]
	mov	edx,[esp+ 4+28+40]

	mov	ebp,[esp+16+28+40]

rowloop:
	push	ebp
	mov	ebp,[esp+12+32+40]
	mov	eax,ebp
	shl	eax,2
	add	esi,eax
colloop:
	push	edx
	push	ebp

	xor	eax,eax
	xor	ebx,ebx
	xor	ecx,ecx

	%macro CONV_ADD	2
	%define	%%p_source	%1
	%define	%%p_mult	%2

		mov		ebp,%%p_source	;u	EDX = xRGB
		mov		edx,ebp			;u	EBP = xRGB
		and		ebp,000000ffh	;v	EBP = 000B
		imul	ebp,%%p_mult	;uv	EBP = 00bb
		shr		edx,8			;u	EDX = 0xRG
		add		eax,ebp			;v	add blues together
		mov		ebp,edx			;u	EBP = 0xRG
		and		edx,0000ff00h	;v	EDX = 00R0
		shr		edx,8			;u	EDX = 000R
		and		ebp,000000ffh	;v	EBP = 000G
		imul	ebp,%%p_mult	;uv	EBP = 00gg
		imul	edx,%%p_mult	;uv	EDX = 00rr
		add		ebx,ebp			;u
		add		ecx,edx			;v

	%endmacro

	CONV_ADD [esi-4],[esp+24+12]
	CONV_ADD [esi  ],[esp+28+12]
	CONV_ADD [esi+4],[esp+32+12]
	CONV_ADD [esi+edi-4],[esp+12+12]
	CONV_ADD [esi+edi  ],[esp+16+12]
	CONV_ADD [esi+edi+4],[esp+20+12]
	CONV_ADD [esi+edi*2-4],[esp+0+12]
	CONV_ADD [esi+edi*2  ],[esp+4+12]
	CONV_ADD [esi+edi*2+4],[esp+8+12]

	mov	edx,[esp+36+12]
	add	eax,edx
	add	ebx,edx
	add	ecx,edx

	test	eax,0ffff0000h
	jz	nocarry1
	sar	eax,31
	xor	eax,-1
nocarry1:

	test	ebx,0ffff0000h
	jz	nocarry2
	sar	ebx,31
	xor	ebx,-1
nocarry2:

	test	ecx,0ffff0000h
	jz	nocarry3
	sar	ecx,31
	xor	ecx,-1
nocarry3:

	shr	eax,8
	and	ecx,0000ff00h
	shl	ecx,8
	and	eax,000000ffh
	and	ebx,0000ff00h
	add	eax,ecx
	sub	esi,4
	add	eax,ebx

	pop	ebp
	pop	edx

	mov	[edx+ebp*4-4],eax
	dec	ebp
	jne	colloop

	pop	ebp

	add	esi,edi
	add	edx,[esp+24+28+40]

	dec	ebp
	jne	rowloop

	add	esp,40

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret


	segment	.text

;asm_dynamic_convolute_run(
;	[esp+ 4] void *dst,
;	[esp+ 8] void *src,
;	[esp+12] ulong width,
;	[esp+16] ulong height,
;	[esp+20] ulong srcstride,
;	[esp+24] ulong dststride,
;	[esp+28] long *convol_matrix
;	[esp+32] void *dyna_code_ptr);

_asm_dynamic_convolute_run:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	sub	esp,4

	mov	esi,[esp+28+28+4]
	mov	eax,[esi+36]
	mov	[esp],eax

	mov	esi,[esp+ 8+28+4]
	mov	edi,[esp+ 4+28+4]

	mov	ebp,[esp+16+28+4]

rowloop_dyna:
	push	ebp
	mov	ebp,[esp+12+32+4]
	mov	eax,ebp
	shl	eax,2
	add	esi,eax

	call	dword [esp+32+32+4]

	pop	ebp

	add	edi,[esp+20+28+4]
	add	esi,[esp+24+28+4]

	dec	ebp
	jne	rowloop_dyna

	add	esp,4

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

;VC++4.0 is a broken compiler!!!!!!!!!!!!

	global	_asm_dynamic_convolute_codecopy	

codetbl:
	dd	_asm_dynamic_convolute_start_1
	dd	_asm_dynamic_convolute_shift_1
	dd	_asm_dynamic_convolute_end_1

_asm_dynamic_convolute_codecopy:
	push	esi
	push	edi
	mov	esi,[esp+8+8]
	mov	esi,[esi*4+codetbl]
	mov	edi,[esp+4+8]
	mov	ecx,[esi-4]
	cld
	rep	movsb
	mov	eax,edi
	pop	edi
	pop	esi
	ret

	dd	_asm_dynamic_convolute_start_2-_asm_dynamic_convolute_start_1
_asm_dynamic_convolute_start_1:
	push	edi
	push	ebp

	xor	edx,edx
	xor	edi,edi
	xor	ebp,ebp
_asm_dynamic_convolute_start_2:

	dd	_asm_dynamic_convolute_shift_2-_asm_dynamic_convolute_shift_1
_asm_dynamic_convolute_shift_1:
	sar	edi,8
	mov	eax,[esp+12+4]
	add	edx,eax
	add	edi,eax
	add	ebp,eax
_asm_dynamic_convolute_shift_2:

	dd	_asm_dynamic_convolute_end_2-_asm_dynamic_convolute_end_1
_asm_dynamic_convolute_end_1:
	test	edx,0ffff0000h
	jz	nocarry1d
	sar	edx,31
	xor	edx,-1
nocarry1d:

	test	edi,0ffff0000h
	jz	nocarry2d
	sar	edi,31
	xor	edi,-1
nocarry2d:

	test	ebp,0ffff0000h
	jz	nocarry3d
	sar	ebp,31
	xor	ebp,-1
nocarry3d:

	shr	edx,8
	and	ebp,0000ff00h
	shl	ebp,8
	and	edx,000000ffh
	and	edi,0000ff00h
	add	edx,ebp
	sub	esi,4
	add	edx,edi

	pop	ebp
	pop	edi

	mov	[edi+ebp*4-4],edx		;aggh!!! AGI!!!
	dec	ebp
_asm_dynamic_convolute_end_2:

	end
