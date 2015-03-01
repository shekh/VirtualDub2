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

#include <stdafx.h>
#include <windows.h>
#include <stdio.h>
#include <stddef.h>
#include <math.h>
#include <algorithm>
#include <vector>
#include <vd2/system/vdstl.h>
#include "imagejpeg.h"

///////////////////////////////////////////////////////////////////////////

namespace {
	const int zigzag[64] = {		// zigzag scan order
		 0,  1,  8, 16,  9,  2,  3, 10,
		17, 24, 32, 25, 18, 11,  4,  5,
		12, 19, 26, 33, 40, 48, 41, 34,
		27, 20, 13,  6,  7, 14, 21, 28,
		35, 42, 49, 56, 57, 50, 43, 36,
		29, 22, 15, 23, 30, 37, 44, 51,
		58, 59, 52, 45, 38, 31, 39, 46,
		53, 60, 61, 54, 47, 55, 62, 63,
	};

	struct JPEGHuffmanHistoSorter {
		JPEGHuffmanHistoSorter(const int *pHisto) : mHisto(pHisto) {}

		bool operator()(int f1, int f2) const {
			return mHisto[f1] > mHisto[f2];
		}

		const int *const mHisto;
	};
};

class VDJPEGHuffmanTable {
public:
	VDJPEGHuffmanTable();

	void Init();

	inline void Tally(unsigned char c) {
		++mHistogram[c];
	}

	void BuildCode();

	const unsigned char *GetDHTSegment() { return mDHT; }
	int GetDHTSegmentLen() const { return mDHTLength; }

private:
	int mHistogram[257];			// one extra code point required to avoid FFFF
	unsigned char mDHT[256 + 16];
	int mDHTLength;
};

VDJPEGHuffmanTable::VDJPEGHuffmanTable() {
}

void VDJPEGHuffmanTable::Init() {
	std::fill(mHistogram, mHistogram+256, 0);
}

void VDJPEGHuffmanTable::BuildCode() {
	int i;
	int nonzero_codes = 0;

	for(i=0; i<256; ++i) {
		mDHT[i+16] = (uint8)i;
		if (mHistogram[i])
			++nonzero_codes;
	}

	// Codes are stored in the second half of the DHT segment in decreasing
	// order of frequency.

	std::sort(&mDHT[16], &mDHT[16+256], JPEGHuffmanHistoSorter(mHistogram));
	mDHTLength = 16 + nonzero_codes;

	// Sort histogram in increasing order.

	mHistogram[256] = 1;					// extra code point to prevent FFFF from being used
	std::sort(mHistogram, mHistogram+257);

	++nonzero_codes;

	int *A = mHistogram+257 - nonzero_codes;

	// Begin merging process (from "In-place calculation of minimum redundancy codes" by A. Moffat and J. Katajainen)
	//
	// There are three merging possibilities:
	//
	// 1) Leaf node with leaf node.
	// 2) Leaf node with internal node.
	// 3) Internal node with internal node.

	int leaf = 2;					// Next, smallest unattached leaf node.
	int internal = 0;				// Next, smallest unattached internal node.

	// Merging always creates one internal node and eliminates one node from
	// the total, so we will always be doing N-1 merges.

	A[0] += A[1];		// First merge is always two leaf nodes.
	for(int next=1; next<nonzero_codes-1; ++next) {		// 'next' is the value that receives the next unattached internal node.
		int a, b;

		// Pick first node.
		if (leaf < nonzero_codes && A[leaf] <= A[internal]) {
			A[next] = a=A[leaf++];			// begin new internal node with P of smallest leaf node
		} else {
			A[next] = a=A[internal];		// begin new internal node with P of smallest internal node
			A[internal++] = next;					// hook smallest internal node as child of new node
		}

		// Pick second node.
		if (internal >= next || (leaf < nonzero_codes && A[leaf] <= A[internal])) {
			A[next] += b=A[leaf++];			// complete new internal node with P of smallest leaf node
		} else {
			A[next] += b=A[internal];		// complete new internal node with P of smallest internal node
			A[internal++] = next;					// hook smallest internal node as child of new node
		}
	}

	// At this point, we have a binary tree composed entirely of pointers to
	// parents, partially sorted such that children are always before their
	// parents in the array.  Traverse the array backwards, replacing each
	// node with its depth in the tree.

	A[nonzero_codes-2] = 0;		// root has height 0 (0 bits)
	for(i = nonzero_codes-3; i>=0; --i)
		A[i] = A[A[i]]+1;		// child height is 1+height(parent).

	// Compute canonical tree bit depths for first part of DHT segment.
	// For each internal node at depth N, add two counts at depth N+1
	// and subtract one count at depth N.  Essentially, we are splitting
	// as we go.  We traverse backwards to ensure that no counts will drop
	// below zero at any time.

	std::fill(mDHT, mDHT+16, 0);

	int overallocation = 0;

	mDHT[0] = 2;		// 2 codes at depth 1 (1 bit)
	for(i = nonzero_codes-3; i>=0; --i) {
		int depth = A[i];

		// The optimal Huffman tree for N nodes can have a depth of N-1,
		// but we have to constrain ourselves at depth 16.  We simply
		// pile up counts at depth 16.  This causes us to overallocate the
		// codespace, but we will compensate for that later.

		if (depth >= 16) {
			++mDHT[15];
		} else {
			--mDHT[depth-1];
			++mDHT[depth];
			++mDHT[depth];
		}
	}

	// Remove the extra code point.
	bool bExtraCodePointFound = false;

	for(i=15; i>=0; --i) {
		if (mDHT[i]) {
			if (!bExtraCodePointFound) {
				bExtraCodePointFound = true;
				--mDHT[i];
			}

			overallocation += mDHT[i] * (0x8000 >> i);
		}
	}
	overallocation -= 0xFFFF;			// we can't allocate FFFF, so 64K-1 codes

	// We may have overallocated the codespace if we were forced to shorten
	// some codewords.

	if (overallocation > 0) {
		// Codespace is overallocated.  Begin lengthening codes from bit depth
		// 15 down until we are under the limit.

		i = 14;
		while(overallocation > 0) {
			if (mDHT[i]) {
				--mDHT[i];
				++mDHT[i+1];
				overallocation -= 0x4000 >> i;
				if (i < 14)
					++i;
			} else
				--i;
		}

		// We may be undercommitted at this point.  Raise codes from bit depth
		// 1 up until we are at the desired limit.

		int underallocation = -overallocation;

		i = 1;
		while(underallocation > 0) {
			if (mDHT[i] && (0x8000>>i) <= underallocation) {
				underallocation -= (0x8000>>i);
				--mDHT[i];
				--i;
				++mDHT[i];
			} else {
				++i;
			}
		}
	}
};

