#include <windows.h>
#include <malloc.h>
#include <tchar.h>
#include <stdio.h>

#ifdef _M_AMD64
	#define APPNAME "VirtualDub64.exe"
#else
	#define APPNAME "VirtualDub.exe"
#endif

struct CopyHandles {
	CRITICAL_SECTION *pWriteLock;
	HANDLE hRead;
	HANDLE hWrite;
};

DWORD WINAPI CopyThread(LPVOID p) {
	CopyHandles ch = *(const CopyHandles *)p;
	char buf[256];

	for(;;) {
		DWORD actual;
		if (!ReadFile(ch.hRead, buf, 256, &actual, NULL))
			break;

		if (ch.pWriteLock)
			EnterCriticalSection(ch.pWriteLock);
		BOOL writeOk = WriteFile(ch.hWrite, buf, actual, &actual, NULL);
		if (ch.pWriteLock)
			LeaveCriticalSection(ch.pWriteLock);
		if (!writeOk)
			break;
	}

	return 0;
}

DWORD g_dwAppThread;
CRITICAL_SECTION g_writeLock;
bool g_bAbortCaught;

BOOL WINAPI CtrlHandler(DWORD dwCtrlType) {
	static const char abortMsg[]="Attempting to abort, please wait....\r\n";
	EnterCriticalSection(&g_writeLock);
	DWORD actual;
	WriteFile(GetStdHandle(STD_ERROR_HANDLE), abortMsg, sizeof(abortMsg)-1, &actual, NULL);
	LeaveCriticalSection(&g_writeLock);

	g_bAbortCaught = true;

	if (g_dwAppThread) {
		PostThreadMessage(g_dwAppThread, WM_QUIT, 0, 0);
		return TRUE;
	}

	return FALSE;
}

BOOL WINAPI SilentCtrlHandler(DWORD dwCtrlType) {
	g_bAbortCaught = true;
	return TRUE;
}

