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

#include "stdafx.h"

#include <vd2/system/error.h>
#include <vd2/system/profile.h>
#include <vd2/system/time.h>
#include <vd2/system/log.h>
#include <vd2/Riza/audiocodec.h>
#include <vd2/Riza/audioout.h>
#include <vd2/Dita/resources.h>

#include "AVIOutput.h"
#include "AVIOutputPreview.h"

const VDStringW& VDPreferencesGetAudioPlaybackDeviceKey();

///////////////////////////////////////////////////////////////////////////

AVIAudioPreviewOutputStream::AVIAudioPreviewOutputStream()
	: mpAudioOut(VDCreateAudioOutputWaveOutW32())
	, mbInitialized(false)
	, mbStarted(false)
	, mbVBRMode(false)
	, mTotalSamples(0)
	, mTotalBytes(0)
	, mBufferLevel(0)
	, mLastPlayTime(0)
	, mLastCPUTime(VDGetAccurateTick())
	, mbFinished(false)
{
	VDRTProfiler *profiler = VDGetRTProfiler();
	if (profiler) {
		profiler->RegisterCounterU32("AudioPreview/BufferLevel", &mBufferLevel);
	}
}

AVIAudioPreviewOutputStream::~AVIAudioPreviewOutputStream() {
	VDRTProfiler *profiler = VDGetRTProfiler();
	if (profiler) {
		profiler->UnregisterCounter(&mBufferLevel);
	}
}

uint32 AVIAudioPreviewOutputStream::GetPreviewTime() {
	uint32 cpuTime = VDGetAccurateTick();
	sint32 t = mpAudioOut->GetPosition();
	uint32 t2 = t < 0 ? 0 : t;

	if (t2 == mLastPlayTime && mbFinished) {
		t2 += (cpuTime - mLastCPUTime);

		if (t2 < mLastPlayTime)
			t2 = mLastPlayTime;

		return t2;
	}

	if (t2 < mLastPlayTime)
		t2 = mLastPlayTime;

	mLastPlayTime = t2;
	mLastCPUTime = cpuTime;

	return t2;
}

void AVIAudioPreviewOutputStream::initAudio() {
	const VDWaveFormat *pwfex = (const VDWaveFormat *)getFormat();
	int blocks;
	int blocksin512;

	// Figure out what a 'good' buffer size is.
	// About a 5th of a second sounds good.

	blocks = (pwfex->mDataRate/5 + pwfex->mBlockSize/2) / pwfex->mBlockSize;

	// How many blocks for 512 bytes?  We don't want buffers smaller than that.

	blocksin512 = (512 + pwfex->mBlockSize - 1) / pwfex->mBlockSize;

	// Use the smaller value and allocate.

	if (!mpAudioOut->Init(std::max<int>(blocks, blocksin512)*pwfex->mBlockSize, 10, (const tWAVEFORMATEX *)pwfex, VDPreferencesGetAudioPlaybackDeviceKey().c_str())) {
		mpAudioOut->GoSilent();
		mbFinished = true;
		VDLogAppMessage(kVDLogInfo, 1, 13); // kVDST_Dub
	}
}

void AVIAudioPreviewOutputStream::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples) {
	partialWriteBegin(flags, cbBuffer, samples);
	partialWrite(pBuffer, cbBuffer);
	partialWriteEnd();
}

void AVIAudioPreviewOutputStream::partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples) {
	if (!mbInitialized) {
		initAudio();
		mbInitialized = true;
	}

	mTotalSamples += samples;
	mTotalBytes += bytes;

	if (!samples) {
		VDASSERT(!bytes);
		return;
	}
}

void AVIAudioPreviewOutputStream::partialWrite(const void *pBuffer, uint32 cbBuffer) {
	mpAudioOut->Write(pBuffer, cbBuffer);
	mBufferLevel = mpAudioOut->GetBufferLevel();
}

void AVIAudioPreviewOutputStream::partialWriteEnd() {
}

bool AVIAudioPreviewOutputStream::isSilent() {
	return mpAudioOut == NULL || mpAudioOut->IsSilent();
}

void AVIAudioPreviewOutputStream::start() {
	if (mbStarted)
		return;

	if (!mbInitialized) {
		initAudio();
		mbInitialized = true;
	}

	if (!mpAudioOut->Start()) {
		delete mpAudioOut;
		mpAudioOut = NULL;
	}

	mbStarted = true;
}

void AVIAudioPreviewOutputStream::stop() {
	if (mbStarted && mpAudioOut)
		mpAudioOut->Stop();

	mbStarted = false;

}

void AVIAudioPreviewOutputStream::flush() {
	if (mpAudioOut && mbStarted)
		mpAudioOut->Flush();
}

void AVIAudioPreviewOutputStream::finish() {
	mbFinished = true;
}

void AVIAudioPreviewOutputStream::finalize() {
	if (mpAudioOut && mbStarted)
		mpAudioOut->Finalize();
}

double AVIAudioPreviewOutputStream::GetPosition() {
	if (!mpAudioOut)
		return -1;

	return mpAudioOut->GetPositionTime();
}

long AVIAudioPreviewOutputStream::getAvailable() {
	return mpAudioOut ? mpAudioOut->GetAvailSpace() : -1;
}

bool AVIAudioPreviewOutputStream::isFrozen() {
	return mpAudioOut ? mpAudioOut->IsFrozen() : true;
}

///////////////////////////////////////////////////////////////////////////

class AVIVideoPreviewOutputStream : public AVIOutputStream, public IVDVideoImageOutputStream {
public:
	void *AsInterface(uint32 id);

	void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples) {}
	void partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples) {}
	void partialWrite(const void *pBuffer, uint32 cbBuffer) {}
	void partialWriteEnd() {}
	void WriteVideoImage(const VDPixmap *px) {}
};

void *AVIVideoPreviewOutputStream::AsInterface(uint32 id) {
	if (id == IVDVideoImageOutputStream::kTypeID)
		return static_cast<IVDVideoImageOutputStream *>(this);

	return AVIOutputStream::AsInterface(id);
}

/////////////////////////////

AVIOutputPreview::AVIOutputPreview() {
}

AVIOutputPreview::~AVIOutputPreview() {
}

IVDMediaOutputStream *AVIOutputPreview::createVideoStream() {
	VDASSERT(!videoOut);
	if (!(videoOut = new_nothrow AVIVideoPreviewOutputStream()))
		throw MyMemoryError();
	return videoOut;
}

IVDMediaOutputStream *AVIOutputPreview::createAudioStream() {
	VDASSERT(!audioOut);
	if (!(audioOut = new_nothrow AVIAudioPreviewOutputStream()))
		throw MyMemoryError();
	return audioOut;
}

bool AVIOutputPreview::init(const wchar_t *szFile) {
	return true;
}

void AVIOutputPreview::finalize() {
	if (audioOut)
		static_cast<AVIAudioPreviewOutputStream *>(audioOut)->finalize();
}
