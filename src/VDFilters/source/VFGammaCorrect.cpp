//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2009 Avery Lee
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
#include <vd2/system/math.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDLib/Dialog.h>
#include "resource.h"

struct VDVideoFilterGammaCorrectConfig {
	bool mbConvertToLinear;

	VDVideoFilterGammaCorrectConfig()
		: mbConvertToLinear(false)
	{
	}
};

class VDVideoFilterGammaCorrectDialog : public VDDialogFrameW32 {
public:
	VDVideoFilterGammaCorrectDialog(VDVideoFilterGammaCorrectConfig& config);

protected:
	void OnDataExchange(bool write);

	VDVideoFilterGammaCorrectConfig& mConfig;
};

VDVideoFilterGammaCorrectDialog::VDVideoFilterGammaCorrectDialog(VDVideoFilterGammaCorrectConfig& config)
	: VDDialogFrameW32(IDD_FILTER_GAMMACORRECT)
	, mConfig(config)
{
}

void VDVideoFilterGammaCorrectDialog::OnDataExchange(bool write) {
	if (write) {
		mConfig.mbConvertToLinear = IsButtonChecked(IDC_SRGBTOLINEAR);
	} else {
		if (mConfig.mbConvertToLinear)
			CheckButton(IDC_SRGBTOLINEAR, true);
		else
			CheckButton(IDC_LINEARTOSRGB, true);
	}
}

class VDVideoFilterGammaCorrect : public VDXVideoFilter {
public:
	VDVideoFilterGammaCorrect() { mLookup16=0; }
	VDVideoFilterGammaCorrect(const VDVideoFilterGammaCorrect& a) { mLookup16=0; mConfig = a.mConfig; }
	~VDVideoFilterGammaCorrect() { free(mLookup16); }
	uint32 GetParams();
	void Start();
	void End();
	void Run();
	bool Configure(VDXHWND hwnd);
	void GetSettingString(char *buf, int maxlen);
	void GetScriptString(char *buf, int maxlen);

	VDXVF_DECLARE_SCRIPT_METHODS();
	void ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc);

protected:
	VDVideoFilterGammaCorrectConfig mConfig;

	uint8 mLookup[256];
	uint16* mLookup16;
};

VDXVF_BEGIN_SCRIPT_METHODS(VDVideoFilterGammaCorrect)
	VDXVF_DEFINE_SCRIPT_METHOD(VDVideoFilterGammaCorrect, ScriptConfig, "0")
VDXVF_END_SCRIPT_METHODS()

uint32 VDVideoFilterGammaCorrect::GetParams() {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	switch (pxlsrc.format) {
	case nsVDXPixmap::kPixFormat_XRGB8888:
	case nsVDXPixmap::kPixFormat_XRGB64:
		break;
	default:
		return FILTERPARAM_NOT_SUPPORTED;
	}

	pxldst.pitch = pxlsrc.pitch;

	return FILTERPARAM_ALIGN_SCANLINES | FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_NORMALIZE16;
}

void VDVideoFilterGammaCorrect::Start() {
	const float a = 0.055f;
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;

	if (pxlsrc.format==nsVDXPixmap::kPixFormat_XRGB8888) {
		if (mConfig.mbConvertToLinear) {
			for(int i=0; i<256; ++i) {
				float x = i / 255.0f;
				float y;

				if (x <= 0.04045f)
					y = x / 12.92f;
				else
					y = powf((x + a)/(1.0f + a), 2.4f);

				mLookup[i] = (uint8)VDRoundToIntFast(y * 255.0f);
			}
		} else {
			for(int i=0; i<256; ++i) {
				float x = i / 255.0f;
				float y;

				if (x <= 0.0031808f)
					y = x * 12.92f;
				else
					y = (1.0f + a) * powf(x, 1.0f / 2.4f) - a;

				mLookup[i] = (uint8)VDRoundToIntFast(y * 255.0f);
			}
		}
	}

	if (pxlsrc.format==nsVDXPixmap::kPixFormat_XRGB64) {
		if (!mLookup16) mLookup16 = (uint16*)malloc(65536*2);
		if (mConfig.mbConvertToLinear) {
			for(int i=0; i<65536; ++i) {
				float x = i / 65535.0f;
				float y;

				if (x <= 0.04045f)
					y = x / 12.92f;
				else
					y = powf((x + a)/(1.0f + a), 2.4f);

				mLookup16[i] = (uint16)VDRoundToIntFast(y * 65535.0f);
			}
		} else {
			for(int i=0; i<65536; ++i) {
				float x = i / 65535.0f;
				float y;

				if (x <= 0.0031808f)
					y = x * 12.92f;
				else
					y = (1.0f + a) * powf(x, 1.0f / 2.4f) - a;

				mLookup16[i] = (uint16)VDRoundToIntFast(y * 65535.0f);
			}
		}
	}
}

void VDVideoFilterGammaCorrect::End() {
	if (mLookup16) {
		free (mLookup16);
		mLookup16 = 0;
	}
}

void VDVideoFilterGammaCorrect::Run() {
	const VDXPixmap& pxdst = *fa->dst.mpPixmap;
	const uint32 w = pxdst.w;
	const uint32 h = pxdst.h;

	if (pxdst.format==nsVDXPixmap::kPixFormat_XRGB8888) {
		const uint8 *VDRESTRICT tab = mLookup;

		uint8 *dst = (uint8 *)pxdst.data;
		for(uint32 y=0; y<h; ++y) {
			uint8 *p = dst;

			for(uint32 x=0; x<w; ++x) {
				p[0] = tab[p[0]];
				p[1] = tab[p[1]];
				p[2] = tab[p[2]];

				p += 4;
			}

			dst += pxdst.pitch;
		}
	}

	if (pxdst.format==nsVDXPixmap::kPixFormat_XRGB64) {
		const uint16 *VDRESTRICT tab = mLookup16;

		uint8 *dst = (uint8 *)pxdst.data;
		for(uint32 y=0; y<h; ++y) {
			uint16 *p = (uint16*)dst;

			for(uint32 x=0; x<w; ++x) {
				p[0] = tab[p[0]];
				p[1] = tab[p[1]];
				p[2] = tab[p[2]];

				p += 4;
			}

			dst += pxdst.pitch;
		}
	}
}

bool VDVideoFilterGammaCorrect::Configure(VDXHWND hwnd) {
	VDVideoFilterGammaCorrectDialog dlg(mConfig);

	return dlg.ShowDialog((VDGUIHandle)hwnd) != 0;
}

void VDVideoFilterGammaCorrect::GetSettingString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, " (%s)", mConfig.mbConvertToLinear ? "to linear" : "to sRGB");
}

void VDVideoFilterGammaCorrect::GetScriptString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, "Config(%d)", mConfig.mbConvertToLinear);
}

void VDVideoFilterGammaCorrect::ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc) {
	mConfig.mbConvertToLinear = (0 != argv[0].asInt());
}

extern const VDXFilterDefinition g_VDVFGammaCorrect = VDXVideoFilterDefinition<VDVideoFilterGammaCorrect>(
	NULL,
	"gamma correct",
	"Converts video color representation between gamma space and linear space."
	);

// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{48,{flat}}' }'' : unreferenced local function has been removed
#pragma warning(disable: 4505)
