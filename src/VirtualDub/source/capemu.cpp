//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2004 Avery Lee
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
#include <vd2/VDCapture/capdriver.h>
#include <vd2/Dita/services.h>
#include <vd2/system/atomic.h>
#include <vd2/system/error.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/vdstring.h>
#include <vd2/system/refcount.h>
#include <vd2/system/registry.h>
#include <vd2/system/time.h>
#include <vd2/system/profile.h>
#include <vd2/system/w32assist.h>
#include <vd2/system/VDRingBuffer.h>
#include <vd2/VDDisplay/display.h>
#include <vd2/Riza/audioout.h>
#include "InputFile.h"
#include "VideoSource.h"
#include "AudioSource.h"

using namespace nsVDCapture;

extern HINSTANCE g_hInst;

extern const char g_szError[];

const VDStringW& VDPreferencesGetAudioPlaybackDeviceKey();

class VDCaptureDriverEmulation : public IVDCaptureDriver, public IVDTimerCallback {
	VDCaptureDriverEmulation(const VDCaptureDriverEmulation&);
	VDCaptureDriverEmulation& operator=(const VDCaptureDriverEmulation&);
public:
	VDCaptureDriverEmulation();
	~VDCaptureDriverEmulation();

	void	*AsInterface(uint32 id) { return NULL; }

	bool	Init(VDGUIHandle hParent);
	void	Shutdown();

	void	SetCallback(IVDCaptureDriverCallback *pCB);

	void	LockUpdates() {}
	void	UnlockUpdates() {}

	bool	IsHardwareDisplayAvailable();

	void	SetDisplayMode(nsVDCapture::DisplayMode m);
	nsVDCapture::DisplayMode		GetDisplayMode();

	void	SetDisplayRect(const vdrect32& r);
	vdrect32	GetDisplayRectAbsolute();
	void	SetDisplayVisibility(bool vis);

	void	SetFramePeriod(sint32 framePeriod100nsUnits);
	sint32	GetFramePeriod();

	uint32	GetPreviewFrameCount();

	bool	GetVideoFormat(vdstructex<BITMAPINFOHEADER>& vformat);
	bool	SetVideoFormat(const BITMAPINFOHEADER *pbih, uint32 size);

	bool	SetTunerChannel(int channel) { return false; }
	int		GetTunerChannel() { return -1; }
	bool	GetTunerChannelRange(int& minChannel, int& maxChannel) { return false; }
	uint32	GetTunerFrequencyPrecision() { return 0; }
	uint32	GetTunerExactFrequency() { return 0; }
	bool	SetTunerExactFrequency(uint32 freq) { return false; }
	nsVDCapture::TunerInputMode	GetTunerInputMode() { return kTunerInputUnknown; }
	void	SetTunerInputMode(nsVDCapture::TunerInputMode tunerMode) {}

	int		GetAudioDeviceCount();
	const wchar_t *GetAudioDeviceName(int idx);
	bool	SetAudioDevice(int idx);
	int		GetAudioDeviceIndex();
	bool	IsAudioDeviceIntegrated(int idx) { return false; }

	int		GetVideoSourceCount();
	const wchar_t *GetVideoSourceName(int idx);
	bool	SetVideoSource(int idx);
	int		GetVideoSourceIndex();

	int		GetAudioSourceCount();
	const wchar_t *GetAudioSourceName(int idx);
	bool	SetAudioSource(int idx);
	int		GetAudioSourceIndex();

	int		GetAudioSourceForVideoSource(int idx) { return -2; }

	int		GetAudioInputCount();
	const wchar_t *GetAudioInputName(int idx);
	bool	SetAudioInput(int idx);
	int		GetAudioInputIndex();

	bool	IsAudioCapturePossible();
	bool	IsAudioCaptureEnabled();
	bool	IsAudioPlaybackPossible() { return false; }
	bool	IsAudioPlaybackEnabled() { return false; }
	void	SetAudioCaptureEnabled(bool b);
	void	SetAudioAnalysisEnabled(bool b);
	void	SetAudioPlaybackEnabled(bool b) {}

