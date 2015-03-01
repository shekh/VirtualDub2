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

		global _g_VDMPEGPredict_mmx

_g_VDMPEGPredict_mmx	dd	predict_Y_normal_MMX
			dd	predict_Y_halfpelX_MMX
			dd	predict_Y_halfpelY_MMX
			dd	predict_Y_quadpel_MMX
			dd	predict_C_normal_MMX
			dd	predict_C_halfpelX_MMX
			dd	predict_C_halfpelY_MMX
			dd	predict_C_quadpel_MMX
			dd	predict_add_Y_normal_MMX
			dd	predict_add_Y_halfpelX_MMX
			dd	predict_add_Y_halfpelY_MMX
			dd	predict_add_Y_quadpel_MMX
			dd	predict_add_C_normal_MMX
			dd	predict_add_C_halfpelX_MMX
			dd	predict_add_C_halfpelY_MMX
			dd	predict_add_C_quadpel_MMX

	align 16

MMX_01b		dq	00101010101010101h
MMX_02b		dq	00202020202020202h
MMX_fcb		dq	0fcfcfcfcfcfcfcfch
MMX_feb		dq	0fefefefefefefefeh
MMX_02w		dq	00002000200020002h
MMX_04w		dq	00004000400040004h

predict_Y_halfpelX_table	dq	 0,64
				dq	 8,56
				dq	16,48
				dq	24,40
				dq	32,32
				dq	40,24
				dq	48,16
				dq	56, 8
				dq	64, 0

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

predict_Y_quadpel_MMX:
	PREDICT_START
	movq	mm6,[MMX_02w]

	pxor	mm7,mm7
	mov	edi,16
predict_Y_quadpel_MMX.loop:
	mov	eax,2
predict_Y_quadpel_MMX.loop_2:
	movq	mm0,[ecx]

	movd	mm1,dword [ecx+8]
	movq	mm4,mm0

	movq	mm2,[ecx+esi]
	psrlq	mm4,8

	movd	mm3,dword [ecx+esi+8]
	psllq	mm1,56

	por	mm1,mm4
	movq	mm5,mm2

	psrlq	mm5,8
	movq	mm4,mm0

	psllq	mm3,56
	punpcklbw mm0,mm7

	por	mm3,mm5
	punpckhbw mm4,mm7

	movq	mm5,mm1
	punpcklbw mm1,mm7

	punpckhbw mm5,mm7
	paddw	mm0,mm1		;mm0: low total
	movq	mm1,mm2

	paddw	mm4,mm5		;mm4: high total
	movq	mm5,mm3

	punpcklbw mm2,mm7
	add	edx,8

	punpcklbw mm3,mm7
	paddw	mm0,mm2

	punpckhbw mm1,mm7
	paddw	mm0,mm3

	punpckhbw mm5,mm7
	paddw	mm4,mm1
	
	paddw	mm4,mm5
	paddw	mm0,mm6

	paddw	mm4,mm6
	psrlw	mm0,2

	psrlw	mm4,2
	add	ecx,8

	packuswb mm0,mm4

	movq	[edx-8],mm0

	dec	eax
	jne	predict_Y_quadpel_MMX.loop_2

	lea	ecx,[ecx+esi-16]
	lea	edx,[edx+esi-16]

	dec	edi
	jne	predict_Y_quadpel_MMX.loop

	PREDICT_END
	ret


;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align 16
predict_Y_halfpelY_MMX:
	PREDICT_START
	movq	mm6,[MMX_01b]
	movq	mm7,[MMX_feb]
	mov	edi,8
	mov	ebx,7
	and	ebx,ecx
	jz	predict_Y_halfpelY_MMX.loop_aligned

	shl	ebx,3
	mov	edi,64
	sub	edi,ebx
	and	ecx,byte -8
	movd	mm7,ebx
	movd	mm6,edi
	mov	edi,16

