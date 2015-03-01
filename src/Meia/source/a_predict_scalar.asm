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

;****************************************************
;
; This module now uses a new algorithm that was suggested to me in email:
;
;    result = (((x^y) & 0xfefefefe)>>1) + (x&y);
;
; The formula rounds down, but it can be reversed to round according to
; MPEG:
;
;    result = (x|y) - (((x^y) & 0xfefefefe)>>1);
;
;****************************************************

	segment	.rdata, align=16

	align 16

	global	_g_VDMPEGPredict_scalar

_g_VDMPEGPredict_scalar	dd	predict_Y_normal
			dd	predict_Y_halfpelX
			dd	predict_Y_halfpelY
			dd	predict_Y_quadpel
			dd	predict_C_normal
			dd	predict_C_halfpelX
			dd	predict_C_halfpelY
			dd	predict_C_quadpel
			dd	predict_add_Y_normal
			dd	predict_add_Y_halfpelX
			dd	predict_add_Y_halfpelY
			dd	predict_add_Y_quadpel
			dd	predict_add_C_normal
			dd	predict_add_C_halfpelX
			dd	predict_add_C_halfpelY
			dd	predict_add_C_quadpel

%macro PREDICT_START 0
		push	ebp
		push	edi
		push	esi
		push	ebx
		mov	edx,[esp+4+16]
		mov	ecx,[esp+8+16]
		mov	esi,[esp+12+16]	
%endmacro

%macro PREDICT_END 0
		pop	ebx
		pop	esi
		pop	edi
		pop	ebp
%endmacro


	segment	.text

;*********************************************************
;*
;*	Luminance - quadpel
;*
;*********************************************************

	align	16
predict_Y_quadpel:
	PREDICT_START
	push	16
loop_Y1_quadpel:

;	[A][B][C][D][E][F][G][H]
;	[I][J][K][L][M][N][O][P]

%macro quadpel_move 1
	mov	edi,[ecx+%1]
	mov	ebp,0fcfcfcfch

	and	ebp,edi
	and	edi,03030303h

	shr	ebp,2
	mov	eax,[ecx+esi+%1]

	mov	ebx,0fcfcfcfch
	and	ebx,eax

	and	eax,03030303h
	add	edi,eax

	shr	ebx,2
	mov	eax,[ecx+1+%1]

	add	ebp,ebx
	mov	ebx,0fcfcfcfch

	and	ebx,eax
	and	eax,03030303h

	shr	ebx,2
	add	edi,eax

	add	ebp,ebx
	mov	eax,[ecx+esi+1+%1]

	add	edi,02020202h
	mov	ebx,0fcfcfcfch

	and	ebx,eax
	and	eax,03030303h

	shr	ebx,2
	add	edi,eax

	shr	edi,2
	add	ebp,ebx

	and	edi,03030303h
	add	ebp,edi

	mov	[edx+%1],ebp
%endmacro

	quadpel_move	0
	quadpel_move	4
	quadpel_move	8
	quadpel_move	12

	mov	eax,[esp]
	lea	ecx,[ecx+esi]
	dec	eax
	lea	edx,[edx+esi]
	mov	[esp],eax
	jne	loop_Y1_quadpel
	pop	eax
	PREDICT_END
	ret


;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align	16
predict_Y_halfpelY:
	PREDICT_START
	mov	ebp,16
loop_Y1_halfpelV:
	mov	edi,[ecx+0]		;[1]
	mov	ebx,[ecx+esi+0]		;[1]
	mov	eax,ebx			;[1]
	xor	ebx,edi			;[1]
	shr	ebx,1			;[1]
	or	eax,edi			;[1]
	and	ebx,7f7f7f7fh		;[1]
	mov	edi,[ecx+4]		;[2]
	sub	eax,ebx			;[1]
	mov	ebx,[ecx+esi+4]		;[2]
	mov	[edx+0],eax		;[1]
	mov	eax,ebx			;[2]
	xor	ebx,edi			;[2]
	shr	ebx,1			;[2]
	or	eax,edi			;[2]
	and	ebx,7f7f7f7fh		;[2]
	mov	edi,[ecx+8]		;[3]
	sub	eax,ebx			;[2]
	mov	ebx,[ecx+esi+8]		;[3]
	mov	[edx+4],eax		;[2]
	mov	eax,ebx			;[3]
	xor	ebx,edi			;[3]
	shr	ebx,1			;[3]
	or	eax,edi			;[3]
	and	ebx,7f7f7f7fh		;[3]
	mov	edi,[ecx+12]		;[4]
	sub	eax,ebx			;[3]
	mov	ebx,[ecx+esi+12]	;[4]
	mov	[edx+8],eax		;[3]
	mov	eax,ebx			;[4]
	xor	ebx,edi			;[4]
	shr	ebx,1			;[4]
	or	eax,edi			;[4]
	and	ebx,7f7f7f7fh		;[4]
	add	ecx,esi
	sub	eax,ebx			;[4]
	dec	ebp
	mov	[edx+12],eax		;[4]
	lea	edx,[edx+esi]
	jne	loop_Y1_halfpelV

	PREDICT_END
	ret

