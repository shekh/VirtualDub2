//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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
#include <vd2/VDDisplay/display.h>
#include <vd2/VDDisplay/displaydrv.h>
#include <vd2/system/w32assist.h>

#include "oshelper.h"

#include "ClippingControl.h"
#include "PositionControl.h"

extern HINSTANCE g_hInst;

static LRESULT APIENTRY ClippingControlWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

const char szClippingControlName[]="birdyClippingControl";
static const char szClippingControlOverlayName[]="birdyClippingControlOverlay";

/////////////////////////////////////////////////////////////////////////////

VDClippingControlOverlay::VDClippingControlOverlay(HWND hwnd)
	: mhwnd(hwnd)
	, mBorderPen(::CreatePen(PS_DOT, 0, RGB(128, 128, 128)))
	, mX(0)
	, mY(0)
	, mWidth(0)
	, mHeight(0)
	, mSourceWidth(0)
	, mSourceHeight(0)
	, mInvSourceWidth(0.0)
	, mInvSourceHeight(0.0)
	, mDragPoleX(-1)
	, mDragPoleY(-1)
{
	mXBounds[0] = mYBounds[0] = 0.0;
	mXBounds[1] = mYBounds[1] = 1.0;

	if (!mBorderPen)
		mBorderPen = (HPEN)GetStockObject(WHITE_PEN);

	hwndDisplay = 0;
	pVD = 0;
	fillBorder = true;
	drawFrame = true;
}

VDClippingControlOverlay::~VDClippingControlOverlay() {
	DeleteObject(mBorderPen);
}

VDClippingControlOverlay *VDClippingControlOverlay::Create(HWND hwndParent, int x, int y, int cx, int cy, UINT id) {
	HWND hwnd = CreateWindowEx(0, szClippingControlOverlayName, "", WS_VISIBLE|WS_CHILD|WS_CLIPSIBLINGS, x, y, cx, cy, hwndParent, (HMENU)id, g_hInst, NULL);

	if (hwnd)
		return (VDClippingControlOverlay *)GetWindowLongPtr(hwnd, 0);

	return NULL;
}

void VDClippingControlOverlay::SetImageRect(int x, int y, int cx, int cy) {
	mX = x;
	mY = y;
	mWidth = cx;
	mHeight = cy;
}

void VDClippingControlOverlay::SetSourceSize(int w, int h) {
	int x1 = VDRoundToLong(mSourceWidth * mXBounds[0]);
	int x2 = VDRoundToLong(mSourceWidth * (1.0 - mXBounds[1]));
	int y1 = VDRoundToLong(mSourceHeight * mYBounds[0]);
	int y2 = VDRoundToLong(mSourceHeight * (1.0 - mYBounds[1]));
	mSourceWidth		= w;
	mSourceHeight		= h;
	mInvSourceWidth		= w ? 1.0 / (double)w : 0.0;
	mInvSourceHeight	= h ? 1.0 / (double)h : 0.0;
	SetBounds(x1,y1,x2,y2);
}

void VDClippingControlOverlay::SetBounds(int x1, int y1, int x2, int y2) {
	mXBounds[0] = x1 * mInvSourceWidth;
	mXBounds[1] = 1.0 - x2 * mInvSourceWidth;
	mYBounds[0] = y1 * mInvSourceHeight;
	mYBounds[1] = 1.0 - y2 * mInvSourceHeight;

	InvalidateRect(mhwnd, NULL, FALSE);
}

void VDClippingControlOverlay::SetDisplayPos(int x, int y, int w, int h) {
	SetImageRect(4, 4, w, h);
	SetWindowPos(mhwnd, NULL, x+4, y+4, w+8, h+8, SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOZORDER);
}

/////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK VDClippingControlOverlay::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDClippingControlOverlay *pThis = (VDClippingControlOverlay *)GetWindowLongPtr(hwnd, 0);

	switch(msg) {
	case WM_NCCREATE:
		if (!(pThis = new_nothrow VDClippingControlOverlay(hwnd)))
			return FALSE;

		SetWindowLongPtr(hwnd, 0, (LONG_PTR)pThis);
		break;

	case WM_NCDESTROY:
		delete pThis;
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return pThis->WndProc(msg, wParam, lParam);
}

LRESULT VDClippingControlOverlay::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(mhwnd, &ps);
			if (drawFrame) {
				RECT r;
				GetClientRect(mhwnd, &r);
				const int w = r.right - r.left;
				const int h = r.bottom - r.top;

				Draw3DRect(hdc, r.left, r.top, w, h, FALSE);
				Draw3DRect(hdc, r.left+3, r.top+3, w-6, h-6, TRUE);
			}
			EndPaint(mhwnd, &ps);
		}
		return 0;
	case WM_NCHITTEST:
		return OnNcHitTest((SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam));
	case WM_MOUSEMOVE:
		OnMouseMove((SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam), wParam);
		return 0;
	case WM_LBUTTONDOWN:
		OnLButtonDown((SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam));
		return 0;
	case WM_LBUTTONUP:
		{
			ClippingControlBounds ccb;
			ccb.state = 1;
			ccb.x1 = VDRoundToLong(mSourceWidth * mXBounds[0]);
			ccb.x2 = VDRoundToLong(mSourceWidth * (1.0 - mXBounds[1]));
			ccb.y1 = VDRoundToLong(mSourceHeight * mYBounds[0]);
			ccb.y2 = VDRoundToLong(mSourceHeight * (1.0 - mYBounds[1]));

			HWND hwndParent = GetParent(mhwnd);
			SendMessage(hwndParent, CCM_SETCLIPBOUNDS, 0, (LPARAM)&ccb);

			mDragPoleX = mDragPoleY = -1;
			ReleaseCapture();
		}
		return 0;
	case WM_SETCURSOR:
		return OnSetCursor(LOWORD(lParam), HIWORD(lParam));
	}
	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

