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
#include <commdlg.h>
#include <dlgs.h>

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
#include <vd2/VDLib/Dialog.h>
#include "VideoSource.h"
#include "AudioSource.h"
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
#include "AVIOutputPlugin.h"
#include <vd2/system/error.h>
#include <vd2/plugin/vdinputdriver.h>

///////////////////////////////////////////////////////////////////////////

enum {
	kFileDialog_WAVAudioIn		= 'wavi',
	kFileDialog_WAVAudioOut		= 'wavo',
	kFileDialog_Config			= 'conf',
	kFileDialog_ImageDst		= 'imgd',
	kFileDialog_Project 		= 'proj'
};

///////////////////////////////////////////////////////////////////////////

HINSTANCE	g_hInst;
HWND		g_hWnd =NULL;
int			g_returnCode;

bool				g_fDropFrames			= false;
bool				g_fDropSeeking			= true;
bool				g_fSwapPanes			= false;
bool				g_bExit					= false;

VDProject *g_project;
extern vdrefptr<VDProjectUI> g_projectui;

vdrefptr<IVDCaptureProject> g_capProject;
vdrefptr<IVDCaptureProjectUI> g_capProjectUI;
extern vdrefptr<AudioSource>	inputAudio;
extern InputFileOptions	*g_pInputOpts;

wchar_t g_szInputAVIFile[MAX_PATH];
wchar_t g_szInputWAVFile[MAX_PATH];
wchar_t g_szFile[MAX_PATH];

char g_serverName[256];

extern const char g_szError[]="VirtualDub Error";
extern const char g_szWarning[]="VirtualDub Warning";
extern const wchar_t g_szWarningW[]=L"VirtualDub Warning";

static const char g_szRegKeyPersistence[]="Persistence";
static const char g_szRegKeyAutoAppendByName[]="Auto-append by name";

extern COMPVARS2 g_Vcompression;
extern void ChooseCompressor(HWND hwndParent, COMPVARS2 *lpCompVars);

///////////////////////////

extern bool Init(HINSTANCE hInstance, int nCmdShow, VDCommandLine& cmdLine);
extern void Deinit();

