//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2006 Avery Lee
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
#include <vd2/VDCapture/ScreenGrabber.h>
#include <vd2/Riza/opengl.h>
#include <vd2/system/binary.h>
#include <vd2/system/vdstring.h>
#include <vd2/system/error.h>
#include <vd2/system/time.h>
#include <vd2/system/profile.h>
#include <vd2/system/registry.h>
#include <vd2/system/w32assist.h>
#include <vd2/VDLib/Dialog.h>
#include <windows.h>
#include <Ks.h>
#include <Ksmedia.h>
#include <vector>
#include "cap_screen.h"
#include "resource.h"

using namespace nsVDCapture;

///////////////////////////////////////////////////////////////////////////

VDCaptureDriverScreenConfig::VDCaptureDriverScreenConfig()
	: mbTrackCursor(false)
	, mbTrackActiveWindow(false)
	, mbTrackActiveWindowClient(false)
	, mTrackOffsetX(0)
	, mTrackOffsetY(0)
	, mbDrawMousePointer(true)
	, mbRescaleImage(false)
	, mbRemoveDuplicates(true)
	, mMode(kVDCaptureDriverScreenMode_GDI)
	, mRescaleW(GetSystemMetrics(SM_CXSCREEN))
	, mRescaleH(GetSystemMetrics(SM_CYSCREEN))
{
}

void VDCaptureDriverScreenConfig::Load() {
	VDRegistryAppKey key("Capture\\Screen capture");

	mbTrackCursor = key.getBool("Track cursor", mbTrackCursor);
	mbTrackActiveWindow = key.getBool("Track active window", mbTrackActiveWindow);
	mbTrackActiveWindowClient = key.getBool("Track active window client", mbTrackActiveWindowClient);
	mbDrawMousePointer = key.getBool("Draw mouse pointer", mbDrawMousePointer);
	mbRescaleImage = key.getBool("Rescale image", mbRescaleImage);
	mbRemoveDuplicates = key.getBool("Remove duplicates", mbRemoveDuplicates);

	mMode = (VDCaptureDriverScreenMode)key.getEnumInt("Capture mode", kVDCaptureDriverScreenModeCount, mMode);

	mRescaleW = key.getInt("Rescale width", mRescaleW);
	mRescaleH = key.getInt("Rescale height", mRescaleH);

	if (mRescaleW < 1)
		mRescaleW = 1;

	if (mRescaleW > 32768)
		mRescaleW = 32768;

	if (mRescaleH < 1)
		mRescaleH = 1;

	if (mRescaleH > 32768)
		mRescaleH = 32768;

	mTrackOffsetX = key.getInt("Position X", mTrackOffsetX);
	mTrackOffsetY = key.getInt("Position Y", mTrackOffsetY);
}

void VDCaptureDriverScreenConfig::Save() {
	VDRegistryAppKey key("Capture\\Screen capture");

	key.setBool("Track cursor", mbTrackCursor);
	key.setBool("Track active window", mbTrackActiveWindow);
	key.setBool("Track active window client", mbTrackActiveWindowClient);
	key.setBool("Draw mouse pointer", mbDrawMousePointer);
	key.setBool("Rescale image", mbRescaleImage);
	key.setBool("Remove duplicates", mbRemoveDuplicates);
	key.setInt("Capture mode", mMode);
	key.setInt("Rescale width", mRescaleW);
	key.setInt("Rescale height", mRescaleH);
	key.setInt("Position X", mTrackOffsetX);
	key.setInt("Position Y", mTrackOffsetY);
}

///////////////////////////////////////////////////////////////////////////

class VDCaptureDriverScreenConfigDialog : public VDDialogFrameW32 {
public:
	VDCaptureDriverScreenConfigDialog(VDCaptureDriverScreenConfig& config);

protected:
	bool OnLoaded();
	void OnDataExchange(bool write);
	bool OnCommand(uint32 id, uint32 extcode);
	void UpdateEnables();

	VDCaptureDriverScreenConfig& mConfig;
};

VDCaptureDriverScreenConfigDialog::VDCaptureDriverScreenConfigDialog(VDCaptureDriverScreenConfig& config)
	: VDDialogFrameW32(IDD_SCREENCAP_OPTS)
	, mConfig(config)
{
}

bool VDCaptureDriverScreenConfigDialog::OnLoaded() {
	CBAddString(IDC_ACCEL_MODE, L"GDI (slowest; most compatible)");
	CBAddString(IDC_ACCEL_MODE, L"OpenGL (fast; not compatible with WDDM drivers)");
	CBAddString(IDC_ACCEL_MODE, L"DXGI (fastest; requires Win8 + WDDM 1.2 driver)");

	return VDDialogFrameW32::OnLoaded();
}

