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

%define		PROFILE	0

;This code is based on the fragments from the Intel Application Note AP-922.
;
;Apologies to Intel; Adobe Acrobat screws up formatting royally.
;
;
;
;IEEE-1180 results:
;	pmse      mse       pme      me
;1	0.018000, 0.013742, 0.003100,-0.000083
;2	0.018600, 0.013573, 0.003300, 0.000217
;3	0.014100, 0.011441, 0.003900, 0.000106
;4	0.017900, 0.013700, 0.004500, 0.000056
;5	0.018300, 0.013623, 0.004900,-0.000239
;6	0.014000, 0.011439, 0.003600,-0.000117



;=============================================================================
;
; These examples contain code fragments for first stage iDCT 8x8
; (for rows) and first stage DCT 8x8 (for columns)
;
;=============================================================================

BITS_INV_ACC	equ		4			; 4 or 5 for IEEE
SHIFT_INV_ROW	equ		16 - BITS_INV_ACC
SHIFT_INV_COL	equ		1 + BITS_INV_ACC
RND_INV_ROW		equ		1024 * (6 - BITS_INV_ACC) ; 1 << (SHIFT_INV_ROW-1)
RND_INV_COL		equ		16 * (BITS_INV_ACC - 3) ; 1 << (SHIFT_INV_COL-1)
RND_INV_CORR	equ		RND_INV_COL - 1 ; correction -1.0 and round

	segment	.rdata, align=16
		Align	16

rounder		dw	4,4,4,4
one_corr	dw	1, 1, 1, 1
round_inv_row	dd	RND_INV_ROW, RND_INV_ROW
round_inv_col	dw	RND_INV_COL, RND_INV_COL, RND_INV_COL, RND_INV_COL
round_inv_corr	dw	RND_INV_CORR, RND_INV_CORR, RND_INV_CORR, RND_INV_CORR
tg_1_16		dw	13036, 13036, 13036, 13036	; tg * (2<<16)
tg_2_16		dw	27146, 27146, 27146, 27146	; tg * (2<<16)
tg_3_16		dw	-21746, -21746, -21746, -21746	; tg * (2<<16) - 1.0
cos_4_16	dw	-19195, -19195, -19195, -19195	; cos * (2<<16) - 1.0
ocos_4_16	dw	23170, 23170, 23170, 23170	; cos * (2<<15) + 0.5
ucos_4_16	dw	46341, 46341, 46341, 46341	; cos * (2<<16)

jump_tab	dd	tail_inter, tail_intra, tail_mjpeg,0

		%if PROFILE
		extern _sprintf:near
		extern _OutputDebugStringA@4:near
last_tick	dq	0
total_cnt	dd	65536*16
total_tick	dd	7fffffffh
total_tick2	dd	7fffffffh
profile_str	db	"avg clocks for 1M IDCTs: %d row, %d col",10,0
		Align	16
		%endif

;=============================================================================
;
; The first stage iDCT 8x8 - inverse DCTs of rows
;
;-----------------------------------------------------------------------------
; The 8-point inverse DCT direct algorithm
;-----------------------------------------------------------------------------
;
; static const short w[32] = {
; FIX(cos_4_16), FIX(cos_2_16), FIX(cos_4_16), FIX(cos_6_16),
; FIX(cos_4_16), FIX(cos_6_16), -FIX(cos_4_16), -FIX(cos_2_16),
; FIX(cos_4_16), -FIX(cos_6_16), -FIX(cos_4_16), FIX(cos_2_16),
; FIX(cos_4_16), -FIX(cos_2_16), FIX(cos_4_16), -FIX(cos_6_16),
; FIX(cos_1_16), FIX(cos_3_16), FIX(cos_5_16), FIX(cos_7_16),
; FIX(cos_3_16), -FIX(cos_7_16), -FIX(cos_1_16), -FIX(cos_5_16),
; FIX(cos_5_16), -FIX(cos_1_16), FIX(cos_7_16), FIX(cos_3_16),
; FIX(cos_7_16), -FIX(cos_5_16), FIX(cos_3_16), -FIX(cos_1_16) };
;
; #define DCT_8_INV_ROW(x, y)
;{
; int a0, a1, a2, a3, b0, b1, b2, b3;
;
; a0 =x[0]*w[0]+x[2]*w[1]+x[4]*w[2]+x[6]*w[3];
; a1 =x[0]*w[4]+x[2]*w[5]+x[4]*w[6]+x[6]*w[7];
; a2 = x[0] * w[ 8] + x[2] * w[ 9] + x[4] * w[10] + x[6] * w[11];
; a3 = x[0] * w[12] + x[2] * w[13] + x[4] * w[14] + x[6] * w[15];
; b0 = x[1] * w[16] + x[3] * w[17] + x[5] * w[18] + x[7] * w[19];
; b1 = x[1] * w[20] + x[3] * w[21] + x[5] * w[22] + x[7] * w[23];
; b2 = x[1] * w[24] + x[3] * w[25] + x[5] * w[26] + x[7] * w[27];
; b3 = x[1] * w[28] + x[3] * w[29] + x[5] * w[30] + x[7] * w[31];
;
; y[0] = SHIFT_ROUND ( a0 + b0 );
; y[1] = SHIFT_ROUND ( a1 + b1 );
; y[2] = SHIFT_ROUND ( a2 + b2 );
; y[3] = SHIFT_ROUND ( a3 + b3 );
; y[4] = SHIFT_ROUND ( a3 - b3 );
; y[5] = SHIFT_ROUND ( a2 - b2 );
; y[6] = SHIFT_ROUND ( a1 - b1 );
; y[7] = SHIFT_ROUND ( a0 - b0 );
;}
;
;-----------------------------------------------------------------------------
;
; In this implementation the outputs of the iDCT-1D are multiplied
; for rows 0,4 - by cos_4_16,
; for rows 1,7 - by cos_1_16,
; for rows 2,6 - by cos_2_16,
; for rows 3,5 - by cos_3_16
; and are shifted to the left for better accuracy
;
; For the constants used,
; FIX(float_const) = (short) (float_const * (1<<15) + 0.5)
;
;=============================================================================

		align 16

; Table for rows 0,4 - constants are multiplied by cos_4_16
; Table for rows 1,7 - constants are multiplied by cos_1_16
; Table for rows 2,6 - constants are multiplied by cos_2_16
; Table for rows 3,5 - constants are multiplied by cos_3_16

