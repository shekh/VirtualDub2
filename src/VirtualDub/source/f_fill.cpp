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
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDLib/Dialog.h>

#include "resource.h"
#include "filter.h"
#include "VBitmap.h"
#include "gui.h"

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"

extern HINSTANCE g_hInst;

///////////////////////

struct FillParam {
	long x1, y1, x2, y2;
	bool use_alpha;
	bool outside;
	COLORREF color;

	FillParam() {
		x1 = 0;
		y1 = 0;
		x2 = 0;
		y2 = 0;
		use_alpha = false;
		outside = false;
		color = 0;
	}
};

class VDVideoFilterFill : public VDXVideoFilter {
public:
	FillParam param;

	VDVideoFilterFill(){}
	VDVideoFilterFill(const VDVideoFilterFill& a) { param = a.param; }

	virtual void Run();
	virtual uint32 GetParams();
	virtual bool Configure(VDXHWND hwnd);
	virtual void GetSettingString(char *buf, int maxlen);
	virtual void GetScriptString(char *buf, int maxlen);

	void ScriptConfig(IVDXScriptInterpreter *env, const VDXScriptValue *argv, int argc);

	VDXVF_DECLARE_SCRIPT_METHODS();
};

void fill_alpha_32(const VDPixmap& pxdst, const vdrect32& rDst, uint32 color) {
	unsigned long w,h;
	int r = (color & 0xff);
	int g = (color & 0xff00)>>8;
	int b = (color & 0xff0000)>>16;

	uint8* dst = (uint8*)pxdst.data + rDst.top*pxdst.pitch + rDst.left*4;
	h = rDst.bottom-rDst.top;
	while (h>0) {
		uint8* dst2 = dst;
		w = rDst.right-rDst.left;
		while (w>0) {
			int a = dst2[3];
			int ra = 255-a;
			dst2[0] = uint8((dst2[0]*ra + b*a + 255)>>8);
			dst2[1] = uint8((dst2[1]*ra + g*a + 255)>>8);
			dst2[2] = uint8((dst2[2]*ra + r*a + 255)>>8);
			dst2+=4;
			w--;
		}

		dst += pxdst.pitch;
		h--;
	}
}

void fill_alpha_64(const VDPixmap& pxdst, const vdrect32& rDst, uint32 color) {
	unsigned long w,h;
	uint32 r = (color & 0xff);
	uint32 g = (color & 0xff00)>>8;
	uint32 b = (color & 0xff0000)>>16;
	r = r*pxdst.info.ref_r/255;
	g = g*pxdst.info.ref_g/255;
	b = b*pxdst.info.ref_b/255;
	uint32 ref_a = pxdst.info.ref_a;

	uint16* dst = (uint16*)pxdst.data + rDst.top*pxdst.pitch/2 + rDst.left*4;
	h = rDst.bottom-rDst.top;
	while (h>0) {
		uint16* dst2 = dst;
		w = rDst.right-rDst.left;
		while (w>0) {
			uint32 a = dst2[3]*0x8000/ref_a;
			uint32 ra = 0x8000-a;
			dst2[0] = uint16((dst2[0]*ra + b*a + 0x4000)>>15);
			dst2[1] = uint16((dst2[1]*ra + g*a + 0x4000)>>15);
			dst2[2] = uint16((dst2[2]*ra + r*a + 0x4000)>>15);
			dst2+=4;
			w--;
		}

		dst += pxdst.pitch/2;
		h--;
	}
}

void VDPixmapRectFill_Blend(const VDPixmap& px, const vdrect32& rDst, uint32 c) {
	using namespace vd2;
	switch (px.format) {
	case kPixFormat_XRGB8888:
		fill_alpha_32(px,rDst,c);
		return;
	case kPixFormat_XRGB64:
		fill_alpha_64(px,rDst,c);
		return;
	}
}

void VDPixmapRectFillRGB32(const VDPixmap& px, const vdrect32f& rDst, uint32 c);