void SaveAVI(HWND, bool);
void SaveSegmentedAVI(HWND);
void SaveImageSeq(HWND);
void SaveConfiguration(HWND);
void SaveProject(HWND, bool reset_path);

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

			if(g_projectui && g_projectui->edit_token){
				g_projectui->StopFilters();
				g_projectui->SetVideoFiltersAsk();
				g_projectui->edit_token = 0;
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

extern const wchar_t fileFiltersAppendAVI[]=
		L"VirtualDub/AVI_IO video segment (*.avi)\0"	L"*.avi\0"
		L"All files (*.*)\0"							L"*.*\0"
		;

extern const wchar_t fileFiltersAppendAll[]=
		L"All files (*.*)\0"							L"*.*\0"
		;

static const wchar_t fileFiltersSaveConfig[]=
		L"VirtualDub script (*.vdscript)\0"			L"*.vdscript;*.vcf;*.syl\0"
		L"All files (*.*)\0"						L"*.*\0"
		;

static const wchar_t fileFiltersSaveProject[]=
		L"VirtualDub project (*.vdproject)\0"		L"*.vdproject\0"
		L"All files (*.*)\0"						L"*.*\0"
		;

class VDOpenVideoDialogW32 {
public:
	VDOpenVideoDialogW32() {
		nFilterIndex = 0;
		select_mode = 1;
		append_mode = false;
		audio_mode = false;
		is_auto = false;
	}

	INT_PTR DlgProc(UINT message, WPARAM wParam, LPARAM lParam);
	void ChangeFilename();
	bool UpdateFilename();
	void ChangeDriver();
	void SetOptions(InputFileOptions* opt);
	void ChangeOptions();
	void ChangeSelection();
	void ChangeInfo();
	void ShowFileInfo();
	void ForceUseDriver(int i);
	bool FileOk();
	void InitSelectMode();

	HWND mhdlg;
	vdrefptr<IVDInputDriver> driver;
	VDString driver_options;
	VDStringW filename;
	VDStringW init_driver;
	VDString format_id;
	VDString last_override_format;
	VDStringW last_override_driver;
	VDXMediaInfo info;
	tVDInputDrivers inputDrivers;
	tVDInputDrivers detectList;
	std::vector<int> xlat;
	int nFilterIndex;
	int select_mode;
	bool append_mode;
	bool audio_mode;
	bool is_auto;
};

void VDOpenVideoDialogW32::ChangeFilename() {
	VDStringW opt_driver;
	if (driver) opt_driver = driver->GetSignatureName();
	detectList.clear();
	format_id.clear();
	int x = nFilterIndex ? xlat[nFilterIndex-1] : -1;
	if (append_mode) {
		// just keep existing driver
	} else if (x==-1) {
		driver = 0;
		try {
			int d0 = VDAutoselectInputDriverForFile(filename.c_str(), audio_mode ? IVDInputDriver::kF_Audio : IVDInputDriver::kF_Video, detectList);
			if (d0!=-1) {
				driver = detectList[d0];
				detectList.erase(detectList.begin()+d0);
				detectList.insert(detectList.begin(),driver);
				is_auto = true;

				VDStringW force_driver;
				VDXMediaInfo info;
				wcsncpy(info.format_name,driver->GetFilenamePattern(),100);
				VDTestInputDriverForFile(info,filename.c_str(),driver);
				format_id = VDTextWToA(info.format_name);

				if (!init_driver.empty()) {
					force_driver = init_driver;
					opt_driver = init_driver;
				} else if(!format_id.empty()) {
					VDRegistryAppKey key(audio_mode ? "File formats (audio)" : "File formats");
					key.getString(format_id.c_str(), force_driver);
				}
				if (format_id==last_override_format) {
					force_driver = last_override_driver;
				} else {
					last_override_format = format_id;
					last_override_driver = force_driver;
				}

				if (!force_driver.empty()) {
					tVDInputDrivers::const_iterator it(detectList.begin()), itEnd(detectList.end());
					for(int i=0; it!=itEnd; ++it,i++) {
						IVDInputDriver *pDriver = *it;
						if (pDriver->GetSignatureName()==force_driver) {
							if (driver!=pDriver) {
								driver = pDriver;
								is_auto = false;
							}
							break;
						}
					}
				}

			}
		} catch (const MyError&) {
		}
	} else {
		driver = inputDrivers[x];
		is_auto = false;
	}

	init_driver.clear();
	VDStringW opt_driver2;
	if (driver) opt_driver2 = driver->GetSignatureName();
	if (opt_driver2!=opt_driver)
		driver_options.clear();

	HWND w1 = GetDlgItem(mhdlg,IDC_DRIVER);
	SendMessage(w1,CB_RESETCONTENT,0,0);
	if (driver) {
		int select = 0;
		if (detectList.empty())
			SendMessageW(w1,CB_ADDSTRING,0,(LPARAM)driver->GetSignatureName());

		tVDInputDrivers::const_iterator it(detectList.begin()), itEnd(detectList.end());
		for(int i=0; it!=itEnd; ++it, i++) {
			IVDInputDriver *pDriver = *it;
			if (pDriver==driver) select = i;
			SendMessageW(w1,CB_ADDSTRING,0,(LPARAM)pDriver->GetSignatureName());
		}

		SendMessage(w1,CB_SETCURSEL,select,0);
	}
	EnableWindow(w1,detectList.size()>1);
	ChangeDriver();
	ChangeSelection();
}

void VDOpenVideoDialogW32::ChangeDriver() {
	if (driver_options.empty())
		SetDlgItemTextW(mhdlg,IDC_DRIVER_OPTIONS,L"Options...");
	else
		SetDlgItemTextW(mhdlg,IDC_DRIVER_OPTIONS,L"Options (+)");

	bool extOpen = (driver && (driver->GetFlags() & IVDInputDriver::kF_SupportsOpts));
	EnableWindow(GetDlgItem(mhdlg,IDC_DRIVER_OPTIONS), extOpen && !filename.empty() && !append_mode);
	EnableWindow(GetDlgItem(mhdlg,IDC_DRIVER_INFO), driver && !filename.empty());
	SetDlgItemText(mhdlg,IDC_INFO_MSG,0);

	if (driver && !filename.empty()) try {
		VDXMediaInfo info;
		wcsncpy(info.format_name,driver->GetFilenamePattern(),100);
		IVDInputDriver::DetectionConfidence result = VDTestInputDriverForFile(info,filename.c_str(),driver);
		this->info = info;
		if (result==IVDInputDriver::kDC_None)
			SetDlgItemText(mhdlg,IDC_INFO_MSG,"Not detected");
		else
			ChangeInfo();
	} catch (const MyError&) {
	}
}

void VDOpenVideoDialogW32::ChangeInfo() {
	VDStringW msg;
	int d=0;
	if (info.format_name[0]) {
		msg += info.format_name;
		d = 1;
	}
	if (info.vcodec_name[0]) {
		if (d==1) msg += L" - ";
		msg += info.vcodec_name;
		d = 1;
	}

	if (info.width && info.height) {
		if (d==1) msg += L" ";
		msg.append_sprintf(L"%d x %d",info.width,info.height);
		if (info.pixmapFormat) {
			msg += L", ";
			msg += VDTextAToW(VDPixmapFormatPrintSpec(info.pixmapFormat));
		}
	}
	SetDlgItemTextW(mhdlg,IDC_INFO_MSG,msg.c_str());
}

void VDOpenVideoDialogW32::ForceUseDriver(int i) {
	if (!format_id.empty()) {
		last_override_format = format_id;
		last_override_driver.clear();
		if (i>0) last_override_driver = detectList[i]->GetSignatureName();
	}

	driver_options.clear();
	driver = detectList[i];
	ChangeDriver();
}

void VDOpenVideoDialogW32::SetOptions(InputFileOptions* opt) {
	int len = opt->write(0,0);
	driver_options.resize(len);
	opt->write(&driver_options[0],len);
}

void VDOpenVideoDialogW32::ChangeOptions() {
	if (!driver) return;
	vdrefptr<InputFile> inputAVI;
	inputAVI = driver->CreateInputFile(0);
	InputFileOptions* opt = inputAVI->promptForOptions((VDGUIHandle)mhdlg);
	if (opt) {
		SetOptions(opt);
		delete opt;
		SetDlgItemTextW(mhdlg,IDC_DRIVER_OPTIONS,L"Options (+)");
	}
}

bool VDOpenVideoDialogW32::UpdateFilename() {
	VDStringW s = OpenSave_GetFileName(mhdlg);
	if (s.length()==0) return false;
	if (s!=filename) {
		filename = s;
		ChangeFilename();
		return true;
	}
	return false;
}

bool VDOpenVideoDialogW32::FileOk() {
	if (!driver) return false;
	if ((driver->GetFlags() & IVDInputDriver::kF_PromptForOpts) && driver_options.empty()) {
		ChangeOptions();
		if (driver_options.empty()) return false;
	}
	return true;
}

void VDOpenVideoDialogW32::ShowFileInfo() {
	if (!driver) return;
	vdrefptr<InputFile> inputAVI;
	inputAVI = driver->CreateInputFile(0);
	if (!driver_options.empty()) {
		InputFileOptions* opt = inputAVI->createOptions(driver_options.c_str(), driver_options.length());
		inputAVI->setOptions(opt);
		delete opt;
	}
	try {
		inputAVI->Init(filename.c_str());
		IVDVideoSource* vs=0;
		AudioSource* as=0;
		inputAVI->GetVideoSource(0,&vs);
		inputAVI->GetAudioSource(0,&as);

		if (vs) {
			vs->setTargetFormat(0);
			VDAVIBitmapInfoHeader* f = vs->getImageFormat();
			info.width = f->biWidth;
			info.height = f->biHeight;

			const VDPixmap& fmt = vs->getTargetFormat();
			if (fmt.format) info.pixmapFormat = fmt.format;
			ChangeInfo();
			inputAVI->InfoDialog((VDGUIHandle)mhdlg);
		} else {
			wcscpy(info.vcodec_name,L"no video");
			ChangeInfo();
			if (audio_mode) inputAVI->InfoDialog((VDGUIHandle)mhdlg);
		}

		if(vs) vs->Release();
		if(as) as->Release();
	} catch (const MyError& e){
		SetDlgItemTextA(mhdlg,IDC_INFO_MSG,e.gets());
	}
}

void VDOpenVideoDialogW32::InitSelectMode() {
	CheckDlgButton(mhdlg,IDC_OPEN_SINGLE, select_mode==0 ? BST_CHECKED:BST_UNCHECKED);
	CheckDlgButton(mhdlg,IDC_OPEN_SEGMENTS, select_mode==1 ? BST_CHECKED:BST_UNCHECKED);
	CheckDlgButton(mhdlg,IDC_OPEN_SEQUENCE, select_mode==2 ? BST_CHECKED:BST_UNCHECKED);
}

void VDOpenVideoDialogW32::ChangeSelection() {
	VDStringW msg(L"Sequence");
	if (select_mode==2 && !filename.empty()) {
		int count = AppendAVIAutoscanEnum(filename.c_str());
		if (count%10==1)
			msg.append_sprintf(L": %d file", count);
		else
			msg.append_sprintf(L": %d files", count);
	}
	SetDlgItemTextW(mhdlg,IDC_OPEN_SEQUENCE, msg.c_str());
}

INT_PTR VDOpenVideoDialogW32::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch(message) {
	case WM_INITDIALOG:
		EnableWindow(GetDlgItem(mhdlg,IDC_DRIVER_OPTIONS),false);
		EnableWindow(GetDlgItem(mhdlg,IDC_DRIVER_INFO),false);
		EnableWindow(GetDlgItem(mhdlg,IDC_OPEN_SEGMENTS),!append_mode && !audio_mode);
		EnableWindow(GetDlgItem(mhdlg,IDC_OPEN_SINGLE),!audio_mode);
		EnableWindow(GetDlgItem(mhdlg,IDC_OPEN_SEQUENCE),!audio_mode);
		InitSelectMode();
		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDC_DRIVER_OPTIONS:
			ChangeOptions();
			return TRUE;
		case IDC_DRIVER_INFO:
			ShowFileInfo();
			return TRUE;
		case IDC_DRIVER:
			if(HIWORD(wParam) == CBN_SELCHANGE){
				int i = SendMessage(GetDlgItem(mhdlg,IDC_DRIVER),CB_GETCURSEL,0,0);
				ForceUseDriver(i);
			}
			return TRUE;
		case IDC_OPEN_SINGLE:
			if (IsDlgButtonChecked(mhdlg,IDC_OPEN_SINGLE)) select_mode = 0;
			ChangeSelection();
			if(HIWORD(wParam) == BN_DBLCLK)
				PostMessage(GetParent(mhdlg),WM_COMMAND,IDOK,0);
			return TRUE;
		case IDC_OPEN_SEGMENTS:
			if (IsDlgButtonChecked(mhdlg,IDC_OPEN_SEGMENTS)) select_mode = 1;
			ChangeSelection();
			if(HIWORD(wParam) == BN_DBLCLK)
				PostMessage(GetParent(mhdlg),WM_COMMAND,IDOK,0);
			return TRUE;
		case IDC_OPEN_SEQUENCE:
			if (IsDlgButtonChecked(mhdlg,IDC_OPEN_SEQUENCE)) select_mode = 2;
			ChangeSelection();
			if(HIWORD(wParam) == BN_DBLCLK)
				PostMessage(GetParent(mhdlg),WM_COMMAND,IDOK,0);
			return TRUE;
		}
		break;
	}

	return FALSE;
}

UINT_PTR CALLBACK OpenVideoProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg){
	case WM_INITDIALOG:
		{
			OPENFILENAMEW* fn = (OPENFILENAMEW*)lParam;
			VDOpenVideoDialogW32* dlg = (VDOpenVideoDialogW32*)fn->lCustData;
			SetWindowLongPtr(hdlg, DWLP_USER, (LONG_PTR)dlg);
			dlg->mhdlg = hdlg;
			dlg->filename = fn->lpstrFile;
			dlg->DlgProc(msg,wParam,lParam);
			dlg->ChangeFilename();
			if (!VDIsAtLeastVistaW32()) SetTimer(hdlg,2,0,0);
			SetTimer(hdlg,1,200,0);
			return TRUE;
		}

	case WM_SIZE:
	case WM_COMMAND:
		{
			VDOpenVideoDialogW32* dlg = (VDOpenVideoDialogW32*)GetWindowLongPtr(hdlg, DWLP_USER);
			dlg->DlgProc(msg,wParam,lParam);
			return TRUE;
		}

	case WM_TIMER:
		if (wParam==2) {
			// workaround fow winxp crappy dialog size
			KillTimer(hdlg,1);
			RECT r0;
			RECT r1;
			GetWindowRect(GetParent(hdlg),&r0);
			GetWindowRect(hdlg,&r1);
			if((r0.right-r0.left)<(r1.right-r1.left)){
				SetWindowPos(GetParent(hdlg),0,0,0,r1.right-r1.left+16,r0.bottom-r0.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
			}
			return TRUE;
		}
		if (wParam==1) {
			VDOpenVideoDialogW32* dlg = (VDOpenVideoDialogW32*)GetWindowLongPtr(hdlg, DWLP_USER);
			dlg->UpdateFilename();
			return TRUE;
		}

	case WM_NOTIFY:
		VDOpenVideoDialogW32* dlg = (VDOpenVideoDialogW32*)GetWindowLongPtr(hdlg, DWLP_USER);
		OFNOTIFY* data = (OFNOTIFY*)lParam;
		if(data->hdr.code==CDN_SELCHANGE){
			dlg->UpdateFilename();
		}
		if(data->hdr.code==CDN_TYPECHANGE){
			dlg->nFilterIndex = data->lpOFN->nFilterIndex;
			dlg->ChangeFilename();
		}
		if(data->hdr.code==CDN_FILEOK){
			dlg->UpdateFilename();
			if (dlg->FileOk()) {
				return 0;
			} else {
				SetWindowLong(hdlg,DWLP_MSGRESULT,1);
				return 1;
			}
		}
		break;
	}

	return FALSE;
}