void VDCaptureDriverScreenConfigDialog::OnDataExchange(bool write) {
	ExchangeControlValueBoolCheckbox(write, IDC_DRAW_CURSOR, mConfig.mbDrawMousePointer);
	ExchangeControlValueBoolCheckbox(write, IDC_RESCALE_IMAGE, mConfig.mbRescaleImage);
	ExchangeControlValueBoolCheckbox(write, IDC_REMOVE_DUPES, mConfig.mbRemoveDuplicates);
	ExchangeControlValueUint32(write, IDC_WIDTH, mConfig.mRescaleW, 1, 32768);
	ExchangeControlValueUint32(write, IDC_HEIGHT, mConfig.mRescaleH, 1, 32768);
	ExchangeControlValueSint32(write, IDC_POSITION_X, mConfig.mTrackOffsetX, -32768, 32768);
	ExchangeControlValueSint32(write, IDC_POSITION_Y, mConfig.mTrackOffsetY, -32768, 32768);

	if (write) {
		mConfig.mbTrackCursor = IsButtonChecked(IDC_POSITION_TRACKMOUSE);

		if (IsButtonChecked(IDC_PANNING_ACTIVECLIENT)) {
			mConfig.mbTrackActiveWindow = true;
			mConfig.mbTrackActiveWindowClient = true;
		} else if (IsButtonChecked(IDC_PANNING_ACTIVEWINDOW)) {
			mConfig.mbTrackActiveWindow = true;
			mConfig.mbTrackActiveWindowClient = true;
		} else {
			mConfig.mbTrackActiveWindow = false;
			mConfig.mbTrackActiveWindowClient = false;
		}

		unsigned idx = CBGetSelectedIndex(IDC_ACCEL_MODE);
		if (idx < kVDCaptureDriverScreenModeCount)
			mConfig.mMode = (VDCaptureDriverScreenMode)idx;
	} else {
		if (mConfig.mbTrackCursor)
			CheckButton(IDC_POSITION_TRACKMOUSE, true);
		else
			CheckButton(IDC_POSITION_FIXED, true);

		if (!mConfig.mbTrackActiveWindow)
			CheckButton(IDC_PANNING_DESKTOP, true);
		else if (mConfig.mbTrackActiveWindowClient)
			CheckButton(IDC_PANNING_ACTIVECLIENT, true);
		else
			CheckButton(IDC_PANNING_ACTIVEWINDOW, true);

		CBSetSelectedIndex(IDC_ACCEL_MODE, mConfig.mMode);

		UpdateEnables();
	}
}

bool VDCaptureDriverScreenConfigDialog::OnCommand(uint32 id, uint32 extcode) {
	if (id == IDC_ACCEL_MODE || id == IDC_RESCALE_IMAGE)
		UpdateEnables();

	return false;
}

void VDCaptureDriverScreenConfigDialog::UpdateEnables() {
	bool enableOpenGL = CBGetSelectedIndex(IDC_ACCEL_MODE) != 0;
	EnableControl(IDC_STATIC_OPENGL, enableOpenGL);
	EnableControl(IDC_RESCALE_IMAGE, enableOpenGL);
	EnableControl(IDC_REMOVE_DUPES, enableOpenGL);

	bool rescale = enableOpenGL && IsButtonChecked(IDC_RESCALE_IMAGE);
	EnableControl(IDC_STATIC_WIDTH, rescale);
	EnableControl(IDC_STATIC_HEIGHT, rescale);
	EnableControl(IDC_WIDTH, rescale);
	EnableControl(IDC_HEIGHT, rescale);
}

///////////////////////////////////////////////////////////////////////////

ATOM VDCaptureDriverScreen::sWndClass;

VDCaptureDriverScreen::VDCaptureDriverScreen()
	: mhwndParent(NULL)
	, mhwnd(NULL)
	, mpGrabber(NULL)
	, mbCapBuffersInited(false)
	, mbCaptureFramePending(false)
	, mbCapturing(false)
	, mbCaptureSetup(false)
	, mbVisible(false)
	, mDisplayArea(0, 0, 0, 0)
	, mbAudioAnalysisEnabled(false)
	, mbAudioAnalysisActive(false)
	, mFramePeriod(10000000 / 30)
	, mCaptureWidth(320)
	, mCaptureHeight(240)
	, mCaptureFormat(kVDScreenGrabberFormat_XRGB32)
	, mpCB(NULL)
	, mDisplayMode(kDisplayNone)
	, mResponsivenessTimer(0)
	, mResponsivenessCounter(0)
	, mbAudioMessagePosted(false)
	, mpAudioGrabberWASAPI(NULL)
	, mPreviewFrameTimer(0)
	, mhMixer(NULL)
	, mhWaveIn(NULL)
{
	memset(mWaveBufHdrs, 0, sizeof mWaveBufHdrs);

	mAudioFormat.resize(sizeof(WAVEFORMATEX));
	mAudioFormat->wFormatTag		= WAVE_FORMAT_PCM;
	mAudioFormat->nSamplesPerSec	= 44100;
	mAudioFormat->nAvgBytesPerSec	= 44100*4;
	mAudioFormat->nChannels			= 2;
	mAudioFormat->nBlockAlign		= 4;
	mAudioFormat->wBitsPerSample	= 16;
	mAudioFormat->cbSize			= 0;
}

VDCaptureDriverScreen::~VDCaptureDriverScreen() {
	Shutdown();
}

void VDCaptureDriverScreen::SetCallback(IVDCaptureDriverCallback *pCB) {
	mpCB = pCB;
}

