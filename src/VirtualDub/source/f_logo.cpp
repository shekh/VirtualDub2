//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2011 Avery Lee
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
#include <commdlg.h>
#include <new>

#include "misc.h"
#include <vd2/system/cpuaccel.h>
#include <vd2/system/filesys.h>
#include <vd2/system/strutil.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDLib/Dialog.h>
#include "resource.h"
#include "gui.h"
#include "filter.h"
#include "vbitmap.h"
#include "oshelper.h"
#include "image.h"
#include <vd2/system/error.h>

extern HINSTANCE g_hInst;

///////////////////////////////////////////////////////////////////////////

struct VDVFLogoConfig {
	char	szLogoPath[MAX_PATH];
	char	szAlphaPath[MAX_PATH];

	int		pos_x, pos_y;
	int		justify_x, justify_y;
	int		opacity;

	bool	bEnableAlphaBlending;
	bool	bNonPremultAlpha;
	bool	bEnableSecondaryAlpha;

	VDVFLogoConfig()
		: pos_x(0)
		, pos_y(0)
		, justify_x(0)
		, justify_y(0)
		, opacity(0x10000)
		, bEnableAlphaBlending(false)
		, bNonPremultAlpha(false)
		, bEnableSecondaryAlpha(false)
	{
		szLogoPath[0] = 0;
		szAlphaPath[0] = 0;
	}

	uint8 GetOpacity8() const {
		return (uint8)((opacity * 255 + 0x8000) >> 16);
	}
};

static const char *logoOpenImage(HWND hwnd, const char *oldfn) {
	OPENFILENAME ofn;
	static char szFile[MAX_PATH];
	char szFileTitle[MAX_PATH];

	///////////////

	if (oldfn)
		strcpy(szFile, oldfn);

	szFileTitle[0]=0;

	ofn.lStructSize			= OPENFILENAME_SIZE_VERSION_400;
	ofn.hwndOwner			= hwnd;
	ofn.lpstrFilter			= "Image file (*.bmp,*.tga,*.jpg,*.jpeg,*.png)\0*.bmp;*.tga;*.jpg;*.jpeg;*.png\0All files (*.*)\0*.*\0";
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= szFile;
	ofn.nMaxFile			= sizeof szFile;
	ofn.lpstrFileTitle		= szFileTitle;
	ofn.nMaxFileTitle		= sizeof szFileTitle;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= "Select image";
	ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING;
	ofn.lpstrDefExt			= NULL;

	if (GetOpenFileName(&ofn))
		return szFile;

	return NULL;
}

class VDVFLogoDialog : public VDDialogFrameW32 {
public:
	VDVFLogoDialog(VDVFLogoConfig& config, IVDXFilterPreview2 *mifp2);

protected:
	bool OnLoaded();
	void OnDestroy();
	bool OnCommand(uint32 id, uint32 extcode);
	void OnHScroll(uint32 id, int code);
	void OnVScroll(uint32 id, int code);
	void UpdateEnables();
	void UpdateOpacityText();
	void UpdateOffsets();
	void UpdateOpacity();

	VDVFLogoConfig& mConfig;
	IVDXFilterPreview2 *mifp2;
};

VDVFLogoDialog::VDVFLogoDialog(VDVFLogoConfig& config, IVDXFilterPreview2 *ifp2)
	: VDDialogFrameW32(IDD_FILTER_LOGO)
	, mConfig(config)
	, mifp2(ifp2)
{
}

