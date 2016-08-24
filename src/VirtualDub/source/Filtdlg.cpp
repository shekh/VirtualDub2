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

#include "VideoSource.h"
#include <vd2/system/error.h>
#include <vd2/system/linearalloc.h>
#include <vd2/system/list.h>
#include <vd2/system/registry.h>
#include <vd2/system/strutil.h>
#include <vd2/Dita/services.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/VDXFrame/VideoFilter.h>

#include "resource.h"
#include "FilterPreview.h"
#include "projectui.h"

#include "filtdlg.h"
#include "filters.h"
#include "FilterInstance.h"
#include "FilterFrameVideoSource.h"
#include "dub.h"

extern const char g_szError[];
extern vdrefptr<VDProjectUI> g_projectui;
extern DubOptions	g_dubOpts;

//////////////////////////////

FilterDefinitionInstance *VDUIShowDialogAddFilter(VDGUIHandle hParent);
bool VDShowFilterClippingDialog(VDGUIHandle hParent, FilterInstance *pFiltInst, VDFilterChainDesc *pFilterChainDesc, sint64 initialTimeUS);
extern void SaveProject(HWND, bool reset_path);

class VDVideoFiltersDialog;

class FiltersEditor{
public:
	VDVideoFiltersDialog* dlg_first;
	VDVideoFiltersDialog* dlg_second;
	vdrefptr<FilterInstance> config_first;
	vdrefptr<FilterInstance> config_second;
	VDFilterChainDesc filter_desc;
	FilterSystem mFiltSys;
	vdrefptr<VDFilterFrameVideoSource>	mpVideoFrameSource;
	vdrefptr<IVDVideoFilterPreviewDialog> preview;
	vdfastvector<IVDPixmapViewDialog*> extra_view;
	int mEditInstance;
	HWND* owner_ref;

	VDFraction	mOldFrameRate;
	sint64		mOldFrameCount;
	int			mInputWidth;
	int			mInputHeight;
	VDPixmapFormatEx	mInputFormat;
	VDFraction	mInputRate;
	VDFraction	mInputPixelAspect;
	sint64		mInputLength;
	VDTime		mInitialTimeUS;
	vdrefptr<IVDVideoSource>	mpVS;
	VDVideoFiltersDialogResult mResult;

	FiltersEditor()
	: mInputWidth(320)
	, mInputHeight(240)
	, mInputFormat(nsVDPixmap::kPixFormat_XRGB8888)
	, mInputRate(30, 1)
	, mInputPixelAspect(1, 1)
	, mInputLength(100)
	, mInitialTimeUS(-1)
	{
		dlg_first = 0;
		dlg_second = 0;
		mEditInstance = -1;
		owner_ref = 0;
		mResult.mbDialogAccepted = false;
		mResult.mbChangeDetected = false;
		mResult.mbRescaleRequested = false;
	}

	void Init(IVDVideoSource *pVS, VDPosition initialTime);
	void Init(int w, int h, int format, const VDFraction& rate, sint64 length, VDPosition initialTime);
	void SetResult();
	VDVideoFiltersDialogResult GetResult() const { return mResult; }

	void PrepareChain();
	void ReadyFilters();
	void EvalView(FilterInstance* fa, IVDPixmapViewDialog* view);
	void EvalAllViews();
	bool DisplayFrame(IVDVideoSource* pVS);
	bool GotoFrame(VDPosition pos, VDPosition time);
	void UndoSystem();

	void ActivateNextWindow();
	void DestroyAllViews();
	void DestroyView(IVDPixmapViewDialog* view);

	static void DestroyView(IVDPixmapViewDialog* view, void* data) {
		FiltersEditor* editor = (FiltersEditor*)data;
		editor->ActivateNextWindow();
		editor->DestroyView(view);
	}

	static void SaveScript(FilterInstance* fa) {
		if (fa) {
			VDStringA scriptStr;
			if (fa->GetScriptString(scriptStr)) {
				fa->mConfigString = scriptStr;
			} else {
				fa->mConfigString.clear();
			}
		}
	}
};

FiltersEditor* g_filtersEditor;

void FiltersEditor::Init(IVDVideoSource *pVS, VDPosition initialTime) {
	IVDStreamSource *pSS = pVS->asStream();
	const VDPixmap& px = pVS->getTargetFormat();

	mpVS			= pVS;
	mInputWidth		= px.w;
	mInputHeight	= px.h;
	mInputFormat	= px;
	mInputRate		= pSS->getRate();
	mInputPixelAspect = pVS->getPixelAspectRatio();
	mInputLength	= pSS->getLength();
	mInitialTimeUS	= initialTime;

	mOldFrameRate	= filters.GetOutputFrameRate();
	mOldFrameCount	= filters.GetOutputFrameCount();
}

void FiltersEditor::Init(int w, int h, int format, const VDFraction& rate, sint64 length, VDPosition initialTime) {
	mpVS			= NULL;
	mInputWidth		= w;
	mInputHeight	= h;
	mInputFormat	= format;
	mInputRate		= rate;
	mInputLength	= length;
	mInitialTimeUS	= initialTime;

	mOldFrameRate	= filters.GetOutputFrameRate();
	mOldFrameCount	= filters.GetOutputFrameCount();
}

void FiltersEditor::SetResult() {
	mResult.mOldFrameRate		= mOldFrameRate;
	mResult.mOldFrameCount		= mOldFrameCount;
	mResult.mNewFrameRate		= mFiltSys.GetOutputFrameRate();
	mResult.mNewFrameCount		= mFiltSys.GetOutputFrameCount();

	mResult.mbRescaleRequested = false;
	mResult.mbChangeDetected = false;

	if (mResult.mOldFrameRate != mResult.mNewFrameRate || mResult.mOldFrameCount != mResult.mNewFrameCount) {
		mResult.mbChangeDetected = true;
		mResult.mbRescaleRequested = true;
	}

	mResult.mbDialogAccepted = true;
}

void FiltersEditor::DestroyAllViews() {
	for(vdfastvector<IVDPixmapViewDialog*>::iterator it(extra_view.begin()), itEnd(extra_view.end());
		it != itEnd;
		++it)
	{
		(*it)->SetDestroyCallback(0,0);
		(*it)->Destroy();
		(*it)->Release();
	}
	extra_view.clear();
	for(VDFilterChainDesc::Entries::const_iterator it(filter_desc.mEntries.begin()), itEnd(filter_desc.mEntries.end());
		it != itEnd;
		++it)
	{
		(*it)->mpView = 0;
	}
}

void FiltersEditor::DestroyView(IVDPixmapViewDialog* view) {
	for(vdfastvector<IVDPixmapViewDialog*>::iterator it(extra_view.begin()), itEnd(extra_view.end());
		it != itEnd;
		++it)
	{
		if(*it==view) {
			view->Release();
			extra_view.erase(it);
			break;
		}
	}

	for(VDFilterChainDesc::Entries::const_iterator it(filter_desc.mEntries.begin()), itEnd(filter_desc.mEntries.end());
		it != itEnd;
		++it)
	{
		if((*it)->mpView==view) (*it)->mpView = 0;
	}
}

void FiltersEditor::EvalAllViews() {
	if(preview && preview->GetHwnd()) {
		preview->AsIVDXFilterPreview2()->RedoSystem();
		return;
	}

	if(extra_view.empty()) return;

	ReadyFilters();

	for(VDFilterChainDesc::Entries::const_iterator it(filter_desc.mEntries.begin()), itEnd(filter_desc.mEntries.end());
		it != itEnd;
		++it)
	{
		if((*it)->mpView) {
			EvalView((*it)->mpInstance, (*it)->mpView);
		}
	}

	UndoSystem();
}

bool FiltersEditor::DisplayFrame(IVDVideoSource* pVS) {
	vdrefptr<IVDVideoSource> prev_vs = mpVS;
	Init(pVS, 0);

	UndoSystem();
	PrepareChain();

	if(preview && preview->GetHwnd()) {
		EvalAllViews();
		return true;
	}

	return false;
}

bool FiltersEditor::GotoFrame(VDPosition pos, VDPosition time) {
	mInitialTimeUS = time;
	if(preview) {
		preview->AsIFilterModPreview()->FMSetPosition(pos);
		if(preview->GetHwnd()) return true;
	}

	return false;
}

void FiltersEditor::PrepareChain() {
	if (mInputFormat) {
		try {
			mFiltSys.prepareLinearChain(&filter_desc, mInputWidth, mInputHeight, mInputFormat, mInputRate, mInputLength, mInputPixelAspect);
		} catch(const MyError&) {
			// eat error
		}
	}
}

