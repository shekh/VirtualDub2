	.686
	.mmx
	.xmm
	.model	flat

	segment	.rdata, align=16

U_coeff_g	dq	0f37df37df37df37dh	; (-0.391 * 32 / 256) * 65536
U_coeff_b	dq	04093409340934093h	; ( 2.018 * 32 / 256) * 65536
V_coeff_r	dq	03312331233123312h	; ( 1.596 * 32 / 256) * 65536
V_coeff_g	dq	0e5fce5fce5fce5fch	; (-0.813 * 32 / 256) * 65536
Y_coeff		dq	0253f253f253f253fh	; ( 1.164 * 32 / 256) * 65536
Y_bias		dq	0fdbcfdbcfdbcfdbch	; (0.5 - 1.164 * 16) * 32
xFF00w		dq	0ff00ff00ff00ff00h
x8000w		dq	08000800080008000h

	segment	.text

	extern	_MMX_enabled:byte
	extern	_ISSE_enabled:byte

	global	_asm_YUVtoRGB32hq_row_ISSE	

;	asm_YUVtoRGB32hq_row(
;		Pixel *ARGB1_pointer,
;		Pixel *ARGB2_pointer,
;		YUVPixel *Y1_pointer,
;		YUVPixel *Y2_pointer,
;		YUVPixel *U_pointer,
;		YUVPixel *V_pointer,
;		long width,
;		long uv_up,
;		long uv_down,
;		);

ARGB1_pointer	equ	[esp+ 4+16]
ARGB2_pointer	equ	[esp+ 8+16]
Y1_pointer	equ	[esp+12+16]
Y2_pointer	equ	[esp+16+16]
U_pointer	equ	[esp+20+16]
V_pointer	equ	[esp+24+16]
count		equ	[esp+28+16]
UV_up		equ	[esp+32+16]
UV_down		equ	[esp+36+16]

_asm_YUVtoRGB32hq_row_ISSE:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	eax,count
	shr	eax,2
	mov	count,eax

	mov	eax,ARGB1_pointer
	mov	ebx,ARGB2_pointer
	mov	ecx,Y1_pointer
	mov	edx,Y2_pointer
	mov	esi,U_pointer
	mov	edi,V_pointer

