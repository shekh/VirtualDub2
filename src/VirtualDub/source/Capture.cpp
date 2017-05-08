//	VirtualDub - Video processing and capture application
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

#include <malloc.h>
#include <ctype.h>
#include <process.h>

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <vfw.h>
#include <shellapi.h>

#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/int128.h>
#include <vd2/system/registry.h>
#include <vd2/system/thread.h>
#include <vd2/system/profile.h>
#include <vd2/system/tls.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/w32assist.h>
#include "AVIOutput.h"
#include "AVIOutputFile.h"
#include "FilterSystem.h"
#include "AVIOutputStriped.h"
#include "AVIStripeSystem.h"
#include <vd2/Dita/services.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <../Kasumi/h/uberblit_rgb64.h>
#include <../Kasumi/h/uberblit_16f.h>
#include <vd2/Riza/bitmap.h>
#include <vd2/Riza/videocodec.h>
#include <vd2/VDCapture/capdriver.h>
#include <vd2/VDCapture/capdrivers.h>
#include <vd2/VDCapture/capresync.h>
#include <vd2/VDCapture/caplog.h>
#include <vd2/VDCapture/capaudiocomp.h>
#include <vd2/VDCapture/cap_dshow.h>

#include "VideoSequenceCompressor.h"
#include "crash.h"
#include "gui.h"
#include "oshelper.h"
#include "filters.h"
#include "capture.h"
#include "helpfile.h"
#include "resource.h"
#include "prefs.h"
#include "misc.h"
#include "capspill.h"
#include "caphisto.h"
#include "optdlg.h"
#include "filtdlg.h"
#include "caputils.h"
#include "capfilter.h"
#include "capvumeter.h"
#include "uiframe.h"

using namespace nsVDCapture;

///////////////////////////////////////////////////////////////////////////
//
//	externs
//
///////////////////////////////////////////////////////////////////////////

extern HINSTANCE g_hInst;
extern const char g_szError[];
extern VDFilterChainDesc g_filterChain;
extern long g_lSpillMinSize;
extern long g_lSpillMaxSize;
extern HWND			g_hWnd;

extern void CaptureBT848Reassert();

IVDCaptureSystem *VDCreateCaptureSystemEmulation();

IVDCaptureProfiler *g_pCaptureProfiler;		// a bit of a cheat for now

extern void VDPreferencesGetAVIIndexingLimits(uint32& superindex, uint32& subindex);

///////////////////////////////////////////////////////////////////////////
//
//	structs
//
///////////////////////////////////////////////////////////////////////////

#define VDCM_EXIT		(WM_APP+0)
#define VDCM_SWITCH_FIN (WM_APP+1)

class VDCaptureProject;

class VDCaptureStatsFilter : public IVDCaptureDriverCallback {
public:
	VDCaptureStatsFilter();

	void Init(IVDCaptureDriverCallback *pCB, const WAVEFORMATEX *pwfex);
	void GetStats(VDCaptureStatus& stats);

	void CapBegin(sint64 global_clock);
	void CapEnd(const MyError *pError);
	bool CapEvent(DriverEvent event, int data);
	void CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock);

protected:
	IVDCaptureDriverCallback *mpCB;

	long		mVideoFramesCaptured;
	long		mVideoFirstCapTime, mVideoLastCapTime;
	long		mAudioFirstCapTime, mAudioLastCapTime;
	sint32		mAudioFirstSize;
	sint64		mTotalAudioDataSize;

	double		mAudioSamplesPerByteX1000;

	// audio sampling rate estimation
	VDCaptureAudioRateEstimator	mAudioRateEstimator;
	VDCaptureAudioRateEstimator	mAudioRelativeRateEstimator;

	VDCriticalSection	mcsLock;
};

VDCaptureStatsFilter::VDCaptureStatsFilter()
	: mpCB(NULL)
{
}

void VDCaptureStatsFilter::Init(IVDCaptureDriverCallback *pCB, const WAVEFORMATEX *pwfex) {
	mpCB = pCB;

	mAudioSamplesPerByteX1000 = 0.0;
	if (pwfex)
		mAudioSamplesPerByteX1000 = 1000.0 * ((double)pwfex->nSamplesPerSec / (double)pwfex->nAvgBytesPerSec);
}

void VDCaptureStatsFilter::GetStats(VDCaptureStatus& status) {
	vdsynchronized(mcsLock) {
		status.mVideoFirstFrameTimeMS	= mVideoFirstCapTime;
		status.mVideoLastFrameTimeMS	= mVideoLastCapTime;
		status.mAudioFirstFrameTimeMS	= mAudioFirstCapTime;
		status.mAudioLastFrameTimeMS	= mAudioLastCapTime;
		status.mAudioFirstSize			= mAudioFirstSize;
		status.mTotalAudioDataSize		= mTotalAudioDataSize;

		// slope is in (bytes/ms), which we must convert to samples/sec.
		double slope;
		status.mActualAudioHz = 0;
		if (mAudioRateEstimator.GetSlope(slope))
			status.mActualAudioHz = slope * mAudioSamplesPerByteX1000;

		status.mRelativeAudioHz = 0;
		if (mAudioRelativeRateEstimator.GetSlope(slope))
			status.mRelativeAudioHz = slope * mAudioSamplesPerByteX1000;
	}
}

void VDCaptureStatsFilter::CapBegin(sint64 global_clock) {
	mVideoFramesCaptured	= 0;
	mVideoFirstCapTime		= 0;
	mVideoLastCapTime		= 0;
	mAudioFirstCapTime		= 0;
	mAudioLastCapTime		= 0;
	mAudioFirstSize			= 0;
	mTotalAudioDataSize		= 0;

	mAudioRateEstimator.Reset();
	mAudioRelativeRateEstimator.Reset();

	if (mpCB)
		mpCB->CapBegin(global_clock);
}

void VDCaptureStatsFilter::CapEnd(const MyError *pError) {
	if (mpCB)
		mpCB->CapEnd(pError);
}

bool VDCaptureStatsFilter::CapEvent(DriverEvent event, int data) {
	if (mpCB)
		return mpCB->CapEvent(event, data);

	return true;
}

void VDCaptureStatsFilter::CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock) {
	vdsynchronized(mcsLock) {
		if (stream == 0) {
			uint32 lTimeStamp = (uint32)(timestamp / 1000);

			if (!mVideoFramesCaptured)
				mVideoFirstCapTime = lTimeStamp;
			mVideoLastCapTime = lTimeStamp;

			++mVideoFramesCaptured;
		} else if (stream == 1) {
			uint32 dwTime = (uint32)(global_clock / 1000);

			if (!mAudioFirstSize) {
				mAudioFirstSize = size;
				mAudioFirstCapTime = dwTime;
			}
			mAudioLastCapTime = dwTime;

			mTotalAudioDataSize += size;
			mAudioRateEstimator.AddSample(dwTime, mTotalAudioDataSize);

			if (mVideoFramesCaptured)
				mAudioRelativeRateEstimator.AddSample(mVideoLastCapTime, mTotalAudioDataSize);
		}
	}

	if (mpCB)
		mpCB->CapProcessData(stream, data, size, timestamp, key, global_clock);
}

class VDCaptureData : public VDThread {
public:
	VDCaptureProject	*mpProject;
	WAVEFORMATEX	mwfex;
	sint32		mTotalJitter;
	sint32		mTotalDisp;
	sint64		mTotalVideoSize;
	sint64		mTotalAudioSize;
	sint64		mLastVideoSize;
	sint64		mDiskFreeBytes;
	long		mTotalFramesCaptured;
	uint32		mFramesDropped;
	uint32		mFramesInserted;
	uint32		mFramePeriod;
	uint64		mLastUpdateTime;
	long		mLastTime;
	int			mSegmentIndex;
	int			mVideoSegmentIndex;
	int			mAudioSegmentIndex;

	vdautoptr<VideoSequenceCompressor> mpVideoCompressor;
	IVDMediaOutput			*volatile mpOutput;
	IVDMediaOutputAVIFile	*volatile mpOutputFile;
	IVDMediaOutputAVIFile	*volatile mpOutputFilePending;
	IVDMediaOutputStream	*volatile mpVideoOut;
	IVDMediaOutputStream	*volatile mpAudioOut;
	int				mAudioSampleSize;
	vdautoptr<MyError>		mpError;
	VDAtomicPtr<MyError>	mpSpillError;
	const wchar_t	*mpszFilename;
	const wchar_t	*mpszPath;
	const wchar_t	*mpszNewPath;
	sint64			mSegmentAudioSize;
	sint64			mSegmentVideoSize;
	sint64			mAudioBlocks;
	sint64			mAudioSwitchPt;
	sint64			mVideoBlocks;
	sint64			mVideoSwitchPt;
	long			mSizeThreshold;
	long			mSizeThresholdPending;

	VDCriticalSection	mCallbackLock;

	VDCaptureStatsFilter	*mpStatsFilter;
	IVDCaptureResyncFilter	*mpResyncFilter;
	IVDCaptureAudioCompFilter *mpAudioCompFilter;

	int				mStatsChannelVTJitter;

	VDCaptureTimingSetup	mTimingSetup;

	bool			mbDoSwitch;
	bool			mbAllFull;
	bool			mbNTSC;

	IVDCaptureFilterSystem *mpFilterSys;
	VDPixmapLayout			mInputLayout;
	VDPixmapLayout			mOutputLayout;
	vdautoptr<IVDPixmapBlitter>	mpOutputBlitter;
	VDPixmapBuffer repack_buffer;
	VDPixmapLayout driverLayout;
	VDPixmapLayout vfwLayout;
	void *pOutputBuffer;

	VDStringW		mCaptureRoot;

	VDCaptureData();
	~VDCaptureData();

	void PostFinalizeRequest() {
		PostThreadMessage(getThreadID(), VDCM_SWITCH_FIN, 0, 0);
	}
	void PostExitRequest() {
		PostThreadMessage(getThreadID(), VDCM_EXIT, 0, 0);
	}

	void createOutputBlitter();
	bool VideoCallback(const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock);
	void VideoCallbackWriteFrame(void *data, uint32 size, bool key);
	bool WaveCallback(const void *data, uint32 size, sint64 global_clock);
	void OnFramesDropped(int framesDropped);
	void OnFramesInserted(int framesInserted);
	void OnException() {
		mpError = new_nothrow MyUserAbortError();
	}
protected:
	void ThreadRun();
	void CreateNewFile();
	void FinalizeOldFile();
	void DoSpill();
	void CheckVideoAfter();
};

VDCaptureData::VDCaptureData()
	: VDThread("Capture spill")
	, mTotalJitter(0)
	, mTotalDisp(0)
	, mTotalVideoSize(0)
	, mTotalAudioSize(0)
	, mLastVideoSize(0)
	, mDiskFreeBytes(0)
	, mTotalFramesCaptured(0)
	, mFramesDropped(0)
	, mFramesInserted(0)
	, mFramePeriod(0)
	, mLastUpdateTime(0)
	, mLastTime(0)
	, mSegmentIndex(0)
	, mVideoSegmentIndex(0)
	, mAudioSegmentIndex(0)
	, mpVideoCompressor(NULL)
	, pOutputBuffer(0)
	, mpOutput(NULL)
	, mpOutputFile(NULL)
	, mpOutputFilePending(NULL)
	, mpVideoOut(NULL)
	, mpAudioOut(NULL)
	, mAudioSampleSize(0)
	, mpError(NULL)
	, mpSpillError(NULL)
	, mpszFilename(NULL)
	, mpszPath(NULL)
	, mpszNewPath(NULL)
	, mSegmentAudioSize(0)
	, mSegmentVideoSize(0)
	, mAudioBlocks(0)
	, mAudioSwitchPt(0)
	, mVideoBlocks(0)
	, mVideoSwitchPt(0)
	, mSizeThreshold(0)
	, mSizeThresholdPending(0)
	, mbDoSwitch(0)
	, mbAllFull(0)
	, mbNTSC(0)
	, mpFilterSys(NULL)
{
}

VDCaptureData::~VDCaptureData() {
	delete mpSpillError;
}

extern LONG __stdcall CrashHandler(struct _EXCEPTION_POINTERS *ExceptionInfo, bool allowForcedExit);

namespace
{
	class VDCaptureTimingAccuracyBooster {
	public:
		VDCaptureTimingAccuracyBooster()
			: mPeriod(0)
		{
			TIMECAPS tc;
			if (TIMERR_NOERROR == timeGetDevCaps(&tc, sizeof tc) && TIMERR_NOERROR == timeBeginPeriod(tc.wPeriodMin))
				mPeriod = tc.wPeriodMin;
		}

		~VDCaptureTimingAccuracyBooster() {
			if (mPeriod)
				timeEndPeriod(mPeriod);
		}

	protected:
		int mPeriod;
	};
}

//////////////////////////////////////////////////////////////////////

extern const char g_szCapture				[]="Capture";

static const char g_szWarnTiming1			[]="Warn Timing1";

///////////////////////////////////////////////////////////////////////////
//
//	dynamics
//
///////////////////////////////////////////////////////////////////////////

COMPVARS2 g_compression;
VDPixmapFormatEx g_compformat;

///////////////////////////////////////////////////////////////////////////
//
//	prototypes
//
///////////////////////////////////////////////////////////////////////////

extern void CaptureWarnCheckDriver(HWND hwnd, const char *s);

static void CaptureInternal(HWND, HWND hWndCapture, bool fTest);

///////////////////////////////////////////////////////////////////////////
//
//	VDCaptureProjectBaseCallback
//
///////////////////////////////////////////////////////////////////////////

void VDCaptureProjectBaseCallback::UICaptureDriversUpdated() {}
void VDCaptureProjectBaseCallback::UICaptureDriverDisconnecting(int driver) {}
void VDCaptureProjectBaseCallback::UICaptureDriverChanging(int driver) {}
void VDCaptureProjectBaseCallback::UICaptureDriverChanged(int driver) {}
void VDCaptureProjectBaseCallback::UICaptureAudioDriversUpdated() {}
void VDCaptureProjectBaseCallback::UICaptureAudioDriverChanged(int driver) {}
void VDCaptureProjectBaseCallback::UICaptureAudioSourceChanged(int input) {}
void VDCaptureProjectBaseCallback::UICaptureAudioInputChanged(int input) {}
void VDCaptureProjectBaseCallback::UICaptureFileUpdated() {}
void VDCaptureProjectBaseCallback::UICaptureAudioFormatUpdated() {}
void VDCaptureProjectBaseCallback::UICaptureVideoFormatUpdated() {}
void VDCaptureProjectBaseCallback::UICaptureVideoPreviewFormatUpdated() {}
void VDCaptureProjectBaseCallback::UICaptureVideoSourceChanged(int input) {}
void VDCaptureProjectBaseCallback::UICaptureTunerChannelChanged(int ch, bool init) {}
void VDCaptureProjectBaseCallback::UICaptureParmsUpdated() {}
bool VDCaptureProjectBaseCallback::UICaptureAnalyzeBegin(const VDPixmap& format) { return false; }
void VDCaptureProjectBaseCallback::UICaptureAnalyzeFrame(const VDPixmap& format) {}
void VDCaptureProjectBaseCallback::UICaptureAnalyzeEnd() {}
void VDCaptureProjectBaseCallback::UICaptureVideoHistoBegin() {}
void VDCaptureProjectBaseCallback::UICaptureVideoHisto(const float data[256]) {}
void VDCaptureProjectBaseCallback::UICaptureVideoHistoEnd() {}
void VDCaptureProjectBaseCallback::UICaptureAudioPeaksUpdated(int count, float* peak) {}
void VDCaptureProjectBaseCallback::UICaptureStart(bool fTest) {}
bool VDCaptureProjectBaseCallback::UICapturePreroll() { return true; }
void VDCaptureProjectBaseCallback::UICaptureStatusUpdated(VDCaptureStatus&) {}
void VDCaptureProjectBaseCallback::UICaptureEnd(bool success) {}

