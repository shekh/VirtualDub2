//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#include <vd2/system/vdtypes.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDLib/Dialog.h>
#include "resource.h"

extern HINSTANCE g_hInst;

///////////////////////

// 4:2:0 MPEG-1:			[1 2 1]/4
// 4:2:0/4:2:2 MPEG-2:		1 [1 1]/2
// 4:1:0:					[1 3 3 1]/8

namespace {
	enum {
		kModeDisable,
		kMode420MPEG1,
		kMode420MPEG2,
		kMode422,
		kMode410,
		kMode411,
	};

	inline int luma(const uint32 p) {
		return ((p>>16)&255)*77 + ((p>>8)&255)*150 + (p&255)*29;
	}

	inline uint32 filterMPEG1(uint32 c1, int y1, uint32 c2, int y2, uint32 c3, int y3) {
		int yd = (2*y2 - (y1+y3) + 512) >> 10;
		unsigned r = yd + ((((c1>>16)&0xff) + 2*((c2>>16)&0xff) + ((c3>>16)&0xff) + 2) >> 2);
		unsigned g = yd + ((((c1>> 8)&0xff) + 2*((c2>> 8)&0xff) + ((c3>> 8)&0xff) + 2) >> 2);
		unsigned b = yd + ((((c1    )&0xff) + 2*((c2    )&0xff) + ((c3    )&0xff) + 2) >> 2);

		if (r >= 0x100)
			r = ((int)~r >> 31) & 0xff;
		if (g >= 0x100)
			g = ((int)~g >> 31) & 0xff;
		if (b >= 0x100)
			b = ((int)~b >> 31) & 0xff;

		return (r<<16) + (g<<8) + b;
	}

	void FilterHorizontalMPEG1(uint32 *dst, const uint32 *src, int count) {
		if (count < 1)
			return;

		uint32 c1, c2, c3;
		int y1, y2, y3;

		c2 = c3 = *src++;
		y2 = y3 = luma(c2);

		--count;
		for(int repcount = 1; repcount >= 0; --repcount) {
			if (count>0) do {
				c1 = c2;
				y1 = y2;
				c2 = c3;
				y2 = y3;
				c3 = *src++;
				y3 = luma(c3);

				*dst++ = filterMPEG1(c1, y1, c2, y2, c3, y3);
			} while(--count);

			count = 1;
			--src;
		}
	}

	void FilterVerticalMPEG1(uint32 *dst, const uint32 *const *src, int count) {
		if (count <= 0)
			return;

		const uint32 *src0 = src[-3];
		const uint32 *src1 = src[-2];
		const uint32 *src2 = src[-1];

		do {
			const uint32 c0 = *src0++;
			const uint32 c1 = *src1++;
			const uint32 c2 = *src2++;
			const int y0 = luma(c0);
			const int y1 = luma(c1);
			const int y2 = luma(c2);

			*dst++ = filterMPEG1(c0, y0, c1, y1, c2, y2);
		} while(--count);
	}

	inline uint32 filterMPEG2(uint32 c1, int y1, uint32 c2, int y2) {
		int yd = (y1 - y2 + 256) >> 9;
		unsigned r = yd + ((((c1>>16)&0xff) + ((c2>>16)&0xff) + 1) >> 1);
		unsigned g = yd + ((((c1>> 8)&0xff) + ((c2>> 8)&0xff) + 1) >> 1);
		unsigned b = yd + ((((c1    )&0xff) + ((c2    )&0xff) + 1) >> 1);

		if (r >= 0x100)
			r = ((int)~r >> 31) & 0xff;
		if (g >= 0x100)
			g = ((int)~g >> 31) & 0xff;
		if (b >= 0x100)
			b = ((int)~b >> 31) & 0xff;

		return (r<<16) + (g<<8) + b;
	}

	void FilterHorizontalMPEG2(uint32 *dst, const uint32 *src, int count) {
		uint32 c1, c2;
		int y1, y2;

		c2 = *src++;
		y2 = luma(c2);

		if (--count > 0)
			do {
				c1 = c2;
				y1 = y2;
				c2 = *src++;
				y2 = luma(c2);

				*dst++ = filterMPEG2(c1, y1, c2, y2);
			} while(--count);

		*dst++ = c2;
	}

