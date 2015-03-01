//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2011 Avery Lee
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
#include <vd2/system/cpuaccel.h>
#include <vd2/system/math.h>
#include <vd2/system/strutil.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/plugin/vdvideoaccel.h>
#include "DSP.h"
#include "x86/DSP_SSE2.h"
#include "resource.h"
#include "VFMergeLayers.inl"

///////////////////////////////////////////////////////////////////////////////

namespace {
	static const wchar_t *const kBlendModes[]={
		L"Lerp",
		L"Select",
	};
}

struct VDVFMergeLayersConfig {
	enum Mode {
		kMode_Lerp,
		kMode_Select,
		kModeCount
	};

	Mode mMode;

	VDVFMergeLayersConfig()
		: mMode(kMode_Lerp)
	{
	}
};

///////////////////////////////////////////////////////////////////////////////

class VDVFMergeLayersDialog : public VDDialogFrameW32 {
public:
	VDVFMergeLayersDialog(VDVFMergeLayersConfig& config, IVDXFilterPreview2 *ifp2);

protected:
	bool OnLoaded();
	void OnDataExchange(bool write);
	bool OnCommand(uint32 id, uint32 extcode);
	void UpdateAmountText();
	void OnSelectionChanged(VDUIProxyListBoxControl *sender, int selIdx);

	VDVFMergeLayersConfig& mConfig;
	IVDXFilterPreview2 *const mifp2;

	VDUIProxyListBoxControl mListBox;

	VDDelegate mDelSelectionChanged;
};

VDVFMergeLayersDialog::VDVFMergeLayersDialog(VDVFMergeLayersConfig& config, IVDXFilterPreview2 *ifp2)
	: VDDialogFrameW32(IDD_FILTER_MERGELAYERS)
	, mConfig(config)
	, mifp2(ifp2)
{
	mListBox.OnSelectionChanged() += mDelSelectionChanged.Bind(this, &VDVFMergeLayersDialog::OnSelectionChanged);
}

bool VDVFMergeLayersDialog::OnLoaded() {
	AddProxy(&mListBox, IDC_BLEND_MODE);

	if (mifp2)
		mifp2->InitButton((VDXHWND)GetControl(IDC_PREVIEW));

	for(size_t i = 0; i < sizeof(kBlendModes)/sizeof(kBlendModes[0]); ++i)
		mListBox.AddItem(kBlendModes[i], i);

	VDASSERTCT(sizeof(kBlendModes)/sizeof(kBlendModes[0]) == VDVFMergeLayersConfig::kModeCount);

	OnDataExchange(false);
	SetFocusToControl(IDC_BLEND_MODE);
	return true;
}

void VDVFMergeLayersDialog::OnDataExchange(bool write) {
	if (write) {
		int sel = mListBox.GetSelection();

		if (sel < 0) {
			FailValidation(IDC_BLEND_MODE);
			return;
		}

		mConfig.mMode = (VDVFMergeLayersConfig::Mode)sel;
	} else {
		mListBox.SetSelection(mConfig.mMode);
	}
}

bool VDVFMergeLayersDialog::OnCommand(uint32 id, uint32 extcode) {
	if (id == IDC_PREVIEW) {
		if (mifp2)
			mifp2->Toggle((VDXHWND)mhdlg);

		return true;
	}

	return false;
}

void VDVFMergeLayersDialog::OnSelectionChanged(VDUIProxyListBoxControl *sender, int selIdx) {
	if (selIdx >= 0 && selIdx != mConfig.mMode) {
		mConfig.mMode = (VDVFMergeLayersConfig::Mode)selIdx;

		if (mifp2)
			mifp2->RedoFrame();
	}
}

///////////////////////////////////////////////////////////////////////////////

class VDVFMergeLayers : public VDXVideoFilter {
public:
	VDVFMergeLayers();

	uint32 GetParams();
	void Run();

	void StartAccel(IVDXAContext *vdxa);
	void RunAccel(IVDXAContext *vdxa);
	void StopAccel(IVDXAContext *vdxa);

	bool Configure(VDXHWND hwnd);
	bool Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher);
	void GetSettingString(char *buf, int maxlen);
	void GetScriptString(char *buf, int maxlen);

	VDXVF_DECLARE_SCRIPT_METHODS();

	void OnScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc);

	enum {
		kMinInputCount = 3,
		kMaxInputCount = 3
	};

protected:
	VDVFMergeLayersConfig mConfig;

	uint32	mVDXAShader;
};

VDVFMergeLayers::VDVFMergeLayers()
	: mVDXAShader(0)
{
}

uint32 VDVFMergeLayers::GetParams() {
	const uint32 numSources = fa->mSourceStreamCount;
	const VDXPixmapLayout& src0 = *fa->mpSourceStreams[0]->mpPixmapLayout;

	switch(src0.format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
		case nsVDXPixmap::kPixFormat_VDXA_RGB:
			break;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
	}

	for(uint32 i=1; i<numSources; ++i) {
		const VDXPixmapLayout& src1 = *fa->mpSourceStreams[i]->mpPixmapLayout;

		if (src0.format != src1.format)
			return FILTERPARAM_NOT_SUPPORTED;

		if (src0.w != src1.w)
			return FILTERPARAM_NOT_SUPPORTED;

		if (src0.h != src1.h)
			return FILTERPARAM_NOT_SUPPORTED;
	}

	VDXFBitmap& dst = fa->dst;

	dst.depth = 0;
	dst.mpPixmapLayout->pitch = 0;

	return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_ALIGN_SCANLINES;
}

