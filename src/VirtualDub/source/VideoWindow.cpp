//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2002 Avery Lee
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

#define f_VIDEOWINDOW_CPP

#include "stdafx.h"

#include <algorithm>

#include <windows.h>

#include <vd2/system/fraction.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/w32assist.h>
#include <vd2/VDDisplay/display.h>

#include "VideoWindow.h"
#include "prefs.h"			// for display preferences

#include "resource.h"

extern HINSTANCE g_hInst;

extern bool VDPreferencesIsDisplay3DEnabled();

static LRESULT APIENTRY VideoWindowWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

////////////////////////////

extern const char g_szVideoWindowClass[]="VDVideoWindow";

////////////////////////////

class VDVideoWindow : public IVDVideoWindow {
public:
	VDVideoWindow(HWND hwnd);
	~VDVideoWindow();

	static ATOM RegisterControl();

	void SetMouseTransparent(bool);
	void GetSourceSize(int& w, int& h);
	void SetSourceSize(int w, int h);
	void GetFrameSize(int& w, int& h);
	void Move(int x, int y);
	void Resize();
	void SetChild(HWND hwnd);
	void SetDisplay(IVDVideoDisplay *pDisplay);
	const VDFraction GetSourcePAR();
	void SetSourcePAR(const VDFraction& fr);
	void SetResizeParentEnabled(bool enabled);
	double GetMaxZoomForArea(int w, int h);
	void SetBorderless(bool v){ mbBorderless=v; }
	bool GetAutoSize(){ return mbAutoSize; }
	void SetAutoSize(bool v){ mbAutoSize = v; }
	void InitSourcePAR();

private:
	HWND mhwnd;
	HWND mhwndChild;
	HMENU mhmenu;
	int	mInhibitParamUpdateLocks;
	int mSourceWidth;
	int mSourceHeight;
	VDFraction mSourcePARFrac;
	double mSourcePAR;
	double mSourceAspectRatio;
	double mZoom;
	double mAspectRatio;
	double mFreeAspectRatio;
	bool mbAspectIsFrameBased;
	bool mbUseSourcePAR;
	bool mbResizing;
	bool mbMouseTransparent;
	bool mbBorderless;
	bool mbAutoSize;

	IVDVideoDisplay *mpDisplay;
	VDStringW	mSourcePARTextPattern;

	LRESULT mLastHitTest;

	static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void NCPaint(HRGN hrgn);
	void RecalcClientArea(RECT& rc);
	LRESULT RecalcClientArea(NCCALCSIZE_PARAMS& params);
	LRESULT HitTest(int x, int y);
	void OnCommand(int cmd);
	void OnContextMenu(int x, int y);

	void SetAspectRatio(double ar, bool bFrame);
	void SetAspectRatioSourcePAR();
	void SetZoom(double zoom);

	void UpdateSourcePARMenuItem();
};

////////////////////////////

ATOM RegisterVideoWindow() {
	return VDVideoWindow::RegisterControl();
}

IVDVideoWindow *VDGetIVideoWindow(HWND hwnd) {
	return (IVDVideoWindow *)(VDVideoWindow *)GetWindowLongPtr(hwnd, 0);
}

VDVideoWindow::VDVideoWindow(HWND hwnd)
	: mhwnd(hwnd)
	, mhwndChild(NULL)
	, mhmenu(LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_DISPLAY_MENU)))
	, mInhibitParamUpdateLocks(0)
	, mSourceWidth(0)
	, mSourceHeight(0)
	, mSourcePAR(0.0)
	, mSourceAspectRatio(1.0)
	, mZoom(1.0f)
	, mAspectRatio(-1)
	, mFreeAspectRatio(1.0)
	, mLastHitTest(HTNOWHERE)
	, mbUseSourcePAR(false)
	, mbResizing(false)
	, mbMouseTransparent(false)
	, mbBorderless(false)
	, mbAutoSize(false)
	, mpDisplay(NULL)
{
	SetWindowLongPtr(mhwnd, 0, (LONG_PTR)this);

	mSourcePARTextPattern = VDGetMenuItemTextByCommandW32(mhmenu, ID_DISPLAY_AR_PIXEL_SOURCE);
	UpdateSourcePARMenuItem();
}

VDVideoWindow::~VDVideoWindow() {
	if (mhmenu)
		DestroyMenu(mhmenu);
}

