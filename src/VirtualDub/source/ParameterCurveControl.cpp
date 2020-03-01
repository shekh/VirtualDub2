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

#include "stdafx.h"

#include <windows.h>
#include <commctrl.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/w32assist.h>
#include <vd2/system/refcount.h>
#include <vd2/VDLib/ParameterCurve.h>
#include "ParameterCurveControl.h"

#include "resource.h"
#include "oshelper.h"

extern HINSTANCE g_hInst;

////////////////////////////

extern const char g_VDParameterCurveControlClass[]="phaeronParameterCurveControl";

///////////////////////////////////////////////////////////////////////////

namespace {
	double distsq(double x, double y) {
		return x*x+y*y;
	}
}

///////////////////////////////////////////////////////////////////////////

struct VDParameterCurveControlW32 : public vdrefcounted<IVDUIParameterCurveControl> {
	friend bool VDRegisterParameterCurveControl();

protected:
	VDParameterCurveControlW32(HWND hwnd);
	~VDParameterCurveControlW32();

	void *AsInterface(uint32 id);

public:
	VDParameterCurve *GetCurve();
	void SetCurve(VDParameterCurve *curve);

	void SetPosition(VDPosition pos);
	void SetSelectedPoint(int x);
	void SetSelectedPoint(VDParameterCurve::PointList::iterator it);
	int GetSelectedPoint() {
		if (!mpCurve) return -1;
		if (mSelectedPoint==mpCurve->Points().end()) return -1;
		return mSelectedPoint-mpCurve->Points().begin();
	}
	void DeletePoint(int x);
	void SetValue(int x, double v);

	VDEvent<IVDUIParameterCurveControl, int>& CurveUpdatedEvent() { return mCurveUpdatedEvent; }
	VDEvent<IVDUIParameterCurveControl, Status>& StatusUpdatedEvent() { return mStatusUpdatedEvent; }

protected:
	static LRESULT APIENTRY StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void OnCreate();
	void OnSize();
	void OnLButtonDown(int x, int y, uint32 modifiers);
	void OnLButtonUp(int x, int y);
	void OnRButtonDown(int x, int y, uint32 modifiers);
	void OnMouseMove(int x, int y);
	void OnMouseLeave();
	void OnPaint();
	void OnSetCursor(HWND hwnd, UINT htCode, UINT msg);
	void Notify(UINT code);

	enum Part {
		kPartNone,
		kPartPoint,
	};

	struct DragObject {
		VDParameterCurve::PointList::iterator mPoint;
		Part		mPart;
	};

	double		ScreenToCurveX(int x);
	double		ScreenToCurveY(int y);
	int			CurveToScreenX(double x);
	int			CurveToScreenY(double y);
	DragObject	ScreenToObject(int x, int y);
	DragObject	CurveToObject(double x, double y);
	void		ObjectToCurve(const DragObject& obj, double& x, double& y);

	void		SetStatus(Status s);

	void		InvalidateAtPoint(VDParameterCurve::PointList::iterator it);
	void		InvalidateAroundPoint(VDParameterCurve::PointList::iterator it);
	void		InvalidateRange(VDParameterCurve::PointList::iterator it);

	HWND				mhwnd;
	vdrefptr<VDParameterCurve>	mpCurve;
	int					mWidth;
	int					mHeight;
	double				mPosX;
	double				mPosY;
	double				mScaleX;
	double				mScaleY;

	Part				mDragPart;
	double				mDragOffsetX;
	double				mDragOffsetY;

	bool				mbTrackingMouse;
	Status				mStatus;

	HFONT				mhfont;
	int					mFontHeight;

	HPEN				mhpenGridLines;
	HPEN				mhpenGridLines2;
	HPEN				mhpenCurve;
	HPEN				mhpenLine;
	HPEN				mhpenEndLine;
	HPEN				mhpenCurrentPos;
	HCURSOR				mhcurMove;
	HCURSOR				mhcurAdd;
	HCURSOR				mhcurRemove;
	HCURSOR				mhcurModify;

	VDParameterCurve::PointList::iterator	mSelectedPoint;
	VDParameterCurve::PointList::iterator	mHighlightedPoint;
	Part				mHighlightedPart;

