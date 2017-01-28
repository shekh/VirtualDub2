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
#include <vd2/system/profile.h>
#include <vd2/system/VDRingBuffer.h>
#include <vd2/system/log.h>
#include "DubIO.h"
#include "Dub.h"
#include "DubUtils.h"
#include "VideoSource.h"
#include "Audio.h"
#include "AVIPipe.h"
#include "DubFrameRequestQueue.h"

using namespace nsVDDub;

bool VDPreferencesIsRenderNoAudioWarningEnabled();

VDDubIOThread::VDDubIOThread(
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
							 )
	: VDThread("Dub-I/O")
	, mpParent(pParent)
	, mbError(false)
	, mbPreview(preview)
	, mAudioSamplesWritten(0)
	, mVideoRequestTargetSample(0)
	, mbVideoRequestActive(false)
	, mbVideoRequestFirstSample(false)
	, mpVideoRequestSource(NULL)
	, mVideoSources(videoSources)
	, mpAudio(pAudio)
	, mpVideoPipe(pVideoPipe)
	, mpAudioPipe(pAudioPipe)
	, aInfo(_aInfo)
	, vInfo(_vInfo)
	, mThreadCounter(threadCounter)
	, mpVideoRequestQueue(videoRequestQueue)
	, mbAbort(false)
	, mpCurrentAction("starting up")
{
}

VDDubIOThread::~VDDubIOThread() {
}

void VDDubIOThread::SetThrottle(float f) {
	mLoopThrottle.SetThrottleFactor(f);
}

void VDDubIOThread::Abort() {
	mbAbort = true;
	mAbortSignal.signal();
}