///////////////////////////////////////////////////////////////////////////
//
//	VDCaptureProject
//
///////////////////////////////////////////////////////////////////////////

class VDCaptureProject : public IVDCaptureProject, public IVDCaptureDriverCallback, public IVDUIFrameEngine {
	friend class VDCaptureData;
public:
	VDCaptureProject();
	~VDCaptureProject();

	int		AddRef();
	int		Release();

	bool	Attach(VDGUIHandle hwnd);
	void	Detach();

	IVDCaptureProjectCallback *GetCallback() { return mpCB; }
	void	SetCallback(IVDCaptureProjectCallback *pCB);

	void	LockUpdates();
	void	UnlockUpdates();

	bool	IsHardwareDisplayAvailable();
	void	SetDisplayMode(DisplayMode mode);
	DisplayMode	GetDisplayMode();
	void	SetDisplayChromaKey(int key) { mDisplayChromaKey = key; }
	void	SetDisplayRect(const vdrect32& r);
	vdrect32 GetDisplayRectAbsolute();
	void	SetDisplayVisibility(bool vis);

	void	SetVideoFrameTransferEnabled(bool ena);
	bool	IsVideoFrameTransferEnabled();

	void	SetVideoHistogramEnabled(bool ena);
	bool	IsVideoHistogramEnabled();

	void	SetFrameTime(sint32 lFrameTime);
	sint32	GetFrameTime();
	VDFraction	GetFrameRate();

	void	SetTimingSetup(const VDCaptureTimingSetup& timing) { mTimingSetup = timing; UpdateTimingOptions(); }
	const VDCaptureTimingSetup&	GetTimingSetup() { return mTimingSetup; }

	void	SetLogEnabled(bool ena);
	bool	IsLogEnabled();
	bool	IsLogAvailable();
	void	SaveLog(const wchar_t *path);

	bool	SetTunerChannel(int channel);
	int		GetTunerChannel();
	bool	GetTunerChannelRange(int& minChannel, int& maxChannel);
	uint32	GetTunerFrequencyPrecision();
	uint32	GetTunerExactFrequency();
	bool	SetTunerExactFrequency(uint32 freq);
	nsVDCapture::TunerInputMode	GetTunerInputMode();
	void	SetTunerInputMode(nsVDCapture::TunerInputMode tunerMode);

	int		GetAudioDeviceCount();
	const wchar_t *GetAudioDeviceName(int idx);
	void	SetAudioDevice(int idx);
	int		GetAudioDeviceIndex();
	int		GetAudioDeviceByName(const wchar_t *name);

	int		GetVideoSourceCount();
	const wchar_t *GetVideoSourceName(int idx);
	bool	SetVideoSource(int idx);
	int		GetVideoSourceIndex();
	int		GetVideoSourceByName(const wchar_t *name);

	int		GetAudioSourceCount();
	const wchar_t *GetAudioSourceName(int idx);
	bool	SetAudioSource(int idx);
	int		GetAudioSourceIndex();
	int		GetAudioSourceByName(const wchar_t *name);

	int		GetAudioSourceForVideoSource(int idx);

	int		GetAudioInputCount();
	const wchar_t *GetAudioInputName(int idx);
	bool	SetAudioInput(int idx);
	int		GetAudioInputIndex();
	int		GetAudioInputByName(const wchar_t *name);

	void	SetAudioCaptureEnabled(bool ena);
	bool	IsAudioCaptureEnabled();
	bool	IsAudioCaptureAvailable();

	void	SetAudioPlaybackEnabled(bool ena);
	bool	IsAudioPlaybackEnabled();
	bool	IsAudioPlaybackAvailable();

	void	SetAudioVumeterEnabled(bool b);
	bool	IsAudioVumeterEnabled();

	void	SetHardwareBuffering(int videoBuffers, int audioBuffers, int audioBufferSize);
	bool	GetHardwareBuffering(int& videoBuffers, int& audioBuffers, int& audioBufferSize);

	bool	IsDriverDialogSupported(DriverDialog dlg);
	void	DisplayDriverDialog(DriverDialog dlg);

	bool	IsPropertySupported(uint32 id);
	sint32	GetPropertyInt(uint32 id, bool *pAutomatic);
	void	SetPropertyInt(uint32 id, sint32 value, bool automatic);
	void	GetPropertyInfoInt(uint32 id, sint32& minVal, sint32& maxVal, sint32& step, sint32& defaultVal, bool& automatic, bool& manual);

	void	GetPreviewImageSize(sint32& w, sint32& h);

	void	SetFilterSetup(const VDCaptureFilterSetup& setup);
	const VDCaptureFilterSetup& GetFilterSetup();

	void	SetStopPrefs(const VDCaptureStopPrefs& prefs);
	const VDCaptureStopPrefs& GetStopPrefs();

	void	SetDiskSettings(const VDCaptureDiskSettings& sets);
	const VDCaptureDiskSettings& GetDiskSettings();

	uint32	GetPreviewFrameCount();

	void  LoadVideoConfig(VDRegistryAppKey& key) {
		if (mpDriver) mpDriver->LoadVideoConfig(key);
	}
	void  SaveVideoConfig(VDRegistryAppKey& key) {
		if (mpDriver) mpDriver->SaveVideoConfig(key);
	}
	void  LoadAudioConfig(VDRegistryAppKey& key) {
		if (mpDriver) mpDriver->LoadAudioConfig(key);
	}
	void  SaveAudioConfig(VDRegistryAppKey& key) {
		if (mpDriver) mpDriver->SaveAudioConfig(key);
	}

	bool	SetVideoFormat(const VDAVIBitmapInfoHeader& bih, LONG cbih);
	bool	GetVideoFormat(vdstructex<VDAVIBitmapInfoHeader>& bih);

	void	GetAvailableAudioFormats(std::list<vdstructex<VDWaveFormat> >& aformats);

	bool	SetAudioFormat(const VDWaveFormat& wfex, LONG cbwfex);
	bool	GetAudioFormat(vdstructex<VDWaveFormat>& wfex);
	void	ValidateAudioFormat();

	void	SetAudioCompFormat();
	void	SetAudioCompFormat(const VDWaveFormat& wfex, uint32 cbwfex, const char *pShortNameHint);
	bool	GetAudioCompFormat(vdstructex<VDWaveFormat>& wfex, VDStringA& hint);

	void		SetCaptureFile(const wchar_t *filename, bool bIsStripeSystem);
	VDStringW	GetCaptureFile();
	void		PreallocateCaptureFile(sint64 size);
	bool		IsStripingEnabled();

	void	SetSpillSystem(bool enable);
	bool	IsSpillEnabled();

	void	IncrementFileID();
	void	DecrementFileID();
	void	IncrementFileIDUntilClear();

	void	ScanForDrivers();
	int		GetDriverCount();
	const wchar_t *GetDriverName(int i);
	int		GetDriverByName(const wchar_t *name);
	bool	SelectDriver(int nDriver);
	bool	IsDriverConnected();
	int		GetConnectedDriverIndex();
	const wchar_t *GetConnectedDriverName() { return GetDriverName(GetConnectedDriverIndex()); }

	void	Capture(bool bTest);
	void	CaptureStop();

protected:
	int		GetByName(int count, const wchar_t *(VDCaptureProject::*pGetNameRout)(int), const wchar_t *name);
	void	UpdateTimingOptions();
	void	InitVideoAnalysis();
	void	ShutdownVideoAnalysis();
	static bool	AreFiltersEnabled(const VDCaptureFilterSetup&);
	bool	InitFilter();
	void	ShutdownFilter();
	bool	InitVideoHistogram();
	void	ShutdownVideoHistogram();
	bool	InitVideoFrameTransfer();
	void	ShutdownVideoFrameTransfer();
	void	DispatchAnalysis(const VDPixmap&);

	LRESULT	OnEngineEvent(WPARAM wParam, LPARAM lParam);
	void	ProcessPendingEvents();

	void	CapBegin(sint64 global_clock);
	void	CapEnd(const MyError *pError);
	bool	CapEvent(DriverEvent event, int data);
	void	CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock);
protected:
	void	CapProcessData2(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock);

	vdautoptr<IVDCaptureDriver>	mpDriver;
	int			mDriverIndex;
	VDGUIHandle	mhwnd;

	IVDCaptureProjectCallback	*mpCB;

	DisplayMode	mDisplayMode;

	VDStringW	mFilename;
	bool		mbStripingEnabled;

	int			mDisplayChromaKey;

	bool		mbEnableSpill;
	bool		mbEnableAudioVumeter;
	bool		mbEnableVideoHistogram;
	bool		mbEnableVideoFrameTransfer;
	VDAtomicInt	mSuspendVideoFrameTransferCount;
	bool		mbVideoFrameTransferActive;

	// driver state shadowing
	bool		mbDisplayVisible;
	bool		mbAudioCaptureEnabled;
	bool		mbAudioPlaybackEnabled;

	VDCriticalSection		mVideoFilterLock;
	vdautoptr<IVDCaptureFilterSystem>	mpFilterSys;
	VDPixmapLayout			mFilterInputLayout;
	VDPixmapLayout			mFilterOutputLayout;

	VDCaptureData	*mpCaptureData;
	DWORD		mMainThreadId;

	bool		mbLoggingEnabled;
	vdautoptr<IVDCaptureLogFilter>	mpLogFilter;

	struct DriverEntry {
		VDStringW	mName;
		int			mSystemId;
		int			mId;

		DriverEntry(const wchar_t *name, int system, int id) : mName(name), mSystemId(system), mId(id) {}
	};

	typedef std::list<IVDCaptureSystem *>	tSystems;
	tSystems	mSystems;

	typedef std::list<DriverEntry>	tDrivers;
	tDrivers	mDrivers;

	vdstructex<VDWaveFormat>	mAudioCompFormat;
	VDStringA					mAudioCompFormatHint;
	vdstructex<WAVEFORMATEX>	mAudioAnalysisFormat;

	vdautoptr<IVDCaptureVideoHistogram>	mpVideoHistogram;
	VDCriticalSection		mVideoAnalysisLock;

	VDCaptureTimingSetup	mTimingSetup;
	VDCaptureFilterSetup	mFilterSetup;
	VDCaptureStopPrefs		mStopPrefs;
	VDCriticalSection		mStopPrefsLock;
	VDCaptureDiskSettings	mDiskSettings;

	uint32		mFilterPalette[256];

	VDAtomicInt	mRefCount;
	VDCaptureTimingAccuracyBooster mTimingAccuracyBooster;

	VDCriticalSection			mEventLock;
	vdfastvector<DriverEvent>	mPendingEvents;
};

IVDCaptureProject *VDCreateCaptureProject() { return new VDCaptureProject; }

VDCaptureProject::VDCaptureProject()
	: mDriverIndex(-1)
	, mhwnd(NULL)
	, mpCB(NULL)
	, mDisplayMode(kDisplayNone)
	, mbStripingEnabled(false)
	, mbEnableSpill(false)
	, mbEnableAudioVumeter(false)
	, mbEnableVideoHistogram(false)
	, mbEnableVideoFrameTransfer(false)
	, mSuspendVideoFrameTransferCount(0)
	, mbVideoFrameTransferActive(false)
	, mbDisplayVisible(true)
	, mbAudioCaptureEnabled(true)
	, mbAudioPlaybackEnabled(false)
	, mpCaptureData(NULL)
	, mbLoggingEnabled(false)
	, mRefCount(0)
{
	mTimingSetup.mSyncMode				= VDCaptureTimingSetup::kSyncAudioToVideo;
	mTimingSetup.mbAllowEarlyDrops		= true;
	mTimingSetup.mbAllowLateInserts		= true;
	mTimingSetup.mbCorrectVideoTiming	= true;
	mTimingSetup.mbResyncWithIntegratedAudio	= true;
	mTimingSetup.mInsertLimit			= 10;

	mTimingSetup.mbUseFixedAudioLatency	= false;
	mTimingSetup.mAudioLatency			= 0;

	mTimingSetup.mbUseLimitedAutoAudioLatency	= true;
	mTimingSetup.mAutoAudioLatencyLimit	= 30;

	mTimingSetup.mbUseAudioTimestamps	= false;
	mTimingSetup.mbDisableClockForPreview	= false;
	mTimingSetup.mbForceAudioRendererClock	= true;
	mTimingSetup.mbIgnoreVideoTimestamps	= false;

	mFilterSetup.mCropRect.clear();
	mFilterSetup.mVertSquashMode		= IVDCaptureFilterSystem::kFilterDisable;
	mFilterSetup.mNRThreshold			= 16;

	mFilterSetup.mbEnableFilterChain	= false;
	mFilterSetup.mbSkipFilterChainConversion	= false;
	mFilterSetup.mbEnableNoiseReduction	= false;
	mFilterSetup.mbEnableLumaSquishBlack	= false;
	mFilterSetup.mbEnableLumaSquishWhite	= false;
	mFilterSetup.mbEnableFieldSwap		= false;

	mStopPrefs.fEnableFlags = 0;
	mStopPrefs.lDiskSpaceThreshold	=	50;
	mStopPrefs.lMaxDropRate			=	5;
	mStopPrefs.lSizeLimit			=	1900;
	mStopPrefs.lTimeLimit			=	30;

	mDiskSettings.mDiskChunkSize		= 512;
	mDiskSettings.mDiskChunkCount		= 2;
	mDiskSettings.mbDisableWriteCache	= 1;

	mSystems.push_back(VDCreateCaptureSystemVFW());
	mSystems.push_back(VDCreateCaptureSystemDS());
	mSystems.push_back(VDCreateCaptureSystemScreen());
	mSystems.push_back(VDCreateCaptureSystemEmulation());
}

VDCaptureProject::~VDCaptureProject() {
	while(!mSystems.empty()) {
		delete mSystems.back();
		mSystems.pop_back();
	}
}

int VDCaptureProject::AddRef() {
	return ++mRefCount;
}

int VDCaptureProject::Release() {
	if (mRefCount == 1) {
		delete this;
		return 0;
	}

	return --mRefCount;
}