void OpenInput(bool append, bool audio) {
	VDOpenVideoDialogW32 dlg;
	VDGetInputDrivers(dlg.inputDrivers, audio ? IVDInputDriver::kF_Audio : IVDInputDriver::kF_Video);
	VDStringW fileFilters(VDMakeInputDriverFileFilter(dlg.inputDrivers, dlg.xlat));

	OPENFILENAMEW fn = {sizeof(fn),0};
	fn.Flags = OFN_ENABLETEMPLATE | OFN_ENABLEHOOK;
	fn.hInstance = GetModuleHandle(0);
	fn.lpTemplateName = MAKEINTRESOURCEW(IDD_OPENVIDEO);
	fn.lpfnHook = OpenVideoProc;
	fn.lCustData = (LONG_PTR)&dlg;

	if (audio) {
		fn.lpstrFile = g_szInputWAVFile;
		dlg.init_driver = g_project->mAudioInputDriverName;
		if (g_project->mpAudioInputOptions)
			dlg.SetOptions(g_project->mpAudioInputOptions);

	} else if (inputAVI && g_szInputAVIFile[0]) {
		fn.lpstrFile = g_szInputAVIFile;
		dlg.init_driver = g_project->mInputDriverName;
		if (g_pInputOpts)
			dlg.SetOptions(g_pInputOpts);
	}

	const wchar_t* title = L"Open video file";
	int fskey = VDFSPECKEY_LOADVIDEOFILE;
	
	VDRegistryAppKey key(g_szRegKeyPersistence);
	if (audio) {
		dlg.select_mode = 0;
		dlg.audio_mode = true;
		title = L"Open audio file";
		fskey = kFileDialog_WAVAudioIn;
	} else if (append) {
		dlg.select_mode = key.getBool(g_szRegKeyAutoAppendByName, true) ? 2:0;
		dlg.append_mode = true;
		dlg.driver = VDGetInputDriverByName(g_inputDriver.c_str());
		title = L"Append video segment";
	}

	VDStringW fname(VDGetLoadFileName(fskey, (VDGUIHandle)g_hWnd, title, fileFilters.c_str(), NULL, 0, 0, &fn));

	if (fname.empty())
		return;

	// remember override on confirmed open
	if (!dlg.format_id.empty()) {
		VDRegistryAppKey key(audio ? "File formats (audio)" : "File formats");
		if (dlg.driver==dlg.detectList[0])
			key.removeValue(dlg.format_id.c_str());
		else
			key.setString(dlg.format_id.c_str(), dlg.driver->GetSignatureName());
	}

	const char* opt = 0;
	int opt_len = 0;
	if (!dlg.driver_options.empty()) {
		opt = dlg.driver_options.c_str();
		opt_len = dlg.driver_options.length();
	}

	VDAutoLogDisplay logDisp;

	if (audio) {
		g_project->OpenWAV(fname.c_str(), dlg.driver, false, false, opt, opt_len);
		if (dlg.is_auto && !opt) g_project->mAudioInputDriverName.clear();
	} else if (append) {
		key.setBool(g_szRegKeyAutoAppendByName, dlg.select_mode==2);
		if (dlg.select_mode==2)
			AppendAVIAutoscan(fname.c_str());
		else
			AppendAVI(fname.c_str());
	} else {
		g_project->Open(fname.c_str(), dlg.driver, false, false, dlg.select_mode, opt, opt_len);
		if (dlg.is_auto && !opt) g_project->mInputDriverName.clear();
	}

	logDisp.Post((VDGUIHandle)g_hWnd);
}

////////////////////////////////////

class VDSaveVideoDialogW32 {
public:
	VDSaveVideoDialogW32(){ removeAudio=false; saveVideo=true; dubber=0; }
	~VDSaveVideoDialogW32(){ delete dubber; }

	INT_PTR DlgProc(UINT message, WPARAM wParam, LPARAM lParam);
	void OnSize();
	void InitCodec();
	void InitDubber();
	bool CheckAudioCodec(const char* format);
	virtual bool Commit(const VDStringW& fname){ return true; }
	virtual void ChangeFilterIndex(){}

	HWND mhdlg;
	int align_now;
	int align_job;
	AudioSource* inputAudio;
	IDubber* dubber;
	bool removeAudio;
	bool addJob;
	bool saveVideo;
	int nFilterIndex;
	VDStringW os_driver;
	VDStringA os_format;
};

void VDSaveVideoDialogW32::InitCodec() {
	if (saveVideo) {
		VDStringW name;
		VDPixmapFormatEx format = g_dubOpts.video.mOutputFormat;
		if (g_dubOpts.video.mode <= DubVideoOptions::M_FASTREPACK) format = 0;
		if (g_Vcompression.driver) {
			ICINFO ici = { sizeof(ICINFO) };
			if (g_Vcompression.driver->getInfo(ici)) {
				name = ici.szDescription;
			}
			int codec_format = g_Vcompression.driver->queryInputFormat(0);
			if (codec_format) format.format = codec_format;
		} else {
			name = VDStringW(L"(Uncompressed RGB/YCbCr)");
		}
		if (g_dubOpts.video.mode==DubVideoOptions::M_NONE) {
			name = VDStringW(L"(Stream copy)");
			EnableWindow(GetDlgItem(mhdlg,IDC_COMPRESSION_CHANGE),false);
		}

		SetDlgItemTextW(mhdlg,IDC_COMPRESSION,name.c_str());

		MakeOutputFormat make;
		make.initGlobal();
		make.initComp(&g_Vcompression);
		make.os_format = os_format;
		make.option = format;
		make.combine();
		make.combineComp();

		VDString s;
		if (!make.error.empty())
			s += "Format not accepted";
		else if (make.mode == DubVideoOptions::M_FASTREPACK)
			s += "autodetect";
		else
			s += VDPixmapFormatPrintSpec(make.out);

		SetDlgItemText(mhdlg,IDC_COMPRESSION2,s.c_str());
	}

	if (inputAudio) {
		CheckDlgButton(mhdlg,IDC_ENABLE_AUDIO,removeAudio ? BST_UNCHECKED:BST_CHECKED);
		EnableWindow(GetDlgItem(mhdlg,IDC_SAVE_AUDIO),true);
		EnableWindow(GetDlgItem(mhdlg,IDC_ENABLE_AUDIO),true);
	} else {
		CheckDlgButton(mhdlg,IDC_ENABLE_AUDIO,BST_UNCHECKED);
		EnableWindow(GetDlgItem(mhdlg,IDC_SAVE_AUDIO),false);
		EnableWindow(GetDlgItem(mhdlg,IDC_ENABLE_AUDIO),false);
	}

	if (inputAudio && !removeAudio) {
		EnableWindow(GetDlgItem(mhdlg,IDC_AUDIO_COMPRESSION),true);
		EnableWindow(GetDlgItem(mhdlg,IDC_AUDIO_INFO),true);
	} else {
		EnableWindow(GetDlgItem(mhdlg,IDC_AUDIO_COMPRESSION),false);
		EnableWindow(GetDlgItem(mhdlg,IDC_AUDIO_INFO),false);
	}

	VDStringW aname = VDStringW(L"(None)");
	if (inputAudio) {
		if (g_ACompressionFormat) {
			aname = VDTextU8ToW(g_ACompressionFormatHint);
			IVDAudioEnc* driver = (IVDAudioEnc*)VDGetAudioEncByName(g_ACompressionFormatHint.c_str());
			if (driver) aname = driver->GetName();
		} else {
			aname = VDStringW(L"No compression (PCM)");
		}
		if (g_dubOpts.audio.mode==DubVideoOptions::M_NONE) {
			const VDWaveFormat* fmt = inputAudio->getWaveFormat();
			if (is_audio_pcm(fmt) || is_audio_float(fmt))
				aname = VDStringW(L"No compression (PCM)");
			else
				aname = VDStringW(L"(Stream copy)");
			EnableWindow(GetDlgItem(mhdlg,IDC_COMPRESSION_CHANGE2),false);
		}
	}

	SetDlgItemTextW(mhdlg,IDC_AUDIO_COMPRESSION,aname.c_str());
}

