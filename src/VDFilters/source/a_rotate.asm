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

x0100010001000100 dq 0100010001000100h
x0080008000800080 dq 0080008000800080h

point_jumptbl	dd	off0, 0
		dd	off7, -28
		dd	off6, -24
		dd	off5, -20
		dd	off4, -16
		dd	off3, -12
		dd	off2, -8
		dd	off1, -4

	segment	.text

	extern _MMX_enabled : byte
	extern _FPU_enabled : byte

	global	_asm_rotate_point	

;	void asm_rotate_point(
;		[esp+ 4] Pixel *src,
;		[esp+ 8] Pixel *dst,
;		[esp+12] long width,
;		[esp+16] long Ufrac,
;		[esp+20] long Vfrac,
;		[esp+24] long UVintstepV,
;		[esp+28] long UVintstepnoV,
;		[esp+32] long Ustep,
;		[esp+36] long Vstep);

_asm_rotate_point:
	push	eax
	push	ebx
	push	ecx
	push	edx
	push	esi
	push	edi
	push	ebp

	mov	esi,[esp +  4 + 28]
	mov	eax,[esp + 12 + 28]
	mov	edi,[esp +  8 + 28]
	mov	ebx,eax
	mov	ecx,[esp + 16 + 28]
	shl	ebx,2
	mov	edx,[esp + 20 + 28]
	add	ebx,edi
	mov	ebp,[esp + 32 + 28]
	shr	esi,2
	mov	[esp + 12 + 28], ebx

;on entry into loop:
;
;	EAX	temporary for pixel copy
;	EBX	temporary for carry flag
;	ECX	Ufrac
;	EDX	Vfrac
;	ESI	source
;	EDI	dest
;	EBP	U increment
;
; This is why inline assembler don't always cut it... there is no way
; to define a jump table with entry points into the middle of a function!
;
; texmap codelet courtesy of Chris Hacker's texture mapping article...

%macro POINTPIX 1
	add	ecx,ebp
	mov	eax,[esi*4]

	adc	esi,[esp + 28 + 28 + ebx*4]
	add	edx,dword [esp+36+28]

	sbb	ebx,ebx
	mov	[edi+%1*4],eax
%endmacro

	; figure out EDI and jump offsets

	and	eax,7
	add	edi,dword [point_jumptbl + eax*8 + 4]

	add	edx,dword [esp+36+28]
	sbb	ebx,ebx

	jmp	dword [point_jumptbl + eax*8]

	align	16
xloop:

off0:	POINTPIX	0
off1:	POINTPIX	1
off2:	POINTPIX	2
off3:	POINTPIX	3
off4:	POINTPIX	4
off5:	POINTPIX	5
off6:	POINTPIX	6
off7:	POINTPIX	7

	add	edi,4*8
	cmp	edi,[esp + 12 + 28]

	jne	xloop

	pop	ebp
	pop	edi
	pop	esi
	pop	edx
	pop	ecx
	pop	ebx
	pop	eax

	ret

;**************************************************************************

	global	_asm_rotate_bilinear	

;	void asm_rotate_bilinear(
;		[esp+ 4] Pixel *src,
;		[esp+ 8] Pixel *dst,
;		[esp+12] long width,
;		[esp+16] long pitch,
;		[esp+20] long Ufrac,
;		[esp+24] long Vfrac,
;		[esp+28] long UVintstepV,
;		[esp+32] long UVintstepnoV,
;		[esp+36] long Ustep,
;		[esp+40] long Vstep);

_asm_rotate_bilinear:
	test	byte [_MMX_enabled],1
	jnz	_asm_rotate_bilinear_MMX

	push	eax
	push	ebx
	push	ecx
	push	edx
	push	esi
	push	edi
	push	ebp

	push	eax


