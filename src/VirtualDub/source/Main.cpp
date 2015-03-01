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

#include <windows.h>
#include <commctrl.h>

#include "resource.h"
#include "prefs.h"
#include "oshelper.h"
#include "gui.h"
#include <vd2/system/cpuaccel.h>
#include <vd2/system/memory.h>
#include <vd2/system/filesys.h>
#include <vd2/system/registry.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/memory.h>
#include <vd2/system/w32assist.h>
#include <vd2/system/debug.h>
#include <vd2/system/cmdline.h>
#include <vd2/Dita/services.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "Dub.h"
#include "DubOutput.h"
#include "command.h"
#include "job.h"
#include "project.h"
#include "projectui.h"
#include "crash.h"
#include "capture.h"
#include "captureui.h"
#include "server.h"
#include "uiframe.h"
#include <vd2/system/strutil.h>

#include "InputFile.h"
#include "AVIOutputImages.h"
#include <vd2/system/error.h>

///////////////////////////////////////////////////////////////////////////

enum {
	kFileDialog_Config			= 'conf',
	kFileDialog_ImageDst		= 'imgd'
};

///////////////////////////////////////////////////////////////////////////

HINSTANCE	g_hInst;
HWND		g_hWnd =NULL;
int			g_returnCode;

bool				g_fDropFrames			= false;
bool				g_fSwapPanes			= false;
bool				g_bExit					= false;

VDProject *g_project;
extern vdrefptr<VDProjectUI> g_projectui;

vdrefptr<IVDCaptureProject> g_capProject;
vdrefptr<IVDCaptureProjectUI> g_capProjectUI;

wchar_t g_szInputAVIFile[MAX_PATH];
wchar_t g_szInputWAVFile[MAX_PATH];
wchar_t g_szFile[MAX_PATH];

char g_serverName[256];

extern const char g_szError[]="VirtualDub Error";
extern const char g_szWarning[]="VirtualDub Warning";
extern const wchar_t g_szWarningW[]=L"VirtualDub Warning";

static const char g_szRegKeyPersistence[]="Persistence";

///////////////////////////

extern bool Init(HINSTANCE hInstance, int nCmdShow, VDCommandLine& cmdLine);
extern void Deinit();

void OpenAVI(int index, bool extended_opt);
void SaveAVI(HWND, bool);
void SaveSegmentedAVI(HWND);
void SaveImageSeq(HWND);
void SaveConfiguration(HWND);

extern int VDProcessCommandLine(const VDCommandLine& cmdLine);

//
//  FUNCTION: WinMain(HANDLE, HANDLE, LPSTR, int)
//
//  PURPOSE: Entry point for the application.
//
//  COMMENTS:
//
//	This function initializes the application and processes the
//	message loop.
//

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR /*lpCmdLine*/, int nCmdShow )
{
	MSG msg;

	VDCommandLine cmdLine(GetCommandLineW());
	if (!Init(hInstance, nCmdShow, cmdLine))
		return 10;

    // Acquire and dispatch messages until a WM_QUIT message is received.

	PostThreadMessage(GetCurrentThreadId(), WM_NULL, 0, 0);

	bool bCommandLineProcessed = false;

	for(;;) {
		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				PostQuitMessage(msg.wParam);
				goto wm_quit_detected;
			}

			if (guiCheckDialogs(&msg))
				continue;

			if (VDUIFrame::TranslateAcceleratorMessage(msg))
				continue;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (!bCommandLineProcessed) {
			bCommandLineProcessed = true;
			int rc = VDProcessCommandLine(cmdLine);

			if (rc >= 0) {
				VDUIFrame::DestroyAll();
				msg.wParam = rc;
				break;
			}
		}

		if (!g_project->Tick() && !g_projectui->Tick() && !JobPollAutoRun()) {
			VDClearEvilCPUStates();		// clear evil CPU states set by Borland DLLs

			WaitMessage();
		}
	}
wm_quit_detected:

	if (g_capProjectUI) {
		g_capProjectUI->Detach();
		g_capProjectUI = NULL;
	}

	if (g_capProject) {
		g_capProject->Detach();
		g_capProject = NULL;
	}

	Deinit();

	VDCHECKPOINT;

    return g_returnCode ? g_returnCode : msg.wParam;           // Returns the value from PostQuitMessage.

}


