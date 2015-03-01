//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <vd2/system/atomic.h>
#include <vd2/system/memory.h>
#include <vd2/Meia/MPEGDecoder.h>
#include <vd2/Meia/MPEGPredict.h>
#include <vd2/Meia/MPEGConvert.h>
#include <vd2/Meia/MPEGIDCT.h>

///////////////////////////////////////////////////////////////////////////

#pragma optimize("agty",on)
#pragma inline_recursion(on)
#pragma inline_depth(32)

#pragma warning(push)
#pragma warning(disable:4035)

#if _MSC_VER >= 1300
	extern unsigned long _byteswap_ulong(unsigned long v);
	#pragma intrinsic(_byteswap_ulong)
	static inline unsigned long bswap(unsigned long v) {
		return _byteswap_ulong(v);
	}
#else
	static inline unsigned long bswap(unsigned long v) {
		__asm {
			mov eax,v
			bswap eax
		}
	}
#endif

#pragma warning(pop)

///////////////////////////////////////////////////////////////////////////

static const int zigzag_std[64]={
	 0,  1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63,
};

#define TAB4_REP1(run,level,bits) {run,(level<0?- level:level),bits,(level<0?-1:0)}
#define TAB4_REP2(run,level,bits) TAB4_REP1(run,level,bits),TAB4_REP1(run,level,bits)
#define TAB4_REP4(run,level,bits) TAB4_REP2(run,level,bits),TAB4_REP2(run,level,bits)
#define TAB4_REP8(run,level,bits) TAB4_REP4(run,level,bits),TAB4_REP4(run,level,bits)

//static const signed char dct_vshort_tab[22][4]={
static const signed char dct_vshort_tab[54][4]={
	TAB4_REP1(0,+3,6),	// 001010
	TAB4_REP1(0,-3,6),	// 001011
	TAB4_REP1(4,+1,6),	// 001100
	TAB4_REP1(4,-1,6),	// 001101
	TAB4_REP1(3,+1,6),	// 001110
	TAB4_REP1(3,-1,6),	// 001111
	TAB4_REP2(0,+2,5),	// 01000
	TAB4_REP2(0,-2,5),	// 01001
	TAB4_REP2(2,+1,5),	// 01010
	TAB4_REP2(2,-1,5),	// 01011
	TAB4_REP4(1,+1,4),	// 0110
	TAB4_REP4(1,-1,4),	// 0111

	TAB4_REP8(0, 0,0),	// 100 (escape)
	TAB4_REP8(0, 0,0),	// 101 (escape)
	TAB4_REP8(0,+1,3),	// 110
	TAB4_REP8(0,-1,3),	// 111
};

static const signed char dct_short_tab[64][4]={
	TAB4_REP2( 2,+2,8),	// 00001000
	TAB4_REP2( 2,-2,8),	// 00001001
	TAB4_REP2( 9,+1,8),	// 00001010
	TAB4_REP2( 9,-1,8),	// 00001011
	TAB4_REP2( 0,+4,8),	// 00001100
	TAB4_REP2( 0,-4,8),	// 00001101
	TAB4_REP2( 8,+1,8),	// 00001110
	TAB4_REP2( 8,-1,8),	// 00001111
	TAB4_REP4( 7,+1,7),	// 0001000
	TAB4_REP4( 7,-1,7),	// 0001001
	TAB4_REP4( 6,+1,7),	// 0001010
	TAB4_REP4( 6,-1,7),	// 0001011
	TAB4_REP4( 1,+2,7),	// 0001100
	TAB4_REP4( 1,-2,7),	// 0001101
	TAB4_REP4( 5,+1,7),	// 0001110
	TAB4_REP4( 5,-1,7),	// 0001111
	TAB4_REP1(13,+1,9),	// 001000000
	TAB4_REP1(13,-1,9),	// 001000001
	TAB4_REP1( 0,+6,9),	// 001000010
	TAB4_REP1( 0,-6,9),	// 001000011
	TAB4_REP1(12,+1,9),	// 001000100
	TAB4_REP1(12,-1,9),	// 001000101
	TAB4_REP1(11,+1,9),	// 001000110
	TAB4_REP1(11,-1,9),	// 001000111
	TAB4_REP1( 3,+2,9),	// 001001000
	TAB4_REP1( 3,-2,9),	// 001001001
	TAB4_REP1( 1,+3,9),	// 001001010
	TAB4_REP1( 1,-3,9),	// 001001011
	TAB4_REP1( 0,+5,9),	// 001001100
	TAB4_REP1( 0,-5,9),	// 001001101
	TAB4_REP1(10,+1,9),	// 001001110
	TAB4_REP1(10,-1,9),	// 001001111
};

static const unsigned char dct_long_tab[112][4]={
	TAB4_REP1(10, 2,7),		// 000000 0010000
	TAB4_REP1( 9, 2,7),		// 000000 0010001
	TAB4_REP1( 5, 3,7),		// 000000 0010010
	TAB4_REP1( 3, 4,7),		// 000000 0010011
	TAB4_REP1( 2, 5,7),		// 000000 0010100
	TAB4_REP1( 1, 7,7),		// 000000 0010101
	TAB4_REP1( 1, 6,7),		// 000000 0010110
	TAB4_REP1( 0,15,7),		// 000000 0010111
	TAB4_REP1( 0,14,7),		// 000000 0011000
	TAB4_REP1( 0,13,7),		// 000000 0011001
	TAB4_REP1( 0,12,7),		// 000000 0011010
	TAB4_REP1(26, 1,7),		// 000000 0011011
	TAB4_REP1(25, 1,7),		// 000000 0011100
	TAB4_REP1(24, 1,7),		// 000000 0011101
	TAB4_REP1(23, 1,7),		// 000000 0011110
	TAB4_REP1(22, 1,7),		// 000000 0011111
	TAB4_REP2( 0,11,6),		// 000000 010000
	TAB4_REP2( 8, 2,6),		// 000000 010001
	TAB4_REP2( 4, 3,6),		// 000000 010010
	TAB4_REP2( 0,10,6),		// 000000 010011
	TAB4_REP2( 2, 4,6),		// 000000 010100
	TAB4_REP2( 7, 2,6),		// 000000 010101
	TAB4_REP2(21, 1,6),		// 000000 010110
	TAB4_REP2(20, 1,6),		// 000000 010111
	TAB4_REP2( 0, 9,6),		// 000000 011000
	TAB4_REP2(19, 1,6),		// 000000 011001
	TAB4_REP2(18, 1,6),		// 000000 011010
	TAB4_REP2( 1, 5,6),		// 000000 011011
	TAB4_REP2( 3, 3,6),		// 000000 011100
	TAB4_REP2( 0, 8,6),		// 000000 011101
	TAB4_REP2( 6, 2,6),		// 000000 011110
	TAB4_REP2(17, 1,6),		// 000000 011111
	TAB4_REP8(16, 1,4),		// 000000 1000
	TAB4_REP8( 5, 2,4),		// 000000 1001
	TAB4_REP8( 0, 7,4),		// 000000 1010
	TAB4_REP8( 2, 3,4),		// 000000 1011
	TAB4_REP8( 1, 4,4),		// 000000 1100
	TAB4_REP8(15, 1,4),		// 000000 1101
	TAB4_REP8(14, 1,4),		// 000000 1110
	TAB4_REP8( 4, 2,4),		// 000000 1111
};

static const unsigned char dct_vlong_tab[112][4]={
	TAB4_REP1( 1,18,10),	// 000000 0000010000
	TAB4_REP1( 1,17,10),	// 000000 0000010001
	TAB4_REP1( 1,16,10),	// 000000 0000010010
	TAB4_REP1( 1,15,10),	// 000000 0000010011
	TAB4_REP1( 6, 3,10),	// 000000 0000010100
	TAB4_REP1(16, 2,10),	// 000000 0000010101
	TAB4_REP1(15, 2,10),	// 000000 0000010110
	TAB4_REP1(14, 2,10),	// 000000 0000010111
	TAB4_REP1(13, 2,10),	// 000000 0000011000
	TAB4_REP1(12, 2,10),	// 000000 0000011001
	TAB4_REP1(11, 2,10),	// 000000 0000011010
	TAB4_REP1(31, 1,10),	// 000000 0000011011
	TAB4_REP1(30, 1,10),	// 000000 0000011100
	TAB4_REP1(29, 1,10),	// 000000 0000011101
	TAB4_REP1(28, 1,10),	// 000000 0000011110
	TAB4_REP1(27, 1,10),	// 000000 0000011111
	TAB4_REP2( 0,40, 9),	// 000000 000010000
	TAB4_REP2( 0,39, 9),	// 000000 000010001
	TAB4_REP2( 0,38, 9),	// 000000 000010010
	TAB4_REP2( 0,37, 9),	// 000000 000010011
	TAB4_REP2( 0,36, 9),	// 000000 000010100
	TAB4_REP2( 0,35, 9),	// 000000 000010101
	TAB4_REP2( 0,34, 9),	// 000000 000010110
	TAB4_REP2( 0,33, 9),	// 000000 000010111
	TAB4_REP2( 0,32, 9),	// 000000 000011000
	TAB4_REP2( 1,14, 9),	// 000000 000011001
	TAB4_REP2( 1,13, 9),	// 000000 000011010
	TAB4_REP2( 1,12, 9),	// 000000 000011011
	TAB4_REP2( 1,11, 9),	// 000000 000011100
	TAB4_REP2( 1,10, 9),	// 000000 000011101
	TAB4_REP2( 1, 9, 9),	// 000000 000011110
	TAB4_REP2( 1, 8, 9),	// 000000 000011111
	TAB4_REP4( 0,31, 8),	// 000000 00010000
	TAB4_REP4( 0,30, 8),	// 000000 00010001
	TAB4_REP4( 0,29, 8),	// 000000 00010010
	TAB4_REP4( 0,28, 8),	// 000000 00010011
	TAB4_REP4( 0,27, 8),	// 000000 00010100
	TAB4_REP4( 0,26, 8),	// 000000 00010101
	TAB4_REP4( 0,25, 8),	// 000000 00010110
	TAB4_REP4( 0,24, 8),	// 000000 00010111
	TAB4_REP4( 0,23, 8),	// 000000 00011000
	TAB4_REP4( 0,22, 8),	// 000000 00011001
	TAB4_REP4( 0,21, 8),	// 000000 00011010
	TAB4_REP4( 0,20, 8),	// 000000 00011011
	TAB4_REP4( 0,19, 8),	// 000000 00011100
	TAB4_REP4( 0,18, 8),	// 000000 00011101
	TAB4_REP4( 0,17, 8),	// 000000 00011110
	TAB4_REP4( 0,16, 8),	// 000000 00011111
};

