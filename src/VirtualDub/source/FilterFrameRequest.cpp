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
#include "FilterFrame.h"
#include "FilterFrameRequest.h"

///////////////////////////////////////////////////////////////////////////

VDFilterFrameClientRequest::VDFilterFrameClientRequest()
	: mpRequest(NULL)
	, mpClient(NULL)
	, mpAllocator(NULL)
{
	static_cast<VDFilterFrameClientRequestAllocatorNode *>(this)->mListNodePrev = NULL;
}

VDFilterFrameClientRequest::~VDFilterFrameClientRequest() {
	Shutdown();
}

int VDFilterFrameClientRequest::Release() {
	bool unlinked = (static_cast<VDFilterFrameClientRequestAllocatorNode *>(this)->mListNodePrev == NULL);

	int rc = vdrefcounted<IVDFilterFrameClientRequest>::Release();
	if (rc == 1) {
		if (mpAllocator) {
			VDASSERT(static_cast<VDFilterFrameClientRequestAllocatorNode *>(this)->mListNodePrev != NULL);
			Shutdown();
			mpAllocator->OnFrameClientRequestIdle(this);
		}
	}

	if (!rc) {
		VDASSERT(unlinked);
	}

	return rc;
}

void VDFilterFrameClientRequest::SetAllocator(VDFilterFrameRequestAllocator *allocator) {
	mpAllocator = allocator;

	if (!allocator)
		static_cast<VDFilterFrameClientRequestAllocatorNode *>(this)->mListNodePrev = NULL;
}

void VDFilterFrameClientRequest::Init(VDFilterFrameRequest *request) {
	VDASSERT(!mpRequest);
	mpRequest = request;
	request->AddRef();
}

void VDFilterFrameClientRequest::Shutdown() {
	if (mpRequest) {
		mpRequest->RemoveClient(this);
		mpRequest->Release();
		mpRequest = NULL;
	}
}

void VDFilterFrameClientRequest::Start(IVDFilterFrameClient *client, uint64 cookie, uint32 srcIndex) {
	mpClient = client;
	mCookie = cookie;
	mSrcIndex = srcIndex;

	if (mpRequest->IsCompleted())
		IssueCallback();
}

VDFilterFrameBuffer *VDFilterFrameClientRequest::GetResultBuffer() {
	return mpRequest->GetResultBuffer();
}

bool VDFilterFrameClientRequest::IsResultBufferStealable() {
	return mpRequest->IsResultBufferStealable();
}

bool VDFilterFrameClientRequest::IsCompleted() {
	return mpRequest->IsCompleted();
}

bool VDFilterFrameClientRequest::IsSuccessful() {
	return mpRequest->IsSuccessful();
}

VDFilterFrameRequestError *VDFilterFrameClientRequest::GetError() {
	return mpRequest->GetError();
}

void VDFilterFrameClientRequest::IssueCallback() {
	if (mpClient)
		mpClient->OnFilterFrameCompleted(this);
}

sint64 VDFilterFrameClientRequest::GetFrameNumber() {
	if (mpRequest)
		return mpRequest->GetTiming().mOutputFrame;

	return -1;
}

uint64 VDFilterFrameClientRequest::GetCookie() {
	return mCookie;
}

uint32 VDFilterFrameClientRequest::GetSrcIndex() {
	return mSrcIndex;
}

void* VDFilterFrameClientRequest::GetExtraInfo() {
	if (mpRequest)
		return mpRequest->GetExtraInfo();

	return 0;
}

FilterModPixmapInfo& VDFilterFrameClientRequest::GetInfo() {
	return mpRequest->GetResultBuffer()->info;
}

///////////////////////////////////////////////////////////////////////////

VDFilterFrameRequest::VDFilterFrameRequest()
	: mbComplete(false)
	, mbSuccessful(false)
	, mbCacheable(true)
	, mbStealable(true)
	, mBatchNumber(0)
	, mpError(NULL)
	, mpResultBuffer(NULL)
	, mpExtraData(NULL)
{
}

VDFilterFrameRequest::~VDFilterFrameRequest() {
	VDASSERT(mClientRequests.empty());
	SetResultBuffer(NULL);
	SetSourceCount(0);
	SetExtraInfo(NULL);
	SetError(NULL);
}

int VDFilterFrameRequest::Release() {
	int rc = vdrefcounted<IVDRefCount>::Release();
	if (rc == 1) {
		if (mpAllocator) {
			VDASSERT(mClientRequests.empty());
			SetResultBuffer(NULL);
			SetSourceCount(0);
			SetExtraInfo(NULL);
			SetError(NULL);
			mbComplete = false;
			mbStealable = true;
			mpAllocator->OnFrameRequestIdle(this);
		}
	}

	return rc;
}

void VDFilterFrameRequest::SetAllocator(VDFilterFrameRequestAllocator *allocator) {
	mpAllocator = allocator;
}

bool VDFilterFrameRequest::IsActive() const {
	return !mClientRequests.empty();
}

