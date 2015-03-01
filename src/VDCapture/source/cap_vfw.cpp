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

#include <stdafx.h>
#include <vd2/VDCapture/capdriver.h>
#include <vd2/system/vdstring.h>
#include <vd2/system/error.h>
#include <vd2/system/time.h>
#include <vd2/system/w32assist.h>
#include <windows.h>
#include <vfw.h>
#include <vector>

using namespace nsVDCapture;

extern HINSTANCE g_hInst;

static const CAPTUREPARMS g_defaultCaptureParms={
	1000000/15,		// 15fps
	FALSE,			// fMakeUserHitOKForCapture
	100,			// wPercentDropForError
	TRUE,			// callbacks won't work if Yield is TRUE
	324000,			// we like index entries
	4,				// wChunkGranularity
	FALSE,			// fUsingDOSMemory
	10,				// wNumVideoRequested
	TRUE,			// fCaptureAudio
	0,				// wNumAudioRequested
	0,				// vKeyAbort
	FALSE,			// fAbortLeftMouse
	FALSE,			// fAbortRightMouse
	FALSE,			// fLimitEnabled
	0,				// wTimeLimit
	FALSE,			// fMCIControl
	FALSE,			// fStepMCIDevice
	0,0,			// dwMCIStartTime, dwMCIStopTime
	FALSE,			// fStepCaptureAt2x
	10,				// wStepCaptureAverageFrames
	0,				// dwAudioBufferSize
	FALSE,			// fDisableWriteCache
	AVSTREAMMASTER_NONE,	//	AVStreamMaster
};

class VDCaptureDriverVFW : public IVDCaptureDriver {
	VDCaptureDriverVFW(const VDCaptureDriverVFW&);
	VDCaptureDriverVFW& operator=(const VDCaptureDriverVFW&);
public:
	VDCaptureDriverVFW(HMODULE hmodAVICap, int driverIndex);
	~VDCaptureDriverVFW();

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

	void	SetFramePeriod(sint32 ms);
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

	int		GetAudioInputCount();
	const wchar_t *GetAudioInputName(int idx);
	bool	SetAudioInput(int idx);
	int		GetAudioInputIndex();

	int		GetAudioSourceForVideoSource(int idx) { return -2; }

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
	void	SyncCaptureStop();
	void	SyncCaptureAbort();
	void	InitMixerSupport();
	void	ShutdownMixerSupport();
	bool	InitWaveAnalysis();
	void	ShutdownWaveAnalysis();
	sint64	ComputeGlobalTime();

	static LRESULT CALLBACK ErrorCallback(HWND hWnd, int nID, LPCSTR lpsz);
	static LRESULT CALLBACK StatusCallback(HWND hWnd, int nID, LPCSTR lpsz);
	static LRESULT CALLBACK ControlCallback(HWND hwnd, int nState);
	static LRESULT CALLBACK PreviewCallback(HWND hWnd, VIDEOHDR *lpVHdr);
	static LRESULT CALLBACK VideoCallback(HWND hWnd, LPVIDEOHDR lpVHdr);
	static LRESULT CALLBACK WaveCallback(HWND hWnd, LPWAVEHDR lpWHdr);
	static LRESULT CALLBACK StaticMessageSinkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	enum { kPreviewTimerID = 100 };

	HWND	mhwnd;
	HWND	mhwndParent;
	HWND	mhwndEventSink;
	HMODULE	mhmodAVICap;

	bool	mbCapturing;
	bool	mbVisible;
	bool	mbBlockVideoFrames;			///< Prevents video frames from being sent the callback. This is used to barrier frames while we sent video format change events.
	bool	mbAudioHardwarePresent;
	bool	mbAudioHardwareEnabled;
	bool	mbAudioCaptureEnabled;
	bool	mbAudioAnalysisEnabled;
	bool	mbAudioAnalysisActive;

	IVDCaptureDriverCallback	*mpCB;
	DisplayMode			mDisplayMode;
	uint32	mGlobalTimeBase;

	int		mDriverIndex;
	UINT	mPreviewFrameTimer;
	VDAtomicInt	mPreviewFrameCount;

	HMIXER	mhMixer;
	int		mMixerInput;
	MIXERCONTROL	mMixerInputControl;
	typedef std::vector<VDStringW>	MixerInputs;
	MixerInputs	mMixerInputs;

	HWAVEIN mhWaveIn;
	WAVEHDR	mWaveBufHdrs[2];
	vdblock<char>	mWaveBuffer;

	CAPDRIVERCAPS		mCaps;

	MyError	mCaptureError;

	static ATOM sMsgSinkClass;
};

ATOM VDCaptureDriverVFW::sMsgSinkClass;

VDCaptureDriverVFW::VDCaptureDriverVFW(HMODULE hmodAVICap, int driverIndex)
	: mhwnd(NULL)
	, mhwndParent(NULL)
	, mhwndEventSink(NULL)
	, mhmodAVICap(hmodAVICap)
	, mpCB(NULL)
	, mbCapturing(false)
	, mbVisible(false)
	, mbBlockVideoFrames(false)
	, mbAudioAnalysisEnabled(false)
	, mbAudioAnalysisActive(false)
	, mDisplayMode(kDisplayNone)
	, mDriverIndex(driverIndex)
	, mPreviewFrameTimer(0)
	, mhMixer(NULL)
	, mhWaveIn(NULL)
{
	memset(mWaveBufHdrs, 0, sizeof mWaveBufHdrs);
}

