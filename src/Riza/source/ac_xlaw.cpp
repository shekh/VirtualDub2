//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2007 Avery Lee
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

#include <vd2/system/VDRingBuffer.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/vdalloc.h>
#include <vd2/Riza/audiocodec.h>

class VDAudioDecompressorXLaw : public IVDAudioCodec {
public:
	VDAudioDecompressorXLaw();
	~VDAudioDecompressorXLaw();

	bool Init(const VDWaveFormat *srcFormat, const VDWaveFormat *dstFormat, bool doALaw);
	void Shutdown();

	bool IsEnded() const;

	unsigned	GetInputLevel() const;
	unsigned	GetInputSpace() const;
	unsigned	GetOutputLevel() const;
	const VDWaveFormat *GetOutputFormat() const;
	unsigned	GetOutputFormatSize() const;

	void		Restart();
	bool		Convert(bool flush, bool requireOutput);

	void		*LockInputBuffer(unsigned& bytes);
	void		UnlockInputBuffer(unsigned bytes);
	const void	*LockOutputBuffer(unsigned& bytes);
	void		UnlockOutputBuffer(unsigned bytes);
	unsigned	CopyOutput(void *dst, unsigned bytes);

protected:
	bool	mbEnded;
	bool	mbPCM16Mode;

	VDRingBuffer<uint8>		mInputBuffer;
	VDRingBuffer<uint8>		mOutputBuffer;
	vdstructex<VDWaveFormat>	mSrcFormat;
	vdstructex<VDWaveFormat>	mDstFormat;

	uint8 mLookup8[256];
	sint16 mLookup16[256];
};

IVDAudioCodec *VDCreateAudioDecompressorMuLaw(const VDWaveFormat *srcFormat, const VDWaveFormat *dstFormat) {
	vdautoptr<VDAudioDecompressorXLaw> codec(new VDAudioDecompressorXLaw);

	if (!codec->Init(srcFormat, dstFormat, false))
		return NULL;

	return codec.release();
}

IVDAudioCodec *VDCreateAudioDecompressorALaw(const VDWaveFormat *srcFormat, const VDWaveFormat *dstFormat) {
	vdautoptr<VDAudioDecompressorXLaw> codec(new VDAudioDecompressorXLaw);

	if (!codec->Init(srcFormat, dstFormat, true))
		return NULL;

	return codec.release();
}

VDAudioDecompressorXLaw::VDAudioDecompressorXLaw()
	: mbEnded(false)
	, mbPCM16Mode(false)
{
}

VDAudioDecompressorXLaw::~VDAudioDecompressorXLaw() {
}

bool VDAudioDecompressorXLaw::Init(const VDWaveFormat *srcFormat, const VDWaveFormat *dstFormat, bool doALaw) {
	if (srcFormat->mTag != (doALaw ? VDWaveFormat::kTagCCITTALaw : VDWaveFormat::kTagCCITTMuLaw))
		return false;

	if (srcFormat->mBlockSize != srcFormat->mChannels)
		return false;

	if (srcFormat->mDataRate != srcFormat->mChannels * srcFormat->mSamplingRate)
		return false;

	if (srcFormat->mSampleBits != 8)
		return false;

	if (dstFormat) {
		if (dstFormat->mTag != VDWaveFormat::kTagPCM)
			return false;

		if (dstFormat->mChannels != srcFormat->mChannels)
			return false;

		if (dstFormat->mSamplingRate != srcFormat->mSamplingRate)
			return false;

		if (dstFormat->mSampleBits == 8) {
			if (dstFormat->mBlockSize != dstFormat->mChannels)
				return false;

			if (dstFormat->mDataRate != dstFormat->mChannels * dstFormat->mSamplingRate)
				return false;
		} else if (dstFormat->mSampleBits == 16) {
			if (dstFormat->mBlockSize != dstFormat->mChannels * 2)
				return false;

			if (dstFormat->mDataRate != dstFormat->mChannels * dstFormat->mSamplingRate * 2)
				return false;
		} else {
			return false;
		}

		mDstFormat.assign(dstFormat, sizeof(VDWaveFormat) + dstFormat->mExtraSize);
	} else {
		mDstFormat.resize(sizeof(VDWaveFormat));
		mDstFormat->mTag = VDWaveFormat::kTagPCM;
		mDstFormat->mChannels = srcFormat->mChannels;
		mDstFormat->mSamplingRate = srcFormat->mSamplingRate;
		mDstFormat->mSampleBits = 16;
		mDstFormat->mBlockSize = 2 * srcFormat->mChannels;
		mDstFormat->mDataRate = mDstFormat->mBlockSize * mDstFormat->mSamplingRate;
		mDstFormat->mExtraSize = 0;
	}

	mSrcFormat.assign(srcFormat, sizeof(VDWaveFormat) + srcFormat->mExtraSize);

	mbPCM16Mode = (mDstFormat->mSampleBits >= 16);

	// Initialize positive conversion table (see ITU-T Rec. G.711 for details).
	// This has been checked to be exact against the Microsoft codecs.
	if (doALaw) {
		// A-Law has a larger linear ramp than mu-law. It also inverts even bits
		// (but note that the spec numbers bits starting at 1). Interestingly,
		// A-Law can't represent 0.
		for(int i=0; i<32; ++i) {
			int j = i ^ 0x15;
			mLookup16[0xc0 + i] =    1*8 + (j <<  4);
		}

		for(int i=0; i<16; ++i) {
			int j = i ^ 0x05;

			mLookup16[0xf0 + i] =   66*8 + (j <<  5);
			mLookup16[0xe0 + i] =  132*8 + (j <<  6);
			mLookup16[0x90 + i] =  264*8 + (j <<  7);
			mLookup16[0x80 + i] =  528*8 + (j <<  8);
			mLookup16[0xb0 + i] = 1056*8 + (j <<  9);
			mLookup16[0xa0 + i] = 2112*8 + (j << 10);
		}
	} else {
		for(int i=0; i<16; ++i) {
			mLookup16[0xff - i] =    0*4 + (i <<  3);
			mLookup16[0xef - i] =   33*4 + (i <<  4);
			mLookup16[0xdf - i] =   99*4 + (i <<  5);
			mLookup16[0xcf - i] =  231*4 + (i <<  6);
			mLookup16[0xbf - i] =  495*4 + (i <<  7);
			mLookup16[0xaf - i] = 1023*4 + (i <<  8);
			mLookup16[0x9f - i] = 2079*4 + (i <<  9);
			mLookup16[0x8f - i] = 4191*4 + (i << 10);
		}

		// fixup zero
		mLookup16[0xff] = 0;
	}

	// initialize negative conversion table
	for(int i=0; i<128; ++i)
		mLookup16[i] = -mLookup16[i + 0x80];

	// initialize 8-bit table
	for(int i=0; i<256; ++i) {
		int y = (mLookup16[i] + 0x8080) >> 8;
		mLookup8[i] = (uint8)(y > 255 ? 255 : y);
	}

	mInputBuffer.Init(4096 * mSrcFormat->mBlockSize);
	mOutputBuffer.Init(4096 * mDstFormat->mBlockSize);

	return true;
}

