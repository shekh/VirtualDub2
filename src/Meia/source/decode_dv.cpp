//	VirtualDub - Video processing and capture application
//	Video decoding library
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

#include <vd2/system/cpuaccel.h>
#include <vd2/system/debug.h>
#include <vd2/system/memory.h>
#include <vd2/Meia/decode_dv.h>
#include <vd2/Meia/MPEGIDCT.h>
#include <vd2/Kasumi/pixmap.h>

#ifdef _M_AMD64
	extern "C" void _mm_sfence();
	#pragma intrinsic(_mm_sfence)
#endif

class VDVideoDecoderDV : public IVDVideoDecoderDV {
public:
	void		DecompressFrame(const void *src, bool isPAL);
	VDPixmap	GetFrameBuffer();

	void* operator new(size_t i)  { return _aligned_malloc(i, 16); }
	void operator delete(void* p) { _aligned_free(p); }

protected:
	void InterpolatePALChroma();

	bool	mbLastWasPAL;

	__declspec(align(16)) uint8	mYPlane[576][736];

	__declspec(align(16)) union {
		struct {
			uint8	mCrPlane[480][184];
			uint8	mCbPlane[480][184];
		} m411;
		struct {
			uint8	mCrPlane[576][368];
			uint8	mCbPlane[576][368];
		} m420;
	};
};

IVDVideoDecoderDV *VDCreateVideoDecoderDV() {
	return new VDVideoDecoderDV;
}

// 10 DIF sequences (NTSC) or 12 DIF sequences (PAL)
// Each DIF sequence contains:
//		1 DIF block		header
//		2 DIF blocks	subcode
//		3 DIF blocks	VAUX
//		9 DIF blocks	audio
//		135 DIF blocks	video
//
// Each DIF block has a 3 byte header and 77 bytes of payload.

namespace {

#define	TIMES_2x(v1,v2,v3) v1,v2,v3,v1,v2,v3
#define	TIMES_4x(v1,v2,v3) v1,v2,v3,v1,v2,v3,v1,v2,v3,v1,v2,v3
#define	TIMES_8x(v1,v2,v3) v1,v2,v3,v1,v2,v3,v1,v2,v3,v1,v2,v3,v1,v2,v3,v1,v2,v3,v1,v2,v3,v1,v2,v3

#define VLC_3(run,amp) TIMES_8x({run+1,3,(amp<<7)}),TIMES_8x({run+1,3,-(amp<<7)})
#define VLC_4(run,amp) TIMES_4x({run+1,4,(amp<<7)}),TIMES_4x({run+1,4,-(amp<<7)})
#define REP_4(run,amp) TIMES_4x({run+1,4,(amp<<7)})
#define VLC_5(run,amp) TIMES_2x({run+1,5,(amp<<7)}),TIMES_2x({run+1,5,-(amp<<7)})
#define VLC_6(run,amp) {run+1,6,(amp<<7)},{run+1,6,-(amp<<7)}

#define VLC_7(run,amp) TIMES_2x({run+1,7,(amp<<7)}),TIMES_2x({run+1,7,-(amp<<7)})
#define VLC_8(run,amp) {run+1,8,(amp<<7)},{run+1,8,-(amp<<7)}

#define VLC_9(run,amp) {run+1,9,(amp<<7)},{run+1,9,-(amp<<7)}

#define VLC_10(run,amp) {run+1,10,(amp<<7)},{run+1,10,-(amp<<7)}

#define VLC_11(run,amp) TIMES_4x({run+1,11,(amp<<7)}),TIMES_4x({run+1,11,-(amp<<7)})
#define REP_11(run,amp) TIMES_4x({run+1,11,(amp<<7)})
#define VLC_12(run,amp) TIMES_2x({run+1,12,(amp<<7)}),TIMES_2x({run+1,12,-(amp<<7)})
#define REP_12(run,amp) TIMES_2x({run+1,12,(amp<<7)})
#define VLC_13(run,amp) {run+1,13,(amp<<7)},{run+1,13,-(amp<<7)}

#define REP_13(run,amp) {run+1,13,(amp<<7)}

#define REP_16(run,amp) {run+1,16,(amp<<7)}

	struct VLCEntry {
		uint8	run;
		uint8	len;
		sint16	coeff;
	};

	static const VLCEntry kDVACDecode1[48]={
						// xxxxxx
		VLC_3(0,1),		// 00s
		VLC_4(0,2),		// 010s
		REP_4(64,0),	// 0110
		VLC_5(1,1),		// 0111s
		VLC_5(0,3),		// 1000s
		VLC_5(0,4),		// 1001s
		VLC_6(2,1),		// 10100s
		VLC_6(1,2),		// 10101s
		VLC_6(0,5),		// 10110s
		VLC_6(0,6),		// 10111s
	};

