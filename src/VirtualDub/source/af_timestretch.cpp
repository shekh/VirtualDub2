//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2006 Avery Lee
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
#include <vd2/system/VDRingBuffer.h>
#include <vd2/system/vdstl.h>
#include <vd2/VDLib/dialog.h>
#include <vd2/VDLib/fft.h>
#include "af_base.h"
#include "resource.h"

#ifdef _MSC_VER
	#pragma warning(disable: 4324)		// warning C4324: 'VDAudioFilterTimeStretch' : structure was padded due to __declspec(align())
#endif

///////////////////////////////////////////////////////////////////////////

VDAFBASE_BEGIN_CONFIG(TimeStretch);
VDAFBASE_CONFIG_ENTRY(TimeStretch, 0, Double, ratio, L"Pitch ratio", L"Factor by which to multiply pitch of sound.");
VDAFBASE_END_CONFIG(TimeStretch, 0);

typedef VDAudioFilterData_TimeStretch VDAudioFilterTimeStretchConfig;

class VDDialogAudioFilterTimeStretchConfig : public VDDialogFrameW32 {
public:
	VDDialogAudioFilterTimeStretchConfig(VDAudioFilterTimeStretchConfig& config) : VDDialogFrameW32(IDD_AF_TIMESTRETCH), mConfig(config) {}

	bool Activate(VDGUIHandle hParent) {
		return 0 != ShowDialog(hParent);
	}

	void OnDataExchange(bool write) {
		ExchangeControlValueDouble(write, IDC_RATIO, L"%.12g", mConfig.ratio, 0.5, 2.0);
	}

	VDAudioFilterTimeStretchConfig& mConfig;
};

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterTimeStretch : public VDAudioFilterBase {
public:
	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	VDAudioFilterTimeStretch();
	~VDAudioFilterTimeStretch();

	uint32 Prepare();
	void Start();
	uint32 Run();
	sint64 Seek(sint64 microsecs);

	void *GetConfigPtr() { return &mConfig; }

	bool Config(HWND hwnd) {
		VDAudioFilterTimeStretchConfig	config(mConfig);

		if (!hwnd)
			return true;

		if (VDDialogAudioFilterTimeStretchConfig(config).Activate((VDGUIHandle)hwnd)) {
			mConfig = config;
			return true;
		}
		return false;
	}

protected:
	enum { kDelayLineSize = 16384 };
	enum { kDelayLineMask = kDelayLineSize - 1 };
	enum { kWindowSize = 2048 };
	enum { kHalfWindowSize = kWindowSize >> 1 };
	enum { kHalfWindowMask = kHalfWindowSize - 1 };
	enum { kOverlapTestSize = kWindowSize >> 1 };
	enum { kFFTSize = kWindowSize * 2 };
	enum { kFFTSizeBits = 12 };

	void ProcessBlock();
	void CopyOutputSamples(void *dst, int count);
	
	VDAudioFilterTimeStretchConfig	mConfig;
	vdfastvector<sint16>			mDelayLineBuffers;
	vdfastvector<sint16>			mInputBuffer;
	vdfastvector<float>				mFFTBuffers;
	uint32 mRateLo;
	uint32 mRateHi;
	uint32 mRateAccum;
	uint32 mOutputSamplePos;
	uint32 mOutputSamplesLeft;

	uint32 mSrcOffset1;
	uint32 mSrcOffset2;
	uint32 mDstOffset;
	uint32 mInputDelay;
	uint32 mActiveInputDelay;

	struct ChannelInfo {
		sint16	*mpDelayLine;
	};

	vdfastvector<ChannelInfo>	mChannels;
	vdfastvector<sint16>		mWindow;

	float	mOverlapWindow[kOverlapTestSize];

	uint32	mFFTRevBits[kFFTSize];
	__declspec(align(16)) float	mFFTBuffer1[kFFTSize];
	__declspec(align(16)) float	mFFTBuffer2[kFFTSize];
	__declspec(align(16)) float	mFFTBuffer3[kFFTSize];
};

void __cdecl VDAudioFilterTimeStretch::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterTimeStretch;
}

VDAudioFilterTimeStretch::VDAudioFilterTimeStretch() {
	mConfig.ratio = 1.0;
}

VDAudioFilterTimeStretch::~VDAudioFilterTimeStretch() {
}

uint32 VDAudioFilterTimeStretch::Prepare() {
	VDAudioFilterPin& inPin = *mpContext->mpInputs[0];
	const VDXWaveFormat& inFormat = *inPin.mpFormat;

	inPin.mGranularity = kHalfWindowSize;

	if (   inFormat.mTag != VDXWaveFormat::kTagPCM
		|| (inFormat.mSampleBits != 8 && inFormat.mSampleBits != 16)
		)
		return kVFAPrepare_BadFormat;

	VDXWaveFormat *pwf = mpContext->mpAudioCallbacks->CopyWaveFormat(&inFormat);

	if (!pwf) {
		mpContext->mpServices->SetErrorOutOfMemory();
		return 0;
	}

	mpContext->mpOutputs[0]->mpFormat = pwf;

	pwf->mSampleBits	= 16;
	pwf->mBlockSize		= (uint16)(2 * pwf->mChannels);
	pwf->mDataRate		= pwf->mSamplingRate * pwf->mBlockSize;

	return 0;
}