#undef TAB4_REP1
#undef TAB4_REP2
#undef TAB4_REP4
#undef TAB4_REP8

///////////////////////////////////////////////////////////////////////////

class VDMPEGDecoder : public IVDMPEGDecoder {
private:
	typedef unsigned char YCCSample;
	
	// critical stuff goes here -- we want short indices.

	// bit decoding -- this needs to be VERY FAST.

	unsigned long bitheap;
	const unsigned char *bitsrc;
	int bitcnt;		// 24-bits in heap

#if 0
	__forceinline void bitheap_reset(const unsigned char *src) {
		bitsrc = src+4;
		bitheap = bswap(*(const unsigned long *)src);
		bitcnt = -8;
	}

	__forceinline unsigned long bitheap_peekbits(unsigned int bits) {
		return bitheap >> (32-bits);
	}

	__forceinline bool bitheap_checkzerobits(unsigned int bits) {
//		return bitheap < (1UL<<(32-bits));
		return !bitheap_peekbits(bits);
	}

	__forceinline unsigned long bitheap_getbits(unsigned int bits) {
		unsigned long rv = bitheap >> (32-bits);

		bitcnt += bits;

		bitheap <<= bits;

		if (bits >= 16) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}

		if (bits >= 8) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}

		if (bitcnt >= 0) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}
		return rv;
	}

	__forceinline long bitheap_getbitssigned(unsigned int bits) {
		long rv = (long)bitheap >> (32-bits);

		bitcnt += bits;
		bitheap <<= bits;

		if (bits >= 16) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}

		if (bits >= 8) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}

		if (bitcnt >= 0) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}

		return rv;
	}

	__forceinline void bitheap_skipbits(unsigned int bits) {
		bitcnt += bits;
		bitheap <<= bits;

		if (bits >= 16) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}

		if (bits >= 8) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}

		if (bitcnt >= 0) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}
	}
#else
	__forceinline void bitheap_reset(const unsigned char *src) {
		bitsrc = src+4;
		bitheap = bswap(*(const unsigned long *)src);
		bitcnt = -8;
	}

	__forceinline unsigned long bitheap_peekbits(unsigned int bits) {
		return bitheap >> (32-bits);
	}

	__forceinline bool bitheap_checkzerobits(unsigned int bits) {
//		return bitheap < (1UL<<(32-bits));
		return !bitheap_peekbits(bits);
	}

	__forceinline long bitheap_getflag() {
		long rv = bitheap & 0x80000000;

		bitheap += bitheap;

		if (++bitcnt >= 0) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}

		return rv;
	}

	__forceinline unsigned long bitheap_getbitsconst(unsigned int bits) {
		unsigned long rv = bitheap >> (32-bits);

		bitcnt += bits;

		bitheap <<= bits;

		if (bits >= 16) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}

		if (bits >= 8) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}

		if ((bits&7) && bitcnt >= 0) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}
		return rv;
	}

	__forceinline unsigned long bitheap_getbits(unsigned int bits) {
		unsigned long rv = bitheap >> (32-bits);

		bitcnt += bits;

		bitheap <<= bits;

		while(bitcnt >= 0) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}
		return rv;
	}

	__forceinline long bitheap_getbitssigned(unsigned int bits) {
		long rv = (long)bitheap >> (32-bits);

		bitcnt += bits;
		bitheap <<= bits;

		while(bitcnt >= 0) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}

		return rv;
	}

	__forceinline long bitheap_getbitssignedconst(unsigned int bits) {
		long rv = (long)bitheap >> (32-bits);

		bitcnt += bits;
		bitheap <<= bits;

		if (bits >= 16) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}

		if (bits >= 8) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}

		if ((bits&7) && bitcnt >= 0) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}

		return rv;
	}

	__forceinline void bitheap_skipbitsconst(unsigned int bits) {
		bitcnt += bits;
		bitheap <<= bits;

		if (bits >= 16) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}

		if (bits >= 8) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}

		if ((bits&7) && bitcnt >= 0) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}
	}

	__forceinline void bitheap_skipbits(unsigned int bits) {
		bitcnt += bits;
		bitheap <<= bits;

		while(bitcnt >= 0) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}
	}

	__forceinline void bitheap_skipbits8(unsigned int bits) {
		bitcnt += bits;
		bitheap <<= bits;

		if (bitcnt >= 0) {
			bitheap += (unsigned long)*bitsrc++ << bitcnt;
			bitcnt -= 8;
		}
	}
#endif

	////////////////////////////////////

	enum {
		kMBF_NewQuant	= 16,
		kMBF_Both		= 12,
		kMBF_Forward	= 8,
		kMBF_Backward	= 4,
		kMBF_Pattern	= 2,
		kMBF_Intra		= 1
	};

	long		mnYPitch;
	long		mnYPitch8;
	long		mnCPitch;
	long		mnCPitch8;
	unsigned	mnBlockW, mnBlockH;
	int			mnLastY_DC;
	int			mnLastCr_DC;
	int			mnLastCb_DC;

	YCCSample *mpY, *mpCr, *mpCb;
	YCCSample *mpFwdY, *mpFwdCr, *mpFwdCb;
	YCCSample *mpBackY, *mpBackCr, *mpBackCb;

	int mnQuantValue;
	const int *mpZigzagOrder;

	void (VDMPEGDecoder::*mpBlockDecoder)(YCCSample *dst, long pitch, bool intra, int dc) throw();

	int		mnForwardRSize;
	int		mnForwardMask;
	int		mnForwardSignExtend;
	int		mnBackwardRSize;
	int		mnBackwardMask;
	int		mnBackwardSignExtend;

	bool	mbForwardFullPel;
	bool	mbBackwardFullPel;

	int mIntraQ[32][64], mNonintraQ[32][64];

	const int *mpCurrentIntraQ, *mpCurrentNonintraQ;

	// non-critical stuff here.

	VDAtomicInt mRefCount;
	
	struct MPEGBuffer {
		YCCSample *pY, *pCr, *pCb;
		YCCSample *pYBuffer, *pCBuffer;
		long frame;
	} *mpBuffers;
	int mnBuffers;
	unsigned	mnHeight;

	long	mErrorState;

	void (VDMPEGDecoder::*mpSliceDecoder)(int) throw();

	const VDMPEGPredictorSet	*mpPredictors;
	const VDMPEGConverterSet	*mpConverters;
	const VDMPEGIDCTSet			*mpIDCTs;

	int		mUnscaledIntraQ[64];
	int		mUnscaledNonintraQ[64];
	bool	mbQuantizersDirty;

	//////

	void SetError(long err) { mErrorState |= err; }

	void UpdateQuantizers();

	void DecodeBlockPrescaled(YCCSample *dst, long pitch, bool intra, int dc) throw();
	void DecodeBlockNonPrescaled(YCCSample *dst, long pitch, bool intra, int dc) throw();

	void DecodeBlock_Y(YCCSample *dst, bool intra) throw();
	void DecodeBlock_C(YCCSample *dst, int& dc, bool intra) throw();

	int DecodeMotionVector(int rsize) throw();
	int DecodeCodedBlockPattern() throw();

	void DecodeSlice_I(int) throw();
	void DecodeSlice_P(int) throw();
	void DecodeSlice_B(int) throw();

	void CopyPredictionForward(int posx, int posy, int dx, int dy);
	void CopyPredictionBackward(int posx, int posy, int dx, int dy);
	void AddPredictionBackward(int posx, int posy, int dx, int dy);

	void CopyPredictionY(YCCSample *dst, YCCSample *src, bool halfpelX, bool halfpelY);
	void CopyPredictionC(YCCSample *dst, YCCSample *src, bool halfpelX, bool halfpelY);
	void AddPredictionY(YCCSample *dst, YCCSample *src, bool halfpelX, bool halfpelY);
	void AddPredictionC(YCCSample *dst, YCCSample *src, bool halfpelX, bool halfpelY);

