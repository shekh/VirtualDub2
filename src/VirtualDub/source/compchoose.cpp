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
#include <ctype.h>

#include <windows.h>
#include <commctrl.h>
#include <vfw.h>

#include <vd2/system/debug.h>
#include <vd2/system/filesys.h>
#include <vd2/system/protscope.h>
#include <vd2/system/text.h>
#include <vd2/system/binary.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/VDLib/UIProxies.h>

#include "resource.h"

#include "oshelper.h"
#include "misc.h"

extern HINSTANCE g_hInst;

const wchar_t g_szNo[]=L"No";
const wchar_t g_szYes[]=L"Yes";

///////////////////////////////////////////////////////////////////////////

INT_PTR CALLBACK ChooseCompressorDlgProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam);

///////////////////////////////////////////////////////////////////////////

void FreeCompressor(COMPVARS *pCompVars) {
	if (!(pCompVars->dwFlags & ICMF_COMPVARS_VALID))
		return;

	if (pCompVars->hic) {
		ICClose(pCompVars->hic);
		pCompVars->hic = NULL;
	}

	pCompVars->dwFlags &= ~ICMF_COMPVARS_VALID;
}

///////////////////////////////////////////////////////////////////////////

HIC ICOpenASV1(DWORD fccType, DWORD fccHandler, DWORD dwMode) {

	// ASUSASV1.DLL 1.0.4.0 causes a crash under Windows NT/2000 due to
	// nasty code that scans the video BIOS directly for "ASUS" by reading
	// 000C0000-000C00FF.  We workaround the problem by mapping a dummy
	// memory block over that address if nothing is there already.  This
	// will always fail under Windows 95/98.  We cannot just check for
	// the ASV1 code because the ASUSASVD.DLL codec does work and uses
	// the same code.
	//
	// Placing "ASUS" in the memory block works, but the codec checks
	// the string often during compression calls and would simply crash
	// on another call.  Also, there are potential licensing issues.
	// Thus, we simply leave the memory block blank, which basically
	// disables the codec but prevents it from crashing.
	//
	// Under Windows 2000, it appears that this region is always reserved.
	// So to be safe, use reserve->commit->ICOpen->decommit->release.

	LPVOID pDummyVideoBIOSRegion = VirtualAlloc((LPVOID)0x000C0000, 4096, MEM_RESERVE, PAGE_READWRITE);
	LPVOID pDummyVideoBIOS = VirtualAlloc((LPVOID)0x000C0000, 4096, MEM_COMMIT, PAGE_READWRITE);
	HIC hic;

	hic = ICOpen(fccType, fccHandler, dwMode);

	if (pDummyVideoBIOS)
		VirtualFree(pDummyVideoBIOS, 4096, MEM_DECOMMIT);

	if (pDummyVideoBIOSRegion)
		VirtualFree(pDummyVideoBIOSRegion, 0, MEM_RELEASE);

	return hic;
}

///////////////////////////////////////////////////////////////////////////

class VDUIDialogChooseVideoCompressorW32 : public VDDialogFrameW32 {
public:
	VDUIDialogChooseVideoCompressorW32(COMPVARS *cv, BITMAPINFOHEADER *src);

protected:
	struct CodecInfo : public ICINFO {
		bool mbFormatSupported;
	};

	bool OnLoaded();
	void OnDestroy();
	void OnDataExchange(bool write);
	bool OnCommand(uint32 id, uint32 extcode);
	void OnHScroll(uint32 id, int code);
	void OnHelp();
	void EnumerateCodecs();
	void RebuildCodecList();
	void UpdateEnables();
	void SelectCompressor(CodecInfo *pii);

	void OnCodecSelectionChanged(VDUIProxyListBoxControl *sender, int index);

	COMPVARS *mpCompVars;
	BITMAPINFOHEADER *mpSrcFormat;
	HIC		mhCodec;
	FOURCC	mfccSelect;
	vdblock<char>	mCodecState;
	CodecInfo *mpCurrent;

	VDStringW	mCurrentCompression;

	typedef vdfastvector<CodecInfo *> Codecs;
	Codecs mCodecs;