bool VDVFLogoDialog::OnLoaded() {
	SetControlText(IDC_LOGOFILE, VDTextAToW(mConfig.szLogoPath).c_str());
	SetControlText(IDC_ALPHAFILE, VDTextAToW(mConfig.szAlphaPath).c_str());
	CheckButton(IDC_ALPHABLEND, mConfig.bEnableAlphaBlending);
	CheckButton(IDC_SECONDARYALPHA, mConfig.bEnableSecondaryAlpha);
	CheckButton(IDC_PREMULTALPHA, !mConfig.bNonPremultAlpha);
	SetControlTextF(IDC_XPOS, L"%d", mConfig.pos_x);
	SetControlTextF(IDC_YPOS, L"%d", mConfig.pos_y);

	SendDlgItemMessage(mhdlg, IDC_SPIN_XOFFSET, UDM_SETRANGE, 0, MAKELONG((short)-(UD_MINVAL-1/2), (short)+(UD_MINVAL-1/2)));
	SendDlgItemMessage(mhdlg, IDC_SPIN_YOFFSET, UDM_SETRANGE, 0, MAKELONG((short)+(UD_MINVAL-1/2), (short)-(UD_MINVAL-1/2)));

	TBSetRange(IDC_OPACITY, 0, 100);
	TBSetValue(IDC_OPACITY, VDRoundToInt(mConfig.opacity * (100 / 65536.0)));

	UpdateOpacityText();

	static const uint32 idbypos[3][3]={
		IDC_DIR_TOPLEFT,
		IDC_DIR_TOPCENTER,
		IDC_DIR_TOPRIGHT,
		IDC_DIR_MIDDLELEFT,
		IDC_DIR_MIDDLECENTER,
		IDC_DIR_MIDDLERIGHT,
		IDC_DIR_BOTTOMLEFT,
		IDC_DIR_BOTTOMCENTER,
		IDC_DIR_BOTTOMRIGHT
	};

	CheckButton(idbypos[mConfig.justify_y][mConfig.justify_x], true);

	if (mifp2)
		mifp2->InitButton((VDXHWND)GetControl(IDC_PREVIEW));

	UpdateEnables();

	return false;
}

void VDVFLogoDialog::OnDestroy() {
	if (mifp2)
		mifp2->Close();
}

bool VDVFLogoDialog::OnCommand(uint32 id, uint32 extcode) {
	switch(id) {
		case IDC_ALPHABLEND:
			mConfig.bEnableAlphaBlending = IsButtonChecked(IDC_ALPHABLEND);
			mifp2->UndoSystem();
			mifp2->RedoSystem();
			UpdateEnables();
			return true;

		case IDC_SECONDARYALPHA:
			mConfig.bEnableSecondaryAlpha = IsButtonChecked(IDC_SECONDARYALPHA);
			mifp2->UndoSystem();
			mifp2->RedoSystem();
			UpdateEnables();
			return true;

		case IDC_PREMULTALPHA:
			mConfig.bNonPremultAlpha = !IsButtonChecked(IDC_PREMULTALPHA);
			mifp2->UndoSystem();
			mifp2->RedoSystem();
			return true;

		case IDC_LOGOFILE:
			if (extcode == EN_KILLFOCUS) {
				VDStringW s;

				if (GetControlText(IDC_LOGOFILE, s))
					vdstrlcpy(mConfig.szLogoPath, VDTextWToA(s).c_str(), sizeof mConfig.szLogoPath);

				mifp2->UndoSystem();
				mifp2->RedoSystem();
			}
			return true;

		case IDC_ALPHAFILE:
			if (extcode == EN_KILLFOCUS) {
				VDStringW s;

				if (GetControlText(IDC_ALPHAFILE, s))
					vdstrlcpy(mConfig.szAlphaPath, VDTextWToA(s).c_str(), sizeof mConfig.szAlphaPath);

				mifp2->UndoSystem();
				mifp2->RedoSystem();
			}
			return true;

		case IDC_LOGOFILE_BROWSE:
			if (const char *fn = logoOpenImage(mhdlg, mConfig.szLogoPath)) {
				SetControlText(IDC_LOGOFILE, VDTextAToW(fn).c_str());
				strcpy(mConfig.szLogoPath, fn);
				mifp2->UndoSystem();
				mifp2->RedoSystem();
			}
			return true;

		case IDC_ALPHAFILE_BROWSE:
			if (const char *fn = logoOpenImage(mhdlg, mConfig.szAlphaPath)) {
				SetControlText(IDC_ALPHAFILE, VDTextAToW(fn).c_str());
				strcpy(mConfig.szAlphaPath, fn);
				mifp2->UndoSystem();
				mifp2->RedoSystem();
			}
			return true;

		case IDC_XPOS:
			if (extcode == EN_KILLFOCUS)
				UpdateOffsets();
			return true;

		case IDC_YPOS:
			if (extcode == EN_KILLFOCUS)
				UpdateOffsets();

			return true;

		case IDC_DIR_TOPLEFT:			mConfig.justify_x=0; mConfig.justify_y=0; mifp2->RedoFrame(); return true;
		case IDC_DIR_TOPCENTER:			mConfig.justify_x=1; mConfig.justify_y=0; mifp2->RedoFrame(); return true;
		case IDC_DIR_TOPRIGHT:			mConfig.justify_x=2; mConfig.justify_y=0; mifp2->RedoFrame(); return true;
		case IDC_DIR_MIDDLELEFT:		mConfig.justify_x=0; mConfig.justify_y=1; mifp2->RedoFrame(); return true;
		case IDC_DIR_MIDDLECENTER:		mConfig.justify_x=1; mConfig.justify_y=1; mifp2->RedoFrame(); return true;
		case IDC_DIR_MIDDLERIGHT:		mConfig.justify_x=2; mConfig.justify_y=1; mifp2->RedoFrame(); return true;
		case IDC_DIR_BOTTOMLEFT:		mConfig.justify_x=0; mConfig.justify_y=2; mifp2->RedoFrame(); return true;
		case IDC_DIR_BOTTOMCENTER:		mConfig.justify_x=1; mConfig.justify_y=2; mifp2->RedoFrame(); return true;
		case IDC_DIR_BOTTOMRIGHT:		mConfig.justify_x=2; mConfig.justify_y=2; mifp2->RedoFrame(); return true;

		case IDC_PREVIEW:
			if (mifp2)
				mifp2->Toggle((VDXHWND)mhdlg);
			return true;
	}

	return false;
}

