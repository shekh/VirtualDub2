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

	extern	_YUV_clip_table: byte
	extern	_YUV_clip_table16: byte

	global	_YUV_Y_table2	
	global	_YUV_U_table2	
	global	_YUV_V_table2	
	global	_YUV_Y2_table2	
	global	_YUV_U2_table2	
	global	_YUV_V2_table2	

	global	_asm_convert_yuy2_bgr16	
	global	_asm_convert_yuy2_bgr16_MMX	
	global	_asm_convert_yuy2_bgr24	
	global	_asm_convert_yuy2_bgr24_MMX	
	global	_asm_convert_yuy2_bgr32	
	global	_asm_convert_yuy2_bgr32_MMX	
	global	_asm_convert_yuy2_fullscale_bgr16	
	global	_asm_convert_yuy2_fullscale_bgr16_MMX	
	global	_asm_convert_yuy2_fullscale_bgr24	
	global	_asm_convert_yuy2_fullscale_bgr24_MMX	
	global	_asm_convert_yuy2_fullscale_bgr32	
	global	_asm_convert_yuy2_fullscale_bgr32_MMX	

;asm_convert_yuy2_bgr16(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

_asm_convert_yuy2_bgr16:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	edi,[esp+4+16]
	mov	esi,[esp+8+16]

yuy2_bgr16_scalar_y:
	mov	ebp,[esp+12+16]
yuy2_bgr16_scalar_x:
	xor	eax,eax
	xor	ebx,ebx
	xor	ecx,ecx
	xor	edx,edx

	mov	al,[esi+1]
	mov	bl,[esi+3]
	mov	cl,[esi+0]
	mov	dl,[esi+2]

	mov	eax,[_YUV_U_table2 + eax*4]
	mov	ebx,[_YUV_V_table2 + ebx*4]
	mov	ecx,[_YUV_Y_table2 + ecx*4]
	mov	edx,[_YUV_Y_table2 + edx*4]

	add	ecx,eax
	add	edx,eax
	add	ecx,ebx
	add	edx,ebx



	mov	eax,edx
	mov	ebx,edx

	shr	ebx,10
	and	edx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	movzx	edx,byte [_YUV_clip_table16 + edx + 256 - 308]	;cl = blue
	movzx	eax,byte [_YUV_clip_table16 + eax + 256 - 244]	;al = red
	movzx	ebx,byte [_YUV_clip_table16 + ebx + 256 - 204]	;bl = green
	shl	edx,16
	shl	eax,16+10
	shl	ebx,16+5
	add	edx,eax
	add	edx,ebx



	mov	eax,ecx
	mov	ebx,ecx

	shr	ebx,10
	and	ecx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	movzx	eax,byte [_YUV_clip_table16 + eax + 256 - 244]		;al = red
	movzx	ebx,byte [_YUV_clip_table16 + ebx + 256 - 204]		;bl = green
	movzx	ecx,byte [_YUV_clip_table16 + ecx + 256 - 308]		;dl = blue

	shl	eax,10
	add	ecx,edx
	shl	ebx,5
	add	ecx,eax
	add	ecx,ebx
	mov	[edi],ecx

	add	esi,4
	add	edi,4

	dec	ebp
	jne	yuy2_bgr16_scalar_x

	add	edi,[esp+20+16]
	add	esi,[esp+24+16]

	dec	dword [esp + 16 + 16]
	jne	yuy2_bgr16_scalar_y

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

;asm_convert_yuy2_fullscale_bgr16(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

_asm_convert_yuy2_fullscale_bgr16:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	edi,[esp+4+16]
	mov	esi,[esp+8+16]

yuy2_fullscale_bgr16_scalar_y:
	mov	ebp,[esp+12+16]
yuy2_fullscale_bgr16_scalar_x:
	xor	eax,eax
	xor	ebx,ebx
	xor	ecx,ecx
	xor	edx,edx

	mov	al,[esi+1]
	mov	bl,[esi+3]
	mov	cl,[esi+0]
	mov	dl,[esi+2]

	mov	eax,[_YUV_U2_table2 + eax*4]
	mov	ebx,[_YUV_V2_table2 + ebx*4]
	mov	ecx,[_YUV_Y2_table2 + ecx*4]
	mov	edx,[_YUV_Y2_table2 + edx*4]

	add	ecx,eax
	add	edx,eax
	add	ecx,ebx
	add	edx,ebx



	mov	eax,edx
	mov	ebx,edx

	shr	ebx,10
	and	edx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	movzx	edx,byte [_YUV_clip_table16 + edx + 256 - 308]	;cl = blue
	movzx	eax,byte [_YUV_clip_table16 + eax + 256 - 244]	;al = red
	movzx	ebx,byte [_YUV_clip_table16 + ebx + 256 - 204]	;bl = green
	shl	edx,16
	shl	eax,16+10
	shl	ebx,16+5
	add	edx,eax
	add	edx,ebx



	mov	eax,ecx
	mov	ebx,ecx

	shr	ebx,10
	and	ecx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	movzx	eax,byte [_YUV_clip_table16 + eax + 256 - 244]		;al = red
	movzx	ebx,byte [_YUV_clip_table16 + ebx + 256 - 204]		;bl = green
	movzx	ecx,byte [_YUV_clip_table16 + ecx + 256 - 308]		;dl = blue

	shl	eax,10
	add	ecx,edx
	shl	ebx,5
	add	ecx,eax
	add	ecx,ebx
	mov	[edi],ecx

	add	esi,4
	add	edi,4

	dec	ebp
	jne	yuy2_bgr16_scalar_x

	add	edi,[esp+20+16]
	add	esi,[esp+24+16]

	dec	dword [esp + 16 + 16]
	jne	yuy2_bgr16_scalar_y

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

;asm_convert_yuy2_bgr24(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

_asm_convert_yuy2_bgr24:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	edi,[esp+4+16]
	mov	esi,[esp+8+16]

yuy2_bgr24_scalar_y:
	mov	ebp,[esp+12+16]
yuy2_bgr24_scalar_x:
	xor	eax,eax
	xor	ebx,ebx
	xor	ecx,ecx
	xor	edx,edx

	mov	al,[esi+1]
	mov	bl,[esi+3]
	mov	cl,[esi+0]
	mov	dl,[esi+2]

	mov	eax,[_YUV_U_table2 + eax*4]
	mov	ebx,[_YUV_V_table2 + ebx*4]
	mov	ecx,[_YUV_Y_table2 + ecx*4]
	mov	edx,[_YUV_Y_table2 + edx*4]

	add	ecx,eax
	add	edx,eax
	add	ecx,ebx
	add	edx,ebx



	mov	eax,ecx
	mov	ebx,ecx

	shr	ebx,10
	and	ecx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	mov	al,[_YUV_clip_table + eax + 256 - 244]		;al = red
	mov	bl,[_YUV_clip_table + ebx + 256 - 204]		;bl = green
	mov	cl,[_YUV_clip_table + ecx + 256 - 308]		;cl = blue
	mov	[edi + 2],al
	mov	[edi + 1],bl
	mov	[edi + 0],cl



	mov	eax,edx
	mov	ebx,edx

	shr	ebx,10
	and	edx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	mov	al,[_YUV_clip_table + eax + 256 - 244]		;al = red
	mov	bl,[_YUV_clip_table + ebx + 256 - 204]		;bl = green
	mov	dl,[_YUV_clip_table + edx + 256 - 308]		;dl = blue
	mov	[edi + 5],al
	mov	[edi + 4],bl
	mov	[edi + 3],dl

	add	esi,4
	add	edi,6

	dec	ebp
	jne	yuy2_bgr24_scalar_x

	add	edi,[esp+20+16]
	add	esi,[esp+24+16]

	dec	dword [esp + 16 + 16]
	jne	yuy2_bgr24_scalar_y

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

;asm_convert_yuy2_fullscale_bgr24(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

_asm_convert_yuy2_fullscale_bgr24:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	edi,[esp+4+16]
	mov	esi,[esp+8+16]

yuy2_fullscale_bgr24_scalar_y:
	mov	ebp,[esp+12+16]
yuy2_fullscale_bgr24_scalar_x:
	xor	eax,eax
	xor	ebx,ebx
	xor	ecx,ecx
	xor	edx,edx

	mov	al,[esi+1]
	mov	bl,[esi+3]
	mov	cl,[esi+0]
	mov	dl,[esi+2]

	mov	eax,[_YUV_U2_table2 + eax*4]
	mov	ebx,[_YUV_V2_table2 + ebx*4]
	mov	ecx,[_YUV_Y2_table2 + ecx*4]
	mov	edx,[_YUV_Y2_table2 + edx*4]

	add	ecx,eax
	add	edx,eax
	add	ecx,ebx
	add	edx,ebx



	mov	eax,ecx
	mov	ebx,ecx

	shr	ebx,10
	and	ecx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	mov	al,[_YUV_clip_table + eax + 256 - 244]		;al = red
	mov	bl,[_YUV_clip_table + ebx + 256 - 204]		;bl = green
	mov	cl,[_YUV_clip_table + ecx + 256 - 308]		;cl = blue
	mov	[edi + 2],al
	mov	[edi + 1],bl
	mov	[edi + 0],cl



	mov	eax,edx
	mov	ebx,edx

	shr	ebx,10
	and	edx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	mov	al,[_YUV_clip_table + eax + 256 - 244]		;al = red
	mov	bl,[_YUV_clip_table + ebx + 256 - 204]		;bl = green
	mov	dl,[_YUV_clip_table + edx + 256 - 308]		;dl = blue
	mov	[edi + 5],al
	mov	[edi + 4],bl
	mov	[edi + 3],dl

	add	esi,4
	add	edi,6

	dec	ebp
	jne	yuy2_fullscale_bgr24_scalar_x

	add	edi,[esp+20+16]
	add	esi,[esp+24+16]

	dec	dword [esp + 16 + 16]
	jne	yuy2_fullscale_bgr24_scalar_y

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

;asm_convert_yuy2_bgr32(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

_asm_convert_yuy2_bgr32:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	edi,[esp+4+16]
	mov	esi,[esp+8+16]

yuy2_bgr32_scalar_y:
	mov	ebp,[esp+12+16]
yuy2_bgr32_scalar_x:
	xor	eax,eax
	xor	ebx,ebx
	xor	ecx,ecx
	xor	edx,edx

	mov	al,[esi+1]
	mov	bl,[esi+3]
	mov	cl,[esi+0]
	mov	dl,[esi+2]

	mov	eax,[_YUV_U_table2 + eax*4]
	mov	ebx,[_YUV_V_table2 + ebx*4]
	mov	ecx,[_YUV_Y_table2 + ecx*4]
	mov	edx,[_YUV_Y_table2 + edx*4]

	add	ecx,eax
	add	edx,eax
	add	ecx,ebx
	add	edx,ebx



	mov	eax,ecx
	mov	ebx,ecx

	shr	ebx,10
	and	ecx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	mov	al,[_YUV_clip_table + eax + 256 - 244]		;al = red
	mov	bl,[_YUV_clip_table + ebx + 256 - 204]		;bl = green
	mov	cl,[_YUV_clip_table + ecx + 256 - 308]		;cl = blue
	mov	[edi + 2],al
	mov	[edi + 1],bl
	mov	[edi + 0],cl



	mov	eax,edx
	mov	ebx,edx

	shr	ebx,10
	and	edx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	mov	al,[_YUV_clip_table + eax + 256 - 244]		;al = red
	mov	bl,[_YUV_clip_table + ebx + 256 - 204]		;bl = green
	mov	dl,[_YUV_clip_table + edx + 256 - 308]		;dl = blue
	mov	[edi + 6],al
	mov	[edi + 5],bl
	mov	[edi + 4],dl

	add	esi,4
	add	edi,8

	dec	ebp
	jne	yuy2_bgr32_scalar_x

	add	edi,[esp+20+16]
	add	esi,[esp+24+16]

	dec	dword [esp + 16 + 16]
	jne	yuy2_bgr32_scalar_y

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

;asm_convert_yuy2_fullscale_bgr32(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

_asm_convert_yuy2_fullscale_bgr32:
	push	ebp
	push	edi
	push	esi
	push	ebx

	mov	edi,[esp+4+16]
	mov	esi,[esp+8+16]

yuy2_fullscale_bgr32_scalar_y:
	mov	ebp,[esp+12+16]
yuy2_fullscale_bgr32_scalar_x:
	xor	eax,eax
	xor	ebx,ebx
	xor	ecx,ecx
	xor	edx,edx

	mov	al,[esi+1]
	mov	bl,[esi+3]
	mov	cl,[esi+0]
	mov	dl,[esi+2]

	mov	eax,[_YUV_U2_table2 + eax*4]
	mov	ebx,[_YUV_V2_table2 + ebx*4]
	mov	ecx,[_YUV_Y2_table2 + ecx*4]
	mov	edx,[_YUV_Y2_table2 + edx*4]

	add	ecx,eax
	add	edx,eax
	add	ecx,ebx
	add	edx,ebx



	mov	eax,ecx
	mov	ebx,ecx

	shr	ebx,10
	and	ecx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	mov	al,[_YUV_clip_table + eax + 256 - 244]		;al = red
	mov	bl,[_YUV_clip_table + ebx + 256 - 204]		;bl = green
	mov	cl,[_YUV_clip_table + ecx + 256 - 308]		;cl = blue
	mov	[edi + 2],al
	mov	[edi + 1],bl
	mov	[edi + 0],cl



	mov	eax,edx
	mov	ebx,edx

	shr	ebx,10
	and	edx,000003ffh
	shr	eax,20
	and	ebx,000003ffh
	mov	al,[_YUV_clip_table + eax + 256 - 244]		;al = red
	mov	bl,[_YUV_clip_table + ebx + 256 - 204]		;bl = green
	mov	dl,[_YUV_clip_table + edx + 256 - 308]		;dl = blue
	mov	[edi + 6],al
	mov	[edi + 5],bl
	mov	[edi + 4],dl

	add	esi,4
	add	edi,8

	dec	ebp
	jne	yuy2_fullscale_bgr32_scalar_x

	add	edi,[esp+20+16]
	add	esi,[esp+24+16]

	dec	dword [esp + 16 + 16]
	jne	yuy2_fullscale_bgr32_scalar_y

	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret

;**************************************************************************

	segment	.data

Y_mask		dq		0000000000ff00ffh
Y_round		dq		0020002000200020h
Y_bias		dq		0000000000100010h
UV_bias		dq		0000000000800080h
Y_coeff		dq		004a004a004a004ah
U_coeff		dq		00000000ffe70081h
V_coeff		dq		00000066ffcc0000h
mask24		dq		0000ffffffffffffh
G_mask		dq		0000f8000000f800h
RB_mask		dq		00f800f800f800f8h
RB_coef		dq		2000000820000008h
Y_coeff2	dq		0040004000400040h
U_coeff2	dq		00000000ffea0071h
V_coeff2	dq		0000005affd20000h

	segment	.text

;asm_convert_yuy2_bgr16_MMX(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

_asm_convert_yuy2_bgr16_MMX:
	push		ebp
	push		edi
	push		esi
	push		ebx

	mov		edi,[esp+4+16]
	mov		esi,[esp+8+16]
	mov		ebp,[esp+12+16]
	mov		ecx,[esp+16+16]
	mov		eax,[esp+20+16]
	mov		ebx,[esp+24+16]
	movq		mm6,[Y_mask]
	movq		mm7,[Y_round]

yuy2_bgr16_MMX_y:
	mov		edx,ebp
yuy2_bgr16_MMX_x:
	movd		mm0,dword [esi]		;mm0 = [V][Y2][U][Y1]

	movq		mm1,mm0			;mm1 = [V][Y2][U][Y1]
	pand		mm0,mm6			;mm0 = [ Y2  ][ Y1  ]

	psubw		mm0,[Y_bias]
	psrlw		mm1,8			;mm1 = [ V ][ U ]

	psubw		mm1,[UV_bias]
	punpcklwd	mm0,mm0			;mm0 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]

	pmullw		mm0,[Y_coeff]
	punpcklwd	mm1,mm1			;mm1 = [ V ][ V ][ U ][ U ]

	movq		mm3,mm1			;mm3 = [ V ][ V ][ U ][ U ]
	punpckldq	mm1,mm1			;mm1 = [ U ][ U ][ U ][ U ]

	pmullw		mm1,[U_coeff]		;mm1 = [ 0 ][ 0 ][ UG ][ UB ]
	punpckhdq	mm3,mm3			;mm3 = [ V ][ V ][ V ][ V ]

	pmullw		mm3,[V_coeff]		;mm3 = [ 0 ][ VR ][ VG ][ 0 ]
	paddw		mm0,mm7			;add rounding to Y

	movq		mm2,mm0			;mm2 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]
	punpckldq	mm0,mm0			;mm0 = [ Y1 ][ Y1 ][ Y1 ][ Y1 ]

	punpckhdq	mm2,mm2			;mm2 = [ Y2 ][ Y2 ][ Y2 ][ Y2 ]
	paddw		mm0,mm1

	paddw		mm2,mm1
	paddw		mm0,mm3

	paddw		mm2,mm3
	psraw		mm0,6

	movq		mm1,[G_mask]
	psraw		mm2,6

	movq		mm3,[RB_mask]
	packuswb	mm0,mm2

	pand		mm1,mm0
	pand		mm3,mm0

	pmaddwd		mm3,[RB_coef]
	;<-->

	add		edi,4
	add		esi,4

	;<-->
	;<-->

	por		mm3,mm1

	psrlq		mm3,6

	packssdw	mm3,mm3

	movd		dword [edi-4],mm3

	dec		edx
	jne		yuy2_bgr16_MMX_x

	add		edi,eax
	add		esi,ebx

	dec		ecx
	jne		yuy2_bgr16_MMX_y

	pop		ebx
	pop		esi
	pop		edi
	pop		ebp
	emms
	ret

;asm_convert_yuy2_fullscale_bgr16_MMX(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

_asm_convert_yuy2_fullscale_bgr16_MMX:
	push		ebp
	push		edi
	push		esi
	push		ebx

	mov		edi,[esp+4+16]
	mov		esi,[esp+8+16]
	mov		ebp,[esp+12+16]
	mov		ecx,[esp+16+16]
	mov		eax,[esp+20+16]
	mov		ebx,[esp+24+16]
	movq		mm6,[Y_mask]
	movq		mm7,[Y_round]

yuy2_fullscale_bgr16_MMX_y:
	mov		edx,ebp
yuy2_fullscale_bgr16_MMX_x:
	movd		mm0,dword [esi]		;mm0 = [V][Y2][U][Y1]

	movq		mm1,mm0			;mm1 = [V][Y2][U][Y1]
	pand		mm0,mm6			;mm0 = [ Y2  ][ Y1  ]

	psrlw		mm1,8			;mm1 = [ V ][ U ]

	psubw		mm1,[UV_bias]
	punpcklwd	mm0,mm0			;mm0 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]

	pmullw		mm0,[Y_coeff2]
	punpcklwd	mm1,mm1			;mm1 = [ V ][ V ][ U ][ U ]

	movq		mm3,mm1			;mm3 = [ V ][ V ][ U ][ U ]
	punpckldq	mm1,mm1			;mm1 = [ U ][ U ][ U ][ U ]

	pmullw		mm1,[U_coeff2]		;mm1 = [ 0 ][ 0 ][ UG ][ UB ]
	punpckhdq	mm3,mm3			;mm3 = [ V ][ V ][ V ][ V ]

	pmullw		mm3,[V_coeff2]		;mm3 = [ 0 ][ VR ][ VG ][ 0 ]
	paddw		mm0,mm7			;add rounding to Y

	movq		mm2,mm0			;mm2 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]
	punpckldq	mm0,mm0			;mm0 = [ Y1 ][ Y1 ][ Y1 ][ Y1 ]

	punpckhdq	mm2,mm2			;mm2 = [ Y2 ][ Y2 ][ Y2 ][ Y2 ]
	paddw		mm0,mm1

	paddw		mm2,mm1
	paddw		mm0,mm3

	paddw		mm2,mm3
	psraw		mm0,6

	movq		mm1,[G_mask]
	psraw		mm2,6

	movq		mm3,[RB_mask]
	packuswb	mm0,mm2

	pand		mm1,mm0
	pand		mm3,mm0

	pmaddwd		mm3,[RB_coef]
	;<-->

	add		edi,4
	add		esi,4

	;<-->
	;<-->

	por		mm3,mm1

	psrlq		mm3,6

	packssdw	mm3,mm3

	movd		dword [edi-4],mm3

	dec		edx
	jne		yuy2_fullscale_bgr16_MMX_x

	add		edi,eax
	add		esi,ebx

	dec		ecx
	jne		yuy2_fullscale_bgr16_MMX_y

	pop		ebx
	pop		esi
	pop		edi
	pop		ebp
	emms
	ret

;asm_convert_yuy2_bgr24_MMX(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

_asm_convert_yuy2_bgr24_MMX:
	push		ebp
	push		edi
	push		esi
	push		ebx

	mov		edi,[esp+4+16]
	mov		esi,[esp+8+16]
	mov		ebp,[esp+12+16]
	mov		ecx,[esp+16+16]
	mov		eax,[esp+20+16]
	mov		ebx,[esp+24+16]
	movq		mm6,[Y_mask]
	movq		mm7,[Y_round]

yuy2_bgr24_MMX_y:
	mov		edx,ebp

	dec		edx
	jz		yuy2_bgr24_MMX_doodd
yuy2_bgr24_MMX_x:
	movd		mm0,dword [esi]		;mm0 = [V][Y2][U][Y1]

	movq		mm1,mm0			;mm1 = [V][Y2][U][Y1]
	pand		mm0,mm6			;mm0 = [ Y2  ][ Y1  ]

	psubw		mm0,[Y_bias]
	psrlw		mm1,8			;mm1 = [ V ][ U ]

	psubw		mm1,[UV_bias]
	punpcklwd	mm0,mm0			;mm0 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]

	pmullw		mm0,[Y_coeff]
	punpcklwd	mm1,mm1			;mm1 = [ V ][ V ][ U ][ U ]

	movq		mm3,mm1			;mm3 = [ V ][ V ][ U ][ U ]
	punpckldq	mm1,mm1			;mm1 = [ U ][ U ][ U ][ U ]

	pmullw		mm1,[U_coeff]		;mm1 = [ 0 ][ 0 ][ UG ][ UB ]
	punpckhdq	mm3,mm3			;mm3 = [ V ][ V ][ V ][ V ]

	pmullw		mm3,[V_coeff]		;mm3 = [ 0 ][ VR ][ VG ][ 0 ]
	paddw		mm0,mm7			;add rounding to Y

	movq		mm2,mm0			;mm2 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]
	punpckldq	mm0,mm0			;mm0 = [ Y1 ][ Y1 ][ Y1 ][ Y1 ]

	punpckhdq	mm2,mm2			;mm2 = [ Y2 ][ Y2 ][ Y2 ][ Y2 ]
	paddw		mm0,mm1

	paddw		mm2,mm1
	paddw		mm0,mm3

	paddw		mm2,mm3
	psraw		mm0,6

	psraw		mm2,6
	pxor		mm5,mm5

	pand		mm0,[mask24]
	packuswb	mm2,mm5			;mm1 = [ 0][ 0][ 0][ 0][ 0][R1][G1][B1]

	psllq		mm2,24			;mm1 = [ 0][ 0][R1][G1][B1][ 0][ 0][ 0]

	pand		mm2,[mask24]
	packuswb	mm0,mm5			;mm0 = [ 0][ 0][ 0][ 0][ 0][R0][G0][B0]

	por		mm0,mm2			;mm0 = [ 0][ 0][R1][G1][B1][R0][G0][B0]

	;----------------------------------

	movd		mm4,dword [esi+4]		;mm4 = [V][Y2][U][Y1]

	movq		mm1,mm4			;mm1 = [V][Y2][U][Y1]
	pand		mm4,mm6			;mm4 = [ Y2  ][ Y1  ]

	psubw		mm4,[Y_bias]
	psrlw		mm1,8			;mm1 = [ V ][ U ]

	psubw		mm1,[UV_bias]
	punpcklwd	mm4,mm4			;mm4 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]

	pmullw		mm4,[Y_coeff]
	punpcklwd	mm1,mm1			;mm1 = [ V ][ V ][ U ][ U ]

	movq		mm3,mm1			;mm3 = [ V ][ V ][ U ][ U ]
	punpckldq	mm1,mm1			;mm1 = [ U ][ U ][ U ][ U ]

	pmullw		mm1,[U_coeff]		;mm1 = [ 0 ][ 0 ][ UG ][ UB ]
	punpckhdq	mm3,mm3			;mm3 = [ V ][ V ][ V ][ V ]

	pmullw		mm3,[V_coeff]		;mm3 = [ 0 ][ VR ][ VG ][ 0 ]
	paddw		mm4,mm7			;add rounding to Y

	movq		mm2,mm4			;mm2 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]
	punpckldq	mm4,mm4			;mm4 = [ Y1 ][ Y1 ][ Y1 ][ Y1 ]

	punpckhdq	mm2,mm2			;mm2 = [ Y2 ][ Y2 ][ Y2 ][ Y2 ]
	paddw		mm4,mm1

	paddw		mm2,mm1
	paddw		mm4,mm3

	paddw		mm2,mm3
	psraw		mm4,6

	pand		mm4,[mask24]
	psraw		mm2,6

	pand		mm2,[mask24]
	packuswb	mm4,mm5			;mm4 = [ 0][ 0][ 0][ 0][ 0][R2][G2][B2]

	packuswb	mm2,mm5			;mm2 = [ 0][ 0][ 0][ 0][ 0][R3][G3][B3]
	movq		mm1,mm4			;mm1 = [ 0][ 0][ 0][ 0][ 0][R2][G2][B2]

	psllq		mm4,48			;mm4 = [G2][B2][ 0][ 0][ 0][ 0][ 0][ 0]

	por		mm0,mm4			;mm0 = [G2][B2][R1][G1][B1][R0][G0][B0]
	psrlq		mm1,16			;mm1 = [ 0][ 0][ 0][ 0][ 0][ 0][ 0][R2]

	psllq		mm2,8			;mm2 = [ 0][ 0][ 0][ 0][R3][G3][B3][ 0]
	movq		mm3,mm0

	por		mm2,mm1			;mm2 = [ 0][ 0][ 0][ 0][R3][G3][B3][R2]
	psrlq		mm0,32			;mm0 = [ 0][ 0][ 0][ 0][G2][B2][R1][G1]

	movd		dword [edi],mm3
	movd		dword [edi+4],mm0
	movd		dword [edi+8],mm2

	add		edi,12
	add		esi,8

	sub		edx,2
	ja		yuy2_bgr24_MMX_x

	jnz		yuy2_bgr24_MMX_noodd

yuy2_bgr24_MMX_doodd:
	movd		mm0,dword [esi]		;mm0 = [V][Y2][U][Y1]

	movq		mm1,mm0			;mm1 = [V][Y2][U][Y1]
	pand		mm0,mm6			;mm0 = [ Y2  ][ Y1  ]

	psubw		mm0,[Y_bias]
	psrlw		mm1,8			;mm1 = [ V ][ U ]

	psubw		mm1,[UV_bias]
	punpcklwd	mm0,mm0			;mm0 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]

	pmullw		mm0,[Y_coeff]
	punpcklwd	mm1,mm1			;mm1 = [ V ][ V ][ U ][ U ]

	movq		mm3,mm1			;mm3 = [ V ][ V ][ U ][ U ]
	punpckldq	mm1,mm1			;mm1 = [ U ][ U ][ U ][ U ]

	pmullw		mm1,[U_coeff]		;mm1 = [ 0 ][ 0 ][ UG ][ UB ]
	punpckhdq	mm3,mm3			;mm3 = [ V ][ V ][ V ][ V ]

	pmullw		mm3,[V_coeff]		;mm3 = [ 0 ][ VR ][ VG ][ 0 ]
	paddw		mm0,mm7			;add rounding to Y

	movq		mm2,mm0			;mm2 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]
	punpckldq	mm0,mm0			;mm0 = [ Y1 ][ Y1 ][ Y1 ][ Y1 ]

	punpckhdq	mm2,mm2			;mm2 = [ Y2 ][ Y2 ][ Y2 ][ Y2 ]
	paddw		mm0,mm1

	paddw		mm2,mm1
	paddw		mm0,mm3

	paddw		mm2,mm3
	psraw		mm0,6

	psraw		mm2,6
	pxor		mm5,mm5

	packuswb	mm0,mm5
	add		edi,6

	packuswb	mm2,mm5
	add		esi,4

	psllq		mm2,24
	push		eax

	por		mm0,mm2
	push		ebx

	movd		eax,mm0
	psrlq		mm2,32

	movd		ebx,mm2

	mov		[edi-6],eax
	mov		[edi+4-6],bx

	pop		ebx
	pop		eax	