bool VDCaptureProject::Attach(VDGUIHandle hwnd) {
	if (mhwnd == hwnd)
		return true;

	if (mhwnd)
		Detach();

	extern void AnnounceCaptureExperimental(VDGUIHandle h);
	AnnounceCaptureExperimental(hwnd);

	mhwnd = hwnd;

	VDUIFrame *pFrame = VDUIFrame::GetFrame((HWND)hwnd);
	pFrame->AttachEngine(this);

	return true;
}

void VDCaptureProject::Detach() {
	if (!mhwnd)
		return;

	SelectDriver(-1);

	FreeCompressor(&g_compression);

	VDUIFrame *pFrame = VDUIFrame::GetFrame((HWND)mhwnd);
	pFrame->DetachEngine();

	mhwnd = NULL;
}

void VDCaptureProject::SetCallback(IVDCaptureProjectCallback *pCB) {
	mpCB = pCB;
}

void VDCaptureProject::LockUpdates() {
	if (mpDriver)
		mpDriver->LockUpdates();
}

void VDCaptureProject::UnlockUpdates() {
	if (mpDriver)
		mpDriver->UnlockUpdates();
}

bool VDCaptureProject::IsHardwareDisplayAvailable() {
	return mpDriver && mpDriver->IsHardwareDisplayAvailable();
}

void VDCaptureProject::SetDisplayMode(DisplayMode mode) {
	if (mDisplayMode == mode)
		return;

	// Analyze mode unfortunately complicates this quite a bit:
	//
	// 1) We have to make sure analysis is setup before turning it on.
	// 2) We have to make sure analysis is turned off before shutting
	//    it down.
	// 3) We have to make sure the existing video display is killed
	//    before initializing analysis, or else a video overlay conflict
	//    may occur.

	// If we are turning ON analysis....
	if (mode == kDisplayAnalyze) {
		// Kill the existing display to free the video overlay.
		if (mpDriver)
			mpDriver->SetDisplayMode(kDisplayNone);

		// Initialize analysis.
		if (mpDriver)
			InitVideoAnalysis();
	}

	// If analysis was enabled, turn it off. ShutdownVideoAnalysis() is
	// threadsafe so this is OK.
	if (mDisplayMode == kDisplayAnalyze)
		ShutdownVideoAnalysis();

	// Flip the display mode.
	mDisplayMode = mode;

	// Finally, switch the display mode.
	if (mpDriver)
		mpDriver->SetDisplayMode(mode);
}

DisplayMode VDCaptureProject::GetDisplayMode() {
	return mDisplayMode;
}

void VDCaptureProject::SetDisplayRect(const vdrect32& r) {
	if (mpDriver) {
		mpDriver->SetDisplayRect(r);
	}
}

vdrect32 VDCaptureProject::GetDisplayRectAbsolute() {
	return mpDriver ? mpDriver->GetDisplayRectAbsolute() : vdrect32(0,0,0,0);
}

void VDCaptureProject::SetDisplayVisibility(bool vis) {
	if (mbDisplayVisible == vis)
		return;

	mbDisplayVisible = vis;

	if (mpDriver)
		mpDriver->SetDisplayVisibility(vis);
}
void VDCaptureProject::SetVideoFrameTransferEnabled(bool ena) {
	if (mbEnableVideoFrameTransfer == ena)
		return;

	mbEnableVideoFrameTransfer = ena;

	if (mDisplayMode == kDisplayAnalyze) {
		if (ena)
			InitVideoFrameTransfer();
		else
			ShutdownVideoFrameTransfer();
	}
}

bool VDCaptureProject::IsVideoFrameTransferEnabled() {
	return mbEnableVideoFrameTransfer;
}

void VDCaptureProject::SetVideoHistogramEnabled(bool ena) {
	if (mbEnableVideoHistogram == ena)
		return;

	mbEnableVideoHistogram = ena;

	if (mDisplayMode == kDisplayAnalyze) {
		if (ena)
			InitVideoHistogram();
		else
			ShutdownVideoHistogram();
	}
}

bool VDCaptureProject::IsVideoHistogramEnabled() {
	return mbEnableVideoHistogram;
}

void VDCaptureProject::SetFrameTime(sint32 lFrameTime) {
	if (mpDriver)
		mpDriver->SetFramePeriod(lFrameTime);
}

sint32 VDCaptureProject::GetFrameTime() {
	if (!VDINLINEASSERT(mpDriver))
		return 10000000/15;

	return mpDriver->GetFramePeriod();
}

VDFraction VDCaptureProject::GetFrameRate() {
	sint32 period = GetFrameTime();

	if ((period|1) == 333667)
		return VDFraction(30000, 1001);

	return VDFraction(10000000, period);
}

void VDCaptureProject::SetLogEnabled(bool ena) {
	mbLoggingEnabled = ena;
	if (!mbLoggingEnabled)
		mpLogFilter = NULL;
}

bool VDCaptureProject::IsLogEnabled() {
	return mbLoggingEnabled;
}

bool VDCaptureProject::IsLogAvailable() {
	return mpLogFilter != NULL;
}

void VDCaptureProject::SaveLog(const wchar_t *path) {
	mpLogFilter->WriteLog(path);
}

bool VDCaptureProject::SetTunerChannel(int channel) {
	if (mpDriver && mpDriver->SetTunerChannel(channel)) {
		if (mpCB)
			mpCB->UICaptureTunerChannelChanged(channel, false);
		return true;
	}
	return false;
}

int VDCaptureProject::GetTunerChannel() {
	return mpDriver ? mpDriver->GetTunerChannel() : -1;
}

bool VDCaptureProject::GetTunerChannelRange(int& minChannel, int& maxChannel) {
	return mpDriver && mpDriver->GetTunerChannelRange(minChannel, maxChannel);
}

uint32 VDCaptureProject::GetTunerFrequencyPrecision() {
	return mpDriver ? mpDriver->GetTunerFrequencyPrecision() : 0;
}

uint32 VDCaptureProject::GetTunerExactFrequency() {
	return mpDriver ? mpDriver->GetTunerExactFrequency() : 0;
}

bool VDCaptureProject::SetTunerExactFrequency(uint32 freq) {
	return mpDriver && mpDriver->SetTunerExactFrequency(freq);
}

nsVDCapture::TunerInputMode	VDCaptureProject::GetTunerInputMode() {
	return mpDriver ? mpDriver->GetTunerInputMode() : nsVDCapture::kTunerInputUnknown;
}

void VDCaptureProject::SetTunerInputMode(nsVDCapture::TunerInputMode tunerMode) {
	if (mpDriver)
		mpDriver->SetTunerInputMode(tunerMode);
}

int VDCaptureProject::GetAudioDeviceCount() {
	return mpDriver ? mpDriver->GetAudioDeviceCount() : 0;
}

const wchar_t *VDCaptureProject::GetAudioDeviceName(int idx) {
	return mpDriver ? mpDriver->GetAudioDeviceName(idx) : NULL;
}

void VDCaptureProject::SetAudioDevice(int idx) {
	if (mpDriver) {
		bool bVumeter = mbEnableAudioVumeter;
		SetAudioVumeterEnabled(false);

		mpDriver->SetAudioDevice(idx);

		if (mpCB) {
			mpCB->UICaptureAudioDriverChanged(mpDriver->GetAudioDeviceIndex());	// must requery in case of failure
			mpCB->UICaptureAudioInputChanged(mpDriver->GetAudioInputIndex());
			mpCB->UICaptureAudioSourceChanged(mpDriver->GetAudioSourceIndex());
			mpCB->UICaptureAudioFormatUpdated();
		}

		SetAudioVumeterEnabled(bVumeter);
	}
}

int VDCaptureProject::GetAudioDeviceIndex() {
	return mpDriver ? mpDriver->GetAudioDeviceIndex() : -1;
}

int VDCaptureProject::GetAudioDeviceByName(const wchar_t *name) {
	return GetByName(GetAudioDeviceCount(), &VDCaptureProject::GetAudioDeviceName, name);
}

int VDCaptureProject::GetVideoSourceCount() {
	return mpDriver ? mpDriver->GetVideoSourceCount() : 0;
}

const wchar_t *VDCaptureProject::GetVideoSourceName(int idx) {
	return mpDriver ? mpDriver->GetVideoSourceName(idx) : NULL;
}

bool VDCaptureProject::SetVideoSource(int idx) {
	if (mpDriver && mpDriver->SetVideoSource(idx)) {
		if (mpCB)
			mpCB->UICaptureVideoSourceChanged(mpDriver->GetVideoSourceIndex());

		return true;
	}

	return false;
}

int VDCaptureProject::GetVideoSourceIndex() {
	return mpDriver ? mpDriver->GetVideoSourceIndex() : -1;
}

int VDCaptureProject::GetVideoSourceByName(const wchar_t *name) {
	return GetByName(GetVideoSourceCount(), &VDCaptureProject::GetVideoSourceName, name);
}

int VDCaptureProject::GetAudioSourceCount() {
	return mpDriver ? mpDriver->GetAudioSourceCount() : 0;
}

const wchar_t *VDCaptureProject::GetAudioSourceName(int idx) {
	return mpDriver ? mpDriver->GetAudioSourceName(idx) : NULL;
}

bool VDCaptureProject::SetAudioSource(int idx) {
	if (mpDriver && mpDriver->SetAudioSource(idx)) {
		if (mpCB)
			mpCB->UICaptureAudioSourceChanged(mpDriver->GetAudioSourceIndex());

		return true;
	}

	return false;
}

int VDCaptureProject::GetAudioSourceIndex() {
	return mpDriver ? mpDriver->GetAudioSourceIndex() : -1;
}

int VDCaptureProject::GetAudioSourceByName(const wchar_t *name) {
	return GetByName(GetAudioSourceCount(), &VDCaptureProject::GetAudioSourceName, name);
}

int VDCaptureProject::GetAudioSourceForVideoSource(int idx) {
	return mpDriver ? mpDriver->GetAudioSourceForVideoSource(idx) : -2;
}

int VDCaptureProject::GetAudioInputCount() {
	return mpDriver ? mpDriver->GetAudioInputCount() : 0;
}

const wchar_t *VDCaptureProject::GetAudioInputName(int idx) {
	return mpDriver ? mpDriver->GetAudioInputName(idx) : NULL;
}

bool VDCaptureProject::SetAudioInput(int idx) {
	if (mpDriver && mpDriver->SetAudioInput(idx)) {
		if (mpCB)
			mpCB->UICaptureAudioInputChanged(mpDriver->GetAudioInputIndex());

		return true;
	}

	return false;
}

int VDCaptureProject::GetAudioInputIndex() {
	return mpDriver ? mpDriver->GetAudioInputIndex() : -1;
}

int VDCaptureProject::GetAudioInputByName(const wchar_t *name) {
	return GetByName(GetAudioInputCount(), &VDCaptureProject::GetAudioInputName, name);
}

void VDCaptureProject::SetAudioCaptureEnabled(bool b) {
	if (mbAudioCaptureEnabled == b)
		return;

	mbAudioCaptureEnabled = b;
	if (mpDriver)
		mpDriver->SetAudioCaptureEnabled(b);
}

bool VDCaptureProject::IsAudioCaptureEnabled() {
	return mbAudioCaptureEnabled;
}

bool VDCaptureProject::IsAudioCaptureAvailable() {
	return mpDriver && mpDriver->IsAudioCapturePossible();
}

void VDCaptureProject::SetAudioPlaybackEnabled(bool b) {
	if (mbAudioPlaybackEnabled == b)
		return;

	mbAudioPlaybackEnabled = b;

	if (mpDriver)
		mpDriver->SetAudioPlaybackEnabled(b);
}

bool VDCaptureProject::IsAudioPlaybackEnabled() {
	return mbAudioPlaybackEnabled;
}

bool VDCaptureProject::IsAudioPlaybackAvailable() {
	return mpDriver && mpDriver->IsAudioPlaybackPossible();
}

void VDCaptureProject::SetAudioVumeterEnabled(bool b) {
	// NOTE: Called from SetAudioFormat().
	if (mbEnableAudioVumeter == b)
		return;

	mbEnableAudioVumeter = b;

	if (mpDriver) {
		mpDriver->SetAudioAnalysisEnabled(false);

		if (b) {
			vdstructex<WAVEFORMATEX> wfex;
			if (mpDriver->GetAudioFormat(wfex)) {
				if (is_audio_pcm((VDWaveFormat*)wfex.data())) {
					mAudioAnalysisFormat = wfex;
					mpDriver->SetAudioAnalysisEnabled(true);

					vdstructex<WAVEFORMATEX> wfex2;
					if (mpDriver->GetAudioFormat(wfex2)) {
						if (mpCB)
							mpCB->UICaptureAudioFormatUpdated();
						if(wfex2!=wfex){
							mAudioAnalysisFormat = wfex2;
							mbEnableAudioVumeter = false;
							SetAudioVumeterEnabled(true);
						}
					}

				}
			}
		}
	}

	if (!b)
		mAudioAnalysisFormat.clear();
}

bool VDCaptureProject::IsAudioVumeterEnabled() {
	return mbEnableAudioVumeter;
}

void VDCaptureProject::SetHardwareBuffering(int videoBuffers, int audioBuffers, int audioBufferSize) {
#if 0
	CAPTUREPARMS cp;

	if (capCaptureGetSetup(mhwndCapture, &cp, sizeof(CAPTUREPARMS))) {
		cp.wNumVideoRequested = videoBuffers;
		cp.wNumAudioRequested = audioBuffers;
		cp.dwAudioBufferSize = audioBufferSize;

		capCaptureSetSetup(mhwndCapture, &cp, sizeof(CAPTUREPARMS));
	}
#endif
}

bool VDCaptureProject::GetHardwareBuffering(int& videoBuffers, int& audioBuffers, int& audioBufferSize) {
#if 0
	CAPTUREPARMS cp;

	if (VDINLINEASSERT(capCaptureGetSetup(mhwndCapture, &cp, sizeof(CAPTUREPARMS)))) {
		videoBuffers = cp.wNumVideoRequested;
		audioBuffers = cp.wNumAudioRequested;
		audioBufferSize = cp.dwAudioBufferSize;
		return true;
	}
#endif
	return false;
}

bool VDCaptureProject::IsDriverDialogSupported(DriverDialog dlg) {
	return mpDriver && mpDriver->IsDriverDialogSupported(dlg);
}

void VDCaptureProject::DisplayDriverDialog(DriverDialog dlg) {
	if (mpDriver)
		mpDriver->DisplayDriverDialog(dlg);
}

bool VDCaptureProject::IsPropertySupported(uint32 id) {
	return mpDriver && mpDriver->IsPropertySupported(id);
}

sint32 VDCaptureProject::GetPropertyInt(uint32 id, bool *pAutomatic) {
	if (mpDriver)
		return mpDriver->GetPropertyInt(id, pAutomatic);
	
	if (pAutomatic)
		*pAutomatic = true;
	return 0;
}

void VDCaptureProject::SetPropertyInt(uint32 id, sint32 value, bool automatic) {
	if (mpDriver)
		mpDriver->SetPropertyInt(id, value, automatic);
}