void VDVFLogoDialog::OnHScroll(uint32 id, int code) {
	switch(id) {
		case IDC_XPOS:
		case IDC_SPIN_XOFFSET:
			UpdateOffsets();
			break;
		case IDC_OPACITY:
			UpdateOpacity();
			break;
	}
}

void VDVFLogoDialog::OnVScroll(uint32 id, int code) {
	switch(id) {
		case IDC_YPOS:
		case IDC_SPIN_YOFFSET:
			UpdateOffsets();
			break;
	}
}

void VDVFLogoDialog::UpdateEnables() {
	bool bAlphaBlendingEnabled = IsButtonChecked(IDC_ALPHABLEND);
	bool bUsingSecondaryAlpha = bAlphaBlendingEnabled && IsButtonChecked(IDC_SECONDARYALPHA);

	EnableControl(IDC_ALPHAFILE, bAlphaBlendingEnabled);
	EnableControl(IDC_SECONDARYALPHA, bAlphaBlendingEnabled);
	EnableControl(IDC_PREMULTALPHA, bAlphaBlendingEnabled);
	EnableControl(IDC_ALPHAFILE, bUsingSecondaryAlpha);
	EnableControl(IDC_ALPHAFILE_BROWSE, bUsingSecondaryAlpha);
}

void VDVFLogoDialog::UpdateOffsets() {
	long pos_x;
	long pos_y;

	mbValidationFailed = false;

	pos_x = GetControlValueSint32(IDC_XPOS);
	if (mbValidationFailed) {
		SetFocus(GetDlgItem(mhdlg, IDC_XPOS));
		MessageBeep(MB_ICONEXCLAMATION);
		return;
	}

	pos_y = GetControlValueSint32(IDC_YPOS);
	if (mbValidationFailed) {
		SetFocus(GetDlgItem(mhdlg, IDC_YPOS));
		MessageBeep(MB_ICONEXCLAMATION);
		return;
	}

	if (pos_x != mConfig.pos_x || pos_y != mConfig.pos_y) {
		mConfig.pos_x = pos_x;
		mConfig.pos_y = pos_y;

		if (mifp2)
			mifp2->RedoFrame();
	}
}

