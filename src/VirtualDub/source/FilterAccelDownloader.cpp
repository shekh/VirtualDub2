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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

#include "stdafx.h"
#include <vd2/Kasumi/pixmaputils.h>
#include "FilterAccelDownloader.h"
#include "FilterAccelEngine.h"
#include "FilterFrameBufferAccel.h"
#include "FilterSystem.h"

void VDFilterSetAccelError(VDFilterAccelStatus status, VDFilterFrameRequest *req) {
	if (status == kVDFilterAccelStatus_DeviceLost) {
		vdrefptr<VDFilterFrameRequestError> err(new_nothrow VDFilterFrameRequestError);

		if (err) {
			err->mError = "A video filter operation has failed because the 3D graphics accelerator is no longer available.";
			req->SetError(err);
		}
	}
}

struct VDFilterAccelDownloader::CallbackMsg : public VDFilterAccelEngineMessage {
	VDFilterAccelDownloader *mpThis;
};

VDFilterAccelDownloader::VDFilterAccelDownloader()
	: mpReadbackBuffer(NULL)
{
}

VDFilterAccelDownloader::~VDFilterAccelDownloader() {
	if (mpRequest) {
		mpEngine->WaitForCall(&mDownloadMsg);
		mpRequest = NULL;
	}

	if (mpReadbackBuffer) {
		CallbackMsg msg;
		msg.mpThis = this;
		msg.mpCallback = StaticShutdownCallback;
		mpEngine->SyncCall(&msg);
	}
}

void VDFilterAccelDownloader::Init(VDFilterAccelEngine *engine, IVDFilterFrameSource *source, const VDPixmapLayout& outputLayout, const VDPixmapLayout *sourceLayoutOverride) {
	mpEngine = engine;
	mpSource = source;
	mSourceLayout = sourceLayoutOverride ? *sourceLayoutOverride : source->GetOutputLayout();
	SetOutputLayout(outputLayout);

	CallbackMsg msg;
	msg.mpThis = this;
	msg.mpCallback = StaticInitCallback;
	mpEngine->SyncCall(&msg);

	if (!mpReadbackBuffer)
		throw MyError("Unable to allocate 3D acceleration readback buffer for frame size: %ux%u", outputLayout.w, outputLayout.h);
}

void VDFilterAccelDownloader::Start(IVDFilterFrameEngine *frameEngine) {
	mpFrameEngine = frameEngine;
}

void VDFilterAccelDownloader::Stop() {
	if (mpRequest) {
		VDFilterAccelStatus status = mpEngine->EndDownload(&mDownloadMsg);

		VDFilterSetAccelError(status, mpRequest);
		mpRequest->MarkComplete(status == kVDFilterAccelStatus_OK);
		CompleteRequest(mpRequest, true);
		mpRequest = NULL;
	}
}

bool VDFilterAccelDownloader::GetDirectMapping(sint64 outputFrame, sint64& sourceFrame, int& sourceIndex) {
	return mpSource->GetDirectMapping(outputFrame, sourceFrame, sourceIndex);
}

sint64 VDFilterAccelDownloader::GetSourceFrame(sint64 outputFrame) {
	return outputFrame;
}

sint64 VDFilterAccelDownloader::GetSymbolicFrame(sint64 outputFrame, IVDFilterFrameSource *source) {
	if (source == this)
		return outputFrame;

	return mpSource->GetSymbolicFrame(outputFrame, source);
}

sint64 VDFilterAccelDownloader::GetNearestUniqueFrame(sint64 outputFrame) {
	return mpSource->GetNearestUniqueFrame(outputFrame);
}

VDFilterAccelDownloader::RunResult VDFilterAccelDownloader::RunRequests(const uint32 *batchNumberLimit) {
	if (mpRequest) {
		if (!mDownloadMsg.mbCompleted)
			return kRunResult_Blocked;

		VDFilterAccelStatus status = mpEngine->EndDownload(&mDownloadMsg);

		VDFilterSetAccelError(status, mpRequest);

		mpRequest->MarkComplete(status == kVDFilterAccelStatus_OK);
		CompleteRequest(mpRequest, true);
		mpRequest.clear();

		// We need to exit to give the filter system a chance to schedule downstreamm filters.
		return kRunResult_Running;
	}

	if (!GetNextRequest(batchNumberLimit, ~mpRequest))
		return kRunResult_Idle;

	VDFilterFrameBufferAccel *srcbuf = static_cast<VDFilterFrameBufferAccel *>(mpRequest->GetSource(0));
	if (!srcbuf) {
		IVDFilterFrameClientRequest *creq0 = mpRequest->GetSourceRequest(0);

		if (creq0)
			mpRequest->SetError(creq0->GetError());

		mpRequest->MarkComplete(false);
		CompleteRequest(mpRequest, false);
		mpRequest = NULL;
		return kRunResult_Running;
	}

	if (!AllocateRequestBuffer(mpRequest)) {
		vdrefptr<VDFilterFrameRequestError> err(new_nothrow VDFilterFrameRequestError);

		if (err) {
			err->mError = "Unable to allocate a video frame buffer.";
			mpRequest->SetError(err);
		}

		mpRequest->MarkComplete(false);
		CompleteRequest(mpRequest, false);
		mpRequest = NULL;
		return kRunResult_Running;
	}

	VDFilterFrameBuffer *dstbuf = mpRequest->GetResultBuffer();
	mpEngine->BeginDownload(&mDownloadMsg, dstbuf, mLayout, srcbuf, mSourceLayout.format == nsVDXPixmap::kPixFormat_VDXA_YUV, mpReadbackBuffer);

	mDownloadMsg.mpCompleteSignal = &mCompletedSignal;
	mDownloadMsg.mpCleanup = StaticCleanupCallback;
	mDownloadMsg.mpDownloader = this;
	mpEngine->PostCall(&mDownloadMsg);

	return kRunResult_Blocked;
}

bool VDFilterAccelDownloader::InitNewRequest(VDFilterFrameRequest *req, sint64 outputFrame, bool writable, uint32 batchNumber) {
	vdrefptr<IVDFilterFrameClientRequest> creq;
	if (!mpSource->CreateRequest(outputFrame, false, batchNumber, ~creq))
		return false;

	req->SetSourceCount(1);
	req->SetSourceRequest(0, creq);
	return true;
}

void VDFilterAccelDownloader::StaticInitCallback(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message) {
	CallbackMsg& msg = *static_cast<CallbackMsg *>(message);

	msg.mpThis->mpReadbackBuffer = msg.mpThis->mpEngine->CreateReadbackBuffer(msg.mpThis->mLayout.w, msg.mpThis->mLayout.h, msg.mpThis->mSourceLayout.format);
}

void VDFilterAccelDownloader::StaticShutdownCallback(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message) {
	CallbackMsg& msg = *static_cast<CallbackMsg *>(message);

	if (msg.mpThis->mpReadbackBuffer) {
		msg.mpThis->mpEngine->DestroyReadbackBuffer(msg.mpThis->mpReadbackBuffer);
		msg.mpThis->mpReadbackBuffer = NULL;
	}
}

void VDFilterAccelDownloader::StaticCleanupCallback(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message) {
	DownloadMsg& msg = *static_cast<DownloadMsg *>(message);

	msg.mpDownloader->mpFrameEngine->Schedule();
}
