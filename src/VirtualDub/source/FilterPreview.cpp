//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2007 Avery Lee
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
#include <vd2/system/w32assist.h>
#include <vd2/VDDisplay/display.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/pixel.h>
#include <vd2/VDLib/Dialog.h>
#include "FilterPreview.h"
#include "FilterInstance.h"
#include "FilterFrameRequest.h"
#include "FilterFrameVideoSource.h"
#include "filters.h"
#include "PositionControl.h"
#include "ProgressDialog.h"
#include "project.h"
#include "VBitmap.h"
#include "command.h"
#include "VideoSource.h"
#include "VideoWindow.h"
#include "resource.h"
#include "oshelper.h"
#include "dub.h"
#include "projectui.h"

extern HINSTANCE	g_hInst;
extern VDProject *g_project;
extern vdrefptr<VDProjectUI> g_projectui;
extern IVDPositionControlCallback *VDGetPositionControlCallbackTEMP();
extern void SaveImage(HWND, VDPosition frame, VDPixmap* px);

int VDRenderSetVideoSourceInputFormat(IVDVideoSource *vsrc, VDPixmapFormatEx format);

namespace {
	static const UINT IDC_FILTDLG_POSITION = 500;

	static const UINT MYWM_REDRAW = WM_USER+100;
	static const UINT MYWM_RESTART = WM_USER+101;
	static const UINT MYWM_INVALIDATE = WM_USER+102;
	static const UINT TIMER_SHUTTLE = 2;
}

///////////////////////////////////////////////////////////////////////////////

class VDVideoFilterPreviewZoomPopup : public VDDialogFrameW32 {
public:
	VDVideoFilterPreviewZoomPopup();
	~VDVideoFilterPreviewZoomPopup();

	void Update(int x, int y, const uint32 pixels[7][7], const VDSample& ps);
	void UpdateText();

	VDZINT_PTR DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);

protected:
	bool OnLoaded();
	bool OnEraseBkgnd(HDC hdc);
	void OnPaint();

	RECT mBitmapRect;

	int x,y;
	VDSample ps;
	uint32	mBitmap[7][7];
	BITMAPINFO mBitmapInfo;
	HPEN black_pen;
	HPEN white_pen;
	bool draw_delayed;
};

VDVideoFilterPreviewZoomPopup::VDVideoFilterPreviewZoomPopup()
	: VDDialogFrameW32(IDD_FILTER_PREVIEW_ZOOM)
{
	mBitmapInfo.bmiHeader.biSize			= sizeof(BITMAPINFOHEADER);
	mBitmapInfo.bmiHeader.biWidth			= 7;
	mBitmapInfo.bmiHeader.biHeight			= 7;
	mBitmapInfo.bmiHeader.biPlanes			= 1;
	mBitmapInfo.bmiHeader.biCompression		= BI_RGB;
	mBitmapInfo.bmiHeader.biSizeImage		= sizeof mBitmap;
	mBitmapInfo.bmiHeader.biBitCount		= 32;
	mBitmapInfo.bmiHeader.biXPelsPerMeter	= 0;
	mBitmapInfo.bmiHeader.biYPelsPerMeter	= 0;
	mBitmapInfo.bmiHeader.biClrUsed			= 0;
	mBitmapInfo.bmiHeader.biClrImportant	= 0;
	black_pen = CreatePen(PS_SOLID,0,RGB(0,0,0));
	white_pen = CreatePen(PS_SOLID,0,RGB(255,255,255));
}

VDVideoFilterPreviewZoomPopup::~VDVideoFilterPreviewZoomPopup() {
	DeleteObject(black_pen);
	DeleteObject(white_pen);
}

void VDVideoFilterPreviewZoomPopup::Update(int x, int y, const uint32 pixels[7][7], const VDSample& ps) {
	memcpy(mBitmap, pixels, sizeof mBitmap);
	this->x = x;
	this->y = y;
	this->ps = ps;
	if(!draw_delayed) {
		SetTimer(mhdlg,1,20,0);
		draw_delayed = true;
	}
}

void VDVideoFilterPreviewZoomPopup::UpdateText() {
	uint32 rgb = mBitmap[3][3] & 0xffffff;
	SetControlTextF(IDC_POSITION, L"%d,%d", x, y);
	SetControlTextF(IDC_COLOR, L"#%06X", rgb);
	SetControlTextF(IDC_RED,   L"R: %1.3g", ps.r);
	SetControlTextF(IDC_GREEN, L"G: %1.3g", ps.g);
	SetControlTextF(IDC_BLUE,  L"B: %1.3g", ps.b);
	if(ps.sa!=-1) {
		ShowControl(IDC_ALPHA, true);
		SetControlTextF(IDC_ALPHA,  L"A: %1.3g", ps.a);
		ShowControl(IDC_ALPHA2, true);
		SetControlTextF(IDC_ALPHA2,  L"A: %X", ps.sa);
	} else {
		ShowControl(IDC_ALPHA, false);
		ShowControl(IDC_ALPHA2, false);
	}
	if(ps.sr!=-1) {
		ShowControl(IDC_RED2, true);
		SetControlTextF(IDC_RED2,  L"R: %X", ps.sr);
	} else if(ps.sy!=-1) {
		ShowControl(IDC_RED2, true);
		SetControlTextF(IDC_RED2,  L"Y: %X", ps.sy);
	} else {
		ShowControl(IDC_RED2, false);
	}
	if(ps.sg!=-1) {
		ShowControl(IDC_GREEN2, true);
		SetControlTextF(IDC_GREEN2,  L"G: %X", ps.sg);
	} else if(ps.scb!=-1) {
		ShowControl(IDC_GREEN2, true);
		SetControlTextF(IDC_GREEN2,  L"Cb: %X", ps.scb);
	} else {
		ShowControl(IDC_GREEN2, false);
	}
	if (ps.sb!=-1) {
		ShowControl(IDC_BLUE2, true);
		SetControlTextF(IDC_BLUE2,  L"B: %X", ps.sb);
	} else if(ps.scr!=-1) {
		ShowControl(IDC_BLUE2, true);
		SetControlTextF(IDC_BLUE2,  L"Cr: %X", ps.scr);
	} else {
		ShowControl(IDC_BLUE2, false);
	}
}

VDZINT_PTR VDVideoFilterPreviewZoomPopup::DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam) {
	switch(msg) {
		case WM_NCHITTEST:
			SetWindowLongPtr(mhdlg, DWLP_MSGRESULT, HTTRANSPARENT);
			return TRUE;

		case WM_PAINT:
			OnPaint();
			return TRUE;

		case WM_TIMER:
			if(wParam==1) {
				KillTimer(mhdlg,wParam);
				draw_delayed = false;
				InvalidateRect(mhdlg,0,false);
				UpdateText();
			}
			break;

		case WM_ERASEBKGND:
			SetWindowLongPtr(mhdlg, DWLP_MSGRESULT, OnEraseBkgnd((HDC)wParam));
			return TRUE;
	}

	return VDDialogFrameW32::DlgProc(msg, wParam, lParam);
}

bool VDVideoFilterPreviewZoomPopup::OnLoaded() {
	HWND hwndImage = GetDlgItem(mhdlg, IDC_IMAGE);

	memset(&mBitmapRect, 0, sizeof mBitmapRect);
	draw_delayed = false;

	if (hwndImage) {
		GetWindowRect(hwndImage, &mBitmapRect);
		MapWindowPoints(NULL, mhdlg, (LPPOINT)&mBitmapRect, 2);
	}

	VDDialogFrameW32::OnLoaded();
	return true;
}

bool VDVideoFilterPreviewZoomPopup::OnEraseBkgnd(HDC hdc) {
	RECT r;

	int saveHandle = SaveDC(hdc);
	if (saveHandle) {
		ExcludeClipRect(hdc, mBitmapRect.left, mBitmapRect.top, mBitmapRect.right, mBitmapRect.bottom);

		GetClientRect(mhdlg, &r);
		//FillRect(hdc, &r, (HBRUSH)GetClassLongPtr(mhdlg, GCLP_HBRBACKGROUND));
		FillRect(hdc, &r, (HBRUSH) (COLOR_BTNFACE+1));
		RestoreDC(hdc, saveHandle);
		return true;
	}

	return false;
}

