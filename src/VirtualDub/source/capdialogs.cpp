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

#include "stdafx.h"
#include <commctrl.h>
#include <vd2/system/registry.h>
#include <vd2/system/filesys.h>
#include <vd2/system/file.h>
#include <vd2/system/error.h>
#include <vd2/Dita/controls.h>
#include <vd2/Dita/interface.h>
#include <vd2/Dita/resources.h>
#include <vd2/Dita/w32peer.h>
#include <vd2/Dita/bytecode.h>
#include "misc.h"
#include "oshelper.h"
#include "gui.h"
#include "resource.h"
#include "capture.h"
#include "capdialogs.h"

#include <vfw.h>

extern const char g_szError[];
extern const char g_szCapture[];
extern const char g_szAdjustVideoTiming[];
extern const char g_szChunkSize[];
extern const char g_szChunkCount[];
extern const char g_szDisableBuffering[];
extern const char g_szStopConditions[];

namespace {
	enum { kVDST_CaptureUI = 8 };

	enum {
		kVDM_CannotDisplayFormat,
		kVDMBase_CapInfoLabels		= 100,
		kVDMBase_CapInfoTypeLabels	= 200
	};
}

////////////////////////////////////////////////////////////////////////////
//
//	disk I/O dialog
//
////////////////////////////////////////////////////////////////////////////

class VDDialogCaptureDiskIO : public VDDialogBaseW32 {
public:
	VDDialogCaptureDiskIO(VDCaptureDiskSettings& sets) : VDDialogBaseW32(IDD_CAPTURE_DISKIO), mDiskSettings(sets) {}

protected:
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

	VDCaptureDiskSettings& mDiskSettings;
};

INT_PTR VDDialogCaptureDiskIO::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {

	static const long sizes[]={
		64,
		128,
		256,
		512,
		1*1024,
		2*1024,
		4*1024,
		6*1024,
		8*1024,
		12*1024,
		16*1024,
	};

	static const char *size_names[]={
		"64K",
		"128K",
		"256K",
		"512K",
		"1MB",
		"2MB",
		"4MB",
		"6MB",
		"8MB",
		"12MB",
		"16MB",
	};

	switch(msg) {
	case WM_INITDIALOG:
		{
			int i;
			HWND hwndItem;

			hwndItem = GetDlgItem(mhdlg, IDC_CHUNKSIZE);
			for(i=0; i<sizeof sizes/sizeof sizes[0]; i++)
				SendMessage(hwndItem, CB_ADDSTRING, 0, (LPARAM)size_names[i]);
			SendMessage(hwndItem, CB_SETCURSEL, NearestLongValue(mDiskSettings.mDiskChunkSize, sizes, sizeof sizes/sizeof sizes[0]), 0);

			SendDlgItemMessage(mhdlg, IDC_CHUNKS_UPDOWN, UDM_SETBUDDY, (WPARAM)GetDlgItem(mhdlg, IDC_CHUNKS), 0);
			SendDlgItemMessage(mhdlg, IDC_CHUNKS_UPDOWN, UDM_SETRANGE, 0, MAKELONG(256, 1));

			SetDlgItemInt(mhdlg, IDC_CHUNKS, mDiskSettings.mDiskChunkCount, FALSE);
			CheckDlgButton(mhdlg, IDC_DISABLEBUFFERING, mDiskSettings.mbDisableWriteCache ? BST_CHECKED : BST_UNCHECKED);
		}
		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
		case IDC_ACCEPT:
			{
				BOOL fOk;
				int chunks;

				chunks = GetDlgItemInt(mhdlg, IDC_CHUNKS, &fOk, FALSE);

				if (!fOk || chunks<1 || chunks>256) {
					SetFocus(GetDlgItem(mhdlg, IDC_CHUNKS));
					MessageBeep(MB_ICONQUESTION);
					return TRUE;
				}

				mDiskSettings.mDiskChunkCount = chunks;
				mDiskSettings.mDiskChunkSize = sizes[SendDlgItemMessage(mhdlg, IDC_CHUNKSIZE, CB_GETCURSEL, 0, 0)];
				mDiskSettings.mbDisableWriteCache = !!IsDlgButtonChecked(mhdlg, IDC_DISABLEBUFFERING);

				if (LOWORD(wParam) == IDOK) {
					VDRegistryAppKey key(g_szCapture);

					key.setInt(g_szChunkCount, mDiskSettings.mDiskChunkCount);
					key.setInt(g_szChunkSize, mDiskSettings.mDiskChunkSize);
					key.setInt(g_szDisableBuffering, mDiskSettings.mbDisableWriteCache);
				}
			}
			End(true);
			return TRUE;
		case IDCANCEL:
			End(0);
			return TRUE;

		case IDC_CHUNKS:
		case IDC_CHUNKSIZE:
			{
				BOOL fOk;
				int chunks;
				int cs;

				chunks = GetDlgItemInt(mhdlg, IDC_CHUNKS, &fOk, FALSE);

				if (fOk) {
					char buf[64];

					cs = SendDlgItemMessage(mhdlg, IDC_CHUNKSIZE, CB_GETCURSEL, 0, 0);

					sprintf(buf, "Total buffer: %ldK", sizes[cs] * chunks);
					SetDlgItemText(mhdlg, IDC_STATIC_BUFFERSIZE, buf);
				} else
					SetDlgItemText(mhdlg, IDC_STATIC_BUFFERSIZE, "Total buffer: ---");
			}
			return TRUE;
		}
		return FALSE;
	}

	return FALSE;
}

