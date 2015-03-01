void __declspec(naked) VDComputeComplexFFT_DIT_radix4_iter0_SSE(float *p, unsigned bits) {
	static const __declspec(align(16)) uint32 kNegate[4]={0,0,0x80000000,0x80000000};
	static const __declspec(align(16)) uint32 kRotate[4]={0,0,0,0x80000000};

	__asm {
		mov		eax, 8
		mov		cl, [esp+8]
		shl		eax, cl
		mov		ecx, [esp+4]
		movaps	xmm7, kNegate
		movaps	xmm6, kRotate
xloop:
		movaps	xmm1, [ecx+16]			;xmm1 =  x3 | x1
		movaps	xmm0, [ecx]				;xmm0 =  x2 | x0
		movaps	xmm3, xmm1				;xmm3 =  x3 | x1
		movaps	xmm2, xmm0				;xmm2 =  x2 | x0
		shufps	xmm1, xmm1, 01001110b	;xmm1 =  x1 | x3
		xorps	xmm3, kNegate			;xmm3 = -x3 | x1
		shufps	xmm0, xmm0, 01001110b	;xmm0 =  x0 | x2
		xorps	xmm2, kNegate				;xmm2 = -x2 | x0
		addps	xmm1, xmm3				;xmm1 = x1-x3 | x1+x3 = y3 | y1
		addps	xmm0, xmm2				;xmm0 = x0-x2 | x0+x2 = y2 | y0
		shufps	xmm1, xmm1, 10110100b	;xmm1 = y3i,y3r | y1
		movaps	xmm2, xmm0
		xorps	xmm1, kRotate				;xmm1 = y3*-j | y1
		addps	xmm0, xmm1
		movaps	[ecx], xmm0
		subps	xmm2, xmm1
		movaps	[ecx+16], xmm2
		add		ecx, 32
		sub		eax, 32
		jnz		xloop

		ret
	}
}

