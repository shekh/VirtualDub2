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
#include "VFBlendLayers.inl"

///////////////////////////////////////////////////////////////////////////////

namespace {
	static const wchar_t *const kBlendModes[]={
		L"Normal",
		L"Darken",
		L"Lighten",
		L"Linear Dodge (Add)",
		L"Multiply",
		L"Linear Burn",
		L"Color Burn",
		L"Screen",
		L"Color Dodge",
		L"Overlay",
		L"Soft Light",
		L"Hard Light",
		L"Vivid Light",
		L"Linear Light",
		L"Pin Light",
		L"Hard Mix",
		L"Difference",
		L"Exclusion"
	};
}

struct VDVFBlendLayersConfig {
	enum Mode {
		kMode_Lerp,
		kMode_Min,
		kMode_Max,
		kMode_Add,
		kMode_Multiply,
		kMode_LinearBurn,
		kMode_ColorBurn,
		kMode_Screen,
		kMode_ColorDodge,
		kMode_Overlay,
		kMode_SoftLight,
		kMode_HardLight,
		kMode_VividLight,
		kMode_LinearLight,
		kMode_PinLight,
		kMode_HardMix,
		kMode_Difference,
		kMode_Exclusion,
		kModeCount
	};

	Mode mMode;
	float mAmount;

	VDVFBlendLayersConfig()
		: mMode(kMode_Lerp)
		, mAmount(1.0f)
	{
	}
};

///////////////////////////////////////////////////////////////////////////////

class VDVFBlendLayersDialog : public VDDialogFrameW32 {
public:
	VDVFBlendLayersDialog(VDVFBlendLayersConfig& config, IVDXFilterPreview2 *ifp2);

protected:
	bool OnLoaded();
	void OnDataExchange(bool write);
	bool OnCommand(uint32 id, uint32 extcode);
	void OnHScroll(uint32 id, int code);
	void UpdateAmountText();
	void OnSelectionChanged(VDUIProxyListBoxControl *sender, int selIdx);

	VDVFBlendLayersConfig& mConfig;
	IVDXFilterPreview2 *const mifp2;

	VDUIProxyListBoxControl mListBox;

	VDDelegate mDelSelectionChanged;
};

VDVFBlendLayersDialog::VDVFBlendLayersDialog(VDVFBlendLayersConfig& config, IVDXFilterPreview2 *ifp2)
	: VDDialogFrameW32(IDD_FILTER_BLENDLAYERS)
	, mConfig(config)
	, mifp2(ifp2)
{
	mListBox.OnSelectionChanged() += mDelSelectionChanged.Bind(this, &VDVFBlendLayersDialog::OnSelectionChanged);
}

bool VDVFBlendLayersDialog::OnLoaded() {
	AddProxy(&mListBox, IDC_BLEND_MODE);

	if (mifp2)
		mifp2->InitButton((VDXHWND)GetControl(IDC_PREVIEW));

	for(size_t i = 0; i < sizeof(kBlendModes)/sizeof(kBlendModes[0]); ++i)
		mListBox.AddItem(kBlendModes[i], i);

	VDASSERTCT(sizeof(kBlendModes)/sizeof(kBlendModes[0]) == VDVFBlendLayersConfig::kModeCount);

	TBSetRange(IDC_AMOUNT, 0, 100);

	OnDataExchange(false);
	SetFocusToControl(IDC_BLEND_MODE);
	return true;
}

void VDVFBlendLayersDialog::OnDataExchange(bool write) {
	if (write) {
		int sel = mListBox.GetSelection();

		if (sel < 0) {
			FailValidation(IDC_BLEND_MODE);
			return;
		}

		mConfig.mMode = (VDVFBlendLayersConfig::Mode)sel;
		mConfig.mAmount = (float)TBGetValue(IDC_AMOUNT) / 100.0f;
	} else {
		mListBox.SetSelection(mConfig.mMode);
		TBSetValue(IDC_AMOUNT, VDRoundToInt(mConfig.mAmount * 100.0f));
		UpdateAmountText();
	}
}

bool VDVFBlendLayersDialog::OnCommand(uint32 id, uint32 extcode) {
	if (id == IDC_PREVIEW) {
		if (mifp2)
			mifp2->Toggle((VDXHWND)mhdlg);

		return true;
	}

	return false;
}

