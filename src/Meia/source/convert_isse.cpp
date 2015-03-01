#include <vd2/Meia/MPEGConvert.h>
#include "tables.h"

///////////////////////////////////////////////////////////////////////////

using namespace nsVDMPEGTables;

extern "C" void asm_YUVtoRGB16_row_ISSE(
		void *ARGB1_pointer,
		void *ARGB2_pointer,
		const void *Y1_pointer,
		const void *Y2_pointer,
		const void *U_pointer,
		const void *V_pointer,
		long width
		);

extern "C" void asm_YUVtoRGB16565_row_ISSE(
		void *ARGB1_pointer,
		void *ARGB2_pointer,
		const void *Y1_pointer,
		const void *Y2_pointer,
		const void *U_pointer,
		const void *V_pointer,
		long width
		);

extern "C" void asm_YUVtoRGB24_row_ISSE(
		void *ARGB1_pointer,
		void *ARGB2_pointer,
		const void *Y1_pointer,
		const void *Y2_pointer,
		const void *U_pointer,
		const void *V_pointer,
		long width
		);

extern "C" void asm_YUVtoRGB32_row_ISSE(
		void *ARGB1_pointer,
		void *ARGB2_pointer,
		const void *Y1_pointer,
		const void *Y2_pointer,
		const void *U_pointer,
		const void *V_pointer,
		long width
		);

namespace nsVDMPEGConvertISSE {

	void __declspec(naked) DecodeUYVY(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height) {
		__asm {
			push		ebp
			push		edi
			push		esi
			push		ebx

			mov			ebx,[esp+36+16]		;ebx = height
			mov			esi,[esp+32+16]		;esi = mbw
			xor			esi,-1				;esi = -mbw-1
			mov			eax,[esp+12+16]		;eax = ysrc
			mov			ecx,[esp+24+16]		;ecx = cbsrc
			lea			esi,[esi*8+8]		;esi = -mbw*8
			mov			edx,[esp+20+16]		;edx = crsrc
			sub			ecx,esi				;ecx = cbsrc + mbw*8
			sub			edx,esi				;edx = crsrc + mbw*8
			mov			[esp+32+16],esi
			add			esi,esi
			sub			eax,esi				;edx = crsrc + mbw*16
			add			esi,esi
			mov			edi,[esp+ 4+16]		;edi = dst
			sub			edi,esi				;edx = crsrc + mbw*32
			mov			esi,[esp+28+16]		;esi = cpitch
yloop:
			mov			ebp,[esp+32+16]		;ebp = -mbw*8
xloop:
			prefetchnta	[ecx+ebp+32]
			movd		mm0,[ecx+ebp]		;mm0 =  0 |  0 |  0 |  0 | U3 | U2 | U1 | U0
			prefetchnta	[edx+ebp+32]
			punpcklbw	mm0,[edx+ebp]		;mm0 = V3 | U3 | V2 | U2 | V1 | U1 | V0 | U0
			prefetchnta	[eax+ebp*2+64]
			movq		mm2,[eax+ebp*2]		;mm2 = Y7 | Y6 | Y5 | Y4 | Y3 | Y2 | Y1 | Y0
			movq		mm1,mm0
			punpcklbw	mm0,mm2				;mm0 = Y3 | V1 | Y2 | U1 | Y1 | V0 | Y0 | V0
			punpckhbw	mm1,mm2				;mm1 = Y7 | V3 | Y6 | U3 | Y5 | V2 | Y4 | V2
			movntq		[edi+ebp*4],mm0
			movntq		[edi+ebp*4+8],mm1
			add			ebp,4
			jne			xloop

			add			eax,[esp+16+16]		;ysrc += ypitch
			add			edi,[esp+ 8+16]		;dst += dpitch
			xor			esi,[esp+28+16]		;only add chroma bump every other line
			add			ecx,esi				;usrc += (y&1) ? cpitch : 0;
			add			edx,esi				;vsrc += (y&1) ? cpitch : 0;

			dec			ebx
			jne			yloop
			
			sfence
			emms
			pop			ebx
			pop			esi
			pop			edi
			pop			ebp

			or			eax,-1
			ret
		};
	}

