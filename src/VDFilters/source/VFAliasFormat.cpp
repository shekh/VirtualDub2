//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2007 Avery Lee
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
#include <windows.h>
#include "resource.h"
#include <vd2/system/strutil.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/Kasumi/pixmaputils.h>

using namespace nsVDXPixmap;

struct VDVFilterAliasFormatConfig {
	ColorSpaceMode mColorSpaceMode;
	ColorRangeMode mColorRangeMode;
	int alphaMode;

	VDVFilterAliasFormatConfig()
		: mColorSpaceMode(kColorSpaceMode_None)
		, mColorRangeMode(kColorRangeMode_None)
	{
		alphaMode = -1;
	}
};

class VDVFilterAliasFormat;

class VDVFilterAliasFormatConfigDialog : public VDDialogFrameW32 {
public:
	VDVFilterAliasFormatConfigDialog(VDVFilterAliasFormatConfig& config);

	bool OnLoaded();
	void OnDataExchange(bool write);
	bool OnCommand(uint32 id, uint32 extcode);
	void redo();
	void redoFrame();

	IVDXFilterPreview2 *fp;
	VDVFilterAliasFormat* filter;

protected:
	VDVFilterAliasFormatConfig& mConfig;
};

VDVFilterAliasFormatConfigDialog::VDVFilterAliasFormatConfigDialog(VDVFilterAliasFormatConfig& config)
	: VDDialogFrameW32(IDD_FILTER_ALIASFORMAT)
	, mConfig(config)
{
	fp = 0;
	filter = 0;
}

bool VDVFilterAliasFormatConfigDialog::OnLoaded() {
	VDDialogFrameW32::OnLoaded();
	SetFocusToControl(IDC_STATIC_COLORSPACE);
	if (fp) {
		EnableWindow(GetDlgItem(mhdlg, IDC_PREVIEW), TRUE);
		fp->InitButton((VDXHWND)GetDlgItem(mhdlg, IDC_PREVIEW));
	}
	return true;
}

void VDVFilterAliasFormatConfigDialog::OnDataExchange(bool write) {
	if (write) {
		if (IsButtonChecked(IDC_CS_NONE))
			mConfig.mColorSpaceMode = kColorSpaceMode_None;
		else if (IsButtonChecked(IDC_CS_601))
			mConfig.mColorSpaceMode = kColorSpaceMode_601;
		else if (IsButtonChecked(IDC_CS_709))
			mConfig.mColorSpaceMode = kColorSpaceMode_709;

		if (IsButtonChecked(IDC_CR_NONE))
			mConfig.mColorRangeMode = kColorRangeMode_None;
		else if (IsButtonChecked(IDC_CR_LIMITED))
			mConfig.mColorRangeMode = kColorRangeMode_Limited;
		else if (IsButtonChecked(IDC_CR_FULL))
			mConfig.mColorRangeMode = kColorRangeMode_Full;

		if (IsButtonChecked(IDC_ALPHA_NONE))
			mConfig.alphaMode = -1;
		else if (IsButtonChecked(IDC_ALPHA_DISABLED))
			mConfig.alphaMode = FilterModPixmapInfo::kAlphaInvalid;
		else if (IsButtonChecked(IDC_ALPHA_MASK))
			mConfig.alphaMode = FilterModPixmapInfo::kAlphaMask;
		else if (IsButtonChecked(IDC_ALPHA_OPACITY_PM))
			mConfig.alphaMode = FilterModPixmapInfo::kAlphaOpacity_pm;
		else if (IsButtonChecked(IDC_ALPHA_OPACITY))
			mConfig.alphaMode = FilterModPixmapInfo::kAlphaOpacity;

	} else {
		CheckButton(IDC_CS_NONE, mConfig.mColorSpaceMode == kColorSpaceMode_None);
		CheckButton(IDC_CS_601, mConfig.mColorSpaceMode == kColorSpaceMode_601);
		CheckButton(IDC_CS_709, mConfig.mColorSpaceMode == kColorSpaceMode_709);
		CheckButton(IDC_CR_NONE, mConfig.mColorRangeMode == kColorRangeMode_None);
		CheckButton(IDC_CR_LIMITED, mConfig.mColorRangeMode == kColorRangeMode_Limited);
		CheckButton(IDC_CR_FULL, mConfig.mColorRangeMode == kColorRangeMode_Full);

		CheckButton(IDC_ALPHA_NONE, mConfig.alphaMode == -1);
		CheckButton(IDC_ALPHA_DISABLED, mConfig.alphaMode == FilterModPixmapInfo::kAlphaInvalid);
		CheckButton(IDC_ALPHA_MASK, mConfig.alphaMode == FilterModPixmapInfo::kAlphaMask);
		CheckButton(IDC_ALPHA_OPACITY_PM, mConfig.alphaMode == FilterModPixmapInfo::kAlphaOpacity_pm);
		CheckButton(IDC_ALPHA_OPACITY, mConfig.alphaMode == FilterModPixmapInfo::kAlphaOpacity);
	}
}

