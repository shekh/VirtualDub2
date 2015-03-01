//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2005 Avery Lee
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
#include "gui.h"
#include "resource.h"

///////////////////////////////////////////////////////////////////////////

VDAFBASE_BEGIN_CONFIG(FormatConv);
VDAFBASE_CONFIG_ENTRY(FormatConv, 0, U32, precision, L"Precision", L"Bit precision to use for output samples.");
VDAFBASE_END_CONFIG(FormatConv, 0);

typedef VDAudioFilterData_FormatConv VDAudioFilterFormatConvConfig;

class VDDialogAudioFilterFormatConvConfig : public VDDialogBaseW32 {
public:
	VDDialogAudioFilterFormatConvConfig(VDAudioFilterFormatConvConfig& config) : VDDialogBaseW32(IDD_AF_FORMATCONV), mConfig(config) {}

	bool Activate(VDGUIHandle hParent) {
		return 0 != ActivateDialog(hParent);
	}

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
		char buf[256];

		switch(msg) {
		case WM_INITDIALOG:
			sprintf(buf, "%d", mConfig.precision);
			SetDlgItemText(mhdlg, IDC_PRECISION, buf);
			return TRUE;
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				{
					BOOL b;
					unsigned v = GetDlgItemInt(mhdlg, IDC_PRECISION, &b, FALSE);

					if (!b || (v != 8 && v != 16)) {
						MessageBeep(MB_ICONEXCLAMATION);
						SetFocus(GetDlgItem(mhdlg, IDC_PRECISION));
						return TRUE;
					}
					mConfig.precision = v;
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

	VDAudioFilterFormatConvConfig& mConfig;
};

class VDAudioFilterFormatConv : public VDAudioFilterBase {
public:
	VDAudioFilterFormatConv();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();

	void *GetConfigPtr() { return &mConfig; }

	bool Config(HWND hwnd) {
		VDAudioFilterFormatConvConfig	config(mConfig);

		if (!hwnd)
			return true;

		if (VDDialogAudioFilterFormatConvConfig(config).Activate((VDGUIHandle)hwnd)) {
			mConfig = config;
			return true;
		}
		return false;
	}

protected:
	VDAudioFilterFormatConvConfig	mConfig;
};

VDAudioFilterFormatConv::VDAudioFilterFormatConv() {
	mConfig.precision = 16;
}

void __cdecl VDAudioFilterFormatConv::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterFormatConv;
}

uint32 VDAudioFilterFormatConv::Prepare() {
	const VDXWaveFormat& format0 = *mpContext->mpInputs[0]->mpFormat;

	if (   format0.mTag != VDXWaveFormat::kTagPCM
		|| (format0.mSampleBits != 8 && format0.mSampleBits != 16)
		)
		return kVFAPrepare_BadFormat;

	if (mConfig.precision != 8 && mConfig.precision != 16) {
		mpContext->mpServices->SetError("Precision must be either 8 or 16 bits.");
		return 0;
	}

	VDXWaveFormat *pwf0;

	if (!(mpContext->mpOutputs[0]->mpFormat = pwf0 = mpContext->mpAudioCallbacks->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat))) {
		mpContext->mpServices->SetErrorOutOfMemory();
		return 0;
	}

	pwf0->mSampleBits	= (uint16)mConfig.precision;
	pwf0->mBlockSize	= (uint16)((pwf0->mChannels * pwf0->mSampleBits + 7) >> 3);
	pwf0->mDataRate		= pwf0->mBlockSize * pwf0->mSamplingRate;

	return 0;
}

uint32 VDAudioFilterFormatConv::Run() {
	VDAudioFilterPin& pin1 = *mpContext->mpInputs[0];

	int samples = mpContext->mCommonSamples;

	if (!samples && pin1.mbEnded)
		return kVFARun_Finished;

	sint16 *dst = (sint16 *)mpContext->mpOutputs[0]->mpBuffer;

	mpContext->mpOutputs[0]->mSamplesWritten = mpContext->mpInputs[0]->Read(dst, samples, true, mConfig.precision==16 ? kVFARead_PCM16 : kVFARead_PCM8);

	return 0;
}

extern const struct VDAudioFilterDefinition afilterDef_formatconv = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterFormatConv),	1, 1,

	&VDAudioFilterData_FormatConv::members.info,

	VDAudioFilterFormatConv::InitProc,
	&VDAudioFilterBase::sVtbl,
};


extern const VDPluginInfo apluginDef_formatconv = {
	sizeof(VDPluginInfo),
	L"format convert",
	NULL,
	L"Converts an audio stream to a different sample precision.",
	0,
	kVDXPluginType_Audio,
	0,

	kVDXPlugin_APIVersion,
	kVDXPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_formatconv
};