	void __declspec(naked) DecodeYUYV(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height) {
		__asm {
			push		ebp
			push		edi
			push		esi
			push		ebx

			mov			ebx,[esp+36+16]		;ebx = height
			mov			esi,[esp+32+16]		;esi = mbw
			xor			esi,-1				;esi = -mbw-1
			mov			eax,[esp+12+16]		;eax = ysrc
			mov			ecx,[esp+24+16]		;ecx = cbsrc
			lea			esi,[esi*8+8]		;esi = -mbw*8
			mov			edx,[esp+20+16]		;edx = crsrc
			sub			ecx,esi				;ecx = cbsrc + mbw*8
			sub			edx,esi				;edx = crsrc + mbw*8
			mov			[esp+32+16],esi
			add			esi,esi
			sub			eax,esi				;edx = crsrc + mbw*16
			add			esi,esi
			mov			edi,[esp+ 4+16]		;edi = dst
			sub			edi,esi				;edx = crsrc + mbw*32
			mov			esi,[esp+28+16]		;esi = cpitch
yloop:
			mov			ebp,[esp+32+16]		;ebp = -mbw*8
xloop:
			prefetchnta	[eax+ebp*2+64]
			movq		mm1,[eax+ebp*2]		;mm1 = Y7 | Y6 | Y5 | Y4 | Y3 | Y2 | Y1 | Y0
			prefetchnta	[ecx+ebp+32]
			movd		mm0,[ecx+ebp]		;mm0 =  0 |  0 |  0 |  0 | U3 | U2 | U1 | U0
			movq		mm2,mm1
			prefetchnta	[edx+ebp+32]
			punpcklbw	mm0,[edx+ebp]		;mm0 = V3 | U3 | V2 | U2 | V1 | U1 | V0 | U0
			punpcklbw	mm1,mm0				;mm1 = V1 | Y3 | U1 | Y2 | V0 | Y1 | U0 | Y0
			punpckhbw	mm2,mm0				;mm2 = V3 | Y7 | U3 | Y6 | V2 | Y5 | U2 | Y4
			movntq		[edi+ebp*4],mm1
			movntq		[edi+ebp*4+8],mm2
			add			ebp,4
			jne			xloop

			add			eax,[esp+16+16]		;ysrc += ypitch
			add			edi,[esp+ 8+16]		;dst += dpitch
			xor			esi,[esp+28+16]		;only add chroma bump every other line
			add			ecx,esi				;usrc += (y&1) ? cpitch : 0;
			add			edx,esi				;vsrc += (y&1) ? cpitch : 0;

			dec			ebx
			jne			yloop
			
			sfence
			emms
			pop			ebx
			pop			esi
			pop			edi
			pop			ebp

			or			eax,-1
			ret
		};
	}

	void __declspec(naked) DecodeYVYU(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height) {
		__asm {
			push		ebp
			push		edi
			push		esi
			push		ebx

			mov			ebx,[esp+36+16]		;ebx = height
			mov			esi,[esp+32+16]		;esi = mbw
			xor			esi,-1				;esi = -mbw-1
			mov			eax,[esp+12+16]		;eax = ysrc
			mov			ecx,[esp+24+16]		;ecx = cbsrc
			lea			esi,[esi*8+8]		;esi = -mbw*8
			mov			edx,[esp+20+16]		;edx = crsrc
			sub			ecx,esi				;ecx = cbsrc + mbw*8
			sub			edx,esi				;edx = crsrc + mbw*8
			mov			[esp+32+16],esi
			add			esi,esi
			sub			eax,esi				;edx = crsrc + mbw*16
			add			esi,esi
			mov			edi,[esp+ 4+16]		;edi = dst
			sub			edi,esi				;edx = crsrc + mbw*32
			mov			esi,[esp+28+16]		;esi = cpitch
yloop:
			mov			ebp,[esp+32+16]		;ebp = -mbw*8
xloop:
			prefetchnta	[eax+ebp*2+64]
			movq		mm1,[eax+ebp*2]		;mm1 = Y7 | Y6 | Y5 | Y4 | Y3 | Y2 | Y1 | Y0
			prefetchnta	[edx+ebp+32]
			movd		mm0,[edx+ebp]		;mm0 =  0 |  0 |  0 |  0 | V3 | V2 | V1 | V0
			movq		mm2,mm1
			prefetchnta	[ecx+ebp+32]
			punpcklbw	mm0,[ecx+ebp]		;mm0 = U3 | V3 | U2 | V2 | U1 | V1 | U0 | V0
			punpcklbw	mm1,mm0				;mm1 = U1 | Y3 | V1 | Y2 | U0 | Y1 | V0 | Y0
			punpckhbw	mm2,mm0				;mm2 = U3 | Y7 | V3 | Y6 | U2 | Y5 | V2 | Y4
			movntq		[edi+ebp*4],mm1
			movntq		[edi+ebp*4+8],mm2
			add			ebp,4
			jne			xloop

			add			eax,[esp+16+16]		;ysrc += ypitch
			add			edi,[esp+ 8+16]		;dst += dpitch
			xor			esi,[esp+28+16]		;only add chroma bump every other line
			add			ecx,esi				;usrc += (y&1) ? cpitch : 0;
			add			edx,esi				;vsrc += (y&1) ? cpitch : 0;

			dec			ebx
			jne			yloop

			sfence
			emms
			pop			ebx
			pop			esi
			pop			edi
			pop			ebp

			or			eax,-1
			ret
		};
	}

