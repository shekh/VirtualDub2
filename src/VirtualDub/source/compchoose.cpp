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
#include <vd2/system/registry.h>
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
#include "filters.h"
#include "optdlg.h"

extern HINSTANCE g_hInst;
extern std::list<class VDExternalModule *>		g_pluginModules;
extern DubOptions			g_dubOpts;
extern VDPixmapFormatEx g_compformat;
extern vdrefptr<IVDVideoSource> inputVideo;

const wchar_t g_szNo[]=L"No";
const wchar_t g_szYes[]=L"Yes";

int vector_find(const vdfastvector<int>& a, int v) {
	for(int i=0; i<a.size(); i++) if(a[i]==v) return i;
	return -1;
}

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
	bool enable_all;
	int filter_format;

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
	void PrintFCC(DWORD code);
	void SelectCompressor(CodecInfo *pii);

	void OnCodecSelectionChanged(VDUIProxyListBoxControl *sender, int index);
	void SetVideoDepthOptionsAsk();
	int testFilterFormat(EncoderHIC* plugin, const char* debug_id);
	int testDIBFormat(EncoderHIC* plugin, BITMAPINFOHEADER* format, const char* debug_id);
	int testVDFormat(EncoderHIC* plugin, int format, const char* debug_id);

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
	int filter_mode;
	bool save_filter_mode;
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
	enable_all = true;
	filter_format = 0;
	filter_mode = 1;
	save_filter_mode = false;
}

bool VDUIDialogChooseVideoCompressorW32::OnLoaded() {
	VDRegistryAppKey key(mCapture ? "Capture\\Persistence" : "Persistence");
	filter_mode = key.getBool("Video codecs: allow all", true);
	save_filter_mode = true;

	if (!filter_format && !mpSrcFormat) {
		filter_mode = 1;
		save_filter_mode = false;
		EnableControl(IDC_SHOW_FILTERED, false);
		EnableControl(IDC_SHOW_ALL, false);
	} else {
		if (!enable_all) {
			filter_mode = 0;
			save_filter_mode = false;
		}

		EnableControl(IDC_SHOW_FILTERED, enable_all);
		EnableControl(IDC_SHOW_ALL, enable_all);
		VDString s;
		if (filter_format) s = VDPixmapFormatPrintSpec(filter_format);
		else s = print_fourcc(mpSrcFormat->biCompression);
		SetControlTextF(IDC_SHOW_FILTERED, L"Similar to source: %hs", s.c_str());
	}

	CheckButton(IDC_SHOW_FILTERED, filter_mode==0);
	CheckButton(IDC_SHOW_ALL, filter_mode==1);

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
	EnableControl(IDC_PIXELFORMAT, (g_dubOpts.video.mode > DubVideoOptions::M_FASTREPACK));
	return true;
}

void VDUIDialogChooseVideoCompressorW32::OnDestroy() {
	if (save_filter_mode) {
		VDRegistryAppKey key(mCapture ? "Capture\\Persistence" : "Persistence");
		key.setBool("Video codecs: allow all", filter_mode==1);
	}

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

		case IDC_SHOW_FILTERED:
			filter_mode = 0;
			RebuildCodecList();
			return true;

		case IDC_SHOW_ALL:
			filter_mode = 1;
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
	format_supported = 1,
	format_compress_ready = 3,
};

int VDUIDialogChooseVideoCompressorW32::testFilterFormat(EncoderHIC* plugin, const char* debug_id) {
	int flags = 0;

	vdprotected1("querying video codec \"%.64s\"", const char *, debug_id) {
		if (mpSrcFormat) {
			// try unknown vfw
			if (plugin->compressQuery(mpSrcFormat, NULL)==ICERR_OK)
				return format_compress_ready;
		}
		if (filter_format) {
			int w = 320;
			int h = 240;

			// try plugin mode
			VDPixmapLayout layout;
			VDPixmapCreateLinearLayout(layout,filter_format,w,h,0);
			if (plugin->compressQuery(NULL, NULL, &layout)==ICERR_OK)
				return format_compress_ready;

			int codec_format = plugin->queryInputFormat(0);
			if (codec_format && VDPixmapFormatDifference(filter_format,codec_format)<=1) {
				// it is already selected but test anyway
				VDPixmapCreateLinearLayout(layout,codec_format,w,h,0);
				if (plugin->compressQuery(NULL, NULL, &layout)==ICERR_OK)
					return format_compress_ready;
			}

			int filter_group = VDPixmapFormatGroup(filter_format);

			{for(int format=1; format<nsVDPixmap::kPixFormat_Max_Standard; format++){
				if (VDPixmapFormatGroup(format)!=filter_group) continue;

				// try known plugin
				VDPixmapCreateLinearLayout(layout,format,0,0,0);
				int r = plugin->inputFormatInfo(&layout);
				if (r!=-1 && r!=0) flags |= format_supported;

				// try known vfw
				int n = VDGetPixmapToBitmapVariants(format);
				for(int variant=0; variant<n; variant++){
					vdstructex<VDAVIBitmapInfoHeader> bm;
					if (VDMakeBitmapFormatFromPixmapFormat(bm,format,variant,w,h)) {
						if (plugin->compressQuery(bm.data(), NULL)==ICERR_OK)
							return format_compress_ready;
					}
				}
			}}
		}
	}

	return flags;
}