	VDEvent<IVDUIParameterCurveControl, int>	mCurveUpdatedEvent;
	VDEvent<IVDUIParameterCurveControl, Status>	mStatusUpdatedEvent;
};

bool VDRegisterParameterCurveControl() {
	WNDCLASS wc;

	wc.style		= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc	= VDParameterCurveControlW32::StaticWndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= sizeof(IVDUnknown *);
	wc.hInstance	= g_hInst;
	wc.hIcon		= NULL;
	wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground= NULL;
	wc.lpszMenuName	= NULL;
	wc.lpszClassName= g_VDParameterCurveControlClass;

	return !!RegisterClass(&wc);
}

IVDUIParameterCurveControl *VDGetIUIParameterCurveControl(VDGUIHandle h) {
	return vdpoly_cast<IVDUIParameterCurveControl *>((IVDUnknown *)GetWindowLongPtr((HWND)h, 0));
}

///////////////////////////////////////////////////////////////////////////

VDParameterCurveControlW32::VDParameterCurveControlW32(HWND hwnd)
	: mhwnd(hwnd)
	, mpCurve(NULL)
	, mWidth(1)
	, mHeight(1)
	, mDragPart(kPartNone)
	, mbTrackingMouse(false)
	, mStatus(kStatus_Nothing)
	, mhpenGridLines(CreatePen(PS_SOLID, 0, RGB(64, 84, 64)))
	, mhpenGridLines2(CreatePen(PS_SOLID, 0, RGB(8, 58, 8)))
	, mhpenCurve(CreatePen(PS_SOLID, 0, RGB(255,255,255)))
	, mhpenLine(CreatePen(PS_SOLID, 0, RGB(128,255,192)))
	, mhpenEndLine(CreatePen(PS_SOLID, 0, RGB(224,224,128)))
	, mhpenCurrentPos(CreatePen(PS_SOLID, 0, RGB(100, 100, 255)))
	, mhcurMove(LoadCursor(g_hInst, MAKEINTRESOURCE(IDC_DRAGPOINT)))
	, mhcurAdd(LoadCursor(g_hInst, MAKEINTRESOURCE(IDC_ADDPOINT)))
	, mhcurRemove(LoadCursor(g_hInst, MAKEINTRESOURCE(IDC_REMOVEPOINT)))
	, mhcurModify(LoadCursor(g_hInst, MAKEINTRESOURCE(IDC_MODIFYPOINT)))
{
	mPosX = 0.0;
	mPosY = 0.5;
	mScaleX = 10.0;
	mScaleY = 50.0;

	mhfont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	mFontHeight = 16;
	if (HDC hdc = GetDC(hwnd)) {
		TEXTMETRIC tm;
		if (SelectObject(hdc, mhfont) && GetTextMetrics(hdc, &tm)) {
			mFontHeight = tm.tmHeight;
		}
		ReleaseDC(hwnd, hdc);
	}
}

VDParameterCurveControlW32::~VDParameterCurveControlW32() {
	// Shared cursors do not need to be deleted.

	if (mhpenCurve)
		DeleteObject(mhpenCurve);
	if (mhpenLine)
		DeleteObject(mhpenLine);
	if (mhpenEndLine)
		DeleteObject(mhpenEndLine);
	if (mhpenGridLines2)
		DeleteObject(mhpenGridLines2);
	if (mhpenGridLines)
		DeleteObject(mhpenGridLines);
	if (mhpenCurrentPos)
		DeleteObject(mhpenCurrentPos);
}

void *VDParameterCurveControlW32::AsInterface(uint32 id) {
	if (id == IVDUIParameterCurveControl::kTypeID)
		return static_cast<IVDUIParameterCurveControl *>(this);

	return NULL;
}

///////////////////////////////////////////////////////////////////////////

VDParameterCurve *VDParameterCurveControlW32::GetCurve() {
	return mpCurve;
}

void VDParameterCurveControlW32::SetCurve(VDParameterCurve *curve) {
	mpCurve = curve;
	if (mpCurve) {
		mSelectedPoint = mpCurve->End();
		mHighlightedPoint = mpCurve->End();
		mHighlightedPart = kPartNone;
	}
	InvalidateRect(mhwnd, NULL, TRUE);
	mCurveUpdatedEvent.Raise(this, 1);
}

