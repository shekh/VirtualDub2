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

#define f_POSITIONCONTROL_CPP

#include <windows.h>
#include <commctrl.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/fraction.h>
#include <vd2/system/w32assist.h>

#include "resource.h"
#include "oshelper.h"
#include "timeline.h"
#include "prefs.h"

#include "PositionControl.h"

extern HINSTANCE g_hInst;

extern void VDPreferencesSetTimeFormat(int format);

////////////////////////////

extern const char szPositionControlName[]="birdyPositionControl";

enum { uIcon_count=15 };

static const UINT uIconIDs[uIcon_count][2]={
	{ IDI_POS_STOP },
	{ IDI_POS_PLAY },
	{ IDI_POS_PLAYPREVIEW },
	{ IDI_POS_START },
	{ IDI_POS_BACKWARD },
	{ IDI_POS_FORWARD },
	{ IDI_POS_END },
	{ IDI_POS_PREV_KEY },
	{ IDI_POS_NEXT_KEY },
	{ IDI_POS_SCENEREV },
	{ IDI_POS_SCENEFWD },
	{ IDI_POS_MARKIN, IDI_POS_MARKIN2 },
	{ IDI_POS_MARKOUT, IDI_POS_MARKOUT2 },
	{ IDI_POS_MARKIN, IDI_POS_MARKIN2 },
	{ IDI_POS_MARKOUT, IDI_POS_MARKOUT2 },
};

static const UINT uIconIDs_x128[uIcon_count][2]={
	{ IDI_POS_STOP_x128 },
	{ IDI_POS_PLAY_x128 },
	{ IDI_POS_PLAYPREVIEW_x128 },
	{ IDI_POS_START_x128 },
	{ IDI_POS_BACKWARD_x128 },
	{ IDI_POS_FORWARD_x128 },
	{ IDI_POS_END_x128 },
	{ IDI_POS_PREV_KEY_x128 },
	{ IDI_POS_NEXT_KEY_x128 },
	{ IDI_POS_SCENEREV_x128 },
	{ IDI_POS_SCENEFWD_x128 },
	{ IDI_POS_MARKIN_x128, IDI_POS_MARKIN2_x128 },
	{ IDI_POS_MARKOUT_x128, IDI_POS_MARKOUT2_x128 },
	{ IDI_POS_MARKIN_x128, IDI_POS_MARKIN2_x128 },
	{ IDI_POS_MARKOUT_x128, IDI_POS_MARKOUT2_x128 },
};

#undef IDC_START
enum {
	IDC_TRACKBAR	= 500,
	IDC_FRAME		= 501,
	IDC_STOP		= 502,
	IDC_PLAY		= 503,
	IDC_PLAYPREVIEW	= 504,
	IDC_START		= 505,
	IDC_BACKWARD	= 506,
	IDC_FORWARD		= 507,
	IDC_END			= 508,
	IDC_KEYPREV		= 509,
	IDC_KEYNEXT		= 510,
	IDC_SCENEREV	= 511,
	IDC_SCENEFWD	= 512,
	IDC_MARKIN		= 513,
	IDC_MARKOUT		= 514,
	IDC_FILTER_MARKIN		= 515,
	IDC_FILTER_MARKOUT		= 516,
};

static const struct {
	UINT id;
	const char *tip;
} g_posctltips[]={
	{ IDC_TRACKBAR, "[Trackbar]\r\n\r\nDrag this to seek to any frame in the movie. Hold down SHIFT to snap to keyframes/I-frames." },
	{ IDC_FRAME, "[Frame indicator]\r\n\r\nDisplays the current frame number, timestamp, and frame type.\r\n\r\n"
					"[ ] AVI delta frame\r\n"
					"[D] AVI dropped frame\r\n"
					"[K] AVI key frame\r\n"
					"[I] MPEG-1 intra frame\r\n"
					"[P] MPEG-1 forward predicted frame\r\n"
					"[B] MPEG-1 bidirectionally predicted frame" },
	{ IDC_STOP, "[Stop] Stops playback or the current dub operation." },
	{ IDC_PLAY, "[Input playback] Starts playback of the input file." },
	{ IDC_PLAYPREVIEW, "[Output playback] Starts preview of processed output." },
	{ IDC_START, "[Start] Move to the first frame." },
	{ IDC_BACKWARD, "[Backward] Back up by one frame." },
	{ IDC_FORWARD, "[Forward] Advance by one frame." },
	{ IDC_END, "[End] Move to the last frame." },
	{ IDC_KEYPREV, "[Key previous] Move to the previous key frame or I-frame." },
	{ IDC_KEYNEXT, "[Key next] Move to the next key frame or I-frame." },
	{ IDC_SCENEREV, "[Scene reverse] Scan backward for the last scene change." },
	{ IDC_SCENEFWD, "[Scene forward] Scan forward for the next scene change." },
	{ IDC_MARKIN, "[Mark in] Specify the start for processing or of a selection to delete." },
	{ IDC_MARKOUT, "[Mark out] Specify the end for processing or of a selection to delete." },
	{ IDC_FILTER_MARKIN, "[Mark in] Specify the start for filter range." },
	{ IDC_FILTER_MARKOUT, "[Mark out] Specify the end for filter range." },
};

HBITMAP LoadImageStretch(LPSTR id, int w, int h)
{
	HBITMAP bm0 = (HBITMAP)LoadImage(g_hInst, id,IMAGE_BITMAP,0,0,LR_LOADTRANSPARENT|LR_LOADMAP3DCOLORS);
	if (!bm0) return 0;
	HDC hdc = GetDC(0);
	BITMAPINFO bmi = {0};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = w;
	bmi.bmiHeader.biHeight = h;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biPlanes = 1;
	HBITMAP bm1 = CreateDIBSection(0,&bmi,0,0,0,0);

	HDC dc0 = CreateCompatibleDC(hdc);
	HDC dc1 = CreateCompatibleDC(hdc);
	SelectObject(dc0,bm0);
	SelectObject(dc1,bm1);

	BITMAP bm0_info;
	GetObject(bm0,sizeof(BITMAP),&bm0_info);

	SetStretchBltMode(dc1,HALFTONE);
	StretchBlt(dc1,0,0,w,h,dc0,0,0,bm0_info.bmWidth,bm0_info.bmHeight,SRCCOPY);
	ReleaseDC(0, hdc);
	DeleteDC(dc1);
	DeleteDC(dc0);
	DeleteObject(bm0);
	return bm1;
}

///////////////////////////////////////////////////////////////////////////

struct VDPositionControlW32 : public vdrefcounted<IVDPositionControl> {
public:
	static ATOM Register();

protected:
	VDPositionControlW32(HWND hwnd);
	~VDPositionControlW32();

	int			GetNiceHeight();

	void		SetFrameTypeCallback(IVDPositionControlCallback *pCB);

	void		SetRange(VDPosition lo, VDPosition hi, bool updateNow);
	void		SetRangeZoom(bool v, bool updateNow);
	VDPosition	GetRangeBegin();
	VDPosition	GetRangeEnd();

	VDPosition	GetPosition();
	void		SetPosition(VDPosition pos);
	void		SetDisplayedPosition(VDPosition pos);
	void		SetAutoPositionUpdate(bool autoUpdate);
	void		SetAutoStep(bool autoStep);

	bool		GetSelection(VDPosition& start, VDPosition& end);
	void		SetSelection(VDPosition start, VDPosition end, bool updateNow);
	bool		GetSelection2(VDPosition& start, VDPosition& end);
	void		SetSelection2(VDPosition start, VDPosition end, bool updateNow);
	void		SetTimeline(VDTimeline& t);

	void		SetFrameRate(const VDFraction& frameRate);

	void		ResetShuttle();

	VDEvent<IVDPositionControl, VDPositionControlEventData>&	PositionUpdated();
	void		SetMessage(const wchar_t* s);

protected:
	void InternalSetPosition(VDPosition pos, VDPositionControlEventData::EventType eventType);
	VDPosition PositionInRange(VDPosition pos);

