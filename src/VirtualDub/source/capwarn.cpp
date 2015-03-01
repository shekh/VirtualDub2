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
#include <vd2/system/registry.h>
#include "oshelper.h"
#include "helpfile.h"

extern const char g_szCapture[];
static const char g_szCaptureWarn[]="Disabled Warnings";

#define CWF_PINNACLE		(0x00000001L)
#define CWF_ZORAN			(0x00000002L)
#define CWF_BROOKTREE		(0x00000004L)

static long g_capwarnFlags = -1;

static void CaptureWarnInit() {
	if (g_capwarnFlags < 0) {
		VDRegistryAppKey key(g_szCapture);

		g_capwarnFlags = key.getInt(g_szCaptureWarn, 0);
	}
}

static void CaptureWarnDisable(long f) {
	g_capwarnFlags |= f;

	VDRegistryAppKey key(g_szCapture);
	key.setInt(g_szCaptureWarn, g_capwarnFlags);
}

void CaptureWarnCheckDriver(HWND hwnd, const char *s) {
	CaptureWarnInit();

	if (!(g_capwarnFlags & CWF_PINNACLE) && (strstr(s, "Pinnacle") || strstr(s, "miroVIDEO"))) {

		if (IDYES == MessageBox(hwnd,
			"You may experience slow GUI performance while this driver "
			"is active, depending on your video card.  Do you want to "
			"know more about this problem?"
			,
			"Miro/Pinnacle Systems driver detected",
			MB_YESNO)) {

			VDShowHelp(hwnd, L"capwarn.html");
		}

		CaptureWarnDisable(CWF_PINNACLE);
	}
}

void CaptureWarnCheckDrivers(HWND hwnd) {
	HANDLE hFind;
	WIN32_FIND_DATA wfd;
	char szPath[MAX_PATH], *s;

	if (!(g_capwarnFlags & CWF_ZORAN)) {
		GetWindowsDirectory(szPath, sizeof szPath);
		s = szPath;
		while(*s) ++s;
		if (s[-1] != '\\')
			*s++ = '\\';

		strcpy(s, "system\\h20capt.dll");

		hFind = FindFirstFile(szPath, &wfd);

		if (hFind == INVALID_HANDLE_VALUE) {
			strcpy(s, "system\\h22capt.dll");

			hFind = FindFirstFile(szPath, &wfd);
		}

		if (hFind != INVALID_HANDLE_VALUE) {
			FindClose(hFind);
			
			CaptureWarnInit();

			if (IDYES == MessageBox(hwnd,
				"You may experience difficulty getting exact framerates "
				"with your capture card, resulting in dropped frames.  Do "
				"you want to know more about this problem?"
				,
				"Zoran drivers detected",
				MB_YESNO)) {

				VDShowHelp(hwnd, L"capwarn.html");
			}
			CaptureWarnDisable(CWF_ZORAN);
		}
	}

	if (!(g_capwarnFlags & CWF_BROOKTREE)) {
		GetWindowsDirectory(szPath, sizeof szPath);
		s = szPath;
		while(*s) ++s;
		if (s[-1] != '\\')
			*s++ = '\\';

		strcpy(s, "system\\bt848_32.dll");

		hFind = FindFirstFile(szPath, &wfd);

		if (hFind == INVALID_HANDLE_VALUE) {
			strcpy(s, "system32\\bt848_32.dll");

			hFind = FindFirstFile(szPath, &wfd);
		}

		if (hFind != INVALID_HANDLE_VALUE) {
			FindClose(hFind);
			
			CaptureWarnInit();

			if (IDYES == MessageBox(hwnd,
				"You may have difficulty capturing above 320x240 with this card.  Do "
				"you want to know more about this problem?"
				,
				"Brooktree Bt848/878 drivers detected",
				MB_YESNO)) {

				VDShowHelp(hwnd, L"capwarn.html");
			}
			CaptureWarnDisable(CWF_BROOKTREE);
		}
	}
}