	void	GetAvailableAudioFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats);

	bool	GetAudioFormat(vdstructex<WAVEFORMATEX>& aformat);
	bool	SetAudioFormat(const WAVEFORMATEX *pwfex, uint32 size);

	bool	IsDriverDialogSupported(nsVDCapture::DriverDialog dlg);
	void	DisplayDriverDialog(nsVDCapture::DriverDialog dlg);

	bool	IsPropertySupported(uint32 id) { return false; }
	sint32	GetPropertyInt(uint32 id, bool *pAutomatic) { if (pAutomatic) *pAutomatic = true; return 0; }
	void	SetPropertyInt(uint32 id, sint32 value, bool automatic) {}
	void	GetPropertyInfoInt(uint32 id, sint32& minVal, sint32& maxVal, sint32& step, sint32& defaultVal, bool& automatic, bool& manual) {}

	bool	CaptureStart();
	void	CaptureStop();
	void	CaptureAbort();

protected:
	void	UpdateDisplayMode();
	void	CloseInputFile();
	void	OpenInputFile(const wchar_t *fn, IVDInputDriver* pDriver);
	void	TimerCallback();
	void	OnTick();

	static LRESULT CALLBACK StaticMessageWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	HWND	mhwnd;
	HWND	mhwndParent;
	HWND	mhwndMessages;
	IVDCaptureDriverCallback	*mpCB;
	bool	mbAudioCaptureEnabled;
	bool	mbAudioAnalysisEnabled;
	bool	mbCapturing;
	VDAtomicInt	mDisplayMode;

	vdrefptr<InputFile>	mpInputFile;
	vdrefptr<IVDVideoSource> mpVideo;
	vdrefptr<AudioSource> mpAudio;
	VDPosition		mAudioPos;
	VDPosition		mLastDisplayedVideoFrame;

	IVDVideoDisplay	*mpDisplay;

	VDCallbackTimer	mFrameTimer;
	VDPosition		mFrame;
	VDPosition		mFrameCount;

	VDTime			mCaptureStart;
	VDPosition		mLastCapturedFrame;

	vdautoptr<IVDAudioOutput>	mpAudioOutput;
	UINT			mAudioTimer;
	uint32			mAudioSampleSize;
	uint32			mAudioSampleRate;
	uint32			mAudioClientPosition;
	vdblock<char>	mAudioClientBuffer;
	VDRingBuffer<char>	mAudioRecordBuffer;

	VDAtomicInt		mPreviewFrameCount;
	MyError			mCaptureError;

	static ATOM sMsgWndClass;
};

ATOM VDCaptureDriverEmulation::sMsgWndClass = NULL;

VDCaptureDriverEmulation::VDCaptureDriverEmulation()
	: mhwnd(NULL)
	, mhwndParent(NULL)
	, mhwndMessages(NULL)
	, mpCB(NULL)
	, mbAudioCaptureEnabled(true)
	, mbAudioAnalysisEnabled(false)
	, mbCapturing(false)
	, mDisplayMode(kDisplayNone)
	, mLastDisplayedVideoFrame(-1)
	, mpVideo(NULL)
	, mpAudio(NULL)
	, mpDisplay(NULL)
	, mpAudioOutput(VDCreateAudioOutputWaveOutW32())
	, mAudioTimer(0)
{
}

VDCaptureDriverEmulation::~VDCaptureDriverEmulation() {
	Shutdown();
}

void VDCaptureDriverEmulation::SetCallback(IVDCaptureDriverCallback *pCB) {
	mpCB = pCB;
}