	static const VLCEntry kDVACDecode2[32]={
						// 11xxxxxx
		VLC_7(3,1),		// 110000s
		VLC_7(4,1),		// 110001s
		VLC_7(0,7),		// 110010s
		VLC_7(0,8),		// 110011s
		VLC_8(5,1),		// 1101000s
		VLC_8(6,1),		// 1101001s
		VLC_8(2,2),		// 1101010s
		VLC_8(1,3),		// 1101011s
		VLC_8(1,4),		// 1101100s
		VLC_8(0,9),		// 1101101s
		VLC_8(0,10),	// 1101110s
		VLC_8(0,11),	// 1101111s
	};

	static const VLCEntry kDVACDecode3[32]={
						// 111xxxxxx
		VLC_9(7,1),		// 11100000s
		VLC_9(8,1),		// 11100001s
		VLC_9(9,1),		// 11100010s
		VLC_9(10,1),	// 11100011s
		VLC_9(3,2),		// 11100100s
		VLC_9(4,2),		// 11100101s
		VLC_9(2,3),		// 11100110s
		VLC_9(1,5),		// 11100111s
		VLC_9(1,6),		// 11101000s
		VLC_9(1,7),		// 11101001s
		VLC_9(0,12),	// 11101010s
		VLC_9(0,13),	// 11101011s
		VLC_9(0,14),	// 11101100s
		VLC_9(0,15),	// 11101101s
		VLC_9(0,16),	// 11101110s
		VLC_9(0,17),	// 11101111s
	};

	static const VLCEntry kDVACDecode4[32]={
						// 1111xxxxxx
		VLC_10(11,1),	// 111100000s
		VLC_10(12,1),	// 111100001s
		VLC_10(13,1),	// 111100010s
		VLC_10(14,1),	// 111100011s
		VLC_10(5,2),	// 111100100s
		VLC_10(6,2),	// 111100101s
		VLC_10(3,3),	// 111100110s
		VLC_10(4,3),	// 111100111s
		VLC_10(2,4),	// 111101000s
		VLC_10(2,5),	// 111101001s
		VLC_10(1,8),	// 111101010s
		VLC_10(0,18),	// 111101011s
		VLC_10(0,19),	// 111101100s
		VLC_10(0,20),	// 111101101s
		VLC_10(0,21),	// 111101110s
		VLC_10(0,22),	// 111101111s
	};

	static const VLCEntry kDVACDecode5[192]={
						// 111110xxxxxxx
		VLC_11(5,3),	// 1111100000s
		VLC_11(3,4),	// 1111100001s
		VLC_11(3,5),	// 1111100010s
		VLC_11(2,6),	// 1111100011s
		VLC_11(1,9),	// 1111100100s
		VLC_11(1,10),	// 1111100101s
		VLC_11(1,11),	// 1111100110s
		REP_11(0,0),	// 11111001110
		REP_11(1,0),	// 11111001111
		VLC_12(6,3),	// 11111010000s
		VLC_12(4,4),	// 11111010001s
		VLC_12(3,6),	// 11111010010s
		VLC_12(1,12),	// 11111010011s
		VLC_12(1,13),	// 11111010100s
		VLC_12(1,14),	// 11111010101s
		REP_12(2,0),	// 111110101100
		REP_12(3,0),	// 111110101101
		REP_12(4,0),	// 111110101110
		REP_12(5,0),	// 111110101111
		VLC_13(7,2),	// 111110110000s
		VLC_13(8,2),	// 111110110001s
		VLC_13(9,2),	// 111110110010s
		VLC_13(10,2),	// 111110110011s
		VLC_13(7,3),	// 111110110100s
		VLC_13(8,3),	// 111110110101s
		VLC_13(4,5),	// 111110110110s
		VLC_13(3,7),	// 111110110111s
		VLC_13(2,7),	// 111110111000s
		VLC_13(2,8),	// 111110111001s
		VLC_13(2,9),	// 111110111010s
		VLC_13(2,10),	// 111110111011s
		VLC_13(2,11),	// 111110111100s
		VLC_13(1,15),	// 111110111101s
		VLC_13(1,16),	// 111110111110s
		VLC_13(1,17),	// 111110111111s
		REP_13(0,0),	// 1111110000000
		REP_13(1,0),	// 1111110000001
		REP_13(2,0),	// 1111110000010
		REP_13(3,0),	// 1111110000011
		REP_13(4,0),	// 1111110000100
		REP_13(5,0),	// 1111110000101
		REP_13(6,0),	// 1111110000110
		REP_13(7,0),	// 1111110000111
		REP_13(8,0),	// 1111110001000
		REP_13(9,0),	// 1111110001001
		REP_13(10,0),	// 1111110001010
		REP_13(11,0),	// 1111110001011
		REP_13(12,0),	// 1111110001100
		REP_13(13,0),	// 1111110001101
		REP_13(14,0),	// 1111110001110
		REP_13(15,0),	// 1111110001111
		REP_13(16,0),	// 1111110010000
		REP_13(17,0),	// 1111110010001
		REP_13(18,0),	// 1111110010010
		REP_13(19,0),	// 1111110010011
		REP_13(20,0),	// 1111110010100
		REP_13(21,0),	// 1111110010101
		REP_13(22,0),	// 1111110010110
		REP_13(23,0),	// 1111110010111
		REP_13(24,0),	// 1111110011000
		REP_13(25,0),	// 1111110011001
		REP_13(26,0),	// 1111110011010
		REP_13(27,0),	// 1111110011011
		REP_13(28,0),	// 1111110011100
		REP_13(29,0),	// 1111110011101
		REP_13(30,0),	// 1111110011110
		REP_13(31,0),	// 1111110011111
		REP_13(32,0),	// 1111110100000
		REP_13(33,0),	// 1111110100001
		REP_13(34,0),	// 1111110100010
		REP_13(35,0),	// 1111110100011
		REP_13(36,0),	// 1111110100100
		REP_13(37,0),	// 1111110100101
		REP_13(38,0),	// 1111110100110
		REP_13(39,0),	// 1111110100111
		REP_13(40,0),	// 1111110101000
		REP_13(41,0),	// 1111110101001
		REP_13(42,0),	// 1111110101010
		REP_13(43,0),	// 1111110101011
		REP_13(44,0),	// 1111110101100
		REP_13(45,0),	// 1111110101101
		REP_13(46,0),	// 1111110101110
		REP_13(47,0),	// 1111110101111
		REP_13(48,0),	// 1111110110000
		REP_13(49,0),	// 1111110110001
		REP_13(50,0),	// 1111110110010
		REP_13(51,0),	// 1111110110011
		REP_13(52,0),	// 1111110110100
		REP_13(53,0),	// 1111110110101
		REP_13(54,0),	// 1111110110110
		REP_13(55,0),	// 1111110110111
		REP_13(56,0),	// 1111110111000
		REP_13(57,0),	// 1111110111001
		REP_13(58,0),	// 1111110111010
		REP_13(59,0),	// 1111110111011
		REP_13(60,0),	// 1111110111100
		REP_13(61,0),	// 1111110111101
		REP_13(62,0),	// 1111110111110
		REP_13(63,0),	// 1111110111111
	};