tab_i_04_short	dw  16384,  21407, 16384,   8867 ; w05 w04 w01 w00
				dw  16384,  -8867, 16384, -21407 ; w13 w12 w09 w08
				dw  22725,  19266, 19266,  -4520 ; w21 w20 w17 w16
				dw  12873, -22725,  4520, -12873 ; w29 w28 w25 w24

tab_i_17_short	dw  22725,  29692, 22725,  12299 ; w05 w04 w01 w00
				dw  22725, -12299, 22725, -29692 ; w13 w12 w09 w08
				dw  31521,  26722, 26722,  -6270 ; w21 w20 w17 w16
				dw  17855, -31521,  6270, -17855 ; w29 w28 w25 w24

tab_i_26_short	dw  21407,  27969, 21407,  11585 ; w05 w04 w01 w00
				dw  21407, -11585, 21407, -27969 ; w13 w12 w09 w08
				dw  29692,  25172, 25172,  -5906 ; w21 w20 w17 w16
				dw  16819, -29692,  5906, -16819 ; w29 w28 w25 w24

tab_i_35_short	dw  19266,  25172, 19266,  10426 ; w05 w04 w01 w00
				dw  19266, -10426, 19266, -25172 ; w13 w12 w09 w08
				dw  26722,  22654, 22654,  -5315 ; w21 w20 w17 w16
				dw  15137, -26722,  5315, -15137 ; w29 w28 w25 w24



; Table for rows 0,4 - constants are multiplied by cos_4_16

tab_i_04	dw  16384,  16384,  16384, -16384 ; w06 w04 w02 w00
			dw  21407,   8867,   8867, -21407 ; w07 w05 w03 w01
			dw  16384, -16384,  16384,  16384 ; w14 w12 w10 w08
			dw  -8867,  21407, -21407,  -8867 ; w15 w13 w11 w09
			dw  22725,  12873,  19266, -22725 ; w22 w20 w18 w16
			dw  19266,   4520,  -4520, -12873 ; w23 w21 w19 w17
			dw  12873,   4520,   4520,  19266 ; w30 w28 w26 w24
			dw -22725,  19266, -12873, -22725 ; w31 w29 w27 w25

; Table for rows 1,7 - constants are multiplied by cos_1_16

tab_i_17	dw  22725,  22725,  22725, -22725 ; movq-> w06 w04 w02 w00
			dw  29692,  12299,  12299, -29692 ; w07 w05 w03 w01
			dw  22725, -22725,  22725,  22725 ; w14 w12 w10 w08
			dw -12299,  29692, -29692, -12299 ; w15 w13 w11 w09
			dw  31521,  17855,  26722, -31521 ; w22 w20 w18 w16
			dw  26722,   6270,  -6270, -17855 ; w23 w21 w19 w17
			dw  17855,   6270,   6270,  26722 ; w30 w28 w26 w24
			dw -31521,  26722, -17855, -31521 ; w31 w29 w27 w25

; Table for rows 2,6 - constants are multiplied by cos_2_16

tab_i_26	dw  21407,  21407,  21407, -21407 ; movq-> w06 w04 w02 w00
			dw  27969,  11585,  11585, -27969 ; w07 w05 w03 w01
			dw  21407, -21407,  21407,  21407 ; w14 w12 w10 w08
			dw -11585,  27969, -27969, -11585 ; w15 w13 w11 w09
			dw  29692,  16819,  25172, -29692 ; w22 w20 w18 w16
			dw  25172,   5906,  -5906, -16819 ; w23 w21 w19 w17
			dw  16819,   5906,   5906,  25172 ; w30 w28 w26 w24
			dw -29692,  25172, -16819, -29692 ; w31 w29 w27 w25

; Table for rows 3,5 - constants are multiplied by cos_3_16

tab_i_35	dw  19266,  19266,  19266, -19266 ; movq-> w06 w04 w02 w00
			dw  25172,  10426,  10426, -25172 ; w07 w05 w03 w01
			dw  19266, -19266,  19266,  19266 ; w14 w12 w10 w08
			dw -10426,  25172, -25172, -10426 ; w15 w13 w11 w09
			dw  26722,  15137,  22654, -26722 ; w22 w20 w18 w16
			dw  22654,   5315,  -5315, -15137 ; w23 w21 w19 w17
			dw  15137,   5315,   5315,  22654 ; w30 w28 w26 w24
			dw -26722,  22654, -15137, -26722 ; w31 w29 w27 w25

rowstart_tbl2	dd	dorow_7is
		dd	dorow_6is
		dd	dorow_5is
		dd	dorow_4is
		dd	dorow_3is
		dd	dorow_2is
		dd	dorow_1is
		dd	dorow_0is
		dd	do_dc_isse

rowstart_tbl:
		dd	dorow_7
		dd	dorow_6
		dd	dorow_5
		dd	dorow_4
		dd	dorow_3
		dd	dorow_2
		dd	dorow_1
		dd	dorow_0
		dd	do_dc_mmx

pos_tab	times	 1 db (8*4)		;pos 0:     DC only
		times	 1 db (7*4)		;pos 1:     1 AC row
		times	 1 db (6*4)		;pos 2:     2 AC rows
		times	 6 db (5*4)		;pos 3-8:   3 AC rows
		times	 1 db (4*4)		;pos 9:     4 AC rows
		times	10 db (3*4)		;pos 10-19: 5 AC rows
		times	 1 db (2*4)		;pos 20:	6 AC rows
		times	14 db (1*4)		;pos 21-34:	7 AC rows
		times	29 db (0*4)		;pos 35-63: 8 AC rows