void VDVFBlendLayersDialog::OnHScroll(uint32 id, int code) {
	float val = (float)TBGetValue(IDC_AMOUNT) / 100.0f;

	if (mConfig.mAmount != val) {
		mConfig.mAmount = val;
		UpdateAmountText();

		if (mifp2)
			mifp2->RedoFrame();
	}
}

void VDVFBlendLayersDialog::UpdateAmountText() {
	SetControlTextF(IDC_STATIC_AMOUNT, L"%.0f%%", mConfig.mAmount * 100.0f);
}

void VDVFBlendLayersDialog::OnSelectionChanged(VDUIProxyListBoxControl *sender, int selIdx) {
	if (selIdx >= 0 && selIdx != mConfig.mMode) {
		mConfig.mMode = (VDVFBlendLayersConfig::Mode)selIdx;

		if (mifp2)
			mifp2->RedoFrame();
	}
}

///////////////////////////////////////////////////////////////////////////////

class VDVFBlendLayers : public VDXVideoFilter {
public:
	VDVFBlendLayers();

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
		kMinInputCount = 2,
		kMaxInputCount = 2
	};

protected:
	VDVFBlendLayersConfig mConfig;

	uint32	mVDXAShader;
};

VDVFBlendLayers::VDVFBlendLayers()
	: mVDXAShader(0)
{
}

uint32 VDVFBlendLayers::GetParams() {
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

void VDVFBlendLayers::Run() {
	const VDXPixmap& dst = *fa->dst.mpPixmap;
	const VDXPixmap& src1 = *fa->mpSourceFrames[0]->mpPixmap;
	const VDXPixmap& src2 = *fa->mpSourceFrames[1]->mpPixmap;
	const uint32 w16 = (dst.w + 3) >> 2;
	const uint32 h = dst.h;

	uint8 *dstp = (uint8 *)dst.data;
	const ptrdiff_t dstpitch = dst.pitch;
	const uint8 *srcp1 = (uint8 *)src1.data;
	const ptrdiff_t src1pitch = src1.pitch;
	const uint8 *srcp2 = (uint8 *)src2.data;
	const ptrdiff_t src2pitch = src2.pitch;

	uint8 factor8 = (uint8)VDRoundToInt(mConfig.mAmount * 255.0f);

	if (!factor8) {
		VDDSPProcessPlane2(dstp, dstpitch, srcp1, src1pitch, w16, h, memcpy);
		return;
	}

	const bool sse2 = (ff->getCPUFlags() & CPUF_SUPPORTS_SSE2) != 0;

	switch(mConfig.mMode) {
		case VDVFBlendLayersConfig::kMode_Lerp:
			if (factor8 == 255)
				VDDSPProcessPlane2(dstp, dstpitch, srcp2, src2pitch, w16 * 16, h, memcpy);
			else if (sse2)
				VDDSPProcessPlane3A(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_LerpConst_SSE2, factor8);
			else
				VDDSPProcessPlane3A(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_LerpConst, factor8);
			return;

		case VDVFBlendLayersConfig::kMode_Min:
			if (sse2)
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_Min_SSE2);
			else
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_Min);
			break;

		case VDVFBlendLayersConfig::kMode_Max:
			if (sse2)
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_Max_SSE2);
			else
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_Max);
			break;

		case VDVFBlendLayersConfig::kMode_Add:
			if (sse2)
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_Add_SSE2);
			else
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_Add);
			break;

		case VDVFBlendLayersConfig::kMode_Multiply:
			if (sse2)
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_Multiply_SSE2);
			else
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_Multiply);
			break;

		case VDVFBlendLayersConfig::kMode_LinearBurn:
			if (sse2)
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_LinearBurn_SSE2);
			else
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_LinearBurn);
			break;

		case VDVFBlendLayersConfig::kMode_ColorBurn:
			VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_ColorBurn);
			break;

		case VDVFBlendLayersConfig::kMode_Screen:
			if (sse2)
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_Screen_SSE2);
			else
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_Screen);
			break;

		case VDVFBlendLayersConfig::kMode_ColorDodge:
			VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_ColorDodge);
			break;

		case VDVFBlendLayersConfig::kMode_Overlay:
			if (sse2)
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_Overlay_SSE2);
			else
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_Overlay);
			break;

		case VDVFBlendLayersConfig::kMode_SoftLight:
			VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_SoftLight);
			break;

		case VDVFBlendLayersConfig::kMode_HardLight:
			if (sse2)
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_HardLight_SSE2);
			else
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_HardLight);
			break;

		case VDVFBlendLayersConfig::kMode_LinearLight:
			if (sse2)
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_LinearLight_SSE2);
			else
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_LinearLight);
			break;

		case VDVFBlendLayersConfig::kMode_VividLight:
			VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_VividLight);
			break;

		case VDVFBlendLayersConfig::kMode_PinLight:
			if (sse2)
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_PinLight_SSE2);
			else
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_PinLight);
			break;

		case VDVFBlendLayersConfig::kMode_HardMix:
			if (sse2)
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_HardMix_SSE2);
			else
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_HardMix);
			break;

		case VDVFBlendLayersConfig::kMode_Difference:
			if (sse2)
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_Difference_SSE2);
			else
				VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_Difference);
			break;

		case VDVFBlendLayersConfig::kMode_Exclusion:
			VDDSPProcessPlane3(dstp, dstpitch, srcp1, src1pitch, srcp2, src2pitch, w16, h, VDDSPBlend8_Exclusion);
			break;
	}

	if (factor8 != 255)
		VDDSPProcessPlane3A(dstp, dstpitch, dstp, dstpitch, srcp1, src1pitch, w16, h, VDDSPBlend8_LerpConst, 255 - factor8);
}

