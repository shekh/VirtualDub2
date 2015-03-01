#include <vd2/Meia/MPEGConvert.h>
#include "tables.h"

///////////////////////////////////////////////////////////////////////////

using namespace nsVDMPEGTables;

extern "C" void asm_YUVtoRGB16_row(
		void *ARGB1_pointer,
		void *ARGB2_pointer,
		const void *Y1_pointer,
		const void *Y2_pointer,
		const void *U_pointer,
		const void *V_pointer,
		long width
		);

extern "C" void asm_YUVtoRGB24_row(
		void *ARGB1_pointer,
		void *ARGB2_pointer,
		const void *Y1_pointer,
		const void *Y2_pointer,
		const void *U_pointer,
		const void *V_pointer,
		long width
		);

extern "C" void asm_YUVtoRGB32_row(
		void *ARGB1_pointer,
		void *ARGB2_pointer,
		const void *Y1_pointer,
		const void *Y2_pointer,
		const void *U_pointer,
		const void *V_pointer,
		long width
		);

namespace nsVDMPEGConvertScalar {

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
			asm_YUVtoRGB16_row(dst1, dst2, srcY1, srcY2, srcCb, srcCr, mbw*8);
			dst1 += dpitch;
			dst2 += dpitch;
			srcY1 += ypitch;
			srcY2 += ypitch;
			srcCr += cpitch;
			srcCb += cpitch;
		} while((height-=2)>0);
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
			asm_YUVtoRGB24_row(dst1, dst2, srcY1, srcY2, srcCb, srcCr, mbw*8);
			dst1 += dpitch;
			dst2 += dpitch;
			srcY1 += ypitch;
			srcY2 += ypitch;
			srcCr += cpitch;
			srcCb += cpitch;
		} while((height-=2)>0);
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
			asm_YUVtoRGB32_row(dst1, dst2, srcY1, srcY2, srcCb, srcCr, mbw*8);
			dst1 += dpitch;
			dst2 += dpitch;
			srcY1 += ypitch;
			srcY2 += ypitch;
			srcCr += cpitch;
			srcCb += cpitch;
		} while((height-=2)>0);
	}
};

///////////////////////////////////////////////////////////////////////////

namespace nsVDMPEGConvertReference {
	extern void DecodeUYVY(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height);
	extern void DecodeYUYV(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height);
	extern void DecodeYVYU(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height);
	extern void DecodeY41P(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height);
	extern void DecodeRGB16(void *dst, ptrdiff_t dpitch, const unsigned char *srcY, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height);
}

const struct VDMPEGConverterSet g_VDMPEGConvert_scalar = {
	nsVDMPEGConvertReference::DecodeUYVY,
	nsVDMPEGConvertReference::DecodeYUYV,
	nsVDMPEGConvertReference::DecodeYVYU,
	nsVDMPEGConvertReference::DecodeY41P,
	nsVDMPEGConvertScalar::DecodeRGB15,
	nsVDMPEGConvertReference::DecodeRGB16,
	nsVDMPEGConvertScalar::DecodeRGB24,
	nsVDMPEGConvertScalar::DecodeRGB32,
};
