//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2004 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "stdafx.h"
#include <vd2/system/error.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/vdstl.h>
#include <vd2/Meia/MPEGIDCT.h>
#include <vector>
#include "imagejpegdec.h"

namespace {
	const int zigzag[64] = {		// the reverse zigzag scan order
		 0,  1,  8, 16,  9,  2,  3, 10,
		17, 24, 32, 25, 18, 11,  4,  5,
		12, 19, 26, 33, 40, 48, 41, 34,
		27, 20, 13,  6,  7, 14, 21, 28,
		35, 42, 49, 56, 57, 50, 43, 36,
		29, 22, 15, 23, 30, 37, 44, 51,
		58, 59, 52, 45, 38, 31, 39, 46,
		53, 60, 61, 54, 47, 55, 62, 63,
	};
}

class VDJPEGDecoder : public IVDJPEGDecoder {
public:
	VDJPEGDecoder();
	~VDJPEGDecoder();

	void Begin(const void *src, uint32 srclen);
	void DecodeHeader(int& w, int& h);
	void DecodeImage(void *dst, ptrdiff_t dstpitch, int format);
	void End();

protected:
	void Parse();
	int ParseLength();
	void ParseQuantTables();
	void ParseHuffmanTables();
	void ParseFrameHeader();
	void ParseScanHeader();
	void ParseMCU_prescaled32(int mcusize);
	void ParseMCU_unscaled16(int mcusize);
	int ParseCoefficient(int bits);
	uint8 ParseHuffmanCode(const uint8 *pTable);
	int GetBits(int count);
	void Refill();

	enum {
		kSOF0	= 0xc0,		// start-of-frame, baseline scan
		kSOF1	= 0xc1,
		kSOF2	= 0xc2,
		kSOF3	= 0xc3,
		kDHT	= 0xc4,		// define huffman tables
		kSOF5	= 0xc5,
		kSOF6	= 0xc6,
		kSOF7	= 0xc7,
		kSOF9	= 0xc9,
		kSOF10	= 0xca,
		kSOF11	= 0xcb,
		kSOF13	= 0xcd,
		kSOF14	= 0xce,
		kSOF15	= 0xcf,
		kRST0	= 0xd0,
		kSOI	= 0xd8,		// start of image
		kEOI	= 0xd9,		// end of image
		kSOS	= 0xda,		// start of scan
		kDQT	= 0xdb,		// define quantization tables
		kDRI	= 0xdd,		// define restart interval
		kAPP0	= 0xe0,
		kAPPF	= 0xef,
		kCOM	= 0xfe
	};

	uint32 mBitHeap;
	int mBitCount;

	const uint8 *mpSrc, *mpSrcEnd, *mpSrcStart;

	uint32	mWidth, mHeight;
	uint32	mBlockShiftX, mBlockShiftY;
	uint32	mRestartInterval;

	const VDMPEGIDCTSet	*mpIDCT;
	const int *mpScan;

	sint32		mQuant[4][64];
	const uint8	*mpHuffTab[2][4];
	uint32		mACAccelLimit[4];

	sint32 *mpQuant[3];

	struct AccelEntry {
		uint8	bits;
		uint8	code;
	};

	struct Block {
		const uint8 *mpDCTable;
		const uint8 *mpACTable;
		const AccelEntry *mpACTableAccel;
		uint32		mACTableAccelLimit;
		const int *mpQuant;
		int			*mpDC;
		uint8		*mpDst;
		ptrdiff_t	mPitch;
		int			mXStep;
		ptrdiff_t	mYStep;
	};

	Block mBlocks[10];

	struct Component {
		vdblock<uint8>		mPlane;
		ptrdiff_t			mPitch;
		int					mDC;
		int					mPerMCUW;
		int					mPerMCUH;
		int					mXStep;
		ptrdiff_t			mYStep;
		uint8				mId;
	};

	int			mCompCount;
	Component	mComponents[3];

	AccelEntry	mACAccel[4][256];
};

IVDJPEGDecoder *VDCreateJPEGDecoder() {
	return new VDJPEGDecoder;
}

VDJPEGDecoder::VDJPEGDecoder() {
}

VDJPEGDecoder::~VDJPEGDecoder() {
}

void VDJPEGDecoder::Begin(const void *src, uint32 srclen) {
#ifdef _M_AMD64
	mpIDCT = &g_VDMPEGIDCT_sse2;
#else
	long cpuflags = CPUGetEnabledExtensions();
	if (cpuflags & CPUF_SUPPORTS_SSE2)
		mpIDCT = &g_VDMPEGIDCT_sse2;
	else if (cpuflags & CPUF_SUPPORTS_INTEGER_SSE)
		mpIDCT = &g_VDMPEGIDCT_isse;
	else if (cpuflags & CPUF_SUPPORTS_MMX)
		mpIDCT = &g_VDMPEGIDCT_mmx;
	else
		mpIDCT = &g_VDMPEGIDCT_scalar;
#endif

	mpScan = mpIDCT->pAltScan ? mpIDCT->pAltScan : zigzag;

	mpSrc = mpSrcStart = (const uint8 *)src;
	mpSrcEnd = mpSrc + srclen;
	mRestartInterval = 0;
	mWidth = mHeight = 0;
}

