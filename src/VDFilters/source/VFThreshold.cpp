//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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
#include <commctrl.h>

#include "resource.h"
#include "SingleValueDialog.h"

#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofilt.h>
#include <vd2/VDXFrame/VideoFilter.h>

extern HINSTANCE g_hInst;

extern "C" void asm_threshold_run(
		void *dst,
		unsigned long width,
		unsigned long height,
		unsigned long stride,
		unsigned long threshold
		);

///////////////////////////////////

class VDVFThreshold : public VDXVideoFilter {
public:
	VDVFThreshold();

	uint32 GetParams();
	void Run();
	bool Configure(VDXHWND hwnd);
	void GetSettingString(char *buf, int maxlen);
	void GetScriptString(char *buf, int maxlen);

	VDXVF_DECLARE_SCRIPT_METHODS();

	void ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc);

protected:
	static void Update(long value, void *pvThis);

	sint32 mThreshold;
};

VDVFThreshold::VDVFThreshold()
	: mThreshold(128)
{
}

uint32 VDVFThreshold::GetParams() {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	pxldst.data = pxlsrc.data;
	pxldst.pitch = pxlsrc.pitch;

	return FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

void VDVFThreshold::Run() {
#ifdef _M_IX86
	asm_threshold_run(
			fa->src.data,
			fa->src.w,
			fa->src.h,
			fa->src.pitch,
			mThreshold
			);
#else
	ptrdiff_t pitch = fa->dst.pitch;
	uint8 *row = (uint8 *)fa->dst.data;
	uint32 h = fa->dst.h;
	uint32 w = fa->dst.w;
	sint32 addend = 0x80000000 - (mThreshold << 8);

	for(uint32 y=0; y<h; ++y) {
		uint8 *p = row;

		for(uint32 x=0; x<w; ++x) {
			int b = p[0];
			int g = p[1];
			int r = p[2];
			int y = 54*r + 183*g + 19*b;

			*(uint32 *)p = (addend + y) >> 31;
			p += 4;
		}

		row += pitch;
	}
#endif
}

bool VDVFThreshold::Configure(VDXHWND hwnd) {
	if (!hwnd)
		return true;

	sint32 result;
	if (!VDFilterGetSingleValue(hwnd, mThreshold, &result, 0, 256, "threshold", fa->ifp2, Update, this)) {
		mThreshold = result;
		return false;
	}

	mThreshold = result;
	return true;
}

void VDVFThreshold::GetSettingString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, " (%d%%)", (mThreshold*25)/64);
}

void VDVFThreshold::GetScriptString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, "Config(%d)", mThreshold);
}

VDXVF_BEGIN_SCRIPT_METHODS(VDVFThreshold)
	VDXVF_DEFINE_SCRIPT_METHOD(VDVFThreshold, ScriptConfig, "i")
VDXVF_END_SCRIPT_METHODS()

void VDVFThreshold::ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc) {
	mThreshold = argv[0].asInt();
}

void VDVFThreshold::Update(long value, void *pvThis) {
	((VDVFThreshold *)pvThis)->mThreshold = value;
}

extern const VDXFilterDefinition g_VDVFThreshold = VDXVideoFilterDefinition<VDVFThreshold>(
	NULL,
	"threshold",
	"Converts an image to black and white by comparing brightness values.\n\n[Assembly optimized]");