	inline uint32 filterMPEG4(uint32 c1, int y1, uint32 c2, int y2, uint32 c3, int y3, uint32 c4, int y4, uint32 c5, int y5) {
		int yd = (6*y3 - y1 - 2*(y2+y4) - y5 + 1024) >> 11;
		unsigned r = yd + ((((c1>>16)&0xff) + ((c5>>16)&0xff) + 2*(((c2>>16)&0xff) + ((c3>>16)&0xff) + ((c4>>16)&0xff)) + 4) >> 3);
		unsigned g = yd + ((((c1>> 8)&0xff) + ((c5>> 8)&0xff) + 2*(((c2>> 8)&0xff) + ((c3>> 8)&0xff) + ((c4>> 8)&0xff)) + 4) >> 3);
		unsigned b = yd + ((((c1    )&0xff) + ((c5    )&0xff) + 2*(((c2    )&0xff) + ((c3    )&0xff) + ((c4    )&0xff)) + 4) >> 3);

		if (r >= 0x100)
			r = ((int)~r >> 31) & 0xff;
		if (g >= 0x100)
			g = ((int)~g >> 31) & 0xff;
		if (b >= 0x100)
			b = ((int)~b >> 31) & 0xff;

		return (r<<16) + (g<<8) + b;
	}

	void FilterHorizontalMPEG4(uint32 *dst, const uint32 *src, int count) {
		if (count < 2) {
			while(count-->0)
				*dst++ = *src++;
			return;
		}

		uint32 c1, c2, c3, c4, c5;
		int y1, y2, y3, y4, y5;

		c2 = c3 = c4 = *src++;
		y2 = y3 = y4 = luma(c2);
		c5 = *src++;
		y5 = luma(c5);

		count -= 2;
		for(int tailcount = 2; tailcount >= 0; --tailcount) {
			if (count > 0)
				do {
					c1 = c2;
					y1 = y2;
					c2 = c3;
					y2 = y3;
					c3 = c4;
					y3 = y4;
					c4 = c5;
					y4 = y5;
					c5 = *src++;
					y5 = luma(c5);
					*dst++ = filterMPEG4(c1, y1, c2, y2, c3, y3, c4, y4, c5, y5);
				} while(--count);

			count = 1;
			--src;
		}
	}

	void FilterVerticalMPEG4(uint32 *dst, const uint32 *const *src, int count) {
		if (count <= 0)
			return;

		const uint32 *src0 = src[-5];
		const uint32 *src1 = src[-4];
		const uint32 *src2 = src[-3];
		const uint32 *src3 = src[-2];
		const uint32 *src4 = src[-1];

		do {
			const uint32 c0 = *src0++;
			const uint32 c1 = *src1++;
			const uint32 c2 = *src2++;
			const uint32 c3 = *src3++;
			const uint32 c4 = *src4++;
			const int y0 = luma(c0);
			const int y1 = luma(c1);
			const int y2 = luma(c2);
			const int y3 = luma(c3);
			const int y4 = luma(c4);

			*dst++ = filterMPEG4(c0, y0, c1, y1, c2, y2, c3, y3, c4, y4);
		} while(--count);
	}

	void FilterHorizontalCopy(uint32 *dst, const uint32 *src, int count) {
		memcpy(dst, src, count*4);
	}
	void FilterVerticalCopy(uint32 *dst, const uint32 *const *src, int count) {
		memcpy(dst, src[-1], count*4);
	}
}

///////////////////////////////////////////////////////////////////////////

#ifdef _M_IX86
extern "C" void asm_chromasmoother_FilterHorizontalMPEG1_MMX(uint32 *dst, const uint32 *src, int count);
extern "C" void asm_chromasmoother_FilterHorizontalMPEG2_MMX(uint32 *dst, const uint32 *src, int count);
extern "C" void asm_chromasmoother_FilterVerticalMPEG1_MMX(uint32 *dst, const uint32 *const *src, int count);
#endif

class VDVFChromaSmoother : public VDXVideoFilter {
public:
	VDVFChromaSmoother() : mMode(0) {}

	void Run();
	uint32 GetParams();
	bool Configure(VDXHWND hwnd);
	void Start();
	void End();
	void GetSettingString(char *buf, int maxlen);
	void GetScriptString(char *buf, int maxlen);

