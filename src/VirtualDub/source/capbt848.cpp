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
//
//
//
//  This file is partially based off of bt848.c from dTV.
//
//	dTV Copyright (c) 2000 John Adcock.  All rights reserved.

#include "stdafx.h"
#include <windows.h>
#include <commctrl.h>
#include "resource.h"
#include <vd2/system/error.h>
#include "dtvdrv.h"

extern const char g_szError[];
extern HINSTANCE g_hInst;
static HWND g_hwndTweaker;
static HMODULE g_hmodDTV;

struct dTVDriverData {
	PIsDriverOpened				isDriverOpened;
	PPCIGetHardwareResources	pciGetHardwareResources;
	PMemoryAlloc				memoryAlloc;
	PMemoryFree					memoryFree;
	PMemoryMap					memoryMap;
	PMemoryMap					memoryUnmap;
	PMemoryRead					memoryReadBYTE;
	PMemoryRead					memoryReadWORD;
	PMemoryRead					memoryReadDWORD;
	PMemoryWrite				memoryWriteBYTE;
	PMemoryWrite				memoryWriteWORD;
	PMemoryWrite				memoryWriteDWORD;

	DWORD						dwMemoryBase;
	DWORD						dwMemoryLength;
	DWORD						dwPhysicalAddress;
} g_dTVDriver;

static void InitializeBT8X8() {
	DWORD ids[4][2]={
		{ 0x109e, 0x036e },
		{ 0x109e, 0x0350 },
		{ 0x109e, 0x0351 },
		{ 0x109e, 0x036f },
	};

	if (g_dTVDriver.dwPhysicalAddress)
		return;

	for(int i=0; i<4; ++i) {
		int ret;
		DWORD dwPhysicalAddress = 0;
		DWORD dwMemoryLength = 0;
		DWORD dwSubSystemID = 0;

		ret = g_dTVDriver.pciGetHardwareResources(ids[i][0], ids[i][1], &dwPhysicalAddress,
			&dwMemoryLength, &dwSubSystemID);

		if (ret == ERROR_SUCCESS) {
			g_dTVDriver.dwMemoryBase = g_dTVDriver.memoryMap(dwPhysicalAddress, dwMemoryLength);

			if (!g_dTVDriver.dwMemoryBase)
				throw MyError("Found BT8X8 chip, but could not lock memory-mapped registers.");

			g_dTVDriver.dwPhysicalAddress = dwPhysicalAddress;
			g_dTVDriver.dwMemoryLength = dwMemoryLength;

			return;
		}

	}

	throw MyError("This function requires a video capture device based on the Brooktree (Conexant) "
			"BT848(A), BT849, or BT878 chip.  None could be found.  Are you sure you have one?");
}