	VDUIProxyListBoxControl	mCodecList;
	VDDelegate mdelSelChanged;
};

VDUIDialogChooseVideoCompressorW32::VDUIDialogChooseVideoCompressorW32(COMPVARS *cv, BITMAPINFOHEADER *src)
	: VDDialogFrameW32(IDD_VIDEOCOMPRESSION)
	, mhCodec(NULL)
	, mfccSelect(0)
	, mpCurrent(NULL)
	, mpCompVars(cv)
	, mpSrcFormat(src)
{
	mCodecList.OnSelectionChanged() += mdelSelChanged.Bind(this, &VDUIDialogChooseVideoCompressorW32::OnCodecSelectionChanged);
}

bool VDUIDialogChooseVideoCompressorW32::OnLoaded() {
	if (!mpSrcFormat) {
		CheckButton(IDC_SHOW_ALL, true);
		EnableControl(IDC_SHOW_ALL, false);
	}

	AddProxy(&mCodecList, IDC_COMP_LIST);

	EnumerateCodecs();
	RebuildCodecList();

	TBSetRange(IDC_QUALITY_SLIDER, 0, 100);

	if (mpCompVars->dwFlags & ICMF_COMPVARS_VALID) {
		TBSetValue(IDC_QUALITY_SLIDER, (mpCompVars->lQ+50)/100);
		SetControlTextF(IDC_EDIT_QUALITY, L"%d", (mpCompVars->lQ+50)/100);
	}

	if ((mpCompVars->dwFlags & ICMF_COMPVARS_VALID) && mpCompVars->lKey) {
		CheckButton(IDC_USE_KEYFRAMES, BST_CHECKED);
		SetControlTextF(IDC_KEYRATE, L"%d", mpCompVars->lKey);
	} else
		CheckButton(IDC_USE_KEYFRAMES, BST_UNCHECKED);
	
	if ((mpCompVars->dwFlags & ICMF_COMPVARS_VALID) && mpCompVars->lDataRate) {
		CheckButton(IDC_USE_DATARATE, BST_CHECKED);
		SetControlTextF(IDC_DATARATE, L"%d", mpCompVars->lDataRate);
	} else
		CheckButton(IDC_USE_DATARATE, BST_UNCHECKED);

	SetFocusToControl(IDC_COMP_LIST);
	return true;
}

void VDUIDialogChooseVideoCompressorW32::OnDestroy() {
	if (mhCodec) {
		ICClose(mhCodec);
		mhCodec = NULL;
	}

	while(!mCodecs.empty()) {
		ICINFO *ici = mCodecs.back();
		delete ici;

		mCodecs.pop_back();
	}
}

void VDUIDialogChooseVideoCompressorW32::OnDataExchange(bool write) {
	if (write) {
		if (!(mpCompVars->dwFlags & ICMF_COMPVARS_VALID)) {
			memset(mpCompVars, 0, sizeof(COMPVARS));

			mpCompVars->dwFlags = ICMF_COMPVARS_VALID;
		}
		mpCompVars->fccType = 'CDIV';

		int ind = mCodecList.GetSelection();
		if (ind > 0) {
			ind = mCodecList.GetItemData(ind);

			mpCompVars->fccHandler = mCodecs[ind]->fccHandler;
		} else
			mpCompVars->fccHandler = 0;

		if (IsButtonChecked(IDC_USE_KEYFRAMES))
			mpCompVars->lKey = GetControlValueSint32(IDC_KEYRATE);
		else
			mpCompVars->lKey = 0;

		if (IsButtonChecked(IDC_USE_DATARATE))
			mpCompVars->lDataRate = GetControlValueSint32(IDC_DATARATE);
		else
			mpCompVars->lDataRate = 0;

		mpCompVars->lQ = TBGetValue(IDC_QUALITY_SLIDER)*100;

		if (mhCodec)
			ICSendMessage(mhCodec, ICM_SETQUALITY, (DWORD_PTR)&mpCompVars->lQ, 0);

		if (mpCompVars->hic)
			ICClose(mpCompVars->hic);
		mpCompVars->hic = mhCodec;
		mhCodec = NULL;
	}
}