bool VDCaptureDriverScreen::Init(VDGUIHandle hParent) {
	mhwndParent = (HWND)hParent;

	HINSTANCE hInst = VDGetLocalModuleHandleW32();

	if (!sWndClass) {
		WNDCLASS wc = { 0, StaticWndProc, 0, sizeof(VDCaptureDriverScreen *), hInst, NULL, NULL, NULL, NULL, "Riza screencap window" };

		sWndClass = RegisterClass(&wc);

		if (!sWndClass)
			return false;
	}

	// Attempt to open mixer device. It is OK for this to fail. Note that we
	// have a bit of a problem in that (a) the mixer API doesn't take
	// WAVE_MAPPER, and (b) we can't get access to the handle that the
	// capture window creates. For now, we sort of fake it.
	InitMixerSupport();

	// Create message sink.
	if (!(mhwnd = CreateWindow((LPCTSTR)sWndClass, "", WS_POPUP, 0, 0, 0, 0, NULL, NULL, hInst, this))) {
		Shutdown();
		return false;
	}

	mPreviewFrameCount = 0;

	mbAudioHardwarePresent = false;
	mbAudioHardwarePresent = true;
	mbAudioHardwareEnabled = mbAudioHardwarePresent;
	mbAudioCaptureEnabled = true;
	mbAudioUseWASAPI = false;

	LoadSettings();

	InitVideoBuffer();

	return true;
}

void VDCaptureDriverScreen::Shutdown() {
	ShutdownWaveCapture();
	ShutdownPreviewTimer();
	ShutdownVideoBuffer();

	SaveSettings();

	if (mhwnd) {
		DestroyWindow(mhwnd);
		mhwnd = NULL;
	}

	if (mhMixer) {
		mixerClose(mhMixer);
		mhMixer = NULL;
	}
}

bool VDCaptureDriverScreen::IsHardwareDisplayAvailable() {
	return true;
}

void VDCaptureDriverScreen::SetDisplayMode(DisplayMode mode) {
	if (mode == mDisplayMode)
		return;

	mDisplayMode = mode;

	ShutdownPreviewTimer();

	if (!mpGrabber)
		return;

	mpGrabber->ShutdownDisplay();

	if (mDisplayMode && mDisplayMode != kDisplayAnalyze)
		mpGrabber->InitDisplay(mhwndParent, mDisplayMode == kDisplaySoftware);

	switch(mode) {
	case kDisplayNone:
		if (mpGrabber)
			mpGrabber->SetDisplayVisible(false);
		break;
	case kDisplayHardware:
		InitPreviewTimer();

		if (mpGrabber)
			mpGrabber->SetDisplayVisible(true);
		break;
	case kDisplaySoftware:
		InitPreviewTimer();

		if (mpGrabber)
			mpGrabber->SetDisplayVisible(true);
		break;
	case kDisplayAnalyze:
		InitPreviewTimer();

		if (mpGrabber)
			mpGrabber->SetDisplayVisible(false);
		break;
	}

	UpdateDisplay();
}

DisplayMode VDCaptureDriverScreen::GetDisplayMode() {
	return mDisplayMode;
}

void VDCaptureDriverScreen::SetDisplayRect(const vdrect32& r) {
	mDisplayArea = r;

	UpdateDisplay();
}

vdrect32 VDCaptureDriverScreen::GetDisplayRectAbsolute() {
	RECT r;
	GetWindowRect(mhwnd, &r);
	MapWindowPoints(GetParent(mhwnd), NULL, (LPPOINT)&r, 2);
	return vdrect32(r.left, r.top, r.right, r.bottom);
}

void VDCaptureDriverScreen::SetDisplayVisibility(bool vis) {
	if (vis == mbVisible)
		return;

	mbVisible = vis;
	UpdateDisplay();
}

void VDCaptureDriverScreen::SetFramePeriod(sint32 ms) {
	if (mFramePeriod == ms)
		return;

	mFramePeriod = ms;

	ShutdownPreviewTimer();

	if (mDisplayMode)
		InitPreviewTimer();

	if (mpCB)
		mpCB->CapEvent(kEventVideoFrameRateChanged, 0);
}

sint32 VDCaptureDriverScreen::GetFramePeriod() {
	return mFramePeriod;
}

uint32 VDCaptureDriverScreen::GetPreviewFrameCount() {
	if (mDisplayMode == kDisplaySoftware || mDisplayMode == kDisplayAnalyze)
		return mPreviewFrameCount;

	return 0;
}

bool VDCaptureDriverScreen::GetVideoFormat(vdstructex<BITMAPINFOHEADER>& vformat) {
	vformat.resize(sizeof(BITMAPINFOHEADER));
	vformat->biSize = sizeof(BITMAPINFOHEADER);
	vformat->biWidth = mCaptureWidth;
	vformat->biHeight = mCaptureHeight;
	vformat->biPlanes = 1;

	switch(mCaptureFormat) {
		case kVDScreenGrabberFormat_XRGB32:
		default:
			vformat->biBitCount = 32;
			vformat->biCompression = BI_RGB;
			vformat->biSizeImage = mCaptureWidth * mCaptureHeight * 4;
			break;

		case kVDScreenGrabberFormat_YUY2:
			vformat->biBitCount = 16;
			vformat->biCompression = VDMAKEFOURCC('Y', 'U', 'Y', '2');
			vformat->biSizeImage = mCaptureWidth * mCaptureHeight * 2;
			break;

		case kVDScreenGrabberFormat_YV12:
			vformat->biBitCount = 12;
			vformat->biCompression = VDMAKEFOURCC('Y', 'V', '1', '2');
			vformat->biSizeImage = mCaptureWidth * mCaptureHeight * 3 / 2;
			break;
	}

	vformat->biXPelsPerMeter = 0;
	vformat->biYPelsPerMeter = 0;
	vformat->biClrUsed = 0;
	vformat->biClrImportant = 0;
	return true;
}

