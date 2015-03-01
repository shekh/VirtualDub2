//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2009 Avery Lee
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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef f_VD2_DUBPROCESS_H
#define f_VD2_DUBPROCESS_H

#include <vd2/system/thread.h>
#include <vd2/system/profile.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "DubProcessVideo.h"
#include "DubPreviewClock.h"

class IVDMediaOutput;
class IVDMediaOutputStream;
class IVDDubberOutputSystem;
class VDAudioPipeline;
class DubOptions;
class IVDVideoSource;
class IVDVideoDisplay;
class AudioStream;
class VDStreamInterleaver;
class IVDVideoCompressor;
class IVDAsyncBlitter;
class IDubStatusHandler;
struct VDRenderVideoPipeFrameInfo;
class VDRenderOutputBufferTracker;
class VDRenderOutputBuffer;
class VDThreadedVideoCompressor;
class FilterSystem;
class VDFilterFrameRequest;
class IVDFilterFrameClientRequest;
class VDFilterFrameManualSource;
class VDTextOutputStream;

class VDDubProcessThread : public VDThread, public IVDDubVideoProcessorCallback {
public:
	VDDubProcessThread();
	~VDDubProcessThread();

	bool IsCompleted() const { return mbCompleted; }

	IVDFilterSystemScheduler *GetVideoFilterScheduler();

	void PreInit();

	void SetParent(IDubberInternal *pParent);
	void SetAbortSignal(VDAtomicInt *pAbort);
	void SetStatusHandler(IDubStatusHandler *pStatusHandler);
	void SetInputDisplay(IVDVideoDisplay *pVideoDisplay);
	void SetOutputDisplay(IVDVideoDisplay *pVideoDisplay);
	void SetVideoFilterOutput(const VDPixmapLayout& layout);
	void SetVideoSources(IVDVideoSource *const *pVideoSources, uint32 count);
	void SetVideoFrameSource(VDFilterFrameManualSource *fs);
	void SetAudioSourcePresent(bool present);
	void SetAudioCorrector(AudioStreamL3Corrector *pCorrector);
	void SetVideoCompressor(IVDVideoCompressor *pCompressor, int maxThreads);
	void SetVideoFilterSystem(FilterSystem *fs);
	void SetVideoRequestQueue(VDDubFrameRequestQueue *q);

	void SetPriority(int priority);

	void Init(const DubOptions& opts, const VDRenderFrameMap *frameMap, DubVideoStreamInfo *pvsi, IVDDubberOutputSystem *pOutputSystem, AVIPipe *pVideoPipe, VDAudioPipeline *pAudioPipe, VDStreamInterleaver *pStreamInterleaver);
	void Shutdown();

	void Abort();
	void UpdateFrames();

	bool GetError(MyError& e) {
		if (mbError) {
			e.TransferFrom(mError);
			return true;
		}
		return false;
	}

	uint32 GetActivityCounter() {
		return mActivityCounter;
	}

	const char *GetCurrentAction() {
		return mpCurrentAction;
	}

	VDSignal *GetBlitterSignal();

	void SetThrottle(float f);
	float GetActivityRatio() const { return mLoopThrottle.GetActivityRatio(); }

	void DumpStatus(VDTextOutputStream& os);

protected:
	void NextSegment();

	bool WriteAudio(sint32 count);

	void ThreadRun();
	void UpdateAudioStreamRate();

	void OnVideoStreamEnded();
	void OnFirstFrameWritten();

	const DubOptions		*opt;

	VDStreamInterleaver		*mpInterleaver;
	VDLoopThrottle			mLoopThrottle;
	IDubberInternal			*mpParent;

	// OUTPUT
	IVDMediaOutput			*mpAVIOut;
	IVDMediaOutputStream	*mpAudioOut;			// alias: AVIout->audioOut
	IVDMediaOutputStream	*mpVideoOut;			// alias: AVIout->videoOut
	IVDDubberOutputSystem	*mpOutputSystem;

	// AUDIO SECTION
	VDAudioPipeline			*mpAudioPipe;
	AudioStreamL3Corrector	*mpAudioCorrector;
	bool				mbAudioPresent;
	bool				mbAudioEnded;
	uint64				mAudioSamplesWritten;
	vdfastvector<char>	mAudioBuffer;

	// VIDEO SECTION
	AVIPipe					*mpVideoPipe;
	bool				mbVideoEnded;
	bool				mbVideoPushEnded;

	DubVideoStreamInfo	*mpVInfo;
	IVDAsyncBlitter		*mpBlitter;
	IDubStatusHandler	*mpStatusHandler;

	typedef vdfastvector<IVDVideoSource *> VideoSources;
	VideoSources		mVideoSources;

	// PREVIEW
	bool				mbPreview;
	bool				mbFirstPacket;

	// ERROR HANDLING
	MyError				mError;
	bool				mbError;
	bool				mbCompleted;
	VDAtomicInt			*mpAbort;

	const char			*volatile mpCurrentAction;
	VDAtomicInt			mActivityCounter;

	VDDubPreviewClock	mPreviewClock;

	VDDubVideoProcessor	mVideoProcessor;
};

#endif
