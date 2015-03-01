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

	global	_asm_scene_lumtile32	
	global	_asm_scene_lumtile24	
	global	_asm_scene_lumtile16	

;asm_scene_lumtile32(src, width, height, modulo, gtotalptr);

_asm_scene_lumtile32:
	push	ebx
	push	ecx
	push	edx
	push	esi
	push	edi
	push	ebp
	mov	esi,[esp+4+24]
	mov	edi,[esp+12+24]
	xor	eax,eax
	xor	ebx,ebx
asm_scene_lumtile32_row:
	mov	ebp,[esp+8+24]
asm_scene_lumtile32_col:
	mov	ecx,[esi + ebp*4 - 4]
	mov	edx,0000ff00h
	and	edx,ecx
	and	ecx,00ff00ffh
	add	eax,ecx
	add	ebx,edx
	dec	ebp
	jne	asm_scene_lumtile32_col
	add	esi,[esp+16+24]
	dec	edi
	jne	asm_scene_lumtile32_row

	add	eax,00200020h
	add	ebx,00002000h
	shr	eax,6
	and	ebx,003fc000h
	shr	ebx,6
	and	eax,00ff00ffh
	add	eax,ebx

	pop	ebp
	pop	edi
	pop	esi
	pop	edx
	pop	ecx
	pop	ebx
	ret

;asm_scene_lumtile24(src, width, height, modulo, btotalptr);

_asm_scene_lumtile24:
	push	ebx
	push	esi
	push	edi
	push	ebp
	mov		esi, [esp+4+16]
	mov		edi, [esp+12+16]
	xor		eax, eax
	xor		ebx, ebx
	xor		ecx, ecx
asm_scene_lumtile24_row:
	mov	ebp,[esp+8+16]
	push	esi
	
	; edx	temp
	; eax	red sum
	; ebx	green sum
	; ecx	blue sum
	xor		edx, edx
asm_scene_lumtile24_col:
	mov		dl, [esi]
	add		ecx, edx
	mov		dl, [esi+1]
	add		ebx, edx
	mov		dl, [esi+2]
	add		eax, edx
	add		esi, 3
	dec		ebp
	jne		asm_scene_lumtile24_col

	pop		esi
	add		esi,[esp+16+16]
	dec		edi
	jne		asm_scene_lumtile24_row
	
	add		ebx, 20h
	add		eax, 20h
	shl		eax, 16-6
	and		ebx, 00003fc0h
	shl		ebx, 8-6
	add		ecx, 20h
	shr		ecx, 6
	and		eax, 00ff0000h
	add		eax, ebx
	add		eax, ecx

	pop		ebp
	pop		edi
	pop		esi
	pop		ebx
	ret


;asm_scene_lumtile16(src, width, height, modulo, btotalptr);

_asm_scene_lumtile16:
	push	ebp
	push	edi
	push	esi
	push	ebx
	sub	esp,8
	mov	esi,[esp+4+24]
	mov	edi,[esp+12+24]
	xor	eax,eax
	xor	ebx,ebx
	mov	[esp], eax
	mov	[esp+4], eax
asm_scene_lumtile16_row:
	mov	ebp,[esp+8+24]
asm_scene_lumtile16_col:
	mov	ecx,[esi + ebp*2 - 4]
	mov	edx,03e07c1fh
	and	edx,ecx
	and	ecx,7c1f03e0h
	shr	ecx,5
	add	eax,edx
	add	ebx,ecx
	sub	ebp,2
	ja	asm_scene_lumtile16_col

	;	3322222222221111111111
	;	10987654321098765432109876543210
	;eax	<--- g ---><--- r ---><---b --->
	;ebx	<--- r ---><---b ---><--- g --->

	mov	ecx,eax
	mov	edx,ebx

	shr	edx,11
	and	ecx,000001ffh		;blue1

	and	edx,000001ffh		;blue2
	mov	ebp,eax

	shl	ebp,6
	add	ecx,edx			;blue

	mov	edx,ebx
	and	ebp,07ff0000h		;red1

	shr	edx,5
	add	ecx,ebp			;ecx = red1 + blue

	and	edx,07ff0000h		;red2

	shl	ebx,8
	add	ecx,edx			;ecx = red + blue

	and	eax,0ffe00000h
	and	ebx,0007ff00h		;green2

	shr	eax,13			;green1
	add	esi,[esp+16+24]

	add	eax,ebx			;green

	mov	ebp,[esp]
	mov	edx,[esp+4]

	add	ebp,ecx		;red/blue
	add	edx,eax		;green

	mov	[esp],ebp
	mov	[esp+4],edx

	xor	eax, eax
	xor	ebx, ebx

	sub	edi, 1
	jne	asm_scene_lumtile16_row

	mov	eax,[esp]
	mov	ebx,[esp+4]

	add	eax,00040004h
	add	ebx,00000400h
	shr	eax,3
	and	ebx,0007f800h
	shr	ebx,3
	and	eax,00ff00ffh
	add	eax,ebx

	add	esp,8
	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

	end