ATOM VDVideoWindow::RegisterControl() {
	WNDCLASS wc;

	wc.style			= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc		= VDVideoWindow::WndProcStatic;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= sizeof(VDVideoWindow *);
	wc.hInstance		= g_hInst;
	wc.hIcon			= NULL;
	wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground	= (HBRUSH)(COLOR_3DFACE+1);
	wc.lpszMenuName		= NULL;
	wc.lpszClassName	= g_szVideoWindowClass;

	return RegisterClass(&wc);
}

void VDVideoWindow::SetMouseTransparent(bool trans) {
	mbMouseTransparent = trans;
}

void VDVideoWindow::GetSourceSize(int& w, int& h) {
	w = mSourceWidth;
	h = mSourceHeight;
}

void VDVideoWindow::SetSourceSize(int w, int h) {
	mSourceWidth		= w;
	mSourceHeight		= h;
	mSourceAspectRatio	= 1.0;

	if (h)
		mSourceAspectRatio = (double)w / (double)h;

	Resize();
}

void VDVideoWindow::GetFrameSize(int& w, int& h) {
	RECT r;
	GetClientRect(mhwnd, &r);
	w = r.right;
	h = r.bottom;
}

void VDVideoWindow::SetAspectRatio(double ar, bool bFrame) {
	mAspectRatio = ar;
	mbAspectIsFrameBased = bFrame;
	mbUseSourcePAR = false;

	if (ar > 0) {
		if (bFrame)
			mFreeAspectRatio = ar / mSourceAspectRatio;
		else
			mFreeAspectRatio = ar;
	}

	Resize();
}

void VDVideoWindow::SetAspectRatioSourcePAR() {
	mAspectRatio = -1;
	mbAspectIsFrameBased = false;
	mbUseSourcePAR = true;

	mFreeAspectRatio = mSourcePAR > 0 ? mSourcePAR : 1.0;

	Resize();
}

void VDVideoWindow::InitSourcePAR() {
	mAspectRatio = -1;
	mbAspectIsFrameBased = false;
	mbUseSourcePAR = true;

	mSourcePAR = 1.0;
	mFreeAspectRatio = 1.0;
}

void VDVideoWindow::SetZoom(double zoom) {
	mZoom = zoom;
	Resize();
}

double VDVideoWindow::GetMaxZoomForArea(int w, int h) {
	double frameAspect;

	if (!mbBorderless)
		w -= 8;
	if (w <= 0)
		return 0;

	if (!mbBorderless)
		h -= 8;
	if (h <= 0)
		return 0;

	if (mbUseSourcePAR) {
		double par = 1.0;
		if (mSourcePAR > 0)
			par = mSourcePAR;

		if (mSourceHeight > 0)
			frameAspect = par * (double)mSourceWidth / (double)mSourceHeight;
		else
			frameAspect = par;
	} else if (mAspectRatio < 0) {
		frameAspect = mFreeAspectRatio * mSourceAspectRatio;
	} else {
		frameAspect = mAspectRatio;

		if (!mbAspectIsFrameBased)
			frameAspect *= mSourceAspectRatio;
	}

	return std::min<double>((double)w / ((double)mSourceHeight * frameAspect), (double)h / (double)mSourceHeight);
}

void VDVideoWindow::Move(int x, int y) {
	++mInhibitParamUpdateLocks;
	SetWindowPos(mhwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	--mInhibitParamUpdateLocks;
}

void VDVideoWindow::Resize() {
	if (mSourceWidth > 0 && mSourceHeight > 0) {
		int w, h;

		if (mbUseSourcePAR) {
			double ratio = 1.0;
			if (mSourcePAR > 0)
				ratio = mSourcePAR;

			w = VDRoundToInt(mSourceHeight * mSourceAspectRatio * ratio * mZoom);
		} else if (mAspectRatio < 0) {
			w = VDRoundToInt(mSourceHeight * mSourceAspectRatio * mFreeAspectRatio * mZoom);
		} else {
			if (mbAspectIsFrameBased)
				w = VDRoundToInt(mSourceHeight * mAspectRatio * mZoom);
			else
				w = VDRoundToInt(mSourceWidth * mAspectRatio * mZoom);
		}
		h = VDRoundToInt(mSourceHeight * mZoom);

		if (w < 1)
			w = 1;
		if (h < 1)
			h = 1;

		if (!mbBorderless) {
			w += 8;
			h += 8;
		}

		++mInhibitParamUpdateLocks;
		SetWindowPos(mhwnd, NULL, 0, 0, w, h, SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE);
		--mInhibitParamUpdateLocks;
	}
}

void VDVideoWindow::SetChild(HWND hwnd) {
	mhwndChild = hwnd;
}

void VDVideoWindow::SetDisplay(IVDVideoDisplay *pDisplay) {
	mpDisplay = pDisplay;
}

const VDFraction VDVideoWindow::GetSourcePAR() {
	return mSourcePARFrac;
}

void VDVideoWindow::SetSourcePAR(const VDFraction& fr) {
	mSourcePAR = 0;
	if (fr.getLo())
		mSourcePAR = fr.asDouble();
	mSourcePARFrac = fr;
	UpdateSourcePARMenuItem();
}

LRESULT CALLBACK VDVideoWindow::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_NCCREATE) {
		VDVideoWindow *pvw = new VDVideoWindow(hwnd);

		if (!pvw)
			return FALSE;
	} else if (msg == WM_NCDESTROY) {
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return reinterpret_cast<VDVideoWindow *>(GetWindowLongPtr(hwnd, 0))->WndProc(msg, wParam, lParam);
}