void VDJPEGDecoder::DecodeHeader(int& w, int& h) {
	Parse();

	if (!mWidth || !mHeight)
		throw MyError("JPEGDecoder: JPEG image data is missing.");

	w = mWidth;
	h = mHeight;
}

namespace {
	// JFIF YCbCr to RGB conversion:
	//
	//	R = Y + 1.402(Cr-128)						[-179,433]
	//	G = Y - 0.34414(Cb-128) - 0.71414(Cr-128)	[-134,390]
	//	B = Y + 1.772(Cb-128)						[-227,480]

	struct YCbCrToRGB {
		sint16 r_cr_tab[256];
		sint16 b_cb_tab[256];
		sint16 g_cr_tab[256];
		sint16 g_cb_tab[256];
		uint8 cliptab[708];

		YCbCrToRGB() {
			int i;

			memset(cliptab, 0, 227);
			memset(cliptab+227+256, 255, 224);

			for(i=0; i<256; ++i) {
				r_cr_tab[i] = (sint16)floor(0.5 + 1.402*(i-128));
				b_cb_tab[i] = (sint16)floor(0.5 + 1.772*(i-128));
				g_cr_tab[i] = (sint16)floor(0.5 - 0.71414*(i-128));
				g_cb_tab[i] = (sint16)floor(0.5 - 0.34414*(i-128));
				cliptab[i+227] = (uint8)i;
			}
		}
	} colorconv;

	void Scanout_XRGB1555_Y(void *dst0, const uint8 *ysrc, uint32 w) {
		uint16 *dst = (uint16 *)dst0;

		do {
			const uint8 y = *ysrc++;

			*dst++ = (uint16)((y>>3) * 0x0421);
		} while(--w);
	}

	void Scanout_RGB888_Y(void *dst0, const uint8 *ysrc, uint32 w) {
		uint8 *dst = (uint8 *)dst0;

		do {
			const uint8 y = *ysrc++;

			dst[0] = y;
			dst[1] = y;
			dst[2] = y;
			dst += 3;
		} while(--w);
	}

	void Scanout_XRGB8888_Y(void *dst0, const uint8 *ysrc, uint32 w) {
		uint8 *dst = (uint8 *)dst0;

		do {
			const uint8 y = *ysrc++;

			dst[0] = y;
			dst[1] = y;
			dst[2] = y;
			dst[3] = 255;
			dst += 4;
		} while(--w);
	}

	typedef void (*tpScanoutYRoutine)(void *, const uint8 *, uint32);

	void Convert_Y(void *dst, ptrdiff_t dstpitch, const uint8 *ysrc, ptrdiff_t ypitch, uint32 w, uint32 h, tpScanoutYRoutine pScanout) {
		do {
			pScanout(dst, ysrc, w);
			dst = (char *)dst + dstpitch;
			ysrc += ypitch;
		} while(--h);
	}

	void Scanout_XRGB1555_YCbCr(void *dst0, const uint8 *ysrc, const uint8 *cbsrc, const uint8 *crsrc, uint32 w) {
		uint16 *dst = (uint16 *)dst0;

		do {
			const uint8 y = *ysrc++;
			const uint8 cb = *cbsrc++;
			const uint8 cr = *crsrc++;

			const uint8 *ytab = colorconv.cliptab + 227 + y;
			uint32 r = ytab[colorconv.r_cr_tab[cr]];
			uint32 g = ytab[colorconv.g_cr_tab[cr] + colorconv.g_cb_tab[cb]];
			uint32 b = ytab[colorconv.b_cb_tab[cb]];

			*dst++ = (uint16)(((r & 0xf8) << 7) + ((g & 0xf8) << 2) + ((b & 0xf8) >> 3));
		} while(--w);
	}

	void Scanout_RGB888_YCbCr(void *dst0, const uint8 *ysrc, const uint8 *cbsrc, const uint8 *crsrc, uint32 w) {
		uint8 *dst = (uint8 *)dst0;

		do {
			const uint8 y = *ysrc++;
			const uint8 cb = *cbsrc++;
			const uint8 cr = *crsrc++;

			const uint8 *ytab = colorconv.cliptab + 227 + y;
			const uint8 r = ytab[colorconv.r_cr_tab[cr]];
			const uint8 g = ytab[colorconv.g_cr_tab[cr] + colorconv.g_cb_tab[cb]];
			const uint8 b = ytab[colorconv.b_cb_tab[cb]];

			dst[0] = b;
			dst[1] = g;
			dst[2] = r;
			dst += 3;
		} while(--w);
	}

