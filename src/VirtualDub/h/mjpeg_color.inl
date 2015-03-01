DECLARE_MJPEG_COLOR_CONVERTER(422_RGB15) {
	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax,[esp+8+16]		;ecx = Y coeff
		mov			edx,[esp+4+16]		;edx = dst
		mov			esi,[esp+12+16]		;esi = dst_pitch

		movq		mm6,mask5
		pxor		mm7,mm7
		mov			edi,[esp+16+16]		;edi = lines
yloop2:
		movd		mm5,[eax+256]		;Cb (0,1)

		movd		mm3,[eax+384]		;Cr (0,1)
		movq		mm4,mm5				;Cb [duplicate]

		movq		mm0,[eax]			;Y (0,1,2,3)
		punpcklwd	mm5,mm5				;Cb [subsampling]

		pmullw		mm5,Cb_coeff_B		;Cb [produce blue impacts]
		punpcklwd	mm4,mm3				;mm4: [Cr1][Cb1][Cr0][Cb0]

		pmaddwd		mm4,CrCb_coeff_G
		punpcklwd	mm3,mm3				;Cr [subsampling]

		pmullw		mm3,Cr_coeff_R		;Cr [produce red impacts]
		psllw		mm0,6

		paddw		mm5,mm0				;B (0,1,2,3)

		packssdw	mm4,mm4				;green impacts

		punpcklwd	mm4,mm4

		paddw		mm3,mm0				;R (0,1,2,3)
		psraw		mm5,6

		paddw		mm4,mm0				;G (0,1,2,3)
		psraw		mm3,6

		movq		[eax],mm7
		packuswb	mm3,mm3

		psraw		mm4,4
		pand		mm3,mm6

		paddsw		mm4,G_const_1
		packuswb	mm5,mm5

		pand		mm5,mm6
		psrlq		mm3,1

		psubusw		mm4,G_const_2
		psrlq		mm5,3

		pand		mm4,G_const_3
		punpcklbw	mm5,mm3

		por			mm5,mm4

		MOVNTQ		[edx],mm5




		movd		mm5,[eax+256+4]			;Cb (0,1)

		movd		mm3,[eax+384+4]		;Cr (0,1)
		movq		mm4,mm5				;Cb [duplicate]

		movq		mm0,[eax+8]			;Y (0,1,2,3)
		punpcklwd	mm5,mm5				;Cb [subsampling]

		pmullw		mm5,Cb_coeff_B		;Cb [produce blue impacts]
		punpcklwd	mm4,mm3				;mm4: [Cr1][Cb1][Cr0][Cb0]

		pmaddwd		mm4,CrCb_coeff_G
		punpcklwd	mm3,mm3				;Cr [subsampling]

		pmullw		mm3,Cr_coeff_R		;Cr [produce red impacts]
		psllw		mm0,6

		movq		[eax+256],mm7
		paddw		mm5,mm0				;B (0,1,2,3)

		movq		[eax+384],mm7
		packssdw	mm4,mm4				;green impacts

		punpcklwd	mm4,mm4

		paddw		mm3,mm0				;R (0,1,2,3)
		psraw		mm5,6

		paddw		mm4,mm0				;G (0,1,2,3)
		psraw		mm3,6

		movq		[eax+8],mm7
		packuswb	mm3,mm3

		psraw		mm4,4
		pand		mm3,mm6

		paddsw		mm4,G_const_1
		packuswb	mm5,mm5

		pand		mm5,mm6
		psrlq		mm3,1

		psubusw		mm4,G_const_2
		psrlq		mm5,3

		pand		mm4,G_const_3
		punpcklbw	mm5,mm3

		por			mm5,mm4

		MOVNTQ		[edx+8],mm5




		movd		mm5,[eax+256+8]		;Cb (0,1)

		movd		mm3,[eax+384+8]		;Cr (0,1)
		movq		mm4,mm5				;Cb [duplicate]

		movq		mm0,[eax+128]		;Y (0,1,2,3)
		punpcklwd	mm5,mm5				;Cb [subsampling]

		pmullw		mm5,Cb_coeff_B		;Cb [produce blue impacts]
		punpcklwd	mm4,mm3				;mm4: [Cr1][Cb1][Cr0][Cb0]

		pmaddwd		mm4,CrCb_coeff_G
		punpcklwd	mm3,mm3				;Cr [subsampling]

		pmullw		mm3,Cr_coeff_R		;Cr [produce red impacts]
		psllw		mm0,6

		paddw		mm5,mm0				;B (0,1,2,3)

		packssdw	mm4,mm4				;green impacts

		punpcklwd	mm4,mm4

		paddw		mm3,mm0				;R (0,1,2,3)
		psraw		mm5,6

		paddw		mm4,mm0				;G (0,1,2,3)
		psraw		mm3,6

		movq		[eax+128],mm7
		packuswb	mm3,mm3

		psraw		mm4,4
		pand		mm3,mm6

		paddsw		mm4,G_const_1
		packuswb	mm5,mm5

		pand		mm5,mm6
		psrlq		mm3,1

		psubusw		mm4,G_const_2
		psrlq		mm5,3

		pand		mm4,G_const_3
		punpcklbw	mm5,mm3

		por			mm5,mm4

		MOVNTQ		[edx+16],mm5



		movd		mm5,[eax+256+12]	;Cb (0,1)

		movd		mm3,[eax+384+12]	;Cr (0,1)
		movq		mm4,mm5				;Cb [duplicate]

		movq		mm0,[eax+128+8]		;Y (0,1,2,3)
		punpcklwd	mm5,mm5				;Cb [subsampling]

		pmullw		mm5,Cb_coeff_B		;Cb [produce blue impacts]
		punpcklwd	mm4,mm3				;mm4: [Cr1][Cb1][Cr0][Cb0]

		pmaddwd		mm4,CrCb_coeff_G
		punpcklwd	mm3,mm3				;Cr [subsampling]

		pmullw		mm3,Cr_coeff_R		;Cr [produce red impacts]
		psllw		mm0,6

		movq		[eax+256+8],mm7
		paddw		mm5,mm0				;B (0,1,2,3)

		movq		[eax+384+8],mm7
		packssdw	mm4,mm4				;green impacts

		punpcklwd	mm4,mm4

		paddw		mm3,mm0				;R (0,1,2,3)
		psraw		mm5,6

		paddw		mm4,mm0				;G (0,1,2,3)
		psraw		mm3,6

		movq		[eax+128+8],mm7
		packuswb	mm3,mm3

		psraw		mm4,4
		pand		mm3,mm6

		paddsw		mm4,G_const_1
		packuswb	mm5,mm5

		pand		mm5,mm6
		psrlq		mm3,1

		psubusw		mm4,G_const_2
		psrlq		mm5,3

		pand		mm4,G_const_3
		punpcklbw	mm5,mm3

		por			mm5,mm4

		MOVNTQ		[edx+24],mm5

		add			eax,16
		add			edx,esi

		dec			edi
		jne			yloop2

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret			16
	}
}