void VDParameterCurveControlW32::SetPosition(VDPosition pos) {
	int a0 = CurveToScreenX(mPosX);
	int a1 = CurveToScreenX(mPosX+1);
	int b0 = CurveToScreenX((double)pos);
	int b1 = CurveToScreenX((double)pos+1);
	mPosX = (double)pos;

	// redo old focus
	RECT r2 = {a0-8,0,a1+8,mHeight};
	InvalidateRect(mhwnd,&r2,false);

	ScrollWindow(mhwnd,a0-b0,0,0,0);

	// redo new focus
	RECT r3 = {b0-8,0,b1+8,mHeight};
	InvalidateRect(mhwnd,&r3,false);
}

///////////////////////////////////////////////////////////////////////////

LRESULT APIENTRY VDParameterCurveControlW32::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDParameterCurveControlW32 *pcd = static_cast<VDParameterCurveControlW32 *>((IVDUnknown *)GetWindowLongPtr(hwnd, 0));

	switch(msg) {
	case WM_NCCREATE:
		if (!(pcd = new VDParameterCurveControlW32(hwnd)))
			return FALSE;

		pcd->AddRef();
		SetWindowLongPtr(hwnd, 0, (LONG_PTR)static_cast<IVDUnknown *>(pcd));
		break;
	case WM_NCDESTROY:
		pcd->mhwnd = NULL;
		pcd->Release();
		SetWindowLongPtr(hwnd, 0, 0);
		pcd = NULL;
		break;
	}

	return pcd ? pcd->WndProc(msg, wParam, lParam) : DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK VDParameterCurveControlW32::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_CREATE:
		OnCreate();
		// fall through
	case WM_SIZE:
		OnSize();
		break;

	case WM_PAINT:
		OnPaint();
		return 0;

	case WM_LBUTTONDOWN:
		OnLButtonDown((SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam), wParam);
		break;

	case WM_LBUTTONUP:
		OnLButtonUp((SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam));
		break;

	case WM_RBUTTONDOWN:
		OnRButtonDown((SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam), wParam);
		break;

	case WM_MOUSEMOVE:
		OnMouseMove((SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam));
		break;

	case WM_MOUSELEAVE:
		OnMouseLeave();
		break;

	case WM_SETCURSOR:
		OnSetCursor((HWND)wParam, LOWORD(lParam), HIWORD(lParam));
		return 0;
	}

	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

void VDParameterCurveControlW32::OnCreate() {
}

void VDParameterCurveControlW32::OnSize() {
	RECT r;
	if (!GetClientRect(mhwnd, &r))
		return;

	mWidth = r.right;
	mHeight = r.bottom;

	mScaleY = mHeight - 2*(4 + mFontHeight);
}

