;	Priss (NekoAmp 2.0) - MPEG-1/2 audio decoding library
;	Copyright (C) 2003-2004 Avery Lee
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
c2		dd	0.92387953251128674,0.92387953251128674,0.92387953251128674,0.92387953251128674
c4		dd	0.70710678118654757,0.70710678118654757,0.70710678118654757,0.70710678118654757
c6		dd	0.38268343236508984,0.38268343236508984,0.38268343236508984,0.38268343236508984

; [1 .5 .5 -.5 .5 .5 .5 .5] ./ cos(pi*[0 5 6 1 4 7 2 3]/16)
d		dd	+1.00000000000000,+1.00000000000000,+1.00000000000000,+1.00000000000000
		dd	+0.89997622313642,+0.89997622313642,+0.89997622313642,+0.89997622313642
		dd	+1.30656296487638,+1.30656296487638,+1.30656296487638,+1.30656296487638
		dd	-0.50979557910416,-0.50979557910416,-0.50979557910416,-0.50979557910416
		dd	+0.70710678118655,+0.70710678118655,+0.70710678118655,+0.70710678118655
		dd	+2.56291544774151,+2.56291544774151,+2.56291544774151,+2.56291544774151
		dd	+0.54119610014620,+0.54119610014620,+0.54119610014620,+0.54119610014620
		dd	+0.60134488693505,+0.60134488693505,+0.60134488693505,+0.60134488693505

invother	dd	0, 80000000h, 0, 80000000h
invall		dd	80000000h, 80000000h, 80000000h, 80000000h

		extern	leecoef1 : far
		extern	leecoef2 : far

	segment	.text

	global	vdasm_mpegaudio_polyphase_dctinputbutterflies
vdasm_mpegaudio_polyphase_dctinputbutterflies:
		xor		r9, r9
		mov		r8, 48
		lea		r10, [leecoef1]
		lea		r11, [leecoef2]
.xloop:
		movups	xmm0, [rdx+r9]			;xmm0 = in[i]
		movups	xmm1, [rdx+r8]			;xmm1 = in[15-i]
		movups	xmm2, [rdx+r9+64]		;xmm2 = in[i+16]
		movups	xmm3, [rdx+r8+64]		;xmm3 = in[31-i]
		shufps	xmm1, xmm1, 00011011b
		shufps	xmm3, xmm3, 00011011b

		;butterfly for first decomposition
		movaps	xmm4, xmm0
		movaps	xmm5, xmm1
		addps	xmm0, xmm3				;xmm0 = y0 = x0+x3
		addps	xmm1, xmm2				;xmm1 = y1 = x1+x2
		subps	xmm4, xmm3				;xmm4 = y2 = x0-x3
		subps	xmm5, xmm2				;xmm5 = y3 = x1-x2
		mulps	xmm4, [r10+r9]
		mulps	xmm5, [r10+r9+32]

		;butterfly for second decomposition
		movaps	xmm2, xmm0
		movaps	xmm3, xmm4
		addps	xmm0, xmm1				;xmm0 = z0 = y0+y1
		subps	xmm2, xmm1				;xmm2 = z1 = y0-y1
		addps	xmm3, xmm5				;xmm3 = z2 = y2+y3
		subps	xmm4, xmm5				;xmm4 = z3 = y2-y3
		mulps	xmm2, [r11+r9]
		mulps	xmm4, [r11+r9]

		;interleave in 0-2-1-3 order
		movaps		xmm1, xmm0
		unpcklps	xmm0, xmm3			;xmm0 = z2B | z0B | z2A | z0A
		unpckhps	xmm1, xmm3			;xmm1 = z2D | z0D | z2C | z0C
		movaps		xmm3, xmm2
		unpcklps	xmm2, xmm4			;xmm2 = z3B | z1B | z3A | z1A
		unpckhps	xmm3, xmm4			;xmm3 = z3D | z1D | z3C | z1C

		movlps	qword [rcx   ], xmm0
		movlps	qword [rcx+ 8], xmm2
		movhps	qword [rcx+16], xmm0
		movhps	qword [rcx+24], xmm2
		movlps	qword [rcx+32], xmm1
		movlps	qword [rcx+40], xmm3
		movhps	qword [rcx+48], xmm1
		movhps	qword [rcx+56], xmm3

		add		rcx, 64
		add		r9, 16
		sub		r8, 16
		cmp		r9, r8
		jb		.xloop
		ret


	global	vdasm_mpegaudio_polyphase_dct4x8