VDCaptureDriverVFW::~VDCaptureDriverVFW() {
	Shutdown();
}

void VDCaptureDriverVFW::SetCallback(IVDCaptureDriverCallback *pCB) {
	mpCB = pCB;
}

bool VDCaptureDriverVFW::Init(VDGUIHandle hParent) {
	mhwndParent = (HWND)hParent;

	if (!sMsgSinkClass) {
		WNDCLASS wc = { 0, StaticMessageSinkWndProc, 0, sizeof(VDCaptureDriverVFW *), g_hInst, NULL, NULL, NULL, NULL, "Riza VFW event sink" };

		sMsgSinkClass = RegisterClass(&wc);

		if (!sMsgSinkClass)
			return false;
	}

	// Attempt to open mixer device. It is OK for this to fail. Note that we
	// have a bit of a problem in that (a) the mixer API doesn't take
	// WAVE_MAPPER, and (b) we can't get access to the handle that the
	// capture window creates. For now, we sort of fake it.
	InitMixerSupport();

	// Create message sink.
	if (!(mhwndEventSink = CreateWindow((LPCTSTR)sMsgSinkClass, "", WS_POPUP, 0, 0, 0, 0, mhwndParent, NULL, g_hInst, this))) {
		Shutdown();
		return false;
	}

	typedef HWND (VFWAPI *tpcapCreateCaptureWindow)(LPCSTR lpszWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hwnd, int nID);
	const tpcapCreateCaptureWindow pcapCreateCaptureWindow = (tpcapCreateCaptureWindow)GetProcAddress(mhmodAVICap, "capCreateCaptureWindowA");
	if (!VDINLINEASSERT(pcapCreateCaptureWindow)) {
		Shutdown();
		return false;
	}

	mhwnd = pcapCreateCaptureWindow("", WS_CHILD, 0, 0, 0, 0, mhwndParent, 0);
	if (!mhwnd) {
		Shutdown();
		return false;
	}

	if (!capDriverConnect(mhwnd, mDriverIndex)) {
		Shutdown();
		return false;
	}

	VDVERIFY(capDriverGetCaps(mhwnd, &mCaps, sizeof mCaps));

	capCaptureSetSetup(mhwnd, &g_defaultCaptureParms, sizeof g_defaultCaptureParms);

	mPreviewFrameCount = 0;

	capSetUserData(mhwnd, this);
	capSetCallbackOnError(mhwnd, ErrorCallback);
	capSetCallbackOnStatus(mhwnd, StatusCallback);
	capSetCallbackOnCapControl(mhwnd, ControlCallback);
	capSetCallbackOnVideoStream(mhwnd, VideoCallback);
	capSetCallbackOnWaveStream(mhwnd, WaveCallback);

	capPreviewRate(mhwnd, 1000 / 15);

	CAPSTATUS cs;

	mbAudioHardwarePresent = false;
	if (VDINLINEASSERT(capGetStatus(mhwnd, &cs, sizeof(CAPSTATUS)))) {
		mbAudioHardwarePresent = (cs.fAudioHardware != 0);
	}
	mbAudioHardwareEnabled = mbAudioHardwarePresent;
	mbAudioCaptureEnabled = true;

	return true;
}

void VDCaptureDriverVFW::Shutdown() {
	ShutdownWaveAnalysis();

	if (mPreviewFrameTimer) {
		KillTimer(mhwndEventSink, mPreviewFrameTimer);
		mPreviewFrameTimer = 0;
	}

	if (mhwndEventSink) {
		DestroyWindow(mhwndEventSink);
		mhwndEventSink = NULL;
	}

	if (mhwnd) {
		capDriverDisconnect(mhwnd);
		DestroyWindow(mhwnd);
		mhwnd = NULL;
	}

	if (mhMixer) {
		mixerClose(mhMixer);
		mhMixer = NULL;
	}
}

bool VDCaptureDriverVFW::IsHardwareDisplayAvailable() {
	return 0 != mCaps.fHasOverlay;
}

void VDCaptureDriverVFW::SetDisplayMode(DisplayMode mode) {
	if (mode == mDisplayMode)
		return;

	if (mDisplayMode == kDisplayAnalyze) {
		if (mPreviewFrameTimer) {
			KillTimer(mhwndEventSink, mPreviewFrameTimer);
			mPreviewFrameTimer = 0;
		}
	}

	mDisplayMode = mode;

	switch(mode) {
	case kDisplayNone:
		capSetCallbackOnFrame(mhwnd, NULL);
		capPreview(mhwnd, FALSE);
		capOverlay(mhwnd, FALSE);
		ShowWindow(mhwnd, SW_HIDE);
		break;
	case kDisplayHardware:
		// we have to kill this callback or dual-field overlay may not work
		capSetCallbackOnFrame(mhwnd, NULL);
		capPreview(mhwnd, FALSE);
		capOverlay(mhwnd, TRUE);
		if (mbVisible)
			ShowWindow(mhwnd, SW_SHOWNA);
		break;
	case kDisplaySoftware:
		capSetCallbackOnFrame(mhwnd, PreviewCallback);
		capOverlay(mhwnd, FALSE);
		capPreview(mhwnd, TRUE);
		if (mbVisible)
			ShowWindow(mhwnd, SW_SHOWNA);
		break;
	case kDisplayAnalyze:
		capSetCallbackOnFrame(mhwnd, PreviewCallback);
		capOverlay(mhwnd, FALSE);
		capPreview(mhwnd, FALSE);
		if (!mPreviewFrameTimer)
			mPreviewFrameTimer = SetTimer(mhwndEventSink, kPreviewTimerID, 66, NULL);
		if (mbVisible)
			ShowWindow(mhwnd, SW_HIDE);
		break;
	}
}

