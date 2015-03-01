//	VirtualDub - Video processing and capture application
//	Internal filter library
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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "stdafx.h"
#include "SingleValueDialog.h"
#include <vd2/VDLib/Dialog.h>
#include "resource.h"

class VDUIDialogFilterSingleValue : public VDDialogFrameW32 {
public:
	VDUIDialogFilterSingleValue(sint32 value, sint32 minValue, sint32 maxValue, IVDXFilterPreview2 *ifp2, const wchar_t *title, void (*cb)(long, void *), void *cbdata);

	sint32 GetValue() const { return mValue; }

protected:
	bool OnLoaded();
	void OnDataExchange(bool write);
	void OnHScroll(uint32 id, int code);
	bool OnCommand(uint32 id, uint32 extcode);

	void UpdateSettingsString();

	sint32 mValue;
	sint32 mMinValue;
	sint32 mMaxValue;
	IVDXFilterPreview2 *mifp2;

	void (*mpUpdateFunction)(long value, void *data);
	void *mpUpdateFunctionData;

	const wchar_t *mpTitle;
};

VDUIDialogFilterSingleValue::VDUIDialogFilterSingleValue(sint32 value, sint32 minValue, sint32 maxValue, IVDXFilterPreview2 *ifp2, const wchar_t *title, void (*cb)(long, void *), void *cbdata)
	: VDDialogFrameW32(IDD_FILTER_SINGVAR)
	, mValue(value)
	, mMinValue(minValue)
	, mMaxValue(maxValue)
	, mifp2(ifp2)
	, mpUpdateFunction(cb)
	, mpUpdateFunctionData(cbdata)
	, mpTitle(title)
{
}

bool VDUIDialogFilterSingleValue::OnLoaded() {
	TBSetRange(IDC_SLIDER, mMinValue, mMaxValue);
	UpdateSettingsString();

	if (mifp2) {
		VDZHWND hwndPreviewButton = GetControl(IDC_PREVIEW);

		if (hwndPreviewButton)
			mifp2->InitButton((VDXHWND)hwndPreviewButton);
	}

	return VDDialogFrameW32::OnLoaded();
}

void VDUIDialogFilterSingleValue::OnDataExchange(bool write) {
	if (!write)
		TBSetValue(IDC_SLIDER, mValue);
}

void VDUIDialogFilterSingleValue::OnHScroll(uint32 id, int code) {
	if (id == IDC_SLIDER) {
		int v = TBGetValue(id);

		if (v != mValue) {
			mValue = v;

			UpdateSettingsString();

			if (mpUpdateFunction)
				mpUpdateFunction(mValue, mpUpdateFunctionData);

			if (mifp2)
				mifp2->RedoFrame();
		}
	}
}

bool VDUIDialogFilterSingleValue::OnCommand(uint32 id, uint32 extcode) {
	if (id == IDC_PREVIEW) {
		if (mifp2)
			mifp2->Toggle((VDXHWND)mhdlg);
		return true;
	}

	return false;
}

void VDUIDialogFilterSingleValue::UpdateSettingsString() {
	SetControlTextF(IDC_SETTING, L"%d", mValue);
}

bool VDFilterGetSingleValue(VDXHWND hWnd, sint32 cVal, sint32 *result, sint32 lMin, sint32 lMax, const char *title, IVDXFilterPreview2 *ifp2, void (*pUpdateFunction)(long value, void *data), void *pUpdateFunctionData) {
	VDStringW tbuf;
	tbuf.sprintf(L"Filter: %hs", title);

	VDUIDialogFilterSingleValue dlg(cVal, lMin, lMax, ifp2, tbuf.c_str(), pUpdateFunction, pUpdateFunctionData);

	if (dlg.ShowDialog((VDGUIHandle)hWnd)) {
		*result = dlg.GetValue();
		return true;
	}

	*result = cVal;
	return false;
}