bool VDShowCaptureDiskIODialog(VDGUIHandle hwndParent, VDCaptureDiskSettings& sets) {
	VDDialogCaptureDiskIO dlg(sets);

	return 0 != dlg.ActivateDialog(hwndParent);
}


////////////////////////////////////////////////////////////////////////////
//
//	noise reduction threshold dlg
//
////////////////////////////////////////////////////////////////////////////

class VDDialogCaptureNRThreshold : public VDDialogBaseW32 {
public:
	VDDialogCaptureNRThreshold(IVDCaptureProject *pProject) : VDDialogBaseW32(IDD_CAPTURE_NOISEREDUCTION), mpProject(pProject) {}

protected:
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

	bool PreNCDestroy() {
		return true;
	}

	ModelessDlgNode mDlgNode;

	IVDCaptureProject *const mpProject;
};

namespace {
	VDDialogCaptureNRThreshold *g_pCapNRDialog;
}

INT_PTR VDDialogCaptureNRThreshold::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	HWND hwndItem;
	switch(msg) {
		case WM_INITDIALOG:
			{
				const VDCaptureFilterSetup& filtSetup = mpProject->GetFilterSetup();

				hwndItem = GetDlgItem(mhdlg, IDC_THRESHOLD);
				SendMessage(hwndItem, TBM_SETRANGE, FALSE, MAKELONG(0, 64));
				SendMessage(hwndItem, TBM_SETPOS, TRUE, filtSetup.mNRThreshold);
				mDlgNode.hdlg = mhdlg;
				guiAddModelessDialog(&mDlgNode);
				g_pCapNRDialog = this;
			}
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDCANCEL:
				DestroyModeless();
				break;
			}
			return TRUE;

		case WM_CLOSE:
			DestroyModeless();
			break;

		case WM_DESTROY:
			mDlgNode.Remove();
			mDlgNode.hdlg = NULL;
			g_pCapNRDialog = NULL;
			return TRUE;

		case WM_HSCROLL:
			{
				VDCaptureFilterSetup filtSetup(mpProject->GetFilterSetup());
				int thresh = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);

				if (filtSetup.mNRThreshold != thresh) {
					filtSetup.mNRThreshold = thresh;

					mpProject->SetFilterSetup(filtSetup);
				}
			}
			return TRUE;
	}

	return FALSE;
}

void VDToggleCaptureNRDialog(VDGUIHandle hwndParent, IVDCaptureProject *pProject) {
	if (!hwndParent) {
		if (g_pCapNRDialog)
			g_pCapNRDialog->DestroyModeless();
		return;
	}

	if (g_pCapNRDialog)
		SetForegroundWindow(g_pCapNRDialog->GetHandle());
	else {
		g_pCapNRDialog = new VDDialogCaptureNRThreshold(pProject);
		g_pCapNRDialog->CreateModeless(hwndParent);
	}
}


////////////////////////////////////////////////////////////////////////////
//
//	settings dlg
//
////////////////////////////////////////////////////////////////////////////

class VDDialogCaptureSettings : public VDDialogBaseW32 {
public:
	VDDialogCaptureSettings(VDCaptureSettings& parms) : VDDialogBaseW32(IDD_CAPTURE_SETTINGS), mParms(parms) {}

protected:
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

	ModelessDlgNode mDlgNode;
	VDCaptureSettings&	mParms;
};

INT_PTR VDDialogCaptureSettings::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_INITDIALOG:
			{
				char buf[32];

				sprintf(buf, "%.4f", 10000000.0 / mParms.mFramePeriod);
				SendMessage(GetDlgItem(mhdlg, IDC_CAPTURE_FRAMERATE), WM_SETTEXT, 0, (LPARAM)buf);

				CheckDlgButton(mhdlg, IDC_CAPTURE_ON_OK, mParms.mbDisplayPrerollDialog ? 1 : 0);
			}	
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_ROUND_FRAMERATE:
				{
					double dFrameRate;
					char buf[32]={0};

					SendMessage(GetDlgItem(mhdlg, IDC_CAPTURE_FRAMERATE), WM_GETTEXT, sizeof buf - 1, (LPARAM)buf);
					if (1!=sscanf(buf, " %lg ", &dFrameRate) || dFrameRate<=0.01 || dFrameRate>1000.0) {
						MessageBeep(MB_ICONQUESTION);
						SetFocus(GetDlgItem(mhdlg, IDC_CAPTURE_FRAMERATE));
						return TRUE;
					}

					// Man, the LifeView driver really sucks...

					sprintf(buf, "%.4lf", floor(10000000.0 / floor(1000.0 / dFrameRate + .5))/10000.0);
					SetDlgItemText(mhdlg, IDC_CAPTURE_FRAMERATE, buf);
				}
				return TRUE;
			case IDOK:
				{
					double dFrameRate;
					char buf[32];

					SendMessage(GetDlgItem(mhdlg, IDC_CAPTURE_FRAMERATE), WM_GETTEXT, sizeof buf, (LPARAM)buf);
					if (1!=sscanf(buf, " %lg ", &dFrameRate) || dFrameRate<=0.01 || dFrameRate>1000.0) {
						MessageBeep(MB_ICONQUESTION);
						SetFocus(GetDlgItem(mhdlg, IDC_CAPTURE_FRAMERATE));
						return TRUE;
					}
					mParms.mFramePeriod = (DWORD)(10000000.0 / dFrameRate);

					mParms.mbDisplayPrerollDialog = !!IsDlgButtonChecked(mhdlg, IDC_CAPTURE_ON_OK);
				}
				End(true);
				break;
			case IDCANCEL:
				End(false);
				break;
			}
			return TRUE;

		case WM_HELP:
			{
				HELPINFO *lphi = (HELPINFO *)lParam;

				if (lphi->iContextType == HELPINFO_WINDOW)
					VDShowHelp(mhdlg, L"d-capturesettings.html");
			}
			return TRUE;
	}

	return FALSE;
}