///////////////////////////////////////////////////////////////////////////

class VDJPEGEncoder : public IVDJPEGEncoder {
public:
	// Quality ranges 0-100, higher is better image.
	void Init(int quality = 50, bool bOptimizeHuffmanTables = false, eChromaMode cmode = kYCC420);
	void Encode(vdfastvector<char>& dst, const char *src, ptrdiff_t srcpitch, int format, int w, int h);

protected:
	enum {
		SOF0	= 0xC0,		// start of frame (baseline JPEG)
		DHT		= 0xC4,		// define huffman tables
		SOI		= 0xD8,		// start of image
		EOI		= 0xD9,		// end of image
		SOS		= 0xDA,		// start of scan
		DQT		= 0xDB,		// define quantization tables
		APP0	= 0xE0
	};

	struct ComponentInfo {
		int				dc;				// Current DC value
		const int		*pCoeff;		// DCT coefficient matrix
		const int		*pQuant;		// quantization matrix
		const uint32	*pInvQuant;		// inverse quantization matrix
		const unsigned (*pDCEncode)[2];	// DC delta encoding table
		const unsigned (*pACEncode)[2];	// AC coefficient encoding table
		VDJPEGHuffmanTable	*pDCTable;
		VDJPEGHuffmanTable	*pACTable;
	};

	// critical variables -- keep this close to top for short [ecx] offsets
	unsigned long	mBitHeap;		// holds bits in flight
	int				mBitCount;		// number of bits in flight

	// non-critical variables
	int	mQuant[2][64];				// Quantization matrices
	uint32	mInvQuant[2][64];			// Inverse quantization matrices
	unsigned mDCEncode[2][16][2];	// DC encoding tables - (data,bits) pairs
	unsigned mACEncode[2][256][2];	// AC encoding tables - (data,bits) pairs

	VDJPEGHuffmanTable	mACHuffman[2];
	VDJPEGHuffmanTable	mDCHuffman[2];

	eChromaMode	mChromaMode;

	bool				mbOptimizeHuffmanTables;

	uint32		mColorTables[5][256];

	std::vector<uint8>	mStripBuffer;
	int			mStripWidth;
	int			mStripHeight;
	uint8		*mpYBuffer;
	uint8		*mpCbBuffer;
	uint8		*mpCrBuffer;

	vdfastvector<uint32>	mCoefficientHeap;
	vdfastvector<char> *mpDst;

	void WriteChar(unsigned char c);
	void WriteBlock(const void *block, int block_len);
	void WriteHeader(int w, int h, bool bWriteCustomDHT);

	void EncodeMacroblock(ComponentInfo *pComponent, int *dct_coeff);
	void TallyMacroblock(unsigned *&pHeap, ComponentInfo *pComponent, int *const pCoeffs);
	void EncodeMacroblock(unsigned *&pHeap, ComponentInfo *pComponent);

	void encode(uint32 encoding, int encoding_bits);
	int size_value(int coeff);
	void encode_value(int run, int coeff, const unsigned (*enctab)[2]);
	unsigned tally_value(int run, int coeff, VDJPEGHuffmanTable& hufftab);
	void dht_to_encoding_table(unsigned (*enctab)[2], const unsigned char *dht_seg);

protected:
	void Transform_RGB15(uint8 *dstY, uint8 *dstCb, uint8 *dstCr, ptrdiff_t yccpitch, const uint8 *src, ptrdiff_t srcpitch, int w, int h);
	void Transform_RGB24(uint8 *dstY, uint8 *dstCb, uint8 *dstCr, ptrdiff_t yccpitch, const uint8 *src, ptrdiff_t srcpitch, int w, int h);
	void Transform_RGB32(uint8 *dstY, uint8 *dstCb, uint8 *dstCr, ptrdiff_t yccpitch, const uint8 *src, ptrdiff_t srcpitch, int w, int h);
	static void Downsample_422(uint8 *p, ptrdiff_t pitch, int qw, int qh);
	static void Downsample_420(uint8 *p, ptrdiff_t pitch, int qw, int qh);

	static void fdct(int dct_coeff[64], const unsigned char *src, ptrdiff_t srcpitch);

	static const unsigned char stock_Huffman_DC0[];
	static const unsigned char stock_Huffman_DC1[];
	static const unsigned char stock_Huffman_AC0[];
	static const unsigned char stock_Huffman_AC1[];
};

const unsigned char VDJPEGEncoder::stock_Huffman_DC0[]={
	0x00,0x01,0x05,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B
};

const unsigned char VDJPEGEncoder::stock_Huffman_DC1[]={
	0x00,0x03,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B
};

const unsigned char VDJPEGEncoder::stock_Huffman_AC0[]={
	0x00,0x02,0x01,0x03,0x03,0x02,0x04,0x03,0x05,0x05,0x04,0x04,0x00,0x00,0x01,0x7D,
	0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,
	0x07,0x22,0x71,0x14,0x32,0x81,0x91,0xA1,0x08,0x23,0x42,0xB1,0xC1,0x15,0x52,0xD1,0xF0,0x24,
	0x33,0x62,0x72,0x82,0x09,0x0A,0x16,0x17,0x18,0x19,0x1A,0x25,0x26,0x27,0x28,0x29,0x2A,0x34,
	0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x53,0x54,0x55,0x56,
	0x57,0x58,0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,
	0x79,0x7A,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,
	0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,
	0xBA,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,
	0xDA,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,
	0xF8,0xF9,0xFA
};

const unsigned char VDJPEGEncoder::stock_Huffman_AC1[]={
	0x00,0x02,0x01,0x02,0x04,0x04,0x03,0x04,0x07,0x05,0x04,0x04,0x00,0x01,0x02,0x77,
	0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
	0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xA1,0xB1,0xC1,0x09,0x23,0x33,0x52,0xF0,0x15,0x62,
	0x72,0xD1,0x0A,0x16,0x24,0x34,0xE1,0x25,0xF1,0x17,0x18,0x19,0x1A,0x26,0x27,0x28,0x29,0x2A,
	0x35,0x36,0x37,0x38,0x39,0x3A,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x53,0x54,0x55,0x56,
	0x57,0x58,0x59,0x5A,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x73,0x74,0x75,0x76,0x77,0x78,
	0x79,0x7A,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x92,0x93,0x94,0x95,0x96,0x97,0x98,
	0x99,0x9A,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,
	0xB9,0xBA,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,
	0xD9,0xDA,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,
	0xF9,0xFA
};

