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
#include <vd2/Dita/resources.h>
#include <vd2/system/error.h>
#include <vd2/system/log.h>
#include <vd2/system/w32assist.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/VDDisplay/display.h>
#include <vd2/Riza/videocodec.h>
#include <vd2/Riza/bitmap.h>
#include "crash.h"
#include "Dub.h"
#include "DubIO.h"
#include "DubProcess.h"
#include "DubUtils.h"
#include "DubOutput.h"
#include "DubStatus.h"
#include "AVIPipe.h"
#include "AVIOutput.h"
#include "AVIOutputPreview.h"
#include "VideoSource.h"
#include "prefs.h"
#include "AsyncBlitter.h"
#include "FilterSystem.h"
#include "FilterFrame.h"
#include "FilterFrameRequest.h"
#include "FilterFrameManualSource.h"

using namespace nsVDDub;

bool VDPreferencesIsRenderNoAudioWarningEnabled();
bool VDPreferencesGetRenderInhibitSystemSleepEnabled();

namespace {
	enum { kVDST_Dub = 1 };

	enum {
		kVDM_SegmentOverflowOccurred,
		kVDM_BeginningNextSegment,
		kVDM_IOThreadLivelock,
		kVDM_ProcessingThreadLivelock,
		kVDM_CodecLoopingDuringDelayedFlush,
		kVDM_FastRecompressUsingFormat,
		kVDM_SlowRecompressUsingFormat,
		kVDM_FullUsingInputFormat,
		kVDM_FullUsingOutputFormat
	};
};

VDDubProcessThread::VDDubProcessThread()
	: VDThread("Processing")
	, opt(NULL)
	, mpInterleaver(NULL)
	, mpParent(NULL)
	, mpAVIOut(NULL)
	, mpVideoOut(NULL)
	, mpAudioOut(NULL)
	, mpOutputSystem(NULL)
	, mpAudioPipe(NULL)
	, mpAudioCorrector(NULL)
	, mbAudioPresent(false)
	, mbAudioEnded(false)
	, mAudioSamplesWritten(0)
	, mpVideoPipe(NULL)
	, mbVideoEnded(false)
	, mbVideoPushEnded(false)
	, mpVInfo(NULL)
	, mpBlitter(NULL)
	, mpStatusHandler(NULL)
	, mbPreview(false)
	, mbFirstPacket(false)
	, mbError(false)
	, mbCompleted(false)
	, mpAbort(NULL)
	, mpCurrentAction("starting up")
	, mActivityCounter(0)
{
}

VDDubProcessThread::~VDDubProcessThread() {
	Shutdown();
}

IVDFilterSystemScheduler *VDDubProcessThread::GetVideoFilterScheduler() {
	return &mVideoProcessor;
}

void VDDubProcessThread::PreInit() {
	mVideoProcessor.PreInit();
}

void VDDubProcessThread::SetParent(IDubberInternal *pParent) {
	mpParent = pParent;
}

void VDDubProcessThread::SetAbortSignal(VDAtomicInt *pAbort) {
	mpAbort = pAbort;

	mVideoProcessor.SetThreadSignals(&mpCurrentAction, &mLoopThrottle);
}

void VDDubProcessThread::SetStatusHandler(IDubStatusHandler *pStatusHandler) {
	mpStatusHandler = pStatusHandler;

	mVideoProcessor.SetStatusHandler(pStatusHandler);
}

void VDDubProcessThread::SetInputDisplay(IVDVideoDisplay *pVideoDisplay) {
	mVideoProcessor.SetInputDisplay(pVideoDisplay);
}

void VDDubProcessThread::SetOutputDisplay(IVDVideoDisplay *pVideoDisplay) {
	mVideoProcessor.SetOutputDisplay(pVideoDisplay);
}

void VDDubProcessThread::SetVideoOutput(const VDPixmapLayout& layout, int mode) {
	if (mode >= DubVideoOptions::M_FULL)
		mVideoProcessor.SetVideoFilterOutput(layout);
	else
		mVideoProcessor.SetVideoDirectOutput(layout);
}

void VDDubProcessThread::SetVideoSources(IVDVideoSource *const *pVideoSources, uint32 count) {
	mVideoSources.assign(pVideoSources, pVideoSources + count);

	mVideoProcessor.SetVideoSources(pVideoSources, count);
}

