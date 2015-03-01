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

#include "resource.h"
#include <vd2/system/list.h>
#include <vd2/system/filesys.h>
#include <vd2/system/VDString.h>
#include <vd2/system/w32assist.h>
#include <vd2/system/registry.h>
#include "gui.h"
#include "oshelper.h"
#include "capspill.h"

CapSpillDrive::CapSpillDrive() {
	threshold = priority = 0;
}

CapSpillDrive::~CapSpillDrive() {
}

void CapSpillDrive::setPath(const wchar_t *s) {
	path = s;
	pathA = VDTextWToA(s);
}

wchar_t *CapSpillDrive::makePath(wchar_t *buf, const wchar_t *fn) const {
	wchar_t *t;
	const wchar_t *s;

	s = path.c_str();
	t = buf;
	while(*t++ = *s++);

	--t;

	if (t>buf) {
		if (t[-1] != L'\\' && t[-1]!=L':')
			*t++ = L'\\'; 
	}

	wcscpy(t, fn);

	return buf;
}

///////////////////////////////////////////////////////////////////////////

extern HINSTANCE g_hInst;
extern const char g_szError[];

static ListAlloc<CapSpillDrive> g_spillDrives;

extern long	g_lSpillMinSize = 50;
extern long g_lSpillMaxSize = 1900;

///////////////////////////////////////////////////////////////////////////

__int64 CapSpillGetFreeSpace() {
	CapSpillDrive *pcsd = g_spillDrives.AtHead();
	CapSpillDrive *pcsd_next;
	__int64 i64Total = 0, i64Free;

	while(pcsd_next = pcsd->NextFromHead()) {

		// Probably shouldn't continuously query free space on a UNC path.

		if (pcsd->path[0] != '\\' || pcsd->path[1] != '\\') {
			i64Free = VDGetDiskFreeSpace(pcsd->path.c_str());

			if (i64Free != -1)
				i64Total += i64Free;
		}

		pcsd = pcsd_next;
	}

	return i64Total;
}

CapSpillDrive *CapSpillPickDrive(bool fAudio) {
	// Poll drives, look for the highest priority with the most free space

	CapSpillDrive *pcsd = g_spillDrives.AtHead();
	CapSpillDrive *pcsd_next;
	CapSpillDrive *pcsd_best = NULL;
	__int64 i64Free, i64FreeBest = 0;

	while(pcsd_next = pcsd->NextFromHead()) {

		i64Free = VDGetDiskFreeSpace(pcsd->path.c_str());

		if ((i64Free>>20) > pcsd->threshold + g_lSpillMinSize &&
			(!pcsd_best || pcsd->priority > pcsd_best->priority
			|| (pcsd->priority == pcsd_best->priority && i64Free > i64FreeBest))) {

			i64FreeBest = i64Free;
			pcsd_best = pcsd;
		}

		pcsd = pcsd_next;
	}

	return pcsd_best;
}

CapSpillDrive *CapSpillFindDrive(const wchar_t *path) {
	CapSpillDrive *pcsd = g_spillDrives.AtHead();
	CapSpillDrive *pcsd_next;

	while(pcsd_next = pcsd->NextFromHead()) {
		const wchar_t *s = path, *t = pcsd->path.c_str();
		wchar_t c, d;

		while((c=*s++)==(d=*t++) && c)
			;

		if ((!c && !d) || (c==L'/' && !d) || (!c && d==L'/'))
			return pcsd;

		pcsd = pcsd_next;
	}

	return NULL;
}

