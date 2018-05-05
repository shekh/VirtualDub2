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
#include <vd2/system/registry.h>
#include <vd2/system/math.h>
#include <vd2/system/w32assist.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/VDLib/Dialog.h>

#include <list>
#include <utility>

#include "optdlg.h"

#include "resource.h"
#include "helpfile.h"
#include "oshelper.h"
#include "misc.h"
#include "gui.h"
#include "filters.h"

#include "AudioSource.h"
#include "VideoSource.h"
#include "Dub.h"
#include "project.h"
#include "command.h"

extern HINSTANCE g_hInst;
extern vdrefptr<IVDVideoSource> inputVideo;
extern VDProject *g_project;
int VDRenderSetVideoSourceInputFormat(IVDVideoSource *vsrc, VDPixmapFormatEx format);

#define VD_FOURCC(fcc) (((fcc&0xff000000)>>24)+((fcc&0xff0000)>>8)+((fcc&0xff00)<<8)+((fcc&0xff)<<24))

uint32& VDPreferencesGetRenderOutputBufferSize();
uint32& VDPreferencesGetRenderWaveBufferSize();
uint32& VDPreferencesGetRenderVideoBufferCount();
uint32& VDPreferencesGetRenderAudioBufferSeconds();
void VDSavePreferences();

///////////////////////////////////////////

void ActivateDubDialog(HINSTANCE hInst, LPCTSTR lpResource, HWND hDlg, DLGPROC dlgProc) {
	DubOptions duh;

	duh = g_dubOpts;
	if (DialogBoxParam(hInst, lpResource, hDlg, dlgProc, (LPARAM)&duh))
		g_dubOpts = duh;
}

///////////////////////////////////////////

class VDDialogAudioConversionW32 : public VDDialogBaseW32 {
public:
	inline VDDialogAudioConversionW32(DubOptions& opts, AudioSource *pSource);

	inline bool Activate(VDGUIHandle hParent) { return 0!=ActivateDialog(hParent); }

protected:
	INT_PTR DlgProc(UINT message, WPARAM wParam, LPARAM lParam);
	void ReinitDialog();
	void RecomputeBandwidth();

	DubOptions& mOpts;
	AudioSource *const mpSource;
	bool mbSource16Bit;
	bool mbSourceFloat;
	bool mbSourcePrecisionKnown;
	int sourceChannels;
};

VDDialogAudioConversionW32::VDDialogAudioConversionW32(DubOptions& opts, AudioSource *pSource)
	: VDDialogBaseW32(IDD_AUDIO_CONVERSION)
	, mOpts(opts)
	, mpSource(pSource)
	, mbSource16Bit(true)
	, mbSourceFloat(false)
	, mbSourcePrecisionKnown(false)
	, sourceChannels(0)
{
	if (pSource) {
		const VDWaveFormat *fmt = pSource->getWaveFormat();
		const VDWaveFormat *sfmt = pSource->getSourceWaveFormat();
		if (sfmt) fmt = sfmt;

		sourceChannels = fmt->mChannels;

		if (is_audio_pcm(fmt)) {
			mbSource16Bit = (fmt->mSampleBits==16);
			mbSourcePrecisionKnown = fmt->mSampleBits<=16;
		}
		if (is_audio_float(fmt)) {
			mbSourceFloat = true;
			mbSourcePrecisionKnown = true;
		}
	}
}

void VDDialogAudioConversionW32::RecomputeBandwidth() {
	long bps=0;

	if (	 IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_NOCHANGE))	bps = mpSource ? mpSource->getWaveFormat()->mSamplingRate : 0;
	else if (IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_11KHZ))		bps = 11025;
	else if (IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_22KHZ))		bps = 22050;
	else if (IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_44KHZ))		bps = 44100;
	else if (IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_8KHZ))		bps = 8000;
	else if (IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_16KHZ))		bps = 16000;
	else if (IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_48KHZ))		bps = 48000;
	else if (IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_CUSTOM))
		bps = GetDlgItemInt(mhdlg, IDC_SAMPLINGRATE_CUSTOM_VAL, NULL, FALSE);

	// prevent UI overflows (this big of a value won't pass validation anyway)
	if (bps >= 0x0FFFFFFF)
		bps = 0;

	if (IsDlgButtonChecked(mhdlg, IDC_PRECISION_NOCHANGE)) {
		if (mbSourcePrecisionKnown && mbSourceFloat)
			bps *= 4;
		else if (mbSourcePrecisionKnown && mbSource16Bit)
			bps *= 2;
		else
			bps = 0;
	} if (IsDlgButtonChecked(mhdlg, IDC_PRECISION_16BIT))
		bps *= 2;

	if (IsDlgButtonChecked(mhdlg, IDC_CHANNELS_NOCHANGE)) {
		bps *= sourceChannels;
	} else if (IsDlgButtonChecked(mhdlg, IDC_CHANNELS_STEREO)) {
		bps *= 2;
	}

	char buf[128];
	if (bps)
		wsprintf(buf, "Bandwidth required: %ldKB/s", (bps+1023)>>10);
	else
		strcpy(buf,"Bandwidth required: (unknown)");

	SetDlgItemText(mhdlg, IDC_BANDWIDTH_REQD, buf);
}

INT_PTR VDDialogAudioConversionW32::DlgProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
			ReinitDialog();
            return TRUE;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_SAMPLINGRATE_NOCHANGE:
			case IDC_SAMPLINGRATE_8KHZ:
			case IDC_SAMPLINGRATE_11KHZ:
			case IDC_SAMPLINGRATE_16KHZ:
			case IDC_SAMPLINGRATE_22KHZ:
			case IDC_SAMPLINGRATE_44KHZ:
			case IDC_SAMPLINGRATE_48KHZ:
				if (!IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_CUSTOM))
					EnableWindow(GetDlgItem(mhdlg, IDC_SAMPLINGRATE_CUSTOM_VAL), FALSE);
			case IDC_PRECISION_NOCHANGE:
			case IDC_PRECISION_8BIT:
			case IDC_PRECISION_16BIT:
			case IDC_CHANNELS_NOCHANGE:
			case IDC_CHANNELS_MONO:
			case IDC_CHANNELS_STEREO:
			case IDC_CHANNELS_LEFT:
			case IDC_CHANNELS_RIGHT:
			case IDC_SAMPLINGRATE_CUSTOM_VAL:
				RecomputeBandwidth();
				break;

			case IDC_SAMPLINGRATE_CUSTOM:
				EnableWindow(GetDlgItem(mhdlg, IDC_SAMPLINGRATE_CUSTOM_VAL), TRUE);
				RecomputeBandwidth();
				break;

			case IDOK:
				if      (IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_NOCHANGE)) mOpts.audio.new_rate = 0;
				else if (IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_8KHZ   )) mOpts.audio.new_rate = 8000;
				else if (IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_11KHZ   )) mOpts.audio.new_rate = 11025;
				else if (IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_16KHZ   )) mOpts.audio.new_rate = 16000;
				else if (IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_22KHZ   )) mOpts.audio.new_rate = 22050;
				else if (IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_44KHZ   )) mOpts.audio.new_rate = 44100;
				else if (IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_48KHZ   )) mOpts.audio.new_rate = 48000;
				else if (IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_CUSTOM)) {
					BOOL valid = FALSE;
					UINT hz = GetDlgItemInt(mhdlg, IDC_SAMPLINGRATE_CUSTOM_VAL, &valid, FALSE);
					if (!valid || !hz || hz > 10000000) {
						SetFocus(GetDlgItem(mhdlg, IDC_SAMPLINGRATE_CUSTOM_VAL));
						MessageBeep(MB_ICONEXCLAMATION);
						return TRUE;
					}

					mOpts.audio.new_rate = hz;
				}

				if		(IsDlgButtonChecked(mhdlg, IDC_PRECISION_NOCHANGE)) mOpts.audio.newPrecision = DubAudioOptions::P_NOCHANGE;
				else if	(IsDlgButtonChecked(mhdlg, IDC_PRECISION_8BIT    )) mOpts.audio.newPrecision = DubAudioOptions::P_8BIT;
				else if	(IsDlgButtonChecked(mhdlg, IDC_PRECISION_16BIT   )) mOpts.audio.newPrecision = DubAudioOptions::P_16BIT;

				if		(IsDlgButtonChecked(mhdlg, IDC_CHANNELS_NOCHANGE)) mOpts.audio.newChannels = DubAudioOptions::C_NOCHANGE;
				else if	(IsDlgButtonChecked(mhdlg, IDC_CHANNELS_MONO    )) mOpts.audio.newChannels = DubAudioOptions::C_MONO;
				else if	(IsDlgButtonChecked(mhdlg, IDC_CHANNELS_STEREO  )) mOpts.audio.newChannels = DubAudioOptions::C_STEREO;
				else if	(IsDlgButtonChecked(mhdlg, IDC_CHANNELS_LEFT    )) mOpts.audio.newChannels = DubAudioOptions::C_MONOLEFT;
				else if	(IsDlgButtonChecked(mhdlg, IDC_CHANNELS_RIGHT   )) mOpts.audio.newChannels = DubAudioOptions::C_MONORIGHT;

				mOpts.audio.fHighQuality = !!IsDlgButtonChecked(mhdlg, IDC_SAMPLINGRATE_HQ);

				End(true);
				return TRUE;
			case IDCANCEL:
				End(false);
				return TRUE;
			}
            break;

		case WM_HELP:
			{
				HELPINFO *lphi = (HELPINFO *)lParam;

				if (lphi->iContextType == HELPINFO_WINDOW)
					VDShowHelp(mhdlg, L"d-audioconversion.html");
			}
			return TRUE;
    }
    return FALSE;
}

void VDDialogAudioConversionW32::ReinitDialog() {
	if (mpSource) {
		char buf[128];

		const VDWaveFormat *pwfex = mpSource->getWaveFormat();
		wsprintf(buf, "No change (%ldHz)", pwfex->mSamplingRate);
		SetDlgItemText(mhdlg, IDC_SAMPLINGRATE_NOCHANGE, buf);

		if (!mbSourcePrecisionKnown)
			strcpy(buf, "No change");
		else if (mbSourceFloat)
			strcpy(buf, "No change (float)");
		else
			wsprintf(buf, "No change (%ld-bit)", mbSource16Bit ? 16 : 8);
		SetDlgItemText(mhdlg, IDC_PRECISION_NOCHANGE, buf);

		if (sourceChannels > 2)
			wsprintf(buf, "No change (%dch.)", sourceChannels);
		else
			wsprintf(buf, "No change (%s)", sourceChannels>1 ? "stereo" : "mono");
		SetDlgItemText(mhdlg, IDC_CHANNELS_NOCHANGE, buf);
	}

	switch(mOpts.audio.new_rate) {
	case 0:		CheckDlgButton(mhdlg, IDC_SAMPLINGRATE_NOCHANGE, TRUE); break;
	case 8000:	CheckDlgButton(mhdlg, IDC_SAMPLINGRATE_8KHZ, TRUE);	break;
	case 11025:	CheckDlgButton(mhdlg, IDC_SAMPLINGRATE_11KHZ, TRUE);	break;
	case 16000:	CheckDlgButton(mhdlg, IDC_SAMPLINGRATE_16KHZ, TRUE);	break;
	case 22050:	CheckDlgButton(mhdlg, IDC_SAMPLINGRATE_22KHZ, TRUE);	break;
	case 44100:	CheckDlgButton(mhdlg, IDC_SAMPLINGRATE_44KHZ, TRUE);	break;
	case 48000:	CheckDlgButton(mhdlg, IDC_SAMPLINGRATE_48KHZ, TRUE);	break;
	default:
		CheckDlgButton(mhdlg, IDC_SAMPLINGRATE_CUSTOM, TRUE);
		EnableWindow(GetDlgItem(mhdlg, IDC_SAMPLINGRATE_CUSTOM_VAL), TRUE);
		SetDlgItemInt(mhdlg, IDC_SAMPLINGRATE_CUSTOM_VAL, mOpts.audio.new_rate, FALSE);
		break;
	}
	CheckDlgButton(mhdlg, IDC_SAMPLINGRATE_HQ, !!mOpts.audio.fHighQuality);
	CheckDlgButton(mhdlg, IDC_PRECISION_NOCHANGE+mOpts.audio.newPrecision,TRUE);
	CheckDlgButton(mhdlg, IDC_CHANNELS_NOCHANGE+mOpts.audio.newChannels,TRUE);

	RecomputeBandwidth();
}

bool VDDisplayAudioConversionDialog(VDGUIHandle hParent, DubOptions& opts, AudioSource *pSource) {
	VDDialogAudioConversionW32 dlg(opts, pSource);

	return dlg.Activate(hParent);
}

///////////////////////////////////////////

void AudioInterleaveDlgEnableStuff(HWND hDlg, BOOL en) {
	EnableWindow(GetDlgItem(hDlg, IDC_PRELOAD), en);
	EnableWindow(GetDlgItem(hDlg, IDC_INTERVAL), en);
	EnableWindow(GetDlgItem(hDlg, IDC_FRAMES), en);
	EnableWindow(GetDlgItem(hDlg, IDC_MS), en);
}

