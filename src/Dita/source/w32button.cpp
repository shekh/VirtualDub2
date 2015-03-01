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
#include <vd2/Dita/w32control.h>

///////////////////////////////////////////////////////////////////////////
//
//	VDUIButtonW32
//
///////////////////////////////////////////////////////////////////////////

class VDUIButtonW32 : public VDUIControlW32 {
public:
	bool Create(IVDUIParameters *);

	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);

	void OnCommandCallback(UINT code);
};

extern IVDUIWindow *VDCreateUIButton() { return new VDUIButtonW32; }

bool VDUIButtonW32::Create(IVDUIParameters *pParams) {
	return CreateW32(pParams, "BUTTON", WS_TABSTOP);
}

void VDUIButtonW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	vduisize pad = mpBase->MapUnitsToPixels(vduisize(8,14));

	SIZE siz = SizeText(parentConstraints.minsize.w, pad.w, pad.h);

	mLayoutSpecs.minsize.w	= pad.w + siz.cx;

	// hack for non-command buttons
	if ((mAlignY & nsVDUI::kAlignTypeMask) == nsVDUI::kFill)
		mLayoutSpecs.minsize.h	= 0;
	else
		mLayoutSpecs.minsize.h	= pad.h;
}

void VDUIButtonW32::OnCommandCallback(UINT code) {
	if (code == BN_CLICKED) {
		mpBase->ProcessActivation(this, mID);
		mpBase->DispatchEvent(this, mID, IVDUICallback::kEventSelect, 0);
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	VDUICheckboxW32
//
///////////////////////////////////////////////////////////////////////////

class VDUICheckboxW32 : public VDUIControlW32 {
public:
	bool Create(IVDUIParameters *);
	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);
	nsVDUI::CompressType GetCompressType();
	int GetValue();
	void SetValue(int);
	void OnCommandCallback(UINT);
};

extern IVDUIWindow *VDCreateUICheckbox() { return new VDUICheckboxW32; }

bool VDUICheckboxW32::Create(IVDUIParameters *pParams) {
	return CreateW32(pParams, "BUTTON", BS_AUTOCHECKBOX|BS_TOP|BS_MULTILINE|WS_TABSTOP);
}

void VDUICheckboxW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	const vduisize pad = mpBase->MapUnitsToPixels(vduisize(14, 10));

	mLayoutSpecs.minsize.w = pad.w;
	mLayoutSpecs.minsize.h = pad.h;

	SIZE siz = SizeText(parentConstraints.minsize.w - pad.w, pad.w, pad.h);

	siz.cy += 2*GetSystemMetrics(SM_CYEDGE);

	mLayoutSpecs.minsize.w += siz.cx;
	if (mLayoutSpecs.minsize.h < siz.cy)
		mLayoutSpecs.minsize.h = siz.cy;
}

nsVDUI::CompressType VDUICheckboxW32::GetCompressType() {
	return nsVDUI::kCompressCheckbox;
}

int VDUICheckboxW32::GetValue() {
	if (mhwnd)
		return BST_CHECKED == SendMessage(mhwnd, BM_GETCHECK, 0, 0);

	return false;
}

void VDUICheckboxW32::SetValue(int i) {
	if (mhwnd)
		SendMessage(mhwnd, BM_SETCHECK, i?BST_CHECKED:BST_UNCHECKED, 0);

}

