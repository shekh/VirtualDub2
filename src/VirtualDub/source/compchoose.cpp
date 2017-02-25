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
#include <vd2/Riza/videocodec.h>
#include <vd2/Riza/bitmap.h>
#include <vd2/Kasumi/pixmaputils.h>

#include "resource.h"

#include "oshelper.h"
#include "misc.h"
#include "plugins.h"
#include "dub.h"
#include "videoSource.h"

extern HINSTANCE g_hInst;
extern std::list<class VDExternalModule *>		g_pluginModules;
extern DubOptions			g_dubOpts;
extern vdrefptr<IVDVideoSource> inputVideo;

const wchar_t g_szNo[]=L"No";
const wchar_t g_szYes[]=L"Yes";

///////////////////////////////////////////////////////////////////////////

INT_PTR CALLBACK ChooseCompressorDlgProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam);

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
	VDUIDialogChooseVideoCompressorW32(COMPVARS2 *cv, BITMAPINFOHEADER *src);

	bool mCapture;

protected:
	struct CodecInfo : public ICINFO {
		bool mbFormatSupported;
		VDStringW path;
		int select;
	};

	struct CodecSort {
		bool operator()(CodecInfo *x, CodecInfo *y) const {
			if (x->path.empty() && !y->path.empty()) return false;
			if (!x->path.empty() && y->path.empty()) return true;
			return wcscmp(x->szDescription,y->szDescription)<0;
		}
	};

	bool OnLoaded();
	void OnDestroy();
	void OnDataExchange(bool write);
	bool OnCommand(uint32 id, uint32 extcode);
	void OnHScroll(uint32 id, int code);
	void OnHelp();
	void EnumerateCodecs();
	void EnumeratePluginCodecs();
	void RebuildCodecList();
	void UpdateEnables();
	void UpdateFormat();
	void SelectCompressor(CodecInfo *pii);

	void OnCodecSelectionChanged(VDUIProxyListBoxControl *sender, int index);
	void SetVideoDepthOptionsAsk();
	int testFormat(EncoderHIC* plugin);

	COMPVARS2 *mpCompVars;
	BITMAPINFOHEADER *mpSrcFormat;
	EncoderHIC*	mhCodec;
	int	mSelect;
	vdblock<char>	mCodecState;
	int mCodecStateId;
	CodecInfo *mpCurrent;

	VDStringW	mCurrentCompression;

	typedef vdfastvector<CodecInfo *> Codecs;
	Codecs mCodecs;

	VDUIProxyListBoxControl	mCodecList;
	VDDelegate mdelSelChanged;
};

VDUIDialogChooseVideoCompressorW32::VDUIDialogChooseVideoCompressorW32(COMPVARS2 *cv, BITMAPINFOHEADER *src)
	: VDDialogFrameW32(IDD_VIDEOCOMPRESSION)
	, mhCodec(NULL)
	, mSelect(-1)
	, mCodecStateId(-1)
	, mpCurrent(NULL)
	, mpCompVars(cv)
	, mpSrcFormat(src)
{
	mCodecList.OnSelectionChanged() += mdelSelChanged.Bind(this, &VDUIDialogChooseVideoCompressorW32::OnCodecSelectionChanged);
	mCapture = false;
}

bool VDUIDialogChooseVideoCompressorW32::OnLoaded() {
	if (!mpSrcFormat) {
		CheckButton(IDC_SHOW_ALL, true);
		EnableControl(IDC_SHOW_ALL, false);
		EnableControl(IDC_SHOW_FILTERED, false);
	} else {
		CheckButton(IDC_SHOW_FILTERED, true);
		EnableControl(IDC_SHOW_ALL, true);
		EnableControl(IDC_SHOW_FILTERED, true);
		int variant;
		int format = VDBitmapFormatToPixmapFormat(*(VDAVIBitmapInfoHeader*)mpSrcFormat, variant);
		VDString s;
		if (format) s = VDPixmapFormatPrintSpec(format);
		else s = print_fourcc(mpSrcFormat->biCompression);
		SetControlTextF(IDC_SHOW_FILTERED, L"Filter by format: %hs and similar", s.c_str());
	}

	AddProxy(&mCodecList, IDC_COMP_LIST);

	EnumerateCodecs();
	EnumeratePluginCodecs();
	std::sort(mCodecs.begin(), mCodecs.end(), CodecSort());

	mCodecStateId = mSelect;
	if (mpCompVars->dwFlags & ICMF_COMPVARS_VALID) {
		if (mpCompVars->driver) {
			int len = mpCompVars->driver->getStateSize();
			if (len > 0) {
				mCodecState.resize(len);
				mpCompVars->driver->getState(mCodecState.data(), len);
			}
		}
	}

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
	EnableControl(IDC_PIXELFORMAT,!mCapture && (g_dubOpts.video.mode > DubVideoOptions::M_FASTREPACK));
	return true;
}

