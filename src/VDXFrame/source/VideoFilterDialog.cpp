//	VDXFrame - Helper library for VirtualDub plugins
//	Copyright (C) 2008 Avery Lee
//
//	The plugin headers in the VirtualDub plugin SDK are licensed differently
//	differently than VirtualDub and the Plugin SDK themselves.  This
//	particular file is thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#include "stdafx.h"
#include <windows.h>
#include <vd2/VDXFrame/VideoFilterDialog.h>

namespace {
#if defined(_MSC_VER) && _MSC_VER >= 1300
		extern "C" char __ImageBase;

	HINSTANCE GetLocalHInstance() {
		return (HINSTANCE)&__ImageBase;
	}
#else
	HINSTANCE GetLocalHInstance() {
		MEMORY_BASIC_INFORMATION meminfo={0};
		if (!VirtualQuery(GetLocalHInstance, &meminfo, sizeof(meminfo)))
			return NULL;

		return (HINSTANCE)meminfo.AllocationBase;
	}
#endif
}

VDXVideoFilterDialog::VDXVideoFilterDialog()
	: mhdlg(NULL)
{
}

LRESULT VDXVideoFilterDialog::Show(HINSTANCE hInst, LPCSTR templName, HWND parent) {
	if (!hInst)
		hInst = GetLocalHInstance();

	return DialogBoxParamA(hInst, templName, parent, StaticDlgProc, (LPARAM)this);
}

LRESULT VDXVideoFilterDialog::Show(HINSTANCE hInst, LPCWSTR templName, HWND parent) {
	if (!hInst)
		hInst = GetLocalHInstance();

	return DialogBoxParamW(hInst, templName, parent, StaticDlgProc, (LPARAM)this);
}

HWND VDXVideoFilterDialog::ShowModeless(HINSTANCE hInst, LPCSTR templName, HWND parent) {
	if (!hInst)
		hInst = GetLocalHInstance();

	return CreateDialogParamA(hInst, templName, parent, StaticDlgProc, (LPARAM)this);
}

HWND VDXVideoFilterDialog::ShowModeless(HINSTANCE hInst, LPCWSTR templName, HWND parent) {
	if (!hInst)
		hInst = GetLocalHInstance();

	return CreateDialogParamW(hInst, templName, parent, StaticDlgProc, (LPARAM)this);
}

INT_PTR CALLBACK VDXVideoFilterDialog::StaticDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDXVideoFilterDialog *pThis;

	if (msg == WM_INITDIALOG) {
		pThis = (VDXVideoFilterDialog *)lParam;
		SetWindowLongPtr(hdlg, DWLP_USER, (LONG_PTR)pThis);
		pThis->mhdlg = hdlg;
	} else
		pThis = (VDXVideoFilterDialog *)GetWindowLongPtr(hdlg, DWLP_USER);

	return pThis ? pThis->DlgProc(msg, wParam, lParam) : FALSE;
}

INT_PTR VDXVideoFilterDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	return FALSE;
}