void VDVideoFilterPreviewZoomPopup::OnPaint() {
	PAINTSTRUCT ps;

	HDC hdc = BeginPaint(mhdlg, &ps);
	if (hdc) {
		StretchDIBits(hdc,
				mBitmapRect.left,
				mBitmapRect.top,
				mBitmapRect.right - mBitmapRect.left,
				mBitmapRect.bottom - mBitmapRect.top,
				0,
				0,
				7,
				7,
				mBitmap, &mBitmapInfo, DIB_RGB_COLORS, SRCCOPY);

		HPEN pen = black_pen;
		uint32 px = mBitmap[3][3] & 0xffffff;
		int pr = px >> 16;
		int pg = (px >> 8) & 0xff;
		int pb = px & 0xff;
		if(pr+pg+pb<300) pen = white_pen;

		HPEN pen0 = (HPEN)SelectObject(hdc,pen);
		int cx = (mBitmapRect.right + mBitmapRect.left)/2;
		int cy = (mBitmapRect.bottom + mBitmapRect.top)/2;

		MoveToEx(hdc,cx-5,cy,0);
		LineTo(hdc,cx+6,cy);
		MoveToEx(hdc,cx,cy-5,0);
		LineTo(hdc,cx,cy+6);

		SelectObject(hdc,pen0);
		EndPaint(mhdlg, &ps);
	}
}

///////////////////////////////////////////////////////////////////////////////

class FilterPreview : public IVDXFilterPreview2, public IFilterModPreview, public vdrefcounted<IVDVideoFilterPreviewDialog> {
	FilterPreview(const FilterPreview&);
	FilterPreview& operator=(const FilterPreview&);
public:
	FilterPreview(FilterSystem *sys, VDFilterChainDesc *desc, FilterInstance *);
	~FilterPreview();

	IVDXFilterPreview2 *AsIVDXFilterPreview2() { return this; }
	IFilterModPreview *AsIFilterModPreview() { return this; }
	void SetInitialTime(VDTime t);
	void SetFilterList(HWND w){ mhwndFilterList = w; }
	void RedoFrame2() { OnVideoRedraw(); }

	void SetButtonCallback(VDXFilterPreviewButtonCallback, void *);
	void SetSampleCallback(VDXFilterPreviewSampleCallback, void *);

	bool isPreviewEnabled();
	bool IsPreviewDisplayed();
	void InitButton(VDXHWND);
	void Toggle(VDXHWND);
	void Display(VDXHWND, bool);
	void RedoFrame();
	void UndoSystem();
	void RedoSystem();
	void Close();
	void CopyOutputFrameToClipboard();
	void SaveImageAsk();
	bool SampleCurrentFrame();
	long SampleFrames();
	long SampleFrames(IFilterModPreviewSample*);
	int64 FMSetPosition(int64 pos);
	void FMSetPositionCallback(FilterModPreviewPositionCallback, void *);
	void FMSetZoomCallback(FilterModPreviewZoomCallback, void *);
	int FMTranslateAccelerator(MSG* msg){ return TranslateAccelerator(mhdlg, mDlgNode.mhAccel, msg); }
	HWND GetHwnd(){ return mhdlg; }
	int TranslateAcceleratorMessage(MSG* msg){ return TranslateAccelerator(mhdlg, mDlgNode.mhAccel, msg); }
	void StartSceneShuttleReverse();
	void StartSceneShuttleForward();
	void SceneShuttleStop();
	void SceneShuttleStep();
	void MoveToPreviousRange();
	void MoveToNextRange();

private:
	static INT_PTR CALLBACK StaticDlgProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam);
	BOOL DlgProc(UINT message, WPARAM wParam, LPARAM lParam);

	void OnInit();
	void OnResize();
	void OnPaint();
	void OnVideoResize(bool bInitial);
	void OnVideoRedraw();
	bool OnCommand(UINT);

	VDPosition FetchFrame();
	VDPosition FetchFrame(VDPosition);

	void UpdateButton();
	void RedrawFrame();
	void ExitZoomMode();

	HWND		mhdlg;
	HWND		mhwndButton;
	HWND		mhwndParent;
	HWND    mhwndPosHost;
	HWND		mhwndPosition;
	HWND		mhwndVideoWindow;
	HWND		mhwndDisplay;
	HWND		mhwndFilterList;

	wchar_t		mButtonAccelerator;

	int			mWidth;
	int			mHeight;
	int			mDisplayX;
	int			mDisplayY;
	int			mDisplayW;
	int			mDisplayH;

	VDTime		mInitialTimeUS;
	sint64		mInitialFrame;
	sint64		mLastOutputFrame;
	sint64		mLastTimelineFrame;
	sint64		mLastTimelineTimeMS;
	int		mSceneShuttleMode;

	IVDPositionControl	*mpPosition;
	IVDVideoDisplay *mpDisplay;
	IVDVideoWindow *mpVideoWindow;
	FilterSystem *mpFiltSys;
	VDFilterChainDesc *mpFilterChainDesc;
	FilterInstance *mpThisFilter;
	VDTimeline	*mpTimeline;
	VDFraction	mTimelineRate;

	VDXFilterPreviewButtonCallback	mpButtonCallback;
	void							*mpvButtonCBData;
	VDXFilterPreviewSampleCallback	mpSampleCallback;
	void							*mpvSampleCBData;
	FilterModPreviewPositionCallback	mpPositionCallback;
	void							*mpvPositionCBData;
	FilterModPreviewZoomCallback	mpZoomCallback;
	void							*mpvZoomCBData;
	PreviewZoomInfo   zoom_info;

	MyError		mFailureReason;

	VDVideoFilterPreviewZoomPopup	mZoomPopup;
	HCURSOR mode_cursor;
	HCURSOR cross_cursor;

	vdrefptr<VDFilterFrameVideoSource>	mpVideoFrameSource;
	vdrefptr<VDFilterFrameBuffer>		mpVideoFrameBuffer;

	ModelessDlgNode		mDlgNode;
};

bool VDCreateVideoFilterPreviewDialog(FilterSystem *sys, VDFilterChainDesc *desc, FilterInstance *finst, IVDVideoFilterPreviewDialog **pp) {
	IVDVideoFilterPreviewDialog *p = new_nothrow FilterPreview(sys, desc, finst);
	if (!p)
		return false;
	p->AddRef();
	*pp = p;
	return true;
}

FilterPreview::FilterPreview(FilterSystem *pFiltSys, VDFilterChainDesc *pFilterChainDesc, FilterInstance *pfiThisFilter)
	: mhdlg(NULL)
	, mhwndButton(NULL)
	, mhwndParent(NULL)
	, mhwndPosHost(NULL)
	, mhwndPosition(NULL)
	, mhwndVideoWindow(NULL)
	, mhwndDisplay(NULL)
	, mhwndFilterList(NULL)
	, mButtonAccelerator(0)
	, mWidth(0)
	, mHeight(0)
	, mDisplayX(0)
	, mDisplayY(0)
	, mDisplayW(0)
	, mDisplayH(0)
	, mInitialTimeUS(-1)
	, mInitialFrame(-1)
	, mLastOutputFrame(0)
	, mLastTimelineFrame(0)
	, mLastTimelineTimeMS(0)
	, mSceneShuttleMode(0)
	, mpPosition(NULL)
	, mpDisplay(NULL)
	, mpVideoWindow(NULL)
	, mpFiltSys(pFiltSys)
	, mpFilterChainDesc(pFilterChainDesc)
	, mpThisFilter(pfiThisFilter)
	, mpTimeline(0)
	, mpButtonCallback(NULL)
	, mpSampleCallback(NULL)
	, mpPositionCallback(NULL)
	, mpZoomCallback(NULL)
{
	mode_cursor = 0;
	cross_cursor = LoadCursor(0,IDC_CROSS);
}

FilterPreview::~FilterPreview() {
	if (mhdlg)
		DestroyWindow(mhdlg);
}

void FilterPreview::SetInitialTime(VDTime t) {
	mInitialTimeUS = t;
	mInitialFrame = -1;
}

void FilterPreview::ExitZoomMode() {
	if(mZoomPopup.IsCreated()){
		mZoomPopup.Destroy();
		mode_cursor = 0;
		if (mpZoomCallback) {
			zoom_info.flags = zoom_info.popup_cancel;
			mpZoomCallback(zoom_info,mpvZoomCBData);
		}
	}
}

