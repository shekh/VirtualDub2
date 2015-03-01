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

	default	rel

	segment	.rdata, align=16

	align 16

SSE2_02b		dq	00202020202020202h,00202020202020202h
SSE2_fcb		dq	0fcfcfcfcfcfcfcfch,0fcfcfcfcfcfcfcfch

		global g_VDMPEGPredict_sse2	

g_VDMPEGPredict_sse2	dq	predict_Y_normal_SSE2
						dq	predict_Y_halfpelX_SSE2
						dq	predict_Y_halfpelY_SSE2
						dq	predict_Y_quadpel_SSE2
						dq	predict_C_normal_SSE2
						dq	predict_C_halfpelX_SSE2
						dq	predict_C_halfpelY_SSE2
						dq	predict_C_quadpel_SSE2
						dq	predict_add_Y_normal_SSE2
						dq	predict_add_Y_halfpelX_SSE2
						dq	predict_add_Y_halfpelY_SSE2
						dq	predict_add_Y_quadpel_SSE2
						dq	predict_add_C_normal_SSE2
						dq	predict_add_C_halfpelX_SSE2
						dq	predict_add_C_halfpelY_SSE2
						dq	predict_add_C_quadpel_SSE2

	segment	.text

%macro PREDICT_START 0
		mov		rax, r8
%endmacro

%macro PREDICT_END 0
%endmacro


;*********************************************************
;*
;*	Luminance - quadpel
;*
;*********************************************************

	align 16

predict_Y_quadpel_SSE2:
	PREDICT_START
	movlhps	xmm14, xmm6
	movlhps	xmm15, xmm7

	movdqa	xmm6, oword [SSE2_02b]
	movdqa	xmm7, oword [SSE2_fcb]
	mov	r9,16
	
	movdqu	xmm0,[rdx]
	movdqu	xmm1,[rdx+1]
	add	rdx,rax
	
	movdqa	xmm3,xmm7
	pandn	xmm3,xmm0
	movdqa	xmm5,xmm7
	pandn	xmm5,xmm1

	paddb	xmm3,xmm5
	paddb	xmm3,xmm6
	
	pand	xmm0,xmm7
	pand	xmm1,xmm7
	pavgb	xmm0,xmm1
	
	;entry:
	; xmm0: last row high sum
	; xmm3: last row low sum + rounder
	
predict_Y_quadpel_SSE2.loop:
	movdqu	xmm1,[rdx]	;xmm1 = p3
	movdqu	xmm2,[rdx+1]	;xmm2 = p4
	add		rdx,rax
	
	movdqa	xmm4,xmm7
	pandn	xmm4,xmm1	;xmm4 = p3 low bits
	movdqa	xmm5,xmm7
	pandn	xmm5,xmm2	;xmm5 = p4 low bits
	pand	xmm1,xmm7	;xmm1 = p3 high bits
	pand	xmm2,xmm7	;xmm2 = p4 high bits
	pavgb	xmm1,xmm2	;xmm1 = p3+p4 high bits
	paddb	xmm4,xmm5
	
	pavgb	xmm0,xmm1	;xmm0 = pout high bits
	paddb	xmm3,xmm4	;xmm3 = (pout low bits << 2) + rounder
	
	psrlq	xmm3,2
	paddb	xmm4,xmm6	;xmm4 = next loop low sum	

	movdqa	xmm5,xmm7
	pandn	xmm5,xmm3
	paddb	xmm0,xmm5
	movdqa	xmm3,xmm4	;xmm3 = next loop low sum
	movdqa	[rcx],xmm0
	movdqa	xmm0,xmm1	;xmm0 = next loop high sum

	add		rcx,rax

	sub		r9,1
	jne		predict_Y_quadpel_SSE2.loop

	movhlps	xmm6, xmm14
	movhlps	xmm7, xmm15
	PREDICT_END
	ret


;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align 16
predict_Y_halfpelY_SSE2:
	PREDICT_START
	mov		r9, 8
	movdqu	xmm0, [rdx]
	add		r8, r8
predict_Y_halfpelY_SSE2.loop:
	movdqu	xmm2, [rdx+rax]

	movdqu	xmm4, [rdx+r8]
	pavgb	xmm0, xmm2

	movdqa	[rcx], xmm0
	pavgb	xmm2, xmm4

	movdqa	[rcx+rax], xmm2
	movdqa	xmm0, xmm4

	add		rdx, r8
	add		rcx, r8
	
	sub		r9, 1
	jne		predict_Y_halfpelY_SSE2.loop

	PREDICT_END
	ret