void VDParameterCurveControlW32::OnLButtonDown(int x, int y, uint32 modifiers) {
	if (!mpCurve)
		return;

	double cx = ScreenToCurveX(x);
	double cy = ScreenToCurveY(y);
	DragObject dobj = CurveToObject(cx, cy);

	if (mHighlightedPart && (dobj.mPart != mHighlightedPart || dobj.mPoint != mHighlightedPoint))
		InvalidateAtPoint(mHighlightedPoint);

	mHighlightedPoint = mpCurve->End();
	mHighlightedPart = kPartNone;

	if (modifiers & MK_SHIFT) {
		VDParameterCurve::PointList& pts = mpCurve->Points();
		VDParameterCurve::PointList::iterator it(mpCurve->LowerBound(cx));

		VDParameterCurvePoint pt((*mpCurve)(cx));

		if (pts.empty() || cx < pts.front().mX || cx > pts.back().mY) {
			cy = std::max<double>(cy, mpCurve->GetYMin());
			cy = std::min<double>(cy, mpCurve->GetYMax());

			pt.mX = cx;
			pt.mY = cy;
		}

		// check if point is too close to another point
		if (it != mpCurve->End()) {
			if (fabs(it->mX - cx) < 0.01f)
				return;
		}

		if (it != mpCurve->Begin()) {
			VDParameterCurve::PointList::iterator it2(it);
			--it2;

			if (fabs(it2->mX - cx) < 0.01f)
				return;
		}

		InvalidateAtPoint(mSelectedPoint);
		VDParameterCurve::PointList::iterator	p1 = mpCurve->Points().insert(it, pt);
		mSelectedPoint = mpCurve->Points().end();
		SetSelectedPoint(p1);
		InvalidateAroundPoint(mSelectedPoint);
	} else if (mDragPart != dobj.mPart || mSelectedPoint != dobj.mPoint) {
		mDragPart = dobj.mPart;
		SetSelectedPoint(dobj.mPoint);

		if (mDragPart)
			InvalidateAtPoint(mSelectedPoint);

		if (mDragPart) {
			if (modifiers & MK_CONTROL) {
				InvalidateAroundPoint(mSelectedPoint);
				InvalidateAtPoint(mSelectedPoint);
				mSelectedPoint = mpCurve->Points().end();
				mpCurve->Points().erase(mSelectedPoint);
				SetSelectedPoint(mpCurve->End());
				mDragPart = kPartNone;
				mCurveUpdatedEvent.Raise(this, 0);
			} else {
				double dcx, dcy;
				ObjectToCurve(dobj, dcx, dcy);

				mDragOffsetX = dcx - cx;
				mDragOffsetY = dcy - cy;

				SetCapture(mhwnd);
				SetStatus(kStatus_PointDrag);
			}
		}
	}
}

void VDParameterCurveControlW32::OnLButtonUp(int x, int y) {
	if (mDragPart) {
		ReleaseCapture();
		mDragPart = kPartNone;

		mCurveUpdatedEvent.Raise(this, 0);
		SetStatus(kStatus_Focused);
	}
}

void VDParameterCurveControlW32::OnRButtonDown(int x, int y, uint32 modifiers) {
	if (!mpCurve)
		return;

	double cx = ScreenToCurveX(x);

	if (modifiers & MK_SHIFT) {
		VDParameterCurve::PointList::iterator it(mpCurve->UpperBound(cx));

		if (it != mpCurve->Begin()) {
			--it;

			VDParameterCurvePoint& selPt = *it;

			selPt.mbLinear = !selPt.mbLinear;
			InvalidateRange(it);
			mCurveUpdatedEvent.Raise(this, 0);
		}
	}
}

void VDParameterCurveControlW32::OnMouseMove(int x, int y) {
	if (!mpCurve)
		return;

	if (!mbTrackingMouse) {
		TRACKMOUSEEVENT tm;

		tm.cbSize		= sizeof(TRACKMOUSEEVENT);
		tm.dwFlags		= TME_LEAVE;
		tm.hwndTrack	= mhwnd;
		tm.dwHoverTime	= 0;

		mbTrackingMouse = !!TrackMouseEvent(&tm);
	}

	if (mDragPart) {
		double cx = ScreenToCurveX(x) + mDragOffsetX;
		double cy = ScreenToCurveY(y) + mDragOffsetY;

		// clamp position
		VDParameterCurve::PointList::iterator it1(mSelectedPoint);
		VDParameterCurve::PointList::iterator it2(mSelectedPoint);

		if (it1 != mpCurve->Begin()) {
			--it1;

			if (cx < it1->mX + 0.01f)
				cx = it1->mX + 0.01f;
		}

		VDASSERT(it2 != mpCurve->End());
		++it2;
		if (it2 != mpCurve->End()) {
			if (cx > it2->mX - 0.01f)
				cx = it2->mX - 0.01f;
		}

		cy = std::max<double>(cy, mpCurve->GetYMin());
		cy = std::min<double>(cy, mpCurve->GetYMax());

		if (cx != mSelectedPoint->mX || cy != mSelectedPoint->mY) {
			int xold = CurveToScreenX(mSelectedPoint->mX);
			int xnew = CurveToScreenX(cx);

			if (xold > xnew)
				std::swap(xold, xnew);

			RECT r = { xold-2, 0, xnew+3, mHeight };
			InvalidateRect(mhwnd, &r, TRUE);
			mSelectedPoint->mX = cx;
			mSelectedPoint->mY = cy;
			InvalidateAroundPoint(mSelectedPoint);
		}
	} else {
		DragObject dobj = ScreenToObject(x, y);

		if (dobj.mPoint != mHighlightedPoint) {
			if (mHighlightedPart)
				InvalidateAtPoint(mHighlightedPoint);
			mHighlightedPoint = dobj.mPoint;
			if (dobj.mPart)
				InvalidateAtPoint(mHighlightedPoint);
		}
		mHighlightedPart = dobj.mPart;

		SetStatus(dobj.mPart != kPartNone ? kStatus_PointHighlighted : kStatus_Focused);
	}
}

