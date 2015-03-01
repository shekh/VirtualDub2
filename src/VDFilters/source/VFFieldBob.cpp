//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2009 Avery Lee
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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "stdafx.h"
#include <windows.h>

#include "resource.h"

extern HINSTANCE g_hInst;

static int fieldbob_run(const VDXFilterActivation *fa, const VDXFilterFunctions *ff);
static long fieldbob_param(VDXFilterActivation *fa, const VDXFilterFunctions *ff);
static int fieldbob_config(VDXFilterActivation *fa, const VDXFilterFunctions *ff, HWND hWnd);
static void fieldbob_string2(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int maxlen);
static int fieldbob_start(VDXFilterActivation *fa, const VDXFilterFunctions *ff);
static int fieldbob_end(VDXFilterActivation *fa, const VDXFilterFunctions *ff);

class fieldbobFilterData {
public:
	int evenmode, oddmode;
};


////////////////////////////////////////////////////////////

static const char * const g_szModes[]={
	"none", "smooth", "up", "down"
};

long fieldbob_param(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

void fieldbob_string2(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int maxlen) {
	fieldbobFilterData *sfd;

	if (!(sfd = (fieldbobFilterData *)fa->filter_data)) return;

	_snprintf(buf, maxlen, " (%s/%s)", g_szModes[sfd->evenmode], g_szModes[sfd->oddmode]);
}

/////////////////////////////////////////////////////////////

BOOL APIENTRY fieldbobConfigDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
        case WM_INITDIALOG:
			{
				fieldbobFilterData *sfd = (fieldbobFilterData *)lParam;
				SetWindowLongPtr(hDlg, DWLP_USER, lParam);

				switch(sfd->evenmode) {
				case 0:		CheckDlgButton(hDlg, IDC_EVEN_NONE, BST_CHECKED); break;
				case 1:		CheckDlgButton(hDlg, IDC_EVEN_SMOOTH, BST_CHECKED); break;
				case 2:		CheckDlgButton(hDlg, IDC_EVEN_HALFUP, BST_CHECKED); break;
				case 3:		CheckDlgButton(hDlg, IDC_EVEN_HALFDOWN, BST_CHECKED); break;
				}
				switch(sfd->oddmode) {
				case 0:		CheckDlgButton(hDlg, IDC_ODD_NONE, BST_CHECKED); break;
				case 1:		CheckDlgButton(hDlg, IDC_ODD_SMOOTH, BST_CHECKED); break;
				case 2:		CheckDlgButton(hDlg, IDC_ODD_HALFUP, BST_CHECKED); break;
				case 3:		CheckDlgButton(hDlg, IDC_ODD_HALFDOWN, BST_CHECKED); break;
				}
			}
            return (TRUE);

        case WM_COMMAND:                 
			switch(LOWORD(wParam)) {

            case IDOK:
				{
					fieldbobFilterData *sfd = (fieldbobFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);

					if (IsDlgButtonChecked(hDlg, IDC_EVEN_NONE)) sfd->evenmode = 0;
					if (IsDlgButtonChecked(hDlg, IDC_EVEN_SMOOTH)) sfd->evenmode = 1;
					if (IsDlgButtonChecked(hDlg, IDC_EVEN_HALFUP)) sfd->evenmode = 2;
					if (IsDlgButtonChecked(hDlg, IDC_EVEN_HALFDOWN)) sfd->evenmode = 3;

					if (IsDlgButtonChecked(hDlg, IDC_ODD_NONE)) sfd->oddmode = 0;
					if (IsDlgButtonChecked(hDlg, IDC_ODD_SMOOTH)) sfd->oddmode = 1;
					if (IsDlgButtonChecked(hDlg, IDC_ODD_HALFUP)) sfd->oddmode = 2;
					if (IsDlgButtonChecked(hDlg, IDC_ODD_HALFDOWN)) sfd->oddmode = 3;
				}
				EndDialog(hDlg, TRUE);
				return TRUE;

			case IDCANCEL:
                EndDialog(hDlg, FALSE);  
                return TRUE;

            }
            break;
    }
    return FALSE;
}

int fieldbob_config(VDXFilterActivation *fa, const VDXFilterFunctions *ff, VDXHWND hWnd) {
	return !DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_FIELDBOB), (HWND)hWnd, (DLGPROC)fieldbobConfigDlgProc, (LPARAM)fa->filter_data);
}

/////////////////////////////////////////////////////////////

int fieldbob_start(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	fieldbobFilterData *sfd;

	if (!(sfd = (fieldbobFilterData *)fa->filter_data)) return 1;

	return 0;
}