void VDDubProcessThread::SetVideoFrameSource(VDFilterFrameManualSource *fs) {
	mVideoProcessor.SetVideoFrameSource(fs);
}

void VDDubProcessThread::SetAudioSourcePresent(bool present) {
	mbAudioPresent = present;
}

void VDDubProcessThread::SetAudioCorrector(AudioStreamL3Corrector *pCorrector) {
	mpAudioCorrector = pCorrector;
}

void VDDubProcessThread::SetVideoCompressor(IVDVideoCompressor *pCompressor, int threadCount) {
	mVideoProcessor.SetVideoCompressor(pCompressor, threadCount);
}

void VDDubProcessThread::SetVideoFilterSystem(FilterSystem *fs) {
	mVideoProcessor.SetVideoFilterSystem(fs);
}

void VDDubProcessThread::SetVideoRequestQueue(VDDubFrameRequestQueue *q) {
	mVideoProcessor.SetVideoRequestQueue(q);
}

void VDDubProcessThread::SetPriority(int priority) {
	ThreadSetPriority(priority);

	mVideoProcessor.SetPriority(priority);
}

void VDDubProcessThread::Init(const DubOptions& opts, const VDRenderFrameMap *frameMap, DubVideoStreamInfo *pvsi, IVDDubberOutputSystem *pOutputSystem, AVIPipe *pVideoPipe, VDAudioPipeline *pAudioPipe, VDStreamInterleaver *pStreamInterleaver) {
	opt = &opts;
	mpVInfo = pvsi;
	mpOutputSystem = pOutputSystem;
	mpVideoPipe = pVideoPipe;
	mpAudioPipe = pAudioPipe;
	mpInterleaver = pStreamInterleaver;

	mVideoProcessor.SetCallback(this);
	mVideoProcessor.SetOptions(&opts);
	mVideoProcessor.SetVideoStreamInfo(pvsi);
	mVideoProcessor.SetVideoPipe(pVideoPipe);
	mVideoProcessor.SetVideoFrameMap(frameMap);

	if (!mpOutputSystem->IsRealTime())
		mpBlitter = VDCreateAsyncBlitter();
	else
		mpBlitter = VDCreateAsyncBlitter(8);

	if (!mpBlitter)
		throw MyError("Couldn't create AsyncBlitter");

	mVideoProcessor.SetBlitter(mpBlitter);
	mVideoProcessor.Init();

	mpBlitter->pulse(1);

	NextSegment();

	// Init playback timer.
	if (mpOutputSystem->IsRealTime()) {
		double frameRate = mpVInfo->mFrameRate.asDouble();
		double frameMultiplier = 2.0;

		if (opt->video.previewFieldMode) {
			frameRate *= 2.0;
			frameMultiplier = 1.0;
		}

		// This MUST happen before the clock is inited.
		mVideoProcessor.SetPreviewClock(&mPreviewClock);

		if (opt->video.fSyncToAudio)
			mPreviewClock.Init(static_cast<AVIAudioPreviewOutputStream *>(mpAudioOut), mpBlitter, frameRate, frameMultiplier);
		else
			mPreviewClock.Init(NULL, mpBlitter, frameRate, frameMultiplier);
	}
}

void VDDubProcessThread::Shutdown() {
	mPreviewClock.Shutdown();
	mVideoProcessor.Shutdown();

	if (mpBlitter) {
		mpBlitter->abort();
		delete mpBlitter;
		mpBlitter = NULL;
	}

	if (mpAVIOut) {
		delete mpAVIOut;
		mpAVIOut = NULL;
		mpAudioOut = NULL;
		mpVideoOut = NULL;
		mVideoProcessor.SetVideoOutput(NULL, false);
	}
}

void VDDubProcessThread::Abort() {
	// NOTE: This function is asynchronous.

	if (mpBlitter)
		mpBlitter->beginFlush();

	mVideoProcessor.Abort();
}

void VDDubProcessThread::UpdateFrames() {
	mVideoProcessor.UpdateFrames();
}

VDSignal *VDDubProcessThread::GetBlitterSignal() {
	return mpBlitter ? mpBlitter->getFlushCompleteSignal() : NULL;
}

void VDDubProcessThread::SetThrottle(float f) {
	mLoopThrottle.SetThrottleFactor(f);
}