void VDCaptureProject::GetPropertyInfoInt(uint32 id, sint32& minVal, sint32& maxVal, sint32& step, sint32& defaultVal, bool& automatic, bool& manual) {
	minVal = maxVal = defaultVal = 0;
	step = 1;
	automatic = false;
	manual = false;

	if (mpDriver)
		mpDriver->GetPropertyInfoInt(id, minVal, maxVal, step, defaultVal, automatic, manual);
}

void VDCaptureProject::GetPreviewImageSize(sint32& w, sint32& h) {
	if (mpFilterSys) {
		w = mFilterOutputLayout.w;
		h = mFilterOutputLayout.h;
	} else {
		w = 320;
		h = 240;

		vdstructex<VDAVIBitmapInfoHeader> vformat;
		if (GetVideoFormat(vformat)) {
			w = vformat->biWidth;
			h = abs(vformat->biHeight);
		}
	}
}

void VDCaptureProject::SetFilterSetup(const VDCaptureFilterSetup& setup) {
	bool rebuildAnalyze = (mDisplayMode == kDisplayAnalyze);

	// check if the change actually requires a rebuild... if it's just
	// the NR threshold, it doesn't

	if (	mFilterSetup.mbEnableFieldSwap == setup.mbEnableFieldSwap
		&&	mFilterSetup.mbEnableFilterChain == setup.mbEnableFilterChain
		&&	mFilterSetup.mbSkipFilterChainConversion == setup.mbSkipFilterChainConversion
		&&	mFilterSetup.mCropRect == setup.mCropRect
		&&	mFilterSetup.mVertSquashMode == setup.mVertSquashMode)
	{
		rebuildAnalyze = false;
	}

	// if we're capturing, we *can't* rebuild the analyze chain
	if (mpCaptureData)
		rebuildAnalyze = false;
	else if (!rebuildAnalyze && mDisplayMode == kDisplayAnalyze) {
		// check if we need to start or stop the filter chain
		bool chainRequired = AreFiltersEnabled(setup);

		if (mpFilterSys) {
			if (!chainRequired)
				rebuildAnalyze = true;
		} else {
			if (chainRequired)
				rebuildAnalyze = true;
		}
	}

	if (rebuildAnalyze)
		ShutdownVideoAnalysis();
	mFilterSetup = setup;
	if (rebuildAnalyze)
		InitVideoAnalysis();

	// if we didn't rebuild, modify any running filter chain
	if (mpFilterSys) {
		// Oops... capture is active. We can only change a few select
		// settings in this mode.

		mFilterSetup.mNRThreshold		= setup.mNRThreshold;
		mFilterSetup.mbEnableNoiseReduction	= setup.mbEnableNoiseReduction;
		mFilterSetup.mbEnableFieldSwap	= setup.mbEnableFieldSwap;
		mFilterSetup.mbEnableLumaSquishBlack	= setup.mbEnableLumaSquishBlack;
		mFilterSetup.mbEnableLumaSquishWhite	= setup.mbEnableLumaSquishWhite;

		if (mpFilterSys) {
			vdsynchronized(mVideoFilterLock) {
				mpFilterSys->SetNoiseReduction(setup.mbEnableNoiseReduction ? setup.mNRThreshold : 0);
				mpFilterSys->SetFieldSwap(setup.mbEnableFieldSwap);
				mpFilterSys->SetLumaSquish(setup.mbEnableLumaSquishBlack, setup.mbEnableLumaSquishWhite);
			}
		}

		return;
	}
}

const VDCaptureFilterSetup& VDCaptureProject::GetFilterSetup() {
	return mFilterSetup;
}

void VDCaptureProject::SetStopPrefs(const VDCaptureStopPrefs& prefs) {
	vdsynchronized(mStopPrefsLock) {
		mStopPrefs = prefs;
	}
}

const VDCaptureStopPrefs& VDCaptureProject::GetStopPrefs() {
	return mStopPrefs;
}

void VDCaptureProject::SetDiskSettings(const VDCaptureDiskSettings& sets) {
	mDiskSettings = sets;
}

const VDCaptureDiskSettings& VDCaptureProject::GetDiskSettings() {
	return mDiskSettings;
}

uint32 VDCaptureProject::GetPreviewFrameCount() {
	return mpDriver ? mpDriver->GetPreviewFrameCount() : 0;
}

bool VDCaptureProject::SetVideoFormat(const VDAVIBitmapInfoHeader& bih, LONG cbih) {
	if (!mpDriver)
		return false;

	bool success = mpDriver->SetVideoFormat((const BITMAPINFOHEADER *)&bih, cbih);

	if (!success)
		return false;

	return true;
}

bool VDCaptureProject::SetAudioFormat(const VDWaveFormat& wfex, LONG cbwfex) {
	if (!mpDriver)
		return false;

	bool bVumeter = mbEnableAudioVumeter;
	bool success = false;

	SetAudioVumeterEnabled(false);
	if (mpDriver->SetAudioFormat((const WAVEFORMATEX *)&wfex, cbwfex)) {
		if (mpCB)
			mpCB->UICaptureAudioFormatUpdated();

		success = true;
	}
	SetAudioVumeterEnabled(bVumeter);

	return success;
}

bool VDCaptureProject::GetVideoFormat(vdstructex<VDAVIBitmapInfoHeader>& bih) {
	vdstructex<BITMAPINFOHEADER> bih0;
	if (!mpDriver || !mpDriver->GetVideoFormat(bih0))
		return false;

	bih.assign((const VDAVIBitmapInfoHeader *)bih0.data(), bih0.size());
	return true;
}

void VDCaptureProject::GetAvailableAudioFormats(std::list<vdstructex<VDWaveFormat> >& aformats) {
	if (mpDriver) {
		std::list<vdstructex<WAVEFORMATEX> > aformats0;
		mpDriver->GetAvailableAudioFormats(aformats0);

		while(!aformats0.empty()) {
			const vdstructex<WAVEFORMATEX>& aformat0 = aformats0.front();
			aformats.push_back(vdstructex<VDWaveFormat>());
			aformats.back().assign((const VDWaveFormat *)aformat0.data(), aformat0.size());
			aformats0.pop_front();
		}
	} else
		aformats.clear();
}

bool VDCaptureProject::GetAudioFormat(vdstructex<VDWaveFormat>& wfex) {
	vdstructex<WAVEFORMATEX> wfex0;
	if (!mpDriver || !mpDriver->GetAudioFormat(wfex0))
		return false;

	wfex.assign((const VDWaveFormat *)wfex0.data(), wfex0.size());
	return true;
}

void VDCaptureProject::ValidateAudioFormat() {
	std::list<vdstructex<VDWaveFormat> > aformats;
	vdstructex<VDWaveFormat> currentFormat;

	GetAudioFormat(currentFormat);
	GetAvailableAudioFormats(aformats);

	std::list<vdstructex<VDWaveFormat> >::const_iterator it(aformats.begin()), itEnd(aformats.end());

	for(int idx=0; it!=itEnd; ++it, ++idx) {
		const vdstructex<VDWaveFormat>& wfex = *it;

		if (wfex == currentFormat) {
			return;
		}
	}

	if (!aformats.empty()) {
		const vdstructex<VDWaveFormat>& wfex = aformats.front();
		SetAudioFormat(*wfex, wfex.size());
	}
}

void VDCaptureProject::SetAudioCompFormat() {
	mAudioCompFormat.clear();
	mAudioCompFormatHint.clear();
}

void VDCaptureProject::SetAudioCompFormat(const VDWaveFormat& wfex, uint32 cbwfex, const char *pHint) {
	if (wfex.mTag == WAVE_FORMAT_PCM) {
		mAudioCompFormat.clear();
		mAudioCompFormatHint.clear();
	} else {
		mAudioCompFormat.assign(&wfex, cbwfex);
		if (pHint)
			mAudioCompFormatHint = pHint;
		else
			mAudioCompFormatHint.clear();
	}
}

bool VDCaptureProject::GetAudioCompFormat(vdstructex<VDWaveFormat>& wfex, VDStringA& hint) {
	wfex.assign((const VDWaveFormat *)mAudioCompFormat.data(), mAudioCompFormat.size());
	hint = mAudioCompFormatHint;
	return !wfex.empty();
}

void VDCaptureProject::SetCaptureFile(const wchar_t *filename, bool bIsStripeSystem) {
	mFilename = filename;
	mbStripingEnabled = bIsStripeSystem;
	if (mpCB)
		mpCB->UICaptureFileUpdated();
}

VDStringW VDCaptureProject::GetCaptureFile() {
	return mFilename;
}

void VDCaptureProject::PreallocateCaptureFile(sint64 size) {
	bool bAttemptExtension = VDFile::enableExtendValid();

	VDFile file(mFilename.c_str(), nsVDFile::kWrite | nsVDFile::kDenyAll | nsVDFile::kOpenAlways);

	file.seek(size);
	file.truncate();

	// attempt to extend valid part of file to avoid preclear cost if admin privileges
	// are available
	if (bAttemptExtension)
		file.extendValidNT(size);

	// zap the first 64K of the file so it isn't recognized as a valid anything
	if (size) {
		vdblock<char> tmp(65536);
		memset(tmp.data(), 0, tmp.size());

		file.seek(0);
		file.write(tmp.data(), tmp.size());
	}
}

bool VDCaptureProject::IsStripingEnabled() {
	return mbStripingEnabled;
}

void VDCaptureProject::SetSpillSystem(bool enable) {
	mbEnableSpill = enable;
}

bool VDCaptureProject::IsSpillEnabled() {
	return mbEnableSpill;
}

void VDCaptureProject::DecrementFileID() {
	VDStringW name(mFilename);
	const wchar_t *s = name.c_str();

	VDStringW::size_type pos = VDFileSplitExt(s) - s;
	
	while(pos > 0) {
		--pos;

		if (iswdigit(name[pos])) {
			if (name[pos] == L'0')
				name[pos] = L'9';
			else {
				--name[pos];
				SetCaptureFile(name.c_str(), mbStripingEnabled);
				if (mpCB)
					mpCB->UICaptureFileUpdated();
				return;
			}
		} else
			break;
	}

	guiSetStatus("Can't decrement filename any farther.", 0);
}

void VDCaptureProject::IncrementFileID() {
	VDStringW name(mFilename);
	const wchar_t *s = name.c_str();

	int pos = VDFileSplitExt(s) - s;
	
	while(--pos >= 0) {
		if (iswdigit(name[pos])) {
			if (name[pos] == '9')
				name[pos] = '0';
			else {
				++name[pos];
				SetCaptureFile(name.c_str(), mbStripingEnabled);
				if (mpCB)
					mpCB->UICaptureFileUpdated();
				return;
			}
		} else
			break;
	}

	name.insert(name.begin() + (pos + 1), L'1');
	SetCaptureFile(name.c_str(), mbStripingEnabled);

	if (mpCB)
		mpCB->UICaptureFileUpdated();
}

void VDCaptureProject::IncrementFileIDUntilClear() {
	for(;;) {
		bool exists = false;
		if (mbEnableSpill) {
			if (VDDoesPathExist((VDFileSplitExtLeft(mFilename) + L".00.avi").c_str()))
				exists = true;
		}

		if (VDDoesPathExist(mFilename.c_str()))
			exists = true;

		if (!exists)
			break;

		IncrementFileID();
	}
}

void VDCaptureProject::ScanForDrivers() {
	tSystems::const_iterator itSys(mSystems.begin()), itSysEnd(mSystems.end());
	int systemID = 0;

	for(; itSys != itSysEnd; ++itSys, ++systemID) {
		IVDCaptureSystem *pSystem = *itSys;

		pSystem->EnumerateDrivers();

		const int nDevices = pSystem->GetDeviceCount();
		for(int dev=0; dev<nDevices; ++dev)
			mDrivers.push_back(DriverEntry(pSystem->GetDeviceName(dev), systemID, dev));
	}

	if (mpCB)
		mpCB->UICaptureDriversUpdated();
}

int VDCaptureProject::GetDriverCount() {
	return mDrivers.size();
}

const wchar_t *VDCaptureProject::GetDriverName(int i) {
	if (i >= 0) {
		tDrivers::const_iterator it(mDrivers.begin()), itEnd(mDrivers.end());

		while(i>0 && it!=itEnd) {
			--i;
			++it;
		}

		if (it != itEnd)
			return (*it).mName.c_str();
	}

	return NULL;
}

int VDCaptureProject::GetDriverByName(const wchar_t *name) {
	return GetByName(mDrivers.size(), &VDCaptureProject::GetDriverName, name);
}

bool VDCaptureProject::SelectDriver(int nDriver) {
	SetDisplayMode(kDisplayNone);

	if (mpDriver) {
		if (mpCB)
			mpCB->UICaptureDriverDisconnecting(mDriverIndex);
	}

	mpDriver = NULL;
	mDriverIndex = -1;

	VDASSERT(nDriver == -1 || (unsigned)nDriver < mDrivers.size());

	if ((unsigned)nDriver >= mDrivers.size()) {
		if (mpCB)
			mpCB->UICaptureDriverChanged(-1);
		return false;
	}

	tDrivers::const_iterator it(mDrivers.begin());
	std::advance(it, nDriver);

	const DriverEntry& ent = *it;

	tSystems::const_iterator itSys(mSystems.begin());

	std::advance(itSys, ent.mSystemId);

	IVDCaptureSystem *pSys = *itSys;

	mpDriver = pSys->CreateDriver(ent.mId);

	if (mpDriver) {
		if (mpCB)
			mpCB->UICaptureDriverChanging(nDriver);

		mpDriver->LockUpdates();

		UpdateTimingOptions();
	}

	if (!mpDriver || !mpDriver->Init(mhwnd)) {
		mpDriver = NULL;
		MessageBox((HWND)mhwnd, "VirtualDub cannot connect to the desired capture driver.", g_szError, MB_OK);
		if (mpCB)
			mpCB->UICaptureDriverChanged(-1);
		return false;
	}

	mDriverIndex = nDriver;
	mpDriver->SetCallback(this);

	mDisplayMode = kDisplayNone;

	if (mpCB) {
		mpCB->UICaptureDriverChanged(nDriver);
		mpCB->UICaptureAudioDriversUpdated();
		mpCB->UICaptureAudioDriverChanged(mpDriver->GetAudioDeviceIndex());
		mpCB->UICaptureAudioInputChanged(mpDriver->GetAudioInputIndex());
		mpCB->UICaptureAudioSourceChanged(mpDriver->GetAudioSourceIndex());
		mpCB->UICaptureParmsUpdated();
		mpCB->UICaptureAudioFormatUpdated();
		mpCB->UICaptureVideoFormatUpdated();
		mpCB->UICaptureVideoPreviewFormatUpdated();
		mpCB->UICaptureVideoSourceChanged(mpDriver->GetVideoSourceIndex());
	}

	mpDriver->SetDisplayVisibility(mbDisplayVisible);
	mpDriver->SetAudioCaptureEnabled(mbAudioCaptureEnabled);
	mpDriver->SetAudioPlaybackEnabled(mbAudioPlaybackEnabled);

	bool bEnableVumeter = mbEnableAudioVumeter;
	mbEnableAudioVumeter = false;
	SetAudioVumeterEnabled(bEnableVumeter);

	mpDriver->UnlockUpdates();

	return true;
}