INT_PTR CALLBACK AudioInterleaveDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	DubOptions *dopt = (DubOptions *)GetWindowLongPtr(hDlg, DWLP_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			SetWindowLongPtr(hDlg, DWLP_USER, lParam);
			dopt = (DubOptions *)lParam;

			CheckDlgButton(hDlg, IDC_INTERLEAVE, dopt->audio.enabled);
			AudioInterleaveDlgEnableStuff(hDlg, dopt->audio.enabled);
//			if (dopt->audio.enabled) {
				SetDlgItemInt(hDlg, IDC_PRELOAD, dopt->audio.preload, FALSE);
				SetDlgItemInt(hDlg, IDC_INTERVAL, dopt->audio.interval, FALSE);
				CheckDlgButton(hDlg, IDC_FRAMES, !dopt->audio.is_ms);
				CheckDlgButton(hDlg, IDC_MS, dopt->audio.is_ms);
//			}
			SetDlgItemInt(hDlg, IDC_DISPLACEMENT, dopt->audio.offset, TRUE);
            return (TRUE);

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_INTERLEAVE:
				AudioInterleaveDlgEnableStuff(hDlg, IsDlgButtonChecked(hDlg, IDC_INTERLEAVE));
				break;

			case IDOK:
				dopt->audio.enabled = !!IsDlgButtonChecked(hDlg, IDC_INTERLEAVE);

				if (dopt->audio.enabled) {
					dopt->audio.preload = GetDlgItemInt(hDlg, IDC_PRELOAD, NULL, TRUE);
					if (dopt->audio.preload<0 || dopt->audio.preload>60000) {
						SetFocus(GetDlgItem(hDlg, IDC_PRELOAD));
						MessageBeep(MB_ICONQUESTION);
						break;
					}

					dopt->audio.interval = GetDlgItemInt(hDlg, IDC_INTERVAL, NULL, TRUE);
					if (dopt->audio.interval<=0 || dopt->audio.interval>3600000) {
						SetFocus(GetDlgItem(hDlg, IDC_INTERVAL));
						MessageBeep(MB_ICONQUESTION);
						break;
					}

					dopt->audio.is_ms = !!IsDlgButtonChecked(hDlg, IDC_MS);
				}

				dopt->audio.offset = GetDlgItemInt(hDlg, IDC_DISPLACEMENT, NULL, TRUE);

				EndDialog(hDlg, TRUE);
				return TRUE;
			case IDCANCEL:
				EndDialog(hDlg, FALSE);
				return TRUE;
			}
            break;
    }
    return FALSE;
}

/////////////////////////////////

class VDDialogSelectVideoFormatW32 : public VDDialogFrameW32 {
public:
	VDDialogSelectVideoFormatW32(int format);

	int GetSelectedFormat() const { return mFormat; }

protected:
	bool OnLoaded();
	void RebuildList();
	void OnDataExchange(bool write);
	void OnSize();
	bool OnErase(VDZHDC hdc);
	void OnDestroy() {
		VDUISaveWindowPlacementW32(mhdlg, "FormatList");
	}
	void OnColumnClicked(VDUIProxyListView *source, int column);

	VDUIProxyListView mListView;
	VDDelegate mDelegateColumnClicked;
	int mFormat;

	class FormatItem : public IVDUIListViewVirtualItem {
	public:
		int AddRef() { return 2; }
		int Release() { return 1; }
		void Init(int format);
		void InitText(int subItem, VDStringW& s) const;
		void GetText(int subItem, VDStringW& s) const;

		int mFormat;
		int depth;
		VDStringW space;
		VDStringW range;
		VDStringW enc;
		VDStringW chroma;
	};

	struct FormatItemSort {
		int col;
		int dir;
		bool operator()(const FormatItem& x, const FormatItem& y) const;
	};

	typedef vdvector<FormatItem> FormatItems;
	FormatItems mFormatItems;

	VDDialogResizerW32 mResizer;
};

bool VDDialogSelectVideoFormatW32::FormatItemSort::operator()(const FormatItem& x, const FormatItem& y) const {
	if (col==2) {
		if (dir==1)
			return x.depth < y.depth;
		else
			return x.depth > y.depth;
	}

	VDStringW s;
	VDStringW t;

	x.GetText(col, s);
	y.GetText(col, t);

	int r = wcscmp(s.c_str(), t.c_str());
	if (dir==1)
		return r < 0;
	else
		return r > 0;
}

VDDialogSelectVideoFormatW32::VDDialogSelectVideoFormatW32(int format)
	: VDDialogFrameW32(IDD_SELECT_VIDEO_FORMAT)
	, mFormat(format)
{
	mListView.OnColumnClicked() += mDelegateColumnClicked.Bind(this, &VDDialogSelectVideoFormatW32::OnColumnClicked);
}

void VDDialogSelectVideoFormatW32::OnColumnClicked(VDUIProxyListView *source, int column) {
	int x = mListView.GetSortIcon(column);
	mListView.SetSortIcon(column, x==1 ? 2:1);
	OnDataExchange(true);
	RebuildList();
}

bool VDDialogSelectVideoFormatW32::OnLoaded() {
	SetCurrentSizeAsMinSize();

	VDSetDialogDefaultIcons(mhdlg);
	mResizer.Init(mhdlg);
	mResizer.Add(IDC_FORMATS, VDDialogResizerW32::kMC | VDDialogResizerW32::kAvoidFlicker);
	mResizer.Add(IDOK, VDDialogResizerW32::kBR);
	mResizer.Add(IDCANCEL, VDDialogResizerW32::kBR);

	AddProxy(&mListView, IDC_FORMATS);

	mListView.SetFullRowSelectEnabled(true);
	mListView.InsertColumn(0, L"Color space", 100);
	mListView.InsertColumn(1, L"Range", 100);
	mListView.InsertColumn(2, L"Precision", 100);
	mListView.InsertColumn(3, L"Encoding", 100);
	mListView.InsertColumn(4, L"Chroma position", 100);

	RebuildList();

	mListView.AutoSizeColumns();
	SendMessage(mListView.GetHandle(), LVM_SETCOLUMNWIDTH, 4, LVSCW_AUTOSIZE_USEHEADER);

	SetFocusToControl(IDC_FORMATS);
	VDUIRestoreWindowPlacementW32(mhdlg, "FormatList", SW_SHOW);
	return true;
}

void VDDialogSelectVideoFormatW32::OnDataExchange(bool write) {
	if (write) {
		int selIdx = mListView.GetSelectedIndex();

		if (selIdx >= 0)
			mFormat = mFormatItems[selIdx].mFormat;
	}
}

void VDDialogSelectVideoFormatW32::OnSize() {
	mResizer.Relayout();
	SendMessage(mListView.GetHandle(), LVM_SETCOLUMNWIDTH, 4, LVSCW_AUTOSIZE_USEHEADER);
}

bool VDDialogSelectVideoFormatW32::OnErase(VDZHDC hdc) {
	mResizer.Erase(&hdc);
	return true;
}

void VDDialogSelectVideoFormatW32::RebuildList() {
	static const int kFormats[]={
		nsVDPixmap::kPixFormat_XRGB1555,
		nsVDPixmap::kPixFormat_RGB565,
		nsVDPixmap::kPixFormat_RGB888,
		nsVDPixmap::kPixFormat_XRGB8888,
		nsVDPixmap::kPixFormat_XRGB64,
		nsVDPixmap::kPixFormat_B64A,
		nsVDPixmap::kPixFormat_R210,
		nsVDPixmap::kPixFormat_R10K,
		nsVDPixmap::kPixFormat_Y8,
		nsVDPixmap::kPixFormat_Y8_FR,
		nsVDPixmap::kPixFormat_YUV422_UYVY,
		nsVDPixmap::kPixFormat_YUV422_YUYV,
		nsVDPixmap::kPixFormat_YUV444_Planar,
		nsVDPixmap::kPixFormat_YUV422_Planar,
		nsVDPixmap::kPixFormat_YUV420_Planar,
		nsVDPixmap::kPixFormat_YUV411_Planar,
		nsVDPixmap::kPixFormat_YUV410_Planar,
		nsVDPixmap::kPixFormat_YUV444_V308,
		nsVDPixmap::kPixFormat_YUV420_NV12,
		nsVDPixmap::kPixFormat_YUV444_Planar16,
		nsVDPixmap::kPixFormat_YUV422_Planar16,
		nsVDPixmap::kPixFormat_YUV420_Planar16,
		nsVDPixmap::kPixFormat_YUV422_V210,
		nsVDPixmap::kPixFormat_YUV444_V410,
		nsVDPixmap::kPixFormat_YUV444_Y410,
		nsVDPixmap::kPixFormat_YUV422_P210,
		nsVDPixmap::kPixFormat_YUV420_P010,
		nsVDPixmap::kPixFormat_YUV422_P216,
		nsVDPixmap::kPixFormat_YUV420_P016,
		nsVDPixmap::kPixFormat_YUVA444_Y416,
		nsVDPixmap::kPixFormat_YUV444_Alpha_Planar16,
		nsVDPixmap::kPixFormat_YUV422_Alpha_Planar16,
		nsVDPixmap::kPixFormat_YUV420_Alpha_Planar16,
		nsVDPixmap::kPixFormat_YUV444_Alpha_Planar,
		nsVDPixmap::kPixFormat_YUV422_Alpha_Planar,
		nsVDPixmap::kPixFormat_YUV420_Alpha_Planar,
		nsVDPixmap::kPixFormat_YUV422_Planar_Centered,
		nsVDPixmap::kPixFormat_YUV420_Planar_Centered,
		nsVDPixmap::kPixFormat_YUV420i_Planar,
		nsVDPixmap::kPixFormat_YUV420it_Planar,
		nsVDPixmap::kPixFormat_YUV420ib_Planar,
	};

	mFormatItems.resize(sizeof(kFormats)/sizeof(kFormats[0]));
	for(uint32 i=0; i<sizeof(kFormats)/sizeof(kFormats[0]); ++i) {
		const int format = kFormats[i];

		mFormatItems[i].Init(format);
	}

	FormatItemSort sort;
	sort.col = -1;
	{for(int i=0; i<=4; i++){
		int x = mListView.GetSortIcon(i);
		if(x){
			sort.col = i;
			sort.dir = x;
		}
	}}
	if(sort.col!=-1) std::sort(mFormatItems.begin(), mFormatItems.end(), sort);

	mListView.Clear();
	int selIdx = 0;
	for(uint32 i=0; i<sizeof(kFormats)/sizeof(kFormats[0]); ++i) {
		mListView.InsertVirtualItem(i, &mFormatItems[i]);

		if (mFormatItems[i].mFormat == mFormat)
			selIdx = i;
	}

	mListView.SetSelectedIndex(selIdx);
	mListView.EnsureItemVisible(selIdx);
}

void VDDialogSelectVideoFormatW32::FormatItem::GetText(int subItem, VDStringW& s) const {
	switch(subItem) {
	case 0:
		s = space;
		break;
	case 1:
		s = range;
		break;
	case 2:
		s.sprintf(L"%d-bit",depth);
		break;
	case 3:
		s = enc;
		break;
	case 4:
		s = chroma;
		break;
	}
}

void VDDialogSelectVideoFormatW32::FormatItem::Init(int format) {
	mFormat = format;
	VDASSERTCT(nsVDPixmap::kPixFormat_Max_Standard == nsVDPixmap::kPixFormat_B64A + 1);
	InitText(0,space);
	InitText(1,range);
	InitText(3,enc);
	InitText(4,chroma);
	switch(mFormat) {
	case nsVDPixmap::kPixFormat_XRGB1555:
	case nsVDPixmap::kPixFormat_RGB565:
		depth = 5;
		break;
	case nsVDPixmap::kPixFormat_XRGB64:
	case nsVDPixmap::kPixFormat_B64A:
	case nsVDPixmap::kPixFormat_YUV444_Planar16:
	case nsVDPixmap::kPixFormat_YUV444_Alpha_Planar16:
	case nsVDPixmap::kPixFormat_YUV422_Planar16:
	case nsVDPixmap::kPixFormat_YUV422_Alpha_Planar16:
	case nsVDPixmap::kPixFormat_YUV420_Planar16:
	case nsVDPixmap::kPixFormat_YUV420_Alpha_Planar16:
	case nsVDPixmap::kPixFormat_YUV422_P216:
	case nsVDPixmap::kPixFormat_YUV420_P016:
	case nsVDPixmap::kPixFormat_YUVA444_Y416:
		depth = 16;
		break;
	case nsVDPixmap::kPixFormat_R210:
	case nsVDPixmap::kPixFormat_R10K:
	case nsVDPixmap::kPixFormat_YUV422_V210:
	case nsVDPixmap::kPixFormat_YUV444_V410:
	case nsVDPixmap::kPixFormat_YUV444_Y410:
	case nsVDPixmap::kPixFormat_YUV422_P210:
	case nsVDPixmap::kPixFormat_YUV420_P010:
		depth = 10;
		break;
	default:
		depth = 8;
		break;
	}
}