bool VDFilterFrameRequest::IsCompleted() const {
	return mbComplete;
}

bool VDFilterFrameRequest::IsSuccessful() const {
	return mbSuccessful;
}

bool VDFilterFrameRequest::AreSourcesReady(bool *anyFailed, VDFilterFrameRequestError **error) const {
	bool failed = false;

	for(SourceRequests::const_iterator it(mSourceRequests.begin()), itEnd(mSourceRequests.end()); it != itEnd; ++it) {
		IVDFilterFrameClientRequest *creq = *it;

		if (creq) {
			if (!creq->IsCompleted())
				return false;

			if (!creq->IsSuccessful()) {
				failed = true;
				*error = creq->GetError();
			}
		}
	}

	if (anyFailed)
		*anyFailed = failed;

	return true;
}

void *VDFilterFrameRequest::GetExtraInfo() const {
	return mpExtraData;
}

void VDFilterFrameRequest::SetExtraInfo(IVDRefCount *extraData) {
	if (mpExtraData == extraData)
		return;

	if (extraData)
		extraData->AddRef();

	if (mpExtraData)
		mpExtraData->Release();

	mpExtraData = extraData;
}

uint32 VDFilterFrameRequest::GetSourceCount() const {
	return mSources.size();
}

VDFilterFrameBuffer *VDFilterFrameRequest::GetSource(uint32 index) {
	if (index >= mSources.size())
		return NULL;

	VDFilterFrameBuffer *buf = mSources[index];
	if (!buf) {
		IVDFilterFrameClientRequest *creq = mSourceRequests[index];

		if (creq) {
			buf = creq->GetResultBuffer();

			if (buf)
				buf->AddRef();

			mSources[index] = buf;
		}
	}

	return buf;
}

IVDFilterFrameClientRequest *VDFilterFrameRequest::GetSourceRequest(uint32 index) {
	if (index >= mSources.size())
		return NULL;

	IVDFilterFrameClientRequest *creq = mSourceRequests[index];

	return creq;
}

void VDFilterFrameRequest::SetSourceCount(uint32 sources) {
	while(!mSources.empty()) {
		VDFilterFrameBuffer *buf = mSources.back();
		mSources.pop_back();

		if (buf)
			buf->Release();
	}

	while(!mSourceRequests.empty()) {
		IVDFilterFrameClientRequest *buf = mSourceRequests.back();
		mSourceRequests.pop_back();

		if (buf)
			buf->Release();
	}

	mSources.resize(sources, NULL);
	mSourceRequests.resize(sources, NULL);
}

void VDFilterFrameRequest::SetSource(uint32 index, VDFilterFrameBuffer *buf) {
	if (index >= mSources.size()) {
		VDASSERT(!"Invalid source index.");
		buf->AddRef();
		buf->Release();
		return;
	}

	VDFilterFrameBuffer *oldBuf = mSources[index];
	if (oldBuf != buf) {
		if (oldBuf)
			oldBuf->Release();

		mSources[index] = buf;

		if (buf)
			buf->AddRef();
	}
}

void VDFilterFrameRequest::SetSourceRequest(uint32 index, IVDFilterFrameClientRequest *creq) {
	if (index >= mSources.size()) {
		VDASSERT(!"Invalid source index.");
		return;
	}

	IVDFilterFrameClientRequest *oldReq = mSourceRequests[index];
	if (oldReq != creq) {
		if (oldReq)
			oldReq->Release();

		mSourceRequests[index] = creq;

		if (creq)
			creq->AddRef();
	}
}

const VDXFBitmap *VDFilterFrameRequest::GetResultFrame() const {
	return NULL;
}

VDFilterFrameBuffer *VDFilterFrameRequest::GetResultBuffer() const {
	return mpResultBuffer;
}

void VDFilterFrameRequest::SetResultBuffer(VDFilterFrameBuffer *buffer) {
	if (buffer == mpResultBuffer)
		return;

	if (buffer)
		buffer->AddRef();
	if (mpResultBuffer)
		mpResultBuffer->Release();

	mpResultBuffer = buffer;
}

bool VDFilterFrameRequest::IsResultBufferStealable() const {
	return mbStealable && mClientRequests.size() <= 1;
}

void VDFilterFrameRequest::CreateClient(bool writable, IVDFilterFrameClientRequest **ppReq) {
	vdrefptr<VDFilterFrameClientRequest> req;
	
	if (mpAllocator)
		mpAllocator->AllocateClientRequest(~req);
	else
		req = new VDFilterFrameClientRequest;

	req->Init(this);
	
	mClientRequests.push_back(req);

	*ppReq = req.release();
}

void VDFilterFrameRequest::RemoveClient(VDFilterFrameClientRequest *creq) {
	VDASSERT(mClientRequests.find(creq) != mClientRequests.end());
	mClientRequests.erase(creq);
	static_cast<VDFilterFrameClientRequestNode *>(creq)->mListNodeNext = NULL;
	static_cast<VDFilterFrameClientRequestNode *>(creq)->mListNodePrev = NULL;
}

