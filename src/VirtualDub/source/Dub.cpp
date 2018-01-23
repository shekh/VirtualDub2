//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#define f_DUB_CPP


#include <process.h>
#include <time.h>
#include <vector>
#include <deque>
#include <utility>

#include <windows.h>
#include <vfw.h>

#include "resource.h"

#include "crash.h"
#include <vd2/system/thread.h>
#include <vd2/system/tls.h>
#include <vd2/system/time.h>
#include <vd2/system/atomic.h>
#include <vd2/system/fraction.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/VDRingBuffer.h>
#include <vd2/system/profile.h>
#include <vd2/system/protscope.h>
#include <vd2/system/w32assist.h>
#include <vd2/Dita/resources.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Riza/bitmap.h>
#include <vd2/VDDisplay/display.h>
#include <vd2/Riza/videocodec.h>
#include <vd2/plugin/vdinputdriver.h>
#include "AudioFilterSystem.h"
#include "convert.h"
#include "filters.h"
#include "gui.h"
#include "prefs.h"
#include "command.h"
#include "misc.h"
#include "timeline.h"

#include <vd2/system/error.h>
#include "AsyncBlitter.h"
#include "AVIOutputPreview.h"
#include "AVIOutput.h"
#include "AVIOutputWAV.h"
#include "AVIOutputImages.h"
#include "AVIOutputStriped.h"
#include "AudioSource.h"
#include "VideoSource.h"
#include "AVIPipe.h"
#include "VBitmap.h"
#include "FrameSubset.h"
#include "InputFile.h"
#include "FilterFrameVideoSource.h"

#include "Dub.h"
#include "DubOutput.h"
#include "DubStatus.h"
#include "DubUtils.h"
#include "DubIO.h"
#include "DubProcess.h"

using namespace nsVDDub;

#ifndef PROCESS_MODE_BACKGROUND_BEGIN
#define PROCESS_MODE_BACKGROUND_BEGIN 0x00100000
#endif

#ifndef PROCESS_MODE_BACKGROUND_END
#define PROCESS_MODE_BACKGROUND_END 0x00200000 
#endif

/// HACK!!!!
#define vSrc mVideoSources[0]
#define aSrc mAudioSources[0]


///////////////////////////////////////////////////////////////////////////

extern const char g_szError[];
extern HWND g_hWnd;
extern uint32& VDPreferencesGetRenderVideoBufferCount();
extern bool VDPreferencesGetFilterAccelVisualDebugEnabled();
extern bool VDPreferencesGetFilterAccelEnabled();
extern sint32 VDPreferencesGetFilterThreadCount();

///////////////////////////////////////////////////////////////////////////

namespace {
	enum { kVDST_Dub = 1 };

	enum {
		kVDM_SegmentOverflowOccurred,
		kVDM_BeginningNextSegment,
		kVDM_IOThreadLivelock,
		kVDM_ProcessingThreadLivelock,
		kVDM_CodecDelayedDuringDelayedFlush,
		kVDM_CodecLoopingDuringDelayedFlush,
		kVDM_FastRecompressUsingFormat,
		kVDM_SlowRecompressUsingFormat,
		kVDM_FullUsingInputFormat,
		kVDM_FullUsingOutputFormat
	};

	enum {
		kLiveLockMessageLimit = 5
	};
};

///////////////////////////////////////////////////////////////////////////

DubOptions g_dubOpts = {
	{
		-1.0f,			// no amp
		500,			// preload by 500ms
		1,				// every frame
		0,				// no new rate
		0,				// offset: 0ms
		false,			// period is in frames
		true,			// audio interleaving enabled
		true,			// yes, offset audio with video
		true,			// yes, clip audio to video length
		true,			// yes, use video timeline
		false,			// no high quality
		false,			// use fixed-function audio pipeline
		DubAudioOptions::P_NOCHANGE,		// no precision change
		DubAudioOptions::C_NOCHANGE,		// no channel change
		DubAudioOptions::M_NONE,
	},

	{
		0,								// input: autodetect
		nsVDPixmap::kPixFormat_RGB888,	// output: 24bit
		DubVideoOptions::M_FULL,	// mode: full
		false,						// use smart encoding
		false,						// preserve empty frames
		0,							// max video compression threads
		true,						// show input video
		true,						// show output video
		false,						// decompress output video before display
		true,						// sync to audio
		1,							// no frame rate decimation
		0,0,						// no target
		0,0,						// no change in frame rate
		0,							// start offset: 0ms
		-1,							// end offset: 0ms
		DubVideoOptions::kPreviewFieldsProgressive,	// progressive preview
	},

	{
		true,					// dynamic enable
		false,
		false,					// directdraw,
		true,					// drop frames
		false,					// not benchmark
	},

	true,			// show status
	false,			// force show status
	false,			// move slider
	false,			// remove audio
	100,			// run at 100%
};

static const int g_iPriorities[][2]={

	// I/O							processor
	{ THREAD_PRIORITY_IDLE,			THREAD_PRIORITY_IDLE,			},
	{ THREAD_PRIORITY_LOWEST,		THREAD_PRIORITY_LOWEST,			},
	{ THREAD_PRIORITY_BELOW_NORMAL,	THREAD_PRIORITY_LOWEST,			},
	{ THREAD_PRIORITY_NORMAL,		THREAD_PRIORITY_BELOW_NORMAL,	},
	{ THREAD_PRIORITY_NORMAL,		THREAD_PRIORITY_NORMAL,			},
	{ THREAD_PRIORITY_ABOVE_NORMAL,	THREAD_PRIORITY_NORMAL,			},
	{ THREAD_PRIORITY_HIGHEST,		THREAD_PRIORITY_ABOVE_NORMAL,	},
	{ THREAD_PRIORITY_HIGHEST,		THREAD_PRIORITY_HIGHEST,		}
};

/////////////////////////////////////////////////
void AVISTREAMINFOtoAVIStreamHeader(VDXAVIStreamHeader *dest, const VDAVIStreamInfo *src) {
	dest->fccType			= src->fccType;
	dest->fccHandler		= src->fccHandler;
	dest->dwFlags			= 0;
	dest->wPriority			= src->wPriority;
	dest->wLanguage			= src->wLanguage;
	dest->dwInitialFrames	= 0;
	dest->dwStart			= src->dwStart;
	dest->dwScale			= src->dwScale;
	dest->dwRate			= src->dwRate;
	dest->dwLength			= src->dwLength;
	dest->dwSuggestedBufferSize = src->dwSuggestedBufferSize;
	dest->dwQuality			= src->dwQuality;
	dest->dwSampleSize		= src->dwSampleSize;
	dest->rcFrame.left		= (short)src->rcFrameLeft;
	dest->rcFrame.top		= (short)src->rcFrameTop;
	dest->rcFrame.right		= (short)src->rcFrameRight;
	dest->rcFrame.bottom	= (short)src->rcFrameBottom;
}

///////////////////////////////////////////////////////////////////////////

VDPosition DubVideoPosition::ResolveToFrames(VDPosition length) const {
	return mOffset < 0 ? length : mOffset;
}

VDPosition DubVideoPosition::ResolveToMS(VDPosition frameCount, const VDFraction& timeBase, bool fromEnd) const {
	VDPosition t = mOffset;

	if (t < 0)
		t = frameCount;

	if (fromEnd)
		t = frameCount - t;

	if (t > frameCount)
		t = frameCount;

	return VDRoundToInt64((double)t * timeBase.AsInverseDouble() * 1000.0);
}

///////////////////////////////////////////////////////////////////////////

namespace {
	bool CheckFormatSizeCompatibility(int format, int w, int h) {
		const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(format);

		// We allow a special case for V210, which permits all even sizes.
		if (format == nsVDPixmap::kPixFormat_YUV422_V210) {
			if (w & 1)
				return false;

			return true;
		}

		if ((w & ((1<<formatInfo.qwbits)-1))
			|| (h & ((1<<formatInfo.qhbits)-1))
			|| (w & ((1<<formatInfo.auxwbits)-1))
			|| (h & ((1<<formatInfo.auxhbits)-1))
			)
		{
			return false;
		}

		return true;
	}

	int DegradeFormatYUV(int format) {
		using namespace nsVDPixmap;

		switch(format) {
		case kPixFormat_YUV410_Planar:	format = kPixFormat_YUV420_Planar; break;
		case kPixFormat_YUV411_Planar:	format = kPixFormat_YUV422_YUYV; break;
		case kPixFormat_YUV420_Planar:	format = kPixFormat_YUV422_YUYV; break;
		case kPixFormat_YUV422_Planar:	format = kPixFormat_YUV422_YUYV; break;
		case kPixFormat_YUV444_Planar:	format = kPixFormat_YUV422_YUYV; break;
		default:
			return 0;
		}

		return format;
	}

	int DegradeFormat(int format, uint32& rgbMask) {
		using namespace nsVDPixmap;

		rgbMask |= (1 << format);

		switch(format) {
		case kPixFormat_YUV410_Planar:	format = kPixFormat_YUV420_Planar; break;
		case kPixFormat_YUV411_Planar:	format = kPixFormat_YUV422_YUYV; break;
		case kPixFormat_YUV420_Planar:	format = kPixFormat_YUV422_YUYV; break;
		case kPixFormat_YUV422_Planar:	format = kPixFormat_YUV422_YUYV; break;
		case kPixFormat_YUV444_Planar:	format = kPixFormat_YUV422_YUYV; break;
		case kPixFormat_YUV422_YUYV:	format = kPixFormat_RGB888; break;
		case kPixFormat_YUV422_UYVY:	format = kPixFormat_RGB888; break;
		case kPixFormat_Y8:				format = kPixFormat_RGB888; break;

		// RGB formats are a bit tricky, as we must always be sure to try the
		// three major formats: 8888, 1555, 888.  The possible chains:
		//
		// 8888 -> 888 -> 1555 -> Pal8
		// 888 -> 8888 -> 1555 -> Pal8
		// 565 -> 8888 -> 888 -> 1555 -> Pal8
		// 1555 -> 8888 -> 888 -> Pal8

		case kPixFormat_RGB888:
			if (!(rgbMask & (1 << kPixFormat_XRGB8888)))
				format = kPixFormat_XRGB8888;
			else if (!(rgbMask & (1 << kPixFormat_XRGB1555)))
				format = kPixFormat_XRGB1555;
			else
				format = kPixFormat_Pal8;
			break;

		case kPixFormat_XRGB8888:
			if (rgbMask & (1 << kPixFormat_RGB888))
				format = kPixFormat_XRGB1555;
			else
				format = kPixFormat_RGB888;
			break;

		case kPixFormat_RGB565:
			format = kPixFormat_XRGB8888;
			break;

		case kPixFormat_XRGB1555:
			if (!(rgbMask & (1 << kPixFormat_XRGB8888)))
				format = kPixFormat_XRGB8888;
			else
				format = kPixFormat_Pal8;
			break;

		default:
			format = kPixFormat_Null;
			break;
		};

		if (rgbMask & (1 << format))
			format = kPixFormat_Null;

		return format;
	}
}

