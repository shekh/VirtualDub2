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

//static LRESULT APIENTRY VideoWindowWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

////////////////////////////

const char g_szVideoWindowClass[]="VDVideoWindow";

////////////////////////////

class VDVideoWindow : public IVDVideoWindow {
public:
	VDVideoWindow(HWND hwnd);
	~VDVideoWindow();

	static ATOM RegisterControl();

	void SetMouseTransparent(bool);
	void GetSourceSize(int& w, int& h);
	void SetSourceSize(int w, int h);
	void GetDisplayRect(int& x0, int& y0, int& w, int& h);
	void Move(int x, int y);
	void Resize(bool useWorkArea=true);
	void SetChildPos(float dx=0, float dy=0);
	void ClipPan(float& x, float& y);
	void SetChild(HWND hwnd);
	void SetMaxDisplayHost(HWND hwnd);
	void SetDisplay(IVDVideoDisplay *pDisplay);
	const VDFraction GetSourcePAR();
	void SetSourcePAR(const VDFraction& fr);
	void SetResizeParentEnabled(bool enabled);
	double GetMaxZoomForArea(int w, int h, int border);
	double GetMaxZoomForArea(int w, int h);
	void SetBorder(int v, int ht=-1){ mbBorderless=v==0; mBorder = v; mHTBorder = ht; }
	bool GetAutoSize(){ return mbAutoSize; }
	void SetAutoSize(bool v){ mbAutoSize = v; }
	void InitSourcePAR();
	void SetDrawMode(IVDVideoDisplayDrawMode *p){ mDrawMode=p; }
	void ToggleFullscreen();
	bool IsFullscreen(){ return mbFullscreen; }
	void SetPanCentering(PanCenteringMode mode);

private:
	HWND mhwnd;
	HWND mhwndChild;
	HWND mhwndMax;
	HWND mhwndParent;
	HMENU mhmenu;
	int	mInhibitParamUpdateLocks;
	int	mInhibitWorkArea;
	int mSourceWidth;
	int mSourceHeight;
	int mPanWidth;
	int mPanHeight;
	PanCenteringMode mPanCentering;
	float mPanX;
	float mPanY;
	POINT mPanStart;
	bool mPanMode;
	int mBorder;
	int mHTBorder;
	RECT mWorkArea;
	RECT parentRect;
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
	bool mbAutoBorder;
	bool mbAutoSize;
	bool mbFullscreen;

	IVDVideoDisplay *mpDisplay;
	IVDVideoDisplayDrawMode *mDrawMode;
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
	void SetZoom(double zoom, bool useWorkArea=true);
	void EvalZoom();
	double EvalWidth(double zoom);
	double EvalWidth();
	void SetWorkArea(RECT& r, bool auto_border){ mWorkArea = r; mbAutoBorder = auto_border; }
	void SyncMonitorChange(){ if(mpDisplay) mpDisplay->SyncMonitorChange(); }

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
	, mhwndMax(NULL)
	, mhwndParent(NULL)
	, mhmenu(LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_DISPLAY_MENU)))
	, mInhibitParamUpdateLocks(0)
	, mInhibitWorkArea(0)
	, mSourceWidth(0)
	, mSourceHeight(0)
	, mPanWidth(0)
	, mPanHeight(0)
	, mPanX(0.5)
	, mPanY(0.5)
	, mPanMode(false)
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
	, mBorder(4)
	, mHTBorder(-1)
	, mbAutoSize(false)
	, mbFullscreen(false)
	, mpDisplay(NULL)
{
	SetWindowLongPtr(mhwnd, 0, (LONG_PTR)this);

	mSourcePARTextPattern = VDGetMenuItemTextByCommandW32(mhmenu, ID_DISPLAY_AR_PIXEL_SOURCE);
	UpdateSourcePARMenuItem();

	mWorkArea.left = 0;
	mWorkArea.top = 0;
	mWorkArea.right = 0;
	mWorkArea.bottom = 0;
	mDrawMode = 0;
	mPanCentering = kPanCenter;
}

VDVideoWindow::~VDVideoWindow() {
	if (mhmenu)
		DestroyMenu(mhmenu);
}

