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

#include <vd2/system/refcount.h>
#include <vd2/kasumi/pixmap.h>

#include "resource.h"
#include "filter.h"
#include "VBitmap.h"
#include "gui.h"

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"

extern HINSTANCE g_hInst;

///////////////////////

typedef struct MyFilterData {
	long x1, y1, x2, y2;
	bool use_alpha, use_alpha_temp;
	COLORREF color, color_temp;
	long x1_temp, y1_temp, x2_temp, y2_temp;

	HBRUSH hbrColor;

	sint32 mSourceWidth;
	sint32 mSourceHeight;

	FilterActivation* fa;
} MyFilterData;

static int fill_run32(const FilterActivation *fa, const FilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	unsigned long w,h;
	int r = (mfd->color & 0xff);
	int g = (mfd->color & 0xff00)>>8;
	int b = (mfd->color & 0xff0000)>>16;

	uint8* dst = (uint8*)fa->dst.data + mfd->y2*fa->dst.pitch + mfd->x1*4;
	h = fa->dst.h - mfd->y1 - mfd->y2;
	do {
		uint8* dst2 = dst;
		w = fa->dst.w - mfd->x1 - mfd->x2;
		if (mfd->use_alpha) {
			do {
				int a = dst2[3];
				int ra = 255-a;
				dst2[0] = uint8((dst2[0]*ra + b*a + 255)>>8);
				dst2[1] = uint8((dst2[1]*ra + g*a + 255)>>8);
				dst2[2] = uint8((dst2[2]*ra + r*a + 255)>>8);
				dst2+=4;
			} while(--w);
		} else {
			do {
				dst2[0] = uint8(b);
				dst2[1] = uint8(g);
				dst2[2] = uint8(r);
				dst2+=4;
			} while(--w);
		}

		dst += fa->dst.pitch;
	} while(--h);

	return 0;
}

static int fill_run64(const FilterActivation *fa, const FilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	VDPixmap* px = (VDPixmap*)fa->src.mpPixmap;

	unsigned long w,h;
	uint32 r = (mfd->color & 0xff);
	uint32 g = (mfd->color & 0xff00)>>8;
	uint32 b = (mfd->color & 0xff0000)>>16;
	r = r*px->info.ref_r/255;
	g = g*px->info.ref_g/255;
	b = b*px->info.ref_b/255;
	uint32 ref_a = px->info.ref_a;

	uint16* dst = (uint16*)fa->dst.data + mfd->y2*fa->dst.pitch/2 + mfd->x1*4;
	h = fa->dst.h - mfd->y1 - mfd->y2;
	do {
		uint16* dst2 = dst;
		w = fa->dst.w - mfd->x1 - mfd->x2;
		if (mfd->use_alpha) {
			do {
				uint32 a = dst2[3]*0x8000/ref_a;
				uint32 ra = 0x8000-a;
				dst2[0] = uint16((dst2[0]*ra + b*a + 0x4000)>>15);
				dst2[1] = uint16((dst2[1]*ra + g*a + 0x4000)>>15);
				dst2[2] = uint16((dst2[2]*ra + r*a + 0x4000)>>15);
				dst2+=4;
			} while(--w);
		} else {
			do {
				dst2[0] = uint16(b);
				dst2[1] = uint16(g);
				dst2[2] = uint16(r);
				dst2+=4;
			} while(--w);
		}

		dst += fa->dst.pitch/2;
	} while(--h);

	return 0;
}

static int fill_run(const FilterActivation *fa, const FilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	VDPixmap* px = (VDPixmap*)fa->src.mpPixmap;
	bool px_alpha = px->info.alpha_type!=FilterModPixmapInfo::kAlphaInvalid;
	if (mfd->use_alpha && !px_alpha)
		return 0;
	if (mfd->x1+mfd->x2 >= fa->dst.w) return 0;
	if (mfd->y1+mfd->y2 >= fa->dst.h) return 0;

	if (px->format==nsVDXPixmap::kPixFormat_XRGB8888) return fill_run32(fa,ff);
	if (px->format==nsVDXPixmap::kPixFormat_XRGB64) return fill_run64(fa,ff);
	return 0;
}

