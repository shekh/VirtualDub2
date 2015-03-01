#include <vd2/system/math.h>
#include <math.h>
#include "af_base.h"

class VDAudioFilterToneGenerator : public VDAudioFilterBase {
public:
	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
	void Start();
	sint64 Seek(sint64);

	unsigned		mPos;
	unsigned		mLimit;
};

void __cdecl VDAudioFilterToneGenerator::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterToneGenerator;
}

uint32 VDAudioFilterToneGenerator::Prepare() {
	VDWaveFormat *pwf = mpContext->mpAudioCallbacks->AllocPCMWaveFormat(44100, 1, 16, false);

	if (!pwf) {
		mpContext->mpServices->SetErrorOutOfMemory();
		return 0;
	}

	mpContext->mpOutputs[0]->mpFormat		= pwf;

	return 0;
}

void VDAudioFilterToneGenerator::Start() {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[0];
	const VDWaveFormat& format = *pin.mpFormat;

	mPos	= 0;
	mLimit	= 44100 * 10;

	pin.mLength = 10000000;
}

uint32 VDAudioFilterToneGenerator::Run() {
	int count = mpContext->mCommonSamples;
	sint16 *dst = (sint16 *)mpContext->mpOutputs[0]->mpBuffer;

	if (mPos < mLimit) {
		if (mPos + count > mLimit)
			count = mLimit - mPos;

		for(int i=0; i<count; ++i)
			dst[i] = VDRoundToIntFast((float)(32767.0 * sin((i+mPos) * (3.1415926535897932 / 22100 * 440))));

		mpContext->mpOutputs[0]->mSamplesWritten = count;

		mPos += count;
	}

	return mPos >= mLimit ? kVFARun_Finished : 0;
}

sint64 VDAudioFilterToneGenerator::Seek(sint64 us) {
	if (us >= 10000000)
		mPos = mLimit;
	else
		mPos = (unsigned)(us * 441 / 10000);

	return us;
}

extern const struct VDAudioFilterDefinition afilterDef_tonegenerator = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterToneGenerator),	0,	1,

	NULL,

	VDAudioFilterToneGenerator::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const struct VDPluginInfo apluginDef_tonegenerator = {
	sizeof(VDPluginInfo),
	L"tone generator",
	NULL,
	L"Produces a constant 440Hz tone.",
	0,
	kVDPluginType_Audio,
	0,

	kVDPlugin_APIVersion,
	kVDPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_tonegenerator
};



class VDAudioFilterRingModulator : public VDAudioFilterBase {
public:
	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
};

void __cdecl VDAudioFilterRingModulator::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterRingModulator;
}

uint32 VDAudioFilterRingModulator::Prepare() {
	VDAudioFilterPin& pin0 = *mpContext->mpInputs[0];
	VDAudioFilterPin& pin1 = *mpContext->mpInputs[1];

	// validate pin requirements
	if (pin0.mpFormat->mTag != VDWaveFormat::kTagPCM
		|| pin1.mpFormat->mTag != VDWaveFormat::kTagPCM
		|| pin0.mpFormat->mChannels != pin1.mpFormat->mChannels
		|| pin0.mpFormat->mSamplingRate != pin1.mpFormat->mSamplingRate
	)
		return kVFAPrepare_BadFormat;

	VDWaveFormat *pwf = mpContext->mpAudioCallbacks->AllocPCMWaveFormat(pin0.mpFormat->mSamplingRate, pin0.mpFormat->mChannels, 16, false);

	if (!pwf) {
		mpContext->mpServices->SetErrorOutOfMemory();
		return 0;
	}

	mpContext->mpOutputs[0]->mpFormat		= pwf;

	return 0;
}

uint32 VDAudioFilterRingModulator::Run() {
	if (mpContext->mInputsEnded >= 2)
		return kVFARun_Finished;

	sint16 *dst = (sint16 *)mpContext->mpOutputs[0]->mpBuffer;

	int count = mpContext->mCommonGranules;

	if (count > 512 / mpContext->mpOutputs[0]->mpFormat->mChannels)
		count = 512 / mpContext->mpOutputs[0]->mpFormat->mChannels;

	sint16 buf0[512];
	sint16 buf1[512];

	mpContext->mpInputs[0]->Read(buf0, count, true, kVFARead_PCM16);
	mpContext->mpInputs[1]->Read(buf1, count, true, kVFARead_PCM16);

	int samples = count * mpContext->mpOutputs[0]->mpFormat->mChannels;
	for(int i=0; i<samples; ++i) {
		int y = (buf0[i] * buf1[i] + 0x4000) >> 15;

		// take care of annoying -1.0 * -1.0 = +1.0 case
		if (y >= 0x7fff)
			y = 0x7fff;

		dst[i] = y;
	}

	mpContext->mpOutputs[0]->mSamplesWritten = count;

	return 0;
}

extern const struct VDAudioFilterDefinition afilterDef_ringmodulator = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterRingModulator),	2,	1,

	NULL,

	VDAudioFilterRingModulator::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const struct VDPluginInfo apluginDef_ringmodulator = {
	sizeof(VDPluginInfo),
	L"ring modulator",
	NULL,
	L"Modulates one audio stream using another.",
	0,
	kVDPluginType_Audio,
	0,

	kVDPlugin_APIVersion,
	kVDPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_ringmodulator
};




extern "C" __declspec(dllexport) const VDPluginInfo *const *VDGetPluginInfo() {
	static const VDPluginInfo *const sPlugins[]={
		&apluginDef_tonegenerator,
		&apluginDef_ringmodulator,
		NULL
	};

	return sPlugins;
}