	static LRESULT APIENTRY StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	static BOOL CALLBACK InitChildrenProc(HWND hWnd, LPARAM lParam);
	void OnCreate();
	void OnSize();
	void OnPaint();
	void UpdateString(VDPosition pos=-1);
	void RecomputeMetrics();
	void RecalcThumbRect(VDPosition pos, bool update = true);
	bool Notify(UINT code, VDPositionControlEventData::EventType eventType);
	void SetTimeFormat(int format);

	inline int FrameToPixel(VDPosition pos) {
		return VDFloorToInt(mPixelToFrameBias + mPixelsPerFrame * pos);
	}

	HWND				mhwnd;
	HMENU				mhmenuPopup;
	HFONT				mFrameFont;
	int					nFrameCtlHeight;
	int					nFrameCtlWidth;
	VDFraction			mFrameRate;
	IVDPositionControlCallback *mpCB;
	void				*pvFTCData;

	HFONT				mFrameNumberFont;
	int					mFrameNumberHeight;
	int					mFrameNumberWidth;

	enum {
		kBrushCurrentFrame,
		kBrushTick,
		kBrushTrack,
		kBrushSelection,
		kBrushSelection2,
		kBrushes
	};

	HBRUSH				mBrushes[kBrushes];

	VDPosition			mPosition;
	VDPosition			mRangeStart;
	VDPosition			mRangeEnd;
	VDPosition			mSelectionStart;
	VDPosition			mSelectionEnd;
	VDPosition			mSelection2Start;
	VDPosition			mSelection2End;
	vdfastvector<sint64> marker;
	vdfastvector<sint64> edit;

	RECT				mPositionArea;			// track, ticks, bar, and numbers
	RECT				mTrackArea;				// track, ticks, and bar
	RECT				mTickArea;				// just ticks
	RECT				mTickArea2;				// just ticks
	RECT				mTrack;					// just the track
	RECT				mThumbRect;
	int					mThumbWidth;
	int					mTickWidth;
	int					mButtonSize;
	int					mGapSize;
	double				mPixelsPerFrame;
	double				mPixelToFrameBias;
	double				mFramesPerPixel;

	int					mWheelAccum;
	int					mDragOffsetX;
	int					mDragAccum;
	VDPosition			mDragAnchorPos;
	enum {
		kDragNone,
		kDragThumbFast,
		kDragThumbSlow
	} mDragMode;

	bool mbHasPosText;
	bool mbHasNavControls;
	bool mbHasPlaybackControls;
	bool mbHasMarkControls;
	bool mbHasSceneControls;
	bool mbHasFilterControls;
	bool mbButtonIcon;
	bool mbAutoFrame;
	bool mbAutoStep;
	bool mbZoom;

	VDEvent<IVDPositionControl, VDPositionControlEventData>	mPositionUpdatedEvent;

	HICON shIcon1[uIcon_count];
	HICON shIcon2[uIcon_count];
};

ATOM VDPositionControlW32::Register() {
	WNDCLASS wc;

	wc.style		= 0;
	wc.lpfnWndProc	= StaticWndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= sizeof(VDPositionControlW32 *);
	wc.hInstance	= g_hInst;
	wc.hIcon		= NULL;
	wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground= (HBRUSH)(COLOR_3DFACE+1);	//GetStockObject(LTGRAY_BRUSH);
	wc.lpszMenuName	= NULL;
	wc.lpszClassName= POSITIONCONTROLCLASS;

	return RegisterClass(&wc);
}

ATOM RegisterPositionControl() {
	return VDPositionControlW32::Register();
}

IVDPositionControl *VDGetIPositionControl(VDGUIHandle h) {
	return static_cast<IVDPositionControl *>((VDPositionControlW32 *)GetWindowLongPtr((HWND)h, 0));
}

///////////////////////////////////////////////////////////////////////////

VDPositionControlW32::VDPositionControlW32(HWND hwnd)
	: mhwnd(hwnd)
	, mFrameFont(NULL)
	, nFrameCtlHeight(0)
	, nFrameCtlWidth(0)
	, mFrameRate(0,0)
	, mpCB(NULL)
	, pvFTCData(NULL)
	, mFrameNumberFont(NULL)
	, mFrameNumberHeight(0)
	, mPosition(0)
	, mRangeStart(0)
	, mRangeEnd(0)
	, mSelectionStart(0)
	, mSelectionEnd(-1)
	, mSelection2Start(0)
	, mSelection2End(-1)
	, mButtonSize(0)
	, mGapSize(0)
	, mPixelsPerFrame(0)
	, mPixelToFrameBias(0)
	, mFramesPerPixel(0)
	, mWheelAccum(0)
	, mDragMode(kDragNone)
	, mbHasPosText(false)
	, mbHasNavControls(false)
	, mbHasPlaybackControls(false)
	, mbHasMarkControls(false)
	, mbHasSceneControls(false)
	, mbHasFilterControls(false)
	, mbAutoFrame(true)
	, mbAutoStep(false)
	, mbZoom(false)
{
	mBrushes[kBrushCurrentFrame] = CreateSolidBrush(RGB(255,0,0));
	mBrushes[kBrushTick] = CreateSolidBrush(RGB(0,0,0));
	mBrushes[kBrushTrack] = CreateSolidBrush(RGB(128,128,128));
	mBrushes[kBrushSelection] = CreateSolidBrush(RGB(192,224,255));
	mBrushes[kBrushSelection2] = CreateSolidBrush(RGB(247,148,29));

	SetRect(&mThumbRect, 0, 0, 0, 0);
	SetRect(&mTrack, 0, 0, 0, 0);
	SetRect(&mTrackArea, 0, 0, 0, 0);
	SetRect(&mPositionArea, 0, 0, 0, 0);
	mhmenuPopup = 0;
}

VDPositionControlW32::~VDPositionControlW32() {
	if (mFrameFont)
		DeleteObject(mFrameFont);
	if (mFrameNumberFont)
		DeleteObject(mFrameNumberFont);
	if (mhmenuPopup)
		DestroyMenu(mhmenuPopup);

	{for(int i=0; i<kBrushes; ++i) {
		if (mBrushes[i])
			DeleteObject(mBrushes[i]);
	}}

	{for(int i=0; i<uIcon_count; i++) {
		if(shIcon1[i]) DeleteObject(shIcon1[i]);
		if(shIcon2[i]) DeleteObject(shIcon2[i]);
	}}
}

///////////////////////////////////////////////////////////////////////////

int VDPositionControlW32::GetNiceHeight() {
	int s = 0;
	if (mbHasPosText) s += mButtonSize;
	s += mFrameNumberHeight;
	s += mFrameNumberHeight*7/4;
	//if (mbHasPosText) s += mFrameNumberHeight*7/4; else s += mFrameNumberHeight*14/12;
	return s;
}

void VDPositionControlW32::SetFrameTypeCallback(IVDPositionControlCallback *pCB) {
	mpCB = pCB;
}

void VDPositionControlW32::SetRange(VDPosition lo, VDPosition hi, bool updateNow) {
	if (lo != mRangeStart || hi != mRangeEnd) {
		mRangeStart = lo;
		mRangeEnd = hi;
		if (mPosition < mRangeStart)
			mPosition = mRangeStart;
		if (mPosition > mRangeEnd)
			mPosition = mRangeEnd;
		if (mSelectionStart < mRangeStart)
			mSelectionStart = mRangeStart;
		if (mSelectionEnd > mRangeEnd)
			mSelectionEnd = mRangeEnd;
		if (mSelection2Start < mRangeStart)
			mSelection2Start = mRangeStart;
		if (mSelection2End > mRangeEnd)
			mSelection2End = mRangeEnd;

		RecomputeMetrics();
		UpdateString();
	}
}

void VDPositionControlW32::SetRangeZoom(bool v, bool updateNow) {
	mbZoom = v;
	RecomputeMetrics();
}

VDPosition VDPositionControlW32::GetRangeBegin() {
	return mRangeStart;
}

VDPosition VDPositionControlW32::GetRangeEnd() {
	return mRangeEnd;
}

VDPosition VDPositionControlW32::GetPosition() {
	return mPosition;
}

void VDPositionControlW32::SetPosition(VDPosition pos) {
	InternalSetPosition(pos, VDPositionControlEventData::kEventJump);
	if (mhwnd)
		UpdateWindow(mhwnd);
}

VDPosition VDPositionControlW32::PositionInRange(VDPosition pos) {
	if (pos < mRangeStart)
		pos = mRangeStart;
	if (pos > mRangeEnd)
		pos = mRangeEnd;
	return pos;
}

void VDPositionControlW32::InternalSetPosition(VDPosition pos, VDPositionControlEventData::EventType eventType) {
	if (!mbZoom)
		pos = PositionInRange(pos);

	if (pos != mPosition) {
		mPosition = pos;
		RecalcThumbRect(pos, true);

		VDPositionControlEventData eventData;
		eventData.mPosition = mPosition;
		eventData.mEventType = eventType;
		mPositionUpdatedEvent.Raise(this, eventData);
	}

	UpdateString();
}

void VDPositionControlW32::SetDisplayedPosition(VDPosition pos) {
	UpdateString(pos);
	RecalcThumbRect(pos, true);
	if (mhwnd)
		UpdateWindow(mhwnd);
}

void VDPositionControlW32::SetAutoPositionUpdate(bool autoUpdate) {
	if (autoUpdate != mbAutoFrame) {
		mbAutoFrame = autoUpdate;

		if (autoUpdate)
			RecalcThumbRect(mPosition, true);
	}
}

void VDPositionControlW32::SetAutoStep(bool autoStep) {
	mbAutoStep = autoStep;
}

bool VDPositionControlW32::GetSelection(VDPosition& start, VDPosition& end) {
	if (mSelectionStart < mSelectionEnd) {
		start	= mSelectionStart;
		end		= mSelectionEnd;
		return true;
	}

	return false;
}

bool VDPositionControlW32::GetSelection2(VDPosition& start, VDPosition& end) {
	if (mSelection2Start < mSelection2End) {
		start	= mSelection2Start;
		end		= mSelection2End;
		return true;
	}

	return false;
}

void VDPositionControlW32::SetSelection(VDPosition start, VDPosition end, bool updateNow) {
	const int tickHeight = mTickArea.bottom - mTickArea.top;

	// wipe old selection
	if (mhwnd && mSelectionStart <= mSelectionEnd) {
		int selx1 = FrameToPixel(mSelectionStart);
		int selx2 = FrameToPixel(mSelectionEnd);

		RECT rOld = { selx1 - tickHeight, mTrack.top, selx2 + tickHeight, mTickArea.bottom };

		InvalidateRect(mhwnd, &rOld, TRUE);
	}

	// render new selection
	mSelectionStart	= start;
	mSelectionEnd	= end;

	if (mhwnd && mSelectionStart <= mSelectionEnd) {
		int selx1 = FrameToPixel(mSelectionStart);
		int selx2 = FrameToPixel(mSelectionEnd);

		RECT rNew = { selx1 - tickHeight, mTrack.top, selx2 + tickHeight, mTickArea.bottom };

		InvalidateRect(mhwnd, &rNew, TRUE);
	}
}

void VDPositionControlW32::SetSelection2(VDPosition start, VDPosition end, bool updateNow) {
	const int tickHeight = mTickArea2.bottom - mTickArea2.top;

	// wipe old selection
	if (mhwnd && mSelection2Start <= mSelection2End) {
		int selx1 = FrameToPixel(mSelection2Start);
		int selx2 = FrameToPixel(mSelection2End);

		RECT rOld = { selx1 - tickHeight, mTickArea2.top, selx2 + tickHeight, mTickArea2.bottom };

		InvalidateRect(mhwnd, &rOld, TRUE);
	}

	// render new selection
	mSelection2Start	= start;
	mSelection2End	= end;

	if (mhwnd && mSelection2Start <= mSelection2End) {
		int selx1 = FrameToPixel(mSelection2Start);
		int selx2 = FrameToPixel(mSelection2End);

		RECT rNew = { selx1 - tickHeight, mTickArea2.top, selx2 + tickHeight, mTickArea2.bottom };

		InvalidateRect(mhwnd, &rNew, TRUE);
	}
}

void VDPositionControlW32::SetTimeline(VDTimeline& t) {
	edit.clear();
	VDPosition p0 = 0;
	while(1){
		p0 = t.GetNextEdit(p0);
		if(p0==-1) break;
		edit.push_back(p0);
	}

	marker.clear();
	sint64 prev = -1;
	{for(int i=0; i<t.GetMarkerCount(); i++){
		sint64 p = t.GetMarker(i);
		if(p!=-1 && p!=prev){
			marker.push_back(p);
			prev = p;
		}
	}}

	InvalidateRect(mhwnd, &mTrack, FALSE);
}

void VDPositionControlW32::SetFrameRate(const VDFraction& frameRate) {
	mFrameRate = frameRate;
	if (mhwnd)
		UpdateString();
}

void VDPositionControlW32::ResetShuttle() {
	if (!mhwnd)
		return;

	CheckDlgButton(mhwnd, IDC_SCENEREV, BST_UNCHECKED);
	CheckDlgButton(mhwnd, IDC_SCENEFWD, BST_UNCHECKED);
}

VDEvent<IVDPositionControl, VDPositionControlEventData>& VDPositionControlW32::PositionUpdated() {
	return mPositionUpdatedEvent;
}

///////////////////////////////////////////////////////////////////////////

BOOL CALLBACK VDPositionControlW32::InitChildrenProc(HWND hWnd, LPARAM lParam) {
	VDPositionControlW32 *pThis = (VDPositionControlW32 *)lParam;
	UINT id;
	int fill = GetSysColor(COLOR_BTNFACE);
	int fill_r = fill & 0xFF;
	int fill_g = (fill & 0xFF00) >> 8;
	int fill_b = (fill & 0xFF0000) >> 16;
	int fill_y = ((fill_b * 19 + fill_g * 183 + fill_r * 54) >> 8);

	switch(id = GetWindowLong(hWnd, GWL_ID)) {
	case IDC_STOP:
	case IDC_PLAY:
	case IDC_PLAYPREVIEW:
	case IDC_START:
	case IDC_BACKWARD:
	case IDC_FORWARD:
	case IDC_END:
	case IDC_KEYPREV:
	case IDC_KEYNEXT:
	case IDC_SCENEREV:
	case IDC_SCENEFWD:
	case IDC_MARKIN:
	case IDC_MARKOUT:
	case IDC_FILTER_MARKIN:
	case IDC_FILTER_MARKOUT:
		{
			int i = id - IDC_STOP;
			void* icon = pThis->shIcon1[i];
			void* icon_dark = pThis->shIcon2[i];
			if (fill_y < 128 && icon_dark) icon = icon_dark;
			SendMessage(hWnd, BM_SETIMAGE, pThis->mbButtonIcon ? IMAGE_ICON:IMAGE_BITMAP, (LPARAM)icon);
		}
		break;

	case IDC_FRAME:
		SendMessage(hWnd, WM_SETFONT, (WPARAM)pThis->mFrameFont, (LPARAM)MAKELONG(FALSE, 0));
		break;

	}

	return TRUE;
}