void VDUICheckboxW32::OnCommandCallback(UINT code) {
	if (code == BN_CLICKED) {
		mpBase->ProcessValueChange(this, mID);
		mpBase->DispatchEvent(this, mID, IVDUICallback::kEventSelect, GetValue());
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	VDUIOptionW32
//
///////////////////////////////////////////////////////////////////////////

class VDUIOptionW32 : public VDUIControlW32 {
public:
	enum { kTypeID = 'optn' };

	VDUIOptionW32();
	~VDUIOptionW32();
	void *AsInterface(uint32 id);
	bool Create(IVDUIParameters *pParams);
	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);
	nsVDUI::CompressType GetCompressType();
	int GetValue();
	void SetValue(int i);
	void OnCommandCallback(UINT code);

protected:
	VDUIOptionW32	*mpBaseOption;
	int	mnItems;
	int mnSelected;
};

IVDUIWindow *VDCreateUIOption() { return new VDUIOptionW32; }

VDUIOptionW32::VDUIOptionW32()
	: mpBaseOption(NULL)
	, mnItems(1)
	, mnSelected(0)
{
}

VDUIOptionW32::~VDUIOptionW32() {
}

void *VDUIOptionW32::AsInterface(uint32 id) {
	if (id == kTypeID)	return static_cast<VDUIOptionW32 *>(this);

	return VDUIControlW32::AsInterface(id);
}

bool VDUIOptionW32::Create(IVDUIParameters *pParams) {
	IVDUIWindow *pWin = this;
	
	while(pWin = mpParent->GetPreviousChild(pWin)) {
		mpBaseOption = vdpoly_cast<VDUIOptionW32 *>(pWin);
		if (mpBaseOption) {
			VDUIOptionW32 *pTrueBase = mpBaseOption->mpBaseOption;
			if (pTrueBase)
				mpBaseOption = pTrueBase;
			break;
		}
	}

	if (CreateW32(pParams, "BUTTON", mpBaseOption	? (BS_AUTORADIOBUTTON|BS_TOP|BS_MULTILINE|WS_TABSTOP)
													: (BS_AUTORADIOBUTTON|BS_TOP|BS_MULTILINE|WS_GROUP)))
	{
		if (mpBaseOption)
			++mpBaseOption->mnItems;
		else
			SendMessage(mhwnd, BM_SETCHECK, BST_CHECKED, 0);

		return true;
	}

	return false;
}

void VDUIOptionW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	vduisize pad = mpBase->MapUnitsToPixels(vduisize(14,10));

	mLayoutSpecs.minsize.w = pad.w;
	mLayoutSpecs.minsize.h = pad.h;

	SIZE siz = SizeText(parentConstraints.minsize.w, pad.w, pad.h);

	siz.cy += 2*GetSystemMetrics(SM_CYEDGE);

	mLayoutSpecs.minsize.w += siz.cx;
	if (mLayoutSpecs.minsize.h < siz.cy)
		mLayoutSpecs.minsize.h = siz.cy;
}

nsVDUI::CompressType VDUIOptionW32::GetCompressType() {
	return nsVDUI::kCompressOption;
}

int VDUIOptionW32::GetValue() {
	VDASSERT(!mpBaseOption);

	return mnSelected;
}

void VDUIOptionW32::SetValue(int i) {
	VDASSERT(!mpBaseOption);

	if (i >= mnItems)
		i = mnItems - 1;

	if (i < 0)
		i = 0;

	if (mnSelected != i) {
		mnSelected = i;
		mpBase->ProcessValueChange(this, mID);

		if (mhwnd) {
			// change me
			SendMessage(mhwnd, BM_SETCHECK, (i==0) ? BST_CHECKED : BST_UNCHECKED, 0);

			// change others
			for(IVDUIWindow *win = this; win; win = mpParent->GetNextChild(win)) {
				VDUIOptionW32 *opt = vdpoly_cast<VDUIOptionW32 *>(win);

				if (opt && opt->mpBaseOption == this)
					SendMessage(opt->mhwnd, BM_SETCHECK, opt->mID - mID == i ? BST_CHECKED : BST_UNCHECKED, 0);
			}
		}
	}
}

void VDUIOptionW32::OnCommandCallback(UINT code) {
	if (code == BN_CLICKED) {
		if (SendMessage(mhwnd, BM_GETCHECK, 0, 0) == BST_CHECKED) {
			int val = 0;
			
			if (mpBaseOption) {
				val = mID - mpBaseOption->mID;

				mpBaseOption->mnSelected = val;
				mpBase->ProcessValueChange(mpBaseOption, mpBaseOption->mID);
			} else {
				mnSelected = 0;
				mpBase->ProcessValueChange(this, mID);
			}
			mpBase->DispatchEvent(this, mID - val, IVDUICallback::kEventSelect, val);
		}
	}
}