void VDVFLogoDialog::UpdateOpacityText() {
	SetControlTextF(IDC_STATIC_OPACITY, L"%d%%", VDRoundToInt(mConfig.opacity * (100 / 65536.0)));
}

void VDVFLogoDialog::UpdateOpacity() {
	int percent = TBGetValue(IDC_OPACITY);
	long opacity = (percent * 65536 + 50) / 100;

	if (opacity != mConfig.opacity) {
		uint8 prevOpaque = mConfig.GetOpacity8() == 255;
		mConfig.opacity = opacity;
		uint8 nextOpaque = mConfig.GetOpacity8() == 255;

		if (prevOpaque != nextOpaque) {
			mifp2->UndoSystem();
			mifp2->RedoSystem();
		} else {
			mifp2->RedoFrame();
		}

		UpdateOpacityText();
	}
}

///////////////////////////////////////////////////////////////////////////

// The Blinn /255 rounded algorithm:
//
// i = a*b + 128;
// y = (i + (i>>8)) >> 8;

static void AlphaBltSCALAR(Pixel32 *dst, PixOffset dstoff, const Pixel32 *src, PixOffset srcoff, PixDim w, PixDim h) {
	if (w<=0 || h<=0)
		return;

	srcoff -= 4*w;
	dstoff -= 4*w;

	do {
		int w2 = w;

		do {
			const Pixel32 x = *dst++;
			const Pixel32 y = *src++;
			const Pixel32 a = y >> 24;
			Pixel32 r = ((x>>16)&0xff)*a + 128;
			Pixel32 g = ((x>> 8)&0xff)*a + 128;
			Pixel32 b = ((x    )&0xff)*a + 128;

			r = (((r + (r>>8)) << 8)&0xff0000) + (y & 0xff0000);
			g = (((g + (g>>8))     )&0x00ff00) + (y & 0x00ff00);
			b = (((b + (b>>8)) >> 8)&0x0000ff) + (y & 0x0000ff);

			if (r >= 0x01000000) r = 0x00ff0000;
			if (g >= 0x00010000) g = 0x0000ff00;
			if (b >= 0x00000100) b = 0x000000ff;

			dst[-1] = r + g + b;
		} while(--w2);
		src = (Pixel32 *)((char *)src + srcoff);
		dst = (Pixel32 *)((char *)dst + dstoff);
	} while(--h);
}

static void CombineAlphaBltSCALAR(Pixel32 *dst, PixOffset dstoff, const Pixel32 *src, PixOffset srcoff, PixDim w, PixDim h) {
	if (w<=0 || h<=0)
		return;

	srcoff -= 4*w;
	dstoff -= 4*w;

	do {
		int w2 = w;
		do {
			const Pixel32 x = *dst++;
			const Pixel32 y = *src++;
			const Pixel32 a = (((y>>16)&0xff)*0x4CCCCC + ((y>>8)&0xff)*0x970A3E + (y&0xff)*0x1C28F6 + 0x800000) & 0xff000000;

			dst[-1] = (x & 0x00ffffff) + a;
		} while(--w2);
		src = (Pixel32 *)((char *)src + srcoff);
		dst = (Pixel32 *)((char *)dst + dstoff);
	} while(--h);
}

static void PremultiplyAlphaSCALAR(Pixel32 *dst, PixOffset dstoff, PixDim w, PixDim h) {
	if (w<=0 || h<=0)
		return;

	dstoff -= 4*w;

	do {
		int w2 = w;
		do {
			const Pixel32 x = *dst++;
			const Pixel32 a = x >> 24;
			Pixel32 r = ((x>>16)&0xff) * a;
			Pixel32 g = ((x>> 8)&0xff) * a;
			Pixel32 b = ((x    )&0xff) * a;

			r = (r + (r>>8))>>8;
			g = (g + (g>>8))>>8;
			b = (b + (b>>8))>>8;

			dst[-1] = (a<<24) + (r<<16) + (g<<8) + b;
		} while(--w2);
		dst = (Pixel32 *)((char *)dst + dstoff);
	} while(--h);
}