public:
	VDMPEGDecoder();
	~VDMPEGDecoder() throw();
	
	int AddRef() { return mRefCount.inc(); }
	int Release() { int rc = mRefCount.dec(); if (!rc) delete this; return rc; }
	
	// decoding
	
	bool Init(int width, int height);
	void Shutdown();
	void SetIntraQuantizers(const unsigned char *pMatrix);
	void SetNonintraQuantizers(const unsigned char *pMatrix);
	void SetPredictors(const VDMPEGPredictorSet *pPredictors);
	void SetConverters(const VDMPEGConverterSet *pConverters);
	void SetIDCTs(const VDMPEGIDCTSet *pIDCTs);
	
	int DecodeFrame(const void *src, long len, long frame, int dst, int fwd, int rev);
	long GetErrorState();
	
	// framebuffer access
	
	int GetFrameBuffer(long frame);
	long GetFrameNumber(int buffer);
	void CopyFrameBuffer(int dst, int src, long frameno);
	void SwapFrameBuffers(int dst, int src);
	void ClearFrameBuffers();
	const void *GetYBuffer(int buffer, ptrdiff_t& pitch);
	const void *GetCrBuffer(int buffer, ptrdiff_t& pitch);
	const void *GetCbBuffer(int buffer, ptrdiff_t& pitch);
	
	// framebuffer conversion

	inline bool Decode(void *dst, long pitch, int buffer, tVDMPEGConverter pConv) {
		if (buffer < 0 || buffer >= mnBuffers)
			return false;

		if (!pConv)
			return false;

		pConv((char *)dst, pitch,
				mpBuffers[buffer].pY, mnYPitch,
				mpBuffers[buffer].pCr,
				mpBuffers[buffer].pCb, mnCPitch, (int)mnBlockW, (int)mnHeight);

		return true;
	}

	bool DecodeUYVY(void *dst, ptrdiff_t pitch, int buffer);
	bool DecodeYUYV(void *dst, ptrdiff_t pitch, int buffer);
	bool DecodeYVYU(void *dst, ptrdiff_t pitch, int buffer);
	bool DecodeY41P(void *dst, ptrdiff_t pitch, int buffer);
	bool DecodeRGB15(void *dst, ptrdiff_t pitch, int buffer);
	bool DecodeRGB16(void *dst, ptrdiff_t pitch, int buffer);
	bool DecodeRGB24(void *dst, ptrdiff_t pitch, int buffer);
	bool DecodeRGB32(void *dst, ptrdiff_t pitch, int buffer);

private:
	template<class T>
	__forceinline void DecodeBlock(YCCSample *dst, ptrdiff_t pitch, bool intra, int dc, bool bPrescaled, T) {
		unsigned long v = bitheap_peekbits(32);
		T dct_coeff0[67];
		T *dct_coeff = (T *)(((uintptr)dct_coeff0 + 7) & ~7);
		const int *quant = mpCurrentIntraQ;
		int idx = 0;
		long level_addend = 0;

		memset(dct_coeff, 0, sizeof(*dct_coeff)*64);

		if (bPrescaled)
			dct_coeff[0] = (dc * quant[0] + 128)>>8;
		else
			dct_coeff[0] = dc<<3;

		if (!intra) {
			quant = mpCurrentNonintraQ;

			idx = -1;

			if (v >= 0x80000000) {
				int level;

				if (bPrescaled)
					level = ((3 * quant[0] + 128)>>12);
				else
					level = ((((3 * quant[0])>>4)-1)|1);

				if (v & 0x40000000)
					level = -level;

				dct_coeff[0] = level;

				idx = 0;
				bitheap_skipbits(2);
				v = bitheap_peekbits(32);
			}

			level_addend = 1;
		}

		for(;;) {
			int level;
			int level_sign;
			int run;

	/*		{
				printf("%08lx | ", v);
				for(int i=11; i>=0; --i)
					putchar('0' + ((v>>i)&1));

			}*/

#if 1
			if (v >= 0x80000000) {		// 1...
				if (v < 0xc0000000) {
					bitheap_skipbitsconst(2);
	//				printf("EOB\n");
					break;
				}

				level = 1;
				run = 0;
				level_sign = 6 - (v>>29);
				bitheap_skipbitsconst(3);
			} else if (v >= 0x28000000) {		// 0010 10xx xxxx
				v >>= 26;

				run = dct_vshort_tab[v-10][0];
				level = dct_vshort_tab[v-10][1];
				bitheap_skipbits8(dct_vshort_tab[v-10][2]);
				level_sign = dct_vshort_tab[v-10][3];
			} else if (v >= 0x08000000) {		// 000 0100 00xx
				v >>= 23;

				run = dct_short_tab[v-16][0];
				level = dct_short_tab[v-16][1];
				bitheap_skipbits8(dct_short_tab[v-16][2]);
				level_sign = dct_short_tab[v-16][3];
			} else if (v >= 0x04000000) {	// 000001 (escape)
#else

			if ((v&0xc0000000) == 0x80000000) {
				bitheap_skipbits(2);
				break;
			}

			if (v >= 0x08000000) {
				typedef const signed char *tpTableEntry;

				const unsigned long ptr1 = (unsigned long)dct_vshort_tab[(v>>26)-10];
				const unsigned long ptr2 = (unsigned long)dct_short_tab[(v>>23)-16];

				signed long switch1 = (signed long)((v>>1) - 0x14000000) >> 31;

				//	range				switch1		switch2
				//	80000000-FFFFFFFF	true		?
				//	28000000-7FFFFFFF	false		false
				//	08000000-27FFFFFF	false		true

				tpTableEntry ptr = (tpTableEntry)((ptr1 & ~switch1) + (ptr2 & switch1));
				
				run = ptr[0];
				level = ptr[1];
				bitheap_skipbits8(ptr[2]);
				level_sign = ptr[3];
			} else if (v >= 0x04000000) {	// 000001 (escape)			

#endif
				run = (v>>20) - 64;

				bitheap_skipbits(12);

				level = bitheap_getbitssignedconst(8);

				if (!(level & 0x7f))
					level = level*2 + bitheap_getbitsconst(8);

				level_sign = 0;

				if (level<0) {
					level_sign = -1;
					level = -level;
				}
			} else {					// 000000...

				// The longest code is 16 bits, but given the six zeroes we already
				// know about, that still leaves a 1K table.  We split the remaining
				// codespace into a 7-bit long table and a 7-bit very-long table.

				bitheap_skipbits(6);

#if 0
				v = bitheap_peekbits(10);

				if (v >= 0x080) {
					v >>= 3;

					run = dct_long_tab[v-16][0];
					level = dct_long_tab[v-16][1];
					bitheap_skipbits(dct_long_tab[v-16][2]);
				} else {
					// codes below 000000 0000010000 are illegal

					if (v < 16) {
						SetError(kError | kErrorBadValue);
						break;
					}

	//				printf("%d\n", v);

					run = dct_vlong_tab[v-16][0];
					level = dct_vlong_tab[v-16][1];
					bitheap_skipbits(dct_vlong_tab[v-16][2]);
				}
#else
				typedef const signed char *tpTableEntry;

				v = bitheap_peekbits(10);

				if (v < 16) {
					SetError(kError | kErrorBadValue);
					break;
				}

				const uintptr ptr1 = (uintptr)dct_long_tab[(v>>3)-16];
				const uintptr ptr2 = (uintptr)dct_vlong_tab[v-16];

				sintptr switch1 = (sintptr)(sint32)(v - 0x080) >> (sizeof(void *) * 8 - 1);

				//	range				switch1		switch2
				//	80000000-FFFFFFFF	true		?
				//	28000000-7FFFFFFF	false		false
				//	08000000-27FFFFFF	false		true

				tpTableEntry ptr = (tpTableEntry)((ptr1 & ~switch1) + (ptr2 & switch1));
				
				run = ptr[0];
				level = ptr[1];
				bitheap_skipbits(ptr[2]);
#endif

				level_sign = bitheap_getbitssignedconst(1);
			}

			VDASSERT(!((level_sign+1)&~1));
			VDASSERT(level>0 && level<=256);

	//		printf("  run(%2d), level(%2d) | ", run, level);

			idx += run+1;

			// forbidden stuff

			if (idx >= 64 || !level) {
				idx = 63;
				SetError(kError | kErrorTooManyCoefficients);
				break;
			}

			// decode coefficient and oddify toward zero

			const int pos = mpZigzagOrder[idx];

	//		printf("%2d : %2d -> %2d\n", pos, (pos&56) + (pos&3)*2 + ((pos&4)>>2), level);

			if (bPrescaled)
				level = ((2*level + level_addend) * quant[idx] + 128) >> 12;
			else
				level = ((((2*level + level_addend) * quant[idx]) >> 4) - 1) | 1;

			// apply sign and store coefficient

			dct_coeff[pos] = (level ^ level_sign) - level_sign;

			// prepare for next round

			v = bitheap_peekbits(32);
		}

		// DCT coefficients are decoded -- now apply IDCT.

#if 0
		static int count[4]={0};
		static int total = 0;
		++count[(idx>0) + (idx>9) + (idx > 2)];
		if (!(++total & 65535)) {
			VDDEBUG2("MPEG: %d DC, %d <3, %d <10, %d full\n", count[0], count[1], count[2], count[3]);
			memset(count, 0, sizeof count);
		}
#endif

		if (intra)
			mpIDCTs->pIntra(dst, pitch, dct_coeff, idx);
		else
			mpIDCTs->pNonintra(dst, pitch, dct_coeff, idx);
	}
};

///////////////////////////////////////////////////////////////////////////

VDMPEGDecoder::VDMPEGDecoder()
	: mRefCount(0)
	, mpBuffers(NULL)
	, mbQuantizersDirty(true)
{
}

