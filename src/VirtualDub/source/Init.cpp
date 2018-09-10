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
#include <shellapi.h>
#include <shlwapi.h>
#include <eh.h>
#include <signal.h>

#include "resource.h"
#include "job.h"
#include "oshelper.h"
#include "prefs.h"
#include "auxdlg.h"
#include <vd2/system/error.h>
#include "gui.h"
#include "filters.h"
#include "command.h"
#include "script.h"
#include <vd2/system/vdalloc.h>
#include <vd2/system/tls.h>
#include <vd2/system/thread.h>
#include <vd2/system/profile.h>
#include <vd2/system/registry.h>
#include <vd2/system/registrymemory.h>
#include <vd2/system/filesys.h>
#include <vd2/system/w32assist.h>
#include <vd2/system/VDString.h>
#include <vd2/system/cmdline.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/protscope.h>
#include <vd2/Dita/resources.h>
#include <vd2/Dita/services.h>
#include <vd2/VDDisplay/display.h>
#include <vd2/Riza/direct3d.h>
#include <vd2/Riza/videocodec.h>
#include <vd2/VDLib/PortableRegistry.h>
#include <vd2/VDLib/win32/DebugOutputFilter.h>
#include "crash.h"
#include "DubSource.h"

#include "ClippingControl.h"
#include "PositionControl.h"
#include "LevelControl.h"
#include "HexViewer.h"
#include "FilterGraph.h"
#include "LogWindow.h"
#include "VideoWindow.h"
#include "AudioDisplay.h"
#include "RTProfileDisplay.h"
#include "plugins.h"

#include "project.h"
#include "projectui.h"
#include "capture.h"
#include "captureui.h"
#include "version.h"
#include "AutoRecover.h"
#include "ExternalEncoderProfile.h"
#include "AVIOutputPlugin.h"
#include "FilterInstance.h"
#include "InputFile.h"

#pragma comment(lib, "shlwapi")

///////////////////////////////////////////////////////////////////////////

extern void InitBuiltinFilters();
extern void VDInitBuiltinAudioFilters();
extern void VDInitBuiltinInputDrivers();
extern void VDInitExternalCallTrap();
extern void VDInitVideoCodecBugTrap();
extern void VDInitProtectedScopeHook();
extern void VDInitTools();
extern void VDShutdownTools();
extern void VDDeletePreferencesShadow();

extern uint32 VDPreferencesGetEnabledCPUFeatures();
extern void VDPreferencesSetFilterAccelVisualDebugEnabled(bool);
extern bool VDPreferencesGetAutoRecoverEnabled();

extern bool VDRegisterRTProfileDisplayControl2();

extern void VDShutdownEventProfiler();

extern void VDDisplayLicense(HWND hwndParent, bool conditional);

///////////////////////////////////////////////////////////////////////////

extern LONG __stdcall CrashHandlerHook(EXCEPTION_POINTERS *pExc);
extern LONG __stdcall CrashHandler(struct _EXCEPTION_POINTERS *ExceptionInfo, bool allowForcedExit);
extern LONG APIENTRY MainWndProc( HWND hWnd, UINT message, UINT wParam, LONG lParam);
extern void DetectDivX();

void VDDumpChangeLog();

bool InitApplication(HINSTANCE hInstance);
bool InitInstance( HANDLE hInstance, int nCmdShow, bool topmost);
void ParseCommandLine(const wchar_t *lpCmdLine);

///////////////////////////////////////////////////////////////////////////

static BOOL compInstalled;	// yuck

extern COMPVARS2		g_Vcompression;

extern HINSTANCE	g_hInst;
extern HWND			g_hWnd;

extern VDProject *g_project;
vdrefptr<VDProjectUI> g_projectui;

extern vdrefptr<IVDCaptureProject> g_capProject;
extern vdrefptr<IVDCaptureProjectUI> g_capProjectUI;

extern DubSource::ErrorMode	g_videoErrorMode;
extern DubSource::ErrorMode	g_audioErrorMode;

extern wchar_t g_szFile[MAX_PATH];

extern const char g_szError[];

bool g_exitOnDone = false;
bool g_bAutoTest = false;
bool g_fWine = false;
bool g_bEnableVTuneProfiling;

void (*g_pPostInitRoutine)();

VDStringW g_portableRegistryPath;
VDRegistryProviderMemory *g_pPortableRegistry = NULL;

///////////////////////////////////////////////////////////////////////////

namespace {
	typedef std::list<VDStringA> tArguments;

	tArguments g_VDStartupArguments;
}

const char *VDGetStartupArgument(int index) {
	tArguments::const_iterator it(g_VDStartupArguments.begin()), itEnd(g_VDStartupArguments.end());

	for(; it!=itEnd && index; ++it, --index)
		;

	if (it == itEnd)
		return NULL;

	return (*it).c_str();
}

void VDterminate() {
	vdprotected("processing call to terminate() (probably caused by exception within destructor)") {
#if _MSC_VER >= 1300
		__debugbreak();
#else
		__asm int 3
#endif
	}
}

extern "C" void _abort() {
	vdprotected("processing call to abort()") {
#if _MSC_VER >= 1300
		__debugbreak();
#else
		__asm int 3
#endif
	}
}

void VDCPUTest() {
	uint32 featureMask = VDPreferencesGetEnabledCPUFeatures();
	long lEnableFlags;

	lEnableFlags = featureMask & CPUF_SUPPORTS_MASK;

	if (!(featureMask & PreferencesMain::OPTF_FORCE)) {
		SYSTEM_INFO si;

		lEnableFlags = CPUCheckForExtensions();

		GetSystemInfo(&si);

		if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
			if (si.wProcessorLevel < 4)
				lEnableFlags &= ~CPUF_SUPPORTS_FPU;		// Not strictly true, but very slow anyway
	}

	// Enable FPU support...

	CPUEnableExtensions(lEnableFlags);

	VDFastMemcpyAutodetect();
}