static void ScalePremultipliedAlphaSCALAR(Pixel32 *dst, PixOffset dstoff, PixDim w, PixDim h, unsigned scale8) {
	if (w<=0 || h<=0)
		return;

	dstoff -= 4*w;

	do {
		int w2 = w;
		do {
			const Pixel32 x = *dst;
			Pixel32 ag = ((x>>8)&0xff00ff) * scale8 + 0x800080;
			Pixel32 rb = ((x   )&0xff00ff) * scale8 + 0x800080;

			*dst = (ag & 0xff00ff00) + ((rb & 0xff00ff00)>>8);
			++dst;
		} while(--w2);
		dst = (Pixel32 *)((char *)dst + dstoff);
	} while(--h);
}

#ifdef VD_CPU_X86
void __cdecl VDVFLogoAlphaBltMMX(Pixel32 *dst, PixOffset dstoff, const Pixel32 *src, PixOffset srcoff, PixDim w, PixDim h);
void __cdecl VDVFLogoCombineAlphaBltMMX(Pixel32 *dst, PixOffset dstoff, const Pixel32 *src, PixOffset srcoff, PixDim w, PixDim h);
void __cdecl VDVFLogoPremultiplyAlphaMMX(Pixel32 *dst, PixOffset dstoff, PixDim w, PixDim h);
void __cdecl VDVFLogoScalePremultipliedAlphaMMX(Pixel32 *dst, PixOffset dstoff, PixDim w, PixDim h, unsigned alpha);
#endif

///////////////////////////////////////////////////////////////////////////

static bool dualclip(const VDPixmap& v1, int& x1, int& y1, const VDPixmap& v2, int& x2, int& y2, int& w, int& h) {
	if (x1 < 0) { x2 -= x1; w += x1; x1 = 0; }
	if (y1 < 0) { y2 -= y1; h += y1; y1 = 0; }
	if (x2 < 0) { x1 -= x2; w += x2; x2 = 0; }
	if (y2 < 0) { y1 -= y2; h += y2; y2 = 0; }
	if (x1+w > v1.w) { w = v1.w - x1; }
	if (y1+h > v1.h) { h = v1.h - y1; }
	if (x2+w > v2.w) { w = v2.w - x2; }
	if (y2+h > v2.h) { h = v2.h - y2; }

	return w>0 && h>0;
}

static void AlphaBlt(const VDPixmap& dst, int x1, int y1, const VDPixmap& src, int x2, int y2, int w, int h) {
	if (!dualclip(dst, x1, y1, src, x2, y2, w, h))
		return;

	uint32 *dstp = (uint32 *)vdptroffset(dst.data, dst.pitch * y1 + 4 * x1);
	const uint32 *srcp = (const uint32 *)vdptroffset(src.data, src.pitch * y2 + 4 * x2);

#ifdef _M_IX86
	(MMX_enabled ? VDVFLogoAlphaBltMMX : AlphaBltSCALAR)(dstp, dst.pitch, srcp, src.pitch, w, h);
#else
	AlphaBltSCALAR(dstp, dst.pitch, srcp, src.pitch, w, h);
#endif
}

static void CombineAlphaBlt(const VDPixmap& dst, int x1, int y1, const VDPixmap& src, int x2, int y2, int w, int h) {
	if (!dualclip(dst, x1, y1, src, x2, y2, w, h))
		return;

	uint32 *dstp = (uint32 *)vdptroffset(dst.data, dst.pitch * y1 + 4 * x1);
	const uint32 *srcp = (const uint32 *)vdptroffset(src.data, src.pitch * y2 + 4 * x2);

#ifdef _M_IX86
	(MMX_enabled ? VDVFLogoCombineAlphaBltMMX : CombineAlphaBltSCALAR)(dstp, dst.pitch, srcp, src.pitch, w, h);
#else
	CombineAlphaBltSCALAR(dstp, dst.pitch, srcp, src.pitch, w, h);
#endif
}

