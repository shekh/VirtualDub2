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
#include <vd2/system/VDRingBuffer.h>
#include "af_base.h"
#include "gui.h"
#include "resource.h"

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterStereoChorus : public VDAudioFilterBase {
public:
	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	VDAudioFilterStereoChorus();
	~VDAudioFilterStereoChorus();

	uint32 Prepare();
	void Start();
	uint32 Run();

protected:
	std::vector<sint16>			mDelayBuffer;
	uint32						mDelayBufferSize;
	uint32						mDelayMask;
	uint32						mDelayPos;
	uint32						mLFOPos;
};

void __cdecl VDAudioFilterStereoChorus::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterStereoChorus;
}

VDAudioFilterStereoChorus::VDAudioFilterStereoChorus() {
}

VDAudioFilterStereoChorus::~VDAudioFilterStereoChorus() {
}

uint32 VDAudioFilterStereoChorus::Prepare() {
	const VDXWaveFormat& inFormat = *mpContext->mpInputs[0]->mpFormat;

	if (   inFormat.mTag != VDXWaveFormat::kTagPCM
		|| (inFormat.mSampleBits != 8 && inFormat.mSampleBits != 16)
		|| (inFormat.mChannels > 2)
		)
		return kVFAPrepare_BadFormat;

	VDXWaveFormat *pwf = mpContext->mpAudioCallbacks->CopyWaveFormat(&inFormat);

	if (!pwf) {
		mpContext->mpServices->SetErrorOutOfMemory();
		return 0;
	}

	mpContext->mpOutputs[0]->mpFormat = pwf;

	pwf->mSampleBits	= 16;
	pwf->mChannels		= 2;
	pwf->mBlockSize		= (uint16)(2 * pwf->mChannels);
	pwf->mDataRate		= pwf->mSamplingRate * pwf->mBlockSize;

	return 0;
}

void VDAudioFilterStereoChorus::Start() {
	const VDAudioFilterPin& pin = *mpContext->mpOutputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;

	// Allocate two 30ms buffers

	mDelayBufferSize = 2*((format.mSamplingRate * 30) / 1000 + 16) - 1;

	while(uint32 tmp = mDelayBufferSize & (mDelayBufferSize-1))
		mDelayBufferSize = tmp;

	mDelayMask = mDelayBufferSize - 1;
	mDelayBufferSize += 16;

	mDelayPos = 0;
	mLFOPos = 0;

	mDelayBuffer.resize(mDelayBufferSize * 2);
}

uint32 VDAudioFilterStereoChorus::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;

	// foo
	sint16 buf16[4096];

	// compute output samples
	int samples = std::min<int>(mpContext->mCommonSamples, 4096 / format.mChannels);
	sint16 *dst = (sint16 *)mpContext->mpOutputs[0]->mpBuffer;
	
	if (!samples) {
		if (pin.mbEnded && !mpContext->mInputSamples)
			return kVFARun_Finished;

		return 0;
	}

	// read buffer

	int actual_samples = mpContext->mpInputs[0]->Read(buf16, samples, false, kVFARead_PCM16);
	VDASSERT(actual_samples == samples);

	// apply filter

	const int step = format.mChannels;

	sint16 delaycen = (sint16)(format.mSamplingRate * 25 / 1000 * 256);
	sint16 delayamp = (sint16)(format.mSamplingRate * 1 / 1000 * 256);

	sint32 lforate = (sint32)(4294967296.0 * 0.3 / format.mSamplingRate);

	for(int ch=0; ch<2; ++ch) {
		const sint16 *src = &buf16[format.mChannels>1 ? ch : 0];
		sint16 *delay = &mDelayBuffer[mDelayBufferSize * ch];
		sint16 *out = dst + ch;
		uint32 delaypos = mDelayPos;
		sint32 lfopos = mLFOPos;

		if (ch)
			lfopos += 0x40000000;

		for(uint32 idx=0; idx<samples; ++idx) {
			sint32 delval = (sint32)(delaycen + delayamp * sin(lfopos * (3.1415926535 / 2147483648.0)));
			sint32 idelval = delval >> 8;
			sint32 fdelval = delval & 255;
			sint32 v = *src;

			delay[delaypos] = (sint16)v;

			sint32 x1 = delay[(delaypos - idelval) & mDelayMask];
			sint32 x2 = delay[(delaypos - idelval + 1) & mDelayMask];

			v += (x1 + (((x2-x1)*fdelval+128)>>8)) >> 1;

			if ((uint32)(v+0x8000) >= 0x10000)
				v = (~v >> 31) - 0x8000;

			*out = (sint16)v;

			src += step;
			out += 2;

			delaypos = (delaypos + 1) & mDelayMask;
			lfopos += lforate;
		}

		if (ch == 1) {
			mDelayPos = delaypos;
			mLFOPos = lfopos - 0x40000000;
		}
	}

	mpContext->mpOutputs[0]->mSamplesWritten = samples;

	return 0;
}

extern const struct VDAudioFilterDefinition afilterDef_stereochorus = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterStereoChorus),	1,	1,

	NULL,

	VDAudioFilterStereoChorus::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const VDPluginInfo apluginDef_stereochorus = {
	sizeof(VDPluginInfo),
	L"stereo chorus",
	NULL,
	L"Applies feedback to a stream using delays driven from a quadrature-phase LFO to simulate or enhance stereo.",
	0,
	kVDXPluginType_Audio,
	0,

	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_stereochorus
};