bool VDCaptureDriverEmulation::Init(VDGUIHandle hParent) {
	if (!sMsgWndClass) {
		WNDCLASS wc;

		wc.style			= 0;
		wc.lpfnWndProc		= StaticMessageWndProc;
		wc.cbClsExtra		= 0;
		wc.cbWndExtra		= sizeof(void *);
		wc.hInstance		= g_hInst;
		wc.hIcon			= NULL;
		wc.hCursor			= NULL;
		wc.hbrBackground	= NULL;
		wc.lpszMenuName		= NULL;
		wc.lpszClassName	= "RizaAudioEmulator";

		sMsgWndClass = RegisterClass(&wc);
		if (!sMsgWndClass)
			return false;
	}

	mhwndParent = (HWND)hParent;

	mhwndMessages = CreateWindowEx(0, (LPCTSTR)sMsgWndClass, "", WS_POPUP, 0, 0, 0, 0, NULL, NULL, g_hInst, NULL);
	if (!mhwndMessages)
		return false;

	SetWindowLongPtr(mhwndMessages, 0, (LONG_PTR)this);

	mhwnd = (HWND)VDCreateDisplayWindowW32(0, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0, 0, 64, 64, (VDGUIHandle)mhwndParent);
	if (!mhwnd) {
		DestroyWindow(mhwndMessages);
		return false;
	}

	mpDisplay = VDGetIVideoDisplay((VDGUIHandle)mhwnd);
	mPreviewFrameCount = 0;

	VDRegistryAppKey key("Capture");
	VDStringW filename;

	if (GetAsyncKeyState(VK_SHIFT) >= 0) {
		if (key.getString("Emulation file", filename)) {
			try {
				OpenInputFile(filename.c_str(), 0);
			} catch(const MyError&) {
				// silently eat the error
			}
		}
	}

	return true;
}

void VDCaptureDriverEmulation::Shutdown() {
	CloseInputFile();

	if (mpDisplay) {
		mpDisplay->Destroy();
		mpDisplay = NULL;
	}

	if (mhwndMessages) {
		DestroyWindow(mhwndMessages);
		mhwndMessages = NULL;
	}
}

bool VDCaptureDriverEmulation::IsHardwareDisplayAvailable() {
	return false;
}

void VDCaptureDriverEmulation::SetDisplayMode(DisplayMode mode) {
	if (mode == mDisplayMode)
		return;

	mDisplayMode = mode;

	UpdateDisplayMode();
}

DisplayMode VDCaptureDriverEmulation::GetDisplayMode() {
	return (DisplayMode)(int)mDisplayMode;
}

void VDCaptureDriverEmulation::SetDisplayRect(const vdrect32& r) {
	SetWindowPos(mhwnd, NULL, r.left, r.top, r.width(), r.height(), SWP_NOZORDER|SWP_NOACTIVATE);
}

vdrect32 VDCaptureDriverEmulation::GetDisplayRectAbsolute() {
	RECT r;
	GetWindowRect(mhwnd, &r);
	MapWindowPoints(GetParent(mhwnd), NULL, (LPPOINT)&r, 2);
	return vdrect32(r.left, r.top, r.right, r.bottom);
}

void VDCaptureDriverEmulation::SetDisplayVisibility(bool vis) {
	ShowWindow(mhwnd, vis ? SW_SHOWNA : SW_HIDE);
}

void VDCaptureDriverEmulation::SetFramePeriod(sint32 framePeriod100nsUnits) {
}

sint32 VDCaptureDriverEmulation::GetFramePeriod() {
	return mpVideo ? (sint32)mpVideo->asStream()->getRate().scale64ir(10000000) : 10000000 / 15;
}

uint32 VDCaptureDriverEmulation::GetPreviewFrameCount() {
	return mPreviewFrameCount;
}

bool VDCaptureDriverEmulation::GetVideoFormat(vdstructex<BITMAPINFOHEADER>& vformat) {
	if (!mpVideo) {
		vformat.resize(sizeof(BITMAPINFOHEADER));
		vformat->biSize = sizeof(BITMAPINFOHEADER);
		vformat->biWidth = 320;
		vformat->biHeight = 240;
		vformat->biPlanes = 1;
		vformat->biBitCount = 32;
		vformat->biSizeImage = 320*240*4;
		vformat->biCompression = BI_RGB;
		vformat->biXPelsPerMeter = 0;
		vformat->biYPelsPerMeter = 0;
		vformat->biClrUsed = 0;
		vformat->biClrImportant = 0;
		return true;
	} else {
		const BITMAPINFOHEADER *format = (const BITMAPINFOHEADER *)mpVideo->getDecompressedFormat();
		if (!format) return false;

		vformat.assign(format, VDGetSizeOfBitmapHeaderW32(format));
		return true;
	}
}

