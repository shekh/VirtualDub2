#include "stdafx.h"
#include <vd2/system/vdalloc.h>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofilt.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/region.h>
#include <vd2/Kasumi/pixel.h>
#include <windows.h>
#include <commctrl.h>
#include "resource.h"

extern bool guiChooseColor(HWND hwnd, COLORREF& rgbOld);
extern VDStringA VDEncodeScriptString(const VDStringSpanA& sa);

COLORREF argb_to_colorref(uint32 color) {
	int r = (color & 0xFF0000) >> 16;
	int g = (color & 0xFF00) >> 8;
	int b = (color & 0xFF);
	return r | (g<<8) | (b<<16);
}

uint32 colorref_to_argb(COLORREF color) {
	int r = (color & 0xFF);
	int g = (color & 0xFF00) >> 8;
	int b = (color & 0xFF0000) >> 16;
	return 0xFF000000 | (r<<16) | (g<<8) | (b);
}

void VDPixmapConvertGDIPathToPath(VDPixmapPathRasterizer& rast, vdfastvector<POINT>& point, vdfastvector<unsigned char>& flag) {
	int loop_start=0;
	{for(int i=0; i<(int)point.size(); i++){
		POINT* p0 = &point[i];
		char f0 = flag[i];

		if(f0==PT_MOVETO){
			loop_start = i;
			continue;
		}

		if(f0 & PT_BEZIERTO){
			vdint2 bp[4];
			bp[0].set(p0[-1].x, p0[-1].y);
			bp[1].set(p0[0].x, p0[0].y);
			bp[2].set(p0[1].x, p0[1].y);
			bp[3].set(p0[2].x, p0[2].y);
			rast.CubicBezier(bp);
			i+=2;
		} else if(f0 & PT_LINETO){
			vdint2 bp[2];
			bp[0].set(p0[-1].x, p0[-1].y);
			bp[1].set(p0[0].x,  p0[0].y);
			rast.Line(bp[0], bp[1]);
		}

		if(flag[i] & PT_CLOSEFIGURE){
			POINT* p0 = &point[i];
			POINT* p1 = &point[loop_start];
			vdint2 bp[2];
			bp[0].set(p0[0].x, p0[0].y);
			bp[1].set(p1[0].x, p1[0].y);
			rast.Line(bp[0], bp[1]);
		}
	}}
}

///////////////////////////////////////////////////////////////////////////////

struct VFDrawTextParam {
	float x0,y0,x1,y1;
	VDStringW face;
	float size;
	int weight;
	bool italic;
	int align;

	VDStringW text;

	uint32 color;
	uint32 shadow_color;
	float shadow_width;

	VFDrawTextParam() {
		x0 = 0;
		y0 = 0;
		x1 = 0;
		y1 = 0;
		face = L"Arial";
		size = 24;
		weight = FW_NORMAL;
		italic = false;
		align = 1;
		text = L"Text";
		color = 0xFFFFFFFF;
		shadow_color = 0xFF000000;
		shadow_width = 1;
	}
};

///////////////////////////////////////////////////////////////////////////////

class VDVFilterDrawText;

class VDDrawTextDialog : public VDDialogFrameW32 {
public:
	int mSourceWidth,mSourceHeight;
	IVDXFilterPreview2 *fp2;
	IFilterModPreview *fmpreview;
	int preview_flags;
	int clip_flags;

	VDVFilterDrawText *fa;
	VFDrawTextParam& param;

	VDDrawTextDialog(VFDrawTextParam& param, VDVFilterDrawText *fa);
	bool OnLoaded();

	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void SetClipEdit();
	void init_crop();
	void apply_crop();
	void init_font();
	void pick_font();
	void init_shadow_width();
	static void ClipEditCallback(ClipEditInfo& info, void *pData);
	void redo();
};

VDDrawTextDialog::VDDrawTextDialog(VFDrawTextParam& param, VDVFilterDrawText *fa)
	: VDDialogFrameW32(IDD_FILTER_DRAWTEXT),
	param(param),
	fa(fa)
{
	mSourceWidth = 0;
	mSourceHeight = 0;
	fp2 = 0;
	fmpreview = 0;
	preview_flags = PreviewExInfo::thick_border | PreviewExInfo::custom_draw;
	clip_flags = 0;
}

