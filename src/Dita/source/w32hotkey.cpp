//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2006 Avery Lee
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
#include <windows.h>
#include <commctrl.h>
#include <vd2/Dita/w32control.h>

///////////////////////////////////////////////////////////////////////////
//
//	VDUIHotkeyW32
//
///////////////////////////////////////////////////////////////////////////

class VDUIHotkeyW32 : public VDUIControlW32 {
public:
	bool Create(IVDUIParameters *);

	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);

	void OnCommandCallback(UINT code);

	int GetValue();
	void SetValue(int value);

protected:
	int mValue;
};

extern IVDUIWindow *VDCreateUIHotkey() { return new VDUIHotkeyW32; }

bool VDUIHotkeyW32::Create(IVDUIParameters *pParams) {
	if (!CreateW32(pParams, HOTKEY_CLASS, WS_TABSTOP))
		return false;

	mValue = 0;
	return true;
}

void VDUIHotkeyW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	vduisize pad = mpBase->MapUnitsToPixels(vduisize(8,12));

	mLayoutSpecs.minsize.w = GetSystemMetrics(SM_CXVSCROLL);
	mLayoutSpecs.minsize.h = pad.h;
}

void VDUIHotkeyW32::OnCommandCallback(UINT code) {
	if (code == EN_CHANGE) {
		mValue = SendMessage(mhwnd, HKM_GETHOTKEY, 0, 0);
		mpBase->ProcessActivation(this, mID);
		mpBase->DispatchEvent(this, mID, IVDUICallback::kEventSelect, 0);
	}
}

int VDUIHotkeyW32::GetValue() {
	return mValue;
}

void VDUIHotkeyW32::SetValue(int value) {
	mValue = value;
	SendMessage(mhwnd, HKM_SETHOTKEY, value, 0);
}