int VDRenderSetVideoSourceInputFormat(IVDVideoSource *vsrc, VDPixmapFormatEx format) {
	// first try incoming format
	if (vsrc->setTargetFormat(format))
		return vsrc->getTargetFormat().format;

	if (DegradeFormatYUV(format)) {
		// if incoming format is some yuv, try variants
		// at last set default
		while (format) {
			format.format = DegradeFormatYUV(format);

			if (vsrc->setTargetFormat(format))
				return vsrc->getTargetFormat().format;
		}

		// decoder failed to set default format
		return 0;
	}

	// proceed to rgb
	// at last set default
	uint32 rgbTrackMask = 0;

	while (format) {
		format.format = DegradeFormat(format, rgbTrackMask);

		if (vsrc->setTargetFormat(format))
			return vsrc->getTargetFormat().format;
	}

	// decoder failed to set default format
	return 0;
}

///////////////////////////////////////////////////////////////////////////
//
//	Dubber
//
///////////////////////////////////////////////////////////////////////////

class Dubber : public IDubber, public IDubberInternal {
private:
	MyError				err;
	bool				fError;

	VDAtomicInt			mStopLock;

	DubOptions			mOptions;

	typedef vdfastvector<AudioSource *> AudioSources;
	AudioSources		mAudioSources;

	typedef vdfastvector<IVDVideoSource *> VideoSources;
	VideoSources		mVideoSources;

	vdrefptr<VDFilterFrameVideoSource>	mpVideoFrameSource;

	IVDDubberOutputSystem	*mpOutputSystem;
	COMPVARS2			*compVars;

	DubAudioStreamInfo	aInfo;
	DubVideoStreamInfo	vInfo;

	bool				mbDoVideo;
	bool				mbDoAudio;
	bool				fPreview;
	bool				mbCompleted;
	bool				mbBackground;
	VDAtomicInt			mbAbort;
	VDAtomicInt			mbUserAbort;
	bool				fADecompressionOk;
	bool				fVDecompressionOk;

	int					mLiveLockMessages;

	VDDubIOThread		*mpIOThread;
	VDDubIOThread		*mpIODirect;
	VDDubProcessThread	mProcessThread;
	VDAtomicInt			mIOThreadCounter;

	vdautoptr<IVDVideoCompressor>	mpVideoCompressor;

	AVIPipe *			mpVideoPipe;
	VDAudioPipeline		mAudioPipe;

	VDDubFrameRequestQueue *mpVideoRequestQueue;

	IVDVideoDisplay *	mpInputDisplay;
	IVDVideoDisplay *	mpOutputDisplay;
	bool				mbInputDisplayInitialized;

	vdstructex<VDAVIBitmapInfoHeader>	mpCompressorVideoFormat;

	std::vector<AudioStream *>	mAudioStreams;
	AudioStream			*audioStream;
	AudioStream			*audioStatusStream;
	AudioStreamL3Corrector	*audioCorrector;
	vdautoptr<VDAudioFilterGraph> mpAudioFilterGraph;

	const FrameSubset		*inputSubsetActive;
	FrameSubset				*inputSubsetAlloc;

	vdstructex<WAVEFORMATEX> mAudioCompressionFormat;
	VDStringA			mAudioCompressionFormatHint;
	vdblock<char>		mAudioCompressionConfig;

	VDPixmapLayout		mVideoFilterOutputPixmapLayout;

	bool				fPhantom;

	IDubStatusHandler	*pStatusHandler;

	long				lVideoSizeEstimate;

	// interleaving
	VDStreamInterleaver		mInterleaver;
	VDRenderFrameMap		mVideoFrameMap;

	///////

	int					mLastProcessingThreadCounter;
	int					mProcessingThreadFailCount;
	int					mLastIOThreadCounter;
	int					mIOThreadFailCount;

	///////

	VDEvent<IDubber, bool>	mStoppedEvent;

public:
	Dubber(DubOptions *);
	~Dubber();

	void SetAudioCompression(const VDWaveFormat *wf, uint32 cb, const char *pShortNameHint, vdblock<char>& config);
	void SetPhantomVideoMode();
	void SetInputDisplay(IVDVideoDisplay *);
	void SetOutputDisplay(IVDVideoDisplay *);
	void SetAudioFilterGraph(const VDAudioFilterGraph& graph);

	void InitAudioConversionChain();
	void InitOutputFile();

	void InitDirectDraw();
	bool NegotiateFastFormat(const BITMAPINFOHEADER& bih);
	bool NegotiateFastFormat(int format);
	void InitSelectInputFormat();
	void Init(IVDVideoSource *const *pVideoSources, uint32 nVideoSources, AudioSource *const *pAudioSources, uint32 nAudioSources, IVDDubberOutputSystem *outsys, void *videoCompVars, const FrameSubset *, const VDFraction& frameRateTimeline);
	AudioStream* InitAudio(AudioSource *const *pAudioSources, uint32 nAudioSources);
	void Go(int iPriority = 0);
	void Stop();

	void InternalSignalStop();
	void Abort(bool userAbort);
	void ForceAbort();
	bool isRunning();
	bool IsAborted();
	bool isAbortedByUser();
	bool IsPreviewing();
	bool IsBackground();

	void SetStatusHandler(IDubStatusHandler *pdsh);
	void SetPriority(int index);
	void SetBackground(bool background);
	void UpdateFrames();
	void SetThrottleFactor(float throttleFactor);
	void GetPerfStatus(VDDubPerfStatus& status);

	void DumpStatus(VDTextOutputStream& os);

	VDEvent<IDubber, bool>& Stopped() { return mStoppedEvent; }
};


///////////////////////////////////////////////////////////////////////////

IDubber::~IDubber() {
}

IDubber *CreateDubber(DubOptions *xopt) {
	return new Dubber(xopt);
}

Dubber::Dubber(DubOptions *xopt)
	: mpIOThread(0)
	, mpIODirect(0)
	, mIOThreadCounter(0)
	, mpAudioFilterGraph(NULL)
	, mStopLock(0)
	, mpVideoPipe(NULL)
	, mpVideoRequestQueue(NULL)
	, mLastProcessingThreadCounter(0)
	, mProcessingThreadFailCount(0)
	, mLastIOThreadCounter(0)
	, mIOThreadFailCount(0)
	, mbBackground(false)
{
	mOptions			= *xopt;

	// clear the workin' variables...

	fError				= false;

	mbAbort				= false;
	mbUserAbort			= false;

	pStatusHandler		= NULL;

	fADecompressionOk	= false;
	fVDecompressionOk	= false;

	mpInputDisplay		= NULL;
	mpOutputDisplay		= NULL;
	vInfo.total_size	= 0;
	aInfo.total_size	= 0;
	vInfo.fAudioOnly	= false;

	audioStream			= NULL;
	audioStatusStream	= NULL;
	audioCorrector		= NULL;

	inputSubsetActive	= NULL;
	inputSubsetAlloc	= NULL;

	mbCompleted			= false;
	fPhantom = false;

	mLiveLockMessages = 0;
}

Dubber::~Dubber() {
	Stop();
}

/////////////////////////////////////////////////

void Dubber::SetAudioCompression(const VDWaveFormat *wf, uint32 cb, const char *pShortNameHint, vdblock<char>& config) {
	mAudioCompressionFormat.assign((const WAVEFORMATEX *)wf, cb);
	if (pShortNameHint)
		mAudioCompressionFormatHint = pShortNameHint;
	else
		mAudioCompressionFormatHint.clear();
		
	mAudioCompressionConfig.resize(config.size());
	memcpy(mAudioCompressionConfig.data(),config.data(),config.size());
}

void Dubber::SetPhantomVideoMode() {
	fPhantom = true;
	vInfo.fAudioOnly = true;
}

void Dubber::SetStatusHandler(IDubStatusHandler *pdsh) {
	pStatusHandler = pdsh;
}


/////////////

void Dubber::SetInputDisplay(IVDVideoDisplay *pDisplay) {
	mpInputDisplay = pDisplay;
}

void Dubber::SetOutputDisplay(IVDVideoDisplay *pDisplay) {
	mpOutputDisplay = pDisplay;
}

/////////////

void Dubber::SetAudioFilterGraph(const VDAudioFilterGraph& graph) {
	mpAudioFilterGraph = new VDAudioFilterGraph(graph);
}

void VDConvertSelectionTimesToFrames(const DubOptions& opt, const FrameSubset& subset, const VDFraction& subsetRate, VDPosition& startFrame, VDPosition& endFrame) {
	VDPosition subsetLength = subset.getTotalFrames();

	startFrame = opt.video.mSelectionStart.ResolveToFrames(subsetLength);
	endFrame = opt.video.mSelectionEnd.ResolveToFrames(subsetLength);
}

