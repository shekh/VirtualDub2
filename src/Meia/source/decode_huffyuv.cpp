//	VirtualDub - Video processing and capture application
//	Video decoding library
//	Copyright (C) 1998-2008 Avery Lee
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

#include <vd2/system/binary.h>
#include <vd2/system/bitmath.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/debug.h>
#include <vd2/system/error.h>
#include <vd2/system/memory.h>
#include <vd2/system/vdstl.h>
#include <vd2/Meia/decode_huffyuv.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>

#include <map>
#include <deque>

#include <intrin.h>

#pragma intrinsic(__ll_lshift)

//#define SEARCH_BASED_DECODE

namespace {
	struct HuffmanDecodeTable {
		uint32 mBaseLen;

#ifdef SEARCH_BASED_DECODE
		const uint8 (*mCodeBase[32])[2];
		uint32 mCodeStart[32];
		uint32 mCodeLimit[32];
		uint8 mCodeTable[256][2];
#endif

		uintptr mBsrLenTable[32];
		uint8 mBsrShiftTable[32];

		// TODO: Find true limit.
		//
		// The justification for this limit is as follows: the maximum space waste occurs when
		// the difference in code lengths is the largest within the same leading bit position.
		// The worst case situation happens like this:
		//
		//	11
		//	101
		//	1001
		//	10001
		//	100001
		//	1000001
		//	...
		//
		// However, because Huffyuv requires codes to be allocated in order of descending code
		// length and because there are only 256 codes to allocate, there is a limit to how far
		// this can go, because eventually the 0xxxxxxx codes must be counted away, and they
		// must be counted away with encodings as least as long as the last code. This limits
		// the longest practical code above to about 100000001.
		//
		// I believe the lowest efficiency possible for this table is just over 50%, so somewhere
		// between 512 and 1024 entries would be the max. Since I don't have a proof, it's 1024
		// and the decoder routine checks for overflow instead.
		//
		uint8 mBsrCodeTable[1024*2];

		const uint8 *Init(const uint8 *src, uint32 len);
	};