void VDAudioFilterTimeStretch::Start() {
	const VDAudioFilterPin& pin = *mpContext->mpOutputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;
	const uint32 nch = format.mChannels;

	// initialize FFT window and overlap window
	for(int i=0; i<kOverlapTestSize; ++i) {
		mOverlapWindow[i] = (float)sin(i * (3.1415926535/(double)kOverlapTestSize));
	}

	uint64 rate = VDRoundToInt64(0x100000000LL * kHalfWindowSize * mConfig.ratio);
	mRateLo = (uint32)rate;
	mRateHi = (uint32)(rate >> 32);
	mRateAccum = 0;
	mOutputSamplesLeft = 0;
	mOutputSamplePos = 0;

	int winsize = mRateHi + 1;
	mWindow.resize(winsize);
	for(int i=0; i<winsize; ++i) {
		mWindow[i] = (sint16)VDRoundToInt(16384.0 * (0.5 - 0.5*cos(i * (3.1415926535/(double)winsize))));
	}

	mInputDelay = (kWindowSize*5)>>2;

	mSrcOffset1 = 0;
	mSrcOffset2 = 0;
	mDstOffset = mInputDelay;
	mActiveInputDelay = mInputDelay;

	mChannels.resize(nch);
	mInputBuffer.resize(nch * kHalfWindowSize);
	mDelayLineBuffers.resize(kDelayLineSize * nch, 0);

	sint16 *pDelaySpace = mDelayLineBuffers.data();

	for(uint32 i=0; i<nch; ++i) {
		ChannelInfo& chanInfo = mChannels[i];

		chanInfo.mpDelayLine = pDelaySpace;
		pDelaySpace += kDelayLineSize;
	}

	VDMakePermuteTable(mFFTRevBits, kFFTSizeBits - 1);
}

uint32 VDAudioFilterTimeStretch::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;
	bool activity = false;

	// compute output samples
	int samples = mpContext->mCommonSamples;

	if (samples > mOutputSamplesLeft)
		samples = mOutputSamplesLeft;

	if (samples) {
		CopyOutputSamples(mpContext->mpOutputs[0]->mpBuffer, samples);
		activity = true;
	}

	mpContext->mpOutputs[0]->mSamplesWritten = samples;

	if (!mOutputSamplesLeft) {
		// read from input
		const uint32 nch = format.mChannels;
		if (pin.mCurrentLevel >= kHalfWindowSize) {
			int actual_samples = pin.Read(mInputBuffer.data(), kHalfWindowSize, false, kVFARead_PCM16);
			VDASSERT(actual_samples == kHalfWindowSize);

			// deinterleave from input into delay lines
			const sint16 *src = mInputBuffer.data();
			for(unsigned ch=0; ch<nch; ++ch) {
				ChannelInfo& chanInfo = mChannels[ch];
				const sint16 *src2 = src + ch;
				sint16 *delay = chanInfo.mpDelayLine;

				// write new samples
				for(int i=0; i<kHalfWindowSize; ++i) {
					delay[(mDstOffset + i) & kDelayLineMask] = *src2;
					src2 += nch;
				}
			}

			mDstOffset += kHalfWindowSize;

			// Process a block.
			ProcessBlock();

			mOutputSamplePos = 0;
			mOutputSamplesLeft = mRateHi;
			mRateAccum += mRateLo;
			if (mRateAccum < mRateLo)
				++mOutputSamplesLeft;

			activity = true;
		}
	}

	if (!activity) {
		if (pin.mbEnded && !mpContext->mInputSamples)
			return kVFARun_Finished;
	}

	return 0;
}

sint64 VDAudioFilterTimeStretch::Seek(sint64 microsecs) {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;
	const uint32 nch = format.mChannels;

	for(unsigned ch=0; ch<nch; ++ch) {
		ChannelInfo& chanInfo = mChannels[ch];

		memset(chanInfo.mpDelayLine, 0, sizeof(sint16)*kDelayLineSize);
	}

	mSrcOffset1 = 0;
	mSrcOffset2 = 0;
	mDstOffset = mInputDelay;
	mActiveInputDelay = mInputDelay;

	return VDRoundToInt64((double)microsecs / mConfig.ratio);
}

void VDAudioFilterTimeStretch::ProcessBlock() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;
	const uint32 nch = format.mChannels;
	int i;
	uint32 currentBase = mSrcOffset2;