;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align 16
predict_Y_halfpelX_SSE2:
	PREDICT_START
	mov		r9,8
	add		r8,r8

predict_Y_halfpelX_SSE2.loop:
	movdqu	xmm0,[rdx]
	movdqu	xmm1,[rdx+1]
	movdqu	xmm2,[rdx+rax]
	movdqu	xmm3,[rdx+rax+1]
	pavgb	xmm0,xmm1
	pavgb	xmm2,xmm3
	movdqa	[rcx],xmm0
	movdqa	[rcx+rax],xmm2
	
	add		rcx,r8
	add		rdx,r8

	sub		r9, 1
	jne		predict_Y_halfpelX_SSE2.loop

	PREDICT_END
	ret

;*********************************************************
;*
;*	Luminance - normal
;*
;*********************************************************

	align 16
predict_Y_normal_SSE2:
	PREDICT_START
	mov		r9, 8
	add		r8, r8

predict_Y_normal_SSE2.loop:
	movdqu	xmm0,[rdx]
	movdqu	xmm2,[rdx+rax]
	movdqa	[rcx],xmm0
	movdqa	[rcx+rax],xmm2
	add		rdx,r8
	add		rcx,r8
	sub		r9, 1
	jne		predict_Y_normal_SSE2.loop

	PREDICT_END
	ret



;*********************************************************
;*
;*	Chrominance - quadpel
;*
;*********************************************************

	align 16
predict_C_quadpel_SSE2:
	PREDICT_START

	pxor		xmm5,xmm5
	mov			r9,8
predict_C_quadpel_SSE2.loop:
	movq		xmm0,qword [rdx]
	movd		xmm1,dword [rdx+8]
	movq		xmm4,xmm0
	psrlq		xmm4,8
	psllq		xmm1,56
	por			xmm1,xmm4

	movq		xmm2,qword [rdx+rax]
	movd		xmm3,dword [rdx+rax+8]
	movq		xmm4,xmm2
	psrlq		xmm4,8
	psllq		xmm3,56
	por			xmm3,xmm4

	punpcklbw	xmm0,xmm5
	punpcklbw	xmm1,xmm5

	paddw		xmm0,xmm1

	punpcklbw	xmm2,xmm5
	punpcklbw	xmm3,xmm5

	paddw		xmm2,xmm3

	paddw		xmm0,xmm2

	psrlw		xmm0, 1
	pavgw		xmm0, xmm5

	packuswb	xmm0,xmm0

	movq		qword [rcx],xmm0

	add			rcx,rax
	add			rdx,rax

	sub			r9, 1
	jne			predict_C_quadpel_SSE2.loop

	PREDICT_END
	ret


;*********************************************************
;*
;*	Chrominance - half-pel Y
;*
;*********************************************************

	align 16
predict_C_halfpelY_SSE2:
	PREDICT_START
	movq	xmm0, qword [rdx]
	mov		r9, 4
	add		r8, r8

predict_C_halfpelY_SSE2.loop:
	movq	xmm2, qword [rdx+rax]
	movq	xmm4, qword [rdx+r8]

	pavgb	xmm0, xmm2
	pavgb	xmm2, xmm4

	movq	qword [rcx], xmm0
	movq	xmm0,xmm4

	movq	qword [rcx+rax],xmm2

	add		rdx, r8
	add		rcx, r8
	sub		r9, 1
	jne		predict_C_halfpelY_SSE2.loop
	PREDICT_END
	ret

;*********************************************************
;*
;*	Chrominance - half-pel X
;*
;*********************************************************

	align 16
predict_C_halfpelX_SSE2:
	PREDICT_START
	mov		r9,4
	add		r8,r8
predict_C_halfpelX_SSE2.loop:
	movq	xmm0, qword [rdx]
	movq	xmm1, qword [rdx+1]
	movq	xmm2, qword [rdx+rax]
	movq	xmm3, qword [rdx+rax+1]

	pavgb	xmm0, xmm1
	pavgb	xmm2, xmm3

	movq	qword [rcx], xmm0
	movq	qword [rcx+rax], xmm2

	add		rdx, r8
	add		rcx, r8
	sub		r9, 1

	jne	predict_C_halfpelX_SSE2.loop

	PREDICT_END
	ret

;*********************************************************
;*
;*	Chrominance - normal
;*
;*********************************************************

	align 16