void VDTranslateSubsetDirectMode(FrameSubset& dst, const FrameSubset& src, IVDVideoSource *const *pVideoSources, VDPosition& selectionStart, VDPosition& selectionEnd) {
	bool selectionStartFixed = false;
	bool selectionEndFixed = false;
	VDPosition srcEnd = 0;
	VDPosition dstEnd = 0;

	for(FrameSubset::const_iterator it(src.begin()), itEnd(src.end()); it != itEnd; ++it) {
		const FrameSubsetNode& srcRange = *it;
		sint64 start = srcRange.start;
		int srcIndex = srcRange.source;

		IVDVideoSource *src = NULL;
		VDPosition srcStart;
		if (srcIndex >= 0) {
			src = pVideoSources[srcIndex];
			srcStart = src->asStream()->getStart();

			start = src->nearestKey(start + srcStart);
			if (start < 0)
				start = 0;
			else
				start -= srcStart;
		}

		srcEnd += srcRange.len;
		FrameSubset::iterator itNew(dst.addRange(srcRange.start, srcRange.len, srcRange.bMask, srcRange.source));
		dstEnd += itNew->len;

		// Mask ranges never need to be extended backwards, because they don't hold any
		// data of their own.  If an include range needs to be extended backwards, though,
		// it may need to extend into a previous merge range.  To avoid this problem,
		// we do a delete of the range before adding the tail.

		if (!itNew->bMask) {
			if (start < itNew->start) {
				FrameSubset::iterator it2(itNew);

				while(it2 != dst.begin()) {
					--it2;

					sint64 prevtail = it2->start + it2->len;

					// check for overlap
					if (prevtail < start || prevtail > itNew->start + itNew->len)
						break;

					if (it2->start >= start || !it2->bMask) {	// within extension range: absorb
						sint64 offset = itNew->start - it2->start;
						dstEnd += offset;
						dstEnd -= it2->len;
						itNew->len += offset;
						itNew->start = it2->start;
						it2 = dst.erase(it2);
					} else {									// before extension range and masked: split merge
						sint64 offset = start - itNew->start;
						it2->len -= offset;
						itNew->start -= offset;
						itNew->len += offset;
						break;
					}
				}

				sint64 left = itNew->start - start;
				
				if (left > 0) {
					itNew->start = start;
					itNew->len += left;
					dstEnd += left;
				}
			}
		}

		VDASSERT(dstEnd == dst.getTotalFrames());

		// Check whether one of the selection pointers needs to be updated.
		if (!selectionStartFixed && selectionStart < srcEnd) {
			sint64 frame = (selectionStart - (srcEnd - srcRange.len)) + srcRange.start;

			if (src) {
				frame = src->nearestKey(srcStart + frame);
				if (frame < 0)
					frame = 0;
				else
					frame -= srcStart;
			}

			selectionStart = (dstEnd - itNew->len) + (frame - itNew->start);
			selectionStartFixed = true;
		}

		if (!selectionEndFixed && selectionEnd < srcEnd) {
			selectionEnd += dstEnd - srcEnd;
			selectionEndFixed = true;
		}
	}

	if (!selectionStartFixed)
		selectionStart = dstEnd;

	if (!selectionEndFixed)
		selectionEnd = dstEnd;
}

void InitVideoStreamValuesStatic(DubVideoStreamInfo& vInfo, IVDVideoSource *video, AudioSource *audio, const DubOptions *opt, const FrameSubset *pfs, const VDPosition *pSelectionStartFrame, const VDPosition *pSelectionEndFrame) {
	vInfo.start_src		= 0;
	vInfo.end_src		= 0;
	vInfo.cur_dst		= 0;
	vInfo.end_dst		= 0;
	vInfo.cur_proc_dst	= 0;
	vInfo.end_proc_dst	= 0;
	vInfo.cur_proc_src = -1;

	if (!video)
		return;

	IVDStreamSource *pVideoStream = video->asStream();

	vInfo.start_src		= 0;
	vInfo.end_src		= pfs->getTotalFrames();

	if (pSelectionStartFrame && *pSelectionStartFrame >= vInfo.start_src)
		vInfo.start_src = *pSelectionStartFrame;

	if (pSelectionEndFrame && *pSelectionEndFrame <= vInfo.end_src)
		vInfo.end_src = *pSelectionEndFrame;

	if (vInfo.end_src < vInfo.start_src)
		vInfo.end_src = vInfo.start_src;

	// compute new frame rate

	VDFraction framerate(pVideoStream->getRate());

	if (opt->video.mFrameRateAdjustLo == 0) {
		if (opt->video.mFrameRateAdjustHi == DubVideoOptions::kFrameRateAdjustSameLength) {
			if (audio && audio->getLength())
				framerate = VDFraction((double)pVideoStream->getLength() * audio->getRate().asDouble() / audio->getLength());
		}
	} else
		framerate = VDFraction(opt->video.mFrameRateAdjustHi, opt->video.mFrameRateAdjustLo);

	vInfo.mFrameRateIn = framerate;
	vInfo.mFrameRatePreFilter = framerate;
	vInfo.mFrameRateIVTCFactor = VDFraction(1, 1);

	// Post-filter frame rate cannot be computed yet.

	// This may be changed post-filter.
	vInfo.mTimelineSourceLength = video->asStream()->getLength();
}

void InitVideoStreamValuesStatic2(DubVideoStreamInfo& vInfo, const DubOptions *opt, const FilterSystem *filtsys, const VDFraction& frameRateTimeline) {
	vInfo.mFrameRatePostFilter = vInfo.mFrameRatePreFilter;

	if (filtsys)
		vInfo.mFrameRatePostFilter = filtsys->GetOutputFrameRate();

	if (frameRateTimeline.getLo())
		vInfo.mFrameRateTimeline = frameRateTimeline;
	else
		vInfo.mFrameRateTimeline = vInfo.mFrameRatePostFilter;

	vInfo.mFrameRate = vInfo.mFrameRatePostFilter;

	if (opt->video.frameRateDecimation==1 && opt->video.frameRateTargetLo)
		vInfo.mFrameRate	= VDFraction(opt->video.frameRateTargetHi, opt->video.frameRateTargetLo);
	else
		vInfo.mFrameRate /= opt->video.frameRateDecimation;

	if (vInfo.end_src <= vInfo.start_src)
		vInfo.end_dst = 0;
	else
		vInfo.end_dst		= (long)(vInfo.mFrameRate / vInfo.mFrameRateTimeline).scale64t(vInfo.end_src - vInfo.start_src);

	vInfo.end_proc_dst	= vInfo.end_dst;
}