void VDSaveVideoDialogW32::InitDubber() {
	if (!inputAudio) return;

	dubber = CreateDubber(&g_dubOpts);
	try {
		if (g_dubOpts.audio.bUseAudioFilterGraph)
			dubber->SetAudioFilterGraph(g_audioFilterGraph);
		//if (g_ACompressionFormat)
		//	dubber->SetAudioCompression((const VDWaveFormat *)g_ACompressionFormat, g_ACompressionFormatSize, g_ACompressionFormatHint.c_str(), g_ACompressionConfig);
		AudioSource* asrc = inputAudio;
		dubber->InitAudio(&asrc,1);
		AudioStream* as = dubber->GetAudioBeforeCompressor();
		VDWaveFormat* fmt = as->GetFormat();
		VDString s;
		if (is_audio_float(fmt))
			s.sprintf("%d Hz float %d ch", fmt->mSamplingRate, fmt->mChannels);
		else
			s.sprintf("%d Hz %d-bit %d ch", fmt->mSamplingRate, fmt->mSampleBits, fmt->mChannels);
		SetDlgItemText(mhdlg,IDC_AUDIO_INFO,s.c_str());

	} catch(const MyError& e) {
		SetDlgItemText(mhdlg,IDC_AUDIO_INFO,e.c_str());
		removeAudio = true;
		inputAudio = 0;
	}
}

bool VDSaveVideoDialogW32::CheckAudioCodec(const char* format) {
	if (removeAudio) return true;
	//dubber->CheckAudioCodec(format);
	return true;
}

void VDSaveVideoDialogW32::OnSize() {
	RECT r0,r1;
	GetClientRect(mhdlg,&r0);
	GetWindowRect(GetDlgItem(mhdlg,IDC_SAVE_TEST),&r1);
	MapWindowPoints(0,mhdlg,(POINT*)&r1,2);
	SetWindowPos(GetDlgItem(mhdlg,IDC_SAVE_DONOW),   NULL, r0.right+align_now, r1.top, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS);
	SetWindowPos(GetDlgItem(mhdlg,IDC_SAVE_MAKEJOB), NULL, r0.right+align_job, r1.top, 0, 0, SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS);
}

INT_PTR VDSaveVideoDialogW32::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch(message) {
	case WM_INITDIALOG:
		{
			RECT r0,r1,r2;
			GetWindowRect(mhdlg,&r0);
			GetWindowRect(GetDlgItem(mhdlg,IDC_SAVE_DONOW),&r1);
			GetWindowRect(GetDlgItem(mhdlg,IDC_SAVE_MAKEJOB),&r2);
			align_now = r1.left-r0.right;
			align_job = r2.left-r0.right;
		}
		ShowWindow(GetDlgItem(mhdlg,IDC_SAVE_TEST),SW_HIDE);
		CheckDlgButton(mhdlg,IDC_SAVE_DONOW, addJob ? BST_UNCHECKED:BST_CHECKED);
		CheckDlgButton(mhdlg,IDC_SAVE_MAKEJOB, addJob ? BST_CHECKED:BST_UNCHECKED);
		SetDlgItemText(mhdlg,IDC_AUDIO_INFO,"");

		InitDubber();
		InitCodec();
		return TRUE;

	case WM_SIZE:
		OnSize();
		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDC_COMPRESSION_CHANGE:
			{
				ChooseCompressor(mhdlg,&g_Vcompression);
				InitCodec();
			}
			break;
		case IDC_COMPRESSION_CHANGE2:
			{
				g_projectui->SetAudioCompressionAsk(mhdlg);
				InitCodec();
			}
			break;
		case IDC_ENABLE_AUDIO:
			removeAudio = !SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
			InitCodec();
			break;
		case IDC_SAVE_DONOW:
		case IDC_SAVE_MAKEJOB:
			addJob = IsDlgButtonChecked(mhdlg,IDC_SAVE_MAKEJOB)!=0;
			break;
		}
		break;
	}

	return FALSE;
}

UINT_PTR CALLBACK SaveVideoProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg){
	case WM_INITDIALOG:
		{
			OPENFILENAMEW* fn = (OPENFILENAMEW*)lParam;
			VDSaveVideoDialogW32* dlg = (VDSaveVideoDialogW32*)fn->lCustData;
			SetWindowLongPtr(hdlg, DWLP_USER, (LONG_PTR)dlg);
			dlg->mhdlg = hdlg;
			dlg->nFilterIndex = fn->nFilterIndex;
			dlg->DlgProc(msg,wParam,lParam);
			if (!VDIsAtLeastVistaW32()) SetTimer(hdlg,2,0,0);
			return TRUE;
		}

	case WM_TIMER:
		if (wParam==2) {
			// workaround fow winxp crappy dialog size
			KillTimer(hdlg,1);
			RECT r0;
			RECT r1;
			GetWindowRect(GetParent(hdlg),&r0);
			GetWindowRect(hdlg,&r1);
			if((r0.right-r0.left)<(r1.right-r1.left)){
				SetWindowPos(GetParent(hdlg),0,0,0,r1.right-r1.left+16,r0.bottom-r0.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
			}
			return TRUE;
		}

	case WM_SIZE:
	case WM_COMMAND:
		{
			VDSaveVideoDialogW32* dlg = (VDSaveVideoDialogW32*)GetWindowLongPtr(hdlg, DWLP_USER);
			dlg->DlgProc(msg,wParam,lParam);
			return TRUE;
		}

	case WM_NOTIFY:
		VDSaveVideoDialogW32* dlg = (VDSaveVideoDialogW32*)GetWindowLongPtr(hdlg, DWLP_USER);
		OFNOTIFY* data = (OFNOTIFY*)lParam;

		if(data->hdr.code==CDN_TYPECHANGE){
			dlg->nFilterIndex = data->lpOFN->nFilterIndex;
			dlg->ChangeFilterIndex();
		}
		if(data->hdr.code==CDN_FILEOK){
			VDStringW fname = OpenSave_GetFilePath(hdlg);
			if (!fname.empty()) return 0;
			// cant use this now because runoperation is modal
			/*try {
				if (!fname.empty() && dlg->Commit(fname)) return 0;
			} catch(MyError& err) {
				err.post(hdlg,g_szError);
			}*/

			SetWindowLong(hdlg,DWLP_MSGRESULT,1);
			return 1;
		}
		break;

	}
	return FALSE;
}

struct VDSaveDialogAVI: public VDSaveVideoDialogW32{
public:
	vdfastvector<IVDOutputDriver*> opt_driver;
	vdfastvector<int> opt_format;
	bool fUseCompatibility;

	virtual bool Commit(const VDStringW& fname);
	virtual void ChangeFilterIndex();
};