void FiltersEditor::ReadyFilters() {
	try {
		IVDStreamSource *pVSS = mpVS->asStream();
		const VDPixmap& px = mpVS->getTargetFormat();
		VDFraction srcRate = pVSS->getRate();

		if (g_dubOpts.video.mFrameRateAdjustLo > 0)
			srcRate.Assign(g_dubOpts.video.mFrameRateAdjustHi, g_dubOpts.video.mFrameRateAdjustLo);

		sint64 len = pVSS->getLength();
		const VDFraction& srcPAR = mpVS->getPixelAspectRatio();

		/*mFiltSys.prepareLinearChain(
				&filter_desc,
				px.w,
				px.h,
				px,
				srcRate,
				pVSS->getLength(),
				srcPAR);*/

		mpVideoFrameSource = new VDFilterFrameVideoSource;
		mpVideoFrameSource->Init(mpVS, mFiltSys.GetInputLayout());

		mFiltSys.initLinearChain(
				NULL,
				VDXFilterStateInfo::kStatePreview,
				&filter_desc,
				mpVideoFrameSource,
				px.w,
				px.h,
				px,
				px.palette,
				srcRate,
				len,
				srcPAR);

		mFiltSys.ReadyFilters();

	} catch(const MyError&) {
	}
}

void FiltersEditor::EvalView(FilterInstance* fa, IVDPixmapViewDialog* view) {
	if (!fa->IsEnabled()) return;

	vdrefptr<IVDFilterFrameClientRequest> req;
	VDPosition frame = 0;
	if (mInitialTimeUS >= 0) {
		const VDFraction outputRate(mFiltSys.GetOutputFrameRate());
		frame = VDRoundToInt64(outputRate.asDouble() * (double)mInitialTimeUS * (1.0 / 1000000.0));
	}

	if (fa->CreateRequest(frame, false, 0, ~req)) {
		while(!req->IsCompleted()) {
			if (mFiltSys.Run(NULL, false) == FilterSystem::kRunResult_Running)
				continue;

			switch(mpVideoFrameSource->RunRequests(NULL,0)) {
				case IVDFilterFrameSource::kRunResult_Running:
				case IVDFilterFrameSource::kRunResult_IdleWasActive:
				case IVDFilterFrameSource::kRunResult_BlockedWasActive:
					continue;
			}

			mFiltSys.Block();
		}

		if (req->IsSuccessful()) {
			vdrefptr<VDFilterFrameBuffer>	buf;
			buf = req->GetResultBuffer();
			if (buf) {
				const void *p = buf->LockRead();
				const VDPixmapLayout& layout = fa->GetOutputLayout();
				VDPixmap px = VDPixmapFromLayout(layout, (void *)p);
				px.info = buf->info;
				view->SetImage(px);
			}
		}
	}
}

void FiltersEditor::UndoSystem() {
	mFiltSys.DeinitFilters();
	mFiltSys.DeallocateBuffers();
	mpVideoFrameSource = NULL;
}

///////////////////////////////////////////////////////////////////////////

class VDXFilterPreviewThunk: public IVDXFilterPreview2{
public:
  FiltersEditor* editor;
  FilterInstance* pFiltInst;

  VDXFilterPreviewThunk(){ editor=0; pFiltInst=0; }
  virtual void SetButtonCallback(VDXFilterPreviewButtonCallback, void *){}
  virtual void SetSampleCallback(VDXFilterPreviewSampleCallback, void *){}

  virtual bool isPreviewEnabled(){ return true; }
  virtual void Toggle(VDXHWND){}
  virtual void Display(VDXHWND, bool){}
  virtual void RedoFrame(){
    editor->mFiltSys.InvalidateCachedFrames(pFiltInst);
    editor->preview->RedoFrame2(); 
  }
  virtual void RedoSystem(){
    editor->preview->AsIVDXFilterPreview2()->RedoSystem();
  }
  virtual void UndoSystem(){
    editor->preview->AsIVDXFilterPreview2()->UndoSystem();
  }
  virtual void InitButton(VDXHWND w){ 
    ShowWindow((HWND)w,SW_HIDE);
  }
  virtual void Close(){}
  virtual bool SampleCurrentFrame(){ return false; }
  virtual long SampleFrames(){ return 0; }

  virtual bool IsPreviewDisplayed(){ return true; }
};

///////////////////////////////////////////////////////////////////////////
//
//	filter options dialog
//
///////////////////////////////////////////////////////////////////////////

class VDDialogFilterOptions : public VDDialogFrameW32 {
public:
	VDDialogFilterOptions(FilterInstance *fi);

protected:
	void OnDataExchange(bool write);

	FilterInstance *mpFI;
};

VDDialogFilterOptions::VDDialogFilterOptions(FilterInstance *fi)
	: VDDialogFrameW32(IDD_FILTER_OPTIONS)
	, mpFI(fi)
{
}