;-----------------------------------------------------------------------------
%macro	DCT_8_INV_ROW_1 3
.inp	equ		%1
.out	equ		%2
.table	equ		%3

		movq		mm0, [.inp+0]		; x5 x1 x4 x0

		movq		mm3, [.table]	; 3	; w06 w04 w02 w00
		movq		mm5, mm0		; 5	; x5 x1 x4 x0

		movq		mm4, [.table+8]	; 4	; w07 w05 w03 w01
		punpckldq	mm0, mm0			; x4 x0 x4 x0

		movq		mm2, [.inp+8]	; 1	; x7 x3 x6 x2
		pmaddwd		mm3, mm0			; x4*w06+x0*w04 x4*w02+x0*w00

		movq		mm1, [.table+32]	; 1	; w22 w20 w18 w16
		movq		mm6, mm2		; 6	; x7 x3 x6 x2

		punpckldq	mm2, mm2			; x6 x2 x6 x2

		punpckhdq	mm6, mm6			; x7 x3 x7 x3
		pmaddwd		mm4, mm2			; x6*w07+x2*w05 x6*w03+x2*w01

		pmaddwd		mm0, [.table+16]		; x4*w14+x0*w12 x4*w10+x0*w08
		punpckhdq	mm5, mm5			; x5 x1 x5 x1

		movq		mm7, [.table+40]	; 7	; w23 w21 w19 w17
		pmaddwd		mm1, mm5			; x5*w22+x1*w20 x5*w18+x1*w16

		paddd		mm3, [round_inv_row]	; +rounder
		pmaddwd		mm7, mm6			; x7*w23+x3*w21 x7*w19+x3*w17

		pmaddwd		mm2, [.table+24]		; x6*w15+x2*w13 x6*w11+x2*w09
		paddd		mm3, mm4		; 4	; a1=sum(even1) a0=sum(even0)

		pmaddwd		mm5, [.table+48]		; x5*w30+x1*w28 x5*w26+x1*w24
		movq		mm4, mm3		; 4	; a1 a0

		pmaddwd		mm6, [.table+56]		; x7*w31+x3*w29 x7*w27+x3*w25
		paddd		mm1, mm7		; 7	; b1=sum(odd1) b0=sum(odd0)

		paddd		mm0, [round_inv_row]	; +rounder
		psubd		mm3, mm1			; a1-b1 a0-b0

		psrad		mm3, SHIFT_INV_ROW		; y6=a1-b1 y7=a0-b0
		paddd		mm1, mm4		; 4	; a1+b1 a0+b0

		paddd		mm0, mm2		; 2	; a3=sum(even3) a2=sum(even2)
		psrad		mm1, SHIFT_INV_ROW		; y1=a1+b1 y0=a0+b0

		paddd		mm5, mm6		; 6	; b3=sum(odd3) b2=sum(odd2)
		movq		mm4, mm0		; 4	; a3 a2

		paddd		mm0, mm5			; a3+b3 a2+b2
		psubd		mm4, mm5		; 5	; a3-b3 a2-b2

		psrad		mm0, SHIFT_INV_ROW		; y3=a3+b3 y2=a2+b2

		psrad		mm4, SHIFT_INV_ROW		; y4=a3-b3 y5=a2-b2

		packssdw	mm1, mm0		; 0	; y3 y2 y1 y0

		packssdw	mm4, mm3		; 3	; y6 y7 y4 y5

		movq		mm7, mm4		; 7	; y6 y7 y4 y5
		psrld		mm4, 16				; 0 y6 0 y4

		pslld		mm7, 16				; y7 0 y5 0

		movq		[.out], mm1		; 1	; save y3 y2 y1 y0
		por		mm7, mm4		; 4	; y7 y6 y5 y4

		movq		[.out+8], mm7	; 7	; save y7 y6 y5 y4
%endmacro


%macro DCT_8_INV_ROW_1_ISSE 3
%%inp	equ		%1
%%out	equ		%2
%%table	equ		%3

		movq		mm0, [%%inp+0]			; x5 x1 x4 x0

		movq		mm3, [%%table]			; 3	; w06 w04 w02 w00
		pshufw		mm5, mm0, 11101110b		; 5	; x5 x1 x5 x1

		movq		mm4, [%%table+8]			; 4	; w07 w05 w03 w01
		punpckldq	mm0, mm0				; x4 x0 x4 x0

		movq		mm2, [%%inp+8]			; 1	; x7 x3 x6 x2
		pmaddwd		mm3, mm0				; x4*w06+x0*w04 x4*w02+x0*w00

		movq		mm1, [%%table+32]		; 1	; w22 w20 w18 w16
		pshufw		mm6, mm2, 11101110b		; 6	; x7 x3 x7 x3

		pmaddwd		mm0, [%%table+16]		; x4*w14+x0*w12 x4*w10+x0*w08
		punpckldq	mm2, mm2				; x6 x2 x6 x2

		pmaddwd		mm4, mm2				; x6*w07+x2*w05 x6*w03+x2*w01

		movq		mm7, [%%table+40]		; 7	; w23 w21 w19 w17
		pmaddwd		mm1, mm5				; x5*w22+x1*w20 x5*w18+x1*w16

		paddd		mm3, [round_inv_row]	; +rounder
		pmaddwd		mm7, mm6				; x7*w23+x3*w21 x7*w19+x3*w17

		pmaddwd		mm2, [%%table+24]		; x6*w15+x2*w13 x6*w11+x2*w09
		paddd		mm3, mm4				; 4	; a1=sum(even1) a0=sum(even0)

		pmaddwd		mm5, [%%table+48]		; x5*w30+x1*w28 x5*w26+x1*w24
		movq		mm4, mm3				; 4	; a1 a0

		pmaddwd		mm6, [%%table+56]		; x7*w31+x3*w29 x7*w27+x3*w25
		paddd		mm1, mm7				; 7	; b1=sum(odd1) b0=sum(odd0)

		paddd		mm0, [round_inv_row]	; +rounder
		psubd		mm3, mm1				; a1-b1 a0-b0

		psrad		mm3, SHIFT_INV_ROW		; y6=a1-b1 y7=a0-b0
		paddd		mm1, mm4				; 4	; a1+b1 a0+b0

		paddd		mm0, mm2				; 2	; a3=sum(even3) a2=sum(even2)
		psrad		mm1, SHIFT_INV_ROW		; y1=a1+b1 y0=a0+b0

		paddd		mm5, mm6				; 6	; b3=sum(odd3) b2=sum(odd2)
		movq		mm4, mm0				; 4	; a3 a2

		paddd		mm0, mm5				; a3+b3 a2+b2
		psubd		mm4, mm5				; 5	; a3-b3 a2-b2

		psrad		mm0, SHIFT_INV_ROW		; y3=a3+b3 y2=a2+b2

		psrad		mm4, SHIFT_INV_ROW		; y4=a3-b3 y5=a2-b2

		packssdw	mm1, mm0				; 0	; y3 y2 y1 y0

		packssdw	mm4, mm3				; 3	; y6 y7 y4 y5

		movq		[%%out], mm1			; 1	; save y3 y2 y1 y0
		pshufw		mm4, mm4, 10110001b		; y7 y6 y5 y4

		movq		[%%out+8], mm4		; 7	; save y7 y6 y5 y4
%endmacro

