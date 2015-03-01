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
#include "resource.h"
#include <vd2/system/cpuaccel.h>
#include <vd2/system/fraction.h>
#include <vd2/system/memory.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/VDXFrame/VideoFilter.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
struct VDVideoFilterFieldDelayConfig {
	bool mbCurTFF;

	VDVideoFilterFieldDelayConfig()
		: mbCurTFF(false)
	{
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
class VDVideoFilterFieldDelayDialog : public VDDialogFrameW32 {
public:
	VDVideoFilterFieldDelayDialog(VDVideoFilterFieldDelayConfig& config, IVDXFilterPreview *ifp)
		: VDDialogFrameW32(IDD_FILTER_FIELDDELAY)
		, mConfig(config)
		, mifp(ifp)
	{}

protected:
	bool OnLoaded();
	void OnDataExchange(bool write);
	bool OnCommand(uint32 id, uint32 excode);

	VDVideoFilterFieldDelayConfig& mConfig;
	IVDXFilterPreview *mifp;
};

bool VDVideoFilterFieldDelayDialog::OnLoaded() {
	if (mifp)
		mifp->InitButton((VDXHWND)GetControl(IDC_PREVIEW));

	return VDDialogFrameW32::OnLoaded();
}

void VDVideoFilterFieldDelayDialog::OnDataExchange(bool write) {
	if (write) {
		if (IsButtonChecked(IDC_FIELDMODE_TFFTOBFF))
			mConfig.mbCurTFF = true;
		else if (IsButtonChecked(IDC_FIELDMODE_BFFTOTFF))
			mConfig.mbCurTFF = false;
	} else {
		CheckButton(mConfig.mbCurTFF ? IDC_FIELDMODE_TFFTOBFF : IDC_FIELDMODE_BFFTOTFF, true);
	}
}

bool VDVideoFilterFieldDelayDialog::OnCommand(uint32 id, uint32 excode) {
	if (id == IDC_PREVIEW) {
		if (mifp)
			mifp->Toggle((VDXHWND)mhdlg);
		return true;
	} else if (id == IDC_FIELDMODE_TFFTOBFF || id == IDC_FIELDMODE_BFFTOTFF) {
		BeginValidation();
		OnDataExchange(true);
		EndValidation();
		
		if (mifp)
			mifp->RedoFrame();
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
class VDVideoFilterFieldDelay : public VDXVideoFilter {
public:
	uint32 GetParams();
	void Run();
	bool Configure(VDXHWND hwnd);
	void GetSettingString(char *buf, int maxlen);
	void GetScriptString(char *buf, int maxlen);

	bool Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher);

	VDXVF_DECLARE_SCRIPT_METHODS();

protected:
	void ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc);

	uint32		mLumaRowBytes;
	uint32		mChromaRowBytes;
	ptrdiff_t	mLumaPlanePitch;
	ptrdiff_t	mChromaPlanePitch;

	VDVideoFilterFieldDelayConfig mConfig;
};

VDXVF_BEGIN_SCRIPT_METHODS(VDVideoFilterFieldDelay)
	VDXVF_DEFINE_SCRIPT_METHOD(VDVideoFilterFieldDelay, ScriptConfig, "i")
VDXVF_END_SCRIPT_METHODS()

uint32 VDVideoFilterFieldDelay::GetParams() {
	VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	switch(pxlsrc.format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
			mLumaRowBytes = pxldst.w * 4;
			mChromaRowBytes = 0;
			break;

		case nsVDXPixmap::kPixFormat_Y8:
			mLumaRowBytes = pxldst.w;
			mChromaRowBytes = 0;
			break;

		case nsVDXPixmap::kPixFormat_YUV422_UYVY:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_FR:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_FR:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_709:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_709:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_709_FR:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_709_FR:
			mLumaRowBytes = ((pxldst.w + 1) & ~1) * 2;
			mChromaRowBytes = 0;
			break;

		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709_FR:
			mLumaRowBytes = pxldst.w;
			mChromaRowBytes = pxldst.w;
			break;

		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709_FR:
			mLumaRowBytes = pxldst.w;
			mChromaRowBytes = pxldst.w >> 1;
			break;

		case nsVDXPixmap::kPixFormat_YUV411_Planar:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709_FR:
			mLumaRowBytes = pxldst.w;
			mChromaRowBytes = pxldst.w >> 2;
			break;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
	}

	fa->dst.depth = 0;

	return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_ALIGN_SCANLINES;
}

void VDVideoFilterFieldDelay::Run() {
	const int idxt = mConfig.mbCurTFF ? 1 : 0;
	const int idxb = idxt ^ 1;

	const VDXPixmap& pxdst = *fa->mpOutputFrames[0]->mpPixmap;
	const VDXPixmap& pxsrct = *fa->mpSourceFrames[idxt]->mpPixmap;
	const VDXPixmap& pxsrcb = *fa->mpSourceFrames[idxb]->mpPixmap;
	const uint32 ht = (pxdst.h + 1) >> 1;
	const uint32 hb = pxdst.h >> 1;

	VDMemcpyRect(pxdst.data, pxdst.pitch * 2, pxsrct.data, pxsrct.pitch * 2, mLumaRowBytes, ht);
	VDMemcpyRect((char *)pxdst.data + pxdst.pitch, pxdst.pitch * 2, (const char *)pxsrcb.data + pxsrcb.pitch, pxsrcb.pitch * 2, mLumaRowBytes, hb);

	if (mChromaRowBytes) {
		VDMemcpyRect(pxdst.data2, pxdst.pitch2 * 2, pxsrct.data2, pxsrct.pitch2 * 2, mChromaRowBytes, ht);
		VDMemcpyRect((char *)pxdst.data2 + pxdst.pitch2, pxdst.pitch2 * 2, (const char *)pxsrcb.data2 + pxsrcb.pitch2, pxsrcb.pitch2 * 2, mChromaRowBytes, hb);

		VDMemcpyRect(pxdst.data3, pxdst.pitch3 * 2, pxsrct.data3, pxsrct.pitch3 * 2, mChromaRowBytes, ht);
		VDMemcpyRect((char *)pxdst.data3 + pxdst.pitch3, pxdst.pitch3 * 2, (const char *)pxsrcb.data3 + pxsrcb.pitch3, pxsrcb.pitch3 * 2, mChromaRowBytes, hb);
	}
}

bool VDVideoFilterFieldDelay::Configure(VDXHWND hwnd) {
	VDVideoFilterFieldDelayConfig cfg(mConfig);
	VDVideoFilterFieldDelayDialog dlg(mConfig, fa->ifp);

	if (dlg.ShowDialog((VDGUIHandle)hwnd))
		return true;

	mConfig = cfg;
	return false;
}

void VDVideoFilterFieldDelay::GetSettingString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, " (mode: %s)", mConfig.mbCurTFF ? "TFF to BFF" : "BFF to TFF");
}

void VDVideoFilterFieldDelay::GetScriptString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, "Config(%d)", mConfig.mbCurTFF);
}

bool VDVideoFilterFieldDelay::Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher) {
	prefetcher->PrefetchFrame(0, frame, 0);
	prefetcher->PrefetchFrame(0, frame+1, 0);

	return true;
}

void VDVideoFilterFieldDelay::ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc) {
	mConfig.mbCurTFF = 0 != argv[0].asInt();
}

extern const VDXFilterDefinition g_VDVFFieldDelay = VDXVideoFilterDefinition<VDVideoFilterFieldDelay>(
	NULL,
	"field delay",
	"Applies a one-field delay to flip the field dominance of an interlaced video stream."
	);

#ifdef _MSC_VER
	#pragma warning(disable: 4505)	// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{48,{flat}}' }'' : unreferenced local function has been removed
#endif