bool VDCaptureDriverEmulation::SetVideoFormat(const BITMAPINFOHEADER *pbih, uint32 size) {
	if (mpVideo) {
		mpDisplay->Reset();
		mLastDisplayedVideoFrame = -1;
		bool success = mpVideo->setDecompressedFormat((const VDAVIBitmapInfoHeader *)pbih);
		UpdateDisplayMode();
		if (mpCB)
			mpCB->CapEvent(kEventVideoFormatChanged, 0);
		return success;
	}
	return false;
}

int VDCaptureDriverEmulation::GetAudioDeviceCount() {
	return mpAudio != 0;
}

const wchar_t *VDCaptureDriverEmulation::GetAudioDeviceName(int idx) {
	return mpAudio && !idx ? L"Audio stream" : NULL;
}

bool VDCaptureDriverEmulation::SetAudioDevice(int idx) {
	// ignored
	return idx == 0;
}

int VDCaptureDriverEmulation::GetAudioDeviceIndex() {
	return mpAudio != 0 ? 0 : -1;
}

int VDCaptureDriverEmulation::GetVideoSourceCount() {
	return 0;
}

const wchar_t *VDCaptureDriverEmulation::GetVideoSourceName(int idx) {
	return NULL;
}

bool VDCaptureDriverEmulation::SetVideoSource(int idx) {
	return idx == -1;
}

int VDCaptureDriverEmulation::GetVideoSourceIndex() {
	return -1;
}

int VDCaptureDriverEmulation::GetAudioSourceCount() {
	return 0;
}

const wchar_t *VDCaptureDriverEmulation::GetAudioSourceName(int idx) {
	return NULL;
}

bool VDCaptureDriverEmulation::SetAudioSource(int idx) {
	return idx==-1;
}

int VDCaptureDriverEmulation::GetAudioSourceIndex() {
	return -1;
}

int VDCaptureDriverEmulation::GetAudioInputCount() {
	return mpAudio != 0 ? 1 : 0;
}

const wchar_t *VDCaptureDriverEmulation::GetAudioInputName(int idx) {
	return !idx && mpAudio != 0 ? L"Audio stream" : NULL;
}

bool VDCaptureDriverEmulation::SetAudioInput(int idx) {
	if (!mpAudio || idx)
		return false;

	return true;
}

int VDCaptureDriverEmulation::GetAudioInputIndex() {
	return mpAudio != 0 ? -1 : 0;
}

bool VDCaptureDriverEmulation::IsAudioCapturePossible() {
	return mpAudio != 0;
}

bool VDCaptureDriverEmulation::IsAudioCaptureEnabled() {
	return mpAudio != 0 && mbAudioCaptureEnabled;
}

void VDCaptureDriverEmulation::SetAudioCaptureEnabled(bool b) {
	mbAudioCaptureEnabled = b;
}

void VDCaptureDriverEmulation::SetAudioAnalysisEnabled(bool b) {
	if (b != mbAudioAnalysisEnabled) {
		mAudioRecordBuffer.Flush();
		mbAudioAnalysisEnabled = b;

		if (mpAudio)
			mAudioClientPosition = (uint32)VDFraction(mAudioSampleRate, 1000).scale64r(mpAudioOutput->GetPosition()) * mAudioSampleSize;
	}
}

void VDCaptureDriverEmulation::GetAvailableAudioFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats) {
	aformats.clear();

	if (mpAudio)
		aformats.push_back(vdstructex<WAVEFORMATEX>((WAVEFORMATEX *)mpAudio->getWaveFormat(), mpAudio->getFormatLen()));
}

