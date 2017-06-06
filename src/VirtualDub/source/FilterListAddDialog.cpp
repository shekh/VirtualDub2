#include "stdafx.h"
#include "filters.h"
#include "plugins.h"
#include "resource.h"
#include <list>
#include <vd2/Dita/services.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/VDLib/UIProxies.h>
#include <vd2/system/registry.h>
#include <commctrl.h>

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
	class ListItem : public vdrefcounted<IVDUIListViewVirtualItem> {
	public:
		int filter;
		VDStringW name;
		VDStringW author;

		void GetText(int subItem, VDStringW& s) const {
			if (subItem==0) s = name;
			if (subItem==1) s = author;
		}
	};

	VDDialogFilterListW32();

	FilterDefinitionInstance *Activate(VDGUIHandle hParent);

protected:
	bool OnLoaded();
	void OnDataExchange(bool write);
	bool OnCommand(uint32 id, uint32 extcode);
	void RebuildList(bool reset_tag=false);
	void ReloadCheck();
	void OnItemCheckedChanged(VDUIProxyListView *sender, int item);
	void OnSelectionChanged(VDUIProxyListView *sender, int selIdx);
	void OnDoubleClick(VDUIProxyListView *sender, int selIdx);
	void OnDestroy();
	void OnSize();
	bool OnErase(VDZHDC hdc);

	VDUIProxyListView mListView;
	FilterDefinitionInstance	*mpFilterDefInst;
	std::list<FilterBlurb>		mFilterList;
	vdfastvector<const FilterBlurb *>	mSortedFilters;
	bool mbShowAll;
	bool mbShowLoaded;
	bool mbShowCheck;

	VDDelegate          mDelegateCheckedChanged;
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
	mListView.OnItemCheckedChanged() += mDelegateCheckedChanged.Bind(this, &VDDialogFilterListW32::OnItemCheckedChanged);
	mListView.OnItemSelectionChanged() += mDelegateSelChanged.Bind(this, &VDDialogFilterListW32::OnSelectionChanged);
	mListView.OnItemDoubleClicked() += mDelegateDblClk.Bind(this, &VDDialogFilterListW32::OnDoubleClick);
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

	AddProxy(&mListView, IDC_FILTER_LIST);
	mListView.SetFullRowSelectEnabled(true);
	mListView.SetItemCheckboxesEnabled(false);
	mListView.InsertColumn(0, L"Name", 230);
	mListView.InsertColumn(1, L"Author", 150);

	mbShowAll = false;
	mbShowLoaded = false;
	mbShowCheck = false;
	RebuildList(true);

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
		int selIdx = mListView.GetSelectedIndex();
		if (selIdx < 0) {
			FailValidation(IDC_FILTER_LIST);
			return;
		}

		ListItem *item = (ListItem *)mListView.GetVirtualItem(selIdx);
		uintptr listIdx = item->filter;
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

		SetFocusToControl(IDC_FILTER_LIST);
		if (!filename.empty()) {
			try {
				VDAddPluginModule(filename.c_str());
				mbShowLoaded = true;
				mbShowAll = false;
				mbShowCheck = false;
				mListView.SetItemCheckboxesEnabled(false);
				RebuildList();
			} catch(const MyError& e) {
				e.post(mhdlg, g_szError);
			}
		}

		return true;
	}

	if (id == IDC_ONOFF) {
		int selIdx = mListView.GetSelectedIndex();
		if (selIdx!=-1) {
			if (!mbShowCheck) {
				mListView.SetItemCheckboxesEnabled(true);
				ReloadCheck();
				mbShowCheck = true;
			}
			bool enabled = mListView.IsItemChecked(selIdx);
			mListView.SetItemChecked(selIdx, !enabled);
		}
		return true;
	}

	if (id == IDC_SHOWALL) {
		if (mbShowLoaded) {
			mbShowLoaded = false;
		} else {
			mbShowAll = true;
			mListView.SetItemCheckboxesEnabled(true);
			mbShowCheck = true;
		}
		SetFocusToControl(IDC_FILTER_LIST);
		RebuildList();
	}

	return false;
}