void VDDialogSelectVideoFormatW32::FormatItem::InitText(int subItem, VDStringW& s) const {
	switch(subItem) {
	case 0:
		switch(mFormat) {
		case nsVDPixmap::kPixFormat_XRGB1555:
		case nsVDPixmap::kPixFormat_RGB565:
		case nsVDPixmap::kPixFormat_RGB888:
		case nsVDPixmap::kPixFormat_R210:
		case nsVDPixmap::kPixFormat_R10K:
			s = L"RGB";
			break;
		case nsVDPixmap::kPixFormat_XRGB8888:
		case nsVDPixmap::kPixFormat_B64A:
		case nsVDPixmap::kPixFormat_XRGB64:
			s = L"RGBA";
			break;
		case nsVDPixmap::kPixFormat_Y8:
		case nsVDPixmap::kPixFormat_Y8_FR:
		case nsVDPixmap::kPixFormat_Y16:
			s = L"Grayscale";
			break;
		case nsVDPixmap::kPixFormat_YUV422_UYVY:
		case nsVDPixmap::kPixFormat_YUV422_YUYV:
		case nsVDPixmap::kPixFormat_YUV444_Planar:
		case nsVDPixmap::kPixFormat_YUV422_Planar:
		case nsVDPixmap::kPixFormat_YUV420_Planar:
		case nsVDPixmap::kPixFormat_YUV411_Planar:
		case nsVDPixmap::kPixFormat_YUV410_Planar:
		case nsVDPixmap::kPixFormat_YUV420i_Planar:
		case nsVDPixmap::kPixFormat_YUV420it_Planar:
		case nsVDPixmap::kPixFormat_YUV420ib_Planar:
		case nsVDPixmap::kPixFormat_YUV444_Planar16:
		case nsVDPixmap::kPixFormat_YUV422_Planar16:
		case nsVDPixmap::kPixFormat_YUV420_Planar16:
		case nsVDPixmap::kPixFormat_YUV422_V210:
		case nsVDPixmap::kPixFormat_YUV444_V410:
		case nsVDPixmap::kPixFormat_YUV444_Y410:
		case nsVDPixmap::kPixFormat_YUV420_NV12:
		case nsVDPixmap::kPixFormat_YUV444_V308:
		case nsVDPixmap::kPixFormat_YUV422_P210:
		case nsVDPixmap::kPixFormat_YUV420_P010:
		case nsVDPixmap::kPixFormat_YUV422_P216:
		case nsVDPixmap::kPixFormat_YUV420_P016:
			s = L"YCbCr";
			break;
		case nsVDPixmap::kPixFormat_YUV422_Planar_Centered:
		case nsVDPixmap::kPixFormat_YUV420_Planar_Centered:
			s = L"YCbCr (601)";
			break;
		case nsVDPixmap::kPixFormat_YUVA444_Y416:
		case nsVDPixmap::kPixFormat_YUV444_Alpha_Planar:
		case nsVDPixmap::kPixFormat_YUV422_Alpha_Planar:
		case nsVDPixmap::kPixFormat_YUV420_Alpha_Planar:
		case nsVDPixmap::kPixFormat_YUV444_Alpha_Planar16:
		case nsVDPixmap::kPixFormat_YUV422_Alpha_Planar16:
		case nsVDPixmap::kPixFormat_YUV420_Alpha_Planar16:
			s = L"YCbCr + Alpha";
			break;
		}
		break;

	case 1:
		switch(mFormat) {
		case nsVDPixmap::kPixFormat_XRGB1555:
		case nsVDPixmap::kPixFormat_RGB565:
		case nsVDPixmap::kPixFormat_RGB888:
		case nsVDPixmap::kPixFormat_XRGB8888:
		case nsVDPixmap::kPixFormat_XRGB64:
		case nsVDPixmap::kPixFormat_B64A:
		case nsVDPixmap::kPixFormat_R210:
		case nsVDPixmap::kPixFormat_R10K:
		case nsVDPixmap::kPixFormat_Y8_FR:
			s = L"Full";
			break;
		case nsVDPixmap::kPixFormat_Y8:
		case nsVDPixmap::kPixFormat_YUV422_Planar_Centered:
		case nsVDPixmap::kPixFormat_YUV420_Planar_Centered:
			s = L"Limited";
			break;
		default:
			s = L"*";
			break;
		}
		break;

	case 3:
		switch(mFormat) {
		case nsVDPixmap::kPixFormat_XRGB1555:
			s = L"packed (XRGB1555)";
			break;
		case nsVDPixmap::kPixFormat_RGB565:
			s = L"packed (RGB565)";
			break;
		case nsVDPixmap::kPixFormat_RGB888:
			s = L"packed (RGB24)";
			break;
		case nsVDPixmap::kPixFormat_XRGB8888:
			s = L"packed (RGBA32)";
			break;
		case nsVDPixmap::kPixFormat_XRGB64:
			s = L"packed (RGBA64)";
			break;
		case nsVDPixmap::kPixFormat_B64A:
			s = L"packed (b64a)";
			break;
		case nsVDPixmap::kPixFormat_R210:
			s = L"packed (r210)";
			break;
		case nsVDPixmap::kPixFormat_R10K:
			s = L"packed (R10k)";
			break;
		case nsVDPixmap::kPixFormat_Y8:
		case nsVDPixmap::kPixFormat_Y8_FR:
			s = L"planar";
			break;
		case nsVDPixmap::kPixFormat_YUV444_Planar:
		case nsVDPixmap::kPixFormat_YUV444_Alpha_Planar:
			s = L"4:4:4 planar";
			break;
		case nsVDPixmap::kPixFormat_YUV422_Planar:
		case nsVDPixmap::kPixFormat_YUV422_Alpha_Planar:
		case nsVDPixmap::kPixFormat_YUV422_Planar_Centered:
			s = L"4:2:2 planar";
			break;
		case nsVDPixmap::kPixFormat_YUV420_Planar:
		case nsVDPixmap::kPixFormat_YUV420_Alpha_Planar:
		case nsVDPixmap::kPixFormat_YUV420_Planar_Centered:
		case nsVDPixmap::kPixFormat_YUV420i_Planar:
		case nsVDPixmap::kPixFormat_YUV420it_Planar:
		case nsVDPixmap::kPixFormat_YUV420ib_Planar:
			s = L"4:2:0 planar";
			break;
		case nsVDPixmap::kPixFormat_YUV411_Planar:
			s = L"4:1:1 planar";
			break;
		case nsVDPixmap::kPixFormat_YUV410_Planar:
			s = L"4:1:0 planar";
			break;
		case nsVDPixmap::kPixFormat_YUV420_NV12:
			s = L"4:2:2 (NV12)";
			break;
		case nsVDPixmap::kPixFormat_YUV422_UYVY:
			s = L"4:2:2 (UYVY)";
			break;
		case nsVDPixmap::kPixFormat_YUV422_YUYV:
			s = L"4:2:2 (YUYV)";
			break;
		case nsVDPixmap::kPixFormat_YUV444_Planar16:
		case nsVDPixmap::kPixFormat_YUV444_Alpha_Planar16:
			s = L"4:4:4 planar";
			break;
		case nsVDPixmap::kPixFormat_YUV422_Planar16:
		case nsVDPixmap::kPixFormat_YUV422_Alpha_Planar16:
			s = L"4:2:2 planar";
			break;
		case nsVDPixmap::kPixFormat_YUV420_Planar16:
		case nsVDPixmap::kPixFormat_YUV420_Alpha_Planar16:
			s = L"4:2:0 planar";
			break;
		case nsVDPixmap::kPixFormat_YUV422_V210:
			s = L"4:2:2 (v210)";
			break;
		case nsVDPixmap::kPixFormat_YUV444_V410:
			s = L"4:4:4 (v410)";
			break;
		case nsVDPixmap::kPixFormat_YUV444_Y410:
			s = L"4:4:4 (Y410)";
			break;
		case nsVDPixmap::kPixFormat_YUV444_V308:
			s = L"4:4:4 (v308)";
			break;
		case nsVDPixmap::kPixFormat_YUV422_P210:
			s = L"4:2:2 (P210)";
			break;
		case nsVDPixmap::kPixFormat_YUV420_P010:
			s = L"4:2:0 (P010)";
			break;
		case nsVDPixmap::kPixFormat_YUV422_P216:
			s = L"4:2:2 (P216)";
			break;
		case nsVDPixmap::kPixFormat_YUV420_P016:
			s = L"4:2:0 (P016)";
			break;
		case nsVDPixmap::kPixFormat_YUVA444_Y416:
			s = L"4:4:4 (Y416)";
			break;
		}
		break;

	case 4:
		switch(mFormat) {
		case nsVDPixmap::kPixFormat_YUV422_Planar_Centered:
		case nsVDPixmap::kPixFormat_YUV420_Planar_Centered:
			s = L"centered";
			break;
		case nsVDPixmap::kPixFormat_YUV420i_Planar:
			s = L"interlaced";
			break;

		case nsVDPixmap::kPixFormat_YUV420it_Planar:
			s = L"interlaced top-field";
			break;

		case nsVDPixmap::kPixFormat_YUV420ib_Planar:
			s = L"interlaced bottom-field";
			break;
		default:
			s = L"-";
			break;
		}
		break;
	}
}

/////////////////////////////////

class VDDialogVideoDepthW32 : public VDDialogFrameW32 {
public:
	bool didChanges;

	inline VDDialogVideoDepthW32(VDPixmapFormatEx& opts, int type, int lockFormat, COMPVARS2* compVars)
		: VDDialogFrameW32(TemplateFromType(type))
		, mOpts(opts)
		, mbInputBrowsePending(false)
		, mType(type)
		, mLockFormat(lockFormat)
		, compVars(compVars)
	{
		didChanges = false;
	}

	inline bool Activate(VDGUIHandle hParent) { return 0!=ShowDialog(hParent); }
	void setCapture(BITMAPINFOHEADER* bm) { capSrc = bm; }

protected:
	INT_PTR DlgProc(UINT message, WPARAM wParam, LPARAM lParam);
	bool OnLoaded();
	bool OnCommand(uint32 id, uint32 extcode);
	void OnDataExchange(bool write);
	void InitFocus();
	void InitFinalFormat();
	void ApplyChanges();
	void SyncControls();
	void SyncInputColor();
	static int TemplateFromType(int type) {
		if (type==DepthDialog_input) return IDD_VIDEO_DECFORMAT;
		return IDD_VIDEO_ENCFORMAT;
	}

	COMPVARS2* compVars;
	BITMAPINFOHEADER* capSrc;
	VDPixmapFormatEx mInputFormat;
	VDPixmapFormatEx& mOpts;
	bool mbInputBrowsePending;
	bool mEnableMatrix;
	int mLockFormat;
	int mType;
	int outputReference;
	bool enableReference;

	struct FormatButtonMapping {
		int mFormat;
		uint32 mInputButton;
	};

	static const FormatButtonMapping kFormatButtonMappings[];
};

const VDDialogVideoDepthW32::FormatButtonMapping VDDialogVideoDepthW32::kFormatButtonMappings[] = {
	{	nsVDPixmap::kPixFormat_Null,			IDC_INPUT_AUTOSELECT},
	{	nsVDPixmap::kPixFormat_RGB888,			IDC_INPUT_RGB888},
	{	nsVDPixmap::kPixFormat_XRGB8888,		IDC_INPUT_XRGB8888},
	{	nsVDPixmap::kPixFormat_XRGB64,			IDC_INPUT_XRGB64},
	{	nsVDPixmap::kPixFormat_YUV422_UYVY,		IDC_INPUT_YUV422_UYVY},
	{	nsVDPixmap::kPixFormat_YUV422_YUYV,		IDC_INPUT_YUV422_YUY2},
	{	nsVDPixmap::kPixFormat_YUV420_Planar,	IDC_INPUT_YUV420_PLANAR},
	{	nsVDPixmap::kPixFormat_YUV422_Planar,	IDC_INPUT_YUV422_PLANAR},
	{	nsVDPixmap::kPixFormat_YUV410_Planar,	IDC_INPUT_YUV410_PLANAR},
	{	nsVDPixmap::kPixFormat_Y8,				IDC_INPUT_Y8},
	{	nsVDPixmap::kPixFormat_Y8_FR,			IDC_INPUT_I8},
	{	nsVDPixmap::kPixFormat_YUV444_Planar,	IDC_INPUT_YUV444_PLANAR},
	{	nsVDPixmap::kPixFormat_YUV422_V210,		IDC_INPUT_YUV422_V210},
	{	nsVDPixmap::kPixFormat_YUV422_UYVY_709,	IDC_INPUT_YUV422_UYVY_709},
	{	nsVDPixmap::kPixFormat_YUV420_NV12,		IDC_INPUT_YUV420_NV12},
	{	nsVDPixmap::kPixFormat_YUV444_Planar16,	IDC_INPUT_YUV444_PLANAR16},
	{	nsVDPixmap::kPixFormat_YUV422_Planar16,	IDC_INPUT_YUV422_PLANAR16},
	{	nsVDPixmap::kPixFormat_YUV420_Planar16,	IDC_INPUT_YUV420_PLANAR16},
};

INT_PTR VDDialogVideoDepthW32::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_HELP:
		{
			HELPINFO *lphi = (HELPINFO *)lParam;

			if (lphi->iContextType == HELPINFO_WINDOW)
				VDShowHelp(mhdlg, L"d-videocolordepth.html");
		}
		return TRUE;

	case WM_USER + 200:
		if (mbInputBrowsePending) {
			mbInputBrowsePending = false;

			VDDialogSelectVideoFormatW32 dlg(VDPixmapFormatNormalize(mInputFormat));
			if (dlg.ShowDialog((VDGUIHandle)mhdlg)) {
				int format = dlg.GetSelectedFormat();
				mInputFormat.format = format;
			}

			ApplyChanges();
			SyncControls();
		}

		return TRUE;
	}

	return VDDialogFrameW32::DlgProc(message, wParam, lParam);
}

bool VDDialogVideoDepthW32::OnLoaded() {
	OnDataExchange(false);
	InitFocus();
	InitFinalFormat();
	if (mType==DepthDialog_input) {
		if (inputAVI) {
			SetDlgItemTextW(mhdlg,IDC_ACTIVEDRIVER,g_inputDriver.c_str());
		} else {
			SetDlgItemTextW(mhdlg,IDC_ACTIVEDRIVER,L"None");
		}
	}
	if (mLockFormat!=-1) {
		EnableControl(IDC_INPUT_OTHER,false);
		for(int i=0; i<(int)sizeof(kFormatButtonMappings)/sizeof(kFormatButtonMappings[0]); ++i) {
			const FormatButtonMapping& fbm = kFormatButtonMappings[i];
			if (fbm.mFormat != mLockFormat)
				EnableControl(fbm.mInputButton,false);
		}
	}
	if (mType==DepthDialog_cap_output) {
		EnableControl(IDC_SAVEASDEFAULT,false);
	}
	return true;
}