static long fill_param(FilterActivation *fa, const FilterFunctions *ff) {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888 && pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB64)
		return FILTERPARAM_NOT_SUPPORTED;

	pxldst.data = pxlsrc.data;
	pxldst.pitch = pxlsrc.pitch;
	return FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

static void ClipEditCallback(ClipEditInfo& info, void *pData) {
	HWND hDlg = (HWND)pData;
	MyFilterData *mfd = (MyFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);
	if (info.flags & info.edit_update) {
		mfd->x1 = info.x1;
		mfd->y1 = info.y1;
		mfd->x2 = info.x2;
		mfd->y2 = info.y2;
	}
	SetDlgItemInt(hDlg, IDC_CLIP_X0, mfd->x1, FALSE);
	SetDlgItemInt(hDlg, IDC_CLIP_X1, mfd->x2, FALSE);
	SetDlgItemInt(hDlg, IDC_CLIP_Y0, mfd->y1, FALSE);
	SetDlgItemInt(hDlg, IDC_CLIP_Y1, mfd->y2, FALSE);
	if (info.flags & info.edit_finish) mfd->fa->ifp->RedoFrame();
}

static void SetClipEdit(MyFilterData *mfd) {
	ClipEditInfo clip;
	clip.x1 = mfd->x1;
	clip.y1 = mfd->y1;
	clip.x2 = mfd->x2;
	clip.y2 = mfd->y2;
	if (mfd->fa->fma->fmpreview)
		mfd->fa->fma->fmpreview->SetClipEdit(clip);
}

static INT_PTR CALLBACK fillDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	MyFilterData *mfd;

	switch (message)
	{
		case WM_INITDIALOG:
		{
			mfd = (MyFilterData *)lParam;
			SetWindowLongPtr(hDlg, DWLP_USER, (LPARAM)mfd);

			mfd->color_temp = mfd->color;
			mfd->use_alpha_temp = mfd->use_alpha;
			mfd->x1_temp = mfd->x1;
			mfd->y1_temp = mfd->y1;
			mfd->x2_temp = mfd->x2;
			mfd->y2_temp = mfd->y2;
			mfd->hbrColor = CreateSolidBrush(mfd->color);

			HWND hWndAlpha = GetDlgItem(hDlg, IDC_USE_ALPHA);
			SendMessage(hWndAlpha,  BM_SETCHECK, mfd->use_alpha ? BST_CHECKED:BST_UNCHECKED, 0);
			SetDlgItemInt(hDlg, IDC_CLIP_X0, mfd->x1, FALSE);
			SetDlgItemInt(hDlg, IDC_CLIP_X1, mfd->x2, FALSE);
			SetDlgItemInt(hDlg, IDC_CLIP_Y0, mfd->y1, FALSE);
			SetDlgItemInt(hDlg, IDC_CLIP_Y1, mfd->y2, FALSE);

			if (mfd->fa->fma->fmpreview) {
				PreviewExInfo mode;
				mode.flags = mode.thick_border | mode.custom_draw | mode.no_exit;
				mfd->fa->fma->fmpreview->DisplayEx((VDXHWND)hDlg,mode);
				mfd->fa->fma->fmpreview->SetClipEditCallback(ClipEditCallback, hDlg);
				SetClipEdit(mfd);
			}
		}
		return (TRUE);

		case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
			{
				mfd = (MyFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);
				if (mfd->hbrColor) {
					DeleteObject(mfd->hbrColor);
					mfd->hbrColor = NULL;
				}
				EndDialog(hDlg, 0);
			}
			return TRUE;

		case IDCANCEL:
			mfd = (MyFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);
			mfd->color = mfd->color_temp;
			mfd->use_alpha = mfd->use_alpha_temp;
			mfd->x1 = mfd->x1_temp;
			mfd->y1 = mfd->y1_temp;
			mfd->x2 = mfd->x2_temp;
			mfd->y2 = mfd->y2_temp;
			if (mfd->hbrColor) {
				DeleteObject(mfd->hbrColor);
				mfd->hbrColor = NULL;
			}
			EndDialog(hDlg, 1);
			return TRUE;

		case IDC_PICK_COLOR:
			mfd = (MyFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);

			if (guiChooseColor(hDlg, mfd->color)) {
				DeleteObject(mfd->hbrColor);
				mfd->hbrColor = CreateSolidBrush(mfd->color);
				RedrawWindow(GetDlgItem(hDlg, IDC_COLOR), NULL, NULL, RDW_ERASE|RDW_INVALIDATE|RDW_UPDATENOW);
				mfd->fa->ifp->RedoFrame();
			}

			return TRUE;

		case IDC_USE_ALPHA:
			mfd = (MyFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);
			mfd->use_alpha = IsDlgButtonChecked(hDlg, IDC_USE_ALPHA)!=0;

			mfd->fa->ifp->RedoFrame();
			return TRUE;

		case IDC_CLIP_X0:
			mfd = (MyFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);
			mfd->x1 = GetDlgItemInt(hDlg,IDC_CLIP_X0,0,false);
			SetClipEdit(mfd);
			mfd->fa->ifp->RedoFrame();
			return TRUE;

		case IDC_CLIP_X1:
			mfd = (MyFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);
			mfd->x2 = GetDlgItemInt(hDlg,IDC_CLIP_X1,0,false);
			SetClipEdit(mfd);
			mfd->fa->ifp->RedoFrame();
			return TRUE;

		case IDC_CLIP_Y0:
			mfd = (MyFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);
			mfd->y1 = GetDlgItemInt(hDlg,IDC_CLIP_Y0,0,false);
			SetClipEdit(mfd);
			mfd->fa->ifp->RedoFrame();
			return TRUE;

		case IDC_CLIP_Y1:
			mfd = (MyFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);
			mfd->y2 = GetDlgItemInt(hDlg,IDC_CLIP_Y1,0,false);
			SetClipEdit(mfd);
			mfd->fa->ifp->RedoFrame();
			return TRUE;
		}
		break;

	case WM_DRAWITEM:
		{
			mfd = (MyFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);
			DRAWITEMSTRUCT* ds = (DRAWITEMSTRUCT*)lParam;
			FillRect(ds->hDC,&ds->rcItem,mfd->hbrColor);
		}
		return TRUE;
	}
	return FALSE;
}

