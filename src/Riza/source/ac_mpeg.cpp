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
#include <vd2/system/math.h>
#include <vd2/Riza/audiocodec.h>
#include <vd2/Priss/decoder.h>

class VDAudioDecompressorMPEG : public IVDAudioCodec, public IVDMPEGAudioBitsource {
public:
	VDAudioDecompressorMPEG();
	~VDAudioDecompressorMPEG();

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
	int			read(void *buffer, int bytes);

	bool	mbEnded;
	uint32	mSamplesPerFrameL1L2;		// 576 or 1152 * channels
	uint32	mSamplesPerFrameL3;
	uint32	mSamplesInNextFrame;
	uint32	mDiscardedBytes;
	uint32	mFillSamples;
	double	mDstToSrcDataRatio;

	enum {
		kHeaderCheckMask = 0xFFF80C00		// check sync, MPEG-2.5 (sync 11), MPEG-2 (id), and sampling rate bits
	};

	uint32	mHeaderCheckValue;

	enum State {
		kStateHeader,
		kStateFill,
		kStateData
	} mState;

	vdautoptr<IVDMPEGAudioDecoder>	mpDecoder;

	vdfastvector<uint8>		mInputBuffer;
	uint32					mInputBufferReadPt;
	uint32					mInputBufferWritePt;
	uint32					mInputBufferHighWatermark;
	VDRingBuffer<sint16>	mOutputBuffer;
	vdstructex<VDWaveFormat>	mSrcFormat;
	vdstructex<VDWaveFormat>	mDstFormat;

	sint16 mDecodeBuffer[1152 * 2];
};

IVDAudioCodec *VDCreateAudioDecompressorMPEG(const VDWaveFormat *srcFormat, const VDWaveFormat *dstFormat) {
	vdautoptr<VDAudioDecompressorMPEG> codec(new VDAudioDecompressorMPEG);

	if (!codec->Init(srcFormat, dstFormat, false))
		return NULL;

	return codec.release();
}

VDAudioDecompressorMPEG::VDAudioDecompressorMPEG()
	: mbEnded(false)
	, mDiscardedBytes(0)
	, mFillSamples(0)
	, mSamplesPerFrameL1L2(0)
	, mSamplesPerFrameL3(0)
	, mSamplesInNextFrame(0)
	, mState(kStateHeader)
	, mpDecoder(VDCreateMPEGAudioDecoder())
{
	mpDecoder->SetSource(this);
	mpDecoder->Init();
}

VDAudioDecompressorMPEG::~VDAudioDecompressorMPEG() {
}