LRESULT VDVideoWindow::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_CREATE:
		{
			RECT rc;

			GetClientRect(mhwnd, &rc);
		}
		return TRUE;
	case WM_DESTROY:
		{
			volatile HWND hwnd = mhwnd;

			delete this;

			return DefWindowProc(hwnd, msg, wParam, lParam);
		}
	case WM_NCHITTEST:
		return mLastHitTest = HitTest((SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam));
	case WM_NCCALCSIZE:
		if (wParam)
			return RecalcClientArea(*(NCCALCSIZE_PARAMS *)lParam);

		RecalcClientArea(*(RECT *)lParam);
		return 0;
	case WM_NCPAINT:
		NCPaint((HRGN)wParam);
		return 0;
	case WM_ERASEBKGND:
		if (mbBorderless) return true;
		break;
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			if (HDC hdc = BeginPaint(mhwnd, &ps)) {
				// We used to dispatch a notification that the video window needed to be updated from
				// here, but that's bad because with composition it's no longer guaranteed that the video
				// display fully blocks painting of this window.
				EndPaint(mhwnd, &ps);
			}
		}
		return 0;

	case WM_MOVE:
		break;
	case WM_SIZE:
		break;

	case WM_ENTERSIZEMOVE:
		mbResizing = true;
		mbAutoSize = false;
		break;
	case WM_EXITSIZEMOVE:
		mbResizing = false;
		break;

	case WM_WINDOWPOSCHANGING:
		if (mbResizing) {
			WINDOWPOS *pwp = ((WINDOWPOS *)lParam);
			pwp->flags |= SWP_NOZORDER;

			if (mAspectRatio > 0 || mbUseSourcePAR) {
				double ar = mbUseSourcePAR ? mSourcePAR > 0 ? mSourcePAR : 1.0 : mAspectRatio;

				if (!mbAspectIsFrameBased)
					ar *= (double)mSourceWidth / (double)mSourceHeight;

				bool bXMajor = pwp->cx > pwp->cy * ar;

				if (mLastHitTest == HTBOTTOM)
					bXMajor = false;
				else if (mLastHitTest == HTRIGHT)
					bXMajor = true;

				if (bXMajor)
					pwp->cy = VDRoundToInt(pwp->cx / ar);
				else
					pwp->cx = VDRoundToInt(pwp->cy * ar);
			}
		}
		break;
	case WM_WINDOWPOSCHANGED:
		{
			NMHDR hdr;
			RECT r;

			GetClientRect(mhwnd, &r);

			const WINDOWPOS& wp = *(const WINDOWPOS *)lParam;
			if (mSourceHeight > 0 && !mInhibitParamUpdateLocks && !(wp.flags & SWP_NOSIZE)) {
				mZoom = (double)r.bottom / mSourceHeight;

				if (mAspectRatio < 0 && !mbUseSourcePAR && r.right && r.bottom)
					mFreeAspectRatio = r.right / (r.bottom * mSourceAspectRatio);
			}

			hdr.hwndFrom = mhwnd;
			hdr.idFrom = GetWindowLong(mhwnd, GWL_ID);
			hdr.code = VWN_RESIZED;

			if (mhwndChild)
				SetWindowPos(mhwndChild, NULL, 0, 0, r.right, r.bottom, SWP_NOZORDER);
			SendMessage(GetParent(mhwnd), WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr);
		}
		break;		// explicitly pass this through to DefWindowProc for WM_SIZE and WM_MOVE

	case WM_CONTEXTMENU:
		OnContextMenu((sint16)LOWORD(lParam), (sint16)HIWORD(lParam));
		return 0;

	case WM_COMMAND:
		OnCommand(LOWORD(wParam));
		break;

	case WM_GETMINMAXINFO:
		{
			MINMAXINFO& mmi = *(MINMAXINFO *)lParam;
			DefWindowProc(mhwnd, msg, wParam, lParam);

			if (mmi.ptMinTrackSize.x < 9)
				mmi.ptMinTrackSize.x = 9;

			if (mmi.ptMinTrackSize.y < 9)
				mmi.ptMinTrackSize.y = 9;
		}
		return 0;
	}

	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