void VDDialogFilterOptions::OnDataExchange(bool write) {
	if (write) {
		mpFI->SetForceSingleFBEnabled(IsButtonChecked(IDC_SINGLE_FB));
	} else {
		CheckButton(IDC_SINGLE_FB, mpFI->IsForceSingleFBEnabled());
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	add filter dialog
//
///////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////

class VDVideoFilterOutputNameDialog : public VDDialogFrameW32 {
public:
	VDVideoFilterOutputNameDialog();

	void SetName(const char *name) { mName = name; }
	const char *GetName() const { return mName.c_str(); }

protected:
	void OnDataExchange(bool write);

	VDStringA mName;
};

VDVideoFilterOutputNameDialog::VDVideoFilterOutputNameDialog()
	: VDDialogFrameW32(IDD_FILTER_OUTPUT_NAME)
{
}

void VDVideoFilterOutputNameDialog::OnDataExchange(bool write) {
	if (write) {
		VDStringW s;
		
		if (!GetControlText(IDC_NAME, s)) {
			FailValidation(IDC_NAME);
			return;
		}

		for(VDStringW::const_iterator it(s.begin()), itEnd(s.end());
			it != itEnd;
			++it)
		{
			const wchar_t c = *it;

			if ((uint32)(c - L'0') >= 10 &&
				(uint32)(c - L'A') >= 26 &&
				(uint32)(c - L'a') >= 26 &&
				c != L'_')
			{
				ShowError(L"Names can only contain alphanumeric characters and underlines (_).", L"Naming error");
				FailValidation(IDC_NAME);
				return;
			}
		}

		mName = VDTextWToA(s);
	} else {
		SetControlText(IDC_NAME, VDTextAToW(mName).c_str());
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	Cropping dialog
//
///////////////////////////////////////////////////////////////////////////

class VDFilterClippingDialog2 : public VDDialogFrameW32 {
public:
	FilterInstance *fa;
	IVDXFilterPreview2 *fp2;
	IFilterModPreview *fmpreview;
	int x1,y1,x2,y2;
	int mSourceWidth,mSourceHeight;

	VDFilterClippingDialog2();
	bool OnLoaded();

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void SetClipEdit();
	void init_crop();
	void init_size();
	static void ClipEditCallback(ClipEditInfo& info, void *pData);
};

VDFilterClippingDialog2::VDFilterClippingDialog2()
	: VDDialogFrameW32(IDD_FILTER_CLIPPING)
{
	mSourceWidth = 0;
	mSourceHeight = 0;
}

void VDFilterClippingDialog2::init_crop() {
	SetDlgItemInt(mhdlg, IDC_CLIP_X0, x1, FALSE);
	SetDlgItemInt(mhdlg, IDC_CLIP_X1, x2, FALSE);
	SetDlgItemInt(mhdlg, IDC_CLIP_Y0, y1, FALSE);
	SetDlgItemInt(mhdlg, IDC_CLIP_Y1, y2, FALSE);
}

void VDFilterClippingDialog2::init_size() {
	int w = mSourceWidth - x1 - x2;
	int h = mSourceHeight - y1 - y2;
	if (w<0) w = 0;
	if (h<0) h = 0;

	SetControlTextF(IDC_CROP_SIZE, L"Size: %dx%u", w, h);
}

void VDFilterClippingDialog2::ClipEditCallback(ClipEditInfo& info, void *pData) {
	VDFilterClippingDialog2* dlg = (VDFilterClippingDialog2*)pData;
	if (info.flags & info.init_size) {
		dlg->mSourceWidth = info.w;
		dlg->mSourceHeight = info.h;
	}
	if (info.flags & info.edit_update) {
		dlg->x1 = info.x1;
		dlg->y1 = info.y1;
		dlg->x2 = info.x2;
		dlg->y2 = info.y2;
	}
	dlg->init_crop();
	dlg->init_size();
}

void VDFilterClippingDialog2::SetClipEdit() {
	ClipEditInfo clip;
	clip.x1 = x1;
	clip.y1 = y1;
	clip.x2 = x2;
	clip.y2 = y2;
	clip.flags = clip.fill_border;
	if (fmpreview)
		fmpreview->SetClipEdit(clip);
}

bool VDFilterClippingDialog2::OnLoaded() {
	const vdrect32& r = fa->GetCropInsets();
	x1 = r.left;
	y1 = r.top;
	x2 = r.right;
	y2 = r.bottom;
	VDSetDialogDefaultIcons(mhdlg);
	init_crop();

	bool precise = fa->IsPreciseCroppingEnabled();
	CheckButton(IDC_CROP_PRECISE, precise);
	CheckButton(IDC_CROP_FAST, !precise);

	if (fmpreview) {
		PreviewExInfo mode;
		mode.flags = mode.thick_border | mode.custom_draw | mode.display_source | mode.no_exit;
		fmpreview->SetClipEditCallback(ClipEditCallback, this);
		fmpreview->DisplayEx((VDXHWND)mhdlg,mode);
		SetClipEdit();
	}
	return true;
}

INT_PTR VDFilterClippingDialog2::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message)
	{
		case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
			fa->SetCrop(x1, y1, x2, y2, IsButtonChecked(IDC_CROP_PRECISE));
			EndDialog(mhdlg,0);
			return TRUE;

		case IDCANCEL:
			EndDialog(mhdlg,1);
			return TRUE;

		case IDC_CLIP_X0:
			x1 = GetDlgItemInt(mhdlg,IDC_CLIP_X0,0,false);
			init_size();
			SetClipEdit();
			fp2->RedoFrame();
			return TRUE;

		case IDC_CLIP_X1:
			x2 = GetDlgItemInt(mhdlg,IDC_CLIP_X1,0,false);
			init_size();
			SetClipEdit();
			fp2->RedoFrame();
			return TRUE;

		case IDC_CLIP_Y0:
			y1 = GetDlgItemInt(mhdlg,IDC_CLIP_Y0,0,false);
			init_size();
			SetClipEdit();
			fp2->RedoFrame();
			return TRUE;

		case IDC_CLIP_Y1:
			y2 = GetDlgItemInt(mhdlg,IDC_CLIP_Y1,0,false);
			init_size();
			SetClipEdit();
			fp2->RedoFrame();
			return TRUE;
		}
		break;
	}

	return VDDialogFrameW32::DlgProc(message, wParam, lParam);
}

///////////////////////////////////////////////////////////////////////////
//
//	Opacity cropping dialog
//
///////////////////////////////////////////////////////////////////////////

class VDFilterBlendingDialog : public VDDialogFrameW32 {
public:
	FilterInstance *fa;
	IVDXFilterPreview2 *fp2;
	IFilterModPreview *fmpreview;
	int x1,y1,x2,y2;
	int mSourceWidth,mSourceHeight;
	vdrect32 r0;
	vdrefptr<VDParameterCurve> curve;

	VDFilterBlendingDialog();
	bool OnLoaded();

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void SetClipEdit();
	void init_crop();
	void init_size();
	void apply_crop();
	void apply_curve(bool v);
	static void ClipEditCallback(ClipEditInfo& info, void *pData);
};

VDFilterBlendingDialog::VDFilterBlendingDialog()
	: VDDialogFrameW32(IDD_FILTER_BLENDING)
{
	mSourceWidth = 0;
	mSourceHeight = 0;
}

void VDFilterBlendingDialog::init_crop() {
	SetDlgItemInt(mhdlg, IDC_CLIP_X0, x1, FALSE);
	SetDlgItemInt(mhdlg, IDC_CLIP_X1, x2, FALSE);
	SetDlgItemInt(mhdlg, IDC_CLIP_Y0, y1, FALSE);
	SetDlgItemInt(mhdlg, IDC_CLIP_Y1, y2, FALSE);
}

void VDFilterBlendingDialog::init_size() {
	int w = mSourceWidth - x1 - x2;
	int h = mSourceHeight - y1 - y2;
	if (w<0) w = 0;
	if (h<0) h = 0;

	SetControlTextF(IDC_CROP_SIZE, L"Size: %dx%u", w, h);
}

void VDFilterBlendingDialog::apply_crop() {
	bool c0 = fa->IsOpacityEnabled();
	fa->SetOpacityCrop(x1, y1, x2, y2);
	bool c1 = fa->IsOpacityEnabled();
	if (fp2) {
		if (c0==c1)
			fp2->RedoFrame();
		else
			fp2->RedoSystem();
	}
}

void VDFilterBlendingDialog::apply_curve(bool v) {
	bool c0 = fa->IsOpacityEnabled();
	if (!v) {
		fa->SetAlphaParameterCurve(0);
	} else if (curve) {
		fa->SetAlphaParameterCurve(curve);
	} else {
		VDParameterCurve *c1 = new_nothrow VDParameterCurve();
		if (c1) {
			c1->SetYRange(0.0f, 1.0f);
			fa->SetAlphaParameterCurve(c1);
		}
	}
	bool c1 = fa->IsOpacityEnabled();
	if (fp2) {
		if (c0==c1)
			fp2->RedoFrame();
		else
			fp2->RedoSystem();
	}
}

void VDFilterBlendingDialog::ClipEditCallback(ClipEditInfo& info, void *pData) {
	VDFilterBlendingDialog* dlg = (VDFilterBlendingDialog*)pData;
	if (info.flags & info.init_size) {
		dlg->mSourceWidth = info.w;
		dlg->mSourceHeight = info.h;
	}
	if (info.flags & info.edit_update) {
		dlg->x1 = info.x1;
		dlg->y1 = info.y1;
		dlg->x2 = info.x2;
		dlg->y2 = info.y2;
	}
	dlg->init_crop();
	dlg->init_size();
	if (info.flags & info.edit_finish) dlg->apply_crop();
}

void VDFilterBlendingDialog::SetClipEdit() {
	ClipEditInfo clip;
	clip.x1 = x1;
	clip.y1 = y1;
	clip.x2 = x2;
	clip.y2 = y2;
	if (fmpreview)
		fmpreview->SetClipEdit(clip);
}

bool VDFilterBlendingDialog::OnLoaded() {
	const vdrect32& r = fa->GetOpacityCropInsets();
	r0 = r;
	x1 = r.left;
	y1 = r.top;
	x2 = r.right;
	y2 = r.bottom;
	VDSetDialogDefaultIcons(mhdlg);
	init_crop();

	curve = fa->GetAlphaParameterCurve();
	CheckButton(IDC_BLEND_CURVE,curve!=0);

	if (fmpreview) {
		PreviewExInfo mode;
		mode.flags = mode.thick_border | mode.custom_draw | mode.no_exit;
		fmpreview->SetClipEditCallback(ClipEditCallback, this);
		fmpreview->DisplayEx((VDXHWND)mhdlg,mode);
		SetClipEdit();
	}
	return true;
}

INT_PTR VDFilterBlendingDialog::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message)
	{
		case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
			fa->SetOpacityCrop(x1, y1, x2, y2);
			EndDialog(mhdlg,0);
			return TRUE;

		case IDCANCEL:
			fa->SetOpacityCrop(r0.left, r0.top, r0.right, r0.bottom);
			fa->SetAlphaParameterCurve(curve);
			EndDialog(mhdlg,1);
			return TRUE;

		case IDC_BLEND_CURVE:
			apply_curve(IsButtonChecked(IDC_BLEND_CURVE));
			return TRUE;

		case IDC_CLIP_X0:
			x1 = GetDlgItemInt(mhdlg,IDC_CLIP_X0,0,false);
			init_size();
			SetClipEdit();
			apply_crop();
			return TRUE;

		case IDC_CLIP_X1:
			x2 = GetDlgItemInt(mhdlg,IDC_CLIP_X1,0,false);
			init_size();
			SetClipEdit();
			apply_crop();
			return TRUE;

		case IDC_CLIP_Y0:
			y1 = GetDlgItemInt(mhdlg,IDC_CLIP_Y0,0,false);
			init_size();
			SetClipEdit();
			apply_crop();
			return TRUE;

		case IDC_CLIP_Y1:
			y2 = GetDlgItemInt(mhdlg,IDC_CLIP_Y1,0,false);
			init_size();
			SetClipEdit();
			apply_crop();
			return TRUE;
		}
		break;
	}

	return VDDialogFrameW32::DlgProc(message, wParam, lParam);
}

///////////////////////////////////////////////////////////////////////////
//
//	Filter list dialog
//
///////////////////////////////////////////////////////////////////////////

class VDVideoFiltersDialog : public VDDialogFrameW32 {
public:
	VDVideoFiltersDialog();
	
public:
	FiltersEditor* editor;
	bool is_first;

protected:
	bool OnLoaded();
	void OnDestroy();
	bool OnOK();
	bool OnCancel();
	void OnSize();
	bool OnErase(VDZHDC hdc);
	bool OnCommand(uint32 id, uint32 extcode);
	void OnContextMenu(uint32 id, int x, int y);