bool VDDialogVideoDepthW32::OnCommand(uint32 id, uint32 extcode) {
	if (extcode == BN_CLICKED) {
		switch(id) {
			case IDC_SAVEASDEFAULT:
				{
					VDRegistryAppKey key("Preferences");

					VDPixmapFormatEx format = mInputFormat;
					if (mLockFormat!=-1) format.format = mLockFormat;
					format = VDPixmapFormatCombine(format);

					if (mType==DepthDialog_input)
						key.setInt("Input format", format);
					else
						key.setInt("Output format", format);

					if (enableReference)
						key.setInt("Output reference", outputReference);

					if (mEnableMatrix) {
						if (mType==DepthDialog_input) {
							key.setInt("Input space", mInputFormat.colorSpaceMode);
							key.setInt("Input range", mInputFormat.colorRangeMode);
						} else {
							key.setInt("Output space", mInputFormat.colorSpaceMode);
							key.setInt("Output range", mInputFormat.colorRangeMode);
						}
					}
				}
				break;

			case IDC_INPUT_OTHER:
				// There are some weird issues with BN_CLICKED being sent multiple times with
				// keyboard selection (mouse selection is OK).
				mbInputBrowsePending = true;
				PostMessage(mhdlg, WM_USER + 200, 0, 0);
				return TRUE;

			case IDC_CS_NONE:
				mInputFormat.colorSpaceMode = vd2::kColorSpaceMode_None;
				ApplyChanges();
				SyncInputColor();
				return TRUE;

			case IDC_CS_601:
				mInputFormat.colorSpaceMode = vd2::kColorSpaceMode_601;
				ApplyChanges();
				SyncInputColor();
				return TRUE;

			case IDC_CS_709:
				mInputFormat.colorSpaceMode = vd2::kColorSpaceMode_709;
				ApplyChanges();
				SyncInputColor();
				return TRUE;

			case IDC_CR_NONE:
				mInputFormat.colorRangeMode = vd2::kColorRangeMode_None;
				ApplyChanges();
				SyncInputColor();
				return TRUE;

			case IDC_CR_LIMITED:
				mInputFormat.colorRangeMode = vd2::kColorRangeMode_Limited;
				ApplyChanges();
				SyncInputColor();
				return TRUE;

			case IDC_CR_FULL:
				mInputFormat.colorRangeMode = vd2::kColorRangeMode_Full;
				ApplyChanges();
				SyncInputColor();
				return TRUE;
		}

		if (mType!=DepthDialog_input) {
			bool ref_changed = false;
			if (id==IDC_DEC_CHECK) {
				ref_changed = true;
				outputReference = IsButtonChecked(IDC_DEC_CHECK) ? 1:0;
				CheckButton(IDC_FLT_CHECK, false);
				if (outputReference) mInputFormat = 0;
			}
			if (id==IDC_FLT_CHECK) {
				ref_changed = true;
				outputReference = IsButtonChecked(IDC_FLT_CHECK) ? 2:0;
				CheckButton(IDC_DEC_CHECK, false);
				if (outputReference) mInputFormat = 0;
			}
			if (ref_changed) {
				if (outputReference==0) {
					if (mInputFormat.format==0) mInputFormat.format = nsVDPixmap::kPixFormat_RGB888;
					if (mInputFormat.colorRangeMode==0) mInputFormat.colorRangeMode = vd2::kColorRangeMode_Limited;
					if (mInputFormat.colorSpaceMode==0) mInputFormat.colorSpaceMode = vd2::kColorSpaceMode_601;
				}
				ApplyChanges();
				SyncControls();
			}
		}

		for(int i=0; i<(int)sizeof(kFormatButtonMappings)/sizeof(kFormatButtonMappings[0]); ++i) {
			const FormatButtonMapping& fbm = kFormatButtonMappings[i];
			if (fbm.mInputButton == id) {
				mInputFormat.format = fbm.mFormat;
				ApplyChanges();
				SyncInputColor();
			}
		}
	}

	return false;
}

void VDDialogVideoDepthW32::OnDataExchange(bool write) {
	if (write) {
		mOpts = mInputFormat;
		if (enableReference) g_dubOpts.video.outputReference = outputReference;
	} else {
		outputReference = g_dubOpts.video.outputReference;
		enableReference = mType==DepthDialog_output;
		if (g_dubOpts.video.mode<DubVideoOptions::M_FULL) enableReference = false;
		if (!enableReference) outputReference = 1;
		mInputFormat = VDPixmapFormatNormalizeOpt(mOpts);
		SyncControls();
		if (mType!=DepthDialog_input) {
			CheckButton(IDC_DEC_CHECK, outputReference==1);
			CheckButton(IDC_FLT_CHECK, outputReference==2);
			EnableControl(IDC_DEC_CHECK, true);
			EnableControl(IDC_FLT_CHECK, enableReference);
			EnableControl(IDC_FLTFORMAT, enableReference);
		}
	}
}

void VDDialogVideoDepthW32::ApplyChanges() {
	if (mType==DepthDialog_input) {
		if (inputVideo) {
			didChanges = true;
			g_project->StopFilters();
			VDRenderSetVideoSourceInputFormat(inputVideo, mInputFormat);
		}
	}
	
	InitFinalFormat();
}

void VDDialogVideoDepthW32::InitFinalFormat() {
	if (mType==DepthDialog_input) {
		VDPixmapFormatEx format;
		bool isDefault = false;
		if (inputVideo) {
			format = VDPixmapFormatNormalize(inputVideo->getTargetFormat());
			VDPixmapFormatEx dformat = VDPixmapFormatNormalize(inputVideo->getDefaultFormat());
			isDefault = format.fullEqual(dformat);
		} else {
			format = mInputFormat;
		}
		VDString s;
		s += " ";
		s += VDPixmapFormatPrintSpec(format);
		SetDlgItemText(mhdlg,IDC_ACTIVEFORMAT,s.c_str());
		ShowControl(IDC_DEFAULT, isDefault);
	}
	if (mType==DepthDialog_output) {
		MakeOutputFormat make;
		make.initGlobal();
		make.reference = outputReference;
		make.mode = DubVideoOptions::M_FULL;
		make.option = mInputFormat;
		if (mLockFormat!=-1) make.option.format = mLockFormat;
		make.initComp(compVars);
		make.combine();
		if (make.w==0){ make.w = 320; make.h = 240; }
		make.combineComp();

		VDString s;
		s = " ";
		s += VDPixmapFormatPrintSpec(make.dec);
		SetDlgItemText(mhdlg,IDC_DECFORMAT,s.c_str());

		s = " ";
		s += VDPixmapFormatPrintSpec(make.flt);
		SetDlgItemText(mhdlg,IDC_FLTFORMAT,s.c_str());

		s = " ";
		s += VDPixmapFormatPrintSpec(make.out);
		SetDlgItemText(mhdlg,IDC_OUTFORMAT,s.c_str());

		ShowControl(IDC_OUT_MSG, !make.error.empty());
	}
	if (mType==DepthDialog_cap_output) {
		MakeOutputFormat make;
		make.initCapture(capSrc);
		make.option = mInputFormat;
		if (mLockFormat!=-1) make.option.format = mLockFormat;
		make.initComp(compVars);
		make.out = make.option;
		if (!make.option) make.out = make.dec;
		make.combineComp();

		VDString s;
		s = " ";
		s += VDPixmapFormatPrintSpec(make.dec);
		SetDlgItemText(mhdlg,IDC_DECFORMAT,s.c_str());

		s = " ";
		s += VDPixmapFormatPrintSpec(make.dec);
		SetDlgItemText(mhdlg,IDC_FLTFORMAT,s.c_str());

		s = " ";
		s += VDPixmapFormatPrintSpec(make.out);
		SetDlgItemText(mhdlg,IDC_OUTFORMAT,s.c_str());

		ShowControl(IDC_OUT_MSG, !make.error.empty());
	}
}

void VDDialogVideoDepthW32::InitFocus() {
	int format = mLockFormat!=-1 ? mLockFormat : mInputFormat;
	for(int i=0; i<(int)sizeof(kFormatButtonMappings)/sizeof(kFormatButtonMappings[0]); ++i) {
		const FormatButtonMapping& fbm = kFormatButtonMappings[i];

		if (fbm.mFormat == format) {
			SetFocusToControl(fbm.mInputButton);
			return;
		}
	}

	SetFocusToControl(IDC_INPUT_OTHER);
}

void VDDialogVideoDepthW32::SyncControls() {
	int format = mLockFormat!=-1 ? mLockFormat : mInputFormat;

	bool enableDefault = mLockFormat==-1;
	if (mType!=DepthDialog_input) {
		if (outputReference==0) enableDefault = false;
	}
	EnableControl(IDC_INPUT_AUTOSELECT,enableDefault);

	uint32 inputButton = 0;
	for(int i=0; i<(int)sizeof(kFormatButtonMappings)/sizeof(kFormatButtonMappings[0]); ++i) {
		const FormatButtonMapping& fbm = kFormatButtonMappings[i];

		if (fbm.mFormat == format)
			inputButton = fbm.mInputButton;

		CheckButton(fbm.mInputButton, fbm.mFormat == format);
	}

	CheckButton(IDC_INPUT_OTHER, inputButton==0);
	SyncInputColor();
}

void VDDialogVideoDepthW32::SyncInputColor() {
	int format = mLockFormat!=-1 ? mLockFormat : mInputFormat;
	bool enable = VDPixmapFormatMatrixType(format)!=0;
	if (format==0 && mType==DepthDialog_input && inputVideo) {
		VDPixmapFormatEx src = inputVideo->getTargetFormat();
		enable = VDPixmapFormatMatrixType(src)!=0;
	}
	if (mType==DepthDialog_cap_output) {
		enable = false;
	}
	bool enableDefault = true;
	if (mType==DepthDialog_output) {
		MakeOutputFormat make;
		make.initGlobal();
		make.reference = outputReference;
		make.mode = DubVideoOptions::M_FULL;
		make.option = format;
		make.combine();
		enable = VDPixmapFormatMatrixType(make.out)!=0;
		if (outputReference==0) enableDefault = false;
		if (outputReference==1) enableDefault = VDPixmapFormatMatrixType(make.dec)!=0;
		if (outputReference==2) enableDefault = VDPixmapFormatMatrixType(make.flt)!=0;
		bool use_ref = false;
		if (mInputFormat.format==0 && mLockFormat==-1) use_ref = true;
		if (mInputFormat.colorRangeMode==0 && enable) use_ref = true;
		if (mInputFormat.colorSpaceMode==0 && enable) use_ref = true;
		ShowControl(IDC_LINK_DEC, outputReference==1 && use_ref);
		ShowControl(IDC_LINK_FLT, outputReference==2 && use_ref);
		ShowControl(IDC_LINK_OUT, outputReference && use_ref);
	}
	if (mType==DepthDialog_cap_output) {
		bool use_ref = false;
		if (mInputFormat.format==0 && mLockFormat==-1) use_ref = true;
		ShowControl(IDC_LINK_DEC, outputReference==1 && use_ref);
		ShowControl(IDC_LINK_FLT, outputReference==2 && use_ref);
		ShowControl(IDC_LINK_OUT, outputReference && use_ref);
	}
	mEnableMatrix = enable;

	EnableControl(IDC_STATIC_COLORSPACE, enable);
	EnableControl(IDC_STATIC_COLORRANGE, enable);
	EnableControl(IDC_CS_NONE,   enable && enableDefault);
	EnableControl(IDC_CS_601,    enable);
	EnableControl(IDC_CS_709,    enable);
	EnableControl(IDC_CR_NONE,   enable && enableDefault);
	EnableControl(IDC_CR_LIMITED,enable);
	EnableControl(IDC_CR_FULL,   enable);
	if (enable) {
		VDPixmapFormatEx format = mInputFormat;
		if (!enableDefault && !format.colorSpaceMode) format.colorSpaceMode = vd2::kColorSpaceMode_601;
		if (!enableDefault && !format.colorRangeMode) format.colorRangeMode = vd2::kColorRangeMode_Limited;
		CheckButton(IDC_CS_NONE, format.colorSpaceMode == vd2::kColorSpaceMode_None);
		CheckButton(IDC_CS_601, format.colorSpaceMode == vd2::kColorSpaceMode_601);
		CheckButton(IDC_CS_709, format.colorSpaceMode == vd2::kColorSpaceMode_709);
		CheckButton(IDC_CR_NONE, format.colorRangeMode == vd2::kColorRangeMode_None);
		CheckButton(IDC_CR_LIMITED, format.colorRangeMode == vd2::kColorRangeMode_Limited);
		CheckButton(IDC_CR_FULL, format.colorRangeMode == vd2::kColorRangeMode_Full);
	} else {
		CheckButton(IDC_CS_NONE,    true);
		CheckButton(IDC_CS_601,     false);
		CheckButton(IDC_CS_709,     false);
		CheckButton(IDC_CR_NONE,    true);
		CheckButton(IDC_CR_LIMITED, false);
		CheckButton(IDC_CR_FULL,    false);
	}
}

bool VDDisplayVideoDepthDialog(VDGUIHandle hParent, VDPixmapFormatEx& opts, int type, int lockFormat, COMPVARS2* compVars, BITMAPINFOHEADER* capSrc) {
	VDDialogVideoDepthW32 dlg(opts,type,lockFormat,compVars);
	dlg.setCapture(capSrc);
	dlg.Activate(hParent);

	return dlg.didChanges;
}

///////////////////////////////////////////////////////////////////////////
//
//	Performance dialog
//
///////////////////////////////////////////////////////////////////////////

static const long outputBufferSizeArray[]={
	128*1024,
	192*1024,
	256*1024,
	512*1024,
	768*1024,
	1*1024*1024,
	2*1024*1024,
	3*1024*1024,
	4*1024*1024,
	6*1024*1024,
	8*1024*1024,
	12*1024*1024,
	16*1024*1024,
	20*1024*1024,
	24*1024*1024,
	32*1024*1024,
	48*1024*1024,
	64*1024*1024,
};

static const long waveBufferSizeArray[]={
	8*1024,
	12*1024,
	16*1024,
	24*1024,
	32*1024,
	48*1024,
	64*1024,
	96*1024,
	128*1024,
	192*1024,
	256*1024,
	384*1024,
	512*1024,
	768*1024,
	1024*1024,
	1536*1024,
	2048*1024,
	3*1024*1024,
	4*1024*1024,
	6*1024*1024,
	8*1024*1024
};

static const long pipeBufferCountArray[]={
	4,
	6,
	8,
	12,
	16,
	24,
	32,
	48,
	64,
	96,
	128,
	192,
	256,
};

static const long audioBufferSizeArray[]={
	1,
	2,
	3,
	4,
	6,
	8,
	10,
	12,
	16,
	20,
	24,
	32,
	48,
	64
};

#define ELEMENTS(x) (sizeof (x)/sizeof(x)[0])

class VDDialogPerformanceOptions : public VDDialogFrameW32 {
public:
	VDDialogPerformanceOptions() : VDDialogFrameW32(IDD_PERFORMANCE) {}

protected:
	bool OnLoaded();
	void OnHScroll(uint32 id, int code);
	void OnDataExchange(bool write);

	VDStringW	mOutputBufferFormat;
	VDStringW	mWaveBufferFormat;
	VDStringW	mVideoBufferFormat;
	VDStringW	mAudioBufferFormat;
};

bool VDDialogPerformanceOptions::OnLoaded() {
	mOutputBufferFormat = GetControlValueString(IDC_OUTPUT_BUFFER_SIZE);
	mWaveBufferFormat = GetControlValueString(IDC_WAVE_BUFFER_SIZE);
	mVideoBufferFormat = GetControlValueString(IDC_STATIC_VIDEO_BUFFERS);
	mAudioBufferFormat = GetControlValueString(IDC_STATIC_AUDIO_BUFFER);

	return VDDialogFrameW32::OnLoaded();
}

