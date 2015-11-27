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
#include <vd2/Kasumi/blitter.h>
#include <../Kasumi/h/uberblit_rgb64.h>

#include "AVIOutputRawVideo.h"

extern uint32 VDPreferencesGetFileAsyncDefaultMode();

//////////////////////////////////////////////////////////////////////
//
// AVIVideoOutputStreamRaw
//
//////////////////////////////////////////////////////////////////////

class AVIVideoOutputStreamRaw : public AVIOutputStream, public IVDVideoImageOutputStream {
public:
	AVIVideoOutputStreamRaw(AVIOutputRawVideo *pParent, const VDAVIOutputRawVideoFormat& format, uint32 w, uint32 h);

	void *AsInterface(uint32 id);

	void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples);
	void partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples);
	void partialWrite(const void *pBuffer, uint32 cbBuffer);
	void partialWriteEnd();

	void WriteVideoImage(const VDPixmap *px);

protected:
	AVIOutputRawVideo *const mpParent;
	vdblock<uint8, vdaligned_alloc<uint8> > mOutputBuffer;
	VDPixmap mOutputPixmap;

	const VDAVIOutputRawVideoFormat& mFormat;
	vdautoptr<IVDPixmapBlitter> mpBlitter;
};

AVIVideoOutputStreamRaw::AVIVideoOutputStreamRaw(AVIOutputRawVideo *pParent, const VDAVIOutputRawVideoFormat& format, uint32 w, uint32 h)
	: mpParent(pParent)
	, mFormat(format)
{
	VDPixmapLayout layout;
	uint32 linsize = VDPixmapCreateLinearLayout(layout, format.mOutputFormat, w, h, format.mScanlineAlignment);

	if (format.mbSwapChromaPlanes) {
		std::swap(layout.data2, layout.data3);
		std::swap(layout.pitch2, layout.pitch3);
	}

	if (format.mbBottomUp)
		VDPixmapLayoutFlipV(layout);

	mOutputBuffer.resize(linsize, 0);
	mOutputPixmap = VDPixmapFromLayout(layout, mOutputBuffer.data());
}

void *AVIVideoOutputStreamRaw::AsInterface(uint32 id) {
	if (id == IVDVideoImageOutputStream::kTypeID)
		return static_cast<IVDVideoImageOutputStream *>(this);

	return AVIOutputStream::AsInterface(id);
}

void AVIVideoOutputStreamRaw::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples) {
	throw MyError("Raw writes are not supported.");
}

void AVIVideoOutputStreamRaw::partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples) {
	throw MyError("Partial writes are not supported.");
}

void AVIVideoOutputStreamRaw::partialWrite(const void *pBuffer, uint32 cbBuffer) {
	throw MyError("Partial writes are not supported.");
}

void AVIVideoOutputStreamRaw::partialWriteEnd() {
	throw MyError("Partial writes are not supported.");
}

void AVIVideoOutputStreamRaw::WriteVideoImage(const VDPixmap *px) {
	if (!mpBlitter) {
		mpBlitter = VDPixmapCreateBlitter(mOutputPixmap, *px);
		if (!mpBlitter)
			throw MyMemoryError();
	}

	mpBlitter->Blit(mOutputPixmap, *px);

	bool wipe_alpha = true;
	if (px->info.alpha_type!=FilterModPixmapInfo::kAlphaInvalid)
		wipe_alpha = false;

	if (wipe_alpha && mOutputPixmap.format==nsVDPixmap::kPixFormat_XRGB8888) {
		char *row = (char*)mOutputPixmap.data;
		for(int y=0; y<mOutputPixmap.h; ++y) {
			uint8 *p = (uint8*)row;
			for(int x=0; x<mOutputPixmap.w; ++x) {
				p[3] = 0xFF;
				p += 4;
			}
			row += mOutputPixmap.pitch;
		}
	}

	if (wipe_alpha && mOutputPixmap.format==nsVDPixmap::kPixFormat_XRGB64) {
		if (!VDPixmap_X16R16G16B16_IsNormalized(mOutputPixmap.info)) 
			VDPixmap_X16R16G16B16_Normalize(mOutputPixmap,mOutputPixmap);

		if (wipe_alpha) {
			char *row = (char*)mOutputPixmap.data;
			for(int y=0; y<mOutputPixmap.h; ++y) {
				uint16 *p = (uint16*)row;
				for(int x=0; x<mOutputPixmap.w; ++x) {
					p[3] = 0xFFFF;
					p += 4;
				}
				row += mOutputPixmap.pitch;
			}
		}
	}

	mpParent->write(mOutputBuffer.data(), mOutputBuffer.size());
}

//////////////////////////////////////////////////////////////////////
//
// AVIOutputRawVideo
//
//////////////////////////////////////////////////////////////////////

AVIOutputRawVideo::AVIOutputRawVideo(const VDAVIOutputRawVideoFormat& format)
	: mFormat(format)
{
	mBytesWritten		= 0;
	mBufferSize			= 65536;
}

AVIOutputRawVideo::~AVIOutputRawVideo() {
}

void AVIOutputRawVideo::SetInputLayout(const VDPixmapLayout& layout) {
	mInputLayout = layout;
}

IVDMediaOutputStream *AVIOutputRawVideo::createVideoStream() {
	VDASSERT(!videoOut);
	if (!(videoOut = new_nothrow AVIVideoOutputStreamRaw(this, mFormat, mInputLayout.w, mInputLayout.h)))
		throw MyMemoryError();
	return videoOut;
}

IVDMediaOutputStream *AVIOutputRawVideo::createAudioStream() {
	return NULL;
}

bool AVIOutputRawVideo::init(const wchar_t *pwszFile) {
	mpFileAsync = VDCreateFileAsync((IVDFileAsync::Mode)VDPreferencesGetFileAsyncDefaultMode());
	mpFileAsync->Open(pwszFile, 2, mBufferSize >> 1);

	mBytesWritten = 0;
	mbDoTruncate = true;
	return true;
}

bool AVIOutputRawVideo::init(VDFileHandle h) {
	mpFileAsync = VDCreateFileAsync((IVDFileAsync::Mode)VDPreferencesGetFileAsyncDefaultMode());
	mpFileAsync->Open(h, 2, mBufferSize >> 1);

	mBytesWritten = 0;
	mbDoTruncate = false;
	return true;
}

void AVIOutputRawVideo::finalize() {
	if (!mpFileAsync->IsOpen())
		return;

	mpFileAsync->FastWriteEnd();
	if (mbDoTruncate)
		mpFileAsync->Truncate(mBytesWritten);
	mpFileAsync->Close();
}

void AVIOutputRawVideo::write(const void *pBuffer, uint32 cbBuffer) {
	mpFileAsync->FastWrite(pBuffer, cbBuffer);
	mBytesWritten += cbBuffer;
}