void VDClippingControlOverlay::PreparePaint(IVDVideoDisplayMinidriver* driver) {
	if (fillBorder) {
		long y0 = VDCeilToInt(mYBounds[0] * mHeight - 0.5) - 1;
		long y1 = VDCeilToInt(mYBounds[1] * mHeight - 0.5);
		long x0 = VDCeilToInt(mXBounds[0] * mWidth - 0.5) - 1;
		long x1 = VDCeilToInt(mXBounds[1] * mWidth - 0.5);
		driver->SetClipRgn(CreateRectRgn(x0,y0,x1,y1));
	}
}

void VDClippingControlOverlay::Paint(HDC hdc) {
	if (hdc) {
		HGDIOBJ hgoOld = SelectObject(hdc, mBorderPen);
		int i;

		if (fillBorder) {
			long y0 = VDCeilToInt(mYBounds[0] * mHeight - 0.5) - 1;
			long y1 = VDCeilToInt(mYBounds[1] * mHeight - 0.5);
			long x0 = VDCeilToInt(mXBounds[0] * mWidth - 0.5) - 1;
			long x1 = VDCeilToInt(mXBounds[1] * mWidth - 0.5);
			RECT r;
			GetClientRect(mhwnd, &r);
			RECT r1 = r;
			r1.bottom = y0;
			FillRect(hdc, &r1, (HBRUSH)(COLOR_3DFACE+1));
			r1.bottom = r.bottom;
			r1.top = y1;
			FillRect(hdc, &r1, (HBRUSH)(COLOR_3DFACE+1));
			r1.top = y0;
			r1.bottom = y1;
			r1.right = x0;
			FillRect(hdc, &r1, (HBRUSH)(COLOR_3DFACE+1));
			r1.right = r.right;
			r1.left = x1;
			FillRect(hdc, &r1, (HBRUSH)(COLOR_3DFACE+1));
		}

		::SetBkMode(hdc, OPAQUE);
		::SetBkColor(hdc, RGB(0, 0, 0));

		// draw horizontal lines
		static const int adjust[2]={-1,0};
		for(i=0; i<2; ++i) {
			long y = VDCeilToInt(mYBounds[i] * mHeight - 0.5) + adjust[i];

			if (y >= 0 && y < mHeight) {
				MoveToEx(hdc, 0, y, NULL);
				LineTo(hdc, mWidth, y);
			}
		}

		// draw vertical lines
		for(i=0; i<2; ++i) {
			long x = VDCeilToInt(mXBounds[i] * mWidth - 0.5) + adjust[i];

			if (x >= 0 && x < mWidth) {
				MoveToEx(hdc, x, 0, NULL);
				LineTo(hdc, x, mHeight);
			}
		}

		SelectObject(hdc, hgoOld);
	}
}

void VDClippingControlOverlay::PaintZoom(HDC hdc, int xDst, int yDst, int dstW, int dstH, int xSrc, int ySrc, int srcW, int srcH) {
	HGDIOBJ hgoOld = SelectObject(hdc, mBorderPen);
	int i;

	::SetBkMode(hdc, OPAQUE);
	::SetBkColor(hdc, RGB(0, 0, 0));

	// draw horizontal lines
	static const int adjust[2]={-1,0};
	for(i=0; i<2; ++i) {
    double p = VDRoundToLong(mSourceHeight * mYBounds[i]);
		long y = VDCeilToInt((p-ySrc)/srcH*dstH - 0.5) + yDst + adjust[i];

		MoveToEx(hdc, xDst, y, NULL);
		LineTo(hdc, xDst+dstW, y);
	}

	// draw vertical lines
	for(i=0; i<2; ++i) {
    double p = VDRoundToLong(mSourceWidth * mXBounds[i]);
		long x = VDCeilToInt((p-xSrc)/srcW*dstW - 0.5) + xDst + adjust[i];

		MoveToEx(hdc, x, yDst, NULL);
		LineTo(hdc, x, yDst+dstH);
	}

	SelectObject(hdc, hgoOld);
}