void VDSwitchUIFrameMode(HWND hwnd, int nextMode) {
	if (g_capProjectUI) {
		g_capProjectUI->Detach();
		g_capProjectUI = NULL;
	}

	if (g_capProject) {
		g_capProject->Detach();
		g_capProject = NULL;
	}

	switch(nextMode) {
	case 1:
		g_capProject = VDCreateCaptureProject();
		if (g_capProject->Attach((VDGUIHandle)hwnd)) {
			g_capProjectUI = VDCreateCaptureProjectUI();
			if (g_capProjectUI->Attach((VDGUIHandle)hwnd, g_capProject)) {
				return;
			}
			g_capProjectUI = NULL;

			g_capProject->Detach();
			g_capProject = NULL;
		}
		break;

		// case 2 is the main project mode

	case 3:
		ActivateFrameServerDialog(hwnd, g_serverName);
		// fall through and reconnect main project when done
		break;
	}

	g_projectui->Attach((VDGUIHandle)hwnd);
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////






extern const wchar_t fileFilters0[]=
		L"Audio-Video Interleave (*.avi)\0"			L"*.avi\0"
		L"All files (*.*)\0"						L"*.*\0"
		;

extern const wchar_t fileFiltersAppend[]=
		L"VirtualDub/AVI_IO video segment (*.avi)\0"	L"*.avi\0"
		L"All files (*.*)\0"							L"*.*\0"
		;

static const wchar_t fileFiltersSaveConfig[]=
		L"VirtualDub script (*.vdscript)\0"			L"*.vdscript;*.vcf;*.syl\0"
		L"All files (*.*)\0"						L"*.*\0"
		;


  
void OpenAVI(bool ext_opt) {
	bool fExtendedOpen = false;
	bool fAutoscan = false;

	IVDInputDriver *pDriver = 0;

	std::vector<int> xlat;
	tVDInputDrivers inputDrivers;

	VDGetInputDrivers(inputDrivers, IVDInputDriver::kF_Video);

	VDStringW fileFilters(VDMakeInputDriverFileFilter(inputDrivers, xlat));

	static const VDFileDialogOption sOptions[]={
		{ VDFileDialogOption::kBool, 0, L"Ask for e&xtended options after this dialog", 0, 0 },
		{ VDFileDialogOption::kBool, 1, L"Automatically load linked segments", 0, 0 },
		{ VDFileDialogOption::kSelectedFilter, 2, 0, 0, 0 },
		{0}
	};

	int optVals[3]={0,1,0};

	VDStringW fname(VDGetLoadFileName(VDFSPECKEY_LOADVIDEOFILE, (VDGUIHandle)g_hWnd, L"Open video file", fileFilters.c_str(), NULL, sOptions, optVals));

	if (fname.empty())
		return;

	fExtendedOpen = !!optVals[0];
	fAutoscan = !!optVals[1];

	if (xlat[optVals[2]-1] >= 0)
		pDriver = inputDrivers[xlat[optVals[2]-1]];

	VDAutoLogDisplay logDisp;
	g_project->Open(fname.c_str(), pDriver, fExtendedOpen, false, fAutoscan);
	logDisp.Post((VDGUIHandle)g_hWnd);
}

////////////////////////////////////

void SaveAVI(HWND hWnd, bool fUseCompatibility, bool queueAsJob) {
	if (!inputVideo) {
		MessageBox(hWnd, "No input video stream to process.", g_szError, MB_OK);
		return;
	}

	VDStringW fname(VDGetSaveFileName(VDFSPECKEY_SAVEVIDEOFILE, (VDGUIHandle)hWnd, fUseCompatibility ? L"Save AVI 1.0 File" : L"Save AVI 2.0 File", fileFilters0, L"avi"));
	if (!fname.empty()) {
		g_project->SaveAVI(fname.c_str(), fUseCompatibility, queueAsJob);
	}
}

///////////////////////////////////////////////////////////////////////////

static const char g_szRegKeySegmentFrameCount[]="Segment frame limit";
static const char g_szRegKeyUseSegmentFrameCount[]="Use segment frame limit";
static const char g_szRegKeySegmentSizeLimit[]="Segment size limit";
static const char g_szRegKeySaveSelectionAndEditList[]="Save edit list";
static const char g_szRegKeySaveTextInfo[]="Save text info";
static const char g_szRegKeySegmentDigitCount[]="Segment digit count";
  
void SaveSegmentedAVI(HWND hWnd, bool queueAsJob) {
	if (!inputVideo) {
		MessageBox(hWnd, "No input video stream to process.", g_szError, MB_OK);
		return;
	}

	static const VDFileDialogOption sOptions[]={
		{ VDFileDialogOption::kEnabledInt, 1, L"&Limit number of video frames per segment:", 1, 0x7fffffff },
		{ VDFileDialogOption::kInt, 3, L"File segment &size limit in MB (50-2048):", 50, 2048 },
		{ VDFileDialogOption::kInt, 4, L"Minimum digit count (1-10):", 1, 10 },
		{0}
	};

	VDRegistryAppKey key(g_szRegKeyPersistence);
	int optVals[5]={
		0,
		key.getBool(g_szRegKeyUseSegmentFrameCount, false),
		key.getInt(g_szRegKeySegmentFrameCount, 100),
		key.getInt(g_szRegKeySegmentSizeLimit, 2000),
		key.getInt(g_szRegKeySegmentDigitCount, 2),
	};

	VDStringW fname(VDGetSaveFileName(VDFSPECKEY_SAVEVIDEOFILE, (VDGUIHandle)hWnd, L"Save segmented AVI", fileFiltersAppend, L"avi", sOptions, optVals));

	if (!fname.empty()) {
		key.setBool(g_szRegKeyUseSegmentFrameCount, !!optVals[1]);
		if (optVals[1])
			key.setInt(g_szRegKeySegmentFrameCount, optVals[2]);
		key.setInt(g_szRegKeySegmentSizeLimit, optVals[3]);

		int digits = optVals[4];

		if (digits < 1)
			digits = 1;

		if (digits > 10)
			digits = 10;

		key.setInt(g_szRegKeySegmentDigitCount, digits);

		char szFile[MAX_PATH];

		strcpy(szFile, VDTextWToA(fname).c_str());

		{
			char szPrefixBuffer[MAX_PATH], szPattern[MAX_PATH*2], *t, *t2, c;
			const char *s;
			int nMatchCount = 0;

			t = VDFileSplitPath(szFile);
			t2 = VDFileSplitExt(t);

			if (!_stricmp(t2, ".avi")) {
				while(t2>t && isdigit((unsigned)t2[-1]))
					--t2;

				if (t2>t && t2[-1]=='.')
					strcpy(t2, "avi");
			}

			strcpy(szPrefixBuffer, szFile);
			VDFileSplitExt(szPrefixBuffer)[0] = 0;

			s = VDFileSplitPath(szPrefixBuffer);
			t = szPattern;

			while(*t++ = *s++)
				if (s[-1]=='%')
					*t++ = '%';

			t = szPrefixBuffer;
			while(*t)
				++t;

			strcpy(t, ".*.avi");

			WIN32_FIND_DATA wfd;
			HANDLE h;

			h = FindFirstFile(szPrefixBuffer, &wfd);
			if (h != INVALID_HANDLE_VALUE) {
				strcat(szPattern, ".%d.av%c");

				do {
					int n;

					if (2 == sscanf(wfd.cFileName, szPattern, &n, &c) && tolower(c)=='i')
						++nMatchCount;
					
				} while(FindNextFile(h, &wfd));
				FindClose(h);
			}

			if (nMatchCount) {
				if (IDOK != guiMessageBoxF(g_hWnd, g_szWarning, MB_OKCANCEL|MB_ICONEXCLAMATION,
					"There %s %d existing file%s which match%s the filename pattern \"%s\". These files "
					"will be erased if you continue, to prevent confusion with the new files."
					,nMatchCount==1 ? "is" : "are"
					,nMatchCount
					,nMatchCount==1 ? "" : "s"
					,nMatchCount==1 ? "es" : ""
					,VDFileSplitPath(szPrefixBuffer)))
					return;

				h = FindFirstFile(szPrefixBuffer, &wfd);
				if (h != INVALID_HANDLE_VALUE) {
					strcat(szPattern, ".%d.av%c");

					t = VDFileSplitPath(szPrefixBuffer);

					do {
						int n;

						if (2 == sscanf(wfd.cFileName, szPattern, &n, &c) && tolower(c)=='i') {
							strcpy(t, wfd.cFileName);
							DeleteFile(t);
						}
							
						
					} while(FindNextFile(h, &wfd));
					FindClose(h);
				}
			}
		}

		if (queueAsJob) {
			JobAddConfiguration(&g_dubOpts, g_szInputAVIFile, NULL, fname.c_str(), true, &inputAVI->listFiles, optVals[3], optVals[1] ? optVals[2] : 0, true, digits);
		} else {
			SaveSegmentedAVI(fname.c_str(), false, NULL, optVals[3], optVals[1] ? optVals[2] : 0, digits);
		}
	}
}

/////////////////////////////

class VDSaveImageSeqDialogW32 : public VDDialogBaseW32 {
public:
	VDSaveImageSeqDialogW32();
	~VDSaveImageSeqDialogW32();

	INT_PTR DlgProc(UINT message, WPARAM wParam, LPARAM lParam);
	void UpdateFilenames();
	void UpdateEnables();
	void UpdateSlider();
	void ChangeExtension(const wchar_t *newExtension);

	VDStringW	mPrefix;
	VDStringW	mPostfix;
	VDStringW	mDirectory;
	VDStringW	mFormatString;

	int digits;
	sint64 mFirstFrame, mLastFrame;
	int mFormat;
	int mQuality;
	bool mbQuickCompress;
};

VDSaveImageSeqDialogW32::VDSaveImageSeqDialogW32()
	: VDDialogBaseW32(IDD_AVIOUTPUTIMAGES_FORMAT)
	, digits(0)
	, mFirstFrame(0)
	, mLastFrame(0)
	, mFormat(AVIOutputImages::kFormatBMP)
{
}
VDSaveImageSeqDialogW32::~VDSaveImageSeqDialogW32() {}

void VDSaveImageSeqDialogW32::UpdateFilenames() {
	mFormatString = VDMakePath(mDirectory.c_str(), mPrefix.c_str());
	
	VDStringW format(mFormatString + L"%0*lld" + mPostfix);

	VDSetWindowTextW32(GetDlgItem(mhdlg, IDC_STATIC_FIRSTFRAMENAME), VDswprintf(format.c_str(), 2, &digits, &mFirstFrame).c_str());
	VDSetWindowTextW32(GetDlgItem(mhdlg, IDC_STATIC_LASTFRAMENAME), VDswprintf(format.c_str(), 2, &digits, &mLastFrame).c_str());
}

void VDSaveImageSeqDialogW32::UpdateEnables() {
	bool bIsJPEG = 0!=IsDlgButtonChecked(mhdlg, IDC_FORMAT_JPEG);
	bool bIsPNG = 0!=IsDlgButtonChecked(mhdlg, IDC_FORMAT_PNG);

	EnableWindow(GetDlgItem(mhdlg, IDC_QUALITY), bIsJPEG);
	EnableWindow(GetDlgItem(mhdlg, IDC_STATIC_QUALITY), bIsJPEG);
	EnableWindow(GetDlgItem(mhdlg, IDC_QUICK), bIsPNG);
}

void VDSaveImageSeqDialogW32::UpdateSlider() {
	mQuality = SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_GETPOS, 0, 0);
	SetDlgItemInt(mhdlg, IDC_STATIC_QUALITY, mQuality, FALSE);
}