///////////////////////////////////////////////////////////////////////////

bool g_consoleMode;

class VDConsoleLogger : public IVDLogger {
public:
	void AddLogEntry(int severity, const wchar_t *s);
	void Write(const wchar_t *s, size_t len);
} g_VDConsoleLogger;

void VDConsoleLogger::AddLogEntry(int severity, const wchar_t *s) {
	const size_t len = wcslen(s);
	const wchar_t *text = s;
	const wchar_t *end = text + len;

	// Don't annotate lines in this routine. We print some 'errors' in
	// the cmdline handling that aren't really errors, and would have
	// to be revisited.
	for(;;) {
		const wchar_t *t = text;

		while(t != end && *t != '\n')
			++t;

		Write(text, t-text);
		if (t == end)
			break;

		text = t+1;
	}
}

void VDConsoleLogger::Write(const wchar_t *text, size_t len) {
	DWORD actual;

	if (!len) {
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), "\r\n", 2, &actual, NULL);
	}

	int mblen = WideCharToMultiByte(CP_ACP, 0, text, len, NULL, 0, NULL, NULL);

	char *buf = (char *)alloca(mblen + 2);

	mblen = WideCharToMultiByte(CP_ACP, 0, text, len, buf, mblen, NULL, NULL);

	if (mblen) {
		buf[mblen] = '\r';
		buf[mblen+1] = '\n';

		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, mblen+2, &actual, NULL);
	}
}

///////////////////////////////////////////////////////////////////////////

#if 0

void crash() {
	__try {
		__asm xor ebx,ebx
		__asm mov eax,dword ptr [ebx]
		__asm mov dword ptr [ebx],eax
		__asm lock add dword ptr cs:[00000000h], 12345678h
		__asm movq xmm0, qword ptr [eax]
		__asm {
__emit 0x66
__emit 0x0f
__emit 0x6f
__emit 0x2d
__emit 0xf0
__emit 0x42
__emit 0x0e
__emit 0x10
		}
	} __except(CrashHandler((EXCEPTION_POINTERS*)_exception_info(), true)) {
	}
}
#else

static void crash3() {
//	vdprotected("doing foo") {
		for(int i=0; i<10; ++i) {
		   vdprotected1("in foo iteration %d", int, i)
//			volatile VDProtectedAutoScope1<int> autoscope(VDProtectedAutoScopeData1<int>(__FILE__, __LINE__, "in foo iteration %d", i));
			
			   if (i == 5)
				   *(volatile char *)0 = 0;
		}
//	}
}

static void crash2() {
	__try {
		crash3();
	} __except(CrashHandler((EXCEPTION_POINTERS*)_exception_info(), true)) {
	}
}

static void crash() {
	vdprotected("deliberately trying to crash")
		crash2();
}

#endif

void VDInitAppResources() {
	HRSRC hResource = FindResource(NULL, MAKEINTRESOURCE(IDR_RESOURCES), "STUFF");

	if (!hResource)
		return;

	HGLOBAL hGlobal = LoadResource(NULL, hResource);
	if (!hGlobal)
		return;

	LPVOID lpData = LockResource(hGlobal);
	if (!lpData)
		return;

	VDLoadResources(0, lpData, SizeofResource(NULL, hResource));
}

#ifndef PROCESS_CALLBACK_FILTER_ENABLED
#define PROCESS_CALLBACK_FILTER_ENABLED     0x1
#endif

void VDEnableExceptionsFromUserCallbacksW32() {
	HMODULE hmodKernel32 = GetModuleHandle("kernel32");

	// SetProcessUserModeExceptionPolicy() is available in Windows 7 SP1 and in Vista/WS2008/Win7
	// systems with the hotfix (274454 for Vista, 299104 for WS2008/Win7).
	typedef BOOL (WINAPI *tpSetProcessUserModeExceptionPolicy)(DWORD dwFlags);
	typedef BOOL (WINAPI *tpGetProcessUserModeExceptionPolicy)(LPDWORD pdwFlags);
	tpSetProcessUserModeExceptionPolicy pSetProcessUserModeExceptionPolicy =
		(tpSetProcessUserModeExceptionPolicy)GetProcAddress(hmodKernel32, "SetProcessUserModeExceptionPolicy");
	tpGetProcessUserModeExceptionPolicy pGetProcessUserModeExceptionPolicy =
		(tpGetProcessUserModeExceptionPolicy)GetProcAddress(hmodKernel32, "GetProcessUserModeExceptionPolicy");

	if (pGetProcessUserModeExceptionPolicy && pSetProcessUserModeExceptionPolicy) {
		DWORD dwFlags = 0;

		if (pGetProcessUserModeExceptionPolicy(&dwFlags)) {
			dwFlags &= ~PROCESS_CALLBACK_FILTER_ENABLED;

			pSetProcessUserModeExceptionPolicy(dwFlags);
		}
	}
}

