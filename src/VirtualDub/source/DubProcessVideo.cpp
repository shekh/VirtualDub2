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

#include "stdafx.h"
#include <vd2/system/log.h>
#include <vd2/system/profile.h>
#include <vd2/system/time.h>
#include <vd2/Dita/resources.h>
#include <vd2/Riza/bitmap.h>
#include <../Kasumi/h/uberblit_base.h>
#include "AVIPipe.h"
#include "AVIOutput.h"
#include "Dub.h"
#include "DubIO.h"
#include "DubPreviewClock.h"
#include "DubProcessVideo.h"
#include "DubProcessVideoDisplay.h"
#include "DubStatus.h"
#include "FilterFrameRequest.h"
#include "FilterFrameManualSource.h"
#include "FilterSystem.h"
#include "prefs.h"
#include "ThreadedVideoCompressor.h"
#include "VideoSource.h"

using namespace nsVDDub;

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
		kVDM_FullUsingOutputFormat,
		kVDM_ReducingCodecThreadCount
	};

	enum {
		kReasonableBFrameBufferLimit = 100
	};
}

extern sint32 VDPreferencesGetVideoFilterProcessAheadCount();

///////////////////////////////////////////////////////////////////////////

VDDubVideoProcessor::VDDubVideoProcessor()
	: mActivePaths(kPathInitial)
	, mReactivatedPaths(0)
	, mpOptions(NULL)
	, mpStatusHandler(NULL)
	, mpVInfo(NULL)
	, mbVideoPushEnded(false)
	, mbVideoEnded(false)
	, mbPreview(false)
	, mbShowLast(false)
	, mbFirstFrame(true)
	, mppCurrentAction(NULL)
	, mpCB(NULL)
	, mpVideoFrameMap(NULL)
	, mpVideoFrameSource(NULL)
	, mpVideoRequestQueue(NULL)
	, mpVideoPipe(NULL)
	, mpIODirect(NULL)
	, mbSourceStageThrottled(false)
	, mbFilterStageThrottled(false)
	, mpVideoFilters(NULL)
	, mFrameProcessAheadCount(0)
	, mBatchNumber(0)
	, mpVideoCompressor(NULL)
	, mpThreadedVideoCompressor(NULL)
	, mVideoFramesDelayed(0)
	, mbFlushingCompressor(false)
	, mpVideoOut(NULL)
	, mpVideoImageOut(NULL)
	, mExtraOutputBuffersRequired(0)
	, mpLoopThrottle(NULL)
	, mpFrameBufferTracker(NULL)
	, mpDisplayBufferTracker(NULL)
	, mFramesToDrop(0)
	, mpProcDisplay(NULL)
	, mThreadPriority(VDThread::kPriorityDefault)
{
}

VDDubVideoProcessor::~VDDubVideoProcessor() {
	if (mpProcDisplay) {
		delete mpProcDisplay;
		mpProcDisplay = NULL;
	}

	if (mpDisplayBufferTracker) {
		mpDisplayBufferTracker->Release();
		mpDisplayBufferTracker = NULL;
	}
}

int VDDubVideoProcessor::AddRef() {
	return 2;
}

int VDDubVideoProcessor::Release() {
	return 1;
}

void VDDubVideoProcessor::SetPriority(int priority) {
	if (mThreadPriority != priority) {
		mThreadPriority = priority;

		if (mpThreadedVideoCompressor)
			mpThreadedVideoCompressor->SetPriority(mThreadPriority);

		if (mpVideoFilters)
			mpVideoFilters->SetAsyncThreadPriority(mThreadPriority);
	}
}

void VDDubVideoProcessor::PreInit() {
	mpProcDisplay = new VDDubVideoProcessorDisplay();
}

void VDDubVideoProcessor::SetCallback(IVDDubVideoProcessorCallback *cb) {
	mpCB = cb;
}

void VDDubVideoProcessor::SetStatusHandler(IDubStatusHandler *handler) {
	mpStatusHandler = handler;
	mpProcDisplay->mpStatusHandler = handler;
}

void VDDubVideoProcessor::SetOptions(const DubOptions *opts) {
	mpOptions = opts;
	mpProcDisplay->SetOptions(opts);
}

void VDDubVideoProcessor::SetThreadSignals(const char *volatile *pStatus, VDLoopThrottle *pLoopThrottle) {
	mppCurrentAction = pStatus;
	mpLoopThrottle = pLoopThrottle;

	mpProcDisplay->SetThreadInfo(pLoopThrottle);
}

void VDDubVideoProcessor::SetVideoStreamInfo(DubVideoStreamInfo *vinfo) {
	mpVInfo = vinfo;
}

void VDDubVideoProcessor::SetInputDisplay(IVDVideoDisplay *pVideoDisplay) {
	mpProcDisplay->SetInputDisplay(pVideoDisplay);
}

void VDDubVideoProcessor::SetOutputDisplay(IVDVideoDisplay *pVideoDisplay) {
	mpProcDisplay->SetOutputDisplay(pVideoDisplay);
}

// used in full processing
void VDDubVideoProcessor::SetVideoFilterOutput(const VDPixmapLayout& layout) {
	VDASSERT(!mpDisplayBufferTracker);
	mpDisplayBufferTracker = new VDRenderOutputBufferTracker;
	mpDisplayBufferTracker->AddRef();

	mpDisplayBufferTracker->Init(3 + mExtraOutputBuffersRequired, layout);
	bufferFormatEx = layout.formatEx;
}

// used in recompress
// note: old code referenced buffer from decoder directly (crazy)
void VDDubVideoProcessor::SetVideoDirectOutput(const VDPixmapLayout& layout) {
	VDASSERT(!mpFrameBufferTracker);
	mpFrameBufferTracker = new VDRenderOutputBufferTracker;
	mpFrameBufferTracker->AddRef();

	mpFrameBufferTracker->Init(1, layout);
	bufferFormatEx = layout.formatEx;
}

void VDDubVideoProcessor::SetBlitter(IVDAsyncBlitter *blitter) {
	mpProcDisplay->SetBlitter(blitter);
}

void VDDubVideoProcessor::SetVideoSources(IVDVideoSource *const *pVideoSources, uint32 count) {
	mVideoSources.assign(pVideoSources, pVideoSources + count);
	if (count)
		mpProcDisplay->SetVideoSource(pVideoSources[0]);
}

void VDDubVideoProcessor::SetVideoFrameSource(VDFilterFrameManualSource *fs) {
	mpVideoFrameSource = fs;
}