	void OnItemCheckedChanged(VDUIProxyListView *sender, int item);
	void OnItemCheckedChanging(VDUIProxyListView *sender, VDUIProxyListView::CheckedChangingEvent *event);
	void OnItemDoubleClicked(VDUIProxyListView *sender, int item);
	void OnItemSelectionChanged(VDUIProxyListView *sender, int item);

	void LoadGlobalChainCopy();
	void LoadSharedChain(VDFilterChainDesc& desc);
	typedef enum {makeChain_local=0, makeChain_copy=1, makeChain_finish=2} MakeChainType;
	void MakeFilterChainDesc(VDFilterChainDesc& desc, MakeChainType type=makeChain_local);
	void EnableConfigureBox(int index = -1);
	void RedoFilters();
	void RelayoutFilterList();
	void CloneDialog();
	void SaveFilters();
	void AddFilter(int pos);
	bool ConfigureFilter(FilterInstance *fa);
	bool ConfigureCrop(FilterInstance *fa);
	bool ConfigureBlend(FilterInstance *fa);
	void CreateView(VDFilterChainEntry *ent);

	bool		mbShowFormats;
	bool		mbShowAspectRatios;
	bool		mbShowFrameRates;

	int			mFilterEnablesUpdateLock;

	HMENU		mhContextMenus;

	VDUIProxyListView mListView;

	VDDelegate mDelItemCheckedChanging;
	VDDelegate mDelItemCheckedChanged;
	VDDelegate mDelItemDoubleClicked;
	VDDelegate mDelItemSelectionChanged;

	VDDialogResizerW32 mResizer;

	class FilterListItemBase : public vdrefcounted<IVDUIListViewVirtualItem>, public IVDUnknown {
	};

	class FilterListItem : public FilterListItemBase {
	public:
		enum { kTypeID = 'fli ' };

		FilterListItem(VDVideoFiltersDialog *parent, VDFilterChainEntry *ent) : mpParent(parent), mpEntry(ent) {}

		void *AsInterface(uint32 id);

		void GetText(int subItem, VDStringW& s) const;

		VDVideoFiltersDialog *mpParent;
		vdrefptr<VDFilterChainEntry> mpEntry;
	};

	class FilterInputListItem : public FilterListItemBase {
	public:
		enum { kTypeID = 'fili' };

		void *AsInterface(uint32 id);

		void GetText(int subItem, VDStringW& s) const;

		VDStringA mName;
	};
};

VDVideoFiltersDialog::VDVideoFiltersDialog()
	: VDDialogFrameW32(IDD_FILTERS)
	, mbShowFormats(false)
	, mbShowAspectRatios(false)
	, mbShowFrameRates(false)
	, mFilterEnablesUpdateLock(0)
	, mhContextMenus(NULL)
{
	is_first = true;

	mListView.OnItemCheckedChanged() += mDelItemCheckedChanged.Bind(this, &VDVideoFiltersDialog::OnItemCheckedChanged);
	mListView.OnItemCheckedChanging() += mDelItemCheckedChanging.Bind(this, &VDVideoFiltersDialog::OnItemCheckedChanging);
	mListView.OnItemDoubleClicked() += mDelItemDoubleClicked.Bind(this, &VDVideoFiltersDialog::OnItemDoubleClicked);
	mListView.OnItemSelectionChanged() += mDelItemSelectionChanged.Bind(this, &VDVideoFiltersDialog::OnItemSelectionChanged);
}

bool VDVideoFiltersDialog::OnLoaded() {
	VDSetDialogDefaultIcons(mhdlg);

	mResizer.Init(mhdlg);
	mResizer.Add(IDC_FILTER_LIST, VDDialogResizerW32::kMC | VDDialogResizerW32::kAvoidFlicker);
	mResizer.Add(IDOK, VDDialogResizerW32::kTR);
	mResizer.Add(IDCANCEL, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_ADD, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_DELETE, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_MOVEUP, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_MOVEDOWN, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_CLIPPING, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_CONFIGURE, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_BLENDING, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_OPTIONS, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_FILTERS_SAVE, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_SHOWIMAGEFORMATS, VDDialogResizerW32::kBL);
	mResizer.Add(IDC_SHOWFRAMERATES, VDDialogResizerW32::kBL);
	mResizer.Add(IDC_SHOWASPECTRATIOS, VDDialogResizerW32::kBL);

	if (!is_first) {
		ShowWindow(GetDlgItem(mhdlg,IDCANCEL),SW_HIDE);
	}

	AddProxy(&mListView, IDC_FILTER_LIST);

	{
		VDRegistryAppKey key("Dialogs\\Filters");
		mbShowFormats = key.getBool("Show formats", mbShowFormats);
		mbShowAspectRatios = key.getBool("Show aspect ratios", mbShowAspectRatios);
		mbShowFrameRates = key.getBool("Show frame rates", mbShowFrameRates);
	}

	CheckButton(IDC_SHOWIMAGEFORMATS, mbShowFormats);
	CheckButton(IDC_SHOWASPECTRATIOS, mbShowAspectRatios);
	CheckButton(IDC_SHOWFRAMERATES, mbShowFrameRates);

	mFilterEnablesUpdateLock = 0;

	mListView.SetFullRowSelectEnabled(true);
	mListView.SetItemCheckboxesEnabled(true);

	mListView.InsertColumn(0, L"", 25);
	mListView.InsertColumn(1, L"Input", 50);
	mListView.InsertColumn(2, L"Output", 50);
	mListView.InsertColumn(3, L"Filter", 200);

	if (!is_first) {
		LoadSharedChain(editor->filter_desc);
		RelayoutFilterList();
	} else {
		// cloning inited filter is not allowed
		filters.DeinitFilters();
		filters.DeallocateBuffers();
		LoadGlobalChainCopy();
		RedoFilters();
	}

	mhContextMenus = LoadMenu(NULL, MAKEINTRESOURCE(IDR_FILTER_LIST_CONTEXT));

	SetFocusToControl(IDC_FILTER_LIST);
	VDUIRestoreWindowPlacementW32(mhdlg, "VideoFilters", SW_SHOW);

	if (is_first) {
		if(editor->owner_ref) *editor->owner_ref = mhdlg;
		if(editor->mEditInstance!=-1){
			mListView.SetSelectedIndex(editor->mEditInstance);
			OnCommand(IDC_CONFIGURE, 0);
			editor->mEditInstance = -1;
		}
	}

	return true;
}

void VDVideoFiltersDialog::LoadGlobalChainCopy() {
	const VDFilterChainDesc& desc = g_filterChain;

	for(VDFilterChainDesc::Entries::const_iterator it(desc.mEntries.begin()), itEnd(desc.mEntries.end());
		it != itEnd;
		++it)
	{
		const VDFilterChainEntry *src = *it;
		vdrefptr<VDFilterChainEntry> ent(new VDFilterChainEntry);

		if (src->mpInstance)
			ent->mpInstance = src->mpInstance->Clone();

		ent->mOutputName = src->mOutputName;

		int index = mListView.InsertVirtualItem(-1, new FilterListItem(this, ent));
		if (index >= 0)
			mListView.SetItemChecked(index, !ent->mpInstance || ent->mpInstance->IsEnabled());

		for(vdvector<VDStringA>::const_iterator itInput(src->mSources.begin()), itInputEnd(src->mSources.end());
			itInput != itInputEnd;
			++itInput)
		{
			const VDStringA& srcName = *itInput;

			vdrefptr<FilterInputListItem> fii(new FilterInputListItem);

			fii->mName = srcName;
			int index2 = mListView.InsertVirtualItem(-1, fii);
			if (index2 >= 0)
				mListView.SetItemCheckedVisible(index2, false);
		}
	}
}

void VDVideoFiltersDialog::LoadSharedChain(VDFilterChainDesc& desc) {
	mListView.Clear();
	for(VDFilterChainDesc::Entries::const_iterator it(desc.mEntries.begin()), itEnd(desc.mEntries.end());
		it != itEnd;
		++it)
	{
		VDFilterChainEntry *ent = *it;

		int index = mListView.InsertVirtualItem(-1, new FilterListItem(this, ent));
		if (index >= 0)
			mListView.SetItemChecked(index, !ent->mpInstance || ent->mpInstance->IsEnabled());

		for(vdvector<VDStringA>::const_iterator itInput(ent->mSources.begin()), itInputEnd(ent->mSources.end());
			itInput != itInputEnd;
			++itInput)
		{
			const VDStringA& srcName = *itInput;

			vdrefptr<FilterInputListItem> fii(new FilterInputListItem);

			fii->mName = srcName;
			int index2 = mListView.InsertVirtualItem(-1, fii);
			if (index2 >= 0)
				mListView.SetItemCheckedVisible(index2, false);
		}
	}
}