bool Init(HINSTANCE hInstance, int nCmdShow, VDCommandLine& cmdLine) {

//#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
//#endif

	VDSetThreadDebugName(GetCurrentThreadId(), "Main");
	VDInitProtectedScopeHook();

	VDSetDataPath(VDGetProgramPath().c_str());

	const wchar_t *pDataPath = VDGetDataPath();
	VDSetCrashDumpPath(pDataPath);

	// setup crash traps
	if (!cmdLine.FindAndRemoveSwitch(L"h")) {
		SetUnhandledExceptionFilter(CrashHandlerHook);

		extern void VDPatchSetUnhandledExceptionFilter();
		VDPatchSetUnhandledExceptionFilter();
	}

	VDEnableExceptionsFromUserCallbacksW32();

	set_terminate(VDterminate);

	VDInitExternalCallTrap();
	VDInitVideoCodecBugTrap();

	// initialize globals
    g_hInst = hInstance;

	// initialize TLS trace system
	VDSetThreadInitHook(VDThreadInitHandler);

	// initialize TLS for main thread
	VDInitThreadData("Main thread");

	// initialize resource system
	VDInitResourceSystem();
	VDInitAppResources();

	// check for console mode and announce startup
	if (cmdLine.FindAndRemoveSwitch(L"console")) {
		g_consoleMode = true;
		VDAttachLogger(&g_VDConsoleLogger, false, true);

		// announce startup
		VDStringW s(L"VirtualDub CLI Video Processor Version $v$s (build $b/$c) for $p");
		VDSubstituteStrings(s);

		VDLog(kVDLogInfo, s);
		VDLog(kVDLogInfo, VDswprintf(
				L"Copyright (C) Avery Lee 1998-2009. Licensed under GNU General Public License\n"
				,1
				,&version_num));
	}

	// prep system stuff

	VDCHECKPOINT;

	AVIFileInit();

	const bool resetAll = cmdLine.FindAndRemoveSwitch(L"resetall");

	const wchar_t *portableAltFile = 0;
	cmdLine.FindAndRemoveSwitch(L"portablealt", portableAltFile);

	VDStringW portableRegPath;
	if (portableAltFile)
		portableRegPath = portableAltFile;
	else
		portableRegPath = VDMakePath(VDGetProgramPath().c_str(), L"VirtualDub.ini");

	if (portableAltFile || cmdLine.FindAndRemoveSwitch(L"portable") || VDDoesPathExist(portableRegPath.c_str())) {
		g_portableRegistryPath = portableRegPath;
		g_pPortableRegistry = new VDRegistryProviderMemory;
		VDSetRegistryProvider(g_pPortableRegistry);

		if (!resetAll && VDDoesPathExist(portableRegPath.c_str())) {
			try {
				VDLoadRegistry(portableRegPath.c_str());
			} catch(const MyError& err) {
				VDStringA message;

				message.sprintf("There was an error loading the settings file:\n\n%s\n\nDo you want to continue? If so, settings will be reset to defaults and may not be saved.", err.c_str());
				if (IDYES != MessageBox(NULL, message.c_str(), "VirtualDub Warning", MB_YESNO | MB_ICONWARNING))
					return false;
			}
		}
	} else {
		if (resetAll)
			SHDeleteKey(HKEY_CURRENT_USER, "Software\\VirtualDub.org\\VirtualDub");
	}

	VDRegistryAppKey::setDefaultKey("Software\\VirtualDub.org\\VirtualDub\\");

	const bool regUseProfileSetting = VDRegistryAppKey("Preferences", false).getBool("Use profile-local path")
		|| VDRegistryAppKey("Preferences", false, true).getBool("Use profile-local path") ;
	const bool useProfileSwitch = cmdLine.FindAndRemoveSwitch(L"useprofile");
	const bool noUseProfileSwitch = cmdLine.FindAndRemoveSwitch(L"nouseprofile");
	const bool useProfileSetting = useProfileSwitch || (!noUseProfileSwitch && regUseProfileSetting);

	if (useProfileSetting) {
		const VDStringW& localAppDataPath = VDGetLocalAppDataPath();
		const VDStringW& localAppDataPath1 = VDMakePath(localAppDataPath.c_str(), L"virtualdub.org");
		const VDStringW& localAppDataPath2 = VDMakePath(localAppDataPath1.c_str(), L"VirtualDub");
		if (!VDDoesPathExist(localAppDataPath2.c_str())) {
			VDCreateDirectory(localAppDataPath1.c_str());
			VDCreateDirectory(localAppDataPath2.c_str());
		}

		VDSetDataPath(localAppDataPath2.c_str());
		VDSetCrashDumpPath(localAppDataPath2.c_str());
	}

	if (useProfileSetting != regUseProfileSetting) {
		VDRegistryAppKey key("Preferences", true);

		key.setBool("Use profile-local path", useProfileSetting);
	}

	VDLoadFilespecSystemData();

	LoadPreferences();
	{
		VDRegistryAppKey key("Preferences");
		unsigned errorMode;

		errorMode = key.getInt("Edit: Video error mode");
		if (errorMode < DubSource::kErrorModeCount)
			g_videoErrorMode = (DubSource::ErrorMode)errorMode;

		errorMode = key.getInt("Edit: Audio error mode");
		if (errorMode < DubSource::kErrorModeCount)
			g_audioErrorMode = (DubSource::ErrorMode)errorMode;
	}

	if (!cmdLine.FindAndRemoveSwitch(L"safecpu"))
		VDCPUTest();

	int pluginsSucceeded = 0;
	int pluginsFailed = 0;
	vdprotected("autoloading filters at startup") {
		int f, s;

		VDStringW programPath(VDGetProgramPath());

		VDLoadPlugins(VDMakePath(programPath.c_str(), L"plugins"), s, f);
		pluginsSucceeded += s;
		pluginsFailed += f;
#ifdef _M_AMD64
		VDLoadPlugins(VDMakePath(programPath.c_str(), L"plugins64"), s, f);
#else
		VDLoadPlugins(VDMakePath(programPath.c_str(), L"plugins32"), s, f);
#endif
		pluginsSucceeded += s;
		pluginsFailed += f;
	}

	// initialize filters, job system, MRU list, help system

	InitBuiltinFilters();
	VDInitBuiltinAudioFilters();
	VDInitBuiltinInputDrivers();
	VDInitInputDrivers();
	VDInitOutputDrivers();
	VDInitAudioEnc();
	VDInitTools();
	VDLoadExternalEncoderProfiles();

	if (!InitJobSystem())
		return FALSE;

	// initialize interface

	VDCHECKPOINT;

    if (!InitApplication(hInstance))
            return (FALSE);              

	VDInstallModelessDialogHookW32();

	// display welcome requester
	//Welcome();
	VDDisplayLicense(NULL, true);

	// Announce experimentality.
	AnnounceExperimental();

    // Create the main window.

	if (cmdLine.FindAndRemoveSwitch(L"min")) {
		nCmdShow = SW_SHOWMINNOACTIVE;
	} else if (cmdLine.FindAndRemoveSwitch(L"max")) {
		nCmdShow = SW_SHOWMAXIMIZED;
	}

	if (!InitInstance(hInstance, nCmdShow, cmdLine.FindAndRemoveSwitch(L"topmost")))
        return (FALSE);

	// Autoload filters.

	VDCHECKPOINT;
	if (pluginsFailed)
		guiSetStatus("Autoloaded %d filter(s) (%d failed). Check the log for details.", 255, pluginsSucceeded, pluginsFailed);
	else if (pluginsSucceeded)
		guiSetStatus("Autoloaded %d filter(s).", 255, pluginsSucceeded);

	// Detect DivX.

	DetectDivX();

	// All done!

	VDCHECKPOINT;

	if (g_pPostInitRoutine)
		g_pPostInitRoutine();

	return true;
}