void VDVFMergeLayers::Run() {
	const VDXPixmap& dst = *fa->dst.mpPixmap;
	const VDXPixmap& src1 = *fa->mpSourceFrames[0]->mpPixmap;
	const VDXPixmap& src2 = *fa->mpSourceFrames[1]->mpPixmap;
	const VDXPixmap& src3 = *fa->mpSourceFrames[2]->mpPixmap;
	const uint32 w16 = (dst.w + 3) >> 2;
	const uint32 h = dst.h;

	uint8 *dstp = (uint8 *)dst.data;
	const ptrdiff_t dstpitch = dst.pitch;
	const uint8 *srcp1 = (uint8 *)src1.data;
	const ptrdiff_t src1pitch = src1.pitch;
	const uint8 *srcp2 = (uint8 *)src2.data;
	const ptrdiff_t src2pitch = src2.pitch;
	const uint8 *srcp3 = (uint8 *)src3.data;
	const ptrdiff_t src3pitch = src3.pitch;

	const bool sse2 = (ff->getCPUFlags() & CPUF_SUPPORTS_SSE2) != 0;

	switch(mConfig.mMode) {
		case VDVFMergeLayersConfig::kMode_Lerp:
			if (sse2)
				VDDSPProcessPlane4(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, srcp3, src3pitch, w16, h, VDDSPBlend8_Lerp_SSE2);
			else
				VDDSPProcessPlane4(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, srcp3, src3pitch, w16, h, VDDSPBlend8_Lerp);
			return;

		case VDVFMergeLayersConfig::kMode_Select:
			if (sse2)
				VDDSPProcessPlane4(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, srcp3, src3pitch, w16, h, VDDSPBlend8_Select_SSE2);
			else
				VDDSPProcessPlane4(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, srcp3, src3pitch, w16, h, VDDSPBlend8_Select);
			break;

	}
}

void VDVFMergeLayers::StartAccel(IVDXAContext *vdxa) {
	const void *bytecode = NULL;
	size_t bclen = 0;

	switch(mConfig.mMode) {
		default:
		case VDVFMergeLayersConfig::kMode_Lerp:
			bytecode = kVDVFMergeLayersFP_Lerp;
			bclen = sizeof kVDVFMergeLayersFP_Lerp;
			break;

		case VDVFMergeLayersConfig::kMode_Select:
			bytecode = kVDVFMergeLayersFP_Select;
			bclen = sizeof kVDVFMergeLayersFP_Select;
			break;
	}

	mVDXAShader = vdxa->CreateFragmentProgram(kVDXAPF_D3D9ByteCodePS20, bytecode, bclen);
}

void VDVFMergeLayers::RunAccel(IVDXAContext *vdxa) {
	VDXFBitmap& src0 = *fa->mpSourceFrames[0];
	VDXFBitmap& src1 = *fa->mpSourceFrames[1];
	VDXFBitmap& src2 = *fa->mpSourceFrames[2];

	vdxa->SetSampler(0, src0.mVDXAHandle, kVDXAFilt_Point);
	vdxa->SetSampler(1, src1.mVDXAHandle, kVDXAFilt_Point);
	vdxa->SetSampler(2, src2.mVDXAHandle, kVDXAFilt_Point);

	vdxa->SetTextureMatrix(0, src0.mVDXAHandle, 0, 0, NULL);
	vdxa->SetTextureMatrix(1, src1.mVDXAHandle, 0, 0, NULL);
	vdxa->SetTextureMatrix(2, src2.mVDXAHandle, 0, 0, NULL);

	vdxa->DrawRect(fa->dst.mVDXAHandle, mVDXAShader, NULL);
}

void VDVFMergeLayers::StopAccel(IVDXAContext *vdxa) {
	if (mVDXAShader) {
		vdxa->DestroyObject(mVDXAShader);
		mVDXAShader = 0;
	}
}

bool VDVFMergeLayers::Configure(VDXHWND hwnd) {
	if (!hwnd)
		return true;

	VDVFMergeLayersConfig cfg(mConfig);
	VDVFMergeLayersDialog dlg(mConfig, fa->ifp2);
	if (dlg.ShowDialog((VDGUIHandle)hwnd))
		return true;

	mConfig = cfg;
	return false;
}

bool VDVFMergeLayers::Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher) {
	for(uint32 i=0; i<fa->mSourceStreamCount; ++i)
		prefetcher->PrefetchFrame(i, frame, 0);

	return true;
}

void VDVFMergeLayers::GetSettingString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, " (%ls)", kBlendModes[mConfig.mMode]);
}

void VDVFMergeLayers::GetScriptString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, "Config(%d)", mConfig.mMode);
}

VDXVF_BEGIN_SCRIPT_METHODS(VDVFMergeLayers)
	VDXVF_DEFINE_SCRIPT_METHOD(VDVFMergeLayers, OnScriptConfig, "i")
VDXVF_END_SCRIPT_METHODS()

void VDVFMergeLayers::OnScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc) {
	uint32 mode = (uint32)argv[0].asInt();

	if (mode >= VDVFMergeLayersConfig::kModeCount)
		mode = 0;

	mConfig.mMode = (VDVFMergeLayersConfig::Mode)mode;
}

extern const VDXFilterDefinition g_VDVFMergeLayers = VDXVideoFilterDefinition<VDVFMergeLayers>(NULL, "merge layers", "Blends two layers together using a third layer.");

// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{24,{flat}}' }'' : unreferenced local function has been removed
#pragma warning(disable: 4505)
