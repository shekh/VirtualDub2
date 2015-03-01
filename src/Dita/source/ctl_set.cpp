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
#include <vd2/Dita/basetypes.h>
#include <vector>

class VDUISet : public VDUIWindow {
public:
	VDUISet();
	~VDUISet();

	bool Create(IVDUIParameters *pParams);
	void PreLayoutBase(const VDUILayoutSpecs&);
	void PostLayoutBase(const vduirect&);

protected:
	int mnFillCount;
	int mComponentWidth;
	int mSpacing;
	bool mbVertical;
};

IVDUIWindow *VDCreateUISet() { return new VDUISet; }

VDUISet::VDUISet() {
	mAlignX = nsVDUI::kFill;
	mAlignY = nsVDUI::kFill;
}

VDUISet::~VDUISet() {
}

bool VDUISet::Create(IVDUIParameters *pParams) {
	mSpacing = pParams->GetI(nsVDUI::kUIParam_Spacing, 0);
	mbVertical = pParams->GetB(nsVDUI::kUIParam_IsVertical, false);
	return VDUIWindow::Create(pParams);
}

void VDUISet::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	vduisize pad = mpBase->MapUnitsToPixels(vduisize(mSpacing, mSpacing));
//	nsVDUI::eCompressType lastCompressType = nsVDUI::kCompressTypeLimit;

	mLayoutSpecs.minsize.w = 0;
	mLayoutSpecs.minsize.h = 0;
	mnFillCount = 0;

	const int spacing = mbVertical ? pad.h : pad.w;

	for(tChildren::iterator it = mChildren.begin(); it != mChildren.end(); ++it) {
		IVDUIWindow *pControl = (*it);
		nsVDUI::Alignment alignX, alignY;
//		nsVDUI::eCompressType compressType = pControl->GetCompressType();

//		if (lastCompressType != compressType || lastCompressType == nsVDUI::kCompressNone)
			if (mbVertical)
				mLayoutSpecs.minsize.h += spacing;
			else
				mLayoutSpecs.minsize.w += spacing;

//		lastCompressType = compressType;

		pControl->GetAlignment(alignX, alignY);

		const nsVDUI::Alignment align = mbVertical ? alignY : alignX;

		if ((align & nsVDUI::kAlignTypeMask) == nsVDUI::kFill)
			++mnFillCount;
		else {
			pControl->PreLayout(parentConstraints);

			const VDUILayoutSpecs& specs = pControl->GetLayoutSpecs();

			if (mbVertical) {
				mLayoutSpecs.minsize.h += specs.minsize.h;
				if (mLayoutSpecs.minsize.w < specs.minsize.w)
					mLayoutSpecs.minsize.w = specs.minsize.w;
			} else {
				mLayoutSpecs.minsize.w += specs.minsize.w;
				if (mLayoutSpecs.minsize.h < specs.minsize.h)
					mLayoutSpecs.minsize.h = specs.minsize.h;
			}
		}
	}

	int fillSize = 0;
	int fillLeft = mnFillCount;

	if (!mChildren.empty()) {
		if (mbVertical) {
			mLayoutSpecs.minsize.h -= spacing;
			fillSize = parentConstraints.minsize.h - mLayoutSpecs.minsize.h;
		} else {
			mLayoutSpecs.minsize.w -= spacing;
			fillSize = parentConstraints.minsize.w - mLayoutSpecs.minsize.w;
		}
	}

	if (fillSize < 0)
		fillSize = 0;

	if (fillLeft) {
		VDUILayoutSpecs constraints(parentConstraints);

		if (mbVertical)
			constraints.minsize.h = fillSize / fillLeft;
		else
			constraints.minsize.w = fillSize / fillLeft;

		for(tChildren::iterator it2 = mChildren.begin(); it2 != mChildren.end(); ++it2) {
			IVDUIWindow *pControl = (*it2);
			nsVDUI::Alignment alignX, alignY;

			pControl->GetAlignment(alignX, alignY);

			if (((mbVertical ? alignY : alignX) & nsVDUI::kAlignTypeMask) == nsVDUI::kFill) {

				pControl->PreLayout(constraints);

				const VDUILayoutSpecs& specs = pControl->GetLayoutSpecs();

				if (mbVertical) {
					mLayoutSpecs.minsize.h += specs.minsize.h;
					if (mLayoutSpecs.minsize.w < specs.minsize.w)
						mLayoutSpecs.minsize.w = specs.minsize.w;
				} else {
					mLayoutSpecs.minsize.w += specs.minsize.w;
					if (mLayoutSpecs.minsize.h < specs.minsize.h)
						mLayoutSpecs.minsize.h = specs.minsize.h;
				}
			}
		}
	}

	// cache this since our minsize will be whacked by alignment specs
	mComponentWidth = mbVertical ? mLayoutSpecs.minsize.h : mLayoutSpecs.minsize.w;
}