void VDDialogPerformanceOptions::OnHScroll(uint32 id, int code) {
	sint32 pos = TBGetValue(id);

	switch(id) {
	case IDC_OUTPUT_BUFFER:
		{
			long bufferSize = outputBufferSizeArray[pos];

			VDStringW s;
			if (bufferSize >= 1048576)
				s.sprintf(L"%uMB", bufferSize >> 20);
			else
				s.sprintf(L"%uKB", bufferSize >> 10);

			SetControlTextF(IDC_OUTPUT_BUFFER_SIZE, mOutputBufferFormat.c_str(), s.c_str());
		}
		break;

	case IDC_WAVE_INPUT_BUFFER:
		{
			long bufferSize = waveBufferSizeArray[pos];

			VDStringW s;
			if (bufferSize >= 1048576)
				s.sprintf(L"%uMB", bufferSize >> 20);
			else
				s.sprintf(L"%uKB", bufferSize >> 10);

			SetControlTextF(IDC_WAVE_BUFFER_SIZE, mOutputBufferFormat.c_str(), s.c_str());
		}
		break;

	case IDC_VIDEO_BUFFERS:
		SetControlTextF(IDC_STATIC_VIDEO_BUFFERS, mVideoBufferFormat.c_str(), pipeBufferCountArray[pos]);
		break;

	case IDC_AUDIO_BUFFER:
		SetControlTextF(IDC_STATIC_AUDIO_BUFFER, mAudioBufferFormat.c_str(), audioBufferSizeArray[pos]);
		break;
	}
}

void VDDialogPerformanceOptions::OnDataExchange(bool write) {
	if (write) {
		VDPreferencesGetRenderOutputBufferSize() = outputBufferSizeArray[TBGetValue(IDC_OUTPUT_BUFFER)];
		VDPreferencesGetRenderWaveBufferSize() = waveBufferSizeArray[TBGetValue(IDC_WAVE_INPUT_BUFFER)];
		VDPreferencesGetRenderVideoBufferCount() = pipeBufferCountArray[TBGetValue(IDC_VIDEO_BUFFERS)];
		VDPreferencesGetRenderAudioBufferSeconds() = audioBufferSizeArray[TBGetValue(IDC_AUDIO_BUFFER)];

		VDSavePreferences();
	} else {
		TBSetRange(IDC_OUTPUT_BUFFER, 0, sizeof outputBufferSizeArray / sizeof outputBufferSizeArray[0] - 1);
		TBSetValue(IDC_OUTPUT_BUFFER, NearestLongValue(VDPreferencesGetRenderOutputBufferSize(), outputBufferSizeArray, ELEMENTS(outputBufferSizeArray)));
		OnHScroll(IDC_OUTPUT_BUFFER, 0);

		TBSetRange(IDC_WAVE_INPUT_BUFFER, 0, sizeof waveBufferSizeArray / sizeof waveBufferSizeArray[0] - 1);
		TBSetValue(IDC_WAVE_INPUT_BUFFER, NearestLongValue(VDPreferencesGetRenderWaveBufferSize(), waveBufferSizeArray, ELEMENTS(waveBufferSizeArray)));
		OnHScroll(IDC_WAVE_INPUT_BUFFER, 0);

		TBSetRange(IDC_VIDEO_BUFFERS, 0, sizeof pipeBufferCountArray / sizeof pipeBufferCountArray[0] - 1);
		TBSetValue(IDC_VIDEO_BUFFERS, NearestLongValue(VDPreferencesGetRenderVideoBufferCount(), pipeBufferCountArray, ELEMENTS(pipeBufferCountArray)));
		OnHScroll(IDC_VIDEO_BUFFERS, 0);

		TBSetRange(IDC_AUDIO_BUFFER, 0, sizeof audioBufferSizeArray / sizeof audioBufferSizeArray[0] - 1);
		TBSetValue(IDC_AUDIO_BUFFER, NearestLongValue(VDPreferencesGetRenderAudioBufferSeconds(), audioBufferSizeArray, ELEMENTS(audioBufferSizeArray)));
		OnHScroll(IDC_AUDIO_BUFFER, 0);
	}
}

void VDShowPerformanceDialog(VDGUIHandle hParent) {
	VDDialogPerformanceOptions dlg;

	dlg.ShowDialog(hParent);
}

INT_PTR CALLBACK DynamicCompileOptionsDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	DubOptions *dopt = (DubOptions *)GetWindowLongPtr(hDlg, DWLP_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			SetWindowLongPtr(hDlg, DWLP_USER, lParam);
			dopt = (DubOptions *)lParam;

			CheckDlgButton(hDlg, IDC_ENABLE, dopt->perf.dynamicEnable);
			CheckDlgButton(hDlg, IDC_DISPLAY_CODE, dopt->perf.dynamicShowDisassembly);

            return (TRUE);

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				dopt->perf.dynamicEnable = !!IsDlgButtonChecked(hDlg, IDC_ENABLE);
				dopt->perf.dynamicShowDisassembly = !!IsDlgButtonChecked(hDlg, IDC_DISPLAY_CODE);
				EndDialog(hDlg, TRUE);
				return TRUE;
			case IDCANCEL:
				EndDialog(hDlg, FALSE);
				return TRUE;
			}
            break;
    }
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////
//
//	video frame rate dialog
//
///////////////////////////////////////////////////////////////////////////

class VDDialogVideoFrameRateW32 : public VDDialogBaseW32 {
public:
	inline VDDialogVideoFrameRateW32(DubOptions& opts, IVDVideoSource *pVS, AudioSource *pAS) : VDDialogBaseW32(IDD_VIDEO_FRAMERATE), mOpts(opts), mpVideo(pVS), mpAudio(pAS) {}

	bool Activate(VDGUIHandle hParent) { return 0 != ActivateDialog(hParent); }

protected:
	INT_PTR DlgProc(UINT message, WPARAM wParam, LPARAM lParam);
	void ReinitDialog();

	void RedoIVTCEnables();

	DubOptions& mOpts;
	IVDVideoSource *const mpVideo;
	AudioSource *const mpAudio;
};

void VDDialogVideoFrameRateW32::RedoIVTCEnables() {
	bool f3, f4;
	BOOL e;

	f3 = !!IsDlgButtonChecked(mhdlg, IDC_IVTC_RECONFIELDSFIXED);
	f4 = !!IsDlgButtonChecked(mhdlg, IDC_IVTC_RECONFRAMESMANUAL);

	e = f3 || f4;

	EnableWindow(GetDlgItem(mhdlg, IDC_STATIC_IVTCOFFSET), e);
	EnableWindow(GetDlgItem(mhdlg, IDC_IVTCOFFSET), e);
	EnableWindow(GetDlgItem(mhdlg, IDC_INVPOLARITY), e);
}

INT_PTR VDDialogVideoFrameRateW32::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
        case WM_INITDIALOG:
			ReinitDialog();
            return (TRUE);

		case WM_HELP:
			{
				HELPINFO *lphi = (HELPINFO *)lParam;

				if (lphi->iContextType == HELPINFO_WINDOW)
					VDShowHelp(mhdlg, L"d-videoframerate.html");
			}
			return TRUE;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_DECIMATE_1:
			case IDC_DECIMATE_2:
			case IDC_DECIMATE_3:
				EnableWindow(GetDlgItem(mhdlg, IDC_DECIMATE_VALUE), FALSE);
				EnableWindow(GetDlgItem(mhdlg, IDC_FRAMERATE_TARGET), FALSE);
				break;
			case IDC_DECIMATE_N:
				EnableWindow(GetDlgItem(mhdlg, IDC_DECIMATE_VALUE), TRUE);
				EnableWindow(GetDlgItem(mhdlg, IDC_FRAMERATE_TARGET), FALSE);
				break;

			case IDC_DECIMATE_TARGET:
				EnableWindow(GetDlgItem(mhdlg, IDC_DECIMATE_VALUE), FALSE);
				EnableWindow(GetDlgItem(mhdlg, IDC_FRAMERATE_TARGET), TRUE);
				SetFocus(GetDlgItem(mhdlg, IDC_FRAMERATE_TARGET));
				break;

			case IDC_FRAMERATE_CHANGE:
				if (SendMessage((HWND)lParam, BM_GETSTATE, 0, 0) & BST_CHECKED)
					EnableWindow(GetDlgItem(mhdlg, IDC_FRAMERATE),TRUE);
				break;

			case IDC_FRAMERATE_SAMELENGTH:
			case IDC_FRAMERATE_NOCHANGE:
				if (SendMessage((HWND)lParam, BM_GETSTATE, 0, 0) & BST_CHECKED)
					EnableWindow(GetDlgItem(mhdlg, IDC_FRAMERATE),FALSE);
				break;

			case IDC_IVTC_OFF:
			case IDC_IVTC_RECONFIELDS:
			case IDC_IVTC_RECONFIELDSFIXED:
			case IDC_IVTC_RECONFRAMESMANUAL:
				{
					BOOL f = IsDlgButtonChecked(mhdlg, IDC_IVTC_OFF);

					EnableWindow(GetDlgItem(mhdlg, IDC_DECIMATE_1), f);
					EnableWindow(GetDlgItem(mhdlg, IDC_DECIMATE_2), f);
					EnableWindow(GetDlgItem(mhdlg, IDC_DECIMATE_3), f);
					EnableWindow(GetDlgItem(mhdlg, IDC_DECIMATE_N), f);
					EnableWindow(GetDlgItem(mhdlg, IDC_DECIMATE_VALUE), f && IsDlgButtonChecked(mhdlg, IDC_DECIMATE_N));
					EnableWindow(GetDlgItem(mhdlg, IDC_DECIMATE_TARGET), f);
					EnableWindow(GetDlgItem(mhdlg, IDC_FRAMERATE_TARGET), f && IsDlgButtonChecked(mhdlg, IDC_DECIMATE_N));
					RedoIVTCEnables();
				}
				break;

			case IDOK:
				{
					VDFraction newTarget(0,0);
					int newFRD = 1;

					if (IsDlgButtonChecked(mhdlg, IDC_DECIMATE_TARGET)) {
						double newFR;
						char buf[128], tmp;

						GetDlgItemText(mhdlg, IDC_FRAMERATE_TARGET, buf, sizeof buf);

						if (1!=sscanf(buf, "%lg %c", &newFR, &tmp) || newFR<=0.0) {
							SetFocus(GetDlgItem(mhdlg, IDC_FRAMERATE));
							MessageBeep(MB_ICONQUESTION);
							return FALSE;
						}

						newTarget = VDFraction((uint32)(0.5 + newFR * 10000.0), 10000);
						if (!newTarget.getHi()) {
							SetFocus(GetDlgItem(mhdlg, IDC_FRAMERATE));
							MessageBeep(MB_ICONQUESTION);
							return FALSE;
						}

						newFRD = 1;
					} else if (IsDlgButtonChecked(mhdlg, IDC_DECIMATE_N)) {
						LONG lv = GetDlgItemInt(mhdlg, IDC_DECIMATE_VALUE, NULL, TRUE);

						if (lv<1) {
							SetFocus(GetDlgItem(mhdlg, IDC_DECIMATE_VALUE));
							MessageBeep(MB_ICONQUESTION);
							return FALSE;
						}

						newFRD = lv;
					} else if (IsDlgButtonChecked(mhdlg, IDC_DECIMATE_1))
						newFRD = 1;
					else if (IsDlgButtonChecked(mhdlg, IDC_DECIMATE_2))
						newFRD = 2;
					else if (IsDlgButtonChecked(mhdlg, IDC_DECIMATE_3))
						newFRD = 3;

					if (IsDlgButtonChecked(mhdlg, IDC_FRAMERATE_CHANGE)) {
						char buf[128];

						buf[0] = 0;
						GetDlgItemText(mhdlg, IDC_FRAMERATE, buf, sizeof buf);

						VDFraction fr;
						unsigned hi, lo;
						bool failed = false;
						if (2==sscanf(buf, " %u / %u", &hi, &lo)) {
							if (!lo)
								failed = true;
							else
								fr = VDFraction(hi, lo);
						} else if (!fr.Parse(buf) || fr.asDouble() >= 1000000.0) {
							failed = true;
						}

						if (fr.getHi() == 0)
							failed = true;

						if (failed) {
							SetFocus(GetDlgItem(mhdlg, IDC_FRAMERATE));
							MessageBeep(MB_ICONQUESTION);
							return FALSE;
						}

						mOpts.video.mFrameRateAdjustHi = fr.getHi();
						mOpts.video.mFrameRateAdjustLo = fr.getLo();
					} else if (IsDlgButtonChecked(mhdlg, IDC_FRAMERATE_SAMELENGTH)) {
						mOpts.video.mFrameRateAdjustHi = DubVideoOptions::kFrameRateAdjustSameLength;
						mOpts.video.mFrameRateAdjustLo = 0;
					} else {
						mOpts.video.mFrameRateAdjustHi = 0;
						mOpts.video.mFrameRateAdjustLo = 0;
					}

					mOpts.video.frameRateDecimation = newFRD;
					mOpts.video.frameRateTargetHi = newTarget.getHi();
					mOpts.video.frameRateTargetLo = newTarget.getLo();
				}

				End(true);
				return TRUE;
			case IDCANCEL:
				End(false);
				return TRUE;
			}
            break;
    }
    return FALSE;
}