void VDClippingControlOverlay::OnMouseMove(int x, int y, int mods) {
	long y0 = VDCeilToInt(mYBounds[0] * mHeight - 0.5) - 1;
	long y1 = VDCeilToInt(mYBounds[1] * mHeight - 0.5);
	long x0 = VDCeilToInt(mXBounds[0] * mWidth - 0.5) - 1;
	long x1 = VDCeilToInt(mXBounds[1] * mWidth - 0.5);

	if (mDragPoleX>=0 || mDragPoleY>=0) {
		bool roundoff = (mods & MK_SHIFT) != 0;

		if (mDragPoleX>=0) {
			double v = (x-mX+1) / (double)(mWidth + 1);
			int i;

			if (roundoff && mSourceWidth) {
				if (mDragPoleX == 0) {
					int x2 = VDRoundToLong(mSourceWidth * mXBounds[1]);

					v = (x2 - ((x2 - VDRoundToLong(mSourceWidth * v) + 8) & ~15)) * mInvSourceWidth;
				} else {
					int x1 = VDRoundToLong(mSourceWidth * mXBounds[0]);

					v = (x1 + ((VDRoundToLong(mSourceWidth * v) - x1 + 8) & ~15)) * mInvSourceWidth;
				}
			}

			if (v > 1.0)
				v = 1.0;
			if (v < 0.0)
				v = 0.0;

			mXBounds[mDragPoleX] = v;

			for(i=mDragPoleX-1; i>=0 && mXBounds[i] > v; --i)
				mXBounds[i] = v;

			for(i=mDragPoleX+1; i<2 && mXBounds[i] < v; ++i)
				mXBounds[i] = v;
		}

		if (mDragPoleY>=0) {
			double v = (y-mY+1) / (double)(mHeight + 1);
			int i;

			if (roundoff && mSourceHeight) {
				if (mDragPoleY == 0) {
					int y2 = VDRoundToLong(mSourceHeight * mYBounds[1]);

					v = (y2 - ((y2 - VDRoundToLong(mSourceHeight * v) + 8) & ~15)) * mInvSourceHeight;
				} else {
					int y1 = VDRoundToLong(mSourceHeight * mYBounds[0]);

					v = (y1 + ((VDRoundToLong(mSourceHeight * v) - y1 + 8) & ~15)) * mInvSourceHeight;
				}
			}

			if (v > 1.0)
				v = 1.0;
			if (v < 0.0)
				v = 0.0;

			mYBounds[mDragPoleY] = v;

			for(i=mDragPoleY-1; i>=0 && mYBounds[i] > v; --i)
				mYBounds[i] = v;

			for(i=mDragPoleY+1; i<2 && mYBounds[i] < v; ++i)
				mYBounds[i] = v;
		}

		ClippingControlBounds ccb;
		ccb.state = 0;
		ccb.x1 = VDRoundToLong(mSourceWidth * mXBounds[0]);
		ccb.x2 = VDRoundToLong(mSourceWidth * (1.0 - mXBounds[1]));
		ccb.y1 = VDRoundToLong(mSourceHeight * mYBounds[0]);
		ccb.y2 = VDRoundToLong(mSourceHeight * (1.0 - mYBounds[1]));

		HWND hwndParent = GetParent(mhwnd);
		SendMessage(hwndParent, CCM_SETCLIPBOUNDS, 0, (LPARAM)&ccb);

		long ny0 = VDCeilToInt(mYBounds[0] * mHeight - 0.5) - 1;
		long ny1 = VDCeilToInt(mYBounds[1] * mHeight - 0.5);
		long nx0 = VDCeilToInt(mXBounds[0] * mWidth - 0.5) - 1;
		long nx1 = VDCeilToInt(mXBounds[1] * mWidth - 0.5);

		HWND d = hwndDisplay;
		if (mDragPoleX==0 && x0!=nx0) {
			RECT r;
			GetClientRect(d, &r);
			r.left = std::min(x0,nx0)-1;
			r.right = std::max(x0,nx0)+2;
			pVD->DrawInvalidate(&r);
		}
		if (mDragPoleX==1 && x1!=nx1) {
			RECT r;
			GetClientRect(d, &r);
			r.left = std::min(x1,nx1)-1;
			r.right = std::max(x1,nx1)+2;
			pVD->DrawInvalidate(&r);
		}
		if (mDragPoleY==0 && y0!=ny0) {
			RECT r;
			GetClientRect(d, &r);
			r.top = std::min(y0,ny0)-1;
			r.bottom = std::max(y0,ny0)+2;
			pVD->DrawInvalidate(&r);
		}
		if (mDragPoleY==1 && y1!=ny1) {
			RECT r;
			GetClientRect(d, &r);
			r.top = std::min(y1,ny1)-1;
			r.bottom = std::max(y1,ny1)+2;
			pVD->DrawInvalidate(&r);
		}
	}

	OnSetCursor(HTCLIENT, WM_MOUSEMOVE);
}

void VDClippingControlOverlay::OnLButtonDown(int x, int y) {
	PoleHitTest(x, y);

	mDragPoleX = x;
	mDragPoleY = y;

	if (x>=0 || y>=0)
		SetCapture(mhwnd);
}

LRESULT VDClippingControlOverlay::OnNcHitTest(int x, int y) {
	if (mDragPoleX >= 0 || mDragPoleY >= 0)
		return HTCLIENT;

	POINT pt = {x, y};
	ScreenToClient(mhwnd, &pt);
	x = pt.x;
	y = pt.y;
	PoleHitTest(x, y);

	return (x&y) < 0 ? HTTRANSPARENT : HTCLIENT;
}

bool VDClippingControlOverlay::OnSetCursor(UINT htcode, UINT mousemsg) {
	DWORD ptdword = GetMessagePos();
	POINT pt = { (SHORT)LOWORD(ptdword), (SHORT)HIWORD(ptdword) };

	ScreenToClient(mhwnd, &pt);

	int x = pt.x;
	int y = pt.y;

	PoleHitTest(x, y);

	static const LPCTSTR sCursor[3][3]={
		{ IDC_ARROW,  IDC_SIZENS, IDC_SIZENS },
		{ IDC_SIZEWE, IDC_SIZENWSE, IDC_SIZENESW },
		{ IDC_SIZEWE, IDC_SIZENESW, IDC_SIZENWSE },
	};

	SetCursor(LoadCursor(NULL, sCursor[x+1][y+1]));
	return true;
}

