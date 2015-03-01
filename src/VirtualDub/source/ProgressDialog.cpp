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

#include "resource.h"

#include "ProgressDialog.h"

#include <vd2/system/error.h>

extern HINSTANCE g_hInst;

ProgressDialog::ProgressDialog(HWND hwndParent, const char *szTitle, const char *szCaption, long _maxval, bool _fAbortEnabled) 
	:lpszTitle(szTitle)
	,lpszCaption(szCaption)
	,lpszValueFormat(NULL)
	,maxval(_maxval)
	,curval(0)
	,newval(0)
	,mSparseCount(1)
	,mSparseInterval(1)
	,fAbortEnabled(_fAbortEnabled)
	,fAbort(false)
	,hwndProgressBar(NULL)
	,hwndValue(NULL)
	,hwndDialog(NULL)
	,mhwndParent(hwndParent)
{
	dwLastTime = GetTickCount();

	CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_PROGRESS), hwndParent, ProgressDlgProc, (LPARAM)this);
}

ProgressDialog::~ProgressDialog() {
	close();
}

void ProgressDialog::setCaption(const char *sz) {
	lpszCaption = sz;

	if (hwndDialog)
		SetDlgItemText(hwndDialog, IDC_STATIC_MESSAGE, sz);
}

void ProgressDialog::setValueFormat(const char *sz) {
	lpszValueFormat = sz;
}

void ProgressDialog::setLimit(long lim) {
	curval = 0;		// force a bar update
	maxval = lim;
}

void ProgressDialog::check() {
	MSG msg;

	if (--mSparseCount)
		return;

	DWORD dwTime = GetTickCount();

	mSparseCount = mSparseInterval;

	if (dwTime < dwLastTime + 50) {
		++mSparseInterval;
	} else if (dwTime > dwLastTime + 150) {
		if (mSparseInterval>1)
			--mSparseInterval;
	}

	dwLastTime = dwTime;

	while(PeekMessage(&msg, mhwndParent ? NULL : hwndDialog, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT && fAbortEnabled) {
			PostQuitMessage(msg.wParam);
			throw MyUserAbortError();
		}

		if (!IsWindow(hwndDialog) || !IsDialogMessage(hwndDialog, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (fAbort)
		throw MyUserAbortError();
}

void ProgressDialog::close() {
	if (hwndDialog) {
		if (mhwndParent)
			EnableWindow(mhwndParent, mbPreviouslyEnabled);
		DestroyWindow(hwndDialog);
		hwndDialog = 0;
	}
}

INT_PTR CALLBACK ProgressDialog::ProgressDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	ProgressDialog *thisPtr = (ProgressDialog *)GetWindowLongPtr(hDlg, DWLP_USER);
	int newval2;

	switch(msg) {
		case WM_INITDIALOG:
			SetWindowLongPtr(hDlg, DWLP_USER, lParam);

			thisPtr = (ProgressDialog *)lParam;
			thisPtr->hwndProgressBar = GetDlgItem(hDlg, IDC_PROGRESS);
			thisPtr->hwndValue = GetDlgItem(hDlg, IDC_CURRENT_VALUE);
			SendMessage(GetDlgItem(hDlg, IDC_PROGRESS), PBM_SETRANGE, 0, MAKELPARAM(0, 16384));

			if (!thisPtr->fAbortEnabled)
				EnableWindow(GetDlgItem(hDlg, IDCANCEL), FALSE);

			SetWindowText(hDlg, thisPtr->lpszTitle);
			SetDlgItemText(hDlg, IDC_STATIC_MESSAGE, thisPtr->lpszCaption);

			thisPtr->hwndDialog = hDlg;

			if (thisPtr->mhwndParent) {
				thisPtr->mbPreviouslyEnabled = !!IsWindowEnabled(thisPtr->mhwndParent);
				EnableWindow(thisPtr->mhwndParent, FALSE);
			}

			SetTimer(hDlg, 1, 500, NULL);

			{
				bool vis = true;

				if (HWND hwndParent = GetParent(hDlg)) {
					while (GetWindowLong(hwndParent, GWL_STYLE) & WS_CHILD)
						hwndParent = GetParent(hwndParent);

					if (IsIconic(hwndParent))
						vis = false;
				}

				ShowWindow(hDlg, vis ? SW_SHOW : SW_SHOWMINNOACTIVE);
			}
			break;

		case WM_TIMER:
			newval2 = MulDiv(thisPtr->newval, 16384, thisPtr->maxval);

			if (newval2 > thisPtr->curval) {
				if (newval2 > 16384) newval2 = 16384;
				thisPtr->curval = newval2;

				SendMessage(thisPtr->hwndProgressBar, PBM_SETPOS, (WPARAM)newval2, 0);
			}

			if (thisPtr->lpszValueFormat) {
				char szTemp[128];

				wsprintf(szTemp, thisPtr->lpszValueFormat, thisPtr->newval, thisPtr->maxval);
				SendMessage(thisPtr->hwndValue, WM_SETTEXT, 0, (LPARAM)szTemp);
			}
			return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDCANCEL)
				thisPtr->fAbort = true;
			return TRUE;
	}

	return FALSE;
}