void VDParameterCurveControlW32::OnMouseLeave() {
	mbTrackingMouse = false;

	SetStatus(kStatus_Nothing);
}

void VDParameterCurveControlW32::OnPaint() {
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(mhwnd, &ps);

	if (!hdc)		// hrm... this is bad
		return;

	int nSavedDC = SaveDC(hdc);
	if (nSavedDC) {
		if (!mpCurve) {
			FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_3DFACE + 1));
		} else {
			const VDParameterCurve::PointList& pts = mpCurve->Points();
			RECT r;

			GetClientRect(mhwnd, &r);

			double x1 = ScreenToCurveX(r.left);
			double x2 = ScreenToCurveX(r.right);

			VDPosition ix1 = VDFloorToInt64(x1);
			VDPosition ix2 = VDCeilToInt64(x2);

			if (ix1 < 0)
				ix1 = 0;

			ix1 -= ix1 % 5;
			ix2 += 4;
			ix2 -= ix2 % 5;

			SelectObject(hdc, mhpenGridLines2);
			SelectObject(hdc, mhfont);

			HBRUSH hBackFill = (HBRUSH)GetStockObject(BLACK_BRUSH);

			RECT rFill = {r.right, r.top, r.right, r.bottom};
			{for(VDPosition ix=ix2-1; ix>=ix1; --ix) {
				int sx = CurveToScreenX((double)ix);

				if (sx+1 < rFill.right) {
					rFill.left = sx+1;
					FillRect(hdc, &rFill, hBackFill);
				}
				rFill.right = sx;
			}}
			if (rFill.right > r.left) {
				rFill.left = r.left;
				FillRect(hdc, &rFill, hBackFill);
			}

			int offset = (int)((ix2-1) % 5);
			if (offset < 0)
				offset += 5;
			{for(VDPosition ix=ix2-1; ix>=ix1; --ix) {
				int sx = CurveToScreenX((double)ix);

				if (offset) {
					MoveToEx(hdc, sx, r.top, NULL);
					LineTo(hdc, sx, r.bottom);
				} else {
					SelectObject(hdc, mhpenGridLines);
					MoveToEx(hdc, sx, r.top, NULL);
					LineTo(hdc, sx, r.bottom);
					SelectObject(hdc, mhpenGridLines2);
				}

				if (--offset < 0)
					offset += 5;
			}}

			{
				// draw x focus line
				int x0 = CurveToScreenX(mPosX);
				int x1 = CurveToScreenX(mPosX+1);
				HBRUSH b1 = CreateSolidBrush(RGB(0, 70, 100));
				RECT rFill = {x0, r.top, x1, r.bottom};
				FillRect(hdc, &rFill, b1);
				DeleteObject(b1);
				SelectObject(hdc, mhpenCurrentPos);
				MoveToEx(hdc, x0, r.top, NULL);
				LineTo(hdc, x0, r.bottom);
			}

			// draw y=0 and y=1 lines
			int sy0 = CurveToScreenY(0.0);
			int sy1 = CurveToScreenY(1.0);
			SelectObject(hdc, mhpenGridLines);
			MoveToEx(hdc, r.left, sy0, NULL);
			LineTo(hdc, r.right, sy0);

			MoveToEx(hdc, r.left, sy1, NULL);
			LineTo(hdc, r.right, sy1);

			if (mSelectedPoint!=pts.end()) {
				const VDParameterCurvePoint& pt = *mSelectedPoint;
				int x = CurveToScreenX(pt.mX);
				int y = CurveToScreenY(pt.mY);
				POINT r[] = {{x-7,y},{x,y+7},{x+8,y},{x,y-7}};
				HBRUSH b1 = CreateSolidBrush(RGB(255, 100, 0));
				HGDIOBJ b0 = SelectObject(hdc,b1);
				SelectObject(hdc,GetStockObject(NULL_PEN));
				Polygon(hdc,r,4);
				SelectObject(hdc,b0);
				DeleteObject(b1);
			}

			SetTextColor(hdc, RGB(255, 255, 255));
			SetBkMode(hdc, TRANSPARENT);
			SetTextAlign(hdc, TA_TOP | TA_LEFT);

			offset = (int)((ix2-1) % 5);
			if (offset < 0)
				offset += 5;
			{for(VDPosition ix=ix2-1; ix>=ix1; --ix) {
				int sx = CurveToScreenX((double)ix);

				if (offset==0) {
					wchar_t buf[64];
					swprintf(buf, 64, L"%lld", (long long)ix);
					ExtTextOutW(hdc, sx+4, sy0+4, 0, NULL, buf, wcslen(buf), NULL);
				}

				if (--offset < 0)
					offset += 5;
			}}

			VDParameterCurve::PointList::const_iterator it1(mpCurve->LowerBound(x1));
			VDParameterCurve::PointList::const_iterator it2(mpCurve->LowerBound(x2));

			if (it1 != pts.begin())
				--it1;

			if (it2 != pts.end())
				++it2;

			SetBkMode(hdc, OPAQUE);

			bool isFirstPt = (it1 == pts.begin());
			bool isEndPtPresent = (it2 == pts.end());
			for(; it1!=it2; ++it1) {
				const VDParameterCurvePoint& pt = *it1;

				int x = CurveToScreenX(pt.mX);
				int y = CurveToScreenY(pt.mY);

				if (isFirstPt) {
					SelectObject(hdc, mhpenEndLine);
					MoveToEx(hdc, 0, y, NULL);
					if (x>0) LineTo(hdc, x, y);
					isFirstPt = false;
				}

				RECT rPt = { x-2, y-2, x+3, y+3 };
				bool selected = (it1 == mSelectedPoint);
				bool highlighted = (it1 == mHighlightedPoint);

				if (selected)
					SetBkColor(hdc, RGB(255, 0, 0));
				else if (highlighted)
					SetBkColor(hdc, RGB(255, 255, 0));
				else
					SetBkColor(hdc, RGB(255, 100, 100));

				if (!selected)
					ExtTextOut(hdc, x, y, ETO_OPAQUE, &rPt, "", 0, NULL);

				VDParameterCurve::PointList::const_iterator itNext(it1);
				++itNext;

				if (itNext != it2) {
					const VDParameterCurvePoint& pt2 = *itNext;

					int xn = CurveToScreenX(pt2.mX);
					int yn = CurveToScreenY(pt2.mY);

					if (pt.mbLinear) {
						SelectObject(hdc, mhpenLine);
						MoveToEx(hdc, x, y, NULL);
						LineTo(hdc, xn, yn);
					} else {
						SelectObject(hdc, mhpenCurve);
						MoveToEx(hdc, x, y, NULL);

						if (xn <= x) {
							LineTo(hdc, xn, yn);
						} else {
							POINT pts[16];

							for(int i=1; i<16; ++i) {
								double xt = pt.mX + (pt2.mX - pt.mX) * ((float)i / 16.0f);
								double yt = mpCurve->operator()(xt).mY;

								pts[i-1].x = CurveToScreenX(xt);
								pts[i-1].y = CurveToScreenY(yt);
							}

							pts[15].x = xn;
							pts[15].y = yn;

							PolylineTo(hdc, pts, 16);
						}
					}
				} else if (isEndPtPresent) {
					SelectObject(hdc, mhpenEndLine);
					MoveToEx(hdc, x, y, NULL);
					LineTo(hdc, mWidth, y);
				}
			}

			if (mSelectedPoint!=pts.end()) {
				const VDParameterCurvePoint& pt = *mSelectedPoint;
				int x = CurveToScreenX(pt.mX);
				int y = CurveToScreenY(pt.mY);
				RECT rPt = { x-2, y-2, x+3, y+3 };
				SetBkColor(hdc, RGB(0, 0, 0));
				ExtTextOut(hdc, x, y, ETO_OPAQUE, &rPt, "", 0, NULL);
			}
		}

		RestoreDC(hdc, nSavedDC);
	}

	EndPaint(mhwnd, &ps);
}

