//	VirtualDub - Video processing and capture application
//	A/V interface library
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

#include <stdafx.h>
#include <vd2/system/binary.h>
#include <vd2/system/vdstring.h>
#include <vd2/system/error.h>
#include <vd2/system/time.h>
#include <vd2/system/profile.h>
#include <vd2/system/registry.h>
#include <vd2/system/w32assist.h>
#include <vd2/VDCapture/ScreenGrabberGDI.h>
#include <windows.h>
#include <tchar.h>
#include <dwmapi.h>

namespace {
	DWORD AutodetectCaptureBltMode() {
		OSVERSIONINFOA verinfo={sizeof(OSVERSIONINFOA)};

		if (GetVersionExA(&verinfo)) {
			if (verinfo.dwPlatformId == VER_PLATFORM_WIN32_NT) {
				// Windows 2000 or newer
				if (verinfo.dwMajorVersion >= 5)
					return SRCCOPY | CAPTUREBLT;
			} else if (verinfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
				// Test for Windows 98 or newer
				if (verinfo.dwMajorVersion >= 5 || (verinfo.dwMajorVersion == 4 && verinfo.dwMinorVersion >= 10))
					return SRCCOPY | CAPTUREBLT;
			}

		}

		return SRCCOPY;
	}
}

VDScreenGrabberGDI::VDScreenGrabberGDI()
	: mhwnd(NULL)
	, mWndClass(NULL)
	, mpCB(NULL)
	, mbCapBuffersInited(false)
	, mhdcOffscreen(NULL)
	, mhbmOffscreen(NULL)
	, mpOffscreenData(NULL)
	, mOffscreenSize(0)
	, mCachedCursor(NULL)
	, mCachedCursorHotspotX(0)
	, mCachedCursorHotspotY(0)
	, mhdcCursorBuffer(NULL)
	, mhbmCursorBuffer(NULL)
	, mhbmCursorBufferOld(NULL)
	, mpCursorBuffer(NULL)
	, mbVisible(false)
	, mbDisplayPreview(false)
	, mDisplayArea(0, 0, 0, 0)
	, mTrackX(0)
	, mTrackY(0)
	, mbDrawMousePointer(true)
	, mCaptureWidth(320)
	, mCaptureHeight(240)
	, mProfileChannel("Capture driver")
{
	mbExcludeSelf = true;
}

VDScreenGrabberGDI::~VDScreenGrabberGDI() {
	Shutdown();
}

bool VDScreenGrabberGDI::Init(IVDScreenGrabberCallback *cb) {
	mpCB = cb;

	return true;
}

void VDScreenGrabberGDI::Shutdown() {
	mpCB = NULL;

	ShutdownDisplay();
	ShutdownCapture();
}

bool VDScreenGrabberGDI::InitCapture(uint32 srcw, uint32 srch, uint32 dstw, uint32 dsth, VDScreenGrabberFormat format) {
	if (srcw != dstw || srch != dsth || format != kVDScreenGrabberFormat_XRGB32)
		return false;

	ShutdownCapture();

	mCaptureWidth = dstw;
	mCaptureHeight = dsth;

	HDC hdc = GetDC(NULL);
	if (!hdc)
		return false;

	mhdcOffscreen = CreateCompatibleDC(hdc);

	uint32 rowlen = mCaptureWidth * 4;
	mOffscreenSize = rowlen * mCaptureHeight;

	BITMAPINFO hdr = {};
	hdr.bmiHeader.biSize			= sizeof(BITMAPINFOHEADER);
	hdr.bmiHeader.biWidth			= mCaptureWidth;
	hdr.bmiHeader.biHeight			= mCaptureHeight;
	hdr.bmiHeader.biPlanes			= 1;
	hdr.bmiHeader.biCompression		= BI_RGB;
	hdr.bmiHeader.biBitCount		= 32;
	hdr.bmiHeader.biSizeImage		= mOffscreenSize;
	hdr.bmiHeader.biXPelsPerMeter	= 0;
	hdr.bmiHeader.biYPelsPerMeter	= 0;
	hdr.bmiHeader.biClrUsed			= 0;
	hdr.bmiHeader.biClrImportant	= 0;

	mhbmOffscreen = CreateDIBSection(hdc, &hdr, DIB_PAL_COLORS, &mpOffscreenData, NULL, 0);
	ReleaseDC(NULL, hdc);

	if (!mhbmOffscreen || !mhdcOffscreen) {
		ShutdownCapture();
		return false;
	}

	DeleteObject(SelectObject(mhdcOffscreen, mhbmOffscreen));

	mbCapBuffersInited = true;
	return true;
}