void __declspec(naked) VDComputeComplexFFT_DIT_radix4_table_SSE(float *p, unsigned bits, unsigned subbits, const float *table, int table_step) {
	static const __declspec(align(16)) uint32 kTwiddleInverts[4]={0x80000000,0,0x80000000,0};
	static const __declspec(align(16)) uint32 kRotationInvert[4]={0,0,0,0x80000000};
	static const __declspec(align(16)) float kIdentity[4]={1,0,1,0};
	__asm {
		push	ebp
		mov		ebp, esp
		push	edi
		push	esi
		push	ebx

#define p_p			[ebp+8]
#define p_bits		[ebp+12]
#define p_subbits	[ebp+16]
#define p_table		[ebp+20]
#define p_tablestep	[ebp+24]

#define l_twiddle0	[esp]
#define l_twiddle1	[esp+16]
#define l_twiddle2	[esp+32]
#define l_twiddle3	[esp+48]
#define l_Mh		[esp+64]
#define l_Np		[esp+68]
#define l_limit		[esp+72]
#define l_p			[esp+76]
#define l_tablestep	[esp+80]
#define l_ebpsav	[esp+84]

		sub		esp, 88
		and		esp, -16

		mov		l_ebpsav, ebp

		mov		edi, p_table		;edi = table

		mov		eax, p_tablestep
		shl		eax, 2
		mov		l_tablestep, eax	;table_step *= sizeof(float)

		mov		cl, p_bits
		mov		eax, 8
		shl		eax, cl				;Np * sizeof(float)
		mov		l_Np, eax

		mov		cl, p_subbits
		mov		eax, 4
		shl		eax, cl				;eax = 4 << subbits = Mh*sizeof(float)
		mov		l_Mh, eax
		mov		ebx, p_p
		add		eax, ebx		;eax = p + (1 << subbits)
		mov		l_limit, eax		;store limit
		mov		l_p, ebx

		movaps	xmm7, kRotationInvert
		mov		eax, l_p			;eax = p
		mov		ebp, l_Mh			;ebp = 4<<subbits = Mh*sizeof(float)
		lea		ebx, [eax+ebp]		;ebx = eax + Mh
		lea		ecx, [ebx+ebp]		;ecx = ebx + Mh
		lea		edx, [ecx+ebp]		;edx = ecx + Mh
		jmp		short yloop
		align	16
yloop:
		;load twiddle factors and bump table pointer
		movaps	xmm4, kIdentity			;2; xmm4 =  ?  |  ?  |  0  |  1
		movhps	xmm4, [edi]				;1; xmm4 = ss1 | sc1 |  0  |  1
		movlps	xmm5, [edi+8]			;1; xmm5 =  ?  |  ?  | ss2 | sc2

		movaps	xmm6, kTwiddleInverts	;2
		movhps	xmm5, [edi+16]			;1; xmm5 = ss3 | sc3 | ss2 | sc2
		movaps	xmm2, xmm4				;1

		shufps	xmm4, xmm4, 11110101b	;3; xmm4 = ss1 | ss1 |  0  |  0
		movlps	xmm0, [eax]				;1; xmm0 =  ? |  ? | i0 | r0
		add		edi, l_tablestep		;1

		shufps	xmm2, xmm2, 10100000b	;3; xmm2 = sc1 | sc1 |  1  |  1
		movhps	xmm0, [ecx]				;1; xmm0 = i1 | r1 | i0 | r0
		movlps	xmm1, [ebx]				;1; xmm1 =  ? |  ? | i2 | r2

		movaps	xmm3, xmm5				;2
		movhps	xmm1, [edx]				;1; xmm1 = i3 | r3 | i2 | r2
		add		ebx, 8					;1

		xorps	xmm4, xmm6				;2; xmm4 = ss1 |-ss1 |  0  |  0
		add		ecx, 8					;1
		add		edx, 8					;1

		shufps	xmm5, xmm5, 11110101b	;3; xmm5 = ss3 | ss3 | ss2 | ss2
		add		eax, 8					;1

		xorps	xmm5, xmm6				;2; xmm5 = ss3 |-ss3 | ss2 |-ss2

		shufps	xmm3, xmm3, 10100000b	;3; xmm3 = sc3 | sc3 | sc2 | sc2

		movaps	l_twiddle0, xmm2		;2
		movaps	l_twiddle1, xmm3		;2


		;perform twiddles
		movaps	xmm2, xmm0				;2; xmm2 = i1 | r1 | i0 | r0
		mulps	xmm0, xmm4				;2; xmm0 =  i1*ss1 | r1*ss1 |  0     |  0
		movaps	xmm3, xmm1				;2; xmm3 = i3 | r3 | i2 | r2
		mulps	xmm1, xmm5				;2; xmm1 =  i3*ss3 |-r3*ss3 | i2*ss2 |-r2*ss2
		shufps	xmm1, xmm1, 10110001b	;3; xmm1 = -r3*ss3 | i3*ss3 |-r2*ss2 | i2*ss2
		mulps	xmm2, l_twiddle0		;4; xmm2 =  i1*sc1 | r1*sc1 | i0     | r0
		shufps	xmm0, xmm0, 10110001b	;3; xmm0 = -r1*ss1 | i1*ss1 |  0     |  0     
		addps	xmm0, xmm2				;2; xmm0 = i1s | r1s | i0  | r0
		mulps	xmm3, l_twiddle1		;4; xmm3 =  i3*sc3 | r3*sc3 | i2*sc2 | r2*sc2
		addps	xmm1, xmm3				;2; xmm1 = i3s | r3s | i2s | r2s

		;first set of butterflies (0/2, 1/3)
		movaps	xmm2, xmm0				;2
		addps	xmm0, xmm1				;2; xmm0 = i1b | r1b | i0b | r0b
		subps	xmm2, xmm1				;2; xmm2 = i3b | r3b | i2b | r2b

		;imaginary rotation
		movaps	xmm3, xmm0				;2; xmm3 = i1b | r1b | i0b | r0b
		movlhps	xmm0, xmm2				;2; xmm0 = i2b | r2b | i0b | r0b
		shufps	xmm3, xmm2, 10111110b	;2; xmm3 = r3b | i3b | i1b | r1b
		xorps	xmm3, xmm7				;2; xmm3 =-r3b | i3b | i1b | r1b


		;second set of butterflies
		;write out one output for each FFT
		movaps	xmm2, xmm0				;2; xmm2 = i2b | r2b | i0b | r0b
		cmp		eax, l_limit			;1; covered all bins in each FFT?

		addps	xmm0, xmm3				;2; xmm0 = i2b-r3b | r2b+i3b | i0b+i1b | r0b+r1b
		movlps	[eax-8], xmm0			;1
		movhps	[ebx-8], xmm0			;1

		subps	xmm2, xmm3				;2; xmm2 = i2b+r3b | r2b-i3b | i0b-i1b | r0b-r1b
		movlps	[ecx-8], xmm2			;1
		movhps	[edx-8], xmm2			;1

		jnz		yloop					;1

		mov		eax, l_ebpsav
		lea		esp, [eax-12]
		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		ret
#undef p_p
#undef p_bits
#undef p_subbits
#undef p_table
#undef p_tablestep

#undef l_twiddle0
#undef l_twiddle1
#undef l_twiddle2
#undef l_twiddle3
#undef l_Mh
#undef l_Np
#undef l_limit
#undef l_p
#undef l_tablestep
#undef l_ebpsav
	}
}