;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align	16
predict_Y_halfpelX:
	PREDICT_START
	mov	ebp,16
loop_Y1_halfpelX:
	mov	edi,[ecx+0]		;[1]
	mov	ebx,[ecx+1]		;[1]
	mov	eax,ebx			;[1]
	xor	ebx,edi			;[1]
	shr	ebx,1			;[1]
	or	eax,edi			;[1]
	and	ebx,7f7f7f7fh		;[1]
	mov	edi,[ecx+4]		;[2]
	sub	eax,ebx			;[1]
	mov	ebx,[ecx+5]		;[2]
	mov	[edx+0],eax		;[1]
	mov	eax,ebx			;[2]
	xor	ebx,edi			;[2]
	shr	ebx,1			;[2]
	or	eax,edi			;[2]
	and	ebx,7f7f7f7fh		;[2]
	mov	edi,[ecx+8]		;[3]
	sub	eax,ebx			;[2]
	mov	ebx,[ecx+9]		;[3]
	mov	[edx+4],eax		;[2]
	mov	eax,ebx			;[3]
	xor	ebx,edi			;[3]
	shr	ebx,1			;[3]
	or	eax,edi			;[3]
	and	ebx,7f7f7f7fh		;[3]
	mov	edi,[ecx+12]		;[4]
	sub	eax,ebx			;[3]
	mov	ebx,[ecx+13]		;[4]
	mov	[edx+8],eax		;[3]
	mov	eax,ebx			;[4]
	xor	ebx,edi			;[4]
	shr	ebx,1			;[4]
	or	eax,edi			;[4]
	and	ebx,7f7f7f7fh		;[4]
	add	ecx,esi
	sub	eax,ebx			;[4]
	dec	ebp
	mov	[edx+12],eax		;[4]
	lea	edx,[edx+esi]
	jne	loop_Y1_halfpelX

	PREDICT_END
	ret


;*********************************************************
;*
;*	Luminance - normal
;*
;*********************************************************

	align	16
predict_Y_normal:
	PREDICT_START
	mov	edi,8
loop_Y:
	mov	eax,[ecx]
	mov	ebx,[ecx+4]
	mov	[edx],eax
	mov	[edx+4],ebx
	mov	eax,[ecx+8]
	mov	ebx,[ecx+12]
	mov	[edx+8],eax
	mov	[edx+12],ebx
	mov	eax,[ecx+esi]
	mov	ebx,[ecx+esi+4]
	mov	[edx+esi],eax
	mov	[edx+esi+4],ebx
	mov	eax,[ecx+esi+8]
	mov	ebx,[ecx+esi+12]
	mov	[edx+esi+8],eax
	mov	[edx+esi+12],ebx
	lea	ecx,[ecx+esi*2]
	lea	edx,[edx+esi*2]
	dec	edi
	jne	loop_Y

	PREDICT_END
	ret




;**************************************************************************
;*
;*
;*
;*  Addition predictors
;*
;*
;*
;**************************************************************************

;*********************************************************
;*
;*	Luminance - quadpel
;*
;*********************************************************

	align	16
predict_add_Y_quadpel:
	PREDICT_START
	push	16
add_loop_Y1_quadpel:

;	[A][B][C][D][E][F][G][H]
;	[I][J][K][L][M][N][O][P]