bool VDCaptureDriverScreen::SetVideoFormat(const BITMAPINFOHEADER *pbih, uint32 size) {
	bool success = false;

	if (pbih->biCompression == VDMAKEFOURCC('Y', 'U', 'Y', '2')) {
		if (mConfig.mMode == kVDCaptureDriverScreenMode_GDI)
			return false;

		if ((pbih->biWidth & 1))
			return false;

		mCaptureFormat = kVDScreenGrabberFormat_YUY2;
	} else if (pbih->biCompression == VDMAKEFOURCC('Y', 'V', '1', '2')) {
		if (mConfig.mMode == kVDCaptureDriverScreenMode_GDI)
			return false;

		if ((pbih->biWidth & 7) || (pbih->biHeight & 1))
			return false;

		mCaptureFormat = kVDScreenGrabberFormat_YV12;
	} else if (pbih->biCompression == BI_RGB) {
		if (pbih->biBitCount != 32)
			return false;

		mCaptureFormat = kVDScreenGrabberFormat_XRGB32;
	} else {
		return false;
	}

	mCaptureWidth = pbih->biWidth;
	mCaptureHeight = pbih->biHeight;

	ShutdownVideoBuffer();

	if (mpCB)
		mpCB->CapEvent(kEventVideoFormatChanged, 0);

	InitVideoBuffer();
	success = true;
	return success;
}

int VDCaptureDriverScreen::GetAudioDeviceCount() {
	return mbAudioHardwarePresent ? VDIsAtLeastVistaW32() ? 2 : 1 : 0;
}

const wchar_t *VDCaptureDriverScreen::GetAudioDeviceName(int idx) {
	if (idx >= 2 || !mbAudioHardwarePresent)
		return NULL;

	if (idx == 1 && !VDIsAtLeastVistaW32())
		return NULL;

	if (idx == 0)
		return L"Wave Mapper (MMSystem)";
	else
		return L"Default output loopback (Core Audio)";
}

bool VDCaptureDriverScreen::SetAudioDevice(int idx) {
	if (idx < -1 || idx >= 2)
		return false;

	if (!VDIsAtLeastVistaW32() && idx == 1)
		return false;
	
	if (!idx && !mbAudioHardwarePresent)
		return false;

	bool enable = idx >= 0;
	bool wasapi = idx == 1;

	if (enable == mbAudioHardwareEnabled && wasapi == mbAudioUseWASAPI)
		return true;

	ShutdownWaveCapture();

	mbAudioHardwareEnabled = enable;
	mbAudioUseWASAPI = wasapi;

	if (mbAudioAnalysisEnabled)
		InitWaveCapture();

	return true;
}

int VDCaptureDriverScreen::GetAudioDeviceIndex() {
	return mbAudioHardwareEnabled ? mbAudioUseWASAPI ? 1 : 0 : -1;
}

int VDCaptureDriverScreen::GetVideoSourceCount() {
	return 0;
}

const wchar_t *VDCaptureDriverScreen::GetVideoSourceName(int idx) {
	return NULL;
}

bool VDCaptureDriverScreen::SetVideoSource(int idx) {
	return idx == -1;
}

int VDCaptureDriverScreen::GetVideoSourceIndex() {
	return -1;
}

int VDCaptureDriverScreen::GetAudioSourceCount() {
	return 0;
}

const wchar_t *VDCaptureDriverScreen::GetAudioSourceName(int idx) {
	return NULL;
}

bool VDCaptureDriverScreen::SetAudioSource(int idx) {
	return idx == -1;
}

int VDCaptureDriverScreen::GetAudioSourceIndex() {
	return -1;
}

int VDCaptureDriverScreen::GetAudioInputCount() {
	return mbAudioHardwareEnabled ? mMixerInputs.size() : 0;
}

const wchar_t *VDCaptureDriverScreen::GetAudioInputName(int idx) {
	if (!mbAudioHardwareEnabled || (unsigned)idx >= mMixerInputs.size())
		return NULL;

	MixerInputs::const_iterator it(mMixerInputs.begin());

	std::advance(it, idx);

	return (*it).c_str();
}

bool VDCaptureDriverScreen::SetAudioInput(int idx) {
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

int VDCaptureDriverScreen::GetAudioInputIndex() {
	return mbAudioHardwareEnabled ? mMixerInput : -1;
}

bool VDCaptureDriverScreen::IsAudioCapturePossible() {
	return mbAudioHardwareEnabled;
}

bool VDCaptureDriverScreen::IsAudioCaptureEnabled() {
	return mbAudioHardwareEnabled && mbAudioCaptureEnabled;
}

void VDCaptureDriverScreen::SetAudioCaptureEnabled(bool b) {
	mbAudioCaptureEnabled = b;
}

void VDCaptureDriverScreen::SetAudioAnalysisEnabled(bool b) {
	if (mbAudioAnalysisEnabled == b)
		return;

	mbAudioAnalysisEnabled = b;

	if (mbAudioAnalysisEnabled)
		InitWaveCapture();
	else
		ShutdownWaveCapture();
}

void VDCaptureDriverScreen::GetAvailableAudioFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats) {
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

	 GetExtraFormats(aformats);
}