	void Scanout_XRGB8888_YCbCr(void *dst0, const uint8 *ysrc, const uint8 *cbsrc, const uint8 *crsrc, uint32 w) {
		uint8 *dst = (uint8 *)dst0;

		do {
			const uint8 y = *ysrc++;
			const uint8 cb = *cbsrc++;
			const uint8 cr = *crsrc++;

			const uint8 *ytab = colorconv.cliptab + 227 + y;
			const uint8 r = ytab[colorconv.r_cr_tab[cr]];
			const uint8 g = ytab[colorconv.g_cr_tab[cr] + colorconv.g_cb_tab[cb]];
			const uint8 b = ytab[colorconv.b_cb_tab[cb]];

			dst[0] = b;
			dst[1] = g;
			dst[2] = r;
			dst[3] = 255;
			dst += 4;
		} while(--w);
	}

	typedef void (*tpScanoutRoutine)(void *, const uint8 *, const uint8 *, const uint8 *, uint32);

	void Convert_H1_to_H2(uint8 *dst, const uint8 *src, uint32 w) {
		int c, d;

		c = *dst++ = *src++;

		if (w > 1) {
			int w2 = w - 2;
			if (w2 > 0)
				do {
					d = c;
					c = *src++;
					*dst++ = (uint8)((3*d+c+2)>>2);
					*dst++ = (uint8)((3*c+d+2)>>2);
				} while((w2-=2) > 0);

			if (!w2)
				*dst = (uint8)((3*d+c+2)>>2);
		}
	}

	void Convert_V1_to_V2(uint8 *dst, const uint8 *src1, const uint8 *src3, uint32 w) {
		uint32 w4 = w >> 2;

		if (w4) {
			uint32 *dst4 = (uint32 *)dst;
			const uint32 *src41 = (const uint32 *)src1;
			const uint32 *src43 = (const uint32 *)src3;
			do {
				const uint32 a = *src43++;
				const uint32 b = *src41++;
				const uint32 ab = (a&b) + (((a^b)>>1)&0x7f7f7f7f);

				*dst4++ = (a|ab) - (((a^ab)>>1)&0x7f7f7f7f);
			} while(--w4);

			dst = (uint8 *)dst4;
			src1 = (const uint8 *)src41;
			src3 = (const uint8 *)src43;
		}

		w &= 3;

		if (w)
			do {
				*dst++ = (uint8)((*src1++ + 3 * *src3++ + 2) >> 2);
			} while(--w);
	}

	void Convert_H1V1(void *dst, ptrdiff_t dstpitch, const uint8 *ysrc, ptrdiff_t ypitch, const uint8 *cbsrc, const uint8 *crsrc, ptrdiff_t cpitch, uint32 w, uint32 h, tpScanoutRoutine pScanout) {
		do {
			pScanout(dst, ysrc, cbsrc, crsrc, w);
			dst = (char *)dst + dstpitch;
			ysrc += ypitch;
			cbsrc += cpitch;
			crsrc += cpitch;
		} while(--h);
	}

	void Convert_H1V2(void *dst, ptrdiff_t dstpitch, const uint8 *ysrc, ptrdiff_t ypitch, const uint8 *cbsrc, const uint8 *crsrc, ptrdiff_t cpitch, uint32 w, uint32 h, tpScanoutRoutine pScanout) {
		vdblock<uint8> buffer(w*2);
		uint8 *const cbtmp = buffer.data();
		uint8 *const crtmp = cbtmp + w;

		const uint8 *cbsrc2 = cbsrc;
		const uint8 *crsrc2 = crsrc;

		do {
			Convert_V1_to_V2(cbtmp, cbsrc2, cbsrc, w);
			Convert_V1_to_V2(crtmp, crsrc2, crsrc, w);
			pScanout(dst, ysrc, cbtmp, crtmp, w);
			ysrc += ypitch;
			dst = (char *)dst + dstpitch;

			if (!--h)
				break;

			cbsrc2 = cbsrc;
			crsrc2 = crsrc;

			if (h > 1) {
				cbsrc += cpitch;
				crsrc += cpitch;
			}

			Convert_V1_to_V2(cbtmp, cbsrc, cbsrc2, w);
			Convert_V1_to_V2(crtmp, crsrc, crsrc2, w);
			pScanout(dst, ysrc, cbtmp, crtmp, w);
			ysrc += ypitch;
			dst = (char *)dst + dstpitch;
		} while(--h);
	}

	void Convert_H2V1(void *dst, ptrdiff_t dstpitch, const uint8 *ysrc, ptrdiff_t ypitch, const uint8 *cbsrc, const uint8 *crsrc, ptrdiff_t cpitch, uint32 w, uint32 h, tpScanoutRoutine pScanout) {
		vdblock<uint8> buffer(w*2);
		uint8 *const cbtmp = buffer.data();
		uint8 *const crtmp = cbtmp + w;

		do {
			Convert_H1_to_H2(cbtmp, cbsrc, w);
			Convert_H1_to_H2(crtmp, crsrc, w);
			pScanout(dst, ysrc, cbtmp, crtmp, w);
			dst = (char *)dst + dstpitch;
			ysrc += ypitch;
			cbsrc += cpitch;
			crsrc += cpitch;
		} while(--h);
	}