	const uint8 *HuffmanDecodeTable::Init(const uint8 *src, uint32 len) {
		const uint8 *limit = src + len;
		uint8 tab[256];

		// decompress table of bit lengths per code
		int i=0;
		while(i < 256) {
			if (src >= limit)
				throw MyError("Decompression error: Invalid VLC table detected in Huffyuv format.");

			uint8 code = *src++;
			uint8 v = code & 31;
			uint8 count = code >> 5;

			if (!count) {
				if (src >= limit)
					throw MyError("Decompression error: Invalid VLC table detected in Huffyuv format.");

				count = *src++;
				if (!count)
					throw MyError("Decompression error: Invalid VLC table detected in Huffyuv format.");
			}

			if (count + i > 256)
				throw MyError("Decompression error: Invalid VLC table detected in Huffyuv format.");

			while(count--)
				tab[i++] = v;
		}

		uint32 total = 0;
		uint32 base = 0;
		uint8 *decdst = mBsrCodeTable;
		uint8 *declimit = mBsrCodeTable + sizeof mBsrCodeTable;

		int curdecidx = -1;

		for(int i=0; i<32; ++i)
			mBsrShiftTable[i] = 32;

		// assign bit patterns starting at 0 to codes by decreasing bit length
		i = 0;
		for(uint32 len=32; len >= 1; --len) {
			if (!i)
				mBaseLen = len;

#ifdef SEARCH_BASED_DECODE
			mCodeBase[len - 1] = mCodeTable + i;
			mCodeStart[len - 1] = base;
#endif

			uint32 inc = 0x80000000 >> (len - 1);

			uint32 prevBase = base;

			for(int j=0; j<256; ++j) {
				if (tab[j] == len) {
					if (i >= 256)
						throw MyError("Decompression error: Invalid VLC table detected in Huffyuv format.");

#ifdef SEARCH_BASED_DECODE
					mCodeTable[i][0] = j;
					mCodeTable[i][1] = len;
#endif
					++i;

					int firstidx = VDFindHighestSetBit(base | 1);
					base += inc;

					int decidx = VDFindHighestSetBit((base - 1) | 1);

					int basedecidx = curdecidx;
					while(curdecidx < decidx) {
						if (curdecidx >= 0) {
							int decrepeat = 1 << (32 - mBsrShiftTable[curdecidx] - len);
							if (declimit - decdst < 2*decrepeat)
								throw MyError("Internal error: Insufficient space for Huffman decoding table.");

							while(decrepeat--) {
								*decdst++ = (uint8)j;
								*decdst++ = (uint8)len;
							}
						}

						++curdecidx;
						unsigned shift = mBsrShiftTable[curdecidx];
						if (shift > 32-len) {
							shift = 32-len;
							mBsrShiftTable[curdecidx] = 32-len;
							mBsrLenTable[curdecidx] = (uintptr)decdst - 2*((1U << curdecidx) >> shift);
						}
					}

					int decrepeat = 1 << (32 - mBsrShiftTable[curdecidx] - len);
					if (declimit - decdst < 2*decrepeat)
						throw MyError("Internal error: Insufficient space for Huffman decoding table.");

					while(decrepeat--) {
						*decdst++ = (uint8)j;
						*decdst++ = (uint8)len;
					}

#if 0
					for(uint32 k=base-inc; k != base; ++k) {
						int idx = VDFindHighestSetBit(k | 1);

						uint8 code = *(const char *)(mBsrLenTable[idx] + 2*(k >> mBsrShiftTable[idx]));
						VDASSERT(code == j);
					}
#endif

					if (base && base < prevBase)
						throw MyError("Decompression error: Invalid VLC table detected in Huffyuv format.");
				}
			}

#ifdef SEARCH_BASED_DECODE
			mCodeLimit[len - 1] = base - 1;
#endif
		}

		if (base)
			throw MyError("Decompression error: Invalid VLC table detected in Huffyuv format.");

		return src;
	}

//#define SHLD(dst, src, count) (uint32)(__ll_lshift(((uint64)(uint32)(dst) << 32) | (uint32)(src), (count) & 31) >> 32)
#define SHLD(dst, src, count) (uint32)(((((uint64)(uint32)(dst) << 32) | (uint32)(src)) << ((count) & 31)) >> 32)

#define CONSUME(bits) (pos += (bits))

#ifdef SEARCH_BASED_DECODE
	#define DECODE(result, table)	\
		{	\
			const uint32 posidx = (pos >> 5);	\
			const uint32 bitpos = pos & 31;	\
			uint32 code = (src32[posidx] << bitpos) + ((src32[posidx + 1] >> (31-bitpos)) >> 1); \
			for(int bitlen = table->mBaseLen; bitlen >= 1; --bitlen) {	\
				if (code <= table->mCodeLimit[bitlen - 1]) {	\
					result = table->mCodeBase[bitlen - 1][(code - table->mCodeStart[bitlen - 1]) >> (32 - bitlen)][0];	\
					CONSUME(bitlen);	\
					break;	\
				}	\
			}	\
		}
#else
#ifdef _M_IX86
	#define LSHIFT(x, y) __ll_lshift((x), (y))
#else
	#define LSHIFT(x, y) __ll_lshift((x), (y) & 31)
#endif

#define DECODE(result, table)	\
	{	\
		const uint32 posidx = (pos >> 5);	\
		union {	\
			struct {	\
				uint32 a, b;	\
			} d;	\
			uint64 q;	\
		} conv = { { src32[posidx+1], src32[posidx] } };	\
		conv.q = LSHIFT(conv.q, pos);	\
		uint32 code = conv.d.b | 1;	\
		uint32 idx = VDFindHighestSetBitFast(code);	\
		const uint8 *p = (const uint8 *)(table->mBsrLenTable[idx] + 2*(code >> table->mBsrShiftTable[idx]));	\
		result = p[0];	\
		CONSUME(p[1]);	\
	}
#endif

#define DECLARE_TABLES_Y()	\
		const HuffmanDecodeTable * VDRESTRICT tabY = &tables[0]

#define DECLARE_TABLES_YUV()	\
		const HuffmanDecodeTable * VDRESTRICT tabY = &tables[0];	\
		const HuffmanDecodeTable * VDRESTRICT tabU = &tables[1];	\
		const HuffmanDecodeTable * VDRESTRICT tabV = &tables[2]

#define DECLARE_PREDICTORS_Y()	\
		uint8 predY = predictors[0]

#define DECLARE_PREDICTORS_YUV()	\
		uint8 predY = predictors[0];	\
		uint8 predU = predictors[1];	\
		uint8 predV = predictors[2]

#define WRITE_PREDICTORS_Y()	\
		predictors[0] = predY

#define WRITE_PREDICTORS_YUV()	\
		predictors[0] = predY;	\
		predictors[1] = predU;	\
		predictors[2] = predV

	uint32 DecodeYUY2(uint8 *VDRESTRICT dst, const uint32 *VDRESTRICT src32, uint32 pos, uint32 count, const HuffmanDecodeTable *VDRESTRICT tables) {
		DECLARE_TABLES_YUV();
		uint8 y1;
		uint8 u;
		uint8 y2;
		uint8 v;

		do {
			DECODE(y1, tabY);
			DECODE(u, tabU);
			DECODE(y2, tabY);
			DECODE(v, tabV);

			*dst++ = y1;
			*dst++ = u;
			*dst++ = y2;
			*dst++ = v;
		} while(--count);

		return pos;
	}

	uint32 DecodeYUY2PredictLeft(uint8 *VDRESTRICT dst, const uint32 *VDRESTRICT src32, uint32 pos, uint32 count, const HuffmanDecodeTable *VDRESTRICT tables, uint8 *VDRESTRICT predictors) {
		DECLARE_TABLES_YUV();
		DECLARE_PREDICTORS_YUV();
		uint8 y1;
		uint8 u;
		uint8 y2;
		uint8 v;

		do {
			DECODE(y1, tabY);
			DECODE(u, tabU);
			DECODE(y2, tabY);
			DECODE(v, tabV);

			predY += y1;
			*dst++ = predY;

			predU += u;
			*dst++ = predU;

			predY += y2;
			*dst++ = predY;

			predV += v;
			*dst++ = predV;
		} while(--count);

		WRITE_PREDICTORS_YUV();

		return pos;
	}