.l_source		equ 32 + 4
.l_dest			equ 32 + 8
.l_destlimit	equ 32 + 12
.l_pitch		equ 32 + 16
.l_Ufrac		equ 32 + 20
.l_Vfrac		equ 32 + 24
.l_UVinctbl		equ 32 + 28
.l_Ustep		equ 32 + 36
.l_Vstep		equ 32 + 40
.l_co3			equ 0

	mov	eax,[esp + .l_destlimit]
	mov	edx,[esp + .l_dest]
	shl	eax,2
	mov	edi,[esp + .l_source]
	add	eax,edx
	shr	edi,2
	mov	[esp + .l_destlimit], eax

;on entry into loop:
;
;	ECX	V fraction
;	EDX	U fraction >> 24
;	EDI	source address

	mov	edx,[esp+.l_Ufrac]
	mov	ecx,[esp+.l_Vfrac]
	shr	edx,24

.xloop_bilinear:
	;compute coefficients and fetch pixels

	shr	ecx,24				;ecx = bottom half
	mov	ebx,edx				;ebx = right half

	imul	edx,ecx				;edx = bottom right

	shr	edx,8

	sub	ecx,edx				;ecx = bottom left

	mov	esi,ebx
	sub	ebx,edx				;ebx = top right

	add	esi,ecx				;esi = !top left
	mov	[esp+.l_source],edi		;store updated source pixel address

	mov	eax,256
	mov	ebp,[esp+.l_pitch]

	sub	eax,esi
	mov	esi,[edi*4 + ebp + 4]		;esi = bottom right pixel

	mov	edi,esi
	and	esi,00ff00ffh			;esi = bottom right red/blue

	and	edi,0000ff00h			;edi = bottom right green
	mov	[esp+.l_co3],ecx

	imul	esi,edx				;esi = scaled bottom right red/blue
	imul	edi,edx				;edi = scaled bottom right green

	;do bottom left pixel

	mov	edx,[esp+.l_source]
	mov	ecx,00ff00ffh

	mov	edx,[edx*4 + ebp]
	mov	ebp,[esp+.l_source]

	and	ecx,edx
	and	edx,0000ff00h

	imul	ecx,[esp+.l_co3]
	imul	edx,[esp+.l_co3]

	add	esi,ecx
	add	edi,edx

	;do top left pixel

	mov	edx,[ebp*4]
	mov	ecx,00ff00ffh

	and	ecx,edx
	and	edx,0000ff00h

	imul	ecx,eax
	imul	edx,eax

	add	esi,ecx
	add	edi,edx

	;do top right pixel

	mov	edx,[ebp*4+4]
	mov	ecx,00ff00ffh

	and	ecx,edx
	and	edx,0000ff00h

	imul	ecx,ebx
	imul	edx,ebx

	add	esi,ecx
	add	edi,edx

	;finish up pixel

	add	esi,00800080h
	add	edi,00008000h

	shr	esi,8
	and	edi,00ff0000h

	shr	edi,8
	and	esi,00ff00ffh

	mov	eax,[esp+.l_dest]
	or	esi,edi

	mov	[eax],esi
	add	eax,4

	;update address and loop

	mov	edx,[esp+.l_Ufrac]
	mov	ecx,[esp+.l_Vfrac]

	add	ecx,[esp+.l_Vstep]

	sbb	ebx,ebx
	add	edx,[esp+.l_Ustep]

	mov	[esp+.l_Ufrac],edx
	mov	[esp+.l_Vfrac],ecx

	adc	ebp,[esp+.l_UVinctbl+4+ebx*4]
	mov	[esp+.l_dest],eax

	shr	edx,24
	mov	edi,ebp

	cmp	eax,[esp+.l_destlimit]
	jne	.xloop_bilinear

	pop	eax

	pop	ebp
	pop	edi
	pop	esi
	pop	edx
	pop	ecx
	pop	ebx
	pop	eax

	ret

;**************************************************************************

	global	_asm_rotate_bilinear	

