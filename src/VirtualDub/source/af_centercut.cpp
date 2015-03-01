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

#include "stdafx.h"
#include <math.h>
#include <vd2/VDLib/fft.h>
#include "af_base.h"

#ifdef _MSC_VER
	#pragma warning(disable: 4324)		// warning C4324: 'VDAudioFilterCenterCut' : structure was padded due to __declspec(align())
#endif

class VDAudioFilterCenterCut : public VDAudioFilterBase {
public:
	VDAudioFilterCenterCut();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
	void Start();

	sint64 Seek(sint64);

protected:
	enum {
		kWindowSizeBits	= 13,
		kWindowSize		= 8192,
		kSubWindow		= 1024
	};

	uint32		mInputSamplesNeeded;
	uint32		mInputPos;

	VDRealFFT	mFFT;
	float		mInputWindow[kWindowSize];
	float		mOutputWindow[kWindowSize];
	float		mInput[kWindowSize][2];
	float		mOverlapC[8][kSubWindow];
	__declspec(align(16)) float		mTempLBuffer[kWindowSize];
	__declspec(align(16)) float		mTempRBuffer[kWindowSize];
	__declspec(align(16)) float		mTempCBuffer[kWindowSize];
};

VDAudioFilterCenterCut::VDAudioFilterCenterCut()
{
}

void __cdecl VDAudioFilterCenterCut::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterCenterCut;
}

uint32 VDAudioFilterCenterCut::Prepare() {
	const VDXWaveFormat& format0 = *mpContext->mpInputs[0]->mpFormat;

	if (   format0.mTag != VDXWaveFormat::kTagPCM
		|| format0.mChannels != 2
		|| (format0.mSampleBits != 8 && format0.mSampleBits != 16)
		)
		return kVFAPrepare_BadFormat;

	mpContext->mpInputs[0]->mGranularity	= kSubWindow;
	mpContext->mpInputs[0]->mDelay			= (sint32)ceil((kSubWindow*3000000.0)/format0.mSamplingRate);
	mpContext->mpOutputs[0]->mGranularity	= kSubWindow;
	mpContext->mpOutputs[1]->mGranularity	= kSubWindow;

	VDXWaveFormat *pwf0, *pwf1;

	if (!(mpContext->mpOutputs[0]->mpFormat = pwf0 = mpContext->mpAudioCallbacks->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat))) {
		mpContext->mpServices->SetErrorOutOfMemory();
		return 0;
	}

	if (!(mpContext->mpOutputs[1]->mpFormat = pwf1 = mpContext->mpAudioCallbacks->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat))) {
		mpContext->mpServices->SetErrorOutOfMemory();
		return 0;
	}

	pwf0->mChannels		= 2;
	pwf0->mSampleBits	= 16;
	pwf0->mBlockSize	= 4;
	pwf0->mDataRate		= pwf1->mSamplingRate * 4;

	pwf1->mChannels		= 1;
	pwf1->mSampleBits	= 16;
	pwf1->mBlockSize	= 2;
	pwf1->mDataRate		= pwf1->mSamplingRate * 2;

	return 0;
}

void VDAudioFilterCenterCut::Start() {
	mFFT.Init(kWindowSizeBits);

	mInputSamplesNeeded = kWindowSize;
	mInputPos = 0;

	memset(mOverlapC, 0, sizeof mOverlapC);

	VDCreateRaisedCosineWindow(mInputWindow, kWindowSize);
	for(unsigned i=0; i<kWindowSize; ++i) {
		mOutputWindow[i] = mInputWindow[i] * (mInputWindow[i]*kWindowSize) * 0.5f * (8.0f / 5.0f) * kWindowSize;
	}
}