	uint32 DecodeY8(uint8 *VDRESTRICT dst, const uint32 *VDRESTRICT src32, uint32 pos, uint32 count, const HuffmanDecodeTable *VDRESTRICT tables) {
		DECLARE_TABLES_Y();
		uint8 y1;
		uint8 y2;

		do {
			DECODE(y1, tabY);
			DECODE(y2, tabY);

			*dst++ = y1;
			*dst++ = y2;
		} while(--count);

		return pos;
	}

	uint32 DecodeY8PredictLeft(uint8 *VDRESTRICT dst, const uint32 *VDRESTRICT src32, uint32 pos, uint32 count, const HuffmanDecodeTable *VDRESTRICT tables, uint8 *VDRESTRICT predictors) {
		DECLARE_TABLES_Y();
		DECLARE_PREDICTORS_Y();
		uint8 y1;
		uint8 y2;

		do {
			DECODE(y1, tabY);
			DECODE(y2, tabY);

			predY += y1;
			*dst++ = predY;

			predY += y2;
			*dst++ = predY;
		} while(--count);

		WRITE_PREDICTORS_Y();

		return pos;
	}

	uint32 DecodeYV12(uint8 *VDRESTRICT dstY, uint8 *VDRESTRICT dstU, uint8 *VDRESTRICT dstV, const uint32 *VDRESTRICT src32, uint32 pos, uint32 count, const HuffmanDecodeTable *VDRESTRICT tables) {
		DECLARE_TABLES_YUV();
		uint8 y1;
		uint8 u;
		uint8 y2;
		uint8 v;

		do {
			DECODE(y1, tabY);
			DECODE(u, tabU);
			DECODE(y2, tabY);
			DECODE(v, tabV);

			*dstY++ = y1;
			*dstU++ = u;
			*dstY++ = y2;
			*dstV++ = v;
		} while(--count);

		return pos;
	}

	uint32 DecodeYV12PredictLeft(uint8 *VDRESTRICT dstY, uint8 *VDRESTRICT dstU, uint8 *VDRESTRICT dstV, const uint32 *VDRESTRICT src32, uint32 pos, uint32 count, const HuffmanDecodeTable *VDRESTRICT tables, uint8 *VDRESTRICT predictors) {
		DECLARE_TABLES_YUV();
		DECLARE_PREDICTORS_YUV();
		uint8 y1;
		uint8 u;
		uint8 y2;
		uint8 v;

		do {
			DECODE(y1, tabY);
			DECODE(u, tabU);
			DECODE(y2, tabY);
			DECODE(v, tabV);

			predY += y1;
			*dstY++ = predY;

			predU += u;
			*dstU++ = predU;

			predY += y2;
			*dstY++ = predY;

			predV += v;
			*dstV++ = predV;
		} while(--count);

		WRITE_PREDICTORS_YUV();

		return pos;
	}

#define DECLARE_TABLES_RGB()	\
		const HuffmanDecodeTable * VDRESTRICT tabB = &tables[0];	\
		const HuffmanDecodeTable * VDRESTRICT tabG = &tables[1];	\
		const HuffmanDecodeTable * VDRESTRICT tabR = &tables[2]

#define DECLARE_PREDICTORS_RGB()	\
		uint8 predB = predictors[0];	\
		uint8 predG = predictors[1];	\
		uint8 predR = predictors[2]

#define WRITE_PREDICTORS_RGB()	\
		predictors[0] = predB;	\
		predictors[1] = predG;	\
		predictors[2] = predR

	uint32 DecodeRGBPredictLeft(uint8 *VDRESTRICT dst, const uint32 *VDRESTRICT src32, uint32 pos, uint32 count, const HuffmanDecodeTable *VDRESTRICT tables, uint8 *VDRESTRICT predictors) {
		DECLARE_TABLES_RGB();
		DECLARE_PREDICTORS_RGB();
		uint8 r;
		uint8 g;
		uint8 b;

		do {
			DECODE(b, tabB);
			DECODE(g, tabG);
			DECODE(r, tabR);

			predB += b;
			*dst++ = predB;

			predG += g;
			*dst++ = predG;

			predR += r;
			*dst++ = predR;
		} while(--count);

		WRITE_PREDICTORS_RGB();

		return pos;
	}

	uint32 DecodeRGBPredictLeftDecorr(uint8 *VDRESTRICT dst, const uint32 *VDRESTRICT src32, uint32 pos, uint32 count, const HuffmanDecodeTable *VDRESTRICT tables, uint8 *VDRESTRICT predictors) {
		DECLARE_TABLES_RGB();
		DECLARE_PREDICTORS_RGB();
		uint8 r;
		uint8 g;
		uint8 b;

		do {
			DECODE(g, tabG);
			DECODE(b, tabB);
			DECODE(r, tabR);

			predB += b;
			predG += g;
			predR += r;

			*dst++ = predB + predG;
			*dst++ = predG;
			*dst++ = predR + predG;

		} while(--count);

		WRITE_PREDICTORS_RGB();

		return pos;
	}

#define DECLARE_PREDICTORS_RGBA()	\
		uint8 predB = predictors[0];	\
		uint8 predG = predictors[1];	\
		uint8 predR = predictors[2];	\
		uint8 predA = predictors[3]

#define WRITE_PREDICTORS_RGBA()	\
		predictors[0] = predB;	\
		predictors[1] = predG;	\
		predictors[2] = predR;	\
		predictors[3] = predA