ATOM VDVideoWindow::RegisterControl() {
	WNDCLASS wc;

	wc.style			= CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
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

void VDVideoWindow::GetDisplayRect(int& x0, int& y0, int& w, int& h) {
	POINT p = {0,0};
	MapWindowPoints(mhwndChild,mhwnd,&p,1);
	x0 = p.x;
	y0 = p.y;
	w = mPanWidth;
	h = mPanHeight;
}

void VDVideoWindow::ToggleFullscreen() {
	mbFullscreen = !mbFullscreen;
	if (mbFullscreen) {
		GetWindowRect(mhwnd,&parentRect);
		mhwndParent = GetParent(mhwnd);
		MapWindowPoints(0,mhwndParent,(POINT*)&parentRect,2);

		HWND prev = GetWindow(mhwndMax,GW_CHILD);

		SetParent(mhwnd,mhwndMax);
		SetBorder(0,0);
		EnableWindow(mhwndMax,true);
		ShowWindow(mhwndMax,SW_SHOW);
		SendMessage(mhwndMax,WM_SIZE,0,0);

		NMHDR hdr;
		hdr.hwndFrom = mhwnd;
		hdr.idFrom = GetWindowLong(mhwnd, GWL_ID);
		hdr.code = VWN_RESIZED;
		SendMessage(mhwndParent, WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr);

		if (prev) {
			IVDVideoWindow* window = VDGetIVideoWindow(prev);
			window->ToggleFullscreen();
		}

	} else {
		SetParent(mhwnd,mhwndParent);
		SetWindowPos(mhwnd,0,parentRect.left,parentRect.top,parentRect.right-parentRect.left,parentRect.bottom-parentRect.top,0);
		if (!GetWindow(mhwndMax,GW_CHILD)) {
			EnableWindow(mhwndMax,false);
			HWND top = GetWindow(mhwndMax,GW_OWNER);
			HWND top2 = GetWindow(top,GW_ENABLEDPOPUP);
			if(top2) top = top2;
			SetActiveWindow(top);
			ShowWindow(mhwndMax,SW_HIDE);
		}
	}
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
	} else {
		RECT r;
		GetClientRect(mhwnd, &r);
		if (r.right && r.bottom) {
			mFreeAspectRatio = r.right / (r.bottom * mSourceAspectRatio);
			mZoom = (double)r.bottom / mSourceHeight;
			mPanWidth = r.right;
			mPanHeight = r.bottom;
		}
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

void VDVideoWindow::SetZoom(double zoom, bool useWorkArea) {
	int w,h;
	if (mSourceWidth > 0 && mSourceHeight > 0 && mpDisplay && mpDisplay->GetMaxArea(w,h)) {
		double z1 = GetMaxZoomForArea(w,h,0);
		if (z1<zoom) zoom = z1;
	}
	mZoom = zoom;
	Resize(useWorkArea);
}

double VDVideoWindow::GetMaxZoomForArea(int w, int h, int border) {
	double frameAspect;

	w -= border*2;
	if (w <= 0)
		return 0;

	h -= border*2;
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

double VDVideoWindow::GetMaxZoomForArea(int w, int h) {
	if (mbAutoBorder && !mbFullscreen && mWorkArea.right && mSourceWidth > 0 && mSourceHeight > 0) {
		double z0 = GetMaxZoomForArea(w,h,0);
		int w0 = VDRoundToInt(EvalWidth(z0));
		int h0 = VDRoundToInt(mSourceHeight * z0);
		if ((w0>=mWorkArea.right-mWorkArea.left-4) && (h0>=mWorkArea.bottom-mWorkArea.top-4)) return z0;

		return GetMaxZoomForArea(w,h,4);

	} else {
		return GetMaxZoomForArea(w,h,mBorder);
	}
}

void VDVideoWindow::Move(int x, int y) {
	++mInhibitParamUpdateLocks;
	SetWindowPos(mhwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	--mInhibitParamUpdateLocks;
}

double VDVideoWindow::EvalWidth(double zoom) {
	if (mbUseSourcePAR) {
		double ratio = 1.0;
		if (mSourcePAR > 0)
			ratio = mSourcePAR;

		return mSourceHeight * mSourceAspectRatio * ratio * zoom;
	} else if (mAspectRatio < 0) {
		return mSourceHeight * mSourceAspectRatio * mFreeAspectRatio * zoom;
	} else {
		if (mbAspectIsFrameBased)
			return mSourceHeight * mAspectRatio * zoom;
		else
			return mSourceWidth * mAspectRatio * zoom;
	}
}

double VDVideoWindow::EvalWidth() {
	return EvalWidth(mZoom);
}

void VDVideoWindow::Resize(bool useWorkArea) {
	if (mSourceWidth > 0 && mSourceHeight > 0) {
		int w, h;
		w = VDRoundToInt(EvalWidth());
		h = VDRoundToInt(mSourceHeight * mZoom);

		if (w < 1)
			w = 1;
		if (h < 1)
			h = 1;

		mPanWidth = w;
		mPanHeight = h;

		if (mbFullscreen) {
			RECT r;
			GetClientRect(mhwnd,&r);
			if (w<r.right || h<r.bottom) EvalZoom();
		} else {

			int border = mBorder;

			if (mbAutoBorder && !mbFullscreen && mWorkArea.right && useWorkArea) {
				if ((w>=mWorkArea.right-mWorkArea.left-4) && (h>=mWorkArea.bottom-mWorkArea.top-4)) {
					border = 0;
				} else {
					border = 4;
				}
			}

			w += border*2;
			h += border*2;

			++mInhibitParamUpdateLocks;
			if (!useWorkArea)
				++mInhibitWorkArea;
			SetWindowPos(mhwnd, NULL, 0, 0, w, h, SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE);
			--mInhibitParamUpdateLocks;
			if (!useWorkArea)
				--mInhibitWorkArea;
		}

		ClipPan(mPanX,mPanY);
		SetChildPos();
	}
}

void VDVideoWindow::EvalZoom() {
	RECT r;
	GetClientRect(mhwnd,&r);
	mZoom = (double)r.bottom / mSourceHeight;

	int w = VDRoundToInt(EvalWidth());
	if (w<r.right) {
		mZoom *= double(r.right) / w;
		mPanWidth = r.right;
		mPanHeight = VDRoundToInt(mSourceHeight * mZoom);
	} else {
		mPanWidth = w;
		mPanHeight = r.bottom;
	}

	if (mAspectRatio < 0 && !mbUseSourcePAR && r.right && r.bottom) {
		mFreeAspectRatio = r.right / (r.bottom * mSourceAspectRatio);
		mZoom = (double)r.bottom / mSourceHeight;
		mPanWidth = r.right;
		mPanHeight = r.bottom;
	}
}

void VDVideoWindow::ClipPan(float& px, float& py) {
	RECT r;
	GetClientRect(mhwnd,&r);
	int x1 = r.right-r.left-mPanWidth;
	int y1 = r.bottom-r.top-mPanHeight;
	if (x1>0) x1 = 0;
	if (y1>0) y1 = 0;
	if (mPanCentering==kPanCenter) {
		int w2 = (r.right-r.left)/2;
		int h2 = (r.bottom-r.top)/2;
		int x = w2 - VDRoundToInt(mPanWidth*px);
		int y = h2 - VDRoundToInt(mPanHeight*py);
		if (mPanWidth>0) {
			if (x>0) px = float(w2)/mPanWidth;
			if (x<x1) px = float(w2-x1)/mPanWidth;
		} else px = 0.5;
			if (mPanHeight>0) {
			if (y>0) py = float(h2)/mPanHeight;
			if (y<y1) py = float(h2-y1)/mPanHeight;
		} else py = 0.5;
	}
	if (mPanCentering==kPanTopLeft) {
		int x = -VDRoundToInt(mPanWidth*px);
		int y = -VDRoundToInt(mPanHeight*py);
		if (mPanWidth>0) {
			if (x>0) px = 0;
			if (x<x1) px = float(-x1)/mPanWidth;
		} else px = 0;
		if (mPanHeight>0) {
			if (y>0) py = 0;
			if (y<y1) py = float(-y1)/mPanHeight;
		} else py = 0;
	}
}

void VDVideoWindow::SetChildPos(float dx, float dy) {
	if (mhwndChild) {
		float px = mPanX+dx;
		float py = mPanY+dy;
		ClipPan(px,py);
		RECT r;
		GetClientRect(mhwnd,&r);
		int x,y;
		if (mPanCentering==kPanCenter) {
			x = (r.right-r.left)/2 - VDRoundToInt(mPanWidth*px);
			y = (r.bottom-r.top)/2 - VDRoundToInt(mPanHeight*py);
		}
		if (mPanCentering==kPanTopLeft) {
			x = -VDRoundToInt(mPanWidth*px);
			y = -VDRoundToInt(mPanHeight*py);
		}
		SetWindowPos(mhwndChild, NULL, x, y, mPanWidth, mPanHeight, SWP_NOZORDER|SWP_NOCOPYBITS);

		if (mDrawMode)
			mDrawMode->SetDisplayPos(x,y,mPanWidth,mPanHeight);
	}
}

void VDVideoWindow::SetPanCentering(PanCenteringMode mode) {
	switch (mode) {
	case kPanCenter:
		mPanCentering = mode;
		mPanX = 0.5;
		mPanY = 0.5;
		break;
	case kPanTopLeft:
		mPanCentering = mode;
		mPanX = 0;
		mPanY = 0;
		break;
	}
	SetChildPos();
}

void VDVideoWindow::SetChild(HWND hwnd) {
	mhwndChild = hwnd;
}

void VDVideoWindow::SetMaxDisplayHost(HWND hwnd) {
	mhwndMax = hwnd;
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
		if (mbResizing && !mInhibitParamUpdateLocks) {
			WINDOWPOS *pwp = ((WINDOWPOS *)lParam);
			pwp->flags |= SWP_NOZORDER;

			if (mAspectRatio > 0 || mbUseSourcePAR) {
				double ar = mbUseSourcePAR ? mSourcePAR > 0 ? mSourcePAR : 1.0 : mAspectRatio;

				if (!mbAspectIsFrameBased)
					ar *= (double)mSourceWidth / (double)mSourceHeight;

				bool bXMajor = (pwp->cx-mBorder*2) > (pwp->cy-mBorder*2) * ar;

				if (mLastHitTest == HTBOTTOM)
					bXMajor = false;
				else if (mLastHitTest == HTRIGHT)
					bXMajor = true;

				if (bXMajor)
					pwp->cy = VDRoundToInt((pwp->cx-mBorder*2) / ar) + mBorder*2;
				else
					pwp->cx = VDRoundToInt((pwp->cy-mBorder*2) * ar) + mBorder*2;
			}
		}

		if (mbFullscreen) {
			WINDOWPOS *pwp = ((WINDOWPOS *)lParam);
			pwp->flags |= SWP_FRAMECHANGED;
		}

		if (!mInhibitWorkArea && !mbFullscreen && mWorkArea.right) {
			WINDOWPOS *pwp = ((WINDOWPOS *)lParam);
			POINT p0 = {0, 0};
			if (pwp->flags & SWP_NOMOVE) {
				MapWindowPoints(mhwnd,GetParent(mhwnd),&p0,1);
				p0.x -= mBorder;
				p0.y -= mBorder;
			} else {
				p0.x = pwp->x;
				p0.y = pwp->y;
			}
			if (pwp->flags & SWP_NOSIZE) {
				RECT r;
				GetWindowRect(mhwnd,&r);
				pwp->cx = r.right-r.left;
				pwp->cy = r.bottom-r.top;
			}
			int maxw = mWorkArea.right - p0.x;
			int maxh = mWorkArea.bottom - p0.y;
			if (pwp->cx>maxw) {
				pwp->cx = maxw;
				pwp->flags &= ~SWP_NOSIZE;
			}
			if (pwp->cy>maxh) {
				pwp->cy = maxh;
				pwp->flags &= ~SWP_NOSIZE;
			}

			if (mbAutoBorder) {
				if ((pwp->cx>=mWorkArea.right-mWorkArea.left-4) && (pwp->cy>=mWorkArea.bottom-mWorkArea.top-4)) {
					if(mBorder==4 || mHTBorder!=8){
						SetBorder(0,8);
						pwp->flags |= SWP_FRAMECHANGED;
					}
					pwp->cx = mWorkArea.right;
					pwp->cy = mWorkArea.bottom;
					pwp->flags &= ~SWP_NOSIZE;
				} else {
					if(mBorder==0){
						SetBorder(4);
						pwp->flags |= SWP_FRAMECHANGED;
					}
				}
			}
		}
		break;
	case WM_WINDOWPOSCHANGED:
		{
			const WINDOWPOS& wp = *(const WINDOWPOS *)lParam;
			if (mSourceHeight > 0 && !mInhibitParamUpdateLocks && !(wp.flags & SWP_NOSIZE)) {
				EvalZoom();
			}

			ClipPan(mPanX,mPanY);
			SetChildPos();
			SyncMonitorChange();

			if (!mbFullscreen) {
				NMHDR hdr;
				hdr.hwndFrom = mhwnd;
				hdr.idFrom = GetWindowLong(mhwnd, GWL_ID);
				hdr.code = VWN_RESIZED;
				SendMessage(GetParent(mhwnd), WM_NOTIFY, (WPARAM)hdr.idFrom, (LPARAM)&hdr);
			}
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

			if (!mbFullscreen && mWorkArea.right) {
				POINT p0 = {0, 0};
				MapWindowPoints(mhwnd,GetParent(mhwnd),&p0,1);
				p0.x -= mBorder;
				p0.y -= mBorder;
				int maxw = mWorkArea.right - p0.x;
				int maxh = mWorkArea.bottom - p0.y;
				mmi.ptMaxTrackSize.x = maxw;
				mmi.ptMaxTrackSize.y = maxh;
			}
		}
		return 0;

	case WM_LBUTTONDBLCLK:
		if (mhwndMax) {
			mPanMode = false;
			ReleaseCapture();
			ToggleFullscreen();
		}
		return TRUE;

	case WM_LBUTTONDOWN:
		{
			mPanMode = true;
			GetCursorPos(&mPanStart);
			SetCapture(mhwnd);
		}
		return TRUE;

	case WM_LBUTTONUP:
		{
			if (mPanMode) {
				POINT p1;
				GetCursorPos(&p1);
				float dx = float(p1.x-mPanStart.x)/mPanWidth;
				float dy = float(p1.y-mPanStart.y)/mPanHeight;
				mPanX -= dx;
				mPanY -= dy;
				ClipPan(mPanX,mPanY);
				mPanMode = false;
				ReleaseCapture();
				SetChildPos();
			}
		}
		return TRUE;

	case WM_MOUSEMOVE:
		{
			if (mPanMode) {
				POINT p1;
				GetCursorPos(&p1);
				float dx = float(p1.x-mPanStart.x)/mPanWidth;
				float dy = float(p1.y-mPanStart.y)/mPanHeight;
				SetChildPos(-dx,-dy);
			}
		}
		return TRUE;
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

		if (mBorder>4) {
			RECT r1=rc;
			RECT r2=rc;
			RECT r3=rc;
			RECT r4=rc;
			r1.right = mBorder-2;
			r2.left = r2.right -(mBorder-2);
			r3.bottom = mBorder-2;
			r4.top = r2.bottom -(mBorder-2);
			FillRect(hdc,&r1,(HBRUSH)(COLOR_3DFACE+1));
			FillRect(hdc,&r2,(HBRUSH)(COLOR_3DFACE+1));
			FillRect(hdc,&r3,(HBRUSH)(COLOR_3DFACE+1));
			FillRect(hdc,&r4,(HBRUSH)(COLOR_3DFACE+1));
		} else {
			DrawEdge(hdc, &rc, BDR_RAISEDOUTER|BDR_RAISEDINNER, BF_RECT);
		}

		rc.left		+= mBorder-2;
		rc.right	-= mBorder-2;
		rc.top		+= mBorder-2;
		rc.bottom	-= mBorder-2;
		DrawEdge(hdc, &rc, BDR_SUNKENOUTER|BDR_SUNKENINNER, BF_RECT);

		ReleaseDC(mhwnd, hdc);
	}
}

void VDVideoWindow::RecalcClientArea(RECT& rc) {
	rc.left += mBorder;
	rc.right -= mBorder;
	rc.top += mBorder;
	rc.bottom -= mBorder;
}

LRESULT VDVideoWindow::RecalcClientArea(NCCALCSIZE_PARAMS& params) {
	// Win32 docs don't say you need to do this, but you do.

	params.rgrc[0].left += mBorder;
	params.rgrc[0].right -= mBorder;
	params.rgrc[0].top += mBorder;
	params.rgrc[0].bottom -= mBorder;

	return 0;//WVR_ALIGNTOP|WVR_ALIGNLEFT;
}

LRESULT VDVideoWindow::HitTest(int x, int y) {
	POINT pt = { x, y };
	RECT rc;

	GetClientRect(mhwnd, &rc);
	ScreenToClient(mhwnd, &pt);

	int border = mHTBorder;
	if (border==-1) border = mBorder;

	if (border==0 || mBorder>4 || (pt.x >= border && pt.y >= border && pt.x < rc.right-border && pt.y < rc.bottom-border))
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
	case ID_DISPLAY_DEFAULT:
		if (mpDisplay)
			mpDisplay->SetDisplayMode(IVDVideoDisplay::kDisplayDefault);
		break;
	case ID_DISPLAY_COLOR:
		if (mpDisplay)
			mpDisplay->SetDisplayMode(IVDVideoDisplay::kDisplayColor);
		break;
	case ID_DISPLAY_ALPHA:
		if (mpDisplay)
			mpDisplay->SetDisplayMode(IVDVideoDisplay::kDisplayAlpha);
		break;
	case ID_DISPLAY_BLENDCHECKER:
		if (mpDisplay)
			mpDisplay->SetDisplayMode(IVDVideoDisplay::kDisplayBlendChecker);
		break;
	case ID_DISPLAY_BLEND0:
		if (mpDisplay)
			mpDisplay->SetDisplayMode(IVDVideoDisplay::kDisplayBlend0);
		break;
	case ID_DISPLAY_BLEND1:
		if (mpDisplay)
			mpDisplay->SetDisplayMode(IVDVideoDisplay::kDisplayBlend1);
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

	DWORD dwEnabled2 = MF_BYCOMMAND | MF_GRAYED;
	if (mpDisplay && !(g_prefs.fDisplay & Preferences::kDisplayDisableDX)) {
		if ((g_prefs.fDisplay & (Preferences::kDisplayEnableD3D))
			|| VDPreferencesIsDisplay3DEnabled())
			dwEnabled2 = MF_BYCOMMAND | MF_ENABLED;
	}
	EnableMenuItem(hmenu, ID_DISPLAY_DEFAULT, dwEnabled2);
	EnableMenuItem(hmenu, ID_DISPLAY_COLOR, dwEnabled2);
	EnableMenuItem(hmenu, ID_DISPLAY_ALPHA, dwEnabled2);
	EnableMenuItem(hmenu, ID_DISPLAY_BLENDCHECKER, dwEnabled2);
	EnableMenuItem(hmenu, ID_DISPLAY_BLEND0, dwEnabled2);
	EnableMenuItem(hmenu, ID_DISPLAY_BLEND1, dwEnabled2);

	if (mpDisplay) {
		IVDVideoDisplay::DisplayMode mode = mpDisplay->GetDisplayMode();

		CheckMenuItem(hmenu, ID_DISPLAY_DEFAULT, mode == IVDVideoDisplay::kDisplayDefault ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
		CheckMenuItem(hmenu, ID_DISPLAY_COLOR, mode == IVDVideoDisplay::kDisplayColor ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
		CheckMenuItem(hmenu, ID_DISPLAY_ALPHA, mode == IVDVideoDisplay::kDisplayAlpha ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
		CheckMenuItem(hmenu, ID_DISPLAY_BLENDCHECKER, mode == IVDVideoDisplay::kDisplayBlendChecker ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
		CheckMenuItem(hmenu, ID_DISPLAY_BLEND0, mode == IVDVideoDisplay::kDisplayBlend0 ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
		CheckMenuItem(hmenu, ID_DISPLAY_BLEND1, mode == IVDVideoDisplay::kDisplayBlend1 ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
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