%macro DCT_8_INV_ROW_1_ISSE_SHORT 3
%%inp	equ		%1
%%out	equ		%2
%%table	equ		%3

		movq		mm0, [%%inp+8]			;     0 x3 0 x2

		movq		mm1, [%%table]
		psllq		mm0, 16

		por			mm0, [%%inp]				;	  x3 x1 x2 x0

		movq		mm2, [%%table+16]
		pshufw		mm3, mm0, 01000100b		;     x2 x0 x2 x0

		pshufw		mm4, mm0, 11101110b		;     x3 x1 x3 x1
		pmaddwd		mm1, mm3				;     w05*x2+w04*x0 w01*x2+w00*x0 == a1 a0

		movq		mm5, [round_inv_row]
		pmaddwd		mm2, mm4				;     w21*x3+w20*x1 w17*x3+w16*x1 == b1 b0

		pmaddwd		mm3, [%%table+8]			;     w13*x2+w12*x0 w09*x2+w08*x0 == a3 a2
		;

		pmaddwd		mm4, [%%table+24]		;     w29*x3+w28*x1 w25*x3+w24*x1 == b3 b2
		paddd		mm1, mm5				;     a1+r a0+r

		movq		mm6, mm1				; mm6 = a1+r     a0+r
		paddd		mm1, mm2				; mm1 = a1+b1+r  a0+b0+r

		paddd		mm3, mm5				; mm3 = a3+r     a2+r
		psubd		mm6, mm2				; mm6 = a1-b1+r  a0-b0+r

		movq		mm0, mm3				; mm0 = a3+r     a2+r
		psrad		mm1, SHIFT_INV_ROW		; mm1 = y1       y0

		paddd		mm3, mm4				; mm3 = a3+b3+r  a2+b2+r
		psrad		mm6, SHIFT_INV_ROW		; mm6 = y6       y7

		psubd		mm0, mm4				; mm0 = a3-b3+r  a2-b2+r
		psrad		mm3, SHIFT_INV_ROW		; mm3 = y3       y2

		psrad		mm0, SHIFT_INV_ROW		; mm0 = y4       y5

		packssdw	mm1, mm3				; mm1 = y3 y2 y1 y0

		packssdw	mm6, mm0				; mm0 = y4 y5 y6 y7

		movq		[%%out], mm1
		pshufw		mm6, mm6, 00011011b		; mm0 = y7 y6 y5 y4

		movq		[%%out+8], mm6
%endmacro


;=============================================================================
;
; The second stage iDCT 8x8 - inverse DCTs of columns
;
; The outputs are premultiplied
; for rows 0,4 - on cos_4_16,
; for rows 1,7 - on cos_1_16,
; for rows 2,6 - on cos_2_16,
; for rows 3,5 - on cos_3_16
; and are shifted to the left for rise of accuracy
;
;-----------------------------------------------------------------------------
;
; The 8-point scaled inverse DCT algorithm (26a8m)
;
;-----------------------------------------------------------------------------
;
;	// Reorder and prescale (implicit)
;
;	ev0 = co[0] / 2.0;
;	ev1 = co[2] / 2.0;
;	ev2 = co[4] / 2.0;
;	ev3 = co[6] / 2.0;
;	od0 = co[1] / 2.0;
;	od1 = co[3] / 2.0;
;	od2 = co[5] / 2.0;
;	od3 = co[7] / 2.0;
;
;	// 5) Apply D8T (implicit in row calculation).
;
;	tmp[0] = ev0*LAMBDA(4);
;	tmp[1] = ev2*LAMBDA(4);
;	tmp[2] = ev1*LAMBDA(2);
;	tmp[3] = ev3*LAMBDA(2);
;	tmp[4] = od0*LAMBDA(1);
;	tmp[5] = od3*LAMBDA(1);
;	tmp[6] = od1*LAMBDA(3);
;	tmp[7] = od2*LAMBDA(3);
;
;	// 4) Apply B8T.
;
;	double x0, x1, x2, x3, y0, y1, y2, y3;
;
;	x0 = tmp[0] + tmp[1];
;	x1 = tmp[0] - tmp[1];
;	x2 = tmp[2] + TAN(2)*tmp[3];
;	x3 = tmp[2]*TAN(2) - tmp[3];
;	y0 = tmp[4] + TAN(1)*tmp[5];
;	y1 = tmp[4]*TAN(1) - tmp[5];
;	y2 = tmp[6] + TAN(3)*tmp[7];
;	y3 = tmp[6]*TAN(3) - tmp[7];
;
;	// 3) Apply E8T.
;	//
;	//	1  0  1  0
;	//	0  1  0  1
;	//	0  1  0 -1
;	//	1  0 -1  0
;	//		    1  0  1  0
;	//		    1  0 -1  0
;	//		    0  1  0  1
;	//		    0  1  0 -1
;
;	double e0, e1, e2, e3, o0, o1, o2, o3;
;
;	e0 = x0 + x2;
;	e1 = x1 + x3;
;	e2 = x1 - x3;
;	e3 = x0 - x2;
;	o0 = y0 + y2;
;	o1 = y0 - y2;
;	o2 = y1 + y3;
;	o3 = y1 - y3;
;
;	// 2) Apply F8T.
;
;	double a, b;
;
;	a = (o1 + o2) * LAMBDA(4);
;	b = (o1 - o2) * LAMBDA(4);
;
;	o1 = a;
;	o2 = b;
;
;	// 6) Apply output butterfly (A8T).
;	//
;	// 1 0 0 0  1  0  0  0
;	// 0 1 0 0  0  1  0  0
;	// 0 0 1 0  0  0  1  0
;	// 0 0 0 1  0  0  0  1
;	// 0 0 0 1  0  0  0 -1
;	// 0 0 1 0  0  0 -1  0
;	// 0 1 0 0  0 -1  0  0
;	// 1 0 0 0 -1  0  0  0
;
;	out[0] = e0 + o0;
;	out[1] = e1 + o1;
;	out[2] = e2 + o2;
;	out[3] = e3 + o3;
;	out[4] = e3 - o3;
;	out[5] = e2 - o2;
;	out[6] = e1 - o1;
;	out[7] = e0 - o0;
;
;=============================================================================
%macro DCT_8_INV_COL_4 2
%define	.inp	%1
%define	.out	%2

