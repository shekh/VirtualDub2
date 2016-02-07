#include "stdafx.h"
#include "filters.h"
#include "plugins.h"
#include "resource.h"
#include <list>
#include <vd2/Dita/services.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/VDLib/UIProxies.h>
#include <vd2/system/registry.h>

extern const char g_szError[];

namespace {
	enum {
		kFileDialog_LoadPlugin		= 'plug',
	};
}

static VDString format_hide_key(const FilterBlurb& fb) {
 	VDString s;
	s.sprintf("%s @ %s", fb.name.c_str(), fb.author.c_str());
	return s;
}

class VDDialogFilterListW32 : public VDDialogFrameW32 {
public:
	VDDialogFilterListW32();

	FilterDefinitionInstance *Activate(VDGUIHandle hParent);

protected:
	bool OnLoaded();
	void OnDataExchange(bool write);
	bool OnCommand(uint32 id, uint32 extcode);
	void RebuildList();
	void OnSelectionChanged(VDUIProxyListBoxControl *sender, int selIdx);
	void OnDoubleClick(VDUIProxyListBoxControl *sender, int selIdx);
	void OnDestroy();
	void OnSize();
	bool OnErase(VDZHDC hdc);

	VDUIProxyListBoxControl		mListBox;
	FilterDefinitionInstance	*mpFilterDefInst;
	std::list<FilterBlurb>		mFilterList;
	vdfastvector<const FilterBlurb *>	mSortedFilters;
	bool mbShowAll;

	VDDelegate					mDelegateSelChanged;
	VDDelegate					mDelegateDblClk;

	VDDialogResizerW32 mResizer;

	struct FilterBlurbSort {
		bool operator()(const FilterBlurb *x, const FilterBlurb *y) {
			return x->name.comparei(y->name) < 0;
		}
	};
};

VDDialogFilterListW32::VDDialogFilterListW32()
	: VDDialogFrameW32(IDD_FILTER_LIST)
{
	mListBox.OnSelectionChanged() += mDelegateSelChanged.Bind(this, &VDDialogFilterListW32::OnSelectionChanged);
	mListBox.OnItemDoubleClicked() += mDelegateDblClk.Bind(this, &VDDialogFilterListW32::OnDoubleClick);
}

FilterDefinitionInstance *VDDialogFilterListW32::Activate(VDGUIHandle hParent) {
	return ShowDialog(hParent) ? mpFilterDefInst : NULL;
}

bool VDDialogFilterListW32::OnLoaded() {
	VDSetDialogDefaultIcons(mhdlg);

	SetCurrentSizeAsMinSize();
	mMaxWidth = mMinWidth;

	mResizer.Init(mhdlg);
	mResizer.Add(IDC_FILTER_LIST, VDDialogResizerW32::kMC | VDDialogResizerW32::kAvoidFlicker);
	mResizer.Add(IDC_FILTER_INFO, VDDialogResizerW32::kBL);

	AddProxy(&mListBox, IDC_FILTER_LIST);

	int tabs[]={ 175 };
	mListBox.SetTabStops(tabs, 1);

	mbShowAll = false;
	RebuildList();

	SetFocusToControl(IDC_FILTER_LIST);
	VDUIRestoreWindowPlacementW32(mhdlg, "FilterListAdd", SW_SHOW);

	return true;
}

void VDDialogFilterListW32::OnDestroy() {
	VDUISaveWindowPlacementW32(mhdlg, "FilterListAdd");
}

void VDDialogFilterListW32::OnSize() {
	mResizer.Relayout();
}

bool VDDialogFilterListW32::OnErase(VDZHDC hdc) {
	mResizer.Erase(&hdc);
	return true;
}

void VDDialogFilterListW32::OnDataExchange(bool write) {
	if (write) {
		int selIdx = mListBox.GetSelection();
		if (selIdx < 0) {
			FailValidation(IDC_FILTER_LIST);
			return;
		}

		uintptr listIdx = mListBox.GetItemData(selIdx);
		if (listIdx >= mSortedFilters.size()) {
			FailValidation(IDC_FILTER_LIST);
			return;
		}

		const FilterBlurb& fb = *mSortedFilters[listIdx];
		mpFilterDefInst = fb.key;
	}
}

