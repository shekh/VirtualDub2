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
#include "oshelper.h"
#include "vbitmap.h"
#include "helpfile.h"
#include <vd2/system/error.h>
#include <vd2/system/thread.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "ClippingControl.h"

#include "capture.h"
#include "capclip.h"
#include "gui.h"

extern HINSTANCE g_hInst;

///////////////////////////////////////////////////////////////////////////

class VDDialogCaptureCropping : public VDDialogBaseW32, public VDCaptureProjectBaseCallback {
public:
	VDDialogCaptureCropping(IVDCaptureProject *pProject) : VDDialogBaseW32(IDD_CAPTURE_CLIPPING), mpProject(pProject) {}

protected:
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void OnInit();
	void OnCleanup();
	void Layout();

	void UICaptureAnalyzeFrame(const VDPixmap& format);

	IVDCaptureProject		*mpProject;
	nsVDCapture::DisplayMode	mOldDisplayMode;

	IVDCaptureProjectCallback	*mpOldCallback;
	VDCaptureFilterSetup	mFilterSetup;

	VDCriticalSection	mDisplayBufferLock;
	VDPixmapBuffer		mDisplayBuffer;

	int		mLastFrameWidth;
	int		mLastFrameHeight;

	bool	mbOldVideoFrameTransferEnabled;
};

INT_PTR VDDialogCaptureCropping::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg)
    {
        case WM_INITDIALOG:
			OnInit();
            return TRUE;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				{
					ClippingControlBounds ccb;

					SendMessage(GetDlgItem(mhdlg, IDC_BORDERS), CCM_GETCLIPBOUNDS, 0, (LPARAM)&ccb);
					mFilterSetup.mCropRect.left = ccb.x1;
					mFilterSetup.mCropRect.top = ccb.y1;
					mFilterSetup.mCropRect.right = ccb.x2;
					mFilterSetup.mCropRect.bottom = ccb.y2;

					OnCleanup();
					End(TRUE);
				}
				return TRUE;
			case IDCANCEL:
				OnCleanup();
				End(FALSE);
				return TRUE;
			}
            break;

		case WM_TIMER:
			break;

		case WM_USER+100:
			vdsynchronized(mDisplayBufferLock) {
				IVDClippingControl *pCC = VDGetIClippingControl((VDGUIHandle)GetDlgItem(mhdlg, IDC_BORDERS));

				if (mLastFrameWidth != mDisplayBuffer.w || mLastFrameHeight != mDisplayBuffer.h) {
					mLastFrameWidth = mDisplayBuffer.w;
					mLastFrameHeight = mDisplayBuffer.h;

					vdrect32 r;
					pCC->GetClipBounds(r);
					pCC->SetBitmapSize(mDisplayBuffer.w, mDisplayBuffer.h);
					Layout();
					pCC->SetClipBounds(r);
				}

				pCC->BlitFrame(&mDisplayBuffer);
			}
			break;

    }
    return FALSE;
}

void VDDialogCaptureCropping::OnInit() {
	mFilterSetup = mpProject->GetFilterSetup();

	IVDClippingControl *pCC = VDGetIClippingControl((VDGUIHandle)GetDlgItem(mhdlg, IDC_BORDERS));
	vdstructex<VDAVIBitmapInfoHeader> bih;

	mLastFrameWidth = 320;
	mLastFrameHeight = 240;

	if (mpProject->GetVideoFormat(bih)) {
		mLastFrameWidth = bih->biWidth;
		mLastFrameHeight = abs(bih->biHeight);

		pCC->SetBitmapSize(mLastFrameWidth, mLastFrameHeight);
	} else
		pCC->SetBitmapSize(320, 240);

	Layout();
	pCC->SetClipBounds(mFilterSetup.mCropRect);

	SetTimer(mhdlg, 1, 500, NULL);

	// save old callback and splice ourselves in
	mpOldCallback = mpProject->GetCallback();
	mpProject->SetCallback(this);

	// save old display mode
	mOldDisplayMode = mpProject->GetDisplayMode();
	mpProject->SetDisplayMode(nsVDCapture::kDisplayNone);

	// clear existing crop rect
	VDCaptureFilterSetup filtSetupNoCrop(mFilterSetup);
	filtSetupNoCrop.mCropRect.set(0, 0, 0, 0);
	mpProject->SetFilterSetup(filtSetupNoCrop);

	// jump to analysis mode
	mpProject->SetDisplayMode(nsVDCapture::kDisplayAnalyze);
	mbOldVideoFrameTransferEnabled = mpProject->IsVideoFrameTransferEnabled();
	mpProject->SetVideoFrameTransferEnabled(true);
}