bool VDVFilterAliasFormatConfigDialog::OnCommand(uint32 id, uint32 extcode) {
	if (extcode == BN_CLICKED) {
		switch(id) {
			case IDC_CS_NONE:
			case IDC_CS_601:
			case IDC_CS_709:
			case IDC_CR_NONE:
			case IDC_CR_LIMITED:
			case IDC_CR_FULL:
				OnDataExchange(true);
				redo();
				return TRUE;

			case IDC_ALPHA_NONE:
			case IDC_ALPHA_DISABLED:
			case IDC_ALPHA_MASK:
			case IDC_ALPHA_OPACITY_PM:
			case IDC_ALPHA_OPACITY:
				OnDataExchange(true);
				redoFrame();
				return TRUE;

			case IDC_PREVIEW:
				if (fp) fp->Toggle((VDXHWND)mhdlg);
				return TRUE;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////

class VDVFilterAliasFormat : public VDXVideoFilter {
public:
	VDVFilterAliasFormat();

	uint32 GetParams();
	void Run();

	bool Configure(VDXHWND hwnd);

	void GetSettingString(char *buf, int maxlen);
	void GetScriptString(char *buf, int maxlen);

	void ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc);

	VDXVF_DECLARE_SCRIPT_METHODS();

	VDVFilterAliasFormatConfig mConfig;
};

VDVFilterAliasFormat::VDVFilterAliasFormat() {
}

uint32 VDVFilterAliasFormat::GetParams() {
	using namespace nsVDXPixmap;

	VDXPixmapLayout& dstl = *fa->dst.mpPixmapLayout;
	int matrix_type = VDPixmapFormatMatrixType(dstl.format);

	if(mConfig.mColorRangeMode || mConfig.mColorSpaceMode) {
		if (matrix_type==2) {
			VDPixmapFormatEx format = dstl.format;
			format.colorSpaceMode = mConfig.mColorSpaceMode;
			format.colorRangeMode = mConfig.mColorRangeMode;
			dstl.format = VDPixmapFormatCombine(format);
		}

		if (matrix_type==1) {
			if (fma && fma->fmpixmap) {
				FilterModPixmapInfo* dst_info = fma->fmpixmap->GetPixmapInfo(fa->dst.mpPixmap);
				if (mConfig.mColorSpaceMode) dst_info->colorSpaceMode = mConfig.mColorSpaceMode;
				if (mConfig.mColorRangeMode) dst_info->colorRangeMode = mConfig.mColorRangeMode;
			}
		}
	}

	if (fa->src.mpPixmapLayout->format > nsVDPixmap::kPixFormat_Max_Standard)
		return FILTERPARAM_NOT_SUPPORTED;

	dstl.pitch = fa->src.mpPixmapLayout->pitch;

	return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM;
}

void VDVFilterAliasFormat::Run() {
	if (fma && fma->fmpixmap) {
		FilterModPixmapInfo* dst_info = fma->fmpixmap->GetPixmapInfo(fa->dst.mpPixmap);
		if (mConfig.alphaMode!=-1 && VDPixmapFormatHasAlpha(fa->dst.mpPixmap->format))
			dst_info->alpha_type = mConfig.alphaMode;
	}
}

void VDVFilterAliasFormatConfigDialog::redo() {
	filter->mConfig = mConfig;
	if (fp) fp->RedoSystem();
}

void VDVFilterAliasFormatConfigDialog::redoFrame() {
	filter->mConfig = mConfig;
	if (fp) fp->RedoFrame();
}

bool VDVFilterAliasFormat::Configure(VDXHWND hwnd) {
	VDVFilterAliasFormatConfigDialog dlg(mConfig);
	dlg.fp = fa->ifp2;
	dlg.filter = this;
	VDVFilterAliasFormatConfig oldConfig = mConfig;

	if (!dlg.ShowDialog((VDGUIHandle)hwnd)) {
		mConfig = oldConfig;
		return false;
	}

	return true;
}

void VDVFilterAliasFormat::GetSettingString(char *buf, int maxlen) {
	static const char *kColorMode[]={
		"same",
		"601",
		"709",
	};

	static const char *kRangeMode[]={
		"same",
		"limited",
		"full"
	};

	const char* alpha = 0;
	if (mConfig.alphaMode!=-1) {
		alpha = "alpha";
		if (mConfig.alphaMode==0)
			alpha = "no alpha";
	}

	if (alpha)
		SafePrintf(buf, maxlen, " (%s, %s, %s)", kColorMode[mConfig.mColorSpaceMode], kRangeMode[mConfig.mColorRangeMode], alpha);
	else
		SafePrintf(buf, maxlen, " (%s, %s)", kColorMode[mConfig.mColorSpaceMode], kRangeMode[mConfig.mColorRangeMode]);
}

void VDVFilterAliasFormat::GetScriptString(char *buf, int maxlen) {
	if (mConfig.alphaMode==-1)
		SafePrintf(buf, maxlen, "Config(%d, %d)", mConfig.mColorSpaceMode, mConfig.mColorRangeMode);
	else
		SafePrintf(buf, maxlen, "Config(%d, %d, %d)", mConfig.mColorSpaceMode, mConfig.mColorRangeMode, mConfig.alphaMode);
}

void VDVFilterAliasFormat::ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc) {
	int colorMode = argv[0].asInt();
	int levelMode = argv[1].asInt();
	int alphaMode = -1;
	if (argc>2) alphaMode = argv[2].asInt();

	if (colorMode < 0 || colorMode > kColorSpaceModeCount)
		isi->ScriptError(VDXScriptError::FCALL_OUT_OF_RANGE);

	if (levelMode < 0 || levelMode > kColorRangeModeCount)
		isi->ScriptError(VDXScriptError::FCALL_OUT_OF_RANGE);

	mConfig.mColorSpaceMode = (ColorSpaceMode)colorMode;
	mConfig.mColorRangeMode = (ColorRangeMode)levelMode;
	mConfig.alphaMode = alphaMode;
}

VDXVF_BEGIN_SCRIPT_METHODS(VDVFilterAliasFormat)
VDXVF_DEFINE_SCRIPT_METHOD(VDVFilterAliasFormat, ScriptConfig, "ii")
VDXVF_DEFINE_SCRIPT_METHOD(VDVFilterAliasFormat, ScriptConfig, "iii")
VDXVF_END_SCRIPT_METHODS()

extern const VDXFilterDefinition2 g_VDVFAliasFormat = VDXVideoFilterDefinition<VDVFilterAliasFormat>(NULL, "alias format", "Relabel video with a different color space or color encoding without changing video data.");

// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{24,{flat}}' }'' : unreferenced local function has been removed
#pragma warning(disable: 4505)