bool VDCaptureDriverEmulation::GetAudioFormat(vdstructex<WAVEFORMATEX>& aformat) {
	if (mpAudio) {
		aformat.assign((WAVEFORMATEX *)mpAudio->getWaveFormat(), mpAudio->getFormatLen());
		return true;
	}
	return false;
}

bool VDCaptureDriverEmulation::SetAudioFormat(const WAVEFORMATEX *pwfex, uint32 size) {
	return false;
}

bool VDCaptureDriverEmulation::IsDriverDialogSupported(DriverDialog dlg) {
	return dlg == kDialogVideoSource;
}

void VDCaptureDriverEmulation::DisplayDriverDialog(DriverDialog dlg) {
	VDASSERT(IsDriverDialogSupported(dlg));

	if (dlg == kDialogVideoSource) {
		tVDInputDrivers inDrivers;
		std::vector<int> xlat;

		VDGetInputDrivers(inDrivers, IVDInputDriver::kF_Video);
		VDStringW fileFilters(VDMakeInputDriverFileFilter(inDrivers, xlat));

		static const VDFileDialogOption sOptions[]={
			{ VDFileDialogOption::kForceTemplate,  0, 0, 0, 0 },
			{ VDFileDialogOption::kSelectedFilter, 1, 0, 0, 0 },
			{0}
		};

		int optVals[2]={1,0};

		const VDStringW fn(VDGetLoadFileName('cpem', (VDGUIHandle)mhwndParent, L"Load video capture emulation clip", fileFilters.c_str(), NULL, sOptions, optVals));

		if (!fn.empty()) {
			CloseInputFile();

			IVDInputDriver *pDriver = 0;
			if (xlat[optVals[1]-1] >= 0)
				pDriver = inDrivers[xlat[optVals[1]-1]];

			try {
				OpenInputFile(fn.c_str(), pDriver);

				VDRegistryAppKey key("Capture");

				key.setString("Emulation file", fn.c_str());
			} catch(const MyError& e) {
				e.post(mhwndParent, g_szError);
			}
		}
	}
}

bool VDCaptureDriverEmulation::CaptureStart() {
	if (!VDINLINEASSERTFALSE(mbCapturing)) {
		if (mpCB && !mpCB->CapEvent(kEventPreroll, 0)) {
			mbCapturing = false;
			return false;
		}

		mbCapturing = true;

		if (mpCB) {
			try {
				mpCB->CapBegin(0);
			} catch(MyError& e) {
				mCaptureError.TransferFrom(e);
				CaptureStop();
				return false;
			}
		}

		mAudioClientPosition = 0;
		mAudioRecordBuffer.Flush();
		mCaptureStart = VDGetAccurateTick();
		mLastCapturedFrame = -1;
	}

	return mbCapturing;
}

void VDCaptureDriverEmulation::CaptureStop() {
	if (VDINLINEASSERT(mbCapturing)) {
		if (mpCB)
			mpCB->CapEnd(mCaptureError.gets() ? &mCaptureError : NULL);
		mCaptureError.discard();
		mbCapturing = false;
	}
}

void VDCaptureDriverEmulation::CaptureAbort() {
	if (mbCapturing) {
		if (mpCB)
			mpCB->CapEnd(mCaptureError.gets() ? &mCaptureError : NULL);
		mCaptureError.discard();
		mbCapturing = false;
	}
}

void VDCaptureDriverEmulation::UpdateDisplayMode() {
	switch(mDisplayMode) {
	case kDisplayHardware:
	case kDisplaySoftware:
		if (mpVideo) {
			//mpDisplay->SetSourcePersistent(true, mpVideo->getTargetFormat());
			mpDisplay->SetSourceSolidColor(0);
			ShowWindow(mhwnd, SW_SHOWNA);
			break;
		}
		// fall through
	case kDisplayNone:
	case kDisplayAnalyze:
		mpDisplay->Reset();
		mLastDisplayedVideoFrame = -1;
		ShowWindow(mhwnd, SW_HIDE);
		break;
	}
}

