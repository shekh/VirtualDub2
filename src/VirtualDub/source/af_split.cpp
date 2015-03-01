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
#include <vd2/system/strutil.h>
#include <vd2/system/fraction.h>

#include "filter.h"
#include "af_base.h"

class VDAudioFilterSplit : public VDAudioFilterBase {
public:
	VDAudioFilterSplit();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
};

VDAudioFilterSplit::VDAudioFilterSplit()
{
}

void __cdecl VDAudioFilterSplit::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterSplit;
}

uint32 VDAudioFilterSplit::Prepare() {
	VDXWaveFormat *pwf0, *pwf1;

	if (!(mpContext->mpOutputs[0]->mpFormat = pwf0 = mpContext->mpAudioCallbacks->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat))) {
		mpContext->mpServices->SetErrorOutOfMemory();
		return 0;
	}
	if (!(mpContext->mpOutputs[1]->mpFormat = pwf1 = mpContext->mpAudioCallbacks->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat))) {
		mpContext->mpServices->SetErrorOutOfMemory();
		return 0;
	}
	return 0;
}

uint32 VDAudioFilterSplit::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];

	int samples = mpContext->mCommonSamples, actual = 0;
	void *dst1 = mpContext->mpOutputs[0]->mpBuffer;
	void *dst2 = mpContext->mpOutputs[1]->mpBuffer;

	actual = mpContext->mpInputs[0]->Read(dst1, samples, false, kVFARead_Native);

	memcpy(dst2, dst1, actual * mpContext->mpOutputs[0]->mpFormat->mBlockSize);

	mpContext->mpOutputs[0]->mSamplesWritten = actual;
	mpContext->mpOutputs[1]->mSamplesWritten = actual;

	return !actual && pin.mbEnded ? kVFARun_Finished : 0;
}

extern const struct VDAudioFilterDefinition afilterDef_split = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterSplit),	1,	2,

	NULL,

	VDAudioFilterSplit::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const VDPluginInfo apluginDef_split = {
	sizeof(VDPluginInfo),
	L"split",
	NULL,
	L"Splits an audio stream into two identical outputs.",
	0,
	kVDXPluginType_Audio,
	0,

	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_split
};