void VDVideoFiltersDialog::OnDestroy() {
	VDUISaveWindowPlacementW32(mhdlg, "VideoFilters");

	if (mhContextMenus) {
		DestroyMenu(mhContextMenus);
		mhContextMenus = NULL;
	}
}

bool VDVideoFiltersDialog::OnOK() {
	// We must force filters to stop before we muck with the global list... in case
	// the pane refresh restarted them.
	filters.DeinitFilters();
	filters.DeallocateBuffers();

	if (!is_first) {
		editor->dlg_first->LoadSharedChain(editor->filter_desc);
	} else {
		editor->DestroyAllViews();
		MakeFilterChainDesc(g_filterChain,makeChain_finish);
		editor->SetResult();
		editor->mResult.mbDialogAccepted = true;
	}

	End(true);
	return true;
}

bool VDVideoFiltersDialog::OnCancel() {
	if(!is_first) return OnOK();

	editor->DestroyAllViews();

	// We must force filters to stop before we muck with the global list... in case
	// the pane refresh restarted them.
	filters.DeinitFilters();
	filters.DeallocateBuffers();

	editor->mResult.mbDialogAccepted = true;
	End(false);
	return true;
}

void VDVideoFiltersDialog::OnSize() {
	mResizer.Relayout();
}

bool VDVideoFiltersDialog::OnErase(VDZHDC hdc) {
	mResizer.Erase(&hdc);
	return true;
}

bool VDVideoFiltersDialog::OnCommand(uint32 id, uint32 extcode) {
	switch(id) {
		case IDC_ADD:
		case ID_FILTERLIST_ADDFILTERAFTER:
			AddFilter(1);
			return true;

		case ID_FILTERLIST_ADDFILTERBEFORE:
			AddFilter(0);
			return true;

		case IDC_DELETE:
		case ID_FILTERLIST_DELETE:
			{
				int index = mListView.GetSelectedIndex();
				FilterListItemBase *flib = static_cast<FilterListItemBase *>(mListView.GetVirtualItem(index));
				FilterListItem *fli = vdpoly_cast<FilterListItem *>(flib);

				if (flib) {
					mListView.DeleteItem(index);

					if (fli) {
						while(vdpoly_cast<FilterInputListItem *>(static_cast<FilterListItemBase *>(mListView.GetVirtualItem(index))))
							mListView.DeleteItem(index);
					}

					mListView.SetSelectedIndex(index);

					RedoFilters();
				}
			}
			return true;

		case IDC_CONFIGURE:
			{
				int index = mListView.GetSelectedIndex();
				FilterListItem *fli = vdpoly_cast<FilterListItem *>(static_cast<FilterListItemBase *>(mListView.GetVirtualItem(index))); 

				if (fli) {
					FilterInstance *fa = fli->mpEntry->mpInstance;
					if (fa && fa->IsConfigurable() && fa!=editor->config_first) {
						//RedoFilters();
						ConfigureFilter(fa);
						RedoFilters();
					}
				}
			}
			return true;

		case IDC_CLIPPING:
			{
				int index = mListView.GetSelectedIndex();
				FilterListItem *fli = vdpoly_cast<FilterListItem *>(static_cast<FilterListItemBase *>(mListView.GetVirtualItem(index)));

				if (fli) {
					FilterInstance *fa = fli->mpEntry->mpInstance;
					if (fa && fa!=editor->config_first) {
						ConfigureCrop(fa);
						RedoFilters();
					}
				}
			}
			return true;

		case IDC_BLENDING:
			{
				int index = mListView.GetSelectedIndex();
				FilterListItem *fli = vdpoly_cast<FilterListItem *>(static_cast<FilterListItemBase *>(mListView.GetVirtualItem(index)));

				if (fli) {
					FilterInstance *fa = fli->mpEntry->mpInstance;
					if (fa && fa!=editor->config_first) {
						ConfigureBlend(fa);
						RedoFilters();
					}
				}
			}
			return true;

		case IDC_OPTIONS:
			{
				int index = mListView.GetSelectedIndex();
				FilterListItem *fli = vdpoly_cast<FilterListItem *>(static_cast<FilterListItemBase *>(mListView.GetVirtualItem(index)));

				if (fli) {
					FilterInstance *fa = fli->mpEntry->mpInstance;

					if (fa) {
						VDDialogFilterOptions optdlg(fa);
						optdlg.ShowDialog((VDGUIHandle)mhdlg);

						RedoFilters();
					}
				}
			}
			return true;

		case IDC_MOVEUP:
			{
				int index = mListView.GetSelectedIndex();

				if (index > 0) {
					FilterListItemBase *flib = static_cast<FilterListItemBase *>(mListView.GetVirtualItem(index));
					FilterListItem *fli = vdpoly_cast<FilterListItem *>(flib);
					FilterInputListItem *fii = vdpoly_cast<FilterInputListItem *>(flib);

					if (fli) {
						int prevIndex = index;

						while(prevIndex > 0) {
							--prevIndex;
							
							if (!vdpoly_cast<FilterInputListItem *>(static_cast<FilterListItemBase *>(mListView.GetVirtualItem(prevIndex))))
								break;
						}

						int nextIndex = index;
						do {
							++nextIndex;
						} while(vdpoly_cast<FilterInputListItem *>(static_cast<FilterListItemBase *>(mListView.GetVirtualItem(nextIndex))));

						int distance2 = nextIndex - index;

						for(int i = 0; i < distance2; ++i) {
							FilterListItemBase *flib2 = static_cast<FilterListItemBase *>(mListView.GetVirtualItem(index + i));
							FilterListItem *fli2 = vdpoly_cast<FilterListItem *>(flib2);
							FilterInputListItem *fii2 = vdpoly_cast<FilterInputListItem *>(flib2);

							int newIdx = mListView.InsertVirtualItem(prevIndex + i, flib2);
							if (newIdx >= 0) {
								if (fli2) {
									FilterInstance *fi = fli2->mpEntry->mpInstance;
									mListView.SetItemChecked(newIdx, fi && fi->IsEnabled());
								} else if (fii2) {
									mListView.SetItemCheckedVisible(newIdx, false);
								}
							}

							mListView.DeleteItem(index + i + 1);
						}

						RedoFilters();

						mListView.SetSelectedIndex(prevIndex);
						mListView.EnsureItemVisible(prevIndex);
					} else if (fii) {
						FilterInputListItem *fiip = vdpoly_cast<FilterInputListItem *>(static_cast<FilterListItemBase *>(mListView.GetVirtualItem(index - 1)));

						if (fiip) {
							int newIdx = mListView.InsertVirtualItem(index - 1, fii);
							if (newIdx >= 0)
								mListView.SetItemCheckedVisible(newIdx, false);

							mListView.DeleteItem(index + 1);

							RedoFilters();

							mListView.SetSelectedIndex(index - 1);
							mListView.EnsureItemVisible(index - 1);
						}
					}
				}
			}
			return true;

		case IDC_MOVEDOWN:
			{
				int index = mListView.GetSelectedIndex();
				int count = mListView.GetItemCount();

				if (index >= 0 && index < count - 1) {
					FilterListItemBase *flib = static_cast<FilterListItemBase *>(mListView.GetVirtualItem(index));
					FilterListItem *fli = vdpoly_cast<FilterListItem *>(flib);
					FilterInputListItem *fii = vdpoly_cast<FilterInputListItem *>(flib);

					if (fli) {
						int nextIndex = index;

						do {
							++nextIndex;
						} while(vdpoly_cast<FilterInputListItem *>(static_cast<FilterListItemBase *>(mListView.GetVirtualItem(nextIndex))));

						if (nextIndex < count) {
							int nextIndex2 = nextIndex;
							do {
								++nextIndex2;
							} while(vdpoly_cast<FilterInputListItem *>(static_cast<FilterListItemBase *>(mListView.GetVirtualItem(nextIndex2))));

							int distance2 = nextIndex2 - nextIndex;

							for(int i=0; i<distance2; ++i) {
								FilterListItemBase *flib2 = static_cast<FilterListItemBase *>(mListView.GetVirtualItem(nextIndex + i));
								FilterListItem *fli2 = vdpoly_cast<FilterListItem *>(flib2);
								FilterInputListItem *fii2 = vdpoly_cast<FilterInputListItem *>(flib2);

								int newIdx = mListView.InsertVirtualItem(index + i, flib2);
								if (newIdx >= 0) {
									if (fli2) {
										FilterInstance *fi = fli2->mpEntry->mpInstance;
										mListView.SetItemChecked(newIdx, fi && fi->IsEnabled());
									} else if (fii2) {
										mListView.SetItemCheckedVisible(newIdx, false);
									}
								}

								mListView.DeleteItem(nextIndex + i + 1);
							}

							RedoFilters();

							mListView.SetSelectedIndex(index + distance2);
							mListView.EnsureItemVisible(index + distance2);
						}
					} else if (fii) {
						FilterInputListItem *fiip = vdpoly_cast<FilterInputListItem *>(static_cast<FilterListItemBase *>(mListView.GetVirtualItem(index + 1)));

						if (fiip) {
							int newIdx = mListView.InsertVirtualItem(index + 2, fii);
							if (newIdx >= 0)
								mListView.SetItemCheckedVisible(newIdx, false);

							mListView.DeleteItem(index);

							RedoFilters();

							mListView.SetSelectedIndex(index + 1);
							mListView.EnsureItemVisible(index + 1);
						}
					}
				}
			}
			return true;

		case IDC_SHOWIMAGEFORMATS:
			{
				bool selected = IsButtonChecked(IDC_SHOWIMAGEFORMATS);

				if (mbShowFormats != selected) {
					mbShowFormats = selected;

					VDRegistryAppKey key("Dialogs\\Filters");
					key.setBool("Show formats", mbShowFormats);

					RelayoutFilterList();
				}
			}
			return true;

		case IDC_SHOWASPECTRATIOS:
			{
				bool selected = IsButtonChecked(IDC_SHOWASPECTRATIOS);

				if (mbShowAspectRatios != selected) {
					mbShowAspectRatios = selected;

					VDRegistryAppKey key("Dialogs\\Filters");
					key.setBool("Show aspect ratios", mbShowAspectRatios);

					RelayoutFilterList();
				}
			}
			return true;

		case IDC_SHOWFRAMERATES:
			{
				bool selected = IsButtonChecked(IDC_SHOWFRAMERATES);

				if (mbShowFrameRates != selected) {
					mbShowFrameRates = selected;

					VDRegistryAppKey key("Dialogs\\Filters");
					key.setBool("Show frame rates", mbShowFrameRates);

					RelayoutFilterList();
				}
			}
			return true;

		case ID_FILTERLIST_NAMEOUTPUT:
			{
				int idx = mListView.GetSelectedIndex();
				if (idx >= 0) {
					FilterListItem *fli = vdpoly_cast<FilterListItem *>(static_cast<FilterListItemBase *>(mListView.GetVirtualItem(idx)));

					if (fli) {
						VDVideoFilterOutputNameDialog dlg;

						dlg.SetName(fli->mpEntry->mOutputName.c_str());
						if (dlg.ShowDialog((VDGUIHandle)mhdlg)) {
							fli->mpEntry->mOutputName = dlg.GetName();

							mListView.RefreshItem(idx);
						}
					}
				}
			}
			return true;

		case ID_FILTERLIST_ADDINPUT:
			{
				int selIdx = mListView.GetSelectedIndex();

				if (selIdx >= 0) {
					FilterListItem *fli = vdpoly_cast<FilterListItem *>(static_cast<FilterListItemBase *>(mListView.GetVirtualItem(selIdx)));

					if (fli) {
						int idx = selIdx;

						do {
							++idx;
						} while(vdpoly_cast<FilterInputListItem *>(static_cast<FilterListItemBase *>(mListView.GetVirtualItem(idx))));

						vdrefptr<FilterInputListItem> fii(new FilterInputListItem);

						int i = mListView.InsertVirtualItem(idx, fii);

						if (i >= 0)
							mListView.SetItemCheckedVisible(i, false);
					}
				}
			}
			return true;

		case ID_FILTERLIST_EXTRAVIEW:
			{
				int index = mListView.GetSelectedIndex();
				FilterListItem *fli = vdpoly_cast<FilterListItem *>(static_cast<FilterListItemBase *>(mListView.GetVirtualItem(index))); 
				if (fli->mpEntry->mpView)
					fli->mpEntry->mpView->Destroy();
				else
					CreateView(fli->mpEntry);
			}
			return true;

		case IDC_FILTERS_SAVE:
			SaveFilters();
			return true;

		case ID_VIDEO_FILTERS:
			CloneDialog();
			return true;

		case ID_VIDEO_FILTERS_HIDE:
			ShowWindow(mhdlg,SW_HIDE);
			return true;
	}

	return false;
}