void VDCaptureDriverEmulation::CloseInputFile() {
	mFrameTimer.Shutdown();
	if (mAudioTimer) {
		KillTimer(mhwndMessages, mAudioTimer);
		mAudioTimer = 0;
	}

	if (mpDisplay)
		mpDisplay->Reset();

	mpAudioOutput->Stop();
	mpAudioOutput->Shutdown();
	mAudioClientBuffer.clear();
	mAudioRecordBuffer.Shutdown();

	mpInputFile = NULL;
	mpAudio = NULL;
	mpVideo = NULL;
}

void VDCaptureDriverEmulation::OpenInputFile(const wchar_t *fn, IVDInputDriver* pDriver) {
	if (!pDriver)
		pDriver = VDAutoselectInputDriverForFile(fn, IVDInputDriver::kF_Video);

	mpInputFile = pDriver->CreateInputFile(0);
	if (!mpInputFile)
		throw MyMemoryError();

	mpInputFile->Init(fn);
	mpInputFile->GetVideoSource(0, ~mpVideo);
	mpInputFile->GetAudioSource(0, ~mpAudio);

	if (mpVideo) {
		mFrame = 0;
		mFrameCount = mpVideo->asStream()->getLength();

		mpVideo->setTargetFormat(0);
		mLastDisplayedVideoFrame = -1;

		sint32 period = (sint32)mpVideo->asStream()->getRate().scale64ir(1000);

		if (mpAudio)
			period >>= 2;

		mFrameTimer.Init(this, period);
	}

	if (mpAudio) {
		WAVEFORMATEX *pwfex = (WAVEFORMATEX *)mpAudio->getWaveFormat();

		if (!is_audio_pcm((VDWaveFormat*)pwfex)) {
			VDWaveFormat target = *(VDWaveFormat*)pwfex;
			target.mTag = VDWaveFormat::kTagPCM;
			target.mChannels = 0;
			target.mSampleBits = 16;
			mpAudio->SetTargetFormat(&target);
		}

		pwfex = (WAVEFORMATEX *)mpAudio->getWaveFormat();

		if (is_audio_pcm((VDWaveFormat*)pwfex)) {
			sint32 audioSamplesPerBlock = (pwfex->nAvgBytesPerSec / 5 + pwfex->nBlockAlign - 1) / pwfex->nBlockAlign;
			sint32 bytesPerBlock = audioSamplesPerBlock * pwfex->nBlockAlign;

			mpAudioOutput->Init(bytesPerBlock, 2, pwfex, VDPreferencesGetAudioPlaybackDeviceKey().c_str());
			mpAudioOutput->Start();
			mAudioClientBuffer.resize(bytesPerBlock);
			mAudioRecordBuffer.Init(bytesPerBlock * 4);		// We need more space to account for device buffering

			mAudioSampleSize = pwfex->nBlockAlign;
			mAudioSampleRate = pwfex->nSamplesPerSec;

			mAudioTimer = SetTimer(mhwndMessages, 1, 10, NULL);
			mAudioPos = 0;
			mAudioClientPosition = 0;
		} else
			mpAudio = NULL;
	}

	if (mpVideo)
		UpdateDisplayMode();

	if (mpCB) {
		mpCB->CapEvent(kEventVideoFormatChanged, 0);
		mpCB->CapEvent(kEventVideoFrameRateChanged, 0);
	}

	VDSetLastLoadSavePath('cpem', fn);
}

void VDCaptureDriverEmulation::TimerCallback() {
	//PostMessage(mhwndMessages, WM_APP, 0, 0);
	SetTimer(mhwndMessages,100,0,0);
}