predict_C_normal_SSE2:
	PREDICT_START
	add		r8, r8

	mov		r10, [rdx]
	mov		r11, [rdx+rax]
	mov		[rcx], r10
	mov		[rcx+rax], r11
	add		rcx, r8
	add		rdx, r8
	mov		r10, [rdx]
	mov		r11, [rdx+rax]
	mov		[rcx], r10
	mov		[rcx+rax], r11
	add		rcx, r8
	add		rdx, r8
	mov		r10, [rdx]
	mov		r11, [rdx+rax]
	mov		[rcx], r10
	mov		[rcx+rax], r11
	add		rcx, r8
	add		rdx, r8
	mov		r10, [rdx]
	mov		r11, [rdx+rax]
	mov		[rcx], r10
	mov		[rcx+rax], r11
	add		rcx, r8
	add		rdx, r8

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

	align 16

	
predict_add_Y_quadpel_SSE2:
	PREDICT_START
	movlhps	xmm14, xmm6
	movlhps	xmm15, xmm7

	movdqa	xmm6, oword [SSE2_02b]
	movdqa	xmm7, oword [SSE2_fcb]
	mov		r9,16
	
	movdqu	xmm0,[rdx]
	movdqu	xmm1,[rdx+1]
	add		rdx,rax
	
	movdqa	xmm3,xmm7
	pandn	xmm3,xmm0
	movdqa	xmm5,xmm7
	pandn	xmm5,xmm1

	paddb	xmm3,xmm5
	paddb	xmm3,xmm6
	
	pand	xmm0,xmm7
	pand	xmm1,xmm7
	pavgb	xmm0,xmm1
	
	;entry:
	; xmm0: last row high sum
	; xmm3: last row low sum + rounder
	
add_Y_quadpel_SSE2.loop:
	movdqu	xmm1,[rdx]	;xmm1 = p3
	movdqu	xmm2,[rdx+1]	;xmm2 = p4
	add		rdx,rax
	
	movdqa	xmm4,xmm7
	pandn	xmm4,xmm1	;xmm4 = p3 low bits
	movdqa	xmm5,xmm7
	pandn	xmm5,xmm2	;xmm5 = p4 low bits
	pand	xmm1,xmm7	;xmm1 = p3 high bits
	pand	xmm2,xmm7	;xmm2 = p4 high bits
	pavgb	xmm1,xmm2	;xmm1 = p3+p4 high bits
	paddb	xmm4,xmm5
	
	pavgb	xmm0,xmm1	;xmm0 = pout high bits
	paddb	xmm3,xmm4	;xmm3 = (pout low bits << 2) + rounder
	
	psrlq	xmm3,2
	paddb	xmm4,xmm6	;xmm4 = next loop low sum	

	movdqa	xmm5,xmm7
	pandn	xmm5,xmm3
	paddb	xmm0,xmm5
	pavgb	xmm0,[rcx]
	movdqa	xmm3,xmm4	;xmm3 = next loop low sum
	movdqa	[rcx],xmm0
	movdqa	xmm0,xmm1	;xmm0 = next loop high sum

	add		rcx,rax

	sub		r9, 1
	jne		add_Y_quadpel_SSE2.loop
	
	movhlps	xmm6, xmm14
	movhlps	xmm7, xmm15
	PREDICT_END
	ret
	
;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align 16
predict_add_Y_halfpelY_SSE2:
	PREDICT_START
	movdqu	xmm0,[rdx]
	add		rdx,rax
	add		r8, r8
	mov		r9, 8
predict_add_Y_halfpelY_SSE2.loop:
	movdqu	xmm1,[rdx]
	movdqu	xmm2,[rdx+rax]
	pavgb	xmm0,xmm1
	pavgb	xmm1,xmm2
	pavgb	xmm0,[rcx]
	pavgb	xmm1,[rcx+rax]
		
	add		rdx,r8

	movdqa	[rcx],xmm0
	movdqa	[rcx+rax],xmm1
	add		rcx,r8
	movdqa	xmm0,xmm2
	sub		r9, 1
	jne		predict_add_Y_halfpelY_SSE2.loop

	PREDICT_END
	ret

;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align 16
predict_add_Y_halfpelX_SSE2:
	PREDICT_START
	mov		r9,8
	add		r8,r8
