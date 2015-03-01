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

#include <vd2/system/math.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/plugin/vdvideoaccel.h>
#include <vd2/plugin/vdvideofilt.h>
#include <windows.h>
#include <commctrl.h>

#include "resource.h"

#include "VFBrightCont.inl"

extern HINSTANCE g_hInst;

extern "C" void asm_brightcont1_run(
		void *dst,
		unsigned long width,
		unsigned long height,
		unsigned long stride,
		unsigned long multiplier,
		unsigned long adder1,
		unsigned long adder2
		);

extern "C" void asm_brightcont2_run(
		void *dst,
		unsigned long width,
		unsigned long height,
		unsigned long stride,
		unsigned long multiplier,
		unsigned long adder1,
		unsigned long adder2
		);

#ifndef _M_IX86
static void brightcont_run_trans(
		void *dst,
		uint32 width,
		uint32 height,
		ptrdiff_t pitch,
		const uint8 tab[256])
{
	do {
		uint8 *p = (uint8 *)dst;

		for(uint32 x=0; x<width; ++x) {
			p[0] = tab[p[0]];
			p[1] = tab[p[1]];
			p[2] = tab[p[2]];
			p += 4;
		}

		dst = (char *)dst + pitch;
	} while(--height);
}
#endif

static void brightcont_run_trans8(
		void *dst,
		uint32 width,
		uint32 height,
		ptrdiff_t pitch,
		const uint8 tab[256])
{
	do {
		uint8 *p = (uint8 *)dst;

		for(uint32 x=0; x<width; ++x) {
			*p = tab[*p];
			++p;
		}

		dst = (char *)dst + pitch;
	} while(--height);
}

///////////////////////////////////

struct VDVFilterBrightContConfig {
	sint32 bright;
	sint32 cont;

	uint8	mLookup[256];
	uint8	mYLookup[256];
	uint8	mCLookup[256];

	VDVFilterBrightContConfig() : bright(0), cont(16) {}

	void RedoTables();

protected:
	static void GenTable(uint8 table[256], float u0, float dudx);
};

void VDVFilterBrightContConfig::RedoTables() {
	float bias = bright - 0.5f;
	float scale = (float)cont / 16.0f;
	GenTable(mLookup, bias, (float)scale);
	GenTable(mYLookup, 16.0f*(1.0f - scale) + bias*(219.0f / 255.0f), scale);
	GenTable(mCLookup, 128.0f*(1.0f - scale), scale);
}

void VDVFilterBrightContConfig::GenTable(uint8 table[256], float fy0, float fdydx) {
	sint32 y0 = VDRoundToInt32(fy0 * 65536.0f);
	sint32 dydx = VDRoundToInt32(fdydx * 65536.0f);

	y0 += 0x8000;
	for(int i=0; i<256; ++i) {
		int y = y0 >> 16;
		y0 += dydx;

		if (y < 0)
			y = 0;
		else if (y > 255)
			y = 255;

		table[i] = (uint8)y;
	}
}

class VDVFilterBrightCont : public VDXVideoFilter {
public:
	VDVFilterBrightCont();

	uint32 GetParams();
	void Start();
	void Run();

	void StartAccel(IVDXAContext *vdxa);
	void RunAccel(IVDXAContext *vdxa);
	void StopAccel(IVDXAContext *vdxa);

	bool Configure(VDXHWND hwnd);
	void GetSettingString(char *buf, int maxlen);
	void GetScriptString(char *buf, int maxlen);
	void ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc);

	VDXVF_DECLARE_SCRIPT_METHODS();

protected:
	VDVFilterBrightContConfig mConfig;

	uint32 mVDXAFP;
};

VDVFilterBrightCont::VDVFilterBrightCont()
	: mVDXAFP(0)
{
}