DECLARE_MJPEG_COLOR_CONVERTER(422_RGB32) {
	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax,[esp+8+16]		;eax = dct_coeff
		mov			edx,[esp+4+16]		;edx = dst
		mov			esi,[esp+12+16]		;esi = dstpitch
		mov			ebx,[esp+16+16]		;ebx = lines
yloop:
		movd		mm4,[eax+384]	;Cr (0,1)
		pxor		mm3,mm3

		movd		mm6,[eax+256]	;Cb (0,1)
		punpcklwd	mm4,mm4

		movq		mm0,[eax]		;Y (0,1,2,3)
		punpcklwd	mm6,mm6

		psllw		mm0,6
		movq		mm5,mm4

		movq		mm7,mm6
		punpckldq	mm4,mm4		;Cr (0,0,0,0)

		pmullw		mm4,Cr_coeff
		punpckhdq	mm5,mm5		;Cr (1,1,1,1)

		movq		mm2,mm0
		punpcklwd	mm0,mm0			;Y (0,0,1,1)

		pmullw		mm5,Cr_coeff
		punpckhwd	mm2,mm2			;Y (2,2,3,3)

		movq		mm1,mm0
		punpckldq	mm0,mm0			;Y (0,0,0,0)

		movq		[eax],mm3
		punpckhdq	mm1,mm1			;Y (1,1,1,1)

		movq		mm3,mm2
		punpckldq	mm6,mm6		;Cb (0,0,0,0)

		pmullw		mm6,Cb_coeff
		punpckhdq	mm7,mm7		;Cb (1,1,1,1)

		pmullw		mm7,Cb_coeff
		punpckhdq	mm3,mm3			;Y (3,3,3,3)

		punpckldq	mm2,mm2			;Y (2,2,2,2)

		paddsw		mm4,mm6
		paddsw		mm0,mm4

		paddsw		mm5,mm7
		psraw		mm0,6

		paddsw		mm1,mm4
		paddsw		mm2,mm5

		psraw		mm1,6
		paddsw		mm3,mm5

		psraw		mm2,6
		packuswb	mm0,mm1

		psraw		mm3,6
		packuswb	mm2,mm3

		MOVNTQ		[edx],mm0
		MOVNTQ		[edx+8],mm2




		movd		mm4,[eax+384+4]	;Cr (0,1)
		pxor		mm3,mm3

		movd		mm6,[eax+256+4]	;Cb (0,1)
		punpcklwd	mm4,mm4

		movq		mm0,[eax+8]		;Y (0,1,2,3)
		punpcklwd	mm6,mm6

		movq		[eax+384],mm3
		psllw		mm0,6

		movq		[eax+256],mm3
		movq		mm5,mm4

		movq		mm7,mm6
		punpckldq	mm4,mm4		;Cr (0,0,0,0)

		pmullw		mm4,Cr_coeff
		punpckhdq	mm5,mm5		;Cr (1,1,1,1)

		movq		mm2,mm0
		punpcklwd	mm0,mm0			;Y (0,0,1,1)

		pmullw		mm5,Cr_coeff
		punpckhwd	mm2,mm2			;Y (2,2,3,3)

		movq		mm1,mm0
		punpckldq	mm0,mm0			;Y (0,0,0,0)

		movq		[eax+8],mm3	
		punpckhdq	mm1,mm1			;Y (1,1,1,1)

		movq		mm3,mm2
		punpckldq	mm6,mm6		;Cb (0,0,0,0)

		pmullw		mm6,Cb_coeff
		punpckhdq	mm7,mm7		;Cb (1,1,1,1)

		pmullw		mm7,Cb_coeff
		punpckhdq	mm3,mm3			;Y (3,3,3,3)

		punpckldq	mm2,mm2			;Y (2,2,2,2)

		paddsw		mm4,mm6
		paddsw		mm0,mm4

		paddsw		mm5,mm7
		psraw		mm0,6

		paddsw		mm1,mm4
		paddsw		mm2,mm5

		psraw		mm1,6
		paddsw		mm3,mm5

		psraw		mm2,6
		packuswb	mm0,mm1

		psraw		mm3,6
		packuswb	mm2,mm3

		MOVNTQ		[edx+16],mm0
		MOVNTQ		[edx+24],mm2





		movd		mm4,[eax+384+8]	;Cr (0,1)
		pxor		mm3,mm3

		movd		mm6,[eax+256+8]	;Cb (0,1)
		punpcklwd	mm4,mm4

		movq		mm0,[eax+128]	;Y (0,1,2,3)
		punpcklwd	mm6,mm6

		psllw		mm0,6
		movq		mm5,mm4

		movq		mm7,mm6
		punpckldq	mm4,mm4		;Cr (0,0,0,0)

		pmullw		mm4,Cr_coeff
		punpckhdq	mm5,mm5		;Cr (1,1,1,1)

		movq		mm2,mm0
		punpcklwd	mm0,mm0			;Y (0,0,1,1)

		pmullw		mm5,Cr_coeff
		punpckhwd	mm2,mm2			;Y (2,2,3,3)

		movq		mm1,mm0
		punpckldq	mm0,mm0			;Y (0,0,0,0)

		movq		[eax+128],mm3
		punpckhdq	mm1,mm1			;Y (1,1,1,1)

		movq		mm3,mm2
		punpckldq	mm6,mm6		;Cb (0,0,0,0)

		pmullw		mm6,Cb_coeff
		punpckhdq	mm7,mm7		;Cb (1,1,1,1)

		pmullw		mm7,Cb_coeff
		punpckhdq	mm3,mm3			;Y (3,3,3,3)

		punpckldq	mm2,mm2			;Y (2,2,2,2)

		paddsw		mm4,mm6
		paddsw		mm0,mm4

		paddsw		mm5,mm7
		psraw		mm0,6

		paddsw		mm1,mm4
		paddsw		mm2,mm5

		psraw		mm1,6
		paddsw		mm3,mm5

		psraw		mm2,6
		packuswb	mm0,mm1

		psraw		mm3,6
		packuswb	mm2,mm3

		MOVNTQ		[edx+32],mm0
		MOVNTQ		[edx+40],mm2





		movd		mm4,[eax+384+12]	;Cr (0,1)
		pxor		mm3,mm3

		movd		mm6,[eax+256+12]	;Cb (0,1)
		punpcklwd	mm4,mm4

		movq		mm0,[eax+128+8]		;Y (0,1,2,3)
		punpcklwd	mm6,mm6

		movq		[eax+384+8],mm3
		psllw		mm0,6

		movq		[eax+256+8],mm3
		movq		mm5,mm4

		movq		mm7,mm6
		punpckldq	mm4,mm4		;Cr (0,0,0,0)

		pmullw		mm4,Cr_coeff
		punpckhdq	mm5,mm5		;Cr (1,1,1,1)

		movq		mm2,mm0
		punpcklwd	mm0,mm0			;Y (0,0,1,1)

		pmullw		mm5,Cr_coeff
		punpckhwd	mm2,mm2			;Y (2,2,3,3)

		movq		mm1,mm0
		punpckldq	mm0,mm0			;Y (0,0,0,0)

		movq		[eax+128+8],mm3	
		punpckhdq	mm1,mm1			;Y (1,1,1,1)

		movq		mm3,mm2
		punpckldq	mm6,mm6		;Cb (0,0,0,0)

		pmullw		mm6,Cb_coeff
		punpckhdq	mm7,mm7		;Cb (1,1,1,1)

		pmullw		mm7,Cb_coeff
		punpckhdq	mm3,mm3			;Y (3,3,3,3)

		punpckldq	mm2,mm2			;Y (2,2,2,2)

		paddsw		mm4,mm6
		paddsw		mm0,mm4

		paddsw		mm5,mm7
		psraw		mm0,6

		paddsw		mm1,mm4
		paddsw		mm2,mm5

		psraw		mm1,6
		paddsw		mm3,mm5

		psraw		mm2,6
		packuswb	mm0,mm1

		psraw		mm3,6
		packuswb	mm2,mm3

		MOVNTQ		[edx+48],mm0
		MOVNTQ		[edx+56],mm2

		add			edx,esi
		add			eax,16

		dec			ebx
		jne			yloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret			16
	}
}

// Customarily, YUY2/UYVY use Rec.601 ranges for YCrCb (Y [16,235] and C
// [16,240]), but JPEG uses full-scale YCrCb.  (That hasn't stopped some
// MJPEG codec writers from going ahead and using full-scale anyway, but
// it looks like crap when displayed via video overlay.)  We need to
// scale the values down before output.

#ifdef DECLARE_MJPEG_CONSTANTS
namespace {
	const __int64 one  = 0x0001000100010001i64;
	const __int64 bias = 0x0010001000100010i64;
	const __int64 lumafactor   = one * 28270;		// 0.5 * (220/255)
	const __int64 chromafactor = one * 28784;		// 0.5 * (224/255)
};
#endif