	uint32 DecodeRGBAPredictLeft(uint8 *VDRESTRICT dst, const uint32 *VDRESTRICT src32, uint32 pos, uint32 count, const HuffmanDecodeTable *VDRESTRICT tables, uint8 *VDRESTRICT predictors) {
		DECLARE_TABLES_RGB();
		DECLARE_PREDICTORS_RGBA();
		uint8 r;
		uint8 g;
		uint8 b;
		uint8 a;

		do {
			DECODE(b, tabB);
			DECODE(g, tabG);
			DECODE(r, tabR);
			DECODE(a, tabR);

			predB += b;
			*dst++ = predB;

			predG += g;
			*dst++ = predG;

			predR += r;
			*dst++ = predR;

			predA += a;
			*dst++ = predA;
		} while(--count);

		WRITE_PREDICTORS_RGBA();

		return pos;
	}

	uint32 DecodeRGBAPredictLeftDecorr(uint8 *VDRESTRICT dst, const uint32 *VDRESTRICT src32, uint32 pos, uint32 count, const HuffmanDecodeTable *VDRESTRICT tables, uint8 *VDRESTRICT predictors) {
		DECLARE_TABLES_RGB();
		DECLARE_PREDICTORS_RGBA();
		uint8 r;
		uint8 g;
		uint8 b;
		uint8 a;

		do {
			DECODE(g, tabG);
			DECODE(b, tabB);
			DECODE(r, tabR);
			DECODE(a, tabR);

			predB += b;
			predG += g;
			predR += r;
			predA += a;

			*dst++ = predB + predG;
			*dst++ = predG;
			*dst++ = predR + predG;
			*dst++ = predA + predG;

		} while(--count);

		WRITE_PREDICTORS_RGBA();

		return pos;
	}

#undef DECODE
#undef CONSUME
#undef PEEK
#undef SHLD

	void DecodeVerticalPrediction(uint8 *dst, const uint8 *src, uint32 count) {
		do {
			*dst++ += *src++;
		} while(--count);
	}

	void DecodeMedianPredictionYUY2(
			uint8 * VDRESTRICT dst,
			const uint8 * VDRESTRICT srcL,
			const uint8 * VDRESTRICT srcTC,
			const uint8 * VDRESTRICT srcTL,
			uint32 count) {
#define MEDIAN(r, a, b, c)	\
				if (a > b) { t=a; a=b; b=t; }	\
				if (b > c) { t=b; b=c; c=t; }	\
				if (a > b) { t=a; a=b; b=t; }	\
				r = b
		uint8 t;
		uint8 a;
		uint8 b;
		uint8 c;
		uint8 y1;
		uint8 u;
		uint8 y2;
		uint8 v;

		do {
			a = srcTC[0];
			b = srcL[2];
			c = a + b - srcTL[2];

			MEDIAN(y1, a, b, c);

			a = srcTC[1];
			b = srcL[1];
			c = a + b - srcTL[1];

			MEDIAN(u, a, b, c);

			dst[0] += y1;
			dst[1] += u;

			a = srcTC[2];
			b = dst[0];
			c = a + b - srcTC[0];

			MEDIAN(y2, a, b, c);

			a = srcTC[3];
			b = srcL[3];
			c = a + b - srcTL[3];

			MEDIAN(v, a, b, c);

			dst[2] += y2;
			dst[3] += v;
			dst += 4;
			srcTC += 4;
			srcTL += 4;
			srcL += 4;
		} while(--count);
	}

	void DecodeMedianPredictionY8(
			uint8 * VDRESTRICT dst,
			const uint8 * VDRESTRICT srcL,
			const uint8 * VDRESTRICT srcTC,
			const uint8 * VDRESTRICT srcTL,
			uint32 count) {
#define MEDIAN(r, a, b, c)	\
				if (a > b) { t=a; a=b; b=t; }	\
				if (b > c) { t=b; b=c; c=t; }	\
				if (a > b) { t=a; a=b; b=t; }	\
				r = b
		uint8 t;
		uint8 a;
		uint8 b;
		uint8 c;
		uint8 y;

		do {
			a = srcTC[0];
			b = srcL[0];
			c = a + b - srcTL[0];

			MEDIAN(y, a, b, c);

			*dst++ += y;

			++srcTC;
			++srcTL;
			++srcL;
		} while(--count);
	}
}

class VDVideoDecoderHuffyuv : public IVDVideoDecoderHuffyuv {
public:
	void		Init(uint32 w, uint32 h, uint32 depth, const uint8 *extradata, uint32 extralen);
	void		DecompressFrame(const void *src, uint32 len);
	VDPixmap	GetFrameBuffer();

protected:
	uint32		LoadAdaptiveTables(const void *src, uint32 len);

	enum FormatMode {
		kFormatMode_YUY2,
		kFormatMode_YV12,
		kFormatMode_RGB,
		kFormatMode_RGBA
	};

