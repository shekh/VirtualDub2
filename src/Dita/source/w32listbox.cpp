//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2004 Avery Lee
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

#include <stdafx.h>
#include <vd2/system/w32assist.h>
#include <windows.h>
#include <commctrl.h>
#include <vector>

#include <vd2/Dita/w32control.h>

class VDUIListBoxW32 : public VDUIControlW32, public IVDUIList {
public:
	VDUIListBoxW32();

	void *AsInterface(uint32 id);

	bool Create(IVDUIParameters *);
	void PreLayoutBaseW32(const VDUILayoutSpecs&);

	int GetValue();
	void SetValue(int);

protected:
	void Clear();
	int GetItemCount();
	uintptr GetItemData(int index);
	void SetItemData(int index, uintptr data);
	int AddItem(const wchar_t *text, uintptr data = 0);

	void OnCommandCallback(UINT code);

	int mSelected;
};

extern IVDUIWindow *VDCreateUIListBox() { return new VDUIListBoxW32; }

VDUIListBoxW32::VDUIListBoxW32()
	: mSelected(-1)
{
}

void *VDUIListBoxW32::AsInterface(uint32 id) {
	if (id == IVDUIList::kTypeID) return static_cast<IVDUIList *>(this);

	return VDUIControlW32::AsInterface(id);
}

bool VDUIListBoxW32::Create(IVDUIParameters *pParameters) {
	return CreateW32(pParameters, "LISTBOX", LBS_NOTIFY|LBS_NOINTEGRALHEIGHT|WS_TABSTOP|WS_VSCROLL);
}

void VDUIListBoxW32::PreLayoutBaseW32(const VDUILayoutSpecs& parentConstraints) {
}

void VDUIListBoxW32::Clear() {
	SendMessage(mhwnd, LB_RESETCONTENT, 0, 0);
}

int VDUIListBoxW32::GetItemCount() {
	LRESULT lr = SendMessage(mhwnd, LB_GETCOUNT, 0, 0);

	VDASSERT(lr != LB_ERR);

	return lr == LB_ERR ? 0 : (int)lr;
}

uintptr VDUIListBoxW32::GetItemData(int index) {
	return SendMessage(mhwnd, LB_GETITEMDATA, index, 0);
}

void VDUIListBoxW32::SetItemData(int index, uintptr data) {
	SendMessage(mhwnd, LB_SETITEMDATA, index, (LPARAM)data);
}

int VDUIListBoxW32::AddItem(const wchar_t *text, uintptr data) {
	int idx;

	if (VDIsWindowsNT())
		idx = (int)SendMessageW(mhwnd, LB_ADDSTRING, 0, (LPARAM)text);
	else
		idx = (int)SendMessageA(mhwnd, LB_ADDSTRING, 0, (LPARAM)VDTextWToA(text).c_str());

	if (idx >= 0)
		SendMessage(mhwnd, LB_SETITEMDATA, (WPARAM)idx, (LPARAM)data);

	return idx;
}

void VDUIListBoxW32::OnCommandCallback(UINT code) {
   	if (code == LBN_SELCHANGE) {
   		int iSel = (int)SendMessage(mhwnd, LB_GETCURSEL, 0, 0);

   		if (iSel != mSelected) {
   			mSelected = iSel;

			mpBase->ProcessValueChange(this, mID);
   			mpBase->DispatchEvent(this, mID, IVDUICallback::kEventSelect, mSelected);
   		}
	}
}

int VDUIListBoxW32::GetValue() {
	return mSelected;
}

void VDUIListBoxW32::SetValue(int value) {
	mSelected = value;		// prevents recursion
	SendMessage(mhwnd, LB_SETCURSEL, value, 0);
}


///////////////////////////////////////////////////////////////////////////

class VDUIComboBoxW32 : public VDUIControlW32, public IVDUIList {
public:
	VDUIComboBoxW32();

	void *AsInterface(uint32 id);

	bool Create(IVDUIParameters *);
	void SetArea(const vduirect&);
	void PreLayoutBaseW32(const VDUILayoutSpecs&);

	int GetValue();
	void SetValue(int);

protected:
	void Clear();
	int GetItemCount();
	uintptr GetItemData(int index);
	void SetItemData(int index, uintptr data);
	int AddItem(const wchar_t *text, uintptr data);

	void OnCommandCallback(UINT code);

	int mSelected;
	int mHeight;
};

extern IVDUIWindow *VDCreateUIComboBox() { return new VDUIComboBoxW32; }

VDUIComboBoxW32::VDUIComboBoxW32()
	: mSelected(-1)
{
}

void *VDUIComboBoxW32::AsInterface(uint32 id) {
	if (id == IVDUIList::kTypeID) return static_cast<IVDUIList *>(this);

	return VDUIControlW32::AsInterface(id);
}

bool VDUIComboBoxW32::Create(IVDUIParameters *pParameters) {
	if (!CreateW32(pParameters, "COMBOBOX", CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL))
		return false;

	RECT r;
	GetClientRect(mhwnd, &r);
	mHeight = r.bottom - r.top;

	return true;
}

void VDUIComboBoxW32::SetArea(const vduirect& pos) {
	SetWindowPos(mhwnd, NULL, pos.left, pos.top, pos.width(), mHeight*5, SWP_NOZORDER|SWP_NOACTIVATE);	// FIXME
	VDUIWindow::SetArea(pos);
}

void VDUIComboBoxW32::PreLayoutBaseW32(const VDUILayoutSpecs& parentConstraints) {
	mLayoutSpecs.minsize.w = GetSystemMetrics(SM_CXVSCROLL);
	mLayoutSpecs.minsize.h = mHeight;
}

void VDUIComboBoxW32::Clear() {
	SendMessage(mhwnd, CB_RESETCONTENT, 0, 0);
}

int VDUIComboBoxW32::GetItemCount() {
	LRESULT lr = SendMessage(mhwnd, CB_GETCOUNT, 0, 0);

	VDASSERT(lr != CB_ERR);

	return (lr == CB_ERR) ? 0 : (int)lr;
}

uintptr VDUIComboBoxW32::GetItemData(int index) {
	return (uintptr)SendMessage(mhwnd, CB_GETITEMDATA, index, 0);
}

void VDUIComboBoxW32::SetItemData(int index, uintptr data) {
	SendMessage(mhwnd, CB_SETITEMDATA, index, (LPARAM)data);
}

int VDUIComboBoxW32::AddItem(const wchar_t *text, uintptr data) {
	int sel;

	if (VDIsWindowsNT())
		sel = (int)SendMessageW(mhwnd, CB_ADDSTRING, 0, (LPARAM)text);
	else
		sel = (int)SendMessageA(mhwnd, CB_ADDSTRING, 0, (LPARAM)VDTextWToA(text).c_str());

	if (sel >= 0)
		SendMessage(mhwnd, CB_SETITEMDATA, (WPARAM)sel, (LPARAM)data);

	return sel;
}

void VDUIComboBoxW32::OnCommandCallback(UINT code) {
   	if (code == CBN_SELCHANGE) {
   		int iSel = (int)SendMessage(mhwnd, CB_GETCURSEL, 0, 0);

   		if (iSel != mSelected) {
   			mSelected = iSel;

			mpBase->ProcessValueChange(this, mID);
   			mpBase->DispatchEvent(this, mID, IVDUICallback::kEventSelect, mSelected);
   		}
	}
}

int VDUIComboBoxW32::GetValue() {
	return mSelected;
}

void VDUIComboBoxW32::SetValue(int value) {
	mSelected = value;		// prevents recursion
	SendMessage(mhwnd, CB_SETCURSEL, value, 0);
}