void InitAudioStreamValuesStatic(DubAudioStreamInfo& aInfo, AudioSource *audio, const DubOptions *opt) {
	aInfo.start_src		= 0;

	if (!audio)
		return;

	aInfo.start_src		= audio->getStart();

	// offset the start of the audio appropriately...
	aInfo.start_us = -(sint64)1000*opt->audio.offset;
	aInfo.start_src += audio->TimeToPositionVBR(aInfo.start_us);

	// resampling audio?

	aInfo.resampling = false;
	aInfo.converting = false;

	if (opt->audio.mode > DubAudioOptions::M_NONE) {
		if (opt->audio.new_rate) {
			aInfo.resampling = true;
		}

		bool change_precision = false;
		bool change_layout = false;
		const VDWaveFormat *fmt = audio->getWaveFormat();
		switch(opt->audio.newPrecision) {
		case DubAudioOptions::P_16BIT:
			change_precision = fmt->mSampleBits!=16;
			break;
		case DubAudioOptions::P_8BIT:
			change_precision = fmt->mSampleBits!=8;
			break;
		}
		switch(opt->audio.newChannels) {
		case DubAudioOptions::C_STEREO:
			change_layout = fmt->mChannels!=2;
			break;
		case DubAudioOptions::C_MONO:
		case DubAudioOptions::C_MONORIGHT:
		case DubAudioOptions::C_MONOLEFT:
			change_layout = fmt->mChannels!=1;
			break;
		}

		if (change_precision || change_layout) {
			aInfo.converting = true;

			aInfo.is_16bit = (opt->audio.newPrecision==DubAudioOptions::P_16BIT);

			if (opt->audio.newPrecision == DubAudioOptions::P_NOCHANGE) {
				const VDWaveFormat *fmt = audio->getWaveFormat();

				if (fmt->mTag != VDWaveFormat::kTagPCM || fmt->mSampleBits > 8)
					aInfo.is_16bit = true;
			}

			aInfo.is_stereo = opt->audio.newChannels==DubAudioOptions::C_STEREO
							|| (opt->audio.newChannels==DubAudioOptions::C_NOCHANGE && audio->getWaveFormat()->mChannels>1);
			aInfo.is_right = (opt->audio.newChannels==DubAudioOptions::C_MONORIGHT);
			aInfo.single_channel = (opt->audio.newChannels==DubAudioOptions::C_MONOLEFT || opt->audio.newChannels==DubAudioOptions::C_MONORIGHT);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

// may be called at any time in Init() after streams setup

void Dubber::InitAudioConversionChain() {

	// ready the audio stream for streaming operation

	aSrc->streamBegin(fPreview, false);
	fADecompressionOk = true;

	// Initialize audio conversion chain

	bool bUseAudioFilterGraph = (mOptions.audio.mode > DubAudioOptions::M_NONE && mpAudioFilterGraph);

	uint32 audioSourceCount = mAudioSources.size();
	vdfastvector<AudioStream *> sourceStreams(audioSourceCount);

	for(uint32 i=0; i<audioSourceCount; ++i) {
		AudioSource *asrc = mAudioSources[i];

		if (bUseAudioFilterGraph) {
			audioStream = new_nothrow AudioFilterSystemStream(*mpAudioFilterGraph, aInfo.start_us);
			if (!audioStream)
				throw MyMemoryError();

			mAudioStreams.push_back(audioStream);
		} else {
			// First, create a source.

			if (!(audioStream = new_nothrow AudioStreamSource(asrc, asrc->getEnd() - aInfo.start_src, mOptions.audio.mode > DubAudioOptions::M_NONE, aInfo.start_us)))
				throw MyMemoryError();

			mAudioStreams.push_back(audioStream);
		}

		// check the stream format and coerce to first stream if necessary
		if (i > 0) {
			const VDWaveFormat *format1 = sourceStreams[0]->GetFormat();
			const VDWaveFormat *format2 = audioStream->GetFormat();

			if (format1->mChannels != format2->mChannels || format1->mSampleBits != format2->mSampleBits) {
				audioStream = new_nothrow AudioStreamConverter(audioStream, format1->mSampleBits > 8, format1->mChannels > 1, false);
				mAudioStreams.push_back(audioStream);
			}

			if (format1->mSamplingRate != format2->mSamplingRate) {
				audioStream = new_nothrow AudioStreamResampler(audioStream, format1->mSamplingRate, true);
				mAudioStreams.push_back(audioStream);
			}
		}

		sourceStreams[i] = audioStream;
	}

	// Tack on a subset filter as well...
	sint64 offset = 0;
	
	if (mOptions.audio.fStartAudio)
		offset = vInfo.mFrameRateTimeline.scale64ir((sint64)1000000 * vInfo.start_src);

	bool applyTail = false;

	if (!mOptions.audio.fEndAudio && (inputSubsetActive->empty() || inputSubsetActive->back().end() >= vInfo.mTimelineSourceLength))
		applyTail = true;

	if (!(audioStream = new_nothrow AudioSubset(sourceStreams, mOptions.audio.mbApplyVideoTimeline ? inputSubsetActive : NULL, vInfo.mFrameRateTimeline, offset, applyTail)))
		throw MyMemoryError();

	mAudioStreams.push_back(audioStream);

	if (!bUseAudioFilterGraph) {
		// Attach a converter if we need to...

		if (aInfo.converting) {
			bool is_16bit = aInfo.is_16bit;

			// fix precision guess based on actual stream output if we are not changing it
			if (mOptions.audio.newPrecision == DubAudioOptions::P_NOCHANGE)
				is_16bit = audioStream->GetFormat()->mSampleBits > 8;

			if (aInfo.single_channel)
				audioStream = new_nothrow AudioStreamConverter(audioStream, is_16bit, aInfo.is_right, true);
			else
				audioStream = new_nothrow AudioStreamConverter(audioStream, is_16bit, aInfo.is_stereo, false);

			if (!audioStream)
				throw MyMemoryError();

			mAudioStreams.push_back(audioStream);
		}

		// Attach a converter if we need to...

		if (aInfo.resampling) {
			if (!(audioStream = new_nothrow AudioStreamResampler(audioStream, mOptions.audio.new_rate ? mOptions.audio.new_rate : aSrc->getWaveFormat()->mSamplingRate, mOptions.audio.fHighQuality)))
				throw MyMemoryError();

			mAudioStreams.push_back(audioStream);
		}

		// Attach an amplifier if needed...

		if (mOptions.audio.mode > DubAudioOptions::M_NONE && mOptions.audio.mVolume >= 0) {
			if (!(audioStream = new_nothrow AudioStreamAmplifier(audioStream, mOptions.audio.mVolume)))
				throw MyMemoryError();

			mAudioStreams.push_back(audioStream);
		}
	}

	// Make sure we only get what we want...

	if (!mVideoSources.empty() && mOptions.audio.fEndAudio) {
		const sint64 nFrames = (sint64)(vInfo.end_src - vInfo.start_src);
		const VDFraction& audioRate = audioStream->GetSampleRate();
		const VDFraction audioPerVideo(audioRate / vInfo.mFrameRateTimeline);

		audioStream->SetLimit(audioPerVideo.scale64r(nFrames));
	}

	audioStatusStream = audioStream;

	// Tack on a compressor if we want...

	AudioCompressor *pCompressor = NULL;

	if (mOptions.audio.mode > DubAudioOptions::M_NONE && !mAudioCompressionFormat.empty()) {
		if (!(pCompressor = new_nothrow AudioCompressor(audioStream, (const VDWaveFormat *)&*mAudioCompressionFormat, mAudioCompressionFormat.size(), mAudioCompressionFormatHint.c_str(), mAudioCompressionConfig)))
			throw MyMemoryError();

		audioStream = pCompressor;
		mAudioStreams.push_back(audioStream);
	}

	// Check the output format, and if we're compressing to
	// MPEG Layer III, compensate for the lag and create a bitrate corrector

	if (!g_prefs.fNoCorrectLayer3 && pCompressor && !pCompressor->fNoCorrectLayer3 && pCompressor->GetFormat()->mTag == WAVE_FORMAT_MPEGLAYER3) {
		pCompressor->CompensateForMP3();

		if (!(audioCorrector = new_nothrow AudioStreamL3Corrector(audioStream)))
			throw MyMemoryError();

		audioStream = audioCorrector;
		mAudioStreams.push_back(audioStream);
	}

}

void Dubber::InitOutputFile() {

	VDXStreamControl sc;
	if (mpOutputSystem)
		mpOutputSystem->GetStreamControl(sc);

	// Do audio.

	if (mbDoAudio) {
		// initialize AVI parameters...

		VDXStreamInfo si;
		VDXAVIStreamHeader& hdr = si.aviHeader;

		AVISTREAMINFOtoAVIStreamHeader(&hdr, &aSrc->getStreamInfo());
		hdr.dwStart			= 0;

		if (mOptions.audio.mode > DubAudioOptions::M_NONE) {
			const VDWaveFormat *outputAudioFormat = audioStream->GetFormat();
			hdr.dwSampleSize	= outputAudioFormat->mBlockSize;
			hdr.dwRate			= outputAudioFormat->mDataRate;
			hdr.dwScale			= outputAudioFormat->mBlockSize;
			hdr.dwLength		= MulDiv(hdr.dwLength, outputAudioFormat->mSamplingRate, aSrc->getWaveFormat()->mSamplingRate);
		}

		audioStream->GetStreamInfo(si);

		mpOutputSystem->SetAudio(si, audioStream->GetFormat(), audioStream->GetFormatLen(), mOptions.audio.enabled, audioStream->IsVBR());
	}

	// Do video.

	if (mbDoVideo) {
		int outputWidth;
		int outputHeight;
		
		if (mOptions.video.mode == DubVideoOptions::M_FULL) {
			const VDPixmapLayout& outputLayout = filters.GetOutputLayout();

			outputWidth = outputLayout.w;
			outputHeight = outputLayout.h;
		} else {
			const VDPixmap& output = vSrc->getTargetFormat();

			outputWidth = output.w;
			outputHeight = output.h;
		}

		VDXStreamInfo si;
		VDXAVIStreamHeader& hdr = si.aviHeader;

		AVISTREAMINFOtoAVIStreamHeader(&hdr, &vSrc->asStream()->getStreamInfo());

		hdr.dwSampleSize = 0;

		bool selectFcchandlerBasedOnFormat = false;
		if (mOptions.video.mode != DubVideoOptions::M_NONE && !mOptions.video.mbUseSmartRendering) {
			if (mpVideoCompressor) {
				hdr.fccHandler	= compVars->fccHandler;
				hdr.dwQuality	= compVars->lQ;
			} else {
				hdr.fccHandler	= VDMAKEFOURCC('D','I','B',' ');
				selectFcchandlerBasedOnFormat = true;
			}
		}

		hdr.dwRate			= vInfo.mFrameRate.getHi();
		hdr.dwScale			= vInfo.mFrameRate.getLo();
		hdr.dwLength		= vInfo.end_dst >= 0xFFFFFFFFUL ? 0xFFFFFFFFUL : (DWORD)vInfo.end_dst;

		hdr.rcFrame.left	= 0;
		hdr.rcFrame.top		= 0;
		hdr.rcFrame.right	= (short)outputWidth;
		hdr.rcFrame.bottom	= (short)outputHeight;

		int stream_mode = 0;

		// initialize compression

		int last_format = vSrc->getTargetFormat().format;
		if(mOptions.video.mode == DubVideoOptions::M_FULL)
			last_format = filters.GetOutputLayout().format;

		VDPixmapFormatEx outputFormatID = 0;
		int outputVariantID = 0;
		FilterModPixmapInfo outputFormatInfo;
		outputFormatInfo.clear();
		VDPixmapLayout driverLayout = {0};
		bool useOverride = false;

		if (mpOutputSystem) {
			outputFormatID = mpOutputSystem->GetVideoOutputFormatOverride(last_format);
			if (outputFormatID) useOverride = true;
		}

		if (!useOverride) {
			outputFormatID = mOptions.video.mOutputFormat;
			if (mOptions.video.mode <= DubVideoOptions::M_FASTREPACK) outputFormatID = 0;

			if (mpVideoCompressor) {
				int codec_format = mpVideoCompressor->QueryInputFormat(&outputFormatInfo);
				if (codec_format) outputFormatID.format = codec_format;
			}

			if (!outputFormatID) outputFormatID = vSrc->getTargetFormat();

			outputFormatID = VDPixmapFormatCombine(outputFormatID);
		}

		VDPixmapFormatEx outputFormatID0 = outputFormatID;

		VDPixmapCreateLinearLayout(driverLayout,VDPixmapFormatNormalize(outputFormatID),outputWidth,outputHeight,16);
		if (mpVideoCompressor && mpVideoCompressor->Query(&driverLayout, NULL)) {
			// use layout
			driverLayout.formatEx = outputFormatID;
		} else {
			driverLayout.format = 0;
		}

		if (!driverLayout.format) {
			if (mOptions.video.mode == DubVideoOptions::M_NONE) {
				const VDAVIBitmapInfoHeader *pFormat = vSrc->getImageFormat();
				mpCompressorVideoFormat.assign(pFormat, vSrc->asStream()->getFormatLen());

			} else if (mOptions.video.mode == DubVideoOptions::M_FASTREPACK) {
				// For fast recompress mode, the format must be already negotiated.
				const VDAVIBitmapInfoHeader *pSrcFormat = vSrc->getDecompressedFormat();
				const uint32 srcFormatLen = vSrc->getDecompressedFormatLen();
				if (!pSrcFormat)
					throw MyError("Unable to initialize video compression: The selected output format is not compatible with the Windows video codec API. Choose a different format.");
				mpCompressorVideoFormat.assign(pSrcFormat, srcFormatLen);

			} else {
				// For full recompression mode, we allow any format variant that the codec can accept.
				// Try to find a variant that works.
				vdfastvector<VDPixmapFormatEx> test_list;
				test_list.push_back(outputFormatID);
				// try to drop qualifiers and use proxy format (the only exception is HDYC)
				// color is still converted as originally requested
				VDPixmapFormatEx f = VDPixmapFormatNormalize(outputFormatID);
				int proxy_index = -1;
				if (f.format!=outputFormatID) {
					test_list.push_back(f.format);
					proxy_index = 1;
				}
				if (outputFormatID==nsVDPixmap::kPixFormat_XRGB64) {
					test_list.push_back(nsVDPixmap::kPixFormat_R10K);
					test_list.push_back(nsVDPixmap::kPixFormat_R210);
				}
				if (outputFormatID==nsVDPixmap::kPixFormat_YUV444_Planar16) {
					test_list.push_back(VDPixmapFormatCombineOpt(nsVDPixmap::kPixFormat_YUV444_V410, outputFormatID));
					test_list.push_back(VDPixmapFormatCombineOpt(nsVDPixmap::kPixFormat_YUV444_Y410, outputFormatID));
				}
				if (outputFormatID==nsVDPixmap::kPixFormat_YUV422_Planar16) {
					test_list.push_back(VDPixmapFormatCombineOpt(nsVDPixmap::kPixFormat_YUV422_P216, outputFormatID));
					test_list.push_back(VDPixmapFormatCombineOpt(nsVDPixmap::kPixFormat_YUV422_P210, outputFormatID));
					test_list.push_back(VDPixmapFormatCombineOpt(nsVDPixmap::kPixFormat_YUV422_V210, outputFormatID));
				}
				if (outputFormatID==nsVDPixmap::kPixFormat_YUV420_Planar16) {
					test_list.push_back(VDPixmapFormatCombineOpt(nsVDPixmap::kPixFormat_YUV420_P016, outputFormatID));
					test_list.push_back(VDPixmapFormatCombineOpt(nsVDPixmap::kPixFormat_YUV420_P010, outputFormatID));
				}

				bool foundDibCompatibleFormat = false;
				bool foundResult = false;

				{for(int test=0; test<test_list.size(); test++){
					VDPixmapFormatEx format = test_list[test];
					const int variants = VDGetPixmapToBitmapVariants(format);

					const VDAVIBitmapInfoHeader *pSrcFormat = vSrc->getDecompressedFormat();
					const uint32 srcFormatLen = vSrc->getDecompressedFormatLen();
					vdstructex<VDAVIBitmapInfoHeader> srcFormat;
					srcFormat.assign(pSrcFormat, srcFormatLen);

					for(int variant=1; variant <= variants; ++variant) {
						// BRA[64] is more simple but is not supported as output yet
						if (format==nsVDPixmap::kPixFormat_XRGB64 && variant==1)
							continue;

						bool dibCompatible;
						if (srcFormat.empty()) {
							dibCompatible = VDMakeBitmapFormatFromPixmapFormat(mpCompressorVideoFormat, format, variant, outputWidth, outputHeight);
						} else {
							dibCompatible = VDMakeBitmapFormatFromPixmapFormat(mpCompressorVideoFormat, srcFormat, format, variant, outputWidth, outputHeight);
						}
						foundDibCompatibleFormat |= dibCompatible;

						// If we have a video compressor, then we need a format that is DIB compatible. Otherwise,
						// we can go ahead and use a pixmap-only format.
						if (mpVideoCompressor) {
							if (dibCompatible)
								foundResult = mpVideoCompressor->Query((LPBITMAPINFO)&*mpCompressorVideoFormat, NULL);
						} else {
							if (dibCompatible) {
								foundResult = true;
							} else {
								mpCompressorVideoFormat.clear();
								foundResult = mpOutputSystem->IsVideoImageOutputEnabled();
							}
						}

						if (foundResult) {
							outputFormatID = format;
							outputVariantID = variant;
							if (test!=proxy_index) outputFormatID0 = format;
							break;
						}
					}

					if (foundResult) break;
				}}

				if (!foundResult) {
					if (foundDibCompatibleFormat) {
						throw MyError("Unable to initialize video compression. Check that the video codec is compatible with the output video frame size and that the settings are correct, or try a different one.");
					} else {
						throw MyError("Unable to initialize video compression: The selected output format is not compatible with the Windows video codec API. Choose a different format.");
					}
				}
			}
		}

		// Initialize output compressor.
		vdstructex<VDAVIBitmapInfoHeader>	outputFormat;

		if (mpVideoCompressor) {
			mpVideoCompressor->SetStreamControl(sc);

			vdstructex<BITMAPINFOHEADER> outputFormatW32;
			if (driverLayout.format)
				mpVideoCompressor->GetOutputFormat(&driverLayout, outputFormatW32);
			else
				mpVideoCompressor->GetOutputFormat(&*mpCompressorVideoFormat, outputFormatW32);
			outputFormat.assign((const VDAVIBitmapInfoHeader *)outputFormatW32.data(), outputFormatW32.size());

			// If we are using smart rendering, we have no choice but to match the source format.
			if (mOptions.video.mbUseSmartRendering) {
				IVDStreamSource *vsrcStream = vSrc->asStream();
				stream_mode = IVDXStreamSourceV5::kStreamModeDirectCopy|IVDXStreamSourceV5::kStreamModeUncompress|IVDXStreamSourceV5::kStreamModePlayForward;
				vsrcStream->applyStreamMode(stream_mode);
				const VDAVIBitmapInfoHeader *srcFormat = vSrc->getImageFormat();
				bool qresult;
				if (driverLayout.format)
					qresult = mpVideoCompressor->Query(&driverLayout, srcFormat);
				else
					qresult = mpVideoCompressor->Query(&*mpCompressorVideoFormat, srcFormat);

				if (!qresult)
					throw MyError("Cannot initialize smart rendering: The selected video codec is able to compress the source video, but cannot match the same compressed format.");

				outputFormat.assign(srcFormat, vsrcStream->getFormatLen());
			}

			if (driverLayout.format) {
				mpVideoCompressor->Start(driverLayout, outputFormatInfo, &*outputFormat, outputFormat.size(), vInfo.mFrameRate, vInfo.end_proc_dst);
				outputFormat.assign((const VDAVIBitmapInfoHeader *)mpVideoCompressor->GetOutputFormat(), mpVideoCompressor->GetOutputFormatSize());
			} else {
				mpVideoCompressor->Start(&*mpCompressorVideoFormat, mpCompressorVideoFormat.size(), &*outputFormat, outputFormat.size(), vInfo.mFrameRate, vInfo.end_proc_dst);
			}

			mpVideoCompressor->GetStreamInfo(si);
			lVideoSizeEstimate = mpVideoCompressor->GetMaxOutputSize();
		} else {
			if (mOptions.video.mode == DubVideoOptions::M_NONE) {

				IVDStreamSource *pVideoStream = vSrc->asStream();
				stream_mode = IVDXStreamSourceV5::kStreamModeDirectCopy|IVDXStreamSourceV5::kStreamModePlayForward;
				pVideoStream->applyStreamMode(stream_mode);

				if (vSrc->getImageFormat()->biCompression == 0xFFFFFFFF)
					throw MyError("Direct stream copy cannot be used with this video stream.\nYou may want to select different Input Driver.");

				outputFormat.assign(vSrc->getImageFormat(), pVideoStream->getFormatLen());

				lVideoSizeEstimate = 0;
				/*
				// cheese
				const VDPosition videoFrameStart	= pVideoStream->getStart();
				const VDPosition videoFrameEnd		= pVideoStream->getEnd();

				for(VDPosition frame = videoFrameStart; frame < videoFrameEnd; ++frame) {
					uint32 bytes = 0;

					if (!pVideoStream->read(frame, 1, 0, 0, &bytes, 0))
						if (lVideoSizeEstimate < bytes)
							lVideoSizeEstimate = bytes;
				}
				*/
			} else {
				if (mOptions.video.mbUseSmartRendering) {
					throw MyError("Cannot initialize smart rendering: No video codec is selected for compression.");
				}

				if (mOptions.video.mode == DubVideoOptions::M_FULL && !mpCompressorVideoFormat.empty()) {
					VDMakeBitmapFormatFromPixmapFormat(outputFormat, mpCompressorVideoFormat, outputFormatID, outputVariantID);
				} else
					outputFormat = mpCompressorVideoFormat;

				if (outputFormat.empty()) {
					VDPixmapLayout tempLayout;
					lVideoSizeEstimate = VDPixmapCreateLinearLayout(tempLayout, outputFormatID, outputWidth, outputHeight, 4);
				} else
					lVideoSizeEstimate = outputFormat->biSizeImage;
				lVideoSizeEstimate = (lVideoSizeEstimate+1) & -2;
			}
		}

		VDPixmapLayout outputLayout = {};
		if (mpCompressorVideoFormat.empty()) {
			VDPixmapCreateLinearLayout(outputLayout, outputFormatID, outputWidth, outputHeight, 16);
		} else {
			VDGetPixmapLayoutForBitmapFormat(*mpCompressorVideoFormat, mpCompressorVideoFormat.size(), outputLayout);
		}

		if (selectFcchandlerBasedOnFormat) {
			// It's been reported that Cinemacraft HDe does not like YCbCr formats whose
			// fccHandler is set to DIB, but it does accept the FOURCC code.

			if (!outputFormat.empty() && outputFormat->biCompression >= 0x20000000) {
				hdr.fccHandler = outputFormat->biCompression;
			}
		}

		if (outputLayout.format && mpOutputSystem->IsVideoImageOutputEnabled()) {
			mpOutputSystem->SetVideoImageLayout(si, outputLayout);
		} else {
			if (!fPreview) {
				if (mpOutputSystem->IsVideoImageOutputRequired() || outputFormat.empty())
					throw MyError("The current output video format is not supported by the selected output path.");
			}

			mpOutputSystem->SetVideo(si, &*outputFormat, outputFormat.size());
		}

		{
			// prepare layout used for output buffer
			// mVideoFilterOutputPixmapLayout

			const VDPixmapLayout& bmout = mOptions.video.mode >= DubVideoOptions::M_FULL ? filters.GetOutputLayout() : filters.GetInputLayout();

			if (driverLayout.format) {
				mVideoFilterOutputPixmapLayout = driverLayout;
			} else {
				VDMakeBitmapCompatiblePixmapLayout(mVideoFilterOutputPixmapLayout, bmout.w, bmout.h, outputFormatID, outputVariantID, bmout.palette);
				mVideoFilterOutputPixmapLayout.formatEx = outputFormatID0;
			}

			const char *s = VDPixmapGetInfo(mVideoFilterOutputPixmapLayout.format).name;

			if(mOptions.video.mode == DubVideoOptions::M_FULL)
				VDLogAppMessage(kVDLogInfo, kVDST_Dub, kVDM_FullUsingOutputFormat, 1, &s);

			if(mOptions.video.mode != DubVideoOptions::M_NONE && mOptions.video.mbUseSmartRendering)
				VDLogAppMessage(kVDLogInfo, kVDST_Dub, 11);
		}

		if (!stream_mode) {
			IVDStreamSource *pVideoStream = vSrc->asStream();
			stream_mode = IVDXStreamSourceV5::kStreamModeUncompress|IVDXStreamSourceV5::kStreamModePlayForward;
			pVideoStream->applyStreamMode(stream_mode);
		}
	}

	if (mbDoAudio) {
		if(audioStream->IsVBR())
			VDLogAppMessage(kVDLogInfo, kVDST_Dub, 12);
	}
}

void Dubber::InitDirectDraw() {
	if (!mOptions.perf.useDirectDraw || !mpInputDisplay)
		return;

	// Should we try and establish a DirectDraw overlay?

	if (mOptions.video.mode == DubVideoOptions::M_SLOWREPACK) {
		static const int kFormats[]={
			nsVDPixmap::kPixFormat_YUV420_Planar,
			nsVDPixmap::kPixFormat_YUV422_UYVY,
			nsVDPixmap::kPixFormat_YUV422_YUYV
		};

		for(int i=0; i<sizeof(kFormats)/sizeof(kFormats[0]); ++i) {
			const int format = kFormats[i];

			VideoSources::const_iterator it(mVideoSources.begin()), itEnd(mVideoSources.end());
			for(; it!=itEnd; ++it) {
				IVDVideoSource *vs = *it;

				if (!vs->setTargetFormat(format))
					break;
			}

			if (it == itEnd) {
				if (mpInputDisplay->SetSource(false, mVideoSources.front()->getTargetFormat(), 0, 0, false, mOptions.video.previewFieldMode>0)) {
					mbInputDisplayInitialized = true;
					break;
				}
			}
		}
	}
}

bool Dubber::NegotiateFastFormat(const BITMAPINFOHEADER& bih) {
	VideoSources::const_iterator it(mVideoSources.begin()), itEnd(mVideoSources.end());
	for(; it!=itEnd; ++it) {
		IVDVideoSource *vs = *it;

		if (!vs->setDecompressedFormat((const VDAVIBitmapInfoHeader *)&bih))
			return false;
	}
	
	const BITMAPINFOHEADER *pbih = (const BITMAPINFOHEADER *)mVideoSources.front()->getDecompressedFormat();

	if (mpVideoCompressor->Query(pbih)) {
		VDString buf;

		if (pbih->biCompression > VDAVIBitmapInfoHeader::kCompressionBitfields)
			buf = print_fourcc(pbih->biCompression);
		else
			buf.sprintf("RGB%d", pbih->biBitCount);

		const char *s = buf.c_str();
		VDLogAppMessage(kVDLogInfo, kVDST_Dub, kVDM_FastRecompressUsingFormat, 1, &s);
		return true;
	}

	return false;
}

bool Dubber::NegotiateFastFormat(int format) {
	VideoSources::const_iterator it(mVideoSources.begin()), itEnd(mVideoSources.end());
	for(; it!=itEnd; ++it) {
		IVDVideoSource *vs = *it;

		if (!vs->setTargetFormat(format))
			return false;
	}
	
	const BITMAPINFOHEADER *pbih = (const BITMAPINFOHEADER *)mVideoSources.front()->getDecompressedFormat();
	if (!pbih) return false;

	int s_variant;
	int s_format = VDBitmapFormatToPixmapFormat((const VDAVIBitmapInfoHeader&)*pbih,s_variant);
	// BRA[64] is not supported as output yet
	if (s_format==nsVDPixmap::kPixFormat_XRGB64 && s_variant==1) return false;

	if (mpVideoCompressor->Query(pbih)) {
		VDString buf;

		if (pbih->biCompression > VDAVIBitmapInfoHeader::kCompressionBitfields)
			buf = print_fourcc(pbih->biCompression);
		else
			buf.sprintf("RGB%d", pbih->biBitCount);

		const char *s = buf.c_str();
		VDLogAppMessage(kVDLogInfo, kVDST_Dub, kVDM_FastRecompressUsingFormat, 1, &s);
		return true;
	}

	return false;
}

void Dubber::InitSelectInputFormat() {
	//	DIRECT:			Don't care.
	//	FASTREPACK:		Negotiate with output compressor.
	//	SLOWREPACK:		[Dub]		Use selected format.
	//					[Preview]	Negotiate with display driver.
	//	FULL:			Use selected format.

	if (mOptions.video.mode == DubVideoOptions::M_NONE)
		return;

	const BITMAPINFOHEADER& bih = *(const BITMAPINFOHEADER *)vSrc->getImageFormat();

	if (mOptions.video.mode == DubVideoOptions::M_FASTREPACK && mpVideoCompressor) {
		// Attempt source format.
		// this is typically codec format and should fail
		// works when source is uncompressed
		if (NegotiateFastFormat(bih)) {
			if (mpInputDisplay)
				mpInputDisplay->Reset();
			mbInputDisplayInitialized = true;
			return;
		}

		// Attempt automatic (best).
		if (NegotiateFastFormat(0)) {
			if (mpInputDisplay)
				mpInputDisplay->Reset();
			mbInputDisplayInitialized = true;
			return;
		}

		// Don't use odd-width YUV formats.  They may technically be allowed, but
		// a lot of codecs crash.  For instance, Huffyuv in "Convert to YUY2"
		// mode will accept a 639x360 format for compression, but crashes trying
		// to decompress it.

		if (!(bih.biWidth & 1)) {
			if (NegotiateFastFormat(nsVDPixmap::kPixFormat_YUV422_UYVY)) {
				if (mpInputDisplay)
					mpInputDisplay->Reset();
				mbInputDisplayInitialized = true;
				return;
			}

			// Attempt YUY2.
			if (NegotiateFastFormat(nsVDPixmap::kPixFormat_YUV422_YUYV)) {
				if (mpInputDisplay)
					mpInputDisplay->Reset();
				mbInputDisplayInitialized = true;
				return;
			}

			if (!(bih.biHeight & 1)) {
				if (NegotiateFastFormat(nsVDPixmap::kPixFormat_YUV420_Planar)) {
					if (mpInputDisplay)
						mpInputDisplay->Reset();
					mbInputDisplayInitialized = true;
					return;
				}
			}
		}

		// Attempt RGB format negotiation.
		int format = mOptions.video.mInputFormat;
		if (format==0) format = vSrc->getSourceFormat();
		uint32 rgbTrackMask = 0;

		do {
			if (NegotiateFastFormat(format))
				return;

			format = DegradeFormat(format, rgbTrackMask);
		} while(format);

		throw MyError("Video format negotiation failed: use normal-recompress or full mode.");
	}

	// Negotiate format.

	VDPixmapFormatEx format = mOptions.video.mInputFormat;

	format = VDRenderSetVideoSourceInputFormat(vSrc, format);
	if (!format)
		throw MyError(
			"The video decompressor cannot decompress to the selected input format. "
			"Check for a \"Force YUY2\" setting in the codec's properties or select a different "
			"input video format under Video > Decode Format.");

	const char *s = VDPixmapGetInfo(vSrc->getTargetFormat().format).name;

	VDLogAppMessage(kVDLogInfo, kVDST_Dub, (mOptions.video.mode == DubVideoOptions::M_FULL) ? kVDM_FullUsingInputFormat : kVDM_SlowRecompressUsingFormat, 1, &s);
}

AudioStream* Dubber::InitAudio(AudioSource *const *pAudioSources, uint32 nAudioSources) {
	mOptions.audio.fStartAudio = false;
	mOptions.audio.fEndAudio = true;
	mAudioSources.assign(pAudioSources, pAudioSources + nAudioSources);
	AudioSource *audioSrc = mAudioSources.empty() ? NULL : mAudioSources.front();
	InitVideoStreamValuesStatic(vInfo, 0, audioSrc, &mOptions, 0, 0, 0);
	InitAudioStreamValuesStatic(aInfo, audioSrc, &mOptions);
	InitAudioConversionChain();
	return audioStream;
}

void Dubber::Init(IVDVideoSource *const *pVideoSources, uint32 nVideoSources, AudioSource *const *pAudioSources, uint32 nAudioSources, IVDDubberOutputSystem *pOutputSystem, void *videoCompVars, const FrameSubset *pfs, const VDFraction& frameRateTimeline) {
	// do some quick sanity checks
	if (pOutputSystem->IsVideoImageOutputRequired()) {
		if (mOptions.video.mbUseSmartRendering)
			throw MyError("The currently selected output plugin cannot be used in smart rendering mode.");

		if (mOptions.video.mode == DubVideoOptions::M_NONE)
			throw MyError("The currently selected output plugin cannot be used in direct stream copy mode.");
	}

	// modify options according to input system
	if (!pOutputSystem->IsCompressedAudioAllowed()) {
		mOptions.audio.mode = DubAudioOptions::M_FULL;
		mAudioCompressionFormat.clear();
		mAudioCompressionFormatHint.clear();
	}

	pOutputSystem->GetInterleavingOverride(mOptions.audio);

	// begin init
	mAudioSources.assign(pAudioSources, pAudioSources + nAudioSources);
	mVideoSources.assign(pVideoSources, pVideoSources + nVideoSources);

	mpOutputSystem		= pOutputSystem;
	mbDoVideo			= !mVideoSources.empty() && mpOutputSystem->AcceptsVideo();
	mbDoAudio			= !mAudioSources.empty() && mpOutputSystem->AcceptsAudio();

	fPreview			= mpOutputSystem->IsRealTime();

	inputSubsetActive	= pfs;
	compVars			= (COMPVARS2 *)videoCompVars;

	if (pOutputSystem->IsVideoCompressionEnabled() && pOutputSystem->AcceptsVideo() && mOptions.video.mode>DubVideoOptions::M_NONE && compVars && (compVars->dwFlags & ICMF_COMPVARS_VALID) && compVars->driver)
		mpVideoCompressor = VDCreateVideoCompressorVCM(compVars->driver, compVars->lDataRate*1024, compVars->lQ, compVars->lKey, false);

	if (!(inputSubsetActive = inputSubsetAlloc = new_nothrow FrameSubset(*pfs)))
		throw MyMemoryError();

	VDPosition selectionStartFrame;
	VDPosition selectionEndFrame;
	VDConvertSelectionTimesToFrames(mOptions, *inputSubsetActive, frameRateTimeline, selectionStartFrame, selectionEndFrame);

	// check the mode; if we're using DirectStreamCopy mode, we'll need to
	// align the subset to keyframe boundaries!
	if (!mVideoSources.empty() && mOptions.video.mode == DubVideoOptions::M_NONE) {
		vdautoptr<FrameSubset> newSubset(new_nothrow FrameSubset());
		if (!newSubset)
			throw MyMemoryError();

		VDTranslateSubsetDirectMode(*newSubset, *inputSubsetActive, mVideoSources.data(), selectionStartFrame, selectionEndFrame);

		delete inputSubsetAlloc;
		inputSubsetAlloc = newSubset.release();
		inputSubsetActive = inputSubsetAlloc;
	}

	// initialize stream values
	AudioSource *audioSrc = mAudioSources.empty() ? NULL : mAudioSources.front();
	InitVideoStreamValuesStatic(vInfo, vSrc, audioSrc, &mOptions, inputSubsetActive, &selectionStartFrame, &selectionEndFrame);
	InitAudioStreamValuesStatic(aInfo, audioSrc, &mOptions);

	// initialize directdraw display if in preview

	mbInputDisplayInitialized = false;
	if (fPreview)
		InitDirectDraw();

	// Select an appropriate input format.  This is really tricky...

	vInfo.fAudioOnly = true;
	if (mbDoVideo) {
		if (!mbInputDisplayInitialized)
			InitSelectInputFormat();
		vInfo.fAudioOnly = false;
	}

	// Initialize filter system.
	const VDPixmap& px = vSrc->getTargetFormat();
	const sint64 srcFrames = vSrc->asStream()->getLength();
	filters.prepareLinearChain(&g_filterChain, px.w, px.h, px, vInfo.mFrameRatePreFilter, srcFrames, vSrc->getPixelAspectRatio());

	mpVideoFrameSource = new VDFilterFrameVideoSource;
	mpVideoFrameSource->Init(vSrc, filters.GetInputLayout());

	filters.SetVisualAccelDebugEnabled(VDPreferencesGetFilterAccelVisualDebugEnabled());
	filters.SetAccelEnabled(VDPreferencesGetFilterAccelEnabled());

	if (mbDoVideo && mOptions.video.mode >= DubVideoOptions::M_FULL) {
		filters.SetAsyncThreadCount(VDPreferencesGetFilterThreadCount());
		filters.initLinearChain(mProcessThread.GetVideoFilterScheduler(), fPreview ? VDXFilterStateInfo::kStateRealTime | VDXFilterStateInfo::kStatePreview : 0, &g_filterChain, mpVideoFrameSource, px.w, px.h, px, px.palette, vInfo.mFrameRatePreFilter, srcFrames, vSrc->getPixelAspectRatio());

		InitVideoStreamValuesStatic2(vInfo, &mOptions, &filters, frameRateTimeline);
		
		const VDPixmapLayout& output = filters.GetOutputLayout();

		int outputFormat = 0;
		
		if (pOutputSystem)
			outputFormat = pOutputSystem->GetVideoOutputFormatOverride(output.format);

		if (!outputFormat && mpVideoCompressor)
			outputFormat = mpVideoCompressor->QueryInputFormat(0);

		if (!outputFormat) {
			outputFormat = mOptions.video.mOutputFormat;

			if (!outputFormat)
				outputFormat = vSrc->getTargetFormat().format;
		}

		if (!CheckFormatSizeCompatibility(outputFormat, output.w, output.h)) {
			const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(outputFormat);

			throw MyError("The output frame size is not compatible with the selected output format. (%dx%d, %s)", output.w, output.h, formatInfo.name);
		}

		filters.ReadyFilters();
	} else {
		// We need this to correctly create the video frame map.
		filters.SetAsyncThreadCount(-1);
		filters.initLinearChain(mProcessThread.GetVideoFilterScheduler(), fPreview ? VDXFilterStateInfo::kStateRealTime | VDXFilterStateInfo::kStatePreview : 0, &g_filterChain, mpVideoFrameSource, px.w, px.h, px, px.palette, vInfo.mFrameRatePreFilter, srcFrames, vSrc->getPixelAspectRatio());
		filters.ReadyFilters();
		InitVideoStreamValuesStatic2(vInfo, &mOptions, NULL, frameRateTimeline);
	}

	vInfo.mTimelineSourceLength = filters.GetOutputFrameCount();

	if (vInfo.end_dst > 0)
		vInfo.cur_dst = 0;

	// initialize input decompressor

	if (mbDoVideo) {
		VideoSources::const_iterator it(mVideoSources.begin()), itEnd(mVideoSources.end());
		for(; it!=itEnd; ++it) {
			IVDVideoSource *vs = *it;

			vs->streamBegin(fPreview, mOptions.video.mode == DubVideoOptions::M_NONE);
		}
		fVDecompressionOk = true;

	}

	// Initialize audio.
	if (mbDoAudio)
		InitAudioConversionChain();

	// Initialize input window display.

	if (!mbInputDisplayInitialized && mpInputDisplay) {
		if (mbDoVideo) {
			const VDPixmap& pxsrc = vSrc->getTargetFormat();
			VDPixmap px;
			px.w = pxsrc.w;
			px.h = pxsrc.h;
			px.format = pxsrc.format;
			px.pitch = pxsrc.pitch;
			px.pitch2 = pxsrc.pitch2;
			px.pitch3 = pxsrc.pitch3;
			px.palette = pxsrc.palette;
			px.data = NULL;
			px.data2 = NULL;
			px.data3 = NULL;
			mpInputDisplay->SetSource(false, px, NULL, 0, true, mOptions.video.previewFieldMode>0);
		}
	}

	// initialize output parameters and output file

	InitOutputFile();

	// Initialize output window display.

	if (mpOutputDisplay && mbDoVideo) {
		if (mOptions.video.mode == DubVideoOptions::M_FULL) {
			VDPixmap px;
			px.w = mVideoFilterOutputPixmapLayout.w;
			px.h = mVideoFilterOutputPixmapLayout.h;
			px.format = mVideoFilterOutputPixmapLayout.format;
			px.pitch = mVideoFilterOutputPixmapLayout.pitch;
			px.pitch2 = mVideoFilterOutputPixmapLayout.pitch2;
			px.pitch3 = mVideoFilterOutputPixmapLayout.pitch3;
			px.palette = mVideoFilterOutputPixmapLayout.palette;
			px.data = NULL;
			px.data2 = NULL;
			px.data3 = NULL;
			mpOutputDisplay->SetSource(false, px, NULL, 0, true, mOptions.video.previewFieldMode>0);
		}
	}

	// initialize interleaver

	bool bAudio = mbDoAudio;

	mInterleaver.Init(bAudio ? 2 : 1);
	mInterleaver.EnableInterleaving(mOptions.audio.enabled);
	mInterleaver.InitStream(0, lVideoSizeEstimate, 0, 1, 1, 1);

	if (bAudio) {
		double audioBlocksPerVideoFrame;

		if (!mOptions.audio.interval)
			audioBlocksPerVideoFrame = 1.0;
		else if (mOptions.audio.is_ms) {
			// blocks / frame = (ms / frame) / (ms / block)
			audioBlocksPerVideoFrame = vInfo.mFrameRate.AsInverseDouble() * 1000.0 / (double)mOptions.audio.interval;
		} else
			audioBlocksPerVideoFrame = 1.0 / (double)mOptions.audio.interval;

		const VDWaveFormat *pwfex = audioStream->GetFormat();
		const VDFraction& samplesPerSec = audioStream->GetSampleRate();
		sint32 preload = (sint32)(samplesPerSec * Fraction(mOptions.audio.preload, 1000)).roundup32ul();

		double samplesPerFrame = samplesPerSec.asDouble() / vInfo.mFrameRate.asDouble();

		mInterleaver.InitStream(1, pwfex->mBlockSize, preload, samplesPerFrame, audioBlocksPerVideoFrame, 262144);		// don't write TOO many samples at once
	}

	// initialize frame iterator

	if (mbDoVideo) {
		mVideoFrameMap.Init(mVideoSources,
			vInfo.start_src,
			vInfo.mFrameRateTimeline / vInfo.mFrameRate * vInfo.mFrameRateIVTCFactor,
			inputSubsetActive,
			vInfo.end_dst,
			mOptions.video.mbUseSmartRendering,
			mOptions.video.mode == DubVideoOptions::M_NONE,
			mOptions.video.mbPreserveEmptyFrames,
			&filters,
			mpOutputSystem->AreNullFramesAllowed(),
			mOptions.video.mode == DubVideoOptions::M_SLOWREPACK || mOptions.video.mode == DubVideoOptions::M_FASTREPACK
			);

		FilterSystem *filtsysToCheck = NULL;

		if (mOptions.video.mode >= DubVideoOptions::M_FULL && !filters.isEmpty() && mOptions.video.mbUseSmartRendering) {
			filtsysToCheck = &filters;
		}
	} else {
		mInterleaver.EndStream(0);
	}

	// Create data pipes.

	if (!(mpVideoPipe = new_nothrow AVIPipe(VDPreferencesGetRenderVideoBufferCount(), 16384)))
		throw MyMemoryError();

	if (mbDoAudio) {
		const VDWaveFormat *pwfex = audioStream->GetFormat();

		uint32 bytes = pwfex->mDataRate * 2;		// 2 seconds

		// we should always allocate at least 64K, just to have a decent size buffer
		if (bytes < 65536)
			bytes = 65536;

		// we must always have at least one block
		if (bytes < pwfex->mBlockSize)
			bytes = pwfex->mBlockSize;

		mAudioPipe.Init(bytes - bytes % pwfex->mBlockSize, pwfex->mBlockSize, audioStream->IsVBR());
	}
}

void Dubber::Go(int iPriority) {
	// check the version.  if NT, don't touch the processing priority!
	bool fNoProcessingPriority = VDIsWindowsNT();

	if (!iPriority)
		iPriority = fNoProcessingPriority || !mpOutputSystem->IsRealTime() ? 5 : 6;

	extern void VDSetProfileMode(int mode);
	if (mOptions.perf.fBenchmark) {
		VDSetProfileMode(1);
	} else {
		VDSetProfileMode(2);
	}

	// Initialize threads.
	mProcessThread.PreInit();
	mProcessThread.SetParent(this);
	mProcessThread.SetAbortSignal(&mbAbort);
	mProcessThread.SetStatusHandler(pStatusHandler);

	if (mbDoVideo) {
		mProcessThread.SetInputDisplay(mpInputDisplay);
		mProcessThread.SetOutputDisplay(mpOutputDisplay);
		mProcessThread.SetVideoCompressor(mpVideoCompressor, mOptions.video.mMaxVideoCompressionThreads);
		mProcessThread.SetVideoOutput(mVideoFilterOutputPixmapLayout, mOptions.video.mode);
	}

	mpVideoRequestQueue = new VDDubFrameRequestQueue;

	if (!mVideoSources.empty() && mVideoSources[0]->isSyncDecode()) {
		if (!(mpIODirect = new_nothrow VDDubIOThread(
					this,
					mVideoSources,
					0,
					mpVideoPipe,
					NULL,
					aInfo,
					vInfo,
					mIOThreadCounter,
					mpVideoRequestQueue,
					fPreview)))
			throw MyMemoryError();
	}

	mProcessThread.SetAudioSourcePresent(!mAudioSources.empty() && mAudioSources[0]);
	mProcessThread.SetVideoSources(mVideoSources.data(), mVideoSources.size());
	mProcessThread.SetVideoFrameSource(mpVideoFrameSource);
	mProcessThread.SetAudioCorrector(audioCorrector);
	mProcessThread.SetVideoRequestQueue(mpVideoRequestQueue);
	mProcessThread.SetVideoFilterSystem(&filters);
	mProcessThread.SetIODirect(mpIODirect);
	mProcessThread.Init(mOptions, &mVideoFrameMap, &vInfo, mpOutputSystem, mpVideoPipe, &mAudioPipe, &mInterleaver);
	mProcessThread.ThreadStart();

	SetThreadPriority(mProcessThread.getThreadHandle(), g_iPriorities[iPriority-1][0]);

	// Continue with other threads.

	if (!(mpIOThread = new_nothrow VDDubIOThread(
				this,
				mVideoSources,
				audioStream,
				(mbDoVideo && !mpIODirect) ? mpVideoPipe : NULL,
				mbDoAudio ? &mAudioPipe : NULL,
				aInfo,
				vInfo,
				mIOThreadCounter,
				mpVideoRequestQueue,
				fPreview)))
		throw MyMemoryError();

	if (!mpIOThread->ThreadStart())
		throw MyError("Couldn't start I/O thread");

	SetThreadPriority(mpIOThread->getThreadHandle(), g_iPriorities[iPriority-1][1]);

	// We need to make sure that 100% actually means 100%.
	SetThrottleFactor((float)(mOptions.mThrottlePercent * 65536 / 100) / 65536.0f);

	// Create status window during the dub.
	if (pStatusHandler) {
		pStatusHandler->InitLinks(&aInfo, &vInfo, audioStatusStream, this, &mOptions);
		pStatusHandler->Display(NULL, iPriority);
	}
}

//////////////////////////////////////////////

void Dubber::Stop() {
	if (mStopLock.xchg(1))
		return;

	mbAbort = true;

	if (mpIOThread)
		mpIOThread->Abort();

	if (mpVideoPipe)
		mpVideoPipe->abort();

	mAudioPipe.Abort();
	mProcessThread.Abort();

	int nObjectsToWaitOn = 0;
	HANDLE hObjects[3];

	if (VDSignal *pBlitterSigComplete = mProcessThread.GetBlitterSignal())
		hObjects[nObjectsToWaitOn++] = pBlitterSigComplete->getHandle();

	if (mProcessThread.isThreadAttached())
		hObjects[nObjectsToWaitOn++] = mProcessThread.getThreadHandle();

	if (mpIOThread && mpIOThread->isThreadAttached())
		hObjects[nObjectsToWaitOn++] = mpIOThread->getThreadHandle();

	uint32 startTime = VDGetCurrentTick();

	bool quitQueued = false;

	while(nObjectsToWaitOn > 0) {
		DWORD dwRes;

		dwRes = MsgWaitForMultipleObjects(nObjectsToWaitOn, hObjects, FALSE, 10000, QS_SENDMESSAGE);

		if (WAIT_OBJECT_0 + nObjectsToWaitOn == dwRes) {
			if (!guiDlgMessageLoop(NULL))
				quitQueued = true;

			continue;
		}
		
		uint32 currentTime = VDGetCurrentTick();

		if ((dwRes -= WAIT_OBJECT_0) < nObjectsToWaitOn) {
			if (dwRes+1 < nObjectsToWaitOn)
				hObjects[dwRes] = hObjects[nObjectsToWaitOn - 1];
			--nObjectsToWaitOn;
			startTime = currentTime;
			continue;
		}

		if (currentTime - startTime > 10000) {
			if (IDOK == MessageBox(g_hWnd, "Something appears to be stuck while trying to stop (thread deadlock). Abort operation and exit program?", "VirtualDub Internal Error", MB_ICONEXCLAMATION|MB_OKCANCEL)) {
				vdprotected("aborting process due to a thread deadlock") {
					ExitProcess(0);
				}
			}

			startTime = currentTime;
		}
	}

	extern void VDSetProfileMode(int mode);
	if (!mOptions.perf.fBenchmark) {
		VDSetProfileMode(0);
	}

	if (quitQueued)
		PostQuitMessage(0);

	mbCompleted = mProcessThread.IsCompleted();

	if (!fError && mpIOThread)
		fError = mpIOThread->GetError(err);

	if (!fError && mpIODirect)
		fError = mpIODirect->GetError(err);

	if (!fError)
		fError = mProcessThread.GetError(err);

	delete mpIOThread;
	mpIOThread = 0;

	mProcessThread.Shutdown();

	delete mpIODirect;
	mpIODirect = 0;

	if (pStatusHandler)
		pStatusHandler->Freeze();

	mpVideoCompressor = NULL;

	if (mpVideoPipe)	{ delete mpVideoPipe; mpVideoPipe = NULL; }
	mAudioPipe.Shutdown();

	if (mpVideoRequestQueue) {
		delete mpVideoRequestQueue;
		mpVideoRequestQueue = NULL;
	}

	filters.DeinitFilters();

	if (fVDecompressionOk)	{
		IVDStreamSource *pVideoStream = vSrc->asStream();
		pVideoStream->streamEnd(); 
		pVideoStream->applyStreamMode(IVDXStreamSourceV5::kStreamModeUncompress);
	}
	if (fADecompressionOk)	{ aSrc->streamEnd(); }

	{
		std::vector<AudioStream *>::const_iterator it(mAudioStreams.begin()), itEnd(mAudioStreams.end());

		for(; it!=itEnd; ++it)
			delete *it;

		mAudioStreams.clear();
	}

	if (inputSubsetAlloc)		{ delete inputSubsetAlloc; inputSubsetAlloc = NULL; }

	// deinitialize DirectDraw

	filters.DeallocateBuffers();
	filters.SetAsyncThreadCount(-1);

	if (pStatusHandler && vInfo.cur_proc_src >= 0)
		pStatusHandler->SetLastPosition(vInfo.cur_proc_src, false);

	if (VDIsAtLeastVistaW32())
		SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_END);

	if (fError) {
		throw err;
		fError = false;
	}
}

///////////////////////////////////////////////////////////////////

void Dubber::InternalSignalStop() {
	if (!mbAbort.compareExchange(true, false) && !mStopLock) {
		if (mpIOThread)
			mpIOThread->Abort();

		mProcessThread.Abort();

		mStoppedEvent.Raise(this, false);
	}
}

void Dubber::Abort(bool userAbort) {
	if (!mbAbort.compareExchange(true, false) && !mStopLock) {
		if (mpIOThread)
			mpIOThread->Abort();

		mProcessThread.Abort();

		mbUserAbort = userAbort;
		mAudioPipe.Abort();
		mpVideoPipe->abort();

		mStoppedEvent.Raise(this, userAbort);
	}
}

bool Dubber::isRunning() {
	return !mbAbort;
}

bool Dubber::IsAborted() {
	return !mbCompleted;
}

bool Dubber::isAbortedByUser() {
	return mbUserAbort != 0;
}

bool Dubber::IsPreviewing() {
	return fPreview;
}

bool Dubber::IsBackground() {
	return mbBackground;
}

void Dubber::SetPriority(int index) {
	if (mpIOThread && mpIOThread->isThreadActive())
		SetThreadPriority(mpIOThread->getThreadHandle(), g_iPriorities[index][0]);

	mProcessThread.SetPriority(g_iPriorities[index][1]);
}

void Dubber::SetBackground(bool background) {
	// Requires Vista or above.
	if (!VDIsAtLeastVistaW32())
		return;

	if (background == mbBackground)
		return;

	mbBackground = background;

	SetPriorityClass(GetCurrentProcess(), background ? PROCESS_MODE_BACKGROUND_BEGIN : PROCESS_MODE_BACKGROUND_END);

	if (pStatusHandler)
		pStatusHandler->OnBackgroundStateUpdated();
}

void Dubber::UpdateFrames() {
	mProcessThread.UpdateFrames();

	if (mLiveLockMessages < kLiveLockMessageLimit && !mStopLock) {
		uint32 curTime = VDGetCurrentTick();

		int iocount = mIOThreadCounter;
		int prcount = mProcessThread.GetActivityCounter();

		if (mLastIOThreadCounter != iocount) {
			mLastIOThreadCounter = iocount;
			mIOThreadFailCount = curTime;
		} else if (mLastIOThreadCounter && (curTime - mIOThreadFailCount - 30000) < 3600000) {		// 30s to 1hr
			if (mpIOThread->isThreadActive()) {
				void *eip = mpIOThread->ThreadLocation();
				const char *action = mpIOThread->GetCurrentAction();
				VDLogAppMessage(kVDLogInfo, kVDST_Dub, kVDM_IOThreadLivelock, 2, &eip, &action);
				++mLiveLockMessages;
			}
			mLastIOThreadCounter = 0;
		}

		if (mLastProcessingThreadCounter != prcount) {
			mLastProcessingThreadCounter = prcount;
			mProcessingThreadFailCount = curTime;
		} else if (mLastProcessingThreadCounter && (curTime - mProcessingThreadFailCount - 30000) < 3600000) {		// 30s to 1hr
			if (mProcessThread.isThreadActive()) {
				void *eip = mProcessThread.ThreadLocation();
				const char *action = mProcessThread.GetCurrentAction();
				VDLogAppMessage(kVDLogInfo, kVDST_Dub, kVDM_ProcessingThreadLivelock, 2, &eip, &action);
				++mLiveLockMessages;
			}
			mLastProcessingThreadCounter = 0;
		}
	}
}

void Dubber::SetThrottleFactor(float throttleFactor) {
	mProcessThread.SetThrottle(throttleFactor);
	if (mpIOThread)
		mpIOThread->SetThrottle(throttleFactor);
}

void Dubber::GetPerfStatus(VDDubPerfStatus& status) {
	status.mVideoBuffersActive = 0;
	status.mVideoBuffersTotal = 0;

	if (mpVideoPipe) {
		int inuse;
		int dummy;
		int allocated;

		mpVideoPipe->getQueueInfo(inuse, dummy, allocated);

		status.mVideoBuffersActive = inuse;
		status.mVideoBuffersTotal = allocated;
	}

	status.mVideoRequestsActive = 0;

	if (mpVideoRequestQueue)
		status.mVideoRequestsActive = mpVideoRequestQueue->GetQueueLength();

	status.mAudioBufferInUse = mAudioPipe.getLevel();
	status.mAudioBufferTotal = mAudioPipe.size();

	status.mIOActivityRatio = 0.0f;
	if (mpIOThread)
		status.mIOActivityRatio = mpIOThread->GetActivityRatio();

	status.mProcActivityRatio = mProcessThread.GetActivityRatio();
}

void Dubber::DumpStatus(VDTextOutputStream& os) {
	mProcessThread.DumpStatus(os);
}