	static const VLCEntry kDVACDecode8[512]={
#define R2(y) {1,16,((y)<<7)},{1,16,-((y)<<7)}
#define R(x) R2(x+0),R2(x+1),R2(x+2),R2(x+3),R2(x+4),R2(x+5),R2(x+6),R2(x+7),R2(x+8),R2(x+9),R2(x+10),R2(x+11),R2(x+12),R2(x+13),R2(x+14),R2(x+15)
		R(0x00),
		R(0x10),
		R(0x20),
		R(0x30),
		R(0x40),
		R(0x50),
		R(0x60),
		R(0x70),
		R(0x80),
		R(0x90),
		R(0xA0),
		R(0xB0),
		R(0xC0),
		R(0xD0),
		R(0xE0),
		R(0xF0),
#undef R
#undef R2
	};

	const VLCEntry *DVDecodeAC(uint32 bitheap) {
		if (bitheap < 0xC0000000)
			return &kDVACDecode1[bitheap >> 26];

		if (bitheap < 0xE0000000)
			return &kDVACDecode2[(bitheap >> 24) - (0xC0000000 >> 24)];

		if (bitheap < 0xF0000000)
			return &kDVACDecode3[(bitheap >> 23) - (0xE0000000 >> 23)];

		if (bitheap < 0xF8000000)
			return &kDVACDecode4[(bitheap >> 22) - (0xF0000000 >> 22)];

		if (bitheap < 0xFE000000)
			return &kDVACDecode5[(bitheap >> 19) - (0xF8000000 >> 19)];

		return &kDVACDecode8[(bitheap >> 16) - (0xFE000000 >> 16)];
	}

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

	static const int zigzag_alt[64]={
		 0,  8,  1,  9, 16, 24,  2, 10,
		17, 25, 32, 40, 48, 56, 33, 41,
		18, 26,  3, 11,  4, 12, 19, 27,
		34, 42, 49, 57, 50, 58, 35, 43,
		20, 28,  5, 13,  6, 14, 21, 29,
		36, 44, 51, 59, 52, 60, 37, 45,
		22, 30,  7, 15, 23, 31, 38, 46,
		53, 61, 54, 62, 39, 47, 55, 63,
	};

	static const int range[64]={
		0,
		0,0,0,0,0,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
	};

	static const char shifttable[][4]={
		{0,0,0,0},
		{0,0,0,1},
		{0,0,1,1},
		{0,1,1,2},
		{1,1,2,2},
		{1,2,2,3},
		{2,2,3,3},
		{2,3,3,4},
		{3,3,4,4},
		{3,4,4,5},
		{4,4,5,5},
		{1,1,1,1},
		{1,1,1,2},
	};

