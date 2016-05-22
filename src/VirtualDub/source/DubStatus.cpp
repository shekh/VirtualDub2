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
#include <commctrl.h>
#include <vfw.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/time.h>
#include <vd2/system/w32assist.h>

#include "resource.h"
#include "gui.h"
#include "audio.h"

#include "AudioSource.h"
#include "VideoSource.h"
#include "InputFile.h"

#include "DubStatus.h"

#include "dub.h"

extern HWND g_hWnd;

///////////////////////////////////////////////////////////////////////////

class DubStatus : public IDubStatusHandler {
private:
	enum { TITLE_IDLE, TITLE_MINIMIZED, TITLE_NORMAL };

	int					iLastTitleMode;
	long				lLastTitleProgress;

	HWND				hwndStatus;
	UINT				statTimer;
	uint32				mStartTime;

	struct SamplePoint {
		uint32		mFramesProcessed;
		uint32		mTicks;
	};

	enum {
		kSamplePoints	= 32			// must be a power of two
	};

	enum {
		MYWM_UPDATE_BACKGROUND_STATE = WM_USER + 100,
		MYWM_NOTIFY_POSITION_CHANGE = WM_USER + 101
	};

	SamplePoint		mSamplePoints[kSamplePoints];
	int				mNextSamplePoint;
	int				mSampleCount;

	enum { MAX_FRAME_SIZES = 512 };

	uint32		mFrameSizes[MAX_FRAME_SIZES];
	long		lFrameFirstIndex, lFrameLastIndex;
	long		lFrameLobound, lFrameHibound;
	RECT				rStatusChild;
	HWND				hwndStatusChild;
	bool				fShowStatusWindow;
	bool				fFrozen;

	// our links...

	DubAudioStreamInfo	*painfo;
	DubVideoStreamInfo	*pvinfo;
	AudioStream			*audioStreamSource;

	IDubber				*pDubber;
	DubOptions			*opt;
	DubPositionCallback	mpPositionCallback;
	void *				mpPositionCallbackCookie;
	int					iPriority;				// current priority level index of processes

	int					mProgress;

	ModelessDlgNode		mModelessDialogNode;

	static INT_PTR CALLBACK StatusMainDlgProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
	static INT_PTR CALLBACK StatusVideoDlgProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
	static INT_PTR CALLBACK StatusPerfDlgProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
	static INT_PTR CALLBACK StatusLogDlgProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
	static INT_PTR CALLBACK StatusDlgProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
	void StatusTimerProc(HWND hWnd);

	static int iPriorities[][2];

public:
	DubStatus();
	~DubStatus();
	void InitLinks(	DubAudioStreamInfo	*painfo,
		DubVideoStreamInfo	*pvinfo,
		AudioStream			*audioStreamSource,

		IDubber				*pDubber,
		DubOptions			*opt);
	HWND Display(HWND hwndParent, int iInitialPriority);
	void Destroy();
	void DumpStatus();
	bool ToggleStatus();
	void SetPositionCallback(DubPositionCallback dpc, void *cookie);
	void NotifyNewFrame(uint32 size, bool isKey);
	void SetLastPosition(VDPosition pos, bool fast_update);
	void NotifyPositionChange(){ PostMessage(hwndStatus, MYWM_NOTIFY_POSITION_CHANGE,0,0); }
	void Freeze();
	bool isVisible();
	bool isFrameVisible(bool fOutput);
	bool ToggleFrame(bool fOutput);
	void OnBackgroundStateUpdated();
};

IDubStatusHandler *CreateDubStatusHandler() {
	return new DubStatus();
}

///////////////////////////////////////////////////////////////////////////

extern wchar_t g_szInputAVIFile[];
extern HINSTANCE g_hInst;
extern HWND g_hWnd;

///////////////////////////////////////////////////////////////////////////

static long pickClosestNiceBound(long val, bool higher) {

	static long bounds[]={
		0,
		100,
		200,
		500,
		1024,
		2048,
		5120,
		10240,
		20480,
		51200,
		102400,
		204800,
		512000,
		1048576,
		2097152,
		5242880,
		10485760,
		20971520,
		52428800,
		104857600,
		209715200,
		524288000,
		1073741824,
		2147483647,
	};

	int i;

	// silly value?

	if (val <= 0)
		return 0;

	if (!higher) {
		for(i=0; i<23; i++)
			if (bounds[i] <= val)
				return bounds[i];

		return 0x7FFFFFFF;
	} else {
		for(i=23; i>0; i--)
			if (bounds[i-1] < val)
				break;

		return bounds[i];
	}
}

///////////////////////////////////////////////////////////////////////////

DubStatus::DubStatus()
	: iLastTitleMode(TITLE_IDLE)
	, lLastTitleProgress(0)
	, hwndStatus(NULL)
	, mNextSamplePoint(0)
	, mSampleCount(0)
	, lFrameFirstIndex(0)
	, lFrameLastIndex(0)
	, fShowStatusWindow(true)
	, fFrozen(false)
	, mpPositionCallback(NULL)
	, mProgress(0)
{
	memset(mFrameSizes, 0, sizeof mFrameSizes);
	memset(mSamplePoints, 0, sizeof mSamplePoints);
}