void VDDubProcessThread::DumpStatus(VDTextOutputStream& os) {
	os.PutLine("=================");
	os.PutLine("Processing thread");
	os.PutLine("=================");
	os.PutLine();

	os.FormatLine("Completed:         %s", mbCompleted ? "Yes" : "No");
	os.FormatLine("Error encountered: %s", mbError ? "Yes" : "No");

	mLoopThrottle.BeginSuspend();
	mVideoProcessor.PreDumpStatus(os);

	if (mLoopThrottle.TryWaitSuspend(3000)) {
		mVideoProcessor.DumpStatus(os);
	} else {
		os.PutLine();
		os.PutLine("The processing thread is busy and could not be suspended.");
	}

	mLoopThrottle.EndSuspend();
}

void VDDubProcessThread::NextSegment() {
	if (mpAVIOut) {
		IVDMediaOutput *temp = mpAVIOut;
		mpAVIOut = NULL;
		mpAudioOut = NULL;
		mpVideoOut = NULL;
		mVideoProcessor.SetVideoOutput(NULL, false);
		mpOutputSystem->CloseSegment(temp, false);
	}

	mpAVIOut = mpOutputSystem->CreateSegment();
	mpAudioOut = mpAVIOut->getAudioOutput();
	mpVideoOut = mpAVIOut->getVideoOutput();
	mVideoProcessor.SetVideoOutput(mpVideoOut, mpOutputSystem->IsVideoImageOutputEnabled());
}

void VDDubProcessThread::ThreadRun() {
	mbVideoEnded = !(mVideoSources[0] && mpOutputSystem->AcceptsVideo());

	mpVInfo->processed = 0;

	mbAudioEnded = !(mbAudioPresent && mpOutputSystem->AcceptsAudio());
	mbFirstPacket = false;
	mbPreview = mpOutputSystem->IsRealTime();

	if (VDPreferencesGetRenderInhibitSystemSleepEnabled())
		VDSetThreadExecutionStateW32(mbPreview ? ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED | ES_CONTINUOUS : ES_SYSTEM_REQUIRED | ES_CONTINUOUS);

	mVideoProcessor.SetPreview(mbPreview);

	IVDMediaOutputAutoInterleave *pOutAI = vdpoly_cast<IVDMediaOutputAutoInterleave *>(mpAVIOut);

	if (pOutAI)
		mpInterleaver = NULL;

	try {
		mpCurrentAction = "running main loop";

		for(;;) {
			int stream;
			sint32 count;

			if (!mLoopThrottle.Delay()) {
				++mActivityCounter;
				if (*mpAbort)
					break;
				continue;
			}

			// check for video decompressor switch
			mVideoProcessor.CheckForDecompressorSwitch();

			VDStreamInterleaver::Action nextAction;
			
			if (mpInterleaver)
				nextAction = mpInterleaver->GetNextAction(stream, count);
			else {
				if (mbAudioEnded && mbVideoEnded)
					break;

				nextAction = VDStreamInterleaver::kActionWrite;
				pOutAI->GetNextPreferredStreamWrite(stream, count);
			}

			++mActivityCounter;

			if (nextAction == VDStreamInterleaver::kActionFinish)
				break;
			else if (nextAction == VDStreamInterleaver::kActionWrite) {
				if (stream == 0) {
					if (mbVideoEnded || mVideoProcessor.IsCompleted()) {
						if (mbPreview && mbAudioPresent) {
							static_cast<AVIAudioPreviewOutputStream *>(mpAudioOut)->start();
						}
						if (mpInterleaver)
							mpInterleaver->EndStream(0);
						mpVideoOut->finish();
						mbVideoEnded = true;
					} else {
						if (!mVideoProcessor.WriteVideo())
							goto abort_requested;
					}
				} else if (stream == 1) {
					if (!WriteAudio(count))
						goto abort_requested;
				} else {
					VDNEVERHERE;
				}
			}

			if (*mpAbort)
				break;

			if (mbVideoEnded && mbAudioEnded)
				break;
		}
abort_requested:
		;

	} catch(MyError& e) {
		if (!mbError) {
			mError.TransferFrom(e);
			mbError = true;
		}

		mpVideoPipe->abort();
		mpParent->InternalSignalStop();
	}

	mbCompleted = mbAudioEnded && mbVideoEnded;

	mpVideoPipe->finalizeAck();
	mpAudioPipe->CloseOutput();

	// attempt a graceful shutdown at this point...
	try {
		// if preview mode, choke the audio

		if (mpAudioOut && mpOutputSystem->IsRealTime())
			static_cast<AVIAudioPreviewOutputStream *>(mpAudioOut)->stop();

		// finalize the output.. if it's not a preview...
		if (!mpOutputSystem->IsRealTime()) {
			// update audio rate...

			if (mpAudioCorrector) {
				UpdateAudioStreamRate();
			}

			// finalize avi
			mpAudioOut = NULL;
			mpVideoOut = NULL;
			mVideoProcessor.SetVideoOutput(NULL, false);
			IVDMediaOutput *temp = mpAVIOut;
			mpAVIOut = NULL;
			mpOutputSystem->CloseSegment(temp, true, mbCompleted);
		}
	} catch(MyError& e) {
		if (!mbError) {
			mError.TransferFrom(e);
			mbError = true;
		}
	}

	mVideoProcessor.PreShutdown();

	mpParent->InternalSignalStop();
}