DisplayMode VDCaptureDriverVFW::GetDisplayMode() {
	return mDisplayMode;
}

void VDCaptureDriverVFW::SetDisplayRect(const vdrect32& r) {
	SetWindowPos(mhwnd, NULL, r.left, r.top, r.width(), r.height(), SWP_NOZORDER|SWP_NOACTIVATE);
}

vdrect32 VDCaptureDriverVFW::GetDisplayRectAbsolute() {
	RECT r;
	GetWindowRect(mhwnd, &r);
	MapWindowPoints(GetParent(mhwnd), NULL, (LPPOINT)&r, 2);
	return vdrect32(r.left, r.top, r.right, r.bottom);
}

void VDCaptureDriverVFW::SetDisplayVisibility(bool vis) {
	if (vis == mbVisible)
		return;

	mbVisible = vis;
	ShowWindow(mhwnd, vis && mDisplayMode != kDisplayAnalyze ? SW_SHOWNA : SW_HIDE);
}

void VDCaptureDriverVFW::SetFramePeriod(sint32 framePeriod100nsUnits) {
	CAPTUREPARMS cp;

	if (capCaptureGetSetup(mhwnd, &cp, sizeof(CAPTUREPARMS))) {
		cp.dwRequestMicroSecPerFrame = (framePeriod100nsUnits + 5) / 10;

		capCaptureSetSetup(mhwnd, &cp, sizeof(CAPTUREPARMS));

		if (mpCB)
			mpCB->CapEvent(kEventVideoFrameRateChanged, 0);
	}
}

sint32 VDCaptureDriverVFW::GetFramePeriod() {
	CAPTUREPARMS cp;

	if (VDINLINEASSERT(capCaptureGetSetup(mhwnd, &cp, sizeof(CAPTUREPARMS))))
		return cp.dwRequestMicroSecPerFrame*10;

	return 10000000 / 15;
}

uint32 VDCaptureDriverVFW::GetPreviewFrameCount() {
	if (mDisplayMode == kDisplaySoftware || mDisplayMode == kDisplayAnalyze)
		return mPreviewFrameCount;

	return 0;
}

bool VDCaptureDriverVFW::GetVideoFormat(vdstructex<BITMAPINFOHEADER>& vformat) {
	DWORD dwSize = capGetVideoFormatSize(mhwnd);

	vformat.resize(dwSize);

	VDVERIFY(capGetVideoFormat(mhwnd, vformat.data(), vformat.size()));
	return true;
}

bool VDCaptureDriverVFW::SetVideoFormat(const BITMAPINFOHEADER *pbih, uint32 size) {
	bool success = false;

	mbBlockVideoFrames = true;
	if (capSetVideoFormat(mhwnd, (BITMAPINFOHEADER *)pbih, size)) {
		if (mpCB)
			mpCB->CapEvent(kEventVideoFormatChanged, 0);
		success = true;
	}
	mbBlockVideoFrames = false;
	return success;
}

int VDCaptureDriverVFW::GetAudioDeviceCount() {
	return mbAudioHardwarePresent ? 1 : 0;
}

const wchar_t *VDCaptureDriverVFW::GetAudioDeviceName(int idx) {
	if (idx || !mbAudioHardwarePresent)
		return NULL;

	return L"Wave Mapper";
}

bool VDCaptureDriverVFW::SetAudioDevice(int idx) {
	if (idx < -1 || idx >= 1)
		return false;
	
	if (!idx && !mbAudioHardwarePresent)
		return false;

	bool enable = !idx;

	if (enable == mbAudioHardwareEnabled)
		return true;

	ShutdownWaveAnalysis();
	mbAudioHardwareEnabled = enable;
	InitWaveAnalysis();

	return true;
}

int VDCaptureDriverVFW::GetAudioDeviceIndex() {
	return mbAudioHardwareEnabled ? 0 : -1;
}

int VDCaptureDriverVFW::GetVideoSourceCount() {
	return 0;
}

const wchar_t *VDCaptureDriverVFW::GetVideoSourceName(int idx) {
	return NULL;
}

bool VDCaptureDriverVFW::SetVideoSource(int idx) {
	return idx == -1;
}

int VDCaptureDriverVFW::GetVideoSourceIndex() {
	return -1;
}

int VDCaptureDriverVFW::GetAudioSourceCount() {
	return 0;
}

const wchar_t *VDCaptureDriverVFW::GetAudioSourceName(int idx) {
	return NULL;
}

bool VDCaptureDriverVFW::SetAudioSource(int idx) {
	return idx == -1;
}

int VDCaptureDriverVFW::GetAudioSourceIndex() {
	return -1;
}

int VDCaptureDriverVFW::GetAudioInputCount() {
	return mbAudioHardwareEnabled ? mMixerInputs.size() : 0;
}

const wchar_t *VDCaptureDriverVFW::GetAudioInputName(int idx) {
	if (!mbAudioHardwareEnabled || (unsigned)idx >= mMixerInputs.size())
		return NULL;

	MixerInputs::const_iterator it(mMixerInputs.begin());

	std::advance(it, idx);

	return (*it).c_str();
}

