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
#include "FilterAccelUploader.h"
#include "FilterAccelEngine.h"
#include "FilterFrameBufferAccel.h"

VDFilterAccelUploader::VDFilterAccelUploader()
	: mProcessStatus(kProcess_Idle)
	, mpLockedSrc(NULL)
{
}

VDFilterAccelUploader::~VDFilterAccelUploader() {
}

void VDFilterAccelUploader::Init(VDFilterAccelEngine *engine, IVDFilterFrameSource *source, const VDPixmapLayout& outputLayout, const VDPixmapLayout *sourceLayoutOverride) {
	mpEngine = engine;
	mpSource = source;
	mSourceLayout = sourceLayoutOverride ? *sourceLayoutOverride : source->GetOutputLayout();
	mLayout = outputLayout;
	mAllocator.Clear();
	mAllocator.AddSizeRequirement((outputLayout.h << 16) + outputLayout.w);
	mAllocator.SetAccelerationRequirement(VDFilterFrameAllocatorProxy::kAccelModeUpload);

	mProcessStatus = kProcess_Idle;
}

void VDFilterAccelUploader::Start(IVDFilterFrameEngine *frameEngine) {
	mpFrameEngine = frameEngine;
}

void VDFilterAccelUploader::Stop() {
	if (mpRequest) {
		if (mpLockedSrc) {
			VDFilterFrameBuffer *srcbuf = mpRequest->GetSource(0);

			if (srcbuf)
				srcbuf->Unlock();

			mpLockedSrc = NULL;
		}

		mpRequest->MarkComplete(false);
		CompleteRequest(mpRequest, false);
		mpRequest.clear();
	}
}

bool VDFilterAccelUploader::GetDirectMapping(sint64 outputFrame, sint64& sourceFrame, int& sourceIndex) {
	return mpSource->GetDirectMapping(outputFrame, sourceFrame, sourceIndex);
}

sint64 VDFilterAccelUploader::GetSourceFrame(sint64 outputFrame) {
	return outputFrame;
}

sint64 VDFilterAccelUploader::GetSymbolicFrame(sint64 outputFrame, IVDFilterFrameSource *source) {
	if (source == this)
		return outputFrame;

	return mpSource->GetSymbolicFrame(outputFrame, source);
}

sint64 VDFilterAccelUploader::GetNearestUniqueFrame(sint64 outputFrame) {
	return mpSource->GetNearestUniqueFrame(outputFrame);
}

VDFilterAccelUploader::RunResult VDFilterAccelUploader::RunRequests(const uint32 *batchNumberLimit, int index) {
	if (index>0)
		return kRunResult_Idle;

	if (mpRequest) {
		if (mProcessStatus == kProcess_Pending)
			return kRunResult_Blocked;

		VDASSERT(mpLockedSrc);
		VDFilterFrameBuffer *srcbuf = mpRequest->GetSource(0);
		srcbuf->Unlock();
		mpLockedSrc = NULL;

		bool succeeded = (mProcessStatus == kProcess_Succeeded);
		mpRequest->MarkComplete(succeeded);
		CompleteRequest(mpRequest, succeeded);
		mpRequest.clear();
		return kRunResult_Running;
	} else {
		VDASSERT(mProcessStatus != kProcess_Pending);
		VDASSERT(!mpLockedSrc);
	}

	vdrefptr<VDFilterFrameRequest> req;
	if (!GetNextRequest(batchNumberLimit, ~req))
		return kRunResult_Idle;

	VDFilterFrameBuffer *srcbuf = req->GetSource(0);
	if (!srcbuf) {
		IVDFilterFrameClientRequest *creq0 = mpRequest->GetSourceRequest(0);

		if (creq0)
			mpRequest->SetError(creq0->GetError());

		req->MarkComplete(false);
		CompleteRequest(req, false);
		return kRunResult_Running;
	}

	if (!AllocateRequestBuffer(req)) {
		vdrefptr<VDFilterFrameRequestError> err(new_nothrow VDFilterFrameRequestError);

		if (err) {
			err->mError = "Unable to allocate an accelerated video frame buffer.";
			mpRequest->SetError(err);
		}

		req->MarkComplete(false);
		CompleteRequest(req, false);
		return kRunResult_Running;
	}

	mpLockedSrc = srcbuf->LockRead();
	if (!mpLockedSrc) {
		req->MarkComplete(false);
		CompleteRequest(req, false);
		return kRunResult_Running;
	}

	VDFilterFrameBufferAccel *dstbuf = static_cast<VDFilterFrameBufferAccel *>(req->GetResultBuffer());
	mpLockedDst = dstbuf;
	mProcessStatus = kProcess_Pending;

	mpFrameEngine->ScheduleProcess(0);
	mpRequest.swap(req);
	return kRunResult_Blocked;
}

VDFilterAccelUploader::RunResult VDFilterAccelUploader::RunProcess(int index) {
	if (index>0)
		return kRunResult_Idle;

	if (mProcessStatus != kProcess_Pending)
		return kRunResult_Idle;

	mpEngine->Upload(mpLockedDst, mpLockedSrc, mSourceLayout);
	mProcessStatus = kProcess_Succeeded;

	mpFrameEngine->Schedule();
	return kRunResult_IdleWasActive;
}

bool VDFilterAccelUploader::InitNewRequest(VDFilterFrameRequest *req, sint64 outputFrame, bool writable, uint32 batchNumber) {
	vdrefptr<IVDFilterFrameClientRequest> creq;
	if (!mpSource->CreateRequest(outputFrame, false, batchNumber, ~creq))
		return false;

	req->SetSourceCount(1);
	req->SetSourceRequest(0, creq);
	return true;
}