void VDCaptureDriverEmulation::OnTick() {
	if (!mpVideo)
		return;

	if (mpCB && mbCapturing) {
		if (!mpCB->CapEvent(kEventCapturing, 0)) {
			CaptureStop();
			return;
		}
	}

	int m = mDisplayMode;

	VDTime clk = (VDTime)(VDGetAccurateTick() - mCaptureStart) * 1000;

	if (mpAudio) {
		IVDStreamSource *videoStream = mpVideo->asStream();
		double audioRate = mpAudio->getRate().asDouble();
		double videoRate = videoStream->getRate().asDouble();
		VDPosition samples = (VDPosition)(mpAudioOutput->GetPosition() / 1000.0 * audioRate) % mpAudio->getLength();
		mFrame = (VDPosition)(samples / audioRate * videoRate);

		VDPosition limit = videoStream->getLength();

		if (mFrame >= limit)
			mFrame = limit - 1;
	}

	try {
		VDPROFILEBEGINEX("getFrame", (uint32)mFrame);
		mpVideo->getFrame(mFrame);
		VDPROFILEEND();

		if (mbCapturing && mLastCapturedFrame != mFrame) {
			mLastCapturedFrame = mFrame;
			const VDPixmap& px = mpVideo->getTargetFormat();
			if (mpCB)
				mpCB->CapProcessData(0, mpVideo->getFrameBuffer(), (px.pitch<0?-px.pitch:px.pitch) * px.h, clk, true, clk);
		}
	} catch(MyError& e) {
		m = 0;

		if (mbCapturing) {
			mCaptureError.TransferFrom(e);
			CaptureStop();
			return;
		}
	}

	if (!mpAudio && ++mFrame >= mFrameCount)
		mFrame = 0;

	if (m && mLastDisplayedVideoFrame != mFrame) {
		mLastDisplayedVideoFrame = mFrame;
		++mPreviewFrameCount;
		if (!mbCapturing && m == kDisplayAnalyze) {
			const VDPixmap& px = mpVideo->getTargetFormat();
			if (mpCB) {
				try {
					mpCB->CapProcessData(-1, mpVideo->getFrameBuffer(), (px.pitch < 0 ? -px.pitch : px.pitch) * px.h, clk, true, clk);
				} catch(const MyError&) {
					// eat the error
				}
			}
		}

		if (m == kDisplayHardware || m == kDisplaySoftware) {
			mpDisplay->SetSourcePersistent(true, mpVideo->getTargetFormat());
			mpDisplay->Update();
		}
	}
}