VDMPEGDecoder::~VDMPEGDecoder() {
	Shutdown();
}

IVDMPEGDecoder *CreateVDMPEGDecoder() {
	return new VDMPEGDecoder();
}

///////////////////////////////////////////////////////////////////////////
//
//	control
//
///////////////////////////////////////////////////////////////////////////

bool VDMPEGDecoder::Init(int width, int height) {
	int i;

	Shutdown();

	mnBlockW	= (width+15) >> 4;
	mnBlockH	= (height+15) >> 4;
	mnHeight	= height;
	mnYPitch	= (mnBlockW * 24 + 15) & ~15;		// all scanlines must be aligned to 16 for SSE/SSE2
	mnYPitch8	= mnYPitch * 8;
	mnCPitch	= mnYPitch * 2;
	mnCPitch8	= mnCPitch * 8;

	// Attempt to allocate all buffers.

	mnBuffers = 3;

	if (!(mpBuffers = new MPEGBuffer[mnBuffers])) {
		Shutdown();
		return false;
	}

	for(i=0; i<mnBuffers; ++i) {
		mpBuffers[i].pYBuffer = mpBuffers[i].pCBuffer = NULL;
		mpBuffers[i].frame = -1;
	}

	for(i=0; i<mnBuffers; ++i) {
		if (!(mpBuffers[i].pYBuffer = new_nothrow YCCSample[mnYPitch * mnBlockH * 16 + 127])) {
			Shutdown();
			return false;
		}

		// Align Y to a 128-byte boundary (P4 L2 cache line) and set up Cb/Cr
		mpBuffers[i].pY  = (YCCSample *)(((uintptr)mpBuffers[i].pYBuffer + 127) & ~127);
		mpBuffers[i].pCr = mpBuffers[i].pY + mnBlockW * 16;
		mpBuffers[i].pCb = mpBuffers[i].pCr + mnYPitch;
	}

	// All good to go.

	return true;
}

void VDMPEGDecoder::Shutdown() {
	int i;

	if (mpBuffers) {
		for(i=0; i<mnBuffers; ++i)
			delete[] mpBuffers[i].pYBuffer;

		delete[] mpBuffers;
		mpBuffers = NULL;
	}
}

void VDMPEGDecoder::SetIntraQuantizers(const unsigned char *pMatrix) {
	static const unsigned char intramatrix_default[64]={
		 8, 16, 19, 22, 26, 27, 29, 34,
		16, 16, 22, 24, 27, 29, 34, 37,
		19, 22, 26, 27, 29, 34, 34, 38,
		22, 22, 26, 27, 29, 34, 37, 40,
		22, 26, 27, 29, 32, 35, 40, 48,
		26, 27, 29, 32, 35, 40, 48, 58, 
		26, 27, 29, 34, 38, 46, 56, 69,
		27, 29, 35, 38, 46, 56, 69, 83
	};

	// The matrix comes in stored in zigzag order.

	if (pMatrix) {
		for(int i=0; i<64; ++i)
			mUnscaledIntraQ[i] = pMatrix[i];
	} else {
		for(int i=0; i<64; ++i)
			mUnscaledIntraQ[i] = intramatrix_default[zigzag_std[i]];
	}

	mbQuantizersDirty = true;
}

void VDMPEGDecoder::SetNonintraQuantizers(const unsigned char *pMatrix) {
	if (pMatrix) {
		for(int i=0; i<64; ++i)
			mUnscaledNonintraQ[i] = pMatrix[i];
	} else {
		for(int i=0; i<64; ++i)
			mUnscaledNonintraQ[i] = 16;
	}

	mbQuantizersDirty = true;
}

void VDMPEGDecoder::SetPredictors(const VDMPEGPredictorSet *pPredictors) {
	mpPredictors = pPredictors;
	mbQuantizersDirty = true;
}

void VDMPEGDecoder::SetConverters(const VDMPEGConverterSet *pConverters) {
	mpConverters = pConverters;
}

void VDMPEGDecoder::SetIDCTs(const VDMPEGIDCTSet *pIDCTs) {
#if 0
	VDIDCTComplianceResult result;
	VDASSERT(VDTestVideoIDCTCompliance(*pIDCTs, result));
#endif
	mpIDCTs = pIDCTs;
	mpZigzagOrder = pIDCTs->pAltScan ? pIDCTs->pAltScan : zigzag_std;
	mpBlockDecoder = pIDCTs->pPrescaler ? &VDMPEGDecoder::DecodeBlockPrescaled : &VDMPEGDecoder::DecodeBlockNonPrescaled;
	mbQuantizersDirty = true;
}

void VDMPEGDecoder::UpdateQuantizers() {
	int i, j;

	if (mpIDCTs->pPrescaler)
		for(i=0; i<64; ++i)
			mIntraQ[1][i] = mUnscaledIntraQ[i] * mpIDCTs->pPrescaler[mpZigzagOrder[i]];
	else
		for(i=0; i<64; ++i)
			mIntraQ[1][i] = mUnscaledIntraQ[i];

	for(j=0; j<32; ++j) {
		// Do not scale the DC coefficient -- it is always 8 in all cases.

		mIntraQ[j][0] = mIntraQ[1][0];

		for(i=1; i<64; ++i)
			mIntraQ[j][i] = mIntraQ[1][i]*j;
	}

	if (mpIDCTs->pPrescaler)
		for(i=0; i<64; ++i)
			mNonintraQ[1][i] = mUnscaledNonintraQ[i] * mpIDCTs->pPrescaler[mpZigzagOrder[i]];
	else
		for(i=0; i<64; ++i)
			mNonintraQ[1][i] = mUnscaledNonintraQ[i];

	for(j=0; j<32; ++j)
		for(i=0; i<64; ++i)
			mNonintraQ[j][i] = mNonintraQ[1][i]*j;
}

///////////////////////////////////////////////////////////////////////////
//
//	decoding
//
///////////////////////////////////////////////////////////////////////////

long VDMPEGDecoder::GetErrorState() {
	long rv = mErrorState;

	mErrorState = 0;

	return rv;
}

///////////////////////////////////////////////////////////////////////////
//
//	framebuffer access
//
///////////////////////////////////////////////////////////////////////////

int VDMPEGDecoder::GetFrameBuffer(long frame) {
	int i;

	for(i=0; i<mnBuffers; ++i)
		if (mpBuffers[i].frame == frame)
			return i;

	return -1;
}

long VDMPEGDecoder::GetFrameNumber(int buffer) {
	if (buffer<0 || buffer >= mnBuffers)
		return -1;

	return mpBuffers[buffer].frame;
}

void VDMPEGDecoder::CopyFrameBuffer(int dst, int src, long frameno) {
	if ((unsigned)src >= (unsigned)mnBuffers || (unsigned)dst >= (unsigned)mnBuffers)
		return;

	VDMemcpyRect(mpBuffers[dst].pY, mnYPitch, mpBuffers[src].pY, mnYPitch, mnBlockW * 16, mnBlockH * 16);
	VDMemcpyRect(mpBuffers[dst].pCr, mnCPitch, mpBuffers[src].pCr, mnCPitch, mnBlockW * 8, mnBlockH * 8);
	VDMemcpyRect(mpBuffers[dst].pCb, mnCPitch, mpBuffers[src].pCb, mnCPitch, mnBlockW * 8, mnBlockH * 8);

	mpBuffers[dst].frame = frameno;
}

void VDMPEGDecoder::SwapFrameBuffers(int dst, int src) {
	if ((src|dst)<0 || src >= mnBuffers)
		return;

	MPEGBuffer t;

	t = mpBuffers[dst];
	mpBuffers[dst] = mpBuffers[src];
	mpBuffers[src] = t;
}

void VDMPEGDecoder::ClearFrameBuffers() {
	for(int i=0; i<mnBuffers; ++i) {
		mpBuffers[i].frame = -1;
	}
}

const void *VDMPEGDecoder::GetYBuffer(int buffer, ptrdiff_t& pitch) {
	if (buffer < 0 || buffer >= mnBuffers)
		return NULL;

	pitch = mnYPitch;

	return mpBuffers[buffer].pY;
}

const void *VDMPEGDecoder::GetCrBuffer(int buffer, ptrdiff_t& pitch) {
	if (buffer < 0 || buffer >= mnBuffers)
		return NULL;

	pitch = mnCPitch;

	return mpBuffers[buffer].pCr;
}

const void *VDMPEGDecoder::GetCbBuffer(int buffer, ptrdiff_t& pitch) {
	if (buffer < 0 || buffer >= mnBuffers)
		return NULL;

	pitch = mnCPitch;

	return mpBuffers[buffer].pCb;
}

///////////////////////////////////////////////////////////////////////////
//
//	colorspace conversion
//
///////////////////////////////////////////////////////////////////////////

bool VDMPEGDecoder::DecodeUYVY(void *dst, ptrdiff_t pitch, int buffer) {
	return Decode(dst, pitch, buffer, mpConverters->DecodeUYVY);
}

bool VDMPEGDecoder::DecodeYUYV(void *dst, ptrdiff_t pitch, int buffer) {
	return Decode(dst, pitch, buffer, mpConverters->DecodeYUYV);
}

bool VDMPEGDecoder::DecodeYVYU(void *dst, ptrdiff_t pitch, int buffer) {
	return Decode(dst, pitch, buffer, mpConverters->DecodeYVYU);
}

bool VDMPEGDecoder::DecodeY41P(void *dst, ptrdiff_t pitch, int buffer) {
	return Decode(dst, pitch, buffer, mpConverters->DecodeY41P);
}