void VDDubIOThread::ThreadRun() {
	bool bAudioActive = mpAudioPipe && (mpAudio != 0);
	bool bVideoActive = mpVideoPipe && !mVideoSources.empty();

	mbVideoWaitingForSpace = false;
	mbVideoWaitingForRequest = false;

	double nVideoRate = 0;

	if (bVideoActive)
		nVideoRate = vInfo.mFrameRateIn.asDouble() * vInfo.mFrameRate.asDouble() / vInfo.mFrameRatePostFilter.asDouble();

	double nAudioRate = bAudioActive ? mpAudio->GetFormat()->mDataRate : 0;

	int minAudioBufferSpace = 0;
	if (bAudioActive) { 
		const VDWaveFormat *format = mpAudio->GetFormat();
		minAudioBufferSpace = format->mBlockSize;

		if (mpAudioPipe->IsVBRModeEnabled())
			minAudioBufferSpace += 4;
	}

	bool waitingForAudioSpace = false;

	try {
		mpCurrentAction = "running main loop";

		while(!mbAbort && (bAudioActive || bVideoActive)) { 
			bool bBlocked = true;
			if (mbPreview) waitingForAudioSpace = false;

			++mThreadCounter;

			if (!mLoopThrottle.Delay())
				continue;

			bool bCanWriteVideo = bVideoActive && !mbVideoWaitingForRequest && !mbVideoWaitingForSpace;
			bool bCanWriteAudio = bAudioActive && !waitingForAudioSpace;
			int preferAudio = 0;

			if (bCanWriteVideo && bCanWriteAudio) {
				const int nAudioLevel = mpAudioPipe->getLevel();
				int nVideoTotal, nVideoFinalQueued, nVideoAllocated;
				mpVideoPipe->getQueueInfo(nVideoTotal, nVideoFinalQueued, nVideoAllocated);

				if (nAudioLevel * nVideoRate < nVideoFinalQueued * nAudioRate)
					preferAudio = 1;
			}

			for(int i=0; i<2; ++i) {
				switch(preferAudio ^ i) {
					case 0:
						if (bCanWriteVideo) {
							bBlocked = false;

							VDDubAutoThreadLocation loc(mpCurrentAction, "reading video data");

							VDPROFILEBEGINEX2("V-Read wait",0,vdprofiler_flag_wait);

							MainAddVideoFrame();

							VDPROFILEEND();
							goto restart_service_loop;
						}
						break;

					case 1:
						if (!bCanWriteAudio)
							break;

						if (mpAudioPipe->getSpace() < minAudioBufferSpace) {
							waitingForAudioSpace = true;
							break;
						}

						{
							bBlocked = false;

							VDDubAutoThreadLocation loc(mpCurrentAction, "reading audio data");

							VDPROFILEBEGIN("Audio-read");

							if (!MainAddAudioFrame() && mpAudio->isEnd()) {
								if (!mbPreview && !mAudioSamplesWritten && VDPreferencesIsRenderNoAudioWarningEnabled()) {
									VDLogF(kVDLogWarning, L"Front end: The audio stream is ending with no samples having been sent.");
								}

								bAudioActive = false;
								mpAudioPipe->CloseInput();
							}

							VDPROFILEEND();
							goto restart_service_loop;
						}
						break;
				}
			}

			if (bBlocked) {
				if (bAudioActive && mpAudioPipe->isOutputClosed()) {
					bAudioActive = false;
					continue;
				}

				if (bVideoActive && mpVideoPipe->isFinalizeAcked()) {
					bVideoActive = false;
					continue;
				}

				VDDubAutoThreadLocation loc(mpCurrentAction, "stalled due to full pipe to processing thread");

				const VDSignalBase *signals[4] = { &mAbortSignal };
				int activeSignals = 1;
				int waitIndexAudioSpace = -2;
				int waitIndexVideoSpace = -2;
				int waitIndexVideoRequest = -2;

				if (waitingForAudioSpace) {
					waitIndexAudioSpace = activeSignals;
					signals[activeSignals++] = &mpAudioPipe->getReadSignal();
				}

				if (mbVideoWaitingForSpace) {
					waitIndexVideoSpace = activeSignals;
					signals[activeSignals++] = &mpVideoPipe->getReadSignal();
				} else if (mbVideoWaitingForRequest) {
					waitIndexVideoRequest = activeSignals;
					signals[activeSignals++] = &mpVideoRequestQueue->GetNotEmptySignal();
				}

				mLoopThrottle.BeginWait();
				int result = VDSignalBase::waitMultiple(signals, activeSignals);
				mLoopThrottle.EndWait();

				if (result == waitIndexAudioSpace) {
					if (mpAudioPipe->getSpace() >= minAudioBufferSpace)
						waitingForAudioSpace = false;
				}

				if (result == waitIndexVideoSpace)
					mbVideoWaitingForSpace = false;

				if (result == waitIndexVideoRequest)
					mbVideoWaitingForRequest = false;
			}
restart_service_loop:
			;
		}
	} catch(MyError& e) {
		if (!mbError) {
			mError.TransferFrom(e);
			mbError = true;
		}

		mpParent->InternalSignalStop();
	}

	if (mpAudioPipe)
		mpAudioPipe->CloseInput();
	if (mpVideoPipe)
		mpVideoPipe->finalize();
}