void VDDialogVideoFrameRateW32::ReinitDialog() {
	char buf[128];

	if (mOpts.video.frameRateDecimation==1 && mOpts.video.frameRateTargetLo)
		CheckDlgButton(mhdlg, IDC_DECIMATE_TARGET, TRUE);
	else
		EnableWindow(GetDlgItem(mhdlg, IDC_FRAMERATE_TARGET), FALSE);

	CheckDlgButton(mhdlg, IDC_DECIMATE_1, mOpts.video.frameRateDecimation==1 && !mOpts.video.frameRateTargetLo);
	CheckDlgButton(mhdlg, IDC_DECIMATE_2, mOpts.video.frameRateDecimation==2);
	CheckDlgButton(mhdlg, IDC_DECIMATE_3, mOpts.video.frameRateDecimation==3);
	CheckDlgButton(mhdlg, IDC_DECIMATE_N, mOpts.video.frameRateDecimation>3);
	if (mOpts.video.frameRateDecimation>3)
		SetDlgItemInt(mhdlg, IDC_DECIMATE_VALUE, mOpts.video.frameRateDecimation, FALSE);
	else
		EnableWindow(GetDlgItem(mhdlg, IDC_DECIMATE_VALUE), FALSE);

	if (mOpts.video.frameRateTargetLo) {
		sprintf(buf, "%.4f", (double)mOpts.video.frameRateTargetHi / (double)mOpts.video.frameRateTargetLo);
		SetDlgItemText(mhdlg, IDC_FRAMERATE_TARGET, buf);
	}

	if (mpVideo) {
		IVDStreamSource *pVSS = mpVideo->asStream();
		sprintf(buf, "No change (current: %.3f fps)", pVSS->getRate().asDouble());
		SetDlgItemText(mhdlg, IDC_FRAMERATE_NOCHANGE, buf);

		if (mpAudio && mpAudio->getLength()) {
			VDFraction framerate = VDFraction((double)pVSS->getLength() * mpAudio->getRate().asDouble() / mpAudio->getLength());
			sprintf(buf, "(%.3f fps)", framerate.asDouble());
			SetDlgItemText(mhdlg, IDC_FRAMERATE_SAMELENGTH_VALUE, buf);
		} else
			EnableWindow(GetDlgItem(mhdlg, IDC_FRAMERATE_SAMELENGTH), FALSE);
	}

	if (mOpts.video.mFrameRateAdjustLo) {
		VDFraction fr(mOpts.video.mFrameRateAdjustHi, mOpts.video.mFrameRateAdjustLo);
		sprintf(buf, "%.4f", fr.asDouble());

		VDFraction fr2(fr);
		VDVERIFY(fr2.Parse(buf));

		if (fr2 != fr)
			sprintf(buf, "%u/%u (~%.7f)", fr.getHi(), fr.getLo(), fr.asDouble());

		SetDlgItemText(mhdlg, IDC_FRAMERATE, buf);
		CheckDlgButton(mhdlg, IDC_FRAMERATE_CHANGE, TRUE);
	} else if (mOpts.video.mFrameRateAdjustHi == DubVideoOptions::kFrameRateAdjustSameLength) {
		if (!mpAudio)
			CheckDlgButton(mhdlg, IDC_FRAMERATE_NOCHANGE, TRUE);
		else
			CheckDlgButton(mhdlg, IDC_FRAMERATE_SAMELENGTH, TRUE);
		EnableWindow(GetDlgItem(mhdlg, IDC_FRAMERATE), FALSE);
	} else {
		CheckDlgButton(mhdlg, IDC_FRAMERATE_NOCHANGE, TRUE);
		EnableWindow(GetDlgItem(mhdlg, IDC_FRAMERATE), FALSE);
	}

	RedoIVTCEnables();
}

bool VDDisplayVideoFrameRateDialog(VDGUIHandle hParent, DubOptions& opts, IVDVideoSource *pVS, AudioSource *pAS) {
	VDDialogVideoFrameRateW32 dlg(opts, pVS, pAS);

	return dlg.Activate(hParent);
}

///////////////////////////////////////////////////////////////////////////
//
//	video range dialog
//
///////////////////////////////////////////////////////////////////////////

class VDDialogVideoRangeW32 : public VDDialogBaseW32 {
public:
	VDDialogVideoRangeW32(DubOptions& opts, const VDFraction& frameRate, VDPosition frameCount, VDPosition& startSel, VDPosition& endSel);

	inline bool Activate(VDGUIHandle hParent) { return 0 != ActivateDialog(hParent); }

protected:
	INT_PTR DlgProc(UINT message, WPARAM wParam, LPARAM lParam);
	void ReinitDialog();

	void MSToFrames(UINT idFrames, UINT idMS);
	void FramesToMS(UINT idMS, UINT idFrames);
	void LengthFrames();
	void LengthMS();

	DubOptions& mOpts;
	VDPosition& mSelStart;
	VDPosition& mSelEnd;
	VDFraction mFrameRate;
	VDPosition mFrameCount;
	VDTime mTotalTimeMS;

	bool mbReentry;
};

VDDialogVideoRangeW32::VDDialogVideoRangeW32(DubOptions& opts, const VDFraction& frameRate, VDPosition frameCount, VDPosition& startSel, VDPosition& endSel)
	: VDDialogBaseW32(IDD_VIDEO_CLIPPING)
	, mOpts(opts)
	, mSelStart(startSel)
	, mSelEnd(endSel)
	, mFrameRate(frameRate)
	, mFrameCount(frameCount)
	, mTotalTimeMS(0)
	, mbReentry(false)
{
	mTotalTimeMS = VDRoundToInt64(mFrameCount * mFrameRate.AsInverseDouble() * 1000.0);
}

void VDDialogVideoRangeW32::MSToFrames(UINT idFrames, UINT idMS) {
	VDPosition frames;
	VDTime ms;
	BOOL ok;

	ms = GetDlgItemInt(mhdlg, idMS, &ok, FALSE);
	if (!ok)
		return;
	mbReentry = true;

	frames = VDRoundToInt64((double)ms * mFrameRate.asDouble() / 1000.0);
	SetDlgItemInt(mhdlg, idFrames, (UINT)frames, FALSE);
	SetDlgItemInt(mhdlg, IDC_LENGTH_MS,
				(UINT)(mTotalTimeMS
				-GetDlgItemInt(mhdlg, IDC_END_MS, NULL, FALSE)
				-GetDlgItemInt(mhdlg, IDC_START_MS, NULL, FALSE)), TRUE);
	SetDlgItemInt(mhdlg, IDC_LENGTH_FRAMES,
				(UINT)(mFrameCount
				-GetDlgItemInt(mhdlg, IDC_END_FRAMES, NULL, FALSE)
				-GetDlgItemInt(mhdlg, IDC_START_FRAMES, NULL, FALSE)), TRUE);
	mbReentry = false;
}

void VDDialogVideoRangeW32::FramesToMS(UINT idMS, UINT idFrames) {
	VDPosition frames;
	VDTime ms;
	BOOL ok;

	frames = GetDlgItemInt(mhdlg, idFrames, &ok, FALSE);
	if (!ok) return;
	mbReentry = true;

	ms = VDRoundToInt64((double)frames * mFrameRate.AsInverseDouble() * 1000.0);
	SetDlgItemInt(mhdlg, idMS, (UINT)ms, FALSE);
	SetDlgItemInt(mhdlg, IDC_LENGTH_MS,
				(UINT)(mTotalTimeMS
				-GetDlgItemInt(mhdlg, IDC_END_MS, NULL, FALSE)
				-GetDlgItemInt(mhdlg, IDC_START_MS, NULL, FALSE)), TRUE);
	SetDlgItemInt(mhdlg, IDC_LENGTH_FRAMES,
				(UINT)(mFrameCount
				-GetDlgItemInt(mhdlg, IDC_END_FRAMES, NULL, FALSE)
				-GetDlgItemInt(mhdlg, IDC_START_FRAMES, NULL, FALSE)), TRUE);
	mbReentry = false;
}

void VDDialogVideoRangeW32::LengthFrames() {
	VDPosition frames;
	VDTime ms;
	BOOL ok;

	frames = GetDlgItemInt(mhdlg, IDC_LENGTH_FRAMES, &ok, TRUE);
	if (!ok) return;
	mbReentry = true;

	ms = VDRoundToInt64((double)frames * mFrameRate.AsInverseDouble() * 1000.0);
	SetDlgItemInt(mhdlg, IDC_LENGTH_MS, (UINT)ms, FALSE);
	SetDlgItemInt(mhdlg, IDC_END_MS,
				(UINT)(mTotalTimeMS
				-ms
				-GetDlgItemInt(mhdlg, IDC_START_MS, NULL, TRUE)), TRUE);
	SetDlgItemInt(mhdlg, IDC_END_FRAMES,
				(UINT)(mFrameCount
				-frames
				-GetDlgItemInt(mhdlg, IDC_START_FRAMES, NULL, TRUE)), TRUE);
	mbReentry = false;
}

void VDDialogVideoRangeW32::LengthMS() {
	VDPosition frames;
	VDTime ms;
	BOOL ok;

	ms = GetDlgItemInt(mhdlg, IDC_LENGTH_MS, &ok, TRUE);
	if (!ok) return;
	mbReentry = TRUE;

	frames = VDRoundToInt64((double)ms * mFrameRate.asDouble() / 1000.0);
	SetDlgItemInt(mhdlg, IDC_LENGTH_FRAMES, (UINT)frames, FALSE);
	SetDlgItemInt(mhdlg, IDC_END_MS,
				(UINT)(mTotalTimeMS
				-ms
				-GetDlgItemInt(mhdlg, IDC_START_MS, NULL, TRUE)), TRUE);
	SetDlgItemInt(mhdlg, IDC_END_FRAMES,
				(UINT)(mFrameCount
				-frames
				-GetDlgItemInt(mhdlg, IDC_START_FRAMES, NULL, TRUE)), TRUE);
	mbReentry = FALSE;
}

INT_PTR VDDialogVideoRangeW32::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG:
			SetDlgItemInt(mhdlg, IDC_START_MS, VDClampToUint32(mOpts.video.mSelectionStart.ResolveToMS(mFrameCount, mFrameRate, false)), FALSE);
			SetDlgItemInt(mhdlg, IDC_END_MS, VDClampToUint32(mOpts.video.mSelectionEnd.ResolveToMS(mFrameCount, mFrameRate, true)), FALSE);
			CheckDlgButton(mhdlg, IDC_OFFSET_AUDIO, mOpts.audio.fStartAudio);
			CheckDlgButton(mhdlg, IDC_CLIP_AUDIO, mOpts.audio.fEndAudio);
			CheckDlgButton(mhdlg, IDC_EDIT_AUDIO, mOpts.audio.mbApplyVideoTimeline);
            return (TRUE);

		case WM_HELP:
			{
				HELPINFO *lphi = (HELPINFO *)lParam;

				if (lphi->iContextType == HELPINFO_WINDOW)
					VDShowHelp(mhdlg, L"d-videorange.html");
			}
			return TRUE;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_START_MS:
				if (HIWORD(wParam)==EN_CHANGE && !mbReentry)
					MSToFrames(IDC_START_FRAMES, IDC_START_MS);
				break;
			case IDC_START_FRAMES:
				if (HIWORD(wParam)==EN_CHANGE && !mbReentry)
					FramesToMS(IDC_START_MS, IDC_START_FRAMES);
				break;
			case IDC_END_MS:
				if (HIWORD(wParam)==EN_CHANGE && !mbReentry)
					MSToFrames(IDC_END_FRAMES, IDC_END_MS);
				break;
			case IDC_END_FRAMES:
				if (HIWORD(wParam)==EN_CHANGE && !mbReentry)
					FramesToMS(IDC_END_MS, IDC_END_FRAMES);
				break;
			case IDC_LENGTH_MS:
				if (HIWORD(wParam)==EN_CHANGE && !mbReentry)
					LengthMS();
				break;
			case IDC_LENGTH_FRAMES:
				if (HIWORD(wParam)==EN_CHANGE && !mbReentry)
					LengthFrames();
				break;
			case IDOK:
				mSelStart = GetDlgItemInt(mhdlg, IDC_START_FRAMES, NULL, FALSE);
				mSelEnd = mFrameCount - GetDlgItemInt(mhdlg, IDC_END_FRAMES, NULL, FALSE);
				mOpts.audio.fStartAudio		= !!IsDlgButtonChecked(mhdlg, IDC_OFFSET_AUDIO);
				mOpts.audio.fEndAudio		= !!IsDlgButtonChecked(mhdlg, IDC_CLIP_AUDIO);
				mOpts.audio.mbApplyVideoTimeline = !!IsDlgButtonChecked(mhdlg, IDC_EDIT_AUDIO);
				End(true);
				return TRUE;
			case IDCANCEL:
				End(false);
				return TRUE;
			}
            break;
    }
    return FALSE;
}

bool VDDisplayVideoRangeDialog(VDGUIHandle hParent, DubOptions& opts, const VDFraction& frameRate, VDPosition frameCount, VDPosition& startSel, VDPosition& endSel) {
	VDDialogVideoRangeW32 dlg(opts, frameRate, frameCount, startSel, endSel);

	return dlg.Activate(hParent);
}

///////////////////////////////////////////////////////////////////////////

class VDDialogAudioVolumeW32 : public VDDialogBaseW32 {
public:
	inline VDDialogAudioVolumeW32(DubOptions& opts) : VDDialogBaseW32(IDD_AUDIO_VOLUME), mOpts(opts) {}

	inline bool Activate(VDGUIHandle hParent) { return 0 != ActivateDialog(hParent); }

protected:
	INT_PTR DlgProc(UINT message, WPARAM wParam, LPARAM lParam);
	void ReinitDialog();

	float SliderPositionToFactor(int pos);
	int FactorToSliderPosition(float factor);

	void UpdateVolumeText();
	void UpdateEnables();

	DubOptions& mOpts;
};