static void DeinitializeBT8X8() {
	if (g_dTVDriver.dwPhysicalAddress) {
		g_dTVDriver.memoryUnmap(g_dTVDriver.dwPhysicalAddress, g_dTVDriver.dwMemoryLength);
		g_dTVDriver.dwPhysicalAddress = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////

INT_PTR CALLBACK CaptureBT848TweakerDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	static const struct {
		DWORD id;
		DWORD offset;
		DWORD bit;
	} s_bits[]={
		{ IDC_LUMANOTCHEVEN,	0x02C, 0x80 },
		{ IDC_LUMANOTCHODD,		0x0AC, 0x80 },

		{ IDC_LUMADECIMATEEVEN,	0x02C, 0x20 },
		{ IDC_LUMADECIMATEODD,	0x0AC, 0x20 },

		{ IDC_CHROMAAGCENEVEN,	0x040, 0x40 },
		{ IDC_CHROMAAGCENODD,	0x0C0, 0x40 },
		{ IDC_CHROMAKILLEVEN,	0x040, 0x20 },
		{ IDC_CHROMAKILLODD,	0x0C0, 0x20 },

		{ IDC_LUMAFULLRANGE,	0x048, 0x80 },

		{ IDC_LUMACOMBEVEN,		0x04C, 0x80 },
		{ IDC_LUMACOMBODD,		0x0CC, 0x80 },
		{ IDC_CHROMACOMBEVEN,	0x04C, 0x40 },
		{ IDC_CHROMACOMBODD,	0x0CC, 0x40 },
		{ IDC_INTERLACEVSEVEN,	0x04C, 0x20 },
		{ IDC_INTERLACEVSODD,	0x0CC, 0x20 },

		{ IDC_AGCCRUSH,			0x068, 0x01 },
		{ IDC_GAMMACORRECTION,	0x0D8, 0x10 },

		{ IDC_LUMAPEAKEVEN,		0x040, 0x80 },
		{ IDC_LUMAPEAKODD,		0x0C0, 0x80 },
		{0},
	};

   static const char *const s_whitept_types[]={
      "3/4 max", "1/2 max", "1/4 max", "auto"
   };

	int i;

	switch(msg) {
	case WM_INITDIALOG:
		for(i=0; s_bits[i].id; ++i)
			CheckDlgButton(hdlg, s_bits[i].id, (g_dTVDriver.memoryReadBYTE(s_bits[i].offset) & s_bits[i].bit) ? BST_CHECKED:BST_UNCHECKED);

		SendDlgItemMessage(hdlg, IDC_SLIDER_LUMAPEAKEVEN, TBM_SETRANGE, TRUE, MAKELONG(0, 3));
		SendDlgItemMessage(hdlg, IDC_SLIDER_LUMAPEAKODD, TBM_SETRANGE, TRUE, MAKELONG(0, 3));
		SendDlgItemMessage(hdlg, IDC_SLIDER_CORING, TBM_SETRANGE, TRUE, MAKELONG(0, 3));

		SendDlgItemMessage(hdlg, IDC_SLIDER_LUMAPEAKEVEN, TBM_SETPOS, TRUE, (g_dTVDriver.memoryReadBYTE(0x040)&0x18)>>3);
		SendDlgItemMessage(hdlg, IDC_SLIDER_LUMAPEAKODD, TBM_SETPOS, TRUE, (g_dTVDriver.memoryReadBYTE(0x0c0)&0x18)>>3);
		SendDlgItemMessage(hdlg, IDC_SLIDER_CORING, TBM_SETPOS, TRUE, (g_dTVDriver.memoryReadBYTE(0x048)&0x60)>>5);

		SendDlgItemMessage(hdlg, IDC_SLIDER_LEFT, TBM_SETRANGE, TRUE, MAKELONG(0, 10239));
		SendDlgItemMessage(hdlg, IDC_SLIDER_RIGHT, TBM_SETRANGE, TRUE, MAKELONG(0, 10239));
		SendDlgItemMessage(hdlg, IDC_SLIDER_VDELAY, TBM_SETRANGE, TRUE, MAKELONG(0, 256));
		SendDlgItemMessage(hdlg, IDC_SLIDER_AGCDELAY, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
		SendDlgItemMessage(hdlg, IDC_SLIDER_BURSTDELAY, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
		SendDlgItemMessage(hdlg, IDC_SLIDER_WHITEUP, TBM_SETRANGE, TRUE, MAKELONG(0, 63));
		SendDlgItemMessage(hdlg, IDC_SLIDER_WHITEDOWN, TBM_SETRANGE, TRUE, MAKELONG(0, 63));

		{
			// grab from the currently captured field

			int regbase = (g_dTVDriver.memoryReadBYTE(0x0DC) & 1) ? 0x000 : 0x080;
			int hscale = g_dTVDriver.memoryReadBYTE(regbase + 0x020)*256+g_dTVDriver.memoryReadBYTE(regbase + 0x024) + 4096;
			int hdelay = ((g_dTVDriver.memoryReadBYTE(regbase + 0x00C)&0x0c)<<6)+(g_dTVDriver.memoryReadBYTE(regbase + 0x018)&0xfe);
			int hactive = ((g_dTVDriver.memoryReadBYTE(regbase + 0x00C)&0x003)<<8)+g_dTVDriver.memoryReadBYTE(regbase + 0x01c);
			int left = MulDiv(hdelay,hscale,512);
			int right = left + MulDiv(hactive, hscale,512);

			SendDlgItemMessage(hdlg, IDC_SLIDER_LEFT, TBM_SETPOS, TRUE,	left);
			SendDlgItemMessage(hdlg, IDC_SLIDER_RIGHT, TBM_SETPOS, TRUE, right);
		}

		SendDlgItemMessage(hdlg, IDC_SLIDER_VDELAY, TBM_SETPOS, TRUE,
			((g_dTVDriver.memoryReadBYTE(0x00C)&0xc0)<<2)+g_dTVDriver.memoryReadBYTE(0x010));

		SendDlgItemMessage(hdlg, IDC_SLIDER_AGCDELAY, TBM_SETPOS, TRUE,
			g_dTVDriver.memoryReadBYTE(0x60));

		SendDlgItemMessage(hdlg, IDC_SLIDER_BURSTDELAY, TBM_SETPOS, TRUE,
			g_dTVDriver.memoryReadBYTE(0x64));

		SendDlgItemMessage(hdlg, IDC_SLIDER_WHITEDOWN, TBM_SETPOS, TRUE,
			g_dTVDriver.memoryReadBYTE(0x78)&0x3f);

		SendDlgItemMessage(hdlg, IDC_SLIDER_WHITEUP, TBM_SETPOS, TRUE,
			g_dTVDriver.memoryReadBYTE(0x44)&0x3f);

		for(i=0; i<4; ++i)
			SendDlgItemMessage(hdlg, IDC_WHITE_POINT, CB_ADDSTRING, 0, (LPARAM)s_whitept_types[i]);

		SendDlgItemMessage(hdlg, IDC_WHITE_POINT, CB_SETCURSEL, g_dTVDriver.memoryReadBYTE(0x44)>>6, 0);

		return TRUE;
	case WM_DESTROY:
		g_hwndTweaker = NULL;
		break;
	case WM_TIMER:
		KillTimer(hdlg, 1);
		wParam = IDC_REASSERT;
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
		case IDCANCEL:
			DestroyWindow(hdlg);
			break;
		case IDC_WHITE_POINT:
			g_dTVDriver.memoryWriteBYTE(0x44, (g_dTVDriver.memoryReadBYTE(0x44)&0x3f)
				+ (SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0)<<6));
			break;
		case IDC_REASSERT:
			// this is, admittedly, goofy
			_RPT0(0, "Reasserting settings\n");

			for(i=0; s_bits[i].id; ++i)
				if (IsDlgButtonChecked(hdlg, s_bits[i].id))
					g_dTVDriver.memoryWriteBYTE(s_bits[i].offset, g_dTVDriver.memoryReadBYTE(s_bits[i].offset) | s_bits[i].bit);
				else
					g_dTVDriver.memoryWriteBYTE(s_bits[i].offset, g_dTVDriver.memoryReadBYTE(s_bits[i].offset) & ~s_bits[i].bit);

			CaptureBT848TweakerDlgProc(hdlg, WM_COMMAND, BN_CLICKED, (LPARAM)GetDlgItem(hdlg, IDC_WHITE_POINT));
			CaptureBT848TweakerDlgProc(hdlg, WM_HSCROLL, SB_THUMBPOSITION, (LPARAM)GetDlgItem(hdlg, IDC_SLIDER_LUMAPEAKEVEN));
			CaptureBT848TweakerDlgProc(hdlg, WM_HSCROLL, SB_THUMBPOSITION, (LPARAM)GetDlgItem(hdlg, IDC_SLIDER_LUMAPEAKODD));
			CaptureBT848TweakerDlgProc(hdlg, WM_HSCROLL, SB_THUMBPOSITION, (LPARAM)GetDlgItem(hdlg, IDC_SLIDER_CORING));
			CaptureBT848TweakerDlgProc(hdlg, WM_HSCROLL, SB_THUMBPOSITION, (LPARAM)GetDlgItem(hdlg, IDC_SLIDER_LEFT));
			CaptureBT848TweakerDlgProc(hdlg, WM_HSCROLL, SB_THUMBPOSITION, (LPARAM)GetDlgItem(hdlg, IDC_SLIDER_RIGHT));
			CaptureBT848TweakerDlgProc(hdlg, WM_HSCROLL, SB_THUMBPOSITION, (LPARAM)GetDlgItem(hdlg, IDC_SLIDER_VDELAY));
			CaptureBT848TweakerDlgProc(hdlg, WM_HSCROLL, SB_THUMBPOSITION, (LPARAM)GetDlgItem(hdlg, IDC_SLIDER_AGCDELAY));
			CaptureBT848TweakerDlgProc(hdlg, WM_HSCROLL, SB_THUMBPOSITION, (LPARAM)GetDlgItem(hdlg, IDC_SLIDER_BURSTDELAY));
			CaptureBT848TweakerDlgProc(hdlg, WM_HSCROLL, SB_THUMBPOSITION, (LPARAM)GetDlgItem(hdlg, IDC_SLIDER_WHITEDOWN));
			CaptureBT848TweakerDlgProc(hdlg, WM_HSCROLL, SB_THUMBPOSITION, (LPARAM)GetDlgItem(hdlg, IDC_SLIDER_WHITEUP));
				
			break;
		default:
			if (HIWORD(wParam) == BN_CLICKED)
				for(i=0; s_bits[i].id; ++i)
					if (s_bits[i].id == LOWORD(wParam)) {
						if (IsDlgButtonChecked(hdlg, s_bits[i].id))
							g_dTVDriver.memoryWriteBYTE(s_bits[i].offset, g_dTVDriver.memoryReadBYTE(s_bits[i].offset) | s_bits[i].bit);
						else
							g_dTVDriver.memoryWriteBYTE(s_bits[i].offset, g_dTVDriver.memoryReadBYTE(s_bits[i].offset) & ~s_bits[i].bit);
						break;
					}
			break;
		}
		return TRUE;

	case WM_HSCROLL:
		if (lParam) {
			int pos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);

			switch(GetWindowLong((HWND)lParam, GWL_ID)) {
			case IDC_SLIDER_LUMAPEAKEVEN:
				g_dTVDriver.memoryWriteBYTE(0x40, (g_dTVDriver.memoryReadBYTE(0x40)&~0x18) + ((pos&3)<<3));
				return TRUE;
			case IDC_SLIDER_LUMAPEAKODD:
				g_dTVDriver.memoryWriteBYTE(0xC0, (g_dTVDriver.memoryReadBYTE(0xC0)&~0x18) + ((pos&3)<<3));
				return TRUE;
			case IDC_SLIDER_CORING:
				g_dTVDriver.memoryWriteBYTE(0x48, (g_dTVDriver.memoryReadBYTE(0x48)&~0x60) + ((pos&3)<<5));
				return TRUE;
			case IDC_SLIDER_LEFT:
			case IDC_SLIDER_RIGHT:
				{
					int left = SendDlgItemMessage(hdlg, IDC_SLIDER_LEFT, TBM_GETPOS, 0, 0);
					int right = SendDlgItemMessage(hdlg, IDC_SLIDER_RIGHT, TBM_GETPOS, 0, 0);

					if (right < left)
						right = left;

					int regbase = (g_dTVDriver.memoryReadBYTE(0x0DC) & 1) ? 0x000 : 0x080;
					int hactive = ((g_dTVDriver.memoryReadBYTE(regbase + 0x00C)&0x003)<<8)+g_dTVDriver.memoryReadBYTE(regbase + 0x01c);
					int hscale = MulDiv(right-left, 512, hactive);

					if (hscale < 4096)
						hscale = 4096;

					int hdelay = MulDiv(left, 512, hscale);

					g_dTVDriver.memoryWriteBYTE(0x20, (hscale-4096)>>8);
					g_dTVDriver.memoryWriteBYTE(0xA0, (hscale-4096)>>8);
					g_dTVDriver.memoryWriteBYTE(0x24, hscale&0xff);
					g_dTVDriver.memoryWriteBYTE(0xA4, hscale&0xff);
					g_dTVDriver.memoryWriteBYTE(0x18, (hdelay&0xfe) + (g_dTVDriver.memoryReadBYTE(0x18) & 0x01));
					g_dTVDriver.memoryWriteBYTE(0x98, (hdelay&0xfe) + (g_dTVDriver.memoryReadBYTE(0x98) & 0x01));
					g_dTVDriver.memoryWriteBYTE(0x0C, (g_dTVDriver.memoryReadBYTE(0x0C) & 0xf3) + ((hdelay&0x300)>>6));
					g_dTVDriver.memoryWriteBYTE(0x8C, (g_dTVDriver.memoryReadBYTE(0x8C) & 0xf3) + ((hdelay&0x300)>>6));
				}
				return TRUE;
			case IDC_SLIDER_VDELAY:
				g_dTVDriver.memoryWriteBYTE(0x10, pos&0xff);
				g_dTVDriver.memoryWriteBYTE(0x90, pos&0xff);
				g_dTVDriver.memoryWriteBYTE(0x0C, (g_dTVDriver.memoryReadBYTE(0x0C) & 0x3f) + ((pos&0x300)>>2));
				g_dTVDriver.memoryWriteBYTE(0x8C, (g_dTVDriver.memoryReadBYTE(0x8C) & 0x3f) + ((pos&0x300)>>2));
				return TRUE;
			case IDC_SLIDER_AGCDELAY:
				g_dTVDriver.memoryWriteBYTE(0x60, pos);
				return TRUE;
			case IDC_SLIDER_BURSTDELAY:
				g_dTVDriver.memoryWriteBYTE(0x64, pos);
				return TRUE;
			case IDC_SLIDER_WHITEDOWN:
				g_dTVDriver.memoryWriteBYTE(0x78, (g_dTVDriver.memoryReadBYTE(0x78) & 0xc0) + pos);
				return TRUE;
			case IDC_SLIDER_WHITEUP:
				g_dTVDriver.memoryWriteBYTE(0x44, (g_dTVDriver.memoryReadBYTE(0x44) & 0xc0) + pos);
				return TRUE;
			}
		}
		break;
	}
	return FALSE;
}