%macro quadpel_add 1
	mov	edi,[ecx+%1]
	mov	ebp,0f8f8f8f8h

	and	ebp,edi
	and	edi,07070707h

	shr	ebp,3
	mov	eax,[ecx+esi+%1]

	mov	ebx,0f8f8f8f8h
	and	ebx,eax

	and	eax,07070707h
	add	edi,eax

	shr	ebx,3
	mov	eax,[ecx+1+%1]

	add	ebp,ebx
	mov	ebx,0f8f8f8f8h

	and	ebx,eax
	and	eax,07070707h

	shr	ebx,3
	add	edi,eax

	add	ebp,ebx
	mov	eax,[ecx+esi+1+%1]

	add	edi,04040404h
	mov	ebx,0f8f8f8f8h

	and	ebx,eax
	and	eax,07070707h

	shr	ebx,3
	add	edi,eax

	mov	eax,[edx+%1]
	add	ebp,ebx

	mov	ebx,eax
	and	eax,0fefefefeh

	shr	eax,1
	and	ebx,01010101h

	shl	ebx,2
	add	ebp,eax

	add	edi,ebx
	shr	edi,3
	and	edi,07070707h
	add	ebp,edi

	mov	[edx+%1],ebp
%endmacro

	quadpel_add	0
	quadpel_add	4
	quadpel_add	8
	quadpel_add	12

	mov	eax,[esp]
	lea	ecx,[ecx+esi]
	dec	eax
	lea	edx,[edx+esi]
	mov	[esp],eax
	jne	add_loop_Y1_quadpel
	pop	eax
	PREDICT_END
	ret

;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align	16
predict_add_Y_halfpelY:
	PREDICT_START
	mov	ebp,16
add_loop_Y1_halfpelV:
	mov	edi,[ecx]		;[1]
	mov	ebx,[ecx+esi]		;[1]
	mov	eax,ebx			;[1]
	xor	ebx,edi			;[1]
	shr	ebx,1			;[1]
	or	edi,eax			;[1]
	and	ebx,7f7f7f7fh		;[1]
	mov	eax,[edx]		;[1]
	sub	edi,ebx			;[1]
	mov	ebx,eax			;[1]
	xor	ebx,edi			;[1]
	shr	ebx,1			;[1]
	or	eax,edi			;[1]
	and	ebx,7f7f7f7fh		;[1]
	mov	edi,[ecx+4]		;[2]
	sub	eax,ebx			;[1]
	mov	ebx,[ecx+esi+4]		;[2]
	mov	[edx],eax		;[1]
	mov	eax,ebx			;[2]
	xor	ebx,edi			;[2]
	shr	ebx,1			;[2]
	or	edi,eax			;[2]
	and	ebx,7f7f7f7fh		;[2]
	mov	eax,[edx+4]		;[2]
	sub	edi,ebx			;[2]
	mov	ebx,eax			;[2]
	xor	ebx,edi			;[2]
	shr	ebx,1			;[2]
	or	eax,edi			;[2]
	and	ebx,7f7f7f7fh		;[2]
	mov	edi,[ecx+8]		;[3]
	sub	eax,ebx			;[2]
	mov	ebx,[ecx+esi+8]		;[3]
	mov	[edx+4],eax		;[2]
	mov	eax,ebx			;[3]
	xor	ebx,edi			;[3]
	shr	ebx,1			;[3]
	or	edi,eax			;[3]
	and	ebx,7f7f7f7fh		;[3]
	mov	eax,[edx+8]		;[3]
	sub	edi,ebx			;[3]
	mov	ebx,eax			;[3]
	xor	ebx,edi			;[3]
	shr	ebx,1			;[3]
	or	eax,edi			;[3]
	and	ebx,7f7f7f7fh		;[3]
	mov	edi,[ecx+12]		;[4]
	sub	eax,ebx			;[3]
	mov	ebx,[ecx+esi+12]	;[4]
	mov	[edx+8],eax		;[3]
	mov	eax,ebx			;[4]
	xor	ebx,edi			;[4]
	shr	ebx,1			;[4]
	or	edi,eax			;[4]
	and	ebx,7f7f7f7fh		;[4]
	mov	eax,[edx+12]		;[4]
	sub	edi,ebx			;[4]
	mov	ebx,eax			;[4]
	xor	ebx,edi			;[4]
	shr	ebx,1			;[4]
	or	eax,edi			;[4]
	and	ebx,7f7f7f7fh		;[4]
	add	ecx,esi
	sub	eax,ebx			;[4]
	dec	ebp
	mov	[edx+12],eax		;[4]
	lea	edx,[edx+esi]
	jne	add_loop_Y1_halfpelV
	PREDICT_END
	ret