IVDJPEGEncoder *VDCreateJPEGEncoder() {
	return new VDJPEGEncoder;
}

void VDJPEGEncoder::Transform_RGB15(uint8 *dstY, uint8 *dstCb, uint8 *dstCr, ptrdiff_t yccpitch, const uint8 *src, ptrdiff_t srcpitch, int w, int h) {
	ptrdiff_t extra = yccpitch - w;

	srcpitch -= 2*w;

	do {
		int x = -w;
		do {
			uint32 v = mColorTables[4][src[0]] + mColorTables[3][src[1]];
			src += 2;

			*dstY++		= (uint8)(v >> 24);
			*dstCb++	= (uint8)(v >> 12);
			*dstCr++	= (uint8)(v >> 2);
		} while(++x);

		dstY += extra;
		dstCr += extra;
		dstCb += extra;
		src += srcpitch;
	} while(--h);
}

void VDJPEGEncoder::Transform_RGB24(uint8 *dstY, uint8 *dstCb, uint8 *dstCr, ptrdiff_t yccpitch, const uint8 *src, ptrdiff_t srcpitch, int w, int h) {
	ptrdiff_t extra = yccpitch - w;

	srcpitch -= 3*w;

	do {
		int x = -w;
		do {
			uint32 v = mColorTables[2][src[0]] + mColorTables[1][src[1]] + mColorTables[0][src[2]];
			src += 3;

			*dstY++		= (uint8)(v >> 24);
			*dstCb++	= (uint8)(v >> 12);
			*dstCr++	= (uint8)(v >> 2);
		} while(++x);

		dstY += extra;
		dstCr += extra;
		dstCb += extra;
		src += srcpitch;
	} while(--h);
}

void VDJPEGEncoder::Transform_RGB32(uint8 *dstY, uint8 *dstCb, uint8 *dstCr, ptrdiff_t yccpitch, const uint8 *src, ptrdiff_t srcpitch, int w, int h) {
	ptrdiff_t extra = yccpitch - w;

	srcpitch -= 4*w;

	do {
		int x = -w;
		do {
			uint32 v = mColorTables[2][src[0]] + mColorTables[1][src[1]] + mColorTables[0][src[2]];
			src += 4;

			*dstY++		= (uint8)(v >> 24);
			*dstCb++	= (uint8)(v >> 12);
			*dstCr++	= (uint8)(v >> 2);
		} while(++x);

		dstY += extra;
		dstCr += extra;
		dstCb += extra;
		src += srcpitch;
	} while(--h);
}

void VDJPEGEncoder::Downsample_422(uint8 *p, ptrdiff_t pitch, int qw, int qh) {
	pitch -= qw*2;

	do {
		uint8 *dst = p;

		int x = -qw;
		do {
			*dst++ = (uint8)((p[0] + p[1] + 1) >> 1);
			p += 2;
		} while(++x);

		p += pitch;
	} while(--qh);
}

void VDJPEGEncoder::Downsample_420(uint8 *p, ptrdiff_t pitch, int qw, int qh) {
	ptrdiff_t step = pitch*2 - qw*2;

	do {
		uint8 *dst = p;
		const uint8 *p2 = p + pitch;

		int x = -qw;
		do {
			*dst++ = (uint8)((p[0] + p[1] + p2[0] + p2[1] + 2) >> 2);
			p += 2;
			p2 += 2;
		} while(++x);

		p += step;
	} while(--qh);
}

// This algorithm does an 8-point forward DCT in 29 adds, 13 multiplies.
// For an 8x8 2D-IDCT, that's 464a208m.  LLM is 29a12m, I think.
// Feig-Winograd is a LOT faster at 464a56m and has the advantage of doing
// only one layer of multiplies, but it's considerably more complex to
// implement.

namespace {
	const int c1 = 2009;		// cos(1*pi/16) << 11
	const int c3 = 1703;		// cos(3*pi/16) << 11
	const int c5 = 1138;		// cos(5*pi/16) << 11
	const int c7 = 400;		// cos(7*pi/16) << 11
	const int c2r2 = 2676;	// cos(2*pi/16)*sqrt(2) << 11
	const int c6r2 = 1108;	// cos(6*pi/16)*sqrt(2) << 11
	const int r2 = 2896;		// sqrt(2) << 11

	template<int stride, int postshift, class T>
	struct fdct_llm {
		enum { round = 1<<(10 + postshift), shift = 11 + postshift, postround = (1<<postshift)>>1 };

		static void go(int *dst, const T *x) { 
			int s0 = x[0*stride];
			int s1 = x[1*stride];
			int s2 = x[2*stride];
			int s3 = x[3*stride];
			int s4 = x[4*stride];
			int s5 = x[5*stride];
			int s6 = x[6*stride];
			int s7 = x[7*stride];
			int t0, t1, t2, t3, t4, t5, t6, t7;
			int tmp;

			t0 = s0+s7;
			t1 = s1+s6;
			t2 = s2+s5;
			t3 = s3+s4;
			t4 = s3-s4;
			t5 = s2-s5;
			t6 = s1-s6;
			t7 = s0-s7;

			s0 = t0+t3;
			s1 = t1+t2;
			s2 = t1-t2;
			s3 = t0-t3;

			t0 = s0+s1;
			t1 = s0-s1;
			tmp = c6r2*(s3+s2);
			t2 = tmp + s3*(c2r2-c6r2);
			t3 = tmp - s2*(c2r2+c6r2);
			dst[0*stride] = (t0 + postround) >> postshift;
			dst[2*stride] = (t2 + round) >> shift;
			dst[4*stride] = (t1 + postround) >> postshift;
			dst[6*stride] = (t3 + round) >> shift;

			tmp = c3*(t7+t4);
			s4 = tmp + t7*(c5-c3);
			s7 = tmp - t4*(c5+c3);

			tmp = c1*(t6+t5);
			s5 = tmp + t6*(c7-c1);
			s6 = tmp - t5*(c7+c1);

			t4 = s4+s6;
			t5 =((s7-s5 + 1024) >> 11) * r2;
			t6 =((s4-s6 + 1024) >> 11) * r2;
			t7 = s7+s5;
			dst[1*stride] = (t7 + t4 + round) >> shift;
			dst[3*stride] = (t5 + round) >> shift;
			dst[5*stride] = (t6 + round) >> shift;
			dst[7*stride] = (t7 - t4 + round) >> shift;
		}
	};
}