predict_Y_halfpelY_MMX.loop_unaligned:
	movq	mm0,[ecx]

	movq	mm1,[ecx+8]
	psrlq	mm0,mm7

	movq	mm2,mm1
	psllq	mm1,mm6

	movq	mm3,[ecx+16]
	psrlq	mm2,mm7

	psllq	mm3,mm6
	por	mm0,mm1			;[1]

	movq	mm4,[ecx+esi]
	por	mm2,mm3			;[2]

	movq	mm1,[ecx+esi+8]
	psrlq	mm4,mm7

	movq	mm5,mm1
	psllq	mm1,mm6

	movq	mm3,[ecx+esi+16]
	psrlq	mm5,mm7

	psllq	mm3,mm6
	por	mm4,mm1			;[1]

	por	mm5,mm3			;[2]
	movq	mm1,mm0			;[1]

	pxor	mm1,mm4			;[1]
	por	mm0,mm4			;[1]

	pand	mm1,[MMX_feb]		;[1]
	movq	mm3,mm2			;[2]

	psrlq	mm1,1			;[1]
	pxor	mm3,mm5			;[2]

	pand	mm3,[MMX_feb]		;[2]
	por	mm2,mm5			;[2]

	psubb	mm0,mm1			;[1]
	psrlq	mm3,1			;[2]

	add	ecx,esi

	movq	[edx],mm0		;[1]
	psubb	mm2,mm3			;[2]

	dec	edi

	movq	[edx+8],mm2		;[2]
	lea	edx,[edx+esi]
	jne	predict_Y_halfpelY_MMX.loop_unaligned
	PREDICT_END
	ret

predict_Y_halfpelY_MMX.loop_aligned:
	movq	mm0,[ecx]		;[1]
	movq	mm1,[ecx+esi]		;[1]
	movq	mm3,mm0			;[1]
	movq	mm2,[ecx+esi*2]		;[1]
	movq	mm4,mm1			;[1]
	pxor	mm3,mm1			;[1]
	por	mm0,mm1			;[1]
	pand	mm3,mm7			;[1]
	movq	mm4,mm1			;[1]
	psrlq	mm3,1			;[1]
	pxor	mm4,mm2			;[1]
	psubb	mm0,mm3			;[1]
	pand	mm4,mm7			;[1]
	movq	mm3,[ecx+8]		;[2]
	psrlq	mm4,1			;[1]
	movq	[edx],mm0		;[1]
	por	mm1,mm2			;[1]
	movq	mm0,[ecx+esi+8]		;[2]
	psubb	mm1,mm4			;[1]
	movq	[edx+esi],mm1		;[1]
	movq	mm1,mm3			;[2]
	movq	mm2,[ecx+esi*2+8]	;[2]
	movq	mm4,mm0			;[2]
	pxor	mm1,mm0			;[2]
	por	mm3,mm0			;[2]
	pand	mm1,mm7			;[2]
	movq	mm4,mm0			;[2]
	psrlq	mm1,1			;[2]
	pxor	mm4,mm2			;[2]
	psubb	mm3,mm1			;[2]
	pand	mm4,mm7			;[2]
	psrlq	mm4,1			;[2]
	lea	ecx,[ecx+esi*2]
	por	mm0,mm2			;[2]
	dec	edi
	movq	[edx+8],mm3		;[2]
	psubb	mm0,mm4			;[2]
	movq	[edx+esi+8],mm0		;[2]
	lea	edx,[edx+esi*2]
	jne	predict_Y_halfpelY_MMX.loop_aligned

	PREDICT_END
	ret

;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align 16
predict_Y_halfpelX_MMX:
	PREDICT_START
	mov	edi,16
	mov	ebx,ecx
	and	ebx,7
	shl	ebx,4
	and	ecx,byte -8
	sub	edx,esi
	movq	mm7,qword [ebx+predict_Y_halfpelX_table+0]
	movq	mm6,[MMX_feb]