void VDAudioDecompressorXLaw::Shutdown() {
	mSrcFormat.clear();
	mDstFormat.clear();
	mInputBuffer.Shutdown();
	mOutputBuffer.Shutdown();
}

bool VDAudioDecompressorXLaw::IsEnded() const {
	return mbEnded;
}

unsigned VDAudioDecompressorXLaw::GetInputLevel() const {
	return mInputBuffer.getLevel();
}

unsigned VDAudioDecompressorXLaw::GetInputSpace() const {
	return mInputBuffer.getSpace();
}

unsigned VDAudioDecompressorXLaw::GetOutputLevel() const {
	return mOutputBuffer.getLevel();
}

const VDWaveFormat *VDAudioDecompressorXLaw::GetOutputFormat() const {
	return mDstFormat.data();
}

unsigned VDAudioDecompressorXLaw::GetOutputFormatSize() const {
	return mDstFormat.size();
}

void VDAudioDecompressorXLaw::Restart() {
	mInputBuffer.Flush();
	mOutputBuffer.Flush();
	mbEnded = false;
}

bool VDAudioDecompressorXLaw::Convert(bool flush, bool requireOutput) {
	bool didSomething = false;

	for(;;) {
		int writeSpace;
		void *dst0 = mOutputBuffer.LockWriteAll(writeSpace);

		if (mbPCM16Mode)
			writeSpace >>= 1;

		if (!writeSpace)
			break;

		int count;
		const uint8 *src = (const uint8 *)mInputBuffer.LockRead(writeSpace, count);

		if (!count)
			break;

		didSomething = true;

		if (mbPCM16Mode) {
			sint16 *dst16 = (sint16 *)dst0;
			const sint16 *const VDRESTRICT lookup = mLookup16;

			for(int i=0; i<count; ++i)
				dst16[i] = lookup[src[i]];

			mOutputBuffer.UnlockWrite(count * 2);
		} else {
			uint8 *dst8 = (uint8 *)dst0;
			const uint8 *const VDRESTRICT lookup = mLookup8;

			for(int i=0; i<count; ++i)
				dst8[i] = lookup[src[i]];

			mOutputBuffer.UnlockWrite(count);
		}

		mInputBuffer.UnlockRead(count);
	}

	if (!didSomething && flush)
		mbEnded = true;

	return didSomething;
}

void *VDAudioDecompressorXLaw::LockInputBuffer(unsigned& bytes) {
	int actual;
	uint8 *p = mInputBuffer.LockWriteAll(actual);
	bytes = actual;
	return p;
}

void VDAudioDecompressorXLaw::UnlockInputBuffer(unsigned bytes) {
	mInputBuffer.UnlockWrite(bytes);
}

const void	*VDAudioDecompressorXLaw::LockOutputBuffer(unsigned& bytes) {
	int actual;
	const uint8 *p = mOutputBuffer.LockReadAll(actual);
	bytes = actual;
	return p;
}

void VDAudioDecompressorXLaw::UnlockOutputBuffer(unsigned bytes) {
	mOutputBuffer.UnlockRead(bytes);
}

unsigned VDAudioDecompressorXLaw::CopyOutput(void *dst, unsigned bytes) {
	if (!dst) {
		int actual;
		mOutputBuffer.LockRead(bytes, actual);
		mOutputBuffer.UnlockRead(actual);
		return actual;
	}

	return mOutputBuffer.Read((uint8 *)dst, bytes);
}
