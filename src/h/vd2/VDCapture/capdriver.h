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

#ifndef f_VD2_RIZA_CAPDRIVER_H
#define f_VD2_RIZA_CAPDRIVER_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/vectors.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/unknown.h>
#include <list>

#include <windows.h>			// hmm, need to get rid of this....
#include <mmsystem.h>

class MyError;
class VDRegistryAppKey;
struct VDAudioMaskParam;

namespace nsVDCapture {
	enum DisplayMode {
		kDisplayNone,
		kDisplayHardware,
		kDisplaySoftware,
		kDisplayAnalyze,
		kDisplayModeCount
	};

	enum TunerInputMode {
		kTunerInputUnknown,
		kTunerInputAntenna,
		kTunerInputCable,
		kTunerInputModeCount
	};

	enum DriverDialog {
		kDialogVideoFormat,
		kDialogVideoSource,
		kDialogVideoDisplay,
		kDialogVideoCapturePin,
		kDialogVideoPreviewPin,
		kDialogVideoCaptureFilter,
		kDialogAudioCaptureFilter,
		kDialogVideoCrossbar,
		kDialogVideoCrossbar2,
		kDialogTVTuner,
		kDialogCount
	};

	enum DriverEvent {
		kEventNone,
		kEventPreroll,
		kEventCapturing,
		kEventVideoFormatChanged,
		kEventVideoFrameRateChanged,
		kEventVideoFramesDropped,
		kEventVideoFramesInserted,
		kEventVideoSourceChanged,
		kEventAudioSourceChanged,
		kEventCount
	};

	enum VDCapturePropertyId {
		kPropBrightness,
		kPropContrast,
		kPropHue,
		kPropSaturation,
		kPropSharpness,
		kPropGamma,
		kPropColorEnable,
		kPropWhiteBalance,
		kPropBacklightCompensation,
		kPropGain,
		kPropCount
	};
};

struct VDAudioMaskParam {
	int mask;
	int mix[16];

	VDAudioMaskParam() {
		mask = -1;
		for(int i=0; i<16; i++) mix[i]=0;
		mix[0] = 1;
		mix[1] = 2;
	}

	bool operator==(const VDAudioMaskParam& a) const { return memcmp(this,&a,sizeof(a))==0; }
};

/////////////////////////////////////////////////////////////////////////////
/// \class IVDCaptureDriverCallback
///
/// Callback class for event data from a capture device. This is called with
/// one-time event information (start, stop, frame rate change, drop/insert,
/// etc.) as well as frame and wave data. This may be called from an
/// alternate thread and thus must be thread-safe.
class VDINTERFACE IVDCaptureDriverCallback {
public:
	virtual void CapBegin(sint64 global_clock) = 0;
	virtual void CapEnd(const MyError *pError) = 0;
	virtual bool CapEvent(nsVDCapture::DriverEvent event, int data) = 0;

	virtual void CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock) = 0;
	///< Delivers captured audio and video data. Both a video and audio block
	///< may be delivered concurrently, so this function must be both
	///< reentrant and thread-safe.
	///<
	///< \param stream			0 for video, 1 for audio, -1 for preview video.
	///< \param data			Pointer to captured data.
	///< \param size			Linear byte size of captured data.
	///< \param timestamp		Stream timestamp of sample start in microseconds.
	///< \param key				True if the video frame is a key frame.
	///< \param global_clock	System time of sample start in microseconds.
};


/////////////////////////////////////////////////////////////////////////////
class VDINTERFACE IVDCaptureProfiler {
public:
	virtual void Clear() = 0;
	virtual int RegisterStatsChannel(const char *name) = 0;
	virtual void AddDataPoint(int channel, float value) = 0;
};

/////////////////////////////////////////////////////////////////////////////
class VDINTERFACE IVDCaptureDriver : public IVDUnknown {
public:
	virtual ~IVDCaptureDriver() {}

	virtual bool	Init(VDGUIHandle hParent) = 0;
	virtual void  LoadVideoConfig(VDRegistryAppKey& key){}
	virtual void  SaveVideoConfig(VDRegistryAppKey& key){}
	virtual void  LoadAudioConfig(VDRegistryAppKey& key){}
	virtual void  SaveAudioConfig(VDRegistryAppKey& key){}

	virtual void	SetCallback(IVDCaptureDriverCallback *pCB) = 0;

	virtual void	LockUpdates() = 0;
	virtual void	UnlockUpdates() = 0;

	virtual bool	IsHardwareDisplayAvailable() = 0;