predict_Y_halfpelX_MMX.loop:
	movq	mm0,[ecx]						;left

	add	edx,esi

	movq	mm1,[ecx+8]						;left
	movq	mm2,mm0							;left

	psrlq	mm0,mm7							;left
	movq	mm3,mm1							;left

	psrlq	mm2,qword [ebx+predict_Y_halfpelX_table+16]		;left
	movq	mm4,mm1							;right

	psllq	mm3,qword [ebx+predict_Y_halfpelX_table+24]		;left
	movq	mm5,mm4							;right

	psllq	mm1,qword [ebx+predict_Y_halfpelX_table+8]		;left
	por	mm2,mm3							;left

	movq	mm3,[ecx+16]						;right
	por	mm0,mm1							;left

	psrlq	mm5,qword [ebx+predict_Y_halfpelX_table+16]		;right
	movq	mm1,mm3							;right

	psllq	mm1,qword [ebx+predict_Y_halfpelX_table+24]		;right
	psrlq	mm4,mm7							;right

	psllq	mm3,qword [ebx+predict_Y_halfpelX_table+8]		;right
	por	mm5,mm1							;right

	movq	mm1,mm0							;left
	pxor	mm0,mm2							;left

	por	mm1,mm2							;left
	pand	mm0,mm6							;left

	psrlq	mm0,1							;left
	por	mm4,mm3							;right

	psubb	mm1,mm0							;left
	movq	mm3,mm4							;right

	pxor	mm4,mm5							;right
	por	mm5,mm3							;right

	movq	[edx],mm1						;left
	pand	mm4,mm6							;right

	psrlq	mm4,1							;right
	add	ecx,esi

	psubb	mm5,mm4							;right
	dec	edi

	movq	[edx+8],mm5						;right
	jne	predict_Y_halfpelX_MMX.loop

	PREDICT_END
	ret

;*********************************************************
;*
;*	Luminance - normal
;*
;*********************************************************

	align 16
predict_Y_normal_MMX:
	PREDICT_START
	mov	eax,8
	mov	ebx,7
	and	ebx,ecx
	jz	predict_Y_normal_MMX.loop_aligned

	shl	ebx,3
	mov	edi,64
	sub	edi,ebx
	and	ecx,byte -8
	movd	mm7,ebx
	movd	mm6,edi
	mov	eax,16

predict_Y_normal_MMX.loop_unaligned:
	movq	mm1,[ecx+8]
	movq	mm0,[ecx]
	movq	mm2,mm1
	psrlq	mm0,mm7
	movq	mm3,[ecx+16]
	psllq	mm1,mm6
	psrlq	mm2,mm7
	por	mm0,mm1
	psllq	mm3,mm6
	por	mm2,mm3
	movq	[edx],mm0
	movq	[edx+8],mm2
	add	ecx,esi
	add	edx,esi
	dec	eax
	jne	predict_Y_normal_MMX.loop_unaligned
	PREDICT_END
	ret

	align 16
predict_Y_normal_MMX.loop_aligned:
	movq	mm0,[ecx]
	movq	mm1,[ecx+8]
	movq	mm2,[ecx+esi]
	movq	mm3,[ecx+esi+8]
	movq	[edx],mm0
	movq	[edx+8],mm1
	movq	[edx+esi],mm2
	movq	[edx+esi+8],mm3
	lea	ecx,[ecx+esi*2]
	lea	edx,[edx+esi*2]
	dec	eax
	jne	predict_Y_normal_MMX.loop_aligned

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
predict_add_Y_quadpel_MMX:
	PREDICT_START
	movq	mm6,[MMX_02w]

	pxor	mm7,mm7
	mov	edi,16
add_Y_quadpel_MMX.loop:
	mov	eax,2
