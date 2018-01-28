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
#include <vd2/system/profile.h>
#include <vd2/Riza/bitmap.h>
#include <vd2/VDDisplay/display.h>
#include "Dub.h"
#include "DubProcessVideoDisplay.h"
#include "DubUtils.h"
#include "AsyncBlitter.h"
#include "ThreadedVideoCompressor.h"
#include "VideoSource.h"
#include "prefs.h"
#include "DubStatus.h"

bool VDPreferencesIsPreferInternalVideoDecodersEnabled();
IVDVideoDecompressor *VDFindVideoDecompressorEx(uint32 fccHandler, const VDAVIBitmapInfoHeader *hdr, uint32 hdrlen, bool preferInternal);

namespace {
	enum {
		// This is to work around an XviD decode bug (see VideoSource.h).
		kDecodeOverflowWorkaroundSize = 16,

		kReasonableBFrameBufferLimit = 100
	};

	enum {
		BUFFERID_INPUT = 1,
		BUFFERID_OUTPUT = 2
	};

	bool AsyncDecompressorFailedCallback(int pass, sint64 timelinePos, void *pDisplayAsVoid, void *, bool aborting) {
		if (aborting)
			return false;

		IVDVideoDisplay *pVideoDisplay = (IVDVideoDisplay *)pDisplayAsVoid;

		pVideoDisplay->SetSourceMessage(L"Unable to display compressed video: no decompressor is available to decode the compressed video.");
		return false;
	}

	bool AsyncDecompressorSuccessfulCallback(int pass, sint64 timelinePos, void *pDisplayAsVoid, void *, bool aborting) {
		if (aborting)
			return false;

		IVDVideoDisplay *pVideoDisplay = (IVDVideoDisplay *)pDisplayAsVoid;

		pVideoDisplay->SetSourceMessage(L"Compressed video preview is enabled.\n\nPreview will resume starting with the next key frame.");
		return false;
	}

	bool AsyncDecompressorErrorCallback(int pass, sint64 timelinePos, void *pDisplayAsVoid, void *, bool aborting) {
		if (aborting)
			return false;

		IVDVideoDisplay *pVideoDisplay = (IVDVideoDisplay *)pDisplayAsVoid;

		pVideoDisplay->SetSourceMessage(L"Unable to display compressed video: An error has occurred during decompression.");
		return false;
	}

	bool AsyncDecompressorUpdateCallback(int pass, sint64 timelinePos, void *pDisplayAsVoid, void *pPixmapAsVoid, bool aborting) {
		if (aborting)
			return false;

		IVDVideoDisplay *pVideoDisplay = (IVDVideoDisplay *)pDisplayAsVoid;
		VDPixmap *pPixmap = (VDPixmap *)pPixmapAsVoid;

		pVideoDisplay->SetSource(true, *pPixmap);
		return false;
	}
}

VDDubVideoProcessorDisplay::VDDubVideoProcessorDisplay()
	: mpOptions(NULL)
	, mpVideoCompressor(NULL)
	, mpLoopThrottle(NULL)
	, mbInputLocked(false)
	, mpBlitter(NULL)
	, mpInputDisplay(NULL)
	, mpOutputDisplay(NULL)
	, mRefreshFlag(true)
	, mbVideoDecompressorEnabled(false)
	, mbVideoDecompressorPending(false)
	, mbVideoDecompressorErrored(false)
{
}

VDDubVideoProcessorDisplay::~VDDubVideoProcessorDisplay() {
}

void VDDubVideoProcessorDisplay::SetThreadInfo(VDLoopThrottle *loopThrottle) {
	mpLoopThrottle = loopThrottle;
}

void VDDubVideoProcessorDisplay::SetOptions(const DubOptions *opts) {
	mpOptions = opts;
}

void VDDubVideoProcessorDisplay::SetInputDisplay(IVDVideoDisplay *pVideoDisplay) {
	mpInputDisplay = pVideoDisplay;
}

void VDDubVideoProcessorDisplay::SetOutputDisplay(IVDVideoDisplay *pVideoDisplay) {
	mpOutputDisplay = pVideoDisplay;
}

void VDDubVideoProcessorDisplay::SetBlitter(IVDAsyncBlitter *blitter) {
	mpBlitter = blitter;
}

void VDDubVideoProcessorDisplay::SetVideoCompressor(IVDVideoCompressor *pCompressor) {
	mpVideoCompressor = pCompressor;
}

void VDDubVideoProcessorDisplay::SetVideoSource(IVDVideoSource *pVideo) {
	mpVideoSource = pVideo;
}

sint32 VDDubVideoProcessorDisplay::GetLatency() const {
	return mpBlitter->getFrameDelta();
}

uint32 VDDubVideoProcessorDisplay::GetDisplayClock() const {
	return mpBlitter->getPulseClock();
}