int fieldbob_run(const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	fieldbobFilterData *sfd = (fieldbobFilterData *)fa->filter_data;

	if (!sfd) return 1;

	int mode = fa->pfsi->lCurrentSourceFrame & 1 ? sfd->oddmode : sfd->evenmode;
	uint32 w = fa->dst.w, h = fa->dst.h, w2;
	uint32 *src = fa->src.data, *dst = fa->dst.data;

	switch(mode) {
	case 0:
		do {
			memcpy(dst, src, w*4);

			src = (uint32 *)((char *)src + fa->src.pitch);
			dst = (uint32 *)((char *)dst + fa->dst.pitch);
		} while(--h);
		break;

	case 1:
		// Copy first and last lines

		memcpy(dst, src, w*4);
		memcpy((char *)dst + fa->dst.pitch*(h-1), (char *)src + fa->src.pitch*(h-1), w*4);

		if ((h-=2) > 0) {
			uint32 *src2 = (uint32 *)((char *)src + fa->src.pitch);
			uint32 *src3 = (uint32 *)((char *)src + fa->src.pitch*2);

			dst = (uint32 *)((char *)dst + fa->dst.pitch);

			do {
				w2 = w;

				do {
					uint32 a = *src++;
					uint32 b = *src2++;
					uint32 c = *src3++;

					*dst++	= (((((a>>1)&0x7f7f7f) + ((c&0xfefefe)>>1) + (a&b&0x010101))>>1)&0x7f7f7f)
							+ ((b&0xfefefe)>>1);
				} while(--w2);

				src = (uint32 *)((char *)src + fa->src.modulo);
				src2 = (uint32 *)((char *)src2 + fa->src.modulo);
				src3 = (uint32 *)((char *)src3 + fa->src.modulo);
				dst = (uint32 *)((char *)dst + fa->dst.modulo);
			} while(--h);
		}
		break;

	case 2:
		memcpy(dst, src, w*4);
		dst = (uint32 *)((char *)dst + fa->dst.pitch);
		if (--h > 0) {
			uint32 *src2 = (uint32 *)((char *)src + fa->src.pitch);
			do {
				w2 = w;

				do {
					uint32 a = *src++;
					uint32 b = *src2++;

					*dst++ = ((a>>2)&0x3f3f3f) + ((b&0xfcfcfc)>>2)*3 + ((((a&0x030303) + (b&0x030303)*3)>>2)&0x030303);
				} while(--w2);

				src = (uint32 *)((char *)src + fa->src.modulo);
				src2 = (uint32 *)((char *)src2 + fa->src.modulo);
				dst = (uint32 *)((char *)dst + fa->dst.modulo);
			} while(--h);
		}
		break;

	case 3:
		memcpy((char *)dst + fa->dst.pitch*(h-1), (char *)src + fa->src.pitch*(h-1), w*4);
		if (--h > 0) {
			uint32 *src2 = (uint32 *)((char *)src + fa->src.pitch);
			do {
				w2 = w;

				do {
					uint32 a = *src++;
					uint32 b = *src2++;

					*dst++ = ((a>>2)&0x3f3f3f)*3 + ((b&0xfcfcfc)>>2) + ((((a&0x030303)*3 + (b&0x030303))>>2)&0x030303);
				} while(--w2);

				src = (uint32 *)((char *)src + fa->src.modulo);
				src2 = (uint32 *)((char *)src2 + fa->src.modulo);
				dst = (uint32 *)((char *)dst + fa->dst.modulo);
			} while(--h);
		}
		break;
	}

	return 0;
}

static int fieldbob_end(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	fieldbobFilterData *sfd;

	if (!(sfd = (fieldbobFilterData *)fa->filter_data)) return 1;

	return 0;
}

static void fieldbob_script_config(IVDXScriptInterpreter *isi, void *lpVoid, VDXScriptValue *argv, int argc) {
	VDXFilterActivation *fa = (VDXFilterActivation *)lpVoid;
	fieldbobFilterData *mfd = (fieldbobFilterData *)fa->filter_data;

	mfd->evenmode	= argv[0].asInt();
	mfd->oddmode	= argv[1].asInt();
}

static VDXScriptFunctionDef fieldbob_func_defs[]={
	{ (VDXScriptFunctionPtr)fieldbob_script_config, "Config", "0ii" },
	{ NULL },
};

static VDXScriptObject fieldbob_obj={
	NULL, fieldbob_func_defs
};

static bool fieldbob_script_line(VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int buflen) {
	fieldbobFilterData *mfd = (fieldbobFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d,%d)", mfd->evenmode, mfd->oddmode);

	return true;
}

///////////////////////////////////////////////////////////////////////////

extern const VDXFilterDefinition g_VDVFFieldBob = {
	0,0,NULL,
	"field bob",
	"Compensates for field jumping in field-split video by applying bob-deinterlacing techniques.",
	NULL,
	NULL,sizeof(fieldbobFilterData),
	NULL,NULL,
	fieldbob_run,
	fieldbob_param,
	fieldbob_config,
	NULL,
	fieldbob_start,
	fieldbob_end,

	&fieldbob_obj,
	fieldbob_script_line,
	fieldbob_string2,
};

