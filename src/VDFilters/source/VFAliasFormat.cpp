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
#include "resource.h"
#include <vd2/system/strutil.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/Kasumi/pixmaputils.h>

struct VDVFilterAliasFormatConfig {
	enum ColorSpaceMode {
		kColorSpaceMode_None,
		kColorSpaceMode_601,
		kColorSpaceMode_709,
		kColorSpaceModeCount
	};

	enum ColorRangeMode {
		kColorRangeMode_None,
		kColorRangeMode_Limited,
		kColorRangeMode_Full,
		kColorRangeModeCount
	};

	ColorSpaceMode mColorSpaceMode;
	ColorRangeMode mColorRangeMode;

	VDVFilterAliasFormatConfig()
		: mColorSpaceMode(kColorSpaceMode_None)
		, mColorRangeMode(kColorRangeMode_None)
	{
	}
};

class VDVFilterAliasFormatConfigDialog : public VDDialogFrameW32 {
public:
	VDVFilterAliasFormatConfigDialog(VDVFilterAliasFormatConfig& config);

	bool OnLoaded();
	void OnDataExchange(bool write);

protected:
	VDVFilterAliasFormatConfig& mConfig;
};

VDVFilterAliasFormatConfigDialog::VDVFilterAliasFormatConfigDialog(VDVFilterAliasFormatConfig& config)
	: VDDialogFrameW32(IDD_FILTER_ALIASFORMAT)
	, mConfig(config)
{
}

bool VDVFilterAliasFormatConfigDialog::OnLoaded() {
	VDDialogFrameW32::OnLoaded();
	SetFocusToControl(IDC_STATIC_COLORSPACE);
	return true;
}

void VDVFilterAliasFormatConfigDialog::OnDataExchange(bool write) {
	if (write) {
		if (IsButtonChecked(IDC_CS_NONE))
			mConfig.mColorSpaceMode = VDVFilterAliasFormatConfig::kColorSpaceMode_None;
		else if (IsButtonChecked(IDC_CS_601))
			mConfig.mColorSpaceMode = VDVFilterAliasFormatConfig::kColorSpaceMode_601;
		else if (IsButtonChecked(IDC_CS_709))
			mConfig.mColorSpaceMode = VDVFilterAliasFormatConfig::kColorSpaceMode_709;

		if (IsButtonChecked(IDC_CR_NONE))
			mConfig.mColorRangeMode = VDVFilterAliasFormatConfig::kColorRangeMode_None;
		else if (IsButtonChecked(IDC_CR_LIMITED))
			mConfig.mColorRangeMode = VDVFilterAliasFormatConfig::kColorRangeMode_Limited;
		else if (IsButtonChecked(IDC_CR_FULL))
			mConfig.mColorRangeMode = VDVFilterAliasFormatConfig::kColorRangeMode_Full;
	} else {
		CheckButton(IDC_CS_NONE, mConfig.mColorSpaceMode == VDVFilterAliasFormatConfig::kColorSpaceMode_None);
		CheckButton(IDC_CS_601, mConfig.mColorSpaceMode == VDVFilterAliasFormatConfig::kColorSpaceMode_601);
		CheckButton(IDC_CS_709, mConfig.mColorSpaceMode == VDVFilterAliasFormatConfig::kColorSpaceMode_709);
		CheckButton(IDC_CR_NONE, mConfig.mColorRangeMode == VDVFilterAliasFormatConfig::kColorRangeMode_None);
		CheckButton(IDC_CR_LIMITED, mConfig.mColorRangeMode == VDVFilterAliasFormatConfig::kColorRangeMode_Limited);
		CheckButton(IDC_CR_FULL, mConfig.mColorRangeMode == VDVFilterAliasFormatConfig::kColorRangeMode_Full);
	}
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

protected:
	VDVFilterAliasFormatConfig mConfig;
};

VDVFilterAliasFormat::VDVFilterAliasFormat() {
}