void VDDialogFilterListW32::RebuildList(bool reset_tag) {
	FilterDefinitionInstance* sel = 0;
	ListItem *item = (ListItem *)mListView.GetSelectedVirtualItem();
	if (item)
		sel = mSortedFilters[item->filter]->key;

	mListView.Clear();
	mFilterList.clear();
	FilterEnumerateFilters(mFilterList);

	VDRegistryAppKey key("Hide Video Filters");
	int hide_count=0;

	mSortedFilters.clear();
	for(std::list<FilterBlurb>::iterator it(mFilterList.begin()), itEnd(mFilterList.end()); it!=itEnd; ++it) {
		FilterBlurb& item = *it;
		VDString s = format_hide_key(item);
		item.hide = key.getBool(s.c_str());

		if (mbShowLoaded) {
			if (item.key) {
				int tag = item.key->tag;
				if (tag!=0) continue;
			}
		} else {
			if (reset_tag && item.key) {
				item.key->tag = 1;
			}
			if (item.hide && !mbShowAll) {
				hide_count++;
				continue;
			}
		}
		mSortedFilters.push_back(&item);
	}

	std::sort(mSortedFilters.begin(), mSortedFilters.end(), FilterBlurbSort());

	uintptr idx = 0;
	uintptr sel_idx = 0;
	for(vdfastvector<const FilterBlurb *>::const_iterator it(mSortedFilters.begin()), itEnd(mSortedFilters.end()); it!=itEnd; ++it, ++idx) {
		const FilterBlurb& fb = **it;

		ListItem* item = new ListItem();
		item->filter = idx;
		item->name = VDTextAToW(fb.name);
		item->author = VDTextAToW(fb.author);
		int index = mListView.InsertVirtualItem(-1,item);
		mListView.SetItemChecked(index, !fb.hide);
		if (fb.key==sel)
			sel_idx = idx;
	}

	if (sel_idx!=-1)
		mListView.SetSelectedIndex(sel_idx);

	EnableWindow(GetDlgItem(mhdlg,IDC_SHOWALL),hide_count>0 || mbShowLoaded);
}

void VDDialogFilterListW32::ReloadCheck() {
	uintptr idx = 0;
	for(vdfastvector<const FilterBlurb *>::const_iterator it(mSortedFilters.begin()), itEnd(mSortedFilters.end()); it!=itEnd; ++it, ++idx) {
		const FilterBlurb& fb = **it;
		mListView.SetItemChecked(idx, !fb.hide);
	}
}

void VDDialogFilterListW32::OnItemCheckedChanged(VDUIProxyListView *sender, int index) {
	if (!mbShowCheck) return;
	ListItem *item = (ListItem *)mListView.GetVirtualItem(index);
	uintptr listIdx = item->filter;
	bool enabled = mListView.IsItemChecked(index);
	VDRegistryAppKey key("Hide Video Filters");
	VDString s = format_hide_key(*mSortedFilters[listIdx]);
	key.setBool(s.c_str(),!enabled);
}

void VDDialogFilterListW32::OnSelectionChanged(VDUIProxyListView *sender, int selIdx) {
	if (selIdx < 0) {
		SetControlText(IDC_FILTER_INFO, L"");
		return;
	}

	ListItem *item = (ListItem *)mListView.GetVirtualItem(selIdx);
	uintptr listIdx = item->filter;

	if (listIdx >= mSortedFilters.size()) {
		SetControlText(IDC_FILTER_INFO, L"");
		return;
	}

	const FilterBlurb& fb = *mSortedFilters[listIdx];

	SetControlText(IDC_FILTER_INFO, VDTextAToW(fb.description).c_str());
}

void VDDialogFilterListW32::OnDoubleClick(VDUIProxyListView *sender, int selIdx) {
	if (!OnOK())
		End(true);
}

FilterDefinitionInstance *VDUIShowDialogAddFilter(VDGUIHandle hParent) {
	VDDialogFilterListW32 dlg;

	return dlg.Activate(hParent);
}