void VDCaptureDriverScreen::GetExtraFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats) {
	static const int kSamplingRates[]={
		48000,
		96000,
		192000
	};

	static const int kChannelCounts[]={
		4,
		6,
		8
	};

	static const int kSampleDepths[]={
		16
	};

	for(int sridx=0; sridx < sizeof kSamplingRates / sizeof kSamplingRates[0]; ++sridx)
		for(int chidx=0; chidx < sizeof kChannelCounts / sizeof kChannelCounts[0]; ++chidx)
			for(int sdidx=0; sdidx < sizeof kSampleDepths / sizeof kSampleDepths[0]; ++sdidx) {
				WAVEFORMATEXTENSIBLE wfex={
					WAVE_FORMAT_EXTENSIBLE,
					kChannelCounts[chidx],
					kSamplingRates[sridx],
					0,
					0,
					kSampleDepths[sdidx],
					0
				};

				wfex.Format.nBlockAlign = wfex.Format.nChannels * (wfex.Format.wBitsPerSample >> 3);
				wfex.Format.nAvgBytesPerSec = wfex.Format.nBlockAlign * wfex.Format.nSamplesPerSec;
				wfex.Samples.wValidBitsPerSample = wfex.Format.wBitsPerSample;
				wfex.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
				wfex.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX);

				int r1 = WAVERR_BADFORMAT;
				int r = waveInOpen(NULL, WAVE_MAPPER, (WAVEFORMATEX*)&wfex, 0, 0, WAVE_FORMAT_QUERY | WAVE_FORMAT_DIRECT);

				if (r==MMSYSERR_NOERROR) {
					aformats.push_back(vdstructex<WAVEFORMATEX>((WAVEFORMATEX*)&wfex, sizeof wfex));
				}
			}
}

bool VDCaptureDriverScreen::GetAudioFormat(vdstructex<WAVEFORMATEX>& aformat) {
	aformat = mAudioFormat;
	return true;
}

bool VDCaptureDriverScreen::SetAudioFormat(const WAVEFORMATEX *pwfex, uint32 size) {
	if (!mbAudioHardwareEnabled)
		return false;

	ShutdownWaveCapture();
	mAudioFormat.assign(pwfex, size);
	if (mbAudioAnalysisEnabled)
		InitWaveCapture();
	return true;
}

bool VDCaptureDriverScreen::IsDriverDialogSupported(DriverDialog dlg) {
	if (dlg == kDialogVideoSource)
		return true;

	return false;
}

void VDCaptureDriverScreen::DisplayDriverDialog(DriverDialog dlg) {
	VDASSERT(IsDriverDialogSupported(dlg));

	switch(dlg) {
	case kDialogVideoFormat:
		break;
	case kDialogVideoSource:
		ShutdownVideoBuffer();

		{
			VDCaptureDriverScreenConfig cfg(mConfig);
			VDCaptureDriverScreenConfigDialog dlg(cfg);

			if (dlg.ShowDialog((VDGUIHandle)mhwndParent)) {
				mConfig = cfg;
				mConfig.Save();
			}
		}

		// without OpenGL or DXGI, we can only do 32-bit RGB
		if (mConfig.mMode == kVDCaptureDriverScreenMode_GDI && mCaptureFormat != kVDScreenGrabberFormat_XRGB32) {
			mCaptureFormat = kVDScreenGrabberFormat_XRGB32;
			mpCB->CapEvent(kEventVideoFormatChanged, 0);
		}

		InitVideoBuffer();
		break;
	case kDialogVideoDisplay:
		break;
	}
}

bool VDCaptureDriverScreen::CaptureStart() {
	if (!mpGrabber) {
		// Okay, if for some reason we couldn't init the grabber, we try one last ditch attempt.
		if (!InitVideoBuffer()) {
			throw MyError("Unable to initialize screen capture. Check settings under Video Source.");
		}
	}

	ShutdownWaveCapture();

	if (!VDINLINEASSERTFALSE(mbCapturing)) {
		if (!mResponsivenessTimer)
			mResponsivenessTimer = SetTimer(mhwnd, kResponsivenessTimerID, 500, NULL);

		mResponsivenessCounter = GetTickCount();

		if (mpCB->CapEvent(kEventPreroll, 0)) {
			mpCB->CapBegin(0);
			if (mbAudioCaptureEnabled)
				InitWaveCapture();

			ShutdownPreviewTimer();

			mGlobalTimeBase = VDGetAccurateTick();
			mVideoTimeBase = mpGrabber->GetCurrentTimestamp();
			mbCapturing = true;
			mbCaptureSetup = true;
			mCaptureTimer.Init2(this, mFramePeriod);
		} else {
			if (mbAudioAnalysisEnabled)
				InitWaveCapture();
		}
	}

	return mbCapturing;
}

void VDCaptureDriverScreen::CaptureStop() {
	SendMessage(mhwnd, WM_APP+16, 0, 0);
}

void VDCaptureDriverScreen::CaptureAbort() {
	SendMessage(mhwnd, WM_APP+17, 0, 0);
	mbCapturing = false;
}