void VDDubVideoProcessorDisplay::AdvanceFrame() {
	mpBlitter->nextFrame(2);
}

bool VDDubVideoProcessorDisplay::TryLockInputChannel(sint32 timeout) {
	if (mbInputLocked) {
		VDASSERT(!"Input channel is already locked.");
		return true;
	}

	if (!mpBlitter->lock(BUFFERID_INPUT, timeout))
		return false;

	mbInputLocked = true;
	return true;
}

void VDDubVideoProcessorDisplay::UnlockInputChannel() {
	if (mbInputLocked) {
		mbInputLocked = false;
		mpBlitter->unlock(BUFFERID_INPUT);
	}
}

bool VDDubVideoProcessorDisplay::TryRevokeOutputBuffer(VDRenderOutputBuffer **buffer) {
	VDVideoDisplayFrame *tmp;
	if (mpOutputDisplay && mpOutputDisplay->RevokeBuffer(false, &tmp)) {
		*buffer = static_cast<VDRenderOutputBuffer *>(tmp);
		return true;
	}

	return false;
}

void VDDubVideoProcessorDisplay::UnlockAndDisplay(bool forceDisplay, VDRenderOutputBuffer *pBuffer, bool outputValid) {
	bool renderFrame = (forceDisplay || mRefreshFlag.xchg(0));
	bool renderInputFrame = renderFrame && mpInputDisplay && mpOptions->video.fShowInputFrame;
	bool renderOutputFrame = (renderFrame || mbVideoDecompressorEnabled) && mpOutputDisplay && mpOptions->video.mode == DubVideoOptions::M_FULL && mpOptions->video.fShowOutputFrame && outputValid;
	bool renderAnyFrame = renderFrame && mpOutputDisplay && (mpOptions->video.fShowInputFrame || mpOptions->video.fShowOutputFrame) && outputValid;
	VDPosition input_frame = -1;
	VDPosition output_frame = -1;
	if (renderInputFrame && !renderOutputFrame)
		input_frame = pBuffer->mTimelineFrame;
	else
		output_frame = pBuffer->mTimelineFrame;

	if (renderInputFrame) {
		mpBlitter->postAPC(BUFFERID_INPUT, input_frame, StaticAsyncUpdateInputCallback, this, 0);
	} else
		mpBlitter->unlock(BUFFERID_INPUT);

	mbInputLocked = false;

	if (renderOutputFrame) {
		if (mbVideoDecompressorEnabled) {
			if (mpVideoDecompressor && !mbVideoDecompressorErrored && !mbVideoDecompressorPending) {
				mpBlitter->lock(BUFFERID_OUTPUT);
				mpBlitter->postAPC(BUFFERID_OUTPUT, output_frame, AsyncDecompressorUpdateCallback, mpOutputDisplay, &mVideoDecompBuffer);
			}
		} else {
			mpLoopThrottle->BeginWait();
			mpBlitter->lock(BUFFERID_OUTPUT);
			mpLoopThrottle->EndWait();
			pBuffer->AddRef();
			mpBlitter->postAPC(BUFFERID_OUTPUT, output_frame, StaticAsyncUpdateOutputCallback, this, pBuffer);
		}
	} else if (renderAnyFrame && !renderInputFrame) {
		mpLoopThrottle->BeginWait();
		mpBlitter->lock(BUFFERID_OUTPUT);
		mpLoopThrottle->EndWait();
		pBuffer->AddRef();
		mpBlitter->postAPC(BUFFERID_OUTPUT, output_frame, StaticAsyncUpdateOutputCallback, this, pBuffer);
	} else
		mpBlitter->unlock(BUFFERID_OUTPUT);
}

void VDDubVideoProcessorDisplay::ScheduleUpdate() {
	mRefreshFlag = 1;
}