///////////////////////////////////////////////////////////////////////////

void Deinit() {
	VDCHECKPOINT;

	g_project->CloseAVI();
	g_project->CloseWAV();

	g_projectui->Detach();
	g_projectui = NULL;
	g_project = NULL;

	VDUIFrame::DestroyAll();

	filters.DeinitFilters();

	VDCHECKPOINT;

	g_filterChain.Clear();

	VDCHECKPOINT;

	CloseJobWindow();
	DeinitJobSystem();

	VDShutdownTools();
	VDShutdownAudioEnc();
	VDShutdownOutputDrivers();
	VDShutdownInputDrivers();			// must be before plugin system
	VDDeinitPluginSystem();

	g_VDStartupArguments.clear();

	VDCHECKPOINT;

	if (g_ACompressionFormat)
		freemem(g_ACompressionFormat);

	if (g_Vcompression.dwFlags & ICMF_COMPVARS_VALID)
		FreeCompressor(&g_Vcompression);

	if (compInstalled)
		ICRemove(ICTYPE_VIDEO, 'TSDV', 0);

	VDDeinstallModelessDialogHookW32();

	VDCHECKPOINT;

	AVIFileExit();

	_CrtCheckMemory();

	VDCHECKPOINT;

	VDShutdownExternalEncoderProfiles();
	VDSaveFilespecSystemData();
	VDShutdownFilespecSystem();

	if (g_pPortableRegistry) {
		try {
			VDSaveRegistry(g_portableRegistryPath.c_str());
		} catch(const MyError&) {
		}

		VDSetRegistryProvider(NULL);
		delete g_pPortableRegistry;
		g_pPortableRegistry = NULL;
	}

	VDDeletePreferencesShadow();
	VDDeinitResourceSystem();
	VDShutdownEventProfiler();
	VDDeinitProfilingSystem();

	VDCHECKPOINT;
}

///////////////////////////////////////////////////////////////////////////

bool InitApplication(HINSTANCE hInstance) {
	// register controls

	InitCommonControls();

	if (!RegisterClippingControl()) return false;
	if (!RegisterPositionControl()) return false;
	if (!RegisterLevelControl()) return false;
	if (!RegisterHexEditor()) return false;

	ATOM RegisterAudioDisplayControl();
	if (!RegisterAudioDisplayControl()) return false;

	if (!VDRegisterVideoDisplayControl()) return false;
	if (!RegisterFilterGraphControl()) return false;
	if (!RegisterLogWindowControl()) return false;
	if (!RegisterRTProfileDisplayControl()) return false;
	if (!VDRegisterRTProfileDisplayControl2()) return false;
	if (!RegisterVideoWindow()) return false;

	extern bool VDRegisterUIFrameWindow();
	if (!VDRegisterUIFrameWindow()) return false;

	extern bool VDRegisterParameterCurveControl();
	if (!VDRegisterParameterCurveControl()) return false;

	extern bool VDUIRegisterHotKeyExControl();
	if (!VDUIRegisterHotKeyExControl()) return false;

	return true;
}

///////////////////////////////////////////////////////////////////////////

