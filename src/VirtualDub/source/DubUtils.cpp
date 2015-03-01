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

#include <ddraw.h>

#include <vd2/system/error.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/w32assist.h>
#include <vd2/system/time.h>
#include <vector>

#include "DubUtils.h"
#include "VideoSource.h"
#include "FrameSubset.h"
#include "vbitmap.h"
#include "FilterSystem.h"

///////////////////////////////////////////////////////////////////////////
//
//	VDStreamInterleaver
//
///////////////////////////////////////////////////////////////////////////

void VDStreamInterleaver::Init(int streams) {
	mStreams.resize(streams);
	mNextStream = 0;
	mFrames = 0;

	mbInterleavingEnabled	= true;
	mNonIntStream		= 0;
	mActiveStreams		= 0;
}

void VDStreamInterleaver::InitStream(int stream, uint32 nSampleSize, sint32 nPreload, double nSamplesPerFrame, double nInterval, sint32 nMaxPush) {
	VDASSERT(stream>=0 && stream<mStreams.size());

	Stream& streaminfo = mStreams[stream];

	streaminfo.mSamplesWrittenToSegment = 0;
	streaminfo.mMaxSampleSize		= nSampleSize;
	streaminfo.mPreloadMicroFrames	= (sint32)((double)nPreload / nSamplesPerFrame * 65536);
	streaminfo.mSamplesPerFrame		= nSamplesPerFrame;
	streaminfo.mSamplesPerFramePending	= -1;
	streaminfo.mIntervalMicroFrames	= (sint32)(65536.0 / nInterval);
	streaminfo.mbActive				= true;
	streaminfo.mMaxPush				= nMaxPush;

	++mActiveStreams;
}

void VDStreamInterleaver::EndStream(int stream) {
	Stream& streaminfo = mStreams[stream];

	if (streaminfo.mbActive) {
		streaminfo.mbActive		= false;
		streaminfo.mSamplesToWrite	= 0;
		--mActiveStreams;

		while(mNonIntStream < mStreams.size() && !mStreams[mNonIntStream].mbActive)
			++mNonIntStream;
	}
}

void VDStreamInterleaver::AdjustStreamRate(int stream, double samplesPerFrame) {
	VDASSERT(stream >= 0 && stream < mStreams.size());
	Stream& streaminfo = mStreams[stream];

	streaminfo.mSamplesPerFramePending = samplesPerFrame;
}

VDStreamInterleaver::Action VDStreamInterleaver::GetNextAction(int& streamID, sint32& samples) {
	const int nStreams = mStreams.size();

	if (!mActiveStreams)
		return kActionFinish;

	for(;;) {
		if (!mNextStream) {
			Action act = PushStreams();

			if (act != kActionWrite)
				return act;
		}

		for(; mNextStream<nStreams; ++mNextStream) {
			Stream& streaminfo = mStreams[mNextStream];

			if (!mbInterleavingEnabled && mNextStream > mNonIntStream)
				break;

			if (streaminfo.mSamplesToWrite > 0) {
				samples = streaminfo.mSamplesToWrite;
				if (samples > streaminfo.mMaxPush)
					samples = streaminfo.mMaxPush;
				streaminfo.mSamplesToWrite -= samples;
				streamID = mNextStream;
				VDASSERT(samples < 2147483647);
				streaminfo.mSamplesWrittenToSegment += samples;
				return kActionWrite;
			}
		}

		mNextStream = 0;
	}
}

