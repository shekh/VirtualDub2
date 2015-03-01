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
#include <vd2/system/file.h>
#include "FilterFrameQueue.h"
#include "FilterFrameRequest.h"

VDFilterFrameQueue::VDFilterFrameQueue() {
}

VDFilterFrameQueue::~VDFilterFrameQueue() {
	Shutdown();
}

void VDFilterFrameQueue::Shutdown() {
	ValidateState();

	while(!mRequests.empty()) {
		VDFilterFrameRequest *r = mRequests.front();
		mRequests.pop_front();

		if (r)
			r->Release();
	}
}

bool VDFilterFrameQueue::GetRequest(sint64 frame, VDFilterFrameRequest **req) {
	Requests::iterator it(mRequests.begin()), itEnd(mRequests.end());
	for(; it != itEnd; ++it) {
		VDFilterFrameRequest *r = *it;

		if (r && r->GetCacheable() && r->GetTiming().mOutputFrame == frame) {
			*req = r;
			r->AddRef();
			return true;
		}
	}

	return false;
}

void VDFilterFrameQueue::CompleteRequests(sint64 frame, VDFilterFrameBuffer *buf) {
	Requests::iterator it(mRequests.begin()), itEnd(mRequests.end());
	for(; it != itEnd; ++it) {
		VDFilterFrameRequest *r = *it;

		if (r->GetCacheable() && r->GetTiming().mOutputFrame == frame) {
			*it = NULL;
			r->SetResultBuffer(buf);
			r->MarkComplete(true);
			r->Release();
			break;
		}
	}
}

void VDFilterFrameQueue::CreateRequest(VDFilterFrameRequest **req) {
	mAllocator.Allocate(req);
}

void VDFilterFrameQueue::Add(VDFilterFrameRequest *req) {
	Requests::iterator it(mRequests.end()), itBegin(mRequests.begin());
	uint32 batchNum = req->GetBatchNumber();

	bool motionRequired = false;
	while(it != itBegin) {
		Requests::iterator itTest(it);
		--itTest;

		VDFilterFrameRequest *other = *itTest;
		uint32 otherBatchNum = other->GetBatchNumber();

		if ((sint32)(batchNum - otherBatchNum) >= 0)
			break;

		it = itTest;
		motionRequired = true;
	}

	if (motionRequired) {
		mRequests.push_back(NULL);

		Requests::iterator itCopyEnd(mRequests.end());
		--itCopyEnd;
		std::copy_backward(it, itCopyEnd, mRequests.end());
		*it = req;
	} else {
		mRequests.push_back(req);
	}
	req->AddRef();
}

bool VDFilterFrameQueue::PeekNextRequest(const uint32 *batchNumberLimit, VDFilterFrameRequest **req) {
	VDFilterFrameRequest *r;
	for(;;) {
		if (mRequests.empty())
			return false;

		r = mRequests.front();
		if (!r) {
			mRequests.pop_front();
			continue;
		}

		if (!r->IsActive()) {
			mRequests.pop_front();
			r->Release();
			continue;
		}

		if (batchNumberLimit && (sint32)(r->GetBatchNumber() - *batchNumberLimit) >= 0)
			return false;

		bool anyFailed;
		VDFilterFrameRequestError *error;
		if (!r->AreSourcesReady(&anyFailed, &error))
			return false;

		if (anyFailed) {
			r->SetError(error);
			r->MarkComplete(false);
			continue;
		}

		r->AddRef();
		*req = r;

		return true;
	}
}

bool VDFilterFrameQueue::GetNextRequest(const uint32 *batchNumberLimit, VDFilterFrameRequest **req) {
	VDFilterFrameRequest *r;
	for(;;) {
		if (mRequests.empty())
			return false;

		r = mRequests.front();
		if (!r) {
			mRequests.pop_front();
			continue;
		}

		if (!r->IsActive()) {
			mRequests.pop_front();
			r->Release();
			continue;
		}

		if (batchNumberLimit && (sint32)(r->GetBatchNumber() - *batchNumberLimit) >= 0)
			return false;

		bool anyFailed;
		VDFilterFrameRequestError *error;
		if (!r->AreSourcesReady(&anyFailed, &error))
			return false;

		if (anyFailed) {
			r->SetError(error);
			r->MarkComplete(false);
			mRequests.pop_front();
			r->Release();
			continue;
		}

		mRequests.pop_front();
		*req = r;

		return true;
	}
}

bool VDFilterFrameQueue::Remove(VDFilterFrameRequest *req) {
	Requests::iterator it(mRequests.begin()), itEnd(mRequests.end());
	for(; it != itEnd; ++it) {
		VDFilterFrameRequest *r = *it;

		if (r == req) {
			*it = mRequests.back();
			mRequests.pop_back();

			req->Release();
			return true;
		}
	}

	return false;
}

void VDFilterFrameQueue::DumpStatus(VDTextOutputStream& os) {
	Requests::iterator it(mRequests.begin()), itEnd(mRequests.end());
	for(; it != itEnd; ++it) {
		VDFilterFrameRequest *r = *it;
		const VDFilterFrameRequestTiming& t =  r->GetTiming();

		os.FormatLine("    Source frame %u | Output frame %u"
			, (unsigned)t.mSourceFrame
			, (unsigned)t.mOutputFrame
			);

		uint32 srcCount = r->GetSourceCount();
		for(uint32 srcIdx = 0; srcIdx < srcCount; ++srcIdx) {
			IVDFilterFrameClientRequest *cr = r->GetSourceRequest(srcIdx);

			os.FormatLine("      Source frame %u | %s"
				, (unsigned)cr->GetFrameNumber()
				, cr->IsCompleted() ? cr->IsSuccessful() ? "Succeeded" : "Failed" : "Pending"
				);
		}
	}
}

#ifdef _DEBUG
	void VDFilterFrameQueue::ValidateState() {
		mAllocator.ValidateState();
	}
#endif