void SaveAVI(HWND hWnd, bool fUseCompatibility, bool queueAsJob) {
	if (!inputVideo) {
		MessageBox(hWnd, "No input video stream to process.", g_szError, MB_OK);
		return;
	}

	VDSaveDialogAVI dlg;
	dlg.inputAudio = inputAudio;
	dlg.addJob = queueAsJob;
	dlg.fUseCompatibility = fUseCompatibility;
	dlg.os_driver = g_FileOutDriver;
	dlg.os_format = g_FileOutFormat;

	OPENFILENAMEW fn = {sizeof(fn),0};
	fn.Flags = OFN_ENABLETEMPLATE | OFN_ENABLEHOOK;
	fn.hInstance = GetModuleHandle(0);
	fn.lpTemplateName = MAKEINTRESOURCEW(IDD_SAVEVIDEO_FORMAT);
	fn.lpfnHook = SaveVideoProc;
	fn.lCustData = (LONG_PTR)&dlg;

	VDStringW filters;
	filters += L"Audio-Video Interleave (*.avi)";
	filters += wchar_t(0);
	filters += L"*.avi";
	filters += wchar_t(0);

	dlg.opt_driver.push_back(0);
	dlg.opt_format.push_back(0);

	int select_filter = 0;
	VDStringW selectExt = VDStringW(L"avi");

	if (!fUseCompatibility) {
		tVDOutputDrivers drivers;
		VDGetOutputDrivers(drivers);
		for(tVDOutputDrivers::const_iterator it(drivers.begin()), itEnd(drivers.end()); it!=itEnd; ++it) {
			IVDOutputDriver *driver = *it;
			bool match_driver = g_FileOutDriver == driver->GetSignatureName();
			for(int i=0; ; i++){
				wchar_t filter[128];
				wchar_t ext[128];
				char name[128];
				if(!driver->GetDriver()->EnumFormats(i,filter,ext,name)) break;
				uint32 caps = driver->GetFormatCaps(i);
				if ((caps & kFormatCaps_UseVideo)!=kFormatCaps_UseVideo) continue;

				filters += filter;
				filters += wchar_t(0);
				filters += ext;
				filters += wchar_t(0);
				dlg.opt_driver.push_back(driver);
				dlg.opt_format.push_back(i);

				if (match_driver && g_FileOutFormat == name) {
					select_filter = dlg.opt_format.size()-1;
					if (wcscmp(ext,L"*.*")!=0)
						selectExt = VDFileSplitExt(ext)+1;
				}
			}
		}
	}

	filters +=	L"All files (*.*)";
	filters += wchar_t(0);
	filters += L"*.*";
	filters += wchar_t(0);
	dlg.opt_driver.push_back(0);
	dlg.opt_format.push_back(0);

	const wchar_t* title = fUseCompatibility ? L"Save AVI 1.0 File" : L"Save File";

	const VDFileDialogOption opts[]={
		{ VDFileDialogOption::kSelectedFilter_always, 0, NULL, 0, 0},
		{0}
	};

	int optvals[]={ select_filter+1 };

	VDStringW fname = VDGetSaveFileName(VDFSPECKEY_SAVEVIDEOFILE, (VDGUIHandle)hWnd, title, filters.c_str(), selectExt.c_str(), opts, optvals, &fn);
	dlg.Commit(fname);
}

void VDSaveDialogAVI::ChangeFilterIndex() {
	int type = nFilterIndex-1;
	IVDOutputDriver *driver = opt_driver[type];
	int format = opt_format[type];

	if (driver) {
		wchar_t filter[128];
		wchar_t ext[128];
		char name[128];
		driver->GetDriver()->EnumFormats(format,filter,ext,name);
		os_driver = driver->GetSignatureName();
		os_format = name;
	} else {
		os_driver.clear();
		os_format.clear();
	}
	InitCodec();
}

bool VDSaveDialogAVI::Commit(const VDStringW& fname) {
	int type = nFilterIndex-1;
	IVDOutputDriver *driver = opt_driver[type];
	int format = opt_format[type];
	RequestVideo req;
	req.fileOutput = fname;
	req.job = addJob;
	//req.propagateErrors = true;
	req.removeAudio = removeAudio;

	if (driver) {
		wchar_t filter[128];
		wchar_t ext[128];
		char name[128];
		driver->GetDriver()->EnumFormats(format,filter,ext,name);
		g_FileOutDriver = driver->GetSignatureName();
		g_FileOutFormat = name;
		if (!fname.empty()) {
			if (!CheckAudioCodec(name)) return false;
			req.driver = driver;
			req.format = name;
			g_project->SavePlugin(req);
		}

	} else {
		g_FileOutDriver.clear();
		g_FileOutFormat.clear();
		if (!fname.empty()) {
			if (!CheckAudioCodec("avi")) return false;
			req.compat = fUseCompatibility;
			g_project->SaveAVI(req);
		}
	}

	return true;
}

////////////////////////////////////

struct VDSaveDialogAudio: public VDSaveVideoDialogW32{
public:
	vdfastvector<IVDOutputDriver*> opt_driver;
	vdfastvector<int> opt_format;
	bool fUseCompatibility;

	virtual bool Commit(const VDStringW& fname);
};

void SaveAudio(HWND hWnd, bool queueAsJob) {
	if (!inputAudio) {
		MessageBox(hWnd, "No input audio stream to extract.", g_szError, MB_OK);
		return;
	}

	VDSaveDialogAudio dlg;
	dlg.saveVideo = false;
	dlg.inputAudio = inputAudio;
	dlg.addJob = queueAsJob;
	dlg.os_driver = g_AudioOutDriver;
	dlg.os_format = g_AudioOutFormat;

	OPENFILENAMEW fn = {sizeof(fn),0};
	fn.Flags = OFN_ENABLETEMPLATE | OFN_ENABLEHOOK;
	fn.hInstance = GetModuleHandle(0);
	fn.lpTemplateName = MAKEINTRESOURCEW(IDD_SAVEAUDIO_FORMAT);
	fn.lpfnHook = SaveVideoProc;
	fn.lCustData = (LONG_PTR)&dlg;

	VDStringW filters;
	filters += L"Windows audio (*.wav, *.w64)";
	filters += wchar_t(0);
	filters += L"*.wav;*.w64";
	filters += wchar_t(0);

	filters += L"Windows audio (*.wav)";
	filters += wchar_t(0);
	filters += L"*.wav";
	filters += wchar_t(0);

	dlg.opt_driver.push_back(0);
	dlg.opt_format.push_back(0);
	dlg.opt_driver.push_back(0);
	dlg.opt_format.push_back(0);

	int select_filter = 0;
	VDStringW selectExt = VDStringW(L"wav");
	if (g_AudioOutFormat == "old_wav") select_filter = 1;

	tVDOutputDrivers drivers;
	VDGetOutputDrivers(drivers);
	for(tVDOutputDrivers::const_iterator it(drivers.begin()), itEnd(drivers.end()); it!=itEnd; ++it) {
		IVDOutputDriver *driver = *it;
		bool match_driver = g_AudioOutDriver == driver->GetSignatureName();
		for(int i=0; ; i++){
			wchar_t filter[128];
			wchar_t ext[128];
			char name[128];
			if(!driver->GetDriver()->EnumFormats(i,filter,ext,name)) break;
			uint32 caps = driver->GetFormatCaps(i);
			if ((caps & (kFormatCaps_UseVideo|kFormatCaps_UseAudio))!=kFormatCaps_UseAudio) continue;

			filters += filter;
			filters += wchar_t(0);
			filters += ext;
			filters += wchar_t(0);
			dlg.opt_driver.push_back(driver);
			dlg.opt_format.push_back(i);

			if (match_driver && g_AudioOutFormat == name) {
				select_filter = dlg.opt_format.size()-1;
				if (wcscmp(ext,L"*.*")!=0)
					selectExt = VDFileSplitExt(ext)+1;
			}
		}
	}

	filters +=	L"All files (*.*)";
	filters += wchar_t(0);
	filters += L"*.*";
	filters += wchar_t(0);
	dlg.opt_driver.push_back(0);
	dlg.opt_format.push_back(0);

	const wchar_t* title = L"Save File";

	const VDFileDialogOption opts[]={
		{ VDFileDialogOption::kSelectedFilter_always, 0, NULL, 0, 0},
		{0}
	};

	int optvals[]={ select_filter+1 };

	VDStringW fname = VDGetSaveFileName(kFileDialog_WAVAudioOut, (VDGUIHandle)hWnd, title, filters.c_str(), selectExt.c_str(), opts, optvals, &fn);
	dlg.Commit(fname);
}

bool VDSaveDialogAudio::Commit(const VDStringW& fname) {
	int type = nFilterIndex-1;
	IVDOutputDriver *driver = opt_driver[type];
	int format = opt_format[type];
	if (driver) {
		wchar_t filter[128];
		wchar_t ext[128];
		char name[128];
		driver->GetDriver()->EnumFormats(format,filter,ext,name);
		g_AudioOutDriver = driver->GetSignatureName();
		g_AudioOutFormat = name;
		if (!fname.empty()) {
			if (!CheckAudioCodec(name)) return false;
			RequestVideo req;
			req.fileOutput = fname;
			req.job = addJob;
			//req.propagateErrors = true;
			req.removeVideo = true;
			req.driver = driver;
			req.format = name;
			g_project->SavePlugin(req);
		}

	} else {
		bool enable_w64 = type==0;
		g_AudioOutDriver.clear();
		g_AudioOutFormat.clear();
		if (!enable_w64) g_AudioOutFormat = "old_wav";
		if (!fname.empty()) {
			if (!CheckAudioCodec("wav")) return false;
			if (addJob) {
				JobRequestAudio req;
				SetProject(req, g_project);
				req.auto_w64 = enable_w64;
				req.fileOutput = fname;
				JobAddConfigurationSaveAudio(req);
			} else {
				RequestWAV req;
				req.fileOutput = fname;
				req.auto_w64 = enable_w64;
				//req.propagateErrors = true;
				SaveWAV(req);
			}
		}
	}

	return true;
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

	VDStringW fname(VDGetSaveFileName(VDFSPECKEY_SAVEVIDEOFILE, (VDGUIHandle)hWnd, L"Save segmented AVI", fileFiltersAppendAVI, L"avi", sOptions, optVals));

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
			JobRequestVideo req;
			SetProject(req, g_project);
			req.fileOutput = fname;
			req.fCompatibility = true;
			req.lSpillThreshold = optVals[3];
			if (optVals[1]) req.lSpillFrameThreshold = optVals[2];
			req.spillDigits = digits;
			JobAddConfiguration(req);
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
	void UpdateChecks();
	void UpdateSlider();
	void ChangeExtension(const wchar_t *newExtension);
	void ChangeTimeline();

	VDStringW	mPrefix;
	VDStringW	mPostfix;
	VDStringW	mDirectory;
	VDStringW	mFormatString;

	int digits;
	int startDigit;
	sint64 mFirstFrame, mLastFrame;
	int mFormat;
	int mQuality;
	bool mbQuickCompress;
	bool useTimeline;
	bool addJob;

	int actualDigits(){
		int r = digits;
		VDStringA s;
		if(useTimeline){
			s.append_sprintf("%d",mLastFrame);
		} else {
			s.append_sprintf("%d",mLastFrame-mFirstFrame+startDigit);
		}
		if(s.length()>r) r = s.length();
		return r;
	}
};