xloop:
	mov		ebp,UV_up

	prefetchnta	[ecx+7]
	prefetchnta	[edx+7]

	movd		mm0,dword [esi]	;U4U3U2U1
	movd		mm6,dword [esi+ebp]
	pavgb		mm6,mm0
	pavgb		mm0,mm6

	movd		mm1,dword [edi]	;V4V3V2V1
	movd		mm6,dword [edi+ebp]
	pavgb		mm6,mm1
	pavgb		mm1,mm6

	movd		mm2,dword [esi-1]	;U3U2U1U0
	movd		mm6,dword [esi+ebp-1]
	pavgb		mm6,mm2
	pavgb		mm2,mm6

	movd		mm3,dword [edi-1]	;V3V2V1V0
	movd		mm6,dword [edi+ebp-1]
	pavgb		mm6,mm3
	pavgb		mm3,mm6

	movd		mm4,dword [esi+1]	;U5U4U3U2
	movd		mm6,dword [esi+ebp+1]
	pavgb		mm6,mm4
	pavgb		mm4,mm6

	movd		mm5,dword [edi+1]	;V5V4V3V2
	movd		mm6,dword [edi+ebp+1]
	pavgb		mm6,mm5
	pavgb		mm5,mm6

	pavgb		mm2,mm0
	pavgb		mm3,mm1
	pavgb		mm4,mm0
	pavgb		mm5,mm1
	pavgb		mm2,mm0
	pavgb		mm3,mm1
	pavgb		mm4,mm0
	pavgb		mm5,mm1

	punpcklbw	mm2,mm2		;(3U4+U3)/4 | (3U3+U2)/4 | (3U2+U1)/4 | (3U1+U0)/4
	punpcklbw	mm3,mm3		;(3V4+V3)/4 | (3V3+V2)/4 | (3V2+V1)/4 | (3V1+V0)/4
	punpcklbw	mm4,mm4		;(3U4+U5)/4 | (3U3+U4)/4 | (3U2+U3)/4 | (3U1+U2)/4
	punpcklbw	mm5,mm5		;(3V4+V5)/4 | (3V3+V4)/4 | (3V2+V3)/4 | (3V1+V2)/4

	psllw		mm2,8
	psllw		mm3,8
	psllw		mm4,8
	psllw		mm5,8

	psubw		mm2,x8000w
	psubw		mm3,x8000w
	psubw		mm4,x8000w
	psubw		mm5,x8000w

	movq		mm1,U_coeff_g
	movq		mm6,V_coeff_g
	pmulhw		mm1,mm2
	pmulhw		mm6,mm3
	pmulhw		mm2,U_coeff_b	;mm2 = B6 | B4 | B2 | B0
	pmulhw		mm3,V_coeff_r	;mm3 = R6 | R4 | R2 | R0
	movq		mm0,U_coeff_g
	paddw		mm1,mm6		;mm1 = G6 | G4 | G2 | G0
	movq		mm6,V_coeff_g
	pmulhw		mm0,mm4
	pmulhw		mm6,mm5
	pmulhw		mm4,U_coeff_b	;mm4 = B7 | B5 | B3 | B1
	pmulhw		mm5,V_coeff_r	;mm5 = R7 | R5 | R3 | R1
	paddw		mm0,mm6		;mm0 = G7 | G5 | G3 | G1

	movq		mm6,[ecx]	;mm6 = Y7Y6Y5Y4Y3Y2Y1Y0
	movq		mm7,mm6		;mm7 = Y7Y6Y5Y4Y3Y2Y1Y0
	psllw		mm6,8		;mm6 = Y6 | Y4 | Y2 | Y0
	pand		mm7,xFF00w	;mm7 = Y7 | Y5 | Y3 | Y1
	pmulhuw		mm6,Y_coeff
	pmulhuw		mm7,Y_coeff

	paddw		mm6,Y_bias
	paddw		mm7,Y_bias
	paddw		mm4,mm7
	paddw		mm2,mm6
	psraw		mm4,5		;mm4 = b7 | b5 | b3 | b1
	paddw		mm5,mm7
	psraw		mm2,5		;mm2 = b6 | b4 | b2 | b0
	paddw		mm3,mm6
	psraw		mm5,5		;mm5 = r7 | r5 | r3 | r1
	paddw		mm0,mm7
	psraw		mm3,5		;mm3 = b7 | b5 | b3 | b1
	paddw		mm1,mm6
	psraw		mm0,5		;mm0 = g7 | g5 | g3 | g1
	psraw		mm1,5		;mm1 = g6 | g4 | g2 | g0

	packuswb	mm4,mm4		;mm4 = b7b5b3b1
	packuswb	mm2,mm2		;mm2 = b6b4b2b0
	packuswb	mm5,mm5		;mm5 = r7r5r3r1
	packuswb	mm3,mm3		;mm3 = r6r4r2r0
	packuswb	mm0,mm0		;mm0 = g7g5g3g1
	packuswb	mm1,mm1		;mm1 = g6g4g2g0

	punpcklbw	mm2,mm4		;mm2 = b7b6b5b4b3b2b1b0
	punpcklbw	mm3,mm5		;mm3 = r7r6r5r4r3r2r1r0
	punpcklbw	mm1,mm0		;mm1 = g7g6g5g4g3g2g1g0

	movq		mm0,mm2
	punpcklbw	mm2,mm3		;mm2 = r3b3r2b2r1b1r0b0
	punpckhbw	mm0,mm3		;mm0 = r7b7r6b6r5b5r4b4
	movq		mm4,mm1
	punpcklbw	mm1,mm1		;mm1 = g3g3g2g2g1g1g0g0
	punpckhbw	mm4,mm4		;mm4 = g7g7g6g6g5g5g4g4

	movq		mm3,mm2
	punpcklbw	mm2,mm1		;mm2 = g1r1g1b1g0r0g0b0
	punpckhbw	mm3,mm1		;mm3 = g3r3g3b3g2r2g2b2
	movq		mm6,mm0
	punpcklbw	mm0,mm4		;mm0 = g5r5g5b5g4r4g4b4
	punpckhbw	mm6,mm4		;mm6 = g7r7g7b7g6r6g6b6

	movntq		[eax],mm2
	movntq		[eax+8],mm3
	movntq		[eax+16],mm0
	movntq		[eax+24],mm6

	;-------------- bottom --------------

	mov		ebp,UV_down

	movd		mm0,dword [esi]	;U4U3U2U1
	movd		mm6,dword [esi+ebp]
	pavgb		mm6,mm0
	pavgb		mm0,mm6

	movd		mm1,dword [edi]	;V4V3V2V1
	movd		mm6,dword [edi+ebp]
	pavgb		mm6,mm1
	pavgb		mm1,mm6

	movd		mm2,dword [esi-1]	;U3U2U1U0
	movd		mm6,dword [esi+ebp-1]
	pavgb		mm6,mm2
	pavgb		mm2,mm6

	movd		mm3,dword [edi-1]	;V3V2V1V0
	movd		mm6,dword [edi+ebp-1]
	pavgb		mm6,mm3
	pavgb		mm3,mm6

	movd		mm4,dword [esi+1]	;U5U4U3U2
	movd		mm6,dword [esi+ebp+1]
	pavgb		mm6,mm4
	pavgb		mm4,mm6

	movd		mm5,dword [edi+1]	;V5V4V3V2
	movd		mm6,dword [edi+ebp+1]
	pavgb		mm6,mm5
	pavgb		mm5,mm6

	pavgb		mm2,mm0
	pavgb		mm3,mm1
	pavgb		mm4,mm0
	pavgb		mm5,mm1
	pavgb		mm2,mm0
	pavgb		mm3,mm1
	pavgb		mm4,mm0
	pavgb		mm5,mm1

	punpcklbw	mm2,mm2		;(3U4+U3)/4 | (3U3+U2)/4 | (3U2+U1)/4 | (3U1+U0)/4
	punpcklbw	mm3,mm3		;(3V4+V3)/4 | (3V3+V2)/4 | (3V2+V1)/4 | (3V1+V0)/4
	punpcklbw	mm4,mm4		;(3U4+U5)/4 | (3U3+U4)/4 | (3U2+U3)/4 | (3U1+U2)/4
	punpcklbw	mm5,mm5		;(3V4+V5)/4 | (3V3+V4)/4 | (3V2+V3)/4 | (3V1+V2)/4

	psllw		mm2,8
	psllw		mm3,8
	psllw		mm4,8
	psllw		mm5,8

	psubw		mm2,x8000w
	psubw		mm3,x8000w
	psubw		mm4,x8000w
	psubw		mm5,x8000w

	movq		mm1,U_coeff_g
	movq		mm6,V_coeff_g
	pmulhw		mm1,mm2
	pmulhw		mm6,mm3
	pmulhw		mm2,U_coeff_b	;mm2 = B6 | B4 | B2 | B0
	pmulhw		mm3,V_coeff_r	;mm3 = R6 | R4 | R2 | R0
	movq		mm0,U_coeff_g
	paddw		mm1,mm6		;mm1 = G6 | G4 | G2 | G0
	movq		mm6,V_coeff_g
	pmulhw		mm0,mm4
	pmulhw		mm6,mm5
	pmulhw		mm4,U_coeff_b	;mm4 = B7 | B5 | B3 | B1
	pmulhw		mm5,V_coeff_r	;mm5 = R7 | R5 | R3 | R1
	paddw		mm0,mm6		;mm0 = G7 | G5 | G3 | G1

	movq		mm6,[edx]	;mm6 = Y7Y6Y5Y4Y3Y2Y1Y0
	movq		mm7,mm6		;mm7 = Y7Y6Y5Y4Y3Y2Y1Y0
	psllw		mm6,8		;mm6 = Y6 | Y4 | Y2 | Y0
	pand		mm7,xFF00w	;mm7 = Y7 | Y5 | Y3 | Y1
	pmulhuw		mm6,Y_coeff
	pmulhuw		mm7,Y_coeff

	paddw		mm6,Y_bias
	paddw		mm7,Y_bias
	paddw		mm4,mm7
	paddw		mm2,mm6
	psraw		mm4,5		;mm4 = b7 | b5 | b3 | b1
	paddw		mm5,mm7
	psraw		mm2,5		;mm2 = b6 | b4 | b2 | b0
	paddw		mm3,mm6
	psraw		mm5,5		;mm5 = r7 | r5 | r3 | r1
	paddw		mm0,mm7
	psraw		mm3,5		;mm3 = b7 | b5 | b3 | b1
	paddw		mm1,mm6
	psraw		mm0,5		;mm0 = g7 | g5 | g3 | g1
	psraw		mm1,5		;mm1 = g6 | g4 | g2 | g0

	packuswb	mm4,mm4		;mm4 = b7b5b3b1
	packuswb	mm2,mm2		;mm2 = b6b4b2b0
	packuswb	mm5,mm5		;mm5 = r7r5r3r1
	packuswb	mm3,mm3		;mm3 = r6r4r2r0
	packuswb	mm0,mm0		;mm0 = g7g5g3g1
	packuswb	mm1,mm1		;mm1 = g6g4g2g0

	punpcklbw	mm2,mm4		;mm2 = b7b6b5b4b3b2b1b0
	punpcklbw	mm3,mm5		;mm3 = r7r6r5r4r3r2r1r0
	punpcklbw	mm1,mm0		;mm1 = g7g6g5g4g3g2g1g0

	movq		mm0,mm2
	punpcklbw	mm2,mm3		;mm2 = r3b3r2b2r1b1r0b0
	punpckhbw	mm0,mm3		;mm0 = r7b7r6b6r5b5r4b4
	movq		mm4,mm1
	punpcklbw	mm1,mm1		;mm1 = g3g3g2g2g1g1g0g0
	punpckhbw	mm4,mm4		;mm4 = g7g7g6g6g5g5g4g4

	movq		mm3,mm2
	punpcklbw	mm2,mm1		;mm2 = g1r1g1b1g0r0g0b0
	punpckhbw	mm3,mm1		;mm3 = g3r3g3b3g2r2g2b2
	movq		mm6,mm0
	punpcklbw	mm0,mm4		;mm0 = g5r5g5b5g4r4g4b4
	punpckhbw	mm6,mm4		;mm6 = g7r7g7b7g6r6g6b6

	movntq		[ebx],mm2
	movntq		[ebx+8],mm3
	movntq		[ebx+16],mm0
	movntq		[ebx+24],mm6

	add		eax,32
	add		ebx,32
	add		ecx,8
	add		edx,8
	add		esi,4
	add		edi,4
	dec		dword ptr count
	jne		xloop

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

	end