void VDClippingControlOverlay::PoleHitTest(int& x, int& y) {
	double xf = (x - mX) / (double)mWidth;
	double yf = (y - mY) / (double)mHeight;
	int xi, yi;

	xi = -1;
	while(xi < 1 && mXBounds[xi+1] <= xf)
		++xi;

	yi = -1;
	while(yi < 1 && mYBounds[yi+1] <= yf)
		++yi;

	// [xi, xi+1] and [yi, yi+1] now bound the cursor.  If the cursor is right
	// of the midpoint, select the second pole.

	if (xi<1 && (xi<0 || xf > 0.5 * (mXBounds[xi] + mXBounds[xi+1])))
		++xi;

	if (yi<1 && (yi<0 || yf > 0.5 * (mYBounds[yi] + mYBounds[yi+1])))
		++yi;

	if (fabs(mXBounds[xi] - xf) * mWidth > 5)
		xi = -1;

	if (fabs(mYBounds[yi] - yf) * mHeight > 5)
		yi = -1;

	x = xi;
	y = yi;
}

/////////////////////////////////////////////////////////////////////////////
//
//	VDClippingControl
//
/////////////////////////////////////////////////////////////////////////////

class VDClippingControl : public IVDClippingControl, public IVDVideoDisplayCallback {
public:
	void SetBitmapSize(int sourceW, int sourceH);
	void SetClipBounds(const vdrect32& r);
	void GetClipBounds(vdrect32& r);
	void AutoSize(int borderw, int borderh);
	void BlitFrame(const VDPixmap *);
	void SetFillBorder(bool v);

	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	void DisplayRequestUpdate(IVDVideoDisplay *pDisp);
private:
	enum {
		kIDC_X1_STATIC		= 500,
		kIDC_X1_EDIT		= 501,
		kIDC_X1_SPIN		= 502,
		kIDC_Y1_STATIC		= 503,
		kIDC_Y1_EDIT		= 504,
		kIDC_Y1_SPIN		= 505,
		kIDC_X2_STATIC		= 506,
		kIDC_X2_EDIT		= 507,
		kIDC_X2_SPIN		= 508,
		kIDC_Y2_STATIC		= 509,
		kIDC_Y2_EDIT		= 510,
		kIDC_Y2_SPIN		= 511,
		kIDC_POSITION		= 512,
		kIDC_VIDEODISPLAY	= 513,
		kIDC_SIZE_LABEL		= 514,
		kIDC_SIZE			= 515
	};

	VDClippingControl(HWND hwnd);
	~VDClippingControl();

	static BOOL CALLBACK InitChildrenProc(HWND hwnd, LPARAM lParam);
	static BOOL CALLBACK ShowChildrenProc(HWND hWnd, LPARAM lParam);
	void ResetDisplayBounds();
	bool VerifyParams();
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void	OnSize(int w, int h);

	long mDisplayWidth, mDisplayHeight;
	long mOverlayX, mOverlayY;

	long	mSourceWidth;
	long	mSourceHeight;
	double	mInvSourceWidth;
	double	mInvSourceHeight;

	long x1,y1,x2,y2;

	const HWND	mhwnd;

	bool fInhibitRefresh;

	VDClippingControlOverlay *pOverlay;
};

ATOM RegisterClippingControl() {
	WNDCLASS wc;

	wc.style		= 0;
	wc.lpfnWndProc	= VDClippingControlOverlay::StaticWndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= sizeof(VDClippingControlOverlay *);
	wc.hInstance	= g_hInst;
	wc.hIcon		= NULL;
	wc.hCursor		= NULL;
	wc.hbrBackground= NULL;
	wc.lpszMenuName	= NULL;
	wc.lpszClassName= szClippingControlOverlayName;

	if (!RegisterClass(&wc))
		return 0;

	wc.style		= 0;
	wc.lpfnWndProc	= VDClippingControl::StaticWndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= sizeof(VDClippingControl *);
	wc.hInstance	= g_hInst;
	wc.hIcon		= NULL;
	wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground= (HBRUSH)(COLOR_3DFACE+1);		//GetStockObject(LTGRAY_BRUSH);
	wc.lpszMenuName	= NULL;
	wc.lpszClassName= CLIPPINGCONTROLCLASS;

	return RegisterClass(&wc);
}

VDClippingControl::VDClippingControl(HWND hwnd)
	: mhwnd(hwnd)
	, mDisplayWidth(0)
	, mDisplayHeight(0)
	, mOverlayX(0)
	, mOverlayY(0)
	, mSourceWidth(0)
	, mSourceHeight(0)
	, mInvSourceWidth(0)
	, mInvSourceHeight(0)
	, x1(0)
	, y1(0)
	, x2(0)
	, y2(0)
{
}

VDClippingControl::~VDClippingControl() {
}

IVDClippingControl *VDGetIClippingControl(VDGUIHandle h) {
	return static_cast<IVDClippingControl *>((VDClippingControl *)GetWindowLongPtr((HWND)h, 0));
}

IVDPositionControl *VDGetIPositionControlFromClippingControl(VDGUIHandle h) {
	HWND hwndPosition = GetDlgItem((HWND)h, 512 /* IDC_POSITION */);

	return VDGetIPositionControl((VDGUIHandle)hwndPosition);
}

