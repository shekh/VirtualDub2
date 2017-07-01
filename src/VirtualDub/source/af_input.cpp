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

#include <mmsystem.h>

#include <vd2/system/VDRingBuffer.h>
#include <vd2/system/Error.h>
#include <vd2/system/strutil.h>
#include <vd2/system/fraction.h>
#include <vd2/system/refcount.h>
#include <vd2/system/math.h>
#include <vd2/system/vdalloc.h>
#include <vd2/Riza/audiocodec.h>

#include "filter.h"
#include "AudioSource.h"
#include "af_base.h"
#include "af_input.h"

bool VDPreferencesIsPreferInternalAudioDecodersEnabled();
const VDStringW& VDPreferencesGetAudioPlaybackDeviceKey();

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterInput : public VDAudioFilterBase, public IVDAudioFilterInput {
public:
	VDAudioFilterInput();

	void EnableDecompression(bool enable) { mbDecompressionAllowed = enable; }

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
	void Start();
	sint64 Seek(sint64);

	AudioSource *mpSrc;
	VDPosition		mPos;
	unsigned		mPad;
	VDPosition		mLimit;
	int				mSrcBlockAlign;
	bool			mbDecompressionAllowed;

	vdautoptr<IVDAudioCodec> 	mpDecompressor;
};

IVDAudioFilterInput *VDGetAudioFilterInputInterface(void *p) {
	return static_cast<IVDAudioFilterInput *>((VDAudioFilterInput *)p);
}

void __cdecl VDAudioFilterInput::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterInput;
}

extern vdrefptr<AudioSource> inputAudio;

VDAudioFilterInput::VDAudioFilterInput()
	: mbDecompressionAllowed(true)
{
}

uint32 VDAudioFilterInput::Prepare() {
	mpSrc = inputAudio;

	if (!mpSrc)
		throw MyError("No audio source is available for the \"input\" audio filter.");

	const VDWaveFormat *pwfex = mpSrc->getWaveFormat();

	mpDecompressor = NULL;

	if (mbDecompressionAllowed) {
		if (!is_audio_pcm(pwfex) && !is_audio_float(pwfex)) {
			mpDecompressor = VDLocateAudioDecompressor((const VDWaveFormat *)pwfex, NULL, VDPreferencesIsPreferInternalAudioDecodersEnabled());
			pwfex = mpDecompressor->GetOutputFormat();
		}

		//VDASSERT(pwfex->mTag == WAVE_FORMAT_PCM);
	}

	int extra = pwfex->mTag == WAVE_FORMAT_PCM ? 0 : pwfex->mExtraSize;

	VDXWaveFormat *pwf = mpContext->mpAudioCallbacks->AllocCustomWaveFormat(extra);

	if (!pwf) {
		mpContext->mpServices->SetErrorOutOfMemory();
		return 0;
	}

	memcpy(pwf, pwfex, pwfex->mTag == WAVE_FORMAT_PCM ? sizeof(PCMWAVEFORMAT) : sizeof(WAVEFORMATEX) + pwfex->mExtraSize);

	mpContext->mpOutputs[0]->mGranularity	= 1;
	mpContext->mpOutputs[0]->mpFormat		= pwf;
	mpContext->mpOutputs[0]->mbVBR			= false;

	VDAudioFilterPin& pin = *mpContext->mpOutputs[0];

	const VDWaveFormat& srcFormat = *mpSrc->getWaveFormat();

	mSrcBlockAlign = srcFormat.mBlockSize;

	const VDFraction& sampleRate = mpSrc->getRate();
	pin.mLength = sampleRate.scale64ir(mpSrc->getLength() * (sint64)1000000);

	return 0;
}

void VDAudioFilterInput::Start() {
	mPos	= mpSrc->getStart();
	mLimit	= mpSrc->getEnd();
	mPad	= 0;
}