;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align	16
predict_add_Y_halfpelX:
	PREDICT_START
	mov	ebp,16
add_loop_Y1_halfpelX:
	mov	edi,[ecx]		;[1]
	mov	ebx,[ecx+1]		;[1]
	mov	eax,ebx			;[1]
	xor	ebx,edi			;[1]
	shr	ebx,1			;[1]
	or	edi,eax			;[1]
	and	ebx,7f7f7f7fh		;[1]
	mov	eax,[edx]		;[1]
	sub	edi,ebx			;[1]
	mov	ebx,eax			;[1]
	xor	ebx,edi			;[1]
	shr	ebx,1			;[1]
	or	eax,edi			;[1]
	and	ebx,7f7f7f7fh		;[1]
	mov	edi,[ecx+4]		;[2]
	sub	eax,ebx			;[1]
	mov	ebx,[ecx+5]		;[2]
	mov	[edx],eax		;[1]
	mov	eax,ebx			;[2]
	xor	ebx,edi			;[2]
	shr	ebx,1			;[2]
	or	edi,eax			;[2]
	and	ebx,7f7f7f7fh		;[2]
	mov	eax,[edx+4]		;[2]
	sub	edi,ebx			;[2]
	mov	ebx,eax			;[2]
	xor	ebx,edi			;[2]
	shr	ebx,1			;[2]
	or	eax,edi			;[2]
	and	ebx,7f7f7f7fh		;[2]
	mov	edi,[ecx+8]		;[3]
	sub	eax,ebx			;[2]
	mov	ebx,[ecx+9]		;[3]
	mov	[edx+4],eax		;[2]
	mov	eax,ebx			;[3]
	xor	ebx,edi			;[3]
	shr	ebx,1			;[3]
	or	edi,eax			;[3]
	and	ebx,7f7f7f7fh		;[3]
	mov	eax,[edx+8]		;[3]
	sub	edi,ebx			;[3]
	mov	ebx,eax			;[3]
	xor	ebx,edi			;[3]
	shr	ebx,1			;[3]
	or	eax,edi			;[3]
	and	ebx,7f7f7f7fh		;[3]
	mov	edi,[ecx+12]		;[4]
	sub	eax,ebx			;[3]
	mov	ebx,[ecx+13]		;[4]
	mov	[edx+8],eax		;[3]
	mov	eax,ebx			;[4]
	xor	ebx,edi			;[4]
	shr	ebx,1			;[4]
	or	edi,eax			;[4]
	and	ebx,7f7f7f7fh		;[4]
	mov	eax,[edx+12]		;[4]
	sub	edi,ebx			;[4]
	mov	ebx,eax			;[4]
	xor	ebx,edi			;[4]
	shr	ebx,1			;[4]
	or	eax,edi			;[4]
	and	ebx,7f7f7f7fh		;[4]
	add	ecx,esi
	sub	eax,ebx			;[4]
	dec	ebp
	mov	[edx+12],eax		;[4]
	lea	edx,[edx+esi]
	jne	add_loop_Y1_halfpelX
	PREDICT_END
	ret





;*********************************************************
;*
;*	Luminance - normal
;*
;*	See note at top, or this will be unreadable.
;*
;*********************************************************

	align	16
predict_add_Y_normal:
	PREDICT_START
	mov	ebp, 16
add_loop_Y1_normal:
	mov	edi,[ecx]		;[1]
	mov	ebx,[edx]		;[1]
	mov	eax,ebx			;[1]
	xor	ebx,edi			;[1]
	shr	ebx,1			;[1]
	or	eax,edi			;[1]
	and	ebx,7f7f7f7fh		;[1]
	mov	edi,[ecx+4]		;[2]
	sub	eax,ebx			;[1]
	mov	ebx,[edx+4]		;[2]
	mov	[edx],eax		;[1]
	mov	eax,ebx			;[2]
	xor	ebx,edi			;[2]
	shr	ebx,1			;[2]
	or	eax,edi			;[2]
	and	ebx,7f7f7f7fh		;[2]
	mov	edi,[ecx+8]		;[3]
	sub	eax,ebx			;[2]
	mov	ebx,[edx+8]		;[3]
	mov	[edx+4],eax		;[2]
	mov	eax,ebx			;[3]
	xor	ebx,edi			;[3]
	shr	ebx,1			;[3]
	or	eax,edi			;[3]
	and	ebx,7f7f7f7fh		;[3]
	mov	edi,[ecx+12]		;[4]
	sub	eax,ebx			;[3]
	mov	ebx,[edx+12]		;[4]
	mov	[edx+8],eax		;[3]
	mov	eax,ebx			;[4]
	xor	ebx,edi			;[4]
	shr	ebx,1			;[4]
	or	eax,edi			;[4]
	and	ebx,7f7f7f7fh		;[4]
	sub	eax,ebx			;[4]
	add	ecx,esi
	mov	[edx+12],eax		;[4]
	add	edx,esi
	dec	ebp
	jne	add_loop_Y1_normal

	PREDICT_END
	ret