void VDVFBlendLayers::StartAccel(IVDXAContext *vdxa) {
	const void *bytecode = NULL;
	size_t bclen = 0;

	switch(mConfig.mMode) {
		default:
		case VDVFBlendLayersConfig::kMode_Lerp:
			bytecode = kVDVFBlendLayersFP_Lerp;
			bclen = sizeof kVDVFBlendLayersFP_Lerp;
			break;

		case VDVFBlendLayersConfig::kMode_Min:
			bytecode = kVDVFBlendLayersFP_Min;
			bclen = sizeof kVDVFBlendLayersFP_Min;
			break;

		case VDVFBlendLayersConfig::kMode_Max:
			bytecode = kVDVFBlendLayersFP_Max;
			bclen = sizeof kVDVFBlendLayersFP_Max;
			break;

		case VDVFBlendLayersConfig::kMode_Add:
			bytecode = kVDVFBlendLayersFP_Add;
			bclen = sizeof kVDVFBlendLayersFP_Add;
			break;

		case VDVFBlendLayersConfig::kMode_Multiply:
			bytecode = kVDVFBlendLayersFP_Multiply;
			bclen = sizeof kVDVFBlendLayersFP_Multiply;
			break;

		case VDVFBlendLayersConfig::kMode_LinearBurn:
			bytecode = kVDVFBlendLayersFP_LinearBurn;
			bclen = sizeof kVDVFBlendLayersFP_LinearBurn;
			break;

		case VDVFBlendLayersConfig::kMode_ColorBurn:
			bytecode = kVDVFBlendLayersFP_ColorBurn;
			bclen = sizeof kVDVFBlendLayersFP_ColorBurn;
			break;

		case VDVFBlendLayersConfig::kMode_Screen:
			bytecode = kVDVFBlendLayersFP_Screen;
			bclen = sizeof kVDVFBlendLayersFP_Screen;
			break;

		case VDVFBlendLayersConfig::kMode_ColorDodge:
			bytecode = kVDVFBlendLayersFP_ColorDodge;
			bclen = sizeof kVDVFBlendLayersFP_ColorDodge;
			break;

		case VDVFBlendLayersConfig::kMode_Overlay:
			bytecode = kVDVFBlendLayersFP_Overlay;
			bclen = sizeof kVDVFBlendLayersFP_Overlay;
			break;

		case VDVFBlendLayersConfig::kMode_SoftLight:
			bytecode = kVDVFBlendLayersFP_SoftLight;
			bclen = sizeof kVDVFBlendLayersFP_SoftLight;
			break;

		case VDVFBlendLayersConfig::kMode_HardLight:
			bytecode = kVDVFBlendLayersFP_HardLight;
			bclen = sizeof kVDVFBlendLayersFP_HardLight;
			break;

		case VDVFBlendLayersConfig::kMode_VividLight:
			bytecode = kVDVFBlendLayersFP_VividLight;
			bclen = sizeof kVDVFBlendLayersFP_VividLight;
			break;

		case VDVFBlendLayersConfig::kMode_LinearLight:
			bytecode = kVDVFBlendLayersFP_LinearLight;
			bclen = sizeof kVDVFBlendLayersFP_LinearLight;
			break;

		case VDVFBlendLayersConfig::kMode_PinLight:
			bytecode = kVDVFBlendLayersFP_PinLight;
			bclen = sizeof kVDVFBlendLayersFP_PinLight;
			break;

		case VDVFBlendLayersConfig::kMode_HardMix:
			bytecode = kVDVFBlendLayersFP_HardMix;
			bclen = sizeof kVDVFBlendLayersFP_HardMix;
			break;

		case VDVFBlendLayersConfig::kMode_Difference:
			bytecode = kVDVFBlendLayersFP_Difference;
			bclen = sizeof kVDVFBlendLayersFP_Difference;
			break;

		case VDVFBlendLayersConfig::kMode_Exclusion:
			bytecode = kVDVFBlendLayersFP_Exclusion;
			bclen = sizeof kVDVFBlendLayersFP_Exclusion;
			break;
	}

	mVDXAShader = vdxa->CreateFragmentProgram(kVDXAPF_D3D9ByteCodePS20, bytecode, bclen);
}