void VDDubVideoProcessorDisplay::CheckForDecompressorSwitch() {
	if (!mpOutputDisplay)
		return;

	if (!mpVideoCompressor)
		return;
	
	if (mbVideoDecompressorEnabled == mpOptions->video.fShowDecompressedFrame)
		return;

	mbVideoDecompressorEnabled = mpOptions->video.fShowDecompressedFrame;

	if (mbVideoDecompressorEnabled) {
		mbVideoDecompressorErrored = false;
		mbVideoDecompressorPending = true;

		const VDAVIBitmapInfoHeader *pbih = (const VDAVIBitmapInfoHeader *)mpVideoCompressor->GetOutputFormat();
		mpVideoDecompressor = VDFindVideoDecompressorEx(0, pbih, mpVideoCompressor->GetOutputFormatSize(), VDPreferencesIsPreferInternalVideoDecodersEnabled());

		if (mpVideoDecompressor) {
			if (!mpVideoDecompressor->SetTargetFormat(0))
				mpVideoDecompressor = NULL;
			else {
				try {
					mpVideoDecompressor->Start();

					mpLoopThrottle->BeginWait();
					mpBlitter->lock(BUFFERID_OUTPUT);
					mpLoopThrottle->EndWait();
					mpBlitter->postAPC(BUFFERID_OUTPUT, -1, AsyncDecompressorSuccessfulCallback, mpOutputDisplay, NULL);					

					int format = mpVideoDecompressor->GetTargetFormat();
					int variant = mpVideoDecompressor->GetTargetFormatVariant();

					VDPixmapLayout layout;
					VDMakeBitmapCompatiblePixmapLayout(layout, abs(pbih->biWidth), abs(pbih->biHeight), format, variant, mpVideoDecompressor->GetTargetFormatPalette(), pbih->biSizeImage);

					mVideoDecompBuffer.init(layout);
				} catch(const MyError&) {
					mpVideoDecompressor = NULL;
				}
			}
		}

		if (!mpVideoDecompressor) {
			mpLoopThrottle->BeginWait();
			mpBlitter->lock(BUFFERID_OUTPUT);
			mpLoopThrottle->EndWait();
			mpBlitter->postAPC(BUFFERID_OUTPUT, -1, AsyncDecompressorFailedCallback, mpOutputDisplay, NULL);
		}
	} else {
		if (mpVideoDecompressor) {
			mpLoopThrottle->BeginWait();
			mpBlitter->lock(BUFFERID_OUTPUT);
			mpLoopThrottle->EndWait();
			mpBlitter->unlock(BUFFERID_OUTPUT);
			mpVideoDecompressor->Stop();
			mpVideoDecompressor = NULL;
		}
		mpBlitter->postAPC(0, -1, AsyncReinitDisplayCallback, this, NULL);
	}
}

void VDDubVideoProcessorDisplay::UpdateDecompressedVideo(const void *data, uint32 size, bool isKey) {
	if (!mpVideoDecompressor || mbVideoDecompressorErrored)
		return;

	VDPROFILEBEGIN("V-Decompress");

	if (mbVideoDecompressorPending && isKey) {
		mbVideoDecompressorPending = false;
	}

	if (!mbVideoDecompressorPending && size) {
		try {
			memset((char *)data + size, 0xA5, kDecodeOverflowWorkaroundSize);
			mpVideoDecompressor->DecompressFrame(mVideoDecompBuffer.base(), (char *)data, size, isKey, false);
			if(mpVideoDecompressor->GetAlpha()) mVideoDecompBuffer.info.alpha_type = FilterModPixmapInfo::kAlphaMask;
			int variant = mpVideoDecompressor->GetTargetFormatVariant();
			VDSetPixmapInfoForBitmap(mVideoDecompBuffer.info, mVideoDecompBuffer.format, variant);
		} catch(const MyError&) {
			mpBlitter->postAPC(0, -1, AsyncDecompressorErrorCallback, mpOutputDisplay, NULL);
			mbVideoDecompressorErrored = true;
		}
	}

	VDPROFILEEND();
}

bool VDDubVideoProcessorDisplay::AsyncReinitDisplayCallback(int pass, sint64 timelinePos, void *pThisAsVoid, void *, bool aborting) {
	if (aborting)
		return false;

	VDDubVideoProcessorDisplay *pThis = (VDDubVideoProcessorDisplay *)pThisAsVoid;
	pThis->mpOutputDisplay->Reset();
	return false;
}

bool VDDubVideoProcessorDisplay::StaticAsyncUpdateOutputCallback(int pass, sint64 timelinePos, void *pThisAsVoid, void *pBuffer, bool aborting) {
	return ((VDDubVideoProcessorDisplay *)pThisAsVoid)->AsyncUpdateOutputCallback(pass, timelinePos, (VDRenderOutputBuffer *)pBuffer, aborting);
}

bool VDDubVideoProcessorDisplay::StaticAsyncUpdateInputCallback(int pass, sint64 timelinePos, void *pThisAsVoid, void *pos, bool aborting) {
	return ((VDDubVideoProcessorDisplay *)pThisAsVoid)->AsyncUpdateInputCallback(pass, timelinePos, aborting);
}