%define .x0	[.inp + 0*16]
%define .x1	[.inp + 1*16]
%define .x2	[.inp + 2*16]
%define .x3	[.inp + 3*16]
%define .x4	[.inp + 4*16]
%define .x5	[.inp + 5*16]
%define .x6	[.inp + 6*16]
%define .x7	[.inp + 7*16]
%define .y0	[.out + 0*16]
%define .y1	[.out + 1*16]
%define .y2	[.out + 2*16]
%define .y3	[.out + 3*16]
%define .y4	[.out + 4*16]
%define .y5	[.out + 5*16]
%define .y6	[.out + 6*16]
%define .y7	[.out + 7*16]

	;======= optimized code

	;ODD ELEMENTS

	movq	mm0, [tg_1_16]

	movq	mm2, [tg_3_16]
	movq	mm3,mm0

	movq	mm1,.x7
	movq	mm6,mm2

	movq	mm4,.x5
	pmulhw	mm0,mm1

	movq	mm5,.x1
	pmulhw	mm2,mm4

	movq	mm7,.x3
	pmulhw	mm3,mm5

	pmulhw	mm6,mm7
	paddw	mm0,mm5

	paddw	mm2,mm4
	psubw	mm6,mm4

	paddw	mm2,mm7
	psubw	mm3,mm1

	paddw	mm0, [one_corr]
	paddw	mm6,mm7

	;E8T butterfly - odd elements

	movq	mm1,mm0
	movq	mm5,mm3
	paddw	mm0,mm2		;mm0 = o0 = y0 + y2
	psubw	mm1,mm2		;mm1 = o1 = y0 - y2
	paddw	mm3,mm6		;mm3 = o2 = y1 + y3
	psubw	mm5,mm6		;mm5 = o3 = y1 - y3

	;F8T stage - odd elements

	movq	mm2,mm1
	paddw	mm1,mm3			;[F8T] mm1 = o1 + o2

	movq	mm4,.x0			;[B8T1] mm3 = tmp[0]
	psubw	mm2,mm3			;[F8T] mm2 = o1 - o2

	movq	mm3,.x4
	movq	mm6,mm2			;[F8T]

	pmulhw	mm2, [cos_4_16]	;[F8T]
	movq	mm7,mm1			;[F8T]

	pmulhw	mm1, [cos_4_16]	;[F8T]
	paddw	mm3,mm4			;[B8T1] mm3 = x0 = tmp[0] + tmp[1]

	paddw	mm3, [round_inv_corr]	;[E8T]
	;<v-pipe>

	psubw	mm4,.x4			;[B8T1] mm4 = x1 = tmp[0] - tmp[1]
	paddw	mm2,mm6			;[F8T]

	por	mm1, [one_corr]	;[F8T]
	;<v-pipe>

	movq	mm6,.x6			;[B8T2] mm7 = tmp[3]
	paddw	mm1,mm7			;[F8T] mm1 = o1' = (o1 + o2)*LAMBDA(4)

	pmulhw	mm6, [tg_2_16]	;mm7 = tmp[3] * TAN(2)
	;<v-pipe>

	movq	mm7, [one_corr]
	;<v-pipe>

	paddw	mm4, [round_inv_col]	;[out]
	psubw	mm2,mm7			;[F8T] mm2 = o2' = (o1 - o2)*LAMBDA(4)

	paddw	mm6,.x2			;[B8T2] mm7 = x2 = tmp[2] + tmp[3]*TAN(2)
	paddw	mm7,mm3			;[E8T]

	paddw	mm7,mm6			;[E8T] mm6 = e0 = x0+x2
	psubw	mm3,mm6			;[E8T] mm3 = e3 = x0-x2

	;output butterfly - 0 and 3

	movq	mm6,mm7		;mm7 = e0
	paddw	mm7,mm0		;mm6 = e0 + o0

	psubw	mm6,mm0		;mm7 = e0 - o0
	psraw	mm7,SHIFT_INV_COL

	movq	mm0,mm3		;mm7 = e3 
	psraw	mm6,SHIFT_INV_COL

	movq	.y0,mm7
	paddw	mm3,mm5		;mm6 = e3 + o3

	movq	mm7,.x2		;[B8T] mm6 = tmp[2]
	psubw	mm0,mm5		;[out] mm7 = e3 - o3

	movq	.y7,mm6
	psraw	mm3,SHIFT_INV_COL

	pmulhw	mm7, [tg_2_16]		;[B8T] mm6 = tmp[2] * TAN(2)
	psraw	mm0,SHIFT_INV_COL

	movq	.y3,mm3
	movq	mm6,mm4				;[E8T]

	psubw	mm6, [one_corr]
	;<v-pipe>


	;B8T stage - x3 element
	;
	;free registers: 03567

	psubw	mm7,.x6				;[B8T] mm6 = x3 = tmp[2]*TAN(2) - tmp[3]
	movq	mm3,mm1

	;E8T stage - x1 and x3 elements

	movq	.y4,mm0
	paddw	mm4,mm7				;[E8T] mm4 = e1 = x1+x3

	psubw	mm6,mm7				;[E8T] mm7 = e2 = x1-x3
	paddw	mm3,mm4				;mm3 = e1 + o1

	psubw	mm4,mm1				;mm4 = e1 - o1
	psraw	mm3,SHIFT_INV_COL

	movq	mm5,mm6				;mm6 = e2
	psraw	mm4,SHIFT_INV_COL

	paddw	mm6,mm2				;mm7 = e2 + o2
	psubw	mm5,mm2				;mm6 = e2 - o2

	movq	.y1,mm3
	psraw	mm6,SHIFT_INV_COL

	movq	.y6,mm4
	psraw	mm5,SHIFT_INV_COL

	movq	.y2,mm6

	movq	.y5,mm5

%endmacro

