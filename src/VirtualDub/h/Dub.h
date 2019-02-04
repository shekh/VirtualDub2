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

#ifndef f_DUB_H
#define f_DUB_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/error.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/VDString.h>
#include <vd2/system/fraction.h>
#include <vd2/system/event.h>
#include <vd2/system/vdstl.h>
#include <vd2/kasumi/pixmap.h>
#include <vd2/Riza/videocodec.h>
#include "audio.h"
#include "fixes.h"

class AsyncBlitter;
class AVIPipe;
class AVIOutput;
class DubSource;
class AudioSource;
class IVDVideoSource;
class IVDVideoDisplay;
class FrameSubset;
class InputFile;
class IDubStatusHandler;
class IVDDubberOutputSystem;
struct VDAudioFilterGraph;
class FilterSystem;
class VDTextOutputStream;
struct VDAVIBitmapInfoHeader;

////////////////////////

class DubAudioOptions {
public:
	enum {
		P_NOCHANGE=0,
		P_8BIT=1,
		P_16BIT=2,
		C_NOCHANGE=0,
		C_MONO=1,
		C_STEREO=2,
		C_MONOLEFT=3,
		C_MONORIGHT=4,
		M_NONE			= 0,
		M_FULL			= 1,
	};

	float	mVolume;		// 0 disables.

	long preload;
	long interval;
	long new_rate;
	long offset;
	bool is_ms;
	bool enabled;
	bool fStartAudio, fEndAudio;
	bool mbApplyVideoTimeline;
	bool fHighQuality;
	bool bUseAudioFilterGraph;
	char newPrecision;
	char newChannels;
	char mode;
};

class DubVideoPosition {
public:
	VDPosition	mOffset;		// in frames from start; -1 means end

	VDPosition ResolveToFrames(VDPosition frameCount) const;
	VDPosition ResolveToMS(VDPosition frameCount, const VDFraction& resultTimeBase, bool fromEnd) const;
};

class DubVideoOptions {
public:
	enum {
		M_NONE		= 0,
		M_FASTREPACK= 1,
		M_SLOWREPACK= 2,
		M_FULL		= 3,
	};
	enum {
		// (0,0) = no change
		// (1,0) = adjust frame rate so audio and video streams match
		kFrameRateAdjustSameLength = 1
	};

	enum PreviewFieldMode {
		kPreviewFieldsProgressive,
		kPreviewFieldsWeaveTFF,
		kPreviewFieldsWeaveBFF,
		kPreviewFieldsBobTFF,
		kPreviewFieldsBobBFF,
		kPreviewFieldsNonIntTFF,
		kPreviewFieldsNonIntBFF
	};
	
	VDPixmapFormatEx		mInputFormat;
	VDPixmapFormatEx		mOutputFormat;
	int		outputReference;
	char	mode;
	bool	mbUseSmartRendering;
	bool	mbPreserveEmptyFrames;
	int		mMaxVideoCompressionThreads;
	bool	fShowInputFrame, fShowOutputFrame, fShowDecompressedFrame;
	bool	fSyncToAudio;
	int		frameRateDecimation;
	uint32	frameRateTargetHi, frameRateTargetLo;

	uint32	mFrameRateAdjustHi;
	uint32	mFrameRateAdjustLo;

	DubVideoPosition mSelectionStart;
	DubVideoPosition mSelectionEnd;

	PreviewFieldMode	previewFieldMode;
};

class DubPerfOptions {
public:
	bool	dynamicEnable;
	bool	dynamicShowDisassembly;
	bool	useDirectDraw;
	bool	fDropFrames;
	bool	fBenchmark;
};

class DubOptions {
public:
	DubAudioOptions audio;
	DubVideoOptions video;
	DubPerfOptions perf;

	bool	fShowStatus;
	bool	mbForceShowStatus;
	bool	fMoveSlider;
	bool	removeAudio;

	uint32	mThrottlePercent;
};

class DubStreamInfo {
public:
	sint64	total_size;
	sint64	offset_num;
	sint64	offset_den;

	DubStreamInfo() {
		total_size = 0;
		offset_num = 0;
		offset_den = 1;
	}
};

class DubAudioStreamInfo : public DubStreamInfo {
public:
	sint64	start_src;

	bool	converting, resampling;
	bool	is_16bit;
	bool	is_stereo;
	bool	is_right;
	bool	single_channel;

	long	lPreloadSamples;
	sint64	start_us;
};

class DubVideoStreamInfo : public DubStreamInfo {
public:
	sint64	start_src;			// start of timeline to process
	sint64	end_src;			// end of timeline to process
	sint64	cur_proc_src;		// last timeline frame processed
	sint64	cur_proc_dst;		// total number of frames written
	sint64	end_proc_dst;		// total number of frames to write
	sint64	cur_dst;			// current render map index for fetch
	sint64	end_dst;			// total number of timeline frames to fetch

	// Frame rate cascade:
	//
	//	video source
	//	frame rate adjust		=> frameRateIn
	//	IVTC					=> frameRatePreFilter
	//	filters					=> frameRatePostFilter, frameRateTimeline
	//	conversion				=> frameRate