void VDJPEGEncoder::fdct(int dct_coeff[64], const uint8 *src, ptrdiff_t srcpitch) {
	int i;

	for(i=0; i<8; ++i) {
		fdct_llm<1, 0, unsigned char>::go(dct_coeff + i*8, src);
		src += srcpitch;
	}

	for(i=0; i<8; ++i)
		fdct_llm<8, 3, int>::go(dct_coeff + i, dct_coeff + i);
}

void VDJPEGEncoder::encode(uint32 encoding, int encoding_bits) {
	mBitHeap <<= encoding_bits;
	mBitHeap += encoding;
	mBitCount += encoding_bits;

	while(mBitCount >= 16) {
		uint8 c = (uint8)(mBitHeap >> (mBitCount-8));
		WriteChar((uint8)c);
		if (c == 0xFF)
			WriteChar(0);

		mBitCount -= 16;

		c = (uint8)(mBitHeap >> mBitCount);
		WriteChar((uint8)c);
		if (c == 0xFF)
			WriteChar(0);
	}
}

int VDJPEGEncoder::size_value(int coeff) {
	static const unsigned char kBSRLookupTable[64]={
		0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
	};

	int value_bits = 0;
	int mag = abs(coeff);

	while(mag >= 64) {
		mag >>= 6;
		value_bits += 6;
	}

	value_bits += kBSRLookupTable[mag];

	return value_bits;
}

void VDJPEGEncoder::encode_value(int run, int coeff, const unsigned (*enctab)[2]) {
	int code = run << 4;
	int value_bits = 0;

	if (coeff) {
		value_bits = size_value(coeff);

		coeff += (coeff>>31);
		coeff &= (1<<value_bits)-1;
	}

	code += value_bits;

	unsigned encoding = enctab[code][0];
	unsigned encoding_bits = enctab[code][1];

	encode(encoding, encoding_bits);
	encode(coeff, value_bits);
}

unsigned VDJPEGEncoder::tally_value(int run, int coeff, VDJPEGHuffmanTable& hufftab) {
	int code = run << 4;
	int value_bits = 0;

	if (coeff) {
		value_bits = size_value(coeff);

		coeff += (coeff>>31);
		coeff &= (1<<value_bits)-1;
	}

	code += value_bits;

	hufftab.Tally((uint8)(code));

	return code + (coeff<<16);
}

void VDJPEGEncoder::dht_to_encoding_table(unsigned (*enctab)[2], const unsigned char *dht_seg) {
	unsigned code = 0;
	unsigned inc = 0x8000;

	const unsigned char *pCounts = dht_seg;
	const unsigned char *pCodes = dht_seg + 16;

	for(int bits=1; bits<=16; ++bits) {
		int count = *pCounts++;

		while(count--) {
			int v = *pCodes++;

			enctab[v][0] = code >> (16-bits);
			enctab[v][1] = bits;
			code += inc;
		}

		inc >>= 1;
	}
}

void VDJPEGEncoder::WriteChar(unsigned char c) {
	mpDst->push_back(c);
}

void VDJPEGEncoder::WriteBlock(const void *block, int block_len) {
	mpDst->insert(mpDst->end(), (const char *)block, (const char *)block + block_len);
}

void VDJPEGEncoder::Init(int q, bool bOptimizeHuffmanTables, eChromaMode cmode) {

	mbOptimizeHuffmanTables = bOptimizeHuffmanTables;

	mDCHuffman[0].Init();
	mDCHuffman[1].Init();
	mACHuffman[0].Init();
	mACHuffman[1].Init();

	static const unsigned char base_quant_Y[64] = {
		16, 11, 10, 16,  24,  40,  51,  61,
		12, 12, 14, 19,  26,  58,  60,  55,
		14, 13, 16, 24,  40,  57,  69,  56,
		14, 17, 22, 29,  51,  87,  80,  62,
		18, 22, 37, 56,  68, 109, 103,  77,
		24, 35, 55, 64,  81, 104, 113,  92,
		49, 64, 78, 87, 103, 121, 120, 101,
		72, 92, 95, 98, 112, 100, 103,  99,
	};

	static const unsigned char base_quant_C[64] = {
		17, 18, 24, 47, 99, 99, 99, 99,
		18, 21, 26, 66, 99, 99, 99, 99,
		24, 26, 56, 99, 99, 99, 99, 99,
		47, 66, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99,
		99, 99, 99, 99, 99, 99, 99, 99,
	};

	// Scale the base quantizers into the target ones.  We use the same
	// ramp as Tom Lane's IJG library, to match scales that people are
	// used to.

	const int scale = q<50 ? 5000/q : 200-(q+q);
	int i, j;

	for(i=0; i<64; ++i) {
		int v = (base_quant_Y[i] * scale + 50) / 100;

		if (v < 1)
			v = 1;
		if (v > 255)
			v = 255;

		mQuant[0][i] = v;

		v = (base_quant_C[i] * scale + 50) / 100;

		if (v < 1)
			v = 1;
		if (v > 255)
			v = 255;

		mQuant[1][i] = v;
	}

	// Compute inverse quantizers.
	for(j=0; j<2; ++j)
		for(i=0; i<64; ++i) {
			if (mQuant[j][i] == 1)
				mInvQuant[j][i] = 0xFFFFFFFF;
			else
				mInvQuant[j][i] = (uint32)(0x100000000 / mQuant[j][i]);
		}

	mChromaMode = cmode;

	// Compute color conversion tables.
	//
	// From JFIF spec:
	//
	//	RGB to YCbCr Conversion
	//	-----------------------
	//	YCbCr (256 levels) can be computed directly from 8-bit RGB as follows:
	//
	//	Y   =     0.299  R + 0.587  G + 0.114  B
	//	Cb  =   - 0.1687 R - 0.3313 G + 0.5    B + 128
	//	Cr  =     0.5    R - 0.4187 G - 0.0813 B + 128
	//
	// We pack the coefficients 12:10:10, YCbCr.

	for(i=0; i<256; ++i) {
		sint32 r_y	= (sint32)((0x007f0000 + i * 0x4c8b44) & 0xfff00000);
		sint32 r_cb	= (sint32)((0x00080700 - i * 0x0002b3) & 0xfffffc00);
		sint32 r_cr	=((sint32)((0x00080700 + i * 0x000800) & 0xfffffc00) >> 10);
		sint32 g_y	= (sint32)((0x00090000 + i * 0x9645a2) & 0xfff00000);
		sint32 g_cb	= (sint32)((0x000002fb - i * 0x00054d) & 0xfffffc00);
		sint32 g_cr	=((sint32)((0x00000220 - i * 0x0006b3) & 0xfffffc00) >> 10);
		sint32 b_y	= (sint32)((0x00080000 + i * 0x1d2f1a) & 0xfff00000);
		sint32 b_cb	= (sint32)((0x00000200 + i * 0x000800) & 0xfffffc00);
		sint32 b_cr	=((sint32)((0x000003e0 - i * 0x00014d) & 0xfffffc00) >> 10);

		mColorTables[0][i] = r_y + r_cb + r_cr;
		mColorTables[1][i] = g_y + g_cb + g_cr;
		mColorTables[2][i] = b_y + b_cb + b_cr;
	}

	for(i=0; i<256; ++i) {
		int r1 = (i & 0x7c) >> 2;
		int g1 = (i & 0x03) << 3;
		int g2 = (i & 0xe0) >> 5;
		int b2 = (i & 0x1f);

		mColorTables[3][i] = mColorTables[0][(r1*33)>>2] + mColorTables[1][(g1*33)>>2];
		mColorTables[4][i] = mColorTables[1][(g2*33)>>2] + mColorTables[2][(b2*33)>>2];
	}
}

