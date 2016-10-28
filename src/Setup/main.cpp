#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <malloc.h>
#include <ctype.h>
#include <crtdbg.h>

#include <windows.h>
#include <commctrl.h>

#include "resource.h"
#include "registry.h"

HWND g_hwnd;
HINSTANCE g_hInst; // current instance
char szAppName[] = "VirtualDub Setup Class"; // The name of this application
char szTitle[]   = ""; // The title bar text
char g_szWinPath[MAX_PATH];
char g_szProgPath[MAX_PATH];
char g_szTempPath[MAX_PATH];

///////////////////

BOOL Init(HINSTANCE, int);
LRESULT APIENTRY WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

BOOL APIENTRY InstallAVIFileDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL APIENTRY UninstallAVIFileDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL APIENTRY RemoveSettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL APIENTRY AboutDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

///////////

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow )
{
	MSG msg;
	char *lpszFilePart;

	///////////

	if (!GetWindowsDirectory(g_szWinPath, sizeof g_szWinPath)) return FALSE;
	if (!GetModuleFileName(NULL, g_szTempPath, sizeof g_szTempPath))
		return FALSE;
	if (!GetFullPathName(g_szTempPath, sizeof g_szProgPath, g_szProgPath, &lpszFilePart))
		return FALSE;
	*lpszFilePart=0;

	if (!Init(hInstance, nCmdShow)) return FALSE;

	// Main message loop.

	while (GetMessage(&msg, NULL, 0, 0)) {
//		if (!IsDialogMessage(g_hwnd, &msg))
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

////////////////

BOOL Init(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASS  wc;

    // Register the window class for my window.                                                           */
    wc.style = 0;                       // Class style.
    wc.lpfnWndProc = (WNDPROC)WndProc; // Window procedure for this class.
    wc.cbClsExtra = 0;                  // No per-class extra data.
    wc.cbWndExtra = DLGWINDOWEXTRA;                  // No per-window extra data.
    wc.hInstance = hInstance;           // Application that owns the class.
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE+1); 
    wc.lpszMenuName =  NULL;   // Name of menu resource in .RC file. 
    wc.lpszClassName = szAppName; // Name used in call to CreateWindow.

    if (!RegisterClass(&wc)) return FALSE;

	g_hInst = hInstance; // Store instance handle in our global variable

	g_hwnd = CreateDialog(hInstance,MAKEINTRESOURCE(IDD_MAINWINDOW),NULL,(DLGPROC)NULL);

	if (!g_hwnd) {
		return (FALSE);
	}

	ShowWindow(g_hwnd, nCmdShow);
	UpdateWindow(g_hwnd);

	return (TRUE);
}

void PrintfWindowText(HWND hWnd, char *format, ...) {
	char buf[256];
	va_list val;

	va_start(val, format);
	vsprintf(buf, format, val);
	va_end(val);
	SetWindowText(hWnd, buf);
}

LRESULT APIENTRY WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	switch (message) { 
		case WM_DESTROY:
			PostQuitMessage(0);
			break;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_EXECUTE:
				if ((int)ShellExecute(hWnd, "open", "VirtualDub.exe", NULL, NULL, SW_SHOWNORMAL) <= 32)
					MessageBox(hWnd, "Couldn't launch VirtualDub.exe.", "Oops", MB_OK);
				break;
			case IDC_INSTALL:
				DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ATTEMPT), hWnd, (DLGPROC)InstallAVIFileDlgProc);
				break;
			case IDC_UNINSTALL:
				DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ATTEMPT), hWnd, (DLGPROC)UninstallAVIFileDlgProc);
				break;
			case IDC_REMOVE:
				DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ATTEMPT), hWnd, (DLGPROC)RemoveSettingsDlgProc);
				break;
			case IDC_ABOUT:
				DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUT), hWnd, (DLGPROC)AboutDlgProc);
				break;
			case IDCANCEL:
				DestroyWindow(hWnd);
				break;
			}
            break;

		default:
			return DefWindowProc(hWnd,message,wParam,lParam);
	}
	return (0);
}