bool VDCaptureDriverVFW::SetAudioInput(int idx) {
	if (!mbAudioHardwareEnabled || !mhMixer)
		return idx == -1;

	VDASSERT(mMixerInputs.size() == mMixerInputControl.cMultipleItems);

	if (idx != -1 && (unsigned)idx >= mMixerInputControl.cMultipleItems)
		return false;

	// attempt to set the appropriate mixer input

	vdblock<MIXERCONTROLDETAILS_BOOLEAN> vals(mMixerInputControl.cMultipleItems);

	for(unsigned i=0; i<mMixerInputControl.cMultipleItems; ++i)
		vals[i].fValue = (i == idx);

	MIXERCONTROLDETAILS details = {sizeof(MIXERCONTROLDETAILS)};

	details.dwControlID		= mMixerInputControl.dwControlID;
	details.cChannels		= 1;
	details.cMultipleItems	= mMixerInputControl.cMultipleItems;
	details.cbDetails		= sizeof(MIXERCONTROLDETAILS_BOOLEAN);
	details.paDetails		= vals.data();

	if (MMSYSERR_NOERROR != mixerSetControlDetails((HMIXEROBJ)mhMixer, &details, MIXER_SETCONTROLDETAILSF_VALUE))
		return false;

	mMixerInput = idx;

	return true;
}

int VDCaptureDriverVFW::GetAudioInputIndex() {
	return mbAudioHardwareEnabled ? mMixerInput : -1;
}

bool VDCaptureDriverVFW::IsAudioCapturePossible() {
	return mbAudioHardwareEnabled;
}

bool VDCaptureDriverVFW::IsAudioCaptureEnabled() {
	return mbAudioHardwareEnabled && mbAudioCaptureEnabled;
}

void VDCaptureDriverVFW::SetAudioCaptureEnabled(bool b) {
	mbAudioCaptureEnabled = b;
}

void VDCaptureDriverVFW::SetAudioAnalysisEnabled(bool b) {
	if (mbAudioAnalysisEnabled == b)
		return;

	mbAudioAnalysisEnabled = b;

	if (mbAudioAnalysisEnabled)
		InitWaveAnalysis();
	else
		ShutdownWaveAnalysis();
}

void VDCaptureDriverVFW::GetAvailableAudioFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats) {
	static const int kSamplingRates[]={
		8000,
		11025,
		12000,
		16000,
		22050,
		24000,
		32000,
		44100,
		48000,
		96000,
		192000
	};

	static const int kChannelCounts[]={
		1,
		2
	};

	static const int kSampleDepths[]={
		8,
		16
	};

	for(int sridx=0; sridx < sizeof kSamplingRates / sizeof kSamplingRates[0]; ++sridx)
		for(int chidx=0; chidx < sizeof kChannelCounts / sizeof kChannelCounts[0]; ++chidx)
			for(int sdidx=0; sdidx < sizeof kSampleDepths / sizeof kSampleDepths[0]; ++sdidx) {
				WAVEFORMATEX wfex={
					WAVE_FORMAT_PCM,
					kChannelCounts[chidx],
					kSamplingRates[sridx],
					0,
					0,
					kSampleDepths[sdidx],
					0
				};

				wfex.nBlockAlign = wfex.nChannels * (wfex.wBitsPerSample >> 3);
				wfex.nAvgBytesPerSec = wfex.nBlockAlign * wfex.nSamplesPerSec;

				if (MMSYSERR_NOERROR ==waveInOpen(NULL, WAVE_MAPPER, &wfex, 0, 0, WAVE_FORMAT_QUERY | WAVE_FORMAT_DIRECT)) {
					aformats.push_back(vdstructex<WAVEFORMATEX>(&wfex, sizeof wfex));
				}
			}
}

bool VDCaptureDriverVFW::GetAudioFormat(vdstructex<WAVEFORMATEX>& aformat) {
	DWORD dwSize = capGetAudioFormatSize(mhwnd);

	if (!dwSize)
		return false;

	aformat.resize(dwSize);

	if (dwSize < sizeof(PCMWAVEFORMAT))
		return false;

	if (!capGetAudioFormat(mhwnd, aformat.data(), aformat.size()))
		return false;

	// adjust PCMWAVEFORMAT to WAVEFORMATEX
	if (dwSize < sizeof(WAVEFORMATEX)) {
		aformat.resize(sizeof(WAVEFORMATEX));

		aformat->cbSize = 0;
	}

	return true;
}

bool VDCaptureDriverVFW::SetAudioFormat(const WAVEFORMATEX *pwfex, uint32 size) {
	if (!mbAudioHardwareEnabled)
		return false;

	ShutdownWaveAnalysis();
	bool success = 0 != capSetAudioFormat(mhwnd, (WAVEFORMATEX *)pwfex, size);
	if (mbAudioAnalysisEnabled)
		InitWaveAnalysis();
	return success;
}

bool VDCaptureDriverVFW::IsDriverDialogSupported(DriverDialog dlg) {
	switch(dlg) {
	case kDialogVideoFormat:
		return mCaps.fHasDlgVideoFormat != 0;
	case kDialogVideoSource:
		return mCaps.fHasDlgVideoSource != 0;
	case kDialogVideoDisplay:
		return mCaps.fHasDlgVideoDisplay != 0;
	}

	return false;
}