DECLARE_MJPEG_COLOR_CONVERTER(422_UYVY) {

	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax,[esp+8+16]
		mov			edx,[esp+4+16]
		mov			ebx,[esp+12+16]		;destination pitch
		mov			esi,[esp+16+16]		;height in lines

		; We unfortunately need to scale all three components here, which means
		; that UYVY isn't really that much faster than RGB16.
		;
		; pmulhw introduces a bias of -0.5 into the result, which is annoying.
		; (INTEL, WE WANT PMULHRW TOO.)  However, since our constants are
		; pretty close to 1, we can simply add one-half to the input and reduce
		; the bias by a factor of 16 or so.  As a bonus, by implementing this
		; as x*2+1 on the input, we also avoid the [-0.5, 0.5) limit of the
		; pmulhw multiplier.
		;
		; Register layout:
		;
		;	mm0-mm3		temporaries
		;	mm4			bias (16.w)
		;	mm5			chroma multiplier
		;	mm6			luma multiplier
		;	mm7			the ever present zero

		movq		mm4,bias
		pxor		mm7,mm7
		movq		mm5,chromafactor
		movq		mm6,lumafactor

yloop:
		movq		mm2,[eax+256]	;U (0-3)

		movq		mm3,[eax+384]	;V (0-3)
		paddw		mm2,mm2

		paddw		mm2,one
		paddw		mm3,mm3

		paddw		mm3,one
		pmulhw		mm2,mm5			;scale U by chroma constant: 0.5*(224/255)

		movq		mm0,[eax]		;Y (0-3)
		pmulhw		mm3,mm5			;scale V by chroma constant: 0.5*(224/255)

		movq		mm1,[eax+8]		;Y (4-7)
		paddw		mm0,mm0

		paddw		mm0,one
		paddw		mm1,mm1

		paddw		mm1,one
		pmulhw		mm0,mm6			;scale Y by chroma constant: 0.5*(220/255)

		movq		[eax+256],mm7
		pmulhw		mm1,mm6			;scale Y by chroma constant: 0.5*(220/255)

		paddw		mm2,mm4			;add in 16 bias
		paddw		mm3,mm4			;add in 16 bias

		packuswb	mm2,mm2
		paddw		mm0,mm4			;add in 16 bias

		packuswb	mm3,mm3
		paddw		mm1,mm4			;add in 16 bias

		movq		[eax+8],mm7
		punpcklbw	mm2,mm3			;mm2 = V3U3V2U2V1U1V0U0

		packuswb	mm0,mm0
		movq		mm3,mm2

		movq		[eax+384],mm7
		packuswb	mm1,mm1

		movq		[eax],mm7
		punpcklbw	mm3,mm0			;mm3 = Y3V1Y2U1Y1V0Y0U0

		movq		mm0,[eax+264]	;[2] U (0-3)
		punpckhbw	mm2,mm1			;[1] mm2 = Y7V3Y6U3Y5V2Y4U2

		movq		mm1,[eax+392]	;[2] V (0-3)
		paddw		mm0,mm0			;[2]

		paddw		mm0,one			;[2]
		paddw		mm1,mm1			;[2]

		paddw		mm1,one			;[2]
		pmulhw		mm0,mm5			;[2] scale U by chroma constant: 0.5*(224/255)

		MOVNTQ		[edx+8],mm2		;[1]
		pmulhw		mm1,mm5			;[2] scale V by chroma constant: 0.5*(224/255)

		movq		mm2,[eax+128]	;[2] Y (0-3)
		;

		MOVNTQ		[edx],mm3		;[1]
		paddw		mm2,mm2

		movq		mm3,[eax+136]	;[2] Y (4-7)
		paddw		mm0,mm4			;[2] add in 16 bias

		paddw		mm2,one
		paddw		mm3,mm3

		paddw		mm3,one
		packuswb	mm0,mm0			;[2]

		pmulhw		mm2,mm6			;scale Y by chroma constant: 0.5*(220/255)
		paddw		mm1,mm4			;add in 16 bias

		pmulhw		mm3,mm6			;scale Y by chroma constant: 0.5*(220/255)
		packuswb	mm1,mm1

		movq		[eax+264],mm7
		punpcklbw	mm0,mm1			;mm0 = V3U3V2U2V1U1V0U0

		movq		[eax+392],mm7
		paddw		mm2,mm4			;add in 16 bias

		paddw		mm3,mm4			;add in 16 bias
		packuswb	mm2,mm2

		packuswb	mm3,mm3
		movq		mm1,mm0

		movq		[eax+128],mm7
		punpcklbw	mm1,mm2			;mm1 = Y3V1Y2U1Y1V0Y0U0

		movq		[eax+136],mm7
		punpckhbw	mm0,mm3			;mm0 = Y7V3Y6U3Y5V2Y4U2

		MOVNTQ		[edx+16],mm1
		add			eax,16
		MOVNTQ		[edx+24],mm0
		add			edx,ebx
		dec			esi
		jne			yloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret			16
	}
}