VDSaveImageSeqDialogW32::VDSaveImageSeqDialogW32()
	: VDDialogBaseW32(IDD_AVIOUTPUTIMAGES_FORMAT)
	, digits(0)
	, mFirstFrame(0)
	, mLastFrame(0)
	, mFormat(AVIOutputImages::kFormatBMP)
{
	addJob = false;
	useTimeline = false;
	startDigit = 0;
}
VDSaveImageSeqDialogW32::~VDSaveImageSeqDialogW32() {}

void VDSaveImageSeqDialogW32::UpdateFilenames() {
	mFormatString = VDMakePath(mDirectory.c_str(), mPrefix.c_str());
	
	VDStringW format(mPrefix + L"%0*lld" + mPostfix);
	int digits = actualDigits();

	sint64 t0 = startDigit;
	sint64 t1 = mLastFrame - mFirstFrame + startDigit;
	if (useTimeline) {
		t0 = mFirstFrame;
		t1 = mLastFrame;
	}

	VDSetWindowTextW32(GetDlgItem(mhdlg, IDC_STATIC_FIRSTFRAMENAME), VDswprintf(format.c_str(), 2, &digits, &t0).c_str());
	VDSetWindowTextW32(GetDlgItem(mhdlg, IDC_STATIC_LASTFRAMENAME), VDswprintf(format.c_str(), 2, &digits, &t1).c_str());
}

void VDSaveImageSeqDialogW32::UpdateEnables() {
	bool bIsJPEG = 0!=IsDlgButtonChecked(mhdlg, IDC_FORMAT_JPEG);
	bool bIsPNG = 0!=IsDlgButtonChecked(mhdlg, IDC_FORMAT_PNG);
	bool bIsTGA = 0!=IsDlgButtonChecked(mhdlg, IDC_FORMAT_TGA);
	bool bIsTIFF = 0!=IsDlgButtonChecked(mhdlg, IDC_FORMAT_TIFF);

	EnableWindow(GetDlgItem(mhdlg, IDC_QUALITY), bIsJPEG);
	EnableWindow(GetDlgItem(mhdlg, IDC_STATIC_QUALITY), bIsJPEG);
	EnableWindow(GetDlgItem(mhdlg, IDC_QUICK), bIsPNG);
	EnableWindow(GetDlgItem(mhdlg, IDC_TARGA_RLE), bIsTGA);
	EnableWindow(GetDlgItem(mhdlg, IDC_TIFF_ZIP), bIsTIFF);
	EnableWindow(GetDlgItem(mhdlg, IDC_TIFF_LZW), bIsTIFF);
}

void VDSaveImageSeqDialogW32::UpdateChecks() {
	CheckDlgButton(mhdlg, IDC_TARGA_RLE, mFormat == AVIOutputImages::kFormatTGA ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_TIFF_LZW, mFormat == AVIOutputImages::kFormatTIFF_LZW ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_TIFF_ZIP, mFormat == AVIOutputImages::kFormatTIFF_ZIP ? BST_CHECKED : BST_UNCHECKED);
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
		!wcscmp(mPostfix.c_str(), L".png") ||
		!wcscmp(mPostfix.c_str(), L".tiff") ||
		!wcscmp(mPostfix.c_str(), L".tif")
		) {
		VDSetWindowTextW32(GetDlgItem(mhdlg, IDC_FILENAME_SUFFIX), newExtension);
	}
}

void VDSaveImageSeqDialogW32::ChangeTimeline() {
	if (useTimeline) {
		SetDlgItemInt(mhdlg, IDC_FILENAME_START, UINT(mFirstFrame), FALSE);
		EnableWindow(GetDlgItem(mhdlg, IDC_FILENAME_START), FALSE);
	} else {
		SetDlgItemInt(mhdlg, IDC_FILENAME_START, startDigit, FALSE);
		EnableWindow(GetDlgItem(mhdlg, IDC_FILENAME_START), TRUE);
	}
}

INT_PTR VDSaveImageSeqDialogW32::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
	UINT uiTemp;
	BOOL fSuccess;

	switch(message) {
	case WM_INITDIALOG:
		CheckDlgButton(mhdlg,IDC_SAVE_DONOW, addJob ? BST_UNCHECKED:BST_CHECKED);
		CheckDlgButton(mhdlg,IDC_SAVE_MAKEJOB, addJob ? BST_CHECKED:BST_UNCHECKED);
		CheckDlgButton(mhdlg, IDC_START_SELECTION, useTimeline ? BST_CHECKED : BST_UNCHECKED);
		ChangeTimeline();
		SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETRANGE, TRUE, MAKELONG(0,100));
		SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETPOS, TRUE, mQuality);
		VDSetWindowTextW32(GetDlgItem(mhdlg, IDC_FILENAME_PREFIX), mPrefix.c_str());
		VDSetWindowTextW32(GetDlgItem(mhdlg, IDC_FILENAME_SUFFIX), mPostfix.c_str());
		SetDlgItemInt(mhdlg, IDC_FILENAME_DIGITS, digits, FALSE);
		VDSetWindowTextW32(GetDlgItem(mhdlg, IDC_DIRECTORY), mDirectory.c_str());
		CheckDlgButton(mhdlg, (mFormat == AVIOutputImages::kFormatTGA || mFormat == AVIOutputImages::kFormatTGAUncompressed) ? IDC_FORMAT_TGA
							: mFormat == AVIOutputImages::kFormatBMP ? IDC_FORMAT_BMP
							: mFormat == AVIOutputImages::kFormatJPEG ? IDC_FORMAT_JPEG
							: (mFormat == AVIOutputImages::kFormatTIFF_LZW || mFormat == AVIOutputImages::kFormatTIFF_RAW || mFormat == AVIOutputImages::kFormatTIFF_ZIP) ? IDC_FORMAT_TIFF
							: IDC_FORMAT_PNG
							, BST_CHECKED);
		CheckDlgButton(mhdlg, IDC_QUICK, mbQuickCompress ? BST_CHECKED : BST_UNCHECKED);
		UpdateFilenames();
		UpdateEnables();
		UpdateChecks();
		UpdateSlider();
		SetFocus(GetDlgItem(mhdlg,IDC_FILENAME_PREFIX));
		SendMessage(GetDlgItem(mhdlg,IDC_FILENAME_PREFIX),EM_SETSEL,0,mPrefix.length());
		return FALSE;

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

		case IDC_FILENAME_START:
			if (HIWORD(wParam) != EN_CHANGE) break;
			uiTemp = GetDlgItemInt(mhdlg, IDC_FILENAME_START, &fSuccess, FALSE);
			if (fSuccess) {
				startDigit = uiTemp;

				if (startDigit < 0)
					startDigit = 0;

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
				UpdateChecks();
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

		case IDC_FORMAT_TIFF:
			if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)) {
				UpdateEnables();
				mFormat = AVIOutputImages::kFormatTIFF_ZIP;
				UpdateChecks();
				ChangeExtension(L".tiff");
			}
			return TRUE;

		case IDC_QUICK:
			mbQuickCompress = !!SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
			return TRUE;

		case IDC_TARGA_RLE:
			{
				bool check = !!SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
				if (check)
					mFormat = AVIOutputImages::kFormatTGA;
				else
					mFormat = AVIOutputImages::kFormatTGAUncompressed;
			}
			return TRUE;

		case IDC_TIFF_LZW:
			{
				bool check = !!SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
				if (check) {
					mFormat = AVIOutputImages::kFormatTIFF_LZW;
					CheckDlgButton(mhdlg, IDC_TIFF_ZIP, BST_UNCHECKED);
				} else
					mFormat = AVIOutputImages::kFormatTIFF_RAW;
			}
			return TRUE;

		case IDC_TIFF_ZIP:
			{
				bool check = !!SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
				if (check) {
					mFormat = AVIOutputImages::kFormatTIFF_ZIP;
					CheckDlgButton(mhdlg, IDC_TIFF_LZW, BST_UNCHECKED);
				} else
					mFormat = AVIOutputImages::kFormatTIFF_RAW;
			}
			return TRUE;

		case IDC_SAVE_DONOW:
		case IDC_SAVE_MAKEJOB:
			addJob = IsDlgButtonChecked(mhdlg,IDC_SAVE_MAKEJOB)!=0;
			break;

		case IDC_START_SELECTION:
			useTimeline = !!SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
			ChangeTimeline();
			UpdateFilenames();
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
static const char g_szRegKeyImageSequenceUseTimeline[]="Image sequence: use timeline";

