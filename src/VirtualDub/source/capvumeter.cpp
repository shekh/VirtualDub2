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

#include "resource.h"
#include <vd2/system/error.h>
#include <vd2/system/time.h>
#include <vd2/Dita/w32control.h>
#include "capvumeter.h"


#define BALANCE_DEADZONE		(2048)

///////////////////////////////////////////////////////////////////////////
namespace {
	void ComputeWavePeaks_8M(const uint8 *p, unsigned count, float& l, float& r) {
		int v=0;

		do {
			int c = abs((sint8)(*p++ ^ 0x80));

			if (v < c)
				v = c;
		} while(--count);

		l = r = (v * (1.0f / 128.0f));
	}

	void ComputeWavePeaks_8S(const uint8 *p, unsigned count, float& l, float& r) {
		int vL=0;
		int vR=0;

		do {
			int cL = abs((sint8)(p[0] ^ 0x80));
			int cR = abs((sint8)(p[1] ^ 0x80));
			p += 2;

			if (vL < cL)
				vL = cL;
			if (vR < cR)
				vR = cR;
		} while(--count);

		l = (vL * (1.0f / 128.0f));
		r = (vR * (1.0f / 128.0f));
	}

	void ComputeWavePeaks_16M(const sint16 *p, unsigned count, float& l, float& r) {
		int v=0;

		do {
			int c = abs((int)*p++);

			if (v < c)
				v = c;
		} while(--count);

		l = r = (v * (1.0f / 32768.0f));
	}

	void ComputeWavePeaks_16S(const sint16 *p, unsigned count, float& l, float& r) {
		int vL=0;
		int vR=0;

		do {
			int cL = abs((int)p[0]);
			int cR = abs((int)p[1]);
			p += 2;

			if (vL < cL)
				vL = cL;
			if (vR < cR)
				vR = cR;
		} while(--count);

		l = (vL * (1.0f / 32768.0f));
		r = (vR * (1.0f / 32768.0f));
	}
}

void VDComputeWavePeaks(const void *p, unsigned depth, unsigned channels, unsigned count, float& l, float& r) {
	l = r = 0;

	if (count) {
		if (depth == 8 && channels == 1)
			ComputeWavePeaks_8M((const uint8 *)p, count, l, r);
		else if (depth == 8 && channels == 2)
			ComputeWavePeaks_8S((const uint8 *)p, count, l, r);
		else if (depth == 16 && channels == 1)
			ComputeWavePeaks_16M((const sint16 *)p, count, l, r);
		else if (depth == 16 && channels == 2)
			ComputeWavePeaks_16S((const sint16 *)p, count, l, r);
	}
}

///////////////////////////////////////////////////////////////////////////

class VDUICaptureVumeterW32 : public VDUICustomControlW32, public IVDUICaptureVumeter {
public:
	VDUICaptureVumeterW32();
	~VDUICaptureVumeterW32();

	void *AsInterface(uint32 id);

	bool Create(IVDUIParameters *pParams) { return VDUICustomControlW32::Create(pParams); }

	void SetArea(const vduirect& r);

	void SetPeakLevels(float l, float r);

protected:
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void OnPaint();

	uint64	mLastPeakL, mLastPeakR;
	float	mFracL, mFracR;
	float	mPeakL, mPeakR;
	HBRUSH	mhbrFillL;
	HBRUSH	mhbrFillR;
	HBRUSH	mhbrErase;
	HBRUSH	mhbrPeak;
};

extern IVDUIWindow *VDCreateUICaptureVumeter() { return new VDUICaptureVumeterW32; }

VDUICaptureVumeterW32::VDUICaptureVumeterW32()
	: mLastPeakL(0)
	, mLastPeakR(0)
	, mPeakL(0)
	, mPeakR(0)
	, mFracL(0.5f)
	, mFracR(0.5f)
	, mhbrFillL(CreateSolidBrush(RGB(0,128,192)))
	, mhbrFillR(CreateSolidBrush(RGB(0,0,255)))
	, mhbrErase(CreateSolidBrush(RGB(0,0,0)))
	, mhbrPeak(CreateSolidBrush(RGB(255,0,0)))
{
}

VDUICaptureVumeterW32::~VDUICaptureVumeterW32() {
	if (mhbrFillL)
		DeleteObject(mhbrFillL);
	if (mhbrFillR)
		DeleteObject(mhbrFillR);
	if (mhbrErase)
		DeleteObject(mhbrErase);
	if (mhbrPeak)
		DeleteObject(mhbrPeak);
}

void *VDUICaptureVumeterW32::AsInterface(uint32 id) {
	switch(id) {
		case IVDUICaptureVumeter::kTypeID: return static_cast<IVDUICaptureVumeter *>(this); break;
	}

	return VDUICustomControlW32::AsInterface(id);
}

void VDUICaptureVumeterW32::SetArea(const vduirect& r) {
	VDUICustomControlW32::SetArea(r);
	InvalidateRect(mhwnd, NULL, TRUE);
}

void VDUICaptureVumeterW32::SetPeakLevels(float l, float r) {
	const float invLn10_4 = 0.2171472409516259138255644594583f;

	if (l < 1e-4f)
		mFracL = 0;
	else
		mFracL = 1.0f + (float)(log(l) * invLn10_4);

	if (r < 1e-4f)
		mFracR = 0;
	else
		mFracR = 1.0f + (float)(log(r) * invLn10_4);

	if (mFracL < 0)
		mFracL = 0;

	if (mFracR < 0)
		mFracR = 0;

	RECT rClient;

	GetClientRect(mhwnd, &rClient);
	rClient.bottom -= (unsigned)(rClient.bottom - rClient.top) >> 1;
	InvalidateRect(mhwnd, &rClient, TRUE);
}

LRESULT VDUICaptureVumeterW32::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_PAINT:
		OnPaint();
		break;
	case WM_ERASEBKGND:
		{
			HDC hdc = (HDC)wParam;
			RECT r;
			GetClientRect(mhwnd, &r);

			r.top += (unsigned)(r.bottom - r.top) >> 1;

			FillRect(hdc, &r, (HBRUSH)GetClassLongPtr(mhwnd, GCLP_HBRBACKGROUND));
		}
		return TRUE;
	}
	return VDUICustomControlW32::WndProc(msg, wParam, lParam);
}

void VDUICaptureVumeterW32::OnPaint() {
	PAINTSTRUCT ps;

	if (HDC hdc = BeginPaint(mhwnd, &ps)) {
		RECT r;
		GetClientRect(mhwnd, &r);

		// compute peak falloff
		uint64 t = VDGetAccurateTick();
		double invfreq = 0.1 / 1000.0;

		if (!mLastPeakL)
			mLastPeakL = t;

		if (!mLastPeakR)
			mLastPeakR = t;

		struct local {
			static double sq(double x) { return x*x; }
		};

		float peakL = mPeakL - (float)local::sq((t - mLastPeakL) * invfreq);
		float peakR = mPeakR - (float)local::sq((t - mLastPeakR) * invfreq);

		if (peakL < mFracL) {
			mPeakL = peakL = mFracL;
			mLastPeakL = t;
		}

		if (peakR < mFracR) {
			mPeakR = peakR = mFracR;
			mLastPeakR = t;
		}

		int xL = VDRoundToIntFast(mFracL * r.right);
		int xR = VDRoundToIntFast(mFracR * r.right);

		const int y0 = 0;
		const int y1 = r.bottom >> 2;
		const int y2 = r.bottom >> 1;

		RECT r2L = {0,y0,xL,y1};
		RECT r2R = {0,y1,xR,y2};

		FillRect(hdc, &r2L, mhbrFillL);
		FillRect(hdc, &r2R, mhbrFillR);

		r2L.left = xL;
		r2L.right = r.right;
		r2R.left = xR;
		r2R.right = r.right;
		FillRect(hdc, &r2L, mhbrErase);
		FillRect(hdc, &r2R, mhbrErase);

		int peakxL = VDRoundToIntFast(peakL * (r.right-1));
		int peakxR = VDRoundToIntFast(peakR * (r.right-1));

		RECT rPeakL = {peakxL, y0, peakxL+1, y1 };
		RECT rPeakR = {peakxR, y1, peakxR+1, y2 };

		FillRect(hdc, &rPeakL, mhbrPeak);
		FillRect(hdc, &rPeakR, mhbrPeak);

		// determine the -10db, -6db, and -3db points; the -20db and 0db points
		// are defined as the ends, which makes this easier

		int x_30db	= VDRoundToIntFast(r.right * (10.0f/40.0f));
		int x_20db	= VDRoundToIntFast(r.right * (20.0f/40.0f));
		int x_10db	= VDRoundToIntFast(r.right * (30.0f/40.0f));
		int x_6db	= VDRoundToIntFast(r.right * (34.0f/40.0f));
		int x_3db	= VDRoundToIntFast(r.right * (37.0f/40.0f));

		if (HGDIOBJ hOldFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT))) {
			TEXTMETRIC tm;
			int y3 = (y2+r.bottom)>>1;

			if (GetTextMetrics(hdc, &tm))
				y3 = r.bottom - tm.tmHeight;

			// draw -10db, -6db, and -3db notches
			RECT r30dbNotch	= { x_30db, y2 + 1, x_30db+1, y3 };
			RECT r20dbNotch	= { x_20db, y2 + 1, x_20db+1, y3 };
			RECT r10dbNotch	= { x_10db, y2 + 1, x_10db+1, y3 };
			RECT r6dbNotch	= { x_6db, y2 + 1, x_6db+1, y3 };
			RECT r3dbNotch	= { x_3db, y2 + 1, x_3db+1, y3 };

			HBRUSH hBlackBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);

			FillRect(hdc, &r30dbNotch, hBlackBrush);
			FillRect(hdc, &r20dbNotch, hBlackBrush);
			FillRect(hdc, &r10dbNotch, hBlackBrush);
			FillRect(hdc, &r6dbNotch, hBlackBrush);
			FillRect(hdc, &r3dbNotch, hBlackBrush);

			// draw db text
			SetBkMode(hdc, TRANSPARENT);
			SetTextAlign(hdc, TA_LEFT | TA_BOTTOM);
			TextOut(hdc, 0, r.bottom, "-40 dB", 6);
			SetTextAlign(hdc, TA_CENTER | TA_BOTTOM);
			TextOut(hdc, x_30db, r.bottom, "-30 dB", 6);
			TextOut(hdc, x_20db, r.bottom, "-20 dB", 6);
			TextOut(hdc, x_10db, r.bottom, "-10 dB", 6);
			TextOut(hdc, x_6db, r.bottom, "-6 dB", 5);
			TextOut(hdc, x_3db, r.bottom, "-3 dB", 5);
			SetTextAlign(hdc, TA_RIGHT | TA_BOTTOM);
			TextOut(hdc, r.right, r.bottom, "0 dB", 4);
			SelectObject(hdc, hOldFont);
		}

		EndPaint(mhwnd, &ps);
	}
}