uint32 VDAudioFilterInput::Run() {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;


	if (mPad > 0) {
		int samples = pin.mAvailSpace;

		if (samples > mPad)
			samples = mPad;

		if (format.mSampleBits < 16)
			memset(pin.mpBuffer, 0x80, samples * format.mBlockSize);
		else
			memset(pin.mpBuffer, 0x00, samples * format.mBlockSize);

		pin.mSamplesWritten = samples;
		mPad -= samples;

		return 0;

	} else if (mpDecompressor) {
			unsigned bytes = mpDecompressor->GetOutputLevel();

		if (bytes > 0) {
			int count = mpDecompressor->CopyOutput(pin.mpBuffer, pin.mAvailSpace * format.mBlockSize);

			pin.mSamplesWritten = count / format.mBlockSize;
			return 0;
		}

		void *dst = mpDecompressor->LockInputBuffer(bytes);

		if (bytes >= mSrcBlockAlign && mPos < mLimit) {
			uint32 actualbytes, samples;

			// NOTE: We have to make sure the count passed in is correct, as Avisynth doesn't
			//       check it properly for audio reads!

			int res = mpSrc->read(mPos, bytes / mSrcBlockAlign, dst, bytes, &actualbytes, &samples);

			VDASSERT(res != IVDStreamSource::kBufferTooSmall);

			if (res)
				throw MyError("Read error on audio sample %u. The source may be corrupted.", (unsigned)mPos);

			mPos += samples;
			mpDecompressor->UnlockInputBuffer(actualbytes);

			if (actualbytes)
				return kVFARun_InternalWork;
		}

		bool inputEnded = mPos >= mLimit; 

		mpDecompressor->Convert(inputEnded, true);

		return inputEnded && mpDecompressor->IsEnded() ? kVFARun_Finished : kVFARun_InternalWork;
	} else {
		uint32 samples = pin.mAvailSpace;
		uint32 bytes = pin.mAvailSpace * format.mBlockSize;

		// NOTE: We have to make sure the count passed in is correct, as Avisynth doesn't
		//       check it properly for audio reads!

		int res = mpSrc->read(mPos, samples, pin.mpBuffer, bytes, &bytes, &samples);

		if (res)
			throw MyError("Read error on audio sample %u. The source may be corrupted.", (unsigned)mPos);

		pin.mSamplesWritten = samples;

		mPos += samples;

		return mPos >= mLimit ? kVFARun_Finished : 0;
	}
}

sint64 VDAudioFilterInput::Seek(sint64 us) {
	mPos = 0;
	mPad = 0;

	if (us < 0) {
		const VDXWaveFormat& format = *(const VDXWaveFormat *)mpContext->mpOutputs[0]->mpFormat;

		mPad = (unsigned)VDMulDiv64(format.mDataRate, -us, format.mBlockSize * VD64(1000000));
	} else {
		mPos = mpSrc->TimeToPositionVBR(us);
	}

	return us;
}

extern const struct VDAudioFilterDefinition afilterDef_input = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_SerializedIO,

	sizeof(VDAudioFilterInput),	0,	1,

	NULL,

	VDAudioFilterInput::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const VDPluginInfo apluginDef_input = {
	sizeof(VDPluginInfo),
	L"input",
	NULL,
	L"Produces audio from current audio source.",
	0,
	kVDXPluginType_Audio,
	0,

	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_input
};

///////////////////////////////////////////////////////////////////////////

#include <vd2/Riza/audioout.h>

class VDAudioFilterPlayback : public VDAudioFilterBase {
public:
	VDAudioFilterPlayback();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
	void Start();

	sint64 Seek(sint64 us);

	vdautoptr<IVDAudioOutput> mpAudioOut;
};

VDAudioFilterPlayback::VDAudioFilterPlayback()
	: mpAudioOut(VDCreateAudioOutputWaveOutW32())
{
}

void __cdecl VDAudioFilterPlayback::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterPlayback;
}

uint32 VDAudioFilterPlayback::Prepare() {
	mpContext->mpInputs[0]->mGranularity	= 1;
	mpContext->mpInputs[0]->mDelay		= 0;
	return 0;
}

void VDAudioFilterPlayback::Start() {
	const VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;

	if (!mpAudioOut->Init(32768, 4, (WAVEFORMATEX *)&format, VDPreferencesGetAudioPlaybackDeviceKey().c_str()))
		mpAudioOut->GoSilent();
	mpAudioOut->Start();
}

uint32 VDAudioFilterPlayback::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;
	char buf[16384];

	for(;;) {
		uint32 samples = mpContext->mpInputs[0]->Read(buf, sizeof buf / format.mBlockSize, false, kVFARead_Native);

		if (samples)
			mpAudioOut->Write(buf, samples*format.mBlockSize);
		else if (pin.mbEnded)
			return kVFARun_Finished;
		else
			break;
	}

	return 0;
}

sint64 VDAudioFilterPlayback::Seek(sint64 us) {
	mpAudioOut->Stop();
	mpAudioOut->Start();
	return us;
}

extern const struct VDAudioFilterDefinition afilterDef_playback = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterPlayback),	1,	0,

	NULL,

	VDAudioFilterPlayback::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const VDPluginInfo apluginDef_playback = {
	sizeof(VDPluginInfo),
	L"*playback",
	NULL,
	L"",
	0,
	kVDXPluginType_Audio,
	0,

	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_playback
};

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterButterfly : public VDAudioFilterBase {
public:
	VDAudioFilterButterfly();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
};

VDAudioFilterButterfly::VDAudioFilterButterfly()
{
}

void __cdecl VDAudioFilterButterfly::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterButterfly;
}