//////////////////////////////////////////////////////////////////////

void ListboxAddf(HWND hwndListbox, char *format, ...) {
	char buf[256];
	va_list val;

	va_start(val, format);
	_vsnprintf(buf, sizeof buf, format, val);
	va_end(val);
	buf[(sizeof buf) - 1] = 0;

	SendMessage(hwndListbox, LB_ADDSTRING, 0, (LPARAM)buf);
}

BOOL InstallFile(char *szSource, char *szDestFormat, ...) {
	char szDest[MAX_PATH];
	char szDestPath[MAX_PATH];
	char szDestFile[MAX_PATH];
	char *lpszDestFile;
	char szCurInst[MAX_PATH];
	char szTempFile[MAX_PATH];
	DWORD dwFlags = VIFF_DONTDELETEOLD;
	DWORD dwRet;
	UINT uTmpLen;
	char szFailure[256];
	va_list val;

	va_start(val, szDestFormat);
	vsprintf(szDest, szDestFormat, val);
	va_end(val);

	if (!GetFullPathName(szDest, sizeof szDestPath, szDestPath, &lpszDestFile))
		return FALSE;

	strcpy(szDestFile, lpszDestFile);
	*lpszDestFile=0;

	do {
		szTempFile[0]=0;
		szCurInst[0]=0;
		uTmpLen = sizeof szTempFile;
		dwRet = VerInstallFile(dwFlags, szSource, szDestFile, g_szProgPath, szDestPath, szCurInst, szTempFile, &uTmpLen);

		if (dwRet & VIF_TEMPFILE) {
			DeleteFile(szTempFile);
			dwRet &= ~VIF_TEMPFILE;
		}

		szFailure[0]=0;

		if (dwRet & (VIF_MISMATCH | VIF_DIFFTYPE))
			sprintf(szFailure,	"The old %s doesn't look like a VirtualDub file.\n"
								"If it belongs to another application, installing the new file may cause "
								"the other app to stop functioning.\n"
								"Install the new file only if you are sure or have a backup."
								,szDestFile);
		else if (dwRet & VIF_SRCOLD)
			sprintf(szFailure,	"%s is older than the %s being installed over.\n"
								"You should install the older %s if you do not use other versions "
								"of VirtualDub, since the newer file may be incompatible."
								,szSource,szDestFile,szSource);
		else if (dwRet & VIF_WRITEPROT)
			sprintf(szFailure,	"The %s being installed over has been marked read-only.\n"
								"Override read-only attribute and install anyway?"
								,szDestFile);
		else if (dwRet & VIF_FILEINUSE)
			sprintf(szFailure,	"%s is in use.  It cannot be installed over right now.\n"
								"If you have any copies of VirtualDub or any programs that "
								"may be using VirtualDub's AVIFile handler, please close them "
								"and then click OK to retry the operation."
								,szDestFile);
		else if (dwRet & VIF_OUTOFSPACE)
			sprintf(szFailure,	"Doh! We're out of space trying to write:\n\t%s\n\nCan you clear some up?"
								,szDest);
		else if (dwRet & VIF_ACCESSVIOLATION)
			sprintf(szFailure,	"Access violation.  Check with your administrator to see if you have "
								"the appropriate permissions to write to \"%s\"."
								,szDest);
		else if (dwRet & VIF_SHARINGVIOLATION)
			sprintf(szFailure,	"Share violation; something else probably has %s open.  Try closing applications that "
								"have the file open, and check permissions on network drives."
								,szDestFile);
		else if (dwRet & VIF_CANNOTCREATE)
			sprintf(szFailure,	"Couldn't create temporary file %s.\nTry again?", szTempFile);
		else if (dwRet & VIF_CANNOTDELETE)
			sprintf(szFailure,	"Couldn't delete temporary file %s.\nTry installing again?", szTempFile);
		else if (dwRet & VIF_CANNOTDELETECUR)
			sprintf(szFailure,	"Couldn't delete existing file \"%s\".\nTry installing again?", szDest);
		else if (dwRet & VIF_CANNOTRENAME)
			sprintf(szFailure,	"Deleted old file %s, but couldn't move %s into its place.\n"
								"You should retry this operation.", szDestFile, szSource);
		else if (dwRet & VIF_CANNOTREADSRC)
			sprintf(szFailure,	"Couldn't read source file \"%s\".  Should I try again?"
								,szSource);
		else if (dwRet & VIF_CANNOTREADDST)
			sprintf(szFailure,	"Couldn't read destination file \"%s\".  I can try installing over it "
								"anyway, though."
								,szDest);
		else if (dwRet & VIF_OUTOFMEMORY)
			sprintf(szFailure,	"Ran out of memory!  Try freeing some up.");
		else if (dwRet)
			sprintf(szFailure,	"Unidentified error copying:\n\t%s\n\t\tto\n\t%s\n\nTry forcing install?"
								,szSource
								,szDest);

		if (szFailure[0])
			if (IDNO==MessageBox(NULL, szFailure, "Install error", MB_YESNO | MB_APPLMODAL))
				return FALSE;

		dwFlags |= VIFF_FORCEINSTALL;
	} while(dwRet);

	return TRUE;
}