	void Convert_H2V2(void *dst, ptrdiff_t dstpitch, const uint8 *ysrc, ptrdiff_t ypitch, const uint8 *cbsrc, const uint8 *crsrc, ptrdiff_t cpitch, uint32 w, uint32 h, tpScanoutRoutine pScanout) {
		const uint32 wh = (w+1)>>1;
		vdblock<uint8> buffer((wh+w)*2);
		uint8 *const cbtmp = buffer.data();
		uint8 *const crtmp = cbtmp + wh;
		uint8 *const cbtmp2 = crtmp + wh;
		uint8 *const crtmp2 = cbtmp2 + w;

		const uint8 *cbsrcprev = cbsrc;
		const uint8 *crsrcprev = crsrc;

		do {
			Convert_V1_to_V2(cbtmp, cbsrcprev, cbsrc, wh);
			Convert_H1_to_H2(cbtmp2, cbtmp, w);
			Convert_V1_to_V2(crtmp, crsrcprev, crsrc, wh);
			Convert_H1_to_H2(crtmp2, crtmp, w);
			pScanout(dst, ysrc, cbtmp2, crtmp2, w);
			ysrc += ypitch;
			dst = (char *)dst + dstpitch;

			if (!--h)
				break;

			cbsrcprev = cbsrc;
			crsrcprev = crsrc;

			if (h > 1) {
				cbsrc += cpitch;
				crsrc += cpitch;
			}

			Convert_V1_to_V2(cbtmp, cbsrc, cbsrcprev, wh);
			Convert_H1_to_H2(cbtmp2, cbtmp, w);
			Convert_V1_to_V2(crtmp, crsrc, crsrcprev, wh);
			Convert_H1_to_H2(crtmp2, crtmp, w);
			pScanout(dst, ysrc, cbtmp2, crtmp2, w);
			ysrc += ypitch;
			dst = (char *)dst + dstpitch;
		} while(--h);
	}
};

void VDJPEGDecoder::DecodeImage(void *dst, ptrdiff_t dstpitch, int format) {
	Parse();

	Component& yComp = mComponents[0];
	Component& cbComp = mComponents[1];
	Component& crComp = mComponents[2];

	VDASSERT(cbComp.mPitch == crComp.mPitch);

	const uint8 *ysrc = &yComp.mPlane[0];
	const ptrdiff_t ypitch = yComp.mPitch;

	const uint8 *cbsrc = &cbComp.mPlane[0];
	const uint8 *crsrc = &crComp.mPlane[0];
	const ptrdiff_t cpitch = cbComp.mPitch;

	if (mCompCount == 1) {
		switch(format) {
		case kFormatXRGB1555:
			Convert_Y(dst, dstpitch, ysrc, ypitch, mWidth, mHeight, Scanout_XRGB1555_Y);
			break;
		case kFormatRGB888:
			Convert_Y(dst, dstpitch, ysrc, ypitch, mWidth, mHeight, Scanout_RGB888_Y);
			break;
		case kFormatXRGB8888:
			Convert_Y(dst, dstpitch, ysrc, ypitch, mWidth, mHeight, Scanout_XRGB8888_Y);
			break;
		default:
			VDASSERT(false);
			return;
		}
	} else {
		tpScanoutRoutine pScanout;

		switch(format) {
		case kFormatXRGB1555:
			pScanout = Scanout_XRGB1555_YCbCr;
			break;
		case kFormatRGB888:
			pScanout = Scanout_RGB888_YCbCr;
			break;
		case kFormatXRGB8888:
			pScanout = Scanout_XRGB8888_YCbCr;
			break;
		default:
			VDASSERT(false);
			return;
		}

		switch(mBlockShiftX + 2*mBlockShiftY) {
		case 0:
			Convert_H1V1(dst, dstpitch, ysrc, ypitch, cbsrc, crsrc, cpitch, mWidth, mHeight, pScanout);
			break;
		case 1:
			Convert_H2V1(dst, dstpitch, ysrc, ypitch, cbsrc, crsrc, cpitch, mWidth, mHeight, pScanout);
			break;
		case 2:
			Convert_H1V2(dst, dstpitch, ysrc, ypitch, cbsrc, crsrc, cpitch, mWidth, mHeight, pScanout);
			break;
		case 3:
			Convert_H2V2(dst, dstpitch, ysrc, ypitch, cbsrc, crsrc, cpitch, mWidth, mHeight, pScanout);
			break;
		default:
			VDASSERT(false);
		}
	}
}

void VDJPEGDecoder::End() {
}