void VDParameterCurveControlW32::OnSetCursor(HWND hwnd, UINT htCode, UINT msg) {
	if (hwnd != mhwnd || htCode != HTCLIENT || !mpCurve)
		SetCursor(LoadCursor(NULL, IDC_ARROW));
	else if (mDragPart)
		SetCursor(mhcurMove);
	else if (GetKeyState(VK_SHIFT) < 0) {
		SetCursor(mhcurAdd);
	} else if (mHighlightedPart) {
		if (GetKeyState(VK_CONTROL) < 0)
			SetCursor(mhcurRemove);
		else
			SetCursor(mhcurMove);
	} else
		SetCursor(LoadCursor(NULL, IDC_ARROW));
}

void VDParameterCurveControlW32::Notify(UINT code) {
	NMHDR nm;
	nm.hwndFrom = mhwnd;
	nm.idFrom	= GetWindowLong(mhwnd, GWL_ID);
	nm.code		= code;
	SendMessage(GetParent(mhwnd), WM_NOTIFY, nm.idFrom, (LPARAM)&nm);
}

double VDParameterCurveControlW32::ScreenToCurveX(int x) {
	return ((double)x - mWidth * 0.5) / mScaleX + mPosX;
}

double VDParameterCurveControlW32::ScreenToCurveY(int y) {
	return (mHeight * 0.5 - (double)y) / mScaleY + mPosY;
}