namespace {
	int revcolor(int c) {
		return ((c>>16)&0xff) | (c&0xff00) | ((c&0xff)<<16);
	}
}
void VDVideoFilterFill::Run() {
	VDPixmap pxdst = VDPixmap::copy(*fa->dst.mpPixmap);
	VDPixmap pxsrc = VDPixmap::copy(*fa->src.mpPixmap);

	if (fa->fma && fa->fma->fmpixmap) {
		pxdst.info = *fa->fma->fmpixmap->GetPixmapInfo(fa->dst.mpPixmap);
		pxsrc.info = *fa->fma->fmpixmap->GetPixmapInfo(fa->src.mpPixmap);
	}

	bool px_alpha = pxsrc.info.alpha_type!=FilterModPixmapInfo::kAlphaInvalid;
	if (param.use_alpha && !px_alpha)
		return;

	int w = pxdst.w;
	int h = pxdst.h;

	if (param.x1+param.x2 >= w) return;
	if (param.y1+param.y2 >= h) return;

	if (param.use_alpha) {
		int x1 = 0;
		int y1 = 0;
		int x2 = param.x1;
		int y2 = param.y1;
		int x3 = w-param.x2;
		int y3 = h-param.y2;
		int x4 = w;
		int y4 = h;
		uint32 fill = param.color;
		if (param.outside) {
			VDPixmapRectFill_Blend(pxdst, vdrect32(x1, y1, x4, y2), fill);
			VDPixmapRectFill_Blend(pxdst, vdrect32(x1, y2, x2, y3), fill);
			VDPixmapRectFill_Blend(pxdst, vdrect32(x3, y2, x4, y3), fill);
			VDPixmapRectFill_Blend(pxdst, vdrect32(x1, y3, x4, y4), fill);
		} else {
			VDPixmapRectFill_Blend(pxdst, vdrect32(x2, y2, x3, y3), fill);
		}
	} else {
		float fx1 = 0.0f;
		float fy1 = 0.0f;
		float fx2 = float(param.x1);
		float fy2 = float(param.y1);
		float fx3 = float(w-param.x2);
		float fy3 = float(h-param.y2);
		float fx4 = float(w);
		float fy4 = float(h);
		uint32 fill = revcolor(param.color);
		if (param.outside) {
			VDPixmapRectFillRGB32(pxdst, vdrect32f(fx1, fy1, fx4, fy2), fill);
			VDPixmapRectFillRGB32(pxdst, vdrect32f(fx1, fy2, fx2, fy3), fill);
			VDPixmapRectFillRGB32(pxdst, vdrect32f(fx3, fy2, fx4, fy3), fill);
			VDPixmapRectFillRGB32(pxdst, vdrect32f(fx1, fy3, fx4, fy4), fill);
		} else {
			VDPixmapRectFillRGB32(pxdst, vdrect32f(fx2, fy2, fx3, fy3), fill);
		}
	}
}

uint32 VDVideoFilterFill::GetParams() {
	using namespace vd2;
	const VDXPixmapLayout& src = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& dst = *fa->dst.mpPixmapLayout;
	VDPixmapFormatEx format = ExtractBaseFormat(src.format);

	if (!param.use_alpha) {
		switch(format) {
		case kPixFormat_XRGB8888:
		case kPixFormat_XRGB64:
		case kPixFormat_RGB_Planar:
		case kPixFormat_RGBA_Planar:
		case kPixFormat_RGB_Planar16:
		case kPixFormat_RGBA_Planar16:
		case kPixFormat_YUV444_Planar:
		case kPixFormat_YUV422_Planar:
		case kPixFormat_YUV411_Planar:
		case kPixFormat_YUV422_Planar16:
		case kPixFormat_YUV444_Planar16:
		case kPixFormat_YUV422_Alpha_Planar16:
		case kPixFormat_YUV444_Alpha_Planar16:
		case kPixFormat_YUV422_Alpha_Planar:
		case kPixFormat_YUV444_Alpha_Planar:
		case kPixFormat_Y8:
		case kPixFormat_Y16:
		case kPixFormat_VDXA_RGB:
		case kPixFormat_VDXA_YUV:
		case kPixFormat_YUV420_Planar:
		case kPixFormat_YUV410_Planar:
		case kPixFormat_YUV420_Planar16:
		case kPixFormat_YUV420_Alpha_Planar16:
		case kPixFormat_YUV420_Alpha_Planar:
			break;
		default:
			return FILTERPARAM_NOT_SUPPORTED;
		}
	} else {
		switch(format) {
		case kPixFormat_XRGB8888:
		case kPixFormat_XRGB64:
			break;
		case kPixFormat_RGBA_Planar:
		case kPixFormat_RGBA_Planar16:
		case kPixFormat_YUV422_Alpha_Planar16:
		case kPixFormat_YUV444_Alpha_Planar16:
		case kPixFormat_YUV422_Alpha_Planar:
		case kPixFormat_YUV444_Alpha_Planar:
		default:
			return FILTERPARAM_NOT_SUPPORTED;
		}
	}

	dst.data = src.data;
	dst.pitch = src.pitch;
	return FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

class VDVFilterFillDialog : public VDDialogFrameW32 {
public:
	VDVFilterFillDialog()
		: VDDialogFrameW32(IDD_FILTER_FILL)
	{
		hbrColor = 0; 
		fa = 0;
		filter = 0;
	}
	~VDVFilterFillDialog() {
		if (hbrColor) {
			DeleteObject(hbrColor);
		}
	}

	bool OnLoaded();
	bool OnCommand(uint32 id, uint32 extcode);
	VDZINT_PTR DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);

	FilterActivation* fa;
	VDVideoFilterFill* filter;
	HBRUSH hbrColor;

	sint32 mSourceWidth;
	sint32 mSourceHeight;

	static void ClipEditCallback(ClipEditInfo& info, void *pData);
	void SetClipEdit();
};