DECLARE_MJPEG_COLOR_CONVERTER(422_YUY2) {

	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax,[esp+8+16]
		mov			edx,[esp+4+16]
		mov			ebx,[esp+12+16]		;destination pitch
		mov			esi,[esp+16+16]		;height in lines

		; We unfortunately need to scale all three components here, which means
		; that UYVY isn't really that much faster than RGB16.
		;
		; pmulhw introduces a bias of -0.5 into the result, which is annoying.
		; (INTEL, WE WANT PMULHRW TOO.)  However, since our constants are
		; pretty close to 1, we can simply add one-half to the input and reduce
		; the bias by a factor of 16 or so.  As a bonus, by implementing this
		; as x*2+1 on the input, we also avoid the [-0.5, 0.5) limit of the
		; pmulhw multiplier.
		;
		; Register layout:
		;
		;	mm0-mm3		temporaries
		;	mm4			bias (16.w)
		;	mm5			chroma multiplier
		;	mm6			luma multiplier
		;	mm7			the ever present zero

		movq		mm4,bias
		pxor		mm7,mm7
		movq		mm5,chromafactor
		movq		mm6,lumafactor

yloop:
		movq		mm2,[eax+256]	;U (0-3)

		movq		mm3,[eax+384]	;V (0-3)
		paddw		mm2,mm2

		paddw		mm2,one
		paddw		mm3,mm3

		paddw		mm3,one
		pmulhw		mm2,mm5			;scale U by chroma constant: 0.5*(224/255)

		movq		mm0,[eax]		;Y (0-3)
		pmulhw		mm3,mm5			;scale V by chroma constant: 0.5*(224/255)

		movq		mm1,[eax+8]		;Y (4-7)
		paddw		mm0,mm0

		paddw		mm0,one
		paddw		mm1,mm1

		paddw		mm1,one
		pmulhw		mm0,mm6			;scale Y by chroma constant: 0.5*(220/255)

		movq		[eax+256],mm7
		pmulhw		mm1,mm6			;scale Y by chroma constant: 0.5*(220/255)

		paddw		mm2,mm4			;add in 16 bias
		paddw		mm3,mm4			;add in 16 bias

		packuswb	mm2,mm2
		paddw		mm0,mm4			;add in 16 bias

		packuswb	mm3,mm3
		paddw		mm1,mm4			;add in 16 bias

		movq		[eax+8],mm7
		punpcklbw	mm2,mm3			;mm2 = V3U3V2U2V1U1V0U0

		packuswb	mm0,mm0
		movq		mm3,mm2

		movq		[eax+384],mm7
		packuswb	mm1,mm1

		movq		[eax],mm7
		punpckhbw	mm1,mm2			;[1] mm1 = V3Y7U3Y6V2Y5U2Y4

		movq		mm2,[eax+264]	;[2] U (0-3)
		punpcklbw	mm0,mm3			;[1] mm0 = V1Y3U1Y2V0Y1U0Y0

		movq		mm3,[eax+392]	;[2] V (0-3)
		paddw		mm2,mm2			;[2]

		paddw		mm2,one			;[2]
		paddw		mm3,mm3			;[2]

		paddw		mm3,one			;[2]
		pmulhw		mm2,mm5			;[2] scale U by chroma constant: 0.5*(224/255)

		MOVNTQ		[edx],mm0		;[1]
		pmulhw		mm3,mm5			;[2] scale V by chroma constant: 0.5*(224/255)

		movq		mm0,[eax+128]	;[2] Y (0-3)
		;

		MOVNTQ		[edx+8],mm1		;[1]
		paddw		mm0,mm0

		movq		mm1,[eax+136]	;[2] Y (4-7)
		paddw		mm2,mm4			;[2] add in 16 bias

		paddw		mm0,one
		paddw		mm1,mm1

		paddw		mm1,one
		packuswb	mm2,mm2			;[2]

		pmulhw		mm0,mm6			;scale Y by chroma constant: 0.5*(220/255)
		paddw		mm3,mm4			;add in 16 bias

		pmulhw		mm1,mm6			;scale Y by chroma constant: 0.5*(220/255)
		packuswb	mm3,mm3

		movq		[eax+264],mm7
		punpcklbw	mm2,mm3			;mm2 = V3U3V2U2V1U1V0U0

		movq		[eax+392],mm7
		paddw		mm0,mm4			;add in 16 bias

		paddw		mm1,mm4			;add in 16 bias
		packuswb	mm0,mm0

		packuswb	mm1,mm1

		movq		[eax+128],mm7
		punpcklbw	mm0,mm2			;mm0 = Y3V1Y2U1Y1V0Y0U0

		movq		[eax+136],mm7
		punpckhbw	mm1,mm2			;mm1 = Y7V3Y6U3Y5V2Y4U2

		MOVNTQ		[edx+16],mm0
		add			eax,16
		MOVNTQ		[edx+24],mm1
		add			edx,ebx
		dec			esi
		jne			yloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret			16
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	4:4:4 (PICVideo extension) routines
//
///////////////////////////////////////////////////////////////////////////

DECLARE_MJPEG_COLOR_CONVERTER(444_RGB15) {
	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax,[esp+8+16]		;ecx = Y coeff
		mov			edx,[esp+4+16]		;edx = dst
		mov			esi,[esp+12+16]		;esi = dst_pitch

		movq		mm6,rb_mask
		pxor		mm7,mm7
		mov			edi,[esp+16+16]		;edi = lines
yloop2:
		movq		mm0,[eax+128]		;mm0 = U3 | U2 | U1 | U0

		movq		mm1,[eax+256]		;mm1 = V3 | V2 | V1 | V0
		movq		mm2,mm0

		movq		mm4,[eax]			;mm4 = Y3 | Y2 | Y1 | Y0
		movq		mm3,mm1

		pmullw		mm0,Cb_coeff_G
		psllw		mm4,6

		pmullw		mm1,Cr_coeff_G

		pmullw		mm2,Cb_coeff_B

		pmullw		mm3,Cr_coeff_R

		paddw		mm0,mm1

		paddw		mm0,mm4
		paddw		mm2,mm4

		psraw		mm0,4
		paddw		mm3,mm4

		paddsw		mm0,G_const_1
		psraw		mm2,6

		psubusw		mm0,G_const_2
		psraw		mm3,6

		pand		mm0,G_const_3
		packuswb	mm2,mm2

		packuswb	mm3,mm3

		psrlw		mm2,3

		psrlw		mm3,1

		punpcklbw	mm2,mm3

		pand		mm2,mm6

		por			mm2,mm0

		MOVNTQ		[edx],mm2

		movq		[eax+128],mm7
		movq		[eax+256],mm7
		movq		[eax],mm7

		movq		mm0,[eax+128+8]		;mm0 = U3 | U2 | U1 | U0

		movq		mm1,[eax+256+8]		;mm1 = V3 | V2 | V1 | V0
		movq		mm2,mm0

		movq		mm4,[eax+8]			;mm4 = Y3 | Y2 | Y1 | Y0
		movq		mm3,mm1

		pmullw		mm0,Cb_coeff_G
		psllw		mm4,6

		pmullw		mm1,Cr_coeff_G

		pmullw		mm2,Cb_coeff_B

		pmullw		mm3,Cr_coeff_R

		paddw		mm0,mm1

		paddw		mm0,mm4

		psraw		mm0,4
		paddw		mm2,mm4

		psraw		mm2,6
		paddw		mm3,mm4

		paddsw		mm0,G_const_1
		psraw		mm3,6

		psubusw		mm0,G_const_2
		packuswb	mm2,mm2

		pand		mm0,G_const_3
		packuswb	mm3,mm3

		psrlw		mm2,3

		psrlw		mm3,1

		punpcklbw	mm2,mm3

		pand		mm2,mm6

		por			mm2,mm0

		MOVNTQ		[edx+8],mm2

		movq		[eax+128+8],mm7
		movq		[eax+256+8],mm7
		movq		[eax+8],mm7

		add			eax,16
		add			edx,esi

		dec			edi
		jne			yloop2

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret			16
	}
}

DECLARE_MJPEG_COLOR_CONVERTER(444_RGB32) {
	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax,[esp+8+16]		;ecx = Y coeff
		mov			edx,[esp+4+16]		;edx = dst
		mov			esi,[esp+12+16]		;esi = dst_pitch

		movq		mm5,G_const_4
		movq		mm6,Cr_coeff_G
		pxor		mm7,mm7
		mov			edi,[esp+16+16]		;edi = lines
yloop2:
		movq		mm0,[eax+128]		;mm0 = U3 | U2 | U1 | U0

		movq		mm1,[eax+256]		;mm1 = V3 | V2 | V1 | V0
		movq		mm2,mm0

		pmullw		mm0,Cb_coeff_G
		movq		mm3,mm1

		movq		mm4,[eax]			;mm4 = Y3 | Y2 | Y1 | Y0
		pmullw		mm1,mm6

		pmullw		mm2,Cb_coeff_B
		psllw		mm4,6

		pmullw		mm3,Cr_coeff_R

		paddw		mm1,mm0

		paddw		mm1,mm4
		paddw		mm2,mm4

		psraw		mm1,6
		paddw		mm3,mm4

		paddsw		mm1,mm5
		psraw		mm2,6

		psubusw		mm1,mm5
		psraw		mm3,6

		movq		[eax+128],mm7
		packuswb	mm2,mm2

		movq		[eax+256],mm7
		packuswb	mm3,mm3

		movq		[eax],mm7
		punpcklbw	mm2,mm3

		movq		mm3,mm2				;[1]
		punpcklbw	mm2,mm1				;[1]

		movq		mm0,[eax+128+8]		;[2] mm0 = U3 | U2 | U1 | U0
		punpckhbw	mm3,mm1				;[1]

		MOVNTQ		[edx],mm2			;[1]
		MOVNTQ		[edx+8],mm3			;[1]

		movq		mm1,[eax+256+8]		;[2] mm1 = V3 | V2 | V1 | V0
		movq		mm2,mm0

		pmullw		mm0,Cb_coeff_G
		movq		mm3,mm1

		movq		mm4,[eax+8]			;mm4 = Y3 | Y2 | Y1 | Y0
		pmullw		mm1,mm6

		pmullw		mm2,Cb_coeff_B
		psllw		mm4,6

		pmullw		mm3,Cr_coeff_R

		paddw		mm0,mm1

		paddw		mm0,mm4
		paddw		mm2,mm4

		psraw		mm0,6
		paddw		mm3,mm4

		paddsw		mm0,mm5
		psraw		mm2,6

		psubusw		mm0,mm5
		psraw		mm3,6

		movq		[eax+128+8],mm7
		packuswb	mm2,mm2

		movq		[eax+256+8],mm7
		packuswb	mm3,mm3

		movq		[eax+8],mm7
		punpcklbw	mm2,mm3

		movq		mm3,mm2
		punpcklbw	mm2,mm0

		punpckhbw	mm3,mm0
		add			eax,16

		MOVNTQ		[edx+16],mm2
		dec			edi
		MOVNTQ		[edx+24],mm3

		lea			edx,[edx+esi]
		jne			yloop2

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret			16
	}
}