INT_PTR CALLBACK FilterPreview::StaticDlgProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam) {
	FilterPreview *fpd = (FilterPreview *)GetWindowLongPtr(hdlg, DWLP_USER);

	if (message == WM_INITDIALOG) {
		SetWindowLongPtr(hdlg, DWLP_USER, lParam);
		fpd = (FilterPreview *)lParam;
		fpd->mhdlg = hdlg;
	}

	return fpd && fpd->DlgProc(message, wParam, lParam);
}

BOOL FilterPreview::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch(message) {
	case WM_INITDIALOG:
		OnInit();
		OnVideoResize(true);
		VDSetDialogDefaultIcons(mhdlg);
		return TRUE;

	case WM_DESTROY:
		if (mpDisplay) {
			mpDisplay->Destroy();
			mpDisplay = NULL;
			mhwndDisplay = NULL;
		}

		mhwndVideoWindow = NULL;

		mDlgNode.Remove();

		DestroyWindow(mhwndPosHost);
		mhwndPosHost = NULL;

		g_projectui->DisplayPreview(false);
		return TRUE;

	case WM_SIZE:
		OnResize();
		return TRUE;

	case WM_PAINT:
		OnPaint();
		return TRUE;

	case WM_LBUTTONDOWN:
		if(mZoomPopup.IsCreated()) {
			if (mpZoomCallback) {
				zoom_info.flags = zoom_info.popup_click;
				mpZoomCallback(zoom_info,mpvZoomCBData);
			}
			return TRUE;
		}
		break;

	case WM_MOUSEMOVE:
		{
			POINT pt = {(SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam)};

			int xoffset = pt.x - mDisplayX;
			int yoffset = pt.y - mDisplayY;

			int DisplayW,DisplayH;
			mpVideoWindow->GetFrameSize(DisplayW,DisplayH);

			if ((wParam & MK_SHIFT) && mpFiltSys->isRunning() && mpVideoFrameBuffer && xoffset < DisplayW && yoffset < DisplayH) {
				VDPixmap output = VDPixmapFromLayout(mpFiltSys->GetOutputLayout(), (void *)mpVideoFrameBuffer->LockRead());
				output.info = mpVideoFrameBuffer->info;
				uint32 pixels[7][7];
				int x = VDFloorToInt((xoffset + 0.5) * (double)output.w / (double)DisplayW);
				int y = VDFloorToInt((yoffset + 0.5) * (double)output.h / (double)DisplayH);

				for(int i=0; i<7; ++i) {
					for(int j=0; j<7; ++j) {
						pixels[i][j] = 0xFFFFFF & VDPixmapSample(output, x+j-3, y+3-i);
					}
				}

				VDSample ps;
				VDPixmapSample(output,x,y,ps);

				mpVideoFrameBuffer->Unlock();

				zoom_info.x = x;
				zoom_info.y = y;
				zoom_info.r = ps.r/255;
				zoom_info.g = ps.g/255;
				zoom_info.b = ps.b/255;
				zoom_info.a = ps.a/255;

				POINT pts = pt;
				ClientToScreen(mhdlg, &pts);
				mZoomPopup.Create((VDGUIHandle)mhdlg);
				HMONITOR monitor = MonitorFromPoint(pts,MONITOR_DEFAULTTONEAREST);
				MONITORINFO minfo = {sizeof(MONITORINFO)};
				GetMonitorInfo(monitor,&minfo);
				RECT r0;
				GetWindowRect(mZoomPopup.GetWindowHandle(),&r0);
				int zw = r0.right-r0.left;
				int zh = r0.bottom-r0.top;
				if(pts.x+32+zw>minfo.rcWork.right)
					pts.x -= 32+zw;
				else
					pts.x += 32;
				if(pts.y+32+zh>minfo.rcWork.bottom)
					pts.y -= 32+zh;
				else
					pts.y += 32;

				SetWindowPos(mZoomPopup.GetWindowHandle(), NULL, pts.x, pts.y, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
				mZoomPopup.Update(x, y, pixels, ps);
				ShowWindow(mZoomPopup.GetWindowHandle(), SW_SHOWNOACTIVATE);

				TRACKMOUSEEVENT tme={sizeof(TRACKMOUSEEVENT), TME_LEAVE, mhdlg, 0};
				TrackMouseEvent(&tme);
				mode_cursor = cross_cursor;

				if(mpZoomCallback) {
					zoom_info.flags = zoom_info.popup_update;
					mpZoomCallback(zoom_info,mpvZoomCBData);
				}

			} else {
				ExitZoomMode();
			}
		}
		return 0;

	case WM_MOUSEWHEEL:
		if (mhwndPosition)
			return SendMessage(mhwndPosition, WM_MOUSEWHEEL, wParam, lParam);
		break;

	case WM_MOUSELEAVE:
		ExitZoomMode();
		break;

	case WM_KEYUP:
		if (wParam == VK_SHIFT)
			ExitZoomMode();
		break;

	case WM_SETCURSOR:
		if(mode_cursor){
			SetCursor(mode_cursor);
			return true;
		}
		break;

	case WM_NOTIFY:
		{
			const NMHDR& hdr = *(const NMHDR *)lParam;
			if (hdr.idFrom == IDC_FILTDLG_POSITION) {
				OnVideoRedraw();
			} else if (hdr.hwndFrom == mhwndVideoWindow) {
				switch(hdr.code) {
					case VWN_RESIZED:
						{
							RECT r;
							GetWindowRect(mhwndVideoWindow, &r);

							AdjustWindowRectEx(&r, GetWindowLong(mhdlg, GWL_STYLE), FALSE, GetWindowLong(mhdlg, GWL_EXSTYLE));
							SetWindowPos(mhdlg, NULL, 0, 0, r.right-r.left, r.bottom-r.top, SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE);
						}
						break;
				}
			}
		}
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_FILTDLG_POSITION) {
			VDTranslatePositionCommand(mhdlg, wParam, lParam);
			return TRUE;
		}
		return OnCommand(LOWORD(wParam));

	case WM_CONTEXTMENU:
		{
			POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
			RECT r;

			if (::GetWindowRect(mhwndVideoWindow, &r) && ::PtInRect(&r, pt)) {
				SendMessage(mhwndVideoWindow, WM_CONTEXTMENU, wParam, lParam);
			}
		}
		break;

	case MYWM_REDRAW:
		OnVideoRedraw();
		return TRUE;

	case MYWM_RESTART:
		OnVideoResize(false);
		return TRUE;

	case MYWM_INVALIDATE:
		mpFiltSys->InvalidateCachedFrames(mpThisFilter);
		OnVideoRedraw();
		return TRUE;

	case WM_TIMER:
		if (wParam==TIMER_SHUTTLE && mSceneShuttleMode)
			SceneShuttleStep();
		return TRUE;
	}

	return FALSE;
}

LRESULT WINAPI preview_pos_host_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if(msg==WM_NCCREATE || msg==WM_CREATE){
		CREATESTRUCT* create = (CREATESTRUCT*)lparam;
		SetWindowLongPtr(wnd,GWLP_USERDATA,(LPARAM)create->lpCreateParams);
	}

	FilterPreview* owner = (FilterPreview*)GetWindowLongPtr(wnd,GWLP_USERDATA);

	switch(msg){
	case WM_NOTIFY:
	case WM_COMMAND:
		return SendMessage(owner->GetHwnd(),msg,wparam,lparam);

	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_CHAR:
		{
			// currently this is not used because active window is forced to preview anyway
			MSG m = {0};
			m.hwnd = owner->GetHwnd();
			m.message = msg;
			m.wParam = wparam;
			m.lParam = lparam;
			m.time = GetMessageTime();
			return owner->TranslateAcceleratorMessage(&m)!=0;
		}

	case WM_MOUSEACTIVATE:
		SetActiveWindow(owner->GetHwnd());
		return MA_NOACTIVATE;

	case WM_ACTIVATE:
		if(LOWORD(wparam)==WA_ACTIVE || LOWORD(wparam)==WA_CLICKACTIVE && lparam) SetActiveWindow((HWND)lparam);
		return 0;
	}

	return DefWindowProcW(wnd,msg,wparam,lparam);
}