int VDUIDialogChooseVideoCompressorW32::testDIBFormat(EncoderHIC* plugin, BITMAPINFOHEADER* format, const char* debug_id) {
	vdprotected1("querying video codec \"%.64s\"", const char *, debug_id) {
		if (format) {
			if (plugin->compressQuery(format, NULL)==ICERR_OK)
				return format_compress_ready;
		}
	}

	return 0;
}

int VDUIDialogChooseVideoCompressorW32::testVDFormat(EncoderHIC* plugin, int format, const char* debug_id) {
	vdprotected1("querying video codec \"%.64s\"", const char *, debug_id) {
		int w = 320;
		int h = 240;

		// try plugin mode
		VDPixmapLayout layout;
		VDPixmapCreateLinearLayout(layout,format,w,h,0);
		if (plugin->compressQuery(NULL, NULL, &layout)==ICERR_OK)
			return format_compress_ready;

		int codec_format = plugin->queryInputFormat(0);
		if (codec_format && VDPixmapFormatDifference(format,codec_format)<=1) {
			// it is already selected but test anyway
			VDPixmapCreateLinearLayout(layout,codec_format,w,h,0);
			if (plugin->compressQuery(NULL, NULL, &layout)==ICERR_OK)
				return format_compress_ready;
		}

		// try known vfw
		int n = VDGetPixmapToBitmapVariants(format);
		for(int variant=0; variant<n; variant++){
			vdstructex<VDAVIBitmapInfoHeader> bm;
			if (VDMakeBitmapFormatFromPixmapFormat(bm,format,variant,w,h)) {
				if (plugin->compressQuery(bm.data(), NULL)==ICERR_OK)
					return format_compress_ready;
			}
		}
	}

	return 0;
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

					bool formatSupported = testFilterFormat(&plugin,namebuf)!=0;

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

				bool formatSupported = testFilterFormat(plugin,namebuf)!=0;

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
	const bool showAll = filter_mode==1;

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

			FOURCC rh = mhCodec->getHandler();
			if (rh!=-1) PrintFCC(rh);
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
	32, 32,
	48,	48,
	64,	64,
	12,
	16,	16,	16,
	24, 24,
	24, 24,
	30,
	32,	32,
	32,	32,
	64
};

static int g_depths_fcc[]={
	0, 0, 0,
	VDMAKEFOURCC('r', '2', '1', '0'),	VDMAKEFOURCC('R', '1', '0', 'k'),
	VDMAKEFOURCC('b', '4', '8', 'r'),	VDMAKEFOURCC('B', 'G', 'R', 48),
	VDMAKEFOURCC('b', '6', '4', 'a'),	VDMAKEFOURCC('B', 'R', 'A', 64),
	VDMAKEFOURCC('Y', 'V', '1', '2'),
	VDMAKEFOURCC('Y', 'V', '1', '6'),	VDMAKEFOURCC('Y', 'U', 'Y', '2'),	VDMAKEFOURCC('H', 'D', 'Y', 'C'),
	VDMAKEFOURCC('Y', 'V', '2', '4'),	VDMAKEFOURCC('v', '3', '0', '8'),
	VDMAKEFOURCC('P', '0', '1', '0'),	VDMAKEFOURCC('P', '0', '1', '6'),
	VDMAKEFOURCC('v', '2', '1', '0'),
	VDMAKEFOURCC('P', '2', '1', '0'),	VDMAKEFOURCC('P', '2', '1', '6'),
	VDMAKEFOURCC('v', '4', '1', '0'),	VDMAKEFOURCC('Y', '4', '1', '0'),
	VDMAKEFOURCC('Y', '4', '1', '6')
};