int VDParameterCurveControlW32::CurveToScreenX(double x) {
	return VDRoundToInt(mWidth * 0.5 + (x - mPosX) * mScaleX);
}

int VDParameterCurveControlW32::CurveToScreenY(double y) {
	return VDRoundToInt(mHeight * 0.5 - (y - mPosY) * mScaleY);
}

VDParameterCurveControlW32::DragObject VDParameterCurveControlW32::ScreenToObject(int x, int y) {
	double cx = ScreenToCurveX(x);
	double cy = ScreenToCurveY(y);

	return CurveToObject(cx, cy);
}

VDParameterCurveControlW32::DragObject VDParameterCurveControlW32::CurveToObject(double cx, double cy) {
	const double threshold = ceil(3.0 / mScaleX);

	VDParameterCurve::PointList::iterator it(mpCurve->GetNearestPoint2D(cx, cy, threshold, mScaleY / mScaleX));

	static const double kThreshold = 9.0f;

	DragObject dobj = { mpCurve->End(), kPartNone };
	if (it != mpCurve->End()) {
		dobj.mPoint = it;
		dobj.mPart = kPartPoint;
	}

	return dobj;
}

void VDParameterCurveControlW32::ObjectToCurve(const DragObject& obj, double& x, double& y) {
	x = y = 0;

	if (obj.mPart) {
		const VDParameterCurvePoint& pt = *obj.mPoint;

		x = pt.mX;
		y = pt.mY;
	}
}

void VDParameterCurveControlW32::SetStatus(Status s) {
	if (mStatus != s) {
		mStatus = s;

		mStatusUpdatedEvent.Raise(this, mStatus);
	}
}

void VDParameterCurveControlW32::DeletePoint(int x) {
	InvalidateAtPoint(mSelectedPoint);
	mSelectedPoint = mpCurve->Points().end();
	VDParameterCurve::PointList::iterator it = mpCurve->Points().begin()+x;
	InvalidateAroundPoint(it);
	mpCurve->Points().erase(it);
	SetSelectedPoint(mpCurve->End());
	mCurveUpdatedEvent.Raise(this, 0);
}

void VDParameterCurveControlW32::SetValue(int x, double cy) {
	VDParameterCurve::PointList::iterator it = mpCurve->Points().begin()+x;
	cy = std::max<double>(cy, mpCurve->GetYMin());
	cy = std::min<double>(cy, mpCurve->GetYMax());

	if (cy != it->mY) {
		int xold = CurveToScreenX(it->mX);
		RECT r = { xold-8, 0, xold+9, mHeight };
		InvalidateRect(mhwnd, &r, TRUE);
		mSelectedPoint->mY = cy;
		InvalidateAroundPoint(it);
		mCurveUpdatedEvent.Raise(this, 0);
	}
}