bool VDUIDialogChooseVideoCompressorW32::OnCommand(uint32 id, uint32 extcode) {
	switch(id) {
		case IDC_CONFIGURE:
			if (mhCodec) {
				ICConfigure(mhCodec, mhdlg);
				mCodecState.clear();

				if (mpCurrent)
					UpdateEnables();
			}
			return TRUE;

		case IDC_ABOUT:
			if (mhCodec)
				ICAbout(mhCodec, mhdlg);
			return TRUE;

		case IDC_EDIT_QUALITY:
			if (extcode == EN_KILLFOCUS) {
				mbValidationFailed = false;
				int v = GetControlValueSint32(IDC_EDIT_QUALITY);

				if (mbValidationFailed) {
					MessageBeep(MB_ICONEXCLAMATION);
					SetFocusToControl(IDC_EDIT_QUALITY);
					return true;
				}

				if (v < 0 || v > 100) {
					v = v<0 ? 0 : 100;

					SetControlTextF(IDC_EDIT_QUALITY, L"%d", v);
				}

				TBSetValue(IDC_QUALITY_SLIDER, v);
				return true;
			}
			return false;

		case IDC_SHOW_ALL:
			RebuildCodecList();
			return true;
	}

	return false;
}

void VDUIDialogChooseVideoCompressorW32::OnHScroll(uint32 id, int code) {
	if (!id)
		return;

	mpCompVars->lQ = TBGetValue(IDC_QUALITY_SLIDER);
	SetControlTextF(IDC_EDIT_QUALITY, L"%d", mpCompVars->lQ, FALSE);

	// Well... it seems Microsoft's ICCompressorChoose() never sends this.

	//if (mhCodec)
	//	ICSendMessage(mhCodec, ICM_SETQUALITY, mpCompVars->lQ, 0);
}

void VDUIDialogChooseVideoCompressorW32::OnHelp() {
	VDShowHelp(mhdlg, L"d-videocompression.html");
}

void VDUIDialogChooseVideoCompressorW32::EnumerateCodecs() {
	ICINFO info = {sizeof(ICINFO)};
	int i;
	int nComp;

	if (mpCompVars->dwFlags & ICMF_COMPVARS_VALID) {
		mfccSelect	= mpCompVars->fccHandler;

		if (mpCompVars->hic) {
			int len = ICGetStateSize(mpCompVars->hic);

			if (len > 0) {
				mCodecState.resize(len);

				ICGetState(mpCompVars->hic, mCodecState.data(), len);
			}
		}
	}

	nComp = 0;

	if (mpSrcFormat && mpSrcFormat->biCompression != BI_RGB) {
		union {
			char fccbuf[5];
			FOURCC fcc;
		};

		fcc = mpSrcFormat->biCompression;
		fccbuf[4] = 0;

		mCurrentCompression.sprintf(L"(No recompression: %hs)", fccbuf);
	} else
		mCurrentCompression = L"(Uncompressed RGB/YCbCr)";

	vdprotected("enumerating video codecs") {
		for(i=0; ICInfo(ICTYPE_VIDEO, i, &info); i++) {
			HIC hic;

			// Use "special" routine for ASV1.

			union {
				FOURCC fcc;
				char buf[5];
			} u = {info.fccHandler};
			u.buf[4] = 0;

			vdprotected1("opening video codec with FOURCC \"%.4s\"", const char *, u.buf) {
				{
					wchar_t buf[64];
					swprintf(buf, 64, L"A video codec with FOURCC '%.4S'", (const char *)&info.fccHandler);
					VDExternalCodeBracket bracket(buf, __FILE__, __LINE__);

					if (isEqualFOURCC(info.fccHandler, '1VSA'))
						hic = ICOpenASV1(info.fccType, info.fccHandler, ICMODE_COMPRESS);
					else	
						hic = ICOpen(info.fccType, info.fccHandler, ICMODE_COMPRESS);
				}

				if (hic) {
					ICINFO ici = { sizeof(ICINFO) };
					char namebuf[64];

					namebuf[0] = 0;

					if (ICGetInfo(hic, &ici, sizeof(ICINFO)))
						VDTextWToA(namebuf, sizeof namebuf, ici.szDescription, -1);

					bool formatSupported = false;

					if (mpSrcFormat) {
						vdprotected1("querying video codec \"%.64s\"", const char *, namebuf) {
							if (ICERR_OK==ICCompressQuery(hic, mpSrcFormat, NULL))
								formatSupported = true;
						}
					} else
						formatSupported = true;

					CodecInfo *pii = new CodecInfo;
					static_cast<ICINFO&>(*pii) = ici;
					pii->fccHandler = info.fccHandler;
					pii->mbFormatSupported = formatSupported;
					mCodecs.push_back(pii);

					ICClose(hic);
				}
			}
		}
	}
}