void FilterPreview::OnInit() {
	mpTimeline = &g_project->GetTimeline();
	mTimelineRate = g_project->GetTimelineFrameRate();

	mWidth = 0;
	mHeight = 0;

	int host_style_ex = WS_EX_NOACTIVATE;
	int host_style = WS_POPUP|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|WS_VISIBLE;
	WNDCLASSW cls={CS_OWNDC,NULL,0,0,GetModuleHandleW(0),0,0,0,0,L"preview_pos_host"};
	cls.lpfnWndProc=preview_pos_host_proc;
	RegisterClassW(&cls);
	mhwndPosHost = CreateWindowExW(host_style_ex,cls.lpszClassName,0,host_style,0,0,0,0,mhdlg,0,cls.hInstance,this);
	RECT r1;
	GetClientRect(g_projectui->GetHwnd(),&r1);
	POINT p1 = {0,r1.bottom-64};
	MapWindowPoints(g_projectui->GetHwnd(),0,&p1,1);
	SetWindowPos(mhwndPosHost,mhdlg,p1.x,p1.y,r1.right,64,SWP_NOACTIVATE);
	EnableWindow(mhwndPosHost,true);

	mhwndPosition = CreateWindow(POSITIONCONTROLCLASS, NULL, WS_CHILD|WS_VISIBLE, 0, 0, 0, 64, mhwndPosHost, (HMENU)IDC_FILTDLG_POSITION, g_hInst, NULL);
	mpPosition = VDGetIPositionControl((VDGUIHandle)mhwndPosition);
	SetWindowPos(mhwndPosition, NULL, 0, 0, r1.right, 64, SWP_NOZORDER|SWP_NOACTIVATE);

	mpPosition->SetRange(0, mpTimeline->GetLength());
	mpPosition->SetFrameTypeCallback(VDGetPositionControlCallbackTEMP());

	VDRenderSetVideoSourceInputFormat(inputVideo, g_dubOpts.video.mInputFormat);
	inputVideo->streamRestart();

	mhwndVideoWindow = CreateWindow(VIDEOWINDOWCLASS, NULL, WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN, 0, 0, 0, 0, mhdlg, (HMENU)100, g_hInst, NULL);
	mhwndDisplay = (HWND)VDCreateDisplayWindowW32(0, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0, 0, 0, 0, (VDGUIHandle)mhwndVideoWindow);
	if (mhwndDisplay)
		mpDisplay = VDGetIVideoDisplay((VDGUIHandle)mhwndDisplay);
	EnableWindow(mhwndDisplay, FALSE);

	mpVideoWindow = VDGetIVideoWindow(mhwndVideoWindow);
	mpVideoWindow->SetChild(mhwndDisplay);
	mpVideoWindow->SetDisplay(mpDisplay);
	mpVideoWindow->SetMouseTransparent(true);
	mpVideoWindow->SetBorderless(true);

	mDlgNode.hdlg = mhdlg;
	mDlgNode.mhAccel = g_projectui->GetAccelPreview();
	guiAddModelessDialog(&mDlgNode);
}

void FilterPreview::OnResize() {
	RECT r;

	GetClientRect(mhdlg, &r);

	mDisplayX	= 0;
	mDisplayY	= 0;
	mDisplayW	= r.right;
	mDisplayH	= r.bottom;

	if (mDisplayW < 0)
		mDisplayW = 0;

	if (mDisplayH < 0)
		mDisplayH = 0;

	//SetWindowPos(mhwndPosition, NULL, 0, r.bottom - 64, r.right, 64, SWP_NOZORDER|SWP_NOACTIVATE);
	SetWindowPos(mhwndVideoWindow, NULL, mDisplayX, mDisplayY, mDisplayW, mDisplayH, SWP_NOZORDER|SWP_NOACTIVATE);

	InvalidateRect(mhdlg, NULL, TRUE);
}

void FilterPreview::OnPaint() {
	PAINTSTRUCT ps;

	HDC hdc = BeginPaint(mhdlg, &ps);

	if (!hdc)
		return;

	if (mpFiltSys->isRunning()) {
		RECT r;

		GetClientRect(mhdlg, &r);
	} else {
		RECT r;

		GetWindowRect(mhwndDisplay, &r);
		MapWindowPoints(NULL, mhdlg, (LPPOINT)&r, 2);

		FillRect(hdc, &r, (HBRUSH)(COLOR_3DFACE+1));
		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, 0);

		HGDIOBJ hgoFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
		char buf[1024];
		const char *s = mFailureReason.gets();
		_snprintf(buf, sizeof buf, "Unable to start filters:\n%s", s?s:"(unknown)");
		buf[1023] = 0;

		RECT r2 = r;
		DrawText(hdc, buf, -1, &r2, DT_CENTER|DT_WORDBREAK|DT_NOPREFIX|DT_CALCRECT);

		int text_h = r2.bottom - r2.top;
		int space_h = r.bottom - r.top;
		if (text_h < space_h)
			r.top += (space_h - text_h) >> 1;

		DrawText(hdc, buf, -1, &r, DT_CENTER|DT_WORDBREAK|DT_NOPREFIX);
		SelectObject(hdc, hgoFont);
	}

	EndPaint(mhdlg, &ps);
}

void FilterPreview::OnVideoResize(bool bInitial) {
	RECT r;
	int w = 320;
	int h = 240;
	bool fResize;

	int oldw = mWidth;
	int oldh = mHeight;

	mWidth = 320;
	mHeight = 240;

	try {
		IVDStreamSource *pVSS = inputVideo->asStream();
		const VDPixmap& px = inputVideo->getTargetFormat();
		VDFraction srcRate = pVSS->getRate();

		if (g_dubOpts.video.mFrameRateAdjustLo > 0)
			srcRate.Assign(g_dubOpts.video.mFrameRateAdjustHi, g_dubOpts.video.mFrameRateAdjustLo);

		sint64 len = pVSS->getLength();
		const VDFraction& srcPAR = inputVideo->getPixelAspectRatio();

		/*mpFiltSys->prepareLinearChain(
				mpFilterChainDesc,
				px.w,
				px.h,
				pxsrc,
				srcRate,
				pVSS->getLength(),
				srcPAR);*/

		mpVideoFrameSource = new VDFilterFrameVideoSource;
		mpVideoFrameSource->Init(inputVideo, mpFiltSys->GetInputLayout());

		mpFiltSys->initLinearChain(
				NULL,
				VDXFilterStateInfo::kStatePreview,
				mpFilterChainDesc,
				mpVideoFrameSource,
				px.w,
				px.h,
				px,
				px.palette,
				srcRate,
				len,
				srcPAR);

		mpFiltSys->ReadyFilters();

		const VDPixmapLayout& output = mpFiltSys->GetOutputLayout();
		w = output.w;
		h = output.h;
		mWidth = w;
		mHeight = h;

		mpVideoWindow->SetSourceSize(w, h);
		mpVideoWindow->SetSourcePAR(mpFiltSys->GetOutputPixelAspect());

		if (mpFiltSys->GetOutputFrameRate() == srcRate)
			mpPosition->SetRange(0, mpTimeline->GetLength());
		else
			mpPosition->SetRange(0, mpFiltSys->GetOutputFrameCount());

		VDPosition sel_start = g_project->GetSelectionStartFrame();
		VDPosition sel_end = g_project->GetSelectionEndFrame();
		mpPosition->SetSelection(sel_start, sel_end);
		mpPosition->SetTimeline(*mpTimeline);

		if (mInitialTimeUS >= 0) {
			const VDFraction outputRate(mpFiltSys->GetOutputFrameRate());
			mpPosition->SetPosition(VDRoundToInt64(outputRate.asDouble() * (double)mInitialTimeUS * (1.0 / 1000000.0)));
		} else if (mInitialFrame >=0) {
			mpPosition->SetPosition(mInitialFrame);
		}

	} catch(const MyError& e) {
		mpDisplay->Reset();
		ShowWindow(mhwndVideoWindow, SW_HIDE);
		mFailureReason.assign(e);
		InvalidateRect(mhdlg, NULL, TRUE);
	}

	fResize = oldw != w || oldh != h;

	// if necessary, resize window

	if (fResize) {
		r.left = r.top = 0;
		r.right = w;
		r.bottom = h;

		AdjustWindowRect(&r, GetWindowLong(mhdlg, GWL_STYLE), FALSE);

		if (bInitial) {
			RECT rParent;
			UINT uiFlags = SWP_NOZORDER|SWP_NOACTIVATE;

			GetWindowRect(mhwndParent, &rParent);

			if (rParent.right + 32 >= GetSystemMetrics(SM_CXSCREEN))
				uiFlags |= SWP_NOMOVE;

			SetWindowPos(mhdlg, NULL,
					rParent.right + 16,
					rParent.top,
					r.right-r.left, r.bottom-r.top,
					uiFlags);
		} else {
			mpVideoWindow->Resize();
			//SetWindowPos(mhdlg, NULL, 0, 0, r.right-r.left, r.bottom-r.top, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
		}
	}

	OnVideoRedraw();
}

