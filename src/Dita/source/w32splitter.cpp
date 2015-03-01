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

class VDUISplitterW32 : public VDUICustomControlW32 {
public:
	VDUISplitterW32();

	bool Create(IVDUIParameters *pParams);

	void PreLayoutBase(const VDUILayoutSpecs& constraints);
	void PostLayoutBase(const vduirect& r);

protected:
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void OnPaint();
	void OnLButtonDown(WPARAM wParam, int x, int y);
	void OnLButtonUp(WPARAM wParam, int x, int y);
	void OnMouseMove(WPARAM wParam, int x, int y);
	void OnCaptureChanged(HWND hwndNewCapture);

	void DrawMovingSplitter();
	void ConvertLocationToFraction();
	void ConvertFractionToLocation();

	float	mFraction;
	RECT	mSplitter;
	int		mDragOffset;
	bool	mbIsVertical;
};

extern IVDUIWindow *VDCreateUISplitter() { return new VDUISplitterW32; }

VDUISplitterW32::VDUISplitterW32()
	: mFraction(0.5f)
	, mbIsVertical(true)
{
}

bool VDUISplitterW32::Create(IVDUIParameters *pParams) {
	mbIsVertical = pParams->GetB(nsVDUI::kUIParam_IsVertical, false);
	return VDUICustomControlW32::Create(pParams);
}

void VDUISplitterW32::PreLayoutBase(const VDUILayoutSpecs& constraints) {
	tChildren::iterator it(mChildren.begin()), itEnd(mChildren.end());

	for(; it != itEnd; ++it) {
		IVDUIWindow *pWin = *it;

		pWin->PreLayout(constraints);

		mLayoutSpecs.minsize.include(pWin->GetLayoutSpecs().minsize);
	}
}

void VDUISplitterW32::PostLayoutBase(const vduirect& r) {
	SetArea(r);
	ConvertFractionToLocation();
	InvalidateRect(mhwnd, NULL, TRUE);

	const vduirect rClient(GetClientArea());
	tChildren::iterator it(mChildren.begin()), itEnd(mChildren.end());
	IVDUIWindow *pWin;

	if (mbIsVertical) {
		if (it != itEnd) {
			pWin = *it;
			++it;

			pWin->Layout(vduirect(0, 0, mSplitter.left, rClient.bottom));
		}
		if (it != itEnd) {
			pWin = *it;
			++it;

			pWin->Layout(vduirect(mSplitter.right, 0, rClient.right, rClient.bottom));
		}
	} else {
		if (it != itEnd) {
			pWin = *it;
			++it;

			pWin->Layout(vduirect(0, 0, rClient.right, mSplitter.top));
		}
		if (it != itEnd) {
			pWin = *it;
			++it;

			pWin->Layout(vduirect(0, mSplitter.bottom, rClient.right, rClient.bottom));
		}
	}
}

LRESULT VDUISplitterW32::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_PAINT:
		OnPaint();
		break;

	case WM_LBUTTONDOWN:
		OnLButtonDown(wParam, (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam));
		return 0;

	case WM_LBUTTONUP:
		OnLButtonUp(wParam, (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam));
		return 0;

	case WM_MOUSEMOVE:
		OnMouseMove(wParam, (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam));
		return 0;

	case WM_CAPTURECHANGED:
		OnCaptureChanged((HWND)lParam);
		return 0;

	case WM_SETCURSOR:
		if ((HWND)wParam == mhwnd && LOWORD(lParam) == HTCLIENT) {
			POINT pt;
			GetCursorPos(&pt);
			ScreenToClient(mhwnd, &pt);
			SetCursor(LoadCursor(NULL, PtInRect(&mSplitter, pt) ? mbIsVertical ? IDC_SIZEWE : IDC_SIZENS : IDC_ARROW));
			return TRUE;
		}
		break;
	}
	return VDUICustomControlW32::WndProc(msg, wParam, lParam);
}

void VDUISplitterW32::OnPaint() {
	PAINTSTRUCT ps;

	if (HDC hdc = BeginPaint(mhwnd, &ps)) {
		if (mbIsVertical)
			DrawEdge(hdc, &mSplitter, EDGE_RAISED, BF_LEFT|BF_RIGHT);
		else
			DrawEdge(hdc, &mSplitter, EDGE_RAISED, BF_TOP|BF_BOTTOM);

		EndPaint(mhwnd, &ps);
	}
}

void VDUISplitterW32::OnLButtonDown(WPARAM wParam, int x, int y) {
	POINT pt={x,y};

	if (PtInRect(&mSplitter, pt)) {
		if (mbIsVertical)
			mDragOffset = mSplitter.left - x;
		else
			mDragOffset = mSplitter.top - y;
		SetCapture(mhwnd);
		LockWindowUpdate(mhwnd);
		DrawMovingSplitter();		
	}
}

