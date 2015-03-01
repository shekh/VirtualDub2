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

#include <vd2/system/Error.h>

#include "filter.h"
#include "af_base.h"

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterCenterMix : public VDAudioFilterBase {
public:
	VDAudioFilterCenterMix();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
};

VDAudioFilterCenterMix::VDAudioFilterCenterMix()
{
}

void __cdecl VDAudioFilterCenterMix::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterCenterMix;
}

uint32 VDAudioFilterCenterMix::Prepare() {
	const VDXWaveFormat& format0 = *mpContext->mpInputs[0]->mpFormat;
	const VDXWaveFormat& format1 = *mpContext->mpInputs[1]->mpFormat;

	if (   format0.mTag != VDXWaveFormat::kTagPCM
		|| format0.mChannels != 2
		|| (format0.mSampleBits != 8 && format0.mSampleBits != 16)
		|| format1.mTag != VDXWaveFormat::kTagPCM
		|| format1.mChannels != 1
		|| (format1.mSampleBits != 8 && format0.mSampleBits != 16)
		|| format0.mSamplingRate != format1.mSamplingRate
		|| format0.mSampleBits != format1.mSampleBits
		)
		return kVFAPrepare_BadFormat;

	VDXWaveFormat *pwf0;

	if (!(mpContext->mpOutputs[0]->mpFormat = pwf0 = mpContext->mpAudioCallbacks->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat))) {
		mpContext->mpServices->SetErrorOutOfMemory();
		return 0;
	}

	pwf0->mChannels		= 2;
	pwf0->mSampleBits	= 16;
	pwf0->mBlockSize	= 4;
	pwf0->mDataRate		= 4 * pwf0->mSamplingRate;

	return 0;
}

uint32 VDAudioFilterCenterMix::Run() {
	VDAudioFilterPin& pin1 = *mpContext->mpInputs[0];
	VDAudioFilterPin& pin2 = *mpContext->mpInputs[1];

	int samples = mpContext->mCommonSamples, actual = 0;

	if (!samples && pin1.mbEnded && pin2.mbEnded)
		return kVFARun_Finished;

	sint16 *dst = (sint16 *)mpContext->mpOutputs[0]->mpBuffer;

	while(samples > 0) {
		sint16 buf[4096];
		int tc = std::min<int>(samples, 2048);		// 4096 / 2 channels

		int tca0 = mpContext->mpInputs[0]->Read(dst, tc, true, kVFARead_PCM16);
		int tca1 = mpContext->mpInputs[1]->Read(buf, tc, true, kVFARead_PCM16);

		VDASSERT(tc == tca0 && tc == tca1);

		for(unsigned i=0; i<tc; ++i) {
			const sint32 t = buf[i];
			sint32 t0 = dst[0] + t + 0x8000;
			sint32 t1 = dst[1] + t + 0x8000;

			if ((uint32)t0 >= 0x10000)
				t0 = ~t0 >> 31;

			if ((uint32)t1 >= 0x10000)
				t1 = ~t1 >> 31;

			dst[0] = (sint16)(t0 - 0x8000);
			dst[1] = (sint16)(t1 - 0x8000);
			dst += 2;
		}

		actual += tc;
		samples -= tc;
	}

	mpContext->mpOutputs[0]->mSamplesWritten = actual;

	return 0;
}

extern const struct VDAudioFilterDefinition afilterDef_centermix = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterCenterMix),	2, 1,

	NULL,

	VDAudioFilterCenterMix::InitProc,
	&VDAudioFilterBase::sVtbl,
};


extern const VDPluginInfo apluginDef_centermix = {
	sizeof(VDPluginInfo),
	L"center mix",
	NULL,
	L"Mixes a stereo stream with a mono stream.",
	0,
	kVDXPluginType_Audio,
	0,

	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_centermix
};