void CaptureDisplayBT848Tweaker(HWND hwndParent) {
	static const char *const g_dtvEntryPts[]={
		"isDriverOpened",
		"pciGetHardwareResources",
		"memoryAlloc",
		"memoryFree",
		"memoryMap",
		"memoryUnmap",
		"memoryReadBYTE",
		"memoryReadWORD",
		"memoryReadDWORD",
		"memoryWriteBYTE",
		"memoryWriteWORD",
		"memoryWriteDWORD",
	};

	if (g_hwndTweaker) {
		SetForegroundWindow(g_hwndTweaker);
		return;
	}

	// Attempt to load dTV driver

	g_hmodDTV = LoadLibrary("dTVdrv.dll");

	try {

		if (!g_hmodDTV)
			throw MyWin32Error("Cannot load DScaler driver: %%s\n\nThis function requires the dTVdrv.dll, dTVdrvNT.sys, and dTVdrv95.vxd "
				"files from DScaler to be copied into the VirtualDub program directory.", GetLastError());

		// dTV driver is loaded -- obtain entry points.

		FARPROC *ppEntries = (FARPROC *)&g_dTVDriver;

		for(int i=0; i<(sizeof g_dtvEntryPts / sizeof g_dtvEntryPts[0]); ++i)
			if (!(ppEntries[i] = GetProcAddress(g_hmodDTV, g_dtvEntryPts[i])))
				throw MyError("Cannot load DScaler driver: entry point \"%s\" is missing!", g_dtvEntryPts[i]);

		// Map the registers.

		InitializeBT8X8();

		// Display the dialog.

		g_hwndTweaker = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_BT8X8), hwndParent, CaptureBT848TweakerDlgProc);

	} catch(const MyError& e) {
		e.post(hwndParent, g_szError);

		DeinitializeBT8X8();

		if (g_hmodDTV) {
			FreeLibrary(g_hmodDTV);
			g_hmodDTV = NULL;
		}
	}
}

void CaptureCloseBT848Tweaker() {
	if (g_hwndTweaker) {
		DestroyWindow(g_hwndTweaker);

		DeinitializeBT8X8();

		if (g_hmodDTV) {
			FreeLibrary(g_hmodDTV);
			g_hmodDTV = NULL;
		}
	}
}

void CaptureBT848Reassert() {
	if (g_hwndTweaker) {
		SetTimer(g_hwndTweaker, 1, 1000, NULL);
	}
}