void VDClippingControl::SetFillBorder(bool v) {
	pOverlay->fillBorder = v;
}

void VDClippingControl::SetBitmapSize(int sourceW, int sourceH) {
	LONG du, duX, duY;
	HWND hwndItem;

	du = GetDialogBaseUnits();
	duX = LOWORD(du);
	duY = HIWORD(du);

	mSourceWidth		= sourceW;
	mSourceHeight		= sourceH;
	mInvSourceWidth		= sourceW ? 1.0 / sourceW : 0.0;
	mInvSourceHeight	= sourceH ? 1.0 / sourceH : 0.0;

	SendMessage(GetDlgItem(mhwnd, kIDC_X1_SPIN), UDM_SETRANGE, 0, (LPARAM)MAKELONG(sourceW,0));
	SendMessage(GetDlgItem(mhwnd, kIDC_Y1_SPIN), UDM_SETRANGE, 0, (LPARAM)MAKELONG(0,sourceH));

	hwndItem = GetDlgItem(mhwnd, kIDC_X2_SPIN);
	SendMessage(hwndItem, UDM_SETRANGE, 0, (LPARAM)MAKELONG(sourceW,0));

	hwndItem = GetDlgItem(mhwnd, kIDC_Y2_SPIN);
	SendMessage(hwndItem, UDM_SETRANGE, 0, (LPARAM)MAKELONG(sourceH,0));

	EnumChildWindows(mhwnd, ShowChildrenProc, 0);

	hwndItem = GetDlgItem(mhwnd, kIDC_VIDEODISPLAY);
	ShowWindow(hwndItem, SW_HIDE);

	pOverlay->SetSourceSize(sourceW, sourceH);

	ResetDisplayBounds();
}

void VDClippingControl::SetClipBounds(const vdrect32& r) {
	x1 = r.left;
	y1 = r.top;
	x2 = r.right;
	y2 = r.bottom;

	pOverlay->SetBounds(x1, y1, x2, y2);

	fInhibitRefresh = TRUE;
	SetDlgItemInt(mhwnd, kIDC_X1_EDIT, x1, FALSE);
	SetDlgItemInt(mhwnd, kIDC_Y1_EDIT, y1, FALSE);
	SetDlgItemInt(mhwnd, kIDC_X2_EDIT, x2, FALSE);
	SetDlgItemInt(mhwnd, kIDC_Y2_EDIT, y2, FALSE);
	fInhibitRefresh = FALSE;

	ResetDisplayBounds();
}

void VDClippingControl::GetClipBounds(vdrect32& r) {
	r.left		= x1;
	r.top		= y1;
	r.right		= x2;
	r.bottom	= y2;
}

void VDClippingControl::AutoSize(int borderw, int borderh) {
	const LONG du = GetDialogBaseUnits();
	const int duX = LOWORD(du);
	const int duY = HIWORD(du);

	int w2 = std::max<int>(mSourceWidth, duX * 12);
	int h2 = std::max<int>(mSourceHeight, duY * 6);

	int wpad = mOverlayX + 8;
	int hpad = (GetDlgItem(mhwnd, kIDC_POSITION)?64:0) + mOverlayY + 8;

	RECT rWorkArea;
	if (SystemParametersInfo(SPI_GETWORKAREA, 0, &rWorkArea, FALSE)) {
		int limitW = rWorkArea.right - rWorkArea.left - wpad - borderw;
		int limitH = rWorkArea.bottom - rWorkArea.top - hpad - borderh;

		if (limitW < 1)
			limitW = 1;

		if (limitH < 1)
			limitH = 1;

		if (w2 > limitW)
			w2 = limitW;

		if (h2 > limitH)
			h2 = limitH;
	}

	SetWindowPos(mhwnd, NULL, 0, 0, w2 + wpad, h2 + hpad, SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOCOPYBITS);
}

void VDClippingControl::BlitFrame(const VDPixmap *px) {
	HWND hwndDisplay = GetDlgItem(mhwnd, kIDC_VIDEODISPLAY);
	bool success = false;

	if (px) {
		IVDVideoDisplay *pVD = VDGetIVideoDisplay((VDGUIHandle)hwndDisplay);

		success = pVD->SetSource(true, *px);
	}

	ShowWindow(GetDlgItem(mhwnd, kIDC_VIDEODISPLAY), success ? SW_SHOWNA : SW_HIDE);
}

BOOL CALLBACK VDClippingControl::InitChildrenProc(HWND hwnd, LPARAM lParam) {
	SendMessage(hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), (LPARAM)MAKELONG(FALSE, 0));

	switch(GetWindowLong(hwnd, GWL_ID)) {
	case kIDC_X1_STATIC:		SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)"X1 offset");
							break;
	case kIDC_Y1_STATIC:		SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)"Y1 offset");
							break;
	case kIDC_X2_STATIC:		SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)"X2 offset");
							break;
	case kIDC_Y2_STATIC:		SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)"Y2 offset");
							break;
	}

	return TRUE;
}

BOOL CALLBACK VDClippingControl::ShowChildrenProc(HWND hWnd, LPARAM lParam) {
	ShowWindow(hWnd, SW_SHOW);

	return TRUE;
}