;	void asm_rotate_bilinear(
;		[esp+ 4] Pixel *src,
;		[esp+ 8] Pixel *dst,
;		[esp+12] long width,
;		[esp+16] long pitch,
;		[esp+20] long Ufrac,
;		[esp+24] long Vfrac,
;		[esp+28] long UVintstepV,
;		[esp+32] long UVintstepnoV,
;		[esp+36] long Ustep,
;		[esp+40] long Vstep);

_asm_rotate_bilinear_MMX:
	push	eax
	push	ebx
	push	ecx
	push	edx
	push	esi
	push	edi
	push	ebp

.l_source		equ 28 + 4
.l_dest			equ 28 + 8
.l_destlimit	equ 28 + 12
.l_pitch		equ 28 + 16
.l_Ufrac		equ 28 + 20
.l_Vfrac		equ 28 + 24
.l_UVinctbl		equ 28 + 28
.l_Ustep		equ 28 + 36
.l_Vstep		equ 28 + 40

	mov	eax,[esp + .l_destlimit]
	mov	edi,[esp + .l_dest]
	shl	eax,2
	mov	esi,[esp + .l_source]
	add	eax,edi
	shr	esi,2
	mov	[esp + .l_destlimit], eax

;on entry into loop:
;
;	ECX	V fraction
;	EDX	U fraction >> 24
;	ESI	source address
;	EDI	dest address
;	EBP	row pitch

	mov	edx,[esp+.l_Ufrac]
	mov	ecx,[esp+.l_Vfrac]
	mov	ebp,[esp+.l_pitch]
	shr	edx,24

.xloop_bilinearMMX:
	;compute coefficients and fetch pixels

	shr	ecx,24				;ecx = bottom half
	mov	ebx,edx				;ebx = right half

	imul	edx,ecx				;edx = bottom right

	shr	edx,8

	movd	mm6,ecx
	mov	eax,edx

	shl	edx,16
	punpcklwd mm6,mm6

	movq	mm4,[x0100010001000100]
	mov	ecx,ebx	

	shl	ebx,16
	or	edx,eax

	movd	mm7,edx
	punpckldq mm6,mm6			;mm6 = bottom half

	punpckldq mm7,mm7			;mm7 = bottom-right
	or	ebx,ecx

	movd	mm5,ebx
	psubw	mm4,mm6				;mm4 = top half

	punpckldq mm5,mm5			;mm5 = right half
	psubw	mm6,mm7				;mm6 = bottom-left

	psubw	mm5,mm7				;mm7 = top-right

	psubw	mm4,mm5				;mm4 = top-left

	;do pixels

	movd	mm0,dword [esi*4+0]
	pxor	mm3,mm3

	movd	mm1,dword [esi*4+4]
	punpcklbw mm0,mm3

	pmullw	mm0,mm4
	punpcklbw mm1,mm3

	movd	mm2,dword [esi*4+ebp+0]
	pxor	mm4,mm4

	pmullw	mm1,mm5
	punpcklbw mm2,mm4

	movd	mm3,[esi*4+ebp+4]
	pmullw	mm2,mm6

	paddw	mm0,[x0080008000800080]
	punpcklbw mm3,mm4

	pmullw	mm3,mm7
	paddw	mm0,mm1

	paddw	mm0,mm2
	mov	edx,[esp+.l_Ufrac]

	paddw	mm0,mm3
	mov	ecx,[esp+.l_Vfrac]

	psrlw	mm0,8
	add	edi,4

	packuswb mm0,mm0
	add	ecx,[esp+.l_Vstep]

	sbb	ebx,ebx
	add	edx,[esp+.l_Ustep]

	mov	[esp+.l_Ufrac],edx
	mov	[esp+.l_Vfrac],ecx

	adc	esi,[esp+.l_UVinctbl+4+ebx*4]

	shr	edx,24
	cmp	edi,[esp+.l_destlimit]

	movd	dword [edi-4],mm0
	jne	.xloop_bilinearMMX

	pop	ebp
	pop	edi
	pop	esi
	pop	edx
	pop	ecx
	pop	ebx
	pop	eax

	emms
	ret

	end