DECLARE_MJPEG_COLOR_CONVERTER(444_UYVY) {
	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax,[esp+8+16]
		mov			edx,[esp+4+16]
		mov			ebx,[esp+12+16]		;destination pitch
		mov			esi,[esp+16+16]		;height in lines

		; We unfortunately need to scale all three components here, which means
		; that UYVY isn't really that much faster than RGB16.
		;
		; pmulhw introduces a bias of -0.5 into the result, which is annoying.
		; (INTEL, WE WANT PMULHRW TOO.)  However, since our constants are
		; pretty close to 1, we can simply add one-half to the input and reduce
		; the bias by a factor of 16 or so.  As a bonus, by implementing this
		; as x*2+1 on the input, we also avoid the [-0.5, 0.5) limit of the
		; pmulhw multiplier.
		;
		; Register layout:
		;
		;	mm0-mm3		temporaries
		;	mm4			bias (16.w)
		;	mm5			chroma multiplier
		;	mm6			luma multiplier
		;	mm7			the ever present zero

		movq		mm4,bias
		pxor		mm7,mm7
		movq		mm5,chromafactor
		movq		mm6,one

yloop:
		movq		mm2,[eax+128]	;U (0-3)
		movq		mm3,[eax+256]	;V (0-3)
		packuswb	mm2,[eax+128+8]
		packuswb	mm3,[eax+256+8]
		pand		mm2,x00FFw
		pand		mm3,x00FFw
		paddw		mm2,mm2

		paddw		mm2,mm6
		paddw		mm3,mm3

		paddw		mm3,mm6
		pmulhw		mm2,mm5			;scale U by chroma constant: 0.5*(224/255)

		movq		mm0,[eax]		;Y (0-3)
		pmulhw		mm3,mm5			;scale V by chroma constant: 0.5*(224/255)

		movq		mm1,[eax+8]		;Y (4-7)
		paddw		mm0,mm0

		paddw		mm0,mm6
		paddw		mm1,mm1

		pmulhw		mm0,lumafactor	;scale Y by chroma constant: 0.5*(220/255)
		paddw		mm1,mm6

		pmulhw		mm1,lumafactor	;scale Y by chroma constant: 0.5*(220/255)
		paddw		mm2,mm4			;add in 16 bias

		movq		[eax+128],mm7
		packuswb	mm2,mm2

		movq		[eax+128+8],mm7
		paddw		mm3,mm4			;add in 16 bias

		movq		[eax+256],mm7
		paddw		mm0,mm4			;add in 16 bias

		movq		[eax+256+8],mm7
		packuswb	mm3,mm3

		paddw		mm1,mm4			;add in 16 bias
		punpcklbw	mm2,mm3			;mm2 = V3U3V2U2V1U1V0U0

		packuswb	mm0,mm1
		movq		mm3,mm2

		movq		[eax],mm7
		punpcklbw	mm3,mm0			;mm3 = Y3V1Y2U1Y1V0Y0U0

		movq		[eax+8],mm7
		punpckhbw	mm2,mm0			;mm2 = Y7V3Y6U3Y5V2Y4U2

		MOVNTQ		[edx],mm3
		add			eax,16
		dec			esi

		MOVNTQ		[edx+8],mm2
		lea			edx,[edx+ebx]
		jne			yloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret			16
	}
}