uint32 VDAudioFilterButterfly::Prepare() {
	const VDXWaveFormat *pFormatIn = mpContext->mpInputs[0]->mpFormat;
	VDXWaveFormat *pFormat;

	if (pFormatIn->mChannels != 2)
		return kVFAPrepare_BadFormat;

	if (!(pFormat = mpContext->mpAudioCallbacks->CopyWaveFormat(pFormatIn))) {
		mpContext->mpServices->SetErrorOutOfMemory();
		return 0;
	}

	mpContext->mpOutputs[0]->mpFormat = pFormat;

	pFormat->mSampleBits = 16;
	pFormat->mBlockSize = (uint16)(2*pFormat->mChannels);

	return 0;
}

uint32 VDAudioFilterButterfly::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];

	int samples = mpContext->mCommonSamples;
	sint16 *dst = (sint16 *)mpContext->mpOutputs[0]->mpBuffer;

	if (!samples)
		return pin.mbEnded ? kVFARun_Finished : 0;

	samples = pin.Read(dst, samples, false, kVFARead_PCM16);

	if (samples) {
		short *p = (short *)dst;

		for(int i=0; i<samples; ++i) {
			const int x = p[0];
			const int y = p[1];

			p[0] = (short)((x+y)>>1);
			p[1] = (short)((x-y)>>1);

			p += 2;
		}
	}

	mpContext->mpOutputs[0]->mSamplesWritten = samples;

	return 0;
}

extern const struct VDAudioFilterDefinition afilterDef_butterfly = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterButterfly),	1,	1,

	NULL,

	VDAudioFilterButterfly::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const VDPluginInfo apluginDef_butterfly = {
	sizeof(VDPluginInfo),
	L"butterfly",
	NULL,
	L"Computes the half-sum and half-difference between stereo channels. This can be used to "
		L"split stereo into mid/side signals or recombine mid/side into stereo.",
	0,
	kVDXPluginType_Audio,
	0,

	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_butterfly
};

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterStereoSplit : public VDAudioFilterBase {
public:
	VDAudioFilterStereoSplit();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
};

VDAudioFilterStereoSplit::VDAudioFilterStereoSplit()
{
}

void __cdecl VDAudioFilterStereoSplit::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterStereoSplit;
}

uint32 VDAudioFilterStereoSplit::Prepare() {
	const VDXWaveFormat& format0 = *mpContext->mpInputs[0]->mpFormat;

	if (   format0.mTag != VDXWaveFormat::kTagPCM
		|| format0.mChannels != 2
		|| (format0.mSampleBits != 8 && format0.mSampleBits != 16)
		)
		return kVFAPrepare_BadFormat;

	mpContext->mpInputs[0]->mGranularity	= 1;
	mpContext->mpInputs[0]->mDelay		= 0;
	mpContext->mpOutputs[0]->mGranularity = 1;
	mpContext->mpOutputs[1]->mGranularity = 1;

	VDXWaveFormat *pwf0, *pwf1;

	if (!(mpContext->mpOutputs[0]->mpFormat = pwf0 = mpContext->mpAudioCallbacks->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat))) {
		mpContext->mpServices->SetErrorOutOfMemory();
		return 0;
	}
	if (!(mpContext->mpOutputs[1]->mpFormat = pwf1 = mpContext->mpAudioCallbacks->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat))) {
		mpContext->mpServices->SetErrorOutOfMemory();
		return 0;
	}
	pwf0->mChannels = 1;
	pwf1->mChannels = 1;
	pwf0->mBlockSize = (uint16)(pwf0->mSampleBits>>3);
	pwf1->mBlockSize = (uint16)(pwf1->mSampleBits>>3);
	pwf0->mDataRate	= pwf0->mBlockSize * pwf0->mSamplingRate;
	pwf1->mDataRate	= pwf1->mBlockSize * pwf1->mSamplingRate;
	return 0;
}

uint32 VDAudioFilterStereoSplit::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;

	int samples = mpContext->mCommonSamples, actual = 0;

	void *dst1 = (sint16 *)mpContext->mpOutputs[0]->mpBuffer;
	void *dst2 = (sint16 *)mpContext->mpOutputs[1]->mpBuffer;

	while(samples > 0) {
		sint16 buf[4096];
		int tc = mpContext->mpInputs[0]->Read(&buf, std::min<int>(samples, 4096 / format.mChannels), false, kVFARead_PCM16);

		if (tc<=0)
			break;

		sint16 *dst1w = (sint16 *)dst1;
		sint16 *dst2w = (sint16 *)dst2;

		for(int i=0; i<tc; ++i) {
			*dst1w++ = buf[i*2+0];
			*dst2w++ = buf[i*2+1];
		}

		dst1 = dst1w;
		dst2 = dst2w;

		actual += tc;
		samples -= tc;
	}

	mpContext->mpOutputs[0]->mSamplesWritten = actual;
	mpContext->mpOutputs[1]->mSamplesWritten = actual;

	return !actual && pin.mbEnded ? kVFARun_Finished : 0;
}