void FilterPreview::OnVideoRedraw() {
	if (!mpFiltSys->isRunning())
		return;

	try {
		bool success = false;

		FetchFrame();

		if (mpVideoFrameBuffer) {
			mpVideoFrameBuffer->Unlock();
			mpVideoFrameBuffer = NULL;
		}

		for(VDFilterChainDesc::Entries::const_iterator it(mpFilterChainDesc->mEntries.begin()), itEnd(mpFilterChainDesc->mEntries.end());
			it != itEnd;
			++it)
		{
			VDFilterChainEntry *ent = *it;
			FilterInstance *fa = ent->mpInstance;
			if (!fa->IsEnabled())
				continue;

			fa->view = ent->mpView;
		}

		vdrefptr<IVDFilterFrameClientRequest> req;
		if (mpFiltSys->RequestFrame(mLastOutputFrame, 0, ~req)) {

			while(!req->IsCompleted()) {
				if (mpFiltSys->Run(NULL, false) == FilterSystem::kRunResult_Running)
					continue;

				switch(mpVideoFrameSource->RunRequests(NULL)) {
					case IVDFilterFrameSource::kRunResult_Running:
					case IVDFilterFrameSource::kRunResult_IdleWasActive:
					case IVDFilterFrameSource::kRunResult_BlockedWasActive:
						continue;
				}

				mpFiltSys->Block();
			}

			success = req->IsSuccessful();
		}

		for(VDFilterChainDesc::Entries::const_iterator it(mpFilterChainDesc->mEntries.begin()), itEnd(mpFilterChainDesc->mEntries.end());
			it != itEnd;
			++it)
		{
			VDFilterChainEntry *ent = *it;
			FilterInstance *fa = ent->mpInstance;

			if (!fa->view) 
				continue;

			vdrefptr<IVDFilterFrameClientRequest> req2;
			if (fa->CreateRequest(mLastOutputFrame, false, 0, ~req2)) {
				while(!req2->IsCompleted()) {
					if (mpFiltSys->Run(NULL, false) == FilterSystem::kRunResult_Running)
						continue;

					switch(mpVideoFrameSource->RunRequests(NULL)) {
						case IVDFilterFrameSource::kRunResult_Running:
						case IVDFilterFrameSource::kRunResult_IdleWasActive:
						case IVDFilterFrameSource::kRunResult_BlockedWasActive:
							continue;
					}

					mpFiltSys->Block();
				}
			}
		}

		if (mpDisplay) {
			ShowWindow(mhwndVideoWindow, SW_SHOW);

			if (success) {
				mpVideoFrameBuffer = req->GetResultBuffer();
				const void *p = mpVideoFrameBuffer->LockRead();

				const VDPixmapLayout& layout = mpFiltSys->GetOutputLayout();
				VDPixmap px = VDPixmapFromLayout(layout, (void *)p);
				px.info = mpVideoFrameBuffer->info;

				mpDisplay->SetSourcePersistent(false, px);
			} else {
				VDFilterFrameRequestError *err = req->GetError();

				if (err)
					mpDisplay->SetSourceMessage(VDTextAToW(err->mError.c_str()).c_str());
				else
					mpDisplay->SetSourceSolidColor(VDSwizzleU32(GetSysColor(COLOR_3DFACE)) >> 8);
			}

			mpDisplay->Update(IVDVideoDisplay::kAllFields);
			mpPosition->SetPosition(mpPosition->GetPosition());
			RedrawWindow(mhwndPosition,0,0,RDW_UPDATENOW);
		}
	} catch(const MyError& e) {
		mpDisplay->Reset();
		ShowWindow(mhwndVideoWindow, SW_HIDE);
		mFailureReason.assign(e);
		InvalidateRect(mhdlg, NULL, TRUE);
		UndoSystem();
	}
}

bool FilterPreview::OnCommand(UINT cmd) {
	switch(cmd) {
	case IDCANCEL:
		if (mpButtonCallback)
			mpButtonCallback(false, mpvButtonCBData);

		SetActiveWindow(mhwndParent);
		DestroyWindow(mhdlg);
		mhdlg = NULL;
		UndoSystem();

		UpdateButton();
		return true;

	case ID_EDIT_JUMPTO:
		{
			SceneShuttleStop();
			extern VDPosition VDDisplayJumpToPositionDialog(VDGUIHandle hParent, VDPosition currentFrame, IVDVideoSource *pVS, const VDFraction& realRate);

			VDPosition pos = VDDisplayJumpToPositionDialog((VDGUIHandle)mhdlg, mpPosition->GetPosition(), inputVideo, g_project->GetInputFrameRate());
			if (pos!=-1) {
				mpPosition->SetPosition(pos);
				OnVideoRedraw();
			}
		}
		return true;

	case ID_VIDEO_SEEK_NEXTSCENE:
		StartSceneShuttleForward();
		return true;

	case ID_VIDEO_SEEK_PREVSCENE:
		StartSceneShuttleReverse();
		return true;

	case ID_VIDEO_SEEK_STOP:
		SceneShuttleStop();
		return true;

	case ID_VIDEO_SEEK_SELSTART:
		{
			VDPosition sel_start, sel_end;
			if (mpPosition->GetSelection(sel_start, sel_end)) {
				SceneShuttleStop();
				mpPosition->SetPosition(sel_start);
				OnVideoRedraw();
			}
		}
		return true;

	case ID_VIDEO_SEEK_SELEND:
		{
			VDPosition sel_start, sel_end;
			if (mpPosition->GetSelection(sel_start, sel_end)) {
				SceneShuttleStop();
				mpPosition->SetPosition(sel_end);
				OnVideoRedraw();
			}
		}
		return true;

	case ID_EDIT_SETMARKER:
		mpTimeline->ToggleMarker(mpPosition->GetPosition());
		mpPosition->SetTimeline(*mpTimeline);
		return true;

	case ID_EDIT_PREVRANGE:
		SceneShuttleStop();
		MoveToPreviousRange();
		OnVideoRedraw();
		return true;

	case ID_EDIT_NEXTRANGE:
		SceneShuttleStop();
		MoveToNextRange();
		OnVideoRedraw();
		return true;

	case ID_VIDEO_COPYOUTPUTFRAME:
		CopyOutputFrameToClipboard();
		return true;

	case ID_FILE_SAVEIMAGE:
		SceneShuttleStop();
		SaveImageAsk();
		return true;

	case ID_FILE_SAVEPROJECT:
		SendMessage(mhwndFilterList,WM_COMMAND,IDC_FILTERS_SAVE,0);
		return true;

	case ID_VIDEO_FILTERS:
		SceneShuttleStop();
		EnableWindow(mhwndParent,false);
		SendMessage(mhwndFilterList,WM_COMMAND,ID_VIDEO_FILTERS,0);
		EnableWindow(mhwndParent,true);
		return true;

	case ID_OPTIONS_SHOWPROFILER:
		extern void VDOpenProfileWindow();
		VDOpenProfileWindow();
		return true;

	default:
		if (VDHandleTimelineCommand(mpPosition, mpTimeline, cmd)) {
			SceneShuttleStop();
			OnVideoRedraw();
			return true;
		}
	}

	return false;
}

