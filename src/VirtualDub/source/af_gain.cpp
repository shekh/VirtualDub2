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

VDAFBASE_BEGIN_CONFIG(Gain);
VDAFBASE_CONFIG_ENTRY(Gain, 0, Double, ratio, L"Gain", L"Factor by which to multiply amplitude of sound.");
VDAFBASE_END_CONFIG(Gain, 0);

typedef VDAudioFilterData_Gain VDAudioFilterGainConfig;

class VDDialogAudioFilterGainConfig : public VDDialogBaseW32 {
public:
	VDDialogAudioFilterGainConfig(VDAudioFilterGainConfig& config) : VDDialogBaseW32(IDD_AF_GAIN), mConfig(config) {}

	bool Activate(VDGUIHandle hParent) {
		return 0 != ActivateDialog(hParent);
	}

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
		char buf[256];

		switch(msg) {
		case WM_INITDIALOG:
			sprintf(buf, "%.4f", mConfig.ratio);
			SetDlgItemText(mhdlg, IDC_FACTOR, buf);
			return TRUE;
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				{
					double v;

					if (!GetDlgItemText(mhdlg, IDC_FACTOR, buf, sizeof buf) || (v=atof(buf))<-8.0 || (v>8.0)) {
						MessageBeep(MB_ICONEXCLAMATION);
						SetFocus(GetDlgItem(mhdlg, IDC_FACTOR));
						return TRUE;
					}
					mConfig.ratio = v;
					End(TRUE);
				}
				return TRUE;
			case IDCANCEL:
				End(FALSE);
				return TRUE;
			}
		}

		return FALSE;
	}

	VDAudioFilterGainConfig& mConfig;
};

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterGain : public VDAudioFilterBase {
public:
	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	VDAudioFilterGain();
	~VDAudioFilterGain();

	uint32 Prepare();
	void Start();
	uint32 Run();

	void *GetConfigPtr() { return &mConfig; }

	bool Config(HWND hwnd) {
		VDAudioFilterGainConfig	config(mConfig);

		if (!hwnd)
			return true;

		if (VDDialogAudioFilterGainConfig(config).Activate((VDGUIHandle)hwnd)) {
			mConfig = config;
			return true;
		}
		return false;
	}

protected:
	VDAudioFilterGainConfig	mConfig;
	VDRingBuffer<sint16>		mOutputBuffer;
	sint32		mScale16;
	sint32		mScale8;
};

void __cdecl VDAudioFilterGain::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterGain;
}

VDAudioFilterGain::VDAudioFilterGain() {
	mConfig.ratio = 1.0;
}

VDAudioFilterGain::~VDAudioFilterGain() {
}

uint32 VDAudioFilterGain::Prepare() {
	const VDXWaveFormat& inFormat = *mpContext->mpInputs[0]->mpFormat;

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

void VDAudioFilterGain::Start() {
	mScale8		= (sint32)floor(0.5 + mConfig.ratio * 65536.0);
	mScale16	= (mScale8 + 128) >> 8;
}

uint32 VDAudioFilterGain::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDXWaveFormat& format = *pin.mpFormat;

	// compute output samples
	int samples = mpContext->mCommonSamples;
	sint16 *dst = (sint16 *)mpContext->mpOutputs[0]->mpBuffer;
	
	if (!samples) {
		if (pin.mbEnded && !mpContext->mInputSamples)
			return kVFARun_Finished;

		return 0;
	}

	// read buffer
	unsigned count = format.mChannels * samples;

	int actual_samples = mpContext->mpInputs[0]->Read(dst, samples, false, kVFARead_PCM16);
	VDASSERT(actual_samples == samples);

	for(unsigned i=0; i<count; ++i) {
		sint32 v = ((sint32)dst[i] * mScale16 + 0x800080) >> 8;

		if ((uint32)v >= 0x10000)
			v = ~v >> 31;

		dst[i] = (sint16)(v - 0x8000);
	}

	mpContext->mpOutputs[0]->mSamplesWritten = samples;

	return 0;
}

extern const struct VDAudioFilterDefinition afilterDef_gain = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_HasConfig,

	sizeof(VDAudioFilterGain),	1,	1,

	&VDAudioFilterData_Gain::members.info,

	VDAudioFilterGain::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const VDPluginInfo apluginDef_gain = {
	sizeof(VDPluginInfo),
	L"gain",
	NULL,
	L"Adjust signal amplitude (volume) by a fixed factor.",
	0,
	kVDXPluginType_Audio,
	0,

	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_gain
};