add_Y_quadpel_MMX.loop2:
	movq	mm0,[ecx]
	movd	mm1,dword [ecx+8]
	movq	mm4,mm0
	psrlq	mm4,8
	psllq	mm1,56
	por	mm1,mm4

	movq	mm2,[ecx+esi]
	movd	mm3,dword [ecx+esi+8]
	movq	mm5,mm2
	psrlq	mm5,8
	psllq	mm3,56
	por	mm3,mm5

	movq	mm4,mm0
	punpcklbw mm0,mm7

	movq	mm5,mm1
	punpcklbw mm1,mm7

	punpckhbw mm4,mm7
	paddw	mm0,mm1		;mm0: low total

	punpckhbw mm5,mm7
	movq	mm1,mm2

	paddw	mm4,mm5		;mm4: high total
	punpcklbw mm2,mm7

	movq	mm5,mm3
	punpcklbw mm3,mm7

	punpckhbw mm1,mm7
	paddw	mm2,mm3

	punpckhbw mm5,mm7
	paddw	mm0,mm2

	paddw	mm1,mm5
	paddw	mm0,mm6

	paddw	mm4,mm1
	psrlw	mm0,2

	movq	mm3,[edx]
	paddw	mm4,mm6

	psrlw	mm4,2
	movq	mm5,mm3

	pand	mm3,[MMX_feb]
	packuswb	mm0,mm4

	por		mm5,mm0
	psrlq	mm3,1

	pand	mm0,[MMX_feb]
	psrlq	mm0,1
	pand	mm5,[MMX_01b]
	paddb	mm0,mm3
	add		edx,8
	paddb	mm0,mm5
	add		ecx, 8
	dec		eax

	movq	[edx-8],mm0
	jne	add_Y_quadpel_MMX.loop2

	lea	ecx,[ecx+esi-16]
	lea	edx,[edx+esi-16]

	dec	edi
	jne	add_Y_quadpel_MMX.loop

	PREDICT_END
	ret

;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align 16
predict_add_Y_halfpelY_MMX:
	PREDICT_START
	movq	mm6,[MMX_01b]
	movq	mm7,[MMX_feb]
	mov	edi,16
predict_add_Y_halfpelY_MMX.loop:
	movq	mm3,[ecx]
	movq	mm0,mm7

	movq	mm1,[ecx+esi]
	pand	mm0,mm3

	por		mm3,mm1
	psrlq	mm0,1

	pand	mm3,mm6
	pand	mm1,mm7

	psrlq	mm1,1
	paddb	mm0,mm3

	movq	mm4,[edx]
	paddb	mm0,mm1

	movq	mm2,mm0
	pand	mm0,mm7

	psrlq	mm0,1
	por	mm2,mm4

	pand	mm2,mm6
	pand	mm4,mm7

	psrlq	mm4,1
	paddb	mm0,mm2
	
	movq	mm1,[ecx+8]		;[2]
	paddb	mm4,mm0			;[1]
	
	movq	mm0,[ecx+esi+8]		;[2]
	movq	mm3,mm1			;[2]

	movq	[edx],mm4		;[1]
	por		mm3,mm0			;[2]

	pand	mm3,mm6
	pand	mm1,mm7

	pand	mm0,mm7
	psrlq	mm1,1

	psrlq	mm0,1
	paddb	mm1,mm3

	movq	mm4,[edx+8]
	paddb	mm1,mm0

	movq	mm2,mm1
	pand	mm1,mm7

	psrlq	mm1,1
	por	mm2,mm4

	pand	mm2,mm6
	pand	mm4,mm7

	psrlq	mm4,1
	paddb	mm1,mm2
	
	paddb	mm4,mm1
		
	add	ecx,esi
	dec	edi

	movq	[edx+8],mm4

	lea	edx,[edx+esi]
	jne	predict_add_Y_halfpelY_MMX.loop

	PREDICT_END
	ret

;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align 16
predict_add_Y_halfpelX_MMX:
	PREDICT_START
	movq	mm6,[MMX_01b]
	movq	mm7,[MMX_feb]
	mov	edi,16