void VDClippingControl::ResetDisplayBounds() {
	int w = mSourceWidth - x1 - x2;
	int h = mSourceHeight - y1 - y2;
	if (w<0) w = 0;
	if (h<0) h = 0;

	HWND hwndSize = GetDlgItem(mhwnd, kIDC_SIZE);
	if (hwndSize)
		VDSetWindowTextFW32(hwndSize, L"%dx%u", w, h);
}

bool VDClippingControl::VerifyParams() {
	x1 = GetDlgItemInt(mhwnd, kIDC_X1_EDIT, NULL, TRUE);
	x2 = GetDlgItemInt(mhwnd, kIDC_X2_EDIT, NULL, TRUE);
	y1 = GetDlgItemInt(mhwnd, kIDC_Y1_EDIT, NULL, TRUE);
	y2 = GetDlgItemInt(mhwnd, kIDC_Y2_EDIT, NULL, TRUE);

	if (x1<0) {
		SetFocus(GetDlgItem(mhwnd, kIDC_X1_EDIT));
		MessageBeep(MB_ICONQUESTION);
		return false;
	}
	if (y1<0) {
		SetFocus(GetDlgItem(mhwnd, kIDC_Y1_EDIT));
		MessageBeep(MB_ICONQUESTION);
		return false;
	}
	if (x2<0) {
		SetFocus(GetDlgItem(mhwnd, kIDC_X2_EDIT));
		MessageBeep(MB_ICONQUESTION);
		return false;
	}
	if (y2<0) {
		SetFocus(GetDlgItem(mhwnd, kIDC_Y2_EDIT));
		MessageBeep(MB_ICONQUESTION);
		return false;
	}

	return true;
}

LRESULT CALLBACK VDClippingControl::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDClippingControl *pThis = (VDClippingControl *)GetWindowLongPtr(hwnd, 0);

	if (msg == WM_NCCREATE) {
		pThis = new VDClippingControl(hwnd);

		if (!pThis)
			return FALSE;

		SetWindowLongPtr(hwnd, 0, (LONG_PTR)pThis);
	} else if (msg == WM_NCDESTROY) {
		delete pThis;
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return pThis->WndProc(msg, wParam, lParam);
}