void VDFilterFrameRequest::MarkComplete(bool successful) {
	if (mbComplete)
		return;

	mbComplete = true;
	mbSuccessful = successful;

	if (!successful)
		SetResultBuffer(NULL);

	SetSourceCount(0);

	for(ClientRequests::iterator it(mClientRequests.begin()), itEnd(mClientRequests.end()); it != itEnd; ++it) {
		VDFilterFrameClientRequest *req = static_cast<VDFilterFrameClientRequest *>(*it);

		req->IssueCallback();
	}
}

void VDFilterFrameRequest::SetError(VDFilterFrameRequestError *error) {
	if (error == mpError)
		return;

	if (error)
		error->AddRef();

	if (mpError)
		mpError->Release();

	mpError = error;
}

///////////////////////////////////////////////////////////////////////////

VDFilterFrameRequestAllocator::VDFilterFrameRequestAllocator() {
}

VDFilterFrameRequestAllocator::~VDFilterFrameRequestAllocator() {
	Shutdown();
}

void VDFilterFrameRequestAllocator::Shutdown() {
	mIdleRequests.splice(mIdleRequests.end(), mActiveRequests);

	while(!mIdleRequests.empty()) {
		VDFilterFrameRequest *buf = static_cast<VDFilterFrameRequest *>(mIdleRequests.back());
		mIdleRequests.pop_back();

		buf->SetAllocator(NULL);
		buf->Release();
	}

	mIdleClientRequests.splice(mIdleClientRequests.end(), mActiveClientRequests);

	while(!mIdleClientRequests.empty()) {
		VDFilterFrameClientRequest *buf = static_cast<VDFilterFrameClientRequest *>(mIdleClientRequests.back());
#ifdef _DEBUG
		VDASSERT(buf->mbInAllocatorActiveList || buf->mbInAllocatorIdleList);
#endif
		mIdleClientRequests.pop_back();

		buf->SetAllocator(NULL);
		buf->Release();
	}
}

void VDFilterFrameRequestAllocator::Allocate(VDFilterFrameRequest **buffer) {
	vdrefptr<VDFilterFrameRequest> buf;

	if (mIdleRequests.empty()) {
		buf = new VDFilterFrameRequest;
		buf->SetAllocator(this);

		buf->AddRef();
		mActiveRequests.push_back(buf);
	} else {
		buf = static_cast<VDFilterFrameRequest *>(mIdleRequests.front());
		mIdleRequests.pop_front();
		mActiveRequests.push_front(buf);
	}

	*buffer = buf.release();
}

void VDFilterFrameRequestAllocator::AllocateClientRequest(VDFilterFrameClientRequest **buffer) {
	vdrefptr<VDFilterFrameClientRequest> buf;

	if (mIdleClientRequests.empty()) {
		buf = new VDFilterFrameClientRequest;
		buf->SetAllocator(this);

		buf->AddRef();
		mActiveClientRequests.push_back(buf);
#ifdef _DEBUG
		buf->mbInAllocatorActiveList = true;
#endif
	} else {
		buf = static_cast<VDFilterFrameClientRequest *>(mIdleClientRequests.front());

#ifdef _DEBUG
		VDASSERT(buf->mbInAllocatorIdleList);
		VDASSERT(!buf->mbInAllocatorActiveList);
		VDASSERT(buf->mAllocatorMagic == VDFilterFrameClientRequest::kAllocatorMagic);
		buf->mbInAllocatorIdleList = false;
		buf->mbInAllocatorActiveList = true;
#endif

		mIdleClientRequests.pop_front();
		mActiveClientRequests.push_front(buf);
	}

	*buffer = buf.release();
}

void VDFilterFrameRequestAllocator::OnFrameRequestIdle(VDFilterFrameRequest *buf) {
	mIdleRequests.splice(mIdleRequests.begin(), mActiveRequests, buf);
}

void VDFilterFrameRequestAllocator::OnFrameClientRequestIdle(VDFilterFrameClientRequest *buf) {
#ifdef _DEBUG
	VDASSERT(buf->mbInAllocatorActiveList);
	VDASSERT(!buf->mbInAllocatorIdleList);
	VDASSERT(buf->mAllocatorMagic == VDFilterFrameClientRequest::kAllocatorMagic);

	buf->mbInAllocatorActiveList = false;
	buf->mbInAllocatorIdleList = true;
#endif

	mIdleClientRequests.splice(mIdleClientRequests.begin(), mActiveClientRequests, buf);
}

#ifdef _DEBUG
	void VDFilterFrameRequestAllocator::ValidateState() {
		// validate all client requests
		for(ClientRequests::const_iterator it(mIdleClientRequests.begin()), itEnd(mIdleClientRequests.end());
			it != itEnd;
			++it)
		{
			const VDFilterFrameClientRequest *p = static_cast<const VDFilterFrameClientRequest *>(*it);

			VDASSERT(p->mAllocatorMagic == VDFilterFrameClientRequest::kAllocatorMagic);
		}
	}
#endif