static void PremultiplyAlpha(const VDPixmap& dst, int x1, int y1, int w, int h) {
	if (x1   < 0   ) { w += x1; x1 = 0; }
	if (y1   < 0   ) { h += y1; y1 = 0; }
	if (x1+w > dst.w) { w = dst.w - x1; }
	if (y1+h > dst.h) { h = dst.h - y1; }

	if (w<=0 || h<=0)
		return;

	uint32 *dstp = (uint32 *)vdptroffset(dst.data, dst.pitch * y1 + 4 * x1);

#ifdef _M_IX86
	(MMX_enabled ? VDVFLogoPremultiplyAlphaMMX : PremultiplyAlphaSCALAR)(dstp, dst.pitch, w, h);
#else
	PremultiplyAlphaSCALAR(dstp, dst.pitch, w, h);
#endif
}

static void ScalePremultipliedAlpha(const VDPixmap& dst, int x1, int y1, int w, int h, unsigned scale8) {
	if (x1   < 0   ) { w += x1; x1 = 0; }
	if (y1   < 0   ) { h += y1; y1 = 0; }
	if (x1+w > dst.w) { w = dst.w - x1; }
	if (y1+h > dst.h) { h = dst.h - y1; }

	if (w<=0 || h<=0)
		return;

	uint32 *dstp = (uint32 *)vdptroffset(dst.data, dst.pitch * y1 + 4 * x1);

#ifdef _M_IX86
	(MMX_enabled ? VDVFLogoScalePremultipliedAlphaMMX : ScalePremultipliedAlphaSCALAR)(dstp, dst.pitch, w, h, scale8);
#else
	ScalePremultipliedAlphaSCALAR(dstp, dst.pitch, w, h, scale8);
#endif
}

static void SetAlpha(const VDPixmap& vbdst, int x1, int y1, int w, int h) {
	if (x1   < 0   ) { w += x1; x1 = 0; }
	if (y1   < 0   ) { h += y1; y1 = 0; }
	if (x1+w > vbdst.w) { w = vbdst.w - x1; }
	if (y1+h > vbdst.h) { h = vbdst.h - y1; }

	if (w<=0 || h<=0)
		return;

	uint32 *dst = (uint32 *)vdptroffset(vbdst.data, vbdst.pitch * y1) + x1;
	ptrdiff_t dstoff = vbdst.pitch - 4*w;

	do {
		int w2 = w;
		do {
			*dst++ |= 0xff000000;
		} while(--w2);

		vdptrstep(dst, dstoff);
	} while(--h);
}

static void InvertAlpha(const VDPixmap& vbdst, int x1, int y1, int w, int h) {
	if (x1   < 0   ) { w += x1; x1 = 0; }
	if (y1   < 0   ) { h += y1; y1 = 0; }
	if (x1+w > vbdst.w) { w = vbdst.w - x1; }
	if (y1+h > vbdst.h) { h = vbdst.h - y1; }

	if (w<=0 || h<=0)
		return;

	uint32 *dst = (uint32 *)vdptroffset(vbdst.data, vbdst.pitch * y1) + x1;
	ptrdiff_t dstoff = vbdst.pitch - 4*w;

	do {
		int w2 = w;
		do {
			*dst++ ^= 0xff000000;
		} while(--w2);

		vdptrstep(dst, dstoff);
	} while(--h);
}

///////////////////////////////////////////////////////////////////////////

class VDVFLogo : public VDXVideoFilter {
public:
	VDVFLogo();

	uint32 GetParams();
	void Start();
	void End();
	void Run();
	bool Configure(VDXHWND hwnd);
	void GetSettingString(char *buf, int maxlen);

	bool Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher);

	void GetScriptString(char *buf, int buflen);

	VDXVF_DECLARE_SCRIPT_METHODS();

	void ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc);

	bool OnInvalidateCaches();

protected:
	void UpdateParameters();

	VDPixmapBuffer	mLogoBuffer;
	IFilterPreview *ifp;
	bool	bAlphaBlendingRequired;
	bool	mbNoPrefetch;
	int		mBltDestX;
	int		mBltDestY;

	VDVFLogoConfig mConfig;
};