vdasm_mpegaudio_polyphase_dct4x8:
;	See the FPU version to get an idea of the flow of this AAN
;	implementation.  Note that we do all four DCTs in parallel!

		movlhps	xmm14, xmm6
		movlhps	xmm15, xmm7

		;even part - B3 (4a)
		movaps	xmm0, [rcx+0*16]		;xmm0 = s[0]
		movaps	xmm1, [rcx+1*16]		;xmm1 = s[1]
		movaps	xmm2, [rcx+2*16]		;xmm2 = s[2]
		movaps	xmm3, [rcx+3*16]		;xmm3 = s[3]
		addps	xmm0, [rcx+7*16]		;xmm0 = s[0]+s[7]
		addps	xmm1, [rcx+6*16]		;xmm1 = s[1]+s[6]
		addps	xmm2, [rcx+5*16]		;xmm2 = s[2]+s[5]
		addps	xmm3, [rcx+4*16]		;xmm3 = s[3]+s[4]

		;even part - B2/~B1a (4a)
		movaps	xmm4, xmm0
		addps	xmm0, xmm3				;xmm0 = b2[0] = b3[0]+b3[3]
		movaps	xmm5, xmm1
		addps	xmm1, xmm2				;xmm1 = b2[1] = b3[1]+b2[2]
		subps	xmm4, xmm3				;xmm4 = b2[2] = b3[0]-b3[3]
		subps	xmm5, xmm2				;xmm5 = b2[3] = b3[1]-b3[2]

		;even part - ~B1b/M (3a1m)
		movaps	xmm2, xmm0
		subps	xmm4, xmm5
		addps	xmm0, xmm1				;xmm0 = m[0] = b2[0] + b2[1]
		mulps	xmm4, [c4]				;xmm4 = m[2] = (b2[2] - b2[3])*c4
		subps	xmm2, xmm1				;xmm2 = m[1] = b2[0] - b2[1]

		;even part - R1 (2a)
		movaps	xmm3, xmm4
		subps	xmm4, xmm5				;xmm4 = r1[3] = m[2]-m[3]
		addps	xmm3, xmm5				;xmm3 = r1[2] = m[2]+m[3]

		;even part - d (4m)
		mulps	xmm0, [d+0*16]			;xmm0 = out[0] = r1[0]*d[0]
		mulps	xmm2, [d+4*16]			;xmm2 = out[4] = r1[1]*d[4]
		mulps	xmm3, [d+2*16]			;xmm3 = out[2] = r1[2]*d[2]
		mulps	xmm4, [d+6*16]			;xmm4 = out[6] = r1[3]*d[6]

		;odd part - B3 (4a)
		movaps	xmm1, [rcx+0*16]
		movaps	xmm5, [rcx+1*16]
		movaps	xmm6, [rcx+2*16]
		movaps	xmm7, [rcx+3*16]
		subps	xmm1, [rcx+7*16]		;xmm1 = b3[4] = s[0]-s[7]
		subps	xmm5, [rcx+6*16]		;xmm5 = b3[5] = s[1]-s[6]
		subps	xmm6, [rcx+5*16]		;xmm6 = b3[6] = s[2]-s[5]
		subps	xmm7, [rcx+4*16]		;xmm7 = b3[7] = s[3]-s[4]

		;even part - writeout
		movaps	[rcx+0*16], xmm0
		movaps	[rcx+4*16], xmm2
		movaps	[rcx+2*16], xmm3
		movaps	[rcx+6*16], xmm4

		;odd part - B2/~B1a (3a)
		addps	xmm5, xmm7				;xmm5 = b2[5] = b3[5]+b3[7]
		subps	xmm7, xmm1				;xmm7 = b2[7] = b3[7]-b3[4]
		subps	xmm1, xmm6				;xmm1 = b2[4] = b3[4]-b3[6]

		;odd part - ~B1b/M (2a5m)
		movaps	xmm0, xmm1
		mulps	xmm7, [c4]				;xmm7 = m[7] = c4*b2[7]
		movaps	xmm2, xmm5
		mulps	xmm0, [c6]
		mulps	xmm1, [c2]
		mulps	xmm2, [c2]
		mulps	xmm5, [c6]
		addps	xmm0, xmm2				;xmm0 = m[4] = c6*b2[4] + c2*b2[5]
		subps	xmm1, xmm5				;xmm1 = m[5] = c2*b2[4] - c6*b2[5]

		;odd part - R1a (2a)
		movaps	xmm5, xmm6
		addps	xmm6, xmm7				;xmm6 = r1a[6] = m[6]+m[7]
		subps	xmm5, xmm7				;xmm5 = r1a[7] = m[6]-m[7]

		;odd part - R1b (4a)
		movaps	xmm3, xmm5
		movaps	xmm4, xmm6
		subps	xmm5, xmm0				;xmm5 = r1b[7] = r1a[7]-r1a[4]
		subps	xmm6, xmm1				;xmm6 = r1b[6] = r1a[6]-r1a[5]
		addps	xmm4, xmm1				;xmm4 = r1b[5] = r1a[6]+r1a[5]
		addps	xmm3, xmm0				;xmm3 = r1b[4] = r1a[7]+r1a[4]

		;odd part - D (4a)
		mulps	xmm3, [d+1*16]
		mulps	xmm4, [d+5*16]
		mulps	xmm6, [d+3*16]
		mulps	xmm5, [d+7*16]

		;odd part - writeout
		movaps	[rcx+1*16], xmm3
		movaps	[rcx+5*16], xmm4
		movaps	[rcx+3*16], xmm6
		movaps	[rcx+7*16], xmm5

		movhlps	xmm6, xmm14
		movhlps	xmm7, xmm15
		ret