void VDUIDialogChooseVideoCompressorW32::RebuildCodecList() {
	const bool showAll = IsButtonChecked(IDC_SHOW_ALL);

	mCodecList.Clear();
	mCodecList.AddItem(mCurrentCompression.c_str(), -1);

	bool foundSelected = false;

	size_t n = mCodecs.size();
	for(size_t i=0; i<n; ++i) {
		CodecInfo& ici = *mCodecs[i];

		bool isSelected = isEqualFOURCC(ici.fccHandler, mfccSelect);

		if (!isSelected && !ici.mbFormatSupported && !showAll)
			continue;

		int ind = mCodecList.AddItem(ici.szDescription, i);
		if (ind < 0)
			continue;

		if (!foundSelected && isSelected) {
			foundSelected = true;

			mCodecList.SetSelection(ind);

			SelectCompressor(&ici);
		}
	}

	if (!mfccSelect) {
		mCodecList.SetSelection(0);
		SelectCompressor(NULL);
	} else {
		// force selection to be visible with sorting
		mCodecList.MakeSelectionVisible();
	}
}

void VDUIDialogChooseVideoCompressorW32::UpdateEnables() {
	ICINFO info = {sizeof(ICINFO)};

	if (mhCodec) {
		// Ask the compressor for its information again, because some
		// compressors change their flags after certain config options
		// are changed... that means you, SERGE ;-)
		//
		// Preserve the existing fccHandler during the copy.  This allows
		// overloaded codecs (i.e. 'MJPG' for miroVideo DRX, 'mjpx' for
		// PICVideo, 'mjpy' for MainConcept, etc.)

		if (ICGetInfo(mhCodec, &info, sizeof info)) {
			FOURCC fccHandler = mpCurrent->fccHandler;

			static_cast<ICINFO&>(*mpCurrent) = info;
			mpCurrent->fccHandler = fccHandler;
		}

		// Query compressor for caps and enable buttons as appropriate.

		EnableControl(IDC_ABOUT, ICQueryAbout(mhCodec));
		EnableControl(IDC_CONFIGURE, ICQueryConfigure(mhCodec));

	} else {
		EnableControl(IDC_ABOUT, FALSE);
		EnableControl(IDC_CONFIGURE, FALSE);
	}

	DWORD dwFlags = 0;
	if (mpCurrent)
		dwFlags = mpCurrent->dwFlags;

	bool enable;
	enable = !!(dwFlags & (VIDCF_CRUNCH | VIDCF_QUALITY));		// Strange but true: Windows expects to be able to crunch even if only the quality bit is set.

	EnableControl(IDC_USE_DATARATE, enable);
	EnableControl(IDC_DATARATE, enable);
	EnableControl(IDC_STATIC_DATARATE, enable);

	enable = !!(dwFlags & (VIDCF_TEMPORAL | VIDCF_FASTTEMPORALC));

	EnableControl(IDC_USE_KEYFRAMES, enable);
	EnableControl(IDC_KEYRATE, enable);
	EnableControl(IDC_STATIC_KEYFRAMES, enable);

	enable = !!(dwFlags & VIDCF_QUALITY);

	EnableControl(IDC_EDIT_QUALITY, enable);
	EnableControl(IDC_QUALITY_SLIDER, enable);
	EnableControl(IDC_STATIC_QUALITY_LABEL, enable);
}

static int g_xres[]={
	160, 176, 320, 352, 640, 720
};