;-----------------------------------------------------------------------------
;
; The half 8-point scaled inverse DCT algorithm (26a8m)
;
;-----------------------------------------------------------------------------
;
;	// Reorder and prescale (implicit)
;
;	ev0 = co[0] / 2.0;
;	ev1 = co[2] / 2.0;
;	od0 = co[1] / 2.0;
;	od1 = co[3] / 2.0;
;
;	// 5) Apply D8T (implicit in row calculation).
;
;	tmp[0] = ev0*LAMBDA(4);
;	tmp[2] = ev1*LAMBDA(2);
;	tmp[4] = od0*LAMBDA(1);
;	tmp[6] = od1*LAMBDA(3);
;
;	// 4) Apply B8T.
;
;	double x0, x1, x2, x3, y0, y1, y2, y3;
;
;	x0 = tmp[0];
;	x1 = tmp[0];
;	x2 = tmp[2];
;	x3 = tmp[2]*TAN(2);
;	y0 = tmp[4];
;	y1 = tmp[4]*TAN(1);
;	y2 = tmp[6];
;	y3 = tmp[6]*TAN(3);
;
;	// 3) Apply E8T.
;	//
;	//	1  0  1  0
;	//	0  1  0  1
;	//	0  1  0 -1
;	//	1  0 -1  0
;	//		    1  0  1  0
;	//		    1  0 -1  0
;	//		    0  1  0  1
;	//		    0  1  0 -1
;
;	double e0, e1, e2, e3, o0, o1, o2, o3;
;
;	e0 = x0 + x2;
;	e1 = x1 + x3;
;	e2 = x1 - x3;
;	e3 = x0 - x2;
;	o0 = y0 + y2;
;	o1 = y0 - y2;
;	o2 = y1 + y3;
;	o3 = y1 - y3;
;
;	// 2) Apply F8T.
;
;	double a, b;
;
;	a = (o1 + o2) * LAMBDA(4);
;	b = (o1 - o2) * LAMBDA(4);
;
;	o1 = a;
;	o2 = b;
;
;	// 6) Apply output butterfly (A8T).
;	//
;	// 1 0 0 0  1  0  0  0
;	// 0 1 0 0  0  1  0  0
;	// 0 0 1 0  0  0  1  0
;	// 0 0 0 1  0  0  0  1
;	// 0 0 0 1  0  0  0 -1
;	// 0 0 1 0  0  0 -1  0
;	// 0 1 0 0  0 -1  0  0
;	// 1 0 0 0 -1  0  0  0
;
;	out[0] = e0 + o0;
;	out[1] = e1 + o1;
;	out[2] = e2 + o2;
;	out[3] = e3 + o3;
;	out[4] = e3 - o3;
;	out[5] = e2 - o2;
;	out[6] = e1 - o1;
;	out[7] = e0 - o0;
;
;=============================================================================

%macro DCT_8_INV_COL_4_SHORT 2
%define	.inp	%1
%define	.out	%2
%define .x0	[.inp + 0*16]
%define .x1	[.inp + 1*16]
%define .x2	[.inp + 2*16]
%define .x3	[.inp + 3*16]
%define .y0	[.out + 0*16]
%define .y1	[.out + 1*16]
%define .y2	[.out + 2*16]
%define .y3	[.out + 3*16]
%define .y4	[.out + 4*16]
%define .y5	[.out + 5*16]
%define .y6	[.out + 6*16]
%define .y7	[.out + 7*16]

	;======= optimized code

	;ODD ELEMENTS

	movq	mm3, [tg_1_16]

	movq	mm0,.x1
	movq	mm6, [tg_3_16]

	movq	mm2,.x3
	pmulhw	mm3,mm0

	pmulhw	mm6,mm2

	paddw	mm0, [one_corr]
	paddw	mm6,mm2

	;E8T butterfly - odd elements

	movq	mm1,mm0
	movq	mm5,mm3
	paddw	mm0,mm2		;mm0 = o0 = y0 + y2
	psubw	mm1,mm2		;mm1 = o1 = y0 - y2
	paddw	mm3,mm6		;mm3 = o2 = y1 + y3
	psubw	mm5,mm6		;mm5 = o3 = y1 - y3

	;F8T stage - odd elements

	movq	mm2,mm1
	paddw	mm1,mm3					;[F8T] mm1 = o1 + o2

	movq	mm4,.x0					;[B8T1] mm4 = x0 = x1 = tmp[0]
	psubw	mm2,mm3					;[F8T] mm2 = o1 - o2

	movq	mm6,mm2					;[F8T]
	movq	mm7,mm1					;[F8T]

	pmulhw	mm2, [cos_4_16]	;[F8T]
	movq	mm3,mm4					;[B8T1] mm3 = x0 = x1 = tmp[0]

	pmulhw	mm1, [cos_4_16]	;[F8T]

	paddw	mm3, [round_inv_corr]	;[E8T]
	;<v-pipe>

	paddw	mm4, [round_inv_col]	;[out]
	paddw	mm2,mm6					;[F8T]

	por		mm1, [one_corr]			;[F8T]
	;<v-pipe>

	psubw	mm2, [one_corr]			;[F8T] mm2 = o2' = (o1 - o2)*LAMBDA(4)
	paddw	mm1,mm7					;[F8T] mm1 = o1' = (o1 + o2)*LAMBDA(4)

	movq	mm6,.x2					;[B8T2] mm7 = x2 = tmp[2]
	movq	mm7,mm4					;[E8T]

	paddw	mm7,mm6					;[E8T] mm6 = e0 = x0+x2
	psubw	mm3,mm6					;[E8T] mm3 = e3 = x0-x2

	;output butterfly - 0 and 3

	movq	mm6,mm7		;mm7 = e0
	paddw	mm7,mm0		;mm6 = e0 + o0

	psubw	mm6,mm0		;mm7 = e0 - o0
	psraw	mm7,SHIFT_INV_COL

	movq	mm0,mm3		;mm7 = e3 
	psraw	mm6,SHIFT_INV_COL

	movq	.y0,mm7
	paddw	mm3,mm5		;mm6 = e3 + o3

	movq	mm7,.x2		;[B8T] mm6 = tmp[2]
	psubw	mm0,mm5		;[out] mm7 = e3 - o3

	movq	.y7,mm6
	psraw	mm3,SHIFT_INV_COL

	pmulhw	mm7, [tg_2_16]		;[B8T] mm6 = x3 = tmp[2] * TAN(2)
	psraw	mm0,SHIFT_INV_COL

	movq	.y3,mm3
	movq	mm6,mm4				;[E8T]

	psubw	mm6, [one_corr]
	;<v-pipe>


	;B8T stage - x3 element
	;
	;free registers: 03567

	movq	mm3,mm1

	;E8T stage - x1 and x3 elements

	movq	.y4,mm0
	paddw	mm4,mm7				;[E8T] mm4 = e1 = x1+x3

	psubw	mm6,mm7				;[E8T] mm7 = e2 = x1-x3
	paddw	mm3,mm4				;mm3 = e1 + o1

	psubw	mm4,mm1				;mm4 = e1 - o1
	psraw	mm3,SHIFT_INV_COL

	movq	mm5,mm6				;mm6 = e2
	psraw	mm4,SHIFT_INV_COL

	paddw	mm6,mm2				;mm7 = e2 + o2
	psubw	mm5,mm2				;mm6 = e2 - o2

	movq	.y1,mm3
	psraw	mm6,SHIFT_INV_COL

	movq	.y6,mm4
	psraw	mm5,SHIFT_INV_COL

	movq	.y2,mm6

	movq	.y5,mm5
