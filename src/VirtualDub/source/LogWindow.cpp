//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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
#include <vd2/system/VDString.h>
#include <vd2/system/thread.h>
#include <vd2/system/log.h>
#include <vd2/system/file.h>
#include <vd2/system/error.h>
#include <vd2/dita/services.h>
#include <vector>
#include <list>
#include <algorithm>

#include "resource.h"
#include "oshelper.h"
#include "LogWindow.h"


enum {
	kFileDialog_Log				= 'log '
};

extern HINSTANCE g_hInst;
extern const char g_szError[];

const char g_szLogWindowControlName[]="phaeronLogWindowControl";

class VDLogWindowControl : public IVDLogWindowControl {
public:
	VDLogWindowControl(HWND hwnd);
	~VDLogWindowControl();

	static VDLogWindowControl *Create(HWND hwndParent, int x, int y, int cx, int cy, UINT id);

	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

public:
	void AttachAsLogger(bool bThisThreadOnly);

	void AddEntry(int severity, const wchar_t *);
	void AddEntry(int severity, const VDStringW&);

	void AddLogEntry(int severity, const wchar_t *s) {
		AddEntry(severity, s);
	}

protected:
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void OnSize(int w, int h);
	void OnPaint();
	void OnSetFont(HFONT hfont, bool bRedraw);
	void OnCommand(int cmd);
	bool OnKey(INT wParam);
	void OnTimer();
	int OnScroll(int type, int action, bool bForceExtreme);
	void ScrollTo(sint32 pos);

	void CreateBuffer(vdfastvector<char>& buffer);

protected:	// internal functions
	const HWND mhwnd;
	HFONT mhfont;
	HMENU mhmenu;

	struct Entry {
		sint32		mPos;
		int			mSeverity;
		VDStringW	*mpText;

		static bool sort(const Entry& e1, const Entry& e2) {
			return e1.mPos < e2.mPos;
		}
	};

	typedef std::vector<Entry> tLineArray;
	tLineArray				mLineArray;
	typedef std::list<VDStringW> tTextArray;
	tTextArray				mTextArray;

	VDCriticalSection		mcsPending;
	tLineArray				mLineArrayPending;
	std::list<VDStringW>	mTextArrayPending;

	int						mLastWidth;
	int						mLastHeight;
	int						mFontHeight;
	int						mFontAscent;
	int						mFontInternalLeading;
	int						mPos;
	bool					mbLockedToBottom;
	bool					mbAttachedToLogSystem;

	enum { kBrushCount = 4 };
	HBRUSH					mBrushes[kBrushCount];

	void SizeEntries(tLineArray::iterator itBegin);
	void Draw(HDC hdc, const VDStringW& s, RECT& r, bool bSizeOnly);
};

/////////////////////////////////////////////////////////////////////////////

VDLogWindowControl::VDLogWindowControl(HWND hwnd)
	: mhwnd(hwnd)
	, mhfont(0)
	, mPos(0)
	, mLastWidth(0)
	, mLastHeight(0)
	, mbLockedToBottom(true)
	, mbAttachedToLogSystem(false)
{
	Entry e = { 0, 0 };
	mLineArray.push_back(e);

	mBrushes[0] = CreateSolidBrush(RGB(64,192,255));
	mBrushes[1] = CreateSolidBrush(RGB(192,255,0));
	mBrushes[2] = CreateSolidBrush(RGB(255,224,0));
	mBrushes[3] = CreateSolidBrush(RGB(255,0,0));

	mhmenu = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_LOG_MENU));
}

VDLogWindowControl::~VDLogWindowControl() {
	if (mbAttachedToLogSystem)
		VDDetachLogger(this);

	for(int i=0; i<kBrushCount; ++i)
		if (mBrushes[i])
			DeleteObject(mBrushes[i]);

	if (mhmenu)
		DestroyMenu(mhmenu);
}

VDLogWindowControl *VDLogWindowControl::Create(HWND hwndParent, int x, int y, int cx, int cy, UINT id) {
	HWND hwnd = CreateWindow(g_szLogWindowControlName, "", WS_VISIBLE|WS_CHILD|WS_VSCROLL, x, y, cx, cy, hwndParent, (HMENU)id, g_hInst, NULL);

	if (hwnd)
		return (VDLogWindowControl *)GetWindowLongPtr(hwnd, 0);

	return NULL;
}