	static const int quanttable[4][16]={
		{            5,5,4,4,3,3,2,2,1,0,0,0,0,0,0,0},
		{      7,6,6,5,5,4,4,3,3,2,2,1,0,0,0,0},
		{8,8,7,7,6,6,5,5,4,4,3,3,2,2,1,0},
		{ 10,9,9,8,8,7,7,6,6,5,5,4,4,12,11,11},
	};

#if 0
	static const double CS1 = 0.98078528040323044912618223613424;
	static const double CS2 = 0.92387953251128675612818318939679;
	static const double CS3 = 0.83146961230254523707878837761791;
	static const double CS4 = 0.70710678118654752440084436210485;
	static const double CS5 = 0.55557023301960222474283081394853;
	static const double CS6 = 0.3826834323650897717284599840304;
	static const double CS7 = 0.19509032201612826784828486847702;

	static const double w[8] = {
		1.0,
		(4.0*CS7*CS2)/CS4,
		(2.0*CS6)/CS4,
		2.0*CS5,
		8.0/7.0,
		CS3/CS4,
		CS2/CS4,
		CS1/CS4
	};
#else
	// Weights 2/(w(i)*w(j)) according to SMPTE 314M 5.2.2, in 12-bit fixed point.
	static const int weights[64]={
		 8192,  8352,  8867,  9102,  9362,  9633, 10703, 11363,
		 8352,  8516,  9041,  9281,  9546,  9821, 10913, 11585,
		 8867,  9041,  9598,  9852, 10134, 10426, 11585, 12299,
		 9102,  9281,  9852, 10114, 10403, 10703, 11893, 12625,
		 9362,  9546, 10134, 10403, 10700, 11009, 12232, 12986,
		 9633,  9821, 10426, 10703, 11009, 11327, 12586, 13361,
		10703, 10913, 11585, 11893, 12232, 12586, 13985, 14846,
		11363, 11585, 12299, 12625, 12986, 13361, 14846, 15760,
	};
#endif

	struct DVBitSource {
		const uint8 *src;
		const uint8 *srclimit;
		int bitpos;

		void Init(const uint8 *_src, const uint8 *_srclimit, int _bitpos) {
			src = _src;
			srclimit = _srclimit;
			bitpos = _bitpos;
		}
	};

	class DVDecoderContext {
	public:
		DVDecoderContext();

		const VDMPEGIDCTSet	*mpIDCT;
	};

	DVDecoderContext::DVDecoderContext() {
		long flags = CPUGetEnabledExtensions();

#ifdef _M_AMD64
		mpIDCT = &g_VDMPEGIDCT_sse2;
#else
		if (flags & CPUF_SUPPORTS_SSE2)
			mpIDCT = &g_VDMPEGIDCT_sse2;
		else if (flags & CPUF_SUPPORTS_INTEGER_SSE)
			mpIDCT = &g_VDMPEGIDCT_isse;
		else if (flags & CPUF_SUPPORTS_MMX)
			mpIDCT = &g_VDMPEGIDCT_mmx;
		else
			mpIDCT = &g_VDMPEGIDCT_scalar;
#endif
	}

	class DVDCTBlockDecoder {
	public:
		void Init(uint8 *dst, ptrdiff_t pitch, DVBitSource& bitsource, int qno, bool split, const int zigzag[2][64], short weights_prescaled[2][13][64]);
		bool Decode(DVBitSource& bitsource, const DVDecoderContext& context, bool final);

	protected:
		uint32		mBitHeap;
		int			mBitCount;
		int			mIdx;
		const char	*mpShiftTab;
		const int	*mpZigzag;
		const short	*mpWeights;

		__declspec(align(16)) sint16		mCoeff[64];

		uint8		*mpDst;
		ptrdiff_t	mPitch;
		bool		mb842;
		bool		mbSplit;
	};

	void DVDCTBlockDecoder::Init(uint8 *dst, ptrdiff_t pitch, DVBitSource& src, int qno, bool split, const int zigzag[2][64], short weights_prescaled[2][13][64]) {
		mpDst = dst;
		mPitch = pitch;
		mBitHeap = mBitCount = 0;
		mIdx = 0;

		memset(&mCoeff, 0, sizeof mCoeff);

		const uint8 *src0 = src.src;
		++src.src;
		src.bitpos = 4;

		const int dctclass = (src0[1] >> 4) & 3;

		mpShiftTab = shifttable[quanttable[dctclass][qno]];
		mCoeff[0] = (sint16)(((sint8)src0[0]*2 + (src0[1]>>7) + 0x100) << 2);

		mb842 = false;
		mpZigzag = zigzag[0];
		mpWeights = weights_prescaled[0][quanttable[dctclass][qno]];
		if (src0[1] & 0x40) {
			mb842 = true;
			mpZigzag = zigzag[1];
			mpWeights = weights_prescaled[1][quanttable[dctclass][qno]];
		}

		mbSplit = split;
	}

#ifdef _MSC_VER
	#pragma auto_inline(off)
#endif
	void WrapPrescaledIDCT(const VDMPEGIDCTSet& idct, uint8 *dst, ptrdiff_t pitch, void *coeff0, int last_pos) {
		const sint16 *coeff = (const sint16 *)coeff0;
		int coeff2[64];

		for(int i=0; i<64; ++i)
			coeff2[i] = (coeff[i] * idct.pPrescaler[i] + 128) >> 8;

		idct.pIntra(dst, pitch, coeff2, last_pos);
	}
#ifdef _MSC_VER
	#pragma auto_inline(on)
#endif

