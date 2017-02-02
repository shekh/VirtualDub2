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
#include <vd2/system/fileasync.h>
#include <vd2/system/binary.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Riza/bitmap.h>
#include "AVIOutput.h"
#include "AVIOutputFLM.h"

extern uint32 VDPreferencesGetFileAsyncDefaultMode();
extern uint32& VDPreferencesGetRenderOutputBufferSize();

////////////////////////////////////

namespace {
	struct FilmstripHeader {
		sint32	signature;
		sint32	numFrames;
		sint16	packing;
		sint16	reserved;
		sint16	width;
		sint16	height;
		sint16	leading;
		sint16	framesPerSec;
		char	spare[16];

		void Write(const void *src) {
			FilmstripHeader *src2 = (FilmstripHeader *)src;

			src2->signature		= VDToBE32(signature);
			src2->numFrames		= VDToBE32(numFrames);
			src2->packing		= VDToBE16(packing);
			src2->reserved		= VDToBE16(reserved);
			src2->width			= VDToBE16(width);
			src2->height		= VDToBE16(height);
			src2->leading		= VDToBE16(leading);
			src2->framesPerSec	= VDToBE16(framesPerSec);
			memset(src2->spare, 0, sizeof src2->spare);
		}
	};
}

////////////////////////////////////

class AVIVideoFLMOutputStream : public AVIOutputStream {
public:
	AVIVideoFLMOutputStream(IVDFileAsync *pAsync);
	~AVIVideoFLMOutputStream();

	void init();
	void finalize();
	void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples);
	void partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples);
	void partialWrite(const void *pBuffer, uint32 cbBuffer);
	void partialWriteEnd();

private:
	IVDFileAsync *mpAsync;
	vdblock<char> mPackBuffer;
	uint32			mFrameSize;
	uint32			mExtraLeader;
	uint32			mFrameCount;

	VDPixmapLayout	mSrcLayout;
	VDPixmapLayout	mDstLayout;
};

AVIVideoFLMOutputStream::AVIVideoFLMOutputStream(IVDFileAsync *pAsync)
	: mpAsync(pAsync)
	, mFrameCount(0)
{
}

AVIVideoFLMOutputStream::~AVIVideoFLMOutputStream() {
}

void AVIVideoFLMOutputStream::init() {
	const VDAVIBitmapInfoHeader *bih = (const VDAVIBitmapInfoHeader *)getFormat();
	int variant;

	int format = VDBitmapFormatToPixmapFormat(*bih, variant);
	VDMakeBitmapCompatiblePixmapLayout(mSrcLayout, bih->biWidth, bih->biHeight, format, variant);

	mFrameSize = VDPixmapCreateLinearLayout(mDstLayout, nsVDPixmap::kPixFormat_XRGB8888, bih->biWidth, bih->biHeight, 4);
	mPackBuffer.resize(mFrameSize);

	mExtraLeader = 0;

	if (mPackBuffer.size() < sizeof(FilmstripHeader)) {
		mExtraLeader = sizeof(FilmstripHeader) - mPackBuffer.size();

		mExtraLeader += mDstLayout.pitch - 1;
		mExtraLeader -= mExtraLeader % mDstLayout.pitch;
	}
}

void AVIVideoFLMOutputStream::finalize() {
	FilmstripHeader hdr;
	const VDXAVIStreamHeader& streamInfo = this->streamInfo.aviHeader;

	hdr.signature		= 0x52616e64;
	hdr.numFrames		= mFrameCount;
	hdr.packing			= 0;
	hdr.reserved		= 0;
	hdr.width			= (sint16)mDstLayout.w;
	hdr.height			= (sint16)mDstLayout.h;
	hdr.leading			= 0;
	hdr.framesPerSec	= (sint16)VDRoundToInt((double)streamInfo.dwRate / (double)streamInfo.dwScale);
	if (!hdr.framesPerSec)
		hdr.framesPerSec = 1;

	char buf[sizeof(FilmstripHeader)];
	hdr.Write(buf);

	mpAsync->FastWrite(NULL, mFrameSize - sizeof buf);
	mpAsync->FastWrite(&buf, sizeof buf);
	sint64 pos = mpAsync->GetFastWritePos();
	mpAsync->FastWriteEnd();
	mpAsync->Truncate(pos);
	mpAsync->Close();
}

void AVIVideoFLMOutputStream::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples) {
	VDPixmap pxsrc(VDPixmapFromLayout(mSrcLayout, (void *)pBuffer));
	VDPixmap pxdst(VDPixmapFromLayout(mDstLayout, mPackBuffer.data()));

	VDPixmapBlt(pxdst, pxsrc);

	// Swizzle the frame from BGRA to RGBA order.
	uint8 *p = (uint8 *)mPackBuffer.data();
	uint32 count = mPackBuffer.size() >> 2;

	do {
		uint8 r = p[0];
		uint8 b = p[2];

		p[0] = b;
		p[2] = r;
		p += 4;
	} while(--count);

	mpAsync->FastWrite(mPackBuffer.data(), mPackBuffer.size());
	mpAsync->FastWrite(NULL, mExtraLeader);
	++mFrameCount;
}

void AVIVideoFLMOutputStream::partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples) {
	throw MyError("Partial write operations are not supported for video streams.");
}

void AVIVideoFLMOutputStream::partialWrite(const void *pBuffer, uint32 cbBuffer) {
}

void AVIVideoFLMOutputStream::partialWriteEnd() {
}

//////////////////////////////////

AVIOutputFLM::AVIOutputFLM()
: mpAsync(VDCreateFileAsync((IVDFileAsync::Mode)VDPreferencesGetFileAsyncDefaultMode()))
{
}

IVDMediaOutputStream *AVIOutputFLM::createVideoStream() {
	VDASSERT(!videoOut);
	if (!(videoOut = new_nothrow AVIVideoFLMOutputStream(mpAsync)))
		throw MyMemoryError();
	return videoOut;
}

IVDMediaOutputStream *AVIOutputFLM::createAudioStream() {
	return NULL;
}

bool AVIOutputFLM::init(const wchar_t *szFile) {
	uint32 bufsize = VDPreferencesGetRenderOutputBufferSize();
	mpAsync->Open(szFile, 2, bufsize >> 1);

	if (!videoOut)
		return false;

	static_cast<AVIVideoFLMOutputStream *>(videoOut)->init();

	return true;
}

void AVIOutputFLM::finalize() {
	if (videoOut)
		static_cast<AVIVideoFLMOutputStream *>(videoOut)->finalize();
}