bool VDDubIOThread::MainAddVideoFrame() {
	const int srcIndex = 0;

	if (!mbVideoRequestActive) {
		if (!mpVideoRequestQueue->RemoveRequest(mVideoRequest)) {
			mbVideoWaitingForRequest = true;
			return false;
		}

		if (mVideoRequest.mSrcFrame < 0) {
			if (mpVideoPipe)
				mpVideoPipe->finalize();
			return false;
		}

		mbVideoRequestActive = true;
		mbVideoRequestFirstSample = true;

		mpVideoRequestSource = mVideoSources[srcIndex];

		sint64 len = mpVideoRequestSource->asStream()->getLength();
		if (len > 0 && mVideoRequest.mSrcFrame >= len) {
			mVideoRequest.mSrcFrame = len - 1;
		}

		mVideoRequestTargetSample = -1;

		if (!mVideoRequest.mbDirect) {
			mpVideoRequestSource->streamSetDesiredFrame(mVideoRequest.mSrcFrame);
			mVideoRequestTargetSample = mpVideoRequestSource->displayToStreamOrder(mVideoRequest.mSrcFrame);
		}
	}

	if (mpVideoPipe->full()) {
		mbVideoWaitingForSpace = true;
		return false;
	}

	// for the direct case, just read the frame and return
	if (mVideoRequest.mbDirect) {
		VDPROFILEBEGINEX("V-Read",(uint32)mVideoRequest.mSrcFrame);
		ReadRawVideoFrame(srcIndex, mpVideoRequestSource->displayToStreamOrder(mVideoRequest.mSrcFrame), mVideoRequest.mSrcFrame, mVideoRequestTargetSample, false, true);
		VDPROFILEEND();
		mbVideoRequestActive = false;
		return true;
	}

	// for the source frame case, read the next required frame and return
	bool preroll;

	VDPROFILEBEGINEX("V-Read",(uint32)mVideoRequest.mSrcFrame);
	VDPosition pos = mpVideoRequestSource->streamGetNextRequiredFrame(preroll);
	if (pos >= 0)
		ReadRawVideoFrame(srcIndex, pos, mVideoRequest.mSrcFrame, mVideoRequestTargetSample, preroll, false);
	else if (mbVideoRequestFirstSample)
		ReadNullVideoFrame(srcIndex, mVideoRequest.mSrcFrame, mVideoRequestTargetSample);
	VDPROFILEEND();

	mbVideoRequestFirstSample = false;

	if (!preroll)
		mbVideoRequestActive = false;
	return true;
}

void VDDubIOThread::ReadRawVideoFrame(int sourceIndex, VDPosition streamFrame, VDPosition displayFrame, VDPosition targetSample, bool preload, bool direct) {
	IVDVideoSource *vsrc = mVideoSources[sourceIndex];

	VDRenderVideoPipeFrameInfo frameInfo;
	frameInfo.mLength			= 0;
	frameInfo.mStreamFrame		= streamFrame;
	frameInfo.mDisplayFrame		= displayFrame;
	frameInfo.mTargetSample		= targetSample;
	frameInfo.mSrcIndex			= sourceIndex;
	frameInfo.mFlags			= (vsrc->isKey(displayFrame) ? 0 : kBufferFlagDelta) + (preload ? kBufferFlagPreload : 0);
	frameInfo.mDroptype			= 0;

	if (direct)
		frameInfo.mFlags |= kBufferFlagDirectWrite;

	// get frame size
	uint32 size;
	int hr;
	{
		VDDubAutoThreadLocation loc(mpCurrentAction, "reading video data from disk");

		hr = vsrc->asStream()->read(streamFrame, 1, NULL, 0x7FFFFFFF, &size, NULL);
	}

	if (hr) {
		if (hr == DubSource::kFileReadError)
			throw MyError("Video frame %d could not be read from the source. The file may be corrupt.", streamFrame);
		else
			throw MyAVIError("Dub/IO-Video", hr);
	}

	// allocate write buffer
	int handle;
	void *buffer = mpVideoPipe->getWriteBuffer(size + vsrc->streamGetDecodePadding(), &handle);
	if (!buffer)
		return;	// hmm, aborted...

	// read frame
	uint32 lActualBytes;
	{
		VDDubAutoThreadLocation loc(mpCurrentAction, "reading video data from disk");
		hr = vsrc->asStream()->read(streamFrame, 1, buffer, size, &lActualBytes, NULL); 
	}

	if (hr) {
		if (hr == DubSource::kFileReadError)
			throw MyError("Video frame %d could not be read from the source. The file may be corrupt.", streamFrame);
		else
			throw MyAVIError("Dub/IO-Video", hr);
	}

	vsrc->streamFillDecodePadding(buffer, size);

	// push into pipe
	frameInfo.mLength	= lActualBytes;
	frameInfo.mDroptype	= vsrc->getDropType(displayFrame);
	frameInfo.mbFinal	= !preload;
	mpVideoPipe->postBuffer(frameInfo);
}

