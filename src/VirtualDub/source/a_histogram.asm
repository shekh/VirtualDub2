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

	segment	.data

histo16_red_tab		dd	000h*54, 008h*54, 010h*54, 018h*54
			dd	021h*54, 029h*54, 031h*54, 039h*54
			dd	042h*54, 04ah*54, 052h*54, 05ah*54
			dd	063h*54, 06bh*54, 073h*54, 07bh*54
			dd	084h*54, 08ch*54, 094h*54, 09ch*54
			dd	0a5h*54, 0adh*54, 0b5h*54, 0bdh*54
			dd	0c6h*54, 0ceh*54, 0d6h*54, 0deh*54
			dd	0e7h*54, 0dfh*54, 0f7h*54, 0ffh*54

histo16_grn_tab		dd	000h*183, 008h*183, 010h*183, 018h*183
			dd	021h*183, 029h*183, 031h*183, 039h*183
			dd	042h*183, 04ah*183, 052h*183, 05ah*183
			dd	063h*183, 06bh*183, 073h*183, 07bh*183
			dd	084h*183, 08ch*183, 094h*183, 09ch*183
			dd	0a5h*183, 0adh*183, 0b5h*183, 0bdh*183
			dd	0c6h*183, 0ceh*183, 0d6h*183, 0deh*183
			dd	0e7h*183, 0dfh*183, 0f7h*183, 0ffh*183

histo16_blu_tab		dd	000h*19, 008h*19, 010h*19, 018h*19
			dd	021h*19, 029h*19, 031h*19, 039h*19
			dd	042h*19, 04ah*19, 052h*19, 05ah*19
			dd	063h*19, 06bh*19, 073h*19, 07bh*19
			dd	084h*19, 08ch*19, 094h*19, 09ch*19
			dd	0a5h*19, 0adh*19, 0b5h*19, 0bdh*19
			dd	0c6h*19, 0ceh*19, 0d6h*19, 0deh*19
			dd	0e7h*19, 0dfh*19, 0f7h*19, 0ffh*19

	segment	.text

	global _asm_histogram_gray_run	
	global _asm_histogram_gray24_run	
	global _asm_histogram_color_run	
	global _asm_histogram_color24_run	
	global _asm_histogram16_run	

;asm_histogram_gray_run(
;	[esp+ 4] void *src,
;	[esp+ 8] ulong width,
;	[esp+12] ulong height,
;	[esp+16] ulong stride,
;	[esp+20] ulong *histo_table);
;
; See the grayscale function for more details.

_asm_histogram_gray_run:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+ 4+28]
	mov	ebp,[esp+12+28]

histogram.rowloop:
	push	ebp
	mov	ebp,[esp+ 8+32]

	push	esi
histogram.colloop:
	mov     eax,[esi]		;1u EAX=?.R.G.B
        xor     ebx,ebx			;1v EBX=0
        mov     edx,eax			;2u EDX=?.R.G.B
        and     eax,00ff00ffh		;2v EAX=[R][B]
        mov     bl,dh			;3u EBX=[][G]
        mov     edi,eax			;3u ESI=[R][B]
	nop				;4u
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
	shr	eax,8			;10u
	mov	edi,[esp+20+36]		;10v
	and	eax,255
	inc	dword [edi+eax*4]	;12u (big AGI penalties... *ugh*)
	dec	ebp			;12v
	jne	histogram.colloop	;15u

	pop	esi

	pop	ebp
	add	esi,[esp+16+28]

	dec	ebp
	jne	histogram.rowloop

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

;asm_histogram_gray24_run(
;	[esp+ 4] void *src,
;	[esp+ 8] ulong width,
;	[esp+12] ulong height,
;	[esp+16] ulong stride,
;	[esp+20] ulong *histo_table);
;
; See the grayscale function for more details.

_asm_histogram_gray24_run:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+ 4+28]
	mov	ebp,[esp+12+28]

histogram24.rowloop:
	push	ebp
	mov	ebp,[esp+ 8+32]

	push	esi