void VDScreenGrabberGDI::ShutdownCapture() {
	mbCapBuffersInited = false;

	if (mhbmCursorBufferOld) {
		SelectObject(mhdcCursorBuffer, mhbmCursorBufferOld);
		mhbmCursorBufferOld = NULL;
	}

	if (mhbmCursorBuffer) {
		DeleteObject(mhbmCursorBuffer);
		mhbmCursorBuffer = NULL;
	}

	if (mhdcCursorBuffer) {
		DeleteDC(mhdcCursorBuffer);
		mhdcCursorBuffer = NULL;
	}

	if (mhdcOffscreen) {
		DeleteDC(mhdcOffscreen);
		mhdcOffscreen = NULL;
	}

	if (mhbmOffscreen) {
		DeleteObject(mhbmOffscreen);
		mhbmOffscreen = NULL;
	}
}

void VDScreenGrabberGDI::SetCaptureOffset(int x, int y) {
	mTrackX = x;
	mTrackY = y;
}

void VDScreenGrabberGDI::SetCapturePointer(bool enable) {
	mbDrawMousePointer = enable;
}

uint64 VDScreenGrabberGDI::GetCurrentTimestamp() {
	return ComputeGlobalTime();
}

sint64 VDScreenGrabberGDI::ConvertTimestampDelta(uint64 t, uint64 base) {
	return (sint32)(uint32)(t - base) * (sint64)1000;
}

bool VDScreenGrabberGDI::AcquireFrame(bool dispatch) {
	if (!mbCapBuffersInited)
		return false;

	sint64 globalTime;

	int w = mCaptureWidth;
	int h = mCaptureHeight;

	globalTime = ComputeGlobalTime();

	// Check for cursor update.
	CURSORINFO ci = {sizeof(CURSORINFO)};
	bool cursorImageUpdated = false;

	if (mbDrawMousePointer) {
		if (!::GetCursorInfo(&ci)) {
			ci.hCursor = NULL;
		}

		if (ci.hCursor) {
			if (mCachedCursor != ci.hCursor) {
				mCachedCursor = ci.hCursor;

				ICONINFO ii;
				if (::GetIconInfo(ci.hCursor, &ii)) {
					mCachedCursorHotspotX = ii.xHotspot;
					mCachedCursorHotspotY = ii.yHotspot;

					if (ii.hbmColor)
						VDVERIFY(::DeleteObject(ii.hbmColor));

					if (ii.hbmMask)
						VDVERIFY(::DeleteObject(ii.hbmMask));
				}
			}

			ci.ptScreenPos.x -= mCachedCursorHotspotX;
			ci.ptScreenPos.y -= mCachedCursorHotspotY;
		}
	}

	mProfileChannel.Begin(0xf0d0d0, "Capture (GDI)");
	if (HDC hdc = GetDC(NULL)) {
		static DWORD sBitBltMode = AutodetectCaptureBltMode();

		int srcx = mTrackX;
		int srcy = mTrackY;

		int limitx = GetSystemMetrics(SM_CXSCREEN) - w;
		int limity = GetSystemMetrics(SM_CYSCREEN) - h;

		if (srcx > limitx)
			srcx = limitx;

		if (srcx < 0)
			srcx = 0;

		if (srcy > limity)
			srcy = limity;

		if (srcy < 0)
			srcy = 0;

		BitBlt(mhdcOffscreen, 0, 0, w, h, hdc, srcx, srcy, sBitBltMode);
		if (mbExcludeSelf) {
			if (HDC hdcw = GetDC(mhwnd)) {
				HRGN rgn0 = CreateRectRgn(0,0,0,0); 
				GetRandomRgn(hdcw,rgn0,SYSRGN);
				OffsetRgn(rgn0,-srcx,-srcy);
				HBRUSH br = CreateSolidBrush(GetSysColor(COLOR_APPWORKSPACE));
				FillRgn(mhdcOffscreen,rgn0,br);
				DeleteObject(br);
				DeleteObject(rgn0);
				ReleaseDC(mhwnd, hdcw);
			}
		}

		if (ci.hCursor)
			DrawIcon(mhdcOffscreen, ci.ptScreenPos.x - srcx, ci.ptScreenPos.y - srcy, ci.hCursor);

		ReleaseDC(NULL, hdc);
	}
	mProfileChannel.End();

	if (mbVisible && mhwnd && mbDisplayPreview) {
		mProfileChannel.Begin(0xe0e0e0, "Preview (GDI)");
		if (HDC hdc = GetDC(mhwnd)) {
			BitBlt(hdc, 0, 0, w, h, mhdcOffscreen, 0, 0, SRCCOPY);
			ReleaseDC(mhwnd, hdc);
		}
		mProfileChannel.End();
	}

	if (dispatch) {
		GdiFlush();

		if (mpCB)
			mpCB->ReceiveFrame(globalTime, (const char *)mpOffscreenData, mCaptureWidth * 4, mCaptureWidth * 4, mCaptureHeight);
	}
	
	if (mbVisible && mhwnd && !mbDisplayPreview) {
		mProfileChannel.Begin(0xe0e0e0, "Overlay (GDI)");
		if (HDC hdcScreen = GetDC(NULL)) {
			if (HDC hdc = GetDC(mhwnd)) {
				int srcx = mTrackX;
				int srcy = mTrackY;

				int limitx = GetSystemMetrics(SM_CXSCREEN) - w;
				int limity = GetSystemMetrics(SM_CYSCREEN) - h;

				if (srcx > limitx)
					srcx = limitx;

				if (srcx < 0)
					srcx = 0;

				if (srcy > limity)
					srcy = limity;

				if (srcy < 0)
					srcy = 0;
				
				if( mbExcludeSelf) {
					POINT p0 = {0,0};
					MapWindowPoints(mhwnd,0,&p0,1);

					HRGN rgn0 = CreateRectRgn(0,0,0,0); 
					HRGN rgn1 = CreateRectRgn(0,0,0,0); 
					GetRandomRgn(hdc,rgn0,SYSRGN);
					CombineRgn(rgn1,rgn0,0,RGN_COPY);
					OffsetRgn(rgn0,-p0.x,-p0.y);
					OffsetRgn(rgn1,-srcx,-srcy);
					CombineRgn(rgn0,rgn0,rgn1,RGN_DIFF);

					SelectClipRgn(hdc, rgn0);
					BitBlt(hdc, 0, 0, w, h, hdcScreen, srcx, srcy, SRCCOPY);

					SelectClipRgn(hdc, rgn1);
					RECT r;
					GetClientRect(mhwnd,&r);
					FillRect(hdc,&r,(HBRUSH)(COLOR_APPWORKSPACE+1));

					DeleteObject(rgn0);
					DeleteObject(rgn1);
					
					ReleaseDC(mhwnd, hdc);
					ValidateRect(mhwnd,0);
				} else {
					BitBlt(hdc, 0, 0, w, h, hdcScreen, srcx, srcy, SRCCOPY);
					ReleaseDC(mhwnd, hdc);
				}
			}
			ReleaseDC(NULL, hdcScreen);
		}
		mProfileChannel.End();
	}

	return true;
}