void VDCaptureDriverScreen::ReceiveFrame(uint64 timestamp, const void *data, ptrdiff_t pitch, uint32 rowlen, uint32 rowcnt) {
	if (!mbCapturing && mDisplayMode != kDisplayAnalyze)
		return;

	if (!mpCB)
		return;

	if (data) {
		mLinearizationBuffer.resize(rowlen * rowcnt);

		VDMemcpyRect(mLinearizationBuffer.data(), rowlen, data, pitch, rowlen, rowcnt);
	} else {
		if (mConfig.mbRemoveDuplicates || mLinearizationBuffer.empty())
			return;
	}

	try {
		if (mbCapturing)
			mpCB->CapProcessData(0, mLinearizationBuffer.data(), (uint32)mLinearizationBuffer.size(), mpGrabber->ConvertTimestampDelta(timestamp, mVideoTimeBase), true, ComputeGlobalTime());
		else
			mpCB->CapProcessData(-1, mLinearizationBuffer.data(), (uint32)mLinearizationBuffer.size(), -1, true, -1);
	} catch(MyError& e) {
		if (mCaptureError.empty())
			mCaptureError.TransferFrom(e);

		CaptureAbort();
	}
}

void VDCaptureDriverScreen::ReceiveAudioDataWASAPI() {
	if (mbAudioMessagePosted.compareExchange(true, false) == false)
		::PostMessage(this->mhwnd, WM_APP + 19, 0, 0);
}

void VDCaptureDriverScreen::SyncCaptureStop() {
	if (mbCaptureSetup) {
		mCaptureTimer.Shutdown();
		mbCapturing = false;
		mbCaptureSetup = false;

		if (mCaptureError.gets()) {
			mpCB->CapEnd(&mCaptureError);
			mCaptureError.discard();
		} else
			mpCB->CapEnd(NULL);

		if (mbAudioCaptureEnabled)
			ShutdownWaveCapture();

		if (mbAudioAnalysisEnabled)
			InitWaveCapture();

		if (mDisplayMode)
			InitPreviewTimer();

		if (mResponsivenessTimer) {
			KillTimer(mhwnd, mResponsivenessTimer);
			mResponsivenessTimer = 0;
		}
	}
}

void VDCaptureDriverScreen::SyncCaptureAbort() {
	SyncCaptureStop();
}

void VDCaptureDriverScreen::InitMixerSupport() {
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
					if (MMSYSERR_NOERROR == res) {
						if (!(mMixerInputControl.fdwControl & MIXERCONTROL_CONTROLF_MULTIPLE))
							res = MMSYSERR_ERROR;
					}

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

void VDCaptureDriverScreen::ShutdownMixerSupport() {
	if (mhMixer) {
		mixerClose(mhMixer);
		mhMixer = NULL;
	}
}

bool VDCaptureDriverScreen::InitWaveCapture() {
	if (!mbAudioHardwareEnabled)
		return false;

	if (mbAudioUseWASAPI) {
		mpAudioGrabberWASAPI = new VDAudioGrabberWASAPI;

		if (!mpAudioGrabberWASAPI->Init(this) || !mpAudioGrabberWASAPI->Start()) {
			ShutdownWaveCapture();
			return false;
		}

		vdstructex<WAVEFORMATEX> aformat;
		mpAudioGrabberWASAPI->GetFormat(aformat);

		if (aformat != mAudioFormat) {
			mAudioFormat = aformat;
		}
	} else {
		vdstructex<WAVEFORMATEX> aformat;

		if (!GetAudioFormat(aformat))
			return false;

		uint32	blockSize = (aformat->nAvgBytesPerSec + 9) / 10 + aformat->nBlockAlign - 1;
		blockSize -= blockSize % aformat->nBlockAlign;

		mWaveBuffer.resize(blockSize*2);

		if (MMSYSERR_NOERROR != waveInOpen(&mhWaveIn, WAVE_MAPPER, aformat.data(), (DWORD_PTR)mhwnd, 0, CALLBACK_WINDOW | WAVE_FORMAT_DIRECT))
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
				ShutdownWaveCapture();
				return false;
			}

			if (MMSYSERR_NOERROR != waveInAddBuffer(mhWaveIn, &hdr, sizeof(WAVEHDR))) {
				ShutdownWaveCapture();
				return false;
			}
		}

		if (MMSYSERR_NOERROR != waveInStart(mhWaveIn)) {
			ShutdownWaveCapture();
			return false;
		}
	}

	return true;
}