//	uint32 base = mDstOffset - ((kWindowSize*3)>>1);
	uint32 base = mDstOffset - kHalfWindowSize - (mRateHi + 1);

	// compute correlations
	for(unsigned ch=0; ch<nch; ++ch) {
		ChannelInfo& chanInfo = mChannels[ch];

		// compute FFT of search pattern
		for(i=0; i<kOverlapTestSize; ++i)
			mFFTBuffer1[i] = chanInfo.mpDelayLine[(currentBase + i) & (kDelayLineSize - 1)] * mOverlapWindow[i];

		memset(mFFTBuffer1 + kOverlapTestSize, 0, sizeof(float) * (kFFTSize - kOverlapTestSize));
		VDPermuteRevBitsComplex(mFFTBuffer1, kFFTSizeBits - 1, mFFTRevBits);
		VDComputeRealFFT(mFFTBuffer1, kFFTSizeBits);

		// compute FFT of search area
		for(i=0; i<((kWindowSize*3)>>1); ++i)
			mFFTBuffer2[i] = chanInfo.mpDelayLine[(base + i) & (kDelayLineSize - 1)];

		memset(mFFTBuffer2 + ((kWindowSize*3)>>1), 0, sizeof(float) * (kFFTSize - ((3*kWindowSize)>>1)));

		VDPermuteRevBitsComplex(mFFTBuffer2, kFFTSizeBits - 1, mFFTRevBits);
		VDComputeRealFFT(mFFTBuffer2, kFFTSizeBits);

		if (!ch) {
			mFFTBuffer3[0] = mFFTBuffer1[0]*mFFTBuffer2[0];
			mFFTBuffer3[1] = mFFTBuffer1[1]*mFFTBuffer2[1];
			for(i=2; i<kFFTSize; i+=2) {
				float r1 = mFFTBuffer1[i];
				float i1 = mFFTBuffer1[i+1];
				float r2 = mFFTBuffer2[i];
				float i2 = mFFTBuffer2[i+1];

				// (A-Bi)*(C+Di) = (AC+BD)+(AD-BC)i
				mFFTBuffer3[i  ] = r1*r2 + i1*i2;
				mFFTBuffer3[i+1] = r1*i2 - i1*r2;
			}
		} else {
			mFFTBuffer3[0] += mFFTBuffer1[0]*mFFTBuffer2[0];
			mFFTBuffer3[1] += mFFTBuffer1[1]*mFFTBuffer2[1];
			for(i=2; i<kFFTSize; i+=2) {
				float r1 = mFFTBuffer1[i];
				float i1 = mFFTBuffer1[i+1];
				float r2 = mFFTBuffer2[i];
				float i2 = mFFTBuffer2[i+1];

				// (A-Bi)*(C+Di) = (AC+BD)+(AD-BC)i
				mFFTBuffer3[i  ] += r1*r2 + i1*i2;
				mFFTBuffer3[i+1] += r1*i2 - i1*r2;
			}
		}
	}

	VDComputeRealIFFT(mFFTBuffer3, kFFTSizeBits);
	VDPermuteRevBitsComplex(mFFTBuffer3, kFFTSizeBits - 1, mFFTRevBits);

	int maxpos = kWindowSize >> 2;
	for(i=0; i<=(kWindowSize>>1); ++i) {
		if (mFFTBuffer3[i] > mFFTBuffer3[maxpos])
			maxpos = i;
	}

	mSrcOffset1 = mSrcOffset2;
	mSrcOffset2 = base + maxpos;
}

void VDAudioFilterTimeStretch::CopyOutputSamples(void *dst, int count) {
	VDASSERT(count <= mOutputSamplesLeft);
	mOutputSamplesLeft -= count;

	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;
	const uint32 nch = format.mChannels;

	for(unsigned ch=0; ch<nch; ++ch) {
		ChannelInfo& chanInfo = mChannels[ch];
		sint16 *delay = chanInfo.mpDelayLine;
		sint16 *dst2 = (sint16 *)dst + ch;
		const sint16 *win = mWindow.data() + mOutputSamplePos;

		// scan out old samples
		uint32 srcOffset1 = mSrcOffset1;
		uint32 srcOffset2 = mSrcOffset2;
		for(int i=0; i<count; ++i) {
			sint16 src1 = delay[srcOffset1 & kDelayLineMask];
			sint16 src2 = delay[srcOffset2 & kDelayLineMask];
			*dst2 = (sint16)(src1 + ((((sint32)src2 - (sint32)src1)*(*win++) + 8192) >> 14));
			dst2 += nch;

			++srcOffset1;
			++srcOffset2;
		}

		if (ch == nch-1) {
			mSrcOffset1 = srcOffset1;
			mSrcOffset2 = srcOffset2;
		}
	}

	mOutputSamplePos += count;
}

///////////////////////////////////////////////////////////////////////////////

extern const struct VDAudioFilterDefinition afilterDef_timestretch = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_HasConfig,

	sizeof(VDAudioFilterTimeStretch),	1,	1,

	&VDAudioFilterData_TimeStretch::members.info,

	VDAudioFilterTimeStretch::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const VDPluginInfo apluginDef_timestretch = {
	sizeof(VDPluginInfo),
	L"time stretch",
	NULL,
	L"Stretches audio in time without changing pitch.",
	0,
	kVDXPluginType_Audio,
	0,

	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_timestretch
};