VDVFLogo::VDVFLogo() {
}

uint32 VDVFLogo::GetParams() {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	pxldst.pitch = pxlsrc.pitch;
	return FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

void VDVFLogo::Run() {
	int x = mBltDestX;
	int y = mBltDestY;
	const VDXPixmap& dst = *fa->dst.mpPixmap;

	if (bAlphaBlendingRequired) {
		AlphaBlt((VDPixmap&)dst, x, y, mLogoBuffer, 0, 0, fa->dst.w, fa->dst.h);
	} else
		VDPixmapBlt((VDPixmap&)dst, x, y, mLogoBuffer, 0, 0, mLogoBuffer.w, mLogoBuffer.h);
}

void VDVFLogo::Start() {
	bool bHasAlpha;

	DecodeImage(mConfig.szLogoPath, mLogoBuffer, nsVDPixmap::kPixFormat_XRGB8888, bHasAlpha);

	if (mConfig.bEnableAlphaBlending) {
		if (mConfig.bEnableSecondaryAlpha) {
			VDPixmapBuffer vbAlphaLogo;
			bool bSecondHasAlpha;

			DecodeImage(mConfig.szAlphaPath, vbAlphaLogo, nsVDPixmap::kPixFormat_XRGB8888, bSecondHasAlpha);

			if (vbAlphaLogo.w != mLogoBuffer.w || vbAlphaLogo.h != mLogoBuffer.h)
				throw MyError("Alpha image has different size than logo image (%dx%d vs. %dx%d)", vbAlphaLogo.w, vbAlphaLogo.h, mLogoBuffer.w, mLogoBuffer.h);

			CombineAlphaBlt(mLogoBuffer, 0, 0, vbAlphaLogo, 0, 0, vbAlphaLogo.w, vbAlphaLogo.h);
		} else if (!bHasAlpha)
			throw MyError("cannot alpha blend logo: image does not have an alpha channel.");

		if (mConfig.bNonPremultAlpha)
			PremultiplyAlpha(mLogoBuffer, 0, 0, mLogoBuffer.w, mLogoBuffer.h);
	} else {
		SetAlpha(mLogoBuffer, 0, 0, mLogoBuffer.w, mLogoBuffer.h);
	}

	int opacity8 = mConfig.GetOpacity8();
	bAlphaBlendingRequired = mConfig.bEnableAlphaBlending;
	if (opacity8 < 255) {
		bAlphaBlendingRequired = true;
		ScalePremultipliedAlpha(mLogoBuffer, 0, 0, mLogoBuffer.w, mLogoBuffer.h, opacity8);
	}

	InvertAlpha(mLogoBuffer, 0, 0, mLogoBuffer.w, mLogoBuffer.h);

	UpdateParameters();
}

void VDVFLogo::End() {
	mLogoBuffer.clear();
}

bool VDVFLogo::Configure(VDXHWND hwnd) {
	VDVFLogoConfig prev(mConfig);
	VDVFLogoDialog dlg(mConfig, fa->ifp2);

	if (!dlg.ShowDialog((VDGUIHandle)hwnd)) {
		mConfig = prev;
		return false;
	}

	return true;
}

void VDVFLogo::GetSettingString(char *buf, int maxlen) {
	if (mConfig.bEnableAlphaBlending && mConfig.bEnableSecondaryAlpha)
		SafePrintf(buf, maxlen, " (logo:\"%s\", alpha:\"%s\")", VDFileSplitPath(mConfig.szLogoPath), VDFileSplitPath(mConfig.szAlphaPath));
	else
		SafePrintf(buf, maxlen, " (logo:\"%s\", alpha:%s)", VDFileSplitPath(mConfig.szLogoPath), mConfig.bEnableAlphaBlending ? "on" : "off");
}

void VDVFLogo::ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc) {
	mConfig.szLogoPath[0] = 0;
	mConfig.szAlphaPath[0] = 0;
	mConfig.pos_x = argv[1].asInt();
	mConfig.pos_y = argv[2].asInt();
	mConfig.justify_x = 0;
	mConfig.justify_y = 0;

	mConfig.bEnableSecondaryAlpha = false;

	strncpy(mConfig.szLogoPath, *argv[0].asString(), sizeof mConfig.szLogoPath);
	mConfig.szLogoPath[sizeof mConfig.szLogoPath - 1] = 0;

	if (argv[3].isString()) {
		strncpy(mConfig.szAlphaPath, *argv[3].asString(), sizeof mConfig.szAlphaPath);
		mConfig.szAlphaPath[sizeof mConfig.szAlphaPath - 1] = 0;
		mConfig.bEnableAlphaBlending = true;
		mConfig.bEnableSecondaryAlpha = true;
	} else {
		mConfig.bEnableAlphaBlending = !!argv[3].asInt();
	}

	mConfig.bNonPremultAlpha = !!argv[4].asInt();

	int xj = argv[5].asInt();
	int yj = argv[6].asInt();

	if (xj>=0 && xj<3 && yj>=0 && yj<3) {
		mConfig.justify_x = xj;
		mConfig.justify_y = yj;
	}

	mConfig.opacity = argv[7].asInt();
}