BOOL InstallRegStr(HKEY hkBase, char *szKeyName, char *szName, char *szValue) {
	char buf[256];

	if (!SetRegString(hkBase, szKeyName, szName, szValue)) {
		sprintf(buf,"Couldn't set registry key %s\\%s",szKeyName,szName?szName:"(default)");
		MessageBox(NULL, buf, "Install error", MB_OK);
		return FALSE;
	}

	return TRUE;
}

BOOL InstallDeleteFile(char *szFileFormat, ...) {
	char szFile[256];
	va_list val;

	va_start(val, szFileFormat);
	vsprintf(szFile, szFileFormat, val);
	va_end(val);

	if (!DeleteFile(szFile))
		if (GetLastError() != ERROR_FILE_NOT_FOUND)
			return FALSE;

	return TRUE;
}


///////////////////////////////////////

BOOL APIENTRY InstallAVIFileDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_INITDIALOG:
			{
				HWND hwndListbox = GetDlgItem(hDlg, IDC_ACTIONLIST);

				SetWindowText(hDlg, "Install AVIFile frameclient");

				ListboxAddf(hwndListbox, "Copy VDREMOTE.DLL to %s\\SYSTEM\\VDREMOTE.DLL", g_szWinPath);
				ListboxAddf(hwndListbox, "Copy VDSRVLNK.DLL to %s\\SYSTEM\\VDSRVLNK.DLL", g_szWinPath);
				ListboxAddf(hwndListbox, "Add VDRemote class and AVIFile entries to Registry");
			}
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				if (	InstallFile("vdremote.dll","%s\\system\\vdremote.dll",g_szWinPath)
					&&	InstallFile("vdsvrlnk.dll","%s\\system\\vdsvrlnk.dll",g_szWinPath)
					&&	InstallRegStr(HKEY_CLASSES_ROOT,"CLSID\\{894288e0-0948-11d2-8109-004845000eb5}",NULL,"VirtualDub link handler")
					&&	InstallRegStr(HKEY_CLASSES_ROOT,"CLSID\\{894288e0-0948-11d2-8109-004845000eb5}\\InprocServer32",NULL,"vdremote.dll")
					&&	InstallRegStr(HKEY_CLASSES_ROOT,"CLSID\\{894288e0-0948-11d2-8109-004845000eb5}\\InprocServer32","ThreadingModel","Apartment")
					&&	InstallRegStr(HKEY_CLASSES_ROOT,"CLSID\\{894288e0-0948-11d2-8109-004845000eb5}\\InprocServer32\\AVIFile",NULL,"1")
					&&	InstallRegStr(HKEY_CLASSES_ROOT,"AVIFile\\Extensions\\VDR",NULL,"{894288e0-0948-11d2-8109-004845000eb5}")
					&&	InstallRegStr(HKEY_CLASSES_ROOT,"AVIFile\\RIFFHandlers\\VDRM",NULL,"{894288e0-0948-11d2-8109-004845000eb5}")
					)

					MessageBox(hDlg, "AVIFile frameclient install successful.", "VirtualDub Setup", MB_OK);
				else
					MessageBox(hDlg, "AVIFile frameclient install failed.", "VirtualDub Setup", MB_OK);

				EndDialog(hDlg, TRUE);
				return TRUE;
			case IDCANCEL:
				EndDialog(hDlg, FALSE);
				return TRUE;
			}
	}
	return FALSE;
}