void VDVFBlendLayers::RunAccel(IVDXAContext *vdxa) {
	VDXFBitmap& src0 = *fa->mpSourceFrames[0];
	VDXFBitmap& src1 = *fa->mpSourceFrames[1];

	vdxa->SetSampler(0, src0.mVDXAHandle, kVDXAFilt_Point);
	vdxa->SetSampler(1, src1.mVDXAHandle, kVDXAFilt_Point);

	vdxa->SetTextureMatrix(0, src0.mVDXAHandle, 0, 0, NULL);
	vdxa->SetTextureMatrix(1, src1.mVDXAHandle, 0, 0, NULL);

	float amount[4] = { mConfig.mAmount, 0, 0, 0 };
	vdxa->SetFragmentProgramConstF(0, 1, amount);

	vdxa->DrawRect(fa->dst.mVDXAHandle, mVDXAShader, NULL);
}

void VDVFBlendLayers::StopAccel(IVDXAContext *vdxa) {
	if (mVDXAShader) {
		vdxa->DestroyObject(mVDXAShader);
		mVDXAShader = 0;
	}
}

bool VDVFBlendLayers::Configure(VDXHWND hwnd) {
	if (!hwnd)
		return true;

	VDVFBlendLayersConfig cfg(mConfig);
	VDVFBlendLayersDialog dlg(mConfig, fa->ifp2);
	if (dlg.ShowDialog((VDGUIHandle)hwnd))
		return true;

	mConfig = cfg;
	return false;
}

bool VDVFBlendLayers::Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher) {
	for(uint32 i=0; i<fa->mSourceStreamCount; ++i)
		prefetcher->PrefetchFrame(i, frame, 0);

	return true;
}

void VDVFBlendLayers::GetSettingString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, " (%ls %.0f%%)", kBlendModes[mConfig.mMode], mConfig.mAmount * 100.0f);
}

void VDVFBlendLayers::GetScriptString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, "Config(%d,%g)", mConfig.mMode, mConfig.mAmount);
}

VDXVF_BEGIN_SCRIPT_METHODS(VDVFBlendLayers)
	VDXVF_DEFINE_SCRIPT_METHOD(VDVFBlendLayers, OnScriptConfig, "id")
VDXVF_END_SCRIPT_METHODS()

void VDVFBlendLayers::OnScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc) {
	uint32 mode = (uint32)argv[0].asInt();

	if (mode >= VDVFBlendLayersConfig::kModeCount)
		mode = 0;

	mConfig.mMode = (VDVFBlendLayersConfig::Mode)mode;

	float amount = (float)argv[1].asDouble();

	if (amount >= 0.0f && amount < 1.0f)
		mConfig.mAmount = amount;
	else
		mConfig.mAmount = 1.0f;
}

extern const VDXFilterDefinition g_VDVFBlendLayers = VDXVideoFilterDefinition<VDVFBlendLayers>(NULL, "blend layers", "Blends layers from two video streams.");

// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{24,{flat}}' }'' : unreferenced local function has been removed
#pragma warning(disable: 4505)