bool VDShowCaptureSettingsDialog(VDGUIHandle hwndParent, VDCaptureSettings& parms) {
	VDDialogCaptureSettings dlg(parms);

	return 0 != dlg.ActivateDialog(hwndParent);
}


///////////////////////////////////////////////////////////////////////////
//
//	'Allocate disk space' dialog
//
///////////////////////////////////////////////////////////////////////////

class VDDialogCaptureAllocate : public VDDialogBaseW32 {
public:
	VDDialogCaptureAllocate(const VDStringW& path) : VDDialogBaseW32(IDD_CAPTURE_PREALLOCATE), mPath(path) {}

protected:
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

	const VDStringW&		mPath;
};

INT_PTR VDDialogCaptureAllocate::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {

		case WM_INITDIALOG:
			{
				sint64 client_free = VDGetDiskFreeSpace(VDFileSplitPathLeft(mPath).c_str());

				SetDlgItemText(mhdlg, IDC_STATIC_DISK_FREE_SPACE, "Free disk space:");

				if (client_free>=0) {
					char pb[64];
					wsprintf(pb, "%ld MB ", (long)(client_free>>20));
					SetDlgItemText(mhdlg, IDC_DISK_FREE_SPACE, pb);
				}

				SetFocus(GetDlgItem(mhdlg, IDC_DISK_SPACE_ALLOCATE));
			}	

			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				{
					LONG lAllocate;
					BOOL fOkay;

					lAllocate = GetDlgItemInt(mhdlg, IDC_DISK_SPACE_ALLOCATE, &fOkay, FALSE);

					if (!fOkay || lAllocate<0) {
						MessageBeep(MB_ICONQUESTION);
						SetFocus(GetDlgItem(mhdlg, IDC_DISK_SPACE_ALLOCATE));
						return TRUE;
					}

					try {
						bool bAttemptExtension = VDFile::enableExtendValid();

						VDFile file(mPath.c_str(), nsVDFile::kWrite | nsVDFile::kDenyAll | nsVDFile::kOpenAlways);

						file.seek((sint64)lAllocate << 20);
						file.truncate();

						// attempt to extend valid part of file to avoid preclear cost if admin privileges
						// are available
						if (bAttemptExtension)
							file.extendValidNT((sint64)lAllocate << 20);

						// zap the first 64K of the file so it isn't recognized as a valid anything
						if (lAllocate) {
							vdblock<char> tmp(65536);
							memset(tmp.data(), 0, tmp.size());

							file.seek(0);
							file.write(tmp.data(), tmp.size());
						}
					} catch(const MyError& e) {
						e.post(mhdlg, g_szError);
					}
				}

				End(true);
				return TRUE;
			case IDCANCEL:
				End(false);
				return TRUE;
			}
			break;
	}

	return FALSE;
}

bool VDShowCaptureAllocateDialog(VDGUIHandle hwndParent, const VDStringW& path) {
	VDDialogCaptureAllocate dlg(path);

	return 0 != dlg.ActivateDialog(hwndParent);
}

////////////////////////////////////////////////////////////////////////////
//
//	stop conditions
//
////////////////////////////////////////////////////////////////////////////

class VDDialogCaptureStopPrefs : public VDDialogBaseW32 {
public:
	VDDialogCaptureStopPrefs(VDCaptureStopPrefs& prefs) : VDDialogBaseW32(IDD_CAPTURE_STOPCOND), mPrefs(prefs) {}

protected:
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

	VDCaptureStopPrefs& mPrefs;
};