void VDJPEGEncoder::WriteHeader(int w, int h, bool bWriteCustomDHT) {
	WriteChar(0xff);
	WriteChar(SOI);

	///////////////////////////////////////
	//
	// JFIF header
	//
	///////////////////////////////////////

	static const struct JFIF_header_t {
		unsigned char	marker;
		unsigned char	type;
		unsigned char	length_hi, length_lo;
		char			tag[5];
		unsigned char	major_version, minor_version;
		unsigned char	units;
		unsigned char	xaspect_hi, xaspect_lo;
		unsigned char	yaspect_hi, yaspect_lo;
		unsigned char	thumbnail_width;
		unsigned char	thumbnail_height;
	} JFIF_hdr={
		0xff,
		APP0,
		0,16,		// length = 16 (including length but excluding APP0)
		"JFIF",		// "JFIF\0"
		1,2,		// rev 1.02
		0,			// no units (specify aspect ratio)
		0,1,		// Xaspect = 1
		0,1,		// Yaspect = 1
		0,			// thumbnail width = 0
		0,			// thumbnail height = 0
	};

	WriteBlock(&JFIF_hdr, sizeof JFIF_hdr);

	///////////////////////////////////////
	//
	// define huffman tables (DHT)
	//
	// for now, we use standard MJPEG tables
	//
	///////////////////////////////////////

	if (bWriteCustomDHT) {
		int dc0_len = mDCHuffman[0].GetDHTSegmentLen();
		int dc1_len = mDCHuffman[1].GetDHTSegmentLen();
		int ac0_len = mACHuffman[0].GetDHTSegmentLen();
		int ac1_len = mACHuffman[1].GetDHTSegmentLen();

		WriteChar(0xff);
		WriteChar(DHT);
		int len = dc0_len + dc1_len + ac0_len + ac1_len + 6;
		WriteChar((uint8)(len>>8));		// length = xx (including length but excluding DHT)
		WriteChar((uint8)len);

		WriteChar(0x00);	// target DC table 0

		WriteBlock(mDCHuffman[0].GetDHTSegment(), dc0_len);

		WriteChar(0x01);	// target DC table 1

		WriteBlock(mDCHuffman[1].GetDHTSegment(), dc1_len);

		WriteChar(0x10);	// target AC table 0

		WriteBlock(mACHuffman[0].GetDHTSegment(), ac0_len);

		WriteChar(0x11);	// target AC table 1

		WriteBlock(mACHuffman[1].GetDHTSegment(), ac1_len);
	} else {
		int dc0_len = sizeof stock_Huffman_DC0;
		int dc1_len = sizeof stock_Huffman_DC1;
		int ac0_len = sizeof stock_Huffman_AC0;
		int ac1_len = sizeof stock_Huffman_AC1;

		WriteChar(0xff);
		WriteChar(DHT);
		int len = dc0_len + dc1_len + ac0_len + ac1_len + 6;
		WriteChar((uint8)(len>>8));		// length = xx (including length but excluding DHT)
		WriteChar((uint8)len);

		WriteChar(0x00);	// target DC table 0

		WriteBlock(stock_Huffman_DC0, dc0_len);

		WriteChar(0x01);	// target DC table 1

		WriteBlock(stock_Huffman_DC1, dc1_len);

		WriteChar(0x10);	// target AC table 0

		WriteBlock(stock_Huffman_AC0, ac0_len);

		WriteChar(0x11);	// target AC table 1

		WriteBlock(stock_Huffman_AC1, ac1_len);
	}


	///////////////////////////////////////
	//
	// define quantization tables (DQT)
	//
	///////////////////////////////////////

	WriteChar(0xff);
	WriteChar(DQT);
	WriteChar(0);		// length = 132 (including length but excluding DQT)
	WriteChar(132);
	WriteChar(0x00);	// precision = 8 bits, define table 0 (Y)

	int i;

	for(i=0; i<64; ++i)
		WriteChar((uint8)mQuant[0][zigzag[i]]);

	WriteChar(0x01);	// precision = 8 bits, define table 1 (C)

	for(i=0; i<64; ++i)
		WriteChar((uint8)mQuant[1][zigzag[i]]);

	///////////////////////////////////////
	//
	// start of baseline frame (SOF0)
	//
	///////////////////////////////////////

	struct frame_header_t {
		unsigned char	marker;
		unsigned char	type;
		unsigned char	length_hi, length_lo;
		unsigned char	precision;
		unsigned char	width_hi, width_lo;
		unsigned char	height_hi, height_lo;
		unsigned char	component_count;

		struct component_info_t {
			unsigned char	id;
			unsigned char	sampling;
			unsigned char	quant;
		} components[3];
	} frame_hdr={
		0xff,
		SOF0,
		0,		// length = 17 (including length but excluding SOF)
		17,
		8,		// 8-bit samples
		(uint8)(h>>8),	// height
		(uint8)(h&255),
		(uint8)(w>>8),	// width
		(uint8)(w&255),
		3,		// three components

		// first component (Y)
		1,		// component identifier
		0x22,	// 2x2 = 4 blocks per MCU
		0,		// select quantization table 0

		// second component (Cb)
		2,		// component identifier
		0x11,	// 1x1 = 1 block per MCU
		1,		// select quantization table 1

		// third component (Cr)
		3,		// component identifier
		0x11,	// 1x1 = 1 block per MCU
		1,		// select quantization table 1
	};

	switch(mChromaMode) {
	case kYCC444:
		frame_hdr.components[0].sampling = 0x11;
		break;
	case kYCC422:
		frame_hdr.components[0].sampling = 0x21;
		break;
	case kYCC420:
		frame_hdr.components[0].sampling = 0x22;
		break;
	}

	WriteBlock(&frame_hdr, sizeof frame_hdr);

	///////////////////////////////////////
	//
	// start of scan (SOS)
	//
	///////////////////////////////////////

	static const struct scan_info_t {
		unsigned char	marker;
		unsigned char	type;
		unsigned char	length_hi, length_lo;
		unsigned char	component_count;

		struct component_info_t {
			unsigned char id;
			unsigned char table_select;
		} components[3];

		unsigned char	first_coeff;
		unsigned char	last_coeff;
		unsigned char	approx_bounds;
	} scan_hdr={
		0xff,
		SOS,
		0,		// length = 12 (including length but excluding SOS)
		12,
		3,		// three components
		1,		// first component is Y
		0x00,	// use DC table 0 and AC table 0
		2,		// second component is Cb
		0x11,	// use DC table 1 and AC table 1
		3,		// third component is Cr
		0x11,	// use DC table 1 and AC table 1
		0,		// first spectral component (must be 0 for baseline)
		63,		// last spectral component (must be 63 for baseline)
		0x00,	// successive approximation values (must be 0 for sequential)
	};

	WriteBlock(&scan_hdr, sizeof scan_hdr);
}