	virtual void	SetDisplayMode(nsVDCapture::DisplayMode m) = 0;
	virtual nsVDCapture::DisplayMode		GetDisplayMode() = 0;

	virtual void	SetDisplayRect(const vdrect32& r) = 0;
	virtual vdrect32	GetDisplayRectAbsolute() = 0;
	virtual void	SetDisplayVisibility(bool vis) = 0;

	virtual void	SetFramePeriod(sint32 framePeriod100nsUnits) = 0;
	virtual sint32	GetFramePeriod() = 0;

	virtual uint32	GetPreviewFrameCount() = 0;

	virtual bool	GetVideoFormat(vdstructex<BITMAPINFOHEADER>& vformat) = 0;
	virtual bool	SetVideoFormat(const BITMAPINFOHEADER *pbih, uint32 size) = 0;

	virtual bool	SetTunerChannel(int channel) = 0;
	virtual int		GetTunerChannel() = 0;
	virtual bool	GetTunerChannelRange(int& minChannel, int& maxChannel) = 0;
	virtual uint32	GetTunerFrequencyPrecision() = 0;
	virtual uint32	GetTunerExactFrequency() = 0;
	virtual bool	SetTunerExactFrequency(uint32 freq) = 0;
	virtual nsVDCapture::TunerInputMode	GetTunerInputMode() = 0;
	virtual void	SetTunerInputMode(nsVDCapture::TunerInputMode) = 0;

	virtual int		GetAudioDeviceCount() = 0;
	virtual const wchar_t *GetAudioDeviceName(int idx) = 0;
	virtual bool	SetAudioDevice(int idx) = 0;
	virtual int		GetAudioDeviceIndex() = 0;
	virtual bool	IsAudioDeviceIntegrated(int idx) = 0;

	virtual int		GetVideoSourceCount() = 0;
	virtual const wchar_t *GetVideoSourceName(int idx) = 0;
	virtual bool	SetVideoSource(int idx) = 0;
	virtual int		GetVideoSourceIndex() = 0;

	virtual int		GetAudioSourceCount() = 0;
	virtual const wchar_t *GetAudioSourceName(int idx) = 0;
	virtual bool	SetAudioSource(int idx) = 0;
	virtual int		GetAudioSourceIndex() = 0;

	virtual int		GetAudioSourceForVideoSource(int idx) = 0;

	virtual int		GetAudioInputCount() = 0;
	virtual const wchar_t *GetAudioInputName(int idx) = 0;
	virtual bool	SetAudioInput(int idx) = 0;
	virtual int		GetAudioInputIndex() = 0;

	virtual	bool	IsAudioCapturePossible() = 0;
	virtual bool	IsAudioCaptureEnabled() = 0;
	virtual bool	IsAudioPlaybackPossible() = 0;
	virtual bool	IsAudioPlaybackEnabled() = 0;
	virtual void	SetAudioCaptureEnabled(bool b) = 0;
	virtual void	SetAudioAnalysisEnabled(bool b) = 0;
	virtual void	SetAudioPlaybackEnabled(bool b) = 0;
	virtual void	SetAudioMask(VDAudioMaskParam& param){}

	virtual void	GetAvailableAudioFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats) = 0;

	virtual bool	GetAudioFormat(vdstructex<WAVEFORMATEX>& aformat) = 0;
	virtual bool	SetAudioFormat(const WAVEFORMATEX *pwfex, uint32 size) = 0;

	virtual bool	IsDriverDialogSupported(nsVDCapture::DriverDialog dlg) = 0;
	virtual void	DisplayDriverDialog(nsVDCapture::DriverDialog dlg) = 0;

	virtual bool	IsPropertySupported(uint32 id) = 0;
	virtual sint32	GetPropertyInt(uint32 id, bool *pAutomatic) = 0;
	virtual void	SetPropertyInt(uint32 id, sint32 value, bool automatic) = 0;
	virtual void	GetPropertyInfoInt(uint32 id, sint32& minVal, sint32& maxVal, sint32& step, sint32& defaultVal, bool& automatic, bool& manual) = 0;

	virtual bool	CaptureStart() = 0;
	virtual void	CaptureStop() = 0;
	virtual void	CaptureAbort() = 0;
};

class VDINTERFACE IVDCaptureSystem {
public:
	virtual ~IVDCaptureSystem() {}

	virtual void			EnumerateDrivers() = 0;

	virtual int				GetDeviceCount() = 0;
	virtual const wchar_t	*GetDeviceName(int index) = 0;

	virtual IVDCaptureDriver	*CreateDriver(int deviceIndex) = 0;
};

#endif
