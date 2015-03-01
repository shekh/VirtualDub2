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
#include <vd2/Kasumi/pixmaputils.h>
#include "FilterFrameVideoSource.h"
#include "VideoSource.h"

VDFilterFrameVideoSource::VDFilterFrameVideoSource()
	: mpVS(NULL)
	, mpRequest(NULL)
{
}

VDFilterFrameVideoSource::~VDFilterFrameVideoSource() {
	if (mpRequest) {
		mpRequest->MarkComplete(false);
		mpRequest->Release();
		mpRequest = NULL;
	}

	if (mpVS)
		mpVS->streamDisown(this);
}

void VDFilterFrameVideoSource::Init(IVDVideoSource *vs, const VDPixmapLayout& layout) {
	mpVS = vs;
	mpSS = vs->asStream();
	mDecodePadding = vs->streamGetDecodePadding();

	SetOutputLayout(layout);
}

VDFilterFrameVideoSource::RunResult VDFilterFrameVideoSource::RunRequests(const uint32 *batchNumberLimit) {
	if (!mpVS->streamOwn(this)) {
		mpVS->streamBegin(false, true);
	}

	try {
		bool activity = false;

		if (mpRequest && !mpRequest->IsActive()) {
			mpRequest->MarkComplete(false);
			CompleteRequest(mpRequest, false);
			mpRequest->Release();
			mpRequest = NULL;

			activity = true;
		}

		if (!mpRequest) {
			if (!GetNextRequest(batchNumberLimit, &mpRequest))
				return activity ? kRunResult_IdleWasActive : kRunResult_Idle;

			VDVERIFY(AllocateRequestBuffer(mpRequest));

			VDPosition frame = mpRequest->GetTiming().mOutputFrame;

			VDPosition limit = mpVS->asStream()->getLength();
			if (frame >= limit)
				frame = limit - 1;

			if (frame < 0)
				frame = 0;

			mTargetSample = mpVS->displayToStreamOrder(frame);
			mpVS->streamSetDesiredFrame(frame);
			mbFirstSample = true;
		}

		bool preroll;

		VDPosition pos = mpVS->streamGetNextRequiredFrame(preroll);

		if (pos < 0) {
			if (mbFirstSample)
				mpVS->streamGetFrame(NULL, 0, false, -1, mTargetSample);

			VDFilterFrameBuffer *buf = mpRequest->GetResultBuffer();
			const VDPixmap& pxsrc = mpVS->getTargetFormat();
			const VDPixmap& pxdst = VDPixmapFromLayout(mLayout, buf->LockWrite());

			if (!mpBlitter)
				mpBlitter = VDPixmapCreateBlitter(pxdst, pxsrc);

			mpBlitter->Blit(pxdst, pxsrc);

			buf->Unlock();

			mpRequest->MarkComplete(true);
			CompleteRequest(mpRequest, true);
			mpRequest->Release();
			mpRequest = NULL;
			return kRunResult_Running;
		}

		IVDStreamSource *ss = mpVS->asStream();
		uint32 bytes;
		uint32 samples;
		uint32 bufferSize = mBuffer.size();
		int result = IVDStreamSource::kBufferTooSmall;

		if (bufferSize && bufferSize >= mDecodePadding)
			result = ss->read(pos, 1, mBuffer.data(), bufferSize - mDecodePadding, &bytes, &samples);

		if (result == IVDStreamSource::kBufferTooSmall) {
			ss->read(pos, 1, NULL, 0, &bytes, &samples);

			if (bytes == 0)
				bytes = 1;

			mBuffer.resize(bytes + mDecodePadding);

			result = ss->read(pos, 1, mBuffer.data(), mBuffer.size() - mDecodePadding, &bytes, &samples);
			if (result)
				throw MyAVIError("Video frame read", result);
		}

		mpVS->streamFillDecodePadding(mBuffer.data(), bytes);
		mpVS->streamGetFrame(mBuffer.data(), bytes, preroll, pos, mTargetSample);
		mbFirstSample = false;
	} catch(const MyError& e) {
		if (mpRequest) {
			vdrefptr<VDFilterFrameRequestError> err(new_nothrow VDFilterFrameRequestError);
			if (err)
				err->mError.sprintf("Error reading source frame %lld: %s", mpRequest->GetTiming().mOutputFrame, e.gets());

			mpRequest->SetError(err);
			mpRequest->MarkComplete(false);
			CompleteRequest(mpRequest, false);
			mpRequest->Release();
			mpRequest = NULL;
		}
	}

	return kRunResult_Running;
}

sint64 VDFilterFrameVideoSource::GetNearestUniqueFrame(sint64 outputFrame) {
	outputFrame = mpVS->getRealDisplayFrame(outputFrame);

	if (outputFrame < 0)
		return 0;

	return outputFrame;
}