bool WereWeRunFromExplorer() {
	// try to guess whether we were run from Explorer:
	//
	// 1) cursor starts at top-left
	//
	CONSOLE_SCREEN_BUFFER_INFO conInfo;
	if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &conInfo))
		return false;

	if (conInfo.dwCursorPosition.X || conInfo.dwCursorPosition.Y)
		return false;

	return true;
}

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	TCHAR exepath[MAX_PATH];
	TCHAR exepath2[MAX_PATH];
	TCHAR *fname;

	GetModuleFileName(NULL, exepath, MAX_PATH);
	GetFullPathName(exepath, MAX_PATH, exepath2, &fname);
	lstrcpy(fname, _T(APPNAME));

	HANDLE hOutputPipeRead;
	HANDLE hOutputPipeWrite;
	HANDLE hErrorPipeRead = NULL;
	HANDLE hErrorPipeWrite = NULL;
	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	HANDLE hStdError = GetStdHandle(STD_ERROR_HANDLE);

	bool bRunFromExplorer = WereWeRunFromExplorer();

	SECURITY_ATTRIBUTES sa={
		sizeof(SECURITY_ATTRIBUTES), NULL, TRUE
	};

	if (!CreatePipe(&hOutputPipeRead, &hOutputPipeWrite, &sa, 0))
		return 5;

	if (hStdError != hStdOut) {
		if (!CreatePipe(&hErrorPipeRead,  &hErrorPipeWrite, &sa, 0))
			return 5;
	}

	InitializeCriticalSection(&g_writeLock);

	CopyHandles chOutput = { &g_writeLock, hOutputPipeRead, hStdOut };
	CopyHandles chError = { &g_writeLock, hErrorPipeRead, hStdError };

	DWORD dwOutputThread, dwErrorThread;
	HANDLE hOutputThread = CreateThread(NULL, 65536, CopyThread, &chOutput, 0, &dwOutputThread);
	HANDLE hErrorThread = NULL;
	
	if (hErrorPipeRead)
		hErrorThread = CreateThread(NULL, 65536, CopyThread, &chError, 0, &dwErrorThread);

	// create command line
	LPTSTR cmdLine = GetCommandLine();
	bool quoted = false;
	while(const TCHAR c = *cmdLine) {
		if (c == '"') {
			quoted = !quoted;
			++cmdLine;
			continue;
		}

		if (!quoted && (c == ' ' || c == '\t'))
			break;

		++cmdLine;
	}

	// check if we have anything worthy after the command line; if so, don't do
	// the pause-when-run-from-Explorer, as we may have been launched from a batch
	// file or something else with an empty screen
	LPCTSTR tailEnd = cmdLine;
	while(const TCHAR c = *tailEnd) {
		if (c != ' ' && c != '\t')
			break;
		++tailEnd;
	}

	if (*tailEnd)
		bRunFromExplorer = false;

	static const TCHAR magicSwitch[] = _T(" /console /x");
	static const size_t magicSwitchLen = sizeof magicSwitch / sizeof magicSwitch[0] - 1;
	size_t cmdLineLen = lstrlen(cmdLine);
	size_t appNameLen = lstrlen(exepath2);
	LPTSTR newCmdLine = (LPTSTR)alloca(sizeof(TCHAR) * (cmdLineLen + appNameLen + magicSwitchLen + 3));
	LPTSTR p = newCmdLine;

	*p++ = '"';
	memcpy(p, exepath2, sizeof(TCHAR) * appNameLen);
	p += appNameLen;
	*p++ = '"';
	memcpy(p, magicSwitch, sizeof(TCHAR) * magicSwitchLen);
	p += magicSwitchLen;
	memcpy(p, cmdLine, sizeof(TCHAR) * (cmdLineLen + 1));

	// create Ctrl+C handler
	SetConsoleCtrlHandler(CtrlHandler, TRUE);

	// kill error dialogs
	UINT dwOldErrorMode = SetErrorMode(0);
	SetErrorMode(dwOldErrorMode | SEM_FAILCRITICALERRORS);

	// launch main VirtualDub exe
	STARTUPINFO si = {sizeof(STARTUPINFO)};
	si.wShowWindow	= SW_SHOWMINNOACTIVE;
	si.hStdInput	= CreateFile(_T("nul"), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	si.hStdOutput	= hOutputPipeWrite;
	si.hStdError	= hErrorPipeWrite;
	si.dwFlags		= STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	PROCESS_INFORMATION pi;

	DWORD rc = 20;

	if (CreateProcess(exepath2, newCmdLine, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
		CloseHandle(pi.hThread);

		g_dwAppThread = pi.dwThreadId;
		WaitForSingleObject(pi.hProcess, INFINITE);
		g_dwAppThread = 0;

		GetExitCodeProcess(pi.hProcess, &rc);

		CloseHandle(pi.hProcess);
	} else {
		TCHAR *msg;

		if (FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, GetLastError(), 0, (LPTSTR)&msg, 0, NULL)) {
			static const TCHAR pretext[]=_T("Cannot launch ") _T(APPNAME) _T(": ");
			DWORD actual;
			WriteFile(hStdOut, pretext, sizeof pretext / sizeof pretext[0] - 1, &actual, NULL);
			WriteFile(hStdOut, msg, lstrlen(msg), &actual, NULL);
			LocalFree((HLOCAL)msg);
		}
	}

	if (g_bAbortCaught && !rc)
		rc = 5;

	// close pipes first to break any blocking console I/O call in the main
	// app
	CloseHandle(hOutputPipeWrite);
	if (hErrorPipeWrite)
		CloseHandle(hErrorPipeWrite);

	// wait for VirtualDub to terminate; use alertable wait so we see Ctrl+C
	WaitForSingleObjectEx(hOutputThread, INFINITE, TRUE);

	// swap Ctrl+C handlers
	SetConsoleCtrlHandler(CtrlHandler, FALSE);
	SetConsoleCtrlHandler(SilentCtrlHandler, TRUE);

	if (hErrorThread)
		WaitForSingleObject(hErrorThread, INFINITE);

	CloseHandle(hOutputThread);
	if (hErrorThread)
		CloseHandle(hErrorThread);

	CloseHandle(hOutputPipeRead);
	if (hErrorPipeRead)
		CloseHandle(hErrorPipeRead);

	DeleteCriticalSection(&g_writeLock);

	// if we were launched from Explorer, wait for close
	if (bRunFromExplorer) {
		while(!g_bAbortCaught)
			SleepEx(100, TRUE);
	}

	return (int)rc;
}