	enum PredictMode {
		kPredictMode_Default,
		kPredictMode_Left,
		kPredictMode_LeftDecorrelate,
		kPredictMode_Gradient,
		kPredictMode_GradientDecorrelate,
		kPredictMode_Median
	};

	PredictMode		mPredictMode;
	FormatMode		mFormatMode;
	bool			mbInterlaced;
	bool			mbAdaptiveHuffman;

	VDPixmapBuffer	mFrameBuffer;

	HuffmanDecodeTable	mTables[3];

	vdfastvector<uint32>	mSafeDecodeArea;
};

IVDVideoDecoderHuffyuv *VDCreateVideoDecoderHuffyuv() {
	return new VDVideoDecoderHuffyuv;
}

void VDVideoDecoderHuffyuv::Init(uint32 w, uint32 h, uint32 depth, const uint8 *extradata, uint32 extralen) {
	switch(depth & 7) {
		case 0:
		default:
			mPredictMode = kPredictMode_Default;
			break;

		case 1:
			mPredictMode = kPredictMode_Left;
			break;

		case 2:
			mPredictMode = kPredictMode_LeftDecorrelate;
			break;

		case 3:
			mPredictMode = kPredictMode_Gradient;
			break;

		case 4:
			mPredictMode = kPredictMode_GradientDecorrelate;
			break;

		case 5:
			mPredictMode = kPredictMode_Median;
			break;
	}

	mbInterlaced = (h > 288);
	mbAdaptiveHuffman = false;

	if (extralen >= 4) {
		uint8 method2 = extradata[0];
		uint8 bpp_override = extradata[1];

		// extension: interlace flag
		switch(extradata[2] & 0x30) {
		case 0x10:
			mbInterlaced = true;
			break;
		case 0x20:
			mbInterlaced = false;
			break;
		}

		// extension: adaptive Huffman
		mbAdaptiveHuffman = (extradata[2] & 0x40) != 0;

		depth = bpp_override;

		if (!mPredictMode) {
			switch(method2) {
				case 0xFE:
					mPredictMode = kPredictMode_Default;
					break;
				case 0x00:
					mPredictMode = kPredictMode_Left;
					break;
				case 0x01:
					mPredictMode = kPredictMode_Gradient;
					break;
				case 0x02:
					mPredictMode = kPredictMode_Median;
					break;
				case 0x40:
					mPredictMode = kPredictMode_LeftDecorrelate;
					break;
				case 0x41:
					mPredictMode = kPredictMode_GradientDecorrelate;
					break;
			}
		}

		if (extralen >= 4 && !mbAdaptiveHuffman) {
			extradata += 4;

			const uint8 *limit = extradata + extralen - 4;
			extradata = mTables[0].Init(extradata, extralen);
			extradata = mTables[1].Init(extradata, limit - extradata);
			extradata = mTables[2].Init(extradata, limit - extradata);
		}
	} else {
		throw MyError("The Huffyuv video stream uses an unsupported old format that lacks embedded encoding tables.");
	}

	if (!mPredictMode)
		mPredictMode = kPredictMode_Left;

	switch(depth & ~7) {
		case 16:
			mFormatMode = kFormatMode_YUY2;
			mFrameBuffer.init(w, h, nsVDPixmap::kPixFormat_YUV422_YUYV);
			break;
		case 24:
			mFormatMode = kFormatMode_RGB;
			mFrameBuffer.init(w, h, nsVDPixmap::kPixFormat_RGB888);
			break;
		case 32:
			mFormatMode = kFormatMode_RGBA;
			mFrameBuffer.init(w, h, nsVDPixmap::kPixFormat_XRGB8888);
			break;
		case 8:
			if (depth == 12) {
				mFormatMode = kFormatMode_YV12;
				mFrameBuffer.init(w, h, nsVDPixmap::kPixFormat_YUV420_Planar);
				break;
			}
			// fall through

		default:
			throw MyError("The Huffyuv video stream uses an unsupported bit depth (%d).", depth);
	}
}