bool VDCaptureProject::IsDriverConnected() {
	return !!mpDriver;
}

int VDCaptureProject::GetConnectedDriverIndex() {
	return mDriverIndex;
}

void VDCaptureProject::Capture(bool fTest) {
	if (!mpDriver)
		return;

	VDCaptureAutoPriority cpw;

	LONG biSizeToFile;
	VDCaptureData icd;

	bool fMainFinalized = false, fPendingFinalized = false;

	icd.mpProject = this;
	icd.mpError	= NULL;
	icd.mTimingSetup = mTimingSetup;

	vdautoptr<AVIStripeSystem> pStripeSystem;

	VDCaptureStatsFilter statsFilt;
	vdautoptr<IVDCaptureResyncFilter> pResyncFilter(VDCreateCaptureResyncFilter());
	vdautoptr<IVDCaptureAudioCompFilter> pAudioCompFilter(VDCreateCaptureAudioCompFilter());

	MyError pendingError;

	try {
		// flush any currently pending events
		ProcessPendingEvents();

		// validate frame rate first
		VDFraction inputFrameRate(GetFrameRate());

		if (!inputFrameRate.getLo())
			throw MyError("Cannot begin capture because the capture device has no associated frame rate (variable frame rate). This is not currently supported.");

		// get the input filename
		icd.mpszFilename = VDFileSplitPath(mFilename.c_str());

		// get capture parms

		bool bCaptureAudio = IsAudioCaptureEnabled() && IsAudioCaptureAvailable();

		// create an output file object

		if (!fTest) {
			if (mbStripingEnabled) {
				pStripeSystem = new AVIStripeSystem(VDTextWToA(mFilename).c_str());

				if (mbEnableSpill)
					throw MyError("Sorry, striping and spilling are not compatible.");

				icd.mpOutput = new_nothrow AVIOutputStriped(pStripeSystem);
				if (!icd.mpOutput)
					throw MyMemoryError();

				if (g_prefs.fAVIRestrict1Gb)
					((AVIOutputStriped *)icd.mpOutput)->set_1Gb_limit();
			} else {
				icd.mpOutputFile = VDCreateMediaOutputAVIFile();
				if (!icd.mpOutputFile)
					throw MyMemoryError();

				if (g_prefs.fAVIRestrict1Gb)
					icd.mpOutputFile->set_1Gb_limit();

				uint32 superIndexLimit, subIndexLimit;
				VDPreferencesGetAVIIndexingLimits(superIndexLimit, subIndexLimit);

				icd.mpOutputFile->setIndexingLimits(superIndexLimit, subIndexLimit);

				icd.mpOutputFile->set_capture_mode(true);
				icd.mpOutput = icd.mpOutputFile;
			}

			// initialize the AVIOutputFile object

			icd.mpVideoOut = icd.mpOutput->createVideoStream();
			icd.mpAudioOut = NULL;
			if (bCaptureAudio)
				icd.mpAudioOut = icd.mpOutput->createAudioStream();

			if (!mbStripingEnabled)
				icd.mpOutputFile->setAlignment(0, 8);
		}

		// initialize audio
		vdstructex<VDWaveFormat> wfexInput;
		vdstructex<VDWaveFormat>& wfexOutput = mAudioCompFormat.empty() ? wfexInput : mAudioCompFormat;

		pResyncFilter->SetVideoRate(inputFrameRate.asDouble());
		pResyncFilter->EnableVideoDrops(mTimingSetup.mbAllowEarlyDrops);
		pResyncFilter->EnableVideoInserts(mTimingSetup.mbAllowLateInserts);
		pResyncFilter->SetVideoInsertLimit(mTimingSetup.mInsertLimit);

		if (mTimingSetup.mbUseFixedAudioLatency)
			pResyncFilter->SetFixedAudioLatency(mTimingSetup.mAudioLatency);
		else if (mTimingSetup.mbUseLimitedAutoAudioLatency)
			pResyncFilter->SetLimitedAutoAudioLatency(mTimingSetup.mAutoAudioLatencyLimit);
		else
			pResyncFilter->SetAutoAudioLatency();

		if (g_pCaptureProfiler) {
			g_pCaptureProfiler->Clear();
			pResyncFilter->SetProfiler(g_pCaptureProfiler);
		}

		bool useVideoTimingCorrection = mTimingSetup.mbCorrectVideoTiming;

		if (bCaptureAudio) {
			if (!GetAudioFormat(wfexInput)) {
//#pragma vdpragma_TODO("Should probably give user feedback when audio capture isn't available.")
				bCaptureAudio = false;
			} else {
				pResyncFilter->SetAudioRate(wfexInput->mDataRate);
				pResyncFilter->SetAudioChannels(wfexInput->mChannels);

				if (mTimingSetup.mbResyncWithIntegratedAudio || !mpDriver->IsAudioDeviceIntegrated(mpDriver->GetAudioDeviceIndex())) {
					switch(mTimingSetup.mSyncMode) {
					case VDCaptureTimingSetup::kSyncAudioToVideo:
						if (wfexInput->mTag == WAVE_FORMAT_PCM) {
							switch(wfexInput->mSampleBits) {
							case 8:
								pResyncFilter->SetAudioFormat(kVDAudioSampleType8U);
								break;
							case 16:
								pResyncFilter->SetAudioFormat(kVDAudioSampleType16S);
								break;
							default:
								goto unknown_PCM_format;
							}

							pResyncFilter->SetResyncMode(IVDCaptureResyncFilter::kModeResampleAudio);
							break;
						}
						// fall through -- format isn't PCM so we can't resample it
					case VDCaptureTimingSetup::kSyncVideoToAudio:
unknown_PCM_format:
						pResyncFilter->SetResyncMode(IVDCaptureResyncFilter::kModeResampleVideo);
						useVideoTimingCorrection = false;
						break;
					}
				} else {
					useVideoTimingCorrection = false;
				}
			}
		} else {
			pResyncFilter->SetAudioChannels(0);
		}

		pResyncFilter->EnableVideoTimingCorrection(useVideoTimingCorrection);
		pResyncFilter->EnableAudioClock(mTimingSetup.mbUseAudioTimestamps);

		// initialize video
		vdstructex<VDAVIBitmapInfoHeader> bmiInput;
		if (!GetVideoFormat(bmiInput))
			throw MyError("The current video capture format is not compatible with AVI files.");

		// initialize filtering
		vdstructex<VDAVIBitmapInfoHeader> filteredFormat;
		VDAVIBitmapInfoHeader *bmiToFile = bmiInput.data();
		biSizeToFile = bmiInput.size();

		icd.mInputLayout.format = 0;

		VDFraction outputFrameRate(inputFrameRate);
		if (InitFilter()) {		// This also sets mFilterInputLayout even if it returns false.
			icd.mpFilterSys = mpFilterSys;

			VDMakeBitmapFormatFromPixmapFormat(filteredFormat, bmiInput, mFilterOutputLayout.format, 0, mFilterOutputLayout.w, mFilterOutputLayout.h);

			bmiToFile = &*filteredFormat;
			biSizeToFile = filteredFormat.size();
			outputFrameRate = mpFilterSys->GetOutputFrameRate();
		}

		icd.mInputLayout = mFilterInputLayout;
		VDGetPixmapLayoutForBitmapFormat(*bmiToFile, biSizeToFile, icd.mOutputLayout);
		icd.vfwLayout.format = 0;

		// initialize final conversion
		vdstructex<VDAVIBitmapInfoHeader> convertedFormat;
		if (g_compformat!=0 && g_compression.driver) {
			//! compchoose does not let to select uncompressed format
			// explicit: convert to selected as "Pixel Format"
			if (icd.mOutputLayout.format!=g_compformat) {
				VDMakeBitmapFormatFromPixmapFormat(convertedFormat, g_compformat, 0, bmiToFile->biWidth, bmiToFile->biHeight);
				bmiToFile = &*convertedFormat;
				biSizeToFile = convertedFormat.size();
				VDGetPixmapLayoutForBitmapFormat(*bmiToFile, biSizeToFile, icd.vfwLayout);
			}
		} else {
			// auto: convert back to capture format
			int format2 = VDBitmapFormatToPixmapFormat(*bmiInput);
			if (icd.mOutputLayout.format!=format2) {
				VDMakeBitmapFormatFromPixmapFormat(convertedFormat, format2, 0, bmiToFile->biWidth, bmiToFile->biHeight);
				bmiToFile = &*convertedFormat;
				biSizeToFile = convertedFormat.size();
				VDGetPixmapLayoutForBitmapFormat(*bmiToFile, biSizeToFile, icd.vfwLayout);
			}
		}

		// initialize video compression
		vdstructex<BITMAPINFOHEADER> bmiOutput;
		icd.driverLayout.format = 0;

		if (g_compression.driver) {
			FilterModPixmapInfo outputFormatInfo;
			outputFormatInfo.clear();
			int outputFormatID = g_compression.driver->queryInputFormat(&outputFormatInfo);
			//VDPixmapLayout driverLayout;
			//VDGetPixmapLayoutForBitmapFormat(*(VDAVIBitmapInfoHeader*)bmiToFile,0,driverLayout);
			VDPixmapCreateLinearLayout(icd.driverLayout,outputFormatID,bmiToFile->biWidth,abs(bmiToFile->biHeight),16);
			if (g_compression.driver->compressQuery(NULL, NULL, &icd.driverLayout)==ICERR_OK) {
				// use layout
				if (mFilterInputLayout.format==0)
					throw MyError("The current video capture format is not supported");
			} else {
				icd.driverLayout.format = 0;

				DWORD icErr = g_compression.driver->compressQuery(bmiToFile, NULL);
				if (icErr != ICERR_OK)
					throw MyICError("Video compressor", icErr);
			}

			if (!(icd.mpVideoCompressor = new VideoSequenceCompressor()))
				throw MyMemoryError();

			VDFraction scaledRate(1000000, VDClampToSint32(outputFrameRate.scale64ir(1000000)));
			icd.mpVideoCompressor->SetDriver(g_compression.driver, g_compression.lDataRate*1024, g_compression.lQ, g_compression.lKey, false);
			if (icd.driverLayout.format) {
				icd.mpVideoCompressor->GetOutputFormat(&icd.driverLayout, bmiOutput);
				icd.mpVideoCompressor->Start(icd.driverLayout, outputFormatInfo, bmiOutput.data(), bmiOutput.size(), scaledRate, 0x0FFFFFFF);
				icd.mpVideoCompressor->GetOutputFormat(&icd.driverLayout, bmiOutput);
				icd.createOutputBlitter();
			} else {
				icd.mpVideoCompressor->GetOutputFormat(bmiToFile, bmiOutput);
				icd.mpVideoCompressor->Start(bmiToFile, biSizeToFile, bmiOutput.data(), bmiOutput.size(), scaledRate, 0x0FFFFFFF);
				icd.createOutputBlitter();
			}
			icd.pOutputBuffer = icd.mpVideoCompressor->createResultBuffer();

			bmiToFile = (VDAVIBitmapInfoHeader *)bmiOutput.data();
			biSizeToFile = bmiOutput.size();
		} else {
			icd.createOutputBlitter();
		}

		// set up output file headers and formats

		icd.mFramePeriod	= VDClampToUint32(outputFrameRate.scale64ir(10000000));
		icd.mbNTSC			= (outputFrameRate == VDFraction(30000, 1001));

		if (!fTest) {
			// setup stream headers

			VDXStreamInfo vsi;
			VDXAVIStreamHeader& vstrhdr = vsi.aviHeader;

			vstrhdr.fccType					= streamtypeVIDEO;
			vstrhdr.fccHandler				= bmiToFile->biCompression;

			vstrhdr.dwScale					= outputFrameRate.getLo();
			vstrhdr.dwRate					= outputFrameRate.getHi();

			if (icd.mbNTSC) {
				vstrhdr.dwScale = 1001;
				vstrhdr.dwRate = 30000;
			}

			vstrhdr.dwSuggestedBufferSize	= 0;
			vstrhdr.dwQuality				= g_compression.driver ? g_compression.lQ : (unsigned long)-1;
			vstrhdr.rcFrame.left			= 0;
			vstrhdr.rcFrame.top				= 0;
			vstrhdr.rcFrame.right			= (short)bmiToFile->biWidth;
			vstrhdr.rcFrame.bottom			= (short)abs(bmiToFile->biHeight);

			icd.mpVideoOut->setFormat(bmiToFile, biSizeToFile);
			icd.mpVideoOut->setStreamInfo(vsi);

			if (bCaptureAudio) {
				VDXStreamInfo asi;
				VDXAVIStreamHeader& astrhdr = asi.aviHeader;
				astrhdr.fccType				= streamtypeAUDIO;
				astrhdr.fccHandler			= 0;
				astrhdr.dwScale				= wfexOutput->mBlockSize;
				astrhdr.dwRate				= wfexOutput->mDataRate;
				astrhdr.dwQuality			= (unsigned long)-1;
				astrhdr.dwSampleSize		= wfexOutput->mBlockSize; 

				icd.mpAudioOut->setFormat(wfexOutput.data(), wfexOutput.size());
				icd.mpAudioOut->setStreamInfo(asi);
			}
		}

		// Setup capture structure
		if (bCaptureAudio) {
			memcpy(&icd.mwfex, wfexOutput.data(), std::min<unsigned>(wfexOutput.size(), sizeof icd.mwfex));
			icd.mAudioSampleSize	= wfexOutput->mBlockSize;
		}

		icd.mCaptureRoot	= VDFileSplitPathLeft(mFilename);
		icd.mpszPath		= icd.mCaptureRoot.c_str();

		// set up resynchronizer and stats filter
		IVDCaptureDriverCallback *pCurrentCB = this;

		icd.mpAudioCompFilter = NULL;
		if (bCaptureAudio && !mAudioCompFormat.empty()) {
			icd.mpAudioCompFilter = pAudioCompFilter;
			pAudioCompFilter->SetChildCallback(pCurrentCB);
			pCurrentCB = pAudioCompFilter;
			pAudioCompFilter->SetSourceSplit(!mAudioAnalysisFormat.empty());
			pAudioCompFilter->Init((const WAVEFORMATEX *)wfexInput.data(), (const WAVEFORMATEX *)wfexOutput.data(), mAudioCompFormatHint.c_str());
		}

		pResyncFilter->SetChildCallback(pCurrentCB);
		pCurrentCB = pResyncFilter;

		statsFilt.Init(pCurrentCB, bCaptureAudio ? (const WAVEFORMATEX *)wfexInput.data() : NULL);
		pCurrentCB = &statsFilt;

		// setup log filter

		if (mbLoggingEnabled)
			mpLogFilter = VDCreateCaptureLogFilter();
		else
			mpLogFilter = NULL;

		if (mpLogFilter) {
			mpLogFilter->SetChildCallback(&statsFilt);
			pCurrentCB = mpLogFilter;
		}

		mpDriver->SetCallback(pCurrentCB);

		icd.mpStatsFilter	= &statsFilt;
		icd.mpResyncFilter	= pResyncFilter;

		icd.mStatsChannelVTJitter = -1;
		if (g_pCaptureProfiler)
			icd.mStatsChannelVTJitter = g_pCaptureProfiler->RegisterStatsChannel("Video time jitter");


		// initialize the file
		//
		// this is kinda sick

		if (!fTest) {
			if (mFilename.empty())
				throw MyError("No capture filename has been set. Use File > Set Capture File... to choose a location for the capture file.");

			if (!pStripeSystem && mDiskSettings.mbDisableWriteCache) {
				icd.mpOutputFile->disable_os_caching();
				icd.mpOutputFile->setBuffering(1024 * mDiskSettings.mDiskChunkSize * mDiskSettings.mDiskChunkCount, 1024 * mDiskSettings.mDiskChunkSize);
			}

			if (mbEnableSpill) {
				VDStringW firstFileName(VDFileSplitExtLeft(mFilename));

				icd.mpOutputFile->setSegmentHintBlock(true, NULL, MAX_PATH+1);

				firstFileName += L".00.avi";

				icd.mpOutputFile->init(firstFileName.c_str());

				// Figure out what drive the first file is on, to get the disk threshold.  If we
				// don't know, make it 50Mb.

				CapSpillDrive *pcsd;

				if (pcsd = CapSpillFindDrive(firstFileName.c_str()))
					icd.mSizeThreshold = pcsd->threshold;
				else
					icd.mSizeThreshold = 50;
			} else
				if (!icd.mpOutput->init(mFilename.c_str()))
					throw MyError("Error initializing capture file.");
		}

		// Allocate audio buffer and begin IO thread.

		if (mbEnableSpill) {
			if (!icd.ThreadStart())
				throw MyWin32Error("Can't start I/O thread: %%s", GetLastError());
		}

		// capture!!

		mpCaptureData = &icd;
		mMainThreadId = GetCurrentThreadId();

		if (mpCB)
			mpCB->UICaptureStart(fTest);

		try {
			if (!mpDriver->CaptureStart()) {
				icd.mpError = new_nothrow MyError("Unable to start video capture.");
			} else {

				vdstructex<WAVEFORMATEX> wfex;
				if (mpDriver->GetAudioFormat(wfex)) {
					if (mpCB)
						mpCB->UICaptureAudioFormatUpdated();
				}

				VDSamplingAutoProfileScope autoVTProfile;

				MSG msg;

				for(;;) {
					BOOL result = GetMessage(&msg, NULL, 0, 0);

					if (result == (BOOL)-1)
						break;

					if (!result) {
						PostQuitMessage(msg.wParam);
						break;
					}

					if (!msg.hwnd && msg.message == WM_APP+100)
						break;

					if (!guiCheckDialogs(&msg) && !VDUIFrame::TranslateAcceleratorMessage(msg)) {
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}
				}
			}

			mpDriver->CaptureAbort();
		} catch(MyError& e) {
			if (!icd.mpError)
				icd.mpError = new_nothrow MyError(e);
		}

		if (mpCB)
			mpCB->UICaptureEnd(!icd.mpError);

VDDEBUG("Capture has stopped.\n");

		if (icd.mpError) {
			MyError e;

			e.TransferFrom(*icd.mpError);
			icd.mpError = NULL;
			throw e;
		}

		if (icd.isThreadAttached()) {
			icd.PostExitRequest();
			icd.ThreadWait();
		}

		if (icd.mpVideoCompressor)
			icd.mpVideoCompressor->Stop();

		// finalize files

		if (!fTest) {
			VDDEBUG("Finalizing main file.\n");

			fMainFinalized = true;
			icd.mpOutput->finalize();

			fPendingFinalized = true;
			if (icd.mpOutputFilePending && icd.mpOutputFilePending != icd.mpOutputFile) {
				VDDEBUG("Finalizing pending file.\n");

				icd.mpOutputFilePending->finalize();
			}
		}

		VDDEBUG("Yatta!!!\n");
	} catch(MyError& e) {
		pendingError.TransferFrom(e);
	}

	// restore the callback
	mpDriver->SetCallback(this);

	mpCaptureData = NULL;

	// Kill the I/O thread.

	if (icd.isThreadAttached()) {
		icd.PostExitRequest();
		icd.ThreadWait();
	}

	// Might as well try and finalize anyway.  If we're finalizing here,
	// we encountered an error, so don't go and throw more out!

	if (!fTest)
		try {
			if (!fMainFinalized) {
				if (icd.mpOutput)
					icd.mpOutput->finalize();
			}

			if (!fPendingFinalized && icd.mpOutputFilePending && icd.mpOutputFilePending != icd.mpOutputFile)
				icd.mpOutputFilePending->finalize();
		} catch(const MyError&) {
		}

	if (mDisplayMode != kDisplayAnalyze)
		ShutdownFilter();

	icd.mpVideoCompressor = NULL;
	delete icd.pOutputBuffer;
	icd.pOutputBuffer = 0;

	if (icd.mpOutputFilePending && icd.mpOutputFilePending == icd.mpOutputFile)
		icd.mpOutputFilePending = NULL;

	delete icd.mpOutput;
	delete icd.mpOutputFilePending;

	delete icd.mpSpillError;

	// throw any pending errors
	if (pendingError.gets())
		throw pendingError;

	// any warnings?

#if 0
	DWORD dw;

	if (icd.mbVideoTimingWrapDetected) {
		if (!QueryConfigDword(g_szCapture, g_szWarnTiming1, &dw) || !dw) {
			if (IDYES != MessageBox((HWND)mhwnd,
					"VirtualDub has detected, and compensated for, a possible bug in your video capture drivers that is causing "
					"its timing information to wrap around at 35 or 71 minutes.  Your capture should be okay, but you may want "
					"to try upgrading your video capture drivers anyway, since this can cause video capture to halt in "
					"other applications.\n"
					"\n"
					"Do you want VirtualDub to warn you the next time this happens?"
					, "VirtualDub Warning", MB_YESNO))

				SetConfigDword(g_szCapture, g_szWarnTiming1, 1);
		}
	}
#endif
}