void VDVideoFiltersDialog::AddFilter(int pos) {
	FilterDefinitionInstance *fdi = VDUIShowDialogAddFilter((VDGUIHandle)mhdlg);
	if (!fdi) return;

	try {
		vdrefptr<VDFilterChainEntry> ent(new VDFilterChainEntry);
		vdrefptr<FilterInstance> fa(new FilterInstance(fdi));

		// Note that this call would end up disabling the filter instance if
		// we didn't have an update lock around it.
		fa->SetEnabled(true);

		ent->mpInstance = fa;

		++mFilterEnablesUpdateLock;
		int insert_index = mListView.GetSelectedIndex();
		if (pos==1) {
			if (insert_index==-1) {
				insert_index = mListView.GetItemCount();
			} else {
				do {
					++insert_index;
				} while(vdpoly_cast<FilterInputListItem *>(static_cast<FilterListItemBase *>(mListView.GetVirtualItem(insert_index))));
			}
		}

		const int index = mListView.InsertVirtualItem(insert_index, new FilterListItem(this, ent));
		if (index >= 0) {
			mListView.SetSelectedIndex(index);
			mListView.SetItemChecked(index, true);
		}
		--mFilterEnablesUpdateLock;

		RedoFilters();

		extern const VDXFilterDefinition g_VDVFNull;
		if (fa->IsConfigurable() && fa->GetDefinition()->name!=g_VDVFNull.name) {
			bool fRemove = ConfigureFilter(fa);
			if (fRemove) {
				mListView.DeleteItem(index);
				return;
			}
		}

		RedoFilters();

		mListView.SetSelectedIndex(index);

		EnableConfigureBox(index);
	} catch(const MyError& e) {
		e.post(mhdlg, g_szError);
	}
}

bool VDVideoFiltersDialog::ConfigureCrop(FilterInstance *fa) {
	bool fRemove = false;

	if (!is_first || !fa->IsEnabled()) {
		editor->config_second = fa;

		VDXFilterPreviewThunk thunk;
		thunk.editor = editor;
		thunk.pFiltInst = fa;
		VDFilterClippingDialog2 dlg;
		dlg.fa = fa;
		dlg.fp2 = &thunk;
		dlg.fmpreview = 0;
		fRemove = dlg.ShowDialog((VDGUIHandle)mhdlg)!=0;

		editor->config_second = 0;

	} else {
		editor->config_first = fa;
		
		if (VDCreateVideoFilterPreviewDialog(&editor->mFiltSys, editor->mpVS ? &editor->filter_desc : NULL, fa, ~editor->preview)) {
			editor->preview->SetFilterList(mhdlg);
			if (editor->mInitialTimeUS >= 0)
				editor->preview->SetInitialTime(editor->mInitialTimeUS);

			VDFilterClippingDialog2 dlg;
			dlg.fa = fa;
			dlg.fp2 = editor->preview->AsIVDXFilterPreview2();
			dlg.fmpreview = editor->preview->AsIFilterModPreview();
			fRemove = dlg.ShowDialog((VDGUIHandle)mhdlg)!=0;
		}

		editor->preview = 0;
		editor->config_first = 0;
	}

	return fRemove;
}

bool VDVideoFiltersDialog::ConfigureBlend(FilterInstance *fa) {
	bool fRemove = false;

	if (!is_first || !fa->IsEnabled()) {
		editor->config_second = fa;

		VDXFilterPreviewThunk thunk;
		thunk.editor = editor;
		thunk.pFiltInst = fa;
		VDFilterBlendingDialog dlg;
		dlg.fa = fa;
		dlg.fp2 = &thunk;
		dlg.fmpreview = 0;
		fRemove = dlg.ShowDialog((VDGUIHandle)mhdlg)!=0;

		editor->config_second = 0;

	} else {
		editor->config_first = fa;
		
		if (VDCreateVideoFilterPreviewDialog(&editor->mFiltSys, editor->mpVS ? &editor->filter_desc : NULL, fa, ~editor->preview)) {
			editor->preview->SetFilterList(mhdlg);
			if (editor->mInitialTimeUS >= 0)
				editor->preview->SetInitialTime(editor->mInitialTimeUS);

			VDFilterBlendingDialog dlg;
			dlg.fa = fa;
			dlg.fp2 = editor->preview->AsIVDXFilterPreview2();
			dlg.fmpreview = editor->preview->AsIFilterModPreview();
			fRemove = dlg.ShowDialog((VDGUIHandle)mhdlg)!=0;
		}

		editor->preview = 0;
		editor->config_first = 0;
	}

	return fRemove;
}