yuy2_bgr24_MMX_noodd:
	add		edi,eax
	add		esi,ebx

	dec		ecx
	jne		yuy2_bgr24_MMX_y

	pop		ebx
	pop		esi
	pop		edi
	pop		ebp
	emms
	ret

;asm_convert_yuy2_fullscale_bgr24_MMX(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

_asm_convert_yuy2_fullscale_bgr24_MMX:
	push		ebp
	push		edi
	push		esi
	push		ebx

	mov		edi,[esp+4+16]
	mov		esi,[esp+8+16]
	mov		ebp,[esp+12+16]
	mov		ecx,[esp+16+16]
	mov		eax,[esp+20+16]
	mov		ebx,[esp+24+16]
	movq		mm6,[Y_mask]
	movq		mm7,[Y_round]

yuy2_fullscale_bgr24_MMX_y:
	mov		edx,ebp

	dec		edx
	jz		yuy2_bgr24_MMX_doodd
yuy2_fullscale_bgr24_MMX_x:
	movd		mm0,dword [esi]		;mm0 = [V][Y2][U][Y1]

	movq		mm1,mm0			;mm1 = [V][Y2][U][Y1]
	pand		mm0,mm6			;mm0 = [ Y2  ][ Y1  ]

	psrlw		mm1,8			;mm1 = [ V ][ U ]

	psubw		mm1,[UV_bias]
	punpcklwd	mm0,mm0			;mm0 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]

	pmullw		mm0,[Y_coeff2]
	punpcklwd	mm1,mm1			;mm1 = [ V ][ V ][ U ][ U ]

	movq		mm3,mm1			;mm3 = [ V ][ V ][ U ][ U ]
	punpckldq	mm1,mm1			;mm1 = [ U ][ U ][ U ][ U ]

	pmullw		mm1,[U_coeff2]		;mm1 = [ 0 ][ 0 ][ UG ][ UB ]
	punpckhdq	mm3,mm3			;mm3 = [ V ][ V ][ V ][ V ]

	pmullw		mm3,[V_coeff2]		;mm3 = [ 0 ][ VR ][ VG ][ 0 ]
	paddw		mm0,mm7			;add rounding to Y

	movq		mm2,mm0			;mm2 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]
	punpckldq	mm0,mm0			;mm0 = [ Y1 ][ Y1 ][ Y1 ][ Y1 ]

	punpckhdq	mm2,mm2			;mm2 = [ Y2 ][ Y2 ][ Y2 ][ Y2 ]
	paddw		mm0,mm1

	paddw		mm2,mm1
	paddw		mm0,mm3

	paddw		mm2,mm3
	psraw		mm0,6

	psraw		mm2,6
	pxor		mm5,mm5

	pand		mm0,[mask24]
	packuswb	mm2,mm5			;mm1 = [ 0][ 0][ 0][ 0][ 0][R1][G1][B1]

	psllq		mm2,24			;mm1 = [ 0][ 0][R1][G1][B1][ 0][ 0][ 0]

	pand		mm2,[mask24]
	packuswb	mm0,mm5			;mm0 = [ 0][ 0][ 0][ 0][ 0][R0][G0][B0]

	por		mm0,mm2			;mm0 = [ 0][ 0][R1][G1][B1][R0][G0][B0]

	;----------------------------------

	movd		mm4,dword [esi+4]		;mm4 = [V][Y2][U][Y1]

	movq		mm1,mm4			;mm1 = [V][Y2][U][Y1]
	pand		mm4,mm6			;mm4 = [ Y2  ][ Y1  ]

	psrlw		mm1,8			;mm1 = [ V ][ U ]

	psubw		mm1,[UV_bias]
	punpcklwd	mm4,mm4			;mm4 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]

	pmullw		mm4,[Y_coeff2]
	punpcklwd	mm1,mm1			;mm1 = [ V ][ V ][ U ][ U ]

	movq		mm3,mm1			;mm3 = [ V ][ V ][ U ][ U ]
	punpckldq	mm1,mm1			;mm1 = [ U ][ U ][ U ][ U ]

	pmullw		mm1,[U_coeff2]		;mm1 = [ 0 ][ 0 ][ UG ][ UB ]
	punpckhdq	mm3,mm3			;mm3 = [ V ][ V ][ V ][ V ]

	pmullw		mm3,[V_coeff2]		;mm3 = [ 0 ][ VR ][ VG ][ 0 ]
	paddw		mm4,mm7			;add rounding to Y

	movq		mm2,mm4			;mm2 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]
	punpckldq	mm4,mm4			;mm4 = [ Y1 ][ Y1 ][ Y1 ][ Y1 ]

	punpckhdq	mm2,mm2			;mm2 = [ Y2 ][ Y2 ][ Y2 ][ Y2 ]
	paddw		mm4,mm1

	paddw		mm2,mm1
	paddw		mm4,mm3

	paddw		mm2,mm3
	psraw		mm4,6

	pand		mm4,[mask24]
	psraw		mm2,6

	pand		mm2,[mask24]
	packuswb	mm4,mm5			;mm4 = [ 0][ 0][ 0][ 0][ 0][R2][G2][B2]

	packuswb	mm2,mm5			;mm2 = [ 0][ 0][ 0][ 0][ 0][R3][G3][B3]
	movq		mm1,mm4			;mm1 = [ 0][ 0][ 0][ 0][ 0][R2][G2][B2]

	psllq		mm4,48			;mm4 = [G2][B2][ 0][ 0][ 0][ 0][ 0][ 0]

	por		mm0,mm4			;mm0 = [G2][B2][R1][G1][B1][R0][G0][B0]
	psrlq		mm1,16			;mm1 = [ 0][ 0][ 0][ 0][ 0][ 0][ 0][R2]

	psllq		mm2,8			;mm2 = [ 0][ 0][ 0][ 0][R3][G3][B3][ 0]
	movq		mm3,mm0

	por		mm2,mm1			;mm2 = [ 0][ 0][ 0][ 0][R3][G3][B3][R2]
	psrlq		mm0,32			;mm0 = [ 0][ 0][ 0][ 0][G2][B2][R1][G1]

	movd		dword [edi],mm3
	movd		dword [edi+4],mm0
	movd		dword [edi+8],mm2

	add		edi,12
	add		esi,8

	sub		edx,2
	ja		yuy2_fullscale_bgr24_MMX_x

	jnz		yuy2_fullscale_bgr24_MMX_noodd

