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

	global _asm_threshold_run	

;asm_threshold_run(
;	[esp+ 4] void *dst,
;	[esp+ 8] ulong width,
;	[esp+12] ulong height,
;	[esp+16] ulong stride,
;	[esp+20] ulong threshold);
;
;	This code is based off of the grayscale code.  See a_grayscale.asm for
;	further info.

_asm_threshold_run:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+ 4+28]
	mov	ebp,[esp+12+28]

	mov	eax,[esp+20+28]
	mov	ebx,80000000h
	shl	eax,8
	sub	ebx,eax
	push	ebx


threshold.rowloop:
	push	ebp
	mov	ebp,[esp+ 8+36]

	push	esi
threshold.colloop:
	mov     eax,[esi]		;1u EAX=?.R.G.B
        xor     ebx,ebx			;1v EBX=0
        mov     edx,eax			;2u EDX=?.R.G.B
        and     eax,00ff00ffh		;2v EAX=[R][B]
        mov     bl,dh			;3u EBX=[][G]
        mov     edi,eax			;3v EDI=[R][B]
        xor     ecx,ecx			;4u ECX=0
        lea     edx,[eax+8*eax]		;4v EDX=[R*9][B*9]
        lea     ecx,[ebx+2*ebx]		;5u ECX=[G*3]
        lea     ebx,[ebx+8*ebx]		;5v EBX=[G*9]
        shl     ecx,6			;6u ECX=[G*192]
        lea     eax,[edx+2*edx]		;6v EAX=[R*27][B*27]
        lea     edi,[edi+2*edx]		;7u EDI=[R*19][B*19]
        sub     ecx,ebx			;7v ECX=[183*G]
        shr     eax,15			;8u EAX=[54*R]
        add     ecx,edi			;8v ECX=[183*G+19*B]
        add     eax,ecx			;9u EAX=54*R+183*G+19*B
	add	esi,4			;9v
	and	eax,0000ffffh		;10u
	add	eax,[esp+8]		;11u
	sar	eax,31			;12u
	dec	ebp			;12v
	mov	[esi-4],eax		;13u
	jne	threshold.colloop	;13v

	pop	esi

	pop	ebp
	add	esi,[esp+16+32]

	dec	ebp
	jne	threshold.rowloop

	pop	eax			;remove thresholding temp

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

	end