void VDUISplitterW32::OnLButtonUp(WPARAM wParam, int x, int y) {
	if (GetCapture() == mhwnd)
		ReleaseCapture();
}

void VDUISplitterW32::OnMouseMove(WPARAM wParam, int x, int y) {
	if (GetCapture() == mhwnd) {
		RECT rClient;

		DrawMovingSplitter();
		GetClientRect(mhwnd, &rClient);
		if (mbIsVertical)
			OffsetRect(&mSplitter, std::min<int>(rClient.right - GetSystemMetrics(SM_CXEDGE)*3, std::max<int>(0, mDragOffset + x)) - mSplitter.left, 0);
		else
			OffsetRect(&mSplitter, 0, std::min<int>(rClient.bottom - GetSystemMetrics(SM_CYEDGE)*3, std::max<int>(0, mDragOffset + y)) - mSplitter.top);
		DrawMovingSplitter();
	}
}

void VDUISplitterW32::OnCaptureChanged(HWND hwndNewCapture) {
	DrawMovingSplitter();
	LockWindowUpdate(NULL);
	ConvertLocationToFraction();
	PostLayoutBase(mArea);
}

void VDUISplitterW32::DrawMovingSplitter() {
	if (HDC hdc = GetDCEx(mhwnd, NULL, DCX_LOCKWINDOWUPDATE|DCX_CACHE)) {
		InvertRect(hdc, &mSplitter);
		ReleaseDC(mhwnd, hdc);
	}
}

void VDUISplitterW32::ConvertLocationToFraction() {
	const vduirect rClient(GetClientArea());

	if (mbIsVertical) {
		const int w = rClient.width();
		int sw = GetSystemMetrics(SM_CXEDGE) * 3;

		if (sw > w)
			sw = w;

		if (sw >= w)
			mFraction = 0;
		else
			mFraction = (float)mSplitter.left / (float)(w-sw);
	} else {
		const int h = rClient.height();
		int sh = GetSystemMetrics(SM_CYEDGE) * 3;

		if (sh > h)
			sh = h;

		if (sh >= h)
			mFraction = 0;
		else
			mFraction = (float)mSplitter.top / (float)(h-sh);
	}

	if (mFraction < 0)
		mFraction = 0;
	else if (mFraction > 1)
		mFraction = 1;
}

void VDUISplitterW32::ConvertFractionToLocation() {
	const vduirect rClient(GetClientArea());

	if (mbIsVertical) {
		const int w = rClient.width();
		int sw = GetSystemMetrics(SM_CXEDGE) * 3;

		if (sw > w)
			sw = w;

		const int x = (int)(0.5 + (w-sw)*mFraction);

		mSplitter.left = x;
		mSplitter.right = x + sw;
		mSplitter.top = rClient.top;
		mSplitter.bottom = rClient.bottom;
	} else {
		const int h = rClient.height();
		int sh = GetSystemMetrics(SM_CYEDGE) * 3;

		if (sh > h)
			sh = h;

		const int y = (int)(0.5 + (h-sh)*mFraction);

		mSplitter.top = y;
		mSplitter.bottom = y + sh;
		mSplitter.left = rClient.left;
		mSplitter.right = rClient.right;
	}
}

///////////////////////////////////////////////////////////////////////////

class VDUISplitSetW32 : public VDUIWindow {
public:
	VDUISplitSetW32();

	bool Create(IVDUIParameters *pParams);

	void PreLayoutBase(const VDUILayoutSpecs& constraints);
	void PostLayoutBase(const vduirect& r);

protected:
	bool	mbIsVertical;
};

extern IVDUIWindow *VDCreateUISplitSet() { return new VDUISplitSetW32; }

VDUISplitSetW32::VDUISplitSetW32()
	: mbIsVertical(true)
{
}

bool VDUISplitSetW32::Create(IVDUIParameters *pParams) {
	mbIsVertical = pParams->GetB(nsVDUI::kUIParam_IsVertical, false);
	return VDUIWindow::Create(pParams);
}

void VDUISplitSetW32::PreLayoutBase(const VDUILayoutSpecs& constraints) {
	tChildren::iterator it(mChildren.begin()), itEnd(mChildren.end());

	for(; it != itEnd; ++it) {
		IVDUIWindow *pWin = *it;

		pWin->PreLayout(constraints);

		mLayoutSpecs.minsize.include(pWin->GetLayoutSpecs().minsize);
	}
}