yuy2_fullscale_bgr24_MMX_doodd:
	movd		mm0,dword [esi]		;mm0 = [V][Y2][U][Y1]

	movq		mm1,mm0			;mm1 = [V][Y2][U][Y1]
	pand		mm0,mm6			;mm0 = [ Y2  ][ Y1  ]

	psrlw		mm1,8			;mm1 = [ V ][ U ]

	psubw		mm1,[UV_bias]
	punpcklwd	mm0,mm0			;mm0 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]

	pmullw		mm0,[Y_coeff2]
	punpcklwd	mm1,mm1			;mm1 = [ V ][ V ][ U ][ U ]

	movq		mm3,mm1			;mm3 = [ V ][ V ][ U ][ U ]
	punpckldq	mm1,mm1			;mm1 = [ U ][ U ][ U ][ U ]

	pmullw		mm1,[U_coeff]		;mm1 = [ 0 ][ 0 ][ UG ][ UB ]
	punpckhdq	mm3,mm3			;mm3 = [ V ][ V ][ V ][ V ]

	pmullw		mm3,[V_coeff]		;mm3 = [ 0 ][ VR ][ VG ][ 0 ]
	paddw		mm0,mm7			;add rounding to Y

	movq		mm2,mm0			;mm2 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]
	punpckldq	mm0,mm0			;mm0 = [ Y1 ][ Y1 ][ Y1 ][ Y1 ]

	punpckhdq	mm2,mm2			;mm2 = [ Y2 ][ Y2 ][ Y2 ][ Y2 ]
	paddw		mm0,mm1

	paddw		mm2,mm1
	paddw		mm0,mm3

	paddw		mm2,mm3
	psraw		mm0,6

	psraw		mm2,6
	pxor		mm5,mm5

	packuswb	mm0,mm5
	add		edi,6

	packuswb	mm2,mm5
	add		esi,4

	psllq		mm2,24
	push		eax

	por		mm0,mm2
	push		ebx

	movd		eax,mm0
	psrlq		mm2,32

	movd		ebx,mm2

	mov		[edi-6],eax
	mov		[edi+4-6],bx

	pop		ebx
	pop		eax