static int fill_config(FilterActivation *fa, const FilterFunctions *ff, VDXHWND hWnd) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	mfd->mSourceWidth = fa->src.w;
	mfd->mSourceHeight = fa->src.h;
	mfd->fa = fa;

	return DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_FILL), (HWND)hWnd, fillDlgProc, (LPARAM)mfd);
}

static void fill_string2(const FilterActivation *fa, const FilterFunctions *ff, char *buf, int maxlen) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	_snprintf(buf, maxlen, " (color: #%02X%02X%02X)", mfd->color&0xff, (mfd->color>>8)&0xff, (mfd->color>>16)&0xff);
}

static void fill_script_config(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterActivation *fa = (FilterActivation *)lpVoid;
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	mfd->x1		= argv[0].asInt();
	mfd->y1		= argv[1].asInt();
	mfd->x2		= argv[2].asInt();
	mfd->y2		= argv[3].asInt();
	int arg4	= argv[4].asInt();
	mfd->color	= arg4 & 0xFFFFFF;
	mfd->use_alpha = (arg4 & 0x80000000)!=0;
}

static ScriptFunctionDef fill_func_defs[]={
	{ (ScriptFunctionPtr)fill_script_config, "Config", "0iiiii" },
	{ NULL },
};

static CScriptObject fill_obj={
	NULL, fill_func_defs
};

static bool fill_script_line(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	int flags = 0;
	if (mfd->use_alpha)
		flags += 0x80000000;
	_snprintf(buf, buflen, "Config(%d,%d,%d,%d,0x%06lx)", mfd->x1, mfd->y1, mfd->x2, mfd->y2, mfd->color | flags);

	return true;
}

FilterDefinition filterDef_fill={
	0,0,NULL,
	"fill",
	"Fills an image rectangle with a color.",
	NULL,NULL,
	sizeof(MyFilterData),
	NULL,NULL,
	fill_run,
	fill_param,
	fill_config,
	NULL,
	NULL,
	NULL,

	&fill_obj,
	fill_script_line,
	fill_string2,
};