LRESULT APIENTRY VDPositionControlW32::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDPositionControlW32 *pcd = (VDPositionControlW32 *)GetWindowLongPtr(hwnd, 0);

	switch(msg) {
	case WM_NCCREATE:
		if (!(pcd = new VDPositionControlW32(hwnd)))
			return FALSE;

		pcd->AddRef();
		SetWindowLongPtr(hwnd, 0, (LONG_PTR)pcd);
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

LRESULT CALLBACK VDPositionControlW32::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_CREATE:
		OnCreate();
		// fall through
	case WM_SIZE:
		OnSize();
		break;

	case WM_SYSCOLORCHANGE:
		if (mFrameFont)
			EnumChildWindows(mhwnd, (WNDENUMPROC)InitChildrenProc, (LPARAM)this);
		break;

	case WM_PAINT:
		OnPaint();
		return 0;

	case WM_NOTIFY:
		if (TTN_GETDISPINFO == ((LPNMHDR)lParam)->code) {
			NMTTDISPINFO *lphdr = (NMTTDISPINFO *)lParam;
			UINT id = (lphdr->uFlags & TTF_IDISHWND) ? GetWindowLong((HWND)lphdr->hdr.idFrom, GWL_ID) : lphdr->hdr.idFrom;

			*lphdr->lpszText = 0;

			SendMessage(lphdr->hdr.hwndFrom, TTM_SETMAXTIPWIDTH, 0, 5000);

			for(int i=0; i<sizeof g_posctltips/sizeof g_posctltips[0]; ++i) {
				if (id == g_posctltips[i].id)
					lphdr->lpszText = const_cast<char *>(g_posctltips[i].tip);
			}

			return TRUE;
		}
		break;

	case WM_COMMAND:
		{
			UINT cmd;
			VDPositionControlEventData::EventType eventType = VDPositionControlEventData::kEventNone;

			switch(LOWORD(wParam)) {
			case IDC_STOP:			cmd = PCN_STOP;			break;
			case IDC_PLAY:			cmd = PCN_PLAY;			break;
			case IDC_PLAYPREVIEW:	cmd = PCN_PLAYPREVIEW;	break;
			case IDC_MARKIN:		cmd = PCN_MARKIN;		break;
			case IDC_MARKOUT:		cmd = PCN_MARKOUT;		break;
			case IDC_FILTER_MARKIN:		cmd = PCN_MARKIN;		break;
			case IDC_FILTER_MARKOUT:		cmd = PCN_MARKOUT;		break;

			case IDC_START:
				cmd = PCN_START;
				if (mbAutoStep)
					InternalSetPosition(mRangeStart, VDPositionControlEventData::kEventJumpToStart);
				break;

			case IDC_BACKWARD:
				cmd = PCN_BACKWARD;
				if (mbAutoStep)
					InternalSetPosition(mPosition - 1, VDPositionControlEventData::kEventJumpToPrev);
				break;

			case IDC_FORWARD:
				cmd = PCN_FORWARD;
				if (mbAutoStep)
					InternalSetPosition(mPosition + 1, VDPositionControlEventData::kEventJumpToNext);
				break;

			case IDC_END:
				cmd = PCN_END;
				if (mbAutoStep)
					InternalSetPosition(mRangeEnd, VDPositionControlEventData::kEventJumpToEnd);
				break;

			case IDC_KEYPREV:
				cmd = PCN_KEYPREV;
				eventType = VDPositionControlEventData::kEventJumpToPrevKey;
				break;

			case IDC_KEYNEXT:
				cmd = PCN_KEYNEXT;
				eventType = VDPositionControlEventData::kEventJumpToNextKey;
				break;

			case IDC_SCENEREV:
				cmd = PCN_SCENEREV;
				if (BST_UNCHECKED!=SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)) {
					if (IsDlgButtonChecked(mhwnd, IDC_SCENEFWD))
						CheckDlgButton(mhwnd, IDC_SCENEFWD, BST_UNCHECKED);
				} else
					cmd = PCN_SCENESTOP;
				break;
			case IDC_SCENEFWD:
				cmd = PCN_SCENEFWD;
				if (BST_UNCHECKED!=SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)) {
					if (IsDlgButtonChecked(mhwnd, IDC_SCENEREV))
						CheckDlgButton(mhwnd, IDC_SCENEREV, BST_UNCHECKED);
				} else
					cmd = PCN_SCENESTOP;
				break;
			case ID_POS_COPY:
				{
					HWND hwndFrame = GetDlgItem(mhwnd, IDC_FRAME);
					VDStringW s(VDGetWindowTextW32(hwndFrame));
					VDCopyTextToClipboard(s.c_str());
				}
				return 0;
			case ID_TIMEFORMAT_H:
				SetTimeFormat(pref_time_hmst);
				return 0;
			case ID_TIMEFORMAT_HR:
				SetTimeFormat(pref_time_hmst_r);
				return 0;
			case ID_TIMEFORMAT_M:
				SetTimeFormat(pref_time_m);
				return 0;
			case ID_TIMEFORMAT_MR:
				SetTimeFormat(pref_time_m_r);
				return 0;
			case ID_TIMEFORMAT_S:
				SetTimeFormat(pref_time_s);
				return 0;
			case ID_TIMEFORMAT_SR:
				SetTimeFormat(pref_time_s_r);
				return 0;
			case ID_TIMEFORMAT_MS:
				SetTimeFormat(pref_time_ms);
				return 0;
			case ID_TIMEFORMAT_MSR:
				SetTimeFormat(pref_time_ms_r);
				return 0;
			case ID_TIMEFORMAT_PERCENT:
				SetTimeFormat(pref_time_r100);
				return 0;
			default:
				return 0;
			}

			LRESULT r = SendMessage(GetParent(mhwnd), WM_COMMAND, MAKELONG(GetWindowLong(mhwnd, GWL_ID), cmd), (LPARAM)mhwnd);
			if(r==-1) switch(LOWORD(wParam)) {
			case IDC_SCENEREV:
			case IDC_SCENEFWD:
				CheckDlgButton(mhwnd,LOWORD(wParam),BST_UNCHECKED);
				break;
			}
		}
		break;

	case WM_LBUTTONDOWN:
		{
			POINT pt = {(SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam)};

			if (PtInRect(&mThumbRect, pt)) {
				mDragOffsetX = pt.x - mThumbRect.left;

				if (mDragMode == kDragThumbSlow)
					ShowCursor(TRUE);

				mDragMode = kDragThumbFast;
				SetCapture(mhwnd);

				if (Notify(PCN_BEGINTRACK, VDPositionControlEventData::kEventNone)) {
					InvalidateRect(mhwnd, &mThumbRect, TRUE);
				} else {
					mDragMode = kDragNone;
					ReleaseCapture();
				}
			} else if (PtInRect(&mPositionArea, pt)) {
				VDPosition prev = mPosition;
				extern bool VDPreferencesGetTimelinePageMode();
				extern int VDPreferencesGetTimelinePageSize();
				if (VDPreferencesGetTimelinePageMode()) {
					if(pt.x>mThumbRect.right) mPosition += VDPreferencesGetTimelinePageSize();
					if(pt.x<mThumbRect.left) mPosition -= VDPreferencesGetTimelinePageSize();
				} else {
					mPosition = mRangeStart + (sint64)floor((pt.x - mTrack.left) * mFramesPerPixel + 0.5);
				}
				if (mPosition < mRangeStart)
					mPosition = mRangeStart;
				if (mPosition > mRangeEnd)
					mPosition = mRangeEnd;
				if (Notify(PCN_THUMBPOSITION, VDPositionControlEventData::kEventJump)) {
					if (mbAutoFrame)
						UpdateString();
					RecalcThumbRect(mPosition);
					InvalidateRect(mhwnd, &mThumbRect, TRUE);
				} else {
					mPosition = prev;
				}
			} else if (mbHasPosText) {
				HWND hwndFrame = GetDlgItem(mhwnd, IDC_FRAME);
				RECT r;
				GetWindowRect(hwndFrame,&r);
				MapWindowPoints(0,mhwnd,(POINT*)&r,2);
				if (PtInRect(&r, pt)) {
					SendMessage(GetParent(mhwnd), WM_COMMAND, MAKELONG(GetWindowLong(mhwnd, GWL_ID), PCN_JUMPTO), (LPARAM)mhwnd);
				}
			}
		}
		break;

	case WM_RBUTTONDOWN:
		{
			POINT pt = {(SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam)};

			if (PtInRect(&mThumbRect, pt)) {
				mDragOffsetX = pt.x - mThumbRect.left;
				mDragAnchorPos = mPosition;

				bool hideCursor = false;
				if (mDragMode != kDragThumbSlow) {
					hideCursor = true;
					mDragMode = kDragThumbSlow;
				}

				mDragAccum = 0;
				SetCapture(mhwnd);

				if (Notify(PCN_BEGINTRACK, VDPositionControlEventData::kEventNone)) {
					InvalidateRect(mhwnd, &mThumbRect, TRUE);

					if (hideCursor)
						ShowCursor(FALSE);
				} else {
					mDragMode = kDragNone;
					ReleaseCapture();
				}
			} else if (mbHasPosText && mhmenuPopup) {
				HWND hwndFrame = GetDlgItem(mhwnd, IDC_FRAME);
				RECT r;
				GetWindowRect(hwndFrame,&r);
				MapWindowPoints(0,mhwnd,(POINT*)&r,2);
				if (PtInRect(&r, pt)) {
					HMENU	popup = GetSubMenu(mhmenuPopup, 0);
					ClientToScreen(mhwnd, &pt);
					TrackPopupMenu(popup, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, mhwnd, NULL);
				}
			}
		}
		break;

	case WM_MOUSEMOVE:
		if (mDragMode == kDragThumbFast) {
			POINT pt = {(SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam)};

			int x = pt.x - mDragOffsetX;

			if (x < mTrack.left - mThumbWidth)
				x = mTrack.left - mThumbWidth;
			if (x > mTrack.right - mThumbWidth)
				x = mTrack.right - mThumbWidth;

			if (x != mThumbRect.left) {
				if (mbAutoFrame) {
					InvalidateRect(mhwnd, &mThumbRect, TRUE);
					mThumbRect.right = x + (mThumbRect.right - mThumbRect.left);
					mThumbRect.left = x;
					InvalidateRect(mhwnd, &mThumbRect, TRUE);
					UpdateWindow(mhwnd);
				}

				sint64 pos = mRangeStart + VDRoundToInt64((x - mTrack.left + mThumbWidth) * mFramesPerPixel);
				if (pos > mRangeEnd)
					pos = mRangeEnd;
				if (pos < mRangeStart)
					pos = mRangeStart;
				if (mPosition != pos) {
					mPosition = pos;

					if (mbAutoFrame)
						UpdateString();

					Notify(PCN_THUMBTRACK, VDPositionControlEventData::kEventTracking);
				}
			}
		} else if (mDragMode == kDragThumbSlow) {
			POINT pt = {(SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam)};

			mDragAccum += (pt.x - (mThumbRect.left + mDragOffsetX));
			int delta = mDragAccum / 8;
			mDragAccum -= delta * 8;

			if (delta) {
				mDragAnchorPos = PositionInRange(mDragAnchorPos + delta);
				SetPosition(mDragAnchorPos);
				Notify(PCN_THUMBTRACK, VDPositionControlEventData::kEventTracking);
			}
			
			pt.x = mThumbRect.left + mDragOffsetX;
			ClientToScreen(mhwnd, &pt);
			SetCursorPos(pt.x, pt.y);
		}
		break;

	case WM_CAPTURECHANGED:
		if ((HWND)lParam == mhwnd)
			break;
	case WM_MOUSELEAVE:
	case WM_RBUTTONUP:
	case WM_LBUTTONUP:
		if (mDragMode) {
			if (mDragMode == kDragThumbSlow)
				ShowCursor(TRUE);

			mDragMode = kDragNone;
			ReleaseCapture();

			Notify(PCN_ENDTRACK, VDPositionControlEventData::kEventNone);
			InvalidateRect(mhwnd, &mThumbRect, TRUE);
		}
		break;

	case WM_MOUSEWHEEL:
		{
			mWheelAccum -= (SHORT)HIWORD(wParam);

			int increments = mWheelAccum / WHEEL_DELTA;

			if (increments) {
				mWheelAccum -= WHEEL_DELTA * increments;

				SetPosition(PositionInRange(mPosition + increments));

				if (increments < 0)
					Notify(PCN_THUMBPOSITIONPREV, VDPositionControlEventData::kEventJump);
				else
					Notify(PCN_THUMBPOSITIONNEXT, VDPositionControlEventData::kEventJump);
			}
		}
		return 0;
	}

	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