ATOM RegisterLogWindowControl() {
	WNDCLASS wc;

	wc.style		= 0;
	wc.lpfnWndProc	= VDLogWindowControl::StaticWndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= sizeof(VDLogWindowControl *);
	wc.hInstance	= g_hInst;
	wc.hIcon		= NULL;
	wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground= (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszMenuName	= NULL;
	wc.lpszClassName= g_szLogWindowControlName;

	return RegisterClass(&wc);
}

IVDLogWindowControl *VDGetILogWindowControl(HWND hwnd) {
	return static_cast<IVDLogWindowControl *>(reinterpret_cast<VDLogWindowControl *>(GetWindowLongPtr(hwnd, 0)));
}

/////////////////////////////////////////////////////////////////////////////

void VDLogWindowControl::AttachAsLogger(bool bThisThreadOnly) {
	if (!mbAttachedToLogSystem) {
		VDAttachLogger(this, bThisThreadOnly, !bThisThreadOnly);
		PostMessage(mhwnd, WM_TIMER, 1, 0);
	}

	mbAttachedToLogSystem = true;
}

void VDLogWindowControl::AddEntry(int severity, const wchar_t *s) {
	vdsynchronized(mcsPending) {
		mTextArrayPending.push_back(VDStringW(s));
		Entry e = { 0, severity, &mTextArrayPending.back() };
		mLineArrayPending.push_back(e);
	}
}

void VDLogWindowControl::AddEntry(int severity, const VDStringW& s) {
	vdsynchronized(mcsPending) {
		mTextArrayPending.push_back(s);
		Entry e = { 0, severity, &mTextArrayPending.back() };
		mLineArrayPending.push_back(e);
	}
}

/////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK VDLogWindowControl::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDLogWindowControl *pThis = (VDLogWindowControl *)GetWindowLongPtr(hwnd, 0);

	switch(msg) {
	case WM_NCCREATE:
		if (!(pThis = new_nothrow VDLogWindowControl(hwnd)))
			return FALSE;

		SetWindowLongPtr(hwnd, 0, (LONG_PTR)pThis);
		break;

	case WM_NCDESTROY:
		delete pThis;
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	if (pThis)
		return pThis->WndProc(msg, wParam, lParam);
	else
		return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT VDLogWindowControl::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_CREATE:
		{
			RECT r;
			GetClientRect(mhwnd, &r);
			OnSetFont(NULL, FALSE);
			OnSize(r.right, r.bottom);
			SetTimer(mhwnd, 1, 250, NULL);
		}
		return 0;
	case WM_SIZE:
		OnSize(LOWORD(lParam), HIWORD(lParam));
		return 0;
	case WM_PAINT:
		OnPaint();
		return 0;
	case WM_ERASEBKGND:
		return 0;
	case WM_SETFONT:
		OnSetFont((HFONT)wParam, lParam != 0);
		return 0;
	case WM_GETFONT:
		return (LRESULT)mhfont;
	case WM_KEYDOWN:
		if (OnKey(wParam))
			return 0;
		break;
	case WM_VSCROLL:
		if (LOWORD(wParam) != SB_ENDSCROLL && LOWORD(wParam) != SB_THUMBPOSITION) {
			int pos = OnScroll(SB_VERT, LOWORD(wParam), false);

			ScrollTo(pos);
		}
		return 0;
	case WM_TIMER:
		OnTimer();
		return 0;

	case WM_COMMAND:
		OnCommand(LOWORD(wParam));
		return 0;

	case WM_CONTEXTMENU:
		if (mhmenu)
			TrackPopupMenu(GetSubMenu(mhmenu, 0), TPM_LEFTALIGN|TPM_TOPALIGN|TPM_RIGHTBUTTON, (short)LOWORD(lParam), (short)HIWORD(lParam), 0, mhwnd, NULL);
		return 0;
	}
	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

void VDLogWindowControl::OnSize(int w, int h) {
	OnTimer();		// force any pending entries to shuttle over

	if (mLastWidth != w) {
		mLastWidth = w;
		mLastHeight = h;
		SizeEntries(mLineArray.begin());
	} else if (mLastHeight != h) {
		mLastHeight = h;

		tLineArray::iterator it(mLineArray.end());

		--it;
		SizeEntries(it);
	}
}

void VDLogWindowControl::OnPaint() {
	PAINTSTRUCT ps;
	if (HDC hdc = BeginPaint(mhwnd, &ps)) {
		HBRUSH hbrBackground = (HBRUSH)GetClassLongPtr(mhwnd, GCLP_HBRBACKGROUND);
		HGDIOBJ hOldFont = 0;
		if (mhfont)
			hOldFont = SelectObject(hdc, mhfont);

		RECT rClient;
		GetClientRect(mhwnd, &rClient);

		const sint32 ypos = mPos;
		const sint32 y1 = ypos + ps.rcPaint.top;
		const sint32 y2 = ypos + ps.rcPaint.bottom;
		const sint32 x1 = mFontHeight;		// yes, this is correct
		const sint32 x2 = rClient.right;

		tLineArray::const_iterator itEnd(mLineArray.end());
		--itEnd;

		Entry tmp1;
		tmp1.mPos = y1;
		Entry tmp2;
		tmp2.mPos = y2;
		tLineArray::const_iterator itFirst(std::lower_bound((tLineArray::const_iterator)mLineArray.begin(), itEnd, tmp1, Entry::sort));
		tLineArray::const_iterator itLast(std::upper_bound(itFirst, itEnd, tmp2, Entry::sort));

		if (itFirst != mLineArray.begin())
			--itFirst;

		if (itLast == mLineArray.end())
			--itLast;

		HGDIOBJ hOldBrush = 0;

		while(itFirst != itLast) {
			const Entry& ent1 = *itFirst;
			const Entry& ent2 = *++itFirst;
			RECT r = { 0, ent1.mPos - ypos, x2, ent2.mPos - ypos };

			FillRect(hdc, &r, hbrBackground);

			r.left = x1;

			HGDIOBJ hTempOldBrush = SelectObject(hdc, mBrushes[ent1.mSeverity]);

			if (!hOldBrush)
				hOldBrush = hTempOldBrush;

			Ellipse(hdc, mFontInternalLeading, r.top + mFontInternalLeading, mFontAscent, r.top + mFontAscent);

			Draw(hdc, *ent1.mpText, r, false);
		}

		const sint32 ylast = (*itFirst).mPos - ypos;
		if (ylast < ps.rcPaint.bottom) {
			RECT rBottomFill = { 0, ylast, x2, ps.rcPaint.bottom };

			FillRect(hdc, &rBottomFill, hbrBackground);
		}

		if (hOldBrush)
			SelectObject(hdc, hOldBrush);

		if (hOldFont)
			SelectObject(hdc, mhfont);

		EndPaint(mhwnd, &ps);
	}
}

void VDLogWindowControl::OnTimer() {
	bool bChanged = false;
	const tLineArray::size_type pos = mLineArray.size();

	vdsynchronized(mcsPending) {
		if (!mLineArrayPending.empty()) {
			mTextArray.splice(mTextArray.end(), mTextArrayPending);
			
			const tLineArray::size_type count = mLineArrayPending.size();
			sint32 ypos = mLineArray[pos-1].mPos;

			mLineArray.resize(pos + count);
			std::copy(mLineArrayPending.begin(), mLineArrayPending.end(), &mLineArray[pos-1]);
			mLineArray[pos-1].mPos = ypos;
			mLineArray.back().mPos = ypos;
			mLineArrayPending.clear();
			bChanged = true;
		}
	}

	if (bChanged) {
		mLineArray.back().mpText = 0;
		SizeEntries(mLineArray.begin() + (pos-1));
	}
}

void VDLogWindowControl::OnSetFont(HFONT hfont, bool bRedraw) {
	mhfont = hfont;
	SizeEntries(mLineArray.begin());
	if (bRedraw)
		InvalidateRect(mhwnd, NULL, TRUE);

	// stuff in some reasonable defaults if we fail measuring font metrics
	mFontHeight				= 16;
	mFontAscent				= 12;
	mFontInternalLeading	= 0;

	// measure font metrics
	if (HDC hdc = GetDC(mhwnd)) {
		HGDIOBJ hOldFont = 0;
		if (mhfont)
			hOldFont = SelectObject(hdc, mhfont);

		TEXTMETRIC tm;
		if (GetTextMetrics(hdc, &tm)) {
			mFontHeight				= tm.tmHeight;
			mFontAscent				= tm.tmAscent;
			mFontInternalLeading	= tm.tmInternalLeading;
		}

		if (hOldFont)
			SelectObject(hdc, mhfont);
	}
}

void VDLogWindowControl::OnCommand(int cmd) {
	switch(cmd) {
	case ID_LOG_CLEAR:
		{
			Entry e = {0,0};
			mLineArray.clear();
			mLineArray.push_back(e);
			mTextArray.clear();
			SizeEntries(mLineArray.begin());
			InvalidateRect(mhwnd, NULL, TRUE);
		}
		break;
	case ID_LOG_SAVEAS:
		{
			const VDStringW fname(VDGetSaveFileName(kFileDialog_Log, (VDGUIHandle)mhwnd, L"Save log", L"Text file (*.txt)\0*.txt\0", L"txt", NULL, NULL));

			if (!fname.empty()) {
				vdfastvector<char> buffer;

				CreateBuffer(buffer);

				try {
					VDFile file(fname.c_str(), nsVDFile::kWrite | nsVDFile::kDenyAll | nsVDFile::kCreateAlways);

					file.write(buffer.data(), buffer.size());
					file.close();
				} catch(const MyError& e) {
					e.post(mhwnd, g_szError);
				}
			}		
		}
		break;
	case ID_LOG_COPY:
		if (OpenClipboard(mhwnd)) {
			if (EmptyClipboard()) {
				vdfastvector<char> buffer;

				CreateBuffer(buffer);

				HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, buffer.size() + 1);

				if (hMem) {
					LPVOID ptr = GlobalLock(hMem);

					if (ptr) {
						memcpy(ptr, buffer.data(), buffer.size());
						((char *)ptr)[buffer.size()] = 0;
						GlobalUnlock(hMem);
						if (SetClipboardData(CF_TEXT, hMem))
							hMem = NULL;
					}

					if (hMem)
						GlobalFree(hMem);
				}
			}
			CloseClipboard();
		}
		break;
	}
}

