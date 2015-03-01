//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2004 Avery Lee
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
#include <vd2/system/w32assist.h>
#include <vd2/Dita/w32control.h>

VDUIControlW32::~VDUIControlW32() {
	Destroy();
}

bool VDUIControlW32::CreateW32(IVDUIParameters *pParms, const char *pClass, DWORD flags) {
	if (!VDUIPeerW32::Create(pParms))
		return false;

	VDUIPeerW32 *pPeer = GetParentPeerW32();
	HWND hwndParent = pPeer ? pPeer->GetHandleW32() : NULL;

	flags |= WS_VISIBLE;

	if (hwndParent)
		flags |= WS_CHILD;

	DWORD exflags = 0;

	if (pParms->GetB(nsVDUI::kUIParam_Raised, !hwndParent))
		exflags |= WS_EX_DLGMODALFRAME;

	if (pParms->GetB(nsVDUI::kUIParam_Sunken, false))
		exflags |= WS_EX_CLIENTEDGE;

	const UINT id = mpBase->GetNextNativeID();

	if (VDIsWindowsNT())
		mhwnd = CreateWindowExW(exflags, VDTextAToW(pClass).c_str(), mCaption.c_str(), flags, 0, 0, 0, 0, hwndParent, (HMENU)id, GetModuleHandle(NULL), NULL);
	else
		mhwnd = CreateWindowExA(exflags, pClass, VDTextWToA(mCaption).c_str(), flags, 0, 0, 0, 0, hwndParent, (HMENU)id, GetModuleHandle(NULL), NULL);

	if (!mhwnd)
		return false;

	SendMessage(mhwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

	if (pPeer)
		pPeer->RegisterCallbackW32(this);

	if (pParms->GetB(nsVDUI::kUIParam_Default, false)) {
		HWND hwndBase = vdpoly_cast<IVDUIWindowW32 *>(mpBase)->GetHandleW32();

		SendMessage(hwndBase, DM_SETDEFID, id, 0);
		if (GetFocus() == hwndBase)
			::SetFocus(mhwnd);
	}

	return true;
}

void VDUIControlW32::Destroy() {
	if (mhwnd) {
		VDUIPeerW32 *pPeer = GetParentPeerW32();
		if (pPeer)
			pPeer->UnregisterCallbackW32(this);

		DestroyWindow(mhwnd);
		mhwnd = NULL;
	}
}

void VDUIControlW32::PreLayoutBase(const VDUILayoutSpecs& specs) {
	PreLayoutBaseW32(specs);

	RECT r = {0,0,mLayoutSpecs.minsize.w,mLayoutSpecs.minsize.h};

	DWORD dwStyle = GetWindowLong(mhwnd, GWL_STYLE);
	bool bMenu = !(dwStyle & WS_CHILD) && GetMenu(mhwnd);

	AdjustWindowRectEx(&r, dwStyle, bMenu, GetWindowLong(mhwnd, GWL_EXSTYLE));

	mLayoutSpecs.minsize.w = r.right-r.left;
	mLayoutSpecs.minsize.h = r.bottom-r.top;
}

SIZE VDUIControlW32::SizeText(int nMaxWidth, int nPadWidth, int nPadHeight) {
	SIZE siz = {0,0};

	if (nMaxWidth) {
		nMaxWidth -= nPadWidth;

		// Uhh, negative is bad....

		if (nMaxWidth < 1)
			nMaxWidth = 1;

	}

	const VDStringW caption(GetCaption());

	if (!VDIsWindowsNT()) {
		VDStringA tempA(VDTextWToA(caption.c_str()));
		const char *str = tempA.c_str();
		HDC hdc;

		if (hdc = GetDC(mhwnd)) {
			HGDIOBJ hgoOldFont;
			RECT r={0,0,nMaxWidth};
			DWORD dwFlags = DT_LEFT|DT_TOP|DT_CALCRECT;

			if (nMaxWidth)
				dwFlags |= DT_WORDBREAK;

			hgoOldFont = SelectObject(hdc, (HGDIOBJ)SendMessage(mhwnd, WM_GETFONT, 0, 0));

			if (DrawTextA(hdc, str, -1, &r, dwFlags)) {
				siz.cx = r.right - r.left;
				siz.cy = r.bottom - r.top;
			}

			SelectObject(hdc, hgoOldFont);

			ReleaseDC(mhwnd, hdc);
		}
	} else {
		HDC hdc;

		if (hdc = GetDC(mhwnd)) {
			HGDIOBJ hgoOldFont;
			RECT r={0,0,nMaxWidth};
			DWORD dwFlags = DT_LEFT|DT_TOP|DT_CALCRECT;

			if (nMaxWidth)
				dwFlags |= DT_WORDBREAK;

			hgoOldFont = SelectObject(hdc, (HGDIOBJ)SendMessage(mhwnd, WM_GETFONT, 0, 0));

			if (DrawTextW(hdc, caption.c_str(), -1, &r, dwFlags)) {
				siz.cx = r.right - r.left;
				siz.cy = r.bottom - r.top;
			}

			SelectObject(hdc, hgoOldFont);

			ReleaseDC(mhwnd, hdc);
		}
	}

	return siz;
}

///////////////////////////////////////////////////////////////////////////

namespace {
	const struct VDDialogTemplateW32 {
		WORD		dlgVer;
		WORD		signature;
		DWORD		helpID;
		DWORD		exStyle;
		DWORD		style;
		WORD		cDlgItems;
		short		x;
		short		y;
		short		cx;
		short		cy;
		short		menu;
		WCHAR		windowClass[18];
		WCHAR		title;
		WORD		pointsize;
		WORD		weight;
		BYTE		italic;
		BYTE		charset;
		WCHAR		typeface[13];
	} g_dummyDialogDef={
		1,					// dlgVer
		0xFFFF,				// signature
		0,					// helpID
		0,					// exStyle
		WS_VISIBLE|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_CLIPSIBLINGS|DS_NOIDLEMSG|DS_SETFONT,
		0,
		0,
		0,
		0,
		0,
		0,					// menu
		L"DitaCustomControl",	// windowClass
		0,					// title
		8,					// pointsize
		0,					// weight
		0,					// italic
		0,					// charset
		L"MS Shell Dlg"		// typeface
	}, g_dummyDialogDefChild={
		1,					// dlgVer
		0xFFFF,				// signature
		0,					// helpID
		0,					// exStyle
		WS_VISIBLE|WS_CHILD|WS_CLIPSIBLINGS|DS_NOIDLEMSG|DS_SETFONT|DS_CONTROL,
		0,
		0,
		0,
		0,
		0,
		0,					// menu
		L"DitaCustomControl",	// windowClass
		0,					// title
		8,					// pointsize
		0,					// weight
		0,					// italic
		0,					// charset
		L"MS Shell Dlg"		// typeface
	};
}

ATOM VDUICustomControlW32::sWindowClass = NULL;

bool VDUICustomControlW32::Create(IVDUIParameters *pParms, bool forceNonChild, DWORD flags) {
	if (!VDUIPeerW32::Create(pParms))
		return false;

	if (!sWindowClass) {
		if (VDIsWindowsNT()) {
			WNDCLASSW wc;

			wc.cbClsExtra		= 0;
			wc.cbWndExtra		= DLGWINDOWEXTRA + sizeof(VDUICustomControlW32 *);
			wc.hbrBackground	= (HBRUSH)(COLOR_3DFACE+1);
			wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
			wc.hIcon			= 0;
			wc.hInstance		= GetModuleHandle(NULL);
			wc.lpfnWndProc		= StaticWndProc;
			wc.lpszClassName	= L"DitaCustomControl";
			wc.lpszMenuName		= NULL;
			wc.style			= 0;

			sWindowClass = RegisterClassW(&wc);
			if (!sWindowClass)
				return false;
		} else {
			WNDCLASSA wc;

			wc.cbClsExtra		= 0;
			wc.cbWndExtra		= DLGWINDOWEXTRA + sizeof(VDUICustomControlW32 *);
			wc.hbrBackground	= (HBRUSH)(COLOR_3DFACE+1);
			wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
			wc.hIcon			= 0;
			wc.hInstance		= GetModuleHandle(NULL);
			wc.lpfnWndProc		= StaticWndProc;
			wc.lpszClassName	= "DitaCustomControl";
			wc.lpszMenuName		= NULL;
			wc.style			= 0;

			sWindowClass = RegisterClassA(&wc);
			if (!sWindowClass)
				return false;
		}
	}

	HWND hwndParent = GetParentW32();
	DWORD exflags = 0;

	if (pParms->GetB(nsVDUI::kUIParam_Raised, !hwndParent))
		exflags |= WS_EX_DLGMODALFRAME;

	if (pParms->GetB(nsVDUI::kUIParam_Sunken, false))
		exflags |= WS_EX_CLIENTEDGE;

	VDDialogTemplateW32 templ;

	if (hwndParent && !forceNonChild)
		templ = g_dummyDialogDefChild;
	else
		templ = g_dummyDialogDef;

	templ.style |= flags;
	templ.exStyle = exflags;

	if (VDIsWindowsNT())
		mhwnd = CreateDialogIndirectParamW(GetModuleHandle(NULL), (LPCDLGTEMPLATE)&templ, hwndParent, StaticDlgProc, (LPARAM)this);
	else
		mhwnd = CreateDialogIndirectParamA(GetModuleHandle(NULL), (LPCDLGTEMPLATE)&templ, hwndParent, StaticDlgProc, (LPARAM)this);

	if (mhwnd) {
		VDSetWindowTextW32(mhwnd, mCaption.c_str());
	}

	return !!mhwnd;
}

void VDUICustomControlW32::Destroy() {
	if (mhwnd)
		DestroyWindow(mhwnd);
}

INT_PTR CALLBACK VDUICustomControlW32::StaticDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	return FALSE;
}