	VDXVF_DECLARE_SCRIPT_METHODS();

	void ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc);

protected:
	vdfastvector<uint32> mTempRows;
	uint32 *mTempWindow[5];

	int		mMode;
};

///////////////////////////////////////////////////////////////////////////

void VDVFChromaSmoother::Run() {
	const char *src = (const char *)fa->src.data;
	const ptrdiff_t srcpitch = fa->src.pitch;
	char *dst = (char *)fa->dst.data;
	ptrdiff_t dstpitch = fa->dst.pitch;
	const int w = fa->dst.w;
	const int h = fa->dst.h;

	int nextsrc = 0;
	int preroll = 0;
	int postlen = 1;

	void (*pHorizFilt)(uint32 *dst, const uint32 *src, int count) = FilterHorizontalCopy;
	void (*pVertFilt)(uint32 *dst, const uint32 *const *src, int count) = FilterVerticalCopy;
	
#ifdef _M_IX86
	switch(mMode) {
	case kMode420MPEG1:
		if (w >= 1)
			pHorizFilt = MMX_enabled ? asm_chromasmoother_FilterHorizontalMPEG1_MMX : FilterHorizontalMPEG1;
		pVertFilt	= MMX_enabled ? asm_chromasmoother_FilterVerticalMPEG1_MMX : FilterVerticalMPEG1;
		preroll		= 1;
		postlen		= 2;
		break;
	case kMode420MPEG2:
		pHorizFilt	= MMX_enabled ? asm_chromasmoother_FilterHorizontalMPEG2_MMX : FilterHorizontalMPEG2;
		pVertFilt	= MMX_enabled ? asm_chromasmoother_FilterVerticalMPEG1_MMX : FilterVerticalMPEG1;
		preroll		= 1;
		postlen		= 2;
		break;
	case kMode422:
		pHorizFilt	= MMX_enabled ? asm_chromasmoother_FilterHorizontalMPEG2_MMX : FilterHorizontalMPEG2;
		break;
	case kMode410:
		pHorizFilt	= FilterHorizontalMPEG4;
		pVertFilt	= FilterVerticalMPEG4;
		preroll		= 1;
		postlen		= 3;
		break;
	case kMode411:
		if (w >= 2)
			pHorizFilt = FilterHorizontalMPEG4;
		break;
	}
#else
	switch(mMode) {
	case kMode420MPEG1:
		if (w >= 1)
			pHorizFilt = FilterHorizontalMPEG1;
		pVertFilt	= FilterVerticalMPEG1;
		preroll		= 1;
		postlen		= 2;
		break;
	case kMode420MPEG2:
		pHorizFilt	= FilterHorizontalMPEG2;
		pVertFilt	= FilterVerticalMPEG1;
		preroll		= 1;
		postlen		= 2;
		break;
	case kMode422:
		pHorizFilt	= FilterHorizontalMPEG2;
		break;
	case kMode410:
		pHorizFilt	= FilterHorizontalMPEG4;
		pVertFilt	= FilterVerticalMPEG4;
		preroll		= 1;
		postlen		= 3;
		break;
	case kMode411:
		if (w >= 2)
			pHorizFilt = FilterHorizontalMPEG4;
		break;
	}
#endif

	for(int y=0; y<h; ++y) {
		while(nextsrc < y+postlen) {
			std::rotate(mTempWindow + 0, mTempWindow + 1, mTempWindow + 5);
			pHorizFilt(mTempWindow[4], (const uint32 *)src, w);

			if (preroll > 0)
				--preroll;
			else if (++nextsrc < h)
				src += srcpitch;
		}

		pVertFilt((uint32 *)dst, mTempWindow + 5, w);
		dst += dstpitch;
	}

#ifndef _M_AMD64
	if (MMX_enabled)
		__asm emms
#endif
}