extern int VDPreferencesGetTimelineScaleTrack();
extern int VDPreferencesGetTimelineScaleInfo();
extern int VDPreferencesGetTimelineScaleButtons();

void VDPositionControlW32::OnCreate() {
	int mTrackScale = VDPreferencesGetTimelineScaleTrack();
	int mTextScale = VDPreferencesGetTimelineScaleInfo();
	int mButtonScale = VDPreferencesGetTimelineScaleButtons();

	DWORD dwStyles;
	TOOLINFO ti;
	HWND hwndTT;

	dwStyles = GetWindowLong(mhwnd, GWL_STYLE);
	mbHasPlaybackControls	= !!(dwStyles & PCS_PLAYBACK);
	mbHasMarkControls		= !!(dwStyles & PCS_MARK);
	mbHasSceneControls		= !!(dwStyles & PCS_SCENE);
	mbHasNavControls		= !(dwStyles & PCS_XNAV);
	mbHasPosText		= !(dwStyles & PCS_XNAV);
	mbHasFilterControls		= !!(dwStyles & PCS_FILTER);

	// We use 24px at 96 dpi.
	int ht = MulDiv(24, mButtonScale, 100);
	int gap = MulDiv(8, mButtonScale, 100);

	mFrameFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	nFrameCtlHeight = MulDiv(18, mTextScale, 100);
	nFrameCtlWidth = MulDiv(320, mTextScale, 100);
	
	if (mFrameFont) {
		if (HDC hdc = GetDC(mhwnd)) {
			ht = MulDiv(GetDeviceCaps(hdc, LOGPIXELSY), ht, 96);
			gap = MulDiv(GetDeviceCaps(hdc, LOGPIXELSX), gap, 96);

			TEXTMETRIC tm;
			int pad = 2*GetSystemMetrics(SM_CYEDGE);
			int availHeight = ht;

			if (mTextScale!=100) {
				LOGFONT lf;
				if (GetObject(mFrameFont, sizeof lf, &lf)) {
					lf.lfHeight = MulDiv(lf.lfHeight,mTextScale,100);
					HFONT hFont = CreateFontIndirect(&lf);
					if (hFont)
						mFrameFont = hFont;		// the old font was a stock object, so it doesn't need to be deleted
				}
			}

			HGDIOBJ hgoFont = SelectObject(hdc, mFrameFont);

			if (GetTextMetrics(hdc, &tm)) {
				LOGFONT lf;

				nFrameCtlHeight = tm.tmHeight + tm.tmInternalLeading + pad;

				if (nFrameCtlHeight > availHeight && GetObject(mFrameFont, sizeof lf, &lf)) {
					lf.lfHeight = availHeight - pad;
					nFrameCtlHeight = availHeight;

					HFONT hFont = CreateFontIndirect(&lf);
					if (hFont) {
						DeleteObject(mFrameFont);
						mFrameFont = hFont;
					}
				}
			}

			nFrameCtlHeight = (nFrameCtlHeight+1) & ~1;

			SelectObject(hdc, hgoFont);
			ReleaseDC(mhwnd, hdc);
		}
	}

	mButtonSize = ht;
	mGapSize = gap;

	mFrameNumberFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	mFrameNumberHeight = 18;
	mFrameNumberWidth = 8;

	if (mFrameNumberFont) {
		if (HDC hdc = GetDC(mhwnd)) {

			if (mTrackScale!=100) {
				LOGFONT lf;
				if (GetObject(mFrameNumberFont, sizeof lf, &lf)) {
					lf.lfHeight = MulDiv(lf.lfHeight,mTrackScale,100);
					HFONT hFont = CreateFontIndirect(&lf);
					if (hFont)
						mFrameNumberFont = hFont;		// the old font was a stock object, so it doesn't need to be deleted
				}
			}

			TEXTMETRIC tm;
			HGDIOBJ hgoFont = SelectObject(hdc, mFrameNumberFont);

			if (GetTextMetrics(hdc, &tm))
				mFrameNumberHeight = tm.tmHeight + tm.tmInternalLeading;

			SIZE siz;
			if (GetTextExtentPoint32(hdc, "0123456789", 10, &siz))
				mFrameNumberWidth = (siz.cx + 9) / 10;

			SelectObject(hdc, hgoFont);
			ReleaseDC(mhwnd, hdc);
		}
	}

	mThumbWidth = mFrameNumberHeight * 5 / 12;
	mTickWidth = mFrameNumberHeight / 14;
	if (mTickWidth<1) mTickWidth = 1;

	if (mbHasPosText) {
		CreateWindowEx(WS_EX_STATICEDGE,"STATIC",NULL,WS_CHILD|WS_VISIBLE,0,0,0,ht,mhwnd,(HMENU)IDC_FRAME,g_hInst,NULL);
		mhmenuPopup = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_POSITION_MENU));
	}

	mbButtonIcon = true;
	int iconSize = 16;
	int buttonStyle = BS_ICON;
	if (mButtonSize>32) {
		mbButtonIcon = false;
		iconSize = mButtonSize*20/24;
		buttonStyle = BS_BITMAP;
	}

	if (mbHasPlaybackControls || mbHasNavControls || mbHasSceneControls || mbHasMarkControls || mbHasFilterControls) {
		for(int i=0; i<uIcon_count; i++) {
			shIcon1[i] = 0;
			shIcon2[i] = 0;
			if (mbButtonIcon) {
				int id_light = uIconIDs[i][0];
				int id_dark = uIconIDs[i][1];
				if (!(shIcon1[i] = (HICON)LoadImage(g_hInst, MAKEINTRESOURCE(id_light),IMAGE_ICON,0,0,0))) {
					_RPT1(0,"PositionControl: load failure on icon #%d\n",i+1);
				}
				if (id_dark && !(shIcon2[i] = (HICON)LoadImage(g_hInst, MAKEINTRESOURCE(id_dark),IMAGE_ICON,0,0,0))) {
					_RPT1(0,"PositionControl: load failure on icon #%d\n",i+1);
				}
			} else {
				int id_light = uIconIDs_x128[i][0];
				int id_dark = uIconIDs_x128[i][1];
				if (!(shIcon1[i] = (HICON)LoadImageStretch(MAKEINTRESOURCE(id_light),iconSize,iconSize))) {
					_RPT1(0,"PositionControl: load failure on icon #%d\n",i+1);
				}
				if (id_dark && !(shIcon2[i] = (HICON)LoadImageStretch(MAKEINTRESOURCE(id_dark),iconSize,iconSize))) {
					_RPT1(0,"PositionControl: load failure on icon #%d\n",i+1);
				}
			}
		}
	}

	if (mbHasPlaybackControls) {
		CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | buttonStyle			,0,0,ht,ht,mhwnd, (HMENU)IDC_STOP		, g_hInst, NULL);
		CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | buttonStyle			,0,0,ht,ht,mhwnd, (HMENU)IDC_PLAY		, g_hInst, NULL);
		CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | buttonStyle			,0,0,ht,ht,mhwnd, (HMENU)IDC_PLAYPREVIEW	, g_hInst, NULL);
	}
	if (mbHasNavControls) {
		CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | buttonStyle			,0,0,ht,ht,mhwnd, (HMENU)IDC_START		, g_hInst, NULL);
		CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | buttonStyle			,0,0,ht,ht,mhwnd, (HMENU)IDC_BACKWARD	, g_hInst, NULL);
		CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | buttonStyle			,0,0,ht,ht,mhwnd, (HMENU)IDC_FORWARD	, g_hInst, NULL);
		CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | buttonStyle			,0,0,ht,ht,mhwnd, (HMENU)IDC_END		, g_hInst, NULL);
		CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | buttonStyle			,0,0,ht,ht,mhwnd, (HMENU)IDC_KEYPREV	, g_hInst, NULL);
		CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | buttonStyle			,0,0,ht,ht,mhwnd, (HMENU)IDC_KEYNEXT	, g_hInst, NULL);
	}
	if (mbHasSceneControls) {
		CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE | buttonStyle	,0,0,ht,ht,mhwnd, (HMENU)IDC_SCENEREV, g_hInst, NULL);
		CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE | buttonStyle	,0,0,ht,ht,mhwnd, (HMENU)IDC_SCENEFWD, g_hInst, NULL);
	}
	if (mbHasMarkControls) {
		CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | buttonStyle			,0,0,ht,ht,mhwnd, (HMENU)IDC_MARKIN	, g_hInst, NULL);
		CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | buttonStyle			,0,0,ht,ht,mhwnd, (HMENU)IDC_MARKOUT	, g_hInst, NULL);
	}
	if (mbHasFilterControls) {
		CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | buttonStyle			,0,0,ht,ht,mhwnd, (HMENU)IDC_FILTER_MARKIN	, g_hInst, NULL);
		CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | buttonStyle			,0,0,ht,ht,mhwnd, (HMENU)IDC_FILTER_MARKOUT	, g_hInst, NULL);
	}

	if (mFrameFont)
		EnumChildWindows(mhwnd, (WNDENUMPROC)InitChildrenProc, (LPARAM)this);

	// Create tooltip control.

	hwndTT = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP|TTS_NOPREFIX|TTS_ALWAYSTIP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			mhwnd, NULL, g_hInst, NULL);

	if (hwndTT) {

		SetWindowPos(hwndTT, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
		SendMessage(hwndTT, TTM_SETDELAYTIME, TTDT_AUTOMATIC, MAKELONG(2000, 0));
		SendMessage(hwndTT, TTM_SETDELAYTIME, TTDT_RESHOW, MAKELONG(2000, 0));

		ti.cbSize		= sizeof(TOOLINFO);
		ti.uFlags		= TTF_SUBCLASS | TTF_IDISHWND;
		ti.hwnd			= mhwnd;
		ti.lpszText		= LPSTR_TEXTCALLBACK;

		for(int i=0; i<sizeof g_posctltips/sizeof g_posctltips[0]; ++i) {
			ti.uId			= (WPARAM)GetDlgItem(mhwnd, g_posctltips[i].id);

			if (ti.uId)
				SendMessage(hwndTT, TTM_ADDTOOL, 0, (LPARAM)&ti);
		}
	}
}

