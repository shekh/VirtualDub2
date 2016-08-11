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

#ifndef f_VD2_DUBPROCESSVIDEO_H
#define f_VD2_DUBPROCESSVIDEO_H

#include <vd2/Kasumi/blitter.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Riza/videocodec.h>
#include "FilterSystem.h"

class DubOptions;
struct VDRenderVideoPipeFrameInfo;
class VDRenderOutputBuffer;
class IVDAsyncBlitter;
class IDubStatusHandler;
class DubVideoStreamInfo;
class VDDubFrameRequestQueue;
class VDRenderFrameMap;
class VDThreadedVideoCompressor;
class IVDMediaOutputStream;
class IVDVideoImageOutputStream;
class VDLoopThrottle;
class VDRenderOutputBufferTracker;
class IVDVideoDisplay;
class IVDVideoSource;
struct VDPixmapLayout;
class VDFilterFrameManualSource;
class IVDVideoCompressor;
class FilterSystem;
class AVIPipe;
class VDFilterFrameRequest;
class IVDFilterFrameClientRequest;
class VDDubVideoProcessorDisplay;
class VDDubPreviewClock;
class VDTextOutputStream;

class IVDDubVideoProcessorCallback {
public:
	virtual void OnFirstFrameWritten() = 0;
	virtual void OnVideoStreamEnded() = 0;
};

class VDDubVideoProcessor : public IVDFilterSystemScheduler {
	VDDubVideoProcessor(const VDDubVideoProcessor&);
	VDDubVideoProcessor& operator=(const VDDubVideoProcessor&);
public:
	enum VideoWriteResult {
		kVideoWriteOK,							// Frame was processed and written
		kVideoWriteDelayed,						// Codec received intermediate frame; no output.
		kVideoWriteNoOutput,					// No output.
		kVideoWriteDiscarded					// Frame was discarded by preview QC.
	};

	VDDubVideoProcessor();
	~VDDubVideoProcessor();

	int AddRef();
	int Release();

	void SetCallback(IVDDubVideoProcessorCallback *cb);
	void SetStatusHandler(IDubStatusHandler *handler);
	void SetOptions(const DubOptions *opts);
	void SetThreadSignals(const char *volatile *pStatus, VDLoopThrottle *pLoopThrottle);
	void SetVideoStreamInfo(DubVideoStreamInfo *vinfo);
	void SetPreview(bool preview);
	void SetInputDisplay(IVDVideoDisplay *pVideoDisplay);
	void SetOutputDisplay(IVDVideoDisplay *pVideoDisplay);
	void SetVideoFilterOutput(const VDPixmapLayout& layout);
	void SetBlitter(IVDAsyncBlitter *blitter);
	void SetVideoSources(IVDVideoSource *const *pVideoSources, uint32 count);
	void SetVideoFrameSource(VDFilterFrameManualSource *fs);
	void SetVideoCompressor(IVDVideoCompressor *pCompressor, int threadCount);
	void SetVideoFrameMap(const VDRenderFrameMap *frameMap);
	void SetVideoRequestQueue(VDDubFrameRequestQueue *q);
	void SetVideoFilterSystem(FilterSystem *fs);
	void SetVideoPipe(AVIPipe *pipe);
	void SetVideoOutput(IVDMediaOutputStream *out, bool enableImageOutput);
	void SetPreviewClock(VDDubPreviewClock *clock);

	void SetPriority(int priority);

	void PreInit();
	void Init();
	void PreShutdown();
	void Shutdown();

	void Abort();

	bool IsCompleted() const;

	void CheckForDecompressorSwitch();
	void UpdateFrames();

	void PreDumpStatus(VDTextOutputStream& os);
	void DumpStatus(VDTextOutputStream& os);

	bool WriteVideo();

public:
	void Reschedule();
	bool Block();

protected:
	void ActivatePaths(uint32 path);
	void DeactivatePaths(uint32 path);

	bool RunPathWriteOutputFrames();
	void RunPathRequestNewOutputFrames();
	bool RunPathWriteAsyncCompressedFrame();
	void RunPathProcessFilters();
	void RunPathSkipLatePreviewFrames();
	bool RunPathFlushCompressor();
	bool RunPathReadFrame();

	void NotifyDroppedFrame(int exdata);
	void NotifyCompletedFrame(uint32 size, bool isKey);

	bool RequestNextVideoFrame();
	void RemoveCompletedOutputFrame();

