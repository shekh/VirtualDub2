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

	global _asm_grayscale_run	

;asm_grayscale_run(
;	[esp+ 4] void *dst,
;	[esp+ 8] ulong width,
;	[esp+12] ulong height,
;	[esp+16] ulong stride);
;
; normal conversion equation is:
;	Y = 0.212671 * R + 0.715160 * G + 0.072169 * B;
;
; We use:
;	Y = (54 * R + 183 * G + 19 * B)/256;
;
; This set of instructions gives us 19:
;
;	lea	ebx,[eax*8+eax]
;	lea	eax,[ebx*2+eax]
;
; This set of instructions gives us 54:
;
;	add	ecx,ecx
;	lea	ecx,[ecx*2+ecx]
;	lea	ecx,[ecx*8+ecx]
;
; This set of instructions gives us 183: = 184-1 = 92*2-1 = 46*4-1 = 23*8-1 = (24-1)*8-1
;
;	lea	edx,[ebp*8+ebp]
;	lea	edx,[edx*4+edx]
;	lea	edx,[edx*2+ebp]
;	lea	edx,[edx*2+ebp]

_asm_grayscale_run:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+ 4+28]
	mov	ebp,[esp+12+28]

grayscale.rowloop:
	push	ebp
	mov	ebp,[esp+ 8+32]

	;the people in r.g.p are a little crazy... this code is based on that
	;which Robert Blum posted.
	;
	;me: 19 cycles (once the stupid loop-initial AGI is removed)
	;him: 15 cycles (doh!)

	%if 1
	push	esi
grayscale.colloop:
	mov     eax,[esi]		;1u EAX=?.R.G.B
        xor     ebx,ebx			;1v EBX=0
        mov     edx,eax			;2u EDX=?.R.G.B
        and     eax,00ff00ffh		;2v EAX=[R][B]
        mov     bl,dh			;3u EBX=[][G]
        mov     edi,eax			;3v ESI=[R][B]
        xor     ecx,ecx			;4u EDI=0
        lea     edx,[eax+8*eax]		;4v EDX=[R*9][B*9]
        lea     ecx,[ebx+2*ebx]		;5u ECX=[G*3]
        lea     ebx,[ebx+8*ebx]		;5v EBX=[G*9]
        shl     ecx,6			;6u ECX=[G*192]
        lea     eax,[edx+2*edx]		;6v EAX=[R*27][B*27]
        lea     edi,[edi+2*edx]		;7u ESI=[R*19][B*19]
        sub     ecx,ebx			;7v ECX=[183*G]
        shr     eax,15			;8u EAX=[54*R]
        add     ecx,edi			;8v ECX=[183*G+19*B]
        add     eax,ecx			;9u EAX=54*R+183*G+19*B
	add	esi,4			;9v
	mov	ebx,eax			;10u
	mov	ecx,eax			;10v
        shr     eax,8			;11u EAX=Y
	and	ebx,0000ff00h		;11v
	shl	ecx,8			;12u
	and	eax,000000ffh		;12v
	or	eax,ebx			;13u
	and	ecx,00ff0000h		;13v
	or	eax,ecx			;14u
	dec	ebp			;14v
	mov	[esi-4],eax		;15u
	jne	grayscale.colloop	;15v
	pop	esi

	%else

grayscale.colloop:
	mov	eax,[esi+ebp*4-4]	;u
	nop				;v
	mov	ecx,eax			;u
	mov	edx,eax			;v
	shr	ecx,16			;u
	and	eax,000000ffh		;v EAX = blue
	shr	edx,8			;u
	and	ecx,000000ffh		;v ECX = red
	lea	ebx,[eax*8+eax]		;u **blue 1**
	and	edx,000000ffh		;v EDX = green
	lea	ecx,[ecx*2+ecx]		;u **red 1**
	nop				;v
	lea	edi,[edx*2+edx]		;u **green 1**
	lea	eax,[ebx*2+eax]		;v **blue 2** FINISHED
	shl	edi,3			;u **green 2**
	add	ecx,ecx			;v **red 2**
	sub	edi,edx			;u **green 3**
	lea	ecx,[ecx*8+ecx]		;v **red 3** FINISHED
	shl	edi,3			;u **green 4**
	add	eax,ecx			;v EAX = blue + red
	sub	edi,edx			;u
	nop				;v
	add	eax,edi			;u EAX = red + green + blue
	nop				;v
	mov	ebx,eax			;u
	mov	ecx,eax			;v
	shl	ecx,8			;u
	and	ebx,0000ff00h		;v
	shr	eax,8			;u
	and	ecx,00ff0000h		;v
	or	eax,ebx			;u
	or	eax,ecx			;u
	mov	[esi+ebp*4-4],eax	;u
	dec	ebp			;u
	jne	grayscale.colloop	;v
	%endif

	pop	ebp
	add	esi,[esp+16+28]

	dec	ebp
	jne	grayscale.rowloop

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

	end