void VDPositionControlW32::UpdateString(VDPosition pos) {
	if (!mbHasPosText) return;

	wchar_t buf[512];

	if (pos < 0)
		pos = mPosition;

	bool success = false;
	if (mpCB)
		success = mpCB->GetFrameString(buf, sizeof buf / sizeof buf[0], pos);

	if (!success) {
		if (mFrameRate.getLo()) {
			int ms, sec, min;
			long ticks = (long)mFrameRate.scale64ir(pos * 1000);

			ms  = ticks %1000; ticks /= 1000;
			sec	= ticks %  60; ticks /=  60;
			min	= ticks %  60; ticks /=  60;

			success = (unsigned)swprintf(buf, sizeof buf / sizeof buf[0], L" Frame %I64d (%d:%02d:%02d.%03d)", (sint64)pos, ticks, min, sec, ms) < sizeof buf / sizeof buf[0];
		} else
			success = (unsigned)swprintf(buf, sizeof buf / sizeof buf[0], L" Frame %I64d", (sint64)pos) < sizeof buf / sizeof buf[0];
	}

	if (success) {
		HWND hwndFrame = GetDlgItem(mhwnd, IDC_FRAME);

		VDSetWindowTextW32(hwndFrame, buf);
	}
}

void VDPositionControlW32::SetTimeFormat(int format) {
	VDPreferencesSetTimeFormat(format);
	UpdateString();
	Notify(PCN_FORMAT, VDPositionControlEventData::kEventNone);
}