LRESULT CALLBACK VDCaptureDriverEmulation::StaticMessageWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_NCCREATE)
		SetWindowLongPtr(hwnd, 0, (LONG_PTR)((LPCREATESTRUCT)lParam)->lpCreateParams);
	else if (msg == WM_APP) {
		VDCaptureDriverEmulation *const pThis = (VDCaptureDriverEmulation *)GetWindowLongPtr(hwnd, 0);

		pThis->OnTick();
		return 0;
	} else if (msg == WM_TIMER) {
		VDCaptureDriverEmulation *const pThis = (VDCaptureDriverEmulation *)GetWindowLongPtr(hwnd, 0);

		if (wParam==100) {
			KillTimer(hwnd,100);
			pThis->OnTick();
			return 0;
		}

		while(long availbytes = pThis->mpAudioOutput->GetAvailSpace()) {
			char buf[8192];
			uint32 bytes = 0, samples = 0;

			if (availbytes > 8192)
				availbytes = 8192;

			uint32 availsamples = (uint32)availbytes / pThis->mAudioSampleSize;

			int err = 0;
			if (availsamples > 0) {
				try {
					err = pThis->mpAudio->read(pThis->mAudioPos, availsamples, buf, availbytes, &bytes, &samples);
				} catch(const MyError&) {
					samples = 0;
				}
			}
			
			if (err || !samples) {
				if (!pThis->mAudioPos)
					break;
				pThis->mAudioPos = 0;
				continue;
			}

			pThis->mpAudioOutput->Write(buf, bytes);
			pThis->mAudioPos += samples;

			// attempt to fill audio recording buffer... if full, send out a client block
			if (pThis->mbCapturing ? pThis->mbAudioCaptureEnabled : pThis->mbAudioAnalysisEnabled) {
				const char *src = buf;
				while(bytes > 0) {
					int actualBytes;
					void *dst = pThis->mAudioRecordBuffer.LockWrite(bytes, actualBytes);

					if (!actualBytes) {
						if (pThis->mbCapturing) {
							if (pThis->mbAudioCaptureEnabled) {
								VDTime clk = (VDTime)(VDGetAccurateTick() - pThis->mCaptureStart) * 1000;
								const uint32 size = pThis->mAudioClientBuffer.size();
								char *data = pThis->mAudioClientBuffer.data();
								pThis->mAudioRecordBuffer.Read(data, size);
								pThis->mAudioClientPosition += size;

								try {
									pThis->mpCB->CapProcessData(1, data, size, clk, false, clk);
								} catch(MyError& e) {
									pThis->mCaptureError.TransferFrom(e);
									pThis->CaptureStop();
									return 0;
								}
							}
						} else {
							VDDEBUG("CaptureEmulation: Audio capture buffer overflow detected.\n");

							const uint32 size = pThis->mAudioClientBuffer.size();
							char *data = pThis->mAudioClientBuffer.data();
							pThis->mAudioRecordBuffer.Read(data, size);

							pThis->mAudioClientPosition += size;

							try {
								pThis->mpCB->CapProcessData(-2, data, size, 0, false, 0);
							} catch(MyError& e) {
								pThis->mCaptureError.TransferFrom(e);
								pThis->CaptureStop();
								return 0;
							}
						}
					}

					memcpy(dst, src, actualBytes);
					src += actualBytes;
					bytes -= actualBytes;
					pThis->mAudioRecordBuffer.UnlockWrite(actualBytes);
				}
			}
		}

		// If we are analyzing, determine if it is time to send out a new block.
		if (!pThis->mbCapturing && pThis->mbAudioAnalysisEnabled) {
			uint32 pos = (uint32)VDFraction(pThis->mAudioSampleRate, 1000).scale64r(pThis->mpAudioOutput->GetPosition()) * pThis->mAudioSampleSize;

			while(pos > pThis->mAudioClientPosition) {
				uint32 limit = pThis->mAudioClientBuffer.size();
				uint32 tc = pos - pThis->mAudioClientPosition;
				char *data = pThis->mAudioClientBuffer.data();

				if (tc > limit)
					tc = limit;

				int actual = pThis->mAudioRecordBuffer.Read(data, tc);

				if (!actual) {
					VDDEBUG("CaptureEmulation: Audio capture buffer underflow detected.\n");
					pThis->mAudioClientPosition = pos;
					break;
				}

				pThis->mAudioClientPosition += actual;

				try {
					pThis->mpCB->CapProcessData(-2, data, actual, 0, false, 0);
				} catch(const MyError&) {
					break;
				}
			}
		}
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

///////////////////////////////////////////////////////////////////////////

class VDCaptureSystemEmulation : public IVDCaptureSystem {
public:
	VDCaptureSystemEmulation();
	~VDCaptureSystemEmulation();

	void EnumerateDrivers();

	int GetDeviceCount();
	const wchar_t *GetDeviceName(int index);

	IVDCaptureDriver *CreateDriver(int deviceIndex);

protected:
	HMODULE	mhmodAVICap;
	int mDriverCount;
	VDStringW mDrivers[10];
};

IVDCaptureSystem *VDCreateCaptureSystemEmulation() {
	return new VDCaptureSystemEmulation;
}

VDCaptureSystemEmulation::VDCaptureSystemEmulation() {
}

VDCaptureSystemEmulation::~VDCaptureSystemEmulation() {
}

void VDCaptureSystemEmulation::EnumerateDrivers() {
}

int VDCaptureSystemEmulation::GetDeviceCount() {
	return 1;
}

const wchar_t *VDCaptureSystemEmulation::GetDeviceName(int index) {
	return L"Video file (Emulation)";
}

IVDCaptureDriver *VDCaptureSystemEmulation::CreateDriver(int index) {
	if ((unsigned)index >= 1)
		return NULL;

	return new VDCaptureDriverEmulation();
}