yuy2_fullscale_bgr24_MMX_noodd:
	add		edi,eax
	add		esi,ebx

	dec		ecx
	jne		yuy2_fullscale_bgr24_MMX_y

	pop		ebx
	pop		esi
	pop		edi
	pop		ebp
	emms
	ret

;asm_convert_yuy2_bgr32_MMX(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

_asm_convert_yuy2_bgr32_MMX:
	push		ebp
	push		edi
	push		esi
	push		ebx

	mov		edi,[esp+4+16]
	mov		esi,[esp+8+16]
	mov		ebp,[esp+12+16]
	mov		ecx,[esp+16+16]
	mov		eax,[esp+20+16]
	mov		ebx,[esp+24+16]
	movq		mm6,[Y_mask]
	movq		mm7,[Y_round]

yuy2_bgr32_MMX_y:
	mov		edx,ebp
yuy2_bgr32_MMX_x:
	movd		mm0,dword [esi]		;mm0 = [V][Y2][U][Y1]

	movq		mm1,mm0			;mm1 = [V][Y2][U][Y1]
	pand		mm0,mm6			;mm0 = [ Y2  ][ Y1  ]

	psubw		mm0,[Y_bias]
	psrlw		mm1,8			;mm1 = [ V ][ U ]

	psubw		mm1,[UV_bias]
	punpcklwd	mm0,mm0			;mm0 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]

	pmullw		mm0,[Y_coeff]
	punpcklwd	mm1,mm1			;mm1 = [ V ][ V ][ U ][ U ]

	movq		mm3,mm1			;mm3 = [ V ][ V ][ U ][ U ]
	punpckldq	mm1,mm1			;mm1 = [ U ][ U ][ U ][ U ]

	pmullw		mm1,[U_coeff]		;mm1 = [ 0 ][ 0 ][ UG ][ UB ]
	punpckhdq	mm3,mm3			;mm3 = [ V ][ V ][ V ][ V ]

	pmullw		mm3,[V_coeff]		;mm3 = [ 0 ][ VR ][ VG ][ 0 ]
	paddw		mm0,mm7			;add rounding to Y

	movq		mm2,mm0			;mm2 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]
	punpckldq	mm0,mm0			;mm0 = [ Y1 ][ Y1 ][ Y1 ][ Y1 ]

	punpckhdq	mm2,mm2			;mm2 = [ Y2 ][ Y2 ][ Y2 ][ Y2 ]
	paddw		mm0,mm1

	paddw		mm2,mm1
	paddw		mm0,mm3

	paddw		mm2,mm3
	psraw		mm0,6

	psraw		mm2,6
	add		edi,8

	packuswb	mm0,mm2
	add		esi,4

	movq		[edi-8],mm0

	dec		edx
	jne		yuy2_bgr32_MMX_x

	add		edi,eax
	add		esi,ebx

	dec		ecx
	jne		yuy2_bgr32_MMX_y

	pop		ebx
	pop		esi
	pop		edi
	pop		ebp
	emms
	ret