void VDCaptureDriverVFW::DisplayDriverDialog(DriverDialog dlg) {
	VDASSERT(IsDriverDialogSupported(dlg));

	switch(dlg) {
	case kDialogVideoFormat:
		// The format dialog is special (i.e. nasty) in that it can change the
		// video format, so we block outgoing frames while it is up and check
		// for a format change.
		{
			vdstructex<BITMAPINFOHEADER> oldFormat, newFormat;

			mbBlockVideoFrames = true;
			GetVideoFormat(oldFormat);
			capDlgVideoFormat(mhwnd);
			GetVideoFormat(newFormat);
			if (oldFormat != newFormat && mpCB)
				mpCB->CapEvent(kEventVideoFormatChanged, 0);
			mbBlockVideoFrames = false;
		}
		break;
	case kDialogVideoSource:
		capDlgVideoSource(mhwnd);
		break;
	case kDialogVideoDisplay:
		capDlgVideoDisplay(mhwnd);
		break;
	}
}

bool VDCaptureDriverVFW::CaptureStart() {
	ShutdownWaveAnalysis();

	// Aim for 0.1s audio buffers.
	CAPTUREPARMS cp;
	if (capCaptureGetSetup(mhwnd, &cp, sizeof(CAPTUREPARMS))) {
		cp.fCaptureAudio = mbAudioHardwareEnabled && mbAudioCaptureEnabled;

		if (cp.fCaptureAudio) {
			vdstructex<WAVEFORMATEX> wfex;
			if (GetAudioFormat(wfex)) {
				cp.wNumAudioRequested = 10;
				cp.dwAudioBufferSize = (wfex->nAvgBytesPerSec / 10 + wfex->nBlockAlign - 1);
				cp.dwAudioBufferSize -= cp.dwAudioBufferSize % wfex->nBlockAlign;
			}
		}

		capCaptureSetSetup(mhwnd, &cp, sizeof(CAPTUREPARMS));
	}

	if (!VDINLINEASSERTFALSE(mbCapturing)) {
		mGlobalTimeBase = VDGetAccurateTick();
		mbCapturing = !!capCaptureSequenceNoFile(mhwnd);

		if (!mbCapturing && mbAudioAnalysisEnabled)
			InitWaveAnalysis();
	}

	return mbCapturing;
}

void VDCaptureDriverVFW::CaptureStop() {
	SendMessage(mhwndEventSink, WM_APP+16, 0, 0);
}

void VDCaptureDriverVFW::CaptureAbort() {
	SendMessage(mhwndEventSink, WM_APP+17, 0, 0);
}

void VDCaptureDriverVFW::SyncCaptureStop() {
	if (VDINLINEASSERT(mbCapturing)) {
		capCaptureStop(mhwnd);
		mbCapturing = false;

		if (mbAudioAnalysisEnabled)
			InitWaveAnalysis();
	}
}

void VDCaptureDriverVFW::SyncCaptureAbort() {
	if (mbCapturing) {
		capCaptureAbort(mhwnd);
		mbCapturing = false;

		if (mbAudioAnalysisEnabled)
			InitWaveAnalysis();
	}
}