void VDDubIOThread::ReadNullVideoFrame(int sourceIndex, VDPosition displayFrame, VDPosition targetSample) {
	VDRenderVideoPipeFrameInfo frameInfo;
	frameInfo.mLength			= 0;
	frameInfo.mStreamFrame		= -1;
	frameInfo.mDisplayFrame		= displayFrame;
	frameInfo.mTargetSample		= targetSample;
	frameInfo.mSrcIndex			= sourceIndex;
	frameInfo.mFlags			= 0;
	frameInfo.mDroptype			= IVDVideoSource::kDependant;
	frameInfo.mbFinal			= false;
	mpVideoPipe->postBuffer(frameInfo);
}

bool VDDubIOThread::MainAddAudioFrame() {
	if (mpAudioPipe->IsVBRModeEnabled()) {
		int totalSamples = 0;

		const VDWaveFormat *format = mpAudio->GetFormat();
		const int blocksize = format->mBlockSize;
		int samplesLeft = mpAudioPipe->getSpace() / blocksize;

		mAudioBuffer.resize(std::max<int>(format->mDataRate / 15, blocksize*4));
		char *buf = mAudioBuffer.data();

		while(samplesLeft > 0) {
			if (mbAbort)
				return false;

			if (mpAudioPipe->getSpace() < blocksize + sizeof(int) + sizeof(sint64))
				break;

			long actualBytes, actualSamples;
			{
				VDDubAutoThreadLocation loc(mpCurrentAction, "reading/processing audio data");
				actualSamples = mpAudio->Read(buf, 1, &actualBytes);
				VDASSERT(actualBytes <= actualSamples * blocksize);
			}

			if (actualSamples <= 0)
				break;

			VDASSERT(actualSamples == 1);

			int sampleSize = actualBytes;
			sint64 duration = mpAudio->GetLastPacketDuration();
			{
				VDDubAutoThreadLocation loc(mpCurrentAction, "pushing audio data to processing thread");

				if (!mpAudioPipe->Write(&sampleSize, sizeof(int), &mbAbort))
					return false;

				if (!mpAudioPipe->Write(&duration, sizeof(duration), &mbAbort))
					return false;

				if (!mpAudioPipe->Write(buf, actualBytes, &mbAbort))
					return false;
			}

			aInfo.total_size += actualBytes;

			totalSamples += actualSamples;
			samplesLeft -= actualSamples;
		}

		mAudioSamplesWritten += totalSamples;
		return totalSamples > 0;
	} else {
		long lActualSamples=0;

		const int blocksize = mpAudio->GetFormat()->mBlockSize;
		int samples = mpAudioPipe->getSpace();

		while(samples > 0) {
			int len = samples * blocksize;

			int tc;
			void *dst;
			
			dst = mpAudioPipe->BeginWrite(len, tc);

			if (!tc)
				break;

			if (mbAbort)
				break;

			long ltActualBytes, ltActualSamples;
			{
				VDDubAutoThreadLocation loc(mpCurrentAction, "reading/processing audio data");
				ltActualSamples = mpAudio->Read(dst, tc / blocksize, &ltActualBytes);
				VDASSERT(ltActualBytes <= tc);
			}

			if (ltActualSamples <= 0)
				break;

			int residue = ltActualBytes % blocksize;

			if (residue) {
				VDASSERT(false);	// This is bad -- it means the input file has partial samples.

				ltActualBytes += blocksize - residue;
			}

			mpAudioPipe->EndWrite(ltActualBytes);

			aInfo.total_size += ltActualBytes;

			lActualSamples += ltActualSamples;

			samples -= ltActualSamples;
		}

		mAudioSamplesWritten += lActualSamples;
		return lActualSamples > 0;
	}
}