bool VDLogWindowControl::OnKey(INT wParam) {
	switch(wParam) {
	case VK_UP:
		if (GetKeyState(VK_CONTROL)<0)
	case VK_HOME:
			ScrollTo(OnScroll(SB_VERT, SB_PAGEUP, true));
		else
			ScrollTo(OnScroll(SB_VERT, SB_LINEUP, false));
		return true;
	case VK_DOWN:
		if (GetKeyState(VK_CONTROL)<0)
	case VK_END:
			ScrollTo(OnScroll(SB_VERT, SB_PAGEDOWN, true));
		else
			ScrollTo(OnScroll(SB_VERT, SB_LINEDOWN, false));
		return true;
	case VK_PRIOR:
		ScrollTo(OnScroll(SB_VERT, SB_PAGEUP, false));
		return true;
	case VK_NEXT:
		ScrollTo(OnScroll(SB_VERT, SB_PAGEDOWN, false));
		return true;
	}

	return false;
}

int VDLogWindowControl::OnScroll(int type, int action, bool bForceExtreme) {
	SCROLLINFO si = { sizeof(SCROLLINFO), SIF_ALL };

	GetScrollInfo(mhwnd, type, &si);

	switch(action) {
	case SB_LEFT:
		--si.nPos;
		break;
	case SB_RIGHT:
		++si.nPos;
		break;
	case SB_LINELEFT:
		si.nPos -= mFontHeight;
		break;
	case SB_LINERIGHT:
		si.nPos += mFontHeight;
		break;
	case SB_PAGELEFT:
		if (bForceExtreme)
			si.nPos = 0;
		else
			si.nPos -= si.nPage;
		break;
	case SB_PAGERIGHT:
		if (bForceExtreme)
			si.nPos = si.nMax;
		else
			si.nPos += si.nPage;
		break;
	case SB_THUMBTRACK:
		si.nPos = si.nTrackPos;
		break;
	}

	if (si.nPos < si.nMin)
		si.nPos = si.nMin;

	mbLockedToBottom = false;
	if (si.nPos >= si.nMax - (int)si.nPage + 1) {
		mbLockedToBottom = true;
		si.nPos = si.nMax - (int)si.nPage + 1;
	}

	si.fMask = SIF_POS | SIF_DISABLENOSCROLL;

	SetScrollInfo(mhwnd, type, &si, TRUE);

	return si.nPos;
}