void FilterPreview::MoveToPreviousRange() {
	VDPosition p0 = mpPosition->GetPosition();
	VDPosition pos = mpTimeline->GetPrevEdit(p0);
	VDPosition mpos = mpTimeline->GetPrevMarker(p0);

	if (mpos >=0 && (mpos>pos || pos==-1)) {
		mpPosition->SetPosition(mpos);
		return;
	}

	if (pos >= 0) {
		mpPosition->SetPosition(pos);
		return;
	}

	mpPosition->SetPosition(0);
}

void FilterPreview::MoveToNextRange() {
	VDPosition p0 = mpPosition->GetPosition();
	VDPosition pos = mpTimeline->GetNextEdit(p0);
	VDPosition mpos = mpTimeline->GetNextMarker(p0);

	if (mpos >=0 && (mpos<pos || pos==-1)) {
		mpPosition->SetPosition(mpos);
		return;
	}

	if (pos >= 0) {
		mpPosition->SetPosition(pos);
		return;
	}

	mpPosition->SetPosition(mpTimeline->GetLength());
}

void FilterPreview::StartSceneShuttleForward() {
	mSceneShuttleMode = 1;
	SetTimer(mhdlg,TIMER_SHUTTLE,0,0);
}

void FilterPreview::StartSceneShuttleReverse() {
	mSceneShuttleMode = -1;
	SetTimer(mhdlg,TIMER_SHUTTLE,0,0);
}

void FilterPreview::SceneShuttleStop() {
	mSceneShuttleMode = 0;
	KillTimer(mhdlg,TIMER_SHUTTLE);
}

void FilterPreview::SceneShuttleStep() {
	VDPosition pos = mpPosition->GetPosition() + mSceneShuttleMode;
	if (pos<0 || pos>=mpTimeline->GetLength()) {
		SceneShuttleStop();
		return;
	}
	mpPosition->SetPosition(pos);
	OnVideoRedraw();
}

VDPosition FilterPreview::FetchFrame() {
	return FetchFrame(mpPosition->GetPosition());
}

VDPosition FilterPreview::FetchFrame(VDPosition pos) {
	try {
		IVDStreamSource *pVSS = inputVideo->asStream();
		const VDFraction frameRate(pVSS->getRate());

		// This is a pretty awful hack, but gets around the problem that the
		// timeline isn't updated for the new frame rate.
		if (mpFiltSys->GetOutputFrameRate() != frameRate)
			mLastOutputFrame = pos;
		else {
			mLastOutputFrame = mpTimeline->TimelineToSourceFrame(pos);
			if (mLastOutputFrame < 0)
				mLastOutputFrame = mpFiltSys->GetOutputFrameCount();
		}
		
		mLastTimelineFrame	= pos;
		mLastTimelineTimeMS	= VDRoundToInt64(mpFiltSys->GetOutputFrameRate().AsInverseDouble() * 1000.0 * (double)pos);

		mInitialTimeUS = VDRoundToInt64(mpFiltSys->GetOutputFrameRate().AsInverseDouble() * 1000000.0 * (double)pos);
		mInitialFrame = -1;

		if (mpPositionCallback)
			mpPositionCallback(pos,mpvPositionCBData);

	} catch(const MyError&) {
		return -1;
	}

	return pos;
}

int64 FilterPreview::FMSetPosition(int64 pos) { 
	if(mhdlg){
		SceneShuttleStop();
		mpPosition->SetPosition(pos);
		OnVideoRedraw();
	} else {
		mInitialTimeUS = -1;
		mInitialFrame = pos;
	}
	return pos;
}

bool FilterPreview::isPreviewEnabled() {
	return !!mpFilterChainDesc;
}

bool FilterPreview::IsPreviewDisplayed() {
	return mhdlg != NULL;
}

void FilterPreview::SetButtonCallback(VDXFilterPreviewButtonCallback pfpbc, void *pvData) {
	mpButtonCallback	= pfpbc;
	mpvButtonCBData	= pvData;
}

void FilterPreview::SetSampleCallback(VDXFilterPreviewSampleCallback pfpsc, void *pvData) {
	mpSampleCallback	= pfpsc;
	mpvSampleCBData	= pvData;
}

void FilterPreview::FMSetPositionCallback(FilterModPreviewPositionCallback pfppc, void *pvData) {
	mpPositionCallback = pfppc;
	mpvPositionCBData	= pvData;
}

void FilterPreview::FMSetZoomCallback(FilterModPreviewZoomCallback pfppc, void *pvData) {
	mpZoomCallback = pfppc;
	mpvZoomCBData	= pvData;
}

void FilterPreview::InitButton(VDXHWND hwnd) {
	mhwndButton = (HWND)hwnd;

	if (hwnd) {
		const VDStringW wintext(VDGetWindowTextW32((HWND)hwnd));

		// look for an accelerator
		mButtonAccelerator = 0;

		if (mpFilterChainDesc) {
			int pos = wintext.find(L'&');
			if (pos != wintext.npos) {
				++pos;
				if (pos < wintext.size()) {
					wchar_t c = wintext[pos];

					if (iswalpha(c))
						mButtonAccelerator = towlower(c);
				}
			}
		}

		EnableWindow((HWND)hwnd, mpFilterChainDesc ? TRUE : FALSE);
	}
}

void FilterPreview::Toggle(VDXHWND hwndParent) {
	Display(hwndParent, !mhdlg);
}

void FilterPreview::Display(VDXHWND hwndParent, bool fDisplay) {
	if (fDisplay == !!mhdlg)
		return;

	if (mhdlg) {
		SetActiveWindow(mhwndParent);
		DestroyWindow(mhdlg);
		mhdlg = NULL;
		UndoSystem();
	} else if (mpFilterChainDesc) {
		mhwndParent = (HWND)hwndParent;
		mhdlg = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_PREVIEW), (HWND)hwndParent, StaticDlgProc, (LPARAM)this);
		g_projectui->DisplayPreview(true);
	}

	UpdateButton();

	if (mpButtonCallback)
		mpButtonCallback(!!mhdlg, mpvButtonCBData);
}

void FilterPreview::RedoFrame() {
	if (mhdlg)
		SendMessage(mhdlg, MYWM_INVALIDATE, 0, 0);
}

void FilterPreview::RedoSystem() {
	if (mhdlg)
		SendMessage(mhdlg, MYWM_RESTART, 0, 0);
}

void FilterPreview::UndoSystem() {
	if (mpDisplay)
		mpDisplay->Reset();

	if (mpVideoFrameBuffer) {
		mpVideoFrameBuffer->Unlock();
		mpVideoFrameBuffer = NULL;
	}
	mpFiltSys->DeinitFilters();
	mpFiltSys->DeallocateBuffers();
	mpVideoFrameSource = NULL;
}

void FilterPreview::Close() {
	InitButton(NULL);
	if (mhdlg)
		Toggle(NULL);
	UndoSystem();
}

void FilterPreview::CopyOutputFrameToClipboard() {
	if (!mpFiltSys->isRunning() || !mpVideoFrameBuffer)
		return;

	VDPixmap px = VDPixmapFromLayout(mpFiltSys->GetOutputLayout(), (void *)mpVideoFrameBuffer->LockRead());
	px.info = mpVideoFrameBuffer->info;
	g_project->CopyFrameToClipboard(px);
	mpVideoFrameBuffer->Unlock();
}

void FilterPreview::SaveImageAsk() {
	if (!mpFiltSys->isRunning() || !mpVideoFrameBuffer)
		return;

	VDPosition pos = mpPosition->GetPosition();
	VDPixmap px = VDPixmapFromLayout(mpFiltSys->GetOutputLayout(), (void *)mpVideoFrameBuffer->LockRead());
	px.info = mpVideoFrameBuffer->info;
	mpVideoFrameBuffer->Unlock();
	SaveImage(mhdlg, pos, &px);
}