predict_add_Y_halfpelX_MMX.loop:
	movq	mm0,[ecx]

	movd	mm1,dword [ecx+8]
	movq	mm2,mm0

	psrlq	mm2,8
	movq	mm3,mm0

	psllq	mm1,56
	pand	mm0,mm7

	por		mm1,mm2

	por		mm3,mm1
	pand	mm1,mm7

	pand	mm3,mm6
	psrlq	mm0,1

	psrlq	mm1,1
	paddb	mm0,mm3

	movq	mm4,[edx]
	paddb	mm0,mm1

	movq	mm2,mm0
	pand	mm0,mm7

	psrlq	mm0,1
	por	mm2,mm4

	pand	mm2,mm6
	pand	mm4,mm7

	psrlq	mm4,1
	paddb	mm0,mm2
	
	movq	mm1,[ecx+8]		;[2]
	paddb	mm4,mm0			;[1]

	movd	mm0,dword [ecx+16]		;[2]
	movq	mm2,mm1			;[2]

	movq	[edx],mm4		;[1]
	psrlq	mm2,8			;[2]

	movq	mm3,mm1			;[2]
	psllq	mm0,56

	pand	mm1,mm7
	por		mm0,mm2

	psrlq	mm1,1
	por		mm3,mm0

	pand	mm3,mm6
	pand	mm0,mm7

	psrlq	mm0,1
	paddb	mm1,mm3

	movq	mm4,[edx+8]
	paddb	mm1,mm0

	movq	mm2,mm1
	pand	mm1,mm7

	psrlq	mm1,1
	por	mm2,mm4

	pand	mm2,mm6
	pand	mm4,mm7

	psrlq	mm4,1
	paddb	mm1,mm2
	
	paddb	mm4,mm1
	
	add	ecx,esi
	dec	edi

	movq	[edx+8],mm4

	lea	edx,[edx+esi]
	jne	predict_add_Y_halfpelX_MMX.loop

	PREDICT_END
	ret



;*********************************************************
;*
;*	Luminance - normal
;*
;*********************************************************

	align 16
predict_add_Y_normal_MMX:
	PREDICT_START
	movq	mm6,[MMX_01b]
	movq	mm7,[MMX_feb]
	mov	edi,16
	mov	ebx,7
	and	ebx,ecx
	jz	add_Y_normal_MMX.loop_aligned

;*** unaligned loop

	shl	ebx,3
	mov	ebp,64
	sub	ebp,ebx
	and	ecx,byte -8
	movd	mm5,ebx
	movd	mm4,ebp

add_Y_normal_MMX.loop_unaligned:
	movq	mm1,[ecx+8]
	movq	mm0,[ecx]
	movq	mm2,mm1
	psrlq	mm0,mm5
	movq	mm3,[ecx+16]
	psllq	mm1,mm4
	psrlq	mm2,mm5
	por	mm0,mm1
	psllq	mm3,mm4
	por	mm2,mm3
	movq	mm1,[edx]
	movq	mm6,mm0
	movq	mm3,[edx+8]
	movq	mm7,mm2
	pand	mm0,[MMX_feb]
	por	mm6,mm1
	pand	mm1,[MMX_feb]
	por	mm7,mm3
	psrlq	mm0,1
	pand	mm2,[MMX_feb]
	psrlq	mm1,1
	pand	mm3,[MMX_feb]
	psrlq	mm2,1
	pand	mm6,[MMX_01b]
	psrlq	mm3,1
	pand	mm7,[MMX_01b]
	paddb	mm0,mm1

	paddb	mm2,mm3
	paddb	mm0,mm6

	paddb	mm2,mm7

	movq	[edx],mm0
	add	ecx,esi

	movq	[edx+8],mm2
	add	edx,esi

	dec	edi
	jne	add_Y_normal_MMX.loop_unaligned

	PREDICT_END
	ret

;*** aligned loop

