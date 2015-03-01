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
#include <vd2/VDCapture/capdriver.h>
#include "capgraph.h"

///////////////////////////////////////////////////////////////////////////

class VDUICaptureGraphW32 : public VDUICustomControlW32, public IVDUICaptureGraph, public IVDCaptureProfiler {
public:
	VDUICaptureGraphW32();
	~VDUICaptureGraphW32();

	void *AsInterface(uint32 id);

	bool Create(IVDUIParameters *pParams) { return VDUICustomControlW32::Create(pParams); }

	void SetArea(const vduirect& r);

public:
	IVDCaptureProfiler *AsICaptureProfiler() { return this; }
	void Clear();

public:
	int RegisterStatsChannel(const char *name);
	void AddDataPoint(int channel, float value);

protected:
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void OnPaint();

	enum { kChannelCount = 4 };
	HBRUSH	mhbrChannels[kChannelCount];
	HBRUSH	mhbrFillR;
	HBRUSH	mhbrLines;

	uint32	mTick;

	struct Sample {
		uint32	mTick;
		float	mValue;
	};

	struct Channel {
		enum { kWindowSize = 4096 };

		const char *mName;
		uint32	mNextSample;
		Sample	mSamples[kWindowSize];
	};

	VDCriticalSection mChannelLock;
	Channel	mChannels[kChannelCount];
};

extern IVDUIWindow *VDCreateUICaptureGraph() { return new VDUICaptureGraphW32; }

VDUICaptureGraphW32::VDUICaptureGraphW32()
	: mhbrLines(CreateSolidBrush(RGB(48, 48, 48)))
{
	static const COLORREF kChannelColors[kChannelCount]={
		RGB(255, 0, 0),
		RGB(0, 255, 0),
		RGB(64, 128, 255),
		RGB(255, 255, 0)
	};

	for(int i=0; i<kChannelCount; ++i)
		mhbrChannels[i] = CreateSolidBrush(kChannelColors[i]);

	Clear();
}

VDUICaptureGraphW32::~VDUICaptureGraphW32() {
	for(int i=0; i<kChannelCount; ++i) {
		if (mhbrChannels[i])
			DeleteObject(mhbrChannels[i]);
	}
	if (mhbrLines)
		DeleteObject(mhbrLines);
}

void *VDUICaptureGraphW32::AsInterface(uint32 id) {
	switch(id) {
		case IVDUICaptureGraph::kTypeID: return static_cast<IVDUICaptureGraph *>(this); break;
	}

	return VDUICustomControlW32::AsInterface(id);
}

void VDUICaptureGraphW32::SetArea(const vduirect& r) {
	VDUICustomControlW32::SetArea(r);
	InvalidateRect(mhwnd, NULL, TRUE);
}

void VDUICaptureGraphW32::Clear() {
	for(int ch=0; ch<kChannelCount; ++ch) {
		mChannels[ch].mName = NULL;
		mChannels[ch].mNextSample = 0;
		memset(mChannels[ch].mSamples, 0, sizeof mChannels[ch].mSamples);
	}
}

int VDUICaptureGraphW32::RegisterStatsChannel(const char *name) {
	vdsynchronized(mChannelLock) {
		for(int ch=0; ch<kChannelCount; ++ch) {
			if (!mChannels[ch].mName) {
				mChannels[ch].mName = name;
				InvalidateRect(mhwnd, NULL, TRUE);
				return ch;
			}

			if (!strcmp(mChannels[ch].mName, name)) {
				return ch;
			}
		}
	}

	return -1;
}

void VDUICaptureGraphW32::AddDataPoint(int channel, float value) {
	if ((unsigned)channel >= kChannelCount)
		return;

	vdsynchronized(mChannelLock) {
		Channel& ch = mChannels[channel];
		Sample& s = ch.mSamples[(ch.mNextSample++) & (Channel::kWindowSize - 1)];

		// must be computed under lock or we can get samples added too late
		uint32 tick = (uint32)VDGetAccurateTick() / 10;
		s.mTick = tick;
		s.mValue = value;
	}
}