void VDVideoDecoderHuffyuv::DecompressFrame(const void *src, uint32 len) {
	uint32 pos = 0;
	if (mbAdaptiveHuffman) {
		uint32 offset = LoadAdaptiveTables(src, len);

		pos = offset << 3;
	}

	uint32 w = mFrameBuffer.w;
	uint32 h = mFrameBuffer.h;

	const uint32 *src32 = (const uint32 *)src;
	uint8 *dstrow = (uint8 *)mFrameBuffer.data;
	ptrdiff_t dstpitch = mFrameBuffer.pitch;
	uint8 *dstrowU = (uint8 *)mFrameBuffer.data2;
	ptrdiff_t dstpitchU = mFrameBuffer.pitch2;
	uint8 *dstrowV = (uint8 *)mFrameBuffer.data3;
	ptrdiff_t dstpitchV = mFrameBuffer.pitch3;

	// flip RGB formats
	if (mFormatMode == kFormatMode_RGB || mFormatMode == kFormatMode_RGBA) {
		dstrow += dstpitch * (h - 1);
		dstpitch = -dstpitch;
	} else {
		w >>= 1;
	}

	uint8 predictors[4];
	uint32 bpp = 4;
	
	uint32 verticalPredictionStart = mbInterlaced ? 2 : 1;
	ptrdiff_t verticalPredDelta = mbInterlaced ? -2*dstpitch : -dstpitch;
	ptrdiff_t verticalPredDeltaU = mbInterlaced ? -2*dstpitchU : -dstpitchU;
	ptrdiff_t verticalPredDeltaV = mbInterlaced ? -2*dstpitchV : -dstpitchV;
	uint32 *safeDecodeArea = mSafeDecodeArea.data();
	uint32 safeDecodeAreaSizeInDwords = mSafeDecodeArea.size();
	uint32 safeDecodeAreaSizeInBits = safeDecodeAreaSizeInDwords << 5;
	uint32 maxSafePosition = 0;
	uint32 bufferSizeInBits = len << 3;
	uint32 validPosLimit = 0xFFFFFFFFU;

	if (bufferSizeInBits > safeDecodeAreaSizeInBits)
		maxSafePosition = bufferSizeInBits - safeDecodeAreaSizeInBits;

	for(uint32 y=0; y<h; ++y) {
		if (pos >= maxSafePosition) {
			maxSafePosition = 0xFFFFFFFFU;

			uint32 posDwordIdx = pos >> 5;
			uint32 bytesLeft = len - (posDwordIdx << 2);

			VDASSERT(bytesLeft <= safeDecodeAreaSizeInDwords * sizeof(uint32));

			memcpy(safeDecodeArea, &src32[posDwordIdx], bytesLeft);
			src32 = safeDecodeArea;
			pos &= 31;
			validPosLimit = bytesLeft << 3;
		}

		uint8 *dst = dstrow;
		uint8 *dstU = dstrowU;
		uint8 *dstV = dstrowV;

		if (!y) {
			uint32 posidx = pos >> 5;
			uint32 bitpos = pos & 31;
			uint32 v = (src32[posidx] << bitpos) + ((src32[posidx + 1] >> (31-bitpos)) >> 1);

			if (mFormatMode == kFormatMode_RGB) {
				uint8 b = (uint8)(v >>  8);
				uint8 g = (uint8)(v >> 16);
				uint8 r = (uint8)(v >> 24);

				dstrow[0] = b;
				dstrow[1] = g;
				dstrow[2] = r;

				switch (mPredictMode) {
					case kPredictMode_LeftDecorrelate:
					case kPredictMode_GradientDecorrelate:
						r -= g;
						b -= g;
						break;
				}

				predictors[0] = b;
				predictors[1] = g;
				predictors[2] = r;
				bpp = 3;
				dst += 3;
			} else if (mFormatMode == kFormatMode_RGBA) {
				uint8 b = (uint8)(v >>  0);
				uint8 g = (uint8)(v >>  8);
				uint8 r = (uint8)(v >> 16);
				uint8 a = (uint8)(v >> 24);

				dstrow[0] = b;
				dstrow[1] = g;
				dstrow[2] = r;
				dstrow[3] = a;

				switch (mPredictMode) {
					case kPredictMode_LeftDecorrelate:
					case kPredictMode_GradientDecorrelate:
						r -= g;
						b -= g;
						a -= g;
						break;
				}

				predictors[0] = b;
				predictors[1] = g;
				predictors[2] = r;
				predictors[3] = a;
				bpp = 4;
				dst += 4;
			} else if (mFormatMode == kFormatMode_YV12) {
				dstrow[0] = (uint8)(v >>  0);
				dstrowU[0] = (uint8)(v >>  8);
				dstrow[1] = (uint8)(v >> 16);
				dstrowV[0] = (uint8)(v >> 24);

				predictors[0] = dstrow[1];
				predictors[1] = dstrowU[0];
				predictors[2] = dstrowV[0];
				dst += 2;
				++dstU;
				++dstV;
			} else {
				dstrow[0] = (uint8)(v >>  0);
				dstrow[1] = (uint8)(v >>  8);
				dstrow[2] = (uint8)(v >> 16);
				dstrow[3] = (uint8)(v >> 24);

				predictors[0] = dstrow[2];
				predictors[1] = dstrow[1];
				predictors[2] = dstrow[3];
				dst += 4;
			}

			pos += 32;
		}

		if (mFormatMode == kFormatMode_RGB) {
			switch(mPredictMode) {
				case kPredictMode_Left:
					pos = DecodeRGBPredictLeft(dst, src32, pos, y ? w : w - 1, mTables, predictors);
					break;

				case kPredictMode_Gradient:
					pos = DecodeRGBPredictLeft(dst, src32, pos, y ? w : w - 1, mTables, predictors);

					if (y >= verticalPredictionStart)
						DecodeVerticalPrediction(dstrow, dstrow + verticalPredDelta, bpp*w);
					break;

				case kPredictMode_LeftDecorrelate:
					pos = DecodeRGBPredictLeftDecorr(dst, src32, pos, y ? w : w - 1, mTables, predictors);
					break;

				case kPredictMode_GradientDecorrelate:
					pos = DecodeRGBPredictLeftDecorr(dst, src32, pos, y ? w : w - 1, mTables, predictors);

					if (y >= verticalPredictionStart)
						DecodeVerticalPrediction(dstrow, dstrow + verticalPredDelta, bpp*w);
					break;
			}
		} else if (mFormatMode == kFormatMode_RGBA) {
			switch(mPredictMode) {
				case kPredictMode_Left:
					pos = DecodeRGBAPredictLeft(dst, src32, pos, y ? w : w - 1, mTables, predictors);
					break;

				case kPredictMode_Gradient:
					pos = DecodeRGBAPredictLeft(dst, src32, pos, y ? w : w - 1, mTables, predictors);

					if (y >= verticalPredictionStart)
						DecodeVerticalPrediction(dstrow, dstrow + verticalPredDelta, bpp*w);
					break;

				case kPredictMode_LeftDecorrelate:
					pos = DecodeRGBAPredictLeftDecorr(dst, src32, pos, y ? w : w - 1, mTables, predictors);
					break;

				case kPredictMode_GradientDecorrelate:
					pos = DecodeRGBAPredictLeftDecorr(dst, src32, pos, y ? w : w - 1, mTables, predictors);

					if (y >= verticalPredictionStart)
						DecodeVerticalPrediction(dstrow, dstrow + verticalPredDelta, bpp*w);
					break;
			}
		} else if (mFormatMode == kFormatMode_YV12) {
			switch(mPredictMode) {
				case kPredictMode_Left:
					if (y & 1)
						pos = DecodeY8PredictLeft(dst, src32, pos, w, mTables, predictors);
					else {
						pos = DecodeYV12PredictLeft(dst, dstU, dstV, src32, pos, y ? w : w - 1, mTables, predictors);

						dstrowU += dstpitchU;
						dstrowV += dstpitchV;
					}
					break;

				case kPredictMode_Gradient:
					if (y & 1) {
						pos = DecodeY8PredictLeft(dst, src32, pos, w, mTables, predictors);

						if (y >= verticalPredictionStart)
							DecodeVerticalPrediction(dstrow, dstrow + verticalPredDelta, w*2);
					} else {
						pos = DecodeYV12PredictLeft(dst, dstU, dstV, src32, pos, y ? w : w - 1, mTables, predictors);

						if (y >= verticalPredictionStart) {
							DecodeVerticalPrediction(dstrow, dstrow + verticalPredDelta, w*2);
							DecodeVerticalPrediction(dstrowU, dstrowU + verticalPredDeltaU, w);
							DecodeVerticalPrediction(dstrowV, dstrowV + verticalPredDeltaV, w);
						}

						dstrowU += dstpitchU;
						dstrowV += dstpitchV;
					}
					break;

				case kPredictMode_Median:
					// Whoever created the median mode for the YV12 variant was clearly smoking crack, because
					// chroma is interleaved with luma at approximately every other line, but there is an anomaly
					// at the beginning of the file where either the first two or four lines of the stream are
					// all YCbCr lines, followed by two or four lines of just Y.

					if (y < verticalPredictionStart) {
						// The first line (non-interlaced) or two lines (interlaced) are just left predicted but
						// fully YCbCr encoded.
						pos = DecodeYV12PredictLeft(dst, dstU, dstV, src32, pos, y ? w : w - 1, mTables, predictors);
						dstrowU += dstpitchU;
						dstrowV += dstpitchV;
					} else if (y == verticalPredictionStart) {
						// Line 1 (ni) or 2 (i) is left predicted for the first four pixels, then median encoded.
						// Full YCbCr encoding.
						pos = DecodeYV12(dst, dstU, dstV, src32, pos, w, mTables);

						dstrow[0] += predictors[0];
						dstrow[1] += dstrow[0];
						dstrow[2] += dstrow[1];
						dstrow[3] += dstrow[2];
						
						DecodeMedianPredictionY8(dstrow + 4, dstrow + 3, dstrow + verticalPredDelta + 4, dstrow + verticalPredDelta + 3, w*2 - 4);

						dstrowU[0] += predictors[1];
						dstrowU[1] += dstrowU[0];
						
						DecodeMedianPredictionY8(dstrowU + 2, dstrowU + 1, dstrowU + verticalPredDeltaU + 2, dstrowU + verticalPredDeltaU + 1, w - 2);

						dstrowV[0] += predictors[2];
						dstrowV[1] += dstrowV[0];
						
						DecodeMedianPredictionY8(dstrowV + 2, dstrowV + 1, dstrowV + verticalPredDeltaV + 2, dstrowV + verticalPredDeltaV + 1, w - 2);

						dstrowU += dstpitchU;
						dstrowV += dstpitchV;
					} else if ((y & 1) || y < verticalPredictionStart * 2 + 2) {
						pos = DecodeY8(dst, src32, pos, w, mTables);

						DecodeMedianPredictionY8(dstrow, dstrow - dstpitch + w*2 - 1, dstrow + verticalPredDelta, dstrow + verticalPredDelta - dstpitch + w*2 - 1, 1);
						DecodeMedianPredictionY8(dstrow + 1, dstrow, dstrow + verticalPredDelta + 1, dstrow + verticalPredDelta, w*2 - 1);
					} else {
						pos = DecodeYV12(dst, dstU, dstV, src32, pos, w, mTables);

						DecodeMedianPredictionY8(dstrow, dstrow - dstpitch + w*2 - 1, dstrow + verticalPredDelta, dstrow + verticalPredDelta - dstpitch + w*2 - 1, 1);
						DecodeMedianPredictionY8(dstrow + 1, dstrow, dstrow + verticalPredDelta + 1, dstrow + verticalPredDelta, 2*w - 1);

						DecodeMedianPredictionY8(dstrowU, dstrowU - dstpitchU + w - 1, dstrowU + verticalPredDeltaU, dstrowU + verticalPredDeltaU - dstpitchU + w - 1, 1);
						DecodeMedianPredictionY8(dstrowU + 1, dstrowU, dstrowU + verticalPredDeltaU + 1, dstrowU + verticalPredDeltaU, w - 1);

						DecodeMedianPredictionY8(dstrowV, dstrowV - dstpitchV + w - 1, dstrowV + verticalPredDeltaV, dstrowV + verticalPredDeltaV - dstpitchV + w - 1, 1);
						DecodeMedianPredictionY8(dstrowV + 1, dstrowV, dstrowV + verticalPredDeltaV + 1, dstrowV + verticalPredDeltaV, w - 1);

						dstrowU += dstpitchU;
						dstrowV += dstpitchV;
					}
					break;
			}
		} else {
			switch(mPredictMode) {
				case kPredictMode_Left:
					pos = DecodeYUY2PredictLeft(dst, src32, pos, y ? w : w - 1, mTables, predictors);
					break;

				case kPredictMode_Gradient:
					pos = DecodeYUY2PredictLeft(dst, src32, pos, y ? w : w - 1, mTables, predictors);

					if (y >= verticalPredictionStart)
						DecodeVerticalPrediction(dstrow, dstrow + verticalPredDelta, bpp*w);
					break;

				case kPredictMode_Median:
					if (y < verticalPredictionStart) {
						pos = DecodeYUY2PredictLeft(dst, src32, pos, y ? w : w - 1, mTables, predictors);
					} else {
						pos = DecodeYUY2(dst, src32, pos, w, mTables);

						if (y > verticalPredictionStart) {
							DecodeMedianPredictionYUY2(dstrow, dstrow - dstpitch + w*4 - 4, dstrow + verticalPredDelta, dstrow + verticalPredDelta - dstpitch + w*4 - 4, 1);
							DecodeMedianPredictionYUY2(dstrow + 4, dstrow, dstrow + verticalPredDelta + 4, dstrow + verticalPredDelta, w - 1);
						} else {
							dstrow[0] += predictors[0];
							dstrow[1] += predictors[1];
							dstrow[2] += dstrow[0];
							dstrow[3] += predictors[2];
							dstrow[4] += dstrow[2];
							dstrow[5] += dstrow[1];
							dstrow[6] += dstrow[4];
							dstrow[7] += dstrow[3];
							
							DecodeMedianPredictionYUY2(dstrow + 8, dstrow + 4, dstrow + verticalPredDelta + 8, dstrow + verticalPredDelta + 4, w - 2);
						}
					}
					break;
			}
		}

		dstrow += dstpitch;

		if (pos > validPosLimit)
			throw MyError("A decompression error occurred while decoding Huffyuv data.");
	}
}