void VDParameterCurveControlW32::SetSelectedPoint(int x) {
	VDParameterCurve::PointList::iterator it = mpCurve->Points().end();
	if (x!=-1) it = mpCurve->Points().begin()+x;
	SetSelectedPoint(it);
}

void VDParameterCurveControlW32::SetSelectedPoint(VDParameterCurve::PointList::iterator it) {
	if(it==mSelectedPoint) return;
	InvalidateAtPoint(mSelectedPoint);
	mSelectedPoint = it;
	InvalidateAtPoint(mSelectedPoint);
	mCurveUpdatedEvent.Raise(this, 1);
}

void VDParameterCurveControlW32::InvalidateAtPoint(VDParameterCurve::PointList::iterator it) {
	if (it != mpCurve->End()) {
		int x = CurveToScreenX(it->mX);
		int y = CurveToScreenY(it->mY);

		RECT rPt = { x-8, y-8, x+9, y+9 };
		InvalidateRect(mhwnd, &rPt, TRUE);
	}
}

void VDParameterCurveControlW32::InvalidateAroundPoint(VDParameterCurve::PointList::iterator it) {
	if (it != mpCurve->End()) {
		double cx1 = it->mX;
		double cx2 = it->mX;
		bool offStart = false;
		bool offEnd = false;

		if (it != mpCurve->Begin()) {
			VDParameterCurve::PointList::iterator it2(it);
			--it2;

			if (it2 != mpCurve->Begin())
				--it2;
			else
				offStart = true;

			cx1 = it2->mX;
		} else
			offStart = true;

		++it;
		if (it != mpCurve->End()) {
			cx2 = it->mX;

			++it;
			if (it != mpCurve->End())
				cx2 = it->mX;
			else
				offEnd = true;
		} else
			offEnd = true;

		int x1 = CurveToScreenX(cx1);
		int x2 = CurveToScreenX(cx2);

		RECT r = { x1-2, 0, x2+3, mHeight };

		if (offStart)
			r.left = 0;

		if (offEnd)
			r.right = mWidth;

		InvalidateRect(mhwnd, &r, TRUE);
	}
}

void VDParameterCurveControlW32::InvalidateRange(VDParameterCurve::PointList::iterator it) {
	if (it != mpCurve->End()) {
		double cx1 = it->mX;
		++it;

		if (it != mpCurve->End()) {
			double cx2 = it->mX;

			int x1 = CurveToScreenX(cx1);
			int x2 = CurveToScreenX(cx2);

			RECT r = { x1, 0, x2+1, mHeight };
			InvalidateRect(mhwnd, &r, TRUE);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////

#if 0

#include <windows.h>
#include <math.h>
#include <vd2/system/memory.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/VDScheduler.h>
#include <vd2/system/error.h>
#include <vd2/system/binary.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofilt.h>
#include <vd2/plugin/vdvideofiltold.h>
#include "InputFile.h"
#include "VBitmap.h"
#include "VideoFilterSystem.h"
#include "FrameSubset.h"
#include "Dub.h"
#include "DubOutput.h"

void VDTestParameterCurveControl() {
	VDRegisterParameterCurveControl();

	HWND hwnd = CreateWindow(g_VDParameterCurveControlClass, "", WS_OVERLAPPEDWINDOW|WS_VISIBLE, 160, 1000, 800, 100, NULL, NULL, GetModuleHandle(NULL), NULL);

	VDParameterCurveControlW32 *p = (VDParameterCurveControlW32 *)GetWindowLongPtr(hwnd, 0);

	VDParameterCurve c;

	VDParameterCurvePoint pts[2]={
		{0,0,4,0,4,0,false},
		{10,1,4,0,4,0,false}
	};
	c.Points().assign(pts, pts+2);

	p->SetCurve(&c);

	MSG msg;
	while(GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

extern void (*g_pPostInitRoutine)();

struct runtests {
	runtests() {
		g_pPostInitRoutine = VDTestParameterCurveControl;
	}
} g_runtests;

#endif
