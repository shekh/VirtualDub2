#include <vd2/Meia/MPEGConvert.h>
#include "tables.h"

///////////////////////////////////////////////////////////////////////////

using namespace nsVDMPEGTables;

namespace nsVDMPEGConvertReference {

	void DecodeUYVY(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height) {
		unsigned char *dst1 = (unsigned char *)_dst;
		unsigned char *dst2 = (unsigned char *)_dst + dpitch;
		const unsigned char *srcY2 = srcY1 + ypitch;

		dpitch = dpitch*2 - mbw*16*2;
		ypitch = ypitch*2 - mbw*16;
		cpitch -= mbw*8;

		do {
			int w = mbw*8;

			if (height == 1) {
				srcY2 = srcY1;
				dst2 = dst1;
			}

			do {
				dst1[0] = srcCb[0];
				dst1[1] = srcY1[0];
				dst1[2] = srcCr[0];
				dst1[3] = srcY1[1];

				dst2[0] = srcCb[0];
				dst2[1] = srcY2[0];
				dst2[2] = srcCr[0];
				dst2[3] = srcY2[1];

				++srcCb;
				++srcCr;
				srcY1 += 2;
				srcY2 += 2;
				dst1 += 4;
				dst2 += 4;
			} while(--w);

			srcY1 += ypitch;
			srcY2 += ypitch;
			srcCr += cpitch;
			srcCb += cpitch;
			dst1 += dpitch;
			dst2 += dpitch;
		} while((height-=2) > 0);
	}

	void DecodeYUYV(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height) {
		unsigned char *dst1 = (unsigned char *)_dst;
		unsigned char *dst2 = (unsigned char *)_dst + dpitch;
		const unsigned char *srcY2 = srcY1 + ypitch;

		dpitch = dpitch*2 - mbw*16*2;
		ypitch = ypitch*2 - mbw*16;
		cpitch -= mbw*8;

		do {
			int w = mbw*8;

			if (height == 1) {
				srcY2 = srcY1;
				dst2 = dst1;
			}

			do {
				dst1[0] = srcY1[0];
				dst1[1] = srcCb[0];
				dst1[2] = srcY1[1];
				dst1[3] = srcCr[0];

				dst2[0] = srcY2[0];
				dst2[1] = srcCb[0];
				dst2[2] = srcY2[1];
				dst2[3] = srcCr[0];

				++srcCb;
				++srcCr;
				srcY1 += 2;
				srcY2 += 2;
				dst1 += 4;
				dst2 += 4;
			} while(--w);

			srcY1 += ypitch;
			srcY2 += ypitch;
			srcCr += cpitch;
			srcCb += cpitch;
			dst1 += dpitch;
			dst2 += dpitch;
		} while((height-=2)>0);
	}

	void DecodeYVYU(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height) {
		unsigned char *dst1 = (unsigned char *)_dst;
		unsigned char *dst2 = (unsigned char *)_dst + dpitch;
		const unsigned char *srcY2 = srcY1 + ypitch;

		dpitch = dpitch*2 - mbw*16*2;
		ypitch = ypitch*2 - mbw*16;
		cpitch -= mbw*8;

		do {
			int w = mbw*8;

			if (height == 1) {
				srcY2 = srcY1;
				dst2 = dst1;
			}

			do {
				dst1[0] = srcY1[0];
				dst1[1] = srcCr[0];
				dst1[2] = srcY1[1];
				dst1[3] = srcCb[0];

				dst2[0] = srcY2[0];
				dst2[1] = srcCr[0];
				dst2[2] = srcY2[1];
				dst2[3] = srcCb[0];

				++srcCb;
				++srcCr;
				srcY1 += 2;
				srcY2 += 2;
				dst1 += 4;
				dst2 += 4;
			} while(--w);

			srcY1 += ypitch;
			srcY2 += ypitch;
			srcCr += cpitch;
			srcCb += cpitch;
			dst1 += dpitch;
			dst2 += dpitch;
		} while((height-=2)>0);
	}

	// Byte order for DecodeY41P is U0Y0V0Y1 U4Y2V4Y3 Y4Y5Y6Y7

