#ifndef f_VD2_VDCAPTURE_CAP_SCREEN_H
#define f_VD2_VDCAPTURE_CAP_SCREEN_H

#include <vd2/system/atomic.h>
#include <vd2/VDCapture/capdriver.h>
#include <vd2/VDCapture/ScreenGrabber.h>
#include <vd2/VDCapture/AudioGrabberWASAPI.h>

class VDAudioGrabberWASAPI;

enum VDCaptureDriverScreenMode {
	kVDCaptureDriverScreenMode_GDI,
	kVDCaptureDriverScreenMode_OpenGL,
	kVDCaptureDriverScreenMode_DXGI12,
	kVDCaptureDriverScreenModeCount
};

struct VDCaptureDriverScreenConfig {
	bool	mbTrackCursor;
	bool	mbTrackActiveWindow;
	bool	mbTrackActiveWindowClient;
	bool	mbDrawMousePointer;
	bool	mbRescaleImage;
	bool	mbRemoveDuplicates;
	VDCaptureDriverScreenMode mMode;
	uint32	mRescaleW;
	uint32	mRescaleH;
	sint32	mTrackOffsetX;
	sint32	mTrackOffsetY;

	VDCaptureDriverScreenConfig();

	void Load();
	void Save();
};

class VDCaptureDriverScreen : public IVDCaptureDriver, public IVDScreenGrabberCallback, IVDTimerCallback, IVDAudioGrabberCallbackWASAPI {
	VDCaptureDriverScreen(const VDCaptureDriverScreen&);
	VDCaptureDriverScreen& operator=(const VDCaptureDriverScreen&);
public:
	VDCaptureDriverScreen();
	~VDCaptureDriverScreen();

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
	nsVDCapture::TunerInputMode	GetTunerInputMode() { return nsVDCapture::kTunerInputUnknown; }
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
	void	GetExtraFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats);

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
	virtual void ReceiveFrame(uint64 timestamp, const void *data, ptrdiff_t pitch, uint32 rowlen, uint32 rowcnt);

protected:
	virtual void ReceiveAudioDataWASAPI();

protected:
	void	SyncCaptureStop();
	void	SyncCaptureAbort();
	void	InitMixerSupport();
	void	ShutdownMixerSupport();
	bool	InitWaveCapture();
	void	ShutdownWaveCapture();
	bool	InitVideoBuffer();
	void	ShutdownVideoBuffer();
	void	UpdateDisplay();
	sint64	ComputeGlobalTime();
	void	DoFrame();
	void	UpdateTracking();
	void	DispatchFrame(const void *data, uint32 size, sint64 timestamp);

	void	TimerCallback();

	void	InitPreviewTimer();
	void	ShutdownPreviewTimer();

	void	LoadSettings();
	void	SaveSettings();

	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	enum { kPreviewTimerID = 100, kResponsivenessTimerID = 101 };

	HWND	mhwndParent;
	HWND	mhwnd;

	IVDScreenGrabber *mpGrabber;
	bool	mbCapBuffersInited;

	VDAtomicInt	mbCaptureFramePending;
	bool	mbCapturing;
	bool	mbCaptureSetup;
	bool	mbVisible;
	bool	mbAudioHardwarePresent;
	bool	mbAudioHardwareEnabled;
	bool	mbAudioCaptureEnabled;
	bool	mbAudioUseWASAPI;
	bool	mbAudioAnalysisEnabled;
	bool	mbAudioAnalysisActive;

	VDCaptureDriverScreenConfig	mConfig;

	uint32	mFramePeriod;

	uint32	mCaptureWidth;
	uint32	mCaptureHeight;
	VDScreenGrabberFormat mCaptureFormat;

	vdstructex<WAVEFORMATEX>		mAudioFormat;

	IVDCaptureDriverCallback	*mpCB;

	nsVDCapture::DisplayMode	mDisplayMode;
	vdrect32	mDisplayArea;

	uint32	mGlobalTimeBase;
	uint64	mVideoTimeBase;

	UINT	mResponsivenessTimer;
	VDAtomicInt	mResponsivenessCounter;

	VDAtomicInt		mbAudioMessagePosted;

	VDAudioGrabberWASAPI *mpAudioGrabberWASAPI;

	UINT	mPreviewFrameTimer;
	VDAtomicInt	mPreviewFrameCount;

	VDCallbackTimer	mCaptureTimer;

	HMIXER	mhMixer;
	int		mMixerInput;
	MIXERCONTROL	mMixerInputControl;
	typedef std::vector<VDStringW>	MixerInputs;
	MixerInputs	mMixerInputs;

	HWAVEIN mhWaveIn;
	WAVEHDR	mWaveBufHdrs[2];
	vdblock<char>	mWaveBuffer;

	vdblock<uint8, vdaligned_alloc<uint8> > mLinearizationBuffer;

	MyError	mCaptureError;

	VDRTProfileChannel mProfileChannel;

	static ATOM sWndClass;
};

#endif
