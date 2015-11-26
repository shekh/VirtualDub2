//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2010 Avery Lee
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
#include "resource.h"
#include <commctrl.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/strutil.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/Dita/services.h>
#include <vd2/vdjson/jsonvalue.h>
#include <vd2/vdjson/jsonreader.h>
#include <vd2/vdjson/jsonwriter.h>
#include "ExternalEncoderProfile.h"

extern const char g_szError[];
const wchar_t g_szErrorW[]=L"VirtualDub Error";

///////////////////////////////////////////////////////////////////////////

namespace {
	class JSONStreamWriter : public IVDJSONWriterOutput {
	public:
		JSONStreamWriter(IVDStream *output)
			: mOutput(output)
		{
		}

		void WriteChars(const wchar_t *src, uint32 len) {
			char buf[4];

			for(uint32 i=0; i<len; ++i) {
				const uint32 c = src[i];

				if (src[i] == L'\n') {
					mOutput.PutLine();
				} else if (c < 0x007F) {
					buf[0] = (char)c;
					mOutput.Write(buf, 1);
				} else if (c < 0x0800) {
					buf[0] = (char)(((c >>  6) & 0x7f) | 0xC0);
					buf[1] = (char)(((c      ) & 0x7f) | 0x80);
					mOutput.Write(buf, 2);
				} else if (c < 0x10000) {
					buf[0] = (char)(((c >> 12) & 0x3f) | 0xE0);
					buf[1] = (char)(((c >>  6) & 0x3f) | 0x80);
					buf[2] = (char)(((c      ) & 0x3f) | 0x80);
					mOutput.Write(buf, 3);
				} else {
					buf[0] = (char)(((c >> 18) & 0x3f) | 0xF0);
					buf[1] = (char)(((c >> 12) & 0x3f) | 0x80);
					buf[2] = (char)(((c >>  6) & 0x3f) | 0x80);
					buf[3] = (char)(((c      ) & 0x3f) | 0x80);
					mOutput.Write(buf, 4);
				}
			}
		}

	protected:
		VDTextOutputStream mOutput;
	};
}

///////////////////////////////////////////////////////////////////////////

class VDUIDialogExtEncMain : public VDDialogFrameW32 {
public:
	VDUIDialogExtEncMain(VDExtEncProfile& profile);
	~VDUIDialogExtEncMain();

	VDEvent<VDUIDialogExtEncMain, VDExtEncType>& OnTypeChanged() {
		return mEventTypeChanged;
	}

protected:
	bool OnLoaded();
	void OnDataExchange(bool write);
	bool OnCommand(uint32 id, uint32 extcode);
	void UpdateEnables();

	void OnTypeChangedHandler(VDUIProxyComboBoxControl *source, int index);

	VDEvent<VDUIDialogExtEncMain, VDExtEncType> mEventTypeChanged;

	VDUIProxyComboBoxControl mTypeCombo;
	VDDelegate mDelegateOnTypeChanged;

	HFONT mhFontMarlett;

	VDExtEncProfile& mProfile;
};

VDUIDialogExtEncMain::VDUIDialogExtEncMain(VDExtEncProfile& profile)
	: VDDialogFrameW32(IDD_EXTENC_EDIT_MAIN)
	, mProfile(profile)
	, mhFontMarlett(NULL)
{
	mTypeCombo.OnSelectionChanged() += mDelegateOnTypeChanged.Bind(this, &VDUIDialogExtEncMain::OnTypeChangedHandler);
}

VDUIDialogExtEncMain::~VDUIDialogExtEncMain() {
	if (mhFontMarlett)
		DeleteObject(mhFontMarlett);
}