void VDVFilterFillDialog::ClipEditCallback(ClipEditInfo& info, void *pData) {
	VDVFilterFillDialog* pthis = (VDVFilterFillDialog*)pData;
	FillParam& param = pthis->filter->param;
	if (info.flags & info.edit_update) {
		param.x1 = info.x1;
		param.y1 = info.y1;
		param.x2 = info.x2;
		param.y2 = info.y2;
	}
	SetDlgItemInt(pthis->mhdlg, IDC_CLIP_X0, param.x1, FALSE);
	SetDlgItemInt(pthis->mhdlg, IDC_CLIP_X1, param.x2, FALSE);
	SetDlgItemInt(pthis->mhdlg, IDC_CLIP_Y0, param.y1, FALSE);
	SetDlgItemInt(pthis->mhdlg, IDC_CLIP_Y1, param.y2, FALSE);
	if (info.flags & info.edit_finish) pthis->fa->ifp->RedoFrame();
}

void VDVFilterFillDialog::SetClipEdit() {
	FillParam& param = filter->param;
	ClipEditInfo clip;
	clip.x1 = param.x1;
	clip.y1 = param.y1;
	clip.x2 = param.x2;
	clip.y2 = param.y2;
	if (fa->fma->fmpreview)
		fa->fma->fmpreview->SetClipEdit(clip);
}

bool VDVFilterFillDialog::OnLoaded() {
	hbrColor = CreateSolidBrush(filter->param.color);

	CheckDlgButton(mhdlg, IDC_USE_ALPHA, filter->param.use_alpha ? BST_CHECKED:BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_OUTSIDE, filter->param.outside ? BST_CHECKED:BST_UNCHECKED);
	SetDlgItemInt(mhdlg, IDC_CLIP_X0, filter->param.x1, FALSE);
	SetDlgItemInt(mhdlg, IDC_CLIP_X1, filter->param.x2, FALSE);
	SetDlgItemInt(mhdlg, IDC_CLIP_Y0, filter->param.y1, FALSE);
	SetDlgItemInt(mhdlg, IDC_CLIP_Y1, filter->param.y2, FALSE);

	if (fa->fma->fmpreview) {
		PreviewExInfo mode;
		mode.flags = mode.thick_border | mode.custom_draw | mode.no_exit;
		fa->fma->fmpreview->DisplayEx((VDXHWND)mhdlg,mode);
		fa->fma->fmpreview->SetClipEditCallback(ClipEditCallback, this);
		SetClipEdit();
	}
	return true;
}