void VDDubVideoProcessor::SetVideoCompressor(IVDVideoCompressor *pCompressor, int threadCount) {
	VDASSERT(!mpThreadedVideoCompressor);

	mpVideoCompressor = pCompressor;

	if (pCompressor) {
		if (threadCount > 1 && !pCompressor->IsKeyFrameOnly()) {
			VDLogAppMessage(kVDLogInfo, kVDST_Dub, kVDM_ReducingCodecThreadCount);

			threadCount = 1;
		}

		if (threadCount > 1)
			mExtraOutputBuffersRequired += (threadCount - 1);

		mpThreadedVideoCompressor = new VDThreadedVideoCompressor;
		mpThreadedVideoCompressor->OnFrameComplete() += mThreadedCompressorDelegate(this, &VDDubVideoProcessor::OnAsyncCompressDone);

		mpThreadedVideoCompressor->Init(threadCount, pCompressor);
	}

	mpProcDisplay->SetVideoCompressor(pCompressor);
}

void VDDubVideoProcessor::SetVideoRequestQueue(VDDubFrameRequestQueue *q) {
	mpVideoRequestQueue = q;

	q->OnLowWatermark() += mVideoRequestQueueDelegate(this, &VDDubVideoProcessor::OnRequestQueueLowWatermark);
}

void VDDubVideoProcessor::SetVideoFrameMap(const VDRenderFrameMap *frameMap) {
	mpVideoFrameMap = frameMap;
}

void VDDubVideoProcessor::SetVideoFilterSystem(FilterSystem *fs) {
	mpVideoFilters = fs;

	if (fs && fs->IsThreadingActive()) {
		uint32 n = VDPreferencesGetVideoFilterProcessAheadCount();

		if (n)
			mFrameProcessAheadCount = n;
		else
			mFrameProcessAheadCount = fs->GetThreadCount();
	}
}

void VDDubVideoProcessor::SetVideoPipe(AVIPipe *pipe) {
	mpVideoPipe = pipe;

	pipe->OnBufferAdded() += mVideoPipeDelegate(this, &VDDubVideoProcessor::OnVideoPipeAdd);
}

void VDDubVideoProcessor::SetIODirect(VDDubIOThread *pIODirect) {
	mpIODirect = pIODirect;
}

void VDDubVideoProcessor::SetVideoOutput(IVDMediaOutputStream *out, bool enableImageOutput) {
	mpVideoOut = out;
	mpVideoImageOut = enableImageOutput ? vdpoly_cast<IVDVideoImageOutputStream *>(out) : NULL;
}

void VDDubVideoProcessor::SetPreviewClock(VDDubPreviewClock *clock) {
	if (mbPreview && mpOptions->perf.fDropFrames)
		clock->OnClockUpdated() += mPreviewClockDelegate(this, &VDDubVideoProcessor::OnClockUpdated);
}

void VDDubVideoProcessor::Init() {
	if (!mpDisplayBufferTracker && !mpFrameBufferTracker) {
		mpFrameBufferTracker = new VDRenderOutputBufferTracker;
		mpFrameBufferTracker->AddRef();

		IVDVideoSource *vs = mVideoSources[0];
		mpFrameBufferTracker->Init((void *)vs->getFrameBuffer(), vs->getTargetFormat());
	}
}

void VDDubVideoProcessor::PreShutdown() {
	// We have to ensure that the threaded compressor is stopped before we exit the processing thread.
	// If the UI thread ends up trying to stop, the result can be a deadlock when XviD attempts to
	// do a SendMessage() back to the UI thread from the worker thread.
	if (mpThreadedVideoCompressor)
		mpThreadedVideoCompressor->Shutdown();
}

void VDDubVideoProcessor::Shutdown() {
	PreShutdown();

	while(!mPendingSourceFrames.empty()) {
		SourceFrameEntry& srcEnt = mPendingSourceFrames.front();

		if (srcEnt.mpRequest)
			srcEnt.mpRequest->Release();

		mPendingSourceFrames.pop_front();
	}

	while(!mPendingOutputFrames.empty()) {
		OutputFrameEntry& outEnt = mPendingOutputFrames.front();

		if (outEnt.mpRequest)
			outEnt.mpRequest->Release();

		mPendingOutputFrames.pop_front();
	}

	if (mpThreadedVideoCompressor) {
		mpThreadedVideoCompressor->Shutdown();
		delete mpThreadedVideoCompressor;
		mpThreadedVideoCompressor = NULL;
	}

	mpHeldCompressionInputBuffer.clear();

	if (mpDisplayBufferTracker) {
		mpDisplayBufferTracker->Shutdown();
		mpDisplayBufferTracker->Release();
		mpDisplayBufferTracker = NULL;
	}

	if (mpFrameBufferTracker) {
		mpFrameBufferTracker->Shutdown();
		mpFrameBufferTracker->Release();
		mpFrameBufferTracker = NULL;
	}
}

void VDDubVideoProcessor::Abort() {
	ActivatePaths(kPath_Abort);
}

bool VDDubVideoProcessor::IsCompleted() const {
	return mbVideoEnded;
}

void VDDubVideoProcessor::CheckForDecompressorSwitch() {
	mpProcDisplay->CheckForDecompressorSwitch();
}

void VDDubVideoProcessor::UpdateFrames() {
	mpProcDisplay->ScheduleUpdate();
}

void VDDubVideoProcessor::PreDumpStatus(VDTextOutputStream& os) {
	os.FormatLine("Video push ended:      %s", mbVideoPushEnded ? "Yes" : "No");
	os.FormatLine("Video ended:           %s", mbVideoEnded ? "Yes" : "No");
	os.FormatLine("Flushing compressor:   %s", mbFlushingCompressor ? "Yes" : "No");
	os.FormatLine("Codec frames buffered: %u", mVideoFramesDelayed);

	os.PutLine();
	ActivatePaths(kPath_SuspendWake);
}