bool VDUIDialogExtEncMain::OnLoaded() {
	AddProxy(&mTypeCombo, IDC_TYPE);

	if (!mhFontMarlett) {
		HFONT hfontDlg = (HFONT)SendMessage(mhdlg, WM_GETFONT, 0, 0);

		if (hfontDlg) {
			LOGFONT lf = {0};
			if (GetObject(hfontDlg, sizeof lf, &lf)) {
				mhFontMarlett = CreateFont(lf.lfHeight, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Marlett");
			}
		}
	}

	if (mhFontMarlett) {
		HWND hwndControl = GetControl(IDC_CMDLINE_ARG);

		if (hwndControl)
			SendMessage(hwndControl, WM_SETFONT, (WPARAM)mhFontMarlett, MAKELONG(TRUE, 0));
	}

	mTypeCombo.AddItem(L"Video encoder");
	mTypeCombo.AddItem(L"Audio encoder");
	mTypeCombo.AddItem(L"Multiplexer");

	OnDataExchange(false);
	SetFocusToControl(IDC_TYPE);
	return true;
}

void VDUIDialogExtEncMain::OnDataExchange(bool write) {
	if (write)
		mProfile.mType = (VDExtEncType)mTypeCombo.GetSelection();
	else
		mTypeCombo.SetSelection((int)mProfile.mType);

	ExchangeControlValueString(write, IDC_PROGRAM, mProfile.mProgram);
	ExchangeControlValueString(write, IDC_ARGUMENTS, mProfile.mCommandArguments);
	ExchangeControlValueString(write, IDC_OUTPUTFILENAME, mProfile.mOutputFilename);
	ExchangeControlValueBoolCheckbox(write, IDC_CHECKRETCODE, mProfile.mbCheckReturnCode);
	ExchangeControlValueBoolCheckbox(write, IDC_LOG_STDOUT, mProfile.mbLogStdout);
	ExchangeControlValueBoolCheckbox(write, IDC_LOG_STDERR, mProfile.mbLogStderr);
	ExchangeControlValueBoolCheckbox(write, IDC_PREDELETE_OUTPUTFILE, mProfile.mbPredeleteOutputFile);

	if (!write)
		UpdateEnables();
}

bool VDUIDialogExtEncMain::OnCommand(uint32 id, uint32 extcode) {
	if (id == IDC_BROWSE) {
		const VDStringW fn(VDGetLoadFileName('encp', (VDGUIHandle)mhdlg, L"Select External Encoder", L"Executable Program (*.exe)\0*.exe\0", L"exe"));

		if (!fn.empty()) {
			SetControlText(IDC_PROGRAM, fn.c_str());
		}

		return true;
	} else if (id == IDC_CMDLINE_ARG) {
		const wchar_t *const kMenuItems[]={
			L"%(width)\tVideo frame width",
			L"%(height)\tVideo frame height",
			L"%(fps)\tVideo frame rate (fractional)",
			L"%(fpsnum)\tVideo frame rate fraction numerator",
			L"%(fpsden)\tVideo frame rate fraction denominator",
			L"%(outputname)\tOutput file name, with extension",
			L"%(outputbasename)\tOutput file name, without extension",
			L"%(outputfile)\tOutput directory and file name",
			L"%(outputdir)\tOutput directory only",
			L"%(hostdir)\tVirtualDub program directory",
			L"%(programdir)\tEncoder program directory",
			L"%(systemdir)\tOS system directory",
			L"%(tempvideofile)\tTemporary video directory and file name",
			L"%(tempaudiofile)\tTemporary audio directory and file name",
			L"%(pix_fmt)\tPixel format, compatible with ffmpeg -pix_fmt",
			L"%(samplingrate)\tAudio sampling rate, in Hz",
			L"%(samplingkhz)\tAudio sampling rate, in KHz (fractional)",
			L"%(channels)\tAudio channel count",
			L"%(audioprecision)\tAudio sample precision, in bits",
			NULL
		};

		int idx = ActivateMenuButton(IDC_CMDLINE_ARG, kMenuItems);

		if ((unsigned)idx < sizeof(kMenuItems)/sizeof(kMenuItems[0]) - 1) {
			const wchar_t *s = kMenuItems[idx];
			const wchar_t *t = wcschr(s, '\t');

			if (t) {
				HWND hwndEdit = GetControl(IDC_ARGUMENTS);

				if (hwndEdit) {
					if (IsWindowUnicode(hwndEdit)) {
						SendMessageW(hwndEdit, EM_SETSEL, -1, -1);
						SendMessageW(hwndEdit, EM_REPLACESEL, TRUE, (LPARAM)VDStringW(s, t).c_str());
					} else {
						SendMessageA(hwndEdit, EM_SETSEL, -1, -1);
						SendMessageA(hwndEdit, EM_REPLACESEL, TRUE, (LPARAM)VDTextWToA(VDStringW(s, t)).c_str());
					}

					SetFocus(hwndEdit);
				}
			}
		}
	}

	return false;
}

void VDUIDialogExtEncMain::UpdateEnables() {
	int type = mTypeCombo.GetSelection();

	EnableControl(IDC_OUTPUTFILENAME, type != kVDExtEncType_Mux);
}

void VDUIDialogExtEncMain::OnTypeChangedHandler(VDUIProxyComboBoxControl *source, int index) {
	if (index < 0)
		return;

	VDExtEncType type = (VDExtEncType)index;

	if (type != kVDExtEncType_Mux) {
		VDStringW outputName;
		GetControlText(IDC_OUTPUTFILENAME, outputName);

		const wchar_t *base = outputName.c_str();
		const wchar_t *ext = VDFileSplitExt(base);

		outputName.replace(ext - base, VDStringW::npos, type == kVDExtEncType_Video ? L".video" : L".audio", 6);
		SetControlText(IDC_OUTPUTFILENAME, outputName.c_str());
	}

	mEventTypeChanged.Raise(this, type);
	UpdateEnables();
}

///////////////////////////////////////////////////////////////////////////

class VDUIDialogExtEncAudio : public VDDialogFrameW32 {
public:
	VDUIDialogExtEncAudio(VDExtEncProfile& profile);
	~VDUIDialogExtEncAudio();

protected:
	void OnDataExchange(bool write);

	VDExtEncProfile& mProfile;
};

VDUIDialogExtEncAudio::VDUIDialogExtEncAudio(VDExtEncProfile& profile)
	: VDDialogFrameW32(IDD_EXTENC_EDIT_AUDIO)
	, mProfile(profile)
{
}

VDUIDialogExtEncAudio::~VDUIDialogExtEncAudio() {
}

void VDUIDialogExtEncAudio::OnDataExchange(bool write) {
	ExchangeControlValueBoolCheckbox(write, IDC_BYPASSCOMPRESSION, mProfile.mbBypassCompression);

	if (write) {
		if (IsButtonChecked(IDC_INPUT_WAV))
			mProfile.mInputFormat = kVDExtEncInputFormat_WAV;
		else
			mProfile.mInputFormat = kVDExtEncInputFormat_Raw;
	} else {
		CheckButton(IDC_INPUT_RAW, mProfile.mInputFormat == kVDExtEncInputFormat_Raw);
		CheckButton(IDC_INPUT_WAV, mProfile.mInputFormat == kVDExtEncInputFormat_WAV);
	}
}

///////////////////////////////////////////////////////////////////////////

class VDUIDialogExtEncVideo : public VDDialogFrameW32 {
public:
	VDUIDialogExtEncVideo(VDExtEncProfile& profile);
	~VDUIDialogExtEncVideo();

protected:
	void OnDataExchange(bool write);

	VDExtEncProfile& mProfile;
	HFONT fixed_font;
};

VDUIDialogExtEncVideo::VDUIDialogExtEncVideo(VDExtEncProfile& profile)
	: VDDialogFrameW32(IDD_EXTENC_EDIT_VIDEO)
	, mProfile(profile)
{
	LOGFONT f = {14,0, 0,0, FW_NORMAL, 0,0,0, ANSI_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH,};
	strcpy(f.lfFaceName,"courier new");
	fixed_font = CreateFontIndirect(&f);
}

VDUIDialogExtEncVideo::~VDUIDialogExtEncVideo() {
	DeleteObject(fixed_font);
}

void VDUIDialogExtEncVideo::OnDataExchange(bool write) {
	HWND cb = GetControl(IDC_PIXEL_FORMAT);
	if (write) {
		int sel = SendMessage(cb, CB_GETCURSEL, 0, 0);
		if (sel==0)
			mProfile.mPixelFormat = L"yuv420p";
		if (sel==1)
			mProfile.mPixelFormat = L"bgr24";
		if (sel==2)
			mProfile.mPixelFormat = L"bgra";
		if (sel==3)
			mProfile.mPixelFormat = L"bgra64le";

	} else {
		SendMessage(cb,WM_SETFONT,(WPARAM)fixed_font,0);
		SendMessage(cb,CB_RESETCONTENT,0,0);
		SendMessage(cb,CB_ADDSTRING, 0, (LPARAM)"yuv420     :   8 bit YUV 4:2:0");
		SendMessage(cb,CB_ADDSTRING, 0, (LPARAM)"bgr24      :   8 bit RGB");
		SendMessage(cb,CB_ADDSTRING, 0, (LPARAM)"bgra       :   8 bit RGBA");
		SendMessage(cb,CB_ADDSTRING, 0, (LPARAM)"bgra64le   :  16 bit RGBA");

		int sel = 0;
		if (mProfile.mPixelFormat==L"bgr24")
			sel = 1;
		if (mProfile.mPixelFormat==L"bgra")
			sel = 2;
		if (mProfile.mPixelFormat==L"bgra64le")
			sel = 3;

		SendMessage(cb,CB_SETCURSEL, sel, 0);
	}
}

///////////////////////////////////////////////////////////////////////////

class VDUIDialogExtEncProfile : public VDDialogFrameW32 {
public:
	VDUIDialogExtEncProfile(VDExtEncProfile& profile);
	~VDUIDialogExtEncProfile();

protected:
	bool OnLoaded();
	void OnDataExchange(bool write);
	void OnSize();

	void SyncAll();
	void SetActivePane(int idx);

	void OnTabChanged(VDUIProxyTabControl *source, int idx);
	void OnTypeChanged(VDUIDialogExtEncMain *source, VDExtEncType type);

	vdpoint32 mPanePos;

	VDUIDialogExtEncMain mMainPane;
	VDUIDialogExtEncAudio mAudioPane;
	VDUIDialogExtEncVideo mVideoPane;
	VDDialogResizerW32 mResizer;
	VDDelegate mDelegateTabChanged;
	VDDelegate mDelegateTypeChanged;
	VDUIProxyTabControl mTabControl;

	VDExtEncProfile& mProfile;
};

VDUIDialogExtEncProfile::VDUIDialogExtEncProfile(VDExtEncProfile& profile)
	: VDDialogFrameW32(IDD_EXTENC_EDIT)
	, mMainPane(profile)
	, mAudioPane(profile)
	, mVideoPane(profile)
	, mProfile(profile)
{
	mTabControl.OnSelectionChanged() += mDelegateTabChanged.Bind(this, &VDUIDialogExtEncProfile::OnTabChanged);
	mMainPane.OnTypeChanged() += mDelegateTypeChanged.Bind(this, &VDUIDialogExtEncProfile::OnTypeChanged);
}

VDUIDialogExtEncProfile::~VDUIDialogExtEncProfile() {
}

bool VDUIDialogExtEncProfile::OnLoaded() {
	AddProxy(&mTabControl, IDC_TABS);

	mResizer.Init(mhdlg);
	mResizer.Add(IDC_TABS, VDDialogResizerW32::kMC);
	mResizer.Add(IDOK, VDDialogResizerW32::kBR);
	mResizer.Add(IDCANCEL, VDDialogResizerW32::kBR);

	mTabControl.AddItem(L"Main");

	OnTypeChanged(&mMainPane, mProfile.mType);

	HWND hwndTabs = GetDlgItem(mhdlg, IDC_TABS);

	if (hwndTabs) {
		mMainPane.Create((VDGUIHandle)mhdlg);
		const vdrect32 r(mMainPane.GetArea());

		RECT r2 = { r.left, r.top, r.right, r.bottom };

		TabCtrl_AdjustRect(hwndTabs, TRUE, &r2);

		RECT r3;
		GetWindowRect(hwndTabs, &r3);

		vdsize32 sz(GetSize());

		sz.w += (r2.right - r2.left) - (r3.right - r3.left);
		sz.h += (r2.bottom - r2.top) - (r3.bottom - r3.top);

		SetSize(sz);

		GetWindowRect(hwndTabs, &r3);
		MapWindowPoints(NULL, mhdlg, (LPPOINT)&r3, 2);
		TabCtrl_AdjustRect(hwndTabs, FALSE, &r3);

		mPanePos = vdpoint32(r3.left, r3.top);
	}

	SetActivePane(0);

	SetFocusToControl(IDC_TABS);
	return true;
}

void VDUIDialogExtEncProfile::OnDataExchange(bool write) {
	if (write)
		SyncAll();
}

void VDUIDialogExtEncProfile::OnSize() {
	mResizer.Relayout();
}

void VDUIDialogExtEncProfile::SyncAll() {
	mMainPane.Sync(true);
	mAudioPane.Sync(true);
	mVideoPane.Sync(true);
}

void VDUIDialogExtEncProfile::SetActivePane(int idx) {
	SyncAll();

	switch(idx) {
		case 0:
			mAudioPane.Destroy();
			mVideoPane.Destroy();

			mMainPane.Create((VDGUIHandle)mhdlg);
			mMainPane.SetPosition(mPanePos);
			mMainPane.BringToFront();
			mMainPane.Show();
			break;

		case 1:
			mMainPane.Destroy();

			if (mProfile.mType == kVDExtEncType_Audio) {
				mAudioPane.Create((VDGUIHandle)mhdlg);
				mAudioPane.SetPosition(mPanePos);
				mAudioPane.BringToFront();
				mAudioPane.Show();
			}
			if (mProfile.mType == kVDExtEncType_Video) {
				mVideoPane.Create((VDGUIHandle)mhdlg);
				mVideoPane.SetPosition(mPanePos);
				mVideoPane.BringToFront();
				mVideoPane.Show();
			}
			break;
	}
}

void VDUIDialogExtEncProfile::OnTabChanged(VDUIProxyTabControl *source, int idx) {
	SetActivePane(idx);
}

void VDUIDialogExtEncProfile::OnTypeChanged(VDUIDialogExtEncMain *source, VDExtEncType type) {
	mTabControl.DeleteItem(1);

	if (type == kVDExtEncType_Audio)
		mTabControl.AddItem(L"Audio");

	if (type == kVDExtEncType_Video)
		mTabControl.AddItem(L"Video");
}

///////////////////////////////////////////////////////////////////////////

void VDUIDisplayDialogExtEncProfile(VDGUIHandle h, VDExtEncProfile& profile) {
	VDUIDialogExtEncProfile dlg(profile);

	dlg.ShowDialog(h);
}

///////////////////////////////////////////////////////////////////////////

class VDUIDialogEditExternalEncoderSet : public VDDialogFrameW32 {
public:
	VDUIDialogEditExternalEncoderSet(VDExtEncSet& eset);
	~VDUIDialogEditExternalEncoderSet();

protected:
	bool OnLoaded();
	void OnDestroy();
	void OnDataExchange(bool write);

	typedef vdfastvector<VDExtEncProfile *> Profiles;
	Profiles mVideoProfiles;
	Profiles mAudioProfiles;
	Profiles mMuxProfiles;

	VDExtEncSet& mSet;
};

VDUIDialogEditExternalEncoderSet::VDUIDialogEditExternalEncoderSet(VDExtEncSet& eset)
	: VDDialogFrameW32(IDD_EXTENC_EDIT_SET)
	, mSet(eset)
{
}

VDUIDialogEditExternalEncoderSet::~VDUIDialogEditExternalEncoderSet() {
}

bool VDUIDialogEditExternalEncoderSet::OnLoaded() {
	uint32 n = VDGetExternalEncoderProfileCount();

	CBAddString(IDC_VIDEO_ENCODER, L"(None)");
	CBAddString(IDC_AUDIO_ENCODER, L"(None)");
	CBAddString(IDC_MULTIPLEXER, L"(None)");

	vdrefptr<VDExtEncProfile> profile;
	for(uint32 i=0; i<n; ++i) {
		if (VDGetExternalEncoderProfileByIndex(i, ~profile)) {
			switch(profile->mType) {
				case kVDExtEncType_Video:
					CBAddString(IDC_VIDEO_ENCODER, profile->mName.c_str());
					mVideoProfiles.push_back(profile);
					profile.release();
					break;
				case kVDExtEncType_Audio:
					CBAddString(IDC_AUDIO_ENCODER, profile->mName.c_str());
					mAudioProfiles.push_back(profile);
					profile.release();
					break;
				case kVDExtEncType_Mux:
					CBAddString(IDC_MULTIPLEXER, profile->mName.c_str());
					mMuxProfiles.push_back(profile);
					profile.release();
					break;
			}
		}
	}

	OnDataExchange(false);
	SetFocusToControl(IDC_VIDEO_ENCODER);
	return true;
}

void VDUIDialogEditExternalEncoderSet::OnDestroy() {
	while(!mVideoProfiles.empty()) {
		mVideoProfiles.back()->Release();
		mVideoProfiles.pop_back();
	}

	while(!mAudioProfiles.empty()) {
		mAudioProfiles.back()->Release();
		mAudioProfiles.pop_back();
	}

	while(!mMuxProfiles.empty()) {
		mMuxProfiles.back()->Release();
		mMuxProfiles.pop_back();
	}
}

namespace {
	struct FindEncByIndexPred {
		FindEncByIndexPred(const wchar_t *name) : mpName(name) {}

		bool operator()(const VDExtEncProfile *profile) const {
			return profile->mName == mpName;
		}

		const wchar_t *const mpName;
	};
}

void VDUIDialogEditExternalEncoderSet::OnDataExchange(bool write) {
	ExchangeControlValueBoolCheckbox(write, IDC_MUXPARTIAL, mSet.mbProcessPartialOutput);
	ExchangeControlValueBoolCheckbox(write, IDC_USEOUTPUTASTEMP, mSet.mbUseOutputAsTemp);
	ExchangeControlValueString(write, IDC_FILEDESC, mSet.mFileDesc);
	ExchangeControlValueString(write, IDC_FILEEXT, mSet.mFileExt);

	if (write) {
		int vidIdx = CBGetSelectedIndex(IDC_VIDEO_ENCODER);
		int audIdx = CBGetSelectedIndex(IDC_AUDIO_ENCODER);
		int muxIdx = CBGetSelectedIndex(IDC_MULTIPLEXER);

		mSet.mVideoEncoder = vidIdx > 0 ? mVideoProfiles[vidIdx - 1]->mName.c_str() : L"";
		mSet.mAudioEncoder = audIdx > 0 ? mAudioProfiles[audIdx - 1]->mName.c_str() : L"";
		mSet.mMultiplexer = muxIdx > 0 ? mMuxProfiles[muxIdx - 1]->mName.c_str() : L"";
	} else {
		Profiles::const_iterator it(std::find_if(mVideoProfiles.begin(), mVideoProfiles.end(), FindEncByIndexPred(mSet.mVideoEncoder.c_str())));
		if (it != mVideoProfiles.end())
			CBSetSelectedIndex(IDC_VIDEO_ENCODER, (it - mVideoProfiles.begin()) + 1);
		else
			CBSetSelectedIndex(IDC_VIDEO_ENCODER, 0);

		it = std::find_if(mAudioProfiles.begin(), mAudioProfiles.end(), FindEncByIndexPred(mSet.mAudioEncoder.c_str()));
		if (it != mAudioProfiles.end())
			CBSetSelectedIndex(IDC_AUDIO_ENCODER, (it - mAudioProfiles.begin()) + 1);
		else
			CBSetSelectedIndex(IDC_AUDIO_ENCODER, 0);

		it = std::find_if(mMuxProfiles.begin(), mMuxProfiles.end(), FindEncByIndexPred(mSet.mMultiplexer.c_str()));
		if (it != mMuxProfiles.end())
			CBSetSelectedIndex(IDC_MULTIPLEXER, (it - mMuxProfiles.begin()) + 1);
		else
			CBSetSelectedIndex(IDC_MULTIPLEXER, 0);
	}
}

///////////////////////////////////////////////////////////////////////////

class VDUIDialogConfigureExternalEncoders : public VDDialogFrameW32 {
public:
	VDUIDialogConfigureExternalEncoders();
	~VDUIDialogConfigureExternalEncoders();

protected:
	bool OnLoaded();
	void OnDestroy();
	void OnDataExchange(bool write);
	bool OnCommand(uint32 id, uint32 extcode);

	void OnTabChanged(VDUIProxyTabControl *sender, int tabIndex);
	void OnSelChanged(VDUIProxyListView *sender, int selIndex);
	void OnDoubleClick(VDUIProxyListView *sender, int selIndex);
	void OnLabelChanged(VDUIProxyListView *sender, VDUIProxyListView::LabelChangedEvent *eventData);

	void UpdateButtonEnables();
	void ResortList();

	static LRESULT CALLBACK TabSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	class ListSetItem : public vdrefcounted<IVDUIListViewVirtualItem> {
	public:
		void GetText(int subItem, VDStringW& s) const {
			switch(subItem) {
				case 0:
					s = mpSet->mName;
					break;

				case 1:
					s = mpSet->mVideoEncoder;
					break;

				case 2:
					s = mpSet->mAudioEncoder;
					break;

				case 3:
					s = mpSet->mMultiplexer;
					break;
			}
		}

		vdrefptr<VDExtEncSet> mpSet;
	};

	class ListSetItemSorter : public IVDUIListViewVirtualComparer {
	public:
		virtual int Compare(IVDUIListViewVirtualItem *x, IVDUIListViewVirtualItem *y) {
			return vdwcsicmp(static_cast<ListSetItem *>(x)->mpSet->mName.c_str(), static_cast<ListSetItem *>(y)->mpSet->mName.c_str());
		}
	};

	class ListProfileItem : public vdrefcounted<IVDUIListViewVirtualItem> {
	public:
		void GetText(int subItem, VDStringW& s) const {
			switch(subItem) {
				case 0:
					s = mpProfile->mName;
					break;

				case 1:
					s = L"\"";
					s += mpProfile->mProgram;
					s += L"\" ";
					s += mpProfile->mCommandArguments;
					break;
			}
		}

		vdrefptr<VDExtEncProfile> mpProfile;
	};

	class ListProfileItemSorter : public IVDUIListViewVirtualComparer {
	public:
		virtual int Compare(IVDUIListViewVirtualItem *x, IVDUIListViewVirtualItem *y) {
			return vdwcsicmp(static_cast<ListProfileItem *>(x)->mpProfile->mName.c_str(), static_cast<ListProfileItem *>(y)->mpProfile->mName.c_str());
		}
	};

	bool mbSetMode;

	VDUIProxyTabControl mTabControl;
	VDUIProxyListView mListView;

	VDDelegate mDelegateTabChanged;
	VDDelegate mDelegateSelectionChanged;
	VDDelegate mDelegateDoubleClick;
	VDDelegate mDelegateLabelChanged;
};

VDUIDialogConfigureExternalEncoders::VDUIDialogConfigureExternalEncoders()
	: VDDialogFrameW32(IDD_EXTENC_SETS)
	, mbSetMode(true)
{
	mTabControl.OnSelectionChanged() += mDelegateTabChanged.Bind(this, &VDUIDialogConfigureExternalEncoders::OnTabChanged);
	mListView.OnItemSelectionChanged() += mDelegateSelectionChanged.Bind(this, &VDUIDialogConfigureExternalEncoders::OnSelChanged);
	mListView.OnItemDoubleClicked() += mDelegateDoubleClick.Bind(this, &VDUIDialogConfigureExternalEncoders::OnDoubleClick);
	mListView.OnItemLabelChanged() += mDelegateLabelChanged.Bind(this, &VDUIDialogConfigureExternalEncoders::OnLabelChanged);
}

VDUIDialogConfigureExternalEncoders::~VDUIDialogConfigureExternalEncoders() {
}

bool VDUIDialogConfigureExternalEncoders::OnLoaded() {
	AddProxy(&mTabControl, IDC_TABS);
	AddProxy(&mListView, IDC_LIST);

	HWND hwndTC = mTabControl.GetHandle();
	SetWindowLongPtr(hwndTC, GWLP_USERDATA, GetWindowLongPtr(hwndTC, GWLP_WNDPROC));
	SetWindowLongPtr(hwndTC, GWLP_WNDPROC, (LONG_PTR)TabSubclassProc);

	mListView.SetFullRowSelectEnabled(true);
	mListView.InsertColumn(0, L"Name", 10);

	mTabControl.AddItem(L"Encoder Sets");
	mTabControl.AddItem(L"Encoders");

	const vdrect32 r(mTabControl.GetContentArea());
	mListView.SetArea(r);

	OnDataExchange(false);
	SetFocusToControl(IDC_LIST);
	return true;
}

void VDUIDialogConfigureExternalEncoders::OnDestroy() {
	VDSaveExternalEncoderProfiles();

	mListView.Clear();
	
	VDDialogFrameW32::OnDestroy();
}

void VDUIDialogConfigureExternalEncoders::OnDataExchange(bool write) {
	if (!write) {
		mListView.Clear();
		mListView.ClearExtraColumns();

		if (mbSetMode) {
			mListView.InsertColumn(1, L"Video Encoder", 10);
			mListView.InsertColumn(2, L"Audio Encoder", 10);
			mListView.InsertColumn(3, L"Multiplexer", 10);

			uint32 n = VDGetExternalEncoderSetCount();

			for(uint32 i=0; i<n; ++i) {
				vdrefptr<ListSetItem> lsi(new ListSetItem);
				if (VDGetExternalEncoderSetByIndex(i, ~lsi->mpSet))
					mListView.InsertVirtualItem(i, lsi);
			}
		} else {
			mListView.InsertColumn(1, L"Command Line", 10);

			uint32 n = VDGetExternalEncoderProfileCount();

			for(uint32 i=0; i<n; ++i) {
				vdrefptr<ListProfileItem> lpi(new ListProfileItem);
				if (VDGetExternalEncoderProfileByIndex(i, ~lpi->mpProfile))
					mListView.InsertVirtualItem(i, lpi);
			}
		}

		mListView.SetRedraw(false);
		mListView.AutoSizeColumns();

		ResortList();
		mListView.SetRedraw(true);

		UpdateButtonEnables();
	}
}

bool VDUIDialogConfigureExternalEncoders::OnCommand(uint32 id, uint32 extcode) {
	try {
		if (id == IDC_ADD) {
			uint32 n = mListView.GetItemCount();

			if (mbSetMode) {
				vdrefptr<ListSetItem> lsi(new ListSetItem);
				lsi->mpSet = new VDExtEncSet;

				uint32 counter = n;
				do {
					lsi->mpSet->mName.sprintf(L"Encoder Set %u", ++counter);
				} while(VDGetExternalEncoderSetByName(lsi->mpSet->mName.c_str(), NULL));

				mListView.InsertVirtualItem(n, lsi);
				VDAddExternalEncoderSet(lsi->mpSet);
			} else {
				vdrefptr<ListProfileItem> lpi(new ListProfileItem);
				lpi->mpProfile = new VDExtEncProfile;

				uint32 counter = n;
				do {
					lpi->mpProfile->mName.sprintf(L"Encoder Profile %u", ++counter);
				} while(VDGetExternalEncoderProfileByName(lpi->mpProfile->mName.c_str(), NULL));

				mListView.InsertVirtualItem(n, lpi);
				VDAddExternalEncoderProfile(lpi->mpProfile);
			}

			SetFocusToControl(IDC_LIST);
			mListView.EnsureItemVisible(n);
			mListView.AutoSizeColumns();
			mListView.EditItemLabel(n);
			return true;
		} else if (id == IDC_REMOVE) {
			int idx = mListView.GetSelectedIndex();

			if (idx >= 0) {
				if (mbSetMode) {
					ListSetItem *lsi = static_cast<ListSetItem *>(mListView.GetVirtualItem(idx));
					if (lsi) {
						VDRemoveExternalEncoderSet(lsi->mpSet);
						mListView.DeleteItem(idx);
					}
				} else {
					ListProfileItem *lpi = static_cast<ListProfileItem *>(mListView.GetVirtualItem(idx));
					if (lpi) {
						VDRemoveExternalEncoderProfile(lpi->mpProfile);
						mListView.DeleteItem(idx);
					}
				}

				int remaining = mListView.GetItemCount();

				if (remaining) {
					if (idx == remaining)
						--idx;

					mListView.SetSelectedIndex(idx);
					mListView.EnsureItemVisible(idx);
				}
			}

			return true;
		} else if (id == IDC_EDIT) {
			int idx = mListView.GetSelectedIndex();

			if (idx >= 0) {
				if (mbSetMode) {
					ListSetItem *lsi = static_cast<ListSetItem *>(mListView.GetVirtualItem(idx));

					if (lsi) {
						VDUIDialogEditExternalEncoderSet dlg(*lsi->mpSet);
						dlg.ShowDialog((VDGUIHandle)mhdlg);
					}
				} else {
					ListProfileItem *lpi = static_cast<ListProfileItem *>(mListView.GetVirtualItem(idx));
					if (lpi) {
						VDUIDialogExtEncProfile dlg(*lpi->mpProfile);

						dlg.ShowDialog((VDGUIHandle)mhdlg);
					}
				}

				mListView.RefreshItem(idx);
				mListView.AutoSizeColumns();
			}

			return true;
		} else if (id == IDC_DUPLICATE) {
			int idx = mListView.GetSelectedIndex();

			if (idx >= 0) {
				uint32 n = mListView.GetItemCount();

				mListView.SetSelectedIndex(-1);

				if (mbSetMode) {
					ListSetItem *lsiprev = static_cast<ListSetItem *>(mListView.GetVirtualItem(idx));

					if (lsiprev) {
						vdrefptr<ListSetItem> lsi(new ListSetItem);
						lsi->mpSet = new VDExtEncSet(*lsiprev->mpSet);

						uint32 counter = n;
						do {
							lsi->mpSet->mName.sprintf(L"Encoder Set %u", ++counter);
						} while(VDGetExternalEncoderSetByName(lsi->mpSet->mName.c_str(), NULL));

						mListView.InsertVirtualItem(n, lsi);
						VDAddExternalEncoderSet(lsi->mpSet);
					}
				} else {
					ListProfileItem *lpiprev = static_cast<ListProfileItem *>(mListView.GetVirtualItem(idx));

					if (lpiprev) {
						vdrefptr<ListProfileItem> lpi(new ListProfileItem);
						lpi->mpProfile = new VDExtEncProfile(*lpiprev->mpProfile);

						uint32 counter = n;
						do {
							lpi->mpProfile->mName.sprintf(L"Encoder Profile %u", ++counter);
						} while(VDGetExternalEncoderProfileByName(lpi->mpProfile->mName.c_str(), NULL));

						mListView.InsertVirtualItem(n, lpi);
						VDAddExternalEncoderProfile(lpi->mpProfile);
					}
				}

				SetFocusToControl(IDC_LIST);
				mListView.EnsureItemVisible(n);
				mListView.AutoSizeColumns();
				mListView.EditItemLabel(n);
			}
			return true;
		} else if (id == IDC_IMPORT) {
			const VDStringW fn(VDGetLoadFileName('impo', (VDGUIHandle)mhdlg, L"Import external encoder profiles", L"External encoder profiles (*.vdprof)\0*.vdprof\0", L"vdprof"));

			if (fn.empty())
				return true;

			VDStringW importDir(VDFileSplitPathLeft(VDGetFullPath(fn.c_str())));

			VDJSONReader reader;
			VDJSONDocument doc;

			vdfastvector<char> buf;
			{
				VDFile f(fn.c_str());

				buf.resize(VDClampToSint32(f.size()));
				f.read(buf.data(), buf.size());
			}

			if (!reader.Parse(buf.data(), buf.size(), doc)) {
				int line, offset;
				reader.GetErrorLocation(line, offset);
				throw MyError("Unable to read profiles: parse error in file at line %d, offset %d.", line, offset);
			}

			const VDJSONValueRef& root = doc.Root();

			const VDJSONValueRef& encoders = root["externalEncoders"];
			if (!encoders.IsValid())
				throw MyError("The profile file does not contain any external encoder profiles.");

			vdvector<vdrefptr<VDExtEncSet > > esets;
			vdvector<vdrefptr<VDExtEncProfile > > eprofs;

			bool overwriteConfirmed = false;

			const VDJSONValueRef& sets = encoders["sets"];
			if (sets.IsValid()) {
				for(VDJSONMemberEnum setEnum(sets.AsObject()); setEnum.IsValid(); ++setEnum) {
					const VDJSONValueRef& setInfo = setEnum.GetValue();
					if (!setInfo.IsValid())
						continue;

					vdrefptr<VDExtEncSet> eset(new VDExtEncSet);
					const VDJSONValueRef& videoEncoder = setInfo["videoEncoder"];
					const VDJSONValueRef& audioEncoder = setInfo["audioEncoder"];
					const VDJSONValueRef& multiplexer = setInfo["multiplexer"];
					const VDJSONValueRef& description = setInfo["description"];
					const VDJSONValueRef& extension = setInfo["extension"];
					const VDJSONValueRef& processPartial = setInfo["processPartial"];
					const VDJSONValueRef& useOutputAsTemp = setInfo["useOutputAsTemp"];

					eset->mName = setEnum.GetName();

					if (!overwriteConfirmed && VDGetExternalEncoderSetByName(eset->mName.c_str(), NULL)) {
						VDStringW message;

						message.sprintf(L"The file contains an external encoder set called \"%ls\" that already exists. Do you want to load the file anyway, replacing existing profiles with the same names?", eset->mName.c_str());

						if (!Confirm(message.c_str(), L"VirtualDub Warning"))
							throw MyUserAbortError();

						overwriteConfirmed = true;
					}

					if (videoEncoder.IsValid())
						eset->mVideoEncoder = videoEncoder.AsString();

					if (audioEncoder.IsValid())
						eset->mAudioEncoder = audioEncoder.AsString();

					if (multiplexer.IsValid())
						eset->mMultiplexer = multiplexer.AsString();

					if (description.IsValid())
						eset->mFileDesc = description.AsString();

					if (extension.IsValid())
						eset->mFileExt = extension.AsString();

					if (processPartial.IsValid())
						eset->mbProcessPartialOutput = processPartial.AsBool();

					if (useOutputAsTemp.IsValid())
						eset->mbUseOutputAsTemp = useOutputAsTemp.AsBool();

					esets.push_back(eset);
				}
			}

			const VDJSONValueRef& profs = encoders["profiles"];
			if (profs.IsValid()) {
				for(VDJSONMemberEnum profEnum(profs.AsObject()); profEnum.IsValid(); ++profEnum) {
					const VDJSONValueRef& profInfo = profEnum.GetValue();
					if (!profInfo.IsValid())
						continue;

					vdrefptr<VDExtEncProfile> eprof(new VDExtEncProfile);
					const VDJSONValueRef& program = profInfo["program"];
					const VDJSONValueRef& commandArguments = profInfo["commandArguments"];
					const VDJSONValueRef& outputFilename = profInfo["outputFilename"];
					const VDJSONValueRef& type = profInfo["type"];
					const VDJSONValueRef& inputFormat = profInfo["inputFormat"];
					const VDJSONValueRef& checkReturnCode = profInfo["checkReturnCode"];
					const VDJSONValueRef& logStdout = profInfo["logStdout"];
					const VDJSONValueRef& logStderr = profInfo["logStderr"];
					const VDJSONValueRef& bypassCompression = profInfo["bypassCompression"];
					const VDJSONValueRef& predeleteOutputFile = profInfo["predeleteOutputFile"];

					eprof->mName = profEnum.GetName();

					if (!overwriteConfirmed && VDGetExternalEncoderProfileByName(eprof->mName.c_str(), NULL)) {
						VDStringW message;

						message.sprintf(L"The file contains an external encoder profile called \"%ls\" that already exists. Do you want to load the file anyway, replacing existing profiles with the same names?", eprof->mName.c_str());

						if (!Confirm(message.c_str(), L"VirtualDub Warning"))
							throw MyUserAbortError();

						overwriteConfirmed = true;
					}

					if (program.IsValid()) {
						const wchar_t *programStr = program.AsString();

						if (VDFileIsRelativePath(programStr))
							eprof->mProgram = VDFileGetCanonicalPath(VDMakePath(importDir.c_str(), programStr).c_str());
						else
							eprof->mProgram = programStr;
					}

					if (commandArguments.IsValid())
						eprof->mCommandArguments = commandArguments.AsString();

					if (outputFilename.IsValid())
						eprof->mOutputFilename = outputFilename.AsString();

					if (type.IsValid())
						eprof->mType = (VDExtEncType)type.AsInt64();

					if (inputFormat.IsValid())
						eprof->mInputFormat = (VDExtEncInputFormat)inputFormat.AsInt64();

					if (checkReturnCode.IsValid())
						eprof->mbCheckReturnCode = checkReturnCode.AsBool();

					if (logStdout.IsValid())
						eprof->mbLogStdout = logStdout.AsBool();

					if (logStderr.IsValid())
						eprof->mbLogStderr = logStderr.AsBool();

					if (predeleteOutputFile.IsValid())
						eprof->mbPredeleteOutputFile = predeleteOutputFile.AsBool();

					if (bypassCompression.IsValid())
						eprof->mbBypassCompression = bypassCompression.AsBool();

					eprofs.push_back(eprof);
				}
			}

			// alright, add them all!

			while(!eprofs.empty()) {
				VDExtEncProfile *eprof = &*eprofs.back();

				vdrefptr<VDExtEncProfile> conflict;
				if (VDGetExternalEncoderProfileByName(eprof->mName.c_str(), ~conflict))
					VDRemoveExternalEncoderProfile(conflict);

				VDAddExternalEncoderProfile(eprof);
				eprofs.pop_back();
			}

			while(!esets.empty()) {
				VDExtEncSet *eset = &*esets.back();
				vdrefptr<VDExtEncSet> conflict;
				
				if (VDGetExternalEncoderSetByName(eset->mName.c_str(), ~conflict))
					VDRemoveExternalEncoderSet(conflict);

				VDAddExternalEncoderSet(eset);
				esets.pop_back();
			}

			OnDataExchange(false);
			return true;

		} else if (id == IDC_EXPORT) {
			const VDStringW fn(VDGetSaveFileName('expo', (VDGUIHandle)mhdlg, L"Export external encoder profiles", L"External encoder profiles (*.vdprof)\0*.vdprof\0", L"vdprof"));

			if (fn.empty())
				return true;

			VDStringW exportDir(VDFileSplitPathLeft(VDGetFullPath(fn.c_str())));

			VDFileStream fs(fn.c_str(), nsVDFile::kWrite | nsVDFile::kDenyRead | nsVDFile::kCreateAlways);
			JSONStreamWriter streamWriter(&fs);
			VDJSONWriter writer;

			writer.Begin(&streamWriter);
			writer.OpenObject();
			writer.WriteMemberName(L"description");
			writer.WriteString(L"VirtualDub external encoder profile collection");

				writer.WriteMemberName(L"externalEncoders");
				writer.OpenObject();

					writer.WriteMemberName(L"sets");
					writer.OpenObject();

						uint32 setCount = VDGetExternalEncoderSetCount();

						for(uint32 setIdx = 0; setIdx < setCount; ++setIdx)
						{
							vdrefptr<VDExtEncSet> eset;

							if (VDGetExternalEncoderSetByIndex(setIdx, ~eset)) {
								writer.WriteMemberName(eset->mName.c_str());
								writer.OpenObject();
									writer.WriteMemberName(L"videoEncoder");
									writer.WriteString(eset->mVideoEncoder.c_str());

									writer.WriteMemberName(L"audioEncoder");
									writer.WriteString(eset->mAudioEncoder.c_str());

									writer.WriteMemberName(L"multiplexer");
									writer.WriteString(eset->mMultiplexer.c_str());

									writer.WriteMemberName(L"description");
									writer.WriteString(eset->mFileDesc.c_str());

									writer.WriteMemberName(L"extension");
									writer.WriteString(eset->mFileExt.c_str());

									writer.WriteMemberName(L"processPartial");
									writer.WriteBool(eset->mbProcessPartialOutput);

									writer.WriteMemberName(L"useOutputAsTemp");
									writer.WriteBool(eset->mbUseOutputAsTemp);
								writer.Close();
							}
						}

					writer.Close();

					writer.WriteMemberName(L"profiles");
					writer.OpenObject();

						uint32 profCount = VDGetExternalEncoderProfileCount();

						for(uint32 profIdx = 0; profIdx < profCount; ++profIdx)
						{
							vdrefptr<VDExtEncProfile> eprof;

							if (VDGetExternalEncoderProfileByIndex(profIdx, ~eprof)) {
								writer.WriteMemberName(eprof->mName.c_str());
								writer.OpenObject();
									writer.WriteMemberName(L"name");
									writer.WriteString(eprof->mName.c_str());

									writer.WriteMemberName(L"program");

									VDStringW rel(VDFileGetRelativePath(exportDir.c_str(), eprof->mProgram.c_str(), false));
									if (rel.empty())
										writer.WriteString(eprof->mProgram.c_str());
									else
										writer.WriteString(rel.c_str());

									writer.WriteMemberName(L"commandArguments");
									writer.WriteString(eprof->mCommandArguments.c_str());

									writer.WriteMemberName(L"outputFilename");
									writer.WriteString(eprof->mOutputFilename.c_str());

									writer.WriteMemberName(L"type");
									writer.WriteInt((sint64)eprof->mType);

									writer.WriteMemberName(L"inputFormat");
									writer.WriteInt((sint64)eprof->mInputFormat);

									writer.WriteMemberName(L"checkReturnCode");
									writer.WriteBool(eprof->mbCheckReturnCode);

									writer.WriteMemberName(L"logStdout");
									writer.WriteBool(eprof->mbLogStdout);

									writer.WriteMemberName(L"logStderr");
									writer.WriteBool(eprof->mbLogStderr);

									writer.WriteMemberName(L"bypassCompression");
									writer.WriteBool(eprof->mbBypassCompression);

									writer.WriteMemberName(L"predeleteOutputFile");
									writer.WriteBool(eprof->mbPredeleteOutputFile);
								writer.Close();
							}
						}

					writer.Close();
				writer.Close();
			writer.Close();
			
			writer.End();
			return true;
		}
	} catch(const MyError& e) {
		e.post(mhdlg, g_szError);
		return true;
	}

	return false;
}

void VDUIDialogConfigureExternalEncoders::OnTabChanged(VDUIProxyTabControl *sender, int tabIndex) {
	bool setMode = tabIndex == 0;

	if (setMode != mbSetMode) {
		mbSetMode = setMode;
		OnDataExchange(false);
	}
}

void VDUIDialogConfigureExternalEncoders::OnSelChanged(VDUIProxyListView *sender, int tabIndex) {
	UpdateButtonEnables();
}

void VDUIDialogConfigureExternalEncoders::OnDoubleClick(VDUIProxyListView *sender, int selIndex) {
	OnCommand(IDC_EDIT, 0);
}

void VDUIDialogConfigureExternalEncoders::OnLabelChanged(VDUIProxyListView *sender, VDUIProxyListView::LabelChangedEvent *eventData) {
	if (mbSetMode) {
		ListSetItem *lsi = static_cast<ListSetItem *>(mListView.GetVirtualItem(eventData->mIndex));

		if (lsi) {
			vdrefptr<VDExtEncSet> conflictingSet;
			VDGetExternalEncoderSetByName(eventData->mpNewLabel, ~conflictingSet);

			if (conflictingSet && conflictingSet != lsi->mpSet) {
				VDStringW msg;

				msg.sprintf(L"The name \"%ls\" is already in use by another set.", eventData->mpNewLabel);
				ShowError(msg.c_str(), g_szErrorW);
				return;
			}

			lsi->mpSet->mName = eventData->mpNewLabel;
		}
	} else {
		ListProfileItem *lpi = static_cast<ListProfileItem *>(mListView.GetVirtualItem(eventData->mIndex));
		if (lpi) {
			vdrefptr<VDExtEncProfile> conflictingProfile;
			VDGetExternalEncoderProfileByName(eventData->mpNewLabel, ~conflictingProfile);

			if (conflictingProfile && conflictingProfile != lpi->mpProfile) {
				VDStringW msg;

				msg.sprintf(L"The name \"%ls\" is already in use by another profile.", eventData->mpNewLabel);
				ShowError(msg.c_str(), g_szErrorW);
				return;
			}

			// Run through all existing sets and rename any references.
			uint32 n = VDGetExternalEncoderSetCount();
			for(uint32 i=0; i<n; ++i) {
				vdrefptr<VDExtEncSet> eset;
				if (!VDGetExternalEncoderSetByIndex(i, ~eset))
					continue;

				if (eset->mVideoEncoder == lpi->mpProfile->mName)
					eset->mVideoEncoder = eventData->mpNewLabel;

				if (eset->mAudioEncoder == lpi->mpProfile->mName)
					eset->mAudioEncoder = eventData->mpNewLabel;

				if (eset->mMultiplexer == lpi->mpProfile->mName)
					eset->mMultiplexer = eventData->mpNewLabel;
			}

			lpi->mpProfile->mName = eventData->mpNewLabel;
		}
	}

	mListView.SetRedraw(false);
	mListView.RefreshItem(eventData->mIndex);
	mListView.AutoSizeColumns();

	ResortList();
	mListView.SetRedraw(true);
}

void VDUIDialogConfigureExternalEncoders::UpdateButtonEnables() {
	bool selPresent = mListView.GetSelectedIndex() >= 0;

	EnableControl(IDC_REMOVE, selPresent);
	EnableControl(IDC_EDIT, selPresent);
	EnableControl(IDC_DUPLICATE, selPresent);
}

void VDUIDialogConfigureExternalEncoders::ResortList() {
	if (mbSetMode) {
		ListSetItemSorter sorter;
		mListView.Sort(sorter);
	} else {
		ListProfileItemSorter sorter;
		mListView.Sort(sorter);
	}
}

LRESULT CALLBACK VDUIDialogConfigureExternalEncoders::TabSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	WNDPROC wp = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	if (msg == WM_ERASEBKGND) {
		HWND hwndParent = GetParent(hwnd);

		if (hwndParent) {
			HWND hwndLV = GetDlgItem(hwndParent, IDC_LIST);

			if (hwndLV) {
				HDC hdc = (HDC)wParam;

				RECT r;
				GetWindowRect(hwndLV, &r);
				MapWindowPoints(NULL, hwnd, (LPPOINT)&r, 2);
				ExcludeClipRect(hdc, r.left, r.top, r.right, r.bottom);
				GetClientRect(hwnd, &r);
				FillRect(hdc, &r, (HBRUSH)(COLOR_3DFACE + 1));
		
				return TRUE;
			}
		}
	}

	return CallWindowProc(wp, hwnd, msg, wParam, lParam);
}

void VDUIDisplayDialogConfigureExternalEncoders(VDGUIHandle h) {
	VDUIDialogConfigureExternalEncoders dlg;

	dlg.ShowDialog(h);
}