DECLARE_MJPEG_COLOR_CONVERTER(444_YUY2) {
	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax,[esp+8+16]
		mov			edx,[esp+4+16]
		mov			ebx,[esp+12+16]		;destination pitch
		mov			esi,[esp+16+16]		;height in lines

		; We unfortunately need to scale all three components here, which means
		; that UYVY isn't really that much faster than RGB16.
		;
		; pmulhw introduces a bias of -0.5 into the result, which is annoying.
		; (INTEL, WE WANT PMULHRW TOO.)  However, since our constants are
		; pretty close to 1, we can simply add one-half to the input and reduce
		; the bias by a factor of 16 or so.  As a bonus, by implementing this
		; as x*2+1 on the input, we also avoid the [-0.5, 0.5) limit of the
		; pmulhw multiplier.
		;
		; Register layout:
		;
		;	mm0-mm3		temporaries
		;	mm4			bias (16.w)
		;	mm5			chroma multiplier
		;	mm6			luma multiplier
		;	mm7			the ever present zero

		movq		mm4,bias
		pxor		mm7,mm7
		movq		mm5,chromafactor
		movq		mm6,one

yloop:
		movq		mm2,[eax+128]	;U (0-3)
		movq		mm3,[eax+256]	;V (0-3)
		packuswb	mm2,[eax+128+8]
		packuswb	mm3,[eax+256+8]
		pand		mm2,x00FFw
		pand		mm3,x00FFw
		paddw		mm2,mm2

		paddw		mm2,mm6
		paddw		mm3,mm3

		paddw		mm3,mm6
		pmulhw		mm2,mm5			;scale U by chroma constant: 0.5*(224/255)

		movq		mm0,[eax]		;Y (0-3)
		pmulhw		mm3,mm5			;scale V by chroma constant: 0.5*(224/255)

		movq		mm1,[eax+8]		;Y (4-7)
		paddw		mm0,mm0

		paddw		mm0,mm6
		paddw		mm1,mm1

		pmulhw		mm0,lumafactor	;scale Y by chroma constant: 0.5*(220/255)
		paddw		mm1,mm6

		pmulhw		mm1,lumafactor	;scale Y by chroma constant: 0.5*(220/255)
		paddw		mm2,mm4			;add in 16 bias


		movq		[eax+128],mm7
		packuswb	mm2,mm2

		movq		[eax+128+8],mm7
		paddw		mm3,mm4			;add in 16 bias

		movq		[eax+256],mm7
		paddw		mm0,mm4			;add in 16 bias

		movq		[eax+256+8],mm7
		packuswb	mm3,mm3

		paddw		mm1,mm4			;add in 16 bias
		punpcklbw	mm2,mm3			;mm2 = V3U3V2U2V1U1V0U0

		packuswb	mm0,mm0
		movq		mm3,mm2

		movq		[eax],mm7
		packuswb	mm1,mm1

		movq		[eax+8],mm7
		punpcklbw	mm0,mm3			;mm0 = V1Y3U1Y2V0Y1U0Y0

		punpckhbw	mm1,mm2			;mm1 = V3Y7U3Y6V2Y5U2Y4
		add			eax,16

		MOVNTQ		[edx],mm0
		dec			esi

		MOVNTQ		[edx+8],mm1
		lea			edx,[edx+ebx]
		jne			yloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret			16
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	4:2:0 (PICVideo 4/1/1 extension) routines
//
///////////////////////////////////////////////////////////////////////////

DECLARE_MJPEG_COLOR_CONVERTER(420_RGB15) {
	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax,[esp+8+16]		;eax = Y coeff
		mov			ecx,[esp+8+16]		;ecx = Cr/Cb coeff
		mov			edx,[esp+4+16]		;edx = dst
		mov			esi,[esp+12+16]		;esi = dst_pitch

		mov			edi,[esp+16+16]		;edi = lines
		add			ecx,512
		xor			ebp,ebp
		movq		mm6,mask5
		pxor		mm7,mm7
yloop2:
		movd		mm5,[ecx]			;Cb (0,1)

		movd		mm3,[ecx+128]		;Cr (0,1)
		movq		mm4,mm5				;Cb [duplicate]

		movq		mm0,[eax]			;Y (0,1,2,3)
		punpcklwd	mm5,mm5				;Cb [subsampling]

		pmullw		mm5,Cb_coeff_B		;Cb [produce blue impacts]
		punpcklwd	mm4,mm3				;mm4: [Cr1][Cb1][Cr0][Cb0]

		pmaddwd		mm4,CrCb_coeff_G
		punpcklwd	mm3,mm3				;Cr [subsampling]

		pmullw		mm3,Cr_coeff_R		;Cr [produce red impacts]
		psllw		mm0,6

		paddw		mm5,mm0				;B (0,1,2,3)

		packssdw	mm4,mm4				;green impacts

		punpcklwd	mm4,mm4

		paddw		mm3,mm0				;R (0,1,2,3)
		psraw		mm5,6

		paddw		mm4,mm0				;G (0,1,2,3)
		psraw		mm3,6

		movq		[eax],mm7
		packuswb	mm3,mm3

		psraw		mm4,4
		pand		mm3,mm6

		paddsw		mm4,G_const_1
		packuswb	mm5,mm5

		pand		mm5,mm6
		psrlq		mm3,1

		psubusw		mm4,G_const_2
		psrlq		mm5,3

		pand		mm4,G_const_3
		punpcklbw	mm5,mm3

		por			mm5,mm4

		MOVNTQ		[edx],mm5




		movd		mm5,[ecx+4]			;Cb (0,1)

		movd		mm3,[ecx+128+4]		;Cr (0,1)
		movq		mm4,mm5				;Cb [duplicate]

		movq		mm0,[eax+8]			;Y (0,1,2,3)
		punpcklwd	mm5,mm5				;Cb [subsampling]

		pmullw		mm5,Cb_coeff_B		;Cb [produce blue impacts]
		punpcklwd	mm4,mm3				;mm4: [Cr1][Cb1][Cr0][Cb0]

		pmaddwd		mm4,CrCb_coeff_G
		punpcklwd	mm3,mm3				;Cr [subsampling]

		pmullw		mm3,Cr_coeff_R		;Cr [produce red impacts]
		psllw		mm0,6

		paddw		mm5,mm0				;B (0,1,2,3)
		packssdw	mm4,mm4				;green impacts

		punpcklwd	mm4,mm4

		paddw		mm3,mm0				;R (0,1,2,3)
		psraw		mm5,6

		paddw		mm4,mm0				;G (0,1,2,3)
		psraw		mm3,6

		movq		[eax+8],mm7
		packuswb	mm3,mm3

		psraw		mm4,4
		pand		mm3,mm6

		paddsw		mm4,G_const_1
		packuswb	mm5,mm5

		pand		mm5,mm6
		psrlq		mm3,1

		psubusw		mm4,G_const_2
		psrlq		mm5,3

		pand		mm4,G_const_3
		punpcklbw	mm5,mm3

		por			mm5,mm4

		MOVNTQ		[edx+8],mm5




		movd		mm5,[ecx+8]			;Cb (0,1)

		movd		mm3,[ecx+128+8]		;Cr (0,1)
		movq		mm4,mm5				;Cb [duplicate]

		movq		mm0,[eax+256]		;Y (0,1,2,3)
		punpcklwd	mm5,mm5				;Cb [subsampling]

		pmullw		mm5,Cb_coeff_B		;Cb [produce blue impacts]
		punpcklwd	mm4,mm3				;mm4: [Cr1][Cb1][Cr0][Cb0]

		pmaddwd		mm4,CrCb_coeff_G
		punpcklwd	mm3,mm3				;Cr [subsampling]

		pmullw		mm3,Cr_coeff_R		;Cr [produce red impacts]
		psllw		mm0,6

		paddw		mm5,mm0				;B (0,1,2,3)

		packssdw	mm4,mm4				;green impacts

		punpcklwd	mm4,mm4

		paddw		mm3,mm0				;R (0,1,2,3)
		psraw		mm5,6

		paddw		mm4,mm0				;G (0,1,2,3)
		psraw		mm3,6

		movq		[eax+256],mm7
		packuswb	mm3,mm3

		psraw		mm4,4
		pand		mm3,mm6

		paddsw		mm4,G_const_1
		packuswb	mm5,mm5

		pand		mm5,mm6
		psrlq		mm3,1

		psubusw		mm4,G_const_2
		psrlq		mm5,3

		pand		mm4,G_const_3
		punpcklbw	mm5,mm3

		por			mm5,mm4

		MOVNTQ		[edx+16],mm5



		movd		mm5,[ecx+12]		;Cb (0,1)

		movd		mm3,[ecx+128+12]	;Cr (0,1)
		movq		mm4,mm5				;Cb [duplicate]

		movq		mm0,[eax+256+8]		;Y (0,1,2,3)
		punpcklwd	mm5,mm5				;Cb [subsampling]

		pmullw		mm5,Cb_coeff_B		;Cb [produce blue impacts]
		punpcklwd	mm4,mm3				;mm4: [Cr1][Cb1][Cr0][Cb0]

		pmaddwd		mm4,CrCb_coeff_G
		punpcklwd	mm3,mm3				;Cr [subsampling]

		pmullw		mm3,Cr_coeff_R		;Cr [produce red impacts]
		psllw		mm0,6

		paddw		mm5,mm0				;B (0,1,2,3)
		packssdw	mm4,mm4				;green impacts

		punpcklwd	mm4,mm4

		paddw		mm3,mm0				;R (0,1,2,3)
		psraw		mm5,6

		paddw		mm4,mm0				;G (0,1,2,3)
		psraw		mm3,6

		movq		[eax+256+8],mm7
		packuswb	mm3,mm3

		psraw		mm4,4
		pand		mm3,mm6

		paddsw		mm4,G_const_1
		packuswb	mm5,mm5

		pand		mm5,mm6
		psrlq		mm3,1

		psubusw		mm4,G_const_2
		psrlq		mm5,3

		pand		mm4,G_const_3
		punpcklbw	mm5,mm3

		por			mm5,mm4

		MOVNTQ		[edx+24],mm5

		add			edx,esi
		add			ecx,ebp
		add			eax,16
		xor			ebp,16
		jnz			nochromafill

		movq		[ecx-16],mm7
		movq		[ecx-8],mm7
		movq		[ecx+128-16],mm7
		movq		[ecx+128-8],mm7

nochromafill:

		dec			edi
		jne			yloop2

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret			16
	}
}