;**************************************************************************
;*
;*
;*	Chrominance predictors
;*
;*
;**************************************************************************

;*********************************************************
;*
;*	Luminance - quadpel
;*
;*********************************************************

	align	16
predict_C_quadpel:
	PREDICT_START
	push	8
loop_C1_quadpel:

;	[A][B][C][D][E][F][G][H]
;	[I][J][K][L][M][N][O][P]

%macro quadpel_move_y 1
	mov	eax,[ecx+%1]		;EAX = [D][C][B][A] (#1)
	mov	ebx,[ecx+1+%1]		;EBX = [E][D][C][B] (#2)
	mov	edi,eax			;EDI = [D][C][B][A] (#1)
	mov	ebp,ebx			;EBP = [E][D][C][B] (#2)
	shr	edi,8			;EDI = [0][D][C][B] (#1>>8)
	and	eax,00ff00ffh		;EAX = [ C ][ A ] (#1 even)
	shr	ebp,8			;EBP = [0][E][D][C] (#2>>8)
	and	ebx,00ff00ffh		;EBX = [ D ][ B ] (#2 even)
	and	edi,00ff00ffh		;EDI = [ D ][ B ] (#1 odd)
	and	ebp,00ff00ffh		;EBP = [ E ][ C ] (#2 odd)
	add	eax,ebx			;EAX = [C+D][A+B]
	add	edi,ebp			;EDI = [D+E][B+C]

	mov	ebx,[ecx+esi+%1]	;EBX = [L][K][J][I]
	add	eax,00020002h		;EAX = [C+D+4][A+B+4]

	mov	ebp,ebx			;EBP = [L][K][J][I]
	and	ebx,00ff00ffh		;EBX = [ K ][ I ]
	shr	ebp,8			;EBP = [0][L][K][J]
	add	eax,ebx			;EAX = [C+D+K+4][A+B+I+4]
	and	ebp,00ff00ffh		;EBP = [ L ][ J ]
	mov	ebx,[ecx+esi+1+%1]	;EBX = [M][L][K][J]
	add	edi,ebp			;EDI = [D+E+L][B+C+J]
	mov	ebp,ebx			;EBP = [M][L][K][J]
	
	shr	ebp,8			;EBP = [0][M][L][K]
	add	edi,00020002h

	and	ebx,00ff00ffh		;EBX = [ L ][ J ]
	and	ebp,00ff00ffh		;EBP = [ M ][ K ]

	add	edi,ebp			;EDI = [D+E+L+M][B+C+J+K]
	add	eax,ebx			;EAX = [C+D+K+L+4][A+B+I+J+4]
	
	shl	edi,6
	and	eax,03fc03fch

	shr	eax,2
	and	edi,0ff00ff00h

	or	eax,edi
	mov	[edx+%1],eax
%endmacro

	quadpel_move_y	0
	quadpel_move_y	4

	mov	eax,[esp]
	lea	ecx,[ecx+esi]
	dec	eax
	lea	edx,[edx+esi]
	mov	[esp],eax
	jne	loop_C1_quadpel
	pop	eax
	PREDICT_END
	ret




;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align	16
predict_C_halfpelY:
	PREDICT_START
	mov	ebp,8
loop_C1_halfpelV:
	mov	edi,[ecx]		;[1]
	mov	ebx,[ecx+esi]		;[1]
	mov	eax,ebx			;[1]
	xor	ebx,edi			;[1]
	shr	ebx,1			;[1]
	or	eax,edi			;[1]
	and	ebx,7f7f7f7fh		;[1]
	mov	edi,[ecx+4]		;[2]
	sub	eax,ebx			;[1]
	mov	ebx,[ecx+esi+4]		;[2]
	mov	[edx],eax		;[1]
	mov	eax,ebx			;[2]
	xor	ebx,edi			;[2]
	shr	ebx,1			;[2]
	or	eax,edi			;[2]
	and	ebx,7f7f7f7fh		;[2]
	add	ecx,esi
	sub	eax,ebx			;[2]
	dec	ebp
	mov	[edx+4],eax		;[2]
	lea	edx,[edx+esi]
	jne	loop_C1_halfpelV

	PREDICT_END
	ret


;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align	16
predict_C_halfpelX:
	PREDICT_START
	mov	ebp,8
loop_C1_halfpelX:
	mov	edi,[ecx]		;[1]
	mov	ebx,[ecx+1]		;[1]
	mov	eax,ebx			;[1]
	xor	ebx,edi			;[1]
	shr	ebx,1			;[1]
	or	eax,edi			;[1]
	and	ebx,7f7f7f7fh		;[1]
	mov	edi,[ecx+4]		;[2]
	sub	eax,ebx			;[1]
	mov	ebx,[ecx+5]		;[2]
	mov	[edx],eax		;[1]
	mov	eax,ebx			;[2]
	xor	ebx,edi			;[2]
	shr	ebx,1			;[2]
	or	eax,edi			;[2]
	and	ebx,7f7f7f7fh		;[2]
	add	ecx,esi
	sub	eax,ebx			;[2]
	dec	ebp
	mov	[edx+4],eax		;[2]
	lea	edx,[edx+esi]
	jne	loop_C1_halfpelX
	PREDICT_END
	ret


;*********************************************************
;*
;*	Luminance - normal
;*
;*********************************************************

	align	16
predict_C_normal:
	PREDICT_START
	mov	edi,4
loop_C:
	mov	eax,[ecx]
	mov	ebx,[ecx+4]
	mov	[edx],eax
	mov	[edx+4],ebx
	mov	eax,[ecx+esi]
	mov	ebx,[ecx+esi+4]
	mov	[edx+esi],eax
	mov	[edx+esi+4],ebx
	lea	ecx,[ecx+esi*2]
	lea	edx,[edx+esi*2]
	dec	edi
	jne	loop_C
	PREDICT_END
	ret





;**************************************************************************
;*
;*
;*
;*  Addition predictors
;*
;*
;*
;**************************************************************************


;*********************************************************
;*
;*	Luminance - quadpel
;*
;*********************************************************

	align	16
predict_add_C_quadpel:
	PREDICT_START
	push	8
add_loop_C1_quadpel:

;	[A][B][C][D][E][F][G][H]
;	[I][J][K][L][M][N][O][P]

%macro quadpel_add_y	1
	mov	edi,[ecx+%1]
	mov	ebp,0f8f8f8f8h

	and	ebp,edi
	and	edi,07070707h

	shr	ebp,3
	mov	eax,[ecx+esi+%1]

	mov	ebx,0f8f8f8f8h
	and	ebx,eax

	and	eax,07070707h
	add	edi,eax

	shr	ebx,3
	mov	eax,[ecx+1+%1]

	add	ebp,ebx
	mov	ebx,0f8f8f8f8h

	and	ebx,eax
	and	eax,07070707h

	shr	ebx,3
	add	edi,eax

	add	ebp,ebx
	mov	eax,[ecx+esi+1+%1]

	add	edi,04040404h
	mov	ebx,0f8f8f8f8h

	and	ebx,eax
	and	eax,07070707h

	shr	ebx,3
	add	edi,eax

	mov	eax,[edx+%1]
	add	ebp,ebx

	mov	ebx,eax
	and	eax,0fefefefeh

	shr	eax,1
	and	ebx,01010101h

	shl	ebx,2
	add	ebp,eax

	add	edi,ebx
	shr	edi,3
	and	edi,07070707h
	add	ebp,edi

	mov	[edx+%1],ebp
%endmacro

	quadpel_add_y	0
	quadpel_add_y	4

	mov	eax,[esp]
	lea	ecx,[ecx+esi]
	dec	eax
	lea	edx,[edx+esi]
	mov	[esp],eax
	jne	add_loop_C1_quadpel
	pop	eax
	PREDICT_END
	ret


;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align	16
predict_add_C_halfpelY:
	PREDICT_START
	mov	ebp,8
add_loop_C1_halfpelV:
	mov	edi,[ecx]		;[1]
	mov	ebx,[ecx+esi]		;[1]
	mov	eax,ebx			;[1]
	xor	ebx,edi			;[1]
	shr	ebx,1			;[1]
	or	edi,eax			;[1]
	and	ebx,7f7f7f7fh		;[1]
	mov	eax,[edx]		;[1]
	sub	edi,ebx			;[1]
	mov	ebx,eax			;[1]
	xor	ebx,edi			;[1]
	shr	ebx,1			;[1]
	or	eax,edi			;[1]
	and	ebx,7f7f7f7fh		;[1]
	mov	edi,[ecx+4]		;[2]
	sub	eax,ebx			;[1]
	mov	ebx,[ecx+esi+4]		;[2]
	mov	[edx],eax		;[1]
	mov	eax,ebx			;[2]
	xor	ebx,edi			;[2]
	shr	ebx,1			;[2]
	or	edi,eax			;[2]
	and	ebx,7f7f7f7fh		;[2]
	mov	eax,[edx+4]		;[2]
	sub	edi,ebx			;[2]
	mov	ebx,eax			;[2]
	xor	ebx,edi			;[2]
	shr	ebx,1			;[2]
	or	eax,edi			;[2]
	and	ebx,7f7f7f7fh		;[2]
	add	ecx,esi
	sub	eax,ebx			;[2]
	dec	ebp
	mov	[edx+4],eax		;[2]
	lea	edx,[edx+esi]
	jne	add_loop_C1_halfpelV
	PREDICT_END
	ret


;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align	16
predict_add_C_halfpelX:
	PREDICT_START
	mov	ebp,8
add_loop_C1_halfpelX:
	mov	edi,[ecx]		;[1]
	mov	ebx,[ecx+1]		;[1]
	mov	eax,ebx			;[1]
	xor	ebx,edi			;[1]
	shr	ebx,1			;[1]
	or	edi,eax			;[1]
	and	ebx,7f7f7f7fh		;[1]
	mov	eax,[edx]		;[1]
	sub	edi,ebx			;[1]
	mov	ebx,eax			;[1]
	xor	ebx,edi			;[1]
	shr	ebx,1			;[1]
	or	eax,edi			;[1]
	and	ebx,7f7f7f7fh		;[1]
	mov	edi,[ecx+4]		;[2]
	sub	eax,ebx			;[1]
	mov	ebx,[ecx+5]		;[2]
	mov	[edx],eax		;[1]
	mov	eax,ebx			;[2]
	xor	ebx,edi			;[2]
	shr	ebx,1			;[2]
	or	edi,eax			;[2]
	and	ebx,7f7f7f7fh		;[2]
	mov	eax,[edx+4]		;[2]
	sub	edi,ebx			;[2]
	mov	ebx,eax			;[2]
	xor	ebx,edi			;[2]
	shr	ebx,1			;[2]
	or	eax,edi			;[2]
	and	ebx,7f7f7f7fh		;[2]
	add	ecx,esi
	sub	eax,ebx			;[2]
	dec	ebp
	mov	[edx+4],eax		;[2]
	lea	edx,[edx+esi]
	jne	add_loop_C1_halfpelX
	PREDICT_END
	ret




;*********************************************************
;*
;*	Luminance - normal
;*
;*********************************************************

	align	16
predict_add_C_normal:
	PREDICT_START
	mov	ebp,8
add_loop_C1_addX:
	mov	edi,[ecx]		;[1]
	mov	ebx,[edx]		;[1]
	mov	eax,ebx			;[1]
	xor	ebx,edi			;[1]
	shr	ebx,1			;[1]
	or	eax,edi			;[1]
	and	ebx,7f7f7f7fh		;[1]
	mov	edi,[ecx+4]		;[2]
	sub	eax,ebx			;[1]
	mov	ebx,[edx+4]		;[2]
	mov	[edx],eax		;[1]
	mov	eax,ebx			;[2]
	xor	ebx,edi			;[2]
	shr	ebx,1			;[2]
	or	eax,edi			;[2]
	and	ebx,7f7f7f7fh		;[2]
	sub	eax,ebx			;[2]
	add	ecx,esi
	mov	[edx+4],eax		;[2]
	add	edx,esi
	dec	ebp
	jne	add_loop_C1_addX
	PREDICT_END
	ret

	end