void VDVideoWindow::NCPaint(HRGN hrgn) {
	if (mbBorderless)
		return;
	
	HDC hdc;
	
	// MSDN docs are in error -- if you do not include 0x10000, GetDCEx() will
	// return NULL but still won't set an error flag.  Feng Yuan's Windows
	// Graphics book calls this the "undocumented flag that makes GetDCEx()
	// succeed" flag. ^^;
	//
	// WINE's source code documents it as DCX_USESTYLE, which makes more sense,
	// and that is the way WINE's DefWindowProc() handles WM_NCPAINT.  This is
	// a cleaner solution than using DCX_CACHE, which also works but overrides
	// CS_OWNDC and CS_PARENTDC.  I'm not going to argue with years of reverse
	// engineering.
	//
	// NOTE: Using DCX_INTERSECTRGN as recommended by MSDN causes Windows 98
	//       GDI to intermittently crash!

	hdc = GetDCEx(mhwnd, NULL, DCX_WINDOW|0x10000);

	if (hdc) {
		RECT rc;
		GetWindowRect(mhwnd, &rc);

		if ((WPARAM)hrgn > 1) {
			OffsetClipRgn(hdc, rc.left, rc.top);
			ExtSelectClipRgn(hdc, hrgn, RGN_AND);
			OffsetClipRgn(hdc, -rc.left, -rc.top);
		}

		OffsetRect(&rc, -rc.left, -rc.top);

		DrawEdge(hdc, &rc, BDR_RAISEDOUTER|BDR_RAISEDINNER, BF_RECT);
		rc.left		+= 2;
		rc.right	-= 2;
		rc.top		+= 2;
		rc.bottom	-= 2;
		DrawEdge(hdc, &rc, BDR_SUNKENOUTER|BDR_SUNKENINNER, BF_RECT);

		ReleaseDC(mhwnd, hdc);
	}
}

void VDVideoWindow::RecalcClientArea(RECT& rc) {
	if (!mbBorderless) {
		rc.left += 4;
		rc.right -= 4;
		rc.top += 4;
		rc.bottom -= 4;
	}
}

LRESULT VDVideoWindow::RecalcClientArea(NCCALCSIZE_PARAMS& params) {
	// Win32 docs don't say you need to do this, but you do.

	if (!mbBorderless) {
		params.rgrc[0].left += 4;
		params.rgrc[0].right -= 4;
		params.rgrc[0].top += 4;
		params.rgrc[0].bottom -= 4;
	}

	return 0;//WVR_ALIGNTOP|WVR_ALIGNLEFT;
}

LRESULT VDVideoWindow::HitTest(int x, int y) {
	POINT pt = { x, y };
	RECT rc;

	GetClientRect(mhwnd, &rc);
	ScreenToClient(mhwnd, &pt);

	if (mbBorderless || (pt.x >= 4 && pt.y >= 4 && pt.x < rc.right-4 && pt.y < rc.bottom-4))
		return mbMouseTransparent ? HTTRANSPARENT : HTCLIENT; //HTCAPTION;
	else {
		int xseg = std::min<int>(16, rc.right/3);
		int yseg = std::min<int>(16, rc.bottom/3);
		int id = 0;

		if (pt.x >= xseg)
			++id;

		if (pt.x >= rc.right - xseg)
			++id;

		if (pt.y >= yseg)
			id += 3;

		if (pt.y >= rc.bottom - yseg)
			id += 3;

		static const LRESULT sHitRegions[9]={
			HTNOWHERE,//HTTOPLEFT,
			HTNOWHERE,//HTTOP,
			HTNOWHERE,//HTTOPRIGHT,
			HTNOWHERE,//HTLEFT,
			HTCLIENT,		// should never be hit
			HTRIGHT,
			HTNOWHERE,//HTBOTTOMLEFT,
			HTBOTTOM,
			HTBOTTOMRIGHT
		};

		return sHitRegions[id];
	}
}