void VDDubVideoProcessor::DumpStatus(VDTextOutputStream& os) {
	if (mpVideoPipe) {
		os.PutLine("Video input pipe:");

		int active, finals, alloc;
		mpVideoPipe->getQueueInfo(active, finals, alloc);
		os.FormatLine("  %d/%d buffers active (%d non-preload frames)", active, alloc, finals);

		const VDRenderVideoPipeFrameInfo *nextRead = mpVideoPipe->TryReadBuffer();

		if (nextRead) {
			os.PutLine("  Next buffer:");

		os.FormatLine("    %d bytes | srcidx %d | stream %lld | display %lld | target %lld | flags:%s%s%s%s%s%s | droptype %d | final %d"
				, nextRead->mLength
				, nextRead->mSrcIndex
				, nextRead->mStreamFrame
				, nextRead->mDisplayFrame
				, nextRead->mTargetSample
				, nextRead->mFlags & nsVDDub::kBufferFlagDelta ? " delta" : ""
				, nextRead->mFlags & nsVDDub::kBufferFlagPreload ? " preload" : ""
				, nextRead->mFlags & nsVDDub::kBufferFlagDirectWrite ? " direct" : ""
				, nextRead->mFlags & nsVDDub::kBufferFlagSameAsLast ? " repeat" : ""
				, nextRead->mFlags & nsVDDub::kBufferFlagInternalDecode ? " intdec" : ""
				, nextRead->mFlags & nsVDDub::kBufferFlagFlushCodec ? " flush" : ""
				, nextRead->mDroptype
				, nextRead->mbFinal
				);
		} else {
			os.PutLine("  No buffers pending.");
		}

		os.PutLine();
	}

	os.FormatLine("Pending source frames: %u", mPendingSourceFrames.size());

	for(PendingSourceFrames::const_iterator it(mPendingSourceFrames.begin()), itEnd(mPendingSourceFrames.end()); it != itEnd; ++it) {
		const SourceFrameEntry& sfe = *it;

		os.FormatLine("  Frame %u | Batch %u | %s"
			, (unsigned)sfe.mSourceFrame
			, (unsigned)sfe.mBatchNumber
			, sfe.mpRequest->IsCompleted() ? sfe.mpRequest->IsSuccessful() ? "Succeeded" : "Failed" : "Pending"
			);
	}

	os.PutLine();

	os.FormatLine("Pending output frames: %u", mPendingOutputFrames.size());

	for(PendingOutputFrames::const_iterator it(mPendingOutputFrames.begin()), itEnd(mPendingOutputFrames.end()); it != itEnd; ++it) {
		const OutputFrameEntry& ofe = *it;

		os.FormatLine("  Frame %u | Batch %u | Hold %u | Null %u | Direct %u"
			, (unsigned)ofe.mTimelineFrame
			, (unsigned)ofe.mBatchNumber
			, ofe.mbHoldFrame
			, ofe.mbNullFrame
			, ofe.mbDirectFrame
			);
	}

	os.PutLine();

	if (mpVideoFilters) {
		os.PutLine("Video filter system");
		os.PutLine("-------------------");

		mpVideoFilters->DumpStatus(os);

		os.PutLine();
	}
}

bool VDDubVideoProcessor::WriteVideo() {
	VDDubAutoThreadLocation loc(*mppCurrentAction, "stepping to next video frame");

	// We cannot wrap the entire loop with a profiling event because typically
	// involves a nice wait in preview mode.
	for(;;) {
		uint32 activePaths = (mActivePaths |= mReactivatedPaths.xchg(0));

		if (activePaths & kPath_WriteAsyncCompressedFrame) {
			if (RunPathWriteAsyncCompressedFrame())
				break;

			continue;
		}

		if (activePaths & kPath_WriteOutputFrame) {
			if (RunPathWriteOutputFrames())
				break;
			continue;
		}

		if (activePaths & kPath_ProcessFilters) {
			RunPathProcessFilters();
			continue;
		}

		if (activePaths & kPath_RequestNewOutputFrames) {
			RunPathRequestNewOutputFrames();
			continue;
		}

		if (activePaths & kPath_SkipLatePreviewFrames) {
			RunPathSkipLatePreviewFrames();
			continue;
		}

		if (activePaths & kPath_FlushCompressor) {
			if (RunPathFlushCompressor())
				break;
			continue;
		}

		if (activePaths & kPath_ReadFrame) {
			if (RunPathReadFrame())
				break;
			continue;
		}

		if (activePaths & kPath_Abort)
			return false;

		if (activePaths & kPath_SuspendWake) {
			DeactivatePaths(kPath_SuspendWake);
		}

		// Uh oh, we have nothing to do. Let's wait.
		mpLoopThrottle->BeginWait();
		{
			VDDubAutoThreadLocation loc2(*mppCurrentAction, "waiting for video frame dependencies");
			mActivePathSignal.wait();
		}
		mpLoopThrottle->EndWait();
	}

	++mpVInfo->cur_proc_dst;

	return true;
}

void VDDubVideoProcessor::Reschedule() {
	ActivatePaths(kPath_ProcessFilters);
}

bool VDDubVideoProcessor::Block() {
	return false;
}

void VDDubVideoProcessor::ActivatePaths(uint32 path) {
	// Note that we don't set mActivePaths directly, instead setting a side variable that
	// is polled. The reason that we do this is to avoid race conditions where a path is
	// reactivated asynchronously while we are trying to reset it. Rather than convolute
	// every path to reset-before-set, we just split the variables instead.
	mReactivatedPaths |= path;
	mActivePathSignal.signal();
}

void VDDubVideoProcessor::DeactivatePaths(uint32 path) {
	mActivePaths &= ~path;
}

bool VDDubVideoProcessor::RunPathWriteOutputFrames() {
	VideoWriteResult result = kVideoWriteNoOutput;
	
	if (!mPendingOutputFrames.empty())
		result = ProcessVideoFrame();

	if (result == kVideoWriteDelayed)
		return false;

	if (result == kVideoWriteDiscarded || result == kVideoWriteOK)
		return true;

	if (result == kVideoWriteNoOutput) {
		DeactivatePaths(kPath_WriteOutputFrame);

		if (mbVideoPushEnded && mPendingOutputFrames.empty()) {
			// There are no more output frames to request. Check if we have delayed frames in the compressor.
			if (!mbFlushingCompressor) {
				if (mVideoFramesDelayed) {
					if (!mbFlushingCompressor) {
						mbFlushingCompressor = true;
						if (mpThreadedVideoCompressor)
							mpThreadedVideoCompressor->SetFlush(true, mpHeldCompressionInputBuffer);

						ActivatePaths(kPath_FlushCompressor);
					}
				} else {
					// We're done.
					mbVideoEnded = true;
					if (mpCB)
						mpCB->OnVideoStreamEnded();
					return true;
				}
			}
		}
	}

	return false;
}

void VDDubVideoProcessor::RunPathRequestNewOutputFrames() {
	DeactivatePaths(kPath_RequestNewOutputFrames);

	if (mbVideoPushEnded) {
		ActivatePaths(kPath_WriteOutputFrame);
		return;
	}

	// Try to request new frames.
	const uint32 vpsize = mpVideoPipe->size();
	bool didSomething = false;
	while(mpVideoRequestQueue->GetQueueLength() < vpsize && mPendingOutputFrames.size() < vpsize) {
		if (!RequestNextVideoFrame())
			break;

		didSomething = true;
	}

	if (didSomething)
		ActivatePaths(kPath_WriteOutputFrame | kPath_ReadFrame);
}

