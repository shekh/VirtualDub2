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
#include <vd2/system/Error.h>

#include "filter.h"
#include "af_base.h"

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterMix : public VDAudioFilterBase {
public:
	VDAudioFilterMix();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
};

VDAudioFilterMix::VDAudioFilterMix()
{
}

void __cdecl VDAudioFilterMix::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterMix;
}

uint32 VDAudioFilterMix::Prepare() {
	const VDXWaveFormat& format0 = *mpContext->mpInputs[0]->mpFormat;
	const VDXWaveFormat& format1 = *mpContext->mpInputs[1]->mpFormat;

	if (   format0.mTag != VDXWaveFormat::kTagPCM
		|| format1.mTag != VDXWaveFormat::kTagPCM
		|| format0.mSamplingRate != format1.mSamplingRate
		|| format0.mSampleBits != format1.mSampleBits
		|| format0.mChannels != format1.mChannels
		|| (format0.mSampleBits != 8 && format0.mSampleBits != 16)
		)
		return kVFAPrepare_BadFormat;

	VDXWaveFormat *pwf0;

	if (!(mpContext->mpOutputs[0]->mpFormat = pwf0 = mpContext->mpAudioCallbacks->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat))) {
		mpContext->mpServices->SetErrorOutOfMemory();
		return 0;
	}

	pwf0->mChannels		= format0.mChannels;
	pwf0->mSampleBits	= 16;
	pwf0->mBlockSize	= (uint16)(2 * pwf0->mChannels);
	pwf0->mDataRate		= pwf0->mBlockSize * pwf0->mSamplingRate;

	return 0;
}

uint32 VDAudioFilterMix::Run() {
	VDAudioFilterPin& pin1 = *mpContext->mpInputs[0];
	VDAudioFilterPin& pin2 = *mpContext->mpInputs[1];
	const VDXWaveFormat& format1 = *pin1.mpFormat;

	int samples = mpContext->mCommonSamples, actual = 0;

	sint16 *dst = (sint16 *)mpContext->mpOutputs[0]->mpBuffer;

	if (!samples && pin1.mbEnded && pin2.mbEnded)
		return kVFARun_Finished;

	while(samples > 0) {
		sint16 buf[4096];
		int tc = std::min<int>(samples, 4096 / format1.mChannels);

		int tca0 = mpContext->mpInputs[0]->Read(dst, tc, true, kVFARead_PCM16);
		int tca1 = mpContext->mpInputs[1]->Read(buf, tc, true, kVFARead_PCM16);

		VDASSERT(tc == tca0 && tc == tca1);

		int elements = tc * format1.mChannels;

		for(unsigned i=0; i<elements; ++i) {
			const sint32 t = buf[i];
			sint32 t0 = dst[i] + t + 0x8000;

			if ((uint32)t0 >= 0x10000)
				t0 = ~t0 >> 31;

			dst[i] = (sint16)(t0 - 0x8000);
		}

		dst += elements;

		actual += tc;
		samples -= tc;
	}

	mpContext->mpOutputs[0]->mSamplesWritten = actual;

	return 0;
}

extern const struct VDAudioFilterDefinition afilterDef_mix = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterMix),	2, 1,

	NULL,

	VDAudioFilterMix::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const VDPluginInfo apluginDef_mix = {
	sizeof(VDPluginInfo),
	L"mix",
	NULL,
	L"Mixes two streams together linearly.",
	0,
	kVDXPluginType_Audio,
	0,

	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_mix
};