DubStatus::~DubStatus() {
	Destroy();
}

void DubStatus::InitLinks(	DubAudioStreamInfo	*painfo,
	DubVideoStreamInfo	*pvinfo,
	AudioStream			*audioStreamSource,

	IDubber				*pDubber,
	DubOptions			*opt) {

	this->painfo			= painfo;
	this->pvinfo			= pvinfo;
	this->audioStreamSource	= audioStreamSource;

	this->pDubber			= pDubber;
	this->opt				= opt;
}


HWND DubStatus::Display(HWND hwndParent, int iInitialPriority) {
	iPriority = iInitialPriority;

	if (GetVersion()&0x80000000)
		hwndStatus = CreateDialogParamA(g_hInst, MAKEINTRESOURCEA(IDD_DUBBING), hwndParent, StatusDlgProc, (LPARAM)this);
	else
		hwndStatus = CreateDialogParamW(g_hInst, MAKEINTRESOURCEW(IDD_DUBBING), hwndParent, StatusDlgProc, (LPARAM)this);

	if (hwndStatus) {
		fShowStatusWindow = opt->fShowStatus || opt->mbForceShowStatus;

		if (fShowStatusWindow) {
			SetWindowLong(hwndStatus, GWL_STYLE, GetWindowLong(hwndStatus, GWL_STYLE) & ~WS_POPUP);

			// Check the status of the main window. If it is minimized, minimize the status
			// window too. This is a bit of an illegal tunnel but it is useful for now.

			if (IsIconic(g_hWnd) && !opt->mbForceShowStatus)
				ShowWindow(hwndStatus, SW_SHOWMINNOACTIVE);
			else {
				// app in front
				HWND hwndForeground = ::GetForegroundWindow();
				bool foreground = false;

				if (hwndForeground) {
					DWORD pid;
					::GetWindowThreadProcessId(hwndForeground, &pid);

					foreground = (pid == ::GetCurrentProcessId());
				}

				ShowWindow(hwndStatus, foreground ? SW_SHOW : SW_SHOWNOACTIVATE);
			}

			SetFocus(GetDlgItem(hwndStatus, IDC_DRAW_INPUT));
		}

		return hwndStatus;
	}
	return NULL;
}

void DubStatus::Destroy() {
	if (hwndStatus) {
		DestroyWindow(hwndStatus);
		hwndStatus = NULL;
	}
}


///////////////////////////////////////////////////////////////////////////