void VDVFilterBrightCont::Run() {	
	const VDXPixmap& pxdst = *fa->dst.mpPixmap;

	switch(pxdst.format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
#if _M_IX86
			if (mConfig.bright>=0)
				asm_brightcont2_run(
						pxdst.data,
						pxdst.w,
						pxdst.h,
						pxdst.pitch,
						mConfig.cont,
						mConfig.bright*0x00100010L,
						mConfig.bright*0x00001000L
						);
			else
				asm_brightcont1_run(
						pxdst.data,
						pxdst.w,
						pxdst.h,
						pxdst.pitch,
						mConfig.cont,
						(-mConfig.bright)*0x00100010L,
						(-mConfig.bright)*0x00001000L
						);
#else
			brightcont_run_trans(
					pxdst.data,
					pxdst.w,
					pxdst.h,
					pxdst.pitch,
					mConfig.mLookup);
#endif
			break;

		case nsVDXPixmap::kPixFormat_RGB888:
			brightcont_run_trans8(pxdst.data, pxdst.w*3, pxdst.h, pxdst.pitch, mConfig.mLookup);
			break;

		case nsVDXPixmap::kPixFormat_Y8:
			brightcont_run_trans8(pxdst.data, pxdst.w, pxdst.h, pxdst.pitch, mConfig.mYLookup);
			break;

		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709:
			brightcont_run_trans8(pxdst.data, pxdst.w, pxdst.h, pxdst.pitch, mConfig.mYLookup);
			brightcont_run_trans8(pxdst.data2, pxdst.w, pxdst.h, pxdst.pitch2, mConfig.mCLookup);
			brightcont_run_trans8(pxdst.data3, pxdst.w, pxdst.h, pxdst.pitch3, mConfig.mCLookup);
			break;

		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709:
			brightcont_run_trans8(pxdst.data, pxdst.w, pxdst.h, pxdst.pitch, mConfig.mYLookup);
			brightcont_run_trans8(pxdst.data2, pxdst.w >> 1, pxdst.h, pxdst.pitch2, mConfig.mCLookup);
			brightcont_run_trans8(pxdst.data3, pxdst.w >> 1, pxdst.h, pxdst.pitch3, mConfig.mCLookup);
			break;

		case nsVDXPixmap::kPixFormat_YUV411_Planar:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709:
			brightcont_run_trans8(pxdst.data, pxdst.w, pxdst.h, pxdst.pitch, mConfig.mYLookup);
			brightcont_run_trans8(pxdst.data2, pxdst.w >> 2, pxdst.h, pxdst.pitch2, mConfig.mCLookup);
			brightcont_run_trans8(pxdst.data3, pxdst.w >> 2, pxdst.h, pxdst.pitch3, mConfig.mCLookup);
			break;

		case nsVDXPixmap::kPixFormat_YUV420_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV420i_Planar:
		case nsVDXPixmap::kPixFormat_YUV420i_Planar_709:
			brightcont_run_trans8(pxdst.data, pxdst.w, pxdst.h, pxdst.pitch, mConfig.mYLookup);
			brightcont_run_trans8(pxdst.data2, pxdst.w >> 1, pxdst.h >> 1, pxdst.pitch2, mConfig.mCLookup);
			brightcont_run_trans8(pxdst.data3, pxdst.w >> 1, pxdst.h >> 1, pxdst.pitch3, mConfig.mCLookup);
			break;

		case nsVDXPixmap::kPixFormat_YUV410_Planar:
		case nsVDXPixmap::kPixFormat_YUV410_Planar_709:
			brightcont_run_trans8(pxdst.data, pxdst.w, pxdst.h, pxdst.pitch, mConfig.mLookup);
			brightcont_run_trans8(pxdst.data2, pxdst.w >> 2, pxdst.h >> 2, pxdst.pitch2, mConfig.mCLookup);
			brightcont_run_trans8(pxdst.data3, pxdst.w >> 2, pxdst.h >> 2, pxdst.pitch3, mConfig.mCLookup);
			break;

		case nsVDXPixmap::kPixFormat_YUV444_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709_FR:
			brightcont_run_trans8(pxdst.data, pxdst.w, pxdst.h, pxdst.pitch, mConfig.mLookup);
			brightcont_run_trans8(pxdst.data2, pxdst.w, pxdst.h, pxdst.pitch2, mConfig.mCLookup);
			brightcont_run_trans8(pxdst.data3, pxdst.w, pxdst.h, pxdst.pitch3, mConfig.mCLookup);
			break;

		case nsVDXPixmap::kPixFormat_YUV422_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709_FR:
			brightcont_run_trans8(pxdst.data, pxdst.w, pxdst.h, pxdst.pitch, mConfig.mLookup);
			brightcont_run_trans8(pxdst.data2, pxdst.w >> 1, pxdst.h, pxdst.pitch2, mConfig.mCLookup);
			brightcont_run_trans8(pxdst.data3, pxdst.w >> 1, pxdst.h, pxdst.pitch3, mConfig.mCLookup);
			break;

		case nsVDXPixmap::kPixFormat_YUV411_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709_FR:
			brightcont_run_trans8(pxdst.data, pxdst.w, pxdst.h, pxdst.pitch, mConfig.mLookup);
			brightcont_run_trans8(pxdst.data2, pxdst.w >> 2, pxdst.h, pxdst.pitch2, mConfig.mCLookup);
			brightcont_run_trans8(pxdst.data3, pxdst.w >> 2, pxdst.h, pxdst.pitch3, mConfig.mCLookup);
			break;

		case nsVDXPixmap::kPixFormat_YUV420_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV420_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_YUV420i_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV420i_Planar_709_FR:
			brightcont_run_trans8(pxdst.data, pxdst.w, pxdst.h, pxdst.pitch, mConfig.mLookup);
			brightcont_run_trans8(pxdst.data2, pxdst.w >> 1, pxdst.h >> 1, pxdst.pitch2, mConfig.mCLookup);
			brightcont_run_trans8(pxdst.data3, pxdst.w >> 1, pxdst.h >> 1, pxdst.pitch3, mConfig.mCLookup);
			break;

		case nsVDXPixmap::kPixFormat_YUV410_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV410_Planar_709_FR:
			brightcont_run_trans8(pxdst.data, pxdst.w, pxdst.h, pxdst.pitch, mConfig.mLookup);
			brightcont_run_trans8(pxdst.data2, pxdst.w >> 2, pxdst.h >> 2, pxdst.pitch2, mConfig.mCLookup);
			brightcont_run_trans8(pxdst.data3, pxdst.w >> 2, pxdst.h >> 2, pxdst.pitch3, mConfig.mCLookup);
			break;
	}
}