VDPixmap VDVideoDecoderHuffyuv::GetFrameBuffer() {
	return mFrameBuffer;
}

uint32 VDVideoDecoderHuffyuv::LoadAdaptiveTables(const void *src, uint32 len) {
	uint32 tmpArea[192];
	uint32 dwords = len >> 2;

	if (dwords > 192)
		dwords = 192;

	const uint32 *src32 = (uint32 *)src;
	for(uint32 i=0; i<dwords; ++i)
		tmpArea[i] = VDSwizzleU32(src32[i]);

	const uint8 *extradata = (const uint8 *)tmpArea;
	const uint8 *limit = extradata + 4*dwords;

	extradata = mTables[0].Init(extradata, limit - extradata);
	extradata = mTables[1].Init(extradata, limit - extradata);
	extradata = mTables[2].Init(extradata, limit - extradata);

	// Compute size of safe decode area.
	//
	// We need up to:
	//	4 bytes for the first pixel
	//	N bits for VLC coding for each pixel in the row
	//	31 bits of VLC padding
	//	32 bits for readover
	uint32 safeDecAreaSize = sizeof(uint32);
	uint32 maxBitsChannel0 = mTables[0].mBaseLen;
	uint32 maxBitsChannel1 = mTables[1].mBaseLen;
	uint32 maxBitsChannel2 = mTables[2].mBaseLen;
	uint32 blocks = mFrameBuffer.w;

	if (mFormatMode == kFormatMode_YUY2 || mFormatMode == kFormatMode_YV12) {
		maxBitsChannel0 += maxBitsChannel0;
		blocks >>= 1;
	}

	uint32 maxBitsInRow = (maxBitsChannel0 + maxBitsChannel1 + maxBitsChannel2) * blocks;
	uint32 maxDwordsInRow = ((maxBitsInRow + 31) >> 5) + 2;

	mSafeDecodeArea.resize(maxDwordsInRow);

	return extradata - (const uint8 *)tmpArea;
}