void VDCaptureDriverVFW::InitMixerSupport() {
	WAVEINCAPS wcaps={0};
	if (MMSYSERR_NOERROR == waveInGetDevCaps(WAVE_MAPPER, &wcaps, sizeof wcaps) && wcaps.dwFormats) {
		WAVEFORMATEX wfex;

		// create lowest-common denominator format for device
		wfex.wFormatTag			= WAVE_FORMAT_PCM;

		if (wcaps.dwFormats & (WAVE_FORMAT_4M08 | WAVE_FORMAT_4M16 | WAVE_FORMAT_4S08 | WAVE_FORMAT_4S16))
			wfex.nSamplesPerSec = 11025;
		else if (wcaps.dwFormats & (WAVE_FORMAT_2M08 | WAVE_FORMAT_2M16 | WAVE_FORMAT_2S08 | WAVE_FORMAT_2S16))
			wfex.nSamplesPerSec = 22050;
		else
			wfex.nSamplesPerSec = 44100;

		if (wcaps.dwFormats & (WAVE_FORMAT_1M08 | WAVE_FORMAT_1M16 | WAVE_FORMAT_2M08 | WAVE_FORMAT_2M16 | WAVE_FORMAT_4M08 | WAVE_FORMAT_4M16))
			wfex.nChannels = 1;
		else
			wfex.nChannels = 2;

		if (wcaps.dwFormats & (WAVE_FORMAT_1M08 | WAVE_FORMAT_1S08 | WAVE_FORMAT_2M08 | WAVE_FORMAT_2S08 | WAVE_FORMAT_4M08 | WAVE_FORMAT_4S08))
			wfex.wBitsPerSample = 8;
		else
			wfex.wBitsPerSample = 16;

		wfex.nBlockAlign		= wfex.wBitsPerSample >> 3;
		wfex.nAvgBytesPerSec	= wfex.nSamplesPerSec * wfex.nBlockAlign;
		wfex.cbSize				= 0;

		// create the device
		HWAVEIN hwi;
		if (MMSYSERR_NOERROR == waveInOpen(&hwi, WAVE_MAPPER, &wfex, 0, 0, CALLBACK_NULL)) {
			// create mixer based on device

			if (MMSYSERR_NOERROR == mixerOpen(&mhMixer, (UINT)hwi, 0, 0, MIXER_OBJECTF_HWAVEIN)) {
				MIXERLINE mixerLine = {sizeof(MIXERLINE)};

				mixerLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_WAVEIN;

				if (MMSYSERR_NOERROR == mixerGetLineInfo((HMIXEROBJ)mhMixer, &mixerLine, MIXER_GETLINEINFOF_COMPONENTTYPE)) {

					// Try to find a MIXER or MUX control
					MIXERLINECONTROLS lineControls = {sizeof(MIXERLINECONTROLS)};

					mMixerInputControl.cbStruct = sizeof(MIXERCONTROL);
					mMixerInputControl.dwControlType = 0;

					lineControls.dwLineID = mixerLine.dwLineID;
					lineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_MUX;
					lineControls.cControls = 1;
					lineControls.pamxctrl = &mMixerInputControl;
					lineControls.cbmxctrl = sizeof(MIXERCONTROL);

					MMRESULT res;

					res = mixerGetLineControls((HMIXEROBJ)mhMixer, &lineControls, MIXER_GETLINECONTROLSF_ONEBYTYPE);

					if (MMSYSERR_NOERROR != res) {
						lineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_MIXER;

						res = mixerGetLineControls((HMIXEROBJ)mhMixer, &lineControls, MIXER_GETLINECONTROLSF_ONEBYTYPE);
					}

					// The mux/mixer control must be of MULTIPLE type; otherwise, we reject it.
					if (!(mMixerInputControl.fdwControl & MIXERCONTROL_CONTROLF_MULTIPLE))
						res = MMSYSERR_ERROR;

					// If we were successful, then enumerate all source lines and push them into the map.
					if (MMSYSERR_NOERROR != res) {
						mixerClose(mhMixer);
						mhMixer = NULL;
					} else {
						// Enumerate control inputs and populate the name array
						vdblock<MIXERCONTROLDETAILS_LISTTEXT> names(mMixerInputControl.cMultipleItems);

						MIXERCONTROLDETAILS details = {sizeof(MIXERCONTROLDETAILS)};

						details.dwControlID		= mMixerInputControl.dwControlID;
						details.cChannels		= 1;
						details.cMultipleItems	= mMixerInputControl.cMultipleItems;
						details.cbDetails		= sizeof(MIXERCONTROLDETAILS_LISTTEXT);
						details.paDetails		= names.data();

						mMixerInput = -1;

						if (MMSYSERR_NOERROR == mixerGetControlDetails((HMIXEROBJ)mhMixer, &details, MIXER_GETCONTROLDETAILSF_LISTTEXT)) {
							mMixerInputs.reserve(details.cMultipleItems);

							for(unsigned i=0; i<details.cMultipleItems; ++i)
								mMixerInputs.push_back(MixerInputs::value_type(VDTextAToW(names[i].szName)));

							vdblock<MIXERCONTROLDETAILS_BOOLEAN> vals(mMixerInputControl.cMultipleItems);

							details.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
							details.paDetails = vals.data();

							if (MMSYSERR_NOERROR == mixerGetControlDetails((HMIXEROBJ)mhMixer, &details, MIXER_GETCONTROLDETAILSF_VALUE)) {
								// Attempt to find a mixer input that is set. Note that for
								// a multiple-select type (MIXER) this will pick the first
								// enabled input.
								for(unsigned i=0; i<details.cMultipleItems; ++i)
									if (vals[i].fValue) {
										mMixerInput = i;
										break;
									}
							}
						}
					}
				}

				// We don't close the mixer here; it is left open while we have the
				// capture device opened.
			}

			waveInClose(hwi);
		}
	}
}

void VDCaptureDriverVFW::ShutdownMixerSupport() {
	if (mhMixer) {
		mixerClose(mhMixer);
		mhMixer = NULL;
	}
}

bool VDCaptureDriverVFW::InitWaveAnalysis() {
	if (!mbAudioHardwareEnabled)
		return false;

	vdstructex<WAVEFORMATEX> aformat;

	if (!GetAudioFormat(aformat))
		return false;

	uint32	blockSize = (aformat->nAvgBytesPerSec + 9) / 10 + aformat->nBlockAlign - 1;
	blockSize -= blockSize % aformat->nBlockAlign;

	mWaveBuffer.resize(blockSize*2);

	if (MMSYSERR_NOERROR != waveInOpen(&mhWaveIn, WAVE_MAPPER, aformat.data(), (DWORD_PTR)mhwndEventSink, 0, CALLBACK_WINDOW | WAVE_FORMAT_DIRECT))
		return false;

	mbAudioAnalysisActive = true;
	for(int i=0; i<2; ++i) {
		WAVEHDR& hdr = mWaveBufHdrs[i];

		hdr.lpData			= &mWaveBuffer[blockSize*i];
		hdr.dwBufferLength	= blockSize;
		hdr.dwBytesRecorded	= 0;
		hdr.dwFlags			= 0;
		hdr.dwLoops			= 0;
		if (MMSYSERR_NOERROR != waveInPrepareHeader(mhWaveIn, &hdr, sizeof(WAVEHDR))) {
			ShutdownWaveAnalysis();
			return false;
		}

		if (MMSYSERR_NOERROR != waveInAddBuffer(mhWaveIn, &hdr, sizeof(WAVEHDR))) {
			ShutdownWaveAnalysis();
			return false;
		}
	}

	if (MMSYSERR_NOERROR != waveInStart(mhWaveIn)) {
		ShutdownWaveAnalysis();
		return false;
	}

	return true;
}

void VDCaptureDriverVFW::ShutdownWaveAnalysis() {
	if (mhWaveIn) {
		mbAudioAnalysisActive = false;
		waveInReset(mhWaveIn);

		for(int i=0; i<2; ++i) {
			if (mWaveBufHdrs[i].dwFlags & WHDR_PREPARED)
				waveInUnprepareHeader(mhWaveIn, &mWaveBufHdrs[i], sizeof(WAVEHDR));
		}

		waveInClose(mhWaveIn);
		mhWaveIn = NULL;
	}

	mWaveBuffer.clear();
}