void DubStatus::StatusTimerProc(HWND hWnd) {
	sint64 nProjSize;
	char buf[256];

	sint64	totalVSamples	= pvinfo->end_proc_dst;
	sint64	totalASamples	= audioStreamSource ? audioStreamSource->GetLength() : 0;
	sint64	curVSample		= pvinfo->cur_proc_dst;
	sint64	curASample		= audioStreamSource ? audioStreamSource->GetSampleCount() : 0;
	char	*s;
	bool	bPreloading = false;

	/////////////

	if (curVSample<0) {
		curVSample = 0;
		bPreloading = true;
	}

	int nProgress	= 0;
	
	if (totalVSamples)
		nProgress = curVSample>=totalVSamples ? 4096 : (int)((curVSample << 12) / totalVSamples);

	if (totalASamples)
		nProgress += curASample>=totalASamples ? 4096 : (int)((curASample << 12) / totalASamples);

	if (!totalASamples || !totalVSamples || pvinfo->fAudioOnly) nProgress *= 2;

	if (bPreloading) {
		SetDlgItemText(hWnd, IDC_CURRENT_VFRAME, "Preloading...");
	} else {
		sprintf(buf, "%I64d/%I64d", curVSample, totalVSamples);
		SetDlgItemText(hWnd, IDC_CURRENT_VFRAME, buf);
	}

	sprintf(buf, "%I64d/%I64d", curASample, totalASamples);
	SetDlgItemText(hWnd, IDC_CURRENT_ASAMPLE, buf);
 
	size_to_str(buf, sizeof buf / sizeof buf[0], pvinfo->total_size);

	if (pvinfo->processed) {
		s = buf;
		while(*s)
			++s;

		sint64 kilobytes = (pvinfo->total_size + 512) >> 10;
		double kbPerFrame = (double)kilobytes / (double)pvinfo->processed;
		double framesPerSecond = pvinfo->mFrameRate.asDouble();

		sprintf(s, " (%3ldKB/s)", VDRoundToInt(kbPerFrame * framesPerSecond));
	}

	SetDlgItemText(hWnd, IDC_CURRENT_VSIZE, buf);

	size_to_str(buf, sizeof buf / sizeof buf[0], painfo->total_size);
	SetDlgItemText(hWnd, IDC_CURRENT_ASIZE, buf);

	nProjSize = 0;
	if (totalVSamples && curVSample) {
		double invVideoProgress = std::max<double>((double)totalVSamples / (double)curVSample, 1.0);

		nProjSize += VDRoundToInt64(pvinfo->total_size * invVideoProgress);
	}
	if (totalASamples && curASample) {
		double invAudioProgress = std::max<double>((double)totalASamples / (double)curASample, 1.0);
		nProjSize += VDRoundToInt64(painfo->total_size * invAudioProgress);
	}

	if (nProjSize) {
		nProjSize += 2048 + 16;

		sint64 kilobytes = (nProjSize+1023)>>10;

		if (kilobytes < 65536)
			wsprintf(buf, "%ldK", kilobytes);
		else {
			kilobytes = (nProjSize*100) >> 20;
			wsprintf(buf, "%ld.%02dMB", (LONG)(kilobytes/100), (LONG)(kilobytes%100));
		}
		SetDlgItemText(hWnd, IDC_PROJECTED_FSIZE, buf);
	} else {
		SetDlgItemText(hWnd, IDC_PROJECTED_FSIZE, "unknown");
	}

	uint32 dwTicks = VDGetCurrentTick() - mStartTime;
	ticks_to_str(buf, sizeof buf / sizeof buf[0], dwTicks);
	SetDlgItemText(hWnd, IDC_TIME_ELAPSED, buf);

	if (nProgress > 16) {
		ticks_to_str(buf, sizeof buf / sizeof buf[0], MulDiv(dwTicks,8192,nProgress));
		SetDlgItemText(hWnd, IDC_TIME_REMAINING, buf);
	}

	sint32 fps100 = 0;
	sint64 nFramesProcessed = pvinfo->processed;

	if (nFramesProcessed) {
		SamplePoint& prevPt = mSamplePoints[(mNextSamplePoint-1)&(kSamplePoints-1)];

		if (prevPt.mFramesProcessed != (uint32)nFramesProcessed) {
			SamplePoint& currentPt = mSamplePoints[mNextSamplePoint];
			currentPt.mTicks				= pvinfo->lastProcessedTimestamp - mStartTime;
			currentPt.mFramesProcessed		= (uint32)nFramesProcessed;		// Yes, this truncates.  We'll be okay as long as no more than 2^32-1 frames are processed per interval.

			if (mSampleCount < kSamplePoints)
				++mSampleCount;

			mNextSamplePoint = (mNextSamplePoint + 1) & (kSamplePoints - 1);
		}

		SamplePoint& frontPt = mSamplePoints[(mNextSamplePoint-1) & (kSamplePoints-1)];
		for(int distance = 2; distance < mSampleCount-1; ++distance) {
			const SamplePoint& lastPt = mSamplePoints[(mNextSamplePoint - distance - 1) & (kSamplePoints - 1)];
			const uint32	tickDelta = frontPt.mTicks - lastPt.mTicks;

			if (!tickDelta)
				break;

			const uint32	frameDelta = frontPt.mFramesProcessed - lastPt.mFramesProcessed;

			fps100 = (sint32)(((sint64)frameDelta*100000 + (sint32)(tickDelta>>1)) / (sint32)tickDelta);

			if (fps100 > 100000 / distance)
				break;
		}
	}

	wsprintf(buf, "%u.%02u fps", fps100/100, fps100%100);
	SetDlgItemText(hWnd, IDC_FPS, buf);

	if (GetWindowLong(g_hWnd, GWL_STYLE) & WS_MINIMIZE) {
		long lNewProgress = (nProgress*25)/2048;

		if (iLastTitleMode != TITLE_MINIMIZED || lLastTitleProgress != lNewProgress) {
			guiSetTitleW(g_hWnd, IDS_TITLE_DUBBING_MINIMIZED, lNewProgress, VDFileSplitPath(g_szInputAVIFile));

			iLastTitleMode = TITLE_MINIMIZED;
			lLastTitleProgress = lNewProgress;
		}
	} else {
		if (iLastTitleMode != TITLE_NORMAL) {
			iLastTitleMode = TITLE_NORMAL;
			guiSetTitleW(g_hWnd, IDS_TITLE_DUBBING, VDFileSplitPath(g_szInputAVIFile));
		}
	}
}

///////////////////////////////////

INT_PTR CALLBACK DubStatus::StatusMainDlgProc( HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam) {
	DubStatus *thisPtr = (DubStatus *)GetWindowLongPtr(hdlg, DWLP_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			{
				SetWindowLongPtr(hdlg, DWLP_USER, lParam);
				thisPtr = (DubStatus *)lParam;
				SetWindowPos(hdlg, HWND_TOP, thisPtr->rStatusChild.left, thisPtr->rStatusChild.top, 0, 0, SWP_NOSIZE|SWP_NOACTIVATE);

				thisPtr->StatusTimerProc(hdlg);
			}
            return FALSE;

		case WM_TIMER:
			thisPtr->StatusTimerProc(hdlg);
			return TRUE;

    }
    return FALSE;
}