	void DecodeY41P(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height) {
		unsigned char *dst1 = (unsigned char *)_dst;
		unsigned char *dst2 = (unsigned char *)_dst + dpitch;
		const unsigned char *srcY2 = srcY1 + ypitch;

		dpitch = dpitch*2 - mbw*16*2;
		ypitch = ypitch*2 - mbw*16;
		cpitch -= mbw*8;

		do {
			int w = mbw*2;

			if (height == 1) {
				srcY2 = srcY1;
				dst2 = dst1;
			}

			do {
				// chroma

				dst1[0] = dst2[0] = (srcCb[0] + srcCb[1] + 1) >> 1;
				dst1[4] = dst2[4] = (srcCb[2] + srcCb[3] + 1) >> 1;

				dst1[2] = dst2[2] = (srcCr[0] + srcCr[1] + 1) >> 1;
				dst1[6] = dst2[6] = (srcCr[2] + srcCr[3] + 1) >> 1;

				dst1[ 1] = srcY1[0];
				dst1[ 3] = srcY1[1];
				dst1[ 5] = srcY1[2];
				dst1[ 7] = srcY1[3];
				dst1[ 8] = srcY1[4];
				dst1[ 9] = srcY1[5];
				dst1[10] = srcY1[6];
				dst1[11] = srcY1[7];

				dst2[ 1] = srcY2[0];
				dst2[ 3] = srcY2[1];
				dst2[ 5] = srcY2[2];
				dst2[ 7] = srcY2[3];
				dst2[ 8] = srcY2[4];
				dst2[ 9] = srcY2[5];
				dst2[10] = srcY2[6];
				dst2[11] = srcY2[7];

				srcCb += 4;
				srcCr += 4;
				srcY1 += 8;
				srcY2 += 8;
				dst1 += 12;
				dst2 += 12;
			} while(--w);

			srcY1 += ypitch;
			srcY2 += ypitch;
			srcCr += cpitch;
			srcCb += cpitch;
			dst1 += dpitch;
			dst2 += dpitch;
		} while((height-=2)>0);
	}

	// R = 1.164(Y-16) + 1.596(Cr-128)
	// G = 1.164(Y-16) - 0.813(Cr-128) - 0.391(Cb-128)
	// B = 1.164(Y-16) + 2.018(Cb-128)

	static const int y_coeff = (int)(1.164*65536 + 0.5);
	static const int cr_red_coeff = (int)(1.596*65536 + 0.5);
	static const int cr_grn_coeff = (int)(-0.813*65536 + 0.5);
	static const int cb_grn_coeff = (int)(-0.391*65536 + 0.5);
	static const int cb_blu_coeff = (int)(2.018*65536 + 0.5);

	void DecodeRGB15(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height) {
		unsigned short *dst1 = (unsigned short *)_dst;
		unsigned short *dst2 = (unsigned short *)((char *)_dst + dpitch);
		const unsigned char *srcY2 = srcY1 + ypitch;

		dpitch = dpitch*2 - mbw*16*2;
		ypitch = ypitch*2 - mbw*16;
		cpitch -= mbw*8;

		do {
			int w = mbw*8;

			if (height == 1) {
				srcY2 = srcY1;
				dst2 = dst1;
			}

			do {
				int cr = (int)*srcCr++ - 128;
				int cb = (int)*srcCb++ - 128;
				int cred = cr * cr_red_coeff;
				int cgrn = cr * cr_grn_coeff + cb * cb_grn_coeff;
				int cblu = cb * cb_blu_coeff;
				int y;

				y = y_coeff*((int)srcY1[0] - 16);
				*dst1++ = ((clip_table[(y + cblu + 32768 + 288*65536)>>16]&0xf8)>>3)
						+ ((clip_table[(y + cgrn + 32768 + 288*65536)>>16]&0xf8)<<2)
						+ ((clip_table[(y + cred + 32768 + 288*65536)>>16]&0xf8)<<7);

				y = y_coeff*((int)srcY1[1] - 16);
				*dst1++ = ((clip_table[(y + cblu + 32768 + 288*65536)>>16]&0xf8)>>3)
						+ ((clip_table[(y + cgrn + 32768 + 288*65536)>>16]&0xf8)<<2)
						+ ((clip_table[(y + cred + 32768 + 288*65536)>>16]&0xf8)<<7);

				y = y_coeff*((int)srcY2[0] - 16);
				*dst2++ = ((clip_table[(y + cblu + 32768 + 288*65536)>>16]&0xf8)>>3)
						+ ((clip_table[(y + cgrn + 32768 + 288*65536)>>16]&0xf8)<<2)
						+ ((clip_table[(y + cred + 32768 + 288*65536)>>16]&0xf8)<<7);

				y = y_coeff*((int)srcY2[1] - 16);
				*dst2++ = ((clip_table[(y + cblu + 32768 + 288*65536)>>16]&0xf8)>>3)
						+ ((clip_table[(y + cgrn + 32768 + 288*65536)>>16]&0xf8)<<2)
						+ ((clip_table[(y + cred + 32768 + 288*65536)>>16]&0xf8)<<7);

				srcY1 += 2;
				srcY2 += 2;
			} while(--w);

			srcY1 += ypitch;
			srcY2 += ypitch;
			srcCr += cpitch;
			srcCb += cpitch;
			dst1 = (unsigned short *)((char *)dst1 + dpitch);
			dst2 = (unsigned short *)((char *)dst2 + dpitch);
		} while((height-=2)>0);
	}

