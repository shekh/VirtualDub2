#pragma warning(push)
#pragma warning(disable: 4733)	// warning C4733: Inline asm assigning to 'FS:0' : handler not registered as safe handler

static void __declspec(naked) __cdecl scalar_idct_feig_winograd_8x8_temp(int *d) {
	__asm {
		push	ebp
		push	edi
		push	esi
		push	ebx

		mov		edi, [esp+16+4]

		//---- round

		add		dword ptr [edi], 256

		// apply horizontal R2 (R18) transform [64a]
		mov		ebp, 7*32

horiz_r2_loop:
		mov		eax, [edi+ebp+2*4]		;eax = x2
		mov		ebx, [edi+ebp+3*4]		;ebx = x3

		mov		edx, [edi+ebp+4*4]		;edx = x4
		mov		esi, [edi+ebp+6*4]		;esi = x6

		lea		ecx, [eax+ebx]			;ecx = y3 = x2+x3
		sub		eax, ebx				;eax = y2 = x2-x3

		mov		[edi+ebp+2*4], eax
		mov		[edi+ebp+3*4], ecx

		mov		eax, [edi+ebp+5*4]		;eax = x5
		mov		ecx, [edi+ebp+7*4]		;ecx = x7

		lea		ebx, [edx+esi]			;ebx = y6 = x4+x6
		sub		edx, esi				;edx = y4 = x4-x6

		lea		esi, [ecx+eax]			;esi = y5 = x7+x5
		sub		ecx, eax				;ecx = y7 = x7-x5

		mov		[edi+ebp+6*4], ebx
		mov		[edi+ebp+7*4], ecx

		lea		eax, [esi+edx]			;eax = y5+y4
		sub		esi, edx				;esi = y5-y4

		mov		[edi+ebp+4*4], eax
		mov		[edi+ebp+5*4], esi

		sub		ebp, 32
		jnc		horiz_r2_loop

		// apply vertical R2 transform [64a]

		mov		ebp, 7*4

vert_r2_loop:
		mov		eax, [edi+ebp+2*32]		;eax = x2
		mov		ebx, [edi+ebp+3*32]		;ebx = x3

		mov		edx, [edi+ebp+4*32]		;edx = x4
		mov		esi, [edi+ebp+6*32]		;esi = x6

		lea		ecx, [eax+ebx]			;ecx = y3 = x2+x3
		sub		eax, ebx				;eax = y2 = x2-x3

		mov		[edi+ebp+2*32], eax
		mov		[edi+ebp+3*32], ecx

		mov		eax, [edi+ebp+5*32]		;eax = x5
		mov		ecx, [edi+ebp+7*32]		;ecx = x7

		lea		ebx, [esi+edx]			;ebx = y6 = x4+x6
		sub		edx, esi				;edx = y4 = x4-x6

		lea		esi, [ecx+eax]			;esi = y7 = x7+x5
		sub		ecx, eax				;ecx = y5 = x7-x5

		mov		[edi+ebp+6*32], ebx
		mov		[edi+ebp+7*32], ecx

		lea		eax, [esi+edx]			;eax = y5+y4
		sub		esi, edx				;esi = y5-y4

		mov		[edi+ebp+4*32], eax
		mov		[edi+ebp+5*32], esi

		sub		ebp, 4
		jnc		vert_r2_loop

		// apply M1/M2 and R1 to first 6 rows

		mov		ebp, [esp+16+4]
		add		ebp, 0*32
		call	m1
		mov		ebp, [esp+16+4]
		add		ebp, 1*32
		call	m1
		mov		ebp, [esp+16+4]
		add		ebp, 2*32
		call	m1
		mov		ebp, [esp+16+4]
		add		ebp, 5*32
		call	m1
		mov		ebp, [esp+16+4]
		add		ebp, 3*32
		call	m2
		mov		ebp, [esp+16+4]
		add		ebp, 4*32
		call	m2

		// Apply M3 and R2 to last two rows together
		//
		// NOTE: Swaps around columns 4<->5 and 6<->7 to make R1 easier.

		mov		ebp, [esp+16+4]

#define x0 [ebp+0*4+6*32]
#define x1 [ebp+1*4+6*32]
#define x2 [ebp+2*4+6*32]
#define x3 [ebp+3*4+6*32]
#define x4 [ebp+4*4+6*32]
#define x5 [ebp+5*4+6*32]
#define x6 [ebp+6*4+6*32]
#define x7 [ebp+7*4+6*32]
#define y0 [ebp+0*4+7*32]
#define y1 [ebp+1*4+7*32]
#define y2 [ebp+2*4+7*32]
#define y3 [ebp+3*4+7*32]
#define y4 [ebp+4*4+7*32]
#define y5 [ebp+5*4+7*32]
#define y6 [ebp+6*4+7*32]
#define y7 [ebp+7*4+7*32]

		mov		eax, x0
		mov		ebx, y0
		imul	ecx, eax, -8867
		add		eax, ebx
		imul	eax, 15137
		imul	ebx, 21407
		add		ecx, eax
		sub		ebx, eax
		mov		x0, ecx
		mov		y0, ebx

		mov		eax, x1
		mov		ebx, y1
		imul	ecx, eax, -8867
		add		eax, ebx
		imul	eax, 15137
		imul	ebx, 21407
		add		ecx, eax
		sub		ebx, eax
		mov		x1, ecx
		mov		y1, ebx

		mov		eax, x2
		mov		ebx, y2
		imul	ecx, eax, -8867
		add		eax, ebx
		imul	eax, 15137
		imul	ebx, 21407
		add		ecx, eax
		sub		ebx, eax
		mov		x2, ecx
		mov		y2, ebx

		mov		eax, x3
		mov		ebx, y3
		imul	ecx, eax, -12540
		add		eax, ebx
		imul	eax, 21407
		imul	ebx, 30274
		add		ecx, eax
		sub		ebx, eax
		mov		x3, ecx
		mov		y3, ebx

		mov		eax, x4
		mov		ebx, y4
		imul	ecx, eax, -12540
		add		eax, ebx
		imul	eax, 21407
		imul	ebx, 30274
		add		ecx, eax
		sub		ebx, eax
		mov		x4, ecx
		mov		y4, ebx

		mov		eax, x5
		mov		ebx, y5
		imul	ecx, eax, -8867
		add		eax, ebx
		imul	eax, 15137
		imul	ebx, 21407
		add		ecx, eax
		sub		ebx, eax
		mov		x5, ecx
		mov		y5, ebx

		mov		eax, y7				;eax = x77
		mov		ebx, x7				;ebx = x67
		mov		ecx, y6				;ecx = x76
		mov		edx, x6				;edx = x66
		lea		esi, [eax+edx]		;esi = y66 = x77+x66
		sub		eax, edx			;eax = y77 = x77-x66
		lea		edi, [ebx+ecx]		;edi = y67 = x67+x76
		sub		ebx, ecx			;ebx = y76 = x67-x76

		shl		ebx, 14				;ebx = x76' = y76 << SCALE_BITS
		shl		esi, 14				;esi = x66' = y66 << SCALE_BITS

		lea		ecx, [eax+edi]		;ecx = y77+y67
		sub		eax, edi			;eax = y77-y67

		imul	ecx, 11585			;ecx = x77' = (y77+y67) * inv_c0
		imul	eax, 11585			;eax = x67' = (y77-y67) * inv_c0

		lea		edx, [eax+ebx]		;edx = y67' = x67'+x76'
		sub		eax, ebx			;eax = y76' = x67'-x76'
		lea		ebx, [esi+ecx]		;ebx = y66' = x66'+x77'
		sub		esi, ecx			;esi = y77' = x66'-x77'

		mov		x6, ebx
		mov		x7, edx
		mov		y6, eax
		mov		y7, esi

#undef y7
#undef y6
#undef y5
#undef y4
#undef y3
#undef y2
#undef y1
#undef y0
#undef x7
#undef x6
#undef x5
#undef x4
#undef x3
#undef x2
#undef x1
#undef x0


		// Apply horizontal R1 (B1t * B2 * B3) [144a]
		//
		//	x0 = d[i+0];
		//	x1 = d[i+1];
		//	x2 = d[i+2];
		//	x3 = d[i+3];
		//	x4 = d[i+4];
		//	x5 = d[i+5];
		//	x6 = d[i+6];
		//	x7 = d[i+7];
		//
		//	y0 = x0+x1;
		//	y1 = x0-x1;
		//	y2 = x2;
		//	y3 = x2+x3;
		//
		//	y5 = x5;
		//	y7 = x5-x7;
		//	y4 = y7-x4;
		//	y6 = y4+x6;
		//
		//	z0 = y0+y3;
		//	z1 = y1+y2;
		//	z2 = y1-y2;
		//	z3 = y0-y3;
		//	z4 = y4;
		//	z5 = y5;
		//	z6 = y6;
		//	z7 = y7;
		//
		//	d[i+ 0] = (z0+z7)>>(PRESCALE_BITS + SCALE_BITS);
		//	d[i+ 8] = (z1-z6)>>(PRESCALE_BITS + SCALE_BITS);
		//	d[i+16] = (z2+z5)>>(PRESCALE_BITS + SCALE_BITS);
		//	d[i+24] = (z3+z4)>>(PRESCALE_BITS + SCALE_BITS);
		//	d[i+32] = (z3-z4)>>(PRESCALE_BITS + SCALE_BITS);
		//	d[i+40] = (z2-z5)>>(PRESCALE_BITS + SCALE_BITS);
		//	d[i+48] = (z1+z6)>>(PRESCALE_BITS + SCALE_BITS);
		//	d[i+56] = (z0-z7)>>(PRESCALE_BITS + SCALE_BITS);



#define x0 [esp+ebp+0*4]
#define x1 [esp+ebp+1*4]
#define x2 [esp+ebp+2*4]
#define x3 [esp+ebp+3*4]
#define x4 [esp+ebp+4*4]
#define x5 [esp+ebp+5*4]
#define x6 [esp+ebp+6*4]
#define x7 [esp+ebp+7*4]

		mov		edi, [esp+16+4]
		push	0
		push	dword ptr fs:[0]
		mov		dword ptr fs:[0], esp
		mov		ebp, 7*32
		mov		esp, edi

horiz_R1_loop:
		mov		eax, x0
		mov		ebx, x1

		mov		ecx, x2				;ecx = y2 = x2
		mov		edx, x3

		lea		edi, [eax+ebx]		;edi = y0 = x0+x1
		sub		eax, ebx			;eax = y1 = x0-x1

		add		edx, ecx			;edx = y3 = x2+x3
		mov		ebx, x5				;ebx = z5 = y5 = x5

		lea		esi, [eax+ecx]		;esi = z1 = y1+y2
		sub		eax, ecx			;eax = z2 = y1-y2

		mov		ecx, eax
		sub		eax, ebx			;eax = o5 = z2-z5

		mov		x5, eax
		add		ecx, ebx			;ecx = o2 = z2+z5

		lea		eax, [edi+edx]		;eax = z0 = y0+y3
		mov		x2, ecx

		sub		edi, edx			;edi = z3 = y0-y3
		mov		edx, x7

		sub		ebx, edx			;ebx = z7 = y5-y7
		mov		ecx, eax

		add		ecx, ebx			;ecx = o0 = z0+z7
		mov		edx, x4

		sub		eax, ebx			;eax = o7 = z0-z7
		sub		ebx, edx			;ebx = z4 = y7-x4

		mov		x0, ecx
		mov		x7, eax

		lea		ecx, [edi+ebx]		;ecx = o3 = z3+z4
		mov		edx, x6

		sub		edi, ebx			;edi = o4 = z3-z4
		add		ebx, edx			;ebx = z6 = y4+x6

		mov		x3, ecx
		mov		x4, edi

		lea		eax, [esi+ebx]		;eax = o6 = z1+z6
		sub		esi, ebx			;esi = o1 = z1-z6

		mov		x6, eax
		mov		x1, esi

		sub		ebp, 32
		jnc		horiz_R1_loop

#undef x7
#undef x6
#undef x5
#undef x4
#undef x3
#undef x2
#undef x1
#undef x0

		// Apply vertical R1 [144a].

		;	for(i=0; i<8; i++) {
		;		int x0, x1, x2, x3, x4, x5, x6, x7;
		;		int y0, y1, y2, y3, y4, y5, y6, y7;
		;		int z0, z1, z2, z3, z4, z5, z6, z7;

		;		x0 = d[i+0];
		;		x1 = d[i+8];
		;		x2 = d[i+16];
		;		x3 = d[i+24];
		;		x4 = d[i+32];
		;		x5 = d[i+40];
		;		x6 = d[i+48];
		;		x7 = d[i+56];
			
		;		y0 = x0+x1;
		;		y1 = x0-x1;
		;		y2 = x2;
		;		y3 = x2+x3;

		;		y5 = x5;
		;		y7 = x5-x7;
		;		y4 = y7-x4;
		;		y6 = y4+x6;

		;		z0 = y0+y3;
		;		z1 = y1+y2;
		;		z2 = y1-y2;
		;		z3 = y0-y3;
		;		z4 = y4;
		;		z5 = y5;
		;		z6 = y6;
		;		z7 = y7;

		;		d[i+ 0] = (z0+z7)>>(PRESCALE_BITS + SCALE_BITS);
		;		d[i+ 8] = (z1-z6)>>(PRESCALE_BITS + SCALE_BITS);
		;		d[i+16] = (z2+z5)>>(PRESCALE_BITS + SCALE_BITS);
		;		d[i+24] = (z3+z4)>>(PRESCALE_BITS + SCALE_BITS);
		;		d[i+32] = (z3-z4)>>(PRESCALE_BITS + SCALE_BITS);
		;		d[i+40] = (z2-z5)>>(PRESCALE_BITS + SCALE_BITS);
		;		d[i+48] = (z1+z6)>>(PRESCALE_BITS + SCALE_BITS);
		;		d[i+56] = (z0-z7)>>(PRESCALE_BITS + SCALE_BITS);
		;	}


#define x0 [esp+ebp+0*32]
#define x1 [esp+ebp+1*32]
#define x2 [esp+ebp+2*32]
#define x3 [esp+ebp+3*32]
#define x4 [esp+ebp+4*32]
#define x5 [esp+ebp+5*32]
#define x6 [esp+ebp+6*32]
#define x7 [esp+ebp+7*32]


		mov		ebp, 7*4
vert_R1_loop:
		mov		eax, x0
		mov		ebx, x1

		mov		ecx, x2				;ecx = y2 = x2
		mov		edx, x3

		lea		edi, [eax+ebx]		;edi = y0 = x0+x1
		sub		eax, ebx			;eax = y1 = x0-x1

		add		edx, ecx			;edx = y3 = x2+x3
		mov		ebx, x5				;ebx = z5 = y5 = x5

		lea		esi, [eax+ecx]		;esi = z1 = y1+y2
		sub		eax, ecx			;eax = z2 = y1-y2

		mov		ecx, eax
		sub		eax, ebx			;eax = o5 = z2-z5

		sar		eax, 22
		add		ecx, ebx			;ecx = o2 = z2+z5

		mov		x5, eax
		lea		eax, [edi+edx]		;eax = z0 = y0+y3

		sar		ecx, 22
		sub		edi, edx			;edi = z3 = y0-y3

		mov		edx, x7
		mov		x2, ecx

		sub		ebx, edx			;ebx = z7 = y5-y7
		mov		ecx, eax

		mov		edx, x4
		add		ecx, ebx			;ecx = o0 = z0+z7

		sar		ecx, 22
		sub		eax, ebx			;eax = o7 = z0-z7

		sar		eax, 22
		sub		ebx, edx			;ebx = z4 = y7-x4

		mov		x0, ecx
		lea		ecx, [edi+ebx]		;ecx = o3 = z3+z4

		mov		x7, eax
		mov		edx, x6

		sar		ecx, 22
		sub		edi, ebx			;edi = o4 = z3-z4

		sar		edi, 22
		add		ebx, edx			;ebx = z6 = y4+x6

		mov		x3, ecx
		lea		eax, [esi+ebx]		;eax = o6 = z1+z6

		sar		eax, 22
		sub		esi, ebx			;esi = o1 = z1-z6

		sar		esi, 22
		mov		x4, edi

		mov		x6, eax
		mov		x1, esi

		sub		ebp, 4
		jnc		vert_R1_loop

		mov		esp, dword ptr fs:[0]
		pop		dword ptr fs:[0]
		pop		eax

#undef x7
#undef x6
#undef x5
#undef x4
#undef x3
#undef x2
#undef x1
#undef x0




		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		ret

#define x0 [ebp+0*4]
#define x1 [ebp+1*4]
#define x2 [ebp+2*4]
#define x3 [ebp+3*4]
#define x4 [ebp+4*4]
#define x5 [ebp+5*4]
#define x6 [ebp+6*4]
#define x7 [ebp+7*4]

m1:
		mov		eax, x0
		mov		ebx, x1
		mov		ecx, x2
		mov		edx, x5

		shl		eax, 13
		shl		ebx, 13
		shl		ecx, 13
		shl		edx, 13
		mov		x0, eax
		mov		x1, ebx
		mov		x2, ecx
		imul	eax, x3, 11585		;inv_a0
		imul	ebx, x4, 11585		;inv_a0
		mov		x5, edx

		mov		ecx, x6
		mov		esi, x7
		mov		edx, esi
		add		esi, ecx			;esi = x6+x7
		imul	ecx, -8868			;ecx = x6*inv_a3
		imul	edx, 21407			;edx = x7*inv_a1
		imul	esi, -15137			;esi = (x6+x7)*inv_a2
		mov		x3, eax
		mov		x4, ebx
		sub		ecx, esi
		add		edx, esi
		mov		x6, ecx
		mov		x7, edx
		ret

m2:
		imul	eax, x0, 11585
		imul	ebx, x1, 11585
		imul	ecx, x2, 11585
		imul	edx, x5, 11585

		mov		x0, eax
		mov		x1, ebx

		mov		esi, x3
		mov		edi, x4
		shl		esi, 14
		shl		edi, 14

		mov		x2, ecx
		mov		x5, edx
		mov		x3, esi
		mov		x4, edi

		mov		ecx, x6
		mov		esi, x7
		mov		edx, esi
		add		esi, ecx			;esi = x6+x7

		imul	ecx, -12541			;ecx = x6*inv_b3
		imul	edx, 30274			;edx = x7*inv_b1
		imul	esi, -21407			;esi = (x6+x7)*inv_b2
		sub		ecx, esi
		add		edx, esi
		mov		x6, ecx
		mov		x7, edx
		ret

#undef x7
#undef x6
#undef x5
#undef x4
#undef x3
#undef x2
#undef x1
#undef x0
	}
}

static void scalar_idct_feig_winograd_8x8(int *d) {
	scalar_idct_feig_winograd_8x8_temp(d);
}

static void scalar_idct_feig_winograd_4x4(int *d) {
	scalar_idct_feig_winograd_8x8(d);
}

#pragma warning(pop)