;void vdasm_mpegaudio_polyphase_matrixout_stereo(const float (*pSrc)[16], const float *pWinFwd, const float *pWinRev, int inc, const uint32 *pSampleInv, const sint16 *pDst, const float (*pSrcFinal)[16], const uint32 *pFinalMask);

	global	vdasm_mpegaudio_polyphase_matrixout_stereo
vdasm_mpegaudio_polyphase_matrixout_stereo:
		;rcx = pointer to subband samples
		;rdx = pointer to forward window
		;r8 = pointer to reverse window
		;r9 = source increment

		movlhps	xmm15, xmm6
		movlhps	xmm14, xmm7

		movsxd	r9, r9d
		mov		r10, [rsp+40]			;r10 = pointer to sample inversion value
		mov		r11, [rsp+48]			;r11 = pointer to first two forward destination samples
		lea		rax, [r11+120]			;rax = pointer to first two reverse destination samples

		;compute first sample (0)

		movaps	xmm5, oword [invother]
		movups	xmm0, [rdx]				;load window samples 0-3
		xorps	xmm0, xmm5				;toggle signs on odd window samples
		movaps	xmm1, xmm0
		mulps	xmm0, [rcx]				;multiply by left subband samples
		mulps	xmm1, [rcx+64]			;multiply by right subband samples
		movups	xmm2, [rdx+16]			;load window samples 4-7
		xorps	xmm2, xmm5				;toggle signs on odd window samples
		movaps	xmm3, xmm2
		mulps	xmm2, [rcx+16]			;multiply by left subband samples
		mulps	xmm3, [rcx+80]			;multiply by right subband samples
		addps	xmm0, xmm2
		addps	xmm1, xmm3
		movups	xmm2, [rdx+32]			;load window samples 8-11
		xorps	xmm2, xmm5				;toggle signs on odd window samples
		movaps	xmm3, xmm2
		mulps	xmm2, [rcx+32]			;multiply by left subband samples
		mulps	xmm3, [rcx+96]			;multiply by right subband samples
		addps	xmm0, xmm2
		addps	xmm1, xmm3
		movups	xmm2, [rdx+48]			;load window samples 12-15
		xorps	xmm2, xmm5				;toggle signs on odd window samples
		movaps	xmm3, xmm2
		mulps	xmm2, [rcx+48]			;multiply by left subband samples
		mulps	xmm3, [rcx+112]			;multiply by right subband samples
		addps	xmm0, xmm2
		addps	xmm1, xmm3

		movaps	xmm2, xmm0				;xmm2 = l3 | l2 | l1 | l0
		movlhps	xmm0, xmm1				;xmm0 = r1 | r0 | l1 | l0
		movhlps	xmm1, xmm2				;xmm1 = r3 | r2 | l3 | l2
		addps	xmm0, xmm1				;xmm0 = r1+r3 | r0+r2 | l1+l3 | l0+l2
		shufps	xmm0, xmm0, 11011000b	;xmm0 = r1+r3 | l1+l3 | r0+r2 | l0+l2
		movhlps	xmm3, xmm0				;xmm3 =   ?   |   ?   | r1+r3 | l1+l3
		movaps	xmm4, [r10]
		movhlps	xmm4, xmm4
		addps	xmm0, xmm3				;xmm0 = ? | ? | r | l
		xorps	xmm0, xmm4
		cvtps2dq	xmm0, xmm0
		packssdw	xmm0, xmm0
		movd	dword [r11-4], xmm0

		add		rdx, 128
		add		r8, 128
		add		rcx, r9

		;compute reflected samples (1-15, 17-31)