extern const struct VDAudioFilterDefinition afilterDef_stereosplit = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterStereoSplit),	1,	2,

	NULL,

	VDAudioFilterStereoSplit::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const VDPluginInfo apluginDef_stereosplit = {
	sizeof(VDPluginInfo),
	L"stereo split",
	NULL,
	L"Splits a stereo stream into two mono streams, one per channel.",
	0,
	kVDXPluginType_Audio,
	0,

	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_stereosplit
};

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterStereoMerge : public VDAudioFilterBase {
public:
	VDAudioFilterStereoMerge();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
};

VDAudioFilterStereoMerge::VDAudioFilterStereoMerge()
{
}

void __cdecl VDAudioFilterStereoMerge::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterStereoMerge;
}

uint32 VDAudioFilterStereoMerge::Prepare() {
	const VDXWaveFormat& format0 = *mpContext->mpInputs[0]->mpFormat;
	const VDXWaveFormat& format1 = *mpContext->mpInputs[1]->mpFormat;

	if (   format0.mTag != VDXWaveFormat::kTagPCM
		|| format0.mChannels != 1
		|| (format0.mSampleBits != 8 && format0.mSampleBits != 16)
		|| format1.mTag != VDXWaveFormat::kTagPCM
		|| format1.mChannels != 1
		|| (format1.mSampleBits != 8 && format0.mSampleBits != 16)
		|| format0.mSamplingRate != format1.mSamplingRate
		|| format0.mSampleBits != format1.mSampleBits
		)
		return kVFAPrepare_BadFormat;

	mpContext->mpInputs[0]->mGranularity		= 1;
	mpContext->mpInputs[0]->mDelay			= 0;
	mpContext->mpInputs[1]->mGranularity		= 1;
	mpContext->mpInputs[1]->mDelay			= 0;
	mpContext->mpOutputs[0]->mGranularity	= 1;

	VDXWaveFormat *pwf0;

	if (!(mpContext->mpOutputs[0]->mpFormat = pwf0 = mpContext->mpAudioCallbacks->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat))) {
		mpContext->mpServices->SetErrorOutOfMemory();
		return 0;
	}

	pwf0->mChannels = 2;
	pwf0->mBlockSize = (uint16)(pwf0->mSampleBits>>2);
	pwf0->mDataRate	= pwf0->mBlockSize * pwf0->mSamplingRate;

	return 0;
}

uint32 VDAudioFilterStereoMerge::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	VDAudioFilterPin& pin2 = *mpContext->mpInputs[1];
	const VDXWaveFormat& format = *pin.mpFormat;

	int samples = mpContext->mCommonSamples, actual = 0;

	void *dst = mpContext->mpOutputs[0]->mpBuffer;

	if (!samples && pin.mbEnded && pin2.mbEnded)
		return kVFARun_Finished;

	while(samples > 0) {
		union {
			sint16	w[4096];
			uint8	b[8192];
		} buf0, buf1;
		int tc = std::min<int>(samples, sizeof buf0 / format.mBlockSize);

		int tca0 = mpContext->mpInputs[0]->Read(&buf0, tc, false, kVFARead_Native);
		int tca1 = mpContext->mpInputs[1]->Read(&buf1, tc, false, kVFARead_Native);

		VDASSERT(tc == tca0 && tc == tca1);

		switch(format.mSampleBits) {
		case 8:
			{
				uint8 *dstb = (uint8 *)dst;
				for(int i=0; i<tc; ++i) {
					*dstb++ = buf0.b[i];
					*dstb++ = buf1.b[i];
				}
				dst = dstb;
			}
			break;
		case 16:
			{
				sint16 *dstw = (sint16 *)dst;
				for(int i=0; i<tc; ++i) {
					*dstw++ = buf0.w[i];
					*dstw++ = buf1.w[i];
				}
				dst = dstw;
			}
			break;
		}

		actual += tc;
		samples -= tc;
	}

	mpContext->mpOutputs[0]->mSamplesWritten = actual;
	return 0;
}

extern const struct VDAudioFilterDefinition afilterDef_stereomerge = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterStereoMerge),	2, 1,

	NULL,

	VDAudioFilterStereoMerge::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const VDPluginInfo apluginDef_stereomerge = {
	sizeof(VDPluginInfo),
	L"stereo merge",
	NULL,
	L"Recombines two mono streams into a single stereo stream.",
	0,
	kVDXPluginType_Audio,
	0,

	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_stereomerge
};