;asm_convert_yuy2_fullscale_bgr32_MMX(void *dst, void *src, int w, int h, ptrdiff_t dstmod, ptrdiff_t srcmod)

	global	_ycblit	

_asm_convert_yuy2_fullscale_bgr32_MMX:
_ycblit:
	push		ebp
	push		edi
	push		esi
	push		ebx

	mov		edi,[esp+4+16]
	mov		esi,[esp+8+16]
	mov		ebp,[esp+12+16]
	mov		ecx,[esp+16+16]
	mov		eax,[esp+20+16]
	mov		ebx,[esp+24+16]
	or		esi,esi
	jnz		yuy2_fullscale_bgr32_MMX_y0

	lea		esi,[_YUV_Y1_table]
	lea		ebx,[edi+4000h]
yuy2_fullscale_bgr32_MMX_x2:
	lodsb
	test		al,al
	jns		yuy2_fullscale_bgr32_MMX_y2
	lea		ecx,[eax-127]
	xor		al,al
	and		ecx,255
	rep		stosb
yuy2_fullscale_bgr32_MMX_x3:
	cmp		edi,ebx
	jb		yuy2_fullscale_bgr32_MMX_x2
	pop		ebx
	pop		esi
	pop		edi
	pop		ebp
	ret
yuy2_fullscale_bgr32_MMX_y2:
	mov		cl,al
	shr		cl,4
yuy2_fullscale_bgr32_MMX_y3:
	and		al,15
	stosb
	or		cl,cl
	jz		yuy2_fullscale_bgr32_MMX_x3
	lodsb
	mov		bl,al
	shr		al,4
	stosb
	mov		al,bl
	dec		cl
	jmp		short yuy2_fullscale_bgr32_MMX_y3
yuy2_fullscale_bgr32_MMX_y0:
	movq		mm6,[Y_mask]
	movq		mm7,[Y_round]
yuy2_fullscale_bgr32_MMX_y:
	mov		edx,ebp
yuy2_fullscale_bgr32_MMX_x:
	movd		mm0,dword [esi]		;mm0 = [V][Y2][U][Y1]

	movq		mm1,mm0			;mm1 = [V][Y2][U][Y1]
	pand		mm0,mm6			;mm0 = [ Y2  ][ Y1  ]

	psrlw		mm1,8			;mm1 = [ V ][ U ]

	psubw		mm1,[UV_bias]
	punpcklwd	mm0,mm0			;mm0 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]

	pmullw		mm0,[Y_coeff2]
	punpcklwd	mm1,mm1			;mm1 = [ V ][ V ][ U ][ U ]

	movq		mm3,mm1			;mm3 = [ V ][ V ][ U ][ U ]
	punpckldq	mm1,mm1			;mm1 = [ U ][ U ][ U ][ U ]

	pmullw		mm1,[U_coeff2]		;mm1 = [ 0 ][ 0 ][ UG ][ UB ]
	punpckhdq	mm3,mm3			;mm3 = [ V ][ V ][ V ][ V ]

	pmullw		mm3,[V_coeff2]		;mm3 = [ 0 ][ VR ][ VG ][ 0 ]
	paddw		mm0,mm7			;add rounding to Y

	movq		mm2,mm0			;mm2 = [ Y2 ][ Y2 ][ Y1 ][ Y1 ]
	punpckldq	mm0,mm0			;mm0 = [ Y1 ][ Y1 ][ Y1 ][ Y1 ]

	punpckhdq	mm2,mm2			;mm2 = [ Y2 ][ Y2 ][ Y2 ][ Y2 ]
	paddw		mm0,mm1

	paddw		mm2,mm1
	paddw		mm0,mm3

	paddw		mm2,mm3
	psraw		mm0,6

	psraw		mm2,6
	add		edi,8

	packuswb	mm0,mm2
	add		esi,4

	movq		[edi-8],mm0

	dec		edx
	jne		yuy2_fullscale_bgr32_MMX_x

	add		edi,eax
	add		esi,ebx

	dec		ecx
	jne		yuy2_fullscale_bgr32_MMX_y

	pop		ebx
	pop		esi
	pop		edi
	pop		ebp
	emms
	ret

	segment	.rdata, align=16

_YUV_Y1_table	dd	032800186h,0a2307657h,010424552h,021821023h,021a12133h,0329d1241h,09d317547h,020653431h
	dd	03243228ah,0a143119fh,064773542h,045319830h,0338d2053h,09b102342h,0a0204421h,077352261h
	dd	093104376h,091205522h,021234333h,012124195h,052a43234h,075776734h,024218f32h,044329553h
	dd	061913123h,043554533h,062a43034h,076774622h,0318a3165h,096106524h,033423541h,081018922h
	dd	044435561h,0a8314444h,077362261h,087426576h,010544632h,033334198h,062853144h,054351122h
	dd	043255545h,03452ad10h,052666556h,020652585h,04334419ch,062811033h,065362055h,032146556h
	dd	0572361b3h,011535665h,081505622h,0219a1011h,021824234h,042804135h,011536666h,0562361b8h
	dd	066664565h,041138302h,011223198h,054628111h,064773520h,04651be31h,043556666h,098511483h
	dd	061833211h,066464334h,051c11043h,032635634h,062158221h,082421198h,066674652h,041c51052h
	dd	040654622h,010632483h,010452195h,075464180h,021901032h,041b51044h,010425524h,097511480h
	dd	011835513h,056339332h,041b81076h,045314424h,041463197h,056319510h,031bd2154h,096513413h
	dd	020544731h,073352193h,0965112c5h,010215631h,045553191h,02014c672h,032573195h,066419020h
	dd	0a0302640h,05321a201h,064259520h,045419121h,0a0304665h,0319e1012h,095305411h,031964015h
	dd	09f417635h,0329e2012h,094405533h,031995411h,09d204543h,022a03011h,042935155h,021326535h
	dd	055235190h,09e203354h,03423a203h,020128c10h,067444281h,041912265h,052436656h,0a01012a0h
	dd	08c103422h,045343261h,094315536h,010303631h,0a02012a0h,08c103521h,056653561h,091315467h
	dd	075235443h,03012a120h,08a4113a1h,032442361h,015763611h,056519130h,020647666h,0a141129fh
	dd	031874112h,085324224h,043534631h,08010128eh,012421341h,041119f10h,0854112a1h,020544631h
	dd	054553289h,032218b10h,0a4018210h,09f124221h,082105222h,021435533h,02065248ch,01441628bh
	dd	064564543h,0102223a1h,01065219eh,040662380h,031652291h,08042128ah,064767754h,0229f3047h
	dd	0129e1033h,076248173h,075239330h,030128931h,082211181h,022a06411h,0419e1034h,010752531h
	dd	010642495h,087301288h,022a07313h,022a02034h,021973166h,013883046h,062158710h,0304421a0h
	dd	01064249fh,020662199h,086101387h,0a0105521h,014a03413h,065239c20h,031118510h,0a1321187h
	dd	0c0313221h,084105521h,023ad4013h,011c02023h,040148554h,0302322adh,0846313c1h,021ad3015h
	dd	021c14133h,011811065h,03421af53h,03531b020h,014891042h,053128262h,0af4311b0h,076776653h
	dd	011873065h,052138265h,0ae3012b1h,012224352h,014883033h,044218061h,0a6018830h,035c34111h
	dd	082422430h,013824113h,02013a651h,0102541c1h,024812014h,014803372h,02012a673h,0302442c1h
	dd	042811014h,071465564h,0c22011a6h,011802311h,043138341h,013643380h,02023ea30h,035218924h
	dd	0c501a442h,088233023h,0a5305522h,001b02012h,033402391h,052462186h,0af2011a7h,021904411h
	dd	022843251h,011a95345h,03431ae20h,0248e2164h,031812035h,0aa106356h,031ad1012h,08e544644h
	dd	081105424h,080106423h,012a83311h,04333ae10h,0138d4035h,010418363h,0a8766724h,032ae2012h
	dd	08d302532h,043856312h,020436677h,0ad2012a6h,051133233h,08450158fh,012813113h,03012a774h
	dd	0804213adh,0148f4014h,030128561h,0a7731480h,024ae4111h,014906343h,041358561h,011a77135h
	dd	03621ae41h,062139061h,067624184h,011a61275h,05412af41h,084621291h,066557642h,04111a673h
	dd	021800198h,0128f4245h,062129162h,015654184h,011a63076h,044629741h,044767767h,061148a32h
	dd	022125181h,084214334h,034866311h,0a6115177h,063954111h,066766756h,010125246h,082301389h
	dd	077566564h,081106477h,041846311h,051674466h,0921013a7h,067434561h,016244453h,046619120h
	dd	076576576h,030158254h,066544583h,013a77156h,041129210h,044765280h,092553545h,034435761h
	dd	001336434h,082301581h,020773251h,013a61037h,075649620h,045554642h,026618f10h,033442451h
	dd	030158350h,043773384h,02012a864h,043746696h,020455545h,04336618fh,062434323h,085301583h
	dd	0a9646724h,062952011h,065774576h,064915444h,034444364h,011814045h,022418774h,0a5104423h
	dd	074639602h,054767756h,064639156h,035336635h,050168250h,077674286h,012a53077h,071639410h
	dd	054767746h,062900146h,042773564h,015825024h,075468660h,0a5101143h,063942011h,076576574h
	dd	090014654h,077456363h,082612443h,011856114h,02011ac31h,050772294h,063554380h,0438f2047h
	dd	042665666h,082511280h,012856212h,02011ac10h,080101390h,081407721h,071346634h,034736691h
	dd	020362054h,085401682h,01012ae01h,01065248eh,082621480h,060356633h,081721491h,024805115h
	dd	015811074h,02011b560h,05335238eh,020652280h,047643480h,075119150h,016514381h,013813175h
	dd	03111b561h,045656391h,037541075h,001801017h,08043118ch,052136365h,081317657h,012b66311h
	dd	046619120h,066567777h,030452175h,05745628ch,075674663h,016824216h,02011b550h,067356193h
	dd	055767777h,0618d1013h,077776746h,010146677h,0b6711481h,035419702h,091434444h,077563462h
	dd	084526677h,011c36411h,03342a620h,086102233h,011b56115h,033218a10h,03013b321h,0af101282h
	dd	0448b2011h,010214266h,043554399h,021851021h,013801011h,0b1028230h,0518c3011h,055667747h
	dd	056629632h,055666777h,065463356h,010118030h,0b0201181h,0638e3011h,065777746h,022234455h
	dd	034629022h,077776656h,086101476h,012b03211h,035619041h,066567556h,08e641655h,052801012h
	dd	044555554h,010138a21h,0924212afh,055441261h,003756655h,01152358eh,08f018111h,011af1013h
	dd	081019343h,033223341h,023618f21h,033334234h,080211344h,011841111h,03014b142h,09231119bh
	dd	022222361h,080213224h,082102222h,013b13211h,0aa019c40h,012b01012h,02011c752h,0c64214b2h
	dd	023b23013h,011c32045h,02422b342h,03111c351h,0543132b3h,02321c010h,04131b210h,022c04125h
	dd	021b01034h,013805331h,03312c063h,0413224b3h,082521380h,04421ba01h,04251b210h,010341045h
	dd	0b8101180h,052b54312h,024105453h,010118110h,0b63213b6h,010444152h,011801034h,021229f32h
	dd	022229010h,05253b620h,010442035h,010322180h,04323319ch,020128d32h,0345343bah,021824221h
	dd	0419e2023h,010224323h,0bc201187h,023245343h,043218330h,095018821h,022233242h,06412c510h
	dd	020342380h,033433284h,010128431h,011223295h,06433c610h,021864312h,012863312h,021128b10h
	dd	023123185h,05411c710h,086421180h,012881012h,023318920h,021851021h,031c73233h,085101055h
	dd	011833111h,021128210h,022224188h,021852132h,001873124h,0105521bdh,083321187h,011821012h
	dd	010128232h,020133182h,023318812h,012831042h,04531bc10h,011852023h,021118232h,010324183h
	dd	051802013h,022221231h,022418510h,0c3313333h,010436534h,081321385h,021832111h,012802034h
	dd	021128031h,033233180h,045228921h,05432c331h,022852034h,012802033h,044628320h,033221221h
	dd	012418010h,087103233h,0c2203422h,042345531h,031332185h,083111180h,044334461h,082314344h
	dd	032333341h,010118610h,0335542c3h,012853144h,010118132h,054236183h,031454345h,023124181h
	dd	011862133h,03551c312h,031552342h,020224183h,061851011h,044222211h,011622233h,032433433h
	dd	0c6211222h,012324552h,012844245h,08e018021h,031443433h,033115181h,082113233h,061c21012h
	dd	024103334h,081013244h,012821111h,022418a10h,083103223h,022222251h,021c51020h,031813123h
	dd	086324423h,0328d2112h,086104333h,021ca1111h,031833023h,082104333h,08d102221h,021333332h
	dd	0ce101182h,084202321h,021333451h,0518f1033h,022332312h,03321d121h,024518410h,010323343h
	dd	032224192h,011d01121h,023618632h,022222132h,011119310h,0102221d2h,081101186h,022222261h
	dd	0e3102122h,0128c2111h,012418010h,0e5101222h,0fb103222h,0fd211321h,0ffff2011h,0ff0fbeffh