INT_PTR CALLBACK DubStatus::StatusVideoDlgProc( HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam) {
	DubStatus *thisPtr = (DubStatus *)GetWindowLongPtr(hdlg, DWLP_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			{
				SetWindowLongPtr(hdlg, DWLP_USER, lParam);
				thisPtr = (DubStatus *)lParam;
				SetWindowPos(hdlg, HWND_TOP, thisPtr->rStatusChild.left, thisPtr->rStatusChild.top, 0, 0, SWP_NOSIZE|SWP_NOACTIVATE);

				thisPtr->lFrameLobound = 0;
				thisPtr->lFrameHibound = 10240;
			}
            return FALSE;

		case WM_TIMER:
			{
				HDC hdc;
				RECT r;
				RECT rUpdate;
				int dx;

				dx = thisPtr->lFrameLastIndex - thisPtr->lFrameFirstIndex;

				if (dx > 0) {
					long lo, hi;
					int i;

					r.left = r.top = 7;
					r.right = 7 + 133;
					r.bottom = 7 + 72;

					MapDialogRect(hdlg, &r);

					// scan the array and recompute bounds

					lo = 0x7FFFFFFF;
					hi = 0;

					for(i=r.left - r.right; i<0; i++) {
						if (thisPtr->lFrameFirstIndex + i >= 0) {
							long size = thisPtr->mFrameSizes[(thisPtr->lFrameFirstIndex+i) & (MAX_FRAME_SIZES-1)] & 0x7FFFFFFF;

							if (size < lo)
								lo = size;

							if (size > hi)
								hi = size;
						}
					}

					// compute "nice" bounds

					if (lo == 0x7FFFFFFF)
						lo = 0;

					lo = pickClosestNiceBound(lo, false);
					hi = pickClosestNiceBound(hi, true);

					if (lo == hi)
						hi = pickClosestNiceBound(hi+1, true);

					// if the bounds are different, force a full redraw, else scroll

					thisPtr->lFrameFirstIndex += dx;
					if (lo != thisPtr->lFrameLobound || hi != thisPtr->lFrameHibound) {
						char buf[64];

						thisPtr->lFrameLobound = lo;
						thisPtr->lFrameHibound = hi;

						if (lo >= 0x40000000)
							wsprintf(buf, "%dGB", lo>>30);
						else if (lo >= 0x100000)
							wsprintf(buf, "%dMB", lo>>20);
						else if (lo >= 0x400)
							wsprintf(buf, "%dK", lo>>10);
						else
							wsprintf(buf, "%d", lo);
						SetDlgItemText(hdlg, IDC_STATIC_LOBOUND, buf);

						if (hi >= 0x40000000)
							wsprintf(buf, "%dGB", hi>>30);
						else if (hi >= 0x100000)
							wsprintf(buf, "%dMB", hi>>20);
						else if (hi >= 0x400)
							wsprintf(buf, "%dK", hi>>10);
						else
							wsprintf(buf, "%d", hi);
						SetDlgItemText(hdlg, IDC_STATIC_HIBOUND, buf);

						InvalidateRect(hdlg, &r, FALSE);
					} else if (hdc = GetDC(hdlg)) {

						ScrollDC(hdc, -dx, 0, &r, &r, NULL, &rUpdate);

						rUpdate.left = r.right - dx;
						rUpdate.right = r.right;
						rUpdate.top = r.top;
						rUpdate.bottom = r.bottom;

						InvalidateRect(hdlg, &rUpdate, FALSE);

						ReleaseDC(hdlg, hdc);
					}
				}

			}
			return TRUE;

		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hdc;
				RECT r, r2;
				RECT rDest;
				HBRUSH hbrRed, hbrBlue;
				int x, width, height;
				long range = thisPtr->lFrameHibound - thisPtr->lFrameLobound;

				if (!range) ++range;

				r.left = r.top = 7;
				r.right = 7+133;
				r.bottom = 72;

				MapDialogRect(hdlg, &r);

				width = r.right - r.left;
				height = r.bottom - r.top;

				hdc = BeginPaint(hdlg, &ps);

				IntersectRect(&rDest, &r, &ps.rcPaint);

				FillRect(hdc, &rDest, (HBRUSH)GetStockObject(BLACK_BRUSH));

				hbrRed = CreateSolidBrush(RGB(255,0,0));
				hbrBlue = CreateSolidBrush(RGB(0,0,255));

				for(x=rDest.left; x<rDest.right; x++) {
					DWORD dwSize;
					int y;

					if (thisPtr->lFrameFirstIndex+x-r.right >= 0) {
						dwSize = thisPtr->mFrameSizes[(thisPtr->lFrameFirstIndex+x-r.right) & (MAX_FRAME_SIZES-1)];

						y = (((dwSize & 0x7FFFFFFF) - thisPtr->lFrameLobound)*height + range - 1)/range;
						if (y > height)
							y = height;

						if (y>0) {

							r2.left = x;
							r2.right = x+1;
							r2.top = r.bottom - y;
							r2.bottom = r.bottom;

							FillRect(hdc, &r2, dwSize & 0x80000000 ? hbrBlue : hbrRed);
						}
					}
				}

				DeleteObject(hbrBlue);
				DeleteObject(hbrRed);

				EndPaint(hdlg, &ps);
			}
			return TRUE;

    }
    return FALSE;
}