bool VDDubVideoProcessor::RunPathWriteAsyncCompressedFrame() {
	if (!mpThreadedVideoCompressor) {
		DeactivatePaths(kPath_WriteAsyncCompressedFrame);
		return false;
	}
	
	if (!CheckForThreadedCompressDone()) {
		DeactivatePaths(kPath_WriteAsyncCompressedFrame);
		return false;
	}

	VDVERIFY((sint32)--mVideoFramesDelayed >= 0);

	if (mbFlushingCompressor && !mVideoFramesDelayed) {
		mbFlushingCompressor = false;
		if (mpThreadedVideoCompressor)
			mpThreadedVideoCompressor->SetFlush(false, NULL);

		mpHeldCompressionInputBuffer.clear();
		ActivatePaths(kPath_WriteOutputFrame | kPath_ReadFrame);
	}

	return true;
}

void VDDubVideoProcessor::RunPathProcessFilters() {
	if (mPendingOutputFrames.empty() || mpOptions->video.mode != DubVideoOptions::M_FULL) {
		DeactivatePaths(kPath_ProcessFilters);
		return;
	}

	// Process frame to backbuffer for Full video mode.  Do not process if we are
	// running in Repack mode only!
	const OutputFrameEntry& nextOutputFrame = mPendingOutputFrames.front();

	VDDubAutoThreadLocation loc2(*mppCurrentAction, "running video filters");

	// Compute batch limit. Truncation is OK here, as the filter system is designed to handle
	// batch number wraparound.
	const uint32 batchLimit = (uint32)nextOutputFrame.mBatchNumber + mFrameProcessAheadCount;

	// process frame
	{
		VDPROFILESCOPE("V-Filter");

		mbFilterStageThrottled = false;
		switch(mpVideoFilters->Run(mFrameProcessAheadCount ? &batchLimit : NULL, false)) {
			case FilterSystem::kRunResult_Idle:
			case FilterSystem::kRunResult_Blocked:
				DeactivatePaths(kPath_ProcessFilters);
				break;

			case FilterSystem::kRunResult_BatchLimited:
				mbFilterStageThrottled = true;
				DeactivatePaths(kPath_ProcessFilters);
				break;

			case FilterSystem::kRunResult_Running:
				break;
		}
	}

	if (nextOutputFrame.mpRequest && nextOutputFrame.mpRequest->IsCompleted())
		ActivatePaths(kPath_WriteOutputFrame);
}

void VDDubVideoProcessor::RunPathSkipLatePreviewFrames() {
	DeactivatePaths(kPath_SkipLatePreviewFrames);

	if (!mbPreview || !mpOptions->perf.fDropFrames)
		return;

	PendingOutputFrames::iterator it(mPendingOutputFrames.begin()), itEnd(mPendingOutputFrames.end());
	if (it == itEnd)
		return;

	++it;

	uint32 clock = mpProcDisplay->GetDisplayClock();
	for(; it != itEnd; ++it) {
		OutputFrameEntry& ofe = *it;

		if (ofe.mbNullFrame)
			continue;

		bool protect = ofe.mTimelineFrame==mpVInfo->end_src-1; // skipping end frame is ugly
		if (protect)
			continue;

		sint32 delta = (sint32)((uint32)ofe.mTimelineFrame*2 - clock);

		if (delta >= 0)
			continue;

		ofe.mbNullFrame = true;
		ofe.mbHoldFrame = true;

		if (ofe.mpRequest) {
			ofe.mpRequest->Release();
			ofe.mpRequest = NULL;
		}
	}
}

bool VDDubVideoProcessor::RunPathFlushCompressor() {
	if (!mbFlushingCompressor) {
		DeactivatePaths(kPath_FlushCompressor);
		return false;
	}

	if (!mVideoFramesDelayed) {
		mbFlushingCompressor = false;
		if (mpThreadedVideoCompressor)
			mpThreadedVideoCompressor->SetFlush(false, NULL);
		DeactivatePaths(kPath_FlushCompressor);
		ActivatePaths(kPath_ReadFrame | kPath_WriteOutputFrame);
		return false;
	}

	if (mpThreadedVideoCompressor->IsAsynchronous()) {
		DeactivatePaths(kPath_FlushCompressor);
		return false;
	}

	// Keep pushing in frames until we clear the codec's queue. Note that we may not know
	// yet how deep the codec queue if we haven't pushed in enough frames to clear it yet.
	VideoWriteResult result = WriteFinishedVideoFrame(mpHeldCompressionInputBuffer, true);

	if (result == kVideoWriteDelayed) {
		// DivX 5.0.5 seems to have a bug where in the second pass of a multipass operation
		// it outputs an endless number of delay frames at the end!  This causes us to loop
		// infinitely trying to flush a codec delay that never ends.  Unfortunately, there is
		// one case where such a string of delay frames is valid: when the length of video
		// being compressed is shorter than the B-frame delay.  We attempt to detect when
		// this situation occurs and avert the loop.

		VDThreadedVideoCompressor::FlushStatus flushStatus = mpThreadedVideoCompressor->GetFlushStatus();

		if (flushStatus & VDThreadedVideoCompressor::kFlushStatusLoopingDetected) {
			int count = kReasonableBFrameBufferLimit;
			VDLogAppMessage(kVDLogWarning, kVDST_Dub, kVDM_CodecLoopingDuringDelayedFlush, 1, &count);
			result = kVideoWriteOK;

			// terminate loop
			mVideoFramesDelayed = 0;
		}
	} else {
		--mVideoFramesDelayed;
	}

	return true;
}

