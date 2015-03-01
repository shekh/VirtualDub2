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
#include <windows.h>
#include <richedit.h>
#include <vd2/system/w32assist.h>
#include <vd2/Dita/w32control.h>
#include <vector>
#include <deque>

#ifndef _MSC_VER
	#pragma comment(lib, "comctl32")
#endif


class VDUITextEditW32 : public VDUIControlW32 {
public:
	bool Create(IVDUIParameters *pParameters);

	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);

	void OnCommandCallback(UINT code);
};

extern IVDUIWindow *VDCreateUITextEdit() { return new VDUITextEditW32; }

bool VDUITextEditW32::Create(IVDUIParameters *pParameters) {
	return CreateW32(pParameters, "EDIT", ES_AUTOHSCROLL|WS_TABSTOP);
}

void VDUITextEditW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	mLayoutSpecs.minsize.w = 0;
	mLayoutSpecs.minsize.h = mpBase->MapUnitsToPixels(vduisize(12,12)).w;
}

void VDUITextEditW32::OnCommandCallback(UINT code) {
	if (code == EN_CHANGE) {
		UpdateCaptionW32();
		mpBase->ProcessValueChange(this, mID);
		mpBase->DispatchEvent(this, mID, IVDUICallback::kEventSelect, 0);
	}
}

///////////////////////////////////////////////////////////////////////////

class VDUITextAreaW32 : public VDUIControlW32 {
public:
	void *AsInterface(uint32 id);
	bool Create(IVDUIParameters *pParameters);

protected:
	static HMODULE shmodRichEdit;
};

extern IVDUIWindow *VDCreateUITextArea() { return new VDUITextAreaW32; }

HMODULE VDUITextAreaW32::shmodRichEdit = NULL;

void *VDUITextAreaW32::AsInterface(uint32 id) {
//	if (id == IVDUIRichText::kTypeID) return static_cast<IVDUIRichText *>(this);

	return VDUIControlW32::AsInterface(id);
}

bool VDUITextAreaW32::Create(IVDUIParameters *pParameters) {
	if (!shmodRichEdit)
		shmodRichEdit = LoadLibrary("riched32");

	DWORD dwStyle = ES_MULTILINE|ES_AUTOVSCROLL|WS_TABSTOP;

	if (pParameters->GetB(nsVDUI::kUIParam_Readonly, false)) {
		dwStyle |= ES_READONLY;
		dwStyle &= ~WS_TABSTOP;
	}

	if (!CreateW32(pParameters, "RichEdit", dwStyle))
		return false;

	if (dwStyle & ES_READONLY)
		SendMessage(mhwnd, EM_SETBKGNDCOLOR, FALSE, GetSysColor(COLOR_3DFACE));

	return true;
}