void VDDrawTextDialog::init_crop() {
	if(GetFocus()!=GetDlgItem(mhdlg,IDC_CLIP_X0)) SetDlgItemInt(mhdlg, IDC_CLIP_X0, int(param.x0), FALSE);
	if(GetFocus()!=GetDlgItem(mhdlg,IDC_CLIP_X1)) SetDlgItemInt(mhdlg, IDC_CLIP_X1, int(param.x1), FALSE);
	if(GetFocus()!=GetDlgItem(mhdlg,IDC_CLIP_Y0)) SetDlgItemInt(mhdlg, IDC_CLIP_Y0, int(param.y0), FALSE);
	if(GetFocus()!=GetDlgItem(mhdlg,IDC_CLIP_Y1)) SetDlgItemInt(mhdlg, IDC_CLIP_Y1, int(param.y1), FALSE);
}

void VDDrawTextDialog::ClipEditCallback(ClipEditInfo& info, void *pData) {
	VDDrawTextDialog* dlg = (VDDrawTextDialog*)pData;
	if (info.flags & info.init_size) {
		dlg->mSourceWidth = info.w;
		dlg->mSourceHeight = info.h;
		SendMessage(GetDlgItem(dlg->mhdlg, IDC_CLIP_X0_SPIN), UDM_SETRANGE, 0, (LPARAM)MAKELONG(info.w,0));
		SendMessage(GetDlgItem(dlg->mhdlg, IDC_CLIP_X1_SPIN), UDM_SETRANGE, 0, (LPARAM)MAKELONG(info.w,0));
		SendMessage(GetDlgItem(dlg->mhdlg, IDC_CLIP_Y0_SPIN), UDM_SETRANGE, 0, (LPARAM)MAKELONG(info.h,0));
		SendMessage(GetDlgItem(dlg->mhdlg, IDC_CLIP_Y1_SPIN), UDM_SETRANGE, 0, (LPARAM)MAKELONG(info.h,0));
	}
	if (info.flags & info.edit_update) {
		dlg->param.x0 = float(info.x1);
		dlg->param.y0 = float(info.y1);
		dlg->param.x1 = float(info.x2);
		dlg->param.y1 = float(info.y2);
	}
	dlg->init_crop();
	if (info.flags & info.edit_finish) dlg->apply_crop();
}

void VDDrawTextDialog::SetClipEdit() {
	ClipEditInfo clip;
	clip.x1 = int(param.x0);
	clip.y1 = int(param.y0);
	clip.x2 = int(param.x1);
	clip.y2 = int(param.y1);
	clip.flags = clip_flags;
	if (fmpreview)
		fmpreview->SetClipEdit(clip);
}

void VDDrawTextDialog::init_font() {
	VDStringW s;
	s.sprintf(L"%s, %g pt",param.face.c_str(), param.size);
	SetDlgItemTextW(mhdlg, IDC_FONT, s.c_str());
}

void VDDrawTextDialog::pick_font() {
	LOGFONTW lf = {0};
	CHOOSEFONTW cf = {0};

	wcscpy(lf.lfFaceName,param.face.c_str());
	lf.lfHeight = LONG(param.size);
	HDC hdc = GetDC(mhdlg);
	lf.lfHeight = -LONG(param.size*GetDeviceCaps(hdc, LOGPIXELSY)/72);
	ReleaseDC(mhdlg, hdc);
	lf.lfItalic = param.italic;
	lf.lfWeight = param.weight;

	cf.lStructSize = sizeof(CHOOSEFONTW);
	cf.hwndOwner = mhdlg;
	cf.lpLogFont = &lf;
	cf.Flags = CF_TTONLY | CF_INITTOLOGFONTSTRUCT | CF_NOSCRIPTSEL;

	if(!::ChooseFontW(&cf)) return;

	param.size = float(cf.iPointSize / 10.0);
	param.face = lf.lfFaceName;
	param.italic = lf.lfItalic!=0;
	param.weight = lf.lfWeight;

	init_font();
	redo();
}

void VDDrawTextDialog::init_shadow_width() {
	if(GetFocus()!=GetDlgItem(mhdlg,IDC_SHADOW_EDIT)) SetDlgItemInt(mhdlg, IDC_SHADOW_EDIT, int(param.shadow_width), FALSE);
}