bool VDDubVideoProcessor::RunPathReadFrame() {
	if (mbFlushingCompressor) {
		DeactivatePaths(kPath_ReadFrame);
		return false;
	}

	const VDRenderVideoPipeFrameInfo *pFrameInfo = NULL;

	if (mpIODirect) {
		VDDubAutoThreadLocation loc(*mppCurrentAction, "reading video data");
		pFrameInfo = mpIODirect->SyncReadVideo();
	} else {
		VDDubAutoThreadLocation loc(*mppCurrentAction, "waiting for video frame from I/O thread");

		mpLoopThrottle->BeginWait();
		pFrameInfo = mpVideoPipe->TryReadBuffer();
		mpLoopThrottle->EndWait();
	}

	if (!pFrameInfo) {
		DeactivatePaths(kPath_ReadFrame);
		return false;
	}

	// Direct-mode path
	if (pFrameInfo->mFlags & kBufferFlagDirectWrite) {
		// First, check if we have a non-direct frame outstanding. If so, we have to stall until
		// that frame is complete.
		if (!mPendingOutputFrames.empty()) {
			const OutputFrameEntry& currentOutputFrame = mPendingOutputFrames.front();

			if (!currentOutputFrame.mbDirectFrame) {
				mbSourceStageThrottled = true;
				DeactivatePaths(kPath_ReadFrame);
				return false;
			}
		}

		// Check if we have frames buffered in the codec due to B-frame encoding. If we do,
		// we can't immediately switch from compression to direct copy for smart rendering
		// purposes -- we have to flush the B-frames first.
		if (mVideoFramesDelayed > 0) {
			mbFlushingCompressor = true;
			if (mpThreadedVideoCompressor)
				mpThreadedVideoCompressor->SetFlush(true, mpHeldCompressionInputBuffer);
			ActivatePaths(kPath_FlushCompressor);

			return false;
		}

		ReadDirectVideoFrame(*pFrameInfo);

		if (pFrameInfo)
			mpVideoPipe->releaseBuffer();

		return true;
	}

	// Decode path
	mbSourceStageThrottled = false;
	if (mFrameProcessAheadCount && !mPendingSourceFrames.empty() && !mPendingOutputFrames.empty()) {
		const SourceFrameEntry& sfe = mPendingSourceFrames.front();
		const OutputFrameEntry& ofe = mPendingOutputFrames.front();

		if ((sfe.mBatchNumber > ofe.mBatchNumber) && (sfe.mBatchNumber - ofe.mBatchNumber > mFrameProcessAheadCount)) {
			DeactivatePaths(kPath_ReadFrame);
			mbSourceStageThrottled = true;
			return false;
		}
	}

	VideoWriteResult result = ReadVideoFrame(*pFrameInfo);

	if (mbPreview && result == kVideoWriteOK) {
		if (mbFirstFrame) {
			mbFirstFrame = false;
			if (mpCB)
				mpCB->OnFirstFrameWritten();
		}
	}

	if (pFrameInfo)
		mpVideoPipe->releaseBuffer();

	if (result == kVideoWriteOK || result == kVideoWriteDiscarded)
		return true;

	return false;
}

void VDDubVideoProcessor::NotifyDroppedFrame(int exdata) {
	if (!(exdata & kBufferFlagPreload)) {
		mpProcDisplay->AdvanceFrame();

		if (mFramesToDrop)
			--mFramesToDrop;

		if (mpStatusHandler)
			mpStatusHandler->NotifyNewFrame(0, false);
	}
}

void VDDubVideoProcessor::NotifyCompletedFrame(uint32 bytes, bool isKey) {
	mpVInfo->lastProcessedTimestamp = VDGetCurrentTick();
	++mpVInfo->processed;

	mpProcDisplay->AdvanceFrame();

	if (mpStatusHandler)
		mpStatusHandler->NotifyNewFrame(bytes, isKey);
}

bool VDDubVideoProcessor::RequestNextVideoFrame() {
	if (mpVInfo->cur_dst >= mpVInfo->end_dst) {
		if (!mbVideoPushEnded) {
			mbVideoPushEnded = true;

			VDDubFrameRequest req;
			req.mbDirect = false;
			req.mSrcFrame = -1;

			mpVideoRequestQueue->AddRequest(req);

			// We have to reactivate the output frame path now since it may have
			// to do cleanup processing on its end, particularly if a threaded
			// compression queue is in use.
			ActivatePaths(kPath_WriteOutputFrame);
		}
		return false;
	}

	VDPosition timelinePos = mpVInfo->cur_dst++;
	bool drop = false;

	if (mbPreview && mpOptions->perf.fDropFrames && !mPendingOutputFrames.empty()) {
		uint32 clock = mpProcDisplay->GetDisplayClock();

		if ((sint32)(clock - (uint32)timelinePos*2) > 0) {
			drop = true;

			mPendingOutputFrames.back().mbHoldFrame = true;
		}
	}

	const VDRenderFrameMap::FrameEntry& frameEntry = (*mpVideoFrameMap)[timelinePos];
	bool protect = frameEntry.mTimelineFrame==mpVInfo->end_src-1; // skipping end frame is ugly
	if (protect) drop = false;

	if (frameEntry.mSourceFrame < 0 || drop) {
		OutputFrameEntry outputEntry;
		outputEntry.mTimelineFrame = frameEntry.mTimelineFrame;
		outputEntry.mBatchNumber = mBatchNumber++;
		outputEntry.mpRequest = NULL;
		outputEntry.mbHoldFrame = true;
		outputEntry.mbNullFrame = true;
		outputEntry.mbDirectFrame = false;

		mPendingOutputFrames.push_back(outputEntry);
		return true;
	}

	vdrefptr<IVDFilterFrameClientRequest> outputReq;
	VDDubFrameRequest req;
	bool activateFilterPath = false;

	if (frameEntry.mbDirect) {
		req.mbDirect = true;
		req.mSrcFrame = frameEntry.mSourceFrame;

		mpVideoRequestQueue->AddRequest(req);
	} else if (mpVideoFilters) {
		if (mpOptions->video.mode == DubVideoOptions::M_FULL)
			mpVideoFilters->RequestFrame(frameEntry.mSourceFrame, mBatchNumber, ~outputReq);
		else
			mpVideoFrameSource->CreateRequest(frameEntry.mSourceFrame, false, mBatchNumber, ~outputReq);

		vdrefptr<VDFilterFrameRequest> freq;
		bool addedAnySources = false;

		while(mpVideoFrameSource->GetNextRequest(NULL, ~freq)) {
			const VDFilterFrameRequestTiming& timing = freq->GetTiming();

			req.mSrcFrame = timing.mOutputFrame;
			req.mbDirect = false;
			mpVideoRequestQueue->AddRequest(req);

			SourceFrameEntry srcEnt;
			srcEnt.mpRequest = freq;
			srcEnt.mSourceFrame = req.mSrcFrame;
			srcEnt.mBatchNumber = freq->GetBatchNumber();
			mPendingSourceFrames.push_back(srcEnt);
			freq.release();
			addedAnySources = true;
		}

		if (!addedAnySources && mPendingOutputFrames.empty())
			activateFilterPath = true;
	} else {
		req.mbDirect = false;
		req.mSrcFrame = frameEntry.mSourceFrame;

		mpVideoRequestQueue->AddRequest(req);
	}

	OutputFrameEntry outputEntry;
	outputEntry.mTimelineFrame = frameEntry.mTimelineFrame;
	outputEntry.mBatchNumber = mBatchNumber++;
	outputEntry.mpRequest = outputReq;
	outputEntry.mbHoldFrame = frameEntry.mbHoldFrame;
	outputEntry.mbNullFrame = false;
	outputEntry.mbDirectFrame = frameEntry.mbDirect;

	mPendingOutputFrames.push_back(outputEntry);
	outputReq.release();

	if (activateFilterPath)
		ActivatePaths(kPath_ProcessFilters | kPath_WriteOutputFrame);

	return true;
}