_YUV_Y_table2	dd	00100401h,00300c03h,00401004h,00501405h,00601806h,00701c07h,00802008h,00a0280ah
	dd	00b02c0bh,00c0300ch,00d0340dh,00e0380eh,00f03c0fh,01104411h,01204812h,01304c13h
	dd	01405014h,01505415h,01605816h,01705c17h,01906419h,01a0681ah,01b06c1bh,01c0701ch
	dd	01d0741dh,01e0781eh,02008020h,02108421h,02208822h,02308c23h,02409024h,02509425h
	dd	02709c27h,0280a028h,0290a429h,02a0a82ah,02b0ac2bh,02c0b02ch,02e0b82eh,02f0bc2fh
	dd	0300c030h,0310c431h,0320c832h,0330cc33h,0350d435h,0360d836h,0370dc37h,0380e038h
	dd	0390e439h,03a0e83ah,03c0f03ch,03d0f43dh,03e0f83eh,03f0fc3fh,04010040h,04110441h
	dd	04310c43h,04411044h,04511445h,04611846h,04711c47h,04812048h,04a1284ah,04b12c4bh
	dd	04c1304ch,04d1344dh,04e1384eh,04f13c4fh,05114451h,05214852h,05314c53h,05415054h
	dd	05515455h,05615856h,05816058h,05916459h,05a1685ah,05b16c5bh,05c1705ch,05d1745dh
	dd	05e1785eh,06018060h,06118461h,06218862h,06318c63h,06419064h,06519465h,06719c67h
	dd	0681a068h,0691a469h,06a1a86ah,06b1ac6bh,06c1b06ch,06e1b86eh,06f1bc6fh,0701c070h
	dd	0711c471h,0721c872h,0731cc73h,0751d475h,0761d876h,0771dc77h,0781e078h,0791e479h
	dd	07a1e87ah,07c1f07ch,07d1f47dh,07e1f87eh,07f1fc7fh,08020080h,08120481h,08320c83h
	dd	08421084h,08521485h,08621886h,08721c87h,08822088h,08a2288ah,08b22c8bh,08c2308ch
	dd	08d2348dh,08e2388eh,08f23c8fh,09124491h,09224892h,09324c93h,09425094h,09525495h
	dd	09625896h,09826098h,09926499h,09a2689ah,09b26c9bh,09c2709ch,09d2749dh,09f27c9fh
	dd	0a0280a0h,0a1284a1h,0a2288a2h,0a328ca3h,0a4290a4h,0a6298a6h,0a729ca7h,0a82a0a8h
	dd	0a92a4a9h,0aa2a8aah,0ab2acabh,0ac2b0ach,0ae2b8aeh,0af2bcafh,0b02c0b0h,0b12c4b1h
	dd	0b22c8b2h,0b32ccb3h,0b52d4b5h,0b62d8b6h,0b72dcb7h,0b82e0b8h,0b92e4b9h,0ba2e8bah
	dd	0bc2f0bch,0bd2f4bdh,0be2f8beh,0bf2fcbfh,0c0300c0h,0c1304c1h,0c330cc3h,0c4310c4h
	dd	0c5314c5h,0c6318c6h,0c731cc7h,0c8320c8h,0ca328cah,0cb32ccbh,0cc330cch,0cd334cdh
	dd	0ce338ceh,0cf33ccfh,0d1344d1h,0d2348d2h,0d334cd3h,0d4350d4h,0d5354d5h,0d6358d6h
	dd	0d8360d8h,0d9364d9h,0da368dah,0db36cdbh,0dc370dch,0dd374ddh,0df37cdfh,0e0380e0h
	dd	0e1384e1h,0e2388e2h,0e338ce3h,0e4390e4h,0e6398e6h,0e739ce7h,0e83a0e8h,0e93a4e9h
	dd	0ea3a8eah,0eb3acebh,0ed3b4edh,0ee3b8eeh,0ef3bcefh,0f03c0f0h,0f13c4f1h,0f23c8f2h
	dd	0f33ccf3h,0f53d4f5h,0f63d8f6h,0f73dcf7h,0f83e0f8h,0f93e4f9h,0fa3e8fah,0fc3f0fch
	dd	0fd3f4fdh,0fe3f8feh,0ff3fcffh,10040100h,10140501h,10340d03h,10441104h,10541505h
	dd	10641906h,10741d07h,10842108h,10a4290ah,10b42d0bh,10c4310ch,10d4350dh,10e4390eh
	dd	10f43d0fh,11144511h,11244912h,11344d13h,11445114h,11545515h,11645916h,11846118h
	dd	11946519h,11a4691ah,11b46d1bh,11c4711ch,11d4751dh,11f47d1fh,12048120h,12148521h
	dd	12248922h,12348d23h,12449124h,12649926h,12749d27h,1284a128h,1294a529h,12a4a92ah