	void DecodeRGB15(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height) {
		char *dst1 = (char *)_dst;
		char *dst2 = (char *)_dst + dpitch;
		const unsigned char *srcY2 = srcY1 + ypitch;

		dpitch *= 2;
		ypitch *= 2;

		do {
			if (height == 1) {
				srcY2 = srcY1;
				dst2 = dst1;
			}
			asm_YUVtoRGB16_row_ISSE(dst1, dst2, srcY1, srcY2, srcCb, srcCr, mbw*8);
			dst1 += dpitch;
			dst2 += dpitch;
			srcY1 += ypitch;
			srcY2 += ypitch;
			srcCr += cpitch;
			srcCb += cpitch;
		} while((height-=2)>0);

		__asm sfence
		__asm emms
	}

	void DecodeRGB24(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height) {
		char *dst1 = (char *)_dst;
		char *dst2 = (char *)_dst + dpitch;
		const unsigned char *srcY2 = srcY1 + ypitch;

		dpitch *= 2;
		ypitch *= 2;

		do {
			if (height == 1) {
				srcY2 = srcY1;
				dst2 = dst1;
			}
			asm_YUVtoRGB24_row_ISSE(dst1, dst2, srcY1, srcY2, srcCb, srcCr, mbw*8);
			dst1 += dpitch;
			dst2 += dpitch;
			srcY1 += ypitch;
			srcY2 += ypitch;
			srcCr += cpitch;
			srcCb += cpitch;
		} while((height-=2)>0);

		__asm sfence
		__asm emms
	}

	void DecodeRGB32(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height) {
		char *dst1 = (char *)_dst;
		char *dst2 = (char *)_dst + dpitch;
		const unsigned char *srcY2 = srcY1 + ypitch;

		dpitch *= 2;
		ypitch *= 2;

		do {
			if (height == 1) {
				srcY2 = srcY1;
				dst2 = dst1;
			}
			asm_YUVtoRGB32_row_ISSE(dst1, dst2, srcY1, srcY2, srcCb, srcCr, mbw*8);
			dst1 += dpitch;
			dst2 += dpitch;
			srcY1 += ypitch;
			srcY2 += ypitch;
			srcCr += cpitch;
			srcCb += cpitch;
		} while((height-=2)>0);

		__asm sfence
		__asm emms
	}
};

///////////////////////////////////////////////////////////////////////////

namespace nsVDMPEGConvertReference {
	extern void DecodeY41P(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height);
	extern void DecodeRGB16(void *dst, ptrdiff_t dpitch, const unsigned char *srcY, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height);
}

const struct VDMPEGConverterSet g_VDMPEGConvert_isse = {
	nsVDMPEGConvertISSE::DecodeUYVY,
	nsVDMPEGConvertISSE::DecodeYUYV,
	nsVDMPEGConvertISSE::DecodeYVYU,
	nsVDMPEGConvertReference::DecodeY41P,
	nsVDMPEGConvertISSE::DecodeRGB15,
	nsVDMPEGConvertReference::DecodeRGB16,
	nsVDMPEGConvertISSE::DecodeRGB24,
	nsVDMPEGConvertISSE::DecodeRGB32,
};