void VDJPEGDecoder::Parse() {
	while(mpSrc < mpSrcEnd) {
		// find marker start
		while(*mpSrc++ != 0xff)
			if (mpSrc >= mpSrcEnd)
				return;

		// find marker end
		uint8 c;
		for(;;) {
			if (mpSrc >= mpSrcEnd)
				return;
			c = *mpSrc++;
			if (c != 0xff)
				break;
		};

		// what marker type did we get?
		switch(c) {
		case kDQT:
			ParseQuantTables();
			break;
		case kDHT:
			ParseHuffmanTables();
			break;
		case kSOF0:
			ParseFrameHeader();
			return;
		case kSOF1:
		case kSOF2:
		case kSOF3:
		case kSOF5:
		case kSOF6:
		case kSOF7:
		case kSOF9:
		case kSOF10:
		case kSOF11:
		case kSOF13:
		case kSOF14:
		case kSOF15:
			throw MyError("JPEGDecoder: Unsupported encoding (SOF%d marker found).", c  - kSOF0);
			break;
		case kSOS:
			ParseScanHeader();
			break;
		case kDRI:
			{
				int len = ParseLength();
				if (len != 2)
					throw MyError("JPEGDecoder: Bad DRI marker.");
				mRestartInterval = (mpSrc[0] << 8) + mpSrc[1];
			}
			break;
		case kEOI:
			return;
		default:
			if (c >= kAPP0 && c <= kAPPF || c == kCOM) {
				int len = ParseLength();

				mpSrc += len;
			}
			break;
		}
	}
}

int VDJPEGDecoder::ParseLength() {
	size_t left = mpSrcEnd - mpSrc;

	if (left >= 2) {
		int len = mpSrc[1] + (mpSrc[0] << 8);

		if (len >= 2 && left >= len) {
			mpSrc += 2;
			return len - 2;
		}
	}

	throw MyError("JPEGDecoder: Damaged JPEG block at offset %4x", mpSrc - mpSrcStart);
}

void VDJPEGDecoder::ParseQuantTables() {
	int len = ParseLength();

	while(len > 0) {
		uint8 type = *mpSrc++;
		--len;

		// high 4 bits indicate precision (0-1)
		// low 4 bits indicate table target (0-3)
		if (type & 0xec)
			goto damaged;

		int *const q = mQuant[type & 3];

		if (type & 0x10) {		// 16-bit Q table
			len -= 128;
			if (len < 0)
				goto damaged;

			for(int i=0; i<64; ++i) {
				q[i] = (mpSrc[0] << 8) + mpSrc[1];
				mpSrc += 2;
			}
		} else {				// 8-bit Q table
			len -= 64;
			if (len < 0)
				goto damaged;

			for(int i=0; i<64; ++i)
				q[i] = *mpSrc++;
		}

		if (mpIDCT->pPrescaler)
			for(int j=0; j<64; ++j)
				q[j] = (q[j] * mpIDCT->pPrescaler[mpScan[j]] + 128) >> 8;
	}
	return;

damaged:
	throw MyError("JPEGDecoder: Damaged JPEG quantization table at offset %04x", mpSrc - mpSrcStart);
}

void VDJPEGDecoder::ParseHuffmanTables() {
	int len = ParseLength();

	while(len > 0) {
		len -= 17;
		if (len < 0)
			goto damaged;

		// Top 4 bits indicate target (0=DC, 1=AC), bottom 4 bits indicate table.
		const uint8 target = *mpSrc++;
		if (target & 0xec)
			goto damaged;

		const uint8 *const pCounts = mpSrc;
		mpSrc += 16;

		// Validate huffman table.  There must be room in the chunk for all of the code points
		// enumerated in the counts and code point FFFF must not be allocated
		uint32 occupied = 0;
		int codes = 0;
		for(int blen=0; blen<16; ++blen) {
			const int v = pCounts[blen];

			occupied += (0x8000 >> blen) * v;
			codes += v;
		}

		if (occupied > 0xFFFF || len < codes)
			goto damaged;		// *coff*BS*coff*

		len -= codes;
		mpSrc += codes;

		// Assign table pointer -- we currently use the DHT directly.
		mpHuffTab[(target>>4)&1][target & 3] = pCounts;

		// If this is an AC table, compute the AC acceleration table.
		uint32 val = 0;

		for(; val < 0xFF000000; val += 0x01000000) {
			uint32 acc = val >> 16;
			const uint8 *pCodes = pCounts + 16;

			for(int i=0; i<8; ++i) {
				int cnt = pCounts[i];
				uint32 cmp = cnt << 16;

				acc += acc;
				if (acc < cmp) {
					uint8 v = pCodes[acc>>16];

					AccelEntry& ae = mACAccel[target & 3][val>>24];

					ae.bits = (uint8)(i + 1);
					ae.code = v;
					goto next_code;
				}

				acc -= cmp;
				pCodes += cnt;
			}
			break;
next_code:
			;
		}

		mACAccelLimit[target & 3] = val - 1;
	}
	return;

damaged:
	throw MyError("JPEGDecoder: Damaged JPEG huffman table at offset %04x", mpSrc - mpSrcStart);
}