void VDDubVideoProcessor::RemoveCompletedOutputFrame() {
	mPendingOutputFrames.pop_front();

	uint32 pathsToActivate = kPath_RequestNewOutputFrames;

	if (mbSourceStageThrottled)
		pathsToActivate |= kPath_ReadFrame;

	if (mbFilterStageThrottled)
		pathsToActivate |= kPath_ProcessFilters;

	ActivatePaths(pathsToActivate);
}

bool VDDubVideoProcessor::DoVideoFrameDropTest(const VDRenderVideoPipeFrameInfo& frameInfo) {
	return false;
}

void VDDubVideoProcessor::ReadDirectVideoFrame(const VDRenderVideoPipeFrameInfo& frameInfo) {
	VDDubAutoThreadLocation loc2(*mppCurrentAction, "reading video frame");

	const void			*buffer				= frameInfo.mpData;
	const int			exdata				= frameInfo.mFlags;
	const uint32		lastSize			= frameInfo.mLength;

	VDASSERT(!mPendingOutputFrames.empty());

	const OutputFrameEntry& nextOutputFrame = mPendingOutputFrames.front();
	VDASSERT(!nextOutputFrame.mpRequest);
	VDASSERT(nextOutputFrame.mbDirectFrame);

	mpVInfo->cur_proc_src = nextOutputFrame.mTimelineFrame;

	RemoveCompletedOutputFrame();

	uint32 flags = (exdata & kBufferFlagDelta) || !(exdata & kBufferFlagDirectWrite) ? 0 : AVIOutputStream::kFlagKeyFrame;
	mpVideoOut->write(flags, (char *)buffer, lastSize, 1);

	mpVInfo->total_size += lastSize + 24;
	mpVInfo->lastProcessedTimestamp = VDGetCurrentTick();
	++mpVInfo->processed;
	if (mpStatusHandler)
		mpStatusHandler->NotifyNewFrame(lastSize, !(exdata & kBufferFlagDelta));

	if (mpThreadedVideoCompressor) {
		mpThreadedVideoCompressor->SkipFrame();
		mpThreadedVideoCompressor->Restart();
	}
}

VDDubVideoProcessor::VideoWriteResult VDDubVideoProcessor::ReadVideoFrame(const VDRenderVideoPipeFrameInfo& frameInfo) {
	VDDubAutoThreadLocation loc2(*mppCurrentAction, "reading video frame");

	const int			exdata				= frameInfo.mFlags;
	const int			srcIndex			= frameInfo.mSrcIndex;

	VDASSERT(!(exdata & kBufferFlagDirectWrite));

	IVDVideoSource *vsrc = mVideoSources[srcIndex];

	if (mbPreview) {
		if (DoVideoFrameDropTest(frameInfo)) {
			NotifyDroppedFrame(exdata);
			return kVideoWriteDiscarded;
		}
	}

	// Repack:      Decompress data and send to compressor.
	// Full:		Decompress, process, filter, convert, send to compressor.

	vdrefptr<VDRenderOutputBuffer> pBuffer;
	if (mpFrameBufferTracker) {
		// If we are using the frame buffer tracker, we need to make sure we allocate the frame buffer
		// before we begin decoding.
		VDDubVideoProcessor::VideoWriteResult getFBResult = GetVideoOutputBuffer(~pBuffer);
		if (getFBResult != kVideoWriteOK)
			return getFBResult;
	}

	VDDubVideoProcessor::VideoWriteResult decodeResult = DecodeVideoFrame(frameInfo);

	if (!(exdata & kBufferFlagPreload)) {
		if (!mPendingSourceFrames.empty()) {
			const SourceFrameEntry& sfe = mPendingSourceFrames.front();

			VDASSERT(sfe.mSourceFrame == frameInfo.mDisplayFrame);
			if (sfe.mpRequest) {
				mpVideoFrameSource->AllocateRequestBuffer(sfe.mpRequest);
				VDFilterFrameBuffer *buf = sfe.mpRequest->GetResultBuffer();
				const VDPixmap pxdst(VDPixmapFromLayout(mpVideoFilters->GetInputLayout(), buf->LockWrite()));
				const VDPixmap& pxsrc = vsrc->getTargetFormat();

				if (!mpInputBlitter)
					mpInputBlitter = VDPixmapCreateBlitter(pxdst, pxsrc);

				VDPROFILEBEGINEX2("Dub Loop", (uint32)frameInfo.mDisplayFrame, vdprofiler_flag_loop);

				VDPROFILEBEGINEX3("V-BlitIn", (uint32)frameInfo.mDisplayFrame, 0, mpInputBlitter->profiler_comment.c_str());
				mpInputBlitter->Blit(pxdst, pxsrc);
				buf->info = pxdst.info;
				VDPROFILEEND();

				buf->Unlock();

				mpProcDisplay->UnlockInputChannel();

				sfe.mpRequest->MarkComplete(true);
				mpVideoFrameSource->CompleteRequest(sfe.mpRequest, true);
				sfe.mpRequest->Release();

				ActivatePaths(kPath_ProcessFilters | kPath_WriteOutputFrame);
			}

			mPendingSourceFrames.pop_front();
		}
	}

	if (decodeResult != kVideoWriteOK)
		return decodeResult;

	// drop frames if the drop counter is >0
	if (mFramesToDrop && mbPreview) {
		mpProcDisplay->UnlockInputChannel();

		NotifyDroppedFrame(exdata);
		return kVideoWriteDiscarded;
	}

	if (mpVideoFilters)
		return kVideoWriteNoOutput;

	if (!pBuffer) {
		VDDubVideoProcessor::VideoWriteResult getOutputResult = GetVideoOutputBuffer(~pBuffer);
		if (getOutputResult != kVideoWriteOK)
			return getOutputResult;
	}

	VDASSERT(!mPendingOutputFrames.empty());

	const OutputFrameEntry& nextOutputFrame = mPendingOutputFrames.front();
	VDASSERT(!nextOutputFrame.mpRequest);

	const VDPixmap& pxsrc = vsrc->getTargetFormat();
	if (!mpOutputBlitter)
		mpOutputBlitter = VDPixmapCreateBlitter(pBuffer->mPixmap, pxsrc);

	VDPROFILEBEGINEX3("V-BlitOut",0,0,mpOutputBlitter->profiler_comment.c_str());
	mpOutputBlitter->Blit(pBuffer->mPixmap, pxsrc);
	VDPROFILEEND();

	mpVInfo->cur_proc_src = nextOutputFrame.mTimelineFrame;

	bool holdFrame = nextOutputFrame.mbHoldFrame;
	RemoveCompletedOutputFrame();

	return WriteFinishedVideoFrame(pBuffer, holdFrame);
}