LRESULT CALLBACK VDUICustomControlW32::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDUICustomControlW32 *pThis;

	if (msg == WM_INITDIALOG) {
		pThis = (VDUICustomControlW32 *)lParam;
		pThis->mhwnd = hwnd;

		SetWindowLongPtr(hwnd, DLGWINDOWEXTRA, (LONG_PTR)pThis);
	} else
		pThis = (VDUICustomControlW32 *)GetWindowLongPtr(hwnd, DLGWINDOWEXTRA);

	return pThis ? pThis->WndProc(msg, wParam, lParam) : (VDIsWindowsNT() ? DefDlgProcW : DefDlgProcA)(hwnd, msg, wParam, lParam);
}

namespace {
	struct VDUIControlDialogW32ValidateData {
		HWND hdlg;
		HDC hdc;
	};

	static BOOL CALLBACK ValidateEnumerator(HWND hwnd, LPARAM pData) {
		VDUIControlDialogW32ValidateData& data = *(VDUIControlDialogW32ValidateData *)pData;
		HBRUSH hbrBackground = (HBRUSH)GetClassLong(hwnd, GCLP_HBRBACKGROUND);

		if (hbrBackground) {
			RECT r;

			GetWindowRect(hwnd, &r);
			MapWindowPoints(NULL, data.hdlg, (LPPOINT)&r, 2);
			ExcludeClipRect(data.hdc, r.left, r.top, r.right, r.bottom);
		}

		return TRUE;
	}
}