bool VDMPEGDecoder::DecodeRGB15(void *dst, ptrdiff_t pitch, int buffer) {
	return Decode(dst, pitch, buffer, mpConverters->DecodeRGB15);
}

bool VDMPEGDecoder::DecodeRGB16(void *dst, ptrdiff_t pitch, int buffer) {
	return Decode(dst, pitch, buffer, mpConverters->DecodeRGB16);
}

bool VDMPEGDecoder::DecodeRGB24(void *dst, ptrdiff_t pitch, int buffer) {
	return Decode(dst, pitch, buffer, mpConverters->DecodeRGB24);
}

bool VDMPEGDecoder::DecodeRGB32(void *dst, ptrdiff_t pitch, int buffer) {
	return Decode(dst, pitch, buffer, mpConverters->DecodeRGB32);
}

///////////////////////////////////////////////////////////////////////////
//
//	decoding (yeeeaaah!!!!)
//
///////////////////////////////////////////////////////////////////////////

// Decoding should start at the first byte after the picture start code.
//
// +-------------------------------+
// |     temporal reference 2-9    | buf[0]
// +-------+-----------+-----------+
// | tr0-1 |   type    |delay 13-15| buf[1]
// +-------------------+-----------+
// |         VBV delay 5-12        | buf[2]
// +-------------------+---+-------+
// |     VBV delay 0-4 |FPF|FFC 1-2| buf[3] (full_pel_forward_vector, forward_f_code)
// +---+---+-----------+---+-------+
// |FFC|FPB|back f code|xxxxxxxxxxx| buf[4] (full_pel_backward_vector)
// +---+---+-----------+-----------+

int VDMPEGDecoder::DecodeFrame(const void *_src, long len, long frame, int dst, int fwd, int rev) {
	const unsigned char *src = (const unsigned char *)_src;
	const unsigned char *limit = src + len - 4;
	int type;

	if (mbQuantizersDirty) {
		UpdateQuantizers();
		mbQuantizersDirty = false;
	}

	// Extract type and check.

	type = (src[1]&0x38)>>3;

	switch(type) {
	case 1:		// I-frame
		mpSliceDecoder = &VDMPEGDecoder::DecodeSlice_I;
		break;
	case 2:		// P-frame
		mpSliceDecoder = &VDMPEGDecoder::DecodeSlice_P;
		break;
	case 3:		// B-frame
		mpSliceDecoder = &VDMPEGDecoder::DecodeSlice_B;

		// extract backward prediction info

		mbBackwardFullPel = (src[4]>>6)&1;
		mnBackwardRSize = ((src[4]>>3)&7)-1;
		mnBackwardMask = (32<<mnBackwardRSize)-1;
		mnBackwardSignExtend = ~((16<<mnBackwardRSize)-1);

		break;
	default:
		SetError(kError | kErrorBadFrameType);
		return -1;
	}

	// extract forward prediction info

	if ((type&0xfe)==2) {
		mbForwardFullPel = (src[3]>>2)&1;
		mnForwardRSize = (((src[3]&3)<<1) + (src[4]>>7))-1;
		mnForwardMask = (32<<mnForwardRSize)-1;
		mnForwardSignExtend = ~((16<<mnForwardRSize)-1);
	}

	mpY = mpBuffers[dst].pY;
	mpCr = mpBuffers[dst].pCr;
	mpCb = mpBuffers[dst].pCb;

	// Hook up referencing frames.

	if (fwd>=0) {
		mpFwdY	= mpBuffers[fwd].pY;
		mpFwdCr = mpBuffers[fwd].pCr;
		mpFwdCb = mpBuffers[fwd].pCb;
	} else {
		mpFwdY = mpFwdCr = mpFwdCb = NULL;
	}
	
	if (rev>=0) {
		mpBackY	= mpBuffers[rev].pY;
		mpBackCr = mpBuffers[rev].pCr;
		mpBackCb = mpBuffers[rev].pCb;
	} else {
		mpBackY = mpBackCr = mpBackCb = NULL;
	}

	// Wipe destination frame.

#ifdef _DEBUG
	for(uint32 y1=0; y1<mnBlockH*16; ++y1)
		memset(mpY + mnYPitch*y1, 0x80, mnBlockW*16);
	for(uint32 y2=0; y2<mnBlockH*8; ++y2)
		memset(mpCr + mnCPitch*y2, 0x80, mnBlockW*8);
	for(uint32 y3=0; y3<mnBlockH*8; ++y3)
		memset(mpCb + mnCPitch*y3, 0x80, mnBlockW*8);
#endif

	// Search for start codes beginning at buf+4 (I) or buf+5 (P/B).

	if (type == 1)
		src += 4;
	else
		src += 5;

	long error = mErrorState;

#ifdef _WIN32
	__try {
#endif
		while(src < limit) {
			if (*src) {
				while(src<limit && *src)
					++src;
				continue;
			} else if (!src[1] && src[2]==1 && src[3]>0 && src[3]<0xb0) {
				src += 3;

				bitheap_reset(src+1);

				mnQuantValue = bitheap_getbitsconst(5);

				mpCurrentIntraQ = mIntraQ[mnQuantValue];
				mpCurrentNonintraQ = mNonintraQ[mnQuantValue];

				while(bitheap_getflag())
					bitheap_skipbitsconst(8);

				mErrorState = 0;

				(this->*mpSliceDecoder)(src[0]);

				// Attempt slice resynchronization if an error occurred.

				if (!mErrorState) {
					bitheap_skipbits(bitcnt & 7);

					src = bitsrc - ((24-bitcnt)>>3) - 2;
				}

				if (!error && mErrorState)
					error = mErrorState;

			}

			++src;
		}
#ifdef _WIN32
	} __except(_exception_code() == EXCEPTION_ACCESS_VIOLATION) {
		error |= kError | kErrorSourceOverrun;
	}
#endif

	mErrorState = error;

	// set frame

	mpBuffers[dst].frame = frame;

	return dst;
}

///////////////////////////////////////////////////////////////////////////
//
//	Macroblock decoder
//
///////////////////////////////////////////////////////////////////////////

void VDMPEGDecoder::DecodeBlockPrescaled(YCCSample *dst, long pitch, bool intra, int dc) {
	DecodeBlock(dst, pitch, intra, dc, true, (int)0);
}

void VDMPEGDecoder::DecodeBlockNonPrescaled(YCCSample *dst, long pitch, bool intra, int dc) {
	DecodeBlock(dst, pitch, intra, dc, false, (short)0);
}

void VDMPEGDecoder::DecodeBlock_Y(YCCSample *dst, bool intra) {

	// decode DCT size

	if (intra) {
		int dc_coeff = 0;
		unsigned int v = bitheap_peekbits(3);
		unsigned int size;

//		printf("%08lx\n", bitheap_peekbits(32));

		if (v < 7) {
			static const unsigned char dc_short_table[][2]={
				{ 1, 2 }, { 1, 2 },		// 00  -> 1
				{ 2, 2 }, { 2, 2 },		// 01  -> 2
				{ 0, 3 },				// 100 -> 0
				{ 3, 3 },				// 101 -> 3
				{ 4, 3 },				// 110 -> 4
			};

			size = dc_short_table[v][0];
			bitheap_skipbits(dc_short_table[v][1]);
		} else {
			bitheap_skipbits(3);

			size = 5;

			while(bitheap_getflag())
				++size;
		}

//		printf("DC size: %d\n", size);

		// read DC coefficient delta

		if (size) {
			long extend = -1L<<size;

			dc_coeff = bitheap_getbitssignedconst(size);
			dc_coeff ^= extend;
			dc_coeff -= (dc_coeff>>31);

			mnLastY_DC += dc_coeff;

			// dc must be in [0,255]

			if (mnLastY_DC & ~0xff) {
				SetError(kError | kErrorBadValue);

				if (mnLastY_DC < 0)
					mnLastY_DC = 0;
				else
					mnLastY_DC = 255;
			}
		}

		(this->*mpBlockDecoder)(dst, mnYPitch, true, mnLastY_DC);
	} else {
		(this->*mpBlockDecoder)(dst, mnYPitch, false, 0);
	}
}