VDStreamInterleaver::Action VDStreamInterleaver::PushStreams() {
	const int nStreams = mStreams.size();

	for(;;) {
		int nFeeding = 0;
		sint64 microFrames = (sint64)mFrames << 16;

		for(int i=mNonIntStream; i<nStreams; ++i) {
			Stream& streaminfo = mStreams[i];

			if (!streaminfo.mbActive)
				continue;

			sint64 microFrameOffset = microFrames;
			
			if (streaminfo.mIntervalMicroFrames != 65536) {
				microFrameOffset += streaminfo.mIntervalMicroFrames - 1;
				microFrameOffset -= microFrameOffset % streaminfo.mIntervalMicroFrames;
			}

			microFrameOffset += streaminfo.mPreloadMicroFrames;

			double frame = microFrameOffset / 65536.0;

			sint64 target = (sint64)ceil(streaminfo.mSamplesPerFrame * frame);
			sint64 toread = target - streaminfo.mSamplesWrittenToSegment;

			if (toread < 0)
				toread = 0;
		
			VDASSERT((sint32)toread == toread);
			streaminfo.mSamplesToWrite = (sint32)toread;

//			VDDEBUG("Dub/Interleaver: Stream #%d: feeding %d samples (%4I64x, %I64d + %I64d -> %I64d)\n", i, (int)toread, microFrameOffset, streaminfo.mSamplesWrittenToSegment, toread, target);
			if (toread > 0)
				++nFeeding;

			if (!mbInterleavingEnabled)
				break;
		}

		if (nFeeding > 0)
			break;

		// If no streams are feeding and we have no cut point, bump the frame target by 1 frame and
		// try again.

		++mFrames;

		for(int i=0; i<nStreams; ++i) {
			Stream& streaminfo = mStreams[i];

			if (streaminfo.mSamplesPerFramePending >= 0) {
				streaminfo.mSamplesPerFrame = streaminfo.mSamplesPerFramePending;
				streaminfo.mSamplesPerFramePending = -1.0;
			}
		}
	}

	if (!mbInterleavingEnabled)
		mNextStream = mNonIntStream;

	return kActionWrite;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDRenderFrameMap
//
///////////////////////////////////////////////////////////////////////////

void VDRenderFrameMap::Init(const vdfastvector<IVDVideoSource *>& videoSources, VDPosition nSrcStart, VDFraction srcStep, const FrameSubset *pSubset, VDPosition nFrameCount, bool allowDirect, bool forceDirect, bool preserveEmptyFrames, const FilterSystem *pRemapperFS, bool allowNullFrames, bool useSourceFrames) {
	VDPosition directLast = -1;
	VDPosition sourceLast = -1;
	int sourceIndexLast = -1;
	IVDVideoSource *pVS = NULL;
	bool lastUseDirect = false;

	mFrameMap.reserve((size_t)nFrameCount);
	for(VDPosition frame = 0; frame < nFrameCount; ++frame) {
		VDPosition timelineFrame = nSrcStart + srcStep.scale64t(frame);

		// translate from timeline to source frame through the subset... this means that the subset
		// is POST-FILTER.
		VDPosition srcFrame = timelineFrame;

		if (pSubset) {
			bool masked;
			int dummy;
			srcFrame = pSubset->lookupFrame((int)srcFrame, masked, dummy);
			if (srcFrame < 0)
				break;
		} else {
			if (srcFrame < 0)
				srcFrame = 0;
		}

		// check if we're allowed to go direct mode for this frame
		sint64 directFrame = -1;
		int directIndex = -1;
		bool useDirect = false;

		if (forceDirect) {
			// direct mode is forced on, so set the direct mapping
			useDirect = true;
			directIndex = 0;
			directFrame = srcFrame;
			
			// allow the filter system to remap if present since the subset depends on it anyway
			if (pRemapperFS)
				directFrame = pRemapperFS->GetSourceFrame(directFrame);
		} else if (allowDirect) {
			// check if the filter system wants to request direct mapping
			if (pRemapperFS) {
				sint64 directFrameTest;
				int directIndexTest;
				if (pRemapperFS->GetDirectFrameMapping(srcFrame, directFrameTest, directIndexTest)) {
					directFrame = directFrameTest;
					directIndex = directIndexTest;
					useDirect = true;
				}
			}
		}

		// switch sources if directed
		if (sourceIndexLast != directIndex) {
			sourceIndexLast = directIndex;

			if (directIndex >= 0)
				pVS = videoSources[directIndex];
			else {
				VDASSERT(!useDirect);
				pVS = NULL;
			}

			// invalidate the last direct frame since we've switched sources
			directLast = -1;
		}

		// check and adjust for direct mode dependency violations
		if (useDirect) {
			// allow video source to remap to last unique frame; note that this is done later if we are
			// not in direct mode, as we don't know the source frame until the filter system requests it
			sint64 actualDirectFrame = pVS->getRealDisplayFrame(directFrame);

			// get nearest key frame
			VDPosition key = pVS->nearestKey(actualDirectFrame);

			// clamp to nearest decodable
			if (directLast < key) {
				// Current frame is in earlier dependency chain.
				directLast = key;
			} else if (directLast > actualDirectFrame) {
				// Desired frame is behind current frame.
				directLast = key;
			} else {
				if (directLast < actualDirectFrame) {
					for(;;) {
						++directLast;
						if (directLast >= actualDirectFrame)
							break;

						if (pVS->getDropType(directLast) != VideoSource::kDroppable)
							break;
					}
				}
			}

			// Check if we couldn't get the frame we wanted due to frame restrictions. If this happens
			// in force-direct mode, tough noogies. If we're in allow-direct mode, though, then we've
			// got to drop to full mode.
			if (!forceDirect && actualDirectFrame != directLast) {
				// Can't copy the right frame -- drop to full mode.
				useDirect = false;
			} else {
				// Looks good -- use the nearest decodable frame as the direct frame.
				srcFrame = directLast;
			}
		}
		
		// If we want only source frames, we must backtranslate output frames.
		if (!useDirect && useSourceFrames) {
			if (pRemapperFS)
				srcFrame = pRemapperFS->GetSourceFrame(srcFrame);
		}

		// If we switched direct modes, we need to invalidate the last frame counter.
		if (lastUseDirect != useDirect) {
			lastUseDirect = useDirect;
			sourceLast = -1;
		}

		// If we're writing the same frame, we may be able to drop it.
		bool sameAsLast = false;

		if (sourceLast >= 0) {
			if (sourceLast == srcFrame) {
				if (useDirect || preserveEmptyFrames) {
					sameAsLast = true;
				}
			} else if (preserveEmptyFrames && pRemapperFS) {
				VDPosition srcUniqueFrame = pRemapperFS->GetNearestUniqueFrame(srcFrame);

				if (sourceLast >= srcUniqueFrame)
					sameAsLast = true;
			}
		}
		
		if (sameAsLast && allowNullFrames) {
			srcFrame = -1;
			useDirect = true;
		} else
			sourceLast = srcFrame;

		if (!useDirect)
			directLast = -1;

		InternalFrameEntry ient;
		ient.mTimelineFrame = timelineFrame;
		ient.mSourceFrameAndDirectFlag = srcFrame + srcFrame + useDirect;
		mFrameMap.push_back(ient);
	}

	mMaxFrame = mFrameMap.size();
}

const VDRenderFrameMap::FrameEntry VDRenderFrameMap::operator[](VDPosition outFrame) const {
	FrameEntry ent;

	if (outFrame < 0 || outFrame >= mMaxFrame) {
		ent.mTimelineFrame = -1;
		ent.mSourceFrame = -1;
		ent.mbDirect = false;
		ent.mbHoldFrame = true;
	} else {
		const InternalFrameEntry& ient = mFrameMap[(FrameMap::size_type)outFrame];

		ent.mTimelineFrame = ient.mTimelineFrame;
		ent.mSourceFrame = ient.mSourceFrameAndDirectFlag >> 1;
		ent.mbDirect = ((int)ient.mSourceFrameAndDirectFlag & 1) != 0;
		ent.mbHoldFrame = false;

		if (!ent.mbDirect) {
			VDPosition nextFrame = outFrame + 1;

			// Note that we should hold frame on the last frame, if it's processed.
			bool nextIsDirect = true;
			if (nextFrame < mMaxFrame)
				nextIsDirect = (mFrameMap[(FrameMap::size_type)nextFrame].mSourceFrameAndDirectFlag & 1) != 0;

			ent.mbHoldFrame = nextIsDirect;
		}
	}

	return ent;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAudioPipeline
//
///////////////////////////////////////////////////////////////////////////

VDAudioPipeline::VDAudioPipeline() {
}

VDAudioPipeline::~VDAudioPipeline() {
}

void VDAudioPipeline::Init(uint32 bytes, uint32 sampleSize, bool vbrModeEnabled) {
	mbVBRModeEnabled = vbrModeEnabled;
	mbInputClosed = false;
	mbOutputClosed = false;
	mSampleSize = sampleSize;

	mBuffer.Init(bytes);
}

void VDAudioPipeline::Shutdown() {
	mBuffer.Shutdown();
}

int VDAudioPipeline::ReadPartial(void *pBuffer, int bytes) {
	int actual = mBuffer.Read((char *)pBuffer, bytes);

	if (actual)
		msigRead.signal();

	return actual;
}

bool VDAudioPipeline::Write(const void *data, int bytes, const VDAtomicInt *abortFlag) {
	int actual;
	while(bytes > 0) {
		void *dst = BeginWrite(bytes, actual);

		if (!actual) {
			if (abortFlag && *abortFlag)
				return false;

			msigRead.wait();
			continue;
		}

		memcpy(dst, data, actual);

		EndWrite(actual);

		data = (const char *)data + actual;
		bytes -= actual;
	}

	VDASSERT(bytes == 0);

	return true;
}

void *VDAudioPipeline::BeginWrite(int requested, int& actual) {
	return mBuffer.LockWrite(requested, actual);
}

void VDAudioPipeline::EndWrite(int actual) {
	if (actual) {
		mBuffer.UnlockWrite(actual);
		msigWrite.signal();
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	VDLoopThrottle
//
///////////////////////////////////////////////////////////////////////////

VDLoopThrottle::VDLoopThrottle()
	: mThrottleFactor(1.0f)
	, mActivityRatio(1.0f)
	, mWaitDepth(0)
	, mWaitTime(0)
	, mLastTime(0)
	, mbLastTimeValid(false)
	, mWindowIndex(0)
	, mWaitTimeWindowSum(0)
	, mActiveTimeWindowSum(0)
	, mSuspendState(false)
	, mSuspendRequested(false)
{
	memset(mWaitTimeWindow, 0, sizeof mWaitTimeWindow);
	memset(mActiveTimeWindow, 0, sizeof mActiveTimeWindow);
}

VDLoopThrottle::~VDLoopThrottle() {
}

bool VDLoopThrottle::Delay() {
	float desiredRatio = mThrottleFactor;

	if (desiredRatio <= 0.0f) {
		mActivityRatio = 0.0f;
		::Sleep(100);
		return false;
	}

	uint32 total = mActiveTimeWindowSum + mWaitTimeWindowSum;

	if (total > 0) {
		float delta = mActiveTimeWindowSum - total * mThrottleFactor;

		mWaitTime += delta * (0.1f / 16.0f);

		mActivityRatio = (float)mActiveTimeWindowSum / (float)total;
	}

	if (desiredRatio >= 1.0f) {
		mWaitTime = 0.0f;
		return true;
	}


	if (mWaitTime > 0) {
		int delayTime = VDRoundToInt(mWaitTime);

		if (delayTime > 1000)
			delayTime = 1000;

		BeginWait();
		::Sleep(delayTime);
		EndWait();
	}

	return true;
}

void VDLoopThrottle::BeginWait() {
	CheckForSuspend();

	if (!mWaitDepth++) {	// transitioning active -> wait
		uint32 currentTime = VDGetAccurateTick();

		if (mbLastTimeValid) {
			sint32 delta = currentTime - mLastTime;

			// Time shouldn't ever go backwards, but clocks on Windows occasionally have
			// the habit of doing so due to time adjustments, broken RDTSC, etc.
			if (delta < 0)
				delta = 0;

			mActiveTimeWindowSum -= mActiveTimeWindow[mWindowIndex];
			mActiveTimeWindow[mWindowIndex] = delta;
			mActiveTimeWindowSum += delta;
		}

		mLastTime = currentTime;
		mbLastTimeValid = true;
	}
}

void VDLoopThrottle::EndWait() {
	CheckForSuspend();

	if (!--mWaitDepth) {	// transitioning wait -> active
		uint32 currentTime = VDGetAccurateTick();

		if (mbLastTimeValid) {
			sint32 delta = currentTime - mLastTime;

			// Time shouldn't ever go backwards, but clocks on Windows occasionally have
			// the habit of doing so due to time adjustments, broken RDTSC, etc.
			if (delta < 0)
				delta = 0;

			mWaitTimeWindowSum -= mWaitTimeWindow[mWindowIndex];
			mWaitTimeWindow[mWindowIndex] = delta;
			mWaitTimeWindowSum += delta;

			mWindowIndex = (mWindowIndex + 1) & 15;
		}

		mLastTime = currentTime;
		mbLastTimeValid = true;
	}

	VDASSERT(mWaitDepth >= 0);
}

void VDLoopThrottle::CheckForSuspend() {
	if (mSuspendRequested) {
		mSuspendState = true;
		mStateChange.signal();
		while(mSuspendRequested)
			mRequestChange.wait();
		mSuspendState = false;
	}
}

void VDLoopThrottle::BeginSuspend() {
	mSuspendRequested = true;
	mRequestChange.signal();
}

bool VDLoopThrottle::TryWaitSuspend(uint32 timeout) {
	while(!mSuspendState) {
		if (!mStateChange.tryWait(timeout))
			return false;
	}

	return true;
}

void VDLoopThrottle::EndSuspend() {
	mSuspendRequested = false;
	mRequestChange.signal();
}