void VDCaptureDriverScreen::ShutdownWaveCapture() {
	// prevent receiving data message
	mbAudioMessagePosted = 0;

	if (mpAudioGrabberWASAPI) {
		mpAudioGrabberWASAPI->Shutdown();
		delete mpAudioGrabberWASAPI;
		mpAudioGrabberWASAPI = NULL;
	}

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

bool VDCaptureDriverScreen::InitVideoBuffer() {
	ShutdownVideoBuffer();

	bool canRescale = true;
	switch(mConfig.mMode) {
		case kVDCaptureDriverScreenMode_GDI:
		default:
			mpGrabber = VDCreateScreenGrabberGDI();
			canRescale = false;
			break;

		case kVDCaptureDriverScreenMode_OpenGL:
			mpGrabber = VDCreateScreenGrabberGL();
			break;

		case kVDCaptureDriverScreenMode_DXGI12:
			mpGrabber = VDCreateScreenGrabberDXGI12();
			break;
	}

	if (!mpGrabber->Init(this)) {
		ShutdownVideoBuffer();
		return false;
	}

	uint32 srcW = mCaptureWidth;
	uint32 srcH = mCaptureHeight;

	if (mConfig.mbRescaleImage && canRescale) {
		srcW = mConfig.mRescaleW;
		srcH = mConfig.mRescaleH;
	}

	if (!mpGrabber->InitCapture(srcW, srcH, mCaptureWidth, mCaptureHeight, mCaptureFormat)) {
		ShutdownVideoBuffer();
		return false;
	}

	mpGrabber->SetCapturePointer(mConfig.mbDrawMousePointer);

	if (mDisplayMode && mDisplayMode != kDisplayAnalyze && !mpGrabber->InitDisplay(mhwndParent, mDisplayMode == kDisplaySoftware)) {
		ShutdownVideoBuffer();
		return false;
	}

	mbCapBuffersInited = true;
	UpdateDisplay();
	return true;
}

void VDCaptureDriverScreen::ShutdownVideoBuffer() {
	mbCapBuffersInited = false;

	delete mpGrabber;
	mpGrabber = NULL;
}

void VDCaptureDriverScreen::UpdateDisplay() {
	if (!mpGrabber)
		return;

	if (!mbVisible)
		mpGrabber->SetDisplayVisible(false);

	mpGrabber->SetDisplayArea(mDisplayArea);

	switch(mDisplayMode) {
		case kDisplayHardware:
		case kDisplaySoftware:
			if (mbVisible)
				mpGrabber->SetDisplayVisible(true);
			break;
	}
}

sint64 VDCaptureDriverScreen::ComputeGlobalTime() {
	sint64 tickDelta = (sint64)VDGetAccurateTick() - (sint64)mGlobalTimeBase;
	if (tickDelta < 0)
		tickDelta = 0;
	return (sint64)((uint64)tickDelta * 1000);
}

void VDCaptureDriverScreen::DoFrame() {
	if (!mbCapBuffersInited)
		return;

	UpdateTracking();

	mpGrabber->AcquireFrame(mbCapturing || mDisplayMode == kDisplayAnalyze);
}

void VDCaptureDriverScreen::UpdateTracking() {
	int w = mCaptureWidth;
	int h = mCaptureHeight;
	int srcw = w;
	int srch = h;

	if (mConfig.mbRescaleImage) {
		srcw = mConfig.mRescaleW;
		srch = mConfig.mRescaleH;
	}

	int x = 0;
	int y = 0;

	if (mConfig.mbTrackCursor) {
		POINT pt;

		if (GetCursorPos(&pt)) {
			x = pt.x - ((w+1) >> 1);
			y = pt.y - ((h+1) >> 1);
		}
	} else {
		x = mConfig.mTrackOffsetX;
		y = mConfig.mTrackOffsetY;
	}

	if (mConfig.mbTrackActiveWindow) {
		HWND hwndFore = GetForegroundWindow();

		if (hwndFore) {
			RECT r;
			bool success = false;

			if (mConfig.mbTrackActiveWindowClient) {
				if (GetClientRect(hwndFore, &r)) {
					if (MapWindowPoints(hwndFore, NULL, (LPPOINT)&r, 2))
						success = true;
				}
			} else {
				if (GetWindowRect(hwndFore, &r))
					success = true;
			}

			if (success) {
				if (!mConfig.mbTrackCursor) {
					x = r.left + mConfig.mTrackOffsetX;
					y = r.left + mConfig.mTrackOffsetY;
				}

				if (x > r.right - srcw)
					x = r.right - srcw;
				if (x < r.left)
					x = r.left;
				if (y > r.bottom - srch)
					y = r.bottom - srch;
				if (y < r.top)
					y = r.top;
			}
		}
	}

	if (mpGrabber)
		mpGrabber->SetCaptureOffset(x, y);
}

void VDCaptureDriverScreen::DispatchFrame(const void *data, uint32 size, sint64 timestamp) {
	if (!mpCB)
		return;

	if (mDisplayMode == kDisplayAnalyze && size) {
		try {
			mpCB->CapProcessData(-1, data, size, 0, true, 0);
		} catch(MyError&) {
			// Eat preview errors.
		}
	}

	if (mbCapturing) {
		try {
			if (mpCB->CapEvent(kEventCapturing, 0))
				mpCB->CapProcessData(0, data, size, timestamp, true, timestamp);
			else {
				mbCapturing = false;
				CaptureAbort();
			}

		} catch(MyError& e) {
			mCaptureError.TransferFrom(e);
			CaptureAbort();
			mbCapturing = false;
		}
	}
}

void VDCaptureDriverScreen::InitPreviewTimer() {
	ShutdownPreviewTimer();

	mPreviewFrameTimer = SetTimer(mhwnd, kPreviewTimerID, mFramePeriod / 10000, NULL);
}

void VDCaptureDriverScreen::ShutdownPreviewTimer() {
	if (mPreviewFrameTimer) {
		KillTimer(mhwnd, mPreviewFrameTimer);
		mPreviewFrameTimer = 0;
	}
}

void VDCaptureDriverScreen::LoadSettings() {
	mConfig.Load();
}

void VDCaptureDriverScreen::SaveSettings() {
	mConfig.Save();
}

void VDCaptureDriverScreen::TimerCallback() {
	if (GetTickCount() - mResponsivenessCounter < 2000) {
		mbCaptureFramePending = true;
		PostMessage(mhwnd, WM_APP+18, 0, 0);
	}
}

LRESULT CALLBACK VDCaptureDriverScreen::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_NCCREATE:
			SetWindowLongPtr(hwnd, 0, (LONG_PTR)((LPCREATESTRUCT)lParam)->lpCreateParams);
			break;
		case MM_WIM_DATA:
			{
				VDCaptureDriverScreen *pThis = (VDCaptureDriverScreen *)GetWindowLongPtr(hwnd, 0);

				if (pThis->mpCB) {
					WAVEHDR& hdr = *(WAVEHDR *)lParam;

					if (pThis->mbCapturing) {
						if (pThis->mbAudioCaptureEnabled) {
							try {
								pThis->mpCB->CapProcessData(1, hdr.lpData, hdr.dwBytesRecorded, -1, false, pThis->ComputeGlobalTime());
							} catch(MyError& e) {
								pThis->mCaptureError.TransferFrom(e);
								pThis->CaptureAbort();
							}

							waveInAddBuffer(pThis->mhWaveIn, &hdr, sizeof(WAVEHDR));
						}
					} else if (pThis->mbAudioAnalysisActive) {
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
				VDCaptureDriverScreen *pThis = (VDCaptureDriverScreen *)GetWindowLongPtr(hwnd, 0);
				pThis->SyncCaptureStop();
			}
			return 0;
		case WM_APP+17:
			{
				VDCaptureDriverScreen *pThis = (VDCaptureDriverScreen *)GetWindowLongPtr(hwnd, 0);
				pThis->SyncCaptureAbort();
			}
			return 0;

		case WM_APP+18:
			{
				VDCaptureDriverScreen *pThis = (VDCaptureDriverScreen *)GetWindowLongPtr(hwnd, 0);
				if (pThis->mbCaptureFramePending && pThis->mbCapturing) {
					pThis->mbCaptureFramePending = false;

					try {
						pThis->DoFrame();
					} catch(MyError& e) {
						if (pThis->mCaptureError.empty())
							pThis->mCaptureError.TransferFrom(e);

						pThis->CaptureAbort();
					}
				}
			}
			return 0;

		case WM_APP+19:
			{
				VDCaptureDriverScreen *pThis = (VDCaptureDriverScreen *)GetWindowLongPtr(hwnd, 0);

				if (pThis->mbAudioMessagePosted.compareExchange(false, true) == (int)true) {
					uint8 buf[4096];

					try {
						sint64 globalTime = -1;
						if (pThis->mbCapturing)
							globalTime = pThis->ComputeGlobalTime();

						for(;;) {
							uint32 actual = pThis->mpAudioGrabberWASAPI->ReadData(buf, (sizeof buf) - (sizeof buf) % pThis->mAudioFormat->nBlockAlign);

							if (!actual)
								break;

							if (pThis->mbCapturing)
								pThis->mpCB->CapProcessData(1, buf, actual, globalTime, true, globalTime);
							else
								pThis->mpCB->CapProcessData(-2, buf, actual, -1, true, -1);
						}
					} catch(MyError& e) {
						if (pThis->mCaptureError.empty())
							pThis->mCaptureError.TransferFrom(e);

						pThis->CaptureAbort();
					}
				}
			}
			return 0;

		case WM_TIMER:
			{
				VDCaptureDriverScreen *pThis = (VDCaptureDriverScreen *)GetWindowLongPtr(hwnd, 0);
				if (wParam == kPreviewTimerID) {
					if (!pThis->mbCapturing && pThis->mpGrabber) {
						pThis->UpdateTracking();

						try {
							pThis->mpGrabber->AcquireFrame(pThis->mDisplayMode == kDisplayAnalyze);
						} catch(const MyError&) {
							// eat the error for preview :-/
						}
					}
				} else if (wParam == kResponsivenessTimerID)
					pThis->mResponsivenessCounter = GetTickCount();

			}
			return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

///////////////////////////////////////////////////////////////////////////

class VDCaptureSystemScreen : public IVDCaptureSystem {
public:
	VDCaptureSystemScreen();
	~VDCaptureSystemScreen();

	void EnumerateDrivers();

	int GetDeviceCount();
	const wchar_t *GetDeviceName(int index);

	IVDCaptureDriver *CreateDriver(int deviceIndex);
};

IVDCaptureSystem *VDCreateCaptureSystemScreen() {
	return new VDCaptureSystemScreen;
}

VDCaptureSystemScreen::VDCaptureSystemScreen()
{
}

VDCaptureSystemScreen::~VDCaptureSystemScreen() {
}

void VDCaptureSystemScreen::EnumerateDrivers() {
}

int VDCaptureSystemScreen::GetDeviceCount() {
	return 1;
}

const wchar_t *VDCaptureSystemScreen::GetDeviceName(int index) {
	return !index ? L"Screen capture" : NULL;
}

IVDCaptureDriver *VDCaptureSystemScreen::CreateDriver(int index) {
	if (index)
		return NULL;

	return new VDCaptureDriverScreen();
}