LRESULT VDClippingControl::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	if ((msg >= CCM__FIRST && msg < CCM__LAST) || msg == WM_SETTEXT) {
		HWND hWndPosition = GetDlgItem(mhwnd, kIDC_POSITION);

		if (hWndPosition)
			return SendMessage(hWndPosition, msg, wParam, lParam);
		else
			return 0;
	}

	switch(msg) {

	case WM_CREATE:
		{
			LONG du, duX, duY;

			du = GetDialogBaseUnits();
			duX = LOWORD(du);
			duY = HIWORD(du);

			mOverlayX = (53*duX)/4;
			mOverlayY = (14*duY)/8;

			fInhibitRefresh = true;

			// x1
			CreateWindowEx(0				,"STATIC"		,"X1 offset"  ,WS_CHILD | SS_LEFT							,( 0*duX)/4, ( 2*duY)/8, (22*duX)/4, ( 8*duY)/8, mhwnd, (HMENU)kIDC_X1_STATIC	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	, "EDIT"		,""  ,WS_CHILD | WS_TABSTOP | ES_LEFT | ES_NUMBER				,(23*duX)/4, ( 0*duY)/8, (24*duX)/4, (10*duY)/8, mhwnd, (HMENU)kIDC_X1_EDIT	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	,UPDOWN_CLASS	,""  ,WS_CHILD | UDS_NOTHOUSANDS | UDS_AUTOBUDDY | UDS_ALIGNRIGHT	| UDS_SETBUDDYINT, 0, 0,	 ( 2*duX)/4, (10*duY)/8, mhwnd, (HMENU)kIDC_X1_SPIN	, g_hInst, NULL);
			
			// x2
			CreateWindowEx(0				,"STATIC"		,"X2 offset"  ,WS_CHILD | SS_LEFT							,0         , 0         , (22*duX)/4, ( 8*duY)/8, mhwnd, (HMENU)kIDC_X2_STATIC	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	, "EDIT"		,""  ,WS_CHILD | WS_TABSTOP | ES_LEFT | ES_NUMBER				,0         , 0         , (24*duX)/4, (10*duY)/8, mhwnd, (HMENU)kIDC_X2_EDIT	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	,UPDOWN_CLASS	,""  ,WS_CHILD | UDS_NOTHOUSANDS | UDS_ALIGNRIGHT	| UDS_SETBUDDYINT	,0         , 0         , ( 2*duX)/4, (10*duY)/8, mhwnd, (HMENU)kIDC_X2_SPIN	, g_hInst, NULL);

			// y1
			CreateWindowEx(0				,"STATIC"		,"Y1 offset"  ,WS_CHILD | SS_LEFT							,( 0*duX)/4, (16*duY)/8, (22*duX)/4, ( 8*duY)/8, mhwnd, (HMENU)kIDC_Y1_STATIC	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	, "EDIT"		,""  ,WS_CHILD | WS_TABSTOP | ES_LEFT | ES_NUMBER				,(23*duX)/4, (14*duY)/8, (24*duX)/4, (10*duY)/8, mhwnd, (HMENU)kIDC_Y1_EDIT	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	,UPDOWN_CLASS	,""  ,WS_CHILD | UDS_NOTHOUSANDS | UDS_AUTOBUDDY | UDS_ALIGNRIGHT	| UDS_SETBUDDYINT, 0, 0,	 ( 2*duX)/4, (10*duY)/8, mhwnd, (HMENU)kIDC_Y1_SPIN	, g_hInst, NULL);

			// size
			CreateWindowEx(0, "STATIC", "Size", WS_CHILD | SS_LEFT, 0, (26*duY)/8, (47*duX)/4, duY, mhwnd, (HMENU)kIDC_SIZE_LABEL, g_hInst, NULL);
			CreateWindowEx(0, "STATIC", "320x240", WS_CHILD | SS_LEFT, 0, (34*duY)/8, (47*duX)/4, (10*duY)/8, mhwnd, (HMENU)kIDC_SIZE, g_hInst, NULL);
						
			// y2
			CreateWindowEx(0				,"STATIC"		,"Y2 offset"  ,WS_CHILD | SS_LEFT							,0         , 0         , (22*duX)/4, ( 8*duY)/8, mhwnd, (HMENU)kIDC_Y2_STATIC	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	, "EDIT"		,""  ,WS_CHILD | WS_TABSTOP | ES_LEFT | ES_NUMBER				,0         , 0         , (24*duX)/4, (10*duY)/8, mhwnd, (HMENU)kIDC_Y2_EDIT	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	,UPDOWN_CLASS	,""  ,WS_CHILD | UDS_NOTHOUSANDS | UDS_ALIGNRIGHT	| UDS_SETBUDDYINT	,0         , 0         , ( 2*duX)/4, (10*duY)/8, mhwnd, (HMENU)kIDC_Y2_SPIN	, g_hInst, NULL);
			
			HWND hwndDisplay = (HWND)VDCreateDisplayWindowW32(0, WS_CHILD, 0, 0, 0, 0, (VDGUIHandle)mhwnd);
			SetWindowLong(hwndDisplay, GWL_ID, kIDC_VIDEODISPLAY);

			HWND hwndItem = GetDlgItem(mhwnd, kIDC_X2_SPIN);
			SendMessage(hwndItem, UDM_SETBUDDY, (WPARAM)GetDlgItem(mhwnd, kIDC_X2_EDIT), 0);
			hwndItem = GetDlgItem(mhwnd, kIDC_Y2_SPIN);
			SendMessage(hwndItem, UDM_SETBUDDY, (WPARAM)GetDlgItem(mhwnd, kIDC_Y2_EDIT), 0);

			if (GetWindowLong(mhwnd, GWL_STYLE) & CCS_POSITION)
				CreateWindowEx(0,POSITIONCONTROLCLASS,NULL,WS_CHILD									,0,0,0,64,mhwnd,(HMENU)kIDC_POSITION,g_hInst,NULL);

			SetWindowLong(mhwnd, GWL_EXSTYLE, GetWindowLong(mhwnd, GWL_EXSTYLE) | WS_EX_CONTROLPARENT);

			EnumChildWindows(mhwnd, InitChildrenProc, 0);

			pOverlay = VDClippingControlOverlay::Create(mhwnd, 0, 0, 0, 0, 0);

			IVDVideoDisplay *pVD = VDGetIVideoDisplay((VDGUIHandle)hwndDisplay);
			pVD->SetDrawMode(pOverlay);
			pOverlay->pVD = pVD;
			pOverlay->hwndDisplay = hwndDisplay;

			pVD->SetCallback(this);
			
			SetWindowPos(hwndDisplay, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOCOPYBITS);
			fInhibitRefresh = false;
		}
		break;

	case WM_SIZE:
		OnSize(LOWORD(lParam), HIWORD(lParam));
		return 0;

	case WM_ERASEBKGND:
		{
			HBRUSH hbrBackground = (HBRUSH)GetClassLongPtr(mhwnd, GCLP_HBRBACKGROUND);
			HDC hdc = (HDC)wParam;
			RECT r, r2;

			GetClientRect(mhwnd, &r);

			r2 = r;
			r2.bottom = mOverlayY;
			FillRect(hdc, &r2, hbrBackground);

			r2 = r;
			r2.top = mOverlayY;
			r2.bottom = mOverlayY + mDisplayHeight + 8;
			r2.right = mOverlayX;
			FillRect(hdc, &r2, hbrBackground);

			r2.left = mOverlayX + mDisplayWidth + 8;
			r2.right = r.right;
			FillRect(hdc, &r2, hbrBackground);

			r2 = r;
			r2.top = mOverlayY + mDisplayHeight + 8;
			FillRect(hdc, &r2, hbrBackground);
		}
		return TRUE;

	case WM_DESTROY:
		{
			HWND hwndDisplay = GetDlgItem(mhwnd, kIDC_VIDEODISPLAY);
			if (hwndDisplay) {
				IVDVideoDisplay *pDisp = VDGetIVideoDisplay((VDGUIHandle)hwndDisplay);

				pDisp->Destroy();
			}
		}
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case kIDC_X1_EDIT:
		case kIDC_X2_EDIT:
		case kIDC_Y1_EDIT:
		case kIDC_Y2_EDIT:
			if (!fInhibitRefresh)
				if (VerifyParams()) {
					HWND hwndDisplay = GetDlgItem(mhwnd, kIDC_VIDEODISPLAY);
					IVDVideoDisplay *pVD = VDGetIVideoDisplay((VDGUIHandle)hwndDisplay);
					pVD->DrawInvalidate(0);

					pOverlay->SetBounds(x1, y1, x2, y2);
					ResetDisplayBounds();
				}
			return 0;
		case kIDC_POSITION:
			SendMessage(GetParent(mhwnd), WM_COMMAND, MAKELONG(GetWindowLong(mhwnd, GWL_ID), HIWORD(wParam)), (LPARAM)mhwnd);
			return 0;
		}
		break;

	case WM_NOTIFY:
		{
			NMHDR nm = *(NMHDR *)lParam;

			nm.idFrom = GetWindowLong(mhwnd, GWL_ID);
			nm.hwndFrom = mhwnd;

			SendMessage(GetParent(mhwnd), WM_NOTIFY, nm.idFrom, (LPARAM)&nm);
		}
		return 0;

	case WM_SETCURSOR:
		if ((HWND)wParam != mhwnd) {
			SetCursor(LoadCursor(NULL, IDC_ARROW));
			return TRUE;
		}
		break;

	case WM_MOUSEWHEEL:
		// Windows forwards all mouse wheel messages down to us, which we then forward
		// to the position control.  Obviously for this to be safe the position control
		// MUST eat the message, which it currently does.
		{
			HWND hwndPosition = GetDlgItem(mhwnd, kIDC_POSITION);

			if (hwndPosition) {
				SendMessage(hwndPosition, WM_MOUSEWHEEL, wParam, lParam);
				return TRUE;
			}
		}
		break;

	case CCM_SETBITMAPSIZE:
		SetBitmapSize(LOWORD(lParam), HIWORD(lParam));
		AutoSize(0, 0);
		break;

	case CCM_SETCLIPBOUNDS:
		{
			ClippingControlBounds *ccb = (ClippingControlBounds *)lParam;

			SetClipBounds(vdrect32(ccb->x1, ccb->y1, ccb->x2, ccb->y2));
		}
		return 0;

	case CCM_GETCLIPBOUNDS:
		{
			ClippingControlBounds *ccb = (ClippingControlBounds *)lParam;

			ccb->x1 = x1;
			ccb->y1 = y1;
			ccb->x2 = x2;
			ccb->y2 = y2;
		}
		return 0;

	case CCM_BLITFRAME2:
		BlitFrame((const VDPixmap *)lParam);
		return 0;
	}

	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