bool VDDialogFilterListW32::OnCommand(uint32 id, uint32 extcode) {
	if (id == IDC_LOAD) {
		const VDStringW filename(VDGetLoadFileName(kFileDialog_LoadPlugin, (VDGUIHandle)mhdlg, L"Load external filter", L"VirtualDub filter (*.vdf)\0*.vdf\0Windows Dynamic-Link Library (*.dll)\0*.dll\0All files (*.*)\0*.*\0", NULL, NULL, NULL));

		if (!filename.empty()) {
			try {
				VDAddPluginModule(filename.c_str());
			} catch(const MyError& e) {
				e.post(mhdlg, g_szError);
			}
		}

		RebuildList();
		return true;
	}

	if (id == IDC_ONOFF) {
		int selIdx = mListBox.GetSelection();
		if (selIdx!=-1) {
			uintptr listIdx = mListBox.GetItemData(selIdx);
			VDRegistryAppKey key("Hide Video Filters");
			VDString s = format_hide_key(*mSortedFilters[listIdx]);
			bool v = key.getBool(s.c_str());
			key.setBool(s.c_str(),!v);
			RebuildList();
			if (selIdx==(int)mSortedFilters.size()) selIdx--;
			if (selIdx!=-1) mListBox.SetSelection(selIdx);
		}
		return true;
	}

	if (id == IDC_SHOWALL) {
		mbShowAll = true;
		RebuildList();
	}

	return false;
}

void VDDialogFilterListW32::RebuildList() {
	mListBox.Clear();
	mFilterList.clear();
	FilterEnumerateFilters(mFilterList);

	VDRegistryAppKey key("Hide Video Filters");
	int hide_count=0;

	mSortedFilters.clear();
	for(std::list<FilterBlurb>::iterator it(mFilterList.begin()), itEnd(mFilterList.end()); it!=itEnd; ++it) {
  	VDString s = format_hide_key(*it);
    (*it).hide = key.getBool(s.c_str());
    if ((*it).hide && !mbShowAll) {
      hide_count++;
      continue;
    }
		mSortedFilters.push_back(&*it);
	}

	std::sort(mSortedFilters.begin(), mSortedFilters.end(), FilterBlurbSort());

	VDStringW s;
	uintptr idx = 0;
	for(vdfastvector<const FilterBlurb *>::const_iterator it(mSortedFilters.begin()), itEnd(mSortedFilters.end()); it!=itEnd; ++it, ++idx) {
		const FilterBlurb& fb = **it;

		s.sprintf(fb.hide ? L" - %ls\t%ls" : L"%ls\t%ls", VDTextAToW(fb.name).c_str(), VDTextAToW(fb.author).c_str());
		mListBox.AddItem(s.c_str(), idx);
	}

	EnableWindow(GetDlgItem(mhdlg,IDC_SHOWALL),hide_count>0);
}

void VDDialogFilterListW32::OnSelectionChanged(VDUIProxyListBoxControl *sender, int selIdx) {
	if (selIdx < 0) {
		SetControlText(IDC_FILTER_INFO, L"");
		return;
	}

	uintptr listIdx = mListBox.GetItemData(selIdx);

	if (listIdx >= mSortedFilters.size()) {
		SetControlText(IDC_FILTER_INFO, L"");
		return;
	}

	const FilterBlurb& fb = *mSortedFilters[listIdx];

	SetControlText(IDC_FILTER_INFO, VDTextAToW(fb.description).c_str());
}

void VDDialogFilterListW32::OnDoubleClick(VDUIProxyListBoxControl *sender, int selIdx) {
	if (!OnOK())
		End(true);
}

FilterDefinitionInstance *VDUIShowDialogAddFilter(VDGUIHandle hParent) {
	VDDialogFilterListW32 dlg;

	return dlg.Activate(hParent);
}