bool VDVideoFiltersDialog::ConfigureFilter(FilterInstance *fa) {
	extern const VDXFilterDefinition g_VDVFCrop;
	if (fa->GetDefinition()->name==g_VDVFCrop.name)
		return ConfigureCrop(fa);

	bool fRemove = false;

	if (!is_first) {
		editor->config_second = fa;

		VDXFilterPreviewThunk thunk;
		thunk.editor = editor;
		thunk.pFiltInst = fa;
		PostMessage(mhdlg,WM_COMMAND,ID_VIDEO_FILTERS_HIDE,0);
		fRemove = !fa->Configure((VDXHWND)mhdlg, &thunk, 0);
		ShowWindow(mhdlg,SW_SHOW);

		editor->config_second = 0;

	} else {

		editor->config_first = fa;

		if (VDCreateVideoFilterPreviewDialog(&editor->mFiltSys, editor->mpVS ? &editor->filter_desc : NULL, fa, ~editor->preview)) {
			editor->preview->SetFilterList(mhdlg);
			if (editor->mInitialTimeUS >= 0)
				editor->preview->SetInitialTime(editor->mInitialTimeUS);

			fRemove = !fa->Configure((VDXHWND)mhdlg, editor->preview->AsIVDXFilterPreview2(), editor->preview->AsIFilterModPreview());
		}

		editor->preview = 0;
		editor->config_first = 0;
	}

	return fRemove;
}

void VDVideoFiltersDialog::CreateView(VDFilterChainEntry *ent) {
	IVDPixmapViewDialog *view;
	VDCreatePixmapViewDialog(&view);
	view->SetDestroyCallback(editor->DestroyView,editor);
	editor->extra_view.push_back(view);

	if (editor->preview && editor->preview->GetHwnd()) {
		editor->UndoSystem();
		editor->ReadyFilters();
		editor->EvalView(ent->mpInstance,view);
		editor->UndoSystem();
		editor->preview->AsIVDXFilterPreview2()->RedoSystem();
 	} else {
		editor->ReadyFilters();
		editor->EvalView(ent->mpInstance,view);
		editor->UndoSystem();
		editor->PrepareChain();
	}

	HWND parent = g_projectui->GetHwnd();
	VDStringW title(L"Filter view: ");
	title += VDTextAToW(ent->mpInstance->GetName());
	view->Display((VDXHWND)parent,title.c_str());

	ent->mpView = view;
}

void VDVideoFiltersDialog::CloneDialog() {
	VDVideoFiltersDialog dlg2;

	editor->dlg_second = &dlg2;
	dlg2.editor = editor;
	dlg2.is_first = false;

	HWND parent = editor->preview->GetHwnd();
	dlg2.ShowDialog((VDGUIHandle)parent);

	editor->dlg_second = 0;
}

void VDVideoFiltersDialog::SaveFilters() {
	editor->SaveScript(editor->config_first);
	editor->SaveScript(editor->config_second);

	MakeFilterChainDesc(g_filterChain,makeChain_copy);
	SaveProject(0,false);
	RedoFilters();
}

void VDVideoFiltersDialog::OnContextMenu(uint32 id, int x, int y) {
	if (id == IDC_FILTER_LIST) {
		int selIdx = mListView.GetSelectedIndex();

		if (selIdx >= 0) {
			FilterListItemBase *flib = static_cast<FilterListItemBase *>(mListView.GetVirtualItem(selIdx));
			FilterListItem *fli = vdpoly_cast<FilterListItem *>(flib);
			FilterInputListItem *fii = vdpoly_cast<FilterInputListItem *>(flib);

			if (x == -1 && y == -1) {
				vdrect32 sr;
				if (mListView.GetItemScreenRect(selIdx, sr)) {
					x = sr.left + ((uint32)(sr.right - sr.left) >> 1);
					y = sr.top + ((uint32)(sr.bottom - sr.top) >> 1);
				}
			}

			if (fli) {
				if (mhContextMenus) {
					HMENU hmenu = GetSubMenu(mhContextMenus, 0);
					if (hmenu) {
  						EnableMenuItem(hmenu, ID_FILTERLIST_EXTRAVIEW, editor->mpVS ? false:true);
						TrackPopupMenu(hmenu, 0, x, y, 0, (HWND)mhdlg, NULL);
					}
				}
			} else if (fii) {
				vdfastvector<const wchar_t *> items;
				vdvector<VDStringA> bindNames;
				VDLinearAllocator linearAlloc;

				items.push_back(L"(No connection)");
				items.push_back(L"(Source)");
				items.push_back(L"(Previous output)");

				bindNames.push_back_as("");
				bindNames.push_back_as("$input");
				bindNames.push_back_as("$prev");

				int n = mListView.GetItemCount();
				for(int i=0; i<n; ++i) {
					FilterListItem *fli = vdpoly_cast<FilterListItem *>(static_cast<FilterListItemBase *>(mListView.GetVirtualItem(i)));

					if (fli) {
						const VDStringA& outputName = fli->mpEntry->mOutputName;

						if (!outputName.empty()) {
							const VDStringW& tmp = VDTextAToW(outputName);
							
							wchar_t *s = (wchar_t *)linearAlloc.Allocate(sizeof(wchar_t) * (tmp.size() + 1));
							wcscpy(s, tmp.c_str());

							items.push_back(s);
							bindNames.push_back(outputName);
						}
					}
				}

				items.push_back(NULL);

				int bindIdx = ActivatePopupMenu(x, y, items.data());

				if (bindIdx >= 0) {
					fii->mName = bindNames[bindIdx];
					mListView.RefreshItem(bindIdx);
					RedoFilters();
				}
			}
		}
	}
}

void VDVideoFiltersDialog::OnItemCheckedChanging(VDUIProxyListView *sender, VDUIProxyListView::CheckedChangingEvent *event) {
	FilterListItem *fli = vdpoly_cast<FilterListItem *>(static_cast<FilterListItemBase *>(sender->GetVirtualItem(event->mIndex)));
	if (!fli) {
		if (event->mbNewVisible)
			event->mbAllowChange = false;
	}
}

void VDVideoFiltersDialog::OnItemCheckedChanged(VDUIProxyListView *sender, int index) {
	FilterListItem *fli = vdpoly_cast<FilterListItem *>(static_cast<FilterListItemBase *>(sender->GetVirtualItem(index)));
	if (!fli)
		return;

	FilterInstance *fi = fli->mpEntry->mpInstance;
	if (!fi)
		return;

	if (!mFilterEnablesUpdateLock) {
		// We fetch the item state because uNewState seems hosed when ListView_SetItemState() is called
		// with a partial mask.
		bool enabled = mListView.IsItemChecked(index);

		if (enabled != fi->IsEnabled()) {
			fi->SetEnabled(enabled);
			RedoFilters();
			EnableConfigureBox(index);
		}
	}
}

void VDVideoFiltersDialog::OnItemDoubleClicked(VDUIProxyListView *sender, int item) {
	OnCommand(IDC_CONFIGURE, 0);
}

void VDVideoFiltersDialog::OnItemSelectionChanged(VDUIProxyListView *sender, int item) {
	EnableConfigureBox(item);
}

void VDVideoFiltersDialog::MakeFilterChainDesc(VDFilterChainDesc& desc, MakeChainType type) {
	// have to do this since the filter list is intrusive
	editor->UndoSystem();
	
	desc.Clear();

	int n = mListView.GetItemCount();
	VDFilterChainEntry *pPrevEntry = NULL;
	for(int i=0; i<n; ++i) {
		FilterListItemBase *flib = static_cast<FilterListItemBase *>(mListView.GetVirtualItem(i));
		FilterListItem *fli = vdpoly_cast<FilterListItem *>(flib);
		FilterInputListItem *fii = vdpoly_cast<FilterInputListItem *>(flib);

		if (fli) {
			VDFilterChainEntry *pEnt = fli->mpEntry;
			if (type==makeChain_finish) pEnt->mpView = 0;
			if (type==makeChain_copy) {
				VDFilterChainEntry *pEnt2 = new VDFilterChainEntry;
				pEnt2->mOutputName = pEnt->mOutputName;
				if (pEnt->mpInstance)
					pEnt2->mpInstance = pEnt->mpInstance->Clone();
				pEnt = pEnt2;
			}
			pPrevEntry = pEnt;
			pPrevEntry->mSources.clear();
			desc.AddEntry(pEnt);
		} else if (fii) {
			if (pPrevEntry)
				pPrevEntry->mSources.push_back(fii->mName);
		}
	}
}

void VDVideoFiltersDialog::EnableConfigureBox(int index) {
	FilterListItem *fli = vdpoly_cast<FilterListItem *>(static_cast<FilterListItemBase *>(mListView.GetSelectedItem()));
	FilterInstance *fa = NULL;

	if (fli)
		fa = fli->mpEntry->mpInstance;

	if (fa) {
		bool skip = fa==editor->config_first;
		EnableControl(IDC_CONFIGURE, fa->IsConfigurable() && !skip);
		EnableControl(IDC_CLIPPING, fa->IsEnabled() && !skip);
		EnableControl(IDC_DELETE, !skip);
		EnableControl(IDC_BLENDING, true);
		EnableControl(IDC_OPTIONS, true);
	} else {
		EnableControl(IDC_CONFIGURE, false);
		EnableControl(IDC_CLIPPING, false);
		EnableControl(IDC_DELETE, true);
		EnableControl(IDC_BLENDING, false);
		EnableControl(IDC_OPTIONS, false);
	}
}