void VDVFilterBrightCont::Start() {
	mConfig.RedoTables();
}

uint32 VDVFilterBrightCont::GetParams() {
	const VDXPixmapLayout& srcLayout = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& dstLayout = *fa->dst.mpPixmapLayout;

	switch(srcLayout.format) {
		case nsVDXPixmap::kPixFormat_RGB888:
		case nsVDXPixmap::kPixFormat_XRGB8888:
		case nsVDXPixmap::kPixFormat_Y8:
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV420_Planar:
		case nsVDXPixmap::kPixFormat_YUV420i_Planar:
		case nsVDXPixmap::kPixFormat_YUV411_Planar:
		case nsVDXPixmap::kPixFormat_YUV410_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV420_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV420i_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV410_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV420_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV420i_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV410_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_YUV420_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_YUV420i_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_YUV410_Planar_709_FR:
			break;

		case nsVDXPixmap::kPixFormat_VDXA_RGB:
		case nsVDXPixmap::kPixFormat_VDXA_YUV:
			return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_PURE_TRANSFORM;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
	}

	dstLayout.pitch = srcLayout.pitch;

	return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM;
}

void VDVFilterBrightCont::StartAccel(IVDXAContext *vdxa) {
	mVDXAFP = vdxa->CreateFragmentProgram(kVDXAPF_D3D9ByteCodePS20, kVDFilterBrightContFP, sizeof kVDFilterBrightContFP);
}

void VDVFilterBrightCont::RunAccel(IVDXAContext *vdxa) {
	float scale = mConfig.cont / 16.0f;
	float biasag = mConfig.bright / 255.0f;
	float biasrb = biasag;

	if (fa->src.mpPixmapLayout->format == nsVDXPixmap::kPixFormat_VDXA_YUV) {
		// y *= 255;
		// y = (y - 16) * 255 / 219;
		// y = a*y + b;
		// y = y * 219 / 255 + 16;
		// y /= 255;
		biasag = biasag * (219.0f / 255.0f) + (16.0f / 255.0f) * (1.0f - scale);

		// y *= 255;
		// y = (y - 128) * 255 / 224;
		// y = a*y + b;
		// y = y * 224 / 255 + 16;
		// y /= 255;
		biasrb = (128.0f / 255.0f) * (1.0f - scale);
	}

	float v[4];
	v[0] = scale;
	v[1] = biasag;
	v[2] = biasrb;
	v[3] = 0;
	vdxa->SetFragmentProgramConstF(0, 1, v);

	vdxa->SetSampler(0, fa->src.mVDXAHandle, kVDXAFilt_Point);
	vdxa->SetTextureMatrix(0, fa->src.mVDXAHandle, 0, 0, NULL);
	vdxa->DrawRect(fa->dst.mVDXAHandle, mVDXAFP, NULL);
}

void VDVFilterBrightCont::StopAccel(IVDXAContext *vdxa) {
	if (mVDXAFP) {
		vdxa->DestroyObject(mVDXAFP);
		mVDXAFP = 0;
	}
}

//////////////////

class VDVFilterBrightContDialog : public VDDialogFrameW32 {
public:
	VDVFilterBrightContDialog(VDVFilterBrightContConfig& config, IVDXFilterPreview *pifp) : VDDialogFrameW32(IDD_FILTER_BRIGHTCONT), mConfig(config), mpPreview(pifp) {}

	bool Activate(VDGUIHandle hParent) {
		return ShowDialog(hParent) != 0;
	}

protected:
	bool OnLoaded();
	bool OnCommand(uint32 id, uint32 extcode);