void VDJPEGDecoder::ParseFrameHeader() {
	int len = ParseLength();

	if (len < 9)
		throw MyError("JPEGDecoder: Malformed frame header at offset %04x", mpSrc - mpSrcStart);

	if (mpSrc[0] != 8)
		throw MyError("JPEGDecoder: Image must be 8-bit precision");

	mWidth = (mpSrc[3]<<8) + mpSrc[4];
	mHeight = (mpSrc[1]<<8) + mpSrc[2];

	if (!mWidth || !mHeight)
		throw MyError("JPEGDecoder: Image has invalid dimensions");

	const int ncomps = mpSrc[5];

	mCompCount = ncomps;

	if (ncomps != 1 && ncomps != 3)
		throw MyError("JPEGDecoder: Image is not in YCbCr format");

	if (len != 6 + ncomps*3)
		throw MyError("JPEGDecoder: Malformed frame header at offset %04x", mpSrc - mpSrcStart);

	const uint8 *ycomp = mpSrc + 6;

	// Init Y component.
	//
	// For some reason, the RAZR V3 phone encodes Exif images where the YCbCr components are
	// labeled 0, 1, and 2 instead of 1, 2, and 3. This looks wrong according to the Exif
	// spec, but oh well....
	if (ycomp[0] != 1 && ycomp[0] != 0)
		throw MyError("JPEGDecoder: Image is not in Y or YCbCr format");

	mpQuant[0] = mQuant[ycomp[2]];
	int yfactor = ycomp[1] & 0x0f;
	int xfactor = (ycomp[1] & 0xf0) >> 4;

	mBlockShiftX = xfactor - 1;
	mBlockShiftY = yfactor - 1;

	int xblocks = ((mWidth - 1) >> (3 + mBlockShiftX)) + 1;
	int yblocks = ((mHeight - 1) >> (3 + mBlockShiftY)) + 1;

	mComponents[0].mPitch = xblocks << (3 + mBlockShiftX);
	mComponents[0].mPlane.resize(mComponents[0].mPitch * (yblocks << (3 + mBlockShiftY)));
	mComponents[0].mPerMCUW = xfactor;
	mComponents[0].mPerMCUH = yfactor;
	mComponents[0].mId = ycomp[0];

	// init Cb/Cr if present
	if (ncomps == 3) {
		const uint8 *cbcomp = mpSrc + 9;
		const uint8 *crcomp = mpSrc + 12;

		if (cbcomp[0] != (uint8)(ycomp[0] + 1) || crcomp[0] != (uint8)(ycomp[0] + 2))
			throw MyError("JPEGDecoder: Image is not in Y or YCbCr format");

		// The second byte holds sampling factors, not subsampling factors... so 4:2:2
		// is represented by Y=21 and Cb=Cr=11, because there are two horizontal Y
		// samples for every chroma sample.
		if (((ycomp[1] - 0x11) & 0xee))
			throw MyError("JPEGDecoder: Unsupported luma sampling format");

		if (cbcomp[1] != crcomp[1] || cbcomp[1] != 0x11)
			throw MyError("JPEGDecoder: Unsupported chroma subsampling format");

		if ((ycomp[2] | cbcomp[2] | crcomp[2]) & ~3)
			throw MyError("JPEGDecoder: Bad frame header");

		mpQuant[1] = mQuant[cbcomp[2]];
		mpQuant[2] = mQuant[crcomp[2]];

		mComponents[1].mPitch = xblocks << 3;
		mComponents[1].mPlane.resize(mComponents[1].mPitch * (yblocks << 3));
		mComponents[1].mPerMCUW = 1;
		mComponents[1].mPerMCUH = 1;
		mComponents[1].mId = cbcomp[0];

		mComponents[2].mPitch = xblocks << 3;
		mComponents[2].mPlane.resize(mComponents[2].mPitch * (yblocks << 3));
		mComponents[2].mPerMCUW = 1;
		mComponents[2].mPerMCUH = 1;
		mComponents[2].mId = crcomp[0];
	}

	for(int i=0; i<ncomps; ++i) {
		mComponents[i].mXStep = mComponents[i].mPerMCUW << 3;
		mComponents[i].mYStep = ((mComponents[i].mPerMCUH << 3) * mComponents[i].mPitch) - mComponents[i].mXStep * xblocks;
	}

	mpSrc += len;
}