bool VDVFLogo::Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher) {
	return mbNoPrefetch;
}

VDXVF_BEGIN_SCRIPT_METHODS(VDVFLogo)
	VDXVF_DEFINE_SCRIPT_METHOD(VDVFLogo, ScriptConfig, "siiiiiii")
	VDXVF_DEFINE_SCRIPT_METHOD2(VDVFLogo, ScriptConfig, "siisiiii")
VDXVF_END_SCRIPT_METHODS()

void VDVFLogo::GetScriptString(char *buf, int buflen) {
	char tmp[1024];

	strncpy(tmp, strCify(mConfig.szLogoPath), sizeof tmp);
	tmp[1023] = 0;

	if (mConfig.bEnableAlphaBlending && mConfig.bEnableSecondaryAlpha)
		SafePrintf(buf, buflen, "Config(\"%s\", %d, %d, \"%s\", %d, %d, %d, %d)", tmp, mConfig.pos_x, mConfig.pos_y, strCify(mConfig.szAlphaPath), mConfig.bNonPremultAlpha, mConfig.justify_x, mConfig.justify_y, mConfig.opacity);
	else
		SafePrintf(buf, buflen, "Config(\"%s\", %d, %d, %d, %d, %d, %d, %d)", tmp, mConfig.pos_x, mConfig.pos_y, mConfig.bEnableAlphaBlending, mConfig.bNonPremultAlpha, mConfig.justify_x, mConfig.justify_y, mConfig.opacity);
}

bool VDVFLogo::OnInvalidateCaches() {
	UpdateParameters();
	return true;
}

void VDVFLogo::UpdateParameters() {
	mBltDestX = mConfig.pos_x + (((fa->dst.w - mLogoBuffer.w) * mConfig.justify_x + 1)>>1);
	mBltDestY = mConfig.pos_y + (((fa->dst.h - mLogoBuffer.h) * mConfig.justify_y + 1)>>1);

	mbNoPrefetch = false;

	if (!mConfig.bEnableAlphaBlending && mConfig.GetOpacity8() == 255 && mBltDestX <= 0 && mBltDestY <= 0 && mBltDestX + mLogoBuffer.w >= fa->dst.w && mBltDestY + mLogoBuffer.h >= fa->dst.h)
		mbNoPrefetch = true;
}


extern FilterDefinition filterDef_logo = VDXVideoFilterDefinition<VDVFLogo>(
	NULL,
	"logo",
	"Overlays an image over video."
#ifdef USE_ASM
			"\n\n[Assembly optimized] [MMX optimized]"
#endif
);

// 1>d:\p4root\dev19\src\h\vd2/VDXFrame/VideoFilter.h(60) : warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{48,{flat}}' }'' : unreferenced local function has been removed
#pragma warning(disable: 4505)