%endmacro

	segment	.text

	align	16
dorow_7:	DCT_8_INV_ROW_1		eax+7*16, eax+7*16, tab_i_17
dorow_6:	DCT_8_INV_ROW_1		eax+6*16, eax+6*16, tab_i_26
dorow_5:	DCT_8_INV_ROW_1		eax+5*16, eax+5*16, tab_i_35
dorow_4:	DCT_8_INV_ROW_1		eax+4*16, eax+4*16, tab_i_04
dorow_3:	DCT_8_INV_ROW_1		eax+3*16, eax+3*16, tab_i_35
dorow_2:	DCT_8_INV_ROW_1		eax+2*16, eax+2*16, tab_i_26
dorow_1:	DCT_8_INV_ROW_1		eax+1*16, eax+1*16, tab_i_17
dorow_0:	DCT_8_INV_ROW_1		eax+0*16, eax+0*16, tab_i_04


	%if PROFILE
	rdtsc
	sub	eax,dword ptr last_tick
	mov	edx,dword ptr total_tick2
	cmp	eax,edx
	cmovl	edx,eax
	mov	dword ptr total_tick2,edx
	rdtsc
	mov	dword ptr last_tick,eax
	mov	eax,[esp+4]
	%endif

	mov	ecx,2
lup:
	DCT_8_INV_COL_4		eax, eax
	add	eax,8
	dec	ecx
	jne	lup

	%if PROFILE
	rdtsc
	sub	eax, [last_tick]
	mov	edx, [total_tick]
	cmp	eax,edx
	cmovl	edx,eax
	mov	[total_tick], edx

	pushad
	dec	[total_cnt]
	jnz	nodump
	mov	[total_cnt], 65536*16

	mov	[total_tick], 07fffffffh

	sub	esp,256
	push	edx

	mov	eax, [total_tick2]
	mov	[total_tick2], 07fffffffh

	push	eax

	push	profile_str
	lea		eax,[esp+20]
	push	eax
	call	_sprintf
	lea		eax,[esp+24]
	push	eax
	call	_OutputDebugStringA@4
	add		esp,256+16
nodump:
	popad
	%endif

	mov	edx,[esp+16]
	mov	eax,[esp+4]
	mov	ecx,[esp+8]
	jmp	dword [jump_tab + edx*4]

	global _IDCT_mmx	
_IDCT_mmx:
	%if PROFILE
	rdtsc
	mov		[last_tick], eax
	%endif

	mov		ecx,[esp+20]
	movzx	ecx,byte [pos_tab+ecx]

	mov		eax,[esp+4]
	jmp		dword [rowstart_tbl+ecx]




	global _IDCT_isse	
_IDCT_isse:
	%if PROFILE
	rdtsc
	mov		[last_tick], eax
	%endif

	mov		ecx,[esp+20]
	movzx	ecx,byte [pos_tab+ecx]

	mov	eax,[esp+4]
	jmp	dword [rowstart_tbl2+ecx]

	align	16
dorow_3is:
	prefetcht0	[tab_i_26_short]
	prefetcht0	[eax+2*16]
	DCT_8_INV_ROW_1_ISSE_SHORT	eax+3*16, eax+3*16, tab_i_35_short
;	DCT_8_INV_ROW_1_ISSE	eax+3*16, eax+3*16, tab_i_35
dorow_2is:
	prefetcht0	[tab_i_17_short]
	prefetcht0	[eax+1*16]
	DCT_8_INV_ROW_1_ISSE_SHORT	eax+2*16, eax+2*16, tab_i_26_short
;	DCT_8_INV_ROW_1_ISSE	eax+2*16, eax+2*16, tab_i_26
dorow_1is:
	prefetcht0	[tab_i_04_short]
	prefetcht0	[eax+0*16]
	DCT_8_INV_ROW_1_ISSE_SHORT	eax+1*16, eax+1*16, tab_i_17_short
;	DCT_8_INV_ROW_1_ISSE	eax+1*16, eax+1*16, tab_i_17
dorow_0is:
	DCT_8_INV_ROW_1_ISSE_SHORT	eax+0*16, eax+0*16, tab_i_04_short
;	DCT_8_INV_ROW_1_ISSE	eax+0*16, eax+0*16, tab_i_04

	mov	ecx,2
lup3:
	DCT_8_INV_COL_4_SHORT		eax, eax
	add	eax,8
	dec	ecx
	jne	lup3
	mov	edx,[esp+16]
	mov	eax,[esp+4]
	mov	ecx,[esp+8]
	jmp	dword [jump_tab + edx*4]

	align	16
dorow_7is:
		prefetcht0	[tab_i_26]
		prefetcht0	[tab_i_26+63]
		prefetcht0	[eax+6*16]
		DCT_8_INV_ROW_1_ISSE	eax+7*16, eax+7*16, tab_i_17
dorow_6is:
		prefetcht0	[tab_i_35]
		prefetcht0	[tab_i_35+63]
		prefetcht0	[eax+5*16]
		DCT_8_INV_ROW_1_ISSE	eax+6*16, eax+6*16, tab_i_26
dorow_5is:
		prefetcht0	[tab_i_04]
		prefetcht0	[tab_i_04+63]
		prefetcht0	[eax+4*16]
		DCT_8_INV_ROW_1_ISSE	eax+5*16, eax+5*16, tab_i_35
dorow_4is:
		prefetcht0	[tab_i_35]
		prefetcht0	[tab_i_35+63]
		prefetcht0	[eax+3*16]
		DCT_8_INV_ROW_1_ISSE	eax+4*16, eax+4*16, tab_i_04
;dorow_3is:
		prefetcht0	[tab_i_26]
		prefetcht0	[tab_i_26+63]
		prefetcht0	[eax+2*16]
		DCT_8_INV_ROW_1_ISSE	eax+3*16, eax+3*16, tab_i_35
;dorow_2is:
		prefetcht0	[tab_i_17]
		prefetcht0	[tab_i_17+63]
		prefetcht0	[eax+1*16]
		DCT_8_INV_ROW_1_ISSE	eax+2*16, eax+2*16, tab_i_26
;dorow_1is:
		prefetcht0	[tab_i_04]
		prefetcht0	[tab_i_04+63]
		prefetcht0	[eax+0*16]
		DCT_8_INV_ROW_1_ISSE	eax+1*16, eax+1*16, tab_i_17