DECLARE_MJPEG_COLOR_CONVERTER(420_RGB32) {
	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax,[esp+8+16]		;eax = dct_coeff (luma)
		mov			ecx,[esp+8+16]		;eax = dct_coeff (chroma)
		mov			edx,[esp+4+16]		;edx = dst
		add			ecx,512
		mov			esi,[esp+12+16]		;esi = dstpitch
		mov			ebx,[esp+16+16]		;ebx = lines
		xor			ebp,ebp
yloop:
		movd		mm4,[ecx+128]	;Cr (0,1)
		pxor		mm3,mm3

		movd		mm6,[ecx]		;Cb (0,1)
		punpcklwd	mm4,mm4

		movq		mm0,[eax]		;Y (0,1,2,3)
		punpcklwd	mm6,mm6

		psllw		mm0,6
		movq		mm5,mm4

		movq		mm7,mm6
		punpckldq	mm4,mm4		;Cr (0,0,0,0)

		pmullw		mm4,Cr_coeff
		punpckhdq	mm5,mm5		;Cr (1,1,1,1)

		movq		mm2,mm0
		punpcklwd	mm0,mm0			;Y (0,0,1,1)

		pmullw		mm5,Cr_coeff
		punpckhwd	mm2,mm2			;Y (2,2,3,3)

		movq		mm1,mm0
		punpckldq	mm0,mm0			;Y (0,0,0,0)

		movq		[eax],mm3
		punpckhdq	mm1,mm1			;Y (1,1,1,1)

		movq		mm3,mm2
		punpckldq	mm6,mm6		;Cb (0,0,0,0)

		pmullw		mm6,Cb_coeff
		punpckhdq	mm7,mm7		;Cb (1,1,1,1)

		pmullw		mm7,Cb_coeff
		punpckhdq	mm3,mm3			;Y (3,3,3,3)

		punpckldq	mm2,mm2			;Y (2,2,2,2)

		paddsw		mm4,mm6
		paddsw		mm0,mm4

		paddsw		mm5,mm7
		psraw		mm0,6

		paddsw		mm1,mm4
		paddsw		mm2,mm5

		psraw		mm1,6
		paddsw		mm3,mm5

		psraw		mm2,6
		packuswb	mm0,mm1

		psraw		mm3,6
		packuswb	mm2,mm3

		MOVNTQ		[edx],mm0
		MOVNTQ		[edx+8],mm2




		movd		mm4,[ecx+128+4]	;Cr (0,1)
		pxor		mm3,mm3

		movd		mm6,[ecx+4]		;Cb (0,1)
		punpcklwd	mm4,mm4

		movq		mm0,[eax+8]		;Y (0,1,2,3)
		punpcklwd	mm6,mm6

		psllw		mm0,6
		movq		mm5,mm4

		movq		mm7,mm6
		punpckldq	mm4,mm4		;Cr (0,0,0,0)

		pmullw		mm4,Cr_coeff
		punpckhdq	mm5,mm5		;Cr (1,1,1,1)

		movq		mm2,mm0
		punpcklwd	mm0,mm0			;Y (0,0,1,1)

		pmullw		mm5,Cr_coeff
		punpckhwd	mm2,mm2			;Y (2,2,3,3)

		movq		mm1,mm0
		punpckldq	mm0,mm0			;Y (0,0,0,0)

		movq		[eax+8],mm3	
		punpckhdq	mm1,mm1			;Y (1,1,1,1)

		movq		mm3,mm2
		punpckldq	mm6,mm6		;Cb (0,0,0,0)

		pmullw		mm6,Cb_coeff
		punpckhdq	mm7,mm7		;Cb (1,1,1,1)

		pmullw		mm7,Cb_coeff
		punpckhdq	mm3,mm3			;Y (3,3,3,3)

		punpckldq	mm2,mm2			;Y (2,2,2,2)

		paddsw		mm4,mm6
		paddsw		mm0,mm4

		paddsw		mm5,mm7
		psraw		mm0,6

		paddsw		mm1,mm4
		paddsw		mm2,mm5

		psraw		mm1,6
		paddsw		mm3,mm5

		psraw		mm2,6
		packuswb	mm0,mm1

		psraw		mm3,6
		packuswb	mm2,mm3

		MOVNTQ		[edx+16],mm0
		MOVNTQ		[edx+24],mm2





		movd		mm4,[ecx+128+8]	;Cr (0,1)
		pxor		mm3,mm3

		movd		mm6,[ecx+8]		;Cb (0,1)
		punpcklwd	mm4,mm4

		movq		mm0,[eax+256]	;Y (0,1,2,3)
		punpcklwd	mm6,mm6

		psllw		mm0,6
		movq		mm5,mm4

		movq		mm7,mm6
		punpckldq	mm4,mm4		;Cr (0,0,0,0)

		pmullw		mm4,Cr_coeff
		punpckhdq	mm5,mm5		;Cr (1,1,1,1)

		movq		mm2,mm0
		punpcklwd	mm0,mm0			;Y (0,0,1,1)

		pmullw		mm5,Cr_coeff
		punpckhwd	mm2,mm2			;Y (2,2,3,3)

		movq		mm1,mm0
		punpckldq	mm0,mm0			;Y (0,0,0,0)

		movq		[eax+256],mm3
		punpckhdq	mm1,mm1			;Y (1,1,1,1)

		movq		mm3,mm2
		punpckldq	mm6,mm6		;Cb (0,0,0,0)

		pmullw		mm6,Cb_coeff
		punpckhdq	mm7,mm7		;Cb (1,1,1,1)

		pmullw		mm7,Cb_coeff
		punpckhdq	mm3,mm3			;Y (3,3,3,3)

		punpckldq	mm2,mm2			;Y (2,2,2,2)

		paddsw		mm4,mm6
		paddsw		mm0,mm4

		paddsw		mm5,mm7
		psraw		mm0,6

		paddsw		mm1,mm4
		paddsw		mm2,mm5

		psraw		mm1,6
		paddsw		mm3,mm5

		psraw		mm2,6
		packuswb	mm0,mm1

		psraw		mm3,6
		packuswb	mm2,mm3

		MOVNTQ		[edx+32],mm0
		MOVNTQ		[edx+40],mm2





		movd		mm4,[ecx+128+12]	;Cr (0,1)
		pxor		mm3,mm3

		movd		mm6,[ecx+12]		;Cb (0,1)
		punpcklwd	mm4,mm4

		movq		mm0,[eax+256+8]		;Y (0,1,2,3)
		punpcklwd	mm6,mm6

		psllw		mm0,6
		movq		mm5,mm4

		movq		mm7,mm6
		punpckldq	mm4,mm4		;Cr (0,0,0,0)

		pmullw		mm4,Cr_coeff
		punpckhdq	mm5,mm5		;Cr (1,1,1,1)

		movq		mm2,mm0
		punpcklwd	mm0,mm0			;Y (0,0,1,1)

		pmullw		mm5,Cr_coeff
		punpckhwd	mm2,mm2			;Y (2,2,3,3)

		movq		mm1,mm0
		punpckldq	mm0,mm0			;Y (0,0,0,0)

		movq		[eax+256+8],mm3	
		punpckhdq	mm1,mm1			;Y (1,1,1,1)

		movq		mm3,mm2
		punpckldq	mm6,mm6		;Cb (0,0,0,0)

		pmullw		mm6,Cb_coeff
		punpckhdq	mm7,mm7		;Cb (1,1,1,1)

		pmullw		mm7,Cb_coeff
		punpckhdq	mm3,mm3			;Y (3,3,3,3)

		punpckldq	mm2,mm2			;Y (2,2,2,2)

		paddsw		mm4,mm6
		paddsw		mm0,mm4

		paddsw		mm5,mm7
		psraw		mm0,6

		paddsw		mm1,mm4
		paddsw		mm2,mm5

		psraw		mm1,6
		paddsw		mm3,mm5

		psraw		mm2,6
		packuswb	mm0,mm1

		psraw		mm3,6
		packuswb	mm2,mm3

		MOVNTQ		[edx+48],mm0
		MOVNTQ		[edx+56],mm2

		add			edx,esi
		add			ecx,ebp
		add			eax,16
		xor			ebp,16
		jnz			nochromafill

		pxor		mm3,mm3
		movq		[ecx-16],mm3
		movq		[ecx-8],mm3
		movq		[ecx+128-16],mm3
		movq		[ecx+128-8],mm3

nochromafill:
		dec			ebx
		jne			yloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret			16
	}
}