void VDClippingControl::DisplayRequestUpdate(IVDVideoDisplay *pDisp) {
	NMHDR nm;

	nm.hwndFrom = mhwnd;
	nm.idFrom = GetWindowLong(mhwnd, GWL_ID);
	nm.code = CCN_REFRESHFRAME;

	SendMessage(GetParent(mhwnd), WM_NOTIFY, (WPARAM)nm.idFrom, (LPARAM)&nm);
}

void VDClippingControl::OnSize(int w, int h) {
	LONG du, duX, duY;

	du = GetDialogBaseUnits();
	duX = LOWORD(du);
	duY = HIWORD(du);

	HWND hwndPosition = GetDlgItem(mhwnd, kIDC_POSITION);
	if (hwndPosition) {
		h -= 64;
		SetWindowPos(hwndPosition, NULL, 0, h, w, 64, SWP_NOZORDER | SWP_NOACTIVATE);
	}

	int overlayW = std::max<int>(9, w - mOverlayX);
	int overlayH = std::max<int>(9, h - mOverlayY);

	mDisplayWidth = overlayW - 8;
	mDisplayHeight = overlayH - 8;

	HWND hwndSpinX2 = GetDlgItem(mhwnd, kIDC_X2_SPIN);
	HWND hwndSpinY2 = GetDlgItem(mhwnd, kIDC_Y2_SPIN);
	RECT r;

	GetWindowRect(hwndSpinX2, &r);
	int spinX2Width = r.right - r.left;
	GetWindowRect(hwndSpinY2, &r);
	int spinY2Width = r.right - r.left;

	SetWindowPos(GetDlgItem(mhwnd, kIDC_X2_STATIC),	NULL, w - (48*duX)/4, (2*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
	SetWindowPos(GetDlgItem(mhwnd, kIDC_X2_EDIT  ),	NULL, w - (24*duX)/4, (0*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
	SetWindowPos(hwndSpinX2,						NULL, w - ( 2*duX)/4 - spinX2Width, (0*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
	SetWindowPos(GetDlgItem(mhwnd, kIDC_Y2_STATIC),	NULL, ( 0*duX)/4, h - ( 9*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
	SetWindowPos(GetDlgItem(mhwnd, kIDC_Y2_EDIT  ),	NULL, (24*duX)/4, h - (10*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
	SetWindowPos(hwndSpinY2,						NULL, (48*duX)/4 - spinY2Width, h - (10*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);

	pOverlay->SetImageRect(4, 4, mDisplayWidth, mDisplayHeight);
	SetWindowPos(pOverlay->GetHwnd(), NULL, mOverlayX, mOverlayY, overlayW, overlayH, SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOZORDER);
	HWND hwndDisplay = GetDlgItem(mhwnd, kIDC_VIDEODISPLAY);
	ShowWindow(hwndDisplay, SW_SHOWNA);
	SetWindowPos(hwndDisplay, NULL, mOverlayX+4, mOverlayY+4, mDisplayWidth, mDisplayHeight, SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOZORDER);
	ResetDisplayBounds();
}