;dorow_0is:
		DCT_8_INV_ROW_1_ISSE	eax+0*16, eax+0*16, tab_i_04

	%if PROFILE
		rdtsc
		sub		eax, [last_tick]
		mov		edx, [total_tick2]
		cmp		eax, edx
		cmovl	edx, eax
		mov		[total_tick2], edx
		rdtsc
		mov		[last_tick], eax
		mov		eax, [esp+4]
	%endif

	mov	ecx,2
lup2:
	DCT_8_INV_COL_4		eax, eax
	add	eax,8
	dec	ecx
	jne	lup2

	%if PROFILE
		rdtsc
		sub		eax, [last_tick]
		mov		edx, [total_tick]
		cmp		eax,edx
		cmovl	edx,eax
		mov		[total_tick], edx

		pushad
		dec		[total_cnt]
		jnz		nodump2
		mov		[total_cnt], 65536*16

		mov		[total_tick], 07fffffffh

		sub		esp,256
		push	edx

		mov		eax, [total_tick2]
		mov		[total_tick2], 07fffffffh

		push	eax

		push	offset profile_str
		lea		eax,[esp+20]
		push	eax
		call	_sprintf
		lea		eax,[esp+24]
		push	eax
		call	_OutputDebugStringA@4
		add		esp,256+16
	nodump2:
		popad
	%endif

	mov	edx,[esp+16]
	mov	eax,[esp+4]
	mov	ecx,[esp+8]
	jmp	dword [jump_tab + edx*4]




	align 16

tail_intra:
	mov		edx,-8*16
	pxor		mm7,mm7
intra_loop:
	movq		mm0,[eax+edx+8*16]
	movq		mm1,[eax+edx+8*16+8]
	packuswb	mm0,mm1
	movq		[ecx],mm0
	add		ecx,[esp+12]
	add		edx,16
	jne		intra_loop
tail_mjpeg:
	ret

	align		16
tail_inter:
	mov		edx,-8*16
	pxor		mm7,mm7
inter_loop:
	movq		mm0,[eax+edx+8*16]
	movq		mm1,[eax+edx+8*16+8]
	movq		mm2,[ecx]
	movq		mm3,mm2
	punpcklbw	mm2,mm7
	punpckhbw	mm3,mm7
	paddw		mm0,mm2
	paddw		mm1,mm3
	packuswb	mm0,mm1
	movq		[ecx],mm0
	add		ecx,[esp+12]
	add		edx,16
	jne		inter_loop
	ret

;--------------------------------------------------------------------------

	align		16

do_dc_mmx:
	movd		mm0,dword [eax]
	pxor		mm1,mm1
	mov			edx,[esp+16]
	punpcklwd	mm0,mm0
	mov			ecx,[esp+8]		;ecx = dst
	punpckldq	mm0,mm0
	cmp			edx, 1
	paddw		mm0, [rounder]
	mov			eax,[esp+12]		;eax = pitch
	psraw		mm0,3
	jb			do_ac_mmx
	ja			do_dc_mjpeg
	packuswb	mm0,mm0

	movq		[ecx],mm0		;row 0
	lea			edx,[eax+eax*2]
	movq		[ecx+eax],mm0		;row 1
	add			edx,ecx
	movq		[ecx+eax*2],mm0		;row 2
	lea			ecx,[ecx+eax*2]
	movq		[edx],mm0		;row 3
	movq		[ecx+eax*2],mm0		;row 4
	movq		[edx+eax*2],mm0		;row 5
	movq		[ecx+eax*4],mm0		;row 6
	movq		[edx+eax*4],mm0		;row 7
	ret

	align		16
do_ac_mmx:
	psubw		mm1,mm0
	packuswb	mm0,mm0			;mm0 = adder
	packuswb	mm1,mm1			;mm1 = subtractor

	mov		edx,8
do_ac_mmx.loop:
	movq		mm2,[ecx]
	paddusb		mm2,mm0
	psubusb		mm2,mm1
	movq		[ecx],mm2
	add		ecx,eax
	dec		edx
	jne		do_ac_mmx.loop
	ret

;--------------------------------------------------------------------------

do_dc_isse:
	pshufw		mm0,[eax],0
	pxor		mm1,mm1
	mov			edx,[esp+16]
	paddw		mm0, [rounder]
	mov			ecx,[esp+8]		;ecx = dst
	psraw		mm0,3
	cmp			edx, 1
	mov			eax,[esp+12]		;eax = pitch
	jb			do_ac_isse
	ja			do_dc_mjpeg
	packuswb	mm0,mm0

	movq		[ecx],mm0		;row 0
	lea			edx,[eax+eax*2]
	movq		[ecx+eax],mm0		;row 1
	add			edx,ecx
	movq		[ecx+eax*2],mm0		;row 2
	lea			ecx,[ecx+eax*2]
	movq		[edx],mm0		;row 3
	movq		[ecx+eax*2],mm0		;row 4
	movq		[edx+eax*2],mm0		;row 5
	movq		[ecx+eax*4],mm0		;row 6
	movq		[edx+eax*4],mm0		;row 7
	ret

	align		16
do_ac_isse:
	psubw		mm1,mm0
	packuswb	mm0,mm0			;mm0 = adder
	packuswb	mm1,mm1			;mm1 = subtractor

	mov		edx,8
do_ac_isse.loop:
	movq		mm2,[ecx]
	paddusb		mm2,mm0
	psubusb		mm2,mm1
	movq		[ecx],mm2
	add		ecx,eax
	dec		edx
	jne		do_ac_mmx.loop
	ret

do_dc_mjpeg:
	movq		[ecx],mm0		;row 0
	movq		[ecx+8],mm0		;row 0
	lea		edx,[eax+eax*2]
	movq		[ecx+eax],mm0		;row 1
	movq		[ecx+eax+8],mm0		;row 1
	add		edx,ecx
	movq		[ecx+eax*2],mm0		;row 2
	movq		[ecx+eax*2+8],mm0	;row 2
	lea		ecx,[ecx+eax*2]
	movq		[edx],mm0		;row 3
	movq		[edx+8],mm0		;row 3
	movq		[ecx+eax*2],mm0		;row 4
	movq		[ecx+eax*2+8],mm0	;row 4
	movq		[edx+eax*2],mm0		;row 5
	movq		[edx+eax*2+8],mm0	;row 5
	movq		[ecx+eax*4],mm0		;row 6
	movq		[ecx+eax*4+8],mm0	;row 6
	movq		[edx+eax*4],mm0		;row 7
	movq		[edx+eax*4+8],mm0	;row 7
	ret

	end