add_Y_normal_MMX.loop_aligned:
	movq	mm0,[ecx]

	movq	mm1,[edx]
	movq	mm2,mm0

	movq	mm3,[ecx+8]
	por	mm2,mm1

	movq	mm4,[edx+8]
	pand	mm2,mm6

	movq	mm5,mm3
	pand	mm0,mm7

	pand	mm3,mm7
	por	mm5,mm4

	pand	mm1,mm7
	psrlw	mm0,1

	psrlw	mm1,1
	pand	mm4,mm7

	psrlw	mm4,1
	paddb	mm0,mm2

	psrlw	mm3,1
	paddb	mm0,mm1

	pand	mm5,mm6
	paddb	mm3,mm4

	movq	[edx],mm0
	paddb	mm3,mm5

	movq	[edx+8],mm3

	add	ecx,esi
	add	edx,esi

	dec	edi
	jne	add_Y_normal_MMX.loop_aligned

	PREDICT_END
	ret










;**************************************************************************


;*********************************************************
;*
;*	Luminance - quadpel
;*
;*********************************************************

	align 16
predict_C_quadpel_MMX:
	PREDICT_START
	movq	mm6,[MMX_02w]

	pxor	mm7,mm7
	mov	edi,8
predict_C_quadpel_MMX.loop:
	movq	mm0,[ecx]
	movd	mm1,dword [ecx+8]
	movq	mm4,mm0
	psrlq	mm4,8
	psllq	mm1,56
	por	mm1,mm4

	movq	mm2,[ecx+esi]
	movd	mm3,dword [ecx+esi+8]
	movq	mm5,mm2
	psrlq	mm5,8
	psllq	mm3,56
	por	mm3,mm5

	movq	mm4,mm0
	movq	mm5,mm1

	punpcklbw mm0,mm7
	punpcklbw mm1,mm7
	punpckhbw mm4,mm7
	punpckhbw mm5,mm7

	paddw	mm0,mm1		;mm0: low total
	paddw	mm4,mm5		;mm4: high total

	movq	mm1,mm2
	movq	mm5,mm3

	punpcklbw mm2,mm7
	punpcklbw mm3,mm7
	punpckhbw mm1,mm7
	punpckhbw mm5,mm7

	paddw	mm2,mm3
	paddw	mm1,mm5

	paddw	mm0,mm2
	paddw	mm4,mm1

	paddw	mm0,mm6
	paddw	mm4,mm6

	psrlw	mm0,2
	psrlw	mm4,2

	packuswb mm0,mm4

	movq	[edx],mm0

	add	ecx,esi
	add	edx,esi

	dec	edi
	jne	predict_C_quadpel_MMX.loop

	PREDICT_END
	ret


;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align 16
predict_C_halfpelY_MMX:
	PREDICT_START
	movq	mm6,[MMX_01b]
	movq	mm7,[MMX_feb]
	mov	edi,4
predict_C_halfpelY_MMX.loop:
	movq	mm0,[ecx]
	movq	mm1,[ecx+esi]
	movq	mm3,mm0
	movq	mm2,[ecx+esi*2]
	movq	mm4,mm1
	por	mm3,mm1			;mm3: carry for r0+r1
	por	mm4,mm2			;mm4: carry for r1+r2
	pand	mm0,mm7
	pand	mm1,mm7
	psrlq	mm0,1
	pand	mm2,mm7
	psrlq	mm1,1
	pand	mm3,mm6
	psrlq	mm2,1
	pand	mm4,mm6
	paddb	mm0,mm3
	paddb	mm2,mm4
	paddb	mm0,mm1
	paddb	mm1,mm2
	lea	ecx,[ecx+esi*2]
	dec	edi
	movq	[edx],mm0
	movq	[edx+esi],mm1

	lea	edx,[edx+esi*2]
	jne	predict_C_halfpelY_MMX.loop

	PREDICT_END
	ret

;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align 16
predict_C_halfpelX_MMX:
	PREDICT_START
	movq	mm6,[MMX_01b]
	movq	mm7,[MMX_feb]
	mov	edi,4