.xloop:
		movups	xmm2, [r8+48]
		shufps	xmm2, xmm2, 00011011b	;xmm2 = reverse window
		movups	xmm3, [rdx]				;xmm3 = forward window
		xorps	xmm3, [invother]		;negate every other sample in forward window
		movaps	xmm0, [rcx]				;xmm0 = left source
		movaps	xmm1, xmm0
		mulps	xmm0, xmm2
		mulps	xmm1, xmm3
		movaps	xmm4, xmm0				;xmm4 = left forward
		movaps	xmm5, xmm1				;xmm5 = left reverse
		movaps	xmm0, [rcx+64]			;xmm0 = left source
		movaps	xmm1, xmm0
		mulps	xmm0, xmm2
		mulps	xmm1, xmm3
		movaps	xmm6, xmm0				;xmm6 = right forward
		movaps	xmm7, xmm1				;xmm7 = right reverse

		movups	xmm2, [r8+32]
		shufps	xmm2, xmm2, 00011011b	;xmm2 = reverse window
		movups	xmm3, [rdx+16]			;xmm3 = forward window
		xorps	xmm3, [invother]		;negate every other sample in forward window
		movaps	xmm0, [rcx+16]			;xmm0 = left source
		movaps	xmm1, xmm0
		mulps	xmm0, xmm2
		mulps	xmm1, xmm3
		addps	xmm4, xmm0				;xmm4 += left forward
		addps	xmm5, xmm1				;xmm5 += left reverse
		movaps	xmm0, [rcx+80]			;xmm0 = left source
		movaps	xmm1, xmm0
		mulps	xmm0, xmm2
		mulps	xmm1, xmm3
		addps	xmm6, xmm0				;xmm6 += right forward
		addps	xmm7, xmm1				;xmm7 += right reverse

		movups	xmm2, [r8+16]
		shufps	xmm2, xmm2, 00011011b	;xmm2 = reverse window
		movups	xmm3, [rdx+32]			;xmm3 = forward window
		xorps	xmm3, [invother]		;negate every other sample in forward window
		movaps	xmm0, [rcx+32]			;xmm0 = left source
		movaps	xmm1, xmm0
		mulps	xmm0, xmm2
		mulps	xmm1, xmm3
		addps	xmm4, xmm0				;xmm4 += left forward
		addps	xmm5, xmm1				;xmm5 += left reverse
		movaps	xmm0, [rcx+96]			;xmm0 = left source
		movaps	xmm1, xmm0
		mulps	xmm0, xmm2
		mulps	xmm1, xmm3
		addps	xmm6, xmm0				;xmm6 += right forward
		addps	xmm7, xmm1				;xmm7 += right reverse

		movups	xmm2, [r8]
		shufps	xmm2, xmm2, 00011011b	;xmm2 = reverse window
		movups	xmm3, [rdx+48]			;xmm3 = forward window
		xorps	xmm3, [invother]		;negate every other sample in forward window
		movaps	xmm0, [rcx+48]			;xmm0 = left source
		movaps	xmm1, xmm0
		mulps	xmm0, xmm2
		mulps	xmm1, xmm3
		addps	xmm4, xmm0				;xmm4 += left forward
		addps	xmm5, xmm1				;xmm5 += left reverse
		movaps	xmm0, [rcx+112]			;xmm0 = left source
		movaps	xmm1, xmm0
		mulps	xmm0, xmm2
		mulps	xmm1, xmm3
		addps	xmm6, xmm0				;xmm6 += right forward
		addps	xmm7, xmm1				;xmm7 += right reverse

		movaps	xmm0, xmm4				;xmm0 = lf3 | lf2 | lf1 | lf0
		movaps	xmm1, xmm5
		movlhps	xmm0, xmm6				;xmm0 = rf0 | rf1 | lf1 | lf0
		movlhps	xmm1, xmm7
		movhlps	xmm6, xmm4				;xmm6 = rf3 | rf2 | lf3 | lf2
		movhlps	xmm7, xmm5
		addps	xmm0, xmm6				;xmm0 = rf0+rf3 | rf1+rf2 | lf1+lf3 | lf0+lf2
		addps	xmm1, xmm7
		movaps	xmm2, xmm0
		movaps	xmm3, xmm1
		shufps	xmm0, xmm0, 10110001b	;xmm0 = rf1+rf2 | rf0+rf3 | lf0+lf2 | lf1+lf3
		shufps	xmm1, xmm1, 10110001b
		addps	xmm0, xmm2				;xmm0 = rf | rf | lf | lf
		addps	xmm1, xmm3				;xmm1 = rb | rb | lb | lb
		shufps	xmm0, xmm1, 10001000b	;xmm0 = rf | lf | rb | lb
		xorps	xmm0, [r10]
		cvtps2dq	xmm0, xmm0
		packssdw	xmm0, xmm0
		movd	dword [rax], xmm0
		psrldq	xmm0, 4
		movd	dword [r11], xmm0

		add		r11,4
		sub		rax,4
		add		rcx,r9
		add		rdx,128
		add		r8,128
		cmp		r11,rax
		jne		.xloop

		;do last sample (16)
		mov		rcx, [rsp+56]
		mov		rax, [rsp+64]
		movaps	xmm5, [rax]				;load final mask (masks out every other sample)

		movups	xmm0, [rdx]				;load window samples 0-3
		andps	xmm0, xmm5				;mask out every other sample
		movaps	xmm1, xmm0
		mulps	xmm0, [rcx]				;multiply by left subband samples
		mulps	xmm1, [rcx+64]			;multiply by right subband samples
		movups	xmm2, [rdx+16]			;load window samples 4-7
		andps	xmm2, xmm5				;mask out every other sample
		movaps	xmm3, xmm2
		mulps	xmm2, [rcx+16]			;multiply by left subband samples
		mulps	xmm3, [rcx+80]			;multiply by right subband samples
		addps	xmm0, xmm2
		addps	xmm1, xmm3
		movups	xmm2, [rdx+32]			;load window samples 8-11
		andps	xmm2, xmm5				;mask out every other sample
		movaps	xmm3, xmm2
		mulps	xmm2, [rcx+32]			;multiply by left subband samples
		mulps	xmm3, [rcx+96]			;multiply by right subband samples
		addps	xmm0, xmm2
		addps	xmm1, xmm3
		movups	xmm2, [rdx+48]			;load window samples 12-15
		andps	xmm2, xmm5				;mask out every other sample
		movaps	xmm3, xmm2
		mulps	xmm2, [rcx+48]			;multiply by left subband samples
		mulps	xmm3, [rcx+112]			;multiply by right subband samples
		addps	xmm0, xmm2
		addps	xmm1, xmm3

		movaps	xmm2, xmm0				;xmm2 = l3 | l2 | l1 | l0
		movlhps	xmm0, xmm1				;xmm0 = r1 | r0 | l1 | l0
		movhlps	xmm1, xmm2				;xmm1 = r3 | r2 | l3 | l2
		addps	xmm0, xmm1				;xmm0 = r1+r3 | r0+r2 | l1+l3 | l0+l2
		shufps	xmm0, xmm0, 11011000b	;xmm0 = r1+r3 | l1+l3 | r0+r2 | l0+l2
		movhlps	xmm3, xmm0				;xmm3 =   ?   |   ?   | r1+r3 | l1+l3
		addps	xmm0, xmm3				;xmm0 = ? | ? | r | l
		xorps	xmm0, [invall]
		cvtps2dq	xmm0, xmm0
		packssdw	xmm0, xmm0
		movd	dword [r11], xmm0

		movhlps	xmm6, xmm15
		movhlps	xmm7, xmm14
		ret

		end