bool VDAudioDecompressorMPEG::Init(const VDWaveFormat *srcFormat, const VDWaveFormat *dstFormat, bool doALaw) {
	if (srcFormat->mTag != VDWaveFormat::kTagMPEGLayer3 && srcFormat->mTag != VDWaveFormat::kTagMPEG1)
		return false;

	// validate incoming sampling rate
	bool is_mpeg2 = false;
	bool is_mpeg25 = false;
	int samplingRateCode = 0;

	switch(srcFormat->mSamplingRate) {
		case 8000:
			is_mpeg2 = true;
			is_mpeg25 = true;
			samplingRateCode = 2;
			break;

		case 11025:
			is_mpeg2 = true;
			is_mpeg25 = true;
			samplingRateCode = 0;
			break;

		case 12000:
			is_mpeg2 = true;
			is_mpeg25 = true;
			samplingRateCode = 1;
			break;

		case 16000:
			is_mpeg2 = true;
			is_mpeg25 = false;
			samplingRateCode = 2;
			break;

		case 22050:
			is_mpeg2 = true;
			is_mpeg25 = false;
			samplingRateCode = 0;
			break;

		case 24000:
			is_mpeg2 = true;
			is_mpeg25 = false;
			samplingRateCode = 1;
			break;

		case 32000:
			is_mpeg2 = false;
			is_mpeg25 = false;
			samplingRateCode = 2;
			break;

		case 44100:
			is_mpeg2 = false;
			is_mpeg25 = false;
			samplingRateCode = 0;
			break;

		case 48000:
			is_mpeg2 = false;
			is_mpeg25 = false;
			samplingRateCode = 1;
			break;
	}

	if (srcFormat->mChannels != 1 && srcFormat->mChannels != 2)
		return false;

	if (dstFormat) {
		if (dstFormat->mTag != VDWaveFormat::kTagPCM)
			return false;

		if (dstFormat->mChannels != srcFormat->mChannels)
			return false;

		if (dstFormat->mSamplingRate != srcFormat->mSamplingRate)
			return false;

		if (dstFormat->mSampleBits == 16) {
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

	mSamplesPerFrameL1L2	= 1152 * mDstFormat->mChannels;
	mSamplesPerFrameL3		= 576 * mDstFormat->mChannels;
	if (mDstFormat->mSamplingRate >= 32000)
		mSamplesPerFrameL3 <<= 1;

	mSrcFormat.assign(srcFormat, sizeof(VDWaveFormat) + srcFormat->mExtraSize);

	mDstToSrcDataRatio = (double)mDstFormat->mDataRate / (double)mSrcFormat->mDataRate;

	mInputBuffer.resize(65536);
	mInputBufferHighWatermark = 0xe000;
	mInputBufferReadPt = 0;
	mInputBufferWritePt = 0;

	mOutputBuffer.Init(1152 * mDstFormat->mBlockSize);

	mHeaderCheckValue = 0xFFE00000;

	if (!is_mpeg25)
		mHeaderCheckValue |= 0x00100000;

	if (!is_mpeg2)
		mHeaderCheckValue |= 0x00080000;

	mHeaderCheckValue |= samplingRateCode << 10;

	return true;
}

void VDAudioDecompressorMPEG::Shutdown() {
	mSrcFormat.clear();
	mDstFormat.clear();
	mInputBuffer.clear();
	mOutputBuffer.Shutdown();
	mpDecoder = NULL;
}

bool VDAudioDecompressorMPEG::IsEnded() const {
	return mbEnded;
}

unsigned VDAudioDecompressorMPEG::GetInputLevel() const {
	return mInputBufferWritePt - mInputBufferReadPt;
}

unsigned VDAudioDecompressorMPEG::GetInputSpace() const {
	return mInputBuffer.size() - mInputBufferWritePt;
}

unsigned VDAudioDecompressorMPEG::GetOutputLevel() const {
	return mOutputBuffer.getLevel();
}

const VDWaveFormat *VDAudioDecompressorMPEG::GetOutputFormat() const {
	return mDstFormat.data();
}

unsigned VDAudioDecompressorMPEG::GetOutputFormatSize() const {
	return mDstFormat.size();
}

void VDAudioDecompressorMPEG::Restart() {
	mInputBufferReadPt = 0;
	mInputBufferWritePt = 0;
	mOutputBuffer.Flush();
	mbEnded = false;
	mState = kStateHeader;
	mpDecoder->Reset();
	mDiscardedBytes = 0;
	mFillSamples = 0;
}

bool VDAudioDecompressorMPEG::Convert(bool flush, bool requireOutput) {
	bool didSomething = false;

	for(;;) {
		if (mState == kStateHeader) {
			int actual = mInputBufferWritePt - mInputBufferReadPt;
			int readpt = mInputBufferReadPt;
			const uint8 *src = mInputBuffer.data();

			// shift in bytes until we see a valid header (this will always be at
			// least four bytes).
			const int size = mInputBuffer.size();
			uint32 hdr = 0;
			int i = 0;
			bool valid = false;

			while(i < actual) {
				++i;
				hdr = (hdr << 8) + src[readpt++];
				if (readpt >= size)
					readpt = 0;

				if ((hdr & kHeaderCheckMask) == mHeaderCheckValue) {
					// MPEG-1.5 isn't valid
					if ((hdr & 0x00180000) == 0x00080000)
						continue;

					// check for valid bitrate
					if (!(hdr & 0x0000F000))
						continue;

					// check for valid layer
					if ((hdr & 0x00060000) == 0)
						continue;

					// check for layer III
					if ((hdr & 0x00060000) == 0x00020000)
						mSamplesInNextFrame = mSamplesPerFrameL3;
					else
						mSamplesInNextFrame = mSamplesPerFrameL1L2;

					// all good
					valid = true;
					break;
				}
			}

			// consume unused bytes
			if (i > 4) {
				didSomething = true;
				mInputBufferReadPt += (i - 4);
				mDiscardedBytes += (i-4);
			}

			if (valid) {
				didSomething = true;
				try {
					mpDecoder->ReadHeader();

					if (mDiscardedBytes) {
						mFillSamples = (uint32)VDRoundToInt64((double)mDiscardedBytes * mDstToSrcDataRatio * 0.5);
						mFillSamples -= mFillSamples % mDstFormat->mChannels;
						mDiscardedBytes = 0;
					}

				} catch(int) {
					mDiscardedBytes += 4;
					break;
				}

				if (mFillSamples)
					mState = kStateFill;
				else
					mState = kStateData;
			} else
				break;
		} else if (mState == kStateFill) {
			if (!mFillSamples) {
				mState = kStateData;
				didSomething = true;
				continue;
			}

			int actual;
			sint16 *dst = mOutputBuffer.LockWrite(VDClampToSint32(mFillSamples), actual);

			if (!actual)
				break;

			didSomething = true;
			memset(dst, 0, sizeof(dst[0]) * actual);
			mOutputBuffer.UnlockWrite(actual);
			mFillSamples -= actual;
		} else {
			// check that we have enough room for the destination frame
			if (mOutputBuffer.getSpace() < (int)mSamplesInNextFrame)
				break;

			// check that we have enough source bytes to feed the decoder
			uint32 dataSize = mpDecoder->GetFrameDataSize();

			if (mInputBufferWritePt - mInputBufferReadPt < (int)dataSize)
				break;

			// decode the frame
			didSomething = true;

			mpDecoder->SetDestination(mDecodeBuffer);

			int samples = 0;
			int samplesToFill = (int)mSamplesInNextFrame;
			uint32 saveReadPt = mInputBufferReadPt;
			try {
				if (mpDecoder->DecodeFrame()) {
					samples = mpDecoder->GetSampleCount();
				} else {
					// Nothing wrong with the frame, but not enough bits in the bit reservoir to decode it.
					// Just conceal the frame, but don't discard bits.
					mpDecoder->ConcealFrame();
				}
			} catch(int) {
				mpDecoder->ConcealFrame();

				// rewind read
				mInputBufferReadPt = saveReadPt;

				// mark header for bad frame as discarded
				mDiscardedBytes += 4;

				// don't output samples
				samplesToFill = 0;
			}

			if (samples < samplesToFill)
				memset(mDecodeBuffer + samples, 0, (samplesToFill - samples)*sizeof(sint16));

			mOutputBuffer.Write(mDecodeBuffer, samplesToFill);

			mState = kStateHeader;
		}
	}

	if (!didSomething && flush && mInputBufferWritePt == mInputBufferReadPt && mOutputBuffer.empty())
		mbEnded = true;

	if (mInputBufferReadPt >= mInputBufferHighWatermark) {
		uint8 *p = mInputBuffer.data();
		memmove(p, p + mInputBufferReadPt, mInputBufferWritePt - mInputBufferReadPt);
		mInputBufferWritePt -= mInputBufferReadPt;
		mInputBufferReadPt = 0;
	}

	return didSomething;
}

void *VDAudioDecompressorMPEG::LockInputBuffer(unsigned& bytes) {
	bytes = mInputBuffer.size() - mInputBufferWritePt;
	return mInputBuffer.data() + mInputBufferWritePt;
}

void VDAudioDecompressorMPEG::UnlockInputBuffer(unsigned bytes) {
	mInputBufferWritePt += bytes;
	VDASSERT(mInputBufferWritePt <= mInputBuffer.size());
}

const void *VDAudioDecompressorMPEG::LockOutputBuffer(unsigned& bytes) {
	int actual;
	const sint16 *p = mOutputBuffer.LockReadAll(actual);
	bytes = actual << 1;
	return p;
}

void VDAudioDecompressorMPEG::UnlockOutputBuffer(unsigned bytes) {
	VDASSERT(!(bytes & 1));
	mOutputBuffer.UnlockRead(bytes >> 1);
}

unsigned VDAudioDecompressorMPEG::CopyOutput(void *dst, unsigned bytes) {
	uint32 samples = bytes >> 1;
	if (!dst) {
		int actual;
		mOutputBuffer.LockRead(samples, actual);
		mOutputBuffer.UnlockRead(actual);
		return actual * 2;
	}

	return mOutputBuffer.Read((sint16 *)dst, samples) * 2;
}

int VDAudioDecompressorMPEG::read(void *buffer, int bytes) {
	int maxread = mInputBufferWritePt - mInputBufferReadPt;

	if (bytes > maxread)
		bytes = maxread;

	if (bytes) {
		memcpy(buffer, mInputBuffer.data() + mInputBufferReadPt, bytes);
		mInputBufferReadPt += bytes;
	}

	return bytes;
}