///////////////////////////////////////////////////////////////////////////////

bool VDDubProcessThread::WriteAudio(sint32 count) {
	if (count <= 0)
		return true;

	const int nBlockAlign = mpAudioPipe->GetSampleSize();

	int totalBytes = 0;
	int totalSamples = 0;

	if (mpAudioPipe->IsVBRModeEnabled()) {
		mAudioBuffer.resize(nBlockAlign);
		char *buf = mAudioBuffer.data();

		VDPROFILEBEGIN("Audio-write");
		while(totalSamples < count) {
			while(mpAudioPipe->getLevel() < sizeof(int) + sizeof(sint64)) {
				if (mpAudioPipe->isInputClosed()) {
					mpAudioPipe->CloseOutput();
					if (mpInterleaver)
						mpInterleaver->EndStream(1);
					mpAudioOut->finish();
					mbAudioEnded = true;
					goto ended;
				}

				VDDubAutoThreadLocation loc(mpCurrentAction, "waiting for audio data from I/O thread");
 				VDPROFILEBEGINEX2("Audio-wait",0,vdprofiler_flag_wait);
				mLoopThrottle.BeginWait();
				mpAudioPipe->ReadWait();
				mLoopThrottle.EndWait();
				VDPROFILEEND();
			}

			int sampleSize;
			int tc = mpAudioPipe->ReadPartial(&sampleSize, sizeof(int));
			VDASSERT(tc == sizeof(int));
			sint64 duration;
			tc = mpAudioPipe->ReadPartial(&duration, sizeof(duration));
			VDASSERT(tc == sizeof(duration));

			VDASSERT(sampleSize <= nBlockAlign);

			int pos = 0;

			while(pos < sampleSize) {
				if (*mpAbort)
					return false;

				tc = mpAudioPipe->ReadPartial(buf + pos, sampleSize - pos);
				if (!tc) {
					if (mpAudioPipe->isInputClosed()) {
						mpAudioPipe->CloseOutput();
						if (mpInterleaver)
							mpInterleaver->EndStream(1);
						mpAudioOut->finish();
						mbAudioEnded = true;
						break;
					}

					VDDubAutoThreadLocation loc(mpCurrentAction, "waiting for audio data from I/O thread");
					VDPROFILEBEGINEX2("Audio-wait",0,vdprofiler_flag_wait);
					mLoopThrottle.BeginWait();
					mpAudioPipe->ReadWait();
					mLoopThrottle.EndWait();
					VDPROFILEEND();
				}

				pos += tc;
				VDASSERT(pos <= sampleSize);
			}

			++mAudioSamplesWritten;

			VDPROFILEBEGIN("A-Write");
			{
				VDDubAutoThreadLocation loc(mpCurrentAction, "writing audio data to disk");

				IVDXOutputFile::PacketInfo info;
				info.flags = AVIOutputStream::kFlagKeyFrame;
				info.samples = 1;
				info.pcm_samples = duration;
				mpAudioOut->write(buf, sampleSize, info);
			}
			VDPROFILEEND();

			totalBytes += sampleSize;
			++totalSamples;
		}
ended:
		VDPROFILEEND();

		if (!totalSamples)
			return true;
	} else {
		int bytes = count * nBlockAlign;

		if (mAudioBuffer.size() < bytes)
			mAudioBuffer.resize(bytes);

		VDPROFILEBEGIN("Audio-write");
		while(totalBytes < bytes) {
			int tc = mpAudioPipe->ReadPartial(&mAudioBuffer[totalBytes], bytes-totalBytes);

			if (*mpAbort)
				return false;

			if (!tc) {
				if (mpAudioPipe->isInputClosed()) {
					if (!mbPreview && !mAudioSamplesWritten && VDPreferencesIsRenderNoAudioWarningEnabled()) {
						VDLogF(kVDLogWarning, L"Back end: The audio stream is ending without any audio samples having been received.");
					}

					mpAudioPipe->CloseOutput();
					totalBytes -= totalBytes % nBlockAlign;
					count = totalBytes / nBlockAlign;
					if (mpInterleaver)
						mpInterleaver->EndStream(1);
					mpAudioOut->finish();
					mbAudioEnded = true;
					break;
				}

				VDDubAutoThreadLocation loc(mpCurrentAction, "waiting for audio data from I/O thread");
				VDPROFILEBEGINEX2("Audio-wait",0,vdprofiler_flag_wait);
				mLoopThrottle.BeginWait();
				mpAudioPipe->ReadWait();
				mLoopThrottle.EndWait();
				VDPROFILEEND();
			}

			mAudioSamplesWritten += tc;
			totalBytes += tc;
		}
		VDPROFILEEND();

		if (totalBytes <= 0)
			return true;

		totalSamples = totalBytes / nBlockAlign;

		VDPROFILEBEGIN("A-Write");
		{
			VDDubAutoThreadLocation loc(mpCurrentAction, "writing audio data to disk");

			IVDXOutputFile::PacketInfo info;
			info.flags = AVIOutputStream::kFlagKeyFrame;
			info.samples = totalSamples;
			mpAudioOut->write(mAudioBuffer.data(), totalBytes, info);
		}
		VDPROFILEEND();
	}

	// if audio and video are ready, start preview
	if (mbFirstPacket && mbPreview) {
		mpAudioOut->flush();
		mpBlitter->enablePulsing(true);
		mbFirstPacket = false;
	}

	// apply audio correction on the fly if we are doing L3
	//
	// NOTE: Don't begin correction until we have at least 20 MPEG frames.  The error
	//       is generally under 5% and we don't want the beginning of the stream to go
	//       nuts.
	if (mpAudioCorrector && mpAudioCorrector->GetFrameCount() >= 20) {
		vdstructex<WAVEFORMATEX> wfex((const WAVEFORMATEX *)mpAudioOut->getFormat(), mpAudioOut->getFormatLen());
		
		double bytesPerSec = mpAudioCorrector->ComputeByterateDouble(wfex->nSamplesPerSec);

		if (mpInterleaver)
			mpInterleaver->AdjustStreamRate(1, bytesPerSec / mpVInfo->mFrameRate.asDouble());
		UpdateAudioStreamRate();
	}

	return true;
}

void VDDubProcessThread::UpdateAudioStreamRate() {
	vdstructex<WAVEFORMATEX> wfex((const WAVEFORMATEX *)mpAudioOut->getFormat(), mpAudioOut->getFormatLen());
	
	wfex->nAvgBytesPerSec = mpAudioCorrector->ComputeByterate(wfex->nSamplesPerSec);

	mpAudioOut->setFormat(&*wfex, wfex.size());

	AVIStreamHeader_fixed hdr(mpAudioOut->getStreamInfo());
	hdr.dwRate = wfex->nAvgBytesPerSec * hdr.dwScale;
	mpAudioOut->updateStreamInfo(hdr);
}

void VDDubProcessThread::OnVideoStreamEnded() {
	if (mpInterleaver)
		mpInterleaver->EndStream(0);
	mpVideoOut->finish();
	mbVideoEnded = true;
}

void VDDubProcessThread::OnFirstFrameWritten() {
	if (!mbAudioPresent) {
		mpBlitter->enablePulsing(true);
	} else {
		static_cast<AVIAudioPreviewOutputStream *>(mpAudioOut)->start();
	}
}