void VDSaveImageSeqDialogW32::ChangeExtension(const wchar_t *newExtension) {
	if (!wcscmp(mPostfix.c_str(), L".bmp") ||
		!wcscmp(mPostfix.c_str(), L".jpeg") ||
		!wcscmp(mPostfix.c_str(), L".jpg") ||
		!wcscmp(mPostfix.c_str(), L".tga") ||
		!wcscmp(mPostfix.c_str(), L".png")
		) {
		VDSetWindowTextW32(GetDlgItem(mhdlg, IDC_FILENAME_SUFFIX), newExtension);
	}
}

INT_PTR VDSaveImageSeqDialogW32::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
	UINT uiTemp;
	BOOL fSuccess;

	switch(message) {
	case WM_INITDIALOG:
		SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETRANGE, TRUE, MAKELONG(0,100));
		SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETPOS, TRUE, mQuality);
		VDSetWindowTextW32(GetDlgItem(mhdlg, IDC_FILENAME_PREFIX), mPrefix.c_str());
		VDSetWindowTextW32(GetDlgItem(mhdlg, IDC_FILENAME_SUFFIX), mPostfix.c_str());
		SetDlgItemInt(mhdlg, IDC_FILENAME_DIGITS, digits, FALSE);
		VDSetWindowTextW32(GetDlgItem(mhdlg, IDC_DIRECTORY), mDirectory.c_str());
		CheckDlgButton(mhdlg, mFormat == AVIOutputImages::kFormatTGA ? IDC_FORMAT_TGA
							: mFormat == AVIOutputImages::kFormatTGAUncompressed ? IDC_FORMAT_TGAUNCOMPRESSED
							: mFormat == AVIOutputImages::kFormatBMP ? IDC_FORMAT_BMP
							: mFormat == AVIOutputImages::kFormatJPEG ? IDC_FORMAT_JPEG
							: IDC_FORMAT_PNG
							, BST_CHECKED);
		CheckDlgButton(mhdlg, IDC_QUICK, mbQuickCompress ? BST_CHECKED : BST_UNCHECKED);
		UpdateFilenames();
		UpdateEnables();
		UpdateSlider();

		return TRUE;

	case WM_HSCROLL:
		UpdateSlider();
		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {

		case IDC_FILENAME_PREFIX:
			if (HIWORD(wParam) != EN_CHANGE) break;
			mPrefix = VDGetWindowTextW32((HWND)lParam);
			UpdateFilenames();
			return TRUE;

		case IDC_FILENAME_SUFFIX:
			if (HIWORD(wParam) != EN_CHANGE) break;
			mPostfix = VDGetWindowTextW32((HWND)lParam);
			UpdateFilenames();
			return TRUE;

		case IDC_FILENAME_DIGITS:
			if (HIWORD(wParam) != EN_CHANGE) break;
			uiTemp = GetDlgItemInt(mhdlg, IDC_FILENAME_DIGITS, &fSuccess, FALSE);
			if (fSuccess) {
				digits = uiTemp;

				if (digits > 15)
					digits = 15;

				UpdateFilenames();
			}
			return TRUE;

		case IDC_DIRECTORY:
			if (HIWORD(wParam) != EN_CHANGE) break;
			mDirectory = VDGetWindowTextW32((HWND)lParam);
			UpdateFilenames();
			return TRUE;

		case IDC_SELECT_DIR:
			{
				const VDStringW dir(VDGetDirectory(kFileDialog_ImageDst, (VDGUIHandle)mhdlg, L"Select a directory for saved images"));

				if (!dir.empty())
					VDSetWindowTextW32(GetDlgItem(mhdlg, IDC_DIRECTORY), dir.c_str());
			}
			return TRUE;


		// There is a distinct sense of non-scalability here

		case IDC_FORMAT_TGA:
			if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)) {
				UpdateEnables();
				mFormat = AVIOutputImages::kFormatTGA;
				ChangeExtension(L".tga");
			}
			return TRUE;

		case IDC_FORMAT_TGAUNCOMPRESSED:
			if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)) {
				UpdateEnables();
				mFormat = AVIOutputImages::kFormatTGAUncompressed;
				ChangeExtension(L".tga");
			}
			return TRUE;

		case IDC_FORMAT_BMP:
			if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)) {
				UpdateEnables();
				mFormat = AVIOutputImages::kFormatBMP;
				ChangeExtension(L".bmp");
			}
			return TRUE;

		case IDC_FORMAT_JPEG:
			if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)) {
				UpdateEnables();
				mFormat = AVIOutputImages::kFormatJPEG;
				ChangeExtension(L".jpeg");
			}
			return TRUE;

		case IDC_FORMAT_PNG:
			if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)) {
				UpdateEnables();
				mFormat = AVIOutputImages::kFormatPNG;
				ChangeExtension(L".png");
			}
			return TRUE;

		case IDC_QUICK:
			mbQuickCompress = !!SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
			return TRUE;

		case IDOK:
			End(TRUE);
			return TRUE;

		case IDCANCEL:
			End(FALSE);
			return TRUE;
		}
		break;
	}

	return FALSE;
}