void VDPositionControlW32::SetMessage(const wchar_t* s) {
	if (!mbHasPosText) return;
	HWND hwndFrame = GetDlgItem(mhwnd, IDC_FRAME);
	VDSetWindowTextW32(hwndFrame, s);
}

void VDPositionControlW32::OnSize() {
	RECT wndr;
	UINT id;
	int x, y;
	HWND hwndButton;

	GetClientRect(mhwnd, &wndr);

	RecomputeMetrics();

	// Reposition controls
	y = wndr.bottom - mButtonSize;
	x = 0;

	if (mbHasPlaybackControls) {
		for(id = IDC_STOP; id < IDC_START; id++) {
			SetWindowPos(GetDlgItem(mhwnd, id), NULL, x, y, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOZORDER);

			x += mButtonSize;
		}

		x+=mGapSize;
	}

	if (mbHasNavControls) {
		for(id = IDC_START; id < IDC_MARKIN; id++) {
			if (hwndButton = GetDlgItem(mhwnd,id)) {
				SetWindowPos(hwndButton, NULL, x, y, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOZORDER);
				x += mButtonSize;
			}
		}
		x+=mGapSize;
	}

	if (mbHasMarkControls) {
		for(id = IDC_MARKIN; id <= IDC_MARKOUT; id++) {
			SetWindowPos(GetDlgItem(mhwnd, id), NULL, x, y, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOZORDER);
			x += mButtonSize;
		}

		x+=mGapSize;
	}

	if (mbHasFilterControls) {
		for(id = IDC_FILTER_MARKIN; id <= IDC_FILTER_MARKOUT; id++) {
			SetWindowPos(GetDlgItem(mhwnd, id), NULL, x, y, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOZORDER);
			x += mButtonSize;
		}

		x+=mGapSize;
	}

	if (mbHasPosText) {
		int width = std::min<int>(wndr.right - x, nFrameCtlWidth);
		SetWindowPos(GetDlgItem(mhwnd, IDC_FRAME), NULL, x, y+((mButtonSize - nFrameCtlHeight)>>1), width, nFrameCtlHeight, SWP_NOACTIVATE|SWP_NOZORDER);
	}
}

void VDPositionControlW32::OnPaint() {
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(mhwnd, &ps);

	if (!hdc)		// hrm... this is bad
		return;

	HGDIOBJ hOldFont = SelectObject(hdc, mFrameNumberFont);
	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
	SetTextAlign(hdc, TA_TOP | TA_CENTER);

	char buf[64];
	RECT rClient;

	VDVERIFY(GetClientRect(mhwnd, &rClient));

	int trackRight = mTrack.right;

	HGDIOBJ hOldPen = SelectObject(hdc, GetStockObject(BLACK_PEN));

	// Determine digit spacing.
	int labelDigits = mRangeEnd > 0 ? (int)floor(log10((double)mRangeEnd)) + 1 : 1;
	int labelWidth = (labelDigits + 1) * mFrameNumberWidth;		// Add 1 digit for nice padding.
	sint64 framesPerLabel = 1;

	if (mRangeEnd > mRangeStart) {
		while(framesPerLabel * mPixelsPerFrame < labelWidth) {
			sint64 fpl2 = framesPerLabel + framesPerLabel;

			if (fpl2 * mPixelsPerFrame >= labelWidth) {
				framesPerLabel = fpl2;
				break;
			}

			sint64 fpl5 = framesPerLabel * 5;
			if (fpl5 * mPixelsPerFrame >= labelWidth) {
				framesPerLabel = fpl5;
				break;
			}

			framesPerLabel *= 10;
		}
	}

	sint64 frame = mRangeStart;
	bool bDrawLabels = ps.rcPaint.bottom >= mTrackArea.bottom;

	while(frame < mRangeEnd) {
		int x = FrameToPixel(frame);

		const RECT rTick = { x-mTickWidth/2, mTickArea.top, x-mTickWidth/2+mTickWidth, mTickArea.bottom };
		FillRect(hdc, &rTick, mBrushes[kBrushTick]);

		if (x > trackRight - labelWidth)
			break;		// don't allow labels to encroach last label

		if (bDrawLabels) {
			sprintf(buf, "%I64d", frame);
			TextOut(hdc, x, mTrackArea.bottom, buf, strlen(buf));
		}

		frame += framesPerLabel;
	}

	const RECT rLastTick = { mTrack.right-mTickWidth/2, mTrack.bottom, mTrack.right-mTickWidth/2+mTickWidth, mTrackArea.bottom };
	FillRect(hdc, &rLastTick, mBrushes[kBrushTick]);

	if (bDrawLabels) {
		sprintf(buf, "%I64d", mRangeEnd);
		TextOut(hdc, trackRight, mTrackArea.bottom, buf, strlen(buf));
	}

	// Fill the track.  We draw the track borders later so they're always on top.
	FillRect(hdc, &mTrack, mBrushes[kBrushTrack]);

	// Draw selection and ticks.
	if (mSelectionEnd >= mSelectionStart) {
		int selx1 = FrameToPixel(mSelectionStart);
		int selx2 = FrameToPixel(mSelectionEnd);

		RECT rSel={selx1, mTrack.top, selx2, mTrack.bottom};

		if (rSel.right == rSel.left)
			++rSel.right;

		FillRect(hdc, &rSel, mBrushes[kBrushSelection]);

		if (HPEN hNullPen = CreatePen(PS_NULL, 0, 0)) {
			if (HGDIOBJ hLastPen = SelectObject(hdc, hNullPen)) {
				if (HGDIOBJ hOldBrush = SelectObject(hdc, GetStockObject(BLACK_BRUSH))) {
					const int tickHeight = mTickArea.bottom - mTickArea.top;

					const POINT pts1[3]={
						{ selx1+1, mTickArea.top },
						{ selx1+1, mTickArea.bottom },
						{ selx1+1-tickHeight, mTickArea.top },
					};

					const POINT pts2[3]={
						{ selx2, mTickArea.top },
						{ selx2, mTickArea.bottom },
						{ selx2+tickHeight, mTickArea.top },
					};

					Polygon(hdc, pts1, 3);
					Polygon(hdc, pts2, 3);

					SelectObject(hdc, hOldBrush);
				}

				SelectObject(hdc, hLastPen);
			}
			DeleteObject(hNullPen);
		}
	}

	// Draw selection2 and ticks.
	if (mSelection2End >= mSelection2Start) {
		int selx1 = FrameToPixel(mSelection2Start);
		int selx2 = FrameToPixel(mSelection2End);

		RECT rSel={selx1, (mTickArea2.top+mTickArea2.bottom)/2, selx2, mTickArea2.bottom};

		if (rSel.right == rSel.left)
			++rSel.right;

		FillRect(hdc, &rSel, mBrushes[kBrushSelection2]);

		if (HPEN hNullPen = CreatePen(PS_NULL, 0, 0)) {
			if (HGDIOBJ hLastPen = SelectObject(hdc, hNullPen)) {
				if (HGDIOBJ hOldBrush = SelectObject(hdc, GetStockObject(BLACK_BRUSH))) {
					const int tickHeight = mTickArea2.bottom - mTickArea2.top;

					const POINT pts1[3]={
						{ selx1+1, mTickArea2.top },
						{ selx1+1, mTickArea2.bottom },
						{ selx1+1-tickHeight, mTickArea2.bottom },
					};

					const POINT pts2[3]={
						{ selx2, mTickArea2.top },
						{ selx2, mTickArea2.bottom },
						{ selx2+tickHeight, mTickArea2.bottom },
					};

					Polygon(hdc, pts1, 3);
					Polygon(hdc, pts2, 3);

					SelectObject(hdc, hOldBrush);
				}

				SelectObject(hdc, hLastPen);
			}
			DeleteObject(hNullPen);
		}
	}

	if(!edit.empty()){
		HPEN hNullPen = CreatePen(PS_NULL, 0, 0);
		HBRUSH br = CreateSolidBrush(RGB(255,255,255));
		HGDIOBJ hLastPen = SelectObject(hdc, hNullPen);
		HGDIOBJ hOldBrush = SelectObject(hdc, br);

		{for(int i=0; i<edit.size(); i++){
			sint64 p = edit[i];
			int x1 = FrameToPixel(p)-mTickWidth/2;

			const POINT pts[4]={
				{ x1, mTrack.top },
				{ x1, mTrack.bottom },
				{ x1+mTickWidth, mTrack.bottom },
				{ x1+mTickWidth, mTrack.top },
			};

			Polygon(hdc, pts, 4);
		}}

		SelectObject(hdc, hOldBrush);
		SelectObject(hdc, hLastPen);
		DeleteObject(hNullPen);
		DeleteObject(br);
	}

	if(!marker.empty()){
		HPEN hNullPen = CreatePen(PS_NULL, 0, 0);
		HBRUSH br1 = CreateSolidBrush(RGB(0,255,0));
		HBRUSH br2 = CreateSolidBrush(RGB(0,100,0));
		HGDIOBJ hLastPen = SelectObject(hdc, hNullPen);
		HGDIOBJ hOldBrush = SelectObject(hdc, br1);
		const int tickHeight = mTrack.bottom - mTrack.top - 4;

		{for(int i=0; i<marker.size(); i++){
			sint64 p = marker[i];
			int x1 = FrameToPixel(p);

			if(p>=mSelectionStart) SelectObject(hdc, br2);
			if(p>mSelectionEnd) SelectObject(hdc, br1);

			const POINT pts[3]={
				{ x1-tickHeight, mTrack.top+2 },
				{ x1, mTrack.bottom-2 },
				{ x1+tickHeight, mTrack.top+2 },
			};

			Polygon(hdc, pts, 3);
		}}

		SelectObject(hdc, hOldBrush);
		SelectObject(hdc, hLastPen);
		DeleteObject(hNullPen);
		DeleteObject(br1);
		DeleteObject(br2);
	}

	// Draw track border.
	const int xedge = GetSystemMetrics(SM_CXEDGE);
	const int yedge = GetSystemMetrics(SM_CYEDGE);
	RECT rEdge = mTrack;
	InflateRect(&rEdge, xedge, yedge);

	DrawEdge(hdc, &rEdge, EDGE_SUNKEN, BF_RECT);

	if (mbZoom) {
		RECT r1 = {0, mTrack.top, mTrack.left-10, mTrack.bottom};
		RECT r2 = {mTrack.right+10, mTrack.top, mPositionArea.right, mTrack.bottom};
		FillRect(hdc, &r1, mBrushes[kBrushTrack]);
		FillRect(hdc, &r2, mBrushes[kBrushTrack]);
		InflateRect(&r1, xedge, yedge);
		InflateRect(&r2, xedge, yedge);
		DrawEdge(hdc, &r1, EDGE_SUNKEN, BF_RECT);
		DrawEdge(hdc, &r2, EDGE_SUNKEN, BF_RECT);
	}

	// Draw cursor.
	RECT rThumb = mThumbRect;

	DrawEdge(hdc, &rThumb, EDGE_RAISED, BF_SOFT|BF_RECT|BF_ADJUST);
	DrawEdge(hdc, &rThumb, EDGE_SUNKEN, BF_SOFT|BF_RECT|BF_ADJUST);

	// All done.
	SelectObject(hdc, hOldPen);
	SelectObject(hdc, hOldFont);
	EndPaint(mhwnd, &ps);
}

