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
#include <windows.h>

void VDUIParameters::Clear() {
	mParams.clear();
}

bool VDUIParameters::GetB(uint32 id, bool defaultVal) {
	if (const Variant *var = Lookup(id))
		return var->b;
	else
		return defaultVal;
}

int VDUIParameters::GetI(uint32 id, int defaultVal) {
	if (const Variant *var = Lookup(id))
		return var->i;
	else
		return defaultVal;
}

float VDUIParameters::GetF(uint32 id, float defaultVal) {
	if (const Variant *var = Lookup(id))
		return var->f;
	else
		return defaultVal;
}

bool VDUIParameters::GetOptB(uint32 id, bool& v) {
	if (const Variant *var = Lookup(id)) {
		v = var->b;
		return true;
	}
	return false;
}

bool VDUIParameters::GetOptI(uint32 id, int& v) {
	if (const Variant *var = Lookup(id)) {
		v = var->i;
		return true;
	}
	return false;
}

bool VDUIParameters::GetOptF(uint32 id, float& v) {
	if (const Variant *var = Lookup(id)) {
		v = var->f;
		return true;
	}
	return false;
}

const VDUIParameters::Variant *VDUIParameters::Lookup(uint32 id) const {
	tParams::const_iterator it(mParams.find(id));

	return it != mParams.end() ? &(*it).second : NULL;
}

/////////////////////////////////////////////////////////////////////////////