sint64 VDCaptureDriverVFW::ComputeGlobalTime() {
	// AVICap internally seems to use timeGetTime() when queried via capGetStatus(), but
	// this isn't guaranteed anywhere.
	return (sint64)((uint64)(VDGetAccurateTick() - mGlobalTimeBase) * 1000);
}

#pragma vdpragma_TODO("Need to find some way to propagate these errors back nicely")

LRESULT CALLBACK VDCaptureDriverVFW::ErrorCallback(HWND hwnd, int nID, LPCSTR lpsz) {
#if 0
	static const struct {
		int id;
		const char *szError;
	} g_betterCaptureErrors[]={
		{ 434,	"Warning: No frames captured.\n"
				"\n"
				"Make sure your capture card is functioning correctly and that a valid video source "
				"is connected.  You might also try turning off overlay, reducing the image size, or "
				"reducing the image depth to 24 or 16-bit." },
		{ 439,	"Error: Cannot find a driver to draw this non-RGB image format.  Preview and histogram functions will be unavailable." },
	};

	char buf[1024];
	int i;

	if (!nID) return 0;

	for(i=0; i<sizeof g_betterCaptureErrors/sizeof g_betterCaptureErrors[0]; i++)
		if (g_betterCaptureErrors[i].id == nID) {
			MessageBox(GetParent(hWnd), g_betterCaptureErrors[i].szError, "VirtualDub capture error", MB_OK);
			return 0;
		}

	buf[0] = 0;
	_snprintf(buf, sizeof buf / sizeof buf[0], "Error %d: %s", nID, lpsz);
	MessageBox(GetParent(hWnd), buf, "VirtualDub capture error", MB_OK);
	VDDEBUG("%s\n",buf);
#endif

	return 0;
}

LRESULT CALLBACK VDCaptureDriverVFW::StatusCallback(HWND hwnd, int nID, LPCSTR lpsz) {
	VDCaptureDriverVFW *pThis = (VDCaptureDriverVFW *)capGetUserData(hwnd);

	if (nID == IDS_CAP_BEGIN) {
		CAPSTATUS capStatus;
		capGetStatus(hwnd, (LPARAM)&capStatus, sizeof(CAPSTATUS));

		if (pThis->mpCB) {
			try {
				pThis->mpCB->CapBegin((sint64)capStatus.dwCurrentTimeElapsedMS * 1000);
			} catch(MyError& e) {
				pThis->mCaptureError.TransferFrom(e);
				capCaptureAbort(hwnd);
				pThis->mbCapturing = false;
			}
		}
	} else if (nID == IDS_CAP_END) {
		if (pThis->mpCB) {
			pThis->mpCB->CapEnd(pThis->mCaptureError.gets() ? &pThis->mCaptureError : NULL);
			pThis->mbCapturing = false;
		}

		pThis->mCaptureError.discard();
	}

#if 0
	char buf[1024];

	// Intercept nID=510 (per frame info)

	if (nID == 510)
		return 0;

	if (nID) {
		buf[0] = 0;
		_snprintf(buf, sizeof buf / sizeof buf[0], "Status %d: %s", nID, lpsz);
		SendMessage(GetDlgItem(GetParent(hWnd), IDC_STATUS_WINDOW), SB_SETTEXT, 0, (LPARAM)buf);
		VDDEBUG("%s\n",buf);
	} else {
		SendMessage(GetDlgItem(GetParent(hWnd), IDC_STATUS_WINDOW), SB_SETTEXT, 0, (LPARAM)"");
	}

	return 0;
#endif

	return 0;
}

LRESULT CALLBACK VDCaptureDriverVFW::ControlCallback(HWND hwnd, int nState) {
	VDCaptureDriverVFW *pThis = (VDCaptureDriverVFW *)capGetUserData(hwnd);

	if (pThis->mpCB) {
		switch(nState) {
		case CONTROLCALLBACK_PREROLL:
			return pThis->mpCB->CapEvent(kEventPreroll, 0);
		case CONTROLCALLBACK_CAPTURING:
			return pThis->mpCB->CapEvent(kEventCapturing, 0);
		}
	}

	return TRUE;
}

LRESULT CALLBACK VDCaptureDriverVFW::PreviewCallback(HWND hwnd, VIDEOHDR *lpVHdr) {
	VDCaptureDriverVFW *pThis = (VDCaptureDriverVFW *)capGetUserData(hwnd);
	if (pThis->mpCB && pThis->mDisplayMode == kDisplayAnalyze && !pThis->mbBlockVideoFrames) {
		try {
			pThis->mpCB->CapProcessData(-1, lpVHdr->lpData, lpVHdr->dwBytesUsed, (sint64)lpVHdr->dwTimeCaptured * 1000, 0 != (lpVHdr->dwFlags & VHDR_KEYFRAME), 0);
		} catch(MyError&) {
			// Eat preview errors.
		}
	}
	++pThis->mPreviewFrameCount;
	return 0;
}