void __declspec(naked) VDComputeComplexFFT_DIT_radix4_table2x_SSE(float *p, unsigned bits, unsigned subbits, const float *table, int table_step) {
	static const __declspec(align(16)) uint32 kTwiddleInverts[4]={0x80000000,0,0x80000000,0};
	static const __declspec(align(16)) uint32 kRotationInvert[4]={0,0x80000000,0,0x80000000};
	static const __declspec(align(16)) float kIdentity[4]={1,0,1,0};

	__asm {
		push	ebp
		mov		ebp, esp
		push	edi
		push	esi
		push	ebx

#define p_p			[ebp+8]
#define p_bits		[ebp+12]
#define p_subbits	[ebp+16]
#define p_table		[ebp+20]
#define p_tablestep	[ebp+24]

#define l_twiddle0	[esp]
#define l_twiddle1	[esp+16]
#define l_twiddle2	[esp+32]
#define l_twiddle3	[esp+48]
#define l_twiddle4	[esp+64]
#define l_twiddle5	[esp+80]
#define l_Mh		[esp+96]
#define l_Np		[esp+100]
#define l_limit		[esp+104]
#define l_p			[esp+108]
#define l_tablestep	[esp+112]
#define l_ebpsav	[esp+116]

		sub		esp, 120
		and		esp, -16

		mov		l_ebpsav, ebp

		mov		edi, p_table		;edi = table

		mov		eax, p_tablestep
		shl		eax, 2
		mov		l_tablestep, eax	;table_step *= sizeof(float)

		mov		cl, p_bits
		mov		eax, 8
		shl		eax, cl				;Np * sizeof(float)
		mov		l_Np, eax

		mov		cl, p_subbits
		mov		eax, 4
		shl		eax, cl				;eax = 4 << subbits = Mh*sizeof(float)
		mov		l_Mh, eax
		mov		ebx, p_p
		add		eax, ebx		;eax = p + (1 << subbits)
		mov		l_limit, eax		;store limit
		mov		l_p, ebx

		movaps	xmm7, kRotationInvert
		mov		eax, l_p			;eax = p
yloop:
		mov		ebp, l_Mh			;ebp = 4<<subbits = Mh*sizeof(float)

		lea		ebx, [eax+ebp]		;ebx = eax + Mh
		lea		ecx, [ebx+ebp]		;ecx = ebx + Mh
		lea		edx, [ecx+ebp]		;edx = ecx + Mh

		shl		ebp, 2				;ebp = Mp*sizeof(float)
		mov		esi, l_Np			;esi = Np*sizeof(float)

		;load twiddle factors and bump table pointer
		movaps	xmm6, kTwiddleInverts

		movlps	xmm0, [edi]
		movlps	xmm1, [edi+8]
		movlps	xmm2, [edi+16]
		add		edi, l_tablestep

		movhps	xmm0, [edi]
		movhps	xmm1, [edi+8]
		movhps	xmm2, [edi+16]
		add		edi, l_tablestep

		movaps	xmm3, xmm0
		shufps	xmm0, xmm0, 10100000b
		shufps	xmm3, xmm3, 11110101b
		movaps	l_twiddle0, xmm0
		xorps	xmm3, xmm6
		movaps	l_twiddle1, xmm3

		movaps	xmm4, xmm1
		shufps	xmm1, xmm1, 10100000b
		shufps	xmm4, xmm4, 11110101b
		movaps	l_twiddle2, xmm1
		xorps	xmm4, xmm6
		movaps	l_twiddle3, xmm4

		movaps	xmm5, xmm2
		shufps	xmm2, xmm2, 10100000b
		shufps	xmm5, xmm5, 11110101b
		movaps	l_twiddle4, xmm2
		xorps	xmm5, xmm6
		movaps	l_twiddle5, xmm5

xloop:
		movaps	xmm0, [eax]				;xmm0 = p0
		movaps	xmm1, [ecx]				;xmm1 = p2
		movaps	xmm2, [ebx]				;xmm2 = p1
		movaps	xmm3, [edx]				;xmm3 = p3

		;perform twiddles

		movaps	xmm4, xmm1
		movaps	xmm5, xmm2
		movaps	xmm6, xmm3
		mulps	xmm1, l_twiddle0
		mulps	xmm4, l_twiddle1
		shufps	xmm4, xmm4, 10110001b
		mulps	xmm2, l_twiddle2
		mulps	xmm5, l_twiddle3
		shufps	xmm5, xmm5, 10110001b
		mulps	xmm3, l_twiddle4
		mulps	xmm6, l_twiddle5
		shufps	xmm6, xmm6, 10110001b
		addps	xmm1, xmm4
		addps	xmm2, xmm5
		addps	xmm3, xmm6

		;first set of butterflies (0/2, 1/3)
		movaps	xmm4, xmm0
		addps	xmm0, xmm2
		subps	xmm4, xmm2
		movaps	xmm5, xmm1
		addps	xmm1, xmm3
		subps	xmm5, xmm3

		;imaginary rotation
		;p0-p3: xmm0, xmm1, xmm4, xmm5
		shufps	xmm5, xmm5, 10110001b
		xorps	xmm5, xmm7

		;second set of butterflies
		;x0-x3: xmm0, xmm1, xmm4, xmm5
		movaps	xmm2, xmm0
		addps	xmm0, xmm1
		subps	xmm2, xmm1
		movaps	[eax], xmm0
		movaps	[ecx], xmm2
		add		eax, ebp				;p0 += Mp
		add		ecx, ebp				;p2 += Mp

		movaps	xmm3, xmm4
		addps	xmm4, xmm5
		subps	xmm3, xmm5
		movaps	[ebx], xmm4
		add		ebx, ebp				;p1 += Mp
		movaps	[edx], xmm3
		add		edx, ebp				;p3 += Mp

		sub		esi, ebp				;i += Mp
		jnz		xloop

		mov		eax, l_p
		add		eax, 16					;4*sizeof(float)
		cmp		eax, l_limit			;covered all bins in each FFT?
		mov		l_p, eax
		jnz		yloop

		mov		eax, l_ebpsav
		lea		esp, [eax-12]
		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		ret
#undef p_p
#undef p_bits
#undef p_subbits
#undef p_table
#undef p_tablestep

#undef l_twiddle0
#undef l_twiddle1
#undef l_twiddle2
#undef l_twiddle3
#undef l_twiddle4
#undef l_twiddle5
#undef l_Mh
#undef l_Np
#undef l_limit
#undef l_p
#undef l_tablestep
#undef l_ebpsav
	}
}
