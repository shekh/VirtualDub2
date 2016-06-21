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
#include <vd2/system/filesys.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/VDLib/UIProxies.h>
#include "filters.h"
#include "plugins.h"
#include "resource.h"

extern const char g_szError[];

class VDUIDialogPlugins : public VDDialogFrameW32 {
public:
	VDUIDialogPlugins();
	~VDUIDialogPlugins();

protected:
	bool OnLoaded();
	void OnDestroy();
	void OnDataExchange(bool write);
	bool OnCommand(uint32 id, uint32 extcode);

	void OnItemSelectionChanged(VDUIProxyListView *source, int index);

	void UpdateEnables();

	struct ListItem : public vdrefcounted<IVDUIListViewVirtualItem> {
		virtual bool HasConfigure() const = 0;
		virtual bool HasAbout() const = 0;
		virtual void Configure(VDZHWND hwndParent) = 0;
		virtual void About(VDZHWND hwndParent) = 0;
	};

	struct FilterItem : public ListItem {
		FilterDefinitionInstance *mpFDI;

		void GetText(int subItem, VDStringW& s) const;

		virtual bool HasConfigure() const;
		virtual bool HasAbout() const;
		virtual void Configure(VDZHWND hwndParent);
		virtual void About(VDZHWND hwndParent);
	};

	struct PluginItem : public ListItem {
		const VDPluginDescription *mpDesc;

		void GetText(int subItem, VDStringW& s) const;

		virtual bool HasConfigure() const;
		virtual bool HasAbout() const;
		virtual void Configure(VDZHWND hwndParent);
		virtual void About(VDZHWND hwndParent);
	};

	VDUIProxyListView mList;
	VDDelegate mDelItemSelectionChanged;

	VDStringW	mRecoveryPath;
};

VDUIDialogPlugins::VDUIDialogPlugins()
	: VDDialogFrameW32(IDD_PLUGINS)
{
}

VDUIDialogPlugins::~VDUIDialogPlugins() {
}

void VDUIDialogPlugins::FilterItem::GetText(int subItem, VDStringW& s) const {
	switch(subItem) {
		case 0:
			{
				VDExternalModule *module = mpFDI->GetModule();

				if (module)
					s = VDFileSplitPath(module->GetFilename().c_str());
				else
					s = L"(internal)";
			}
			break;

		case 1:
			s = L"Video filter";
			break;

		case 2:
			s = VDTextAToW(mpFDI->GetName());
			break;
	}
}

bool VDUIDialogPlugins::FilterItem::HasConfigure() const {
	return mpFDI->HasStaticConfigure();
}

bool VDUIDialogPlugins::FilterItem::HasAbout() const {
	return mpFDI->HasStaticAbout();
}

void VDUIDialogPlugins::FilterItem::Configure(VDZHWND hwndParent) {
	if (!mpFDI->HasStaticConfigure())
		return;

	try {
		const FilterDefinition& fd = mpFDI->Attach();

		if (fd.mpStaticConfigureProc)
			fd.mpStaticConfigureProc((VDXHWND)hwndParent);

		mpFDI->Detach();
	} catch(const MyError& e) {
		e.post(hwndParent, g_szError);
	}
}

void VDUIDialogPlugins::FilterItem::About(VDZHWND hwndParent) {
	if (!mpFDI->HasStaticAbout())
		return;

	try {
		const FilterDefinition& fd = mpFDI->Attach();

		if (fd.mpStaticAboutProc)
			fd.mpStaticAboutProc((VDXHWND)hwndParent);

		mpFDI->Detach();
	} catch(const MyError& e) {
		e.post(hwndParent, g_szError);
	}
}

void VDUIDialogPlugins::PluginItem::GetText(int subItem, VDStringW& s) const {
	switch(subItem) {
		case 0:
			s = VDFileSplitPath(mpDesc->mpModule->GetFilename().c_str());
			break;

		case 1:
			s = L"Plugin";
			break;

		case 2:
			s = mpDesc->mName.c_str();
			break;
	}
}