uint32 VDVFilterAliasFormat::GetParams() {
	using namespace nsVDXPixmap;

	VDXPixmapLayout& dstl = *fa->dst.mpPixmapLayout;

	switch(mConfig.mColorSpaceMode) {
		case VDVFilterAliasFormatConfig::kColorSpaceMode_601:
			switch(dstl.format) {
				case kPixFormat_YUV422_UYVY:
				case kPixFormat_YUV422_YUYV:
				case kPixFormat_YUV444_Planar:
				case kPixFormat_YUV422_Planar:
				case kPixFormat_YUV420_Planar:
				case kPixFormat_YUV411_Planar:
				case kPixFormat_YUV410_Planar:
				case kPixFormat_YUV422_UYVY_FR:
				case kPixFormat_YUV422_YUYV_FR:
				case kPixFormat_YUV444_Planar_FR:
				case kPixFormat_YUV422_Planar_FR:
				case kPixFormat_YUV420_Planar_FR:
				case kPixFormat_YUV411_Planar_FR:
				case kPixFormat_YUV410_Planar_FR:
				case kPixFormat_YUV420i_Planar:
				case kPixFormat_YUV420i_Planar_FR:
				case kPixFormat_YUV420it_Planar:
				case kPixFormat_YUV420it_Planar_FR:
				case kPixFormat_YUV420ib_Planar:
				case kPixFormat_YUV420ib_Planar_FR:
					break;

				case kPixFormat_YUV422_UYVY_709:		dstl.format = kPixFormat_YUV422_UYVY; break;
				case kPixFormat_YUV422_YUYV_709:		dstl.format = kPixFormat_YUV422_YUYV; break;
				case kPixFormat_YUV444_Planar_709:		dstl.format = kPixFormat_YUV444_Planar; break;
				case kPixFormat_YUV422_Planar_709:		dstl.format = kPixFormat_YUV422_Planar; break;
				case kPixFormat_YUV420_Planar_709:		dstl.format = kPixFormat_YUV420_Planar; break;
				case kPixFormat_YUV411_Planar_709:		dstl.format = kPixFormat_YUV411_Planar; break;
				case kPixFormat_YUV410_Planar_709:		dstl.format = kPixFormat_YUV410_Planar; break;
				case kPixFormat_YUV422_UYVY_709_FR:		dstl.format = kPixFormat_YUV422_UYVY_FR; break;
				case kPixFormat_YUV422_YUYV_709_FR:		dstl.format = kPixFormat_YUV422_YUYV_FR; break;
				case kPixFormat_YUV444_Planar_709_FR:	dstl.format = kPixFormat_YUV444_Planar_FR; break;
				case kPixFormat_YUV422_Planar_709_FR:	dstl.format = kPixFormat_YUV422_Planar_FR; break;
				case kPixFormat_YUV420_Planar_709_FR:	dstl.format = kPixFormat_YUV420_Planar_FR; break;
				case kPixFormat_YUV411_Planar_709_FR:	dstl.format = kPixFormat_YUV411_Planar_FR; break;
				case kPixFormat_YUV410_Planar_709_FR:	dstl.format = kPixFormat_YUV410_Planar_FR; break;
				case kPixFormat_YUV420i_Planar_709:		dstl.format = kPixFormat_YUV420i_Planar; break;
				case kPixFormat_YUV420i_Planar_709_FR:	dstl.format = kPixFormat_YUV420i_Planar_FR; break;
				case kPixFormat_YUV420it_Planar_709:	dstl.format = kPixFormat_YUV420it_Planar; break;
				case kPixFormat_YUV420it_Planar_709_FR:	dstl.format = kPixFormat_YUV420it_Planar_FR; break;
				case kPixFormat_YUV420ib_Planar_709:	dstl.format = kPixFormat_YUV420ib_Planar; break;
				case kPixFormat_YUV420ib_Planar_709_FR:	dstl.format = kPixFormat_YUV420ib_Planar_FR; break;
				default:
					return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_NOT_SUPPORTED;
			}
			break;

		case VDVFilterAliasFormatConfig::kColorSpaceMode_709:
			switch(dstl.format) {
				case kPixFormat_YUV422_UYVY_709:
				case kPixFormat_YUV422_YUYV_709:
				case kPixFormat_YUV444_Planar_709:
				case kPixFormat_YUV422_Planar_709:
				case kPixFormat_YUV420_Planar_709:
				case kPixFormat_YUV411_Planar_709:
				case kPixFormat_YUV410_Planar_709:
				case kPixFormat_YUV422_UYVY_709_FR:
				case kPixFormat_YUV422_YUYV_709_FR:
				case kPixFormat_YUV444_Planar_709_FR:
				case kPixFormat_YUV422_Planar_709_FR:
				case kPixFormat_YUV420_Planar_709_FR:
				case kPixFormat_YUV411_Planar_709_FR:
				case kPixFormat_YUV410_Planar_709_FR:
				case kPixFormat_YUV420i_Planar_709:
				case kPixFormat_YUV420i_Planar_709_FR:
				case kPixFormat_YUV420it_Planar_709:
				case kPixFormat_YUV420it_Planar_709_FR:
				case kPixFormat_YUV420ib_Planar_709:
				case kPixFormat_YUV420ib_Planar_709_FR:
					break;

				case kPixFormat_YUV422_UYVY:			dstl.format = kPixFormat_YUV422_UYVY_709; break;
				case kPixFormat_YUV422_YUYV:			dstl.format = kPixFormat_YUV422_YUYV_709; break;
				case kPixFormat_YUV444_Planar:			dstl.format = kPixFormat_YUV444_Planar_709; break;
				case kPixFormat_YUV422_Planar:			dstl.format = kPixFormat_YUV422_Planar_709; break;
				case kPixFormat_YUV420_Planar:			dstl.format = kPixFormat_YUV420_Planar_709; break;
				case kPixFormat_YUV411_Planar:			dstl.format = kPixFormat_YUV411_Planar_709; break;
				case kPixFormat_YUV410_Planar:			dstl.format = kPixFormat_YUV410_Planar_709; break;
				case kPixFormat_YUV422_UYVY_FR:			dstl.format = kPixFormat_YUV422_UYVY_709_FR; break;
				case kPixFormat_YUV422_YUYV_FR:			dstl.format = kPixFormat_YUV422_YUYV_709_FR; break;
				case kPixFormat_YUV444_Planar_FR:		dstl.format = kPixFormat_YUV444_Planar_709_FR; break;
				case kPixFormat_YUV422_Planar_FR:		dstl.format = kPixFormat_YUV422_Planar_709_FR; break;
				case kPixFormat_YUV420_Planar_FR:		dstl.format = kPixFormat_YUV420_Planar_709_FR; break;
				case kPixFormat_YUV411_Planar_FR:		dstl.format = kPixFormat_YUV411_Planar_709_FR; break;
				case kPixFormat_YUV410_Planar_FR:		dstl.format = kPixFormat_YUV410_Planar_709_FR; break;
				case kPixFormat_YUV420i_Planar:			dstl.format = kPixFormat_YUV420i_Planar_709; break;
				case kPixFormat_YUV420i_Planar_FR:		dstl.format = kPixFormat_YUV420i_Planar_709_FR; break;
				case kPixFormat_YUV420it_Planar:		dstl.format = kPixFormat_YUV420it_Planar_709; break;
				case kPixFormat_YUV420it_Planar_FR:		dstl.format = kPixFormat_YUV420it_Planar_709_FR; break;
				case kPixFormat_YUV420ib_Planar:		dstl.format = kPixFormat_YUV420ib_Planar_709; break;
				case kPixFormat_YUV420ib_Planar_FR:		dstl.format = kPixFormat_YUV420ib_Planar_709_FR; break;
				default:
					return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_NOT_SUPPORTED;
			}
			break;
	}

	switch(mConfig.mColorRangeMode) {
		case VDVFilterAliasFormatConfig::kColorRangeMode_Limited:
			switch(dstl.format) {
				case kPixFormat_Y8:
				case kPixFormat_YUV422_UYVY:
				case kPixFormat_YUV422_YUYV:
				case kPixFormat_YUV444_Planar:
				case kPixFormat_YUV422_Planar:
				case kPixFormat_YUV420_Planar:
				case kPixFormat_YUV411_Planar:
				case kPixFormat_YUV410_Planar:
				case kPixFormat_YUV422_UYVY_709:
				case kPixFormat_YUV422_YUYV_709:
				case kPixFormat_YUV444_Planar_709:
				case kPixFormat_YUV422_Planar_709:
				case kPixFormat_YUV420_Planar_709:
				case kPixFormat_YUV411_Planar_709:
				case kPixFormat_YUV410_Planar_709:
				case kPixFormat_YUV420i_Planar:
				case kPixFormat_YUV420i_Planar_709:
				case kPixFormat_YUV420it_Planar:
				case kPixFormat_YUV420it_Planar_709:
				case kPixFormat_YUV420ib_Planar:
				case kPixFormat_YUV420ib_Planar_709:
					break;

				case kPixFormat_Y8_FR:					dstl.format = kPixFormat_Y8; break;
				case kPixFormat_YUV422_UYVY_FR:			dstl.format = kPixFormat_YUV422_UYVY; break;
				case kPixFormat_YUV422_YUYV_FR:			dstl.format = kPixFormat_YUV422_YUYV; break;
				case kPixFormat_YUV444_Planar_FR:		dstl.format = kPixFormat_YUV444_Planar; break;
				case kPixFormat_YUV422_Planar_FR:		dstl.format = kPixFormat_YUV422_Planar; break;
				case kPixFormat_YUV420_Planar_FR:		dstl.format = kPixFormat_YUV420_Planar; break;
				case kPixFormat_YUV411_Planar_FR:		dstl.format = kPixFormat_YUV411_Planar; break;
				case kPixFormat_YUV410_Planar_FR:		dstl.format = kPixFormat_YUV410_Planar; break;
				case kPixFormat_YUV422_UYVY_709_FR:		dstl.format = kPixFormat_YUV422_UYVY_709; break;
				case kPixFormat_YUV422_YUYV_709_FR:		dstl.format = kPixFormat_YUV422_YUYV_709; break;
				case kPixFormat_YUV444_Planar_709_FR:	dstl.format = kPixFormat_YUV444_Planar_709; break;
				case kPixFormat_YUV422_Planar_709_FR:	dstl.format = kPixFormat_YUV422_Planar_709; break;
				case kPixFormat_YUV420_Planar_709_FR:	dstl.format = kPixFormat_YUV420_Planar_709; break;
				case kPixFormat_YUV411_Planar_709_FR:	dstl.format = kPixFormat_YUV411_Planar_709; break;
				case kPixFormat_YUV410_Planar_709_FR:	dstl.format = kPixFormat_YUV410_Planar_709; break;
				case kPixFormat_YUV420i_Planar_FR:		dstl.format = kPixFormat_YUV420i_Planar; break;
				case kPixFormat_YUV420i_Planar_709_FR:	dstl.format = kPixFormat_YUV420i_Planar_709; break;
				case kPixFormat_YUV420it_Planar_FR:		dstl.format = kPixFormat_YUV420it_Planar; break;
				case kPixFormat_YUV420it_Planar_709_FR:	dstl.format = kPixFormat_YUV420it_Planar_709; break;
				case kPixFormat_YUV420ib_Planar_FR:		dstl.format = kPixFormat_YUV420ib_Planar; break;
				case kPixFormat_YUV420ib_Planar_709_FR:	dstl.format = kPixFormat_YUV420ib_Planar_709; break;
				default:
					return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_NOT_SUPPORTED;
			}
			break;

		case VDVFilterAliasFormatConfig::kColorRangeMode_Full:
			switch(dstl.format) {
				case kPixFormat_Y8_FR:
				case kPixFormat_YUV422_UYVY_FR:
				case kPixFormat_YUV422_YUYV_FR:
				case kPixFormat_YUV444_Planar_FR:
				case kPixFormat_YUV422_Planar_FR:
				case kPixFormat_YUV420_Planar_FR:
				case kPixFormat_YUV411_Planar_FR:
				case kPixFormat_YUV410_Planar_FR:
				case kPixFormat_YUV422_UYVY_709_FR:
				case kPixFormat_YUV422_YUYV_709_FR:
				case kPixFormat_YUV444_Planar_709_FR:
				case kPixFormat_YUV422_Planar_709_FR:
				case kPixFormat_YUV420_Planar_709_FR:
				case kPixFormat_YUV411_Planar_709_FR:
				case kPixFormat_YUV410_Planar_709_FR:
				case kPixFormat_YUV420i_Planar_FR:
				case kPixFormat_YUV420i_Planar_709_FR:
				case kPixFormat_YUV420it_Planar_FR:
				case kPixFormat_YUV420it_Planar_709_FR:
				case kPixFormat_YUV420ib_Planar_FR:
				case kPixFormat_YUV420ib_Planar_709_FR:
					break;

				case kPixFormat_Y8:						dstl.format = kPixFormat_Y8_FR; break;
				case kPixFormat_YUV422_UYVY:			dstl.format = kPixFormat_YUV422_UYVY_FR; break;
				case kPixFormat_YUV422_YUYV:			dstl.format = kPixFormat_YUV422_YUYV_FR; break;
				case kPixFormat_YUV444_Planar:			dstl.format = kPixFormat_YUV444_Planar_FR; break;
				case kPixFormat_YUV422_Planar:			dstl.format = kPixFormat_YUV422_Planar_FR; break;
				case kPixFormat_YUV420_Planar:			dstl.format = kPixFormat_YUV420_Planar_FR; break;
				case kPixFormat_YUV411_Planar:			dstl.format = kPixFormat_YUV411_Planar_FR; break;
				case kPixFormat_YUV410_Planar:			dstl.format = kPixFormat_YUV410_Planar_FR; break;
				case kPixFormat_YUV422_UYVY_709:		dstl.format = kPixFormat_YUV422_UYVY_709_FR; break;
				case kPixFormat_YUV422_YUYV_709:		dstl.format = kPixFormat_YUV422_YUYV_709_FR; break;
				case kPixFormat_YUV444_Planar_709:		dstl.format = kPixFormat_YUV444_Planar_709_FR; break;
				case kPixFormat_YUV422_Planar_709:		dstl.format = kPixFormat_YUV422_Planar_709_FR; break;
				case kPixFormat_YUV420_Planar_709:		dstl.format = kPixFormat_YUV420_Planar_709_FR; break;
				case kPixFormat_YUV411_Planar_709:		dstl.format = kPixFormat_YUV411_Planar_709_FR; break;
				case kPixFormat_YUV410_Planar_709:		dstl.format = kPixFormat_YUV410_Planar_709_FR; break;
				case kPixFormat_YUV420i_Planar:			dstl.format = kPixFormat_YUV420i_Planar_FR; break;
				case kPixFormat_YUV420i_Planar_709:		dstl.format = kPixFormat_YUV420i_Planar_709_FR; break;
				case kPixFormat_YUV420it_Planar:		dstl.format = kPixFormat_YUV420it_Planar_FR; break;
				case kPixFormat_YUV420it_Planar_709:	dstl.format = kPixFormat_YUV420it_Planar_709_FR; break;
				case kPixFormat_YUV420ib_Planar:		dstl.format = kPixFormat_YUV420ib_Planar_FR; break;
				case kPixFormat_YUV420ib_Planar_709:	dstl.format = kPixFormat_YUV420ib_Planar_709_FR; break;
					break;

				default:
					return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_NOT_SUPPORTED;
			}
			break;
	}

	if (fa->src.mpPixmapLayout->format > kPixFormat_YUV420ib_Planar_709_FR)
		return FILTERPARAM_NOT_SUPPORTED;

	dstl.pitch = fa->src.mpPixmapLayout->pitch;

	return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM;
}