	bool DVDCTBlockDecoder::Decode(DVBitSource& bitsource, const DVDecoderContext& context, bool final) {
		if (!mpWeights)
			return true;

		if (bitsource.src >= bitsource.srclimit) {
			if (final)
				goto output;
			return false;
		}

		VDASSERT(mBitCount < 24);

		mBitHeap += (((*bitsource.src++) << bitsource.bitpos) & 0xff) << (24-mBitCount);
		mBitCount += 8 - bitsource.bitpos;

		for(;;) {
			if (mBitCount < 16) {
				if(bitsource.src < bitsource.srclimit) {
					mBitHeap += *bitsource.src++ << (24 - mBitCount);
					mBitCount += 8;
				}
				if(bitsource.src < bitsource.srclimit) {
					mBitHeap += *bitsource.src++ << (24 - mBitCount);
					mBitCount += 8;
				}
			}

			const VLCEntry *acdec = DVDecodeAC(mBitHeap);

			int tmpcnt = mBitCount - acdec->len;

			if (tmpcnt < 0) {
				if (final)
					break;
				return false;
			}

			mBitCount = tmpcnt;
			mBitHeap <<= acdec->len;

			if (acdec->run >= 64)
				break;

			int tmpidx = mIdx + acdec->run;

			if (tmpidx >= 64) {
				mIdx = 63;
				break;
			}

			mIdx = tmpidx;

			const int q = mpWeights[mIdx];

			mCoeff[mpZigzag[mIdx]] = (short)((((sint32)acdec->coeff /*<< mpShiftTab[range[mIdx]]*/) * q + 2048) >> 12);
		}

		// release bits
		bitsource.src -= mBitCount >> 3;
		bitsource.bitpos = 0;
		if (mBitCount & 7) {
			--bitsource.src;
			bitsource.bitpos = 8 - (mBitCount & 7);
		}

output:
		mpWeights = NULL;

#pragma vdpragma_TODO("optimize interlaced 8x4x2 IDCT")
		if (mb842)
			g_VDMPEGIDCT_reference.pIntra4x2(mpDst, mPitch, mCoeff, 63);
		else {
			if (context.mpIDCT->pPrescaler) {
#pragma vdpragma_TODO("consider whether we should suck less here on scalar")
				WrapPrescaledIDCT(*context.mpIDCT, mpDst, mPitch, mCoeff, mIdx);
			} else {
				context.mpIDCT->pIntra(mpDst, mPitch, mCoeff, mIdx);
			}
		}

		if (mbSplit) {
			for(int i=0; i<8; ++i) {
				*(uint32 *)(mpDst + mPitch*(8+i)) = *(uint32 *)(mpDst + 4 + mPitch*i);
			}
		}

		return true;
	}
}