void VDLogWindowControl::SizeEntries(tLineArray::iterator itBegin) {
	const sint32 ystart = (*itBegin).mPos;
	const sint32 yendold = mLineArray.back().mPos;
	sint32 y = ystart;
	RECT rClient;
	GetClientRect(mhwnd, &rClient);

	if (HDC hdc = GetDC(mhwnd)) {
		HGDIOBJ hOldFont = 0;
		if (mhfont)
			hOldFont = SelectObject(hdc, mhfont);

		tLineArray::iterator itEnd(mLineArray.end());

		for(; itBegin != itEnd; ++itBegin) {
			Entry& ent = *itBegin;

			ent.mPos = y;

			if (ent.mpText) {
				RECT r = { rClient.left + mFontHeight, y, rClient.right, y };

				Draw(hdc, *ent.mpText, r, true);

				y += r.bottom - r.top;
			}
		}

		if (hOldFont)
			SelectObject(hdc, mhfont);
	}

	SCROLLINFO si = { sizeof(SCROLLINFO), SIF_POS };

	if (!GetScrollInfo(mhwnd, SB_VERT, &si)) {
		si.nPos = 0;
	}

	si.nPage	= rClient.bottom;
	si.nMin		= 0;
	si.nMax		= mLineArray.back().mPos - 1;

	if (si.nMax < si.nPage-1)
		si.nMax = si.nPage-1;

	if (mbLockedToBottom)
		si.nPos = si.nMax;

	if (si.nPos > si.nMax - (int)si.nPage + 1)
		si.nPos = si.nMax - (int)si.nPage + 1;
	if (si.nPos < si.nMin)
		si.nPos = si.nMin;

	si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL;

	SetScrollInfo(mhwnd, SB_VERT, &si, TRUE);

	ScrollTo(si.nPos);

	struct local {
		static bool RangesIntersect(int x1, int x2, int y1, int y2) {
			return x2>x1 && y2>y1 && y2>x1 && x2>y1;
		}
	};

	// refresh ystart to y
	if (local::RangesIntersect(si.nPos, si.nPos + si.nPage, ystart, yendold)) {
		RECT rInv = {0, ystart - si.nPos, rClient.right, yendold - si.nPos};
		InvalidateRect(mhwnd, &rInv, TRUE);
	}

	// refresh yendold to y
	if (local::RangesIntersect(si.nPos, si.nPos + si.nPage, yendold, y)) {
		RECT rInv = {0, yendold - si.nPos, rClient.right, y - si.nPos};
		InvalidateRect(mhwnd, &rInv, TRUE);
	}
}