INT_PTR CALLBACK DubStatus::StatusPerfDlgProc( HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam) {
	DubStatus *thisPtr = (DubStatus *)GetWindowLongPtr(hdlg, DWLP_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			{
				SetWindowLongPtr(hdlg, DWLP_USER, lParam);
				thisPtr = (DubStatus *)lParam;
				SetWindowPos(hdlg, HWND_TOP, thisPtr->rStatusChild.left, thisPtr->rStatusChild.top, 0, 0, SWP_NOSIZE|SWP_NOACTIVATE);

			}
            return FALSE;

		case WM_TIMER:
			{
				VDDubPerfStatus status;
				thisPtr->pDubber->GetPerfStatus(status);

				VDSetWindowTextFW32(GetDlgItem(hdlg, IDC_STATIC_VIDEOBUFFER), L"%u/%u", status.mVideoBuffersActive, status.mVideoBuffersTotal);
				VDSetWindowTextFW32(GetDlgItem(hdlg, IDC_STATIC_VIDEOREQUESTS), L"%u", status.mVideoRequestsActive);
				VDSetWindowTextFW32(GetDlgItem(hdlg, IDC_STATIC_AUDIOBUFFER), L"%u/%uKB", (status.mAudioBufferInUse + 512) >> 10, (status.mAudioBufferTotal + 512) >> 10);
				VDSetWindowTextFW32(GetDlgItem(hdlg, IDC_STATIC_IOTIME), L"%.0f%%", status.mIOActivityRatio * 100.0f);
				VDSetWindowTextFW32(GetDlgItem(hdlg, IDC_STATIC_PROCTIME), L"%.0f%%", status.mProcActivityRatio * 100.0f);
			}
			return TRUE;

		case WM_COMMAND:
			if (wParam == IDC_DUMPSTATUS) {
				thisPtr->DumpStatus();
				return TRUE;
			}
			break;

    }
    return FALSE;
}

INT_PTR CALLBACK DubStatus::StatusLogDlgProc( HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam) {
	DubStatus *thisPtr = (DubStatus *)GetWindowLongPtr(hdlg, DWLP_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			{
				SetWindowLongPtr(hdlg, DWLP_USER, lParam);
				thisPtr = (DubStatus *)lParam;
				SetWindowPos(hdlg, HWND_TOP, thisPtr->rStatusChild.left, thisPtr->rStatusChild.top, 0, 0, SWP_NOSIZE|SWP_NOACTIVATE);

				IVDLogWindowControl *pLogWin = VDGetILogWindowControl(GetDlgItem(hdlg, IDC_LOG));
				pLogWin->AttachAsLogger(false);
			}
            return FALSE;

		case WM_TIMER:
			return TRUE;

    }
    return FALSE;
}

///////////////////////////////////

const char * const g_szDubPriorities[]={
		"Idle",
		"Lowest",
		"Even lower",
		"Lower",
		"Normal",
		"Higher",
		"Even higher",
		"Highest",
};