namespace {
	vduisize VDUIGetUnitFactors() {
		vduisize f(4,4);

		HDC hdc = GetDC(NULL);
		HGDIOBJ hOldFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
		TEXTMETRIC tm = {sizeof(TEXTMETRIC)};

		if (GetTextMetrics(hdc, &tm)) {
			// the WINE guys figured this out, not me
			SIZE s;
			GetTextExtentPoint32(hdc, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", 52, &s);
//			f.w = tm.tmAveCharWidth << 2;
			f.w = s.cx/13;
			f.h = tm.tmHeight << 1;
		}

		SelectObject(hdc, hOldFont);
		ReleaseDC(NULL, hdc);

		return f;
	}

	vduisize VDUIMapUnitsToPixels(const vduisize& s) {
		static const vduisize factors = VDUIGetUnitFactors();

		return vduisize((s.w*factors.w+8) >> 4, (s.h*factors.h+8) >> 4);
	}
}

/////////////////////////////////////////////////////////////////////////////

VDUIWindow::VDUIWindow()
	: mpParent(NULL)
	, mpBase(NULL)
	, mID(0)
	, mArea(0, 0, 0, 0)
	, mMinSize(0,0)
	, mMaxSize(0,0)
	, mMargins(0)
	, mPadding(0)
	, mAlignX(nsVDUI::kLeft)
	, mAlignY(nsVDUI::kTop)
	, mDesiredAspectRatio(1.0f)
	, mbVisible(true)
	, mbEnabled(true)
	, mRefCount(0)
{
	mLayoutSpecs.minsize.w = 0;
	mLayoutSpecs.minsize.h = 0;
}

VDUIWindow::~VDUIWindow() {
	Shutdown();
}

IVDUIWindow *VDCreateUIWindow() {
	return new VDUIWindow;
}

int VDUIWindow::AddRef() {
	return ++mRefCount;
}

int VDUIWindow::Release() {
	int rc = --mRefCount;
	VDASSERT(rc >= 0);

	if (!rc)
		delete this;

	return rc;
}

void *VDUIWindow::AsInterface(uint32 id) {
	switch(id) {
	case IVDUIWindow::kTypeID:	return static_cast<IVDUIWindow *>(this);
	default:					return NULL;
	}
}

void VDUIWindow::Shutdown() {
	while(!mChildren.empty()) {
		IVDUIWindow *pChild = mChildren.back();
		mChildren.pop_back();

		pChild->Shutdown();
		pChild->Release();
	}

	Destroy();
}

void VDUIWindow::SetParent(IVDUIWindow *pParent) {
	if (mpParent != pParent) {
		Destroy();

		if (mpBase)
			mpBase->RemoveControl(this);

		mpParent = pParent;
		mpBase = NULL;

		if (mpParent) {
			mpBase = vdpoly_cast<IVDUIBase *>(mpParent);

			if (!mpBase)
				mpBase = mpParent->GetBase();

			if (mpBase)
				mpBase->AddControl(this);
		}
	}
}

bool VDUIWindow::Create(IVDUIParameters *pParms) {
	using namespace nsVDUI;

	int i;

	if (pParms->GetOptI(kUIParam_MarginL, i))	mMargins.left	= VDUIMapUnitsToPixels(vduisize(i,i)).w;
	if (pParms->GetOptI(kUIParam_MarginT, i))	mMargins.top	= VDUIMapUnitsToPixels(vduisize(i,i)).h;
	if (pParms->GetOptI(kUIParam_MarginR, i))	mMargins.right	= VDUIMapUnitsToPixels(vduisize(i,i)).w;
	if (pParms->GetOptI(kUIParam_MarginB, i))	mMargins.bottom	= VDUIMapUnitsToPixels(vduisize(i,i)).h;
	if (pParms->GetOptI(kUIParam_PadL, i))	mPadding.left	= VDUIMapUnitsToPixels(vduisize(i,i)).w;
	if (pParms->GetOptI(kUIParam_PadT, i))	mPadding.top	= VDUIMapUnitsToPixels(vduisize(i,i)).h;
	if (pParms->GetOptI(kUIParam_PadR, i))	mPadding.right	= VDUIMapUnitsToPixels(vduisize(i,i)).w;
	if (pParms->GetOptI(kUIParam_PadB, i))	mPadding.bottom	= VDUIMapUnitsToPixels(vduisize(i,i)).h;

	if (pParms->GetOptI(kUIParam_MinW, i))	mMinSize.w	= VDUIMapUnitsToPixels(vduisize(i,i)).w;
	if (pParms->GetOptI(kUIParam_MinH, i))	mMinSize.h	= VDUIMapUnitsToPixels(vduisize(i,i)).h;
	if (pParms->GetOptI(kUIParam_MaxW, i))	mMaxSize.w	= VDUIMapUnitsToPixels(vduisize(i,i)).w;
	if (pParms->GetOptI(kUIParam_MaxH, i))	mMaxSize.h	= VDUIMapUnitsToPixels(vduisize(i,i)).h;

	mAlignX = (Alignment)pParms->GetI(kUIParam_Align, mAlignX);
	mAlignY = (Alignment)pParms->GetI(kUIParam_VAlign, mAlignY);

	mDesiredAspectRatio = pParms->GetF(kUIParam_Aspect, mDesiredAspectRatio);
	return true;
}

void VDUIWindow::Destroy() {
}

void VDUIWindow::AddChild(IVDUIWindow *pWindow) {
	tChildren::iterator it(std::find(mChildren.begin(), mChildren.end(), pWindow));

	if (it == mChildren.end()) {
		mChildren.push_back(pWindow);
		pWindow->AddRef();
		pWindow->SetParent(this);
	}
}

void VDUIWindow::RemoveChild(IVDUIWindow *pWindow) {
	tChildren::iterator it(std::find(mChildren.begin(), mChildren.end(), pWindow));

	if (it != mChildren.end()) {
		mChildren.erase(it);
		pWindow->SetParent(NULL);
		pWindow->Release();
	}
}

IVDUIWindow *VDUIWindow::GetStartingChild() {
	return mChildren.empty() ? NULL : mChildren.front();
}

IVDUIWindow *VDUIWindow::GetPreviousChild(IVDUIWindow *pWindow) {
	tChildren::iterator it(std::find(mChildren.begin(), mChildren.end(), pWindow));

	if (it == mChildren.end())
		return NULL;

	if (it == mChildren.begin())
		return NULL;

	return *--it;
}

IVDUIWindow *VDUIWindow::GetNextChild(IVDUIWindow *pWindow) {
	tChildren::iterator it(std::find(mChildren.begin(), mChildren.end(), pWindow));

	if (it == mChildren.end())
		return NULL;

	if (++it == mChildren.end())
		return NULL;

	return *it;
}

bool VDUIWindow::IsActuallyVisible() {
	IVDUIWindow *pWin = this;

	for(; pWin; pWin = pWin->GetParent())
		if (!pWin->IsVisible())
			return false;

	return true;
}

bool VDUIWindow::IsActuallyEnabled() {
	IVDUIWindow *pWin = this;

	for(; pWin; pWin = pWin->GetParent())
		if (!pWin->IsEnabled())
			return false;

	return true;
}

void VDUIWindow::PropagateVisible(bool vis) {
	tChildren::iterator it(mChildren.begin()), itEnd(mChildren.end());
	vis &= mbVisible;

	for(; it!=itEnd; ++it){
		IVDUIWindow *pWin = *it;

		pWin->PropagateVisible(vis);
	}
}

void VDUIWindow::PropagateEnabled(bool ena) {
	tChildren::iterator it(mChildren.begin()), itEnd(mChildren.end());
	ena &= mbEnabled;

	for(; it!=itEnd; ++it){
		IVDUIWindow *pWin = *it;

		pWin->PropagateEnabled(ena);
	}
}

void VDUIWindow::SetFocus() {
}

uint32 VDUIWindow::GetID() const {
	return mID;
}

void VDUIWindow::SetID(uint32 id) {
	if (mpBase)
		mpBase->RemoveControl(this);
	mID = id;
	if (mpBase)
		mpBase->AddControl(this);
}

void VDUIWindow::Layout(const vduirect& r) {
	VDUILayoutSpecs specs;
	specs.minsize = r.size();
	PreLayout(specs);
	PostLayout(r);
}

void VDUIWindow::PreLayout(const VDUILayoutSpecs& parentConstraints) {
	VDUILayoutSpecs constraints(parentConstraints);
	VDUILayoutSpecs lastSpecs;

	PreLayoutAttempt(constraints);
	lastSpecs = mLayoutSpecs;

	if (mMaxSize.w) {
		VDDEBUG("Attempting to crunch control - %d -> %d [%S]\n", mLayoutSpecs.minsize.w, mMaxSize.w, mCaption.c_str());
		while(mLayoutSpecs.minsize.w > mMaxSize.w) {
			constraints.minsize.w = mLayoutSpecs.minsize.w - 1;
			PreLayoutAttempt(constraints);

			if (mLayoutSpecs.minsize.w >= lastSpecs.minsize.w) {
				VDDEBUG("  ...failed (crunched to %d x %d)\n", mLayoutSpecs.minsize.w, mLayoutSpecs.minsize.h);
				mLayoutSpecs = lastSpecs;
				break;
			}
			VDDEBUG("  ...crunched to %d x %d\n", mLayoutSpecs.minsize.w, mLayoutSpecs.minsize.h);

			lastSpecs = mLayoutSpecs;
		}
	} else if (mMaxSize.h) {
		while(mLayoutSpecs.minsize.h > mMaxSize.h) {
			constraints.minsize.h = mLayoutSpecs.minsize.h - 1;
			PreLayoutAttempt(constraints);

			if (mLayoutSpecs.minsize.h >= lastSpecs.minsize.h) {
				mLayoutSpecs = lastSpecs;
				break;
			}

			lastSpecs = mLayoutSpecs;
		}
	}
}

void VDUIWindow::PreLayoutAttempt(const VDUILayoutSpecs& parentConstraints) {
	mLayoutSpecs.minsize.w = 0;
	mLayoutSpecs.minsize.h = 0;

	VDUILayoutSpecs tempConstraints(parentConstraints);

	tempConstraints.minsize.w = std::max<int>(0, tempConstraints.minsize.w - (mMargins.width() + mPadding.width()));
	tempConstraints.minsize.h = std::max<int>(0, tempConstraints.minsize.h - (mMargins.height() + mPadding.height()));

	PreLayoutBase(tempConstraints);

	int& w = mLayoutSpecs.minsize.w;
	int& h = mLayoutSpecs.minsize.h;

	w += mPadding.width();
	h += mPadding.height();

	if (w < mMinSize.w)
		w = mMinSize.w;

	if (h < mMinSize.h)
		h = mMinSize.h;

	if (w && h && ((mAlignX|mAlignY) & nsVDUI::kExpandFlag)) {
		float rCurrentAR = (float)w / (float)h;

		if (rCurrentAR > mDesiredAspectRatio) {			// wider/shorter than desired
			if (mAlignY & nsVDUI::kExpandFlag) {
				h = (int)ceil(w / mDesiredAspectRatio);
			}
		} else if (rCurrentAR < mDesiredAspectRatio) {	// narrower/taller than desired
			if (mAlignX & nsVDUI::kExpandFlag) {
				w = (int)ceil(h * mDesiredAspectRatio);
			}
		}
	}

	w += mMargins.width();
	h += mMargins.height();

#if 0
	int level = 0;
	for(IVDUIWindow *pWin = GetParent(); pWin; pWin = pWin->GetParent())
		++level;

	VDDEBUG("%*sid=%08x  constraints: %3ux%3u  minsize: %3ux%3u  caption: %S\n", level, "", GetID(), parentConstraints.minsize.w, parentConstraints.minsize.h, w, h, GetCaption().c_str());
#endif
}

void VDUIWindow::PostLayout(const vduirect& target) {
	vduirect				r(target);
	int						alignx = mAlignX, aligny = mAlignY;
	const VDUILayoutSpecs&	specs = GetLayoutSpecs();

	alignx &= nsVDUI::kAlignTypeMask;
	aligny &= nsVDUI::kAlignTypeMask;

	if (alignx != nsVDUI::kFill) {
		int padX = ((target.width() - specs.minsize.w) * ((int)alignx - 1) + 1) >> 1;

		if (padX < 0)
			padX = 0;

		r.left += padX;
		r.right = r.left + specs.minsize.w;
	}

	if (aligny != nsVDUI::kFill) {
		int padY = ((target.height() - specs.minsize.h) * ((int)aligny - 1) + 1) >> 1;

		if (padY < 0)
			padY = 0;

		r.top += padY;
		r.bottom = r.top + specs.minsize.h;
	}

	r.left += mMargins.left;
	r.top += mMargins.top;
	r.right -= mMargins.right;
	r.bottom -= mMargins.bottom;

	PostLayoutBase(r);
}

int VDUIWindow::GetValue() {
	return 0;
}

void VDUIWindow::SetValue(int) {
}
