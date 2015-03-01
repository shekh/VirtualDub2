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

	align 16

SSE2_02b		dq	00202020202020202h,00202020202020202h
SSE2_fcb		dq	0fcfcfcfcfcfcfcfch,0fcfcfcfcfcfcfcfch
SSE2_02w		dq	00002000200020002h

		global _g_VDMPEGPredict_sse2

		extern predict_C_normal_ISSE : near
		extern predict_C_halfpelX_ISSE : near
		extern predict_C_halfpelY_ISSE : near
		extern predict_C_quadpel_ISSE : near
		extern predict_add_C_normal_ISSE : near
		extern predict_add_C_halfpelX_ISSE : near
		extern predict_add_C_halfpelY_ISSE : near
		extern predict_add_C_quadpel_ISSE : near

_g_VDMPEGPredict_sse2	dd	predict_Y_normal_SSE2
			dd	predict_Y_halfpelX_SSE2
			dd	predict_Y_halfpelY_SSE2
			dd	predict_Y_quadpel_SSE2
			dd	predict_C_normal_ISSE
			dd	predict_C_halfpelX_ISSE
			dd	predict_C_halfpelY_ISSE
			dd	predict_C_quadpel_ISSE
			dd	predict_add_Y_normal_SSE2
			dd	predict_add_Y_halfpelX_SSE2
			dd	predict_add_Y_halfpelY_SSE2
			dd	predict_add_Y_quadpel_SSE2
			dd	predict_add_C_normal_ISSE
			dd	predict_add_C_halfpelX_ISSE
			dd	predict_add_C_halfpelY_ISSE
			dd	predict_add_C_quadpel_ISSE

	segment	.text

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


;*********************************************************
;*
;*	Luminance - quadpel
;*
;*********************************************************

	align 16

predict_Y_quadpel_SSE2:
	PREDICT_START
	movdqa	xmm6, oword [SSE2_02b]
	movdqa	xmm7, oword [SSE2_fcb]
	mov	edi,16
	
	movdqu	xmm0,[ecx]
	movdqu	xmm1,[ecx+1]
	add	ecx,esi
	
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
	movdqu	xmm1,[ecx]	;xmm1 = p3
	movdqu	xmm2,[ecx+1]	;xmm2 = p4
	add	ecx,esi
	
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
	movdqa	[edx],xmm0
	movdqa	xmm0,xmm1	;xmm0 = next loop high sum

	add	edx,esi

	dec	edi
	jne	predict_Y_quadpel_SSE2.loop

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
	mov	edi,8
	mov	eax,esi
	movdqu	xmm0,[ecx]
	add	eax,eax
predict_Y_halfpelY_SSE2.loop:
	prefetcht0 [ecx+eax]
	movdqu	xmm2,[ecx+esi]

	movdqu	xmm4,[ecx+eax]
	pavgb	xmm0,xmm2

	movdqa	[edx],xmm0
	pavgb	xmm2,xmm4

	movdqa	[edx+esi],xmm2
	movdqa	xmm0,xmm4

	add	ecx,eax
	add	edx,eax
	
	dec	edi
	jne	predict_Y_halfpelY_SSE2.loop

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
	mov	edi,8
	mov	eax,esi
	add	eax,eax

predict_Y_halfpelX_SSE2.loop:
	movdqu	xmm0,[ecx]
	movdqu	xmm1,[ecx+1]
	movdqu	xmm2,[ecx+esi]
	movdqu	xmm3,[ecx+esi+1]
	pavgb	xmm0,xmm1
	pavgb	xmm2,xmm3
	movdqa	[edx],xmm0
	movdqa	[edx+esi],xmm2
	
	add	edx,eax
	add	ecx,eax

	dec	edi
	jne	predict_Y_halfpelX_SSE2.loop

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
	mov	edi,8
	mov	eax,esi
	add	eax,eax

predict_Y_normal_SSE2.loop:
	movdqu	xmm0,[ecx]
	movdqu	xmm2,[ecx+esi]
	movdqa	[edx],xmm0
	movdqa	[edx+esi],xmm2
	add	ecx,eax
	add	edx,eax
	dec	edi
	jne	predict_Y_normal_SSE2.loop

	PREDICT_END
	ret



;*********************************************************
;*
;*	Luminance - quadpel
;*
;*********************************************************

	align 16

	
predict_add_Y_quadpel_SSE2:
	PREDICT_START
	movdqa	xmm6, [SSE2_02b]
	movdqa	xmm7, [SSE2_fcb]
	mov	edi,16
	
	movdqu	xmm0,[ecx]
	movdqu	xmm1,[ecx+1]
	add	ecx,esi
	
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
	movdqu	xmm1,[ecx]	;xmm1 = p3
	movdqu	xmm2,[ecx+1]	;xmm2 = p4
	add	ecx,esi
	
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
	pavgb	xmm0,[edx]
	movdqa	xmm3,xmm4	;xmm3 = next loop low sum
	movdqa	[edx],xmm0
	movdqa	xmm0,xmm1	;xmm0 = next loop high sum

	add	edx,esi

	dec	edi
	jne	add_Y_quadpel_SSE2.loop
	
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
	movdqu	xmm0,[ecx]
	add	ecx,esi
	mov	eax,esi
	add	eax,eax
	mov	edi,8
predict_add_Y_halfpelY_SSE2.loop:
	movdqu	xmm1,[ecx]
	movdqu	xmm2,[ecx+esi]
	pavgb	xmm0,xmm1
	pavgb	xmm1,xmm2
	pavgb	xmm0,[edx]
	pavgb	xmm1,[edx+esi]
		
	add	ecx,eax

	movdqa	[edx],xmm0
	movdqa	[edx+esi],xmm1
	add	edx,eax
	movdqa	xmm0,xmm2
	dec	edi
	jne	predict_add_Y_halfpelY_SSE2.loop

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
	mov	edi,8
	mov	eax,esi
	add	eax,eax
predict_add_Y_halfpelX_SSE2.loop:
	movdqu	xmm0,[ecx]
	movdqu	xmm2,[ecx+esi]
	movdqu	xmm1,[ecx+1]
	movdqu	xmm3,[ecx+esi+1]
	pavgb	xmm0,xmm1
	pavgb	xmm2,xmm3
	pavgb	xmm0,[edx]
	pavgb	xmm2,[edx+esi]

	add	ecx,eax
	movdqa	[edx],xmm0
	movdqa	[edx+esi],xmm2
	add	edx,eax
	dec	edi
	jne	predict_add_Y_halfpelX_SSE2.loop

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
	mov	edi,8
	mov	eax,esi
	add	eax,eax

add_Y_normal_SSE2.loop:
	movdqu	xmm0,[ecx]
	movdqu	xmm2,[ecx+esi]
	pavgb	xmm0,[edx]
	pavgb	xmm2,[edx+esi]
	movdqa	[edx],xmm0
	movdqa	[edx+esi],xmm2

	add	ecx,eax
	add	edx,eax

	dec	edi
	jne	add_Y_normal_SSE2.loop

	PREDICT_END
	ret



	end