void VDJPEGDecoder::ParseScanHeader() {
	int len = ParseLength();
	int components = 0;

	if (len >= 4) {
		components = mpSrc[0];

		if (components < 1 || components > 3)
			throw MyError("JPEGDecoder: Malformed scan header at offset %04x", mpSrc - mpSrcStart);
	}

	if (len != 4 + 2*components)
		throw MyError("JPEGDecoder: Malformed scan header at offset %04x", mpSrc - mpSrcStart);

	int blockw = ((mWidth -1) >> (mBlockShiftX + 3)) + 1;
	int blockh = ((mHeight-1) >> (mBlockShiftY + 3)) + 1;
	int mcusize = 0;

	int dcbase = 128*8;
	
	if (mpIDCT->pPrescaler)
		dcbase = (dcbase * mpIDCT->pPrescaler[mpScan[0]] + 128) >> 8;

	for(int i=0; i<components; ++i) {
		int compID = mpSrc[1 + 2*i];
		const uint8 tablesel = mpSrc[2+2*i];

		int comp = -1;
		for(int j = 0; j < mCompCount; ++j) {
			if (mComponents[j].mId == compID) {
				comp = j;
				break;
			}
		}

		if ((tablesel & 0xcc) || comp < 0)
			throw MyError("JPEGDecoder: Malformed scan header at offset %04x", mpSrc - mpSrcStart);

		const uint8 *pDCTable = mpHuffTab[0][(tablesel>>4) & 15];
		const uint8 *pACTable = mpHuffTab[1][tablesel & 15];
		const AccelEntry *pACTableAccel = mACAccel[tablesel & 15];
		const uint32 nACTableLimit = mACAccelLimit[tablesel & 15];

		Component& compinfo = mComponents[comp];
		compinfo.mDC = dcbase;

		for(int y=0; y<compinfo.mPerMCUH; ++y) {
			for(int x=0; x<compinfo.mPerMCUW; ++x) {
				Block& blk = mBlocks[mcusize++];

				blk.mpACTable	= pACTable;
				blk.mpDCTable	= pDCTable;
				blk.mpACTableAccel	= pACTableAccel;
				blk.mACTableAccelLimit	= nACTableLimit;
				blk.mpQuant		= mpQuant[comp];
				blk.mpDC		= &compinfo.mDC;
				blk.mpDst		= &compinfo.mPlane[(x + (y * compinfo.mPitch)) << 3];
				blk.mPitch		= compinfo.mPitch;
				blk.mXStep		= compinfo.mXStep;
				blk.mYStep		= compinfo.mYStep;
			}
		}
	}

	mpSrc += len;

	// fill
	mBitHeap = 0;
	mBitCount = 0;

	Refill();

	uint32 restartCounter = mRestartInterval ? mRestartInterval + 1 : 0;
	uint8 nextRST = kRST0;
	const uint8 *pLastECS = mpSrc;

	for(int y=0; y<blockh; ++y) {
		for(int x=0; x<blockw; ++x) {
			if (!--restartCounter) {
				restartCounter = mRestartInterval;
				mpSrc -= mBitCount >> 3;

				bool found = false;
				if (mpSrc - pLastECS > 4) {
					mpSrc -= 4;

					for(int i=0; i<8; ++i) {
						if (mpSrc[i] == 0xFF && mpSrc[i+1] == nextRST) {
							found = true;
							mpSrc += i;
							break;
						}
					}
				}

				if (!found) {
					mpSrc = pLastECS;
					
					while(mpSrc < mpSrcEnd-1) {
						if (mpSrc[0] == 0xFF && mpSrc[1] == nextRST) {
							found = true;
							break;
						}
						++mpSrc;
					}

					if (!found)
						throw MyError("JPEGDecoder: Derailed while trying to find RST.");
				}
				mpSrc += 2;

				pLastECS = mpSrc;
				mBitHeap = 0;
				mBitCount = 0;
				Refill();
				nextRST = (uint8)(kRST0 + ((nextRST+1) & 7));

				mComponents[0].mDC = mComponents[1].mDC = mComponents[2].mDC = dcbase;
			}

			if (mpIDCT->pPrescaler)
				ParseMCU_prescaled32(mcusize);
			else
				ParseMCU_unscaled16(mcusize);
		}

		for(int j=0; j<mcusize; ++j) {
			Block& blk = mBlocks[j];

			blk.mpDst += blk.mYStep;
		}
	}

	mBitHeap >>= -(int)mBitCount & 7;
	mpSrc -= mBitCount >> 3;
	for(int j=0; j<4; ++j) {
		if ((uint8)mBitHeap == 0xff)
			--mpSrc;
		mBitHeap >>= 8;
	}

#ifdef _M_AMD64
	_mm_sfence();
#else
	if (MMX_enabled)
		__asm emms
	if (ISSE_enabled)
		__asm sfence
#endif
}