INT_PTR CALLBACK DubStatus::StatusDlgProc( HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam) {

	static struct DubStatusTabs {
		LPTSTR	rsrc;
		char	*name;
		DLGPROC	dProc;
	} tabs[]={
		{	MAKEINTRESOURCE(IDD_DUBBING_MAIN),	"Main",		StatusMainDlgProc	},
		{	MAKEINTRESOURCE(IDD_DUBBING_VIDEO),	"Video",	StatusVideoDlgProc	},
		{	MAKEINTRESOURCE(IDD_DUBBING_PERF),	"Perf",		StatusPerfDlgProc	},
		{	MAKEINTRESOURCE(IDD_DUBBING_LOG),	"Log",		StatusLogDlgProc	},
	};

	DubStatus *thisPtr = (DubStatus *)GetWindowLongPtr(hdlg, DWLP_USER);
	HWND hwndItem;
	RECT r, r2;
	int i;

#define MYWM_NULL (WM_APP + 0)

    switch (message)
    {
        case WM_INITDIALOG:
			{
				long xoffset, yoffset;

				SetWindowLongPtr(hdlg, DWLP_USER, lParam);
				thisPtr = (DubStatus *)lParam;

				thisPtr->hwndStatus = hdlg;

				VDSetDialogDefaultIcons(hdlg);

				// must do this before any timer requests occur
				thisPtr->mStartTime		= VDGetCurrentTick();

				// Initialize tab window

				hwndItem = GetDlgItem(hdlg, IDC_TABS);

				for(i=0; i<(sizeof tabs/sizeof tabs[0]); i++) {
					TC_ITEM ti;

					ti.mask		= TCIF_TEXT;
					ti.pszText	= tabs[i].name;

					TabCtrl_InsertItem(hwndItem, i, &ti);
				}

				// Compute size of tab control needed to hold this child dialog

				r.left = r.top = 0;
				r.right = 172;
				r.bottom = 102;
				MapDialogRect(hdlg, &r);

				TabCtrl_AdjustRect(hwndItem, TRUE, &r);

				// Resize tab control and compute offsets for other controls

				GetWindowRect(hwndItem, &r2);
				ScreenToClient(hdlg, (LPPOINT)&r2 + 0);
				ScreenToClient(hdlg, (LPPOINT)&r2 + 1);

				OffsetRect(&r, r2.left - r.left, r2.top - r.top);

				SetWindowPos(hwndItem, NULL, r.left, r.top, r.right-r.left, r.bottom-r.top, SWP_NOZORDER|SWP_NOACTIVATE);
				thisPtr->rStatusChild = r;

				TabCtrl_AdjustRect(hwndItem, FALSE, &thisPtr->rStatusChild);

				xoffset = (r.right-r.left) - (r2.right-r2.left);
				yoffset = (r.bottom-r.top) - (r2.bottom-r2.top);

				guiResizeDlgItem(hdlg, IDC_PROGRESS, 0, yoffset, xoffset, 0);
				guiResizeDlgItem(hdlg, IDC_PRIORITY, 0, yoffset, xoffset, 0);
				guiResizeDlgItem(hdlg, IDC_LIMIT, 0, yoffset, xoffset, 0);
				guiOffsetDlgItem(hdlg, IDC_ABORT, xoffset, yoffset);
				guiOffsetDlgItem(hdlg, IDC_STATIC_PROGRESS, 0, yoffset);
				guiOffsetDlgItem(hdlg, IDC_STATIC_PRIORITY, 0, yoffset);
				guiOffsetDlgItem(hdlg, IDC_STATIC_LIMIT, 0, yoffset);
				guiOffsetDlgItem(hdlg, IDC_DRAW_INPUT, 0, yoffset);
				guiOffsetDlgItem(hdlg, IDC_DRAW_OUTPUT, 0, yoffset);
				guiOffsetDlgItem(hdlg, IDC_DRAW_DOUTPUT, 0, yoffset);
				guiOffsetDlgItem(hdlg, IDC_BACKGROUND, 0, yoffset);

				// resize us

				GetWindowRect(hdlg, &r);
				SetWindowPos(hdlg, NULL, 0, 0, r.right-r.left + xoffset, r.bottom-r.top + yoffset, SWP_NOMOVE | SWP_NOZORDER|SWP_NOACTIVATE);

				// open up child dialog

				thisPtr->hwndStatusChild = CreateDialogParam(g_hInst, tabs[0].rsrc, hdlg, tabs[0].dProc, (LPARAM)thisPtr);

				// setup timer, progress bar

				thisPtr->statTimer = SetTimer(hdlg, 1, 500, NULL);
				SendMessage(GetDlgItem(hdlg, IDC_PROGRESS), PBM_SETRANGE, 0, MAKELPARAM(0, 8192));

				CheckDlgButton(hdlg, IDC_DRAW_INPUT, thisPtr->opt->video.fShowInputFrame);
				CheckDlgButton(hdlg, IDC_DRAW_OUTPUT, thisPtr->opt->video.fShowOutputFrame);
				CheckDlgButton(hdlg, IDC_DRAW_DOUTPUT, thisPtr->opt->video.fShowDecompressedFrame);

				if (VDIsAtLeastVistaW32())
					CheckDlgButton(hdlg, IDC_BACKGROUND, thisPtr->pDubber->IsBackground());
				else {
					HWND hwndItem = GetDlgItem(hdlg, IDC_BACKGROUND);

					if (hwndItem)
						ShowWindow(hwndItem, SW_HIDE);
				}

				hwndItem = GetDlgItem(hdlg, IDC_PRIORITY);
				SendMessage(hwndItem, CB_RESETCONTENT,0,0);
				for(i=0; i<8; i++)
					SendMessage(hwndItem, CB_ADDSTRING, 0, (LPARAM)g_szDubPriorities[i]);

				SendMessage(hwndItem, CB_SETCURSEL, thisPtr->iPriority-1, 0);

				hwndItem = GetDlgItem(hdlg, IDC_LIMIT);
				SendMessage(hwndItem, TBM_SETRANGE, TRUE, MAKELONG(0, 10));
				SendMessage(hwndItem, TBM_SETPOS, TRUE, (thisPtr->opt->mThrottlePercent + 5) / 10);

				guiSetTitleW(hdlg, IDS_TITLE_STATUS, VDFileSplitPath(g_szInputAVIFile));

			}
			thisPtr->mModelessDialogNode.hdlg = hdlg;
			guiAddModelessDialog(&thisPtr->mModelessDialogNode);
            return FALSE;

		case WM_DESTROY:
			thisPtr->mModelessDialogNode.Remove();
			thisPtr->hwndStatus = NULL;
			if (thisPtr->statTimer)
				KillTimer(hdlg, thisPtr->statTimer);
			PostMessage(GetParent(hdlg), MYWM_NULL, 0, 0);
			return TRUE;

		case MYWM_NOTIFY_POSITION_CHANGE:
			if (thisPtr->pvinfo->cur_proc_src >= 0)
				thisPtr->SetLastPosition(thisPtr->pvinfo->cur_proc_src, true);
			return TRUE;

		case WM_TIMER:
			if (thisPtr->fFrozen)
				return TRUE;

			if (thisPtr->pvinfo->cur_proc_src >= 0)
				thisPtr->SetLastPosition(thisPtr->pvinfo->cur_proc_src, false);

			if (thisPtr->hwndStatusChild)
				SendMessage(thisPtr->hwndStatusChild, WM_TIMER, 0, 0);

			{
				sint64	totalVSamples	= thisPtr->pvinfo->end_proc_dst;
				sint64	totalASamples	= thisPtr->audioStreamSource ? thisPtr->audioStreamSource->GetLength() : 0;
				sint64	curVSample		= thisPtr->pvinfo->cur_proc_dst;
				sint64	curASample		= thisPtr->audioStreamSource ? thisPtr->audioStreamSource->GetSampleCount() : 0;

				/////////////

				if (curVSample<0)
					curVSample = 0;

				int nProgress = 0;
				
				if (totalVSamples && !thisPtr->pvinfo->fAudioOnly)
					nProgress += curVSample>=totalVSamples ? 4096 : (int)((curVSample << 12) / totalVSamples);

				if (totalASamples)
					nProgress += curASample>totalASamples ? 4096 : (int)((curASample << 12) / totalASamples);

				if (!totalASamples || !totalVSamples || thisPtr->pvinfo->fAudioOnly) nProgress *= 2;

				thisPtr->mProgress = nProgress;

				SendMessage(GetDlgItem(hdlg, IDC_PROGRESS), PBM_SETPOS,	(WPARAM)nProgress, 0);
			}

			thisPtr->pDubber->UpdateFrames();

			return TRUE;

		case WM_NOTIFY: {
			NMHDR *nm = (LPNMHDR)lParam;

			switch(nm->code) {
			case TCN_SELCHANGE:
				{
					int iTab = TabCtrl_GetCurSel(nm->hwndFrom);

					if (iTab>=0) {
						if (thisPtr->hwndStatusChild)
							DestroyWindow(thisPtr->hwndStatusChild);

						thisPtr->hwndStatusChild = CreateDialogParam(g_hInst, tabs[iTab].rsrc, hdlg, tabs[iTab].dProc, (LPARAM)thisPtr);
					}
				}
				return TRUE;
			}
			}break;

        case WM_COMMAND:                      
			switch(LOWORD(wParam)) {
			case IDC_DRAW_INPUT:
				thisPtr->opt->video.fShowInputFrame = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)==BST_CHECKED;
				break;

			case IDC_DRAW_OUTPUT:
				thisPtr->opt->video.fShowOutputFrame = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)==BST_CHECKED;
				break;

			case IDC_DRAW_DOUTPUT:
				thisPtr->opt->video.fShowDecompressedFrame = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)==BST_CHECKED;
				break;

			case IDC_PRIORITY:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					LRESULT index;

					if (CB_ERR != (index = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0))) {
						thisPtr->pDubber->SetPriority(index);
					}
				}
				break;

			case IDC_BACKGROUND:
				thisPtr->pDubber->SetBackground(IsDlgButtonChecked(hdlg, IDC_BACKGROUND) != 0);
				break;

			case IDC_ABORT:
				extern bool VDPreferencesIsRenderAbortConfirmEnabled();

				if (thisPtr->pDubber->IsPreviewing() || !VDPreferencesIsRenderAbortConfirmEnabled() ||
					IDOK == MessageBox(hdlg, "Stop the operation at this point?", "VirtualDub Warning", MB_ICONEXCLAMATION|MB_OKCANCEL))
				{
					SendMessage(hdlg, WM_SETTEXT, 0, (LPARAM)"Aborting...");
					EnableWindow((HWND)lParam, FALSE);
					thisPtr->pDubber->Abort();
					thisPtr->hwndStatus = NULL;
					DestroyWindow(hdlg);
				}
				break;

			case IDCANCEL:
				_RPT0(0,"Received cancel\n");
				thisPtr->ToggleStatus();
				break;
            }
            break;

		case WM_HSCROLL:
			if (lParam) {
				HWND hwndScroll = (HWND)lParam;
				switch(GetWindowLong(hwndScroll, GWL_ID)) {
				case IDC_LIMIT:
					{
						int pos = SendMessage(hwndScroll, TBM_GETPOS, 0, 0);

						thisPtr->pDubber->SetThrottleFactor(pos / 10.0f);
					}
					break;
				}
			}
			break;

		case MYWM_UPDATE_BACKGROUND_STATE:
			CheckDlgButton(hdlg, IDC_BACKGROUND, thisPtr->pDubber->IsBackground());
			return TRUE;
    }
    return FALSE;
}

