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

#include "resource.h"
#include "FilterPreview.h"

#include "filtdlg.h"
#include "filters.h"
#include "FilterInstance.h"
#include "FilterFrameVideoSource.h"

extern const char g_szError[];

//////////////////////////////

FilterDefinitionInstance *VDUIShowDialogAddFilter(VDGUIHandle hParent);
bool VDShowFilterClippingDialog(VDGUIHandle hParent, FilterInstance *pFiltInst, VDFilterChainDesc *pFilterChainDesc, sint64 initialTimeUS);

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
//	Filter list dialog
//
///////////////////////////////////////////////////////////////////////////

class VDVideoFiltersDialog : public VDDialogFrameW32 {
public:
	VDVideoFiltersDialog();
	
	void Init(IVDVideoSource *pVS, VDPosition initialTime);
	void Init(int w, int h, int format, const VDFraction& rate, sint64 length, VDPosition initialTime);

	VDVideoFiltersDialogResult GetResult() const { return mResult; }

public:
	int mEditInstance;

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

	void MakeFilterChainDesc(VDFilterChainDesc& desc);
	void EnableConfigureBox(int index = -1);
	void RedoFilters();
	void RelayoutFilterList();

	VDFraction	mOldFrameRate;
	sint64		mOldFrameCount;
	int			mInputWidth;
	int			mInputHeight;
	int			mInputFormat;
	VDFraction	mInputRate;
	VDFraction	mInputPixelAspect;
	sint64		mInputLength;
	VDTime		mInitialTimeUS;
	IVDVideoSource	*mpVS;

	bool		mbShowFormats;
	bool		mbShowAspectRatios;
	bool		mbShowFrameRates;

	int			mFilterEnablesUpdateLock;

	HMENU		mhContextMenus;