void VDJPEGDecoder::ParseMCU_prescaled32(int mcusize) {
	for(int i=0; i<mcusize; ++i) {
		Block& blk = mBlocks[i];
		const int *const pQuant = blk.mpQuant;

		int coeff[64]={0};

		int dcbits = ParseHuffmanCode(blk.mpDCTable);

		if (dcbits) {
			int dcdelta = ParseCoefficient(dcbits);

			*blk.mpDC += dcdelta * pQuant[0];
		}
		coeff[0] = *blk.mpDC;

		const uint8 *const pACTable = blk.mpACTable;
		const AccelEntry *const pACTableAccel = blk.mpACTableAccel;
		const uint32 nACTableAccelLimit = blk.mACTableAccelLimit;
		int idx = 1;

		while(idx < 64) {
			uint8 rs;
			
			if (mBitHeap < nACTableAccelLimit) {
				const AccelEntry& ae = pACTableAccel[mBitHeap >> 24];

				Refill();
				if (mBitCount < ae.bits)
					throw MyError("JPEGDecoder: Huffman decoding error");
				mBitCount -= ae.bits;
				mBitHeap <<= ae.bits;

				rs = ae.code;
			} else
				rs = ParseHuffmanCode(pACTable);

			if (!rs)
				break;

			if (rs == 0xf0) {
				idx += 16;
				continue;
			}

			idx += rs >> 4;
			if (idx >= 64) {
				idx = 64;
				break;
			}

			coeff[mpScan[idx]] = ParseCoefficient(rs & 15) * pQuant[idx];
			++idx;
		}

		// IDCT

		uint8 *p = blk.mpDst;
		const ptrdiff_t pitch = blk.mPitch;

		mpIDCT->pIntra(p, pitch, coeff, idx-1);

		blk.mpDst += blk.mXStep;
	}
}

void VDJPEGDecoder::ParseMCU_unscaled16(int mcusize) {
	for(int i=0; i<mcusize; ++i) {
		Block& blk = mBlocks[i];
		const int *const pQuant = blk.mpQuant;

		__declspec(align(16)) sint16 coeff[64]={0};

		int dcbits = ParseHuffmanCode(blk.mpDCTable);

		if (dcbits) {
			int dcdelta = ParseCoefficient(dcbits);
			*blk.mpDC += dcdelta * pQuant[0];
		}
		coeff[0] = (sint16)*blk.mpDC;

		const uint8 *const pACTable = blk.mpACTable;
		const AccelEntry *const pACTableAccel = blk.mpACTableAccel;
		const uint32 nACTableAccelLimit = blk.mACTableAccelLimit;
		int idx = 1;

		while(idx < 64) {
			uint8 rs;

			if (mBitHeap < nACTableAccelLimit) {
				const AccelEntry& ae = pACTableAccel[mBitHeap >> 24];

				Refill();
				if (mBitCount < ae.bits)
					throw MyError("JPEGDecoder: Huffman decoding error");

				mBitCount -= ae.bits;
				mBitHeap <<= ae.bits;

				rs = ae.code;
			} else
				rs = ParseHuffmanCode(pACTable);

			if (!rs)
				break;

			idx += rs >> 4;
			if (idx >= 64) {
				idx = 64;
				break;
			}

			if (rs & 15)
				coeff[mpScan[idx]] = (sint16)(ParseCoefficient(rs & 15) * pQuant[idx]);

			++idx;
		}

		// IDCT

		uint8 *p = blk.mpDst;
		const ptrdiff_t pitch = blk.mPitch;

		mpIDCT->pIntra(p, pitch, coeff, idx-1);

		blk.mpDst += blk.mXStep;
	}
}

int VDJPEGDecoder::ParseCoefficient(int bits) {
	Refill();

	if (mBitCount < bits)
		throw MyError("JPEGDecoder: Corrupted image");

	int q = (sint32)~mBitHeap >> 31;
	int v = (q << bits) - q;

	v += mBitHeap >> (32-bits);

	mBitHeap <<= bits;
	mBitCount -= bits;

	return v;
}

uint8 VDJPEGDecoder::ParseHuffmanCode(const uint8 *pTable) {
	Refill();

	uint32 acc = mBitHeap >> 16;
	const uint8 *pCodes = pTable + 16;

	for(int i=0; i<16; ++i) {
		int cnt = pTable[i];
		uint32 cmp = cnt << 16;

		acc += acc;
		mBitHeap += mBitHeap;
		if (--mBitCount < 0)
			break;

		if (acc < cmp) {
			return pCodes[acc>>16];
		}

		acc -= cmp;
		pCodes += cnt;
	}

	throw MyError("JPEGDecoder: Huffman decoding error");
}

int VDJPEGDecoder::GetBits(int count) {
	if (!count)
		return 0;

	Refill();

	if (mBitCount < count)
		throw MyError("JPEGDecoder: Corrupted image");

	int v = mBitHeap >> (32-count);

	mBitHeap <<= count;
	mBitCount -= count;

	return v;
}

void VDJPEGDecoder::Refill() {
	while(mBitCount <= 24) {
		if (mpSrc >= mpSrcEnd)
			return;
		uint8 c = *mpSrc++;
		if (c == 0xff) {
			// We used to throw an error here, but it turns out some models of Ricoh digital
			// camera don't write out the final byte of the EOI marker, so instead we just
			// stop the bitstream.
			if (mpSrc >= mpSrcEnd)
				return;

			++mpSrc;
		}

		VDASSERT(!(mBitHeap << mBitCount));
		mBitHeap += (uint32)c << (24 - mBitCount);
		mBitCount += 8;
	}
}