static int g_yres[]={
	120, 144, 240, 288, 480, 576
};

static int g_depths[]={
	16, 24, 32,
	48,
	64
};

static int g_depths_fcc[]={
	0, 0, 0,
	VDMAKEFOURCC('b', '4', '8', 'r'),
	VDMAKEFOURCC('b', '6', '4', 'a')
};


#define		NWIDTHS		(sizeof g_xres / sizeof g_xres[0])
#define		NHEIGHTS	(sizeof g_yres / sizeof g_yres[0])
#define		NDEPTHS		(sizeof g_depths / sizeof g_depths[0])

void set_depth(BITMAPINFO& bi, int k) {
	int d = g_depths[k];
	int w = bi.bmiHeader.biWidth;
	int h = bi.bmiHeader.biHeight;

	bi.bmiHeader.biBitCount = (WORD)d;
	bi.bmiHeader.biSizeImage = ((w*d+31)/32)*4*h;
	bi.bmiHeader.biCompression = g_depths_fcc[k];
}

void VDUIDialogChooseVideoCompressorW32::SelectCompressor(CodecInfo *pii) {
	// Clear restrictions box.

	LBClear(IDC_SIZE_RESTRICTIONS);

	if (!pii || !pii->fccHandler) {
		if (mhCodec) {
			ICClose(mhCodec);
			mhCodec = NULL;
		}

		SetControlText(IDC_STATIC_DELTA, g_szNo);
		SetControlText(IDC_STATIC_FOURCC, L"");
		SetControlText(IDC_STATIC_DRIVER, L"");

		mpCurrent = pii;
		UpdateEnables();
		return;
	}

	// Show driver caps.

	SetControlText(IDC_STATIC_DELTA, (pii->dwFlags & (VIDCF_TEMPORAL|VIDCF_FASTTEMPORALC)) ? g_szYes : g_szNo);

	// Show driver fourCC code.

	wchar_t fccbuf[7];
	for(int i=0; i<4; ++i) {
		char c = ((char *)&pii->fccHandler)[i];

		if (isprint((unsigned char)c))
			fccbuf[i+1] = c;
		else
			fccbuf[i+1] = ' ';
	}

	fccbuf[0] = fccbuf[5] = '\'';
	fccbuf[6] = 0;
	SetControlText(IDC_STATIC_FOURCC, fccbuf);

	SetControlText(IDC_STATIC_DRIVER, VDFileSplitPath(pii->szDriver));

	// Attempt to open the compressor.

	if (mhCodec) {
		ICClose(mhCodec);
		mhCodec = NULL;
	}

	{
		wchar_t buf[64];
		swprintf(buf, 64, L"A video codec with FOURCC '%.4S'", (const char *)&pii->fccHandler);
		VDExternalCodeBracket bracket(buf, __FILE__, __LINE__);

		mhCodec = ICOpen(pii->fccType, pii->fccHandler, ICMODE_COMPRESS);
	}

	if (!mhCodec) {
		LBAddString(IDC_SIZE_RESTRICTIONS, L"<Unable to open driver>");
		return;
	}

	if (pii->fccHandler == mfccSelect && !mCodecState.empty())
		ICSetState(mhCodec, mCodecState.data(), mCodecState.size());

	mpCurrent = pii;
	UpdateEnables();

	// Start querying the compressor for what it can handle

	BITMAPINFO bi;
	bi.bmiHeader.biSize				= sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biPlanes			= 1;
	bi.bmiHeader.biCompression		= BI_RGB;
	bi.bmiHeader.biXPelsPerMeter	= 80;
	bi.bmiHeader.biYPelsPerMeter	= 72;
	bi.bmiHeader.biClrUsed			= 0;
	bi.bmiHeader.biClrImportant		= 0;

	// Loop until we can find a width, height, and depth that works!

	int w;
	int h;
	int kd;

	for(int i=0; i<NWIDTHS; i++) {
		w = g_xres[i];
		bi.bmiHeader.biWidth = w;

		for(int j=0; j<NHEIGHTS; j++) {
			h = g_yres[j];
			bi.bmiHeader.biHeight = h;

			for(int k=0; k<NDEPTHS; k++) {
				set_depth(bi,k);
				kd = k;

				if (ICERR_OK == ICCompressQuery(mhCodec, &bi.bmiHeader, NULL))
					goto pass;
			}
		}
	}

	LBAddString(IDC_SIZE_RESTRICTIONS, L"Couldn't find compatible format.");
	LBAddString(IDC_SIZE_RESTRICTIONS, L"Possible reasons:");
	LBAddString(IDC_SIZE_RESTRICTIONS, L"*  Codec may only support YUV");
	LBAddString(IDC_SIZE_RESTRICTIONS, L"*  Codec might be locked.");
	LBAddString(IDC_SIZE_RESTRICTIONS, L"*  Codec might be decompression-only");
	return;

pass:

	int depth_bits = 0;

	// Check all the depths; see if they work

	for(int k=0; k<NDEPTHS; k++) {
		set_depth(bi,k);

		if (ICERR_OK == ICCompressQuery(mhCodec, &bi.bmiHeader, NULL))
			depth_bits |= (1<<k);
	}

	VDStringW s;
	int i, j, k;

	// Look for X alignment

	for(i=3; i>=0; i--) {
		bi.bmiHeader.biWidth	 = w + (1<<i);
		set_depth(bi,kd);

		if (ICERR_OK != ICCompressQuery(mhCodec, &bi.bmiHeader, NULL))
			break;

	}

	bi.bmiHeader.biWidth	 = w + (1<<(i+2));
	set_depth(bi,kd);

	if (ICERR_OK != ICCompressQuery(mhCodec, &bi.bmiHeader, NULL))
		i = -2;

	if (i>=0) {
		s.sprintf(L"Width must be a multiple of %d", 1<<(i+1));
		LBAddString(IDC_SIZE_RESTRICTIONS, s.c_str());
	} else if (i<-1) {
		s.sprintf(L"Width: unknown (%dx%d worked)", w, h);
		LBAddString(IDC_SIZE_RESTRICTIONS, s.c_str());
	}

	// Look for Y alignment

	bi.bmiHeader.biWidth = w;

	for(j=3; j>=0; j--) {
		bi.bmiHeader.biHeight	 = h + (1<<j);
		set_depth(bi,kd);

		if (ICERR_OK != ICCompressQuery(mhCodec, &bi.bmiHeader, NULL))
			break;
	}

	bi.bmiHeader.biHeight	 = h + (1<<(j+2));
	set_depth(bi,kd);

	if (ICERR_OK != ICCompressQuery(mhCodec, &bi.bmiHeader, NULL))
		j = -2;

	if (j>=0) {
		s.sprintf(L"Height must be a multiple of %d", 1<<(j+1));
		LBAddString(IDC_SIZE_RESTRICTIONS, s.c_str());
	} else if (j<-1) {
		s.sprintf(L"Height: unknown (%dx%d worked)", w, h);
		LBAddString(IDC_SIZE_RESTRICTIONS, s.c_str());
	}

	// Print out results
	if (depth_bits != 7) {
		s = L"Valid depths:";

		for(k=0; k<NDEPTHS; k++)
			if (depth_bits & (1<<k))
				s.append_sprintf(L" %d", g_depths[k]);

		LBAddString(IDC_SIZE_RESTRICTIONS, s.c_str());
	}

	if (depth_bits==7 && i<0 && j<0)
		LBAddString(IDC_SIZE_RESTRICTIONS, L"No known restrictions.");
}

void VDUIDialogChooseVideoCompressorW32::OnCodecSelectionChanged(VDUIProxyListBoxControl *sender, int index) {
	if (index < 0)
		return;

	int data = mCodecList.GetItemData(index);

	CodecInfo *pii = data >= 0 ? mCodecs[data] : NULL;

	SelectCompressor(pii);
}

///////////////////////////////////////////////////////////////////////////

void ChooseCompressor(HWND hwndParent, COMPVARS *lpCompVars, BITMAPINFOHEADER *bihInput) {
	VDUIDialogChooseVideoCompressorW32 dlg(lpCompVars, bihInput);

	dlg.ShowDialog((VDGUIHandle)hwndParent);
}