INT_PTR VDDialogCaptureStopPrefs::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {

	case WM_INITDIALOG:
		EnableWindow(GetDlgItem(mhdlg, IDC_TIMELIMIT_SETTING), mPrefs.fEnableFlags & CAPSTOP_TIME);
		EnableWindow(GetDlgItem(mhdlg, IDC_FILELIMIT_SETTING), mPrefs.fEnableFlags & CAPSTOP_FILESIZE);
		EnableWindow(GetDlgItem(mhdlg, IDC_DISKLIMIT_SETTING), mPrefs.fEnableFlags & CAPSTOP_DISKSPACE);
		EnableWindow(GetDlgItem(mhdlg, IDC_DROPLIMIT_SETTING), mPrefs.fEnableFlags & CAPSTOP_DROPRATE);

		CheckDlgButton(mhdlg, IDC_TIMELIMIT, mPrefs.fEnableFlags & CAPSTOP_TIME ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(mhdlg, IDC_FILELIMIT, mPrefs.fEnableFlags & CAPSTOP_FILESIZE ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(mhdlg, IDC_DISKLIMIT, mPrefs.fEnableFlags & CAPSTOP_DISKSPACE ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(mhdlg, IDC_DROPLIMIT, mPrefs.fEnableFlags & CAPSTOP_DROPRATE ? BST_CHECKED : BST_UNCHECKED);

		SetDlgItemInt(mhdlg, IDC_TIMELIMIT_SETTING, mPrefs.lTimeLimit, FALSE);
		SetDlgItemInt(mhdlg, IDC_FILELIMIT_SETTING, mPrefs.lSizeLimit, FALSE);
		SetDlgItemInt(mhdlg, IDC_DISKLIMIT_SETTING, mPrefs.lDiskSpaceThreshold, FALSE);
		SetDlgItemInt(mhdlg, IDC_DROPLIMIT_SETTING, mPrefs.lMaxDropRate, FALSE);

		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
		case IDC_ACCEPT:
			mPrefs.lTimeLimit = GetDlgItemInt(mhdlg, IDC_TIMELIMIT_SETTING, NULL, FALSE);
			mPrefs.lSizeLimit = GetDlgItemInt(mhdlg, IDC_FILELIMIT_SETTING, NULL, FALSE);
			mPrefs.lDiskSpaceThreshold = GetDlgItemInt(mhdlg, IDC_DISKLIMIT_SETTING, NULL, FALSE);
			mPrefs.lMaxDropRate = GetDlgItemInt(mhdlg, IDC_DROPLIMIT_SETTING, NULL, FALSE);
			mPrefs.fEnableFlags = 0;

			if (IsDlgButtonChecked(mhdlg, IDC_TIMELIMIT))
				mPrefs.fEnableFlags |= CAPSTOP_TIME;

			if (IsDlgButtonChecked(mhdlg, IDC_FILELIMIT))
				mPrefs.fEnableFlags |= CAPSTOP_FILESIZE;

			if (IsDlgButtonChecked(mhdlg, IDC_DISKLIMIT))
				mPrefs.fEnableFlags |= CAPSTOP_DISKSPACE;

			if (IsDlgButtonChecked(mhdlg, IDC_DROPLIMIT))
				mPrefs.fEnableFlags |= CAPSTOP_DROPRATE;

			if (LOWORD(wParam) == IDOK) {
				VDRegistryAppKey key(g_szCapture);

				key.setBinary(g_szStopConditions, (char *)&mPrefs, sizeof mPrefs);
			}

			End(1);
			return TRUE;

		case IDCANCEL:
			End(0);
			return TRUE;

		case IDC_TIMELIMIT:
			EnableWindow(GetDlgItem(mhdlg, IDC_TIMELIMIT_SETTING), SendMessage((HWND)lParam, BM_GETSTATE, 0, 0) & BST_CHECKED);
			return TRUE;

		case IDC_FILELIMIT:
			EnableWindow(GetDlgItem(mhdlg, IDC_FILELIMIT_SETTING), SendMessage((HWND)lParam, BM_GETSTATE, 0, 0) & BST_CHECKED);
			return TRUE;

		case IDC_DISKLIMIT:
			EnableWindow(GetDlgItem(mhdlg, IDC_DISKLIMIT_SETTING), SendMessage((HWND)lParam, BM_GETSTATE, 0, 0) & BST_CHECKED);
			return TRUE;

		case IDC_DROPLIMIT:
			EnableWindow(GetDlgItem(mhdlg, IDC_DROPLIMIT_SETTING), SendMessage((HWND)lParam, BM_GETSTATE, 0, 0) & BST_CHECKED);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

bool VDShowCaptureStopPrefsDialog(VDGUIHandle hwndParent, VDCaptureStopPrefs& prefs) {
	VDDialogCaptureStopPrefs dlg(prefs);

	return 0 != dlg.ActivateDialog(hwndParent);
}

//////////////////////////////////////////////////////////////////////////////
//
//	preferences
//
//////////////////////////////////////////////////////////////////////////////

class VDDialogCapturePreferencesInfoPanel : public VDDialogBase {
public:
	VDCapturePreferences& mPrefs;
	VDDialogCapturePreferencesInfoPanel(VDCapturePreferences& p) : mPrefs(p) {}

	bool HandleUIEvent(IVDUIBase *pBase, IVDUIWindow *pWin, uint32 id, eEventType type, int item) {
		static const uint32 kItems[]={
			kVDCaptureInfo_FramesCaptured,
			kVDCaptureInfo_TotalTime,
			kVDCaptureInfo_TimeLeft,
			kVDCaptureInfo_TotalFileSize,
			kVDCaptureInfo_DiskSpaceFree,
			kVDCaptureInfo_CPUUsage,
			kVDCaptureInfo_SpillStatus,
			kVDCaptureInfo_VideoSize,
			kVDCaptureInfo_VideoAverageRate,
			kVDCaptureInfo_VideoDataRate,
			kVDCaptureInfo_VideoCompressionRatio,
			kVDCaptureInfo_VideoAverageFrameSize,
			kVDCaptureInfo_VideoFramesDropped,
			kVDCaptureInfo_VideoFramesInserted,
			kVDCaptureInfo_VideoResamplingFactor,
			kVDCaptureInfo_AudioSize,
			kVDCaptureInfo_AudioAverageRate,
			kVDCaptureInfo_AudioRelativeRate,
			kVDCaptureInfo_AudioDataRate,
			kVDCaptureInfo_AudioCompressionRatio,
			kVDCaptureInfo_AudioResamplingFactor,
			kVDCaptureInfo_SyncVideoTimingAdjust,
			kVDCaptureInfo_SyncRelativeLatency,
			kVDCaptureInfo_SyncCurrentLatency,
		};

		switch(type) {
		case kEventAttach:
			mpBase = pBase;

			{
				IVDUIList *pList = vdpoly_cast<IVDUIList *>(pBase->GetControl(100));
				IVDUIListView *pListView = vdpoly_cast<IVDUIListView *>(pList);
				if (pListView) {
					pListView->AddColumn(L"Item", 0, 1);

					for(int i=0; i<sizeof kItems / sizeof kItems[0]; ++i) {
						const wchar_t *itemLabel = VDLoadString(0, kVDST_CaptureUI, kVDMBase_CapInfoLabels + kItems[i]);
						int idx = pList->AddItem(itemLabel, kItems[i]);
						if (idx >= 0)
							pListView->SetItemChecked(idx, std::find(mPrefs.mInfoItems.begin(), mPrefs.mInfoItems.end(), kItems[i]) != mPrefs.mInfoItems.end());
					}
				}
			}

			pBase->ExecuteAllLinks();
			return true;
		case kEventSync:
		case kEventDetach:
			mPrefs.mInfoItems.clear();
			{
				IVDUIList *pList = vdpoly_cast<IVDUIList *>(pBase->GetControl(100));
				IVDUIListView *pListView = vdpoly_cast<IVDUIListView *>(pList);

				for(int i=0; i<sizeof kItems / sizeof kItems[0]; ++i) {
					if (pListView->IsItemChecked(i))
						mPrefs.mInfoItems.push_back(i);
				}
			}
			return true;
		}
		return false;
	}
};

class VDDialogCapturePreferencesHotKey : public VDDialogBase {
public:
	VDCapturePreferences& mPrefs;
	VDDialogCapturePreferencesHotKey(VDCapturePreferences& p) : mPrefs(p) {}

	bool HandleUIEvent(IVDUIBase *pBase, IVDUIWindow *pWin, uint32 id, eEventType type, int item) {
		switch(type) {
		case kEventAttach:
			mpBase = pBase;
			SetValue(100, mPrefs.mHotKeyStart);
			SetValue(101, mPrefs.mHotKeyStop);

			pBase->ExecuteAllLinks();
			return true;
		case kEventSync:
		case kEventDetach:
			mPrefs.mHotKeyStart = GetValue(100);
			mPrefs.mHotKeyStop = GetValue(101);
			return true;
		}
		return false;
	}
};

class VDDialogCapturePreferences : public VDDialogBase {
public:
	VDCapturePreferences& mPrefs;
	VDDialogCapturePreferences(VDCapturePreferences& p) : mPrefs(p) {}

	bool HandleUIEvent(IVDUIBase *pBase, IVDUIWindow *pWin, uint32 id, eEventType type, int item) {
		if (type == kEventAttach) {
			mpBase = pBase;
			SetValue(100, 0);
			pBase->ExecuteAllLinks();
		} else if (id == 101 && type == kEventSelect) {
			IVDUIBase *pSubDialog = vdpoly_cast<IVDUIBase *>(pBase->GetControl(101)->GetStartingChild());

			if (pSubDialog) {
				switch(item) {
				case 0:	pSubDialog->SetCallback(new VDDialogCapturePreferencesInfoPanel(mPrefs), true); break;
				case 1:	pSubDialog->SetCallback(new VDDialogCapturePreferencesHotKey(mPrefs), true); break;
				}
			}
		} else if (type == kEventSelect) {
			if (id == 10) {
				pBase->EndModal(true);
				return true;
			} else if (id == 11) {
				pBase->EndModal(false);
				return true;
			}
		}
		return false;
	}
};

bool VDShowCapturePreferencesDialog(VDGUIHandle h, VDCapturePreferences& prefs) {
	vdautoptr<IVDUIWindow> peer(VDUICreatePeer(h));

	IVDUIWindow *pWin = VDCreateDialogFromResource(2000, peer);
	VDCapturePreferences temp(prefs);
	VDDialogCapturePreferences prefDlg(temp);

	IVDUIBase *pBase = vdpoly_cast<IVDUIBase *>(pWin);
	
	pBase->SetCallback(&prefDlg, false);
	int result = pBase->DoModal();

	peer->Shutdown();

	if (result) {
		prefs = temp;
		return true;
	}

	return false;
}

void VDCaptureSavePreferences(const VDCapturePreferences& prefs) {
	VDRegistryAppKey key(g_szCapture);
	
	key.setBinary("Capture: Info panel items", prefs.mInfoItems.empty() ? "" : (const char *)&prefs.mInfoItems[0], prefs.mInfoItems.size() * 4);
	key.setInt("Capture: Start hotkey", prefs.mHotKeyStart);
	key.setInt("Capture: Stop hotkey", prefs.mHotKeyStop);
}

void VDCaptureLoadPreferences(VDCapturePreferences& prefs) {
	VDRegistryAppKey key(g_szCapture);

	int len = key.getBinaryLength("Capture: Info panel items") >> 2;
	if (len >= 0) {
		prefs.mInfoItems.resize(len);
		if (len)
			key.getBinary("Capture: Info panel items", (char *)&prefs.mInfoItems[0], len * 4);
	} else {
		static const uint32 kDefaultInfoItems[]={
			kVDCaptureInfo_FramesCaptured,
			kVDCaptureInfo_TotalTime,
			kVDCaptureInfo_TimeLeft,
			kVDCaptureInfo_TotalFileSize,
			kVDCaptureInfo_DiskSpaceFree,
			kVDCaptureInfo_CPUUsage,
			kVDCaptureInfo_VideoSize,
			kVDCaptureInfo_VideoAverageRate,
			kVDCaptureInfo_VideoDataRate,
			kVDCaptureInfo_VideoCompressionRatio,
			kVDCaptureInfo_VideoAverageFrameSize,
			kVDCaptureInfo_VideoFramesDropped,
			kVDCaptureInfo_VideoFramesInserted,
			kVDCaptureInfo_VideoResamplingFactor,
			kVDCaptureInfo_AudioSize,
			kVDCaptureInfo_AudioRelativeRate,
			kVDCaptureInfo_AudioDataRate,
			kVDCaptureInfo_AudioCompressionRatio,
			kVDCaptureInfo_AudioResamplingFactor,
			kVDCaptureInfo_SyncVideoTimingAdjust,
			kVDCaptureInfo_SyncCurrentLatency
		};

		prefs.mInfoItems.assign(kDefaultInfoItems, kDefaultInfoItems + sizeof kDefaultInfoItems / sizeof kDefaultInfoItems[0]);
	}

	prefs.mHotKeyStart = key.getInt("Capture: Start hotkey", 0);
	prefs.mHotKeyStop = key.getInt("Capture: Stop hotkey", 0);
}

//////////////////////////////////////////////////////////////////////////////
//
//	raw audio format
//
//////////////////////////////////////////////////////////////////////////////

class VDDialogCaptureRawAudioFormat : public VDDialogBase {
public:
	const std::list<vdstructex<VDWaveFormat> >& mFormats;
	int mSelected;

	VDDialogCaptureRawAudioFormat(const std::list<vdstructex<VDWaveFormat> >& formats, int sel) : mFormats(formats), mSelected(sel) {}

	bool HandleUIEvent(IVDUIBase *pBase, IVDUIWindow *pWin, uint32 id, eEventType type, int item) {
		if (type == kEventAttach) {
			mpBase = pBase;

			std::list<vdstructex<VDWaveFormat> >::const_iterator it(mFormats.begin()), itEnd(mFormats.end());

			IVDUIList *pList = vdpoly_cast<IVDUIList *>(mpBase->GetControl(100));
			for(; it!=itEnd; ++it) {
				const VDWaveFormat& wfex = **it;

				const unsigned samprate = wfex.mSamplingRate;
				const unsigned channels = wfex.mChannels;
				const unsigned depth	= wfex.mSampleBits;
				const unsigned kbps		= wfex.mDataRate / 125;
				const wchar_t *chstr	= wfex.mChannels == 2 ? L"stereo" : L"mono";
				const wchar_t *lineformat;

				if (wfex.mTag == VDWaveFormat::kTagPCM) {
					if (wfex.mChannels > 2)
						lineformat = L"PCM: %uHz, %uch, %[3]u-bit";
					else
						lineformat = L"PCM: %uHz, %[2]ls, %[3]u-bit";
				} else {
					if (wfex.mChannels > 2)
						lineformat = L"Compressed: %uHz, %uch, %[4]uKbps";
					else
						lineformat = L"Compressed: %uHz, %[2]ls, %[4]uKbps";
				}

				VDStringW buf(VDswprintf(lineformat, 5, &samprate, &channels, &chstr, &depth, &kbps));
				pList->AddItem(buf.c_str());
			}

			if ((unsigned)mSelected < mFormats.size())
				SetValue(100, mSelected);
			pBase->ExecuteAllLinks();
		} else if (type == kEventSelect) {
			if (id == 10) {
				pBase->EndModal(GetValue(100));
				return true;
			} else if (id == 11) {
				pBase->EndModal(-1);
				return true;
			}
		}
		return false;
	}
};

int VDShowCaptureRawAudioFormatDialog(VDGUIHandle h, const std::list<vdstructex<VDWaveFormat> >& formats, int sel) {
	vdautoptr<IVDUIWindow> peer(VDUICreatePeer(h));

	IVDUIWindow *pWin = VDCreateDialogFromResource(2100, peer);
	VDDialogCaptureRawAudioFormat prefDlg(formats, sel);

	IVDUIBase *pBase = vdpoly_cast<IVDUIBase *>(pWin);
	
	pBase->SetCallback(&prefDlg, false);
	int result = pBase->DoModal();
	peer->Shutdown();

	return result;
}

//////////////////////////////////////////////////////////////////////////////
//
//	timing
//
//////////////////////////////////////////////////////////////////////////////

class VDDialogTimingOptions : public VDDialogBase {
public:
	VDDialogTimingOptions(VDCaptureTimingSetup& timing) : mTimingSetup(timing) {}

	bool HandleUIEvent(IVDUIBase *pBase, IVDUIWindow *pWin, uint32 id, eEventType type, int item) {
		if (type == kEventAttach) {
			mpBase = pBase;

			SetValue(100, mTimingSetup.mbAllowEarlyDrops);
			SetValue(101, mTimingSetup.mbAllowLateInserts);
			SetValue(102, mTimingSetup.mbCorrectVideoTiming);
			SetValue(103, !mTimingSetup.mbResyncWithIntegratedAudio);
			SetValue(210, mTimingSetup.mbUseFixedAudioLatency);

			SetValue(200, mTimingSetup.mSyncMode);

			int v = mTimingSetup.mInsertLimit;
			SetCaption(300, VDswprintf(L"%d", 1, &v).c_str());

			v = mTimingSetup.mAutoAudioLatencyLimit;
			SetCaption(301, VDswprintf(L"%d", 1, &v).c_str());

			v = mTimingSetup.mAudioLatency;
			SetCaption(302, VDswprintf(L"%d", 1, &v).c_str());

			SetValue(104, mTimingSetup.mbUseAudioTimestamps);
			SetValue(105, mTimingSetup.mbDisableClockForPreview);
			SetValue(106, mTimingSetup.mbForceAudioRendererClock);
			SetValue(107, mTimingSetup.mbIgnoreVideoTimestamps);

			pBase->ExecuteAllLinks();
		} else if (type == kEventSelect) {
			if (id == 10) {
				mTimingSetup.mSyncMode = (VDCaptureTimingSetup::SyncMode)GetValue(200);
				mTimingSetup.mbAllowEarlyDrops = GetValue(100) != 0;
				mTimingSetup.mbAllowLateInserts = GetValue(101) != 0;
				mTimingSetup.mbCorrectVideoTiming = GetValue(102) != 0;
				mTimingSetup.mbResyncWithIntegratedAudio = !GetValue(103);
				mTimingSetup.mbUseFixedAudioLatency = GetValue(210) != 0;
				mTimingSetup.mbUseAudioTimestamps = GetValue(104) != 0;
				mTimingSetup.mbDisableClockForPreview = GetValue(105) != 0;
				mTimingSetup.mbForceAudioRendererClock = GetValue(106) != 0;
				mTimingSetup.mbIgnoreVideoTimestamps = GetValue(107) != 0;

				VDStringW s(GetCaption(300));

				int v;
				wchar_t dummy;
				
				if (1 != swscanf(s.c_str(), L" %d %c", &v, &dummy)) {
					if (mTimingSetup.mbAllowLateInserts) {
						MessageBeep(MB_ICONEXCLAMATION);
						SetFocus(vdpoly_cast<VDUIPeerW32 *>(pBase->GetControl(300))->GetHandleW32());
						return true;
					}
				} else
					mTimingSetup.mInsertLimit = v;

				s = GetCaption(302);

				if (1 != swscanf(s.c_str(), L" %d %c", &v, &dummy)) {
					if (mTimingSetup.mbUseFixedAudioLatency) {
						MessageBeep(MB_ICONEXCLAMATION);
						SetFocus(vdpoly_cast<VDUIPeerW32 *>(pBase->GetControl(302))->GetHandleW32());
						return true;
					}
				} else
					mTimingSetup.mAudioLatency = v;

				s = GetCaption(301);

				if (1 != swscanf(s.c_str(), L" %d %c", &v, &dummy)) {
					if (!mTimingSetup.mbUseFixedAudioLatency) {
						MessageBeep(MB_ICONEXCLAMATION);
						SetFocus(vdpoly_cast<VDUIPeerW32 *>(pBase->GetControl(301))->GetHandleW32());
						return true;
					}
				} else
					mTimingSetup.mAutoAudioLatencyLimit = v;

				mTimingSetup.mbUseLimitedAutoAudioLatency = !mTimingSetup.mbUseFixedAudioLatency && mTimingSetup.mAutoAudioLatencyLimit > 0;

				pBase->EndModal(true);
				return true;
			} else if (id == 11) {
				pBase->EndModal(false);
				return true;
			}
		}
		return false;
	}

	VDCaptureTimingSetup& mTimingSetup;
};

bool VDShowCaptureTimingDialog(VDGUIHandle h, VDCaptureTimingSetup& timing) {
	vdautoptr<IVDUIWindow> peer(VDUICreatePeer(h));

	IVDUIWindow *pWin = VDCreateDialogFromResource(2101, peer);
	VDDialogTimingOptions prefDlg(timing);

	IVDUIBase *pBase = vdpoly_cast<IVDUIBase *>(pWin);
	
	pBase->SetCallback(&prefDlg, false);
	bool result = 0 != pBase->DoModal();
	peer->Shutdown();

	return result;
}

//////////////////////////////////////////////////////////////////////////////
//
//	device options
//
//////////////////////////////////////////////////////////////////////////////

class VDDialogCaptureDeviceOptions : public VDDialogBase {
public:
	VDDialogCaptureDeviceOptions(uint32& opts) : mOptions(opts) {}

	bool HandleUIEvent(IVDUIBase *pBase, IVDUIWindow *pWin, uint32 id, eEventType type, int item) {
		if (type == kEventAttach) {
			mpBase = pBase;

			SetValue(105, 0 != (mOptions & kVDCapDevOptSaveCurrentDisplayMode));
			SetValue(106, 0 != (mOptions & kVDCapDevOptSwitchSourcesTogether));
			SetValue(107, 0 != (mOptions & kVDCapDevOptSlowOverlay));
			SetValue(108, 0 != (mOptions & kVDCapDevOptSlowPreview));

			pBase->ExecuteAllLinks();
		} else if (type == kEventSelect) {
			if (id == 10) {
				mOptions = 0;

				if (GetValue(105))	mOptions += kVDCapDevOptSaveCurrentDisplayMode;
				if (GetValue(106))	mOptions += kVDCapDevOptSwitchSourcesTogether;
				if (GetValue(107))	mOptions += kVDCapDevOptSlowOverlay;
				if (GetValue(108))	mOptions += kVDCapDevOptSlowPreview;

				pBase->EndModal(true);
				return true;
			} else if (id == 11) {
				pBase->EndModal(false);
				return true;
			}
		}
		return false;
	}

	uint32& mOptions;
};

bool VDShowCaptureDeviceOptionsDialog(VDGUIHandle h, uint32& opts) {
	vdautoptr<IVDUIWindow> peer(VDUICreatePeer(h));

	IVDUIWindow *pWin = VDCreateDialogFromResource(2102, peer);
	VDDialogCaptureDeviceOptions prefDlg(opts);

	IVDUIBase *pBase = vdpoly_cast<IVDUIBase *>(pWin);
	
	pBase->SetCallback(&prefDlg, false);
	bool result = 0 != pBase->DoModal();
	peer->Shutdown();

	return result;
}

//////////////////////////////////////////////////////////////////////////////
//
//	procamp
//
//////////////////////////////////////////////////////////////////////////////

class VDDialogCaptureVideoProcAmp : public IVDUICallback {
public:
	VDDialogCaptureVideoProcAmp(IVDCaptureProject *pProject) : mpProject(pProject) {}

	bool HandleUIEvent(IVDUIBase *pBase, IVDUIWindow *pWin, uint32 id, eEventType type, int item) {
		if (type == kEventAttach) {
			mpBase = pBase;

		} else if (type == kEventCreate) {
			vdrefptr<IVDUIWindow> child;
			VDUIParameters parms;

			vduirectinsets labelpad(0);

			labelpad.right = pBase->MapUnitsToPixels(vduisize(3, 0)).w;
			
			child = VDCreateUIGrid();
			child->SetAlignment(nsVDUI::kFill, nsVDUI::kFill);
			pWin->AddChild(child);

			IVDUIGrid *pGrid = vdpoly_cast<IVDUIGrid *>(child);

			static const wchar_t *const kPropertyNames[]={
				L"Brightness",
				L"Contrast",
				L"Hue",
				L"Saturation",
				L"Sharpness",
				L"Gamma",
				L"Color enable",
				L"White balance",
				L"Backlight compensation",
				L"Gain"
			};

			int propcount = 0;
			for(int prop = 0; prop < nsVDCapture::kPropCount; ++prop) {
				if (mpProject->IsPropertySupported(prop)) {
					sint32 minVal, maxVal, step, defaultVal;
					bool automatic, manual;

					mpProject->GetPropertyInfoInt(prop, minVal, maxVal, step, defaultVal, automatic, manual);

					if (!manual)
						continue;

					child = VDCreateUILabel();
					child->SetCaption(kPropertyNames[prop]);
					child->SetMinimumSize(pBase->MapUnitsToPixels(vduisize(0, 14)));
					child->SetPadding(labelpad);
					pGrid->AddChild(child, 0, propcount, 1, 1);
					child->Create(&parms);

					child = VDCreateUITrackbar();
					child->SetMinimumSize(pBase->MapUnitsToPixels(vduisize(100, 14)));
					child->SetID(100 + prop);
					pGrid->AddChild(child, 1, propcount, 1, 1);
					child->Create(&parms);

					IVDUITrackbar *track = vdpoly_cast<IVDUITrackbar *>(child);

					track->SetRange(minVal, maxVal);
					child->SetValue(mpProject->GetPropertyInt(prop, NULL));

					child = VDCreateUINumericLabel();
					child->SetMinimumSize(pBase->MapUnitsToPixels(vduisize(40, 14)));
					child->SetID(200 + prop);
					pGrid->AddChild(child, 2, propcount, 1, 1);
					child->Create(&parms);

					const uint8 bytecode1[] = {
						1, 1,
						(uint8)(((100 + prop) >> 0) & 255),
						(uint8)(((100 + prop) >> 8) & 255),
						(uint8)(((100 + prop) >> 16) & 255),
						(uint8)(((100 + prop) >> 24) & 255),
						3, 0,
						nsVDDitaBytecode::kBCE_GetValue, 0,
						nsVDDitaBytecode::kBCE_End,
					};

					pBase->Link(200 + prop, nsVDUI::kLinkTarget_Value, bytecode1, sizeof bytecode1);

					++propcount;
				}
			}

			if (propcount == 0) {
				child = VDCreateUILabel();
				child->SetCaption(L"No video level controls are available with this capture driver.");
				vduisize padding(pBase->MapUnitsToPixels(vduisize(40, 20)));
				child->SetMargins(vduirectinsets(padding.w, padding.h, padding.w, padding.h));
				pGrid->AddChild(child, 0, 0, 1, 1);
				child->Create(&parms);
			}
		} else if (type == kEventSelect) {
			if (id >= 100 && id < 200) {
				mpProject->SetPropertyInt(id - 100, item, false);
			}
		}
		return false;
	}

	IVDUIBase *mpBase;
	IVDCaptureProject *const mpProject;
};

void VDCreateDialogCaptureVideoProcAmp(IVDUIWindow *pParent, IVDCaptureProject *pProject) {
	IVDUIWindow *pWin = VDCreateUIBaseWindow();
	pWin->SetCaption(L"Video levels");
	VDDialogCaptureVideoProcAmp *procAmpDlg = new VDDialogCaptureVideoProcAmp(pProject);

	IVDUIBase *pBase = vdpoly_cast<IVDUIBase *>(pWin);
	
	pBase->SetCallback(procAmpDlg, true);
	pParent->AddChild(pWin);
	pBase->SetAutoDestroy(true);
	VDUIParameters parms;
	pWin->Create(&parms);
}