INT_PTR VDDialogAudioVolumeW32::DlgProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
			{
				HWND hwndSlider = GetDlgItem(mhdlg, IDC_SLIDER_VOLUME);

				SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 600));
				SendMessage(hwndSlider, TBM_SETTICFREQ, 10, 0);

				if (mOpts.audio.mVolume >= 0) {
					CheckDlgButton(mhdlg, IDC_ADJUSTVOL, BST_CHECKED);

					SendMessage(hwndSlider, TBM_SETPOS, TRUE, FactorToSliderPosition(mOpts.audio.mVolume));
				} else {
					SendMessage(hwndSlider, TBM_SETPOS, TRUE, 300);
					EnableWindow(hwndSlider, FALSE);
					EnableWindow(GetDlgItem(mhdlg, IDC_STATIC_VOLUME), FALSE);
				}
				UpdateVolumeText();
			}
            return (TRUE);

		case WM_HELP:
			{
				HELPINFO *lphi = (HELPINFO *)lParam;

				if (lphi->iContextType == HELPINFO_WINDOW)
					VDShowHelp(mhdlg, L"d-audiovolume.html");
			}
			return TRUE;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				if (IsDlgButtonChecked(mhdlg, IDC_ADJUSTVOL)) {
					int pos = SendDlgItemMessage(mhdlg, IDC_SLIDER_VOLUME, TBM_GETPOS, 0, 0);

					mOpts.audio.mVolume = SliderPositionToFactor(pos);
				} else
					mOpts.audio.mVolume = -1.0f;

				End(true);
				return TRUE;
			case IDCANCEL:
				End(false);
				return TRUE;

			case IDC_ADJUSTVOL:
				if (HIWORD(wParam)==BN_CLICKED)
					UpdateEnables();
				return TRUE;
			}
            break;

		case WM_HSCROLL:
			if (lParam)
				UpdateVolumeText();
			break;
    }
    return FALSE;
}

float VDDialogAudioVolumeW32::SliderPositionToFactor(int pos) {
	return expf((float)(pos - 300) * (0.005f*nsVDMath::kfLn10));
}

int VDDialogAudioVolumeW32::FactorToSliderPosition(float factor) {
	return VDRoundToInt((200.0f*nsVDMath::kfOneOverLn10)*logf(mOpts.audio.mVolume)) + 300;
}

void VDDialogAudioVolumeW32::UpdateVolumeText() {
	char buf[64];
	int pos = SendDlgItemMessage(mhdlg, IDC_SLIDER_VOLUME, TBM_GETPOS, 0, 0);

	float factor = SliderPositionToFactor(pos);
	sprintf(buf, "%+.1fdB (%.1f%%)", (float)(pos - 300) * 0.1f, 100.0f*factor);
	SetDlgItemText(mhdlg, IDC_STATIC_VOLUME, buf);
}

void VDDialogAudioVolumeW32::UpdateEnables() {
	BOOL f = !!IsDlgButtonChecked(mhdlg, IDC_ADJUSTVOL);

	EnableWindow(GetDlgItem(mhdlg, IDC_SLIDER_VOLUME), f);
	EnableWindow(GetDlgItem(mhdlg, IDC_STATIC_VOLUME), f);
}

bool VDDisplayAudioVolumeDialog(VDGUIHandle hParent, DubOptions& opts) {
	VDDialogAudioVolumeW32 dlg(opts);

	return dlg.Activate(hParent);
}

///////////////////////////////////////////////////////////////////////////
//
//	jump to position dialog
//
///////////////////////////////////////////////////////////////////////////

class VDDialogJumpToPositionW32 : public VDDialogBaseW32 {
public:
	inline VDDialogJumpToPositionW32(VDPosition currentFrame, IVDVideoSource *pVS, const VDFraction& realRate) : VDDialogBaseW32(IDD_JUMPTOFRAME), mFrame(currentFrame), mpVideo(pVS), mRealRate(realRate) {}

	VDPosition Activate(VDGUIHandle hParent) { return ActivateDialog(hParent) ? mFrame : -1; }

protected:
	INT_PTR DlgProc(UINT message, WPARAM wParam, LPARAM lParam);
	void ReinitDialog();

	VDPosition mFrame;
	IVDVideoSource *const mpVideo;
	VDFraction	mRealRate;
};

INT_PTR VDDialogJumpToPositionW32::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	char buf[64];

	switch(msg) {
	case WM_INITDIALOG:
		ReinitDialog();
		return FALSE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDCANCEL:
			End(false);
			break;
		case IDOK:
			if (IsDlgButtonChecked(mhdlg, IDC_JUMPTOFRAME)) {
				BOOL fOk;
				UINT uiFrame = GetDlgItemInt(mhdlg, IDC_FRAMENUMBER, &fOk, FALSE);

				if (!fOk) {
					SetFocus(GetDlgItem(mhdlg, IDC_FRAMENUMBER));
					MessageBeep(MB_ICONEXCLAMATION);
					return TRUE;
				}

				mFrame = uiFrame;

				End(true);
			} else {
				unsigned int hr, min;
				double sec = 0;
				int n;

				GetDlgItemText(mhdlg, IDC_FRAMETIME, buf, sizeof buf);

				n = sscanf(buf, "%u:%u:%lf", &hr, &min, &sec);

				if (n < 3) {
					hr = 0;
					n = sscanf(buf, "%u:%lf", &min, &sec);
				}

				if (n < 2) {
					min = 0;
					n = sscanf(buf, "%lf", &sec);
				}

				if (n < 1 || sec < 0) {
					SetFocus(GetDlgItem(mhdlg, IDC_FRAMETIME));
					MessageBeep(MB_ICONEXCLAMATION);
					return TRUE;
				}

				mFrame = VDRoundToInt64(mRealRate.asDouble() * (sec + min*60 + hr*3600));

				End(true);
			}
			break;
		case IDC_FRAMENUMBER:
			if (HIWORD(wParam) == EN_CHANGE) {
				CheckDlgButton(mhdlg, IDC_JUMPTOFRAME, BST_CHECKED);
				CheckDlgButton(mhdlg, IDC_JUMPTOTIME, BST_UNCHECKED);
			}
			break;
		case IDC_FRAMETIME:
			if (HIWORD(wParam) == EN_CHANGE) {
				CheckDlgButton(mhdlg, IDC_JUMPTOFRAME, BST_UNCHECKED);
				CheckDlgButton(mhdlg, IDC_JUMPTOTIME, BST_CHECKED);
			}
			break;
		case IDC_JUMPTOFRAME:
			SetFocus(GetDlgItem(mhdlg, IDC_FRAMENUMBER));
			SendDlgItemMessage(mhdlg, IDC_FRAMENUMBER, EM_SETSEL, 0, -1);
			break;
		case IDC_JUMPTOTIME:
			SetFocus(GetDlgItem(mhdlg, IDC_FRAMETIME));
			SendDlgItemMessage(mhdlg, IDC_FRAMETIME, EM_SETSEL, 0, -1);
			break;
		}
		return TRUE;
	}
	return FALSE;
}

void VDDialogJumpToPositionW32::ReinitDialog() {
	long ticks = VDRoundToLong(mFrame * 1000.0 / mRealRate.asDouble());
	long ms, sec, min;
	char buf[64];

	SendDlgItemMessage(mhdlg, IDC_FRAMETIME, EM_LIMITTEXT, 30, 0);
	SetDlgItemInt(mhdlg, IDC_FRAMENUMBER, (UINT)mFrame, FALSE);
	SetFocus(GetDlgItem(mhdlg, IDC_FRAMENUMBER));
	SendDlgItemMessage(mhdlg, IDC_FRAMENUMBER, EM_SETSEL, 0, -1);

	ms  = ticks %1000; ticks /= 1000;
	sec	= ticks %  60; ticks /=  60;
	min	= ticks %  60; ticks /=  60;

	if (ticks)
		wsprintf(buf, "%d:%02d:%02d.%03d", ticks, min, sec, ms);
	else
		wsprintf(buf, "%d:%02d.%03d", min, sec, ms);

	SetDlgItemText(mhdlg, IDC_FRAMETIME, buf);

	CheckDlgButton(mhdlg, IDC_JUMPTOFRAME, BST_CHECKED);
	CheckDlgButton(mhdlg, IDC_JUMPTOTIME, BST_UNCHECKED);
}

VDPosition VDDisplayJumpToPositionDialog(VDGUIHandle hParent, VDPosition currentFrame, IVDVideoSource *pVS, const VDFraction& realRate) {
	VDDialogJumpToPositionW32 dlg(currentFrame, pVS, realRate);

	return dlg.Activate(hParent);
}

///////////////////////////////////////////////////////////////////////////
//
//	error mode dialog
//
///////////////////////////////////////////////////////////////////////////

class VDDialogErrorModeW32 : public VDDialogBaseW32 {
public:
	inline VDDialogErrorModeW32(const char *pszSettingsKey, IVDStreamSource *pSource) : VDDialogBaseW32(IDD_ERRORMODE), mpszSettingsKey(pszSettingsKey), mpSource(pSource) {}

	DubSource::ErrorMode Activate(VDGUIHandle hParent, DubSource::ErrorMode oldMode);

	void ComputeMode();

protected:
	INT_PTR DlgProc(UINT message, WPARAM wParam, LPARAM lParam);
	void ReinitDialog();

	const char *const mpszSettingsKey;
	IVDStreamSource::ErrorMode	mErrorMode;
	IVDStreamSource *const mpSource;
};

DubSource::ErrorMode VDDialogErrorModeW32::Activate(VDGUIHandle hParent, DubSource::ErrorMode oldMode) {
	mErrorMode = oldMode;
	ActivateDialog(hParent);
	return mErrorMode;
}

void VDDialogErrorModeW32::ReinitDialog() {
	EnableWindow(GetDlgItem(mhdlg, IDC_SAVEASDEFAULT), mpszSettingsKey != 0);
	EnableWindow(GetDlgItem(mhdlg, IDC_ERROR_CONCEAL), !mpSource || mpSource->isDecodeErrorModeSupported(DubSource::kErrorModeConceal));
	EnableWindow(GetDlgItem(mhdlg, IDC_ERROR_DECODE), !mpSource || mpSource->isDecodeErrorModeSupported(DubSource::kErrorModeDecodeAnyway));

	CheckDlgButton(mhdlg, IDC_ERROR_REPORTALL,	mErrorMode == DubSource::kErrorModeReportAll);
	CheckDlgButton(mhdlg, IDC_ERROR_CONCEAL,	mErrorMode == DubSource::kErrorModeConceal);
	CheckDlgButton(mhdlg, IDC_ERROR_DECODE,		mErrorMode == DubSource::kErrorModeDecodeAnyway);
}

INT_PTR VDDialogErrorModeW32::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
        case WM_INITDIALOG:
			ReinitDialog();
            return TRUE;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				ComputeMode();
				End(true);
				return TRUE;
			case IDCANCEL:
				End(false);
				return TRUE;
			case IDC_SAVEASDEFAULT:
				{
					VDRegistryAppKey key("Preferences");

					ComputeMode();
					key.setInt(mpszSettingsKey, mErrorMode);
				}
				return TRUE;
			}
            break;
    }
    return FALSE;
}

void VDDialogErrorModeW32::ComputeMode() {
	if (IsDlgButtonChecked(mhdlg, IDC_ERROR_REPORTALL))
		mErrorMode = DubSource::kErrorModeReportAll;
	if (IsDlgButtonChecked(mhdlg, IDC_ERROR_CONCEAL))
		mErrorMode = DubSource::kErrorModeConceal;
	if (IsDlgButtonChecked(mhdlg, IDC_ERROR_DECODE))
		mErrorMode = DubSource::kErrorModeDecodeAnyway;
}

DubSource::ErrorMode VDDisplayErrorModeDialog(VDGUIHandle hParent, IVDStreamSource::ErrorMode oldMode, const char *pszSettingsKey, IVDStreamSource *pSource) {
	VDDialogErrorModeW32 dlg(pszSettingsKey, pSource);

	return dlg.Activate(hParent, oldMode);
}

///////////////////////////////////////////////////////////////////////////
//
//	File info dialog
//
///////////////////////////////////////////////////////////////////////////

class VDDialogFileTextInfoW32 : public VDDialogBaseW32 {
public:
	typedef std::map<uint32, VDStringW> tTextInfo;
	typedef std::list<std::pair<uint32, VDStringA> > tRawTextInfo;

	VDDialogFileTextInfoW32(tRawTextInfo& info);
	void Activate(VDGUIHandle hParent);

protected:
	void Read();
	void Write();
	void ReinitDialog();
	void RedoColumnWidths();
	void BeginEdit(int index);
	void EndEdit(bool write);
	void UpdateRow(int index);