uint32 VDAudioFilterCenterCut::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;

	unsigned i;

	if (!mpContext->mOutputGranules)
		return 0;

	sint16 *dst1 = (sint16 *)mpContext->mpOutputs[0]->mpBuffer;
	sint16 *dst2 = (sint16 *)mpContext->mpOutputs[1]->mpBuffer;

	// fill up the input window

	int actual = 0;
	while(mInputSamplesNeeded > 0) {
		sint16 buf[4096];
		int tc = mpContext->mpInputs[0]->Read(&buf, std::min<int>(mInputSamplesNeeded, 4096 / format.mChannels), false, kVFARead_PCM16);

		if (tc<=0)
			return !actual && pin.mbEnded ? kVFARun_Finished : 0;

		for(i=0; i<tc; ++i) {
			mInput[mInputPos][0] = buf[i*2+0];
			mInput[mInputPos][1] = buf[i*2+1];
			mInputPos = (mInputPos + 1) & (kWindowSize-1);
		}

		actual += tc;
		mInputSamplesNeeded -= tc;
	}

	// copy to temporary buffer and FHT

	for(i=0; i<kWindowSize; ++i) {
		const unsigned j = i;
		const unsigned k = (j + mInputPos) & (kWindowSize-1);
		const float w = mInputWindow[i];

		mTempLBuffer[i] = mInput[k][0] * w;
		mTempRBuffer[i] = mInput[k][1] * w;
	}

	mFFT.ComputeRealFFT(mTempLBuffer);
	mFFT.ComputeRealFFT(mTempRBuffer);

	// perform stereo separation

	mTempCBuffer[0] = 0;
	mTempCBuffer[1] = 0;
	for(i=2; i<kWindowSize; i+=2) {
		float lR = mTempLBuffer[i  ];
		float lI = mTempLBuffer[i+1];
		float rR = mTempRBuffer[i  ];
		float rI = mTempRBuffer[i+1];

		float sumR = lR + rR;
		float sumI = lI + rI;
		float diffR = lR - rR;
		float diffI = lI - rI;

		float sumSq = sumR*sumR + sumI*sumI;
		float diffSq = diffR*diffR + diffI*diffI;
		float alpha = 0;

		if (sumSq > 1e-10F)
			alpha = 0.5f - sqrt(diffSq / sumSq) * 0.5f;

		mTempCBuffer[i  ]	= sumR * alpha;
		mTempCBuffer[i+1]	= sumI * alpha;
	}

	// reconstitute left/right/center channels
	mFFT.ComputeRealIFFT(mTempCBuffer);

	// writeout

	enum {
		M0 = 0,
		M1 = kSubWindow,
		M2 = kSubWindow*2,
		M3 = kSubWindow*3,
		M4 = kSubWindow*4,
		M5 = kSubWindow*5,
		M6 = kSubWindow*6,
		M7 = kSubWindow*7
	};

	struct local {
		static sint16 float_to_sint16_clip(float f) {
			f += (12582912.0f + 32768.0f);			// 2^23 + 2^22 + 32K
			int v = reinterpret_cast<int&>(f) - 0x4B400000;

			if ((unsigned)v >= 0x10000)
				v = ~v >> 31;

			return (sint16)(v - 0x8000);
		}
	};

	for(i=0; i<kWindowSize; ++i) {
		mTempCBuffer[i] *= mOutputWindow[i];
	}

	for(i=0; i<kSubWindow; ++i) {
		float c = mOverlapC [0][i]    + mTempCBuffer[i+M0];
		float l = mInput[mInputPos+i][0] - c;
		float r = mInput[mInputPos+i][1] - c;

		dst1[0] = local::float_to_sint16_clip(l);
		dst1[1] = local::float_to_sint16_clip(r);
		dst1 += 2;

		*dst2++ = local::float_to_sint16_clip(c);

		mOverlapC [0][i]    = mOverlapC [1][i]    + mTempCBuffer[i+M1];
		mOverlapC [1][i]    = mOverlapC [2][i]    + mTempCBuffer[i+M2];
		mOverlapC [2][i]    = mOverlapC [3][i]    + mTempCBuffer[i+M3];
		mOverlapC [3][i]    = mOverlapC [4][i]    + mTempCBuffer[i+M4];
		mOverlapC [4][i]    = mOverlapC [5][i]    + mTempCBuffer[i+M5];
		mOverlapC [5][i]    = mOverlapC [6][i]    + mTempCBuffer[i+M6];
		mOverlapC [6][i]    = mTempCBuffer[i+M7];
	}

	mInputSamplesNeeded = kSubWindow;

	mpContext->mpOutputs[0]->mSamplesWritten = kSubWindow;
	mpContext->mpOutputs[1]->mSamplesWritten = kSubWindow;

	return 0;
}

sint64 VDAudioFilterCenterCut::Seek(sint64 us) {
	const VDXWaveFormat& inputFormat = *mpContext->mpInputs[0]->mpFormat;

	return us - ((sint64)1000000 * kWindowSize / inputFormat.mSamplingRate);
}

extern const struct VDAudioFilterDefinition afilterDef_centercut = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterCenterCut),	1,	2,

	NULL,

	VDAudioFilterCenterCut::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const VDPluginInfo apluginDef_centercut = {
	sizeof(VDPluginInfo),
	L"center cut",
	NULL,
	L"Splits a stereo stream into stereo-side and mono-center outputs using phase analysis.",
	0,
	kVDXPluginType_Audio,
	0,

	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_centercut
};