void CapSpillSaveToRegistry() {
	int numDrives = 0;
	CapSpillDrive *pcsd = g_spillDrives.AtHead(), *pcsd_next;

	VDRegistryAppKey driveKey("Capture\\Spill Drives");

	while(pcsd_next = (CapSpillDrive *)pcsd->NextFromHead()) {
		char szValueName[32];
		sprintf(szValueName, "Drive %d", numDrives + 1);

		wchar_t szDriveConfig[1024];
		if ((unsigned)_snwprintf(szDriveConfig, 1024, L"%d,%d,%s", pcsd->priority, pcsd->threshold, pcsd->path.c_str()) < 1024) {
			driveKey.setString(szValueName, szDriveConfig);
			++numDrives;
		}

		pcsd = pcsd_next;
	}

	driveKey.setInt("Number", numDrives);
	driveKey.setInt("Minimum file size", g_lSpillMinSize);
	driveKey.setInt("Maximum file size", g_lSpillMaxSize);
}

void CapSpillRestoreFromRegistry() {
	CapSpillDrive *pcsd;

	VDRegistryAppKey driveKey("Capture\\Spill Drives");

	g_lSpillMinSize = driveKey.getInt("Minimum file size", 50);
	g_lSpillMaxSize = driveKey.getInt("Maximum file size", 1900);

	int numDrives = driveKey.getInt("Number", 0);
	if (!numDrives)
		return;

	while(pcsd = g_spillDrives.RemoveTail())
		delete pcsd;

	for(int drive = 1; drive <= numDrives; ++drive) {
		char szValueName[32];
		sprintf(szValueName, "Drive %d", drive);

		VDStringW driveConfig;
		if (driveKey.getString(szValueName, driveConfig)) {
			int pri, thresh, pos;

			if (2==swscanf(driveConfig.c_str(), L"%d,%d%n", &pri, &thresh, &pos) && driveConfig[pos]==',') {
				pcsd = new_nothrow CapSpillDrive();

				if (pcsd) {
					const wchar_t *pszPath = driveConfig.c_str()+pos+1;

					pcsd->priority = pri;
					pcsd->threshold = thresh;
					pcsd->setPath(pszPath);
					g_spillDrives.AddTail(pcsd);
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////

static LRESULT APIENTRY LVWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void LVBeginEdit(HWND hwndLV, int index, int subitem);

///////////////////////////////////////////////////////////////////////////

bool CapSpillAdd(HWND hwnd, CapSpillDrive *pcsd, bool fAddList) {
	LVITEM lvi;

	lvi.mask		= LVIF_TEXT | LVIF_PARAM;
	lvi.iItem		= 0;
	lvi.iSubItem	= 0;
	lvi.pszText		= LPSTR_TEXTCALLBACK;
	lvi.lParam		= (LPARAM)pcsd;

	if (-1 == ListView_InsertItem(hwnd, &lvi))
		return false;

	if (fAddList)
		g_spillDrives.AddTail(pcsd);

	return true;
}

INT_PTR CALLBACK CaptureSpillDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_INITDIALOG:
		{
			HWND hwndItem;
			LVCOLUMN lvc;
			RECT r;
			CapSpillDrive *pcsd, *pcsd_next;

			hwndItem = GetDlgItem(hdlg, IDC_SPILL_DRIVES);
			GetClientRect(hwndItem, &r);

			ListView_SetExtendedListViewStyleEx(hwndItem, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

			lvc.mask	= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
			lvc.fmt		= LVCFMT_LEFT;
			lvc.cx		= 50;
			lvc.pszText	= "Priority";

			ListView_InsertColumn(hwndItem, 0, &lvc);

			lvc.pszText	= "Threshold";
			lvc.cx		= 100;
			ListView_InsertColumn(hwndItem, 1, &lvc);

			lvc.pszText	= "Path";
			lvc.cx		= r.right - r.left - 150;
			ListView_InsertColumn(hwndItem, 2, &lvc);

			pcsd = (CapSpillDrive *)g_spillDrives.AtHead();

			while(pcsd_next = (CapSpillDrive *)pcsd->NextFromHead()) {
				CapSpillAdd(hwndItem, pcsd, false);
				pcsd = pcsd_next;
			}

			guiSubclassWindow(hwndItem, LVWndProc);

			SetDlgItemInt(hdlg, IDC_MIN_SIZE, g_lSpillMinSize, FALSE);
			SetDlgItemInt(hdlg, IDC_MAX_SIZE, g_lSpillMaxSize, FALSE);
		}
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
		case IDCANCEL:
			{
				BOOL fOk;
				LONG lMin, lMax;

				lMin = GetDlgItemInt(hdlg, IDC_MIN_SIZE, &fOk, FALSE);
				if (!fOk || lMin > 2048) {
					SetFocus(GetDlgItem(hdlg, IDC_MIN_SIZE));
					MessageBox(hdlg, "Minimum size must be between 0 and 2048 megabytes.", g_szError, MB_OK);
					return TRUE;
				}
				lMax = GetDlgItemInt(hdlg, IDC_MAX_SIZE, &fOk, FALSE);
				if (!fOk || lMax < 50 || lMax > 2048) {
					SetFocus(GetDlgItem(hdlg, IDC_MAX_SIZE));
					MessageBox(hdlg, "Maximum size must be between 50 and 2048 megabytes.", g_szError, MB_OK);
					return TRUE;
				}

				g_lSpillMinSize = lMin;
				g_lSpillMaxSize = lMax;
			}
			CapSpillSaveToRegistry();
			EndDialog(hdlg, 0);
			return TRUE;
		case IDC_ADD:
			{
				CapSpillDrive *pcsd = new CapSpillDrive();

				if (!pcsd)
					return TRUE;

				pcsd->threshold = 50;

				if (!CapSpillAdd(GetDlgItem(hdlg, IDC_SPILL_DRIVES), pcsd, true))
					delete pcsd;
			}
			return TRUE;
		case IDC_DEL:
			{
				HWND hwndLV = GetDlgItem(hdlg, IDC_SPILL_DRIVES);
				int ind;

				ind = ListView_GetNextItem(hwndLV, -1, MAKELPARAM(LVNI_SELECTED,0));

				if (ind >= 0) {
					LVITEM lvi;

					lvi.iItem = ind;
					lvi.iSubItem = 0;
					lvi.mask = LVIF_PARAM;

					if (ListView_GetItem(hwndLV, &lvi)) {
						CapSpillDrive *pcsd = (CapSpillDrive *)lvi.lParam;

						pcsd->Remove();
						delete pcsd;
					}

					ListView_DeleteItem(hwndLV, ind);
				}
			}
			return TRUE;
		}
		break;

	case WM_NOTIFY:
		if (((NMHDR *)lParam)->idFrom == IDC_SPILL_DRIVES) {
			NMLVDISPINFO *pnldi = (NMLVDISPINFO *)lParam;

			if (pnldi->hdr.code == LVN_GETDISPINFO) {
				CapSpillDrive *pcsd;

				if (!(pnldi->item.mask & LVIF_TEXT))
					return FALSE;

				pcsd = (CapSpillDrive *)pnldi->item.lParam;

				if (!pcsd) {
					pnldi->item.pszText[0] = 0;
					return TRUE;
				}

				switch(pnldi->item.iSubItem) {
				case 0:
					_snprintf(pnldi->item.pszText, pnldi->item.cchTextMax, "%+d", pcsd->priority);
					break;
				case 1:
					_snprintf(pnldi->item.pszText, pnldi->item.cchTextMax, "%ldMB", pcsd->threshold);
					break;
				case 2:
					pnldi->item.pszText = (TCHAR *)pcsd->pathA.c_str();
					break;
				default:
					pnldi->item.pszText[0] = 0;
					break;
				}
			}
		}
		return TRUE;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////

static HWND g_hwndBox;
static HWND g_hwndEdit=NULL;
static HWND g_hwndSpin=NULL;
static CapSpillDrive *g_csdPtr;
static int g_csdItem;
static int g_index;

int CALLBACK LVSorter(LPARAM lp1, LPARAM lp2, LPARAM lp) {
	CapSpillDrive *pcsd1 = (CapSpillDrive *)lp1;
	CapSpillDrive *pcsd2 = (CapSpillDrive *)lp2;

	if (pcsd1->priority > pcsd2->priority)			return -1;
	else if (pcsd1->priority < pcsd2->priority)		return 1;
	else if (pcsd1->threshold > pcsd2->threshold)	return 1;
	else if (pcsd1->threshold < pcsd2->threshold)	return -1;
	else
		return _wcsicmp(pcsd1->path.c_str(), pcsd2->path.c_str());
}

static void LVDestroyEdit(bool write, bool sort) {
	if (g_hwndSpin) {
		DestroyWindow(g_hwndSpin);
		g_hwndSpin = NULL;
	}

	if (g_hwndEdit) {
		if (g_csdItem == 2) {
			g_csdPtr->setPath(VDGetWindowTextW32(g_hwndEdit).c_str());
		} else {
			char buf[32];
			long lv;

			GetWindowText(g_hwndEdit, buf, sizeof buf);

			if (1 == sscanf(buf, "%ld", &lv)) {
				if (!g_csdItem) {
					if (lv<-128) lv = -128; else if (lv>127) lv=127;
					g_csdPtr->priority = lv;
				} else {
					if (lv<0) lv=0;
					g_csdPtr->threshold = lv;
				}
			}
		}
		DestroyWindow(g_hwndEdit);
		g_hwndEdit = NULL;

		if (sort) {
			SendMessage(g_hwndBox, LVM_REDRAWITEMS, g_index, g_index);
			SendMessage(g_hwndBox, LVM_SORTITEMS, 0, (LPARAM)&LVSorter);
		}
	}
}

static LRESULT APIENTRY LVEditWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	WNDPROC wpOld = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch(msg) {
	case WM_GETDLGCODE:
		return CallWindowProc(wpOld, hwnd, msg, wParam, lParam) | DLGC_WANTALLKEYS;
		break;
	case WM_KEYDOWN:
		if (wParam == VK_UP) {
			if (g_index > 0)
				LVBeginEdit(g_hwndBox, g_index-1, g_csdItem);
			return 0;
		} else if (wParam == VK_DOWN) {
			if (g_index < SendMessage(g_hwndBox, LVM_GETITEMCOUNT, 0, 0)-1)
				LVBeginEdit(g_hwndBox, g_index+1, g_csdItem);
			return 0;
		} else if (wParam == VK_TAB) {
			if ((SHORT)GetKeyState(VK_SHIFT) < 0) {
				if (g_csdItem > 0)
					LVBeginEdit(g_hwndBox, g_index, g_csdItem-1);
			} else {
				if (g_csdItem < 2)
					LVBeginEdit(g_hwndBox, g_index, g_csdItem+1);
			}
			return 0;
		}
		break;
	case WM_CHAR:
		if (wParam == 0x0d) {
			LVDestroyEdit(true, true);
			return 0;
		} else if (wParam == 0x1b) {
			LVDestroyEdit(false, true);
			return 0;
		}
		break;
	case WM_KILLFOCUS:
		LVDestroyEdit(true, true);
		break;
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
			if (!g_hwndSpin || (HWND)lParam != g_hwndSpin)
				LVDestroyEdit(true, true);
		break;
	}
	return CallWindowProc(wpOld, hwnd, msg, wParam, lParam);
}

static void LVBeginEdit(HWND hwndLV, int index, int subitem) {
	RECT r;
	int w=0, w2=0;
	int i;

	for(i=0; i<=subitem; i++) {
		w2 += w = SendMessage(hwndLV, LVM_GETCOLUMNWIDTH, i, 0);
	}

	LVDestroyEdit(true, false);

	g_index = index;

	r.left = LVIR_BOUNDS;

	SendMessage(hwndLV, LVM_GETITEMRECT, index, (LPARAM)&r);

	g_hwndBox = hwndLV;
	g_hwndEdit = CreateWindow("EDIT",
			NULL,
			WS_VISIBLE|WS_CHILD|WS_BORDER | ES_WANTRETURN|ES_AUTOHSCROLL,
			w2-w - 1,
			r.top - 1,
			w + 2,
			r.bottom-r.top + 2,
			hwndLV, (HMENU)1, g_hInst, NULL);
	
	if (g_hwndEdit) {
		LVITEM lvi;
		CapSpillDrive *pcsd;
		char buf[32];

		lvi.iItem = index;
		lvi.iSubItem = 0;
		lvi.mask = LVIF_PARAM;

		SendMessage(hwndLV, LVM_GETITEM, 0, (LPARAM)&lvi);

		g_csdPtr = pcsd = (CapSpillDrive *)lvi.lParam;
		g_csdItem = subitem;

		if (subitem<2)
			g_hwndSpin = CreateUpDownControl(WS_VISIBLE|WS_CHILD|UDS_ALIGNRIGHT|UDS_SETBUDDYINT, 0,0,0,0, hwndLV, 1, g_hInst, g_hwndEdit, 127, -128, 0);

		guiSubclassWindow(g_hwndEdit, LVEditWndProc);
		SendMessage(g_hwndEdit, WM_SETFONT, SendMessage(hwndLV, WM_GETFONT, 0, 0), MAKELPARAM(FALSE,0));

		switch(subitem) {
		case 0:
			sprintf(buf, "%d", pcsd->priority);
			SetWindowText(g_hwndEdit, buf);
			break;
		case 1:
			sprintf(buf, "%d", pcsd->threshold);
			SetWindowText(g_hwndEdit, buf);
			break;
		case 2:
			VDSetWindowTextW32(g_hwndEdit, pcsd->path.c_str());
			break;
		}

		SetFocus(g_hwndEdit);
	}
}

static LRESULT APIENTRY LVWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	WNDPROC wpOld = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch(msg) {
	case WM_DESTROY:
		LVDestroyEdit(true, false);
		break;

	case WM_GETDLGCODE:
		if (g_hwndEdit)
			return CallWindowProc(wpOld, hwnd, msg, wParam, lParam) | DLGC_WANTALLKEYS;
		else
			return CallWindowProc(wpOld, hwnd, msg, wParam, lParam) | DLGC_DEFPUSHBUTTON;

	case WM_CHAR:
		if (wParam == '\r') {
			int index = CallWindowProc(wpOld, hwnd, LVM_GETNEXTITEM, -1, MAKELPARAM(LVNI_ALL|LVNI_SELECTED,0));

			if (index>=0) {
				LVBeginEdit(hwnd, index, 0);
			}
		}
		break;

	case WM_LBUTTONDOWN:
		{
			LVHITTESTINFO htinfo;
			LVITEM lvi;
			int index;

			// if this isn't done, the control doesn't gain focus properly...

			CallWindowProc(wpOld, hwnd, msg, wParam, lParam);

			htinfo.pt.x	= 2;
			htinfo.pt.y = HIWORD(lParam);

			index = CallWindowProc(wpOld, hwnd, LVM_HITTEST, 0, (LPARAM)&htinfo);

			if (index >= 0) {
				int x = LOWORD(lParam);
				int w2=0, w;
				int i=-1;

				lvi.state = lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
				CallWindowProc(wpOld, hwnd, LVM_SETITEMSTATE, index, (LPARAM)&lvi);

				for(i=0; i<3; i++) {
					w2 += w = CallWindowProc(wpOld, hwnd, LVM_GETCOLUMNWIDTH, i, 0);
					if (x<w2) {
						LVBeginEdit(hwnd, index, i);

						return 0;
					}
				}
			}
			LVDestroyEdit(true, false);
		}
		return 0;
	}
	return CallWindowProc(wpOld, hwnd, msg, wParam, lParam);
}
