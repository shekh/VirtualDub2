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
	void ComputeWavePeaks_8(const uint8 *data, unsigned count, int channels, float* peak) {
		{for(int ch=0; ch<channels; ch++){
			const uint8* p = data+ch;
			int v = 0;
			{for(int i=0; i<count; i++){
				int c = abs((sint8)(*p ^ 0x80));
				if (v < c) v = c;
				p += channels;
			}}
			peak[ch] = (v * (1.0f / 128.0f));
		}}
	}

	void ComputeWavePeaks_16(const sint16 *data, unsigned count, int channels, float* peak) {
		{for(int ch=0; ch<channels; ch++){
			const sint16* p = data+ch;
			int v = 0;
			{for(int i=0; i<count; i++){
				int c = abs((int)(*p));
				if (v < c) v = c;
				p += channels;
			}}
			peak[ch] = (v * (1.0f / 32768.0f));
		}}
	}
}

void VDComputeWavePeaks(const void *p, unsigned depth, unsigned channels, unsigned count, float* peak) {
	{for(int i=0; i<channels; i++) peak[i] = 0; }

	if (count) {
		if(depth==8) ComputeWavePeaks_8((const uint8 *)p, count, channels, peak);
		if(depth==16) ComputeWavePeaks_16((const sint16 *)p, count, channels, peak);
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

	void SetPeakLevels(int count, float* peak, int mask);

protected:
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void OnPaint();

	int peak_count;
	uint64	mLastPeak[16];
	float	mFrac[16];
	float	mPeak[16];
	HBRUSH	mhbrFillL;
	HBRUSH	mhbrFillR;
	HBRUSH	mhbrErase;
	HBRUSH	mhbrPeak;
};

extern IVDUIWindow *VDCreateUICaptureVumeter() { return new VDUICaptureVumeterW32; }

VDUICaptureVumeterW32::VDUICaptureVumeterW32()
	: mhbrFillL(CreateSolidBrush(RGB(0,128,192)))
	, mhbrFillR(CreateSolidBrush(RGB(0,0,255)))
	, mhbrErase(CreateSolidBrush(RGB(0,0,0)))
	, mhbrPeak(CreateSolidBrush(RGB(255,0,0)))
{
	{for(int i=0; i<16; i++){
		mLastPeak[i] = 0;
		mPeak[i] = 0;
		mFrac[i] = 0.5;
	}}
	peak_count = 0;
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

void VDUICaptureVumeterW32::SetPeakLevels(int count, float* src_peak, int mask) {
	const float invLn10_4 = 0.2171472409516259138255644594583f;

	float peak[16];
	peak_count = 0;
	{for(int i=0; i<count; i++){
		if((1<<i) & mask){
			peak[peak_count] = src_peak[i];
			peak_count++;
		}
	}}

	{for(int i=0; i<peak_count; i++){
		if (peak[i] < 1e-4f)
			mFrac[i] = 0;
		else
			mFrac[i] = 1.0f + (float)(log(peak[i]) * invLn10_4);

		if (mFrac[i] < 0)
			mFrac[i] = 0;
	}}

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

		struct local {
			static double sq(double x) { return x*x; }
		};

		int peakx[16];
		int y2 = r.bottom >> 1;

		if (!peak_count) {
			RECT r2 = {0,0,r.right,y2};
			FillRect(hdc, &r2, mhbrErase);
		}

		{for(int i=0; i<peak_count; i++){
			if (!mLastPeak[i])
				mLastPeak[i] = t;

			float peak = mPeak[i] - (float)local::sq((t - mLastPeak[i]) * invfreq);
			if (peak < mFrac[i]) {
				mPeak[i] = peak = mFrac[i];
				mLastPeak[i] = t;
			}

			int x = VDRoundToIntFast(mFrac[i] * r.right);
			peakx[i] = VDRoundToIntFast(peak * (r.right-1));

			int y0 = y2*i/peak_count;
			int y1 = y2*(i+1)/peak_count;
			RECT r2 = {0,y0,x,y1};
			FillRect(hdc, &r2, (i%2==0) ? mhbrFillL:mhbrFillR);
			r2.left = x;
			r2.right = r.right;
			FillRect(hdc, &r2, mhbrErase);
		}}

		{for(int i=0; i<peak_count; i++){
			int y0 = y2*i/peak_count;
			int y1 = y2*(i+1)/peak_count;
			RECT rPeak = {peakx[i], y0, peakx[i]+1, y1 };
			FillRect(hdc, &rPeak, mhbrPeak);
		}}

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