DECLARE_MJPEG_COLOR_CONVERTER(420_UYVY) {

	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax,[esp+8+16]		;eax = dct_coeff (luma)
		mov			ecx,[esp+8+16]		;eax = dct_coeff (chroma)
		mov			edx,[esp+4+16]		;edx = dst
		add			ecx,512
		mov			ebx,[esp+12+16]		;esi = dstpitch
		mov			esi,[esp+16+16]		;ebx = lines
		xor			ebp,ebp

		; We unfortunately need to scale all three components here, which means
		; that UYVY isn't really that much faster than RGB16.
		;
		; pmulhw introduces a bias of -0.5 into the result, which is annoying.
		; (INTEL, WE WANT PMULHRW TOO.)  However, since our constants are
		; pretty close to 1, we can simply add one-half to the input and reduce
		; the bias by a factor of 16 or so.  As a bonus, by implementing this
		; as x*2+1 on the input, we also avoid the [-0.5, 0.5) limit of the
		; pmulhw multiplier.
		;
		; Register layout:
		;
		;	mm0-mm3		temporaries
		;	mm4			bias (16.w)
		;	mm5			chroma multiplier
		;	mm6			luma multiplier
		;	mm7			the ever present zero

		movq		mm4,bias
		pxor		mm7,mm7
		movq		mm5,chromafactor
		movq		mm6,lumafactor

yloop:
		movq		mm2,[ecx]		;U (0-3)

		movq		mm3,[ecx+128]	;V (0-3)
		paddw		mm2,mm2

		paddw		mm2,one
		paddw		mm3,mm3

		paddw		mm3,one
		pmulhw		mm2,mm5			;scale U by chroma constant: 0.5*(224/255)

		movq		mm0,[eax]		;Y (0-3)
		pmulhw		mm3,mm5			;scale V by chroma constant: 0.5*(224/255)

		movq		mm1,[eax+8]		;Y (4-7)
		paddw		mm0,mm0

		paddw		mm0,one
		paddw		mm1,mm1

		paddw		mm1,one
		pmulhw		mm0,mm6			;scale Y by chroma constant: 0.5*(220/255)

		pmulhw		mm1,mm6			;scale Y by chroma constant: 0.5*(220/255)

		paddw		mm2,mm4			;add in 16 bias
		paddw		mm3,mm4			;add in 16 bias

		packuswb	mm2,mm2
		paddw		mm0,mm4			;add in 16 bias

		packuswb	mm3,mm3
		paddw		mm1,mm4			;add in 16 bias

		movq		[eax+8],mm7
		punpcklbw	mm2,mm3			;mm2 = V3U3V2U2V1U1V0U0

		packuswb	mm0,mm0
		movq		mm3,mm2

		packuswb	mm1,mm1

		movq		[eax],mm7
		punpcklbw	mm3,mm0			;mm3 = Y3V1Y2U1Y1V0Y0U0

		movq		mm0,[ecx+8]		;[2] U (0-3)
		punpckhbw	mm2,mm1			;[1] mm2 = Y7V3Y6U3Y5V2Y4U2

		movq		mm1,[ecx+136]	;[2] V (0-3)
		paddw		mm0,mm0			;[2]

		paddw		mm0,one			;[2]
		paddw		mm1,mm1			;[2]

		paddw		mm1,one			;[2]
		pmulhw		mm0,mm5			;[2] scale U by chroma constant: 0.5*(224/255)

		MOVNTQ		[edx+8],mm2		;[1]
		pmulhw		mm1,mm5			;[2] scale V by chroma constant: 0.5*(224/255)

		movq		mm2,[eax+256]	;[2] Y (0-3)
		;

		MOVNTQ		[edx],mm3		;[1]
		paddw		mm2,mm2

		movq		mm3,[eax+264]	;[2] Y (4-7)
		paddw		mm0,mm4			;[2] add in 16 bias

		paddw		mm2,one
		paddw		mm3,mm3

		paddw		mm3,one
		packuswb	mm0,mm0			;[2]

		pmulhw		mm2,mm6			;scale Y by chroma constant: 0.5*(220/255)
		paddw		mm1,mm4			;add in 16 bias

		pmulhw		mm3,mm6			;scale Y by chroma constant: 0.5*(220/255)
		packuswb	mm1,mm1

		punpcklbw	mm0,mm1			;mm0 = V3U3V2U2V1U1V0U0

		paddw		mm2,mm4			;add in 16 bias

		paddw		mm3,mm4			;add in 16 bias
		packuswb	mm2,mm2

		packuswb	mm3,mm3
		movq		mm1,mm0

		movq		[eax+256],mm7
		punpcklbw	mm1,mm2			;mm1 = Y3V1Y2U1Y1V0Y0U0

		movq		[eax+264],mm7
		punpckhbw	mm0,mm3			;mm0 = Y7V3Y6U3Y5V2Y4U2

		MOVNTQ		[edx+16],mm1
		add			eax,16
		MOVNTQ		[edx+24],mm0
		add			edx,ebx
		add			ecx,ebp
		xor			ebp,16
		jnz			nochromafill

		pxor		mm3,mm3
		movq		[ecx-16],mm3
		movq		[ecx-8],mm3
		movq		[ecx+128-16],mm3
		movq		[ecx+128-8],mm3

nochromafill:

		dec			esi
		jne			yloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret			16
	}
}

DECLARE_MJPEG_COLOR_CONVERTER(420_YUY2) {

	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			eax,[esp+8+16]		;eax = dct_coeff (luma)
		mov			ecx,[esp+8+16]		;eax = dct_coeff (chroma)
		mov			edx,[esp+4+16]		;edx = dst
		add			ecx,512
		mov			ebx,[esp+12+16]		;esi = dstpitch
		mov			esi,[esp+16+16]		;ebx = lines
		xor			ebp,ebp

		; We unfortunately need to scale all three components here, which means
		; that UYVY isn't really that much faster than RGB16.
		;
		; pmulhw introduces a bias of -0.5 into the result, which is annoying.
		; (INTEL, WE WANT PMULHRW TOO.)  However, since our constants are
		; pretty close to 1, we can simply add one-half to the input and reduce
		; the bias by a factor of 16 or so.  As a bonus, by implementing this
		; as x*2+1 on the input, we also avoid the [-0.5, 0.5) limit of the
		; pmulhw multiplier.
		;
		; Register layout:
		;
		;	mm0-mm3		temporaries
		;	mm4			bias (16.w)
		;	mm5			chroma multiplier
		;	mm6			luma multiplier
		;	mm7			the ever present zero

		movq		mm4,bias
		pxor		mm7,mm7
		movq		mm5,chromafactor
		movq		mm6,lumafactor

yloop:
		movq		mm2,[ecx]		;U (0-3)

		movq		mm3,[ecx+128]	;V (0-3)
		paddw		mm2,mm2

		paddw		mm2,one
		paddw		mm3,mm3

		paddw		mm3,one
		pmulhw		mm2,mm5			;scale U by chroma constant: 0.5*(224/255)

		movq		mm0,[eax]		;Y (0-3)
		pmulhw		mm3,mm5			;scale V by chroma constant: 0.5*(224/255)

		movq		mm1,[eax+8]		;Y (4-7)
		paddw		mm0,mm0

		paddw		mm0,one
		paddw		mm1,mm1

		paddw		mm1,one
		pmulhw		mm0,mm6			;scale Y by chroma constant: 0.5*(220/255)

		pmulhw		mm1,mm6			;scale Y by chroma constant: 0.5*(220/255)

		paddw		mm2,mm4			;add in 16 bias
		paddw		mm3,mm4			;add in 16 bias

		packuswb	mm2,mm2
		paddw		mm0,mm4			;add in 16 bias

		packuswb	mm3,mm3
		paddw		mm1,mm4			;add in 16 bias

		movq		[eax],mm7
		punpcklbw	mm2,mm3			;mm2 = V3U3V2U2V1U1V0U0

		packuswb	mm0,mm0
		movq		mm3,mm2

		packuswb	mm1,mm1

		movq		[eax+8],mm7
		punpckhbw	mm1,mm2			;[1] mm1 = V3Y7U3Y6V2Y5U2Y4

		movq		mm2,[ecx+8]		;[2] U (0-3)
		punpcklbw	mm0,mm3			;[1] mm0 = V1Y3U1Y2V0Y1U0Y0

		movq		mm3,[ecx+136]	;[2] V (0-3)
		paddw		mm2,mm2			;[2]

		paddw		mm2,one			;[2]
		paddw		mm3,mm3			;[2]

		paddw		mm3,one			;[2]
		pmulhw		mm2,mm5			;[2] scale U by chroma constant: 0.5*(224/255)

		MOVNTQ		[edx],mm0		;[1]
		pmulhw		mm3,mm5			;[2] scale V by chroma constant: 0.5*(224/255)

		movq		mm0,[eax+256]	;[2] Y (0-3)
		;

		MOVNTQ		[edx+8],mm1		;[1]
		paddw		mm0,mm0

		movq		mm1,[eax+264]	;[2] Y (4-7)
		paddw		mm2,mm4			;[2] add in 16 bias

		paddw		mm0,one
		paddw		mm1,mm1

		paddw		mm1,one
		packuswb	mm2,mm2			;[2]

		pmulhw		mm0,mm6			;scale Y by chroma constant: 0.5*(220/255)
		paddw		mm3,mm4			;add in 16 bias

		pmulhw		mm1,mm6			;scale Y by chroma constant: 0.5*(220/255)
		packuswb	mm3,mm3

		punpcklbw	mm2,mm3			;mm2 = V3U3V2U2V1U1V0U0
		paddw		mm0,mm4			;add in 16 bias

		paddw		mm1,mm4			;add in 16 bias
		packuswb	mm0,mm0

		packuswb	mm1,mm1

		movq		[eax+256],mm7
		punpcklbw	mm0,mm2			;mm0 = V1Y3U1Y2V0Y1U0Y0

		movq		[eax+264],mm7
		punpckhbw	mm1,mm2			;mm1 = V3Y7U3Y6V2Y5U2Y4

		MOVNTQ		[edx+16],mm0
		add			eax,16
		MOVNTQ		[edx+24],mm1
		add			edx,ebx
		add			ecx,ebp
		xor			ebp,16
		jnz			nochromafill

		pxor		mm3,mm3
		movq		[ecx-16],mm3
		movq		[ecx-8],mm3
		movq		[ecx+128-16],mm3
		movq		[ecx+128-8],mm3

nochromafill:
		dec			esi
		jne			yloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		ret			16
	}
}