bool VDDrawTextDialog::OnLoaded() {
	init_crop();
	init_font();
	init_shadow_width();
	SendMessage(GetDlgItem(mhdlg, IDC_SHADOW_SPIN), UDM_SETRANGE, 0, (LPARAM)MAKELONG(100,0));

	CheckDlgButton(mhdlg, IDC_ALIGN_LEFT,   param.align==0 ? BST_CHECKED:BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_ALIGN_CENTER, param.align==1 ? BST_CHECKED:BST_UNCHECKED);
	CheckDlgButton(mhdlg, IDC_ALIGN_RIGHT,  param.align==2 ? BST_CHECKED:BST_UNCHECKED);

	if (fmpreview) {
		PreviewExInfo mode;
		mode.flags = preview_flags;
		fmpreview->SetClipEditCallback(ClipEditCallback, this);
		fmpreview->DisplayEx((VDXHWND)mhdlg,mode);
		SetClipEdit();
	}

	if (fp2) {
		EnableWindow(GetDlgItem(mhdlg, IDC_PREVIEW), TRUE);
		fp2->InitButton((VDXHWND)GetDlgItem(mhdlg, IDC_PREVIEW));
	}

	SetDlgItemTextW(mhdlg,IDC_TEXT,param.text.c_str());

	return true;
}

INT_PTR VDDrawTextDialog::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
			EndDialog(mhdlg,1);
			return TRUE;

		case IDCANCEL:
			EndDialog(mhdlg,0);
			return TRUE;

		case IDC_PREVIEW:
			if (fp2)
				fp2->Toggle((VDXHWND)mhdlg);

		case IDC_TEXT:
			if (HIWORD(wParam)==EN_CHANGE) {
				HWND w = GetDlgItem(mhdlg,IDC_TEXT);
				int cch = SendMessageW(w,WM_GETTEXTLENGTH,0,0);
				param.text.resize(cch);
				if(cch>0) GetWindowTextW(w,&param.text[0],cch+1);
				redo();
			}
			break;

		case IDC_ALIGN_LEFT:
			if (IsDlgButtonChecked(mhdlg,IDC_ALIGN_LEFT)) {
				param.align = 0;
				redo();
			}
			break;

		case IDC_ALIGN_CENTER:
			if (IsDlgButtonChecked(mhdlg,IDC_ALIGN_CENTER)) {
				param.align = 1;
				redo();
			}
			break;

		case IDC_ALIGN_RIGHT:
			if (IsDlgButtonChecked(mhdlg,IDC_ALIGN_RIGHT)) {
				param.align = 2;
				redo();
			}
			break;

		case IDC_SHADOW_EDIT:
			if (HIWORD(wParam)==EN_CHANGE) {
				param.shadow_width = (float)GetDlgItemInt(mhdlg,IDC_SHADOW_EDIT,0,false);
				redo();
				return TRUE;
			}
			break;

		case IDC_CLIP_X0:
			if (HIWORD(wParam)==EN_CHANGE) {
				param.x0 = (float)GetDlgItemInt(mhdlg,IDC_CLIP_X0,0,false);
				SetClipEdit();
				apply_crop();
				return TRUE;
			}
			break;

		case IDC_CLIP_X1:
			if (HIWORD(wParam)==EN_CHANGE) {
				param.x1 = (float)GetDlgItemInt(mhdlg,IDC_CLIP_X1,0,false);
				SetClipEdit();
				apply_crop();
				return TRUE;
			}
			break;

		case IDC_CLIP_Y0:
			if (HIWORD(wParam)==EN_CHANGE) {
				param.y0 = (float)GetDlgItemInt(mhdlg,IDC_CLIP_Y0,0,false);
				SetClipEdit();
				apply_crop();
				return TRUE;
			}
			break;

		case IDC_CLIP_Y1:
			if (HIWORD(wParam)==EN_CHANGE) {
				param.y1 = (float)GetDlgItemInt(mhdlg,IDC_CLIP_Y1,0,false);
				SetClipEdit();
				apply_crop();
				return TRUE;
			}
			break;

		case IDC_PICK_COLOR1:
			{
				COLORREF c = argb_to_colorref(param.color);
				if (guiChooseColor(mhdlg, c)) {
					param.color = colorref_to_argb(c);
					RedrawWindow(GetDlgItem(mhdlg, IDC_COLOR1), NULL, NULL, RDW_ERASE|RDW_INVALIDATE|RDW_UPDATENOW);
					if(fp2) fp2->RedoFrame();
				}
			}
			return TRUE;

		case IDC_PICK_COLOR0:
			{
				COLORREF c = argb_to_colorref(param.shadow_color);
				if (guiChooseColor(mhdlg, c)) {
					param.shadow_color = colorref_to_argb(c);
					RedrawWindow(GetDlgItem(mhdlg, IDC_COLOR0), NULL, NULL, RDW_ERASE|RDW_INVALIDATE|RDW_UPDATENOW);
					if(fp2) fp2->RedoFrame();
				}
			}
			return TRUE;

		case IDC_PICK_FONT:
			pick_font();
			return TRUE;
		}
		break;

	case WM_VSCROLL:
		if ((HWND)lParam==GetDlgItem(mhdlg,IDC_SHADOW_SPIN)) {
			param.shadow_width = (float)GetDlgItemInt(mhdlg,IDC_SHADOW_EDIT,0,false);
			redo();
		} else {
			param.x0 = (float)GetDlgItemInt(mhdlg,IDC_CLIP_X0,0,false);
			param.x1 = (float)GetDlgItemInt(mhdlg,IDC_CLIP_X1,0,false);
			param.y0 = (float)GetDlgItemInt(mhdlg,IDC_CLIP_Y0,0,false);
			param.y1 = (float)GetDlgItemInt(mhdlg,IDC_CLIP_Y1,0,false);
			SetClipEdit();
			apply_crop();
		}
		break;

	case WM_DRAWITEM:
		if(wParam==IDC_COLOR1){
			DRAWITEMSTRUCT* ds = (DRAWITEMSTRUCT*)lParam;
			HBRUSH br = CreateSolidBrush(argb_to_colorref(param.color));
			FillRect(ds->hDC, &ds->rcItem, br);
			DeleteObject(br);
		}
		if(wParam==IDC_COLOR0){
			DRAWITEMSTRUCT* ds = (DRAWITEMSTRUCT*)lParam;
			HBRUSH br = CreateSolidBrush(argb_to_colorref(param.shadow_color));
			FillRect(ds->hDC, &ds->rcItem, br);
			DeleteObject(br);
		}
		return TRUE;
	}

	return VDDialogFrameW32::DlgProc(message, wParam, lParam);
}