bool VDDubVideoProcessorDisplay::AsyncUpdateOutputCallback(int pass, VDPosition timelinePos, VDRenderOutputBuffer *pBuffer, bool aborting) {
	if (aborting) {
		if (pBuffer)
			pBuffer->Release();

		return false;
	}

	if (timelinePos!=-1)
		mpStatusHandler->NotifyPositionChange(timelinePos);

	IVDVideoDisplay *pVideoDisplay = mpOutputDisplay;
	int nFieldMode = mpOptions->video.previewFieldMode;

	uint32 baseFlags = IVDVideoDisplay::kVisibleOnly | IVDVideoDisplay::kDoNotCache;

	if (g_prefs.fDisplay & Preferences::kDisplayEnableVSync)
		baseFlags |= IVDVideoDisplay::kVSync;

	if (pBuffer) {
		if (nFieldMode) {
			switch(nFieldMode) {
				case DubVideoOptions::kPreviewFieldsWeaveTFF:
					pBuffer->mFlags = baseFlags | IVDVideoDisplay::kEvenFieldOnly | IVDVideoDisplay::kAutoFlipFields;
					break;

				case DubVideoOptions::kPreviewFieldsWeaveBFF:
					pBuffer->mFlags = baseFlags | IVDVideoDisplay::kOddFieldOnly | IVDVideoDisplay::kAutoFlipFields;
					break;

				case DubVideoOptions::kPreviewFieldsBobTFF:
					pBuffer->mFlags = baseFlags | IVDVideoDisplay::kBobEven | IVDVideoDisplay::kAllFields | IVDVideoDisplay::kAutoFlipFields;
					break;

				case DubVideoOptions::kPreviewFieldsBobBFF:
					pBuffer->mFlags = baseFlags | IVDVideoDisplay::kBobOdd | IVDVideoDisplay::kAllFields | IVDVideoDisplay::kAutoFlipFields;
					break;

				case DubVideoOptions::kPreviewFieldsNonIntTFF:
					pBuffer->mFlags = baseFlags | IVDVideoDisplay::kEvenFieldOnly | IVDVideoDisplay::kAutoFlipFields | IVDVideoDisplay::kSequentialFields;
					break;

				case DubVideoOptions::kPreviewFieldsNonIntBFF:
					pBuffer->mFlags = baseFlags | IVDVideoDisplay::kOddFieldOnly | IVDVideoDisplay::kAutoFlipFields | IVDVideoDisplay::kSequentialFields;
					break;
			}

			pBuffer->mbInterlaced = true;
		} else {
			pBuffer->mFlags = IVDVideoDisplay::kAllFields | baseFlags;
			pBuffer->mbInterlaced = false;
		}

		pBuffer->mbAllowConversion = true;

		pVideoDisplay->PostBuffer(pBuffer);
		pBuffer->Release();
		return false;
	}

	if (nFieldMode) {
		if (nFieldMode == 2) {
			if (pass)
				pVideoDisplay->Update(IVDVideoDisplay::kEvenFieldOnly | baseFlags);
			else
				pVideoDisplay->Update(IVDVideoDisplay::kOddFieldOnly | IVDVideoDisplay::kFirstField | baseFlags);
		} else {
			if (pass)
				pVideoDisplay->Update(IVDVideoDisplay::kOddFieldOnly | baseFlags);
			else
				pVideoDisplay->Update(IVDVideoDisplay::kEvenFieldOnly | IVDVideoDisplay::kFirstField | baseFlags);
		}

		return !pass;
	} else {
		pVideoDisplay->Update(IVDVideoDisplay::kAllFields | baseFlags);
		return false;
	}
}

bool VDDubVideoProcessorDisplay::AsyncUpdateInputCallback(int pass, VDPosition timelinePos, bool aborting) {
	if (aborting)
		return false;

	if (timelinePos!=-1)
		mpStatusHandler->NotifyPositionChange(timelinePos);

	IVDVideoDisplay *pVideoDisplay = mpInputDisplay;
	int nFieldMode = mpOptions->video.previewFieldMode;

	const VDPixmap& px = mpVideoSource->getTargetFormat();
	pVideoDisplay->SetSource(false, px, NULL, 0, true, nFieldMode>0);

	uint32 baseFlags = IVDVideoDisplay::kVisibleOnly;

	if (g_prefs.fDisplay & Preferences::kDisplayEnableVSync)
		baseFlags |= IVDVideoDisplay::kVSync;

	if (nFieldMode) {
		if ((nFieldMode - 1) & 1) {
			if (pass)
				pVideoDisplay->Update(IVDVideoDisplay::kEvenFieldOnly | baseFlags);
			else
				pVideoDisplay->Update(IVDVideoDisplay::kOddFieldOnly | IVDVideoDisplay::kFirstField | baseFlags);
		} else {
			if (pass)
				pVideoDisplay->Update(IVDVideoDisplay::kOddFieldOnly | baseFlags);
			else
				pVideoDisplay->Update(IVDVideoDisplay::kEvenFieldOnly | IVDVideoDisplay::kFirstField | baseFlags);
		}

		return !pass;
	} else {
		pVideoDisplay->Update(IVDVideoDisplay::kAllFields | baseFlags);
		return false;
	}
}