bool FilterPreview::SampleCurrentFrame() {
	if (!mpFilterChainDesc || !mhdlg || !mpSampleCallback)
		return false;

	if (!mpFiltSys->isRunning()) {
		RedoSystem();

		if (!mpFiltSys->isRunning())
			return false;
	}

	VDPosition pos = mpPosition->GetPosition();

	if (pos >= 0) {
		try {
			IVDStreamSource *pVSS = inputVideo->asStream();
			const VDFraction frameRate(pVSS->getRate());

			// This hack is for consistency with FetchFrame().
			sint64 frame = pos;

			if (mpFiltSys->GetOutputFrameRate() == frameRate) {
				frame = mpTimeline->TimelineToSourceFrame(frame);

				if (frame < 0)
					frame = mpFiltSys->GetOutputFrameCount();
			}

			frame = mpFiltSys->GetSymbolicFrame(frame, mpThisFilter);

			if (frame >= 0) {
				vdrefptr<IVDFilterFrameClientRequest> req;
				if (mpThisFilter->CreateSamplingRequest(frame, mpSampleCallback, mpvSampleCBData, 0, 0, ~req)) {
					while(!req->IsCompleted()) {
						if (mpFiltSys->Run(NULL, false) == FilterSystem::kRunResult_Running)
							continue;

						switch(mpVideoFrameSource->RunRequests(NULL)) {
							case IVDFilterFrameSource::kRunResult_Running:
							case IVDFilterFrameSource::kRunResult_IdleWasActive:
							case IVDFilterFrameSource::kRunResult_BlockedWasActive:
								continue;
						}

						mpFiltSys->Block();
					}
				}
			}
		} catch(const MyError& e) {
			e.post(mhdlg, "Video sampling error");
		}
	}

	RedrawFrame();

	return true;
}

void FilterPreview::UpdateButton() {
	if (mhwndButton) {
		VDStringW text(mhdlg ? L"Hide preview" : L"Show preview");

		if (mButtonAccelerator) {
			VDStringW::size_type pos = text.find(mButtonAccelerator);

			if (pos == VDStringW::npos)
				pos = text.find(towupper(mButtonAccelerator));

			if (pos != VDStringW::npos)
				text.insert(text.begin() + pos, L'&');
		}

		VDSetWindowTextW32(mhwndButton, text.c_str());
	}
}

///////////////////////

#define FPSAMP_KEYONESEC		(1)
#define	FPSAMP_KEYALL			(2)
#define	FPSAMP_ALL				(3)

static INT_PTR CALLBACK SampleFramesDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
			if (IsDlgButtonChecked(hdlg, IDC_ONEKEYPERSEC))
				EndDialog(hdlg, FPSAMP_KEYONESEC);
			else if (IsDlgButtonChecked(hdlg, IDC_ALLKEYS))
				EndDialog(hdlg, FPSAMP_KEYALL);
			else
				EndDialog(hdlg, FPSAMP_ALL);
			return TRUE;
		case IDCANCEL:
			EndDialog(hdlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

long FilterPreview::SampleFrames() {
	static const char *const szCaptions[]={
		"Sampling one keyframe per second",
		"Sampling keyframes only",
		"Sampling all frames",
	};

	long lCount = 0;

	if (!mpFilterChainDesc || !mhdlg || !mpSampleCallback)
		return -1;

	if (!mpFiltSys->isRunning()) {
		RedoSystem();

		if (!mpFiltSys->isRunning())
			return -1;
	}

	int mode = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_FILTER_SAMPLE), mhdlg, SampleFramesDlgProc);

	if (!mode)
		return -1;

	// Time to do the actual sampling.
	try {
		VDPosition count = mpThisFilter->GetOutputFrameCount();
		ProgressDialog pd(mhdlg, "Sampling input video", szCaptions[mode-1], VDClampToSint32(count), true);
		IVDStreamSource *pVSS = inputVideo->asStream();
		VDPosition secondIncrement = pVSS->msToSamples(1000)-1;

		pd.setValueFormat("Sampling frame %ld of %ld");

		if (secondIncrement<0)
			secondIncrement = 0;

		vdrefptr<IVDFilterFrameClientRequest> req;

		VDPosition lastFrame = 0;
		for(VDPosition frame = 0; frame < count; ++frame) {
			pd.advance(VDClampToSint32(frame));
			pd.check();

			VDPosition srcFrame = mpThisFilter->GetSourceFrame(frame);

			if (mode != FPSAMP_ALL) {
				if (!inputVideo->isKey(srcFrame))
					continue;

				if (mode == FPSAMP_KEYONESEC) {
					if (frame - lastFrame < secondIncrement)
						continue;
				}

				lastFrame = frame;
			}

			if (mpThisFilter->CreateSamplingRequest(frame, mpSampleCallback, mpvSampleCBData, 0, 0, ~req)) {
				while(!req->IsCompleted()) {
					if (mpFiltSys->Run(NULL, false) == FilterSystem::kRunResult_Running)
						continue;

					switch(mpVideoFrameSource->RunRequests(NULL)) {
						case IVDFilterFrameSource::kRunResult_Running:
						case IVDFilterFrameSource::kRunResult_IdleWasActive:
						case IVDFilterFrameSource::kRunResult_BlockedWasActive:
							continue;
					}

					mpFiltSys->Block();
				}

				++lCount;
			}
		}
	} catch(MyUserAbortError e) {

		/* so what? */

	} catch(const MyError& e) {
		e.post(mhdlg, "Video sampling error");
	}

	RedrawFrame();

	return lCount;
}

long FilterPreview::SampleFrames(IFilterModPreviewSample* handler) {
	long lCount = 0;

	if (!mpFilterChainDesc || !mhdlg || !handler)
		return -1;

	if (!mpFiltSys->isRunning()) {
		RedoSystem();

		if (!mpFiltSys->isRunning())
			return -1;
	}

	bool image_changed = false;

	// Time to do the actual sampling.
	try {
		ProgressDialog pd(mhdlg, "Sampling input video", "Sampling all frames", 1, true);

		pd.setValueFormat("Sampling frame %ld of %ld");

		vdrefptr<IVDFilterFrameClientRequest> req;

		VDPosition sel_start = g_project->GetSelectionStartFrame();
		VDPosition sel_end = g_project->GetSelectionEndFrame();
		if (sel_end==-1) sel_end = g_project->GetFrameCount();
		sint64 total_count = sel_end-sel_start-1;

		VDPosition frame = -1;
		while(1){
			VDPosition nextFrame = frame+1;
			if (frame==-1) nextFrame = sel_start;
			if (frame>=sel_end-1) nextFrame = -1;
			handler->GetNextFrame(frame,&nextFrame,&total_count);
			if (nextFrame==-1) break;
			frame = nextFrame;

			pd.setLimit(VDClampToSint32(total_count));
			pd.advance(lCount);
			pd.check();

			if (mpThisFilter->CreateSamplingRequest(frame, 0, 0, handler, 0, ~req)) {
				while(!req->IsCompleted()) {
					if (mpFiltSys->Run(NULL, false) == FilterSystem::kRunResult_Running)
						continue;

					switch(mpVideoFrameSource->RunRequests(NULL)) {
						case IVDFilterFrameSource::kRunResult_Running:
						case IVDFilterFrameSource::kRunResult_IdleWasActive:
						case IVDFilterFrameSource::kRunResult_BlockedWasActive:
							continue;
					}

					mpFiltSys->Block();
				}

				int result = FilterInstance::GetSamplingRequestResult(req);
				if ((result & IFilterModPreviewSample::result_image) && req->GetResultBuffer()) {
					if (mpVideoFrameBuffer) {
						mpVideoFrameBuffer->Unlock();
						mpVideoFrameBuffer = NULL;
					}

					mpVideoFrameBuffer = req->GetResultBuffer();
					const void *p = mpVideoFrameBuffer->LockRead();

					const VDPixmapLayout& layout = mpFiltSys->GetOutputLayout();
					VDPixmap px = VDPixmapFromLayout(layout, (void *)p);
					px.info = mpVideoFrameBuffer->info;

					mpDisplay->SetSourcePersistent(false, px);
					mpDisplay->Update(IVDVideoDisplay::kAllFields);
					image_changed = true;
				}

				++lCount;
			}
		}
	} catch(MyUserAbortError e) {

		handler->Cancel();

	} catch(const MyError& e) {
		e.post(mhdlg, "Video sampling error");
	}

	if (image_changed)
		mpDisplay->Update(IVDVideoDisplay::kAllFields);
	else
		RedrawFrame();

	return lCount;
}

void FilterPreview::RedrawFrame() {
	if (mhdlg)
		SendMessage(mhdlg, MYWM_REDRAW, 0, 0);
}

///////////////////////////////////////////////////////////////////////////////