bool VDUIDialogPlugins::PluginItem::HasConfigure() const {
	return mpDesc->mbHasStaticConfigure;
}

bool VDUIDialogPlugins::PluginItem::HasAbout() const {
	return mpDesc->mbHasStaticAbout;
}

void VDUIDialogPlugins::PluginItem::Configure(VDZHWND hwndParent) {
	try {
		mpDesc->mpModule->Lock();
		if (mpDesc->mpInfo && mpDesc->mbHasStaticConfigure)
			mpDesc->mpInfo->mpStaticConfigureProc((VDXHWND)hwndParent);
		mpDesc->mpModule->Unlock();
	} catch(const MyError& e) {
		e.post(hwndParent, g_szError);
	}
}

void VDUIDialogPlugins::PluginItem::About(VDZHWND hwndParent) {
	try {
		mpDesc->mpModule->Lock();
		if (mpDesc->mpInfo && mpDesc->mbHasStaticAbout)
			mpDesc->mpInfo->mpStaticAboutProc((VDXHWND)hwndParent);
		mpDesc->mpModule->Unlock();
	} catch(const MyError& e) {
		e.post(hwndParent, g_szError);
	}
}

bool VDUIDialogPlugins::OnLoaded() {
	AddProxy(&mList, IDC_LIST);

	mList.SetFullRowSelectEnabled(true);

	mList.InsertColumn(0, L"Module", 0);
	mList.InsertColumn(1, L"Type", 0);
	mList.InsertColumn(2, L"Name", 0);

	vdfastvector<FilterDefinitionInstance *> filters;
	VDEnumerateFilters(filters);

	while(!filters.empty()) {
		FilterDefinitionInstance *fdi = filters.back();

		if (fdi->HasStaticAbout() || fdi->HasStaticConfigure()) {
			vdrefptr<FilterItem> fi(new FilterItem);
			fi->mpFDI = fdi;

			mList.InsertVirtualItem(-1, fi);
		}

		filters.pop_back();
	}

	std::vector<VDPluginDescription *> descs;
	VDEnumeratePluginDescriptions(descs, kVDXPluginType_Input);
	VDEnumeratePluginDescriptions(descs, kVDXPluginType_Audio);
	VDEnumeratePluginDescriptions(descs, kVDXPluginType_Tool);

	while(!descs.empty()) {
		VDPluginDescription *desc = descs.back();

		if (desc->mbHasStaticAbout || desc->mbHasStaticConfigure) {
			vdrefptr<PluginItem> pi(new PluginItem);
			pi->mpDesc = descs.back();

			mList.InsertVirtualItem(-1, pi);
		}

		descs.pop_back();
	}

	mList.AutoSizeColumns();

	SetFocusToControl(IDC_LIST);
	return true;
}

void VDUIDialogPlugins::OnDestroy() {
}

void VDUIDialogPlugins::OnDataExchange(bool write) {
}

bool VDUIDialogPlugins::OnCommand(uint32 id, uint32 extcode) {
	switch(id) {
		case IDC_CONFIGURE:
			{
				ListItem *item = static_cast<ListItem *>(mList.GetSelectedItem());

				if (item && item->HasConfigure()) {
					item->Configure(mhdlg);
				}
			}
			return true;

		case IDC_ABOUT:
			{
				ListItem *item = static_cast<ListItem *>(mList.GetSelectedItem());

				if (item && item->HasAbout()) {
					item->About(mhdlg);
				}
			}
			return true;
	}
	return false;
}

void VDUIDialogPlugins::OnItemSelectionChanged(VDUIProxyListView *source, int index) {
	ListItem *item = static_cast<ListItem *>(mList.GetSelectedItem());

	EnableControl(IDC_ABOUT, item && item->HasAbout());
	EnableControl(IDC_ABOUT, item && item->HasConfigure());
}

void VDUIDialogPlugins::UpdateEnables() {
}

///////////////////////////////////////////////////////////////////////////

void VDUIShowDialogPlugins(VDGUIHandle h) {
	VDUIDialogPlugins dlg;

	dlg.ShowDialog(h);
}