LRESULT VDUICustomControlW32::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_SIZE:
		OnResize();

		if (!(GetWindowLong(mhwnd, GWL_STYLE) & WS_CHILD)) {
			vduirect r(GetClientArea());

			tChildren::iterator it(mChildren.begin()), itEnd(mChildren.end());

			for(; it!=itEnd; ++it) {
				IVDUIWindow *pWin = *it;

				pWin->PostLayout(r);
			}
		}
		break;

	case WM_DESTROY:
		mhwnd = NULL;
		break;

	case WM_ERASEBKGND:
		{
			VDUIControlDialogW32ValidateData data = {mhwnd, (HDC)wParam};

			EnumChildWindows(mhwnd, ValidateEnumerator, (LPARAM)&data);
		}
		break;

	case WM_COMMAND:
		{
			tCallbacks::const_iterator it(mCallbacks.find((HWND)lParam));

			if (it != mCallbacks.end()) {
				VDUIPeerW32 *pPeer = (*it).second;

				pPeer->OnCommandCallback(HIWORD(wParam));
				return 0;
			}
		}
		break;

	case WM_NOTIFY:
		{
			const NMHDR& hdr = *(const NMHDR *)lParam;
			tCallbacks::const_iterator it(mCallbacks.find(hdr.hwndFrom));

			if (it != mCallbacks.end()) {
				VDUIPeerW32 *pPeer = (*it).second;

				pPeer->OnNotifyCallback(&hdr);
				return 0;
			}
		}

	case WM_HSCROLL:
	case WM_VSCROLL:
		{
			tCallbacks::const_iterator it(mCallbacks.find((HWND)lParam));

			if (it != mCallbacks.end()) {
				VDUIPeerW32 *pPeer = (*it).second;

				pPeer->OnScrollCallback(LOWORD(wParam));
				return 0;
			}
		}
		break;
	}

	return (VDIsWindowsNT() ? DefDlgProcW : DefDlgProcA)(mhwnd, msg, wParam, lParam);
}