class PixmapView : public vdrefcounted<IVDPixmapViewDialog> {
public:
	PixmapView();
	~PixmapView();
	void Display(VDXHWND hwndParent, const wchar_t* title);
	void Destroy();
	void SetImage(VDPixmap& px);
	void SetDestroyCallback(PixmapViewDestroyCallback cb, void* cbData){ destroyCB = cb; destroyCBData = cbData; }

private:
	static INT_PTR CALLBACK StaticDlgProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam);
	BOOL DlgProc(UINT message, WPARAM wParam, LPARAM lParam);

	void OnInit();
	void OnResize();
	void OnPaint();
	void OnVideoRedraw();
	bool OnCommand(UINT);
	void CopyOutputFrameToClipboard();
	void SaveImageAsk();

	HWND		mhdlg;
	HWND		mhwndParent;
	HWND		mhwndVideoWindow;
	HWND		mhwndDisplay;

	IVDVideoDisplay *mpDisplay;
	IVDVideoWindow *mpVideoWindow;

	VDPixmapBuffer image;

	ModelessDlgNode		mDlgNode;
	PixmapViewDestroyCallback destroyCB;
	void* destroyCBData;
};

bool VDCreatePixmapViewDialog(IVDPixmapViewDialog **pp) {
	IVDPixmapViewDialog *p = new_nothrow PixmapView();
	if (!p)
		return false;
	p->AddRef();
	*pp = p;
	return true;
}

PixmapView::PixmapView()
	: mhdlg(NULL)
	, mhwndParent(NULL)
	, mhwndVideoWindow(NULL)
	, mhwndDisplay(NULL)
	, mpDisplay(NULL)
	, mpVideoWindow(NULL)
{
	destroyCB = 0;
	destroyCBData = 0;
}

PixmapView::~PixmapView() {
	if (mhdlg)
		DestroyWindow(mhdlg);
}

void PixmapView::Display(VDXHWND hwndParent, const wchar_t* title) {
	mhwndParent = (HWND)hwndParent;
	mhdlg = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_PREVIEW), (HWND)hwndParent, StaticDlgProc, (LPARAM)this);
	SetWindowTextW(mhdlg,title);
}

void PixmapView::Destroy() {
	if (mhdlg) {
		//SetActiveWindow(mhwndParent);
		DestroyWindow(mhdlg);
		mhdlg = NULL;
	}
}

INT_PTR CALLBACK PixmapView::StaticDlgProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam) {
	PixmapView *fpd = (PixmapView *)GetWindowLongPtr(hdlg, DWLP_USER);

	if (message == WM_INITDIALOG) {
		SetWindowLongPtr(hdlg, DWLP_USER, lParam);
		fpd = (PixmapView *)lParam;
		fpd->mhdlg = hdlg;
	}

	return fpd && fpd->DlgProc(message, wParam, lParam);
}

BOOL PixmapView::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch(message) {
	case WM_INITDIALOG:
		OnInit();
		VDSetDialogDefaultIcons(mhdlg);
		OnVideoRedraw();
		return TRUE;

	case WM_DESTROY:
		if (mpDisplay) {
			mpDisplay->Destroy();
			mpDisplay = NULL;
			mhwndDisplay = NULL;
		}

		mpVideoWindow = NULL;
		mhwndVideoWindow = NULL;

		mDlgNode.Remove();
		if (destroyCB) destroyCB(this,destroyCBData);
		return TRUE;

	case WM_SIZE:
		OnResize();
		OnVideoRedraw();
		return TRUE;

	case WM_PAINT:
		OnPaint();
		return TRUE;

	case WM_ERASEBKGND:
		return TRUE;

	case WM_NOTIFY:
		{
			const NMHDR& hdr = *(const NMHDR *)lParam;
			if (hdr.hwndFrom == mhwndVideoWindow) {
				switch(hdr.code) {
					case VWN_RESIZED:
						{
							RECT r;
							GetWindowRect(mhwndVideoWindow, &r);

							AdjustWindowRectEx(&r, GetWindowLong(mhdlg, GWL_STYLE), FALSE, GetWindowLong(mhdlg, GWL_EXSTYLE));
							SetWindowPos(mhdlg, NULL, 0, 0, r.right-r.left, r.bottom-r.top, SWP_NOZORDER|SWP_NOMOVE|SWP_NOACTIVATE);
						}
						break;
				}
			}
		}
		return TRUE;

	case WM_COMMAND:
		return OnCommand(LOWORD(wParam));

	case WM_CONTEXTMENU:
		{
			POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
			RECT r;

			if (::GetWindowRect(mhwndVideoWindow, &r) && ::PtInRect(&r, pt)) {
				SendMessage(mhwndVideoWindow, WM_CONTEXTMENU, wParam, lParam);
			}
		}
		break;
	}

	return FALSE;
}

void PixmapView::OnInit() {
	mhwndVideoWindow = CreateWindow(VIDEOWINDOWCLASS, NULL, WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN, 0, 0, 0, 0, mhdlg, (HMENU)100, g_hInst, NULL);
	mhwndDisplay = (HWND)VDCreateDisplayWindowW32(0, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0, 0, 0, 0, (VDGUIHandle)mhwndVideoWindow);
	if (mhwndDisplay)
		mpDisplay = VDGetIVideoDisplay((VDGUIHandle)mhwndDisplay);
	EnableWindow(mhwndDisplay, FALSE);

	mpVideoWindow = VDGetIVideoWindow(mhwndVideoWindow);
	mpVideoWindow->SetChild(mhwndDisplay);
	mpVideoWindow->SetDisplay(mpDisplay);
	mpVideoWindow->SetMouseTransparent(true);
	mpVideoWindow->SetBorderless(true);

	mDlgNode.hdlg = mhdlg;
	mDlgNode.mhAccel = g_projectui->GetAccelPreview();
	guiAddModelessDialog(&mDlgNode);

	if (image.w)
		mpVideoWindow->SetSourceSize(image.w, image.h);
	else
		mpVideoWindow->SetSourceSize(256, 256);
}

void PixmapView::OnResize() {
	RECT r;

	GetClientRect(mhdlg, &r);

	int mDisplayX	= 0;
	int mDisplayY	= 0;
	int mDisplayW	= r.right;
	int mDisplayH	= r.bottom;

	if (mDisplayW < 0)
		mDisplayW = 0;

	if (mDisplayH < 0)
		mDisplayH = 0;

	SetWindowPos(mhwndVideoWindow, NULL, mDisplayX, mDisplayY, mDisplayW, mDisplayH, SWP_NOZORDER|SWP_NOACTIVATE);

	InvalidateRect(mhdlg, NULL, TRUE);
}

void PixmapView::OnPaint() {
	PAINTSTRUCT ps;
	BeginPaint(mhdlg, &ps);
	EndPaint(mhdlg, &ps);
}

void PixmapView::OnVideoRedraw() {
	ShowWindow(mhwndVideoWindow, SW_SHOW);
	mpDisplay->SetSourcePersistent(false, image);
}

bool PixmapView::OnCommand(UINT cmd) {
	switch(cmd) {
	case IDCANCEL:
		SetActiveWindow(mhwndParent);
		DestroyWindow(mhdlg);
		mhdlg = NULL;
		return true;

	case ID_VIDEO_COPYOUTPUTFRAME:
		CopyOutputFrameToClipboard();
		return true;

	case ID_FILE_SAVEIMAGE:
		SaveImageAsk();
		return true;

	case ID_OPTIONS_SHOWPROFILER:
		extern void VDOpenProfileWindow();
		VDOpenProfileWindow();
		return true;
	}

	return false;
}

void PixmapView::SetImage(VDPixmap& px) {
	image.assign(px);

	if (mpVideoWindow) {
		if (image.w)
			mpVideoWindow->SetSourceSize(image.w, image.h);
		else
			mpVideoWindow->SetSourceSize(256, 256);
		mpDisplay->SetSourcePersistent(false, image);
		//InvalidateRect(mhdlg, NULL, false);
		mpDisplay->Update(IVDVideoDisplay::kAllFields);
	}
}

void PixmapView::CopyOutputFrameToClipboard() {
	if (image.w==0 || image.h==0) return;
	g_project->CopyFrameToClipboard(image);
}

void PixmapView::SaveImageAsk() {
	if (image.w==0 || image.h==0) return;
	SaveImage(mhdlg, -1, &image);
}