bool VDVFilterFillDialog::OnCommand(uint32 id, uint32 extcode) {
	switch(id) {
	case IDC_PICK_COLOR:
		if (guiChooseColor(mhdlg, filter->param.color)) {
			DeleteObject(hbrColor);
			hbrColor = CreateSolidBrush(filter->param.color);
			RedrawWindow(GetDlgItem(mhdlg, IDC_COLOR), NULL, NULL, RDW_ERASE|RDW_INVALIDATE|RDW_UPDATENOW);
			fa->ifp->RedoFrame();
		}
		return TRUE;

	case IDC_USE_ALPHA:
		filter->param.use_alpha = IsDlgButtonChecked(mhdlg, IDC_USE_ALPHA)!=0;
		fa->ifp->RedoSystem();
		return TRUE;

	case IDC_OUTSIDE:
		filter->param.outside = IsDlgButtonChecked(mhdlg, IDC_OUTSIDE)!=0;
		fa->ifp->RedoFrame();
		return TRUE;

	case IDC_CLIP_X0:
		filter->param.x1 = GetDlgItemInt(mhdlg,IDC_CLIP_X0,0,false);
		SetClipEdit();
		fa->ifp->RedoFrame();
		return TRUE;

	case IDC_CLIP_X1:
		filter->param.x2 = GetDlgItemInt(mhdlg,IDC_CLIP_X1,0,false);
		SetClipEdit();
		fa->ifp->RedoFrame();
		return TRUE;

	case IDC_CLIP_Y0:
		filter->param.y1 = GetDlgItemInt(mhdlg,IDC_CLIP_Y0,0,false);
		SetClipEdit();
		fa->ifp->RedoFrame();
		return TRUE;

	case IDC_CLIP_Y1:
		filter->param.y2 = GetDlgItemInt(mhdlg,IDC_CLIP_Y1,0,false);
		SetClipEdit();
		fa->ifp->RedoFrame();
		return TRUE;
	}

	return false;
}

VDZINT_PTR VDVFilterFillDialog::DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam) {
	switch (msg) {
	case WM_DRAWITEM:
		{
			DRAWITEMSTRUCT* ds = (DRAWITEMSTRUCT*)lParam;
			FillRect(ds->hDC,&ds->rcItem,hbrColor);
		}
		return TRUE;
	}

	return VDDialogFrameW32::DlgProc(msg,wParam,lParam);
}

bool VDVideoFilterFill::Configure(VDXHWND hwnd) {
	VDVFilterFillDialog dlg;
	dlg.mSourceWidth = fa->src.w;
	dlg.mSourceHeight = fa->src.h;
	dlg.filter = this;
	dlg.fa = fa;
	FillParam old_param = param;

	if (!dlg.ShowDialog((VDGUIHandle)hwnd)) {
		param = old_param;
		return false;
	}
	return true;
}

void VDVideoFilterFill::GetSettingString(char *buf, int maxlen) {
	_snprintf(buf, maxlen, " (color: #%02X%02X%02X)", param.color&0xff, (param.color>>8)&0xff, (param.color>>16)&0xff);
}

void VDVideoFilterFill::ScriptConfig(IVDXScriptInterpreter *env, const VDXScriptValue *argv, int argc) {
	param.x1		= argv[0].asInt();
	param.y1		= argv[1].asInt();
	param.x2		= argv[2].asInt();
	param.y2		= argv[3].asInt();
	int arg4	= argv[4].asInt();
	param.color	= arg4 & 0xFFFFFF;
	param.use_alpha = (arg4 & 0x80000000)!=0;
	param.outside = (arg4 & 0x40000000)!=0;
}

void VDVideoFilterFill::GetScriptString(char *buf, int maxlen) {
	int flags = 0;
	if (param.use_alpha)
		flags += 0x80000000;
	if (param.outside)
		flags += 0x40000000;
	_snprintf(buf, maxlen, "Config(%d,%d,%d,%d,0x%06lx)", param.x1, param.y1, param.x2, param.y2, param.color | flags);
}

VDXVF_BEGIN_SCRIPT_METHODS(VDVideoFilterFill)
	VDXVF_DEFINE_SCRIPT_METHOD(VDVideoFilterFill, ScriptConfig, "iiiii")
VDXVF_END_SCRIPT_METHODS()

extern const VDXFilterDefinition filterDef_fill = VDXVideoFilterDefinition<VDVideoFilterFill>(
	NULL,
	"fill",
	"Fills an image rectangle with a color.\n"
	"Fills border (letterbox) with a color.\n"
);

#pragma warning(disable: 4505)