void VDUISplitSetW32::PostLayoutBase(const vduirect& r) {
	SetArea(r);

	vduirect rClient(GetClientArea());
	tChildren::iterator itBegin(mChildren.begin()), itEnd(mChildren.end());
	IVDUIWindow *pWin;

	if (!mbIsVertical) {
		if (itEnd != itBegin) {
			while(--itEnd != itBegin) {
				pWin = *itEnd;

				const vduirect rChild(pWin->GetArea());
				int w = rChild.width();
				int h = rChild.height();

				rClient.right -= w;

				pWin->SetArea(vduirect(rClient.right, rClient.top, rClient.right + w, rClient.bottom));
			}

			if (rClient.right < rClient.left)
				rClient.right = rClient.left;

			pWin = *itEnd;
			pWin->SetArea(rClient);
		}
	} else {
		if (itEnd != itBegin) {
			while(--itEnd != itBegin) {
				pWin = *itEnd;

				const vduirect rChild(pWin->GetArea());
				int w = rChild.width();
				int h = rChild.height();

				int minh = pWin->GetLayoutSpecs().minsize.h;
				if (h < minh)
					h = minh;

				rClient.bottom -= h;

				pWin->PostLayout(vduirect(rClient.left, rClient.bottom, rClient.right, rClient.bottom + h));
			}

			if (rClient.bottom < rClient.top)
				rClient.bottom = rClient.top;

			pWin = *itEnd;
			pWin->SetArea(rClient);
		}
	}
}

///////////////////////////////////////////////////////////////////////////

class VDUISplitBarW32 : public VDUICustomControlW32 {
public:
	VDUISplitBarW32();

	bool Create(IVDUIParameters *pParams);

	void PreLayoutBase(const VDUILayoutSpecs& constraints);

protected:
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void OnPaint();
	void OnLButtonDown(WPARAM wParam, int x, int y);
	void OnLButtonUp(WPARAM wParam, int x, int y);
	void OnMouseMove(WPARAM wParam, int x, int y);

	int		mDragOffset;
	int		mSplitterSize;
	bool	mbIsVertical;
};

extern IVDUIWindow *VDCreateUISplitBar() { return new VDUISplitBarW32; }

VDUISplitBarW32::VDUISplitBarW32()
	: mbIsVertical(true)
{
}

bool VDUISplitBarW32::Create(IVDUIParameters *pParams) {
	mbIsVertical = pParams->GetB(nsVDUI::kUIParam_IsVertical, false);
	mSplitterSize = GetSystemMetrics(mbIsVertical ? SM_CXEDGE : SM_CYEDGE)*3;
	return VDUICustomControlW32::Create(pParams);
}

void VDUISplitBarW32::PreLayoutBase(const VDUILayoutSpecs& constraints) {
	if (mbIsVertical)
		mLayoutSpecs.minsize.include(vduisize(mSplitterSize, 0));
	else
		mLayoutSpecs.minsize.include(vduisize(0, mSplitterSize));
}

LRESULT VDUISplitBarW32::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_PAINT:
		OnPaint();
		break;

	case WM_LBUTTONDOWN:
		OnLButtonDown(wParam, (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam));
		return 0;

	case WM_LBUTTONUP:
		OnLButtonUp(wParam, (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam));
		return 0;

	case WM_MOUSEMOVE:
		OnMouseMove(wParam, (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam));
		return 0;

	case WM_SETCURSOR:
		if ((HWND)wParam == mhwnd && LOWORD(lParam) == HTCLIENT) {
			SetCursor(LoadCursor(NULL, mbIsVertical ? IDC_SIZEWE : IDC_SIZENS));
			return TRUE;
		}
		break;
	}
	return VDUICustomControlW32::WndProc(msg, wParam, lParam);
}

void VDUISplitBarW32::OnPaint() {
	PAINTSTRUCT ps;

	if (HDC hdc = BeginPaint(mhwnd, &ps)) {
		RECT r;
		GetClientRect(mhwnd, &r);
		if (mbIsVertical)
			DrawEdge(hdc, &r, EDGE_RAISED, BF_LEFT|BF_RIGHT);
		else
			DrawEdge(hdc, &r, EDGE_RAISED, BF_TOP|BF_BOTTOM);

		EndPaint(mhwnd, &ps);
	}
}

void VDUISplitBarW32::OnLButtonDown(WPARAM wParam, int x, int y) {
	POINT pt={x,y};

	if (mbIsVertical)
		mDragOffset = x;
	else
		mDragOffset = y;
	SetCapture(mhwnd);
}

void VDUISplitBarW32::OnLButtonUp(WPARAM wParam, int x, int y) {
	if (GetCapture() == mhwnd)
		ReleaseCapture();
}

void VDUISplitBarW32::OnMouseMove(WPARAM wParam, int x, int y) {
	if (GetCapture() == mhwnd) {
		RECT rClient;

		GetClientRect(mhwnd, &rClient);

		IVDUIWindow *pParent = GetParent();
		if (pParent) {
			IVDUIWindow *pSibling = pParent->GetNextChild(this);
			vduirect r(pSibling->GetArea());
			if (mbIsVertical) {
				int delta = x - mDragOffset;
				if (!delta)
					return;
				r.right += delta;
			} else {
				int delta = y - mDragOffset;
				if (!delta)
					return;
				r.top += delta;
			}
			pSibling->SetArea(r);
			pParent->PostLayout(pParent->GetArea());
		}
	}
}