namespace {
#ifndef _M_IX86
	void __stdcall QuantizeCoefficients(int dst[64], const int coeffs[64], const unsigned iquant[64]) {
#define ITER(i,o)	dst[o] = (int)((coeffs[i] * (uint64)iquant[i] - (iquant[i] & (coeffs[i] >> 31)) + 0x80000000) >> 32);

	ITER( 0, 0)
	ITER( 1, 1)
	ITER( 8, 2)
	ITER(16, 3)
	ITER( 9, 4)
	ITER( 2, 5)
	ITER( 3, 6)
	ITER(10, 7)
	ITER(17, 8)
	ITER(24, 9)
	ITER(32,10)
	ITER(25,11)
	ITER(18,12)
	ITER(11,13)
	ITER( 4,14)
	ITER( 5,15)
	ITER(12,16)
	ITER(19,17)
	ITER(26,18)
	ITER(33,19)
	ITER(40,20)
	ITER(48,21)
	ITER(41,22)
	ITER(34,23)
	ITER(27,24)
	ITER(20,25)
	ITER(13,26)
	ITER( 6,27)
	ITER( 7,28)
	ITER(14,29)
	ITER(21,30)
	ITER(28,31)
	ITER(35,32)
	ITER(42,33)
	ITER(49,34)
	ITER(56,35)
	ITER(57,36)
	ITER(50,37)
	ITER(43,38)
	ITER(36,39)
	ITER(29,40)
	ITER(22,41)
	ITER(15,42)
	ITER(23,43)
	ITER(30,44)
	ITER(37,45)
	ITER(44,46)
	ITER(51,47)
	ITER(58,48)
	ITER(59,49)
	ITER(52,50)
	ITER(45,51)
	ITER(38,52)
	ITER(31,53)
	ITER(39,54)
	ITER(46,55)
	ITER(53,56)
	ITER(60,57)
	ITER(61,58)
	ITER(54,59)
	ITER(47,60)
	ITER(55,61)
	ITER(62,62)
	ITER(63,63)
#undef ITER
	}
#else
	void __declspec(naked) __stdcall QuantizeCoefficients(int dst[64], const int coeffs[64], const unsigned iquant[64]) {
		__asm {
			push	ebp
			push	edi
			push	ebx
			mov		ebp, [esp+16]
			mov		edi, [esp+20]
			mov		ebx, [esp+24]
		}
#define ITER(i,o)							\
			__asm	mov		eax, [edi+i*4]			\
			__asm	cdq								\
			__asm	mov		ecx, [ebx+i*4]			\
			__asm	and		ecx, edx				\
			__asm	mul		dword ptr [ebx+i*4]		\
			__asm	sub		edx, ecx				\
			__asm	add		eax, 80000000h			\
			__asm	adc		edx, 0					\
			__asm	mov		[ebp+o*4], edx

	ITER( 0, 0)
	ITER( 1, 1)
	ITER( 8, 2)
	ITER(16, 3)
	ITER( 9, 4)
	ITER( 2, 5)
	ITER( 3, 6)
	ITER(10, 7)
	ITER(17, 8)
	ITER(24, 9)
	ITER(32,10)
	ITER(25,11)
	ITER(18,12)
	ITER(11,13)
	ITER( 4,14)
	ITER( 5,15)
	ITER(12,16)
	ITER(19,17)
	ITER(26,18)
	ITER(33,19)
	ITER(40,20)
	ITER(48,21)
	ITER(41,22)
	ITER(34,23)
	ITER(27,24)
	ITER(20,25)
	ITER(13,26)
	ITER( 6,27)
	ITER( 7,28)
	ITER(14,29)
	ITER(21,30)
	ITER(28,31)
	ITER(35,32)
	ITER(42,33)
	ITER(49,34)
	ITER(56,35)
	ITER(57,36)
	ITER(50,37)
	ITER(43,38)
	ITER(36,39)
	ITER(29,40)
	ITER(22,41)
	ITER(15,42)
	ITER(23,43)
	ITER(30,44)
	ITER(37,45)
	ITER(44,46)
	ITER(51,47)
	ITER(58,48)
	ITER(59,49)
	ITER(52,50)
	ITER(45,51)
	ITER(38,52)
	ITER(31,53)
	ITER(39,54)
	ITER(46,55)
	ITER(53,56)
	ITER(60,57)
	ITER(61,58)
	ITER(54,59)
	ITER(47,60)
	ITER(55,61)
	ITER(62,62)
	ITER(63,63)

#undef ITER
		__asm {
			pop		ebx
			pop		edi
			pop		ebp
			ret		12
		}
	}
#endif
}