void VDDrawTextDialog::apply_crop() {
	redo();
}

///////////////////////////////////////////////////////////////////////////////

class VDVFilterDrawText : public VDXVideoFilter {
public:
	VDPixmapRegion  mTextRegion;
	VDPixmapRegion  mTextBorderRegion;

	VFDrawTextParam param;

	VDVFilterDrawText(){}
	VDVFilterDrawText(const VDVFilterDrawText& a) {
		param = a.param;
	}

	uint32 GetParams();
	void Start();
	void End();
	void Run();
	bool Configure(VDXHWND hwnd);
	void GetScriptString(char *buf, int maxlen);
	void ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc);

	VDXVF_DECLARE_SCRIPT_METHODS();
};

uint32 VDVFilterDrawText::GetParams() {
	using namespace vd2;
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	int base_format = ExtractBaseFormat(pxldst.format);

	switch(base_format) {
	case kPixFormat_XRGB8888:
	case kPixFormat_XRGB64:
	case kPixFormat_RGB_Planar:
	case kPixFormat_RGB_Planar16:
	case kPixFormat_RGBA_Planar:
	case kPixFormat_RGBA_Planar16:
	case kPixFormat_YUV444_Planar:
	case kPixFormat_YUV422_Planar:
	case kPixFormat_YUV420_Planar:
	case kPixFormat_YUV444_Planar16:
	case kPixFormat_YUV422_Planar16:
	case kPixFormat_YUV420_Planar16:
	case kPixFormat_YUV444_Alpha_Planar:
	case kPixFormat_YUV422_Alpha_Planar:
	case kPixFormat_YUV420_Alpha_Planar:
	case kPixFormat_YUV444_Alpha_Planar16:
	case kPixFormat_YUV422_Alpha_Planar16:
	case kPixFormat_YUV420_Alpha_Planar16:
	case kPixFormat_Y8:
	case kPixFormat_Y16:
		pxldst.data = pxlsrc.data;
		pxldst.pitch = pxlsrc.pitch;
		return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_NORMALIZE16;

	default:
		return FILTERPARAM_NOT_SUPPORTED;
	}
}

void VDDrawTextDialog::redo() {
	if (!fp2) return;
	fa->End();
	fa->Start();
	fp2->RedoFrame();
}