	VDUIProxyListView mListView;
	VDVideoFiltersDialogResult mResult;

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
	, mInputWidth(320)
	, mInputHeight(240)
	, mInputFormat(nsVDPixmap::kPixFormat_XRGB8888)
	, mInputRate(30, 1)
	, mInputPixelAspect(1, 1)
	, mInputLength(100)
	, mInitialTimeUS(-1)
	, mpVS(NULL)
	, mEditInstance(-1)
	, mbShowFormats(false)
	, mbShowAspectRatios(false)
	, mbShowFrameRates(false)
	, mFilterEnablesUpdateLock(0)
	, mhContextMenus(NULL)
{
	mResult.mbDialogAccepted = false;
	mResult.mbChangeDetected = false;
	mResult.mbRescaleRequested = false;

	mListView.OnItemCheckedChanged() += mDelItemCheckedChanged.Bind(this, &VDVideoFiltersDialog::OnItemCheckedChanged);
	mListView.OnItemCheckedChanging() += mDelItemCheckedChanging.Bind(this, &VDVideoFiltersDialog::OnItemCheckedChanging);
	mListView.OnItemDoubleClicked() += mDelItemDoubleClicked.Bind(this, &VDVideoFiltersDialog::OnItemDoubleClicked);
	mListView.OnItemSelectionChanged() += mDelItemSelectionChanged.Bind(this, &VDVideoFiltersDialog::OnItemSelectionChanged);
}

void VDVideoFiltersDialog::Init(IVDVideoSource *pVS, VDPosition initialTime) {
	IVDStreamSource *pSS = pVS->asStream();
	const VDPixmap& px = pVS->getTargetFormat();

	mpVS			= pVS;
	mInputWidth		= px.w;
	mInputHeight	= px.h;
	mInputFormat	= px.format;
	mInputRate		= pSS->getRate();
	mInputPixelAspect = pVS->getPixelAspectRatio();
	mInputLength	= pSS->getLength();
	mInitialTimeUS	= initialTime;
}

void VDVideoFiltersDialog::Init(int w, int h, int format, const VDFraction& rate, sint64 length, VDPosition initialTime) {
	mpVS			= NULL;
	mInputWidth		= w;
	mInputHeight	= h;
	mInputFormat	= format;
	mInputRate		= rate;
	mInputLength	= length;
	mInitialTimeUS	= initialTime;
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
	mResizer.Add(IDC_SHOWIMAGEFORMATS, VDDialogResizerW32::kBL);
	mResizer.Add(IDC_SHOWFRAMERATES, VDDialogResizerW32::kBL);
	mResizer.Add(IDC_SHOWASPECTRATIOS, VDDialogResizerW32::kBL);

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

	RedoFilters();

	mhContextMenus = LoadMenu(NULL, MAKEINTRESOURCE(IDR_FILTER_LIST_CONTEXT));

	mOldFrameRate	= filters.GetOutputFrameRate();
	mOldFrameCount	= filters.GetOutputFrameCount();

	SetFocusToControl(IDC_FILTER_LIST);
	VDUIRestoreWindowPlacementW32(mhdlg, "VideoFilters", SW_SHOW);

	if(mEditInstance!=-1){
		mListView.SetSelectedIndex(mEditInstance);
		OnCommand(IDC_CONFIGURE, 0);
		mEditInstance = -1;
	}

	return true;
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

	MakeFilterChainDesc(g_filterChain);

	mResult.mOldFrameRate		= mOldFrameRate;
	mResult.mOldFrameCount		= mOldFrameCount;
	mResult.mNewFrameRate		= filters.GetOutputFrameRate();
	mResult.mNewFrameCount		= filters.GetOutputFrameCount();

	mResult.mbRescaleRequested = false;
	mResult.mbChangeDetected = false;

	if (mResult.mOldFrameRate != mResult.mNewFrameRate || mResult.mOldFrameCount != mResult.mNewFrameCount) {
		mResult.mbChangeDetected = true;
		mResult.mbRescaleRequested = true;
	}

	mResult.mbDialogAccepted = true;
	End(true);
	return true;
}

bool VDVideoFiltersDialog::OnCancel() {
	// We must force filters to stop before we muck with the global list... in case
	// the pane refresh restarted them.
	filters.DeinitFilters();
	filters.DeallocateBuffers();

	mResult.mbDialogAccepted = true;
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
			if (FilterDefinitionInstance *fdi = VDUIShowDialogAddFilter((VDGUIHandle)mhdlg)) {
				try {
					vdrefptr<VDFilterChainEntry> ent(new VDFilterChainEntry);
					vdrefptr<FilterInstance> fa(new FilterInstance(fdi));

					// Note that this call would end up disabling the filter instance if
					// we didn't have an update lock around it.
					fa->SetEnabled(true);

					ent->mpInstance = fa;

					++mFilterEnablesUpdateLock;
					const int index = mListView.InsertVirtualItem(mListView.GetItemCount(), new FilterListItem(this, ent));
					if (index >= 0)
						mListView.SetItemChecked(index, true);
					--mFilterEnablesUpdateLock;

					RedoFilters();

					if (fa->IsConfigurable()) {
						VDFilterChainDesc desc;
						bool fRemove;

						if (mpVS)
							MakeFilterChainDesc(desc);

						vdrefptr<IVDVideoFilterPreviewDialog> fp;
						if (VDCreateVideoFilterPreviewDialog(mpVS ? &desc : NULL, fa, ~fp)) {
							if (mInitialTimeUS >= 0)
								fp->SetInitialTime(mInitialTimeUS);

							fRemove = !fa->Configure((VDXHWND)mhdlg, fp->AsIVDXFilterPreview2(), fp->AsIFilterModPreview());
						}

						fp = NULL;

						if (fRemove) {
							mListView.DeleteItem(index);
							break;
						}
					}

					RedoFilters();

					mListView.SetSelectedIndex(index);

					EnableConfigureBox(index);
				} catch(const MyError& e) {
					e.post(mhdlg, g_szError);
				}
			}
			return true;

		case IDC_DELETE:
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

					if (fa && fa->IsConfigurable()) {
						VDFilterChainDesc desc;

						RedoFilters();

						if (mpVS)
							MakeFilterChainDesc(desc);

						vdrefptr<IVDVideoFilterPreviewDialog> fp;
						if (VDCreateVideoFilterPreviewDialog(mpVS ? &desc : NULL, fa, ~fp)) {
							if (mInitialTimeUS >= 0)
								fp->SetInitialTime(mInitialTimeUS);

							fa->Configure((VDXHWND)mhdlg, fp->AsIVDXFilterPreview2(), fp->AsIFilterModPreview());
						}

						fp = NULL;

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

					if (fa && fa->IsEnabled()) {
						filters.DeinitFilters();
						filters.DeallocateBuffers();

						VDFilterChainDesc desc;
						MakeFilterChainDesc(desc);

						VDShowFilterClippingDialog((VDGUIHandle)mhdlg, fa, &desc, mInitialTimeUS);

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

					if (fa) {
						if (fa->GetAlphaParameterCurve()) {
							fa->SetAlphaParameterCurve(NULL);
						} else {
							VDParameterCurve *curve = new_nothrow VDParameterCurve();
							if (curve) {
								curve->SetYRange(0.0f, 1.0f);
								fa->SetAlphaParameterCurve(curve);
							}
						}

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
	}

	return false;
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
					if (hmenu)
						TrackPopupMenu(hmenu, 0, x, y, 0, (HWND)mhdlg, NULL);
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

void VDVideoFiltersDialog::MakeFilterChainDesc(VDFilterChainDesc& desc) {
	// have to do this since the filter list is intrusive
	filters.DeinitFilters();
	filters.DeallocateBuffers();

	desc.Clear();

	int n = mListView.GetItemCount();
	VDFilterChainEntry *pPrevEntry = NULL;
	for(int i=0; i<n; ++i) {
		FilterListItemBase *flib = static_cast<FilterListItemBase *>(mListView.GetVirtualItem(i));
		FilterListItem *fli = vdpoly_cast<FilterListItem *>(flib);
		FilterInputListItem *fii = vdpoly_cast<FilterInputListItem *>(flib);

		if (fli) {
			VDFilterChainEntry *pEnt = fli->mpEntry;
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
		EnableControl(IDC_CONFIGURE, fa->IsConfigurable());
		EnableControl(IDC_CLIPPING, fa->IsEnabled());
		EnableControl(IDC_BLENDING, true);
		EnableControl(IDC_OPTIONS, true);
	} else {
		EnableControl(IDC_CONFIGURE, false);
		EnableControl(IDC_CLIPPING, false);
		EnableControl(IDC_BLENDING, false);
		EnableControl(IDC_OPTIONS, false);
	}
}

void VDVideoFiltersDialog::RedoFilters() {
	VDFilterChainDesc desc;

	MakeFilterChainDesc(desc);

	if (mInputFormat) {
		try {
			filters.prepareLinearChain(&desc, mInputWidth, mInputHeight, mInputFormat, mInputRate, mInputLength, mInputPixelAspect);
		} catch(const MyError&) {
			// eat error
		}
	}

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
						, fi->GetAlphaParameterCurve() ? L"[B] " : L""
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

////////////////////////////////////////////////////////////////////////////

VDVideoFiltersDialogResult VDShowDialogVideoFilters(VDGUIHandle h, IVDVideoSource *pVS, VDPosition initialTime, int edit_instance) {
	VDVideoFiltersDialog dlg;

	if (pVS)
		dlg.Init(pVS, initialTime);

	dlg.mEditInstance = edit_instance;

	dlg.ShowDialog(h);

	return dlg.GetResult();
}

VDVideoFiltersDialogResult VDShowDialogVideoFilters(VDGUIHandle hParent, int w, int h, int format, const VDFraction& rate, sint64 length, VDPosition initialTime) {
	VDVideoFiltersDialog dlg;

	dlg.Init(w, h, format, rate, length, initialTime);
	dlg.ShowDialog(hParent);

	return dlg.GetResult();
}