static const wchar_t* g_depths_id[]={
	L"rgb16", L"rgb", L"rgba",
	L"r210",  L"R10k",
	L"b48r",  L"BGR[48]",
	L"b64a",  L"BRA[64]",
	L"YV12",
	L"YV16",  L"YUY2",  L"HDYC",
	L"YV24",  L"v308",
	L"P010",  L"P016",
	L"v210",
	L"P210",  L"P216",
	L"v410",  L"Y410",
	L"Y416"
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

void VDUIDialogChooseVideoCompressorW32::PrintFCC(DWORD code) {
	// Show driver fourCC code.

	wchar_t fccbuf[7];
	for(int i=0; i<4; ++i) {
		char c = ((char *)&code)[i];

		if (isprint((unsigned char)c))
			fccbuf[i+1] = c;
		else
			fccbuf[i+1] = ' ';
	}

	fccbuf[0] = fccbuf[5] = '\'';
	fccbuf[6] = 0;
	SetControlText(IDC_STATIC_FOURCC, fccbuf);
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

	PrintFCC(pii->fccHandler);

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
	if (mCapture) {
		format = g_compformat;
		if (format==0 || !mhCodec) format = filter_format;
	} else {
		format = g_dubOpts.video.mOutputFormat;
		if (g_dubOpts.video.mode <= DubVideoOptions::M_FASTREPACK) format = 0;
	}
	if (mhCodec) {
		int codec_format = mhCodec->queryInputFormat(0);
		if (codec_format) format.format = codec_format;
	}
	if (mpSrcFormat && !VDBitmapFormatToPixmapFormat((VDAVIBitmapInfoHeader&)*mpSrcFormat)) {
		// source format is unknown so conversion is not possible
		format = 0;
	}

	VDString s;

	if(format==0) {
		if (mCapture) {
			s += "as capture";
		} else if (inputVideo) {
			VDPixmapFormatEx inputFormat = inputVideo->getTargetFormat();
			if (g_dubOpts.video.mode <= DubVideoOptions::M_FASTREPACK) inputFormat = inputVideo->getSourceFormat();
			format = inputFormat;
			s += VDPixmapFormatPrintSpec(format);
		} else {
			s += "as decoding";
		}
	} else {
		s += VDPixmapFormatPrintSpec(format);
	}

	VDStringW msg;
	format = VDPixmapFormatCombine(format);
	if (mhCodec && format) {
		int test = testVDFormat(mhCodec,format,"selected");
		if((test & format_compress_ready)!=format_compress_ready) {
			msg = VDStringW(L"(?) Format not accepted by codec, YMMV");
			// codec is vfw and format decision is too convolved to show precise answer
		} else {
			VDPixmapFormatEx src;
			if (mCapture) {
				src = filter_format;
			} else if (inputVideo) {
				src = inputVideo->getTargetFormat().format;
				if (g_dubOpts.video.mode <= DubVideoOptions::M_FASTREPACK) src = inputVideo->getSourceFormat();
				if (g_dubOpts.video.mode == DubVideoOptions::M_FULL) src = filters.GetOutputLayout().formatEx;
			}
			src = VDPixmapFormatCombine(src);
			if (src) {
				if (src.fullEqual(format)){
					msg = VDStringW(L"No conversion required on output");
				} else {
					VDString conv = VDPixmapFormatPrintSpec(src) + " -> " + VDPixmapFormatPrintSpec(format);
					msg = VDStringW(L"Using conversion: ") + VDTextAToW(conv);
				}
			}
		}
	}
	if (mhCodec && !format && mpSrcFormat) {
		int test = testDIBFormat(mhCodec,mpSrcFormat,"selected");
		if((test & format_compress_ready)!=format_compress_ready) {
			msg = VDStringW(L"(!) Format not accepted by codec");
		} else {
			msg = VDStringW(L"Using DIB format");
		}
	}

	SetDlgItemText(mhdlg,IDC_ACTIVEFORMAT,s.c_str());
	SetDlgItemTextW(mhdlg,IDC_FORMAT_INFO,msg.c_str());
}

void VDUIDialogChooseVideoCompressorW32::SetVideoDepthOptionsAsk() {
	int lockFormat = -1;
	if (mhCodec) {
		int codec_format = mhCodec->queryInputFormat(0);
		if (codec_format) lockFormat = codec_format;
	} else {
		if (mCapture) lockFormat = 0;
	}

	VDPixmapFormatEx* target = mCapture ? &g_compformat : &g_dubOpts.video.mOutputFormat;
	int type = mCapture ? DepthDialog_cap_output : DepthDialog_output;
	VDPixmapFormatEx outputFormatOld = *target;
	VDDisplayVideoDepthDialog((VDGUIHandle)mhdlg, *target, type, lockFormat);
	bool changed = !outputFormatOld.fullEqual(*target);
	if (changed) {
		SelectCompressor(mpCurrent);
		UpdateFormat();
	}
}

///////////////////////////////////////////////////////////////////////////

void ChooseCompressor(HWND hwndParent, COMPVARS2 *lpCompVars) {
	VDUIDialogChooseVideoCompressorW32 dlg(lpCompVars, 0);
	dlg.filter_format = nsVDPixmap::kPixFormat_RGB888;
	if (inputVideo)
		dlg.filter_format = inputVideo->getSourceFormat();

	dlg.ShowDialog((VDGUIHandle)hwndParent);
}

void ChooseCaptureCompressor(HWND hwndParent, COMPVARS2 *lpCompVars, BITMAPINFOHEADER *bihInput) {
	VDUIDialogChooseVideoCompressorW32 dlg(lpCompVars, bihInput);
	dlg.mCapture = true;
	dlg.filter_format = nsVDPixmap::kPixFormat_RGB888;
	if (bihInput) {
		int variant;
		dlg.filter_format = VDBitmapFormatToPixmapFormat(*(VDAVIBitmapInfoHeader*)bihInput, variant);
		if (!dlg.filter_format) dlg.enable_all = false;
	}

	dlg.ShowDialog((VDGUIHandle)hwndParent);
}