predict_C_halfpelX_MMX.loop:
	movq	mm0,[ecx]
	movd	mm1,dword [ecx+8]
	movq	mm2,mm0
	movq	mm3,[ecx+esi]
	psrlq	mm2,8
	movd	mm4,dword [ecx+esi+8]
	psllq	mm1,56
	movq	mm5,mm3
	por	mm2,mm1
	psrlq	mm5,8
	movq	mm1,mm0
	psllq	mm4,56
	por	mm1,mm2
	por	mm5,mm4
	pand	mm1,mm6
	movq	mm4,mm3
	pand	mm0,mm7
	por	mm4,mm5
	pand	mm2,mm7
	pand	mm4,mm6
	psrlq	mm0,1
	pand	mm3,mm7
	psrlq	mm2,1
	pand	mm5,mm7
	paddb	mm0,mm2
	psrlq	mm3,1
	paddb	mm0,mm1
	paddb	mm3,mm4
	psrlq	mm5,1
	movq	[edx],mm0
	paddb	mm3,mm5
	movq	[edx+esi],mm3
	lea	ecx,[ecx+esi*2]
	lea	edx,[edx+esi*2]
	dec	edi
	jne	predict_C_halfpelX_MMX.loop

	PREDICT_END
	ret

;*********************************************************
;*
;*	Luminance - normal
;*
;*********************************************************

	align 16
predict_C_normal_MMX:
	PREDICT_START
	movq	mm0,[ecx]
	lea	ebx,[edx+esi*2]

	movq	mm1,[ecx+esi]
	lea	eax,[ecx+esi*2]

	movq	[edx],mm0
	add	ebx,esi

	movq	mm2,[ecx+esi*2]
	add	eax,esi

	movq	[edx+esi],mm1

	movq	mm3,[eax]
	lea	eax,[eax+esi*2]

	movq	[edx+esi*2],mm2

	movq	mm4,[ecx+esi*4]

	movq	[ebx],mm3
	lea	ebx,[ebx+esi*2]

	movq	mm5,[eax]

	movq	[edx+esi*4],mm4

	movq	mm6,[eax+esi]

	movq	[ebx],mm5

	movq	mm7,[eax+esi*2]

	movq	[ebx+esi],mm6

	movq	[ebx+esi*2],mm7

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
predict_add_C_quadpel_MMX:
	PREDICT_START
	movq	mm6,[MMX_02w]

	pxor	mm7,mm7
	mov	edi,8
add_C_quadpel_MMX.loop:
	movq	mm0,[ecx]
	movd	mm1,dword [ecx+8]
	movq	mm4,mm0
	psrlq	mm4,8
	psllq	mm1,56
	por	mm1,mm4

	movq	mm2,[ecx+esi]
	movd	mm3,dword [ecx+esi+8]
	movq	mm5,mm2
	psrlq	mm5,8
	psllq	mm3,56
	por	mm3,mm5

	movq	mm4,mm0
	punpcklbw mm0,mm7

	movq	mm5,mm1
	punpcklbw mm1,mm7

	punpckhbw mm4,mm7
	paddw	mm0,mm1		;mm0: low total

	punpckhbw mm5,mm7
	movq	mm1,mm2

	paddw	mm4,mm5		;mm4: high total
	punpcklbw mm2,mm7

	movq	mm5,mm3
	punpcklbw mm3,mm7

	punpckhbw mm1,mm7
	paddw	mm2,mm3

	punpckhbw mm5,mm7
	paddw	mm0,mm2

	paddw	mm1,mm5
	paddw	mm0,mm6

	paddw	mm4,mm1
	psrlw	mm0,2

	movq	mm3,[edx]
	paddw	mm4,mm6

	psrlw	mm4,2
	movq	mm5,mm3

	pand	mm3,[MMX_feb]
	packuswb	mm0,mm4

	por		mm5,mm0
	psrlq	mm3,1

	pand	mm0,[MMX_feb]
	psrlq	mm0,1
	pand	mm5,[MMX_01b]
	paddb	mm0,mm3
	add		ecx,esi
	paddb	mm0,mm5

	movq	[edx],mm0

	add	edx,esi

	dec	edi
	jne	add_C_quadpel_MMX.loop

	PREDICT_END
	ret