bool VDVFilterDrawText::Configure(VDXHWND hwnd) {
	VFDrawTextParam old_param = param;
	VDDrawTextDialog dlg(param, this);
	dlg.fp2 = fa->ifp2;
	if(fma) dlg.fmpreview = fma->fmpreview;

	if (!dlg.ShowDialog((VDGUIHandle)hwnd)) {
		param = old_param;
		return false;
	}

	return true;
}

void VDVFilterDrawText::Start() {
	const VDXPixmapLayout& pxsrc = *fa->src.mpPixmapLayout;

	HDC hdc = CreateDC("DISPLAY", 0, 0, 0);

	LOGFONTW f = {0,0, 0,0, param.weight, 0,0,0, ANSI_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH,};
	f.lfHeight = -LONG(param.size*GetDeviceCaps(hdc, LOGPIXELSY)/72*64);
	wcscpy(f.lfFaceName,param.face.c_str());
	f.lfItalic = param.italic;
	HFONT hfont = CreateFontIndirectW(&f);

	HGDIOBJ hfontOld = SelectObject(hdc, hfont);
	SetMapperFlags(hdc, 1);
	SetTextColor(hdc,0xFFFFFF);
	SetBkColor(hdc,0x000000);
	SetBkMode(hdc,TRANSPARENT);

	BeginPath(hdc);
	RECT rect;
	rect.left = long(param.x0*64);
	rect.top = long(param.y0*64);
	rect.right = long((pxsrc.w-param.x1)*64);
	rect.bottom = long((pxsrc.h-param.y1)*64);
	int dtflags = DT_NOCLIP|DT_WORDBREAK;
	if(param.align==0) dtflags |= DT_LEFT;
	if(param.align==1) dtflags |= DT_CENTER;
	if(param.align==2) dtflags |= DT_RIGHT;
	DrawTextW(hdc,param.text.c_str(),param.text.length(),&rect,dtflags);
	EndPath(hdc);

	int count = GetPath(hdc,0,0,0);
	vdfastvector<POINT> point;
	vdfastvector<unsigned char> flags;
	point.resize(count);
	flags.resize(count);
	GetPath(hdc,point.begin(),flags.begin(),count);

	VDPixmapPathRasterizer mTextRasterizer;
	VDPixmapConvertGDIPathToPath(mTextRasterizer, point, flags);
	SelectObject(hdc, hfontOld);
	DeleteDC(hdc);
	DeleteObject(hfont);

	mTextRasterizer.ScanConvert(mTextRegion);

	if (param.shadow_width>0) {
		VDPixmapRegion mTextOutlineBrush;
		VDPixmapCreateRoundRegion(mTextOutlineBrush, param.shadow_width*8);
		VDPixmapRegion mTempRegion;
		VDPixmapConvolveRegion(mTextBorderRegion, mTextRegion, mTextOutlineBrush, &mTempRegion);
	}
}

void VDVFilterDrawText::End() {
	mTextRegion.clear();
	mTextBorderRegion.clear();
}

void VDVFilterDrawText::Run() {
	using namespace vd2;
	const VDXPixmap& pxdst = *fa->dst.mpPixmap;

	int base_format = ExtractBaseFormat(pxdst.format);
	int colorSpace = ExtractColorSpace(&fa->dst);
	int colorRange = ExtractColorRange(&fa->dst);

	uint32 color1 = param.color;
	uint32 color0 = param.shadow_color;

	switch(base_format) {
	case kPixFormat_YUV444_Planar:
	case kPixFormat_YUV422_Planar:
	case kPixFormat_YUV420_Planar:
	case kPixFormat_YUV444_Planar16:
	case kPixFormat_YUV422_Planar16:
	case kPixFormat_YUV420_Planar16:
	case kPixFormat_YUV444_Alpha_Planar:
	case kPixFormat_YUV422_Alpha_Planar:
	case kPixFormat_YUV420_Alpha_Planar:
	case kPixFormat_YUV444_Alpha_Planar16:
	case kPixFormat_YUV422_Alpha_Planar16:
	case kPixFormat_YUV420_Alpha_Planar16:
	case kPixFormat_Y8:
	case kPixFormat_Y16:
		{
			color1 = VDConvertRGBToYCbCr(param.color,        colorSpace==kColorSpaceMode_709, colorRange==kColorRangeMode_Full);
			color0 = VDConvertRGBToYCbCr(param.shadow_color, colorSpace==kColorSpaceMode_709, colorRange==kColorRangeMode_Full);
			break;
		}
	}

	if (!mTextBorderRegion.mSpans.empty())
		VDPixmapFillPixmapAntialiased8x(VDPixmap::copy(pxdst), mTextBorderRegion, 0, 0, color0);

	VDPixmapFillPixmapAntialiased8x(VDPixmap::copy(pxdst), mTextRegion, 0, 0, color1);
}