bool InitInstance( HANDLE hInstance, int nCmdShow, bool topmost) {
	VDStringW versionFormat(VDLoadStringW32(IDS_TITLE_INITIAL, true));

	DWORD dwExStyle = topmost ? WS_EX_TOPMOST : 0;

    // Create a main window for this application instance. 
	if (GetVersion() < 0x80000000) {
		g_hWnd = CreateWindowExW(
			dwExStyle,
			(LPCWSTR)VDUIFrame::Class(),
			L"",
			WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,            // Window style.
			CW_USEDEFAULT,                  // Default horizontal position.
			CW_USEDEFAULT,                  // Default vertical position.
			CW_USEDEFAULT,                  // Default width.
			CW_USEDEFAULT,                  // Default height.
			NULL,                           // Overlapped windows have no parent.
			NULL,                           // Use the window class menu.
			g_hInst,                        // This instance owns this window.
			NULL                            // Pointer not needed.
		);
	} else {
		g_hWnd = CreateWindowExA(
			dwExStyle,
			(LPCSTR)VDUIFrame::Class(),
			"",
			WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,            // Window style.
			CW_USEDEFAULT,                  // Default horizontal position.
			CW_USEDEFAULT,                  // Default vertical position.
			CW_USEDEFAULT,                  // Default width.
			CW_USEDEFAULT,                  // Default height.
			NULL,                           // Overlapped windows have no parent.
			NULL,                           // Use the window class menu.
			g_hInst,                        // This instance owns this window.
			NULL                            // Pointer not needed.
		);
	}

    // If window could not be created, return "failure".
    if (!g_hWnd)
        return (FALSE);

	VDUIFrame *pFrame = VDUIFrame::GetFrame(g_hWnd);
	pFrame->SetRegistryName("Main window");

	g_projectui = new VDProjectUI;
	g_project = &*g_projectui;
	g_projectui->Attach((VDGUIHandle)g_hWnd);

    // Make the window visible; update its client area; and return "success".
	pFrame->RestorePlacement(nCmdShow);

	if (nCmdShow != SW_HIDE && !(GetWindowLong(g_hWnd, GWL_STYLE) & WS_VISIBLE))
		ShowWindow(g_hWnd, nCmdShow);

    UpdateWindow(g_hWnd);          

	VDSetWindowTextW32(g_hWnd, versionFormat.c_str());

    return (TRUE);               

}

///////////////////////////////////////////////////////////////////////////

struct ProcessCommandLine {
	bool fExitOnDone;
	bool fHelp;
	bool forceAutoRecoverScan;
	bool captureMode;
	VDStringW captureFile;
	VDStringW openFile;

	ProcessCommandLine() {
		fExitOnDone = false;
		fHelp = false;
		forceAutoRecoverScan = false;
		captureMode = false;
	}
	int scan(const VDCommandLine& cmdLine, bool execute);
	int showHelp();
};

int VDProcessCommandLine(const VDCommandLine& cmdLine) {
	g_szFile[0] = 0;

	ProcessCommandLine cmd;
	int rc = cmd.scan(cmdLine, false);
	if (rc>=0) return rc;
	if (cmd.fHelp) return cmd.showHelp();

	if (cmd.fExitOnDone)
		g_exitOnDone = true;
	rc = cmd.scan(cmdLine, true);
	return rc;
}

int ProcessCommandLine::showHelp() {
	MyError msg(
	//   12345678901234567890123456789012345678901234567890123456789012345678901234567890
	"  /autorecover              Scan for auto-recover files\n"
	"  /b <src-dir> <dst-dir>    Add batch entries for a directory\n"
	"  /blockDebugOutput         Block debug output from specific DLLs\n"
	"         [+/-dllname,...]\n"
	"  /c                        Clear job list\n"
	"  /capture                  Switch to capture mode\n"
	"  /capaudiorec [on|off]     Enable/disable capture audio recording\n"
	"  /capaudioplay [on|off]    Enable/disable capture audio playback\n"
	"  /capchannel <ch> [<freq>] Set capture channel (opt. frequency in MHz)\n"
	"                            Use antenna:<n> or cable:<n> to force mode\n"
	"  /capdevice <devname>      Set capture device\n"
	"  /capfile <filename>       Set capture filename\n"
	"  /capfileinc <filename>    Set capture filename and bump until clear\n"
	"  /capfilealloc <size>      Preallocate capture file in megabytes\n"
	"  /capstart [<time>[s]]     Capture with optional time limit\n"
	"                            (default is minutes, use 's' for seconds)\n"
	"  /cmd <command>            Run quick script command\n"
	"  /F <filter>               Load filter\n"
	"  /edit <instance>          Open video filter configure dialog\n"
	"  /h                        Disable exception filter\n"
	"  /hexedit [<filename>]     Open hex editor\n"
	"  /hexview [<filename>]     Open hex editor (read-only mode)\n"
	"  /i <script> [<args...>]   Invoke script with arguments\n"
	"  /master <file>            Join shared job queue in non-autostart mode\n"
	"  /min                      Start minimized\n"
	"  /max                      Start maximized\n"
	"  /noStupidAntiDebugChecks  Stop lame drivers from screwing up debugging\n"
	"                            sessions\n"
	"  /p <src> <dst>            Add a batch entry for a file\n"
	"  /portable                 Switch to portable settings mode\n"
	"  /priority <pri>           Start in low, belowNormal, normal, aboveNormal,\n"
	"                            high, or realtime priority\n"
	"  /queryVersion             Return build number\n"
	"  /r                        Run job queue\n"
	"  /resetall                 Reset all settings to defaults\n"
	"  /s <script>               Run a script\n"
	"  /safecpu                  Do not use CPU extensions on startup\n"
	"  /slave <file>             Join shared job queue in autostart mode\n"
	"  /topmost                  Create window as always-on-top\n"
	"  /vdxadebug                Enable filter acceleration debug window\n"
	"  /x                        Exit when complete\n"
	);

	if (g_consoleMode) {
		VDLog(kVDLogError, VDTextAToW(msg.gets()));
		return 5;
	} else {
		msg.post(g_hWnd, "Command-line flags:");
		return -1;
	}
}