namespace {
	class VectorStream : public IVDStream {
	public:
		void Finalize();
		const char *GetText() const { return mBuffer.data(); }
		uint32 GetLength() const { return mBuffer.size(); }

		virtual const wchar_t *GetNameForError();
		virtual sint64 Pos();
		virtual void Read(void *buffer, sint32 bytes);
		virtual sint32 ReadData(void *buffer, sint32 bytes);
		virtual void Write(const void *buffer, sint32 bytes);

	protected:
		uint32 mPos;

		vdfastvector<char> mBuffer;
	};

	void VectorStream::Finalize() {
		mBuffer.push_back(0);
	}

	const wchar_t *VectorStream::GetNameForError() {
		return L"";
	}

	sint64 VectorStream::Pos() {
		return mBuffer.size();
	}

	void VectorStream::Read(void *buffer, sint32 bytes) {
	}

	sint32 VectorStream::ReadData(void *buffer, sint32 bytes) {
		return 0;
	}

	void VectorStream::Write(const void *buffer, sint32 bytes) {
		if (bytes > 0)
			mBuffer.insert(mBuffer.end(), (const char *)buffer, (const char *)buffer + bytes);
	}

	INT_PTR CALLBACK DumpStatusDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
		switch(msg) {
			case WM_INITDIALOG:
				{
					HWND hwndEdit = GetDlgItem(hdlg, IDC_EDIT);

					if (hwndEdit) {
						::SetFocus(hwndEdit);
						::SetWindowTextA(hwndEdit, (const char *)lParam);
						::SendMessage(hwndEdit, EM_SETSEL, 0, 0);
					}
				}
				return FALSE;

			case WM_SIZE:
				{
					int w = (int)LOWORD(lParam);
					int h = (int)HIWORD(lParam);

					HWND hwndEdit = GetDlgItem(hdlg, IDC_EDIT);

					if (hwndEdit)
						::SetWindowPos(hwndEdit, NULL, 0, 0, w, h, SWP_NOZORDER|SWP_NOACTIVATE);
				}
				return 0;

			case WM_COMMAND:
				switch(wParam) {
					case IDCANCEL:
					case IDOK:
						EndDialog(hdlg, 0);
						return TRUE;
				}
				break;
		}