void VDCaptureProject::CaptureStop() {
	if (mpCaptureData)
		mpDriver->CaptureStop();
}

LRESULT	VDCaptureProject::OnEngineEvent(WPARAM wParam, LPARAM lParam) {
	ProcessPendingEvents();
	return 0;
}

void VDCaptureProject::ProcessPendingEvents() {
	DriverEvent event;

	for(;;) {
		vdsynchronized(mEventLock) {
			if (mPendingEvents.empty())
				return;

			event = mPendingEvents.front();

			// Yes, this is an expensive vector erase()....
			mPendingEvents.erase(mPendingEvents.begin());
		}

		switch(event) {
		case kEventVideoFrameRateChanged:
			if (mpCB)
				mpCB->UICaptureParmsUpdated();
			break;

		case kEventVideoFormatChanged:
			if (mDisplayMode == kDisplayAnalyze)
				ShutdownVideoAnalysis();
			if (mpCB) {
				mpCB->UICaptureVideoFormatUpdated();
				mpCB->UICaptureVideoPreviewFormatUpdated();
				mpCB->UICaptureParmsUpdated();
			}
			if (mDisplayMode == kDisplayAnalyze)
				InitVideoAnalysis();

			VDVERIFY(--mSuspendVideoFrameTransferCount >= 0);
			break;
		}
	}
}

void VDCaptureProject::CapBegin(sint64 global_clock) {
}

void VDCaptureProject::CapEnd(const MyError *pError) {
	if (pError) {
		if (!mpCaptureData->mpError)
			mpCaptureData->mpError = new MyError(*pError);
	}

	PostThreadMessage(mMainThreadId, WM_APP+100, 0, 0);
}

bool VDCaptureProject::CapEvent(DriverEvent event, int data) {
	// WARNING: This executes asynchronously!

	switch(event) {
	case kEventPreroll:
		if (mpCB && !mpCB->UICapturePreroll())
			return false;
		CaptureBT848Reassert();
		break;

	case kEventCapturing:
		vdsynchronized(mStopPrefsLock) {
			VDCaptureData *const cd = mpCaptureData;
			vdsynchronized(cd->mCallbackLock) {

				if (mStopPrefs.fEnableFlags & CAPSTOP_TIME)
					if (cd->mLastTime >= mStopPrefs.lTimeLimit*1000)
						return false;

				if (mStopPrefs.fEnableFlags & CAPSTOP_FILESIZE)
					if ((long)((cd->mTotalVideoSize + cd->mTotalAudioSize + 2048)>>20) > mStopPrefs.lSizeLimit)
						return false;

				if (mStopPrefs.fEnableFlags & CAPSTOP_DISKSPACE)
					if (cd->mDiskFreeBytes && (long)(cd->mDiskFreeBytes>>20) < mStopPrefs.lDiskSpaceThreshold)
						return false;

				if (mStopPrefs.fEnableFlags & CAPSTOP_DROPRATE)
					if (cd->mTotalFramesCaptured > 50 && (cd->mFramesDropped + cd->mFramesInserted)*100 > mStopPrefs.lMaxDropRate*cd->mTotalFramesCaptured)
						return false;
			}
		}
		break;

	case kEventVideoFrameRateChanged:
	case kEventVideoFormatChanged:
		if (event == kEventVideoFormatChanged)
			++mSuspendVideoFrameTransferCount;

		vdsynchronized(mEventLock) {
			mPendingEvents.push_back(event);
		}

		PostMessage((HWND)mhwnd, VDWM_ENGINE_EVENT, 0, 0);
		break;

	case kEventVideoFramesDropped:
		if (mpCaptureData)
			mpCaptureData->OnFramesDropped(data);
		break;

	case kEventVideoFramesInserted:
		if (mpCaptureData)
			mpCaptureData->OnFramesInserted(data);
		break;

	case kEventVideoSourceChanged:
		if (mpCB)
			mpCB->UICaptureVideoSourceChanged(data);
		break;

	case kEventAudioSourceChanged:
		if (mpCB)
			mpCB->UICaptureAudioSourceChanged(data);
		break;
	}

	return true;
}

void VDCaptureProject::CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock) {
	VDCaptureData *const icd = mpCaptureData;

	__try {
		CapProcessData2(stream, data, size, timestamp, key, global_clock);
	} __except(((EXCEPTION_POINTERS *)_exception_info())->ExceptionRecord->ExceptionCode == 0xe06d7363 ||
			IsDebuggerPresent()
			? false
			: CrashHandler((EXCEPTION_POINTERS *)_exception_info(), false)) {
		icd->OnException();
		mpDriver->CaptureAbort();
	}
}

void VDCaptureProject::CapProcessData2(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock) {
	if (stream < 0) {
		if (mpCB) {
			if (stream == -1) {
				VDPixmap px(VDPixmapFromLayout(mFilterInputLayout, (void *)data));
				bool firstFrame = true;

				vdsynchronized(mVideoFilterLock) {
					if (!mpFilterSys) {
						DispatchAnalysis(px);
					} else {
						for(;;) {
							bool validFrame = true;

							if (mpFilterSys) {
								if (firstFrame)
									mpFilterSys->ProcessIn(px);

								void *data;
								uint32 size;
								validFrame = mpFilterSys->ProcessOut(px, data, size);
							}

							if (!validFrame)
								break;

							// Must do this under lock so that a filter target bitmap is not deleted while
							// display is happening.
							if (firstFrame) {
								firstFrame = false;
								DispatchAnalysis(px);
							}
						}
					}
				}
			} else {
				if (!mAudioAnalysisFormat.empty() && is_audio_pcm((VDWaveFormat*)mAudioAnalysisFormat.data())) {
					float peak[16];
					int n = mAudioAnalysisFormat->nChannels;
					vdstructex<WAVEFORMATEX> wfex;
					mpDriver->GetAudioFormat(wfex);
					if(wfex==mAudioAnalysisFormat)
						VDComputeWavePeaks(data, mAudioAnalysisFormat->wBitsPerSample, mAudioAnalysisFormat->nChannels, size / mAudioAnalysisFormat->nBlockAlign, peak);
					else
						n = 0;
					if (mpCB)
						mpCB->UICaptureAudioPeaksUpdated(n, peak);
				}
			}
		}
		return;
	}

	VDCaptureData *const icd = mpCaptureData;

	if (icd->mpError)
		return;

	if (MyError *e = icd->mpSpillError.xchg(NULL)) {
		icd->mpError = e;
		mpDriver->CaptureAbort();
		return;
	}

	bool success;

	if (stream > 0) {
		success = icd->WaveCallback(data, size, global_clock);

		if (!icd->mpAudioCompFilter && !mAudioAnalysisFormat.empty() && is_audio_pcm((VDWaveFormat*)mAudioAnalysisFormat.data())) {
			float peak[16];
			int n = mAudioAnalysisFormat->nChannels;
			vdstructex<WAVEFORMATEX> wfex;
			mpDriver->GetAudioFormat(wfex);
			if(wfex==mAudioAnalysisFormat)
				VDComputeWavePeaks(data, mAudioAnalysisFormat->wBitsPerSample, mAudioAnalysisFormat->nChannels, size / mAudioAnalysisFormat->nBlockAlign, peak);
			else
				n = 0;
			if (mpCB)
				mpCB->UICaptureAudioPeaksUpdated(n, peak);
		}
	} else {
		if (!stream)
			success = icd->VideoCallback(data, size, timestamp, key, global_clock);
	}
}

namespace {
	bool fuzzymatch(const wchar_t *s, const wchar_t *t) {
		for(;;) {
			wchar_t c = *s;
			wchar_t d = *t;

			if (c == d) {
				if (!c)
					return true;

				++s;
				++t;
				continue;
			}

			if ((unsigned)(c-1) < 0x7f && !isalnum((int)(0xff & c))) {
				++s;
				continue;
			}

			if ((unsigned)(d-1) < 0x7f && !isalnum((int)(0xff & d))) {
				++t;
				continue;
			}

			return false;
		}
	}

	bool wcsistr(const wchar_t *s, const wchar_t *t) {		// slow, but that's OK here
		const size_t l1 = wcslen(s);
		const size_t l2 = wcslen(t);

		if (l1 < l2)
			return false;

		for(int off=0; off<(int)(l1-l2); ++off) {
			if (!_wcsnicmp(s+off, t, l2))
				return true;
		}

		return false;
	}
}

int	VDCaptureProject::GetByName(int count, const wchar_t *(VDCaptureProject::*pGetNameRout)(int), const wchar_t *name) {
	int best = -1;
	int bestscore = 0;

	for(int i=0; i<count; ++i) {
		const wchar_t *s = (this->*pGetNameRout)(i);

		if (!s)
			continue;

		// exact match automatically wins
		if (!wcscmp(s, name))
			return i;

		// two points for fuzzy match, one for substring match
		if (fuzzymatch(s, name)) {
			if (bestscore < 2) {
				bestscore = 2;
				best = i;
			}
		} else if (wcsistr(s, name)) {
			if (bestscore < 1) {
				bestscore = 1;
				best = i;
			}
		}
	}

	return best;
}