uint32 VDVFChromaSmoother::GetParams() {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

void VDVFChromaSmoother::Start() {
	const VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;
	const int w = pxldst.w;

	mTempRows.resize(w * 5);
	for(int i=0; i<5; ++i)
		mTempWindow[i] = &mTempRows[w * i];
}

void VDVFChromaSmoother::End() {
	vdfastvector<uint32>().swap(mTempRows);
}

void VDVFChromaSmoother::GetSettingString(char *buf, int buflen) {
	static const char *const sModes[]={
		"disabled",
		"4:2:0 MPEG-1",
		"4:2:0 MPEG-2",
		"4:2:2",
		"4:1:0",
		"4:1:1",
	};

	SafePrintf(buf, buflen, " (mode: %s)", sModes[mMode]);
}

void VDVFChromaSmoother::GetScriptString(char *buf, int buflen) {
	SafePrintf(buf, buflen, "Config(%d)", mMode);
}

class VDVFChromaSmootherDialog : public VDDialogFrameW32 {
public:
	VDVFChromaSmootherDialog(int& mode, IVDXFilterPreview *pifp)
		: VDDialogFrameW32(IDD_FILTER_CHROMASMOOTHER)
		, mMode(mode)
		, mpPreview(pifp)
	{
	}

protected:
	bool OnLoaded();
	void OnDataExchange(bool write);
	bool OnCommand(uint32 id, uint32 extcode);

	IVDXFilterPreview *const mpPreview;
	int&	mMode;
};

bool VDVFChromaSmootherDialog::OnLoaded() {
	if (mpPreview)
		mpPreview->InitButton((VDXHWND)GetDlgItem(mhdlg, IDC_PREVIEW));

	return VDDialogFrameW32::OnLoaded();
}

void VDVFChromaSmootherDialog::OnDataExchange(bool write) {
	if (write) {
	} else {
		switch(mMode) {
		case kModeDisable:
			CheckButton(IDC_MODE_DISABLE, true);
			break;
		case kMode420MPEG1:
			CheckButton(IDC_MODE_420MPEG1, true);
			break;
		case kMode420MPEG2:
			CheckButton(IDC_MODE_420MPEG2, true);
			break;
		case kMode422:
			CheckButton(IDC_MODE_422, true);
			break;
		case kMode410:
			CheckButton(IDC_MODE_410, true);
			break;
		case kMode411:
			CheckButton(IDC_MODE_411, true);
			break;
		}
	}
}

bool VDVFChromaSmootherDialog::OnCommand(uint32 id, uint32 extcode) {
	if (extcode)
		return false;

	switch(id) {
		case IDC_MODE_DISABLE:
			mMode = kModeDisable;
			mpPreview->RedoFrame();
			return true;

		case IDC_MODE_420MPEG1:
			mMode = kMode420MPEG1;
			mpPreview->RedoFrame();
			return true;

		case IDC_MODE_420MPEG2:
			mMode = kMode420MPEG2;
			mpPreview->RedoFrame();
			return true;

		case IDC_MODE_422:
			mMode = kMode422;
			mpPreview->RedoFrame();
			return true;

		case IDC_MODE_410:
			mMode = kMode410;
			mpPreview->RedoFrame();
			return true;

		case IDC_MODE_411:
			mMode = kMode411;
			mpPreview->RedoFrame();
			return true;

		case IDC_PREVIEW:
			if (mpPreview)
				mpPreview->Toggle((VDXHWND)mhdlg);
			return true;
	}

	return false;
}

bool VDVFChromaSmoother::Configure(VDXHWND hwnd) {
	const int mOldMode = mMode;
	VDVFChromaSmootherDialog dlg(mMode, fa->ifp);

	if (!dlg.ShowDialog((VDGUIHandle)hwnd)) {
		mMode = mOldMode;
		return false;
	}

	return true;
}

VDXVF_BEGIN_SCRIPT_METHODS(VDVFChromaSmoother)
	VDXVF_DEFINE_SCRIPT_METHOD(VDVFChromaSmoother, ScriptConfig, "i")
VDXVF_END_SCRIPT_METHODS()

void VDVFChromaSmoother::ScriptConfig(IVDXScriptInterpreter *isi, const VDXScriptValue *argv, int argc) {
	mMode = argv[0].asInt();
}

extern const VDXFilterDefinition g_VDVFChromaSmoother = VDXVideoFilterDefinition<VDVFChromaSmoother>(
	NULL,
	"chroma smoother",
	"Applies linear interpolation to point-upsampled chroma without affecting luma.");