_YUV_U_table2	dd	0001a81eh,0001a820h,0001a422h,0001a424h,0001a026h,0001a028h,0001a02ah,00019c2ch
	dd	00019c2eh,00019c30h,00019832h,00019834h,00019436h,00019438h,0001943ah,0001903ch
	dd	0001903eh,00018c40h,00018c42h,00018c44h,00018846h,00018848h,0001844ah,0001844ch
	dd	0001844eh,00018050h,00018052h,00017c54h,00017c56h,00017c58h,0001785ah,0001785ch
	dd	0001785eh,00017460h,00017462h,00017064h,00017066h,00017068h,00016c6ah,00016c6ch
	dd	0001686eh,00016870h,00016872h,00016474h,00016476h,00016079h,0001607bh,0001607dh
	dd	00015c7fh,00015c81h,00015883h,00015885h,00015887h,00015489h,0001548bh,0001548dh
	dd	0001508fh,00015091h,00014c93h,00014c95h,00014c97h,00014899h,0001489bh,0001449dh
	dd	0001449fh,000144a1h,000140a3h,000140a5h,00013ca7h,00013ca9h,00013cabh,000138adh
	dd	000138afh,000138b1h,000134b3h,000134b5h,000130b7h,000130b9h,000130bbh,00012cbdh
	dd	00012cbfh,000128c1h,000128c3h,000128c5h,000124c7h,000124c9h,000120cbh,000120cdh
	dd	000120cfh,00011cd1h,00011cd3h,000118d5h,000118d7h,000118d9h,000114dbh,000114ddh
	dd	000114dfh,000110e1h,000110e3h,00010ce5h,00010ce7h,00010ceah,000108ech,000108eeh
	dd	000104f0h,000104f2h,000104f4h,000100f6h,000100f8h,0000fcfah,0000fcfch,0000fcfeh
	dd	0000f900h,0000f902h,0000f504h,0000f506h,0000f508h,0000f10ah,0000f10ch,0000f10eh
	dd	0000ed10h,0000ed12h,0000e914h,0000e916h,0000e918h,0000e51ah,0000e51ch,0000e11eh
	dd	0000e120h,0000e122h,0000dd24h,0000dd26h,0000d928h,0000d92ah,0000d92ch,0000d52eh
	dd	0000d530h,0000d132h,0000d134h,0000d136h,0000cd38h,0000cd3ah,0000cd3ch,0000c93eh
	dd	0000c940h,0000c542h,0000c544h,0000c546h,0000c148h,0000c14ah,0000bd4ch,0000bd4eh
	dd	0000bd50h,0000b952h,0000b954h,0000b556h,0000b559h,0000b55bh,0000b15dh,0000b15fh
	dd	0000ad61h,0000ad63h,0000ad65h,0000a967h,0000a969h,0000a96bh,0000a56dh,0000a56fh
	dd	0000a171h,0000a173h,0000a175h,00009d77h,00009d79h,0000997bh,0000997dh,0000997fh
	dd	00009581h,00009583h,00009185h,00009187h,00009189h,00008d8bh,00008d8dh,0000898fh
	dd	00008991h,00008993h,00008595h,00008597h,00008599h,0000819bh,0000819dh,00007d9fh
	dd	00007da1h,00007da3h,000079a5h,000079a7h,000075a9h,000075abh,000075adh,000071afh
	dd	000071b1h,00006db3h,00006db5h,00006db7h,000069b9h,000069bbh,000069bdh,000065bfh
	dd	000065c1h,000061c3h,000061c5h,000061c7h,00005dcah,00005dcch,000059ceh,000059d0h
	dd	000059d2h,000055d4h,000055d6h,000051d8h,000051dah,000051dch,00004ddeh,00004de0h
	dd	000049e2h,000049e4h,000049e6h,000045e8h,000045eah,000045ech,000041eeh,000041f0h
	dd	00003df2h,00003df4h,00003df6h,000039f8h,000039fah,000035fch,000035feh,00003600h
	dd	00003202h,00003204h,00002e06h,00002e08h,00002e0ah,00002a0ch,00002a0eh,00002610h
	dd	00002612h,00002614h,00002216h,00002218h,0000221ah,00001e1ch,00001e1eh,00001a20h
_YUV_V_table2	dd	0143a000h,01539c00h,01739800h,01939800h,01a39400h,01c39000h,01d38c00h,01f38800h
	dd	02038800h,02238400h,02438000h,02537c00h,02737800h,02837400h,02a37400h,02c37000h
	dd	02d36c00h,02f36800h,03036400h,03236400h,03436000h,03535c00h,03735800h,03835400h
	dd	03a35400h,03c35000h,03d34c00h,03f34800h,04034400h,04234000h,04434000h,04533c00h
	dd	04733800h,04833400h,04a33000h,04c33000h,04d32c00h,04f32800h,05032400h,05232000h
	dd	05432000h,05531c00h,05731800h,05831400h,05a31000h,05c30c00h,05d30c00h,05f30800h
	dd	06030400h,06230000h,0642fc00h,0652fc00h,0672f800h,0682f400h,06a2f000h,06b2ec00h
	dd	06d2ec00h,06f2e800h,0702e400h,0722e000h,0732dc00h,0752d800h,0772d800h,0782d400h
	dd	07a2d000h,07b2cc00h,07d2c800h,07f2c800h,0802c400h,0822c000h,0832bc00h,0852b800h
	dd	0872b800h,0882b400h,08a2b000h,08b2ac00h,08d2a800h,08f2a400h,0902a400h,0922a000h
	dd	09329c00h,09529800h,09729400h,09829400h,09a29000h,09b28c00h,09d28800h,09f28400h
	dd	0a028400h,0a228000h,0a327c00h,0a527800h,0a727400h,0a827000h,0aa27000h,0ab26c00h
	dd	0ad26800h,0af26400h,0b026000h,0b226000h,0b325c00h,0b525800h,0b725400h,0b825000h
	dd	0ba25000h,0bb24c00h,0bd24800h,0be24400h,0c024000h,0c223c00h,0c323c00h,0c523800h
	dd	0c623400h,0c823000h,0ca22c00h,0cb22c00h,0cd22800h,0ce22400h,0d022000h,0d221c00h
	dd	0d321c00h,0d521800h,0d621400h,0d821000h,0da20c00h,0db20800h,0dd20800h,0de20400h
	dd	0e020000h,0e21fc00h,0e31f800h,0e51f800h,0e61f400h,0e81f000h,0ea1ec00h,0eb1e800h
	dd	0ed1e400h,0ee1e400h,0f01e000h,0f21dc00h,0f31d800h,0f51d400h,0f61d400h,0f81d000h
	dd	0fa1cc00h,0fb1c800h,0fd1c400h,0fe1c400h,1001c000h,1021bc00h,1031b800h,1051b400h
	dd	1061b000h,1081b000h,1091ac00h,10b1a800h,10d1a400h,10e1a000h,1101a000h,11119c00h
	dd	11319800h,11519400h,11619000h,11819000h,11918c00h,11b18800h,11d18400h,11e18000h
	dd	12017c00h,12117c00h,12317800h,12517400h,12617000h,12816c00h,12916c00h,12b16800h
	dd	12d16400h,12e16000h,13015c00h,13115c00h,13315800h,13515400h,13615000h,13814c00h
	dd	13914800h,13b14800h,13d14400h,13e14000h,14013c00h,14113800h,14313800h,14513400h
	dd	14613000h,14812c00h,14912800h,14b12800h,14d12400h,14e12000h,15011c00h,15111800h
	dd	15311400h,15511400h,15611000h,15810c00h,15910800h,15b10400h,15c10400h,15e10000h
	dd	1600fc00h,1610f800h,1630f400h,1640f400h,1660f000h,1680ec00h,1690e800h,16b0e400h
	dd	16c0e000h,16e0e000h,1700dc00h,1710d800h,1730d400h,1740d000h,1760d000h,1780cc00h
	dd	1790c800h,17b0c400h,17c0c000h,17e0c000h,1800bc00h,1810b800h,1830b400h,1840b000h
	dd	1860ac00h,1880ac00h,1890a800h,18b0a400h,18c0a000h,18e09c00h,19009c00h,19109800h
	dd	19309400h,19409000h,19608c00h,19808c00h,19908800h,19b08400h,19c08000h,19e07c00h
	dd	1a007800h,1a107800h,1a307400h,1a407000h,1a606c00h,1a806800h,1a906800h,1ab06400h

_YUV_Y2_table2	dd	01405014h,01505415h,01605816h,01705c17h,01806018h,01906419h,01a0681ah,01b06c1bh
	dd	01c0701ch,01d0741dh,01e0781eh,01f07c1fh,02008020h,02108421h,02208822h,02308c23h
	dd	02409024h,02509425h,02609826h,02709c27h,0280a028h,0290a429h,02a0a82ah,02b0ac2bh
	dd	02c0b02ch,02d0b42dh,02e0b82eh,02f0bc2fh,0300c030h,0310c431h,0320c832h,0330cc33h
	dd	0340d034h,0350d435h,0360d836h,0370dc37h,0380e038h,0390e439h,03a0e83ah,03b0ec3bh
	dd	03c0f03ch,03d0f43dh,03e0f83eh,03f0fc3fh,04010040h,04110441h,04210842h,04310c43h
	dd	04411044h,04511445h,04611846h,04711c47h,04812048h,04912449h,04a1284ah,04b12c4bh
	dd	04c1304ch,04d1344dh,04e1384eh,04f13c4fh,05014050h,05114451h,05214852h,05314c53h
	dd	05415054h,05515455h,05615856h,05715c57h,05816058h,05916459h,05a1685ah,05b16c5bh
	dd	05c1705ch,05d1745dh,05e1785eh,05f17c5fh,06018060h,06118461h,06218862h,06318c63h
	dd	06419064h,06519465h,06619866h,06719c67h,0681a068h,0691a469h,06a1a86ah,06b1ac6bh
	dd	06c1b06ch,06d1b46dh,06e1b86eh,06f1bc6fh,0701c070h,0711c471h,0721c872h,0731cc73h
	dd	0741d074h,0751d475h,0761d876h,0771dc77h,0781e078h,0791e479h,07a1e87ah,07b1ec7bh
	dd	07c1f07ch,07d1f47dh,07e1f87eh,07f1fc7fh,08020080h,08120481h,08220882h,08320c83h
	dd	08421084h,08521485h,08621886h,08721c87h,08822088h,08922489h,08a2288ah,08b22c8bh
	dd	08c2308ch,08d2348dh,08e2388eh,08f23c8fh,09024090h,09124491h,09224892h,09324c93h
	dd	09425094h,09525495h,09625896h,09725c97h,09826098h,09926499h,09a2689ah,09b26c9bh
	dd	09c2709ch,09d2749dh,09e2789eh,09f27c9fh,0a0280a0h,0a1284a1h,0a2288a2h,0a328ca3h
	dd	0a4290a4h,0a5294a5h,0a6298a6h,0a729ca7h,0a82a0a8h,0a92a4a9h,0aa2a8aah,0ab2acabh
	dd	0ac2b0ach,0ad2b4adh,0ae2b8aeh,0af2bcafh,0b02c0b0h,0b12c4b1h,0b22c8b2h,0b32ccb3h
	dd	0b42d0b4h,0b52d4b5h,0b62d8b6h,0b72dcb7h,0b82e0b8h,0b92e4b9h,0ba2e8bah,0bb2ecbbh
	dd	0bc2f0bch,0bd2f4bdh,0be2f8beh,0bf2fcbfh,0c0300c0h,0c1304c1h,0c2308c2h,0c330cc3h
	dd	0c4310c4h,0c5314c5h,0c6318c6h,0c731cc7h,0c8320c8h,0c9324c9h,0ca328cah,0cb32ccbh
	dd	0cc330cch,0cd334cdh,0ce338ceh,0cf33ccfh,0d0340d0h,0d1344d1h,0d2348d2h,0d334cd3h
	dd	0d4350d4h,0d5354d5h,0d6358d6h,0d735cd7h,0d8360d8h,0d9364d9h,0da368dah,0db36cdbh
	dd	0dc370dch,0dd374ddh,0de378deh,0df37cdfh,0e0380e0h,0e1384e1h,0e2388e2h,0e338ce3h
	dd	0e4390e4h,0e5394e5h,0e6398e6h,0e739ce7h,0e83a0e8h,0e93a4e9h,0ea3a8eah,0eb3acebh
	dd	0ec3b0ech,0ed3b4edh,0ee3b8eeh,0ef3bcefh,0f03c0f0h,0f13c4f1h,0f23c8f2h,0f33ccf3h
	dd	0f43d0f4h,0f53d4f5h,0f63d8f6h,0f73dcf7h,0f83e0f8h,0f93e4f9h,0fa3e8fah,0fb3ecfbh
	dd	0fc3f0fch,0fd3f4fdh,0fe3f8feh,0ff3fcffh,10040100h,10140501h,10240902h,10340d03h
	dd	10441104h,10541505h,10641906h,10741d07h,10842108h,10942509h,10a4290ah,10b42d0bh
	dd	10c4310ch,10d4350dh,10e4390eh,10f43d0fh,11044110h,11144511h,11244912h,11344d13h