void VDDialogCaptureCropping::OnCleanup() {
	// restore old display mode
	mpProject->SetVideoFrameTransferEnabled(mbOldVideoFrameTransferEnabled);
	mpProject->SetDisplayMode(nsVDCapture::kDisplayNone);
	mpProject->SetFilterSetup(mFilterSetup);
	mpProject->SetDisplayMode(mOldDisplayMode);

	// restore old callback
	mpProject->SetCallback(mpOldCallback);
}

void VDDialogCaptureCropping::Layout() {
	RECT rw, rc, rcok, rccancel;
	HWND hwnd, hwndCancel;
	LONG hborder, hspace;

	hwnd = GetDlgItem(mhdlg, IDC_BORDERS);
	IVDClippingControl *pCC = VDGetIClippingControl((VDGUIHandle)hwnd);
	GetWindowRect(mhdlg, &rw);
	GetWindowRect(hwnd, &rc);
	hborder = rc.left - rw.left;

	pCC->AutoSize((rw.right - rw.left) - (rc.right - rc.left), (rw.bottom - rw.top) - (rc.bottom - rc.top));
	GetWindowRect(hwnd, &rc);

	MapWindowPoints(NULL, mhdlg, (LPPOINT)&rc, 2);

	RECT rPad = {0,0,7,7};
	MapDialogRect(mhdlg, &rPad);

	hwndCancel = GetDlgItem(mhdlg, IDCANCEL);
	hwnd = GetDlgItem(mhdlg, IDOK);
	GetWindowRect(hwnd, &rcok);
	GetWindowRect(hwndCancel, &rccancel);
	hspace = rccancel.left - rcok.right;
	MapWindowPoints(NULL, mhdlg, (LPPOINT)&rcok, 2);
	MapWindowPoints(NULL, mhdlg, (LPPOINT)&rccancel, 2);

	int yPad = rPad.bottom - rPad.top;
	int y = yPad + rc.bottom;

	RECT rNew = { 0, 0, (rc.right - rc.left) + hborder*2, y + (rccancel.bottom - rccancel.top) + yPad };

	AdjustWindowRect(&rNew, GetWindowLong(mhdlg, GWL_STYLE), FALSE);

	SetWindowPos(mhdlg, NULL, 0, 0, rNew.right - rNew.left, rNew.bottom - rNew.top, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOMOVE);
	SetWindowPos(hwndCancel, NULL, rc.right - (rccancel.right-rccancel.left), y, 0,0,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
	SetWindowPos(hwnd, NULL, rc.right - (rccancel.right-rccancel.left) - (rcok.right-rcok.left) - hspace, y, 0,0,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
}

void VDDialogCaptureCropping::UICaptureAnalyzeFrame(const VDPixmap& format) {
	vdsynchronized(mDisplayBufferLock) {
		mDisplayBuffer.assign(format);
	}

	PostMessage(mhdlg, WM_USER+100, 0, 0);
}

void VDShowCaptureCroppingDialog(VDGUIHandle hParent, IVDCaptureProject *pProject) {
	VDDialogCaptureCropping dlg(pProject);

	dlg.ActivateDialog(hParent);
}