	INT_PTR DlgProc(UINT message, WPARAM wParam, LPARAM lParam);

	IVDXFilterPreview *const mpPreview;
	VDVFilterBrightContConfig&	mConfig;
};

bool VDVFilterBrightContDialog::OnLoaded() {
	HWND hWnd;

	hWnd = GetDlgItem(mhdlg, IDC_BRIGHTNESS);
	SendMessage(hWnd, TBM_SETTICFREQ, 16, 0);
	SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 512));
	SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mConfig.bright+256);

	hWnd = GetDlgItem(mhdlg, IDC_CONTRAST);
	SendMessage(hWnd, TBM_SETTICFREQ, 4, 0);
	SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 32));
	SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mConfig.cont);

	hWnd = GetDlgItem(mhdlg, IDC_PREVIEW);
	if (mpPreview) {
		EnableWindow(hWnd, TRUE);
		mpPreview->InitButton((VDXHWND)GetDlgItem(mhdlg, IDC_PREVIEW));
	}

	return false;
}

bool VDVFilterBrightContDialog::OnCommand(uint32 id, uint32 extcode) {
	switch(id) {
		case IDOK:
			mConfig.bright = SendMessage(GetDlgItem(mhdlg, IDC_BRIGHTNESS), TBM_GETPOS, 0, 0)-256;
			mConfig.cont = SendMessage(GetDlgItem(mhdlg, IDC_CONTRAST), TBM_GETPOS, 0, 0);

			End(true);
			return true;

		case IDCANCEL:
			End(false);
			return true;

		case IDC_PREVIEW:
			if (mpPreview)
				mpPreview->Toggle((VDXHWND)mhdlg);

			return true;
	}

	return false;
}

INT_PTR VDVFilterBrightContDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg)
	{
		case WM_HSCROLL:
			if (lParam) {
				HWND hwndScroll = (HWND)lParam;
				UINT id = GetWindowLong(hwndScroll, GWL_ID);

				if (id == IDC_BRIGHTNESS) {
					int bright = SendMessage(hwndScroll, TBM_GETPOS, 0, 0)-256;
					if (mConfig.bright != bright) {
						mConfig.bright = bright;

						if (mpPreview) {
							mConfig.RedoTables();
							mpPreview->RedoFrame();
						}
					}
				} else if (id == IDC_CONTRAST) {
					int cont = SendMessage(hwndScroll, TBM_GETPOS, 0, 0);
					if (mConfig.cont != cont) {
						mConfig.cont = cont;

						if (mpPreview) {
							mConfig.RedoTables();
							mpPreview->RedoFrame();
						}
					}
				}
					

				SetWindowLong(mhdlg, DWLP_MSGRESULT, 0);
				return TRUE;
			}
			break;
	}

	return VDDialogFrameW32::DlgProc(msg, wParam, lParam);
}

bool VDVFilterBrightCont::Configure(VDXHWND hwnd) {
	const VDVFilterBrightContConfig mOldConfig = mConfig;
	VDVFilterBrightContDialog dlg(mConfig, fa->ifp);

	if (!dlg.Activate((VDGUIHandle)hwnd)) {
		mConfig = mOldConfig;
		return false;
	}

	return true;
}

void VDVFilterBrightCont::GetSettingString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, " (bright %+d%%, cont %d%%)", (mConfig.bright*25)/64, (mConfig.cont*25)/4);
}

void VDVFilterBrightCont::ScriptConfig(IVDXScriptInterpreter *, const VDXScriptValue *argv, int argc) {
	mConfig.bright	= argv[0].asInt();
	mConfig.cont	= argv[1].asInt();
}

VDXVF_BEGIN_SCRIPT_METHODS(VDVFilterBrightCont)
VDXVF_DEFINE_SCRIPT_METHOD(VDVFilterBrightCont, ScriptConfig, "ii")
VDXVF_END_SCRIPT_METHODS()

void VDVFilterBrightCont::GetScriptString(char *buf, int maxlen) {
	SafePrintf(buf, maxlen, "Config(%d,%d)", mConfig.bright, mConfig.cont);
}


extern const VDXFilterDefinition g_VDVFBrightCont = VDXVideoFilterDefinition<VDVFilterBrightCont>(
	NULL,
	"brightness/contrast",
	"Adjusts brightness and contrast of an image linearly."
);

#ifdef _MSC_VER
	#pragma warning(disable: 4505)	// warning C4505: 'VDVFilterBrightCont::[thunk]: __thiscall VDVFilterBrightCont::`vcall'{24,{flat}}' }'' : unreferenced local function has been removed
#endif