	INT_PTR DlgProc(UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK LVStaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT LVWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK LVStaticEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT LVEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	HWND	mhwndList;
	HWND	mhwndEdit;
	WNDPROC	mOldLVProc;
	WNDPROC	mOldEditProc;
	int		mIndex;
	uint32	mID;

	tTextInfo mTextInfo;
	tRawTextInfo& mTextInfoOrig;

	static const struct FieldEntry {
		uint32 fcc;
		const char *desc;
	} kFields[];
};

const struct VDDialogFileTextInfoW32::FieldEntry VDDialogFileTextInfoW32::kFields[]={
	{ VD_FOURCC('ISBJ'), "Subject" },
	{ VD_FOURCC('IART'), "Artist (Author)" },
	{ VD_FOURCC('ICOP'), "Copyright" },
	{ VD_FOURCC('IARL'), "Archival Location" },
	{ VD_FOURCC('ICMS'), "Commissioned" },
	{ VD_FOURCC('ICMT'), "Comments" },
	{ VD_FOURCC('ICRD'), "Creation Date" },
	{ VD_FOURCC('ICRP'), "Cropped" },
	{ VD_FOURCC('IDIM'), "Dimensions" },
	{ VD_FOURCC('IDPI'), "Dots Per Inch" },
	{ VD_FOURCC('IENG'), "Engineer" },
	{ VD_FOURCC('IGNR'), "Genre" },
	{ VD_FOURCC('IKEY'), "Keywords" },
	{ VD_FOURCC('ILGT'), "Lightness" },
	{ VD_FOURCC('IMED'), "Medium" },
	{ VD_FOURCC('INAM'), "Name" },
	{ VD_FOURCC('IPLT'), "Palette Setting" },
	{ VD_FOURCC('IPRD'), "Product" },
	{ VD_FOURCC('ISFT'), "Software" },
	{ VD_FOURCC('ISHP'), "Sharpness" },
	{ VD_FOURCC('ISRC'), "Source" },
	{ VD_FOURCC('ISRF'), "Source Form" },
	{ VD_FOURCC('ITCH'), "Technician" },
};

VDDialogFileTextInfoW32::VDDialogFileTextInfoW32(tRawTextInfo& info)
	: VDDialogBaseW32(IDD_FILE_SETTEXTINFO)
	, mhwndEdit(NULL)
	, mTextInfoOrig(info)
{
}

void VDDialogFileTextInfoW32::Activate(VDGUIHandle hParent) {
	ActivateDialogDual(hParent);
}

void VDDialogFileTextInfoW32::Read() {
	tRawTextInfo::const_iterator itSrc(mTextInfoOrig.begin()), itSrcEnd(mTextInfoOrig.end());
	for(; itSrc != itSrcEnd; ++itSrc)
		mTextInfo[(*itSrc).first] = VDTextAToW((*itSrc).second);
}

void VDDialogFileTextInfoW32::Write() {
	mTextInfoOrig.clear();

	tTextInfo::const_iterator itSrc(mTextInfo.begin()), itSrcEnd(mTextInfo.end());
	for(; itSrc != itSrcEnd; ++itSrc)
		mTextInfoOrig.push_back(tRawTextInfo::value_type((*itSrc).first, VDTextWToA((*itSrc).second)));
}

void VDDialogFileTextInfoW32::ReinitDialog() {
	HWND hwndList = GetDlgItem(mhdlg, IDC_LIST);

	mhwndList = hwndList;

	SetWindowLong(mhwndList, GWL_STYLE, GetWindowLong(mhwndList, GWL_STYLE) | WS_CLIPCHILDREN);

	union {
		LVCOLUMNA a;
		LVCOLUMNW w;
	} lvc;

	if (VDIsWindowsNT()) {
		SendMessageW(hwndList, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

		lvc.w.mask = LVCF_TEXT | LVCF_WIDTH;
		lvc.w.pszText = L"Field";
		lvc.w.cx = 50;
		SendMessageW(hwndList, LVM_INSERTCOLUMNW, 0, (LPARAM)&lvc.w);

		lvc.w.pszText = L"Text";
		lvc.w.cx = 100;
		SendMessageW(hwndList, LVM_INSERTCOLUMNW, 0, (LPARAM)&lvc.w);
	} else {
		SendMessageA(hwndList, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

		lvc.a.mask = LVCF_TEXT | LVCF_WIDTH;
		lvc.a.iSubItem = 0;
		lvc.a.pszText = "Field";
		lvc.a.cx = 50;
		SendMessageA(hwndList, LVM_INSERTCOLUMNA, 0, (LPARAM)&lvc.a);

		lvc.a.pszText = "Text";
		lvc.a.cx = 100;
		SendMessageA(hwndList, LVM_INSERTCOLUMNA, 0, (LPARAM)&lvc.a);
	}

	for(int i=0; i<sizeof kFields / sizeof kFields[0]; ++i) {
		union {
			LVITEMA a;
			LVITEMW w;
		} lvi;

		if (VDIsWindowsNT()) {
			VDStringW wtext(VDTextAToW(kFields[i].desc));
			lvi.w.mask = LVIF_TEXT | LVIF_PARAM;
			lvi.w.pszText = (LPWSTR)wtext.c_str();
			lvi.w.iItem = i;
			lvi.w.iSubItem = 0;
			lvi.w.lParam = (LPARAM)kFields[i].fcc;

			SendMessageW(hwndList, LVM_INSERTITEMW, 0, (LPARAM)&lvi.w);
		} else {
			lvi.a.mask = LVIF_TEXT | LVIF_PARAM;
			lvi.a.pszText = (LPSTR)kFields[i].desc;
			lvi.a.iItem = i;
			lvi.a.iSubItem = 0;
			lvi.a.lParam = (LPARAM)kFields[i].fcc;

			SendMessageA(hwndList, LVM_INSERTITEMA, 0, (LPARAM)&lvi.a);
		}

		UpdateRow(i);
	}

	RedoColumnWidths();

	if (VDIsWindowsNT()) {
		mOldLVProc = (WNDPROC)GetWindowLongPtrW(mhwndList, GWLP_WNDPROC);
		SetWindowLongPtrW(mhwndList, GWLP_USERDATA, (LONG_PTR)this);
		SetWindowLongPtrW(mhwndList, GWLP_WNDPROC, (LONG_PTR)LVStaticWndProc);
	} else {
		mOldLVProc = (WNDPROC)GetWindowLongPtrA(mhwndList, GWLP_WNDPROC);
		SetWindowLongPtrA(mhwndList, GWLP_USERDATA, (LONG_PTR)this);
		SetWindowLongPtrA(mhwndList, GWLP_WNDPROC, (LONG_PTR)LVStaticWndProc);
	}
}

void VDDialogFileTextInfoW32::RedoColumnWidths() {
	SendMessage(mhwndList, LVM_SETCOLUMNWIDTH, 0, LVSCW_AUTOSIZE);
	SendMessage(mhwndList, LVM_SETCOLUMNWIDTH, 1, LVSCW_AUTOSIZE_USEHEADER);
}

void VDDialogFileTextInfoW32::BeginEdit(int index) {
	RECT r;
	int w=0, w2=0;
	int i;

	ListView_EnsureVisible(mhwndList, index, FALSE);

	for(i=0; i<=1; i++)
		w2 += w = SendMessage(mhwndList, LVM_GETCOLUMNWIDTH, i, 0);

	EndEdit(true);

	r.left = LVIR_BOUNDS;

	LVITEM lvi;
	lvi.mask = LVIF_PARAM;
	lvi.iItem = index;
	lvi.iSubItem = 0;
	ListView_GetItem(mhwndList, &lvi);
	SendMessage(mhwndList, LVM_GETITEMRECT, index, (LPARAM)&r);

	mID = lvi.lParam;
	mIndex = index;

	DWORD dwEditStyle = WS_VISIBLE|WS_CHILD|WS_BORDER | ES_WANTRETURN|ES_AUTOHSCROLL;

	r.left = w2-w;
	r.right = w2;

	InflateRect(&r, GetSystemMetrics(SM_CXEDGE), GetSystemMetrics(SM_CYEDGE));

	AdjustWindowRect(&r, dwEditStyle, FALSE);

	if (VDIsWindowsNT()) {
		mhwndEdit = CreateWindowW(L"EDIT",
				NULL,
				dwEditStyle,
				r.left,
				r.top,
				r.right - r.left,
				r.bottom - r.top,
				mhwndList, (HMENU)1, g_hInst, NULL);
	} else {
		mhwndEdit = CreateWindowA("EDIT",
				NULL,
				dwEditStyle,
				r.left,
				r.top,
				r.right - r.left,
				r.bottom - r.top,
				mhwndList, (HMENU)1, g_hInst, NULL);
	}
	
	if (mhwndEdit) {
		if (VDIsWindowsNT()) {
			mOldEditProc = (WNDPROC)GetWindowLongPtrW(mhwndEdit, GWLP_WNDPROC);
			SetWindowLongPtrW(mhwndEdit, GWLP_USERDATA, (LONG_PTR)this);
			SetWindowLongPtrW(mhwndEdit, GWLP_WNDPROC, (LONG_PTR)LVStaticEditProc);
		} else {
			mOldEditProc = (WNDPROC)GetWindowLongPtrA(mhwndEdit, GWLP_WNDPROC);
			SetWindowLongPtrA(mhwndEdit, GWLP_USERDATA, (LONG_PTR)this);
			SetWindowLongPtrA(mhwndEdit, GWLP_WNDPROC, (LONG_PTR)LVStaticEditProc);
		}

		SendMessage(mhwndEdit, WM_SETFONT, SendMessage(mhwndList, WM_GETFONT, 0, 0), MAKELPARAM(FALSE,0));

		tTextInfo::iterator it(mTextInfo.find(mID));
		if (it != mTextInfo.end())
			VDSetWindowTextW32(mhwndEdit, (*it).second.c_str());

		SetFocus(mhwndEdit);
	}
}

void VDDialogFileTextInfoW32::EndEdit(bool write) {
	if (!mhwndEdit)
		return;

	if (write) {
		const VDStringW text(VDGetWindowTextW32(mhwndEdit));

		if (text.empty())
			mTextInfo.erase(mID);
		else
			mTextInfo[mID] = text;

		UpdateRow(mIndex);
	}

	DestroyWindow(mhwndEdit);
	mhwndEdit = NULL;
}

void VDDialogFileTextInfoW32::UpdateRow(int index) {
	union {
		LVITEMA a;
		LVITEMW w;
	} lvi;

	uint32 id;

	if (VDIsWindowsNT()) {
		lvi.w.mask = LVIF_PARAM;
		lvi.w.iItem = index;
		lvi.w.iSubItem = 0;
		SendMessageW(mhwndList, LVM_GETITEMW, 0, (LPARAM)&lvi.w);
		id = lvi.w.lParam;
	} else {
		lvi.a.mask = LVIF_PARAM;
		lvi.a.iItem = index;
		lvi.a.iSubItem = 0;
		SendMessageA(mhwndList, LVM_GETITEMA, 0, (LPARAM)&lvi.a);
		id = lvi.a.lParam;
	}

	const wchar_t *text = L"";

	tTextInfo::iterator it(mTextInfo.find(id));
	if (it != mTextInfo.end())
		text = (*it).second.c_str();

	if (VDIsWindowsNT()) {
		lvi.w.mask = LVIF_TEXT;
		lvi.w.iSubItem = 1;
		lvi.w.pszText = (LPWSTR)text;
		SendMessageW(mhwndList, LVM_SETITEMW, 0, (LPARAM)&lvi.w);
		id = lvi.w.lParam;
	} else {
		VDStringA textA(VDTextWToA(text));
		lvi.a.mask = LVIF_TEXT;
		lvi.a.iSubItem = 1;
		lvi.a.pszText = (LPSTR)textA.c_str();
		SendMessageA(mhwndList, LVM_SETITEMA, 0, (LPARAM)&lvi.a);
		id = lvi.a.lParam;
	}
}

INT_PTR VDDialogFileTextInfoW32::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
        case WM_INITDIALOG:
			Read();
			ReinitDialog();
			SetFocus(mhwndList);
            return FALSE;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				EndEdit(true);
				Write();
				End(true);
				return TRUE;
			case IDCANCEL:
				EndEdit(false);
				End(false);
				return TRUE;
			}
            break;
    }
    return FALSE;
}

LRESULT CALLBACK VDDialogFileTextInfoW32::LVStaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDDialogFileTextInfoW32 *p = (VDDialogFileTextInfoW32 *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	return p->LVWndProc(hwnd, msg, wParam, lParam);
}

LRESULT VDDialogFileTextInfoW32::LVWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_DESTROY:
		EndEdit(true);
		break;

	case WM_GETDLGCODE:
		if (lParam) {
			const MSG& msg = *(const MSG *)lParam;

			if (msg.message == WM_KEYDOWN && wParam == VK_RETURN)
				return DLGC_WANTMESSAGE;
		} else
			return VDDualCallWindowProcW32(mOldLVProc, hwnd, msg, wParam, lParam) | DLGC_WANTALLKEYS;

		break;

	case WM_KEYDOWN:
		if (wParam == VK_RETURN) {
			int index = VDDualCallWindowProcW32(mOldLVProc, hwnd, LVM_GETNEXTITEM, -1, MAKELPARAM(LVNI_ALL|LVNI_SELECTED,0));

			if (index>=0)
				BeginEdit(index);
		}
		break;

	case WM_LBUTTONDOWN:
		{
			LVHITTESTINFO htinfo;
			LVITEM lvi;
			int index;

			// if this isn't done, the control doesn't gain focus properly...

			VDDualCallWindowProcW32(mOldLVProc, hwnd, msg, wParam, lParam);

			htinfo.pt.x	= 2;
			htinfo.pt.y = HIWORD(lParam);

			index = VDDualCallWindowProcW32(mOldLVProc, hwnd, LVM_HITTEST, 0, (LPARAM)&htinfo);

			if (index >= 0) {
				int x = LOWORD(lParam);
				int w2=0, w;
				int i=-1;

				lvi.state = lvi.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
				VDDualCallWindowProcW32(mOldLVProc, hwnd, LVM_SETITEMSTATE, index, (LPARAM)&lvi);

				for(i=0; i<3; i++) {
					w2 += w = VDDualCallWindowProcW32(mOldLVProc, hwnd, LVM_GETCOLUMNWIDTH, i, 0);
					if (x<w2) {
						BeginEdit(index);

						return 0;
					}
				}
			}
			EndEdit(true);
		}
		return 0;

	case WM_VSCROLL:
	case WM_MOUSEWHEEL:
		EndEdit(true);
		break;
	}
	return VDDualCallWindowProcW32(mOldLVProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK VDDialogFileTextInfoW32::LVStaticEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDDialogFileTextInfoW32 *p = (VDDialogFileTextInfoW32 *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	return p->LVEditProc(hwnd, msg, wParam, lParam);
}

LRESULT VDDialogFileTextInfoW32::LVEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_GETDLGCODE:
		return VDDualCallWindowProcW32(mOldEditProc, hwnd, msg, wParam, lParam) | DLGC_WANTALLKEYS;
		break;
	case WM_KEYDOWN:
		if (wParam == VK_UP) {
			if (mIndex > 0) {
				ListView_SetItemState(mhwndList, -1, 0, LVIS_SELECTED|LVIS_FOCUSED);
				ListView_SetItemState(mhwndList, mIndex-1, LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED);
				BeginEdit(mIndex-1);
			}
			return 0;
		} else if (wParam == VK_DOWN) {
			if (mIndex < SendMessage(mhwndList, LVM_GETITEMCOUNT, 0, 0)-1) {
				ListView_SetItemState(mhwndList, -1, 0, LVIS_SELECTED|LVIS_FOCUSED);
				ListView_SetItemState(mhwndList, mIndex+1, LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED);
				BeginEdit(mIndex+1);
			}
			return 0;
		}
		break;
	case WM_CHAR:
		if (wParam == 0x0d) {
			EndEdit(true);
			return 0;
		} else if (wParam == 0x1b) {
			EndEdit(false);
			return 0;
		}
		break;
	case WM_KILLFOCUS:
		EndEdit(true);
		break;
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
			EndEdit(true);
		break;
	}
	return VDDualCallWindowProcW32(mOldEditProc, hwnd, msg, wParam, lParam);
}

void VDDisplayFileTextInfoDialog(VDGUIHandle hParent, std::list<std::pair<uint32, VDStringA> >& info) {
	VDDialogFileTextInfoW32 dlg(info);

	dlg.Activate(hParent);
}