void SaveImageSeq(HWND hwnd, bool queueAsJob) {
	VDSaveImageSeqDialogW32 dlg;
	dlg.addJob = queueAsJob;

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
	if (g_project->IsSelectionPresent()) {
		dlg.mFirstFrame	= g_project->GetSelectionStartFrame();
		dlg.mLastFrame	= g_project->GetSelectionEndFrame() - 1;
	}

	dlg.mQuality	= key.getInt(g_szRegKeyImageSequenceQuality, 95);
	dlg.mbQuickCompress	= key.getBool(g_szRegKeyImageSequenceQuickCompress, true);
	dlg.digits		= key.getInt(g_szRegKeyImageSequenceMinDigits, 4);
	dlg.useTimeline	= key.getBool(g_szRegKeyImageSequenceUseTimeline, false);

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
		key.setBool(g_szRegKeyImageSequenceUseTimeline, dlg.useTimeline);

		dlg.digits = dlg.actualDigits();
		if (dlg.useTimeline) dlg.startDigit = int(dlg.mFirstFrame);

		int q = dlg.mQuality;

		if (dlg.mFormat == AVIOutputImages::kFormatPNG)
			q = dlg.mbQuickCompress ? 0 : 100;

		if (dlg.addJob) {
			JobRequestImages req;
			SetProject(req, g_project);
			req.filePrefix = dlg.mFormatString;
			req.fileSuffix = dlg.mPostfix;
			req.minDigits = dlg.digits;
			req.startDigit = dlg.startDigit;
			req.imageFormat = dlg.mFormat;
			req.quality = q;
			JobAddConfigurationImages(req);
		} else {
			RequestImages req;
			req.filePrefix = dlg.mFormatString;
			req.fileSuffix = dlg.mPostfix;
			req.minDigits = dlg.digits;
			req.startDigit = dlg.startDigit;
			req.imageFormat = dlg.mFormat;
			req.quality = q;
			SaveImageSequence(req);
		}
	}
}

class VDSaveImageDialogW32 {
public:
	VDSaveImageDialogW32();

	INT_PTR DlgProc(UINT message, WPARAM wParam, LPARAM lParam);
	void UpdateEnables();
	void UpdateChecks();
	void UpdateSlider();
	void ChangeExtension(const wchar_t *newExtension);
	void ChangeFilename(const wchar_t *newName);
	void InitFormat();

	HWND mhdlg;
	int mFormat;
	int mQuality;
	bool mbQuickCompress;
};

VDSaveImageDialogW32::VDSaveImageDialogW32()
	: mFormat(AVIOutputImages::kFormatBMP)
{
}

void VDSaveImageDialogW32::UpdateEnables() {
	bool bIsJPEG = 0!=IsDlgButtonChecked(mhdlg, IDC_FORMAT_JPEG);
	bool bIsPNG = 0!=IsDlgButtonChecked(mhdlg, IDC_FORMAT_PNG);
	bool bIsTGA = 0!=IsDlgButtonChecked(mhdlg, IDC_FORMAT_TGA);
	bool bIsTIFF = 0!=IsDlgButtonChecked(mhdlg, IDC_FORMAT_TIFF);

	EnableWindow(GetDlgItem(mhdlg, IDC_QUALITY), bIsJPEG);
	EnableWindow(GetDlgItem(mhdlg, IDC_STATIC_QUALITY), bIsJPEG);
	EnableWindow(GetDlgItem(mhdlg, IDC_QUICK), bIsPNG);
	EnableWindow(GetDlgItem(mhdlg, IDC_TARGA_RLE), bIsTGA);
	EnableWindow(GetDlgItem(mhdlg, IDC_TIFF_ZIP), bIsTIFF);
	EnableWindow(GetDlgItem(mhdlg, IDC_TIFF_LZW), bIsTIFF);
}

void VDSaveImageDialogW32::UpdateChecks() {
	CheckDlgButton(mhdlg, IDC_TARGA_RLE, mFormat == AVIOutputImages::kFormatTGA ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_TIFF_LZW, mFormat == AVIOutputImages::kFormatTIFF_LZW ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_TIFF_ZIP, mFormat == AVIOutputImages::kFormatTIFF_ZIP ? BST_CHECKED : BST_UNCHECKED);
}

void VDSaveImageDialogW32::UpdateSlider() {
	mQuality = SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_GETPOS, 0, 0);
	SetDlgItemInt(mhdlg, IDC_STATIC_QUALITY, mQuality, FALSE);
}

void VDSaveImageDialogW32::ChangeExtension(const wchar_t *newExtension) {
	wchar_t buf[MAX_PATH];
	HWND parent = (HWND)GetWindowLongPtr(mhdlg,GWLP_HWNDPARENT);
	CommDlg_OpenSave_GetSpec(parent,buf,MAX_PATH);
	VDStringW name(buf);
	VDStringW base = VDFileSplitExtLeft(name);
	VDStringW new_name = base+newExtension;
	CommDlg_OpenSave_SetControlText(parent, cmb13, new_name.c_str());
	CommDlg_OpenSave_SetDefExt(parent, newExtension+1);
}

int FormatFromName(const wchar_t *cname) {
	VDStringW name(cname);
	VDStringW ext = VDFileSplitExtRight(name);
	if (_wcsicmp(ext.c_str(),L".bmp")==0) return AVIOutputImages::kFormatBMP;
	if (_wcsicmp(ext.c_str(),L".tga")==0) return AVIOutputImages::kFormatTGA;
	if (_wcsicmp(ext.c_str(),L".jpg")==0) return AVIOutputImages::kFormatJPEG;
	if (_wcsicmp(ext.c_str(),L".jpeg")==0) return AVIOutputImages::kFormatJPEG;
	if (_wcsicmp(ext.c_str(),L".png")==0) return AVIOutputImages::kFormatPNG;
	if (_wcsicmp(ext.c_str(),L".tif")==0) return AVIOutputImages::kFormatTIFF_RAW;
	if (_wcsicmp(ext.c_str(),L".tiff")==0) return AVIOutputImages::kFormatTIFF_RAW;
	return -1;
}

const wchar_t* ExtFromFormat(int format) {
	if (format==AVIOutputImages::kFormatBMP) return L".bmp";
	if (format==AVIOutputImages::kFormatTGA) return L".tga";
	if (format==AVIOutputImages::kFormatTGAUncompressed) return L".tga";
	if (format==AVIOutputImages::kFormatPNG) return L".png";
	if (format==AVIOutputImages::kFormatJPEG) return L".jpeg";
	if (format==AVIOutputImages::kFormatTIFF_LZW) return L".tiff";
	if (format==AVIOutputImages::kFormatTIFF_RAW) return L".tiff";
	if (format==AVIOutputImages::kFormatTIFF_ZIP) return L".tiff";
	return 0;
}

void VDSaveImageDialogW32::ChangeFilename(const wchar_t *newName) {
	int format = FormatFromName(newName);
	if (format!=-1 && format!=mFormat) {
		mFormat = format;
		InitFormat();
	}
}

void VDSaveImageDialogW32::InitFormat() {
	CheckDlgButton(mhdlg, IDC_FORMAT_TGA, BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_FORMAT_BMP, BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_FORMAT_JPEG, BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_FORMAT_PNG, BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_FORMAT_TIFF, BST_UNCHECKED);

	CheckDlgButton(mhdlg, (mFormat == AVIOutputImages::kFormatTGA || mFormat == AVIOutputImages::kFormatTGAUncompressed) ? IDC_FORMAT_TGA
						: mFormat == AVIOutputImages::kFormatBMP ? IDC_FORMAT_BMP
						: mFormat == AVIOutputImages::kFormatJPEG ? IDC_FORMAT_JPEG
						: (mFormat == AVIOutputImages::kFormatTIFF_LZW || mFormat == AVIOutputImages::kFormatTIFF_RAW || mFormat == AVIOutputImages::kFormatTIFF_ZIP) ? IDC_FORMAT_TIFF
						: IDC_FORMAT_PNG
						, BST_CHECKED);

	UpdateEnables();
}