BOOL APIENTRY UninstallAVIFileDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	BOOL fSuccess;

	switch(msg) {
		case WM_INITDIALOG:
			{
				HWND hwndListbox = GetDlgItem(hDlg, IDC_ACTIONLIST);

				SetWindowText(hDlg, "Uninstall AVIFile frameclient");

				ListboxAddf(hwndListbox, "Delete %s\\SYSTEM\\VDREMOTE.DLL", g_szWinPath);
				ListboxAddf(hwndListbox, "Delete %s\\SYSTEM\\VDSRVLNK.DLL", g_szWinPath);
				ListboxAddf(hwndListbox, "Remove VDRemote class and AVIFile entries from Registry");
			}
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				fSuccess =  InstallDeleteFile("%s\\system\\vdremote.dll",g_szWinPath);
				fSuccess &= InstallDeleteFile("%s\\system\\vdsvrlnk.dll",g_szWinPath);

				if (	ERROR_SUCCESS != RegDeleteKey(HKEY_CLASSES_ROOT,"Clsid\\{894288e0-0948-11d2-8109-004845000eb5}\\InprocServer32\\AVIFile")
					||	ERROR_SUCCESS != RegDeleteKey(HKEY_CLASSES_ROOT,"Clsid\\{894288e0-0948-11d2-8109-004845000eb5}\\InprocServer32")
					||	ERROR_SUCCESS != RegDeleteKey(HKEY_CLASSES_ROOT,"Clsid\\{894288e0-0948-11d2-8109-004845000eb5}")
					||	ERROR_SUCCESS != RegDeleteKey(HKEY_CLASSES_ROOT,"AVIFile\\Extensions\\VDR")
					||	ERROR_SUCCESS != RegDeleteKey(HKEY_CLASSES_ROOT,"AVIFile\\RIFFHandlers\\VDRM"))

					MessageBox(hDlg, "Registry entries were in use.  Deinstall not successful.\n"
									"\n"
									"A partial installation now exists on your system.  Reinstall the AVIFile "
									"handler to restore frameclient functionality, or close applications that may "
									"be occupying the Registry entries and retry the deinstall."
									,"VirtualDub Setup",MB_OK);

				else if (!fSuccess)
					MessageBox(hDlg, "DLL files were in use.  Deinstall not successful.\n"
									"\n"
									"A partial installation now exists on your system.  Reinstall the AVIFile "
									"handler to restore frameclient functionality, or close applications that may "
									"be occupying the shared DLLs and retry the deinstall."
									,"VirtualDub Setup",MB_OK);
				else
					MessageBox(hDlg, "AVIFile frameclient deinstall successful.", "VirtualDub Setup", MB_OK);

				EndDialog(hDlg, TRUE);
				return TRUE;
			case IDCANCEL:
				EndDialog(hDlg, FALSE);
				return TRUE;
			}
	}
	return FALSE;
}

///////////////////////////////////////////////