	void DecodeRGB16(void *dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height) {
		unsigned short *dst1 = (unsigned short *)dst;
		unsigned short *dst2 = (unsigned short *)((char *)dst + dpitch);
		const unsigned char *srcY2 = srcY1 + ypitch;

		dpitch = dpitch*2 - mbw*16*2;
		ypitch = ypitch*2 - mbw*16;
		cpitch -= mbw*8;

		do {
			int w = mbw*8;

			if (height == 1) {
				srcY2 = srcY1;
				dst2 = dst1;
			}

			do {
				int cr = (int)*srcCr++ - 128;
				int cb = (int)*srcCb++ - 128;
				int cred = cr * cr_red_coeff;
				int cgrn = cr * cr_grn_coeff + cb * cb_grn_coeff;
				int cblu = cb * cb_blu_coeff;
				int y;

				y = y_coeff*((int)srcY1[0] - 16);
				*dst1++ = ((clip_table[(y + cblu + 32768 + 288*65536)>>16]&0xf8)>>3)
						+ ((clip_table[(y + cgrn + 32768 + 288*65536)>>16]&0xfc)<<3)
						+ ((clip_table[(y + cred + 32768 + 288*65536)>>16]&0xf8)<<8);

				y = y_coeff*((int)srcY1[1] - 16);
				*dst1++ = ((clip_table[(y + cblu + 32768 + 288*65536)>>16]&0xf8)>>3)
						+ ((clip_table[(y + cgrn + 32768 + 288*65536)>>16]&0xfc)<<3)
						+ ((clip_table[(y + cred + 32768 + 288*65536)>>16]&0xf8)<<8);

				y = y_coeff*((int)srcY2[0] - 16);
				*dst2++ = ((clip_table[(y + cblu + 32768 + 288*65536)>>16]&0xf8)>>3)
						+ ((clip_table[(y + cgrn + 32768 + 288*65536)>>16]&0xfc)<<3)
						+ ((clip_table[(y + cred + 32768 + 288*65536)>>16]&0xf8)<<8);

				y = y_coeff*((int)srcY2[1] - 16);
				*dst2++ = ((clip_table[(y + cblu + 32768 + 288*65536)>>16]&0xf8)>>3)
						+ ((clip_table[(y + cgrn + 32768 + 288*65536)>>16]&0xfc)<<3)
						+ ((clip_table[(y + cred + 32768 + 288*65536)>>16]&0xf8)<<8);

				srcY1 += 2;
				srcY2 += 2;
			} while(--w);

			srcY1 += ypitch;
			srcY2 += ypitch;
			srcCr += cpitch;
			srcCb += cpitch;
			dst1 = (unsigned short *)((char *)dst1 + dpitch);
			dst2 = (unsigned short *)((char *)dst2 + dpitch);
		} while((height-=2)>0);
	}