void VDLogWindowControl::ScrollTo(sint32 pos) {
	if (pos != mPos) {
		RECT r;
		GetClientRect(mhwnd, &r);
		ScrollWindow(mhwnd, 0, mPos - pos, NULL, &r);

		mPos = pos;
	}
}

void VDLogWindowControl::Draw(HDC hdc, const VDStringW& s, RECT& r, bool bSizeOnly) {
#if 0
	// This would be the right way to draw text... but it's butt slow on
	// a P4 1.6 running XP, because it burns time in the mallocator in
	// Uniscribe.  It also can't be used under 98, which doesn't support
	// DrawTextW().
	DrawTextW(hdc, s.data(), s.size(), &r, bSizeOnly ? DT_TOP|DT_WORDBREAK|DT_CALCRECT|DT_NOPREFIX|DT_NOCLIP : DT_TOP|DT_WORDBREAK|DT_NOPREFIX|DT_NOCLIP);
#else
	int y = r.top;
	VDStringW::size_type nPos = 0;
	const VDStringW::size_type nChars = s.size();

	while(nPos < nChars) {
		VDStringW::size_type nLineEnd = s.find(L'\n', nPos);

		if (nLineEnd == VDStringW::npos)
			nLineEnd = nChars;

		while(nPos < nLineEnd) {
			INT nMaxChars;
			SIZE siz;

			static const bool bIsWindows9x = (0 != (GetVersion() & 0x80000000));

			if (bIsWindows9x) {
				// Win9x path
				siz.cx = siz.cy = 0;

				nMaxChars = 0;
				for(VDStringW::size_type i=nPos; i<nLineEnd; ++i) {
					SIZE csiz;

					if (!GetTextExtentPoint32W(hdc, s.data() + i, 1, &csiz))
						break;

					int width = siz.cx + csiz.cx;
					if (width > r.right - r.left)
						break;
					++nMaxChars;
					siz.cx = width;
					if (csiz.cy > siz.cy)
						siz.cy = csiz.cy;
				}
			} else {
				// WinNT path
				if (!GetTextExtentExPointW(hdc, s.data() + nPos, nLineEnd - nPos, r.right - r.left, &nMaxChars, NULL, &siz))
					break;
			}

			size_t nEnd = nPos + nMaxChars;

			if (!nMaxChars) {
				// If no characters fit, force one.
				++nEnd;
			} else {
				// check for split in word
				if (nEnd < nLineEnd && s.data()[nEnd] != L' ') {
					while(nEnd > nPos && s.data()[nEnd-1] != L' ')
						--nEnd;

					// check for one-long-word case
					if (nEnd == nPos)
						nEnd += nMaxChars;		// hack the word
				}
			}

			if (!bSizeOnly) {
				RECT rLine = {r.left, y, r.right, y+siz.cy};
				ExtTextOutW(hdc, r.left, y, ETO_OPAQUE, &rLine, s.data() + nPos, nEnd - nPos, NULL);
			}

			nPos = nEnd;
			while(nPos < nLineEnd && s.data()[nPos] == L' ')
				++nPos;

			y += siz.cy;
		}

		nPos = nLineEnd + 1;
	}

	if (bSizeOnly)
		r.bottom = y;
#endif
}