void RemoveVirtualDubKeys(HWND hwndListbox) {
	FILETIME ftModified;
	int i;
	LONG err;
	char szKeyName[MAX_PATH];
	char szErrorText[128];
	DWORD dwKeyNameLen;

	SendMessage(hwndListbox, LB_RESETCONTENT, 0, 0);

	i=0;
	for(;;) {
		dwKeyNameLen = sizeof szKeyName;
		err = RegEnumKeyEx(HKEY_USERS, i++, szKeyName, &dwKeyNameLen, 0, NULL, 0, &ftModified);

		if (err == ERROR_NO_MORE_ITEMS)
			break;
		else if (err == ERROR_SUCCESS) {
			HKEY hkeyUser;
			char *bp = szKeyName + strlen(szKeyName);
			
			err = RegOpenKeyEx(HKEY_USERS, szKeyName, 0, KEY_ALL_ACCESS, &hkeyUser);
			if (err != ERROR_SUCCESS) {
				FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, szErrorText, sizeof szErrorText, NULL);
				ListboxAddf(hwndListbox, "HKEY_USERS\\%s: %s", szKeyName, szErrorText);
				continue;
			}

			strcpy(bp, "\\Software\\Freeware\\VirtualDub\\Capture");

			err = RegDeleteKey(HKEY_USERS,szKeyName);
			if (err != ERROR_SUCCESS && err != ERROR_FILE_NOT_FOUND)
				FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, szErrorText, sizeof szErrorText, NULL);
			else
				strcpy(szErrorText, "Deleted");
			ListboxAddf(hwndListbox, "HKEY_USERS\\%s: %s", szKeyName, szErrorText);

			strcpy(bp, "\\Software\\Freeware\\VirtualDub\\MRUList");

			err = RegDeleteKey(HKEY_USERS,szKeyName);
			if (err != ERROR_SUCCESS && err != ERROR_FILE_NOT_FOUND)
				FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, szErrorText, sizeof szErrorText, NULL);
			else
				strcpy(szErrorText, "Deleted");
			ListboxAddf(hwndListbox, "HKEY_USERS\\%s: %s", szKeyName, szErrorText);

			bp[29]=0;

			err = RegDeleteKey(HKEY_USERS,szKeyName);
			if (err != ERROR_SUCCESS && err != ERROR_FILE_NOT_FOUND)
				FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, szErrorText, sizeof szErrorText, NULL);
			else
				strcpy(szErrorText, "Deleted");
			ListboxAddf(hwndListbox, "HKEY_USERS\\%s: %s", szKeyName, szErrorText);

			RegCloseKey(hkeyUser);
		}
	}
}

BOOL APIENTRY RemoveSettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_INITDIALOG:
			{
				HWND hwndListbox = GetDlgItem(hDlg, IDC_ACTIONLIST);

				SetWindowText(hDlg, "Remove VirtualDub preference data");

				ListboxAddf(hwndListbox, "Remove HKEY_USERS\\*\\Software\\Freeware\\VirtualDub\\*");
			}
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				SetWindowText(GetDlgItem(hDlg, IDC_ACTION), "Results:");

				RemoveVirtualDubKeys(GetDlgItem(hDlg, IDC_ACTIONLIST));

				SetWindowText(GetDlgItem(hDlg, IDOK), "Retry");
				SetWindowText(GetDlgItem(hDlg, IDCANCEL), "Done");
				return TRUE;
			case IDCANCEL:
				EndDialog(hDlg, FALSE);
				return TRUE;
			}
	}
	return FALSE;
}


///////////////////////////////////////////////////////////////////////////
//
//	About...
//
///////////////////////////////////////////////////////////////////////////

BOOL APIENTRY AboutDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_INITDIALOG:
			SetDlgItemText(hDlg, IDC_FINALS_SUCK,
#ifdef _DEBUG
				"Debug build"
#else
				"Release build"
#endif
				" (" __DATE__ " " __TIME__ ")");

			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
			case IDCANCEL:
				EndDialog(hDlg, FALSE);
				return TRUE;
			}
	}
	return FALSE;
}
