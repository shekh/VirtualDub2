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
#include "FilterAccelConverter.h"
#include "FilterAccelEngine.h"
#include "FilterFrameBufferAccel.h"

VDFilterAccelConverter::VDFilterAccelConverter() {
}

VDFilterAccelConverter::~VDFilterAccelConverter() {
}

void VDFilterAccelConverter::Init(VDFilterAccelEngine *engine, IVDFilterFrameSource *source, const VDPixmapLayout& outputLayout, const vdrect32 *srcBounds) {
	mpEngine = engine;
	mpSource = source;
	mSourceLayout = source->GetOutputLayout();

	if (srcBounds) {
		mSourceRect = *srcBounds;
	} else {
		mSourceRect.set(0, 0, mSourceLayout.w, mSourceLayout.h);
	}

	mLayout = outputLayout;
	mAllocator.Clear();
	mAllocator.AddSizeRequirement((outputLayout.h << 16) + outputLayout.w);
	mAllocator.SetAccelerationRequirement(VDFilterFrameAllocatorProxy::kAccelModeRender);
}

void VDFilterAccelConverter::Start(IVDFilterFrameEngine *frameEngine) {
	mpFrameEngine = frameEngine;
}

void VDFilterAccelConverter::Stop() {
	if (mpRequest) {
		mpRequest->MarkComplete(false);
		CompleteRequest(mpRequest, false);
		mpRequest.clear();
	}
}

bool VDFilterAccelConverter::GetDirectMapping(sint64 outputFrame, sint64& sourceFrame, int& sourceIndex) {
	return mpSource->GetDirectMapping(outputFrame, sourceFrame, sourceIndex);
}

sint64 VDFilterAccelConverter::GetSourceFrame(sint64 outputFrame) {
	return outputFrame;
}

sint64 VDFilterAccelConverter::GetSymbolicFrame(sint64 outputFrame, IVDFilterFrameSource *source) {
	if (source == this)
		return outputFrame;

	return mpSource->GetSymbolicFrame(outputFrame, source);
}

sint64 VDFilterAccelConverter::GetNearestUniqueFrame(sint64 outputFrame) {
	return mpSource->GetNearestUniqueFrame(outputFrame);
}

VDFilterAccelConverter::RunResult VDFilterAccelConverter::RunRequests(const uint32 *batchNumberLimit) {
	if (mpRequest) {
		if (mProcessStatus == kProcess_Pending)
			return kRunResult_Blocked;

		bool succeeded = (mProcessStatus == kProcess_Succeeded);
		mpRequest->MarkComplete(succeeded);
		CompleteRequest(mpRequest, succeeded);
		mpRequest.clear();
		return kRunResult_Running;
	} else {
		VDASSERT(mProcessStatus != kProcess_Pending);
	}

	vdrefptr<VDFilterFrameRequest> req;
	if (!GetNextRequest(batchNumberLimit, ~req))
		return kRunResult_Idle;

	VDFilterFrameBufferAccel *srcbuf = vdpoly_cast<VDFilterFrameBufferAccel *>(req->GetSource(0));
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

	VDFilterFrameBufferAccel *dstbuf = static_cast<VDFilterFrameBufferAccel *>(req->GetResultBuffer());
	mpLockedSrc = srcbuf;
	mpLockedDst = dstbuf;
	mProcessStatus = kProcess_Pending;

	mpFrameEngine->ScheduleProcess();
	mpRequest.swap(req);
	return kRunResult_Blocked;
}

VDFilterAccelConverter::RunResult VDFilterAccelConverter::RunProcess() {
	if (mProcessStatus != kProcess_Pending)
		return kRunResult_Idle;

	mpEngine->Convert(mpLockedDst, mLayout, mpLockedSrc, mSourceLayout, mSourceRect);
	mProcessStatus = kProcess_Succeeded;

	mpFrameEngine->Schedule();
	return kRunResult_IdleWasActive;
}

bool VDFilterAccelConverter::InitNewRequest(VDFilterFrameRequest *req, sint64 outputFrame, bool writable, uint32 batchNumber) {
	vdrefptr<IVDFilterFrameClientRequest> creq;
	if (!mpSource->CreateRequest(outputFrame, false, batchNumber, ~creq))
		return false;

	req->SetSourceCount(1);
	req->SetSourceRequest(0, creq);
	return true;
}