void VDUISet::PostLayoutBase(const vduirect& target) {
	VDUIWindow::PostLayoutBase(target);

	vduisize pad = mpBase->MapUnitsToPixels(vduisize(mSpacing, mSpacing));
//	nsVDUI::eCompressType lastCompressType = nsVDUI::kCompressTypeLimit;

	int spacing		= mbVertical ? pad.h : pad.w;
	int pos			= mbVertical ? target.top - spacing : target.left - spacing;
	int spill		= mbVertical ? target.height() - mComponentWidth : target.width() - mComponentWidth;
	int fillleft	= mnFillCount;

	for(tChildren::iterator it = mChildren.begin(); it != mChildren.end(); ++it) {
		IVDUIWindow *pControl = (*it);
		nsVDUI::Alignment alignX, alignY;
//		nsVDUI::eCompressType compressType = pControl->GetCompressType();

//		if (lastCompressType != compressType || lastCompressType == nsVDUI::kCompressNone)
			pos += spacing;

//		lastCompressType = compressType;

		pControl->GetAlignment(alignX, alignY);

		const VDUILayoutSpecs& specs = pControl->GetLayoutSpecs();
		int size = mbVertical ? specs.minsize.h : specs.minsize.w;

		if (((mbVertical ? alignY : alignX) & nsVDUI::kAlignTypeMask) == nsVDUI::kFill) {
			int span = (spill + fillleft - 1) / fillleft;

			size += span;
			spill -= span;
			--fillleft;
		}


		if (mbVertical)
			pControl->PostLayout(vduirect(target.left, pos, target.right, pos+size));
		else
			pControl->PostLayout(vduirect(pos, target.top, pos+size, target.bottom));

		pos += size;
	}
}


///////////////////////////////////////////////////////////////////////////

class VDUIPageSet : public VDUIWindow, public IVDUIPageSet {
public:
	VDUIPageSet();
	~VDUIPageSet();

	void *AsInterface(uint32 id);

	void PreLayoutBase(const VDUILayoutSpecs&);
	void PostLayoutBase(const vduirect&);

	int GetValue();
	void SetValue(int value);

	void AddPage(int dialogID);
protected:
	int mCurrentPage;

	std::vector<int>	mPages;
};

IVDUIWindow *VDCreateUIPageSet() { return new VDUIPageSet; }

VDUIPageSet::VDUIPageSet()
	: mCurrentPage(-1)
{
}

VDUIPageSet::~VDUIPageSet() {
}

void *VDUIPageSet::AsInterface(uint32 id) {
	switch(id) {
	case IVDUIPageSet::kTypeID:	return static_cast<IVDUIPageSet *>(this);
	}

	return VDUIWindow::AsInterface(id);
}

void VDUIPageSet::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	if (!mChildren.empty()) {
		IVDUIWindow *pWin = mChildren.front();

		pWin->PreLayout(parentConstraints);
		mLayoutSpecs = pWin->GetLayoutSpecs();
	}
}

void VDUIPageSet::PostLayoutBase(const vduirect& target) {
	VDUIWindow::PostLayoutBase(target);

	if (!mChildren.empty()) {
		IVDUIWindow *pWin = mChildren.front();

		pWin->PostLayout(target);
	}
}

int VDUIPageSet::GetValue() {
	return mCurrentPage;
}

void VDUIPageSet::SetValue(int value) {
	if (mCurrentPage == value)
		return;

	mCurrentPage = value;

	// destroy children
	while(!mChildren.empty()) {
		IVDUIWindow *pChild = mChildren.back();
		mChildren.pop_back();

		pChild->Shutdown();
		pChild->Release();
	}

	// create new dialog
	if ((unsigned)value < mPages.size()) {
		IVDUIWindow *pChildWin = VDCreateDialogFromResource(mPages[value], this);

		if (pChildWin)
			pChildWin->GetBase()->ExecuteAllLinks();
	}

	mpBase->Relayout();

	// send out value change notification
	mpBase->DispatchEvent(this, mID, IVDUICallback::kEventSelect, mCurrentPage);
}

void VDUIPageSet::AddPage(int dialogID) {
	mPages.push_back(dialogID);
}