	bool DoVideoFrameDropTest(const VDRenderVideoPipeFrameInfo& frameInfo);
	void ReadDirectVideoFrame(const VDRenderVideoPipeFrameInfo& frameInfo);
	VideoWriteResult ReadVideoFrame(const VDRenderVideoPipeFrameInfo& frameInfo);
	VideoWriteResult DecodeVideoFrame(const VDRenderVideoPipeFrameInfo& frameInfo);
	VideoWriteResult GetVideoOutputBuffer(VDRenderOutputBuffer **ppBuffer);
	bool CheckForThreadedCompressDone();
	VideoWriteResult ProcessVideoFrame();
	VideoWriteResult WriteFinishedVideoFrame(VDRenderOutputBuffer *pBuffer, bool holdFrame);
	void WriteNullVideoFrame();
	void WriteFinishedVideoFrame(const void *data, uint32 size, bool isKey, bool renderEnabled, VDRenderOutputBuffer *pBuffer);

	void OnVideoPipeAdd(AVIPipe *pipe, const bool&);
	void OnRequestQueueLowWatermark(VDDubFrameRequestQueue *queue, const bool&);
	void OnAsyncCompressDone(VDThreadedVideoCompressor *compressor, const bool&);
	void OnClockUpdated(VDDubPreviewClock *clock, const uint32& val);

	enum {
		kPath_WriteOutputFrame			= 0x00000001,
		kPath_RequestNewOutputFrames	= 0x00000002,
		kPath_WriteAsyncCompressedFrame	= 0x00000004,
		kPath_ProcessFilters			= 0x00000008,
		kPath_SkipLatePreviewFrames		= 0x00000010,
		kPath_FlushCompressor			= 0x00000020,
		kPath_ReadFrame					= 0x00000040,
		kPath_Abort						= 0x00000080,
		kPath_SuspendWake				= 0x00000100,
		kPathInitial					= 0x0000007F
	};

	uint32				mActivePaths;
	VDAtomicInt			mReactivatedPaths;
	VDSignal			mActivePathSignal;

	// status
	const DubOptions	*mpOptions;
	IDubStatusHandler	*mpStatusHandler;
	DubVideoStreamInfo	*mpVInfo;
	bool				mbVideoPushEnded;
	bool				mbVideoEnded;
	bool				mbPreview;
	bool				mbFirstFrame;
	VDAtomicInt			*mpAbort;
	const char			*volatile *mppCurrentAction;
	IVDDubVideoProcessorCallback	*mpCB;

	// video sourcing
	const VDRenderFrameMap	*mpVideoFrameMap;
	VDFilterFrameManualSource	*mpVideoFrameSource;
	VDDubFrameRequestQueue	*mpVideoRequestQueue;
	AVIPipe					*mpVideoPipe;
	vdautoptr<IVDPixmapBlitter>	mpInputBlitter;
	bool mbSourceStageThrottled;

	// video decompression
	typedef vdfastvector<IVDVideoSource *> VideoSources;
	VideoSources		mVideoSources;

	// video filtering
	FilterSystem		*mpVideoFilters;
	uint32				mFrameProcessAheadCount;
	uint32				mBatchNumber;
	bool mbFilterStageThrottled;

	// video output conversion
	vdautoptr<IVDPixmapBlitter>	mpOutputBlitter;
	VDPixmapBuffer repack_buffer;

	// video compression
	vdrefptr<VDRenderOutputBuffer> mpHeldCompressionInputBuffer;
	IVDVideoCompressor	*mpVideoCompressor;
	VDThreadedVideoCompressor *mpThreadedVideoCompressor;
	uint32				mVideoFramesDelayed;
	bool				mbFlushingCompressor;

	// video output
	IVDMediaOutputStream	*mpVideoOut;			// alias: AVIout->videoOut
	IVDVideoImageOutputStream	*mpVideoImageOut;	// alias: AVIout->videoOut
	uint32				mExtraOutputBuffersRequired;

	struct SourceFrameEntry {
		VDFilterFrameRequest *mpRequest;
		VDPosition mSourceFrame;
		uint32 mBatchNumber;
	};

	typedef vdfastdeque<SourceFrameEntry> PendingSourceFrames;
	PendingSourceFrames	mPendingSourceFrames;

	struct OutputFrameEntry {
		IVDFilterFrameClientRequest *mpRequest;
		VDPosition mTimelineFrame;
		uint32 mBatchNumber;
		bool mbHoldFrame;
		bool mbNullFrame;
		bool mbDirectFrame;
	};

	typedef vdfastdeque<OutputFrameEntry> PendingOutputFrames;
	PendingOutputFrames	mPendingOutputFrames;

	VDLoopThrottle		*mpLoopThrottle;

	VDRenderOutputBufferTracker *mpFrameBufferTracker;
	VDRenderOutputBufferTracker *mpDisplayBufferTracker;
	VDPixmapFormatEx bufferFormatEx;

	uint32				mFramesToDrop;
	VDDubVideoProcessorDisplay	*mpProcDisplay;

	int mThreadPriority;

	VDDelegate	mVideoRequestQueueDelegate;
	VDDelegate	mVideoPipeDelegate;
	VDDelegate	mThreadedCompressorDelegate;
	VDDelegate	mPreviewClockDelegate;
};

#endif	// f_VD2_DUBPROCESSVIDEO_H
