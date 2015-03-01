//	Priss (NekoAmp 2.0) - MPEG-1/2 audio decoding library
//	Copyright (C) 2003 Avery Lee
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

#ifndef f_VD2_PRISS_BITREADER_H
#define f_VD2_PRISS_BITREADER_H

#pragma warning(push)
#pragma warning(disable: 4035)		// warning C4035: 'bswap': no return value

#if _MSC_VER >= 1300
	extern unsigned long _byteswap_ulong(unsigned long v);
	#pragma intrinsic(_byteswap_ulong)
#endif

// our favorite bitreader
class VDMPEGAudioBitReader {
public:
	VDMPEGAudioBitReader(const uint8 *src, uint32 len) : mpSrc(src), mpSrcLimit(src+len), mBitOffset(0) {}

#if _MSC_VER >= 1300
	static inline uint32 bswap(uint32 v) {
		return _byteswap_ulong(v);
	}
#else
	static inline uint32 __fastcall bswap(uint32 v) {
		__asm {
			mov eax, v
			bswap eax
		}
	}
#endif

	unsigned get(unsigned bits) {
		static const uint32 masks[17]={
			(1<<0)-1,
			(1<<1)-1,
			(1<<2)-1,
			(1<<3)-1,
			(1<<4)-1,
			(1<<5)-1,
			(1<<6)-1,
			(1<<7)-1,
			(1<<8)-1,
			(1<<9)-1,
			(1<<10)-1,
			(1<<11)-1,
			(1<<12)-1,
			(1<<13)-1,
			(1<<14)-1,
			(1<<15)-1,
			(1<<16)-1,
		};
#if 0
		unsigned v = (((mpSrc[0]<<16)+(mpSrc[1]<<8)+mpSrc[2]) >> (24-mBitOffset-bits)) & masks[bits];
#elif defined(__INTEL_COMPILER)
		unsigned v = (_bswap(*(const uint32 *)mpSrc) >> (32-mBitOffset-bits)) & masks[bits];
#else
		unsigned v = (bswap(*(const uint32 *)mpSrc) >> (32-mBitOffset-bits)) & masks[bits];
#endif

		mBitOffset += bits;
		mpSrc += mBitOffset>>3;
		mBitOffset &= 7;

		return v;
	}

	bool getbool() {
		bool b = 0 != (mpSrc[0] & (0x80 >> mBitOffset));

		++mBitOffset;
		mpSrc += mBitOffset>>3;
		mBitOffset &= 7;

		return b;
	}

	int avail() const {
		return (int)(((mpSrcLimit - mpSrc)<<3) - mBitOffset);
	}

	bool chkavail(int needed) const {
		return avail() >= needed;
	}

protected:
	const uint8 *mpSrc;
	const uint8 *const mpSrcLimit;
	unsigned	mBitOffset;
};


class VDMPEGAudioHuffBitReader {
public:
	enum {
		kAddressMask = 2047
	};

	VDMPEGAudioHuffBitReader(const uint8 *src, uint32 offset) : mpSrc(src), mInitialByteOffset(offset & kAddressMask), mByteOffset(offset & kAddressMask), mBitHeap(0), mBitShift(24), mBitOffset(-24) {
		refill();
	}

	unsigned peek(unsigned bits) const {
		return mBitHeap >> (32-bits);
	}

	void consume(unsigned bits) {
		mBitHeap <<= bits;
		mBitShift += bits;
		refill();
	}

	unsigned get(unsigned bits) {
		const unsigned v = peek(bits);
		consume(bits);
		return v;
	}

	bool getbit() {
		const bool v = (sint32)mBitHeap < 0;
		consume(1);
		return v;
	}

	void refill_one() {
		mBitHeap += mpSrc[mByteOffset] << mBitShift;
		mBitShift -= 8;
		mByteOffset = (mByteOffset+1) & kAddressMask;
		mBitOffset += 8;
	}

	void refill() {
		while(mBitShift >= 0)
			refill_one();
	}

	unsigned pos() const { return mBitOffset + mBitShift; }

	void seek(unsigned offset) {
		mByteOffset = (mInitialByteOffset + (offset>>3)) & kAddressMask;
		mBitOffset = (offset & ~7) - 24;
		mBitShift = 24;
		mBitHeap = 0;
		refill();
		get(offset & 7);
	}

protected:
	uint32		mBitHeap;
	sint8		mBitShift;		// left shift for next byte (24-bits_in_heap)
	const uint8 *mpSrc;
	unsigned	mByteOffset;
	unsigned	mBitOffset;		// bitoffset - bitshift = bit position
	unsigned	mInitialByteOffset;
};


#pragma warning(pop)

#endif
