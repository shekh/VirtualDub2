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

#include "resource.h"

extern HINSTANCE g_hInst;

enum {
	MODE_LEFT90 = 0,
	MODE_RIGHT90 = 1,
	MODE_180 = 2
};

const wchar_t *const g_szMode[]={
	L"left 90\u00b0",
	L"right 90\u00b0",
	L"180\u00b0",
};

typedef struct MyFilterData {
	int mode;
} MyFilterData;

///////////////////////////////////////////////////////////////////////////

static int rotate_run(const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	uint32 *src, *dst, *dst0;
	uint32 w0 = fa->src.w, w;
	uint32 h = fa->src.h;
	ptrdiff_t dpitch = fa->dst.pitch;
	ptrdiff_t dmodulo = fa->dst.modulo;
	ptrdiff_t smodulo = fa->src.modulo;

	switch(mfd->mode) {
	case MODE_LEFT90:
		src = fa->src.data;
		dst0 = fa->dst.data + fa->dst.w;

		do {
			dst = --dst0;
			w = w0;
			do {
				*dst = *src++;
				dst = (uint32*)((char *)dst + dpitch);
			} while(--w);

			src = (uint32*)((char *)src + smodulo);
		} while(--h);
		break;

	case MODE_RIGHT90:
		src = fa->src.data;
		dst0 = (uint32 *)((char *)fa->dst.data + fa->dst.pitch*(fa->dst.h-1));

		do {
			dst = dst0++;
			w = w0;
			do {
				*dst = *src++;
				dst = (uint32*)((char *)dst - dpitch);
			} while(--w);

			src = (uint32*)((char *)src + smodulo);
		} while(--h);
		break;

	case MODE_180:
		src = fa->src.data;
		dst = (uint32 *)((char *)fa->dst.data + fa->dst.pitch*(fa->dst.h-1) + fa->dst.w*4 - 4);

		h>>=1;
		if (h) do {
			w = w0;
			do {
				uint32 a, b;

				a = *src;
				b = *dst;

				*src++ = b;
				*dst-- = a;
			} while(--w);

			src = (uint32*)((char *)src + smodulo);
			dst = (uint32*)((char *)dst - dmodulo);
		} while(--h);

		// if there is an odd line, flip half of it

		if (fa->src.h & 1) {
			w = w0>>1;
			if (w) do {
				uint32 a, b;

				a = *src;
				b = *dst;

				*src++ = b;
				*dst-- = a;
			} while(--w);
		}
		break;
	}
	return 0;
}

static long rotate_param(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	if (mfd->mode == MODE_180) {
		pxldst.pitch = pxlsrc.pitch;
		return FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
	}

	pxldst.w = pxlsrc.h;
	pxldst.h = pxlsrc.w;

	return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

static INT_PTR APIENTRY rotateDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
        case WM_INITDIALOG:
			{
				MyFilterData *mfd = (MyFilterData *)lParam;
				SetWindowLongPtr(hDlg, DWLP_USER, (LPARAM)mfd);

				switch(mfd->mode) {
				case MODE_LEFT90:	CheckDlgButton(hDlg, IDC_ROTATE_LEFT, BST_CHECKED); break;
				case MODE_RIGHT90:	CheckDlgButton(hDlg, IDC_ROTATE_RIGHT, BST_CHECKED); break;
				case MODE_180:		CheckDlgButton(hDlg, IDC_ROTATE_180, BST_CHECKED); break;
				}
			}
            return (TRUE);

        case WM_COMMAND:                      
            if (LOWORD(wParam) == IDOK) {
				MyFilterData *mfd = (struct MyFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);

				if (IsDlgButtonChecked(hDlg, IDC_ROTATE_LEFT)) mfd->mode = MODE_LEFT90;
				if (IsDlgButtonChecked(hDlg, IDC_ROTATE_RIGHT)) mfd->mode = MODE_RIGHT90;
				if (IsDlgButtonChecked(hDlg, IDC_ROTATE_180)) mfd->mode = MODE_180;

				EndDialog(hDlg, 0);
				return TRUE;
			} else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, 1);  
                return TRUE;
            }
            break;
    }
    return FALSE;
}

static int rotate_config(VDXFilterActivation *fa, const VDXFilterFunctions *ff, VDXHWND hWnd) {
	return DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_ROTATE), (HWND)hWnd, rotateDlgProc, (LPARAM)fa->filter_data);
}

static void rotate_string2(const VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int maxlen) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	_snprintf(buf, maxlen, " (%s)", VDTextWToA(g_szMode[mfd->mode]).c_str());
}

static void rotate_script_config(IVDXScriptInterpreter *isi, void *lpVoid, VDXScriptValue *argv, int argc) {
	VDXFilterActivation *fa = (VDXFilterActivation *)lpVoid;
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	mfd->mode	= argv[0].asInt();

	if (mfd->mode < 0 || mfd->mode > 2)
		mfd->mode = 0;
}

static VDXScriptFunctionDef rotate_func_defs[]={
	{ (VDXScriptFunctionPtr)rotate_script_config, "Config", "0i" },
	{ NULL },
};

static VDXScriptObject rotate_obj={
	NULL, rotate_func_defs
};

static bool rotate_script_line(VDXFilterActivation *fa, const VDXFilterFunctions *ff, char *buf, int buflen) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d)", mfd->mode);

	return true;
}

extern const VDXFilterDefinition g_VDVFRotate={
	0,0,NULL,
	"rotate",
	"Rotates an image by 90, 180, or 270 degrees.",
	NULL,NULL,
	sizeof(MyFilterData),
	NULL,NULL,
	rotate_run,
	rotate_param,
	rotate_config,
	NULL,
	NULL,
	NULL,

	&rotate_obj,
	rotate_script_line,
	rotate_string2,
};