void VDVideoWindow::OnCommand(int cmd) {
	switch(cmd) {
	case ID_DISPLAY_ZOOM_6:			mbAutoSize = false; SetZoom(0.0625); break;
	case ID_DISPLAY_ZOOM_12:		mbAutoSize = false; SetZoom(0.125); break;
	case ID_DISPLAY_ZOOM_25:		mbAutoSize = false; SetZoom(0.25); break;
	case ID_DISPLAY_ZOOM_33:		mbAutoSize = false; SetZoom(1.0/3.0); break;
	case ID_DISPLAY_ZOOM_50:		mbAutoSize = false; SetZoom(0.5); break;
	case ID_DISPLAY_ZOOM_66:		mbAutoSize = false; SetZoom(2.0/3.0); break;
	case ID_DISPLAY_ZOOM_75:		mbAutoSize = false; SetZoom(3.0/4.0); break;
	case ID_DISPLAY_ZOOM_100:		mbAutoSize = false; SetZoom(1.0); break;
	case ID_DISPLAY_ZOOM_150:		mbAutoSize = false; SetZoom(1.5); break;
	case ID_DISPLAY_ZOOM_200:		mbAutoSize = false; SetZoom(2.0); break;
	case ID_DISPLAY_ZOOM_300:		mbAutoSize = false; SetZoom(3.0); break;
	case ID_DISPLAY_ZOOM_400:		mbAutoSize = false; SetZoom(4.0); break;
	case ID_DISPLAY_ZOOM_EXACT:
		mbAutoSize = false;
		mZoom = 1.0;
		SetAspectRatio(1.0, false);
		break;
	case ID_DISPLAY_AR_FREE:		SetAspectRatio(-1, false); break;
	case ID_DISPLAY_AR_PIXEL_SOURCE: SetAspectRatioSourcePAR(); break;
	case ID_DISPLAY_AR_PIXEL_0909:  SetAspectRatio( 10.0/11.0, false); break;
	case ID_DISPLAY_AR_PIXEL_1000:	SetAspectRatio(  1.0     , false); break;
	case ID_DISPLAY_AR_PIXEL_1093:  SetAspectRatio( 59.0/54.0, false); break;
	case ID_DISPLAY_AR_PIXEL_1212:  SetAspectRatio( 40.0/33.0, false); break;
	case ID_DISPLAY_AR_PIXEL_1364:  SetAspectRatio( 15.0/11.0, false); break;
	case ID_DISPLAY_AR_PIXEL_1457:  SetAspectRatio(118.0/81.0, false); break;
	case ID_DISPLAY_AR_PIXEL_1639:  SetAspectRatio( 59.0/36.0, false); break;
	case ID_DISPLAY_AR_PIXEL_1818:  SetAspectRatio( 20.0/11.0, false); break;
	case ID_DISPLAY_AR_PIXEL_2185:  SetAspectRatio( 59.0/27.0, false); break;
	case ID_DISPLAY_AR_FRAME_1333:	SetAspectRatio(4.0/3.0, true); break;
	case ID_DISPLAY_AR_FRAME_1364:	SetAspectRatio(15.0/11.0, true); break;
	case ID_DISPLAY_AR_FRAME_1777:	SetAspectRatio(16.0/9.0, true); break;
	case ID_DISPLAY_FILTER_POINT:
		if (mpDisplay)
			mpDisplay->SetFilterMode(IVDVideoDisplay::kFilterPoint);
		break;
	case ID_DISPLAY_FILTER_BILINEAR:
		if (mpDisplay)
			mpDisplay->SetFilterMode(IVDVideoDisplay::kFilterBilinear);
		break;
	case ID_DISPLAY_FILTER_BICUBIC:
		if (mpDisplay)
			mpDisplay->SetFilterMode(IVDVideoDisplay::kFilterBicubic);
		break;
	case ID_DISPLAY_FILTER_ANY:
		if (mpDisplay)
			mpDisplay->SetFilterMode(IVDVideoDisplay::kFilterAnySuitable);
		break;
	}
}

