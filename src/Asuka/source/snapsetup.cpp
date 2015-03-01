//	Asuka - VirtualDub Build/Post-Mortem Utility
//	Copyright (C) 2005 Avery Lee
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
#include <vd2/system/vdtypes.h>
#include <stdio.h>
#include <windows.h>

namespace {
	BOOL WINAPI CtrlCFunc(DWORD event) {
		return event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT;
	}
}

void tool_snapsetup() {
	SetConsoleCtrlHandler(CtrlCFunc, TRUE);

	BOOL fontSmoothing;

	// disable font smoothing
	SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &fontSmoothing, 0);
	SystemParametersInfo(SPI_SETFONTSMOOTHING, FALSE, NULL, 0);

	// disable title bar gradient
	const INT kColorsToSet[]={ COLOR_GRADIENTACTIVECAPTION, COLOR_GRADIENTINACTIVECAPTION };
	const COLORREF oldColorValues[]={ GetSysColor(COLOR_GRADIENTACTIVECAPTION), GetSysColor(COLOR_GRADIENTINACTIVECAPTION) };
	const COLORREF newColorValues[]={ GetSysColor(COLOR_ACTIVECAPTION), GetSysColor(COLOR_INACTIVECAPTION) };

	SetSysColors(2, kColorsToSet, newColorValues);

	// refresh screen
	InvalidateRect(NULL, NULL, TRUE);

	puts("Display set up for screenshots. Press Return to restore system settings.");
	getchar();

	// restore title bar gradient
	SetSysColors(2, kColorsToSet, oldColorValues);

	// restore font smoothing
	SystemParametersInfo(SPI_SETFONTSMOOTHING, fontSmoothing, NULL, 0);

	// refresh screen
	InvalidateRect(NULL, NULL, TRUE);
}