	VDFraction	mFrameRateIn;
	VDFraction	mFrameRatePreFilter;
	VDFraction	mFrameRatePostFilter;
	VDFraction	mFrameRateTimeline;
	VDFraction	mFrameRate;
	VDFraction	mFrameRateIVTCFactor;
	sint64		mTimelineSourceLength;
	long	processed;
	uint32	lastProcessedTimestamp;
	bool	fAudioOnly;
};

struct VDDubPerfStatus {
	uint32	mVideoBuffersActive;
	uint32	mVideoBuffersTotal;
	uint32	mVideoRequestsActive;
	uint32	mAudioBufferInUse;
	uint32	mAudioBufferTotal;
	float	mIOActivityRatio;
	float	mProcActivityRatio;
};

class IDubber {
public:
	virtual ~IDubber()					=0;

	virtual void SetAudioCompression(const VDWaveFormat *wf, uint32 cb, const char *pShortNameHint, vdblock<char>& config) = 0;
	virtual void SetInputDisplay(IVDVideoDisplay *pDisplay) = 0;
	virtual void SetOutputDisplay(IVDVideoDisplay *pDisplay) = 0;
	virtual void SetAudioFilterGraph(const VDAudioFilterGraph& graph)=0;
	virtual void Init(IVDVideoSource *const *pVideoSources, uint32 nVideoSources, AudioSource *const *pAudioSources, uint32 nAudioSources, IVDDubberOutputSystem *out, void *videoCompVars, const FrameSubset *pfs, const VDFraction& frameRateTimeline) = 0;
	virtual void InitAudio(AudioSource *const *pAudioSources, uint32 nAudioSources) = 0;
	virtual AudioCompressor* GetAudioCompressor() = 0;
	virtual AudioStream* GetAudioBeforeCompressor() = 0;
	virtual void CheckAudioCodec(const char* format) = 0;

	virtual void Go(int iPriority = 0) = 0;
	virtual void Stop() = 0;

	virtual void Abort(bool userAbort = true)	=0;
	virtual bool isRunning()		=0;
	virtual bool isAbortedByUser()	=0;
	virtual bool IsAborted()		=0;
	virtual bool IsPreviewing()		=0;
	virtual bool IsBackground()		=0;

	virtual void SetStatusHandler(IDubStatusHandler *pdsh)		=0;
	virtual void SetPriority(int index)=0;
	virtual void SetBackground(bool background) = 0;
	virtual void UpdateFrames()=0;

	virtual void SetThrottleFactor(float throttleFactor) = 0;

	virtual void GetPerfStatus(VDDubPerfStatus& status) = 0;

	virtual void DumpStatus(VDTextOutputStream& os) = 0;

	virtual VDEvent<IDubber, bool>& Stopped() = 0;
};

class IDubberInternal {
public:
	virtual void InternalSignalStop()	= 0;
};

IDubber *CreateDubber(DubOptions *xopt);
void VDConvertSelectionTimesToFrames(const DubOptions& opt, const FrameSubset& subset, const VDFraction& subsetRate, VDPosition& startFrame, VDPosition& endFrame);
void InitVideoStreamValuesStatic(DubVideoStreamInfo& vInfo, IVDVideoSource *video, AudioSource *audio, const DubOptions *opt, const FrameSubset *pfs, const VDPosition *pSelectionStartFrame, const VDPosition *pSelectionEndFrame);
void InitVideoStreamValuesStatic2(DubVideoStreamInfo& vInfo, const DubOptions *opt, const FilterSystem *filtsys, const VDFraction& frameRateTimeline);
void InitAudioStreamValuesStatic(DubAudioStreamInfo& aInfo, AudioSource *audio, const DubOptions *opt);

struct MakeOutputFormat {
	VDPixmapFormatEx option;
	VDPixmapFormatEx dec;
	VDPixmapFormatEx flt;
	VDPixmapFormatEx out;
	VDPixmapFormatEx comp;
	IVDDubberOutputSystem* os;
	IVDVideoCompressor* vc;
	DWORD vc_fccHandler;
	VDStringA os_format;
	vdstructex<VDAVIBitmapInfoHeader> srcDib;
	vdstructex<VDAVIBitmapInfoHeader> compDib;
	int compVariant;

	int mode;
	int reference;
	int w,h;
	bool use_os_format;
	bool use_vc_format;
	bool own_vc;
	VDStringA error;

	MakeOutputFormat() {
		os = 0;
		vc = 0;
		vc_fccHandler = 0;
		own_vc = false;
		mode = DubVideoOptions::M_FULL;
		reference = 1;
		w = 0; h = 0;
		use_os_format = false;
		use_vc_format = false;
		compVariant = 0;
	}
	~MakeOutputFormat() {
		if (own_vc) delete vc;
	}

	void init(DubOptions& opts, IVDVideoSource* vs);
	void initCapture(BITMAPINFOHEADER* bm);
	void initComp(IVDDubberOutputSystem* os, IVDVideoCompressor* vc);
	void initComp(COMPVARS2* compvars);
	void initGlobal();
	void combine();
	void combineComp();
	void combineComp_repack();
	bool accept_format(int format, int variant);
};

#ifndef f_DUB_CPP

extern DubOptions g_dubOpts;
#endif

#endif