void VDVideoDecoderDV::DecompressFrame(const void *src, bool isPAL) {
	VDASSERT(VDIsValidReadRegion(src, isPAL ? 144000 : 120000));

	static const int sNTSCMacroblockOffsets[5][27][2]={
#define P(x,y) {x*32+y*8*sizeof(mYPlane[0]), x*8+y*8*sizeof(m411.mCrPlane[0])}
		{
			P( 9,0),P( 9,1),P( 9,2),P( 9,3),P( 9,4),P( 9,5),
			P(10,5),P(10,4),P(10,3),P(10,2),P(10,1),P(10,0),
			P(11,0),P(11,1),P(11,2),P(11,3),P(11,4),P(11,5),
			P(12,5),P(12,4),P(12,3),P(12,2),P(12,1),P(12,0),
			P(13,0),P(13,1),P(13,2)
		},
		{
								P(4,3),P(4,4),P(4,5),
			P(5,5),P(5,4),P(5,3),P(5,2),P(5,1),P(5,0),
			P(6,0),P(6,1),P(6,2),P(6,3),P(6,4),P(6,5),
			P(7,5),P(7,4),P(7,3),P(7,2),P(7,1),P(7,0),
			P(8,0),P(8,1),P(8,2),P(8,3),P(8,4),P(8,5),
		},
		{
									P(13,3),P(13,4),P(13,5),
			P(14,5),P(14,4),P(14,3),P(14,2),P(14,1),P(14,0),
			P(15,0),P(15,1),P(15,2),P(15,3),P(15,4),P(15,5),
			P(16,5),P(16,4),P(16,3),P(16,2),P(16,1),P(16,0),
			P(17,0),P(17,1),P(17,2),P(17,3),P(17,4),P(17,5),
		},
		{
			P(0,0),P(0,1),P(0,2),P(0,3),P(0,4),P(0,5),
			P(1,5),P(1,4),P(1,3),P(1,2),P(1,1),P(1,0),
			P(2,0),P(2,1),P(2,2),P(2,3),P(2,4),P(2,5),
			P(3,5),P(3,4),P(3,3),P(3,2),P(3,1),P(3,0),
			P(4,0),P(4,1),P(4,2)
		},
		{
			P(18,0),P(18,1),P(18,2),P(18,3),P(18,4),P(18,5),
			P(19,5),P(19,4),P(19,3),P(19,2),P(19,1),P(19,0),
			P(20,0),P(20,1),P(20,2),P(20,3),P(20,4),P(20,5),
			P(21,5),P(21,4),P(21,3),P(21,2),P(21,1),P(21,0),
			P(22,0),P(22,2),P(22,4)
		},
#undef P
	};

	static const int sPALMacroblockOffsets[5][27][2]={
#define P(x,y) {x*16+y*16*sizeof(mYPlane[0]), x*8+y*16*sizeof(m420.mCrPlane[0])}
		{
			P(18,0),P(18,1),P(18,2),P(19,2),P(19,1),P(19,0),
			P(20,0),P(20,1),P(20,2),P(21,2),P(21,1),P(21,0),
			P(22,0),P(22,1),P(22,2),P(23,2),P(23,1),P(23,0),
			P(24,0),P(24,1),P(24,2),P(25,2),P(25,1),P(25,0),
			P(26,0),P(26,1),P(26,2),
		},
		{
			P( 9,0),P( 9,1),P( 9,2),P(10,2),P(10,1),P(10,0),
			P(11,0),P(11,1),P(11,2),P(12,2),P(12,1),P(12,0),
			P(13,0),P(13,1),P(13,2),P(14,2),P(14,1),P(14,0),
			P(15,0),P(15,1),P(15,2),P(16,2),P(16,1),P(16,0),
			P(17,0),P(17,1),P(17,2),
		},
		{
			P(27,0),P(27,1),P(27,2),P(28,2),P(28,1),P(28,0),
			P(29,0),P(29,1),P(29,2),P(30,2),P(30,1),P(30,0),
			P(31,0),P(31,1),P(31,2),P(32,2),P(32,1),P(32,0),
			P(33,0),P(33,1),P(33,2),P(34,2),P(34,1),P(34,0),
			P(35,0),P(35,1),P(35,2),
		},
		{
			P(0,0),P(0,1),P(0,2),P(1,2),P(1,1),P(1,0),
			P(2,0),P(2,1),P(2,2),P(3,2),P(3,1),P(3,0),
			P(4,0),P(4,1),P(4,2),P(5,2),P(5,1),P(5,0),
			P(6,0),P(6,1),P(6,2),P(7,2),P(7,1),P(7,0),
			P(8,0),P(8,1),P(8,2)
		},
		{
			P(36,0),P(36,1),P(36,2),P(37,2),P(37,1),P(37,0),
			P(38,0),P(38,1),P(38,2),P(39,2),P(39,1),P(39,0),
			P(40,0),P(40,1),P(40,2),P(41,2),P(41,1),P(41,0),
			P(42,0),P(42,1),P(42,2),P(43,2),P(43,1),P(43,0),
			P(44,0),P(44,1),P(44,2),
		},
#undef P
	};

	static const int sDCTYBlockOffsets411[2][4]={
		0, 8, 16, 24,
		0, 8, 8 * sizeof mYPlane[0], 8 + 8 * sizeof mYPlane[0],
	};

	static const int sDCTYBlockOffsets420[2][4]={
		0, 8, 8 * sizeof mYPlane[0], 8 + 8 * sizeof mYPlane[0],
		0, 8, 8 * sizeof mYPlane[0], 8 + 8 * sizeof mYPlane[0],
	};

	const int (*const pMacroblockOffsets)[27][2] = (isPAL ? sPALMacroblockOffsets : sNTSCMacroblockOffsets);
	const int (*const pDCTYBlockOffsets)[4] = (isPAL ? sDCTYBlockOffsets420 : sDCTYBlockOffsets411);
	const uint8 *pVideoBlock = (const uint8 *)src + 7*80;

	int chromaStep;

	uint8 *pCr, *pCb;
	ptrdiff_t chroma_pitch;
	int nDIFSequences;

	if (isPAL) {
		chromaStep = sizeof(m420.mCrPlane[0]) * 48;

		memset(m420.mCrPlane, 0x80, sizeof m420.mCrPlane);
		memset(m420.mCbPlane, 0x80, sizeof m420.mCbPlane);

		pCr = m420.mCrPlane[0];
		pCb = m420.mCbPlane[1];
		chroma_pitch = sizeof m420.mCrPlane[0] * 2;
		nDIFSequences = 12;
	} else {
		chromaStep = sizeof(m411.mCrPlane[0]) * 48;

		memset(m411.mCrPlane, 0x80, sizeof m411.mCrPlane);
		memset(m411.mCbPlane, 0x80, sizeof m411.mCbPlane);

		pCr = m411.mCrPlane[0];
		pCb = m411.mCbPlane[0];
		chroma_pitch = sizeof m411.mCrPlane[0];
		nDIFSequences = 10;
	}

	int zigzag[2][64];
	short weights_prescaled[2][13][64];

	DVDecoderContext context;

	if (context.mpIDCT->pAltScan)
		memcpy(zigzag[0], context.mpIDCT->pAltScan, sizeof(int)*64);
	else
		memcpy(zigzag[0], zigzag_std, sizeof(int)*64);

	memcpy(zigzag[1], zigzag_alt, sizeof(int)*64);

	int i;
	for(i=0; i<13; ++i) {
		for(int j=0; j<64; ++j) {
			weights_prescaled[0][i][j] = (short)(((weights[zigzag_std[j]] >> (6 - shifttable[i][range[j]]))+1)>>1);

			int zig = zigzag_alt[j];		// for great justice

			// fold sum/diff together and then double y
			zig = (zig & 0x1f) + (zig & 0x18);

			weights_prescaled[1][i][j] = (short)(((weights[zig] >> (6 - shifttable[i][range[j]]))+1)>>1);
		}
	}

	for(i=0; i<nDIFSequences; ++i) {			// 10/12 DIF sequences
		int audiocounter = 0;

		const int columns[5]={
			(i+2) % nDIFSequences,
			(i+6) % nDIFSequences,
			(i+8) % nDIFSequences,
			(i  ) % nDIFSequences,
			(i+4) % nDIFSequences,
		};

		for(int k=0; k<27; ++k) {
			DVBitSource mSources[30];
			__declspec(align(16)) DVDCTBlockDecoder mDecoders[30];

			int blk = 0;

			for(int j=0; j<5; ++j) {
				const int y_offset = pMacroblockOffsets[j][k][0];
				const int c_offset = pMacroblockOffsets[j][k][1];
				const int super_y = columns[j];

				uint8 *yptr = mYPlane[super_y*48] + y_offset;
				uint8 *crptr = pCr + chromaStep * super_y;
				uint8 *cbptr = pCb + chromaStep * super_y;

				int qno = pVideoBlock[3] & 15;

				bool bHalfBlock = nDIFSequences == 10 && j==4 && k>=24;
				
				mSources[blk+0].Init(pVideoBlock +  4, pVideoBlock + 18, 0);
				mSources[blk+1].Init(pVideoBlock + 18, pVideoBlock + 32, 0);
				mSources[blk+2].Init(pVideoBlock + 32, pVideoBlock + 46, 0);
				mSources[blk+3].Init(pVideoBlock + 46, pVideoBlock + 60, 0);
				mSources[blk+4].Init(pVideoBlock + 60, pVideoBlock + 70, 0);
				mSources[blk+5].Init(pVideoBlock + 70, pVideoBlock + 80, 0);
				mDecoders[blk+0].Init(yptr + pDCTYBlockOffsets[bHalfBlock][0], sizeof mYPlane[0], mSources[blk+0], qno, false, zigzag, weights_prescaled);
				mDecoders[blk+1].Init(yptr + pDCTYBlockOffsets[bHalfBlock][1], sizeof mYPlane[0], mSources[blk+1], qno, false, zigzag, weights_prescaled);
				mDecoders[blk+2].Init(yptr + pDCTYBlockOffsets[bHalfBlock][2], sizeof mYPlane[0], mSources[blk+2], qno, false, zigzag, weights_prescaled);
				mDecoders[blk+3].Init(yptr + pDCTYBlockOffsets[bHalfBlock][3], sizeof mYPlane[0], mSources[blk+3], qno, false, zigzag, weights_prescaled);
				mDecoders[blk+4].Init(crptr + c_offset, chroma_pitch, mSources[blk+4], qno, bHalfBlock, zigzag, weights_prescaled);
				mDecoders[blk+5].Init(cbptr + c_offset, chroma_pitch, mSources[blk+5], qno, bHalfBlock, zigzag, weights_prescaled);

				int i;

				for(i=0; i<6; ++i)
					mDecoders[blk+i].Decode(mSources[blk+i], context, false);

				int source = 0;

				i = 0;
				while(i < 6 && source < 6) {
					if (!mDecoders[blk+i].Decode(mSources[blk+source], context, false))
						++source;
					else
						++i;
				}

				blk += 6;

				pVideoBlock += 80;
				if (++audiocounter >= 15) {
					audiocounter = 0;
					pVideoBlock += 80;
				}
			}

			int source = 0;
			blk = 0;

			while(blk < 30 && source < 30) {
				if (!mDecoders[blk].Decode(mSources[source], context, false))
					++source;
				else
					++blk;
			}

			while(blk < 30) {
				mDecoders[blk++].Decode(mSources[29], context, true);
			}
		}

		pVideoBlock += 80 * 6;
	}

	if (isPAL)
		InterpolatePALChroma();

#ifndef _M_AMD64
	if (MMX_enabled)
		__asm emms
	if (ISSE_enabled)
		__asm sfence
#else
	_mm_sfence();
#endif

	mbLastWasPAL = isPAL;
}