void VDCaptureProject::UpdateTimingOptions() {
	IVDCaptureDriverDShow *pds = vdpoly_cast<IVDCaptureDriverDShow *>(mpDriver);

	if (pds) {
		pds->SetDisableClockForPreview(mTimingSetup.mbDisableClockForPreview);
		pds->SetForceAudioRendererClock(mTimingSetup.mbForceAudioRendererClock);
		pds->SetIgnoreVideoTimestamps(mTimingSetup.mbIgnoreVideoTimestamps);
	}
}

void VDCaptureProject::InitVideoAnalysis() {
	InitFilter();

	if (mbEnableVideoHistogram)
		InitVideoHistogram();

	if (mbEnableVideoFrameTransfer)
		InitVideoFrameTransfer();

	if (mpCB)
		mpCB->UICaptureVideoPreviewFormatUpdated();
}

void VDCaptureProject::ShutdownVideoAnalysis() {
	// The order of these is critical if preview is running when this
	// is called.
	ShutdownVideoFrameTransfer();
	ShutdownVideoHistogram();
	ShutdownFilter();

	if (mpCB)
		mpCB->UICaptureVideoPreviewFormatUpdated();
}

bool VDCaptureProject::AreFiltersEnabled(const VDCaptureFilterSetup& filtsetup) {
	bool nonTrivialCrop = filtsetup.mCropRect.left
						+ filtsetup.mCropRect.top
						+ filtsetup.mCropRect.right
						+ filtsetup.mCropRect.bottom > 0;

	if (   !nonTrivialCrop
		&& !filtsetup.mbEnableFilterChain
		&& !filtsetup.mbEnableLumaSquishBlack
		&& !filtsetup.mbEnableLumaSquishWhite
		&& !filtsetup.mbEnableFieldSwap
		&& !filtsetup.mVertSquashMode
		&& !filtsetup.mbEnableNoiseReduction)
	{
		return false;
	}

	return true;
}

bool VDCaptureProject::InitFilter() {
	if (mpFilterSys)
		return true;

	mFilterInputLayout.format = 0;

	vdstructex<VDAVIBitmapInfoHeader> vformat;
	if (!GetVideoFormat(vformat))
		return false;

	int variant;
	int format = VDBitmapFormatToPixmapFormat(*vformat, variant);

	if (!format)
		return false;

	// some code needs this, so we need to do it anyway
	VDMakeBitmapCompatiblePixmapLayout(mFilterInputLayout, vformat->biWidth, vformat->biHeight, format, variant);

	if (!AreFiltersEnabled(mFilterSetup))
		return false;

	uint32 palEnts = 0;

	memset(mFilterPalette, 0, sizeof mFilterPalette);

	if (vformat->biCompression == BI_RGB && vformat->biBitCount <= 8) {
		palEnts = vformat->biClrUsed;

		if (!palEnts)
			palEnts = 1 << vformat->biBitCount;
	}

	int palOffset = VDGetSizeOfBitmapHeaderW32((const BITMAPINFOHEADER *)vformat.data());
	int realPalEnts = (vformat.size() - palOffset) >> 2;

	if (realPalEnts > 0) {
		if (palEnts > (uint32)realPalEnts)
			palEnts = realPalEnts;

		if (palEnts > 256)
			palEnts = 256;

		memcpy(mFilterPalette, (char *)vformat.data() + palOffset, sizeof(uint32)*palEnts);
	}

	mFilterInputLayout.palette = mFilterPalette;

	vdautoptr<IVDCaptureFilterSystem> pFilterSys(VDCreateCaptureFilterSystem());

	pFilterSys->SetCrop(mFilterSetup.mCropRect.left,
							mFilterSetup.mCropRect.top,
							mFilterSetup.mCropRect.right,
							mFilterSetup.mCropRect.bottom);

	if (mFilterSetup.mbEnableNoiseReduction)
		pFilterSys->SetNoiseReduction(mFilterSetup.mNRThreshold);

	pFilterSys->SetLumaSquish(mFilterSetup.mbEnableLumaSquishBlack, mFilterSetup.mbEnableLumaSquishWhite);
	pFilterSys->SetFieldSwap(mFilterSetup.mbEnableFieldSwap);
	pFilterSys->SetVertSquashMode(mFilterSetup.mVertSquashMode);
	pFilterSys->SetChainEnable(mFilterSetup.mbEnableFilterChain, !mFilterSetup.mbSkipFilterChainConversion);

	mFilterOutputLayout = mFilterInputLayout;
	pFilterSys->Init(mFilterOutputLayout, GetFrameRate());

	vdsynchronized(mVideoFilterLock) {
		// This could be atomic, but it's not that critical.
		mpFilterSys = pFilterSys.release();
	}

	return true;
}

void VDCaptureProject::ShutdownFilter() {
	vdsynchronized(mVideoFilterLock) {
		// This has to be under lock, because we need to ensure not only
		// that mpFilterSys isn't being accessed, but also that the filter
		// system isn't being used.
		mpFilterSys = NULL;
	}
}

bool VDCaptureProject::InitVideoHistogram() {
	if (mpVideoHistogram)
		return true;

	vdsynchronized(mVideoAnalysisLock) {
		mpVideoHistogram = VDCreateCaptureVideoHistogram();
		if (!mpVideoHistogram)
			return false;
	}

	if (mpCB)
		mpCB->UICaptureVideoHistoBegin();
	return true;
}

void VDCaptureProject::ShutdownVideoHistogram() {
	if (!mpVideoHistogram)
		return;

	vdsynchronized(mVideoAnalysisLock) {
		if (mpCB)
			mpCB->UICaptureVideoHistoEnd();

		mpVideoHistogram = NULL;
	}
}

bool VDCaptureProject::InitVideoFrameTransfer() {
	if (mpCB && !mbVideoFrameTransferActive) {
		VDPixmap px;
		
		px.data		= NULL;
		px.data2	= NULL;
		px.data3	= NULL;
		px.format	= mFilterInputLayout.format;
		px.w		= mFilterInputLayout.w;
		px.h		= mFilterInputLayout.h;
		px.palette	= mFilterInputLayout.palette;
		px.pitch	= mFilterInputLayout.pitch;
		px.pitch2	= mFilterInputLayout.pitch2;
		px.pitch3	= mFilterInputLayout.pitch3;

		mpCB->UICaptureAnalyzeBegin(px);

		vdsynchronized(mVideoAnalysisLock) {
			mbVideoFrameTransferActive = true;
		}
	}

	return true;
}

void VDCaptureProject::ShutdownVideoFrameTransfer() {
	if (mbVideoFrameTransferActive) {
		// synchronize to make sure we're not actually doing the transfer
		// while trying to disable it
		vdsynchronized(mVideoAnalysisLock) {
			mbVideoFrameTransferActive = false;
		}

		if (mpCB)
			mpCB->UICaptureAnalyzeEnd();
	}
}

void VDCaptureProject::DispatchAnalysis(const VDPixmap& px) {
	vdsynchronized(mVideoAnalysisLock) {
		if (mDisplayMode == kDisplayAnalyze && mpCB && !mSuspendVideoFrameTransferCount) {
			if (mpVideoHistogram) {
				float data[256];

				float scale = 0.1f;
				if (mpVideoHistogram->Process(px, data, scale)) {
					mpCB->UICaptureVideoHisto(data);
				}

			}

			if (mbVideoFrameTransferActive) {
				mpCB->UICaptureAnalyzeFrame(px);
			}
		}
	}
}











































///////////////////////////////////////////////////////////////////////////
//
//	Internal capture
//
///////////////////////////////////////////////////////////////////////////

void VDCaptureData::CreateNewFile() {
	IVDMediaOutputAVIFile *pNewFile = NULL;
	BITMAPINFO *bmi;
	wchar_t fname[MAX_PATH];
	CapSpillDrive *pcsd;

	pcsd = CapSpillPickDrive(false);
	if (!pcsd) {
		mbAllFull = true;
		return;
	}

	mpszNewPath = pcsd->path.c_str();

	try {
		pNewFile = VDCreateMediaOutputAVIFile();
		if (!pNewFile)
			throw MyMemoryError();

		pNewFile->setSegmentHintBlock(true, NULL, MAX_PATH+1);

		IVDMediaOutputStream *pNewVideo = pNewFile->createVideoStream();
		IVDMediaOutputStream *pNewAudio = NULL;
		
		if (mpAudioOut)
			pNewAudio = pNewFile->createAudioStream();

		if (g_prefs.fAVIRestrict1Gb)
			pNewFile->set_1Gb_limit();

		uint32 superIndexLimit, subIndexLimit;
		VDPreferencesGetAVIIndexingLimits(superIndexLimit, subIndexLimit);

		pNewFile->setIndexingLimits(superIndexLimit, subIndexLimit);

		pNewFile->set_capture_mode(true);
		pNewFile->setAlignment(0, 8);

		// copy over information to new file

		pNewVideo->setStreamInfo(mpVideoOut->getStreamInfo());
		pNewVideo->setFormat(mpVideoOut->getFormat(), mpVideoOut->getFormatLen());

		if (mpAudioOut) {
			pNewAudio->setStreamInfo(mpAudioOut->getStreamInfo());
			pNewAudio->setFormat(mpAudioOut->getFormat(), mpAudioOut->getFormatLen());
		} 

		// init the new file

		if (!mpProject->IsStripingEnabled()) {
			const VDCaptureDiskSettings& sets = mpProject->GetDiskSettings();

			if (sets.mbDisableWriteCache) {
				pNewFile->disable_os_caching();
				pNewFile->setBuffering(1024 * sets.mDiskChunkSize * sets.mDiskChunkCount, 1024 * sets.mDiskChunkSize);
			}
		}

		bmi = (BITMAPINFO *)mpVideoOut->getFormat();

		pcsd->makePath(fname, mpszFilename);

		// edit the filename up

		wchar_t *ext = const_cast<wchar_t *>(VDFileSplitExt(fname));
		swprintf(ext, (fname + sizeof(fname)/sizeof(fname[0])) - ext, L".%02d.avi", mSegmentIndex+1);

		// init the file

		pNewFile->init(fname);

		mpOutputFilePending = pNewFile;

		*const_cast<wchar_t *>(VDFileSplitPath(fname)) = 0;

//#pragma vdpragma_TODO("This drops Unicode characters not representable in ANSI")
		VDStringA fnameA(VDTextWToA(fname));

		int len = fnameA.size();
		if (len < MAX_PATH)
			len = MAX_PATH;

		mpOutputFile->setSegmentHintBlock(false, VDTextWToA(fname).c_str(), len+1);

		++mSegmentIndex;
		mSizeThresholdPending = pcsd->threshold;

	} catch(const MyError&) {
		delete pNewFile;
		throw;
	}
}

void VDCaptureData::FinalizeOldFile() {
	IVDMediaOutput *ao = mpOutput;

	mpOutputFile	= mpOutputFilePending;
	mpOutput		= mpOutputFile;
	ao->finalize();
	delete ao;
	mpszPath = mpszNewPath;
	mSizeThreshold = mSizeThresholdPending;
}

void VDCaptureData::ThreadRun() {
	MSG msg;
	bool fSwitch = false;
	DWORD dwTimer = GetTickCount();
	bool fTimerActive = true;

	for(;;) {
		bool fSuccess = false;

		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == VDCM_EXIT)
				return;
			else if (msg.message == VDCM_SWITCH_FIN) {
				fSwitch = true;
				if (!fTimerActive) {
					fTimerActive = true;
					dwTimer = GetTickCount();
				}
			}

			if (msg.message)
				fSuccess = true;
		}

		// We'd like to do this stuff while the system is idle, but it's
		// possible the system is so busy that it's never idle -- so force
		// processing to take place if the timeout expires.  Right now,
		// we set it to 10 seconds.

		if (!fSuccess || (fTimerActive && (GetTickCount()-dwTimer) > 1000) ) {

			// Kill timer.

			fTimerActive = false;

			// Time to initialize new output file?

			if (!mpSpillError) try {

				if (mpOutputFile && !mpOutputFilePending && !mbAllFull)
					CreateNewFile();

				// Finalize old output?

				if (fSwitch) {
					FinalizeOldFile();
					fSwitch = false;

					// Restart timer for new file to open.

					dwTimer = GetTickCount();
					fTimerActive = true;
				}
			} catch(const MyError& e) {
				delete mpSpillError.xchg(new MyError(e));
			}
		}

		if (!fSuccess) {
			if (fTimerActive)
				MsgWaitForMultipleObjects(0, NULL, FALSE, 1000, QS_ALLINPUT);
			else
				WaitMessage();
		}
	}

	VDDeinitThreadData();
}

////////////////

void VDCaptureData::DoSpill() {
	if (!mpProject->IsSpillEnabled()) return;

	sint64 nAudioFromVideo;
	sint64 nVideoFromAudio;

	if (mbAllFull)
		throw MyError("Capture stopped: All assigned spill drives are full.");

	// If there is no audio, then switch now.

	if (mpAudioOut) {

		// Find out if the audio or video stream is ahead, and choose a stop point.

		if (mbNTSC)
			nAudioFromVideo = int64divround(mVideoBlocks * 1001i64 * mwfex.nAvgBytesPerSec, mAudioSampleSize * 30000i64);
		else
			nAudioFromVideo = int64divround(mVideoBlocks * (__int64)mFramePeriod * mwfex.nAvgBytesPerSec, mAudioSampleSize * 10000000i64);

		if (nAudioFromVideo < mAudioBlocks) {

			// Audio is ahead of the corresponding video point.  Figure out how many frames ahead
			// we need to trigger from now.

			if (mbNTSC) {
				nVideoFromAudio = int64divroundup(mAudioBlocks * mAudioSampleSize * 30000i64, mwfex.nAvgBytesPerSec * 1001i64);
				nAudioFromVideo = int64divround(nVideoFromAudio * 1001i64 * mwfex.nAvgBytesPerSec, mAudioSampleSize * 30000i64);
			} else {
				nVideoFromAudio = int64divroundup(mAudioBlocks * mAudioSampleSize * 10000000i64, mwfex.nAvgBytesPerSec * (__int64)mFramePeriod);
				nAudioFromVideo = int64divround(nVideoFromAudio * (__int64)mFramePeriod * mwfex.nAvgBytesPerSec, mAudioSampleSize * 10000000i64);
			}

			mVideoSwitchPt = nVideoFromAudio;
			mAudioSwitchPt = nAudioFromVideo;

			VDDEBUG("SPILL: (%I64d,%I64d) > trigger at > (%I64d,%I64d)\n", mVideoBlocks, mAudioBlocks, mVideoSwitchPt, mAudioSwitchPt);

			return;

		} else if (nAudioFromVideo > mAudioBlocks) {

			// Audio is behind the corresponding video point, so switch the video stream now
			// and post a trigger for audio.

			mAudioSwitchPt = nAudioFromVideo;

			VDDEBUG("SPILL: video frozen at %I64d, audio(%I64d) trigger at (%I64d)\n", mVideoBlocks, mAudioBlocks, mAudioSwitchPt);

			mSegmentVideoSize = 0;
			mpVideoOut = mpOutputFilePending->getVideoOutput();
			++mVideoSegmentIndex;

			return;

		}
	}

	// Hey, they're exactly synched!  Well then, let's switch them now!

	VDDEBUG("SPILL: exact sync switch at %I64d, %I64d\n", mVideoBlocks, mAudioBlocks);

	IVDMediaOutput *pOutputPending = mpOutputFilePending;
	mpVideoOut = pOutputPending->getVideoOutput();
	mpAudioOut = pOutputPending->getAudioOutput();
	mSegmentAudioSize = mSegmentVideoSize = 0;
	++mVideoSegmentIndex;
	++mAudioSegmentIndex;

	PostFinalizeRequest();
}