VDDubVideoProcessor::VideoWriteResult VDDubVideoProcessor::GetVideoOutputBuffer(VDRenderOutputBuffer **ppBuffer) {
	vdrefptr<VDRenderOutputBuffer> pBuffer;

	VDPROFILEBEGINEX2("V-Lock2",0,vdprofiler_flag_wait);
	mpLoopThrottle->BeginWait();
	for(;;) {
		if (mpProcDisplay->TryRevokeOutputBuffer(~pBuffer))
			break;

		bool successful = mpDisplayBufferTracker ? mpDisplayBufferTracker->AllocFrame(100, ~pBuffer) : mpFrameBufferTracker->AllocFrame(100, ~pBuffer);

		if ((mActivePaths | mReactivatedPaths) & kPath_Abort)
			break;

		if (successful)
			break;
	}
	mpLoopThrottle->EndWait();
	VDPROFILEEND();

	if (!pBuffer)
		return kVideoWriteDiscarded;

	*ppBuffer = pBuffer.release();
	return kVideoWriteOK;
}

bool VDDubVideoProcessor::CheckForThreadedCompressDone() {
	if (!mpThreadedVideoCompressor)
		return false;

	VDDubAutoThreadLocation loc2(*mppCurrentAction, "pulling compressed video frame");

	vdrefptr<VDRenderPostCompressionBuffer> pOutBuffer;
	if (!mpThreadedVideoCompressor->ExchangeBuffer(NULL, ~pOutBuffer))
		return false;

	WriteFinishedVideoFrame(pOutBuffer->mOutputBuffer.data(), pOutBuffer->mOutputSize, pOutBuffer->packetInfo, false, NULL);
	return true;
}

VDDubVideoProcessor::VideoWriteResult VDDubVideoProcessor::ProcessVideoFrame() {
	VDDubAutoThreadLocation loc(*mppCurrentAction, "processing video frame");

	vdrefptr<VDRenderOutputBuffer> pBuffer;

	// Bail if the next frame is an direct frame. We don't handle these here.
	const OutputFrameEntry& nextOutputFrame = mPendingOutputFrames.front();
	if (nextOutputFrame.mbDirectFrame)
		return kVideoWriteNoOutput;

	// If the next frame is a null frame, write it and go onto the next.
	if (nextOutputFrame.mbNullFrame) {
		WriteNullVideoFrame();
		RemoveCompletedOutputFrame();
		return kVideoWriteOK;
	}

	IVDFilterFrameClientRequest *pOutputReq = nextOutputFrame.mpRequest;

	if (!pOutputReq || !pOutputReq->IsCompleted())
		return kVideoWriteNoOutput;

	if (!pOutputReq->IsSuccessful()) {
		const VDFilterFrameRequestError *err = pOutputReq->GetError();

		if (err)
			throw MyError("%s", err->mError.c_str());
		else
			throw MyError("An unknown error occurred during video filtering.");
	}

	VDDubVideoProcessor::VideoWriteResult getOutputResult = GetVideoOutputBuffer(~pBuffer);
	if (getOutputResult != kVideoWriteOK)
		return getOutputResult;

	pBuffer->mTimelineFrame = nextOutputFrame.mTimelineFrame;
	const VDPixmapLayout& layout = (mpOptions->video.mode == DubVideoOptions::M_FULL) ? mpVideoFilters->GetOutputLayout() : mpVideoFilters->GetInputLayout();
	VDFilterFrameBuffer *buf = pOutputReq->GetResultBuffer();
	VDPixmap pxsrc = VDPixmapFromLayout(layout, (void *)buf->LockRead());
	pxsrc.info = buf->info;
	pxsrc.info.frame_num = nextOutputFrame.mTimelineFrame;
	if (bufferFormatEx) {
		pBuffer->mPixmap.format = bufferFormatEx.format;
		pBuffer->mPixmap.info.colorSpaceMode = bufferFormatEx.colorSpaceMode;
		pBuffer->mPixmap.info.colorRangeMode = bufferFormatEx.colorRangeMode;
	}
	if (!mpOutputBlitter && !mbPreview) {
		FilterModPixmapInfo out_info = pBuffer->mPixmap.info;
		VDSetPixmapInfoForBitmap(out_info, pBuffer->mPixmap.format);

		if (mpVideoCompressor) {
			if (mpVideoCompressor->GetInputFormat(&out_info)) {
				VDAdjustPixmapInfoForRange(out_info, pBuffer->mPixmap.format);
			} else {
				vdstructex<tagBITMAPINFOHEADER> bm;
				mpVideoCompressor->GetInputBitmapFormat(bm);
				int variant;
				VDBitmapFormatToPixmapFormat((VDAVIBitmapInfoHeader&)*bm.data(),variant);
				VDSetPixmapInfoForBitmap(out_info, pBuffer->mPixmap.format, variant);
			}
		}

		IVDPixmapExtraGen* extraDst = VDPixmapCreateNormalizer(pBuffer->mPixmap.format, out_info);
		mpOutputBlitter = VDPixmapCreateBlitter(pBuffer->mPixmap, pxsrc, extraDst);
		delete extraDst;
	} else if(!mpOutputBlitter) {
		mpOutputBlitter = VDPixmapCreateBlitter(pBuffer->mPixmap, pxsrc);
	}

	VDPROFILEBEGINEX3("V-BlitOut",(uint32)nextOutputFrame.mTimelineFrame,0,mpOutputBlitter->profiler_comment.c_str());
	mpOutputBlitter->Blit(pBuffer->mPixmap, pxsrc);
	buf->Unlock();
	VDPROFILEEND();

	if (!mbPreview)
		mpStatusHandler->NotifyPositionChange(nextOutputFrame.mTimelineFrame);

	pOutputReq->Release();

	bool holdFrame = nextOutputFrame.mbHoldFrame;
	RemoveCompletedOutputFrame();

	return WriteFinishedVideoFrame(pBuffer, holdFrame);
}

VDDubVideoProcessor::VideoWriteResult VDDubVideoProcessor::WriteFinishedVideoFrame(VDRenderOutputBuffer *pBuffer, bool holdBuffer) {
	// write it to the file	
	const void *frameBuffer = pBuffer->mpBase;

	vdrefptr<VDRenderPostCompressionBuffer> pNewBuffer;
	uint32 dwBytes;
	VDPacketInfo packetInfo;

	if (mpVideoCompressor) {
		bool gotFrame;

		if (holdBuffer)
			mpHeldCompressionInputBuffer = pBuffer;

		VDPROFILEBEGIN("V-Pack");
		{
			VDDubAutoThreadLocation loc(*mppCurrentAction, "compressing video frame");
			gotFrame = mpThreadedVideoCompressor->ExchangeBuffer(pBuffer, ~pNewBuffer);
		}
		VDPROFILEEND();

		// Check if codec buffered a frame.
		if (!gotFrame) {
			if (!mbFlushingCompressor)
				++mVideoFramesDelayed;

			return kVideoWriteDelayed;
		}

		// If we don't have frames queued up, then release the held frame, as we don't need it
		// anymore. This is important to keep fast recompress from locking up, since only one
		// FB is available.
		if (!mVideoFramesDelayed)
			mpHeldCompressionInputBuffer.clear();

		frameBuffer = pNewBuffer->mOutputBuffer.data();
		dwBytes = pNewBuffer->mOutputSize;
		packetInfo = pNewBuffer->packetInfo;

	} else {

		dwBytes = pBuffer->mBuffer.size();
		packetInfo.keyframe = true;
	}

	WriteFinishedVideoFrame(frameBuffer, dwBytes, packetInfo, true, pBuffer);
	return kVideoWriteOK;
}