static const char g_szRegKeyImageSequenceFormat[]="Image sequence: format";
static const char g_szRegKeyImageSequenceQuality[]="Image sequence: quality";
static const char g_szRegKeyImageSequenceDirectory[]="Image sequence: directory";
static const char g_szRegKeyImageSequencePrefix[]="Image sequence: prefix";
static const char g_szRegKeyImageSequenceSuffix[]="Image sequence: suffix";
static const char g_szRegKeyImageSequenceMinDigits[]="Image sequence: min digits";
static const char g_szRegKeyImageSequenceQuickCompress[]="Image sequence: quick compress";

void SaveImageSeq(HWND hwnd, bool queueAsJob) {
	VDSaveImageSeqDialogW32 dlg;

	if (!inputVideo) {
		MessageBox(hwnd, "No input video stream to process.", g_szError, MB_OK);
		return;
	}

	VDRegistryAppKey key(g_szRegKeyPersistence);

	dlg.mFormat = key.getInt(g_szRegKeyImageSequenceFormat, AVIOutputImages::kFormatTGA);
	if ((unsigned)dlg.mFormat >= AVIOutputImages::kFormatCount)
		dlg.mFormat = AVIOutputImages::kFormatTGA;
	dlg.mFirstFrame	= 0;
	dlg.mLastFrame	= g_project->GetFrameCount() - 1;
	dlg.mQuality	= key.getInt(g_szRegKeyImageSequenceQuality, 95);
	dlg.mbQuickCompress	= key.getBool(g_szRegKeyImageSequenceQuickCompress, true);
	dlg.digits		= key.getInt(g_szRegKeyImageSequenceMinDigits, 4);

	dlg.mPostfix = L".tga";

	key.getString(g_szRegKeyImageSequenceDirectory, dlg.mDirectory);
	key.getString(g_szRegKeyImageSequencePrefix, dlg.mPrefix);
	key.getString(g_szRegKeyImageSequenceSuffix, dlg.mPostfix);

	if (dlg.mQuality < 0)
		dlg.mQuality = 0;
	else if (dlg.mQuality > 100)
		dlg.mQuality = 100;

	if (dlg.ActivateDialogDual((VDGUIHandle)hwnd)) {
		key.setInt(g_szRegKeyImageSequenceFormat, dlg.mFormat);
		key.setInt(g_szRegKeyImageSequenceQuality, dlg.mQuality);
		key.setInt(g_szRegKeyImageSequenceMinDigits, dlg.digits);
		key.setString(g_szRegKeyImageSequenceDirectory, dlg.mDirectory.c_str());
		key.setString(g_szRegKeyImageSequencePrefix, dlg.mPrefix.c_str());
		key.setString(g_szRegKeyImageSequenceSuffix, dlg.mPostfix.c_str());
		key.setBool(g_szRegKeyImageSequenceQuickCompress, dlg.mbQuickCompress);

		int q = dlg.mQuality;

		if (dlg.mFormat == AVIOutputImages::kFormatPNG)
			q = dlg.mbQuickCompress ? 0 : 100;

		if (queueAsJob)
			JobAddConfigurationImages(&g_dubOpts, g_szInputAVIFile, NULL, dlg.mFormatString.c_str(), dlg.mPostfix.c_str(), dlg.digits, dlg.mFormat, q, &inputAVI->listFiles);
		else
			SaveImageSequence(dlg.mFormatString.c_str(), dlg.mPostfix.c_str(), dlg.digits, false, NULL, dlg.mFormat, q);
	}
}

/////////////////////////////////////////////////////////////////////////////////

void SaveConfiguration(HWND hWnd) {
	static const VDFileDialogOption sOptions[]={
		{ VDFileDialogOption::kBool, 0, L"Include selection and edit list", 0, 0 },
		{ VDFileDialogOption::kBool, 1, L"Include file text information strings", 0, 0 },
		{0}
	};

	VDRegistryAppKey key(g_szRegKeyPersistence);
	int optVals[2]={
		key.getBool(g_szRegKeySaveSelectionAndEditList, false),
		key.getBool(g_szRegKeySaveTextInfo, false),
	};

	const VDStringW filename(VDGetSaveFileName(kFileDialog_Config, (VDGUIHandle)hWnd, L"Save Configuration", fileFiltersSaveConfig, L"vdscript", sOptions, optVals));

	if (!filename.empty()) {
		key.setBool(g_szRegKeySaveSelectionAndEditList, !!optVals[0]);
		key.setBool(g_szRegKeySaveTextInfo, !!optVals[1]);

		try {
			JobWriteConfiguration(filename.c_str(), &g_dubOpts, !!optVals[0], !!optVals[1]);
		} catch(const MyError& e) {
			e.post(NULL, g_szError);
		}
	}
}