INT_PTR VDSaveImageDialogW32::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch(message) {
	case WM_INITDIALOG:
		SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETRANGE, TRUE, MAKELONG(0,100));
		SendDlgItemMessage(mhdlg, IDC_QUALITY, TBM_SETPOS, TRUE, mQuality);
		CheckDlgButton(mhdlg, IDC_QUICK, mbQuickCompress ? BST_CHECKED : BST_UNCHECKED);
		InitFormat();
		UpdateChecks();
		UpdateSlider();

		return TRUE;

	case WM_HSCROLL:
		UpdateSlider();
		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {

		// There is a distinct sense of non-scalability here

		case IDC_FORMAT_TGA:
			if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)) {
				UpdateEnables();
				mFormat = AVIOutputImages::kFormatTGA;
				UpdateChecks();
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

		case IDC_FORMAT_TIFF:
			if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)) {
				UpdateEnables();
				mFormat = AVIOutputImages::kFormatTIFF_ZIP;
				UpdateChecks();
				ChangeExtension(L".tiff");
			}
			return TRUE;

		case IDC_QUICK:
			mbQuickCompress = !!SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
			return TRUE;

		case IDC_TARGA_RLE:
			{
				bool check = !!SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
				if (check)
					mFormat = AVIOutputImages::kFormatTGA;
				else
					mFormat = AVIOutputImages::kFormatTGAUncompressed;
			}
			return TRUE;

		case IDC_TIFF_LZW:
			{
				bool check = !!SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
				if (check) {
					mFormat = AVIOutputImages::kFormatTIFF_LZW;
					CheckDlgButton(mhdlg, IDC_TIFF_ZIP, BST_UNCHECKED);
				} else
					mFormat = AVIOutputImages::kFormatTIFF_RAW;
			}
			return TRUE;

		case IDC_TIFF_ZIP:
			{
				bool check = !!SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
				if (check) {
					mFormat = AVIOutputImages::kFormatTIFF_ZIP;
					CheckDlgButton(mhdlg, IDC_TIFF_LZW, BST_UNCHECKED);
				} else
					mFormat = AVIOutputImages::kFormatTIFF_RAW;
			}
			return TRUE;
		}
		break;
	}

	return FALSE;
}

static const char g_szRegKeyImageFormat[]="Image: format";
static const char g_szRegKeyImageQuality[]="Image: quality";
static const char g_szRegKeyImageQuickCompress[]="Image: quick compress";

UINT_PTR CALLBACK SaveImageProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg){
	case WM_INITDIALOG:
		{
			OPENFILENAMEW* fn = (OPENFILENAMEW*)lParam;
			VDSaveImageDialogW32* dlg = (VDSaveImageDialogW32*)fn->lCustData;
			SetWindowLongPtr(hdlg, DWLP_USER, (LONG_PTR)dlg);
			dlg->mhdlg = hdlg;
			dlg->DlgProc(msg,wParam,lParam);
			if (!VDIsAtLeastVistaW32()) SetTimer(hdlg,2,0,0);
			return TRUE;
		}

	case WM_TIMER:
		if (wParam==2) {
			// workaround fow winxp crappy dialog size
			KillTimer(hdlg,1);
			RECT r0;
			RECT r1;
			GetWindowRect(GetParent(hdlg),&r0);
			GetWindowRect(hdlg,&r1);
			if((r0.right-r0.left)<(r1.right-r1.left)){
				SetWindowPos(GetParent(hdlg),0,0,0,r1.right-r1.left+16,r0.bottom-r0.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
			}
			return TRUE;
		}

	case WM_HSCROLL:
		{
			VDSaveImageDialogW32* dlg = (VDSaveImageDialogW32*)GetWindowLongPtr(hdlg, DWLP_USER);
			dlg->DlgProc(msg,wParam,lParam);
			return TRUE;
		}

	case WM_COMMAND:
		{
			VDSaveImageDialogW32* dlg = (VDSaveImageDialogW32*)GetWindowLongPtr(hdlg, DWLP_USER);
			dlg->DlgProc(msg,wParam,lParam);
			return TRUE;
		}

	case WM_NOTIFY:
		OFNOTIFY* data = (OFNOTIFY*)lParam;
		if(data->hdr.code==CDN_SELCHANGE){
			wchar_t buf[MAX_PATH];
			CommDlg_OpenSave_GetSpec(data->hdr.hwndFrom,buf,MAX_PATH);
			VDSaveImageDialogW32* dlg = (VDSaveImageDialogW32*)GetWindowLongPtr(hdlg, DWLP_USER);
			dlg->ChangeFilename(buf);
		}
		break;
	}
	return FALSE;
}

void SaveImage(HWND hwnd, VDPosition frame, VDPixmap* px) {
	VDSaveImageDialogW32 dlg;

	VDRegistryAppKey key(g_szRegKeyPersistence);

	dlg.mFormat = key.getInt(g_szRegKeyImageFormat, AVIOutputImages::kFormatTGA);
	if ((unsigned)dlg.mFormat >= AVIOutputImages::kFormatCount)
		dlg.mFormat = AVIOutputImages::kFormatTGA;
	dlg.mQuality	= key.getInt(g_szRegKeyImageQuality, 95);
	dlg.mbQuickCompress	= key.getBool(g_szRegKeyImageQuickCompress, true);

	if (dlg.mQuality < 0)
		dlg.mQuality = 0;
	else if (dlg.mQuality > 100)
		dlg.mQuality = 100;

	OPENFILENAMEW fn = {sizeof(fn),0};
	fn.hwndOwner = hwnd;
	fn.Flags	= OFN_PATHMUSTEXIST|OFN_ENABLESIZING|OFN_EXPLORER|OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY;
	fn.Flags |= OFN_ENABLETEMPLATE | OFN_ENABLEHOOK;
	fn.hInstance = GetModuleHandle(0);
	fn.lpTemplateName = MAKEINTRESOURCEW(IDD_SAVEIMAGE_FORMAT);
	fn.lpfnHook = SaveImageProc;
	fn.lCustData = (LONG_PTR)&dlg;

	const wchar_t* filter = L"Images\0*.bmp;*.tga;*.jpg;*.jpeg;*.png;*.tif;*.tiff\0";

	VDStringW title;
	if (frame==-1)
		title = L"Save Image";
	else
		title.sprintf(L"Save Image: frame %lld", frame);

	const VDFileDialogOption opts[]={
		{ VDFileDialogOption::kSelectedFilter_always, 0, NULL, 0, 0},
		{0}
	};

	int optvals[]={ 1 };

	VDStringW name = VDGetSaveFileName(VDFSPECKEY_SAVEIMAGEFILE, (VDGUIHandle)hwnd, title.c_str(), filter, ExtFromFormat(dlg.mFormat)+1, opts, optvals, &fn);

	if (!name.empty()) {
		int format = FormatFromName(name.c_str());
		if (format==-1)
			name = VDFileSplitExtLeft(name) + ExtFromFormat(dlg.mFormat);

		key.setInt(g_szRegKeyImageFormat, dlg.mFormat);
		key.setInt(g_szRegKeyImageQuality, dlg.mQuality);
		key.setBool(g_szRegKeyImageQuickCompress, dlg.mbQuickCompress);

		int q = dlg.mQuality;

		if (dlg.mFormat == AVIOutputImages::kFormatPNG)
			q = dlg.mbQuickCompress ? 0 : 100;

		AVIOutputImages::WriteSingleImage(name.c_str(),dlg.mFormat,dlg.mQuality,px);
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

void SaveProject(HWND hWnd, bool reset_path) {
	VDStringW filename = g_project->mProjectFilename;
	
	if (filename.empty() || reset_path || g_project->mProjectReadonly) {
		if (!hWnd) return;
		filename = VDGetSaveFileName(kFileDialog_Project, (VDGUIHandle)hWnd, L"Save Project", fileFiltersSaveProject, L"vdproject", 0, 0);
	}

	if (!filename.empty()) {
		try {
			VDFile f;
			f.open(filename.c_str(), nsVDFile::kWrite | nsVDFile::kCreateAlways);
			VDStringW dataSubdir;
			g_project->SaveData(filename,dataSubdir);
			g_project->SaveProjectPath(filename,dataSubdir);
			g_project->SaveScript(f,dataSubdir,true);
			f.close();
		} catch(const MyError& e) {
			e.post(NULL, g_szError);
		}
	}
}