void VDVideoWindow::OnContextMenu(int x, int y) {
	HMENU hmenu = GetSubMenu(mhmenu, 0);

	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_6, fabs(mZoom - 0.0625) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_12, fabs(mZoom - 0.125) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_25, fabs(mZoom - 0.25) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_33, fabs(mZoom - 1.0/3.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_50, fabs(mZoom - 0.5) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_66, fabs(mZoom - 2.0/3.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_75, fabs(mZoom - 3.0/4.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_100, fabs(mZoom - 1.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_150, fabs(mZoom - 1.5) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_200, fabs(mZoom - 2.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_300, fabs(mZoom - 3.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_ZOOM_400, fabs(mZoom - 4.0) < 1e-5);

	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_SOURCE, mbUseSourcePAR);

	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_FREE, !mbUseSourcePAR && mAspectRatio < 0);

	// The aspect ratio values below come from "mir DMG: Aspect Ratios and Frame Sizes",
	// http://www.mir.com/DMG/aspect.html, as seen on 09/02/2004.
	bool explicitFrame = !mbUseSourcePAR && mbAspectIsFrameBased;
	bool explicitPixel = !mbUseSourcePAR && !mbAspectIsFrameBased;

	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_0909, explicitPixel && fabs(mAspectRatio - 10.0/11.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_1000, explicitPixel && fabs(mAspectRatio -  1.0     ) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_1093, explicitPixel && fabs(mAspectRatio - 59.0/54.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_1212, explicitPixel && fabs(mAspectRatio - 40.0/33.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_1364, explicitPixel && fabs(mAspectRatio - 15.0/11.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_1457, explicitPixel && fabs(mAspectRatio -118.0/81.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_1639, explicitPixel && fabs(mAspectRatio - 59.0/36.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_1818, explicitPixel && fabs(mAspectRatio - 20.0/11.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_PIXEL_2185, explicitPixel && fabs(mAspectRatio - 59.0/27.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_FRAME_1333, explicitFrame && fabs(mAspectRatio -  4.0/ 3.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_FRAME_1364, explicitFrame && fabs(mAspectRatio - 15.0/11.0) < 1e-5);
	VDCheckMenuItemByCommandW32(hmenu, ID_DISPLAY_AR_FRAME_1777, explicitFrame && fabs(mAspectRatio - 16.0/ 9.0) < 1e-5);

	DWORD dwEnabled1 = MF_BYCOMMAND | MF_GRAYED;
	if (mpDisplay && !(g_prefs.fDisplay & Preferences::kDisplayDisableDX)) {
		if ((g_prefs.fDisplay & (Preferences::kDisplayEnableD3D | Preferences::kDisplayEnableOpenGL))
			|| VDPreferencesIsDisplay3DEnabled())
			dwEnabled1 = MF_BYCOMMAND | MF_ENABLED;
	}
	EnableMenuItem(hmenu, ID_DISPLAY_FILTER_POINT, dwEnabled1);
	EnableMenuItem(hmenu, ID_DISPLAY_FILTER_BILINEAR, dwEnabled1);
	EnableMenuItem(hmenu, ID_DISPLAY_FILTER_BICUBIC, dwEnabled1);
	EnableMenuItem(hmenu, ID_DISPLAY_FILTER_ANY, dwEnabled1);

	if (mpDisplay) {
		IVDVideoDisplay::FilterMode mode = mpDisplay->GetFilterMode();

		CheckMenuItem(hmenu, ID_DISPLAY_FILTER_POINT, mode == IVDVideoDisplay::kFilterPoint ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
		CheckMenuItem(hmenu, ID_DISPLAY_FILTER_BILINEAR, mode == IVDVideoDisplay::kFilterBilinear ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
		CheckMenuItem(hmenu, ID_DISPLAY_FILTER_BICUBIC, mode == IVDVideoDisplay::kFilterBicubic ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
		CheckMenuItem(hmenu, ID_DISPLAY_FILTER_ANY, mode == IVDVideoDisplay::kFilterAnySuitable ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
	}

	TrackPopupMenu(hmenu, TPM_LEFTALIGN|TPM_TOPALIGN|TPM_LEFTBUTTON, x, y, 0, mhwnd, NULL);
}

void VDVideoWindow::UpdateSourcePARMenuItem() {
	VDStringW s;

	if (mSourcePAR <= 0)
		s = L"Unknown ratio";
	else
		s.sprintf(L"%.4g:1 pixel", mSourcePAR);

	VDStringW t(mSourcePARTextPattern);
	VDStringW::size_type pos = t.find('?');

	if (pos != VDStringW::npos)
		t.replace(pos, 1, s.data(), s.size());
	
	VDSetMenuItemTextByCommandW32(mhmenu, ID_DISPLAY_AR_PIXEL_SOURCE, t.c_str());
}
