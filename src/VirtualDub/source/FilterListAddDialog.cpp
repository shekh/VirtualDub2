#include "stdafx.h"
#include "filters.h"
#include "plugins.h"
#include "resource.h"
#include <list>
#include <vd2/Dita/services.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/VDLib/UIProxies.h>
#include <vd2/system/registry.h>
#include <vd2/system/filesys.h>
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
		VDStringW module;

		void GetText(int subItem, VDStringW& s) const {
			if (subItem==0) s = name;
			if (subItem==1) s = author;
			if (subItem==2) {
				if(module.empty())
					s = L"(internal)";
				else
					s = module;
			}
		}
	};

	VDDialogFilterListW32();

	FilterDefinitionInstance *Activate(VDGUIHandle hParent);

protected:
	bool OnLoaded();
	void OnDataExchange(bool write);
	bool OnCommand(uint32 id, uint32 extcode);
	void RebuildList();
	void ReloadCheck();
	void OnItemCheckedChanged(VDUIProxyListView *sender, int item);
	void OnSelectionChanged(VDUIProxyListView *sender, int selIdx);
	void OnDoubleClick(VDUIProxyListView *sender, int selIdx);
	void OnColumnClicked(VDUIProxyListView *source, int column);
	void OnDestroy();
	void OnSize();
	bool OnErase(VDZHDC hdc);

	VDUIProxyListView mListView;
	FilterDefinitionInstance	*mpFilterDefInst;
	std::list<FilterBlurb>		mFilterList;
	vdfastvector<const FilterBlurb *>	mSortedFilters;
	VDStringW mShowModule;
	bool mbShowAll;
	bool mbShowCheck;

	VDDelegate					mDelegateCheckedChanged;
	VDDelegate					mDelegateSelChanged;
	VDDelegate					mDelegateDblClk;
	VDDelegate					mDelegateColumnClicked;

	VDDialogResizerW32 mResizer;

	struct FilterBlurbSort {
		int sort_name;
		int sort_author;
		int sort_module;

		bool operator()(const FilterBlurb *x, const FilterBlurb *y) {
			if (sort_author) {
				if (x->author==y->author)
					return x->name.comparei(y->name) < 0;
				else {
					if (sort_author==1)
						return x->author.comparei(y->author) < 0;
					if (sort_author==2)
						return x->author.comparei(y->author) > 0;
				}
			}

			if (sort_module) {
				if (x->module==y->module)
					return x->name.comparei(y->name) < 0;
				else {
					if (sort_module==1)
						return x->module.comparei(y->module) < 0;
					if (sort_module==2)
						return x->module.comparei(y->module) > 0;
				}
			}

			if (sort_name==2)
				return x->name.comparei(y->name) > 0;
			else
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
	mListView.OnColumnClicked() += mDelegateColumnClicked.Bind(this, &VDDialogFilterListW32::OnColumnClicked);
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
	mListView.InsertColumn(2, L"Module", 150);
	mListView.SetSortIcon(0, 1);

	mbShowAll = false;
	mbShowCheck = false;
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
	SendMessage(mListView.GetHandle(), LVM_SETCOLUMNWIDTH, 2, LVSCW_AUTOSIZE_USEHEADER);
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
		VDStringW title(L"Load external filter");
		#ifdef _M_AMD64
		title += L" (x64)";
		#else
		title += L" (x86)";
		#endif

		const VDStringW filename(VDGetLoadFileName(kFileDialog_LoadPlugin, (VDGUIHandle)mhdlg, title.c_str(), L"VirtualDub filter (*.vdf)\0*.vdf\0Windows Dynamic-Link Library (*.dll)\0*.dll\0All files (*.*)\0*.*\0", NULL, NULL, NULL));

		SetFocusToControl(IDC_FILTER_LIST);
		if (!filename.empty()) {
			try {
				VDAddPluginModule(filename.c_str());
				mShowModule = filename;
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
		if (!mShowModule.empty()) {
			mShowModule.clear();
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

void VDDialogFilterListW32::RebuildList() {
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
		VDStringW module = item.module;
		item.module = VDFileSplitPathRight(module);

		if (!mShowModule.empty()) {
			if (module!=mShowModule) continue;
		} else {
			if (item.hide && !mbShowAll) {
				hide_count++;
				continue;
			}
		}
		mSortedFilters.push_back(&item);
	}

	FilterBlurbSort sort;
	sort.sort_name = mListView.GetSortIcon(0);
	sort.sort_author = mListView.GetSortIcon(1);
	sort.sort_module = mListView.GetSortIcon(2);

	std::sort(mSortedFilters.begin(), mSortedFilters.end(), sort);

	uintptr idx = 0;
	uintptr sel_idx = 0;
	for(vdfastvector<const FilterBlurb *>::const_iterator it(mSortedFilters.begin()), itEnd(mSortedFilters.end()); it!=itEnd; ++it, ++idx) {
		const FilterBlurb& fb = **it;

		ListItem* item = new ListItem();
		item->filter = idx;
		item->name = VDTextAToW(fb.name);
		item->author = VDTextAToW(fb.author);
		item->module = fb.module;
		int index = mListView.InsertVirtualItem(-1,item);
		mListView.SetItemChecked(index, !fb.hide);
		if (fb.key==sel)
			sel_idx = idx;
	}

	if (sel_idx!=-1)
		mListView.SetSelectedIndex(sel_idx);

	EnableWindow(GetDlgItem(mhdlg,IDC_SHOWALL),hide_count>0 || !mShowModule.empty());
	SendMessage(mListView.GetHandle(), LVM_SETCOLUMNWIDTH, 2, LVSCW_AUTOSIZE_USEHEADER);
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

void VDDialogFilterListW32::OnColumnClicked(VDUIProxyListView *source, int column) {
	int x = mListView.GetSortIcon(column);
	mListView.SetSortIcon(column, x==1 ? 2:1);
	RebuildList();
}

FilterDefinitionInstance *VDUIShowDialogAddFilter(VDGUIHandle hParent) {
	VDDialogFilterListW32 dlg;

	return dlg.Activate(hParent);
}