histogram24.colloop:
	mov     eax,[esi]		;1u EAX=?.R.G.B
        xor     ebx,ebx			;1v EBX=0
        mov     edx,eax			;2u EDX=?.R.G.B
        and     eax,00ff00ffh		;2v EAX=[R][B]
        mov     bl,dh			;3u EBX=[][G]
        mov     edi,eax			;3u ESI=[R][B]
	nop				;4u
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
	add	esi,3			;9v
	shr	eax,8			;10u
	mov	edi,[esp+20+36]		;10v
	and	eax,255
	inc	dword [edi+eax*4]	;12u (big AGI penalties... *ugh*)
	dec	ebp			;12v
	jne	histogram24.colloop	;15u

	pop	esi

	pop	ebp
	add	esi,[esp+16+28]

	dec	ebp
	jne	histogram24.rowloop

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

;asm_histogram_color_run(
;	[esp+ 4] uchar *dst,
;	[esp+ 8] ulong width,
;	[esp+12] ulong height,
;	[esp+16] ulong stride,
;	[esp+20] ulong *histo_table);
;

_asm_histogram_color_run:
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

histogram_color.rowloop:
	push	ebp
	mov	ebp,[esp+ 8+32]

	push	esi
	xor	eax,eax
histogram_color.colloop:
	mov	al,[esi]		;1u
	add	esi,4			;1v
	mov	ebx,[edi+eax*4]		;2u [AGI]
	inc	ebx			;4u
	dec	ebp			;4v
	mov	[edi+eax*4],ebx		;5u
	jne	histogram_color.colloop	;5v

	pop	esi

	pop	ebp
	add	esi,[esp+16+28]

	dec	ebp
	jne	histogram_color.rowloop

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

;asm_histogram24_color_run(
;	[esp+ 4] uchar *dst,
;	[esp+ 8] ulong width,
;	[esp+12] ulong height,
;	[esp+16] ulong stride,
;	[esp+20] ulong *histo_table);
;

_asm_histogram_color24_run:
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

histogram_color24.rowloop:
	push	ebp
	mov	ebp,[esp+ 8+32]

	push	esi
	xor	eax,eax
histogram_color24.colloop:
	mov	al,[esi]		;1u
	add	esi,3			;1v
	mov	ebx,[edi+eax*4]		;2u [AGI]
	inc	ebx			;4u
	dec	ebp			;4v
	mov	[edi+eax*4],ebx		;5u
	jne	histogram_color24.colloop	;5v

	pop	esi

	pop	ebp
	add	esi,[esp+16+28]

	dec	ebp
	jne	histogram_color24.rowloop

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

;**************************************************************************

;asm_histogram16_run(
;	[esp+ 4] void *src,
;	[esp+ 8] ulong width,
;	[esp+12] ulong height,
;	[esp+16] ulong stride,
;	[esp+20] ulong *histo_table,
;	[esp+24] ulong pixelmask);
;
; See the grayscale function for more details.

_asm_histogram16_run:
	push	ebp
	push	edi
	push	esi
	push	edx
	push	ecx
	push	ebx
	push	eax

	mov	esi,[esp+ 4+28]
	mov	ebp,[esp+12+28]
	mov	edx,[esp+24+28]

histogram16.rowloop:
	push	ebp
	mov	ebp,[esp+ 8+32]

	push	esi
histogram16.colloop:
	xor	eax,eax

	mov	ax,[esi+0]		;eax = RRRRRGGGGGBBBBB

	and	eax,edx

	mov	ebx,eax			;ebx = RRRRRGGGGGBBBBB

	shr	ebx,10			;ebx = red
	mov	ecx,eax			;ecx = RRRRRGGGGGBBBBB

	shr	ecx,5			;ecx = xRRRRRGGGGG
	and	eax,31			;eax = blue

	mov	ebx,[ebx*4+histo16_red_tab]
	and	ecx,31

	mov	eax,[eax*4+histo16_blu_tab]
	add	ebx,128

	mov	ecx,[ecx*4+histo16_grn_tab]
	add	eax,ebx			;eax = red*rc + blue*bc

	add	eax,ecx			;eax = Y<<8
	mov	edi,[esp+20+36]

	shr	eax,8
	add	esi,2

	inc	dword [edi+eax*4]	;12u (big AGI penalties... *ugh*)

	dec	ebp			;12v
	jne	histogram16.colloop	;15u

	pop	esi

	pop	ebp
	add	esi,[esp+16+28]

	dec	ebp
	jne	histogram16.rowloop

	pop	eax
	pop	ebx
	pop	ecx
	pop	edx
	pop	esi
	pop	edi
	pop	ebp
	ret

	end