_YUV_U2_table2	dd	0001903dh,0001903fh,00018c41h,00018c43h,00018c44h,00018846h,00018848h,0001884ah
	dd	0001844bh,0001844dh,0001844fh,00018051h,00018052h,00018054h,00017c56h,00017c58h
	dd	00017c5ah,0001785bh,0001785dh,0001785fh,00017461h,00017462h,00017064h,00017066h
	dd	00017068h,00016c69h,00016c6bh,00016c6dh,0001686fh,00016871h,00016872h,00016474h
	dd	00016476h,00016478h,00016079h,0001607bh,0001607dh,00015c7fh,00015c81h,00015c82h
	dd	00015884h,00015886h,00015888h,00015489h,0001548bh,0001548dh,0001508fh,00015090h
	dd	00015092h,00014c94h,00014c96h,00014898h,00014899h,0001489bh,0001449dh,0001449fh
	dd	000144a0h,000140a2h,000140a4h,000140a6h,00013ca8h,00013ca9h,00013cabh,000138adh
	dd	000138afh,000138b0h,000134b2h,000134b4h,000134b6h,000130b7h,000130b9h,000130bbh
	dd	00012cbdh,00012cbfh,00012cc0h,000128c2h,000128c4h,000128c6h,000124c7h,000124c9h
	dd	000124cbh,000120cdh,000120ceh,00011cd0h,00011cd2h,00011cd4h,000118d6h,000118d7h
	dd	000118d9h,000114dbh,000114ddh,000114deh,000110e0h,000110e2h,000110e4h,00010ce6h
	dd	00010ce7h,00010ce9h,000108ebh,000108edh,000108eeh,000104f0h,000104f2h,000104f4h
	dd	000100f5h,000100f7h,000100f9h,0000fcfbh,0000fcfdh,0000fcfeh,0000f900h,0000f902h
	dd	0000f904h,0000f505h,0000f507h,0000f109h,0000f10bh,0000f10dh,0000ed0eh,0000ed10h
	dd	0000ed12h,0000e914h,0000e915h,0000e917h,0000e519h,0000e51bh,0000e51ch,0000e11eh
	dd	0000e120h,0000e122h,0000dd24h,0000dd25h,0000dd27h,0000d929h,0000d92bh,0000d92ch
	dd	0000d52eh,0000d530h,0000d532h,0000d133h,0000d135h,0000d137h,0000cd39h,0000cd3bh
	dd	0000c93ch,0000c93eh,0000c940h,0000c542h,0000c543h,0000c545h,0000c147h,0000c149h
	dd	0000c14bh,0000bd4ch,0000bd4eh,0000bd50h,0000b952h,0000b953h,0000b955h,0000b557h
	dd	0000b559h,0000b55ah,0000b15ch,0000b15eh,0000b160h,0000ad62h,0000ad63h,0000ad65h
	dd	0000a967h,0000a969h,0000a96ah,0000a56ch,0000a56eh,0000a570h,0000a172h,0000a173h
	dd	00009d75h,00009d77h,00009d79h,0000997ah,0000997ch,0000997eh,00009580h,00009581h
	dd	00009583h,00009185h,00009187h,00009189h,00008d8ah,00008d8ch,00008d8eh,00008990h
	dd	00008991h,00008993h,00008595h,00008597h,00008598h,0000819ah,0000819ch,0000819eh
	dd	00007da0h,00007da1h,00007da3h,000079a5h,000079a7h,000079a8h,000075aah,000075ach
	dd	000071aeh,000071b0h,000071b1h,00006db3h,00006db5h,00006db7h,000069b8h,000069bah
	dd	000069bch,000065beh,000065bfh,000065c1h,000061c3h,000061c5h,000061c7h,00005dc8h
	dd	00005dcah,00005dcch,000059ceh,000059cfh,000059d1h,000055d3h,000055d5h,000055d7h
	dd	000051d8h,000051dah,000051dch,00004ddeh,00004ddfh,000049e1h,000049e3h,000049e5h
	dd	000045e6h,000045e8h,000045eah,000041ech,000041eeh,000041efh,00003df1h,00003df3h
	dd	00003df5h,000039f6h,000039f8h,000039fah,000035fch,000035feh,000035ffh,00003201h
_YUV_V2_table2	dd	02d36c00h,02e36c00h,02f36800h,03136400h,03236400h,03436000h,03535c00h,03635800h
	dd	03835800h,03935400h,03b35000h,03c35000h,03d34c00h,03f34800h,04034400h,04234400h
	dd	04334000h,04433c00h,04633c00h,04733800h,04933400h,04a33000h,04b33000h,04d32c00h
	dd	04e32800h,05032800h,05132400h,05232000h,05431c00h,05531c00h,05731800h,05831400h
	dd	05931400h,05b31000h,05c30c00h,05e30800h,05f30800h,06030400h,06230000h,06330000h
	dd	0652fc00h,0662f800h,0672f400h,0692f400h,06a2f000h,06c2ec00h,06d2ec00h,06e2e800h
	dd	0702e400h,0712e000h,0732e000h,0742dc00h,0752d800h,0772d800h,0782d400h,07a2d000h
	dd	07b2cc00h,07c2cc00h,07e2c800h,07f2c400h,0812c400h,0822c000h,0832bc00h,0852b800h
	dd	0862b800h,0882b400h,0892b000h,08a2b000h,08c2ac00h,08d2a800h,08f2a400h,0902a400h
	dd	0912a000h,09329c00h,09429c00h,09629800h,09729400h,09829000h,09a29000h,09b28c00h
	dd	09d28800h,09e28800h,0a028400h,0a128000h,0a227c00h,0a427c00h,0a527800h,0a727400h
	dd	0a827400h,0a927000h,0ab26c00h,0ac26800h,0ae26800h,0af26400h,0b026000h,0b226000h
	dd	0b325c00h,0b525800h,0b625400h,0b725400h,0b925000h,0ba24c00h,0bc24c00h,0bd24800h
	dd	0be24400h,0c024000h,0c124000h,0c323c00h,0c423800h,0c523800h,0c723400h,0c823000h
	dd	0ca22c00h,0cb22c00h,0cc22800h,0ce22400h,0cf22400h,0d122000h,0d221c00h,0d321800h
	dd	0d521800h,0d621400h,0d821000h,0d921000h,0da20c00h,0dc20800h,0dd20400h,0df20400h
	dd	0e020000h,0e11fc00h,0e31fc00h,0e41f800h,0e61f400h,0e71f000h,0e81f000h,0ea1ec00h
	dd	0eb1e800h,0ed1e800h,0ee1e400h,0ef1e000h,0f11dc00h,0f21dc00h,0f41d800h,0f51d400h
	dd	0f61d400h,0f81d000h,0f91cc00h,0fb1c800h,0fc1c800h,0fd1c400h,0ff1c000h,1001c000h
	dd	1021bc00h,1031b800h,1041b400h,1061b400h,1071b000h,1091ac00h,10a1ac00h,10b1a800h
	dd	10d1a400h,10e1a000h,1101a000h,11119c00h,11219800h,11419800h,11519400h,11719000h
	dd	11818c00h,11918c00h,11b18800h,11c18400h,11e18400h,11f18000h,12017c00h,12217800h
	dd	12317800h,12517400h,12617000h,12817000h,12916c00h,12a16800h,12c16400h,12d16400h
	dd	12f16000h,13015c00h,13115c00h,13315800h,13415400h,13615000h,13715000h,13814c00h
	dd	13a14800h,13b14800h,13d14400h,13e14000h,13f13c00h,14113c00h,14213800h,14413400h
	dd	14513400h,14613000h,14812c00h,14912800h,14b12800h,14c12400h,14d12000h,14f12000h
	dd	15011c00h,15211800h,15311400h,15411400h,15611000h,15710c00h,15910c00h,15a10800h
	dd	15b10400h,15d10000h,15e10000h,1600fc00h,1610f800h,1620f800h,1640f400h,1650f000h
	dd	1670ec00h,1680ec00h,1690e800h,16b0e400h,16c0e400h,16e0e000h,16f0dc00h,1700d800h
	dd	1720d800h,1730d400h,1750d000h,1760d000h,1770cc00h,1790c800h,17a0c400h,17c0c400h
	dd	17d0c000h,17e0bc00h,1800bc00h,1810b800h,1830b400h,1840b000h,1850b000h,1870ac00h
	dd	1880a800h,18a0a800h,18b0a400h,18c0a000h,18e09c00h,18f09c00h,19109800h,19209400h

	end