VDPixmap VDVideoDecoderDV::GetFrameBuffer() {
	VDPixmap pxsrc = {0};
	pxsrc.data		= mYPlane;
	pxsrc.pitch		= sizeof mYPlane[0];
	pxsrc.w			= 720;
	if (mbLastWasPAL) {
		pxsrc.data2		= m420.mCbPlane;
		pxsrc.data3		= m420.mCrPlane;
		pxsrc.pitch2	= sizeof m420.mCbPlane[0];
		pxsrc.pitch3	= sizeof m420.mCrPlane[0];
		pxsrc.h			= 576;
		pxsrc.format	= nsVDPixmap::kPixFormat_YUV422_Planar;
	} else {
		pxsrc.data2		= m411.mCbPlane;
		pxsrc.data3		= m411.mCrPlane;
		pxsrc.pitch2	= sizeof m411.mCbPlane[0];
		pxsrc.pitch3	= sizeof m411.mCrPlane[0];
		pxsrc.h			= 480;
		pxsrc.format	= nsVDPixmap::kPixFormat_YUV411_Planar;
	}

	return pxsrc;
}

namespace {
	void AverageRows(uint8 *dst0, const uint8 *src10, const uint8 *src20) {
		uint32 *dst = (uint32 *)dst0;
		const uint32 *src1 = (const uint32 *)src10;
		const uint32 *src2 = (const uint32 *)src20;

		int x = 45;
		do {
			uint32 a, b;

			a = src1[0];
			b = src2[0];
			dst[0] = (a|b) - (((a^b)&0xfefefefe)>>1);

			a = src1[1];
			b = src2[1];
			dst[1] = (a|b) - (((a^b)&0xfefefefe)>>1);

			src1 += 2;
			src2 += 2;
			dst += 2;
		} while(--x);
	}
}