void VDMPEGDecoder::DecodeBlock_C(YCCSample *dst, int& dc, bool intra) {

	// decode DCT size

	if (intra) {
		unsigned int v = bitheap_getbitsconst(2);
		unsigned int size;
		int dc_coeff = 0;

		if (v < 3) {
			size = v;
		} else {
			size = 3;

			while(bitheap_getflag())
				++size;
		}

		// read DC coefficient delta

		if (size) {
			long extend = -1L<<size;

			dc_coeff = bitheap_getbitssignedconst(size);
			dc_coeff ^= extend;
			dc_coeff -= (dc_coeff>>31);

			dc += dc_coeff;

			// dc_coeff must be in [0,255]

			if (dc & ~0xff) {
				SetError(kError | kErrorBadValue);

				if (dc < 0)
					dc = 0;
				else
					dc = 255;
			}
		}

		(this->*mpBlockDecoder)(dst, mnCPitch, true, dc);
	} else {
		(this->*mpBlockDecoder)(dst, mnCPitch, false, 0);
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	I-frame decoder
//
///////////////////////////////////////////////////////////////////////////

static const unsigned char mb_short_lookup[14][2]={
	{ 6, 4 },									// 000 10xx xxxx
	{ 5, 4 },									// 000 11xx xxxx
	{ 4, 3 }, { 4, 3 },							// 001 0xxx xxxx
	{ 3, 3 }, { 3, 3 },							// 001 1xxx xxxx
	{ 2, 2 }, { 2, 2 }, { 2, 2 }, { 2, 2 },		// 010 xxxx xxxx
	{ 1, 2 }, { 1, 2 }, { 1, 2 }, { 1, 2 },		// 011 xxxx xxxx
};

void VDMPEGDecoder::DecodeSlice_I(int slice) {
	unsigned int pos_x = (unsigned int)-1;
	unsigned int pos_y = slice-1;

	mnLastY_DC = 128;
	mnLastCr_DC = 128;
	mnLastCb_DC = 128;

	do {
		int inc = 1;
		unsigned long v = bitheap_peekbits(11);

		// 000 0000 1111 is stuffing.

		while(v == 15)  {
			bitheap_skipbits(11);
			v = bitheap_peekbits(11);
		}

		// 000 0000 1000 is the escape.

		while(v == 8) {
			bitheap_skipbits(11);
			v = bitheap_peekbits(11);
			inc += 33;
		}

		// whatever is left must be the VLC for the macroblock increment
		//
		// 1xx xxxx xxxx -> 1

		bitheap_skipbits(1);

		if (v < 0x400) {
			if (v >= 0x80) {
				inc += mb_short_lookup[(v>>6)-2][0];
				bitheap_skipbits(mb_short_lookup[(v>>6)-2][1]);
			} else if (v >= 0x60) {			// 000 011? xxxx
				inc += 8 - ((v>>4)&1);
				bitheap_skipbits(6);
			} else if (v >= 0x30) {			// 000 0011 0000
				inc += 20 - (v>>3);
				bitheap_skipbits(7);
			} else if (v >= 0x24) {			// 000 0010 0100
				inc += 38 - (v>>1);
				bitheap_skipbits(9);
			} else {
				inc += 56 - v;
				bitheap_skipbits(10);
			}
		}

		// process macroblock skip

		pos_x += inc;

		if (pos_x >= mnBlockW) {
			pos_y += pos_x / mnBlockW;
			pos_x %= mnBlockW;
		}

		// no decoding out of bounds!!

		if (pos_y >= mnBlockH) {
			SetError(kError | kErrorTooManyMacroblocks);
			return;
		}

		// decode macroblock type:
		//
		//	1	keep quantizer
		//	01	change quantizer

		if (!bitheap_getflag()) {
			if (!bitheap_getflag()) {
				SetError(kError | kErrorBadValue);
			}

			mnQuantValue = bitheap_getbitsconst(5);

			mpCurrentIntraQ = mIntraQ[mnQuantValue];
		}

		DecodeBlock_Y(mpY + mnYPitch8 * (2*pos_y+0) + 16 * pos_x, true);
		DecodeBlock_Y(mpY + mnYPitch8 * (2*pos_y+0) + 16 * pos_x + 8, true);
		DecodeBlock_Y(mpY + mnYPitch8 * (2*pos_y+1) + 16 * pos_x, true);
		DecodeBlock_Y(mpY + mnYPitch8 * (2*pos_y+1) + 16 * pos_x + 8, true);
		DecodeBlock_C(mpCb + mnCPitch8*pos_y + 8*pos_x, mnLastCb_DC, true);
		DecodeBlock_C(mpCr + mnCPitch8*pos_y + 8*pos_x, mnLastCr_DC, true);

	} while(!bitheap_checkzerobits(23));
}

///////////////////////////////////////////////////////////////////////////
//
//	P/B-frame helpers
//
///////////////////////////////////////////////////////////////////////////

#define REPEAT1(v,b) {v,b}
#define REPEAT2(v,b) {v,b},{v,b}
#define REPEAT4(v,b) {v,b},{v,b},{v,b},{v,b}
#define REPEAT8(v,b) {v,b},{v,b},{v,b},{v,b},{v,b},{v,b},{v,b},{v,b}
#define REPEAT16(v,b) REPEAT8(v,b),REPEAT8(v,b)

int VDMPEGDecoder::DecodeMotionVector(int rsize) {
	if (bitheap_getflag())
		return 0;

	unsigned int v = bitheap_peekbits(9);

	static const unsigned char mv_decode_short[7]={
		3,			// 0001
		2,2,		// 001
		1,1,1,1,	// 01
	};

	static const unsigned char mv_decode_long[52][2]={
		REPEAT1(16, 9),		// 0000 0011 00
		REPEAT1(15, 9),		// 0000 0011 01
		REPEAT1(14, 9),		// 0000 0011 10
		REPEAT1(13, 9),		// 0000 0011 11
		REPEAT1(12, 9),		// 0000 0100 00
		REPEAT1(11, 9),		// 0000 0100 01
		REPEAT2(10, 8),		// 0000 0100 1
		REPEAT2( 9, 8),		// 0000 0101 0
		REPEAT2( 8, 8),		// 0000 0101 1
		REPEAT8( 7, 6),		// 0000 011
		REPEAT8( 6, 6),		// 0000 100
		REPEAT8( 5, 6),		// 0000 101
		REPEAT16( 4, 5),	// 0000 11
	};

	if (v >= 64) {
		v = mv_decode_short[(v>>6)-1];

		bitheap_skipbits(v);
	} else {
		if (v < 12) {
			SetError(kError | kErrorBadValue);
			return 0;
		}

		bitheap_skipbits(mv_decode_long[v-12][1]);
		v = mv_decode_long[v-12][0];
	}

	int inverter = bitheap_getbitssignedconst(1);

	if (rsize)
		v = ((v-1)<<rsize) + bitheap_getbits(rsize) + 1;

	return (v ^ inverter) - inverter;
}

int VDMPEGDecoder::DecodeCodedBlockPattern() {
	static const unsigned char cbp_short[52][2]={
		REPEAT1(63,6),		// 001100
		REPEAT1( 3,6),		// 001101
		REPEAT1(36,6),		// 001110
		REPEAT1(24,6),		// 001111
		REPEAT2(62,5),		// 01000
		REPEAT2( 2,5),		// 01001
		REPEAT2(61,5),		// 01010
		REPEAT2( 1,5),		// 01011
		REPEAT2(56,5),		// 01100
		REPEAT2(52,5),		// 01101
		REPEAT2(44,5),		// 01110
		REPEAT2(28,5),		// 01111
		REPEAT2(40,5),		// 10000
		REPEAT2(20,5),		// 10001
		REPEAT2(48,5),		// 10010
		REPEAT2(12,5),		// 10011
		REPEAT4(32,4),		// 1010
		REPEAT4(16,4),		// 1011
		REPEAT4( 8,4),		// 1100
		REPEAT4( 4,4),		// 1101
		REPEAT8(60,3),		// 111
	};

	static const unsigned char cbp_long[94][2]={
		REPEAT1(39,9),		// 0000 0001 0
		REPEAT1(27,9),		// 0000 0001 1
		REPEAT1(59,9),		// 0000 0010 0
		REPEAT1(55,9),		// 0000 0010 1
		REPEAT1(47,9),		// 0000 0011 0
		REPEAT1(31,9),		// 0000 0011 1
		REPEAT2(58,8),		// 0000 0100
		REPEAT2(54,8),		// 0000 0101
		REPEAT2(46,8),		// 0000 0110
		REPEAT2(30,8),		// 0000 0111
		REPEAT2(57,8),		// 0000 1000
		REPEAT2(53,8),		// 0000 1001
		REPEAT2(45,8),		// 0000 1010
		REPEAT2(29,8),		// 0000 1011
		REPEAT2(38,8),		// 0000 1100
		REPEAT2(26,8),		// 0000 1101
		REPEAT2(37,8),		// 0000 1110
		REPEAT2(25,8),		// 0000 1111
		REPEAT2(43,8),		// 0001 0000
		REPEAT2(23,8),		// 0001 0001
		REPEAT2(51,8),		// 0001 0010
		REPEAT2(15,8),		// 0001 0011
		REPEAT2(42,8),		// 0001 0100
		REPEAT2(22,8),		// 0001 0101
		REPEAT2(50,8),		// 0001 0110
		REPEAT2(14,8),		// 0001 0111
		REPEAT2(41,8),		// 0001 1000
		REPEAT2(21,8),		// 0001 1001
		REPEAT2(49,8),		// 0001 1010
		REPEAT2(13,8),		// 0001 1011
		REPEAT2(35,8),		// 0001 1100
		REPEAT2(19,8),		// 0001 1101
		REPEAT2(11,8),		// 0001 1110
		REPEAT2( 7,8),		// 0001 1111
		REPEAT4(34,7),		// 0010 000
		REPEAT4(18,7),		// 0010 001
		REPEAT4(10,7),		// 0010 010
		REPEAT4( 6,7),		// 0010 011
		REPEAT4(33,7),		// 0010 100
		REPEAT4(17,7),		// 0010 101
		REPEAT4( 9,7),		// 0010 110
		REPEAT4( 5,7),		// 0010 111
	};

	unsigned long v = bitheap_peekbits(9);

	if (v < 96) {
		if (v < 2) {
			SetError(kError | kErrorBadValue);
			return 0;
		}

		bitheap_skipbits(cbp_long[v-2][1]);
		return cbp_long[v-2][0];
	} else {
		v>>=3;

		bitheap_skipbits(cbp_short[v-12][1]);
		return cbp_short[v-12][0];
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	P-frame decoder
//
///////////////////////////////////////////////////////////////////////////

void VDMPEGDecoder::DecodeSlice_P(int slice) {
	unsigned int pos_x = (unsigned int)-1;
	unsigned int pos_y = slice-1;
	int forw_x = 0, forw_y = 0;

	mnLastY_DC = 128;
	mnLastCr_DC = 128;
	mnLastCb_DC = 128;

	do {
		int inc = 1;
		unsigned long v = bitheap_peekbits(11);

		// 000 0000 1111 is stuffing.

		while(v == 15)  {
			bitheap_skipbits(11);
			v = bitheap_peekbits(11);
		}

		// 000 0000 1000 is the escape.

		while(v == 8) {
			bitheap_skipbits(11);
			v = bitheap_peekbits(11);
			inc += 33;
		}

		// whatever is left must be the VLC for the macroblock increment
		//
		// 1xx xxxx xxxx -> 1

		bitheap_skipbits(1);

		if (v < 0x400) {
			if (v >= 0x80) {
				inc += mb_short_lookup[(v>>6)-2][0];
				bitheap_skipbits(mb_short_lookup[(v>>6)-2][1]);
			} else if (v >= 0x60) {			// 000 011? xxxx
				inc += 8 - ((v>>4)&1);
				bitheap_skipbits(6);
			} else if (v >= 0x30) {			// 000 0011 0000
				inc += 20 - (v>>3);
				bitheap_skipbits(7);
			} else if (v >= 0x24) {			// 000 0010 0100
				inc += 38 - (v>>1);
				bitheap_skipbits(9);
			} else {
				inc += 56 - v;
				bitheap_skipbits(10);
			}
		}

		// process macroblock skip

		if (pos_x == (unsigned int)-1) {
			pos_x += inc;

			if (pos_x >= mnBlockW) {
				pos_y += pos_x / mnBlockW;
				pos_x %= mnBlockW;
			}
		} else {
			if (inc > 1) {
				mnLastY_DC = 128;
				mnLastCr_DC = 128;
				mnLastCb_DC = 128;
				forw_x = forw_y = 0;

				while(--inc>0) {
					if (++pos_x >= mnBlockW) {
						pos_x = 0;
						if (++pos_y >= mnBlockH) {
							SetError(kError | kErrorTooManyMacroblocks);
							return;
						}
					}

					CopyPredictionForward(pos_x, pos_y, 0,0);
				}
			}

			if (++pos_x >= mnBlockW) {
				pos_x = 0;
				if (++pos_y >= mnBlockH) {
					SetError(kError | kErrorTooManyMacroblocks);
					return;
				}
			}

		}

		// no decoding out of bounds!!

		if (pos_y >= mnBlockH) {
			SetError(kError | kErrorTooManyMacroblocks);
			return;
		}

//		printf("Decoding (%d,%d): %08lx\n", pos_x, pos_y, bitheap_peekbits(32));

		// decode macroblock type

		enum {
			kMBF_NewQuant	= 8,
			kMBF_Forward	= 4,
			kMBF_Pattern	= 2,
			kMBF_Intra		= 1
		};

		static const unsigned char mb_type_P[32][2]={
			REPEAT1(0,0),
			REPEAT1(kMBF_NewQuant+kMBF_Intra, 5),
			REPEAT2(kMBF_NewQuant+kMBF_Pattern, 4),
			REPEAT2(kMBF_NewQuant+kMBF_Forward+kMBF_Pattern, 4),
			REPEAT2(kMBF_Intra, 4),
			REPEAT8(kMBF_Forward, 2),
			REPEAT16(kMBF_Pattern, 1),
		};

		int mb_flags;

		if (bitheap_getflag()) {
			mb_flags = kMBF_Forward + kMBF_Pattern;
		} else {
			v = bitheap_peekbits(5);

			mb_flags = mb_type_P[v][0];
			bitheap_skipbits(mb_type_P[v][1]);
		}

		// read in quantizer

		if (mb_flags & kMBF_NewQuant) {
			mnQuantValue = bitheap_getbitsconst(5);

			mpCurrentIntraQ = mIntraQ[mnQuantValue];
			mpCurrentNonintraQ = mNonintraQ[mnQuantValue];
		}

		// read in motion vector and predict

		if (mb_flags & kMBF_Forward) {
			int recon_x = DecodeMotionVector(mnForwardRSize);
			int recon_y = DecodeMotionVector(mnForwardRSize);

			forw_x += recon_x;
			forw_y += recon_y;

			forw_x = ((forw_x & mnForwardMask) + mnForwardSignExtend) ^ mnForwardSignExtend;
			forw_y = ((forw_y & mnForwardMask) + mnForwardSignExtend) ^ mnForwardSignExtend;

			CopyPredictionForward(pos_x, pos_y, forw_x, forw_y);
		} else {
			forw_x = forw_y = 0;

			if (!(mb_flags & kMBF_Intra))
				CopyPredictionForward(pos_x, pos_y, 0, 0);
		}

		// read in coded block pattern

		int cbp = (mb_flags & kMBF_Intra) ? 63 : 0;

		if (mb_flags & kMBF_Pattern) {
			cbp = DecodeCodedBlockPattern();
		}

		// decode blocks

		bool intraflag = (mb_flags & kMBF_Intra);

		if (!intraflag) {
			mnLastY_DC = 128;
			mnLastCr_DC = 128;
			mnLastCb_DC = 128;
		}

		if (cbp&32) DecodeBlock_Y(mpY + mnYPitch8 * (2*pos_y+0) + 16 * pos_x, intraflag);
		if (cbp&16) DecodeBlock_Y(mpY + mnYPitch8 * (2*pos_y+0) + 16 * pos_x + 8, intraflag);
		if (cbp& 8) DecodeBlock_Y(mpY + mnYPitch8 * (2*pos_y+1) + 16 * pos_x, intraflag);
		if (cbp& 4) DecodeBlock_Y(mpY + mnYPitch8 * (2*pos_y+1) + 16 * pos_x + 8, intraflag);
		if (cbp& 2) DecodeBlock_C(mpCb + mnCPitch8*pos_y + 8*pos_x, mnLastCb_DC, intraflag);
		if (cbp& 1) DecodeBlock_C(mpCr + mnCPitch8*pos_y + 8*pos_x, mnLastCr_DC, intraflag);

	} while(!bitheap_checkzerobits(23));
}

///////////////////////////////////////////////////////////////////////////
//
//	B-frame decoder
//
///////////////////////////////////////////////////////////////////////////

void VDMPEGDecoder::DecodeSlice_B(int slice) {
	unsigned int pos_x = (unsigned int)-1;
	unsigned int pos_y = slice-1;
	int forw_x = 0, forw_y = 0;
	int back_x = 0, back_y = 0;
	int mb_flags = 0;

	mnLastY_DC = 128;
	mnLastCr_DC = 128;
	mnLastCb_DC = 128;

	do {
		int inc = 1;
		unsigned long v = bitheap_peekbits(11);

		// 000 0000 1111 is stuffing.

		while(v == 15)  {
			bitheap_skipbits(11);
			v = bitheap_peekbits(11);
		}

		// 000 0000 1000 is the escape.

		while(v == 8) {
			bitheap_skipbits(11);
			v = bitheap_peekbits(11);
			inc += 33;
		}

		// whatever is left must be the VLC for the macroblock increment
		//
		// 1xx xxxx xxxx -> 1

		bitheap_skipbits(1);

		if (v < 0x400) {
			if (v >= 0x80) {
				inc += mb_short_lookup[(v>>6)-2][0];
				bitheap_skipbits(mb_short_lookup[(v>>6)-2][1]);
			} else if (v >= 0x60) {			// 000 011? xxxx
				inc += 8 - ((v>>4)&1);
				bitheap_skipbits(6);
			} else if (v >= 0x30) {			// 000 0011 0000
				inc += 20 - (v>>3);
				bitheap_skipbits(7);
			} else if (v >= 0x24) {			// 000 0010 0100
				inc += 38 - (v>>1);
				bitheap_skipbits(9);
			} else {
				inc += 56 - v;
				bitheap_skipbits(10);
			}
		}

		// process macroblock skip

		if (pos_x == (unsigned int)-1) {
			pos_x += inc;

			if (pos_x >= mnBlockW) {
				pos_y += pos_x / mnBlockW;
				pos_x %= mnBlockW;
			}
		} else {
			if (inc > 1) {
				mnLastY_DC = 128;
				mnLastCr_DC = 128;
				mnLastCb_DC = 128;

				// We will have neither the forward nor backward flags set whenever
				// we encounter a first-block skip -- in this case we shouldn't draw
				// anything.

				if (!(mb_flags & (kMBF_Forward | kMBF_Backward))) {
					// Non-initial skip after intra block is illegal.  In this case,
					// set the error flag, then reset motion vectors to a relatively
					// safe configuration.

					SetError(kError);
					mb_flags = kMBF_Forward | kMBF_Backward;
					forw_x = forw_y = back_x = back_y = 0;
				}

				while(--inc>0) {
					if (++pos_x >= mnBlockW) {
						pos_x = 0;
						if (++pos_y >= mnBlockH) {
							SetError(kError | kErrorTooManyMacroblocks);
							return;
						}
					}

					if (mb_flags & kMBF_Forward) {
						CopyPredictionForward(pos_x, pos_y, forw_x, forw_y);

						if (mb_flags & kMBF_Backward)
							AddPredictionBackward(pos_x, pos_y, back_x, back_y);
					} else
						CopyPredictionBackward(pos_x, pos_y, back_x, back_y);
				}
			}

			if (++pos_x >= mnBlockW) {
				pos_x = 0;
				if (++pos_y >= mnBlockH) {
					SetError(kError | kErrorTooManyMacroblocks);
					return;
				}
			}

		}

		// no decoding out of bounds!!

		if (pos_y >= mnBlockH) {
			SetError(kError | kErrorTooManyMacroblocks);
			return;
		}

//		printf("Decoding (%d,%d): %08lx\n", pos_x, pos_y, bitheap_peekbits(32));

		// decode macroblock type

		static const unsigned char mb_type_B[64][2]={
			REPEAT1(0,0),												// umm...
			REPEAT1(kMBF_NewQuant + kMBF_Intra, 6),						// 000001
			REPEAT1(kMBF_NewQuant + kMBF_Backward + kMBF_Pattern, 6),	// 000010
			REPEAT1(kMBF_NewQuant + kMBF_Forward + kMBF_Pattern, 6),	// 000011
			REPEAT2(kMBF_NewQuant + kMBF_Both + kMBF_Pattern, 5),		// 00010
			REPEAT2(kMBF_Intra, 5),										// 00011
			REPEAT4(kMBF_Forward, 4),									// 0010
			REPEAT4(kMBF_Forward + kMBF_Pattern, 4),					// 0011
			REPEAT8(kMBF_Backward, 3),									// 010
			REPEAT8(kMBF_Backward + kMBF_Pattern, 3),					// 011
			REPEAT16(kMBF_Forward + kMBF_Backward, 2),					// 10
			REPEAT16(kMBF_Forward + kMBF_Backward + kMBF_Pattern, 2),	// 11
		};

		v = bitheap_peekbits(6);

		mb_flags = mb_type_B[v][0];
		bitheap_skipbits(mb_type_B[v][1]);

		// read in quantizer

		if (mb_flags & kMBF_NewQuant) {
			mnQuantValue = bitheap_getbitsconst(5);

			mpCurrentIntraQ = mIntraQ[mnQuantValue];
			mpCurrentNonintraQ = mNonintraQ[mnQuantValue];
		}

		// read in motion vector and predict

		if (mb_flags & kMBF_Forward) {
			int recon_x = DecodeMotionVector(mnForwardRSize);
			int recon_y = DecodeMotionVector(mnForwardRSize);

			forw_x += recon_x;
			forw_y += recon_y;

			forw_x = ((forw_x & mnForwardMask) + mnForwardSignExtend) ^ mnForwardSignExtend;
			forw_y = ((forw_y & mnForwardMask) + mnForwardSignExtend) ^ mnForwardSignExtend;

//			printf("at(%2d,%2d) forward vector %+3d,%+3d\n", pos_x, pos_y, forw_x, forw_y);

			CopyPredictionForward(pos_x, pos_y, forw_x, forw_y);
		}

		if (mb_flags & kMBF_Backward) {
			int recon_x = DecodeMotionVector(mnBackwardRSize);
			int recon_y = DecodeMotionVector(mnBackwardRSize);

			back_x += recon_x;
			back_y += recon_y;

			back_x = ((back_x & mnBackwardMask) + mnBackwardSignExtend) ^ mnBackwardSignExtend;
			back_y = ((back_y & mnBackwardMask) + mnBackwardSignExtend) ^ mnBackwardSignExtend;

//			printf("at(%2d,%2d) backward vector %+3d,%+3d\n", pos_x, pos_y, back_x, back_y);

			if (mb_flags & kMBF_Forward)
				AddPredictionBackward(pos_x, pos_y, back_x, back_y);
			else
				CopyPredictionBackward(pos_x, pos_y, back_x, back_y);
		}

		// read in coded block pattern

		int cbp = 0;

		if (mb_flags & kMBF_Intra) {
			cbp = 63;
			forw_x = forw_y = back_x = back_y = 0;
		} else {
			mnLastY_DC = 128;
			mnLastCr_DC = 128;
			mnLastCb_DC = 128;
		}

		if (mb_flags & kMBF_Pattern) {
			cbp = DecodeCodedBlockPattern();
		}

		// decode blocks

		bool intraflag = (mb_flags & kMBF_Intra);

		if (cbp&32) DecodeBlock_Y(mpY + mnYPitch8 * (2*pos_y+0) + 16 * pos_x, intraflag);
		if (cbp&16) DecodeBlock_Y(mpY + mnYPitch8 * (2*pos_y+0) + 16 * pos_x + 8, intraflag);
		if (cbp& 8) DecodeBlock_Y(mpY + mnYPitch8 * (2*pos_y+1) + 16 * pos_x, intraflag);
		if (cbp& 4) DecodeBlock_Y(mpY + mnYPitch8 * (2*pos_y+1) + 16 * pos_x + 8, intraflag);
		if (cbp& 2) DecodeBlock_C(mpCb + mnCPitch8*pos_y + 8*pos_x, mnLastCb_DC, intraflag);
		if (cbp& 1) DecodeBlock_C(mpCr + mnCPitch8*pos_y + 8*pos_x, mnLastCr_DC, intraflag);

	} while(!bitheap_checkzerobits(23));
}

///////////////////////////////////////////////////////////////////////////
//
//	prediction
//
///////////////////////////////////////////////////////////////////////////

void VDMPEGDecoder::CopyPredictionForward(int posx, int posy, int dx, int dy) {
	if (mbForwardFullPel) {
		dx *= 2;
		dy *= 2;
	}

	int dxY		= posx*32 + dx;
	int dyY		= posy*32 + dy;
	int dxC		= posx*16 + dx/2;
	int dyC		= posy*16 + dy/2;

	// check vectors

	if ((unsigned)dxY > (unsigned)(mnBlockW*32-32) || (unsigned)dyY > (unsigned)(mnBlockH*32-32)) {
		dxY = posx*32;
		dyY = posy*32;
		dxC = posx*16;
		dyC = posy*16;
		SetError(kError | kErrorBadMotionVector);
	}

	(mpPredictors->Y_predictors[dyY&1][dxY&1])(mpY + posy*mnYPitch8*2 + posx*16, mpFwdY + mnYPitch*(dyY>>1) + (dxY>>1), mnYPitch);
	(mpPredictors->C_predictors[dyC&1][dxC&1])(mpCr + posy*mnCPitch8 + posx*8, mpFwdCr + mnCPitch*(dyC>>1) + (dxC>>1), mnCPitch);
	(mpPredictors->C_predictors[dyC&1][dxC&1])(mpCb + posy*mnCPitch8 + posx*8, mpFwdCb + mnCPitch*(dyC>>1) + (dxC>>1), mnCPitch);

//	__asm emms
}

void VDMPEGDecoder::CopyPredictionBackward(int posx, int posy, int dx, int dy) {
	if (mbBackwardFullPel) {
		dx *= 2;
		dy *= 2;
	}

	int dxY		= posx*32 + dx;
	int dyY		= posy*32 + dy;
	int dxC		= posx*16 + dx/2;
	int dyC		= posy*16 + dy/2;

	// check vectors

	if ((unsigned)dxY > (unsigned)(mnBlockW*32-32) || (unsigned)dyY > (unsigned)(mnBlockH*32-32)) {
		dxY = posx*32;
		dyY = posy*32;
		dxC = posx*16;
		dyC = posy*16;
		SetError(kError | kErrorBadMotionVector);
	}

	(mpPredictors->Y_predictors[dyY&1][dxY&1])(mpY + posy*mnYPitch8*2 + posx*16, mpBackY + mnYPitch*(dyY>>1) + (dxY>>1), mnYPitch);
	(mpPredictors->C_predictors[dyC&1][dxC&1])(mpCr + posy*mnCPitch8 + posx*8, mpBackCr + mnCPitch*(dyC>>1) + (dxC>>1), mnCPitch);
	(mpPredictors->C_predictors[dyC&1][dxC&1])(mpCb + posy*mnCPitch8 + posx*8, mpBackCb + mnCPitch*(dyC>>1) + (dxC>>1), mnCPitch);

//	__asm emms
}

void VDMPEGDecoder::AddPredictionBackward(int posx, int posy, int dx, int dy) {
	if (mbBackwardFullPel) {
		dx *= 2;
		dy *= 2;
	}

	int dxY		= posx*32 + dx;
	int dyY		= posy*32 + dy;
	int dxC		= posx*16 + dx/2;
	int dyC		= posy*16 + dy/2;

	// check vectors

	if ((unsigned)dxY > (unsigned)(mnBlockW*32-32) || (unsigned)dyY > (unsigned)(mnBlockH*32-32)) {
		dxY = posx*32;
		dyY = posy*32;
		dxC = posx*16;
		dyC = posy*16;
		SetError(kError | kErrorBadMotionVector);
	}

	(mpPredictors->Y_adders[dyY&1][dxY&1])(mpY + posy*mnYPitch8*2 + posx*16, mpBackY + mnYPitch*(dyY>>1) + (dxY>>1), mnYPitch);
	(mpPredictors->C_adders[dyC&1][dxC&1])(mpCr + posy*mnCPitch8 + posx*8, mpBackCr + mnCPitch*(dyC>>1) + (dxC>>1), mnCPitch);
	(mpPredictors->C_adders[dyC&1][dxC&1])(mpCb + posy*mnCPitch8 + posx*8, mpBackCb + mnCPitch*(dyC>>1) + (dxC>>1), mnCPitch);

//	__asm emms
}