void VDDubVideoProcessor::WriteNullVideoFrame() {
	VDPacketInfo packetInfo;
	packetInfo.keyframe = false;
	WriteFinishedVideoFrame(NULL, 0, packetInfo, false, NULL);
}

void VDDubVideoProcessor::WriteFinishedVideoFrame(const void *data, uint32 size, VDPacketInfo& packetInfo, bool renderEnabled, VDRenderOutputBuffer *pBuffer) {
	if (mpVideoCompressor) {
		mpProcDisplay->UpdateDecompressedVideo(data, size, packetInfo.keyframe);
	}

	////// WRITE VIDEO FRAME TO DISK
	if (mpVideoImageOut) {
		VDDubAutoThreadLocation loc(*mppCurrentAction, "writing video frame to disk");
		if (data)
			mpVideoImageOut->WriteVideoImage(&pBuffer->mPixmap);
		else
			mpVideoImageOut->WriteVideoImage(NULL);
	} else if (mpVideoOut) {
		VDDubAutoThreadLocation loc(*mppCurrentAction, "writing video frame to disk");
		IVDXOutputFile::PacketInfo packetInfo2;
		packetInfo2.flags = packetInfo.keyframe ? AVIOutputStream::kFlagKeyFrame : 0;
		packetInfo2.pts = packetInfo.pts;
		packetInfo2.dts = packetInfo.dts;
		packetInfo2.samples = 1;
		mpVideoOut->write((char *)data, size, packetInfo2, &pBuffer->mPixmap.info);
	}
	mpVInfo->total_size += size + 24;

	////// RENDERING

	if (renderEnabled) {
		VDPROFILEBEGINEX2("V-Lock3",0,vdprofiler_flag_wait);
		bool bLockSuccessful;

		mpLoopThrottle->BeginWait();
		do {
			bLockSuccessful = mpProcDisplay->TryLockInputChannel(mbPreview ? 500 : -1);
		} while(!bLockSuccessful && !((mActivePaths | mReactivatedPaths) & kPath_Abort));
		mpLoopThrottle->EndWait();
		VDPROFILEEND();

		bool protect = mbShowLast && (pBuffer->mTimelineFrame==mpVInfo->end_src-1);
		mpProcDisplay->UnlockAndDisplay(mbPreview || protect, pBuffer, size != 0);
	}

	if (mpOptions->perf.fDropFrames && mbPreview) {
		long lFrameDelta;

		lFrameDelta = mpProcDisplay->GetLatency() / 2;

		if (lFrameDelta < 0) lFrameDelta = 0;
		
		if (lFrameDelta > 0) {
			mFramesToDrop = lFrameDelta;
		}
	}

	NotifyCompletedFrame(size, packetInfo.keyframe);
}

VDDubVideoProcessor::VideoWriteResult VDDubVideoProcessor::DecodeVideoFrame(const VDRenderVideoPipeFrameInfo& frameInfo) {
	const void *buffer = frameInfo.mpData;
	const int exdata = frameInfo.mFlags;
	const int srcIndex = frameInfo.mSrcIndex;
	const VDPosition sample_num = frameInfo.mStreamFrame;
	const VDPosition target_num = frameInfo.mTargetSample;

	IVDVideoSource *vsrc = mVideoSources[srcIndex];

	VDPROFILEBEGINEX2("V-Lock1",0,vdprofiler_flag_wait);
	bool bLockSuccessful;

	mpLoopThrottle->BeginWait();
	do {
		bLockSuccessful = mpProcDisplay->TryLockInputChannel(mbPreview ? 500 : -1);
	} while(!bLockSuccessful && !((mActivePaths | mReactivatedPaths) & kPath_Abort));
	mpLoopThrottle->EndWait();
	VDPROFILEEND();

	if (!bLockSuccessful)
		return kVideoWriteDiscarded;

	mpProcDisplay->UnlockInputChannel();

	///// DECODE FRAME

	if (exdata & kBufferFlagPreload) {
		VDPROFILEBEGINEX("V-Preload", (uint32)target_num);
	} else {
		VDPROFILEBEGINEX("V-Decode", (uint32)target_num);
	}

	if (!(exdata & kBufferFlagFlushCodec)) {
		VDDubAutoThreadLocation loc(*mppCurrentAction, "decompressing video frame");
		vsrc->streamGetFrame(buffer, frameInfo.mLength, 0 != (exdata & kBufferFlagPreload), sample_num, target_num);
	}

	VDPROFILEEND();

	if (exdata & kBufferFlagPreload) {
		mpProcDisplay->UnlockInputChannel();
		return kVideoWriteNoOutput;
	}

	return kVideoWriteOK;
}

void VDDubVideoProcessor::OnVideoPipeAdd(AVIPipe *pipe, const bool&) {
	ActivatePaths(kPath_ReadFrame);
}

void VDDubVideoProcessor::OnRequestQueueLowWatermark(VDDubFrameRequestQueue *queue, const bool&) {
	ActivatePaths(kPath_RequestNewOutputFrames);
}

void VDDubVideoProcessor::OnAsyncCompressDone(VDThreadedVideoCompressor *compressor, const bool&) {
	ActivatePaths(kPath_WriteAsyncCompressedFrame);
}

void VDDubVideoProcessor::OnClockUpdated(VDDubPreviewClock *clock, const uint32& val) {
	ActivatePaths(kPath_SkipLatePreviewFrames);
}

//-------------------------------------------------------------------------------------------------------------
// overview
/*

RequestNextVideoFrame
Output frame requested from filter instance:
request is added instance internal list, (multiple) source requests are made from upstream filters and so on,
finally video source request list is populated.
If filters are bypassed it is the same end result.

ReadVideoFrame
Pick requests from video source and execute them.
Marking requests as completed allows dependent filter instances to continue.
How the hell mPendingSourceFrames and VDRenderVideoPipeFrameInfo relate?
mPendingSourceFrames -> mpVideoRequestQueue -> mpVideoPipe -> pFrameInfo

*/