void VDVideoDecoderDV::InterpolatePALChroma() {
	//	Cr:
	//	0	e			e			ok
	//	1					o		copy
	//	2		o		.			lerp
	//	3					.		lerp
	//	4	e			e
	//	5					o
	//	6		o		.
	//	7					.

	uint8 *p0 = m420.mCrPlane[0];
	const ptrdiff_t pitch = sizeof m420.mCrPlane[0];
	const size_t bpr = 360;

	int y;
	for(y=0; y<572; y+=4) {
		uint8 *p1 = p0 + pitch;
		uint8 *p2 = p0 + pitch*2;
		uint8 *p3 = p1 + pitch*2;
		uint8 *p4 = p0 + pitch*4;
		uint8 *p6 = p4 + pitch*2;

		memcpy(p1, p2, bpr);
		AverageRows(p3, p2, p6);
		AverageRows(p2, p0, p4);

		p0 = p4;
	}

	memcpy(p0+pitch*1, p0+pitch*2, bpr);
	memcpy(p0+pitch*2, p0        , bpr);
	memcpy(p0+pitch*3, p0+pitch*1, bpr);

	//	Cb:
	//	0				.			lerp
	//	1	e				.		copy, lerp
	//	2				e			-
	//	3		o			o		ok
	//	4				.
	//	5	e				.
	//	6				e
	//	7		o			o

	p0 = m420.mCbPlane[0];

	memcpy(p0        , p0+pitch*1, bpr);
	memcpy(p0+pitch*2, p0+pitch*1, bpr);
	memcpy(p0+pitch*1, p0+pitch*3, bpr);

	for(y=4; y<576; y+=4) {
		p0 += pitch*4;

		uint8 *py = p0 - pitch*2;
		uint8 *pz = p0 - pitch;
		uint8 *p1 = p0 + pitch;
		uint8 *p2 = p0 + pitch*2;
		uint8 *p3 = p1 + pitch*2;

		AverageRows(p0, py, p1);
		memcpy(p2, p1, bpr);
		AverageRows(p1, p3, pz);
	}
}
