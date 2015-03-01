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

#ifndef f_DUBIO_H
#define f_DUBIO_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/error.h>
#include <vd2/system/thread.h>
#include <vd2/system/vdstl.h>
#include "DubUtils.h"
#include "DubFrameRequestQueue.h"

class VDAtomicInt;
class IVDStreamSource;
class IVDVideoSource;
class VDRenderFrameIterator;
class AudioStream;
class AVIPipe;
class VDAudioPipeline;
template<class T, class Allocator> class VDRingBuffer;
class DubAudioStreamInfo;
class DubVideoStreamInfo;
class IDubberInternal;

///////////////////////////////////////////////////////////////////////////
//
//	VDDubIOThread
//
///////////////////////////////////////////////////////////////////////////

namespace nsVDDub {
	enum {
		kBufferFlagDelta			= 1,		///< This video frame is a delta frame (i.e. not a key frame).
		kBufferFlagPreload			= 2,		///< This video frame is not a final frame and is being queued for decoding purposes.
		kBufferFlagDirectWrite		= 4,		///< This video frame should be streamed to the output rather than being processed (smart rendering).
		kBufferFlagSameAsLast		= 8,		///< This video frame is the same as the previous final frame and can be dropped as a duplicate.
		kBufferFlagInternalDecode	= 16,		///< This video frame is a dummy to pull a frame that the video decoder already has decoded due to reordering.
		kBufferFlagFlushCodec		= 32		///< This video frame is a dummy to pull a frame that the video encoder has queued. Decoding and filtering should be skipped.
	};
}

class VDDubIOThread : public VDThread {
	VDDubIOThread(const VDDubIOThread&);
	VDDubIOThread& operator=(const VDDubIOThread&);
public:
	VDDubIOThread(
		IDubberInternal		*pParent,
		const vdfastvector<IVDVideoSource *>& videoSources,
		AudioStream			*pAudio,
		AVIPipe				*const pVideoPipe,
		VDAudioPipeline		*const pAudioPipe,
		DubAudioStreamInfo&	_aInfo,
		DubVideoStreamInfo& _vInfo,
		VDAtomicInt&		threadCounter,
		VDDubFrameRequestQueue *videoRequestQueue,
		bool				preview
		);
	~VDDubIOThread();

	bool GetError(MyError& e) {
		if (mbError) {
			e.TransferFrom(mError);
			return true;
		}
		return false;
	}

	const char *GetCurrentAction() const {
		return mpCurrentAction;
	}

	void SetThrottle(float f);

	float GetActivityRatio() const { return mLoopThrottle.GetActivityRatio(); }

	void Abort();

protected:
	void ThreadRun();
	bool MainAddVideoFrame();
	void ReadRawVideoFrame(int sourceIndex, VDPosition streamFrame, VDPosition displayFrame, VDPosition targetSample, bool preload, bool direct);
	void ReadNullVideoFrame(int sourceIndex, VDPosition displayFrame, VDPosition targetSample);
	bool MainAddAudioFrame();

	IDubberInternal		*mpParent;
	MyError				mError;
	bool				mbError;
	bool				mbPreview;

	vdfastvector<char>	mAudioBuffer;
	uint64				mAudioSamplesWritten;

	VDDubFrameRequest	mVideoRequest;
	bool				mbVideoRequestActive;
	bool				mbVideoRequestFirstSample;
	VDPosition			mVideoRequestTargetSample;
	IVDVideoSource		*mpVideoRequestSource;

	bool				mbVideoWaitingForSpace;
	bool				mbVideoWaitingForRequest;

	VDLoopThrottle		mLoopThrottle;

	// config vars (ick)
	const vdfastvector<IVDVideoSource *>& mVideoSources;
	AudioStream			*const mpAudio;
	AVIPipe				*const mpVideoPipe;
	VDAudioPipeline		*const mpAudioPipe;
	DubAudioStreamInfo&	aInfo;
	DubVideoStreamInfo& vInfo;
	VDAtomicInt&		mThreadCounter;
	VDDubFrameRequestQueue *mpVideoRequestQueue;

	VDAtomicInt			mbAbort;
	VDSignal			mAbortSignal;

	const char			*volatile mpCurrentAction;
};


#endif