		return FALSE;
	}
}

void DubStatus::DumpStatus() {
	VectorStream vs;
	VDTextOutputStream out(&vs);

	pDubber->DumpStatus(out);
	out.Flush();

	vs.Finalize();

	DialogBoxParamA(g_hInst, MAKEINTRESOURCE(IDD_DUMPSTATUS), hwndStatus, DumpStatusDlgProc, (LPARAM)vs.GetText());
}

bool DubStatus::ToggleStatus() {

	fShowStatusWindow = !fShowStatusWindow;

	if (hwndStatus) {
		if (fShowStatusWindow) {
			SetWindowLong(hwndStatus, GWL_STYLE, GetWindowLong(hwndStatus, GWL_STYLE) & ~WS_POPUP);
			ShowWindow(hwndStatus, SW_SHOW);
		} else {
			SetWindowLong(hwndStatus, GWL_STYLE, GetWindowLong(hwndStatus, GWL_STYLE) | WS_POPUP);
			ShowWindow(hwndStatus, SW_HIDE);
		}
	}

	return fShowStatusWindow;
}

void DubStatus::SetPositionCallback(DubPositionCallback dpc, void *cookie) {
	mpPositionCallback			= dpc;
	mpPositionCallbackCookie	= cookie;
}

void DubStatus::NotifyNewFrame(uint32 size, bool isKey) {
	mFrameSizes[(lFrameLastIndex++)&(MAX_FRAME_SIZES-1)] = size | (isKey ? 0 : 0x80000000);
}

void DubStatus::SetLastPosition(VDPosition pos, bool fast_update) {
		if (mpPositionCallback)
			mpPositionCallback(
					pvinfo->start_src,
					pos < pvinfo->start_src
							? pvinfo->start_src
							: pos > pvinfo->end_src
									? pvinfo->end_src
									: pos,
					pvinfo->end_src, mProgress, fast_update, mpPositionCallbackCookie);
}

void DubStatus::Freeze() {
	fFrozen = true;
}

bool DubStatus::isVisible() {
	return fShowStatusWindow;
}

bool DubStatus::isFrameVisible(bool fOutput) {
	return !!(fOutput ? opt->video.fShowOutputFrame : opt->video.fShowInputFrame);
}

bool DubStatus::ToggleFrame(bool fFrameOutput) {
	if (fFrameOutput) {
		if (hwndStatus)
			PostMessage(GetDlgItem(hwndStatus, IDC_DRAW_OUTPUT), BM_SETCHECK, !opt->video.fShowOutputFrame ? BST_CHECKED : BST_UNCHECKED, 0);
		return opt->video.fShowOutputFrame = !opt->video.fShowOutputFrame;
	} else {
		if (hwndStatus)
			PostMessage(GetDlgItem(hwndStatus, IDC_DRAW_INPUT), BM_SETCHECK, !opt->video.fShowInputFrame ? BST_CHECKED : BST_UNCHECKED, 0);
		return opt->video.fShowInputFrame = !opt->video.fShowInputFrame;
	}
}

void DubStatus::OnBackgroundStateUpdated() {
	PostMessage(hwndStatus, MYWM_UPDATE_BACKGROUND_STATE, 0, 0);
}