LRESULT CALLBACK VDCaptureDriverVFW::VideoCallback(HWND hwnd, LPVIDEOHDR lpVHdr) {
	VDCaptureDriverVFW *pThis = (VDCaptureDriverVFW *)capGetUserData(hwnd);

	if (pThis->mpCB && !pThis->mbBlockVideoFrames) {
		try {
			pThis->mpCB->CapProcessData(0, lpVHdr->lpData, lpVHdr->dwBytesUsed, (sint64)lpVHdr->dwTimeCaptured * 1000, 0 != (lpVHdr->dwFlags & VHDR_KEYFRAME), pThis->ComputeGlobalTime());
		} catch(MyError& e) {
			pThis->mCaptureError.TransferFrom(e);
			capCaptureAbort(hwnd);
			pThis->mbCapturing = false;
		}
	}
	return 0;
}

LRESULT CALLBACK VDCaptureDriverVFW::WaveCallback(HWND hwnd, LPWAVEHDR lpWHdr) {
	VDCaptureDriverVFW *pThis = (VDCaptureDriverVFW *)capGetUserData(hwnd);

	if (pThis->mpCB) {
		try {
			pThis->mpCB->CapProcessData(1, lpWHdr->lpData, lpWHdr->dwBytesRecorded, -1, false, pThis->ComputeGlobalTime());
		} catch(MyError& e) {
			pThis->mCaptureError.TransferFrom(e);
			capCaptureAbort(hwnd);
			pThis->mbCapturing = false;
		}
	}
	return 0;
}

LRESULT CALLBACK VDCaptureDriverVFW::StaticMessageSinkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_NCCREATE:
			SetWindowLongPtr(hwnd, 0, (LONG_PTR)((LPCREATESTRUCT)lParam)->lpCreateParams);
			break;
		case MM_WIM_DATA:
			{
				VDCaptureDriverVFW *pThis = (VDCaptureDriverVFW *)GetWindowLongPtr(hwnd, 0);

				if (pThis->mpCB) {
					WAVEHDR& hdr = *(WAVEHDR *)lParam;

					if (pThis->mbAudioAnalysisActive) {
						// For some reason this is sometimes called after reset. Don't know why yet.
						if (hdr.dwBytesRecorded) {
							try {
								pThis->mpCB->CapProcessData(-2, hdr.lpData, hdr.dwBytesRecorded, -1, false, 0);
							} catch(const MyError&) {
								// eat the error
							}
						}

						waveInAddBuffer(pThis->mhWaveIn, &hdr, sizeof(WAVEHDR));
					}
				}
			}
			return 0;
		case WM_APP+16:
			{
				VDCaptureDriverVFW *pThis = (VDCaptureDriverVFW *)GetWindowLongPtr(hwnd, 0);
				pThis->SyncCaptureStop();
			}
			return 0;
		case WM_APP+17:
			{
				VDCaptureDriverVFW *pThis = (VDCaptureDriverVFW *)GetWindowLongPtr(hwnd, 0);
				pThis->SyncCaptureAbort();
			}
			return 0;
		case WM_TIMER:
			{
				VDCaptureDriverVFW *pThis = (VDCaptureDriverVFW *)GetWindowLongPtr(hwnd, 0);
				if (pThis->mDisplayMode == kDisplayAnalyze && !pThis->mbCapturing)
					capGrabFrameNoStop(pThis->mhwnd);
			}
			return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

///////////////////////////////////////////////////////////////////////////

class VDCaptureSystemVFW : public IVDCaptureSystem {
public:
	VDCaptureSystemVFW();
	~VDCaptureSystemVFW();

	void EnumerateDrivers();

	int GetDeviceCount();
	const wchar_t *GetDeviceName(int index);

	IVDCaptureDriver *CreateDriver(int deviceIndex);

protected:
	HMODULE	mhmodAVICap;
	int mDriverCount;
	VDStringW mDrivers[10];
};

IVDCaptureSystem *VDCreateCaptureSystemVFW() {
	return new VDCaptureSystemVFW;
}

VDCaptureSystemVFW::VDCaptureSystemVFW()
	: mhmodAVICap(VDLoadSystemLibraryW32("avicap32"))
	, mDriverCount(0)
{
}

VDCaptureSystemVFW::~VDCaptureSystemVFW() {
	if (mhmodAVICap)
		FreeLibrary(mhmodAVICap);
}

void VDCaptureSystemVFW::EnumerateDrivers() {
	if (!mhmodAVICap)
		return;

	typedef BOOL (VFWAPI *tpcapGetDriverDescriptionA)(UINT wDriverIndex, LPSTR lpszName, INT cbName, LPSTR lpszVer, INT cbVer);

	const tpcapGetDriverDescriptionA pcapGetDriverDescriptionA = (tpcapGetDriverDescriptionA)GetProcAddress(mhmodAVICap, "capGetDriverDescriptionA");

	if (pcapGetDriverDescriptionA) {
		char buf[256], ver[256];

		mDriverCount = 0;
		while(mDriverCount < 10 && pcapGetDriverDescriptionA(mDriverCount, buf, sizeof buf, ver, sizeof ver)) {
			mDrivers[mDriverCount] = VDTextAToW(buf) + L" (VFW)";
			++mDriverCount;
		}
	}
}

int VDCaptureSystemVFW::GetDeviceCount() {
	return mDriverCount;
}

const wchar_t *VDCaptureSystemVFW::GetDeviceName(int index) {
	return (unsigned)index < (unsigned)mDriverCount ? mDrivers[index].c_str() : NULL;
}

IVDCaptureDriver *VDCaptureSystemVFW::CreateDriver(int index) {
	if ((unsigned)index >= (unsigned)mDriverCount)
		return NULL;

	return new VDCaptureDriverVFW(mhmodAVICap, index);
}