void VDVFilterAliasFormat::Run() {
}

bool VDVFilterAliasFormat::Configure(VDXHWND hwnd) {
	VDVFilterAliasFormatConfigDialog dlg(mConfig);

	if (!dlg.ShowDialog((VDGUIHandle)hwnd))
		return false;

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

	SafePrintf(buf, maxlen, " (%s, %s)", kColorMode[mConfig.mColorSpaceMode], kRangeMode[mConfig.mColorRangeMode]);
}

void VDVFilterAliasFormat::GetScriptString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, "Config(%d, %d)", mConfig.mColorSpaceMode, mConfig.mColorRangeMode);
}

void VDVFilterAliasFormat::ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc) {
	int colorMode = argv[0].asInt();
	int levelMode = argv[1].asInt();

	if (colorMode < 0 || colorMode > VDVFilterAliasFormatConfig::kColorSpaceModeCount)
		isi->ScriptError(VDXScriptError::FCALL_OUT_OF_RANGE);

	if (levelMode < 0 || levelMode > VDVFilterAliasFormatConfig::kColorRangeModeCount)
		isi->ScriptError(VDXScriptError::FCALL_OUT_OF_RANGE);

	mConfig.mColorSpaceMode = (VDVFilterAliasFormatConfig::ColorSpaceMode)colorMode;
	mConfig.mColorRangeMode = (VDVFilterAliasFormatConfig::ColorRangeMode)levelMode;
}

VDXVF_BEGIN_SCRIPT_METHODS(VDVFilterAliasFormat)
VDXVF_DEFINE_SCRIPT_METHOD(VDVFilterAliasFormat, ScriptConfig, "ii")
VDXVF_END_SCRIPT_METHODS()

extern const VDXFilterDefinition g_VDVFAliasFormat = VDXVideoFilterDefinition<VDVFilterAliasFormat>(NULL, "alias format", "Relabel video with a different color space or color encoding without changing video data.");

// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{24,{flat}}' }'' : unreferenced local function has been removed
#pragma warning(disable: 4505)
