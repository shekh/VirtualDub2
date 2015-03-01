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
#include "af_base.h"
#include "af_sink.h"

class VDAudioFilterSink : public VDAudioFilterBase, public IVDAudioFilterSink {
public:
	VDAudioFilterSink();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
	void Start();

	uint32 ReadSamples(void *dst, uint32 samples);
	const void *GetFormat();
	int GetFormatLen();
	sint64 GetLength();
	bool IsEnded();

	VDRingBuffer<char> mOutputBuffer;
};

VDAudioFilterSink::VDAudioFilterSink()
{
}

void __cdecl VDAudioFilterSink::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterSink;
}

uint32 VDAudioFilterSink::Prepare() {
	return 0;
}

void VDAudioFilterSink::Start() {
	const VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;

	mOutputBuffer.Init(format.mBlockSize * pin.mBufferSize);
}

uint32 VDAudioFilterSink::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;

	int samples;

	char *dst = mOutputBuffer.LockWrite(mOutputBuffer.getSize(), samples);

	samples /= format.mBlockSize;

	if (!samples)
		return pin.mbEnded ? kVFARun_Finished : 0;

	samples = mpContext->mpInputs[0]->Read(dst, samples, false, kVFARead_Native);

	if (!samples)
		return pin.mbEnded ? kVFARun_Finished : 0;

	mOutputBuffer.UnlockWrite(samples * format.mBlockSize);
	return 0;
}

uint32 VDAudioFilterSink::ReadSamples(void *dst, uint32 samples) {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;

	samples = std::min<uint32>(samples, mOutputBuffer.getLevel() / format.mBlockSize);

	if (dst) {
		mOutputBuffer.Read((char *)dst, samples * format.mBlockSize);
		if (samples > 0)
			mpContext->mpAudioCallbacks->Wake(mpContext);
	}

	return samples;
}

const void *VDAudioFilterSink::GetFormat() {
	return mpContext->mpInputs[0]->mpFormat;
}

int VDAudioFilterSink::GetFormatLen() {
	return sizeof(VDXWaveFormat) + mpContext->mpInputs[0]->mpFormat->mExtraSize;
}

sint64 VDAudioFilterSink::GetLength() {
	return mpContext->mpInputs[0]->mLength;
}

bool VDAudioFilterSink::IsEnded() {
	return mpContext->mpInputs[0]->mbEnded && mOutputBuffer.getLevel()==0;
}

extern const struct VDAudioFilterDefinition afilterDef_sink = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterSink),	1,	0,

	NULL,

	VDAudioFilterSink::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const VDPluginInfo apluginDef_sink = {
	sizeof(VDPluginInfo),
	L"*sink",
	NULL,
	L"",
	0,
	kVDXPluginType_Audio,
	0,

	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_sink
};

IVDAudioFilterSink *VDGetAudioFilterSinkInterface(void *p) {
	return static_cast<IVDAudioFilterSink *>(static_cast<VDAudioFilterSink *>(static_cast<VDAudioFilterBase *>(p)));
}

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterOutput : public VDAudioFilterBase {
public:
	VDAudioFilterOutput();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
	void Start();
};

VDAudioFilterOutput::VDAudioFilterOutput()
{
}

void __cdecl VDAudioFilterOutput::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterOutput;
}

uint32 VDAudioFilterOutput::Prepare() {
	mpContext->mpInputs[0]->mGranularity	= 1;
	return 0;
}

void VDAudioFilterOutput::Start() {
}

uint32 VDAudioFilterOutput::Run() {
	char buf[4096];
	int maxsamp = sizeof buf / mpContext->mpInputs[0]->mpFormat->mBlockSize;
	int actual = 0;

	while(uint32 c = mpContext->mpInputs[0]->Read(buf, maxsamp, false, kVFARead_Native))
		actual += c;

	return !actual && mpContext->mpInputs[0]->mbEnded ? kVFARun_Finished : 0;
}

extern const struct VDAudioFilterDefinition afilterDef_output = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterOutput),	1,	0,

	NULL,

	VDAudioFilterOutput::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const VDPluginInfo apluginDef_output = {
	sizeof(VDPluginInfo),
	L"output",
	NULL,
	L"Generic output sink for audio graph representing file output or playback filter.",
	0,
	kVDXPluginType_Audio,
	0,

	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_output
};

extern const struct VDAudioFilterDefinition afilterDef_discard = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterOutput),	1,	0,

	NULL,

	VDAudioFilterOutput::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const VDPluginInfo apluginDef_discard = {
	sizeof(VDPluginInfo),
	L"discard",
	NULL,
	L"Discards all input.",
	0,
	kVDXPluginType_Audio,
	0,

	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_discard
};