	void DecodeRGB24(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height) {
		unsigned char *dst1 = (unsigned char *)_dst;
		unsigned char *dst2 = (unsigned char *)_dst + dpitch;
		const unsigned char *srcY2 = srcY1 + ypitch;

		dpitch = dpitch*2 - mbw*16*3;
		ypitch = ypitch*2 - mbw*16;
		cpitch -= mbw*8;

		do {
			int w = mbw*8;

			if (height == 1) {
				srcY2 = srcY1;
				dst2 = dst1;
			}

			do {
				int cr = (int)*srcCr++ - 128;
				int cb = (int)*srcCb++ - 128;
				int cred = cr * cr_red_coeff;
				int cgrn = cr * cr_grn_coeff + cb * cb_grn_coeff;
				int cblu = cb * cb_blu_coeff;
				int y;

				y = y_coeff*((int)srcY1[0] - 16);
				dst1[0] = clip_table[(y + cblu + 32768 + 288*65536)>>16];
				dst1[1] = clip_table[(y + cgrn + 32768 + 288*65536)>>16];
				dst1[2] = clip_table[(y + cred + 32768 + 288*65536)>>16];

				y = y_coeff*((int)srcY1[1] - 16);
				dst1[3] = clip_table[(y + cblu + 32768 + 288*65536)>>16];
				dst1[4] = clip_table[(y + cgrn + 32768 + 288*65536)>>16];
				dst1[5] = clip_table[(y + cred + 32768 + 288*65536)>>16];

				y = y_coeff*((int)srcY2[0] - 16);
				dst2[0] = clip_table[(y + cblu + 32768 + 288*65536)>>16];
				dst2[1] = clip_table[(y + cgrn + 32768 + 288*65536)>>16];
				dst2[2] = clip_table[(y + cred + 32768 + 288*65536)>>16];

				y = y_coeff*((int)srcY2[1] - 16);
				dst2[3] = clip_table[(y + cblu + 32768 + 288*65536)>>16];
				dst2[4] = clip_table[(y + cgrn + 32768 + 288*65536)>>16];
				dst2[5] = clip_table[(y + cred + 32768 + 288*65536)>>16];

				srcY1 += 2;
				srcY2 += 2;
				dst1 += 6;
				dst2 += 6;
			} while(--w);

			srcY1 += ypitch;
			srcY2 += ypitch;
			srcCr += cpitch;
			srcCb += cpitch;
			dst1 += dpitch;
			dst2 += dpitch;
		} while((height-=2)>0);
	}

	void DecodeRGB32(void *_dst, ptrdiff_t dpitch, const unsigned char *srcY1, ptrdiff_t ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t cpitch, int mbw, int height) {
		unsigned char *dst1 = (unsigned char *)_dst;
		unsigned char *dst2 = (unsigned char *)_dst + dpitch;
		const unsigned char *srcY2 = srcY1 + ypitch;

		dpitch = dpitch*2 - mbw*16*4;
		ypitch = ypitch*2 - mbw*16;
		cpitch -= mbw*8;

		do {
			int w = mbw*8;

			if (height == 1) {
				srcY2 = srcY1;
				dst2 = dst1;
			}

			do {
				int cr = (int)*srcCr++ - 128;
				int cb = (int)*srcCb++ - 128;
				int cred = cr * cr_red_coeff;
				int cgrn = cr * cr_grn_coeff + cb * cb_grn_coeff;
				int cblu = cb * cb_blu_coeff;
				int y;

				y = y_coeff*((int)srcY1[0] - 16);
				dst1[0] = clip_table[(y + cblu + 32768 + 288*65536)>>16];
				dst1[1] = clip_table[(y + cgrn + 32768 + 288*65536)>>16];
				dst1[2] = clip_table[(y + cred + 32768 + 288*65536)>>16];

				y = y_coeff*((int)srcY1[1] - 16);
				dst1[4] = clip_table[(y + cblu + 32768 + 288*65536)>>16];
				dst1[5] = clip_table[(y + cgrn + 32768 + 288*65536)>>16];
				dst1[6] = clip_table[(y + cred + 32768 + 288*65536)>>16];

				y = y_coeff*((int)srcY2[0] - 16);
				dst2[0] = clip_table[(y + cblu + 32768 + 288*65536)>>16];
				dst2[1] = clip_table[(y + cgrn + 32768 + 288*65536)>>16];
				dst2[2] = clip_table[(y + cred + 32768 + 288*65536)>>16];

				y = y_coeff*((int)srcY2[1] - 16);
				dst2[4] = clip_table[(y + cblu + 32768 + 288*65536)>>16];
				dst2[5] = clip_table[(y + cgrn + 32768 + 288*65536)>>16];
				dst2[6] = clip_table[(y + cred + 32768 + 288*65536)>>16];

				srcY1 += 2;
				srcY2 += 2;
				dst1 += 8;
				dst2 += 8;
			} while(--w);

			srcY1 += ypitch;
			srcY2 += ypitch;
			srcCr += cpitch;
			srcCb += cpitch;
			dst1 += dpitch;
			dst2 += dpitch;
		} while((height-=2)>0);
	}
};

///////////////////////////////////////////////////////////////////////////

const struct VDMPEGConverterSet g_VDMPEGConvert_reference = {
	nsVDMPEGConvertReference::DecodeUYVY,
	nsVDMPEGConvertReference::DecodeYUYV,
	nsVDMPEGConvertReference::DecodeYVYU,
	nsVDMPEGConvertReference::DecodeY41P,
	nsVDMPEGConvertReference::DecodeRGB15,
	nsVDMPEGConvertReference::DecodeRGB16,
	nsVDMPEGConvertReference::DecodeRGB24,
	nsVDMPEGConvertReference::DecodeRGB32,
};