LRESULT VDUICaptureGraphW32::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_INITDIALOG:
		SetTimer(mhwnd, 1, 50, NULL);
		break;
	case WM_PAINT:
		OnPaint();
		break;
	case WM_ERASEBKGND:
		return FALSE;
	case WM_TIMER:
		{
			RECT r;
			GetClientRect(mhwnd, &r);
			uint32 newOffset = VDGetAccurateTick() / 10;
			uint32 delta = newOffset - mTick;

			// Note that we are doing an unsigned comparison here, so any backstep in time will
			// fail and cause a full refresh. This is intentional.
			RECT rGraphArea = { r.left + 200, r.top, r.right, r.bottom };
			if (delta < r.right)
				ScrollWindowEx(mhwnd, -(int)delta, 0, &rGraphArea, &rGraphArea, NULL, NULL, SW_ERASE | SW_INVALIDATE);
			else
				InvalidateRect(mhwnd, &rGraphArea, FALSE);

			mTick = newOffset;
		}
		break;
	}
	return VDUICustomControlW32::WndProc(msg, wParam, lParam);
}

void VDUICaptureGraphW32::OnPaint() {
	PAINTSTRUCT ps;

	if (HDC hdc = BeginPaint(mhwnd, &ps)) {
		RECT r;
		GetClientRect(mhwnd, &r);

		FillRect(hdc, &ps.rcPaint, (HBRUSH)GetStockObject(BLACK_BRUSH));

		if (ps.rcPaint.left < 300) {
			SelectObject(hdc, (HFONT)GetStockObject(DEFAULT_GUI_FONT));

			TEXTMETRIC tm;
			GetTextMetrics(hdc, &tm);
			SetTextAlign(hdc, TA_LEFT | TA_TOP);
			SetBkMode(hdc, TRANSPARENT);
			SetTextColor(hdc, RGB(255, 255, 255));

			vdsynchronized(mChannelLock) {
				int y = 0;
				for(int channel=0; channel<kChannelCount; ++channel) {
					const Channel& ch = mChannels[channel];

					if (!ch.mName)
						continue;

					HBRUSH hbr = mhbrChannels[channel];

					RECT rBlock = { r.left, y, r.left + 24, y + tm.tmHeight };
					RECT rText = { r.left+32, y, r.left + 200, y + tm.tmHeight };
					FillRect(hdc, &rBlock, hbr);

					ExtTextOut(hdc, rText.left, rText.top, ETO_CLIPPED, &rText, ch.mName, strlen(ch.mName), NULL);

					y += tm.tmHeight;
				}
			}
		}

		int leftBound = ps.rcPaint.left;

		if (leftBound < r.left + 200)
			leftBound = r.left + 200;

		// draw second lines
		int offset = (mTick % 100) + (r.right - ps.rcPaint.right);

		int x = offset - offset % 100;
		if (x < offset)
			x += 100;

		x = (r.right - 1) - offset;

		for(; x >= leftBound; x -= 100) {
			RECT rLine = { x, 0, x+1, r.bottom };
			FillRect(hdc, &rLine, mhbrLines);
		}

		// Display channel data.
		//
		// Note: Any drawing we do here has to be forward-compatible, i.e. adding a new
		// channel point should never change the way past samples are drawn. This is
		// required because we update by scrolling. Otherwise, adding a new sample point
		// could cause half of the window to be redrawn. Interpolating between points
		// with a line is thus out.

		int ycen = (r.bottom - 1) >> 1;
		float yscale = ycen * 0.5f;

		RECT rCenterLine = { leftBound, ycen, ps.rcPaint.right, ycen + 1 };
		FillRect(hdc, &rCenterLine, mhbrLines);

		vdsynchronized(mChannelLock) {
			for(int channel=0; channel<kChannelCount; ++channel) {
				const Channel& ch = mChannels[channel];
				HBRUSH hbr = mhbrChannels[channel];

				for(int pos=0; pos<Channel::kWindowSize; ++pos) {
					const Sample& samp = ch.mSamples[(ch.mNextSample - 1 - pos) & (Channel::kWindowSize - 1)];
					int x = r.right + samp.mTick - mTick;

					if (x >= ps.rcPaint.right)
						continue;

					if (x < leftBound)
						break;

					int y = ycen + VDRoundToInt(samp.mValue * yscale);

					RECT rDot = { x, y, x+1, y+1 };
					FillRect(hdc, &rDot, hbr);
				}
			}
		}

		EndPaint(mhwnd, &ps);
	}
}