void VDPositionControlW32::RecomputeMetrics() {
	if (!mhwnd)
		return;

	RECT r;

	VDVERIFY(GetClientRect(mhwnd, &r));

	mPositionArea = r;
	if (mbHasPosText) mPositionArea.bottom -= mButtonSize;

	// Compute space we need for the ticks.
	int labelDigits = mRangeEnd > 0 ? ((int)floor(log10((double)mRangeEnd)) + 1) : 1;
	int labelSpace = ((labelDigits * mFrameNumberWidth) >> 1) + 8;

	if (labelSpace < 16)
		labelSpace = 16;

	if (mbZoom)
		labelSpace += 16;

	mTrackArea.left		= mPositionArea.left;
	mTrackArea.top		= mPositionArea.top;
	mTrackArea.right	= mPositionArea.right;
	mTrackArea.bottom	= mPositionArea.bottom - 2 - mFrameNumberHeight;

	int trackRailHeight = (mTrackArea.bottom - mTrackArea.top + 1) / 3;

	mTrack.left			= mTrackArea.left + labelSpace;
	mTrack.top			= mTrackArea.top + trackRailHeight;
	mTrack.right		= mTrackArea.right - labelSpace;
	mTrack.bottom		= mTrackArea.bottom - trackRailHeight;

	mTickArea.top		= mTrack.bottom + 1*GetSystemMetrics(SM_CYEDGE);
	mTickArea.bottom	= mTrackArea.bottom;

	const int tickHeight = mTickArea.bottom - mTickArea.top;

	mTickArea.left		= mTrack.left - tickHeight;
	mTickArea.right		= mTrack.right + tickHeight;

	mTickArea2.top		= mPositionArea.top;
	mTickArea2.bottom	= mTrack.top - 1*GetSystemMetrics(SM_CYEDGE);
	mTickArea2.left		= mTickArea.left;
	mTickArea2.right		= mTickArea.right;

	// (left+0.5) -> mRangeStart
	// (right-0.5) -> mRangeEnd

	if (mRangeEnd > mRangeStart)
		mPixelsPerFrame = (double)(mTrack.right - mTrack.left - 1) / (double)(mRangeEnd - mRangeStart);
	else
		mPixelsPerFrame = 0.0;

	mPixelToFrameBias = mTrack.left + 0.5 - mPixelsPerFrame*mRangeStart;

	if (mTrack.right > mTrack.left + 1)
		mFramesPerPixel = (double)(mRangeEnd - mRangeStart) / (double)(mTrack.right - mTrack.left - 1);
	else
		mFramesPerPixel = 0.0;

	RecalcThumbRect(mPosition, false);

	RECT rInv = {0,0,r.right,r.bottom-mButtonSize};
	InvalidateRect(mhwnd, &rInv, TRUE);
}

void VDPositionControlW32::RecalcThumbRect(VDPosition pos, bool update) {
	RECT rOld(mThumbRect);

	mThumbRect.left		= FrameToPixel(pos) - mThumbWidth;
	mThumbRect.right	= mThumbRect.left + 2*mThumbWidth;
	mThumbRect.top		= mTrackArea.top;
	mThumbRect.bottom	= mTrackArea.bottom;

	if (update && mhwnd && memcmp(&mThumbRect, &rOld, sizeof(RECT))) {
		InvalidateRect(mhwnd, &rOld, TRUE);
		InvalidateRect(mhwnd, &mThumbRect, TRUE);
	}
}

bool VDPositionControlW32::Notify(UINT code, VDPositionControlEventData::EventType eventType) {
	NMHDR nm;
	nm.hwndFrom = mhwnd;
	nm.idFrom	= GetWindowLong(mhwnd, GWL_ID);
	nm.code		= code;
	int r = SendMessage(GetParent(mhwnd), WM_NOTIFY, nm.idFrom, (LPARAM)&nm);
	if (r==-1) return false;

	if (eventType) {
		VDPositionControlEventData eventData;
		eventData.mPosition = mPosition;
		eventData.mEventType = eventType;
		mPositionUpdatedEvent.Raise(this, eventData);
	}

	return true;
}