void VDVFilterDrawText::ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc) {
	param.x0 = (float)argv[0].asDouble();
	param.y0 = (float)argv[1].asDouble();
	param.x1 = (float)argv[2].asDouble();
	param.y1 = (float)argv[3].asDouble();

	param.color = argv[4].asInt() | 0xFF000000;
	param.shadow_color = argv[5].asInt() | 0xFF000000;

	param.shadow_width = (float)argv[6].asDouble();
	param.size = (float)argv[7].asDouble();
	param.weight = argv[8].asInt();
	int flags = argv[9].asInt();
	param.align = flags & 3;
	param.italic = (flags & 4)!=0;

	VDString face_s(*argv[10].asString());
	param.face = VDTextU8ToW(face_s);

	if (argc==12) {
		VDString text_s(*argv[11].asString());
		param.text = VDTextU8ToW(text_s);
	} else if (fma && fma->fmproject) {
		size_t len;
		fma->fmproject->GetData(0,&len,L"text.txt");
		if (len) {
			VDString s;
			s.resize(len);
			fma->fmproject->GetData(&s[0],&len,L"text.txt");
			if (s.length()>3 && s.subspan(0,3)=="\xEF\xBB\xBF") {
				s.erase(0,3);
				param.text = VDTextU8ToW(s);
			} else if (s.length()>2 && s.subspan(0,2)=="\xFF\xFE") {
				s.erase(0,2);
				param.text.resize(s.length()/2);
				memcpy(&param.text[0],s.c_str(),s.length()/2*2);
			} else {
				param.text = VDTextAToW(s);
			}
		} else {
			param.text.clear();
		}
	}
}

void VDVFilterDrawText::GetScriptString(char *buf, int maxlen) {
	VDString face_s = VDEncodeScriptString(VDTextWToU8(param.face));
	VDString text_s = VDEncodeScriptString(VDTextWToU8(param.text));
	int flags = param.align;
	if(param.italic) flags |= 4;
	int color1 = param.color & 0xFFFFFF;
	int color0 = param.shadow_color & 0xFFFFFF;

	VDString s;
	s.sprintf("Config(%g,%g,%g,%g, 0x%06lx, 0x%06lx, %g,%g,%d,%d,\"%s\",\"%s\")", param.x0, param.y0, param.x1, param.y1, color1, color0, param.shadow_width,param.size,param.weight, flags, face_s.c_str(), text_s.c_str());

	if ((int)s.length()>=maxlen) {
		SafePrintf(buf, maxlen, "Config(%g,%g,%g,%g, 0x%06lx, 0x%06lx, %g,%g,%d,%d,\"%s\")", param.x0, param.y0, param.x1, param.y1, color1, color0, param.shadow_width,param.size,param.weight, flags, face_s.c_str());
		if (fma && fma->fmproject) {
			VDStringW text_s = VDStringW(L"\xFEFF") + param.text;
			fma->fmproject->SetData(text_s.c_str(),text_s.length()*2,L"text.txt");
		}
	} else {
		strcpy(buf,s.c_str());
		if (fma && fma->fmproject) {
			fma->fmproject->SetData(0,0,L"text.txt");
		}
	}
}

VDXVF_BEGIN_SCRIPT_METHODS(VDVFilterDrawText)
VDXVF_DEFINE_SCRIPT_METHOD(VDVFilterDrawText, ScriptConfig, "ddddiiddiiss")
VDXVF_DEFINE_SCRIPT_METHOD(VDVFilterDrawText, ScriptConfig, "ddddiiddiis")
VDXVF_END_SCRIPT_METHODS()

///////////////////////////////////////////////////////////////////////////////

extern const VDXFilterDefinition2 g_VDVFDrawText = VDXVideoFilterDefinition<VDVFilterDrawText>(
		NULL,
		"DrawText",
		"Draw basic text using vector fonts.");