predict_add_Y_halfpelX_SSE2.loop:
	movdqu	xmm0,[rdx]
	movdqu	xmm2,[rdx+rax]
	movdqu	xmm1,[rdx+1]
	movdqu	xmm3,[rdx+rax+1]
	pavgb	xmm0,xmm1
	pavgb	xmm2,xmm3
	pavgb	xmm0,[rcx]
	pavgb	xmm2,[rcx+rax]

	add		rdx,r8
	movdqa	[rcx],xmm0
	movdqa	[rcx+rax],xmm2
	add		rcx,r8
	sub		r9, 1
	jne		predict_add_Y_halfpelX_SSE2.loop

	PREDICT_END
	ret



;*********************************************************
;*
;*	Luminance - normal
;*
;*********************************************************

	align 16
predict_add_Y_normal_SSE2:
	PREDICT_START
	mov		r9,8
	add		r8,r8

add_Y_normal_SSE2.loop:
	movdqu	xmm0,[rdx]
	movdqu	xmm2,[rdx+rax]
	pavgb	xmm0,[rcx]
	pavgb	xmm2,[rcx+rax]
	movdqa	[rcx],xmm0
	movdqa	[rcx+rax],xmm2

	add		rdx, r8
	add		rcx, r8

	sub		r9, 1
	jne		add_Y_normal_SSE2.loop

	PREDICT_END
	ret


;*********************************************************
;*
;*	Chrominance - quadpel
;*
;*********************************************************

	align 16
predict_add_C_quadpel_SSE2:
	PREDICT_START

	pxor	xmm5, xmm5
	mov		r9, 8
add_C_quadpel_SSE2.loop:
	movq		xmm0, qword [rdx]
	movd		xmm1, dword [rdx+8]
	movq		xmm2, xmm0
	psrlq		xmm2, 8
	psllq		xmm1, 56
	por			xmm1, xmm2

	movq		xmm2, qword [rdx+rax]
	movd		xmm3, dword [rdx+rax+8]
	movq		xmm4, xmm2
	psrlq		xmm4, 8
	psllq		xmm3, 56
	por			xmm3, xmm4

	punpcklbw	xmm0, xmm5
	punpcklbw	xmm1, xmm5

	paddw		xmm0, xmm1

	punpcklbw	xmm2, xmm5
	punpcklbw	xmm3, xmm5

	paddw		xmm2, xmm3

	movq		xmm3, qword [rcx]
	paddw		xmm0, xmm2

	psrlw		xmm0, 1
	pavgw		xmm0, xmm5

	packuswb	xmm0, xmm0
	pavgb		xmm0, xmm3

	movq		qword [rcx],xmm0

	add			rdx, rax
	add			rcx, rax

	sub			r9, 1
	jne			add_C_quadpel_SSE2.loop

	PREDICT_END
	ret

;*********************************************************
;*
;*	Chrominance - half-pel Y
;*
;*********************************************************

	align 16
predict_add_C_halfpelY_SSE2:
	PREDICT_START
	mov		r9,8
predict_add_C_halfpelY_SSE2.loop:
	movq	xmm0, qword [rdx]
	movq	xmm1, qword [rdx+rax]
	movq	xmm2, qword [rcx]
	pavgb	xmm0, xmm1
	pavgb	xmm0, xmm2
	movq	qword [rcx], xmm0
	add		rdx, rax
	add		rcx, rax
	sub		r9,1
	jne	predict_add_C_halfpelY_SSE2.loop
	PREDICT_END
	ret

;*********************************************************
;*
;*	Chrominance - half-pel X
;*
;*********************************************************

	align 16
predict_add_C_halfpelX_SSE2:
	PREDICT_START
	mov		r9,8
predict_add_C_halfpelX_SSE2.loop:
	movq	xmm0, qword [rdx]
	movq	xmm1, qword [rdx+1]
	movq	xmm2, qword [rcx]
	pavgb	xmm0, xmm1
	pavgb	xmm0, xmm2
	movq	qword [rcx], xmm0
	add		rdx,rax
	add		rcx,rax
	sub		r9, 1
	jne		predict_add_C_halfpelX_SSE2.loop
	PREDICT_END
	ret



;*********************************************************
;*
;*	Chrominance - normal
;*
;*********************************************************

	align 16
predict_add_C_normal_SSE2:
	PREDICT_START
	add		r8, r8

	%rep	4
	movq	xmm0, qword [rdx]
	movq	xmm1, qword [rcx]
	movq	xmm2, qword [rdx+rax]
	movq	xmm3, qword [rcx+rax]
	pavgb	xmm0, xmm1
	pavgb	xmm2, xmm3
	movq	qword [rcx], xmm0
	movq	qword [rcx+rax], xmm2
	add		rcx, r8
	add		rdx, r8
	%endrep

	PREDICT_END
	ret


	end