void VDJPEGEncoder::EncodeMacroblock(ComponentInfo *pComponent, int *const pCoeffs) {
	const unsigned (*pACEncode)[2] = pComponent->pACEncode;
	const unsigned (*pDCEncode)[2] = pComponent->pDCEncode;
	const int *pQuant = pComponent->pQuant;
	int v;

	int tmp[64];

	pCoeffs[0] -= pComponent->dc;
	QuantizeCoefficients(tmp, pCoeffs, pComponent->pInvQuant);

	// Encode DC coefficient.

	v = tmp[0];
	pComponent->dc += v * pQuant[0];

	encode_value(0, v, pDCEncode);

	// Un-zigzag, RLE, and Huffman-compress AC coefficients.

	int co = 1;
	int run = 0;

	while(co < 64) {
		v = tmp[co++];

		if (!v)
			++run;
		else {
			while(run >= 16) {
				run -= 16;

				encode_value(15, 0, pACEncode);	// 0xF0 is a special code for skip 16
			}

			encode_value(run, v, pACEncode);
			run = 0;
		}
	}

	// Write out end-of-block (0x00) if we AC coefficient 63 was zero.

	if (run)
		encode_value(0, 0, pACEncode);
}

void VDJPEGEncoder::TallyMacroblock(unsigned *&pHeap, ComponentInfo *pComponent, int *const pCoeffs) {
	VDJPEGHuffmanTable& actable = *pComponent->pACTable;
	VDJPEGHuffmanTable& dctable = *pComponent->pDCTable;
	const int *pQuant = pComponent->pQuant;
	int v;

	// Encode DC coefficient.

	int tmp[64];

	pCoeffs[0] -= pComponent->dc;
	QuantizeCoefficients(tmp, pCoeffs, pComponent->pInvQuant);

	v = tmp[0];
	pComponent->dc += v * pQuant[0];

	*pHeap++ = tally_value(0, v, dctable);

	// Un-zigzag, RLE, and Huffman-compress AC coefficients.

	int co = 1;
	int run = 0;

	while(co < 64) {
		v = tmp[co++];

		if (!v)
			++run;
		else {
			while(run >= 16) {
				run -= 16;

				*pHeap++ = tally_value(15, 0, actable);	// 0xF0 is a special code for skip 16
			}

			*pHeap++ = tally_value(run, v, actable);
			run = 0;
		}
	}

	// Write out end-of-block (0x00) if we AC coefficient 63 was zero.

	if (run)
		*pHeap++ = tally_value(0, 0, actable);

	*pHeap++ = 0xFFFFFFFFU;
}

void VDJPEGEncoder::EncodeMacroblock(unsigned *&pHeap, ComponentInfo *pComponent) {
	const unsigned (*pACEncode)[2] = pComponent->pACEncode;
	const unsigned (*pDCEncode)[2] = pComponent->pDCEncode;

	unsigned v;

	v = *pHeap++;

	encode(pDCEncode[0xff&v][0], pDCEncode[0xff&v][1]);
	encode(v>>16, v&15);

	while(0xFFFFFFFFU != (v = *pHeap++)) {
		encode(pACEncode[0xff&v][0], pACEncode[0xff&v][1]);
		encode(v>>16, v&15);
	}
}