bool VDScreenGrabberGDI::InitDisplay(HWND hwndParent, bool preview) {
	const HINSTANCE hInst = VDGetLocalModuleHandleW32();

	if (!mWndClass) {
		TCHAR buf[64];
		_sntprintf(buf, 64, _T("VDScreenGrabberGDI[%p])"), this);
		buf[63] = 0;

		WNDCLASS wc = { 0, StaticWndProc, 0, sizeof(VDScreenGrabberGDI *), hInst, NULL, NULL, NULL, NULL, buf };

		mWndClass = RegisterClass(&wc);

		if (!mWndClass)
			return false;
	}

	// Create message sink.
	const DWORD dwFlags = mbVisible ? WS_CHILD | WS_VISIBLE : WS_CHILD;

	mhwnd = CreateWindow((LPCTSTR)mWndClass, _T(""), dwFlags, mDisplayArea.left, mDisplayArea.top, mDisplayArea.width(), mDisplayArea.height(), hwndParent, NULL, hInst, this);
	if (!mhwnd) {
		ShutdownDisplay();
		return false;
	}

	mbExcludeSelf = true;
	BOOL enabled = false;
	if (VDIsAtLeastVistaW32()) {
		// with composition enabled GetRandomRgn is useless.
		DwmIsCompositionEnabled(&enabled);
		if (enabled) mbExcludeSelf = false;
	}
	mbDisplayPreview = preview;
	return true;
}

void VDScreenGrabberGDI::ShutdownDisplay() {
	if (mhwnd) {
		DestroyWindow(mhwnd);
		mhwnd = NULL;
	}

	if (mWndClass) {
		UnregisterClass((LPCTSTR)mWndClass, VDGetLocalModuleHandleW32());
		mWndClass = NULL;
	}
}

void VDScreenGrabberGDI::SetDisplayArea(const vdrect32& r) {
	mDisplayArea = r;

	if (mhwnd)
		SetWindowPos(mhwnd, NULL, r.left, r.top, r.width(), r.height(), SWP_NOZORDER|SWP_NOACTIVATE);
}

void VDScreenGrabberGDI::SetDisplayVisible(bool vis) {
	if (vis == mbVisible)
		return;

	mbVisible = vis;

	if (mhwnd)
		ShowWindow(mhwnd, vis ? SW_SHOWNA : SW_HIDE);
}

sint64 VDScreenGrabberGDI::ComputeGlobalTime() {
	return VDGetAccurateTick();
}

LRESULT CALLBACK VDScreenGrabberGDI::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_NCCREATE:
			SetWindowLongPtr(hwnd, 0, (LONG_PTR)((LPCREATESTRUCT)lParam)->lpCreateParams);
			break;

		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hwnd, &ps);
				if (hdc) {
					FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_ACTIVECAPTION + 1));
					EndPaint(hwnd, &ps);
				}
			}
			return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

///////////////////////////////////////////////////////////////////////////

IVDScreenGrabber *VDCreateScreenGrabberGDI() {
	return new VDScreenGrabberGDI;
}