void VDLogWindowControl::CreateBuffer(vdfastvector<char>& buffer) {
	buffer.clear();

	tLineArray::const_iterator it(mLineArray.begin()), itEnd(mLineArray.end());

	// don't process marker
	if (itEnd != it)
		--itEnd;

	for(; it!=itEnd; ++it) {
		const Entry& e = *it;
		const VDStringW& s = *e.mpText;

		// sigh... more word wrapping code....	
		VDStringW::size_type nPos = 0;
		const VDStringW::size_type nChars = s.size();

		while(nPos < nChars) {
			VDStringW::size_type nLineEnd = s.find(L'\n', nPos);

			if (nLineEnd == VDStringW::npos)
				nLineEnd = nChars;

			while(nPos < nLineEnd) {
				const int nMaxChars = std::min<int>(74, nLineEnd - nPos);
				size_t nEnd = nPos + nMaxChars;

				if (!nMaxChars) {
					// If no characters fit, force one.
					++nEnd;
				} else {
					// check for split in word
					if (nEnd < nLineEnd && s.data()[nEnd] != L' ') {
						while(nEnd > nPos && s.data()[nEnd-1] != L' ')
							--nEnd;

						// check for one-long-word case
						if (nEnd == nPos)
							nEnd += nMaxChars;		// hack the word
					}
				}

				int alen = VDTextWToALength(s.data() + nPos, nEnd - nPos);
				int blen = buffer.size();

				buffer.resize(blen + alen + 6, ' ');
				if (!nPos) {
					buffer[blen] = '[';
					buffer[blen+1] = "i*!E"[e.mSeverity];
					buffer[blen+2] = ']';
				}
				VDTextWToA(&buffer[blen + 4], alen, s.data() + nPos, nEnd-nPos);
				buffer[blen+alen+4] = '\r';
				buffer[blen+alen+5] = '\n';

				nPos = nEnd;
				while(nPos < nLineEnd && s.data()[nPos] == L' ')
					++nPos;
			}

			nPos = nLineEnd + 1;
		}

		buffer.push_back('\r');
		buffer.push_back('\n');
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	test harness
//
///////////////////////////////////////////////////////////////////////////

#if 0

namespace {
	struct TestHarness {
		TestHarness() {
			RegisterLogWindowControl();
			HWND foo = CreateWindow(LOGWINDOWCONTROLCLASS, "Log window", WS_OVERLAPPEDWINDOW|WS_VISIBLE|WS_VSCROLL, 0, 0, 400, 300, NULL, NULL, (HINSTANCE)GetModuleHandle(NULL), 0);
			SendMessage(foo, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

			MSG msg;
#if 0
			while(GetMessage(&msg, 0, 0, 0)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
#else
			for(;;) {
				while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				static DWORD dwLastTime = 0;
				DWORD dwCurTime = GetTickCount()/100;

				if (dwCurTime == dwLastTime)
					Sleep(1);
				else {
					dwLastTime = dwCurTime;
					char buf[64];
					sprintf(buf, "Current time: %u", dwCurTime);
					VDGetILogWindowControl(foo)->AddEntry(0, buf);
				}
			}
#endif
		}
	} g_logWindowTestHarness;
}

#endif