int ProcessCommandLine::scan(const VDCommandLine& cmdLine, bool execute) {
	static const wchar_t seps[] = L" \t\n\r";

	// parse cmdline looking for switches
	//
	//	/s						run script
	//	/i <script> <params...>	run script with parameters
	//	/c						clear job list
	//	/b<srcdir>,<dstdir>		add directory batch process to job list
	//	/r						run job list
	//	/x						exit when jobs complete
	//	/h						disable crash handler
	//	/fsck					test crash handler
	//	/vtprofile				enable VTune profiling

	int argsFound = 0;
	int rc = -1;

	JobLockDubber();

	VDAutoLogDisplay disp;

	try {
		VDCommandLineIterator it;
		const wchar_t *token;
		bool isSwitch;

		while(cmdLine.GetNextArgument(it, token, isSwitch)) {
			++argsFound;

			if (!isSwitch) {
				if (execute) {
					if (g_capProjectUI)
						g_capProjectUI->SetCaptureFile(token);
					else
						g_project->CmdOpen(token);
				} else {
					if (captureMode)
						captureFile = token;
					else
						openFile = token;
				}
			} else {
				// parse out the switch name
				++token;
				if (!wcscmp(token, L"?")) {
					fHelp = true;
				}
				else if (!wcscmp(token, L"autorecover")) {
					forceAutoRecoverScan = true;
				}
				else if (!wcscmp(token, L"autotest")) {
					if (execute) g_bAutoTest = true;
				}
				else if (!wcscmp(token, L"b")) {
					const wchar_t *path2;

					if (!cmdLine.GetNextNonSwitchArgument(it, token) || !cmdLine.GetNextNonSwitchArgument(it, path2))
						throw MyError("Command line error: syntax is /b <src_dir> <dst_dir>");

					if (execute) JobAddBatchDirectory(token, path2);
				}
				else if (!wcscmp(token, L"blockDebugOutput")) {
					if (!cmdLine.GetNextNonSwitchArgument(it, token))
						throw MyError("Command line error: syntax is /blockDebugOutput <filter>");
					
					if (execute) VDInitDebugOutputFilterW32(VDTextWToA(token).c_str());
				}
				else if (!wcscmp(token, L"c")) {
					if (execute) JobClearList();
				}
				else if (!wcscmp(token, L"capture")) {
					if (execute) {
						VDUIFrame *pFrame = VDUIFrame::GetFrame(g_hWnd);
						pFrame->SetNextMode(1);
						if (!g_capProjectUI)
							throw MyError("Command line error: not in capture mode");
					}
					captureMode = true;
				}
				else if (!wcscmp(token, L"capaudiorec")) {
					if (!captureMode)
						throw MyError("Command line error: not in capture mode");

					bool recognized = false;
					if (cmdLine.GetNextNonSwitchArgument(it, token)) {
						if (!wcscmp(token, L"on") || !wcscmp(token, L"true") || !wcscmp(token, L"yes")) {
							if (execute) g_capProjectUI->SetAudioCaptureEnabled(true);
							recognized = true;
						} else if (!wcscmp(token, L"off") || !wcscmp(token, L"false") || !wcscmp(token, L"no")) {
							if (execute) g_capProjectUI->SetAudioCaptureEnabled(false);
							recognized = true;
						}
					}

					if (!recognized)
						throw MyError("Command line error: syntax is /capaudiorec on|off");
				}
				else if (!wcscmp(token, L"capaudioplay")) {
					if (!captureMode)
						throw MyError("Command line error: not in capture mode");

					bool recognized = false;
					if (cmdLine.GetNextNonSwitchArgument(it, token)) {
						if (!wcscmp(token, L"on") || !wcscmp(token, L"true") || !wcscmp(token, L"yes")) {
							if (execute) g_capProjectUI->SetAudioPlaybackEnabled(true);
							recognized = true;
						} else if (!wcscmp(token, L"off") || !wcscmp(token, L"false") || !wcscmp(token, L"no")) {
							if (execute) g_capProjectUI->SetAudioPlaybackEnabled(false);
							recognized = true;
						}
					}

					if (!recognized)
						throw MyError("Command line error: syntax is /capaudioplay on|off");
				}
				else if (!wcscmp(token, L"capchannel")) {
					if (!captureMode)
						throw MyError("Command line error: not in capture mode");

					if (!cmdLine.GetNextNonSwitchArgument(it, token))
						throw MyError("Command line error: syntax is /capchannel [antenna:|cable:]<channel>");

					if (!wcsncmp(token, L"antenna:", 8)) {
						if (execute) g_capProjectUI->SetTunerInputMode(false);
						token += 8;
					} else if (!wcsncmp(token, L"cable:", 6)) {
						if (execute) g_capProjectUI->SetTunerInputMode(true);
						token += 6;
					}

					if (execute) g_capProjectUI->SetTunerChannel(_wtoi(token));

					if (cmdLine.GetNextNonSwitchArgument(it, token))
						if (execute) g_capProjectUI->SetTunerExactFrequency(VDRoundToInt(wcstod(token, NULL) * 1000000));
				}
				else if (!wcscmp(token, L"capdevice")) {
					if (!captureMode)
						throw MyError("Command line error: not in capture mode");

					if (!cmdLine.GetNextNonSwitchArgument(it, token))
						throw MyError("Command line error: syntax is /capdevice <device>");

					if (execute && !g_capProjectUI->SetDriver(token))
						throw MyError("Unable to initialize capture device: %ls\n", token);
				}
				else if (!wcscmp(token, L"capfile")) {
					if (!captureMode)
						throw MyError("Command line error: not in capture mode");

					if (!cmdLine.GetNextNonSwitchArgument(it, token))
						throw MyError("Command line error: syntax is /capfile <filename>");

					captureFile = token;
					if (execute) g_capProjectUI->SetCaptureFile(token);
				}
				else if (!wcscmp(token, L"capfileinc")) {
					if (!captureMode)
						throw MyError("Command line error: not in capture mode");

					if (!cmdLine.GetNextNonSwitchArgument(it, token))
						throw MyError("Command line error: syntax is /capfileinc <filename>");

					captureFile = token;
					if (execute) {
						g_capProjectUI->SetCaptureFile(token);
						g_capProject->IncrementFileIDUntilClear();
					}
				}
				else if (!wcscmp(token, L"capfilealloc")) {
					if (!captureMode)
						throw MyError("Command line error: not in capture mode");

					if (!cmdLine.GetNextNonSwitchArgument(it, token))
						throw MyError("Command line error: syntax is /capfileinc <filename>");

					unsigned long mbsize = wcstoul(token, NULL, 0);

					if (!mbsize)
						throw MyError("Command line error: invalid size '%ls' for preallocating capture file.", token);

					if (execute) g_capProject->PreallocateCaptureFile((sint64)mbsize << 20);
				}
				else if (!wcscmp(token, L"capstart")) {
					if (!captureMode)
						throw MyError("Command line error: not in capture mode");

					if (cmdLine.GetNextNonSwitchArgument(it, token)) {
						int multiplier = 60;

						if (*token && token[wcslen(token)-1] == L's')
							multiplier = 1;

						int limit = multiplier*_wtoi(token);

						if (execute) g_capProjectUI->SetTimeLimit(limit);
					}

					if (execute) g_capProjectUI->Capture();
				}
				else if (!wcscmp(token, L"changelog")) {
					if (execute) VDDumpChangeLog();
				}
				else if (!wcscmp(token, L"cmd")) {
					if (!cmdLine.GetNextNonSwitchArgument(it, token))
						throw MyError("Command line error: syntax is /cmd <script>");
					VDStringW token2(token);
					const size_t len = token2.size();
					for(int i=0; i<len; ++i)
						if (token2[i] == '\'')
							token2[i] = '"';
					token2 += L';';
					if (execute) RunScriptMemory((char *)VDTextWToA(token2).c_str(), false, true);
				}
				else if (!wcscmp(token, L"fsck")) {
					if (execute) crash();
				}
				else if (!wcscmp(token, L"F")) {
					if (!cmdLine.GetNextNonSwitchArgument(it, token))
						throw MyError("Command line error: syntax is /F <filter>");
					if (execute) {
						VDAddPluginModule(token);
						VDInitInputDrivers();
						VDInitOutputDrivers();
						VDInitAudioEnc();
						VDInitTools();
	
						guiSetStatus("Loaded external module: %s", 255, VDTextWToA(token).c_str());
					}
				}
				else if (!wcscmp(token, L"edit")) {
					if (!cmdLine.GetNextNonSwitchArgument(it, token))
						throw MyError("Command line error: syntax is /edit <instance>");
					if (execute && g_projectui) g_projectui->edit_token = token;
				}
				else if (!wcscmp(token, L"h")) {
					if (execute) SetUnhandledExceptionFilter(NULL);
				}
				else if (!wcscmp(token, L"hexedit")) {
					if (cmdLine.GetNextNonSwitchArgument(it, token))
						if (execute) HexEdit(NULL, token, false);
					else
						if (execute) HexEdit(NULL, NULL, false);
				}
				else if (!wcscmp(token, L"hexview")) {
					if (cmdLine.GetNextNonSwitchArgument(it, token))
						if (execute) HexEdit(NULL, token, true);
					else
						if (execute) HexEdit(NULL, NULL, true);
				}
				else if (!wcscmp(token, L"i")) {
					const wchar_t *filename;

					if (!cmdLine.GetNextNonSwitchArgument(it, filename))
						throw MyError("Command line error: syntax is /i <script> [<args>...]");

					if (execute) {
						g_VDStartupArguments.clear();
						while(cmdLine.GetNextNonSwitchArgument(it, token))
							g_VDStartupArguments.push_back(VDTextWToU8(VDStringW(token)));

						RunScript(filename);
					} else {
						while(cmdLine.GetNextNonSwitchArgument(it, token));
					}
				}
				else if (!wcscmp(token, L"lockd3d")) {
					// PerfHUD doesn't like it when you keep loading and unloading the device.
					class D3DLock : public VDD3D9Client {
					public:
						D3DLock() {
							mpMgr = VDInitDirect3D9(this, NULL, false);
						}

						~D3DLock() {
							if (mpMgr)
								VDDeinitDirect3D9(mpMgr, this);
						}

						void OnPreDeviceReset() {}
						void OnPostDeviceReset() {}

					protected:
						VDD3D9Manager *mpMgr;
					};
					if (execute) {
						static D3DLock sD3DLock;
					}
				}
				else if (!wcscmp(token, L"master")) {
					if (!cmdLine.GetNextNonSwitchArgument(it, token))
						throw MyError("Command line error: syntax is /master <queue-file>");

					if (execute) JobSetQueueFile(token, true, false);
				}
				else if (!wcscmp(token, L"noStupidAntiDebugChecks")) {
					// Note that this actually screws our ability to call IsDebuggerPresent() as well,
					// not that we care.

					if (execute) {
						HMODULE hmodKernel32 = GetModuleHandleA("kernel32");
						FARPROC fpIDP = GetProcAddress(hmodKernel32, "IsDebuggerPresent");

						DWORD oldProtect;
						if (VirtualProtect(fpIDP, 3, PAGE_EXECUTE_READWRITE, &oldProtect)) {
							static const uint8 patch[]={
								0x33, 0xC0,				// XOR EAX, EAX
								0xC3,					// RET
							};
							memcpy(fpIDP, patch, 3);
							VirtualProtect(fpIDP, 3, oldProtect, &oldProtect);
						}
					}
				}
				else if (!wcscmp(token, L"p")) {
					const wchar_t *path2;

					if (!cmdLine.GetNextNonSwitchArgument(it, token) || !cmdLine.GetNextNonSwitchArgument(it, path2))
						throw MyError("Command line error: syntax is /p <src_file> <dst_file>");

					if (execute) JobAddBatchFile(token, path2);
				}
				else if (!wcscmp(token, L"priority")) {
					if (!cmdLine.GetNextNonSwitchArgument(it, token))
						throw MyError("Command line error: syntax is /priority <priority>");

					DWORD priority = NORMAL_PRIORITY_CLASS;
					if (!wcscmp(token, L"normal"))
						priority = NORMAL_PRIORITY_CLASS;
					else if (!wcscmp(token, L"aboveNormal"))
						priority = ABOVE_NORMAL_PRIORITY_CLASS;
					else if (!wcscmp(token, L"belowNormal"))
						priority = BELOW_NORMAL_PRIORITY_CLASS;
					else if (!wcscmp(token, L"high"))
						priority = HIGH_PRIORITY_CLASS;
					else if (!wcscmp(token, L"low"))
						priority = IDLE_PRIORITY_CLASS;
					else if (!wcscmp(token, L"realtime"))
						priority = REALTIME_PRIORITY_CLASS;
					else
						throw MyError("Command line error: unknown priority '%ls'", token);

					if (execute) SetPriorityClass(GetCurrentProcess(), priority);

				}
				else if (!wcscmp(token, L"queryVersion")) {
					rc = version_num;
					break;
				}
				else if (!wcscmp(token, L"r")) {
					if (execute) {
						JobUnlockDubber();
						bool success = JobRunList();
						JobLockDubber();

						if (!success)
							break;
					}
				}
				else if (!wcscmp(token, L"s")) {
					if (!cmdLine.GetNextNonSwitchArgument(it, token))
						throw MyError("Command line error: syntax is /s <script>");

					if (execute) RunScript(token);
				}
				else if (!wcscmp(token, L"safecpu")) {
					// already handled elsewhere
				}
				else if (!wcscmp(token, L"slave")) {
					if (!cmdLine.GetNextNonSwitchArgument(it, token))
						throw MyError("Command line error: syntax is /slave <queue-file>");

					if (execute) {
						JobSetQueueFile(token, true, true);

						if (!g_consoleMode) {
							OpenJobWindow();
							ShowWindow(g_hWnd, SW_SHOWMINNOACTIVE);
						} else {
							VDLog(kVDLogInfo, VDswprintf(L"Joining shared job queue in slave mode: %ls", 1, &token));
						}
					}

					fExitOnDone = false;
				}
				else if (!wcscmp(token, L"vtprofile")) {
					if (execute) g_bEnableVTuneProfiling = true;
				}
				else if (!wcscmp(token, L"w")) {
					if (execute) g_fWine = true;
				}
				else if (!wcscmp(token, L"vdxadebug")) {
					if (execute) VDPreferencesSetFilterAccelVisualDebugEnabled(true);
				}
				else if (!wcscmp(token, L"limitmem")) {
					if (execute) for(int i=0; i<6; ++i)
						VirtualAlloc(NULL, 256 * 1024 * 1024, MEM_RESERVE, PAGE_READWRITE);
				}
				else if (!wcscmp(token, L"x")) {
					fExitOnDone = true;

					// don't count the /x flag as an argument that does work
					--argsFound;
				} else
					throw MyError("Command line error: unknown switch %ls", token);
			}
		}

		if (!argsFound && g_consoleMode)
			throw MyError(
				"This application allows usage of VirtualDub from the command line. To use\n"
				"the program interactively, launch " VD_PROGRAM_EXEFILE_NAMEA " directly.\n"
				"\n"
				"Usage: " VD_PROGRAM_CLIEXE_NAMEA " ( /<switches> | video-file ) ...\n"
				"       " VD_PROGRAM_CLIEXE_NAMEA " /? for help\n");

		if (!g_consoleMode && !fExitOnDone)
			disp.Post((VDGUIHandle)g_hWnd);
	} catch(const MyUserAbortError&) {
		if (g_consoleMode) {
			VDLog(kVDLogInfo, VDStringW(L"Operation was aborted by user."));

			rc = 1;
		} else if (g_exitOnDone) {
			// WM_CLOSE behavior: terminate in unsafe way and exit with code 1000
			// normal abort way: terminate safely and return 1
			rc = 1;
		}
	} catch(const MyError& e) {
		if (g_consoleMode) {
			const char *err = e.gets();

			if (err)
				VDLog(kVDLogError, VDTextAToW(err));

			rc = 5;
		} else
			e.post(g_hWnd, g_szError);
	}
	JobUnlockDubber();

	if (rc >= 0)
		return rc;

	if (!execute)
		return -1;

	if (fExitOnDone)
		return 0;

	if ((!argsFound && !g_consoleMode && VDPreferencesGetAutoRecoverEnabled()) || forceAutoRecoverScan) {
		VDUICheckForAutoRecoverFiles((VDGUIHandle)g_hWnd);
	}

	return -1;
}