void VDUIDialogChooseVideoCompressorW32::OnDestroy() {
	if (mhCodec) {
		delete mhCodec;
		mhCodec = NULL;
	}

	while(!mCodecs.empty()) {
		CodecInfo *ici = mCodecs.back();
		delete ici;

		mCodecs.pop_back();
	}
}

void VDUIDialogChooseVideoCompressorW32::OnDataExchange(bool write) {
	if (write) {
		if (!(mpCompVars->dwFlags & ICMF_COMPVARS_VALID)) {
			mpCompVars->clear();

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
			mhCodec->sendMessage(ICM_SETQUALITY, (DWORD_PTR)&mpCompVars->lQ, 0);

		delete mpCompVars->driver;
		mpCompVars->driver = mhCodec;
		mhCodec = NULL;
	}
}

bool VDUIDialogChooseVideoCompressorW32::OnCommand(uint32 id, uint32 extcode) {
	switch(id) {
		case IDC_CONFIGURE:
			if (mhCodec) {
				mhCodec->configure(mhdlg);
				mCodecStateId = mSelect;
				mCodecState.clear();
				int len = mhCodec->getStateSize();
				if (len > 0) {
					mCodecState.resize(len);
					mhCodec->getState(mCodecState.data(), len);
				}

				if (mpCurrent)
					UpdateEnables();

				SelectCompressor(mpCurrent);
				UpdateFormat();
			}
			return TRUE;

		case IDC_ABOUT:
			if (mhCodec)
				mhCodec->about(mhdlg);
			return TRUE;

		case IDC_PIXELFORMAT:
			SetVideoDepthOptionsAsk();
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

enum {
	format_ok = 0,
	format_no_compress = 1,
	format_no_convert = 2,
};

int VDUIDialogChooseVideoCompressorW32::testFormat(EncoderHIC* plugin) {
	bool formatSupported = false;
	if (mpSrcFormat) {
		char namebuf[64];
		namebuf[0] = 0;
		vdprotected1("querying video codec \"%.64s\"", const char *, namebuf) {
			if (plugin->compressQuery(mpSrcFormat, NULL)==ICERR_OK)
				formatSupported = true;

			VDPixmapLayout layout;
			VDGetPixmapLayoutForBitmapFormat(*(VDAVIBitmapInfoHeader*)mpSrcFormat,0,layout);
			if (plugin->compressQuery(NULL, NULL, &layout)==ICERR_OK)
				formatSupported = true;

  		int codec_format = plugin->queryInputFormat(0);
      if (codec_format && VDPixmapFormatDifference(layout.format,codec_format)==0) {
        VDPixmapCreateLinearLayout(layout,codec_format,mpSrcFormat->biWidth,mpSrcFormat->biHeight,16);
			  if (plugin->compressQuery(NULL, NULL, &layout)==ICERR_OK)
				  formatSupported = true;
      }
		}
	}

	if (formatSupported) return format_ok;
	return format_no_compress;
}

void VDUIDialogChooseVideoCompressorW32::EnumerateCodecs() {
	vdprotected("enumerating video codecs") {
		ICINFO info = {sizeof(ICINFO)};
		for(int i=0; ICInfo(ICTYPE_VIDEO, i, &info); i++) {
			EncoderHIC plugin;

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
						plugin.hic = ICOpenASV1(info.fccType, info.fccHandler, ICMODE_COMPRESS);
					else	
						plugin.hic = ICOpen(info.fccType, info.fccHandler, ICMODE_COMPRESS);
				}

				if (plugin.hic) {
					ICINFO ici = { sizeof(ICINFO) };
					char namebuf[64];

					namebuf[0] = 0;

				  if (plugin.getInfo(ici))
						VDTextWToA(namebuf, sizeof namebuf, ici.szDescription, -1);

					bool formatSupported = testFormat(&plugin)==format_ok;

					CodecInfo *pii = new CodecInfo;
					static_cast<ICINFO&>(*pii) = ici;
					pii->select = mCodecs.size();
					pii->fccHandler = info.fccHandler;
					pii->mbFormatSupported = formatSupported;
					mCodecs.push_back(pii);

					if (mpCompVars->dwFlags & ICMF_COMPVARS_VALID) {
						if(pii->fccHandler==mpCompVars->fccHandler)
							mSelect = pii->select;
					}

					plugin.close();
				}
			}
		}
	}
}

void VDUIDialogChooseVideoCompressorW32::EnumeratePluginCodecs() {
	std::list<class VDExternalModule *>::const_iterator it(g_pluginModules.begin()),
			itEnd(g_pluginModules.end());

	vdprotected("enumerating video codec plugins") {
		for(; it!=itEnd; ++it) {
			VDExternalModule *pModule = *it;
			const VDStringW& path = pModule->GetFilename();

			int next_fcc = 0;
			while(1){
				EncoderHIC* plugin = EncoderHIC::load(path, ICTYPE_VIDEO, next_fcc, ICMODE_COMPRESS);
				if (!plugin) break;
				ICINFO ici = { sizeof(ICINFO) };
				char namebuf[64];

				namebuf[0] = 0;

				if (plugin->getInfo(ici))
					VDTextWToA(namebuf, sizeof namebuf, ici.szDescription, -1);

				bool formatSupported = testFormat(plugin)==format_ok;

				CodecInfo *pii = new CodecInfo;
				static_cast<ICINFO&>(*pii) = ici;
				pii->select = mCodecs.size();
				pii->fccHandler = ici.fccHandler;
				pii->mbFormatSupported = formatSupported;
				pii->path = path;
				mCodecs.push_back(pii);

				if (mpCompVars->driver) {
					if(pii->fccHandler==mpCompVars->fccHandler && plugin->module==mpCompVars->driver->module)
						mSelect = pii->select;
				}

				next_fcc = plugin->getNext(ici.fccHandler);
				delete plugin;
				if(next_fcc==0||next_fcc==-1) break;
			}
		}
	}
}

void VDUIDialogChooseVideoCompressorW32::RebuildCodecList() {
	const bool showAll = IsButtonChecked(IDC_SHOW_ALL);

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

	mCodecList.Clear();
	mCodecList.AddItem(mCurrentCompression.c_str(), -1);

	bool foundSelected = false;

	size_t n = mCodecs.size();
	for(size_t i=0; i<n; ++i) {
		CodecInfo& ici = *mCodecs[i];

		bool isSelected = ici.select==mSelect;

		if (!isSelected && !ici.mbFormatSupported && !showAll)
			continue;

		int ind = mCodecList.AddItem(ici.szDescription, i);
		if (ind < 0)
			continue;

		if (!foundSelected && isSelected) {
			foundSelected = true;

			mCodecList.SetSelection(ind);

			SelectCompressor(&ici);
			UpdateFormat();
		}
	}

	if (mSelect==-1) {
		mCodecList.SetSelection(0);
		SelectCompressor(NULL);
		UpdateFormat();
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

		if (mhCodec->getInfo(info)) {
			FOURCC fccHandler = mpCurrent->fccHandler;

			static_cast<ICINFO&>(*mpCurrent) = info;
			mpCurrent->fccHandler = fccHandler;
		}

		// Query compressor for caps and enable buttons as appropriate.

		EnableControl(IDC_ABOUT, mhCodec->queryAbout());
		EnableControl(IDC_CONFIGURE, mhCodec->queryConfigure());

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
	48,
	64,
	64,
	12,
	16,
	16,
	16,
	24,
	30,
	32,
	32
};

static int g_depths_fcc[]={
	0, 0, 0,
	VDMAKEFOURCC('b', '4', '8', 'r'),
	VDMAKEFOURCC('B', 'G', 'R', 48),
	VDMAKEFOURCC('b', '6', '4', 'a'),
	VDMAKEFOURCC('B', 'R', 'A', 64),
	VDMAKEFOURCC('Y', 'V', '1', '2'),
	VDMAKEFOURCC('Y', 'V', '1', '6'),
	VDMAKEFOURCC('Y', 'U', 'Y', '2'),
	VDMAKEFOURCC('H', 'D', 'Y', 'C'),
	VDMAKEFOURCC('Y', 'V', '2', '4'),
	VDMAKEFOURCC('v', '2', '1', '0'),
	VDMAKEFOURCC('P', '2', '1', '0'),
	VDMAKEFOURCC('P', '2', '1', '6')
};

static const wchar_t* g_depths_id[]={
  L"rgb16", L"rgb", L"rgba",
  L"b48r",
  L"BGR[48]",
  L"b64a",
  L"BRA[64]",
  L"YV12",
  L"YV16",
  L"YUY2",
  L"HDYC",
  L"YV24",
  L"v210",
  L"P210",
  L"P216"
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

	if (!pii || (!pii->fccHandler && pii->path.empty())) {
		if (mhCodec) {
			delete mhCodec;
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

	if(pii->path.empty())
		SetControlText(IDC_STATIC_DRIVER, VDFileSplitPath(pii->szDriver));
	else
		SetControlText(IDC_STATIC_DRIVER, VDFileSplitPath(pii->path.c_str()));

	// Attempt to open the compressor.

	if (mhCodec) {
		delete mhCodec;
		mhCodec = NULL;
	}

	{
		wchar_t buf[64];
		swprintf(buf, 64, L"A video codec with FOURCC '%.4S'", (const char *)&pii->fccHandler);
		VDExternalCodeBracket bracket(buf, __FILE__, __LINE__);

		if(pii->path.empty())
			mhCodec = EncoderHIC::open(pii->fccType, pii->fccHandler, ICMODE_COMPRESS);
		else {
			mhCodec = EncoderHIC::load(pii->path, pii->fccType, pii->fccHandler, ICMODE_COMPRESS);
		}
	}

	if (!mhCodec) {
		LBAddString(IDC_SIZE_RESTRICTIONS, L"<Unable to open driver>");
		return;
	}

	if (pii->select == mCodecStateId && !mCodecState.empty())
		mhCodec->setState(mCodecState.data(), mCodecState.size());

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

	VDPixmapLayout layout = {0};

	// Loop until we can find a width, height, and depth that works!

	int w;
	int h;
	int kd;

	int codec_format = mhCodec->queryInputFormat(0);
	
	for(int i=0; i<NWIDTHS; i++) {
		w = g_xres[i];
		bi.bmiHeader.biWidth = w;

		for(int j=0; j<NHEIGHTS; j++) {
			h = g_yres[j];
			bi.bmiHeader.biHeight = h;
			VDPixmapCreateLinearLayout(layout,codec_format,bi.bmiHeader.biWidth,bi.bmiHeader.biHeight,16);

			for(int k=0; k<NDEPTHS; k++) {
				set_depth(bi,k);
				kd = k;

				if (ICERR_OK==mhCodec->compressQuery(&bi.bmiHeader, NULL, &layout))
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

		if (ICERR_OK==mhCodec->compressQuery(&bi.bmiHeader, NULL))
			depth_bits |= (1<<k);
	}

	VDStringW s;
	int i, j, k;

	// Look for X alignment

	for(i=3; i>=0; i--) {
		bi.bmiHeader.biWidth	 = w + (1<<i);
		set_depth(bi,kd);
		VDPixmapCreateLinearLayout(layout,codec_format,bi.bmiHeader.biWidth,bi.bmiHeader.biHeight,16);

		if (ICERR_OK!=mhCodec->compressQuery(&bi.bmiHeader, NULL, &layout))
			break;

	}

	bi.bmiHeader.biWidth	 = w + (1<<(i+2));
	set_depth(bi,kd);
	VDPixmapCreateLinearLayout(layout,codec_format,bi.bmiHeader.biWidth,bi.bmiHeader.biHeight,16);

	if (ICERR_OK!=mhCodec->compressQuery(&bi.bmiHeader, NULL, &layout))
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
		VDPixmapCreateLinearLayout(layout,codec_format,bi.bmiHeader.biWidth,bi.bmiHeader.biHeight,16);

		if (ICERR_OK!=mhCodec->compressQuery(&bi.bmiHeader, NULL, &layout))
			break;
	}

	bi.bmiHeader.biHeight	 = h + (1<<(j+2));
	set_depth(bi,kd);
	VDPixmapCreateLinearLayout(layout,codec_format,bi.bmiHeader.biWidth,bi.bmiHeader.biHeight,16);

	if (ICERR_OK!=mhCodec->compressQuery(&bi.bmiHeader, NULL, &layout))
		j = -2;

	if (j>=0) {
		s.sprintf(L"Height must be a multiple of %d", 1<<(j+1));
		LBAddString(IDC_SIZE_RESTRICTIONS, s.c_str());
	} else if (j<-1) {
		s.sprintf(L"Height: unknown (%dx%d worked)", w, h);
		LBAddString(IDC_SIZE_RESTRICTIONS, s.c_str());
	}

	// Print out results
	if (depth_bits) {
		s = L"Valid pixel formats:";

		for(k=0; k<NDEPTHS; k++)
			if (depth_bits & (1<<k))
				s.append_sprintf(L" %s", g_depths_id[k]);

		LBAddString(IDC_SIZE_RESTRICTIONS, s.c_str());
	}

	if (codec_format) {
		LBAddString(IDC_SIZE_RESTRICTIONS, L"Works with VD formats");
	}

	if ((depth_bits & 7)==7 && i<0 && j<0)
		LBAddString(IDC_SIZE_RESTRICTIONS, L"No known restrictions.");
}

void VDUIDialogChooseVideoCompressorW32::OnCodecSelectionChanged(VDUIProxyListBoxControl *sender, int index) {
	if (index < 0)
		return;

	int data = mCodecList.GetItemData(index);

	CodecInfo *pii = data >= 0 ? mCodecs[data] : NULL;

	mSelect = pii ? pii->select : -1;
	SelectCompressor(pii);
	UpdateFormat();
}

void VDUIDialogChooseVideoCompressorW32::UpdateFormat() {
	VDPixmapFormatEx format = 0;
	if (mpSrcFormat) {
		format = VDBitmapFormatToPixmapFormat(*(VDAVIBitmapInfoHeader*)mpSrcFormat);
	} else {
		format = g_dubOpts.video.mOutputFormat;
		if (g_dubOpts.video.mode <= DubVideoOptions::M_FASTREPACK) format = 0;
	}
	if (mhCodec) {
		int codec_format = mhCodec->queryInputFormat(0);
		if (codec_format) format.format = codec_format;
	}

	VDString s;

	if(format==0) {
		if (mCapture) {
			//s += "auto";
		} else if (inputVideo) {
			VDPixmapFormatEx inputFormat = inputVideo->getTargetFormat().format;
			if (g_dubOpts.video.mode <= DubVideoOptions::M_FASTREPACK) inputFormat = inputVideo->getSourceFormat();
			s += "(auto) ";
			s += VDPixmapFormatPrintSpec(inputFormat);
		} else {
			s += "auto";
		}
	} else {
		s += VDPixmapFormatPrintSpec(format);
	}

	if (mhCodec && testFormat(mhCodec)!=format_ok) {
		s = "Incompatible!";
	}

	SetDlgItemText(mhdlg,IDC_ACTIVEFORMAT,s.c_str());
}

void VDUIDialogChooseVideoCompressorW32::SetVideoDepthOptionsAsk() {
	extern bool VDDisplayVideoDepthDialog(VDGUIHandle hParent, DubOptions& opts, bool input, int lockFormat);

	int lockFormat = -1;
	if (mhCodec) {
		int codec_format = mhCodec->queryInputFormat(0);
		if (codec_format) lockFormat = codec_format;
	}

	VDPixmapFormatEx outputFormatOld = g_dubOpts.video.mOutputFormat;
	VDDisplayVideoDepthDialog((VDGUIHandle)mhdlg, g_dubOpts, false, lockFormat);
	bool changed = !outputFormatOld.fullEqual(g_dubOpts.video.mOutputFormat);
	if (changed) {
		SelectCompressor(mpCurrent);
		UpdateFormat();
	}
}

///////////////////////////////////////////////////////////////////////////

void ChooseCompressor(HWND hwndParent, COMPVARS2 *lpCompVars) {
	VDUIDialogChooseVideoCompressorW32 dlg(lpCompVars, 0);

	dlg.ShowDialog((VDGUIHandle)hwndParent);
}

void ChooseCaptureCompressor(HWND hwndParent, COMPVARS2 *lpCompVars, BITMAPINFOHEADER *bihInput) {
	VDUIDialogChooseVideoCompressorW32 dlg(lpCompVars, bihInput);
	dlg.mCapture = true;

	dlg.ShowDialog((VDGUIHandle)hwndParent);
}