;*********************************************************
;*
;*	Luminance - half-pel Y
;*
;*********************************************************

	align 16
predict_add_C_halfpelY_MMX:
	PREDICT_START
	movq	mm6,[MMX_01b]
	movq	mm7,[MMX_feb]
	mov	edi,8
predict_add_C_halfpelY_MMX.loop:
	movq	mm0,[ecx]

	movq	mm1,[ecx+esi]
	movq	mm3,mm0

	por		mm3,mm1
	pand	mm0,mm7

	pand	mm3,mm6
	psrlq	mm0,1

	pand	mm1,mm7
	paddb	mm0,mm3

	movq	mm4,[edx]
	psrlq	mm1,1

	paddb	mm0,mm1

	movq	mm2,mm0
	pand	mm0,mm7

	psrlq	mm0,1
	por		mm2,mm4

	pand	mm4,mm7
	pand	mm2,mm6

	psrlq	mm4,1
	paddb	mm0,mm2
	
	paddb	mm4,mm0
	
	add	ecx,esi
	dec	edi

	movq	[edx],mm4

	lea	edx,[edx+esi]
	jne	predict_add_C_halfpelY_MMX.loop

	PREDICT_END
	ret

;*********************************************************
;*
;*	Luminance - half-pel X
;*
;*********************************************************

	align 16
predict_add_C_halfpelX_MMX:
	PREDICT_START
	movq	mm6,[MMX_01b]
	movq	mm7,[MMX_feb]
	mov	edi,8
predict_add_C_halfpelX_MMX.loop:
	movq	mm0,[ecx]

	movd	mm1,dword [ecx+8]
	movq	mm2,mm0

	movq	mm3,mm0
	psllq	mm1,56

	psrlq	mm2,8
	pand	mm0,mm7

	por		mm1,mm2
	psrlq	mm0,1

	por		mm3,mm1
	pand	mm1,mm7

	pand	mm3,mm6

	psrlq	mm1,1
	paddb	mm0,mm3

	movq	mm4,[edx]
	paddb	mm0,mm1

	movq	mm2,mm0
	pand	mm0,mm7

	psrlq	mm0,1
	por		mm2,mm4

	pand	mm2,mm6
	pand	mm4,mm7

	psrlq	mm4,1
	paddb	mm0,mm2
	
	paddb	mm4,mm0
	
	add	ecx,esi
	dec	edi

	movq	[edx],mm4

	lea	edx,[edx+esi]
	jne	predict_add_C_halfpelX_MMX.loop

	PREDICT_END
	ret



;*********************************************************
;*
;*	Luminance - normal
;*
;*********************************************************

	align 16
predict_add_C_normal_MMX:
	PREDICT_START
	movq	mm6,[MMX_01b]
	movq	mm7,[MMX_feb]
	mov	edi,4
add_C_normal_MMX.loop:
	movq	mm0,[ecx]

	movq	mm1,[edx]
	movq	mm2,mm0

	movq	mm3,[ecx+esi]
	por	mm2,mm1

	movq	mm4,[edx+esi]
	pand	mm2,mm6

	movq	mm5,mm3
	pand	mm0,mm7

	pand	mm3,mm7
	por	mm5,mm4

	pand	mm1,mm7
	psrlw	mm0,1

	psrlw	mm1,1
	pand	mm4,mm7

	psrlw	mm4,1
	paddb	mm0,mm2

	psrlw	mm3,1
	paddb	mm0,mm1

	pand	mm5,mm6
	paddb	mm3,mm4

	movq	[edx],mm0
	paddb	mm3,mm5

	movq	[edx+esi],mm3

	lea	ecx,[ecx+esi*2]
	lea	edx,[edx+esi*2]
	dec	edi
	jne	add_C_normal_MMX.loop

	PREDICT_END
	ret

	end