void VDCaptureData::CheckVideoAfter() {
	++mVideoBlocks;
	
	if (mVideoSwitchPt && mVideoBlocks == mVideoSwitchPt) {

		mpVideoOut = mpOutputFilePending->getVideoOutput();
		++mVideoSegmentIndex;

		if (!mAudioSwitchPt) {
			PostFinalizeRequest();

			VDDEBUG("VIDEO: Triggering finalize & switch.\n");
		} else
			VDDEBUG("VIDEO: Switching stripes, waiting for audio to reach sync point (%I64d < %I64d)\n", mAudioBlocks, mAudioSwitchPt);

		mVideoSwitchPt = 0;
		mSegmentVideoSize = 0;
	}
}

bool VDCaptureData::VideoCallback(const void *data, uint32 size, sint64 timestamp64, bool key, sint64 global_clock) {
	VDCriticalSection::AutoLock lock(mCallbackLock);

	// Has the I/O thread successfully completed the switch?
	if (mpOutputFile == mpOutputFilePending)
		mpOutputFilePending = NULL;

	// Determine what frame we are *supposed* to be on.
	//
	// Let's say our capture interval is 500ms:
	//		Frame 0: 0-249ms
	//		Frame 1: 250-749ms
	//		Frame 2: 750-1249ms
	//		...and so on.
	//
	// We have to do this because AVICap doesn't keep track
	// of dropped frames in no-file capture mode.

	mTotalVideoSize += mLastVideoSize;
	mLastVideoSize = 0;

	////////////////////////////

	if (timestamp64 < 0) {
		VDDEBUG("Capture: Fixing negative timestamp %I64d!\n", timestamp64);

		timestamp64 = 0;
	}

	uint32 microFrame;		// 1/64th frames
	uint32 dwCurrentFrame;
	long jitter;
	if (mbNTSC) {
		microFrame = (uint32)((timestamp64 * 3 * 64) / 100100);
		dwCurrentFrame = (microFrame + 32) >> 6;
		jitter = (long)(timestamp64 - (sint64)((sint64)dwCurrentFrame * 100100 / 3));
	} else {
		microFrame = (uint32)((timestamp64 * 640) / mFramePeriod);
		dwCurrentFrame = (microFrame + 32) >> 6;
		jitter = (long)(timestamp64 - ((sint64)dwCurrentFrame * mFramePeriod + 5) / 10);
	}

	mTotalDisp += abs(jitter);
	mTotalJitter += jitter;

	if (g_pCaptureProfiler)
		g_pCaptureProfiler->AddDataPoint(mStatsChannelVTJitter, (float)jitter / (float)mFramePeriod * 0.2f);

	++mTotalFramesCaptured;

	mLastTime = (uint32)(global_clock / 1000);

	// Run the frame through the filterer.

	uint32 dwBytesUsed = size;
	void *pFilteredData = (void *)data;

	VDPixmap px(VDPixmapFromLayout(mInputLayout, pFilteredData));

	// We don't need to lock here as it is illegal to change the filter
	// mode while capture is running.
	bool firstFrame = true;

	for(;;) {
		bool filterSystemActive = false;
		bool frameProduced = firstFrame;

		vdsynchronized(mpProject->mVideoFilterLock) {
			if (mpFilterSys) {
				VDPROFILEBEGIN("V-Filter");

				if (firstFrame)
					mpFilterSys->ProcessIn(px);

				frameProduced = mpFilterSys->ProcessOut(px, pFilteredData, dwBytesUsed);

				VDPROFILEEND();
				filterSystemActive = true;
				key = true;
			}
		}

		if (!frameProduced)
			break;

		if (firstFrame) {
			firstFrame = false;

			mpProject->DispatchAnalysis(px);
		}

		if (mpOutputBlitter) {
			VDPROFILEBEGIN("V-BlitOut");
			mpOutputBlitter->Blit(repack_buffer, px);
			VDPROFILEEND();

			VideoCallbackWriteFrame(repack_buffer.base(), repack_buffer.size(), key);
		} else {
			VideoCallbackWriteFrame(pFilteredData, dwBytesUsed, key);
		}

		if (!filterSystemActive)
			break;
	}

	if (global_clock - mLastUpdateTime > 500000)
	{
		if (mpOutputFilePending && !mAudioSwitchPt && !mVideoSwitchPt && mpProject->IsSpillEnabled()) {
			if (mSegmentVideoSize + mSegmentAudioSize >= ((__int64)g_lSpillMaxSize<<20)
				|| VDGetDiskFreeSpace(mpszPath) < ((__int64)mSizeThreshold << 20))

				DoSpill();
		}

		sint64 i64;
		if (mpProject->IsSpillEnabled())
			i64 = CapSpillGetFreeSpace();
		else {
			if (!mCaptureRoot.empty())
				i64 = VDGetDiskFreeSpace(mCaptureRoot.c_str());
			else
				i64 = VDGetDiskFreeSpace(L".");
		}

		mDiskFreeBytes = i64;

		VDCaptureStatus status;

		status.mFramesCaptured	= mTotalFramesCaptured;
		status.mFramesDropped	= mFramesDropped;
		status.mFramesInserted	= mFramesInserted;
		status.mTotalJitter		= mTotalJitter;
		status.mTotalDisp		= mTotalDisp;
		status.mTotalVideoSize	= mTotalVideoSize;
		status.mTotalAudioSize	= mTotalAudioSize;

		status.mCurrentVideoSegment	= mVideoSegmentIndex;
		status.mCurrentAudioSegment	= mAudioSegmentIndex;

		status.mElapsedTimeMS	= (uint32)(global_clock / 1000);
		status.mDiskFreeSpace	= mDiskFreeBytes;

		status.mVideoFirstFrameTimeMS	= 0;
		status.mVideoLastFrameTimeMS	= 0;
		status.mAudioFirstFrameTimeMS	= 0;
		status.mAudioLastFrameTimeMS	= 0;
		status.mAudioFirstSize			= 0;
		status.mTotalAudioDataSize		= 0;
		status.mActualAudioHz			= 0;
		if (mpStatsFilter)
			mpStatsFilter->GetStats(status);

		status.mVideoResamplingRate	= 1.0f;
		status.mVideoTimingAdjustMS = 0;
		status.mAudioResamplingRate	= 0;
		status.mAudioRelativeLatency= 0;
		status.mAudioCurrentLatency	= 0;
		if (mpResyncFilter) {
			VDCaptureResyncStatus rstat;

			mpResyncFilter->GetStatus(rstat);

			status.mVideoTimingAdjustMS = rstat.mVideoTimingAdjust;
			status.mVideoResamplingRate	= rstat.mVideoResamplingRate;
			status.mAudioResamplingRate = rstat.mAudioResamplingRate;
			status.mAudioCurrentLatency	= rstat.mCurrentLatency;
			status.mAudioRelativeLatency= rstat.mMeasuredLatency;
		}

		if (mpProject->mpCB)
			mpProject->mpCB->UICaptureStatusUpdated(status);

		mLastUpdateTime = global_clock - global_clock % 500000;
		mTotalJitter = mTotalDisp = 0;
	};

	return true;
}

void VDCaptureData::createOutputBlitter() {
	if (driverLayout.format) {
		VDPixmap pxsrc(VDPixmapFromLayout(mOutputLayout, 0));
		FilterModPixmapInfo out_info;
		out_info.ref_r = 0xFFFF;
		out_info.ref_g = 0xFFFF;
		out_info.ref_b = 0xFFFF;
		out_info.ref_a = 0xFFFF;
		int format = 0;
		if (mpVideoCompressor) {
			format = mpVideoCompressor->GetInputFormat(&out_info);
		}
		if (!format) {
			throw MyError("bad path");
		}

		IVDPixmapExtraGen* extraDst = 0;
		switch (format) {
		case nsVDPixmap::kPixFormat_XRGB8888:
			{
				ExtraGen_X8R8G8B8_Normalize* normalize = new ExtraGen_X8R8G8B8_Normalize;
				extraDst = normalize;
			}
			break;
		case nsVDPixmap::kPixFormat_XRGB64:
			{
				ExtraGen_X16R16G16B16_Normalize* normalize = new ExtraGen_X16R16G16B16_Normalize;
				normalize->max_value = out_info.ref_r;
				extraDst = normalize;
			}
			break;
		case nsVDPixmap::kPixFormat_YUV420_Planar16:
		case nsVDPixmap::kPixFormat_YUV422_Planar16:
		case nsVDPixmap::kPixFormat_YUV444_Planar16:
			{
				ExtraGen_YUV_Normalize* normalize = new ExtraGen_YUV_Normalize;
				normalize->max_value = out_info.ref_r;
				extraDst = normalize;
			}
			break;
		}
		repack_buffer.init(driverLayout);
		VDPixmapFormatEx fmt = VDPixmapFormatCombine(driverLayout.format,VDPixmapFormatNormalize(pxsrc.format));
		repack_buffer.format = fmt.format;
		repack_buffer.info.colorSpaceMode = fmt.colorSpaceMode;
		repack_buffer.info.colorRangeMode = fmt.colorRangeMode;
		mpOutputBlitter = VDPixmapCreateBlitter(repack_buffer, pxsrc, extraDst);
		delete extraDst;
	} else if (vfwLayout.format) {
		VDPixmap pxsrc(VDPixmapFromLayout(mOutputLayout, 0));
		repack_buffer.init(vfwLayout);
		VDPixmapFormatEx fmt = VDPixmapFormatCombine(driverLayout.format,VDPixmapFormatNormalize(pxsrc.format));
		repack_buffer.format = fmt.format;
		repack_buffer.info.colorSpaceMode = fmt.colorSpaceMode;
		repack_buffer.info.colorRangeMode = fmt.colorRangeMode;
		mpOutputBlitter = VDPixmapCreateBlitter(repack_buffer, pxsrc);
	}
}

void VDCaptureData::VideoCallbackWriteFrame(void *pFilteredData, uint32 dwBytesUsed, bool key) {
	// While we are early, write padding frames (grr)
	//
	// Don't do this for the first frame, since we don't
	// have any frames preceding it!

	if (mpVideoCompressor) {
		uint32 lBytes = 0;
		VDPacketInfo packetInfo;

		VDPROFILEBEGIN("V-Compress");
		void* lpCompressedData = pOutputBuffer;
		if (!mpVideoCompressor->packFrame(lpCompressedData, pFilteredData, lBytes, packetInfo))
			lpCompressedData = 0;
		VDPROFILEEND();

		if (mpOutput) {
			VDPROFILEBEGIN("V-Write");
			mpVideoOut->write(
					packetInfo.keyframe ? AVIOutputStream::kFlagKeyFrame : 0,
					lpCompressedData,
					lBytes, 1);
			VDPROFILEEND();

			CheckVideoAfter();
		}

		mLastVideoSize = lBytes + 24;
	} else {
		if (mpOutput) {
			VDPROFILEBEGIN("V-Write");
			mpVideoOut->write(key ? AVIOutputStream::kFlagKeyFrame : 0, pFilteredData, dwBytesUsed, 1);
			VDPROFILEEND();
			CheckVideoAfter();
		}

		mLastVideoSize = dwBytesUsed + 24;
	}

	mSegmentVideoSize += mLastVideoSize;
}

bool VDCaptureData::WaveCallback(const void *data, uint32 size, sint64 global_clock) {
	VDCriticalSection::AutoLock lock(mCallbackLock);

	// Has the I/O thread successfully completed the switch?

	if (mpOutputFile == mpOutputFilePending)
		mpOutputFilePending = NULL;

	if (mpOutput) {
		if (mpProject->IsSpillEnabled()) {
			const char *pSrc = (const char *)data;
			long left = (long)size;

			// If there is a switch point, write up to it.  Otherwise, write it all!

			while(left > 0) {
				long tc;

				tc = left;

				if (mAudioSwitchPt && mAudioBlocks+tc/mAudioSampleSize >= mAudioSwitchPt)
					tc = (long)((mAudioSwitchPt - mAudioBlocks) * mAudioSampleSize);

				mpAudioOut->write(0, pSrc, tc, tc / mAudioSampleSize);
				mTotalAudioSize += tc + 24;
				mSegmentAudioSize += tc + 24;
				mAudioBlocks += tc / mAudioSampleSize;

				if (mAudioSwitchPt && mAudioBlocks == mAudioSwitchPt) {
					// Switch audio to next stripe.

					mpAudioOut = mpOutputFilePending->getAudioOutput();

					if (!mVideoSwitchPt) {
						PostFinalizeRequest();
						VDDEBUG("AUDIO: Triggering finalize & switch.\n");
					} else
						VDDEBUG("AUDIO: Switching to next, waiting for video to reach sync point (%I64d < %I64d)\n", mVideoBlocks, mVideoSwitchPt);

					mAudioSwitchPt = 0;
					mSegmentAudioSize = 0;
					++mAudioSegmentIndex;
				}

				left -= tc;
				pSrc += tc;
			}
		} else {
			VDPROFILEBEGIN("A-Write");
			mpAudioOut->write(0, data, size, size / mAudioSampleSize);
			VDPROFILEEND();
			mTotalAudioSize += size + 24;
			mSegmentAudioSize += size + 24;
		}
	} else {
		mTotalAudioSize += size + 24;
		mSegmentAudioSize += size + 24;
	}

	return true;
}

void VDCaptureData::OnFramesDropped(int framesDropped) {
	VDCriticalSection::AutoLock lock(mCallbackLock);

	mFramesDropped += framesDropped;
}

void VDCaptureData::OnFramesInserted(int framesInserted) {
	VDCriticalSection::AutoLock lock(mCallbackLock);

	mFramesInserted += framesInserted;
	mTotalVideoSize += 24 * framesInserted;
	mSegmentVideoSize += 24 * framesInserted;

	while(framesInserted-->0) {
		if (mpOutputFile)
			mpVideoOut->write(0, "", 0, 1);

		if (mpVideoCompressor)
			mpVideoCompressor->dropFrame();

		if (mpOutputFile)
			CheckVideoAfter();
	}
}