void VDVideoFiltersDialog::RedoFilters() {
	MakeFilterChainDesc(editor->filter_desc);
	editor->EvalAllViews();
	editor->PrepareChain();

	mListView.RefreshAllItems();
	RelayoutFilterList();
}

void VDVideoFiltersDialog::RelayoutFilterList() {
	mListView.AutoSizeColumns();
}

void *VDVideoFiltersDialog::FilterListItem::AsInterface(uint32 id) {
	if (id == FilterListItem::kTypeID)
		return this;

	return NULL;
}

void VDVideoFiltersDialog::FilterListItem::GetText(int subItem, VDStringW& s) const {
	if (!mpEntry->mpInstance) {
		switch(subItem) {
			case 0:
			case 1:
			case 2:
				break;

			case 3:
				s = L"<unknown entry>";
				break;
		}

		return;
	}

	FilterInstance *fi = mpEntry->mpInstance;
	const VDFilterPrepareInfo& prepareInfo = fi->mPrepareInfo;
	const VDFilterPrepareStreamInfo *streamInfo = prepareInfo.mStreams.empty() ? (const VDFilterPrepareStreamInfo *)NULL : &prepareInfo.mStreams[0];

	const VDFilterPrepareInfo2& prepareInfo2 = fi->mPrepareInfo2;
	const VDFilterPrepareStreamInfo2 *streamInfo2 = prepareInfo2.mStreams.empty() ? (const VDFilterPrepareStreamInfo2 *)NULL : &prepareInfo2.mStreams[0];

	switch(subItem) {
	case 0:
		if (fi->IsEnabled()) {
			s.sprintf(L"%s%s%s%s"
						, fi->IsOpacityEnabled() ? L"[B] " : L""
						, streamInfo2 && streamInfo2->mbConvertOnEntry ? L"[C] " : streamInfo && streamInfo->mbAlignOnEntry ? L"[A]" : L""
						, fi->IsAccelerated() ? L"[3D]" : L""
						, fi->IsForceSingleFBEnabled() ? L"[F]" : L""
				);
		}
		break;
	case 1:
	case 2:
		{
			const VDFilterStreamDesc desc = subItem == 2 ? fi->GetOutputDesc() : fi->GetSourceDesc();
			const VDPixmapLayout& layout = desc.mLayout;

			if (!fi->IsEnabled()) {
				s = L"-";
			} else if (subItem == 2 && fi->GetExcessiveFrameSizeState()) {
				s = L"(too big)";
			} else {
				s.sprintf(L"%ux%u", layout.w, layout.h);

				if (mpParent->mbShowFormats) {
					const char *const kFormatNames[]={
						"?",
						"P1",
						"P2",
						"P4",
						"P8",
						"RGB15",
						"RGB16",
						"RGB24",
						"RGB32",
						"Y8",
						"UYVY",
						"YUYV",
						"YUV",
						"YUV444",
						"YUV422",
						"YUV420",
						"YUV411",
						"YUV410",
						"YUV422C",
						"YUV420C",
						"YUV422-16F",
						"V210",
						"UYVY-709",
						"NV12",
						"I8",
						"YUYV-709",
						"YUV444-709",
						"YUV422-709",
						"YUV420-709",
						"YUV411-709",
						"YUV410-709",
						"UYVY-FR",
						"YUYV-FR",
						"YUV444-FR",
						"YUV422-FR",
						"YUV420-FR",
						"YUV411-FR",
						"YUV410-FR",
						"UYVY-709-FR",
						"YUYV-709-FR",
						"YUV444-709-FR",
						"YUV422-709-FR",
						"YUV420-709-FR",
						"YUV411-709-FR",
						"YUV410-709-FR",
						"YUV420i",
						"YUV420i-FR",
						"YUV420i-709",
						"YUV420i-709-FR",
						"YUV420it",
						"YUV420it-FR",
						"YUV420it-709",
						"YUV420it-709-FR",
						"YUV420ib",
						"YUV420ib-FR",
						"YUV420ib-709",
						"YUV420ib-709-FR",
						"RGB64",
						"YUV444P16",
						"YUV422P16",
						"YUV420P16",
					};

					VDASSERTCT(sizeof(kFormatNames)/sizeof(kFormatNames[0]) == nsVDPixmap::kPixFormat_Max_Standard);

					if (layout.format == nsVDXPixmap::kPixFormat_VDXA_RGB)
						s += L" (RGB)";
					else if (layout.format == nsVDXPixmap::kPixFormat_VDXA_YUV)
						s += L" (YUV)";
					else
						s.append_sprintf(L" (%hs)", kFormatNames[layout.format]);
				}

				if (mpParent->mbShowAspectRatios && subItem == 2) {
					if (desc.mAspectRatio.getLo()) {
						VDFraction reduced = desc.mAspectRatio.reduce();

						if (reduced.getLo() < 1000)
							s.append_sprintf(L" (%u:%u)", reduced.getHi(), reduced.getLo());
						else
							s.append_sprintf(L" (%.4g)", reduced.asDouble());
					} else
						s += L" (?)";
				}

				if (mpParent->mbShowFrameRates && subItem == 2) {
					s.append_sprintf(L" (%.3g fps)", desc.mFrameRate.asDouble());
				}
			}
		}
		break;
	case 3:
		{
			VDStringA blurb;
			VDStringA settings;

			if (!mpEntry->mOutputName.empty()) {
				blurb += mpEntry->mOutputName;
				blurb += " = ";
			}

			blurb += fi->GetName();

			if (fi->GetSettingsString(settings))
				blurb += settings;

			s = VDTextAToW(blurb);
		}
		break;
	}
}

void *VDVideoFiltersDialog::FilterInputListItem::AsInterface(uint32 id) {
	if (id == FilterInputListItem::kTypeID)
		return this;

	return NULL;
}

void VDVideoFiltersDialog::FilterInputListItem::GetText(int subItem, VDStringW& s) const {
	if (subItem == 3) {
		if (mName.empty())
			s = L"(No connection)";
		else if (mName == "$prev")
			s = L"(Previous output)";
		else if (mName == "$input")
			s = L"(Source)";
		else
			s = VDTextAToW(mName);
	}
}

void FiltersEditor::ActivateNextWindow() {
	if (dlg_second) {
		HWND wnd = dlg_second->GetWindowHandle();
		HWND wnd2 = GetWindow(wnd,GW_ENABLEDPOPUP);
		if (wnd2) wnd = wnd2;

		SetActiveWindow(wnd);
		return;
	}

	if (dlg_first) {
		HWND wnd = dlg_first->GetWindowHandle();
		HWND wnd2 = GetWindow(wnd,GW_ENABLEDPOPUP);
		if (wnd2) wnd = wnd2;
		if (preview) {
			wnd2 = preview->GetHwnd(); 
			if (wnd2) wnd = wnd2;
		}

		SetActiveWindow(wnd);
		return;
	}
}

bool FiltersEditorDisplayFrame(IVDVideoSource *pVS) {
	if (!g_filtersEditor) return false;
	return g_filtersEditor->DisplayFrame(pVS);
}

bool FiltersEditorGotoFrame(VDPosition pos, VDPosition time) {
	if (!g_filtersEditor) return false;
	return g_filtersEditor->GotoFrame(pos, time);
}

////////////////////////////////////////////////////////////////////////////

VDVideoFiltersDialogResult VDShowDialogVideoFilters(VDGUIHandle h, IVDVideoSource *pVS, VDPosition initialTime, int edit_instance, HWND* owner_ref) {
	VDVideoFiltersDialog dlg;

	FiltersEditor editor;
	editor.dlg_first = &dlg;
	dlg.editor = &editor;
	editor.mEditInstance = edit_instance;
	editor.owner_ref = owner_ref;
	g_filtersEditor = &editor;

	if (pVS)
		editor.Init(pVS, initialTime);

	dlg.ShowDialog(h);

	if(owner_ref) *owner_ref = 0;
	g_filtersEditor = 0;

	return editor.GetResult();
}

VDVideoFiltersDialogResult VDShowDialogVideoFilters(VDGUIHandle hParent, int w, int h, int format, const VDFraction& rate, sint64 length, VDPosition initialTime) {
	VDVideoFiltersDialog dlg;

	FiltersEditor editor;
	editor.dlg_first = &dlg;
	dlg.editor = &editor;

	editor.Init(w, h, format, rate, length, initialTime);
	dlg.ShowDialog(hParent);

	return editor.GetResult();
}