void VDJPEGEncoder::Encode(vdfastvector<char>& dst, const char *src, ptrdiff_t srcpitch, int format, int w, int h) {
	mpDst = &dst;

	///////////////////////////////////////

	if (!mbOptimizeHuffmanTables) {
		WriteHeader(w, h, false);

		dht_to_encoding_table(mDCEncode[0], stock_Huffman_DC0);
		dht_to_encoding_table(mACEncode[0], stock_Huffman_AC0);
		dht_to_encoding_table(mDCEncode[1], stock_Huffman_DC1);
		dht_to_encoding_table(mACEncode[1], stock_Huffman_AC1);
	}

	///////////////////////////////////////
	//
	// encoding entropy data
	//
	///////////////////////////////////////

	ComponentInfo components[3], *pBlockComponents[6];

	// Y component
	components[0].dc		= 128*8;
	components[0].pQuant	= mQuant[0];
	components[0].pInvQuant	= mInvQuant[0];
	components[0].pDCEncode	= mDCEncode[0];
	components[0].pACEncode	= mACEncode[0];
	components[0].pDCTable	= &mDCHuffman[0];
	components[0].pACTable	= &mACHuffman[0];

	// Cb component
	components[1].dc		= 128*8;
	components[1].pQuant	= mQuant[1];
	components[1].pInvQuant	= mInvQuant[1];
	components[1].pDCEncode	= mDCEncode[1];
	components[1].pACEncode	= mACEncode[1];
	components[1].pDCTable	= &mDCHuffman[1];
	components[1].pACTable	= &mACHuffman[1];

	// Cr component
	components[2].dc		= 128*8;
	components[2].pQuant	= mQuant[1];
	components[2].pInvQuant	= mInvQuant[1];
	components[2].pDCEncode	= mDCEncode[1];
	components[2].pACEncode	= mACEncode[1];
	components[2].pDCTable	= &mDCHuffman[1];
	components[2].pACTable	= &mACHuffman[1];

	int dct_coeff[6][64];
	int mcu_width, mcu_height, mcu_size;

	mBitHeap = 0;
	mBitCount = 0;

	switch(mChromaMode) {
	case kYCC420:
		mcu_width = 16;
		mcu_height = 16;
		mcu_size = 6;
		break;

	case kYCC422:
		mcu_width = 16;
		mcu_height = 8;
		mcu_size = 4;
		break;

	case kYCC444:
		mcu_width = 8;
		mcu_height = 8;
		mcu_size = 3;
		break;
	}

	for(int i=0; i<mcu_size-2; ++i)
		pBlockComponents[i] = &components[0];

	pBlockComponents[mcu_size-2] = &components[1];
	pBlockComponents[mcu_size-1] = &components[2];

	// transform and encode all minimum coded units (MCUs)

	const int mcu_horiz_count = (w + mcu_width - 1) / mcu_width;
	const int mcu_vert_count = (h + mcu_height - 1) / mcu_height;
	const int clip_h = h % mcu_height;
	const int mcu_clip_y = clip_h ? mcu_vert_count-1 : -1;

	uint32 *pHeap;

	if (mbOptimizeHuffmanTables) {
		mCoefficientHeap.resize(65 * mcu_size * mcu_horiz_count * mcu_vert_count);
		pHeap = mCoefficientHeap.data();
	}

	mStripWidth		= ((w + 15) & ~15) + 4;
	mStripHeight	= mcu_height << 3;
	mStripBuffer.resize(mStripWidth * mStripHeight * 3 + 15, 0);

	mpYBuffer	= &mStripBuffer[15];
	mpYBuffer	-= (int)mpYBuffer & 15;
	mpCbBuffer	= mpYBuffer + mStripWidth * mStripHeight;
	mpCrBuffer	= mpCbBuffer + mStripWidth * mStripHeight;
	memset(mpCbBuffer, 0x80, mStripWidth * mStripHeight);
	memset(mpCrBuffer, 0x80, mStripWidth * mStripHeight);

	for(int y=0; y<mcu_vert_count; ++y) {
		int srch = mcu_height;

		if (y == mcu_clip_y)
			srch = clip_h;

		switch(format) {
		case kFormatRGB15:
			Transform_RGB15(mpYBuffer, mpCbBuffer, mpCrBuffer, mStripWidth, (const uint8 *)src, srcpitch, w, srch);
			break;
		case kFormatRGB24:
			Transform_RGB24(mpYBuffer, mpCbBuffer, mpCrBuffer, mStripWidth, (const uint8 *)src, srcpitch, w, srch);
			break;
		case kFormatRGB32:
			Transform_RGB32(mpYBuffer, mpCbBuffer, mpCrBuffer, mStripWidth, (const uint8 *)src, srcpitch, w, srch);
			break;
		}

		// replicate Y/C right
		if (w & 15) {
			uint8 *yp = mpYBuffer + w;
			uint8 *cbp = mpCbBuffer + w;
			uint8 *crp = mpCrBuffer + w;

			for(int yc=0; yc<srch; ++yc) {
				memset(yp, yp[-1], (-(int)w) & 15);
				memset(cbp, cbp[-1], (-(int)w) & 15);
				memset(crp, crp[-1], (-(int)w) & 15);

				yp += mStripWidth;
				cbp += mStripWidth;
				crp += mStripWidth;
			}
		}

		// replicate Y/C down
		for(int ypad = srch; ypad < mcu_height; ++ypad) {
			memcpy(mpYBuffer  + mStripWidth*ypad, mpYBuffer  + mStripWidth*(srch-1), mStripWidth);
			memcpy(mpCbBuffer + mStripWidth*ypad, mpCbBuffer + mStripWidth*(srch-1), mStripWidth);
			memcpy(mpCrBuffer + mStripWidth*ypad, mpCrBuffer + mStripWidth*(srch-1), mStripWidth);
		}

		src += srch * srcpitch;

		switch(mChromaMode) {
			case kYCC444:
				break;
			case kYCC422:
				Downsample_422(mpCbBuffer, mStripWidth, (w+1) >> 1, srch);
				Downsample_422(mpCrBuffer, mStripWidth, (w+1) >> 1, srch);
				break;
			case kYCC420:
				Downsample_420(mpCbBuffer, mStripWidth, (w+1) >> 1, (srch+1) >> 1);
				Downsample_420(mpCrBuffer, mStripWidth, (w+1) >> 1, (srch+1) >> 1);
				break;
		}

		const uint8 *ysrc = mpYBuffer;
		const uint8 *cbsrc = mpCbBuffer;
		const uint8 *crsrc = mpCrBuffer;

		for(int x=0; x<mcu_horiz_count; ++x) {
			int block;

			switch(mChromaMode) {
			case kYCC420:
				fdct(dct_coeff[0], ysrc, mStripWidth);
				fdct(dct_coeff[1], ysrc+8, mStripWidth);
				fdct(dct_coeff[2], ysrc+(mStripWidth<<3), mStripWidth);
				fdct(dct_coeff[3], ysrc+(mStripWidth<<3)+8, mStripWidth);
				fdct(dct_coeff[4], cbsrc, mStripWidth*2);
				fdct(dct_coeff[5], crsrc, mStripWidth*2);
				ysrc += 16;
				break;
			case kYCC422:
				fdct(dct_coeff[0], ysrc, mStripWidth);
				fdct(dct_coeff[1], ysrc+8, mStripWidth);
				fdct(dct_coeff[2], cbsrc, mStripWidth);
				fdct(dct_coeff[3], crsrc, mStripWidth);
				ysrc += 16;
				break;
			case kYCC444:
				fdct(dct_coeff[0], ysrc, mStripWidth);
				fdct(dct_coeff[1], cbsrc, mStripWidth);
				fdct(dct_coeff[2], crsrc, mStripWidth);
				ysrc += 8;
				break;
			}

			crsrc += 8;
			cbsrc += 8;

			if (mbOptimizeHuffmanTables) {
				for(block=0; block<mcu_size; ++block)
					TallyMacroblock(pHeap, pBlockComponents[block], dct_coeff[block]);
			} else {
				for(block=0; block<mcu_size; ++block)
					EncodeMacroblock(pBlockComponents[block], dct_coeff[block]);
			}
		}
	}

	if (mbOptimizeHuffmanTables) {
		mDCHuffman[0].BuildCode();
		mACHuffman[0].BuildCode();
		mDCHuffman[1].BuildCode();
		mACHuffman[1].BuildCode();

		WriteHeader(w, h, true);

		for(int i=0; i<16; ++i)
			mDCEncode[0][i][1] = mDCEncode[1][i][1] = -1;

		for(int j=0; j<256; ++j)
			mACEncode[0][j][1] = mACEncode[1][j][1] = -1;

		dht_to_encoding_table(mDCEncode[0], mDCHuffman[0].GetDHTSegment());
		dht_to_encoding_table(mACEncode[0], mACHuffman[0].GetDHTSegment());
		dht_to_encoding_table(mDCEncode[1], mDCHuffman[1].GetDHTSegment());
		dht_to_encoding_table(mACEncode[1], mACHuffman[1].GetDHTSegment());

		pHeap = mCoefficientHeap.data();

		for(int mcu = 0; mcu < mcu_horiz_count * mcu_vert_count; ++mcu) {
			for(int block = 0; block < mcu_size; ++block)
				EncodeMacroblock(pHeap, pBlockComponents[block]);
		}
	}

	// flush any remaining bits

	if (mBitCount & 7) {
		int pad = 8-(mBitCount & 7);

		mBitHeap <<= pad;
		mBitHeap += (1<<pad)-1;
		mBitCount += pad;
	}

	while(mBitCount >= 8) {
		mBitCount -= 8;
		uint8 c = (uint8)(mBitHeap >> mBitCount);
		WriteChar((uint8)c);
		if (c == 0xFF)
			WriteChar(0);
	}

	///////////////////////////////////////
	//
	// end of image (EOI)
	//
	///////////////////////////////////////

	WriteChar(0xFF);
	WriteChar(EOI);
}
