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

#ifndef f_VD2_FILTERFRAMEREQUEST_H
#define f_VD2_FILTERFRAMEREQUEST_H

#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/VDString.h>

class VDFilterFrameBuffer;
class VDFilterFrameRequest;
class IVDFilterFrameClientRequest;
class VDFilterFrameRequestAllocator;
class VDXFBitmap;
struct FilterModPixmapInfo;

///////////////////////////////////////////////////////////////////////////

class IVDFilterFrameClient {
public:
	virtual void OnFilterFrameCompleted(IVDFilterFrameClientRequest *request) = 0;
};

///////////////////////////////////////////////////////////////////////////

struct VDFilterFrameRequestError : public vdrefcounted<IVDRefCount> {
	VDStringA mError;
};

///////////////////////////////////////////////////////////////////////////

class IVDFilterFrameClientRequest : public IVDRefCount {
public:
	virtual void Start(IVDFilterFrameClient *callback, uint64 cookie, uint32 srcIndex) = 0;
	virtual VDFilterFrameBuffer *GetResultBuffer() = 0;
	virtual bool IsResultBufferStealable() = 0;
	virtual bool IsCompleted() = 0;
	virtual bool IsSuccessful() = 0;
	virtual VDFilterFrameRequestError *GetError() = 0;
	virtual sint64 GetFrameNumber() = 0;
	virtual uint64 GetCookie() = 0;
	virtual uint32 GetSrcIndex() = 0;
	virtual FilterModPixmapInfo& GetInfo() = 0;
};

///////////////////////////////////////////////////////////////////////////

struct VDFilterFrameClientRequestAllocatorNode : public vdlist_node {
#ifdef _DEBUG
	bool mbInAllocatorIdleList;
	bool mbInAllocatorActiveList;
	uint32 mAllocatorMagic;

	enum { kAllocatorMagic = 0xABCD0001 };

	VDFilterFrameClientRequestAllocatorNode()
		: mbInAllocatorIdleList(false)
		, mbInAllocatorActiveList(false)
		, mAllocatorMagic(kAllocatorMagic)
	{
	}
#endif
};

struct VDFilterFrameClientRequestNode : public vdlist_node {};

class VDFilterFrameClientRequest : public VDFilterFrameClientRequestNode, public vdrefcounted<IVDFilterFrameClientRequest>, public VDFilterFrameClientRequestAllocatorNode {
	VDFilterFrameClientRequest(const VDFilterFrameClientRequest&);
	VDFilterFrameClientRequest& operator=(const VDFilterFrameClientRequest&);
public:
	VDFilterFrameClientRequest();
	~VDFilterFrameClientRequest();

	int Release();

	void SetAllocator(VDFilterFrameRequestAllocator *allocator);

	void Init(VDFilterFrameRequest *request);
	void Shutdown();

	void Start(IVDFilterFrameClient *client, uint64 cookie, uint32 srcIndex);
	VDFilterFrameBuffer *GetResultBuffer();
	bool IsResultBufferStealable();

	bool IsCompleted();
	bool IsSuccessful();
	VDFilterFrameRequestError *GetError();

	void IssueCallback();

	sint64 GetFrameNumber();
	uint64 GetCookie();
	uint32 GetSrcIndex();
	FilterModPixmapInfo& GetInfo();

protected:
	VDFilterFrameRequest *mpRequest;
	IVDFilterFrameClient *mpClient;
	uint64	mCookie;
	uint32	mSrcIndex;

	VDFilterFrameRequestAllocator *mpAllocator;
};

///////////////////////////////////////////////////////////////////////////

struct VDFilterFrameRequestTiming {
	sint64		mSourceFrame;
	sint64		mOutputFrame;
};

struct VDFilterFrameRequestAllocatorNode : public vdlist_node {};

class VDFilterFrameRequest : public vdrefcounted<IVDRefCount>, public VDFilterFrameRequestAllocatorNode {
	VDFilterFrameRequest(const VDFilterFrameRequest&);
	VDFilterFrameRequest& operator=(const VDFilterFrameRequest&);
public:
	VDFilterFrameRequest();
	~VDFilterFrameRequest();

	int Release();

	void SetAllocator(VDFilterFrameRequestAllocator *allocator);

	bool IsCompleted() const;
	bool IsActive() const;
	bool IsSuccessful() const;
	bool AreSourcesReady(bool *anyFailed, VDFilterFrameRequestError **error) const;

	bool GetCacheable() const { return mbCacheable; }
	void SetCacheable(bool cacheable) { mbCacheable = cacheable; }

	bool IsStealable() const { return mbStealable; }
	void SetStealable(bool stealable) { mbStealable = stealable; }

	const VDFilterFrameRequestTiming& GetTiming() const { return mTiming; }
	void SetTiming(const VDFilterFrameRequestTiming& timing) { mTiming = timing; }

	void *GetExtraInfo() const;
	void SetExtraInfo(IVDRefCount *extraData);

	uint32 GetBatchNumber() const { return mBatchNumber; }
	void SetBatchNumber(uint32 batch) { mBatchNumber = batch; }

	uint32 GetSourceCount() const;
	VDFilterFrameBuffer *GetSource(uint32 index);
	IVDFilterFrameClientRequest *GetSourceRequest(uint32 index);

	void SetSourceCount(uint32 sources);
	void SetSource(uint32 index, VDFilterFrameBuffer *buf);
	void SetSourceRequest(uint32 index, IVDFilterFrameClientRequest *creq);

	const VDXFBitmap *GetResultFrame() const;
	VDFilterFrameBuffer *GetResultBuffer() const;
	void SetResultBuffer(VDFilterFrameBuffer *buffer);
	bool IsResultBufferStealable() const;

	void CreateClient(bool writable, IVDFilterFrameClientRequest **ppReq);
	void RemoveClient(VDFilterFrameClientRequest *creq);
	void MarkComplete(bool successful);

	VDFilterFrameRequestError *GetError() const { return mpError; }
	void SetError(VDFilterFrameRequestError *error);

protected:
	typedef vdfastvector<IVDFilterFrameClientRequest *> SourceRequests;
	SourceRequests mSourceRequests;

	typedef vdlist<VDFilterFrameClientRequestNode> ClientRequests;
	ClientRequests mClientRequests;

	typedef vdfastvector<VDFilterFrameBuffer *> Sources;
	Sources mSources;

	VDFilterFrameRequestTiming mTiming;
	bool		mbComplete;
	bool		mbSuccessful;
	bool		mbCacheable;
	bool		mbStealable;

	uint32		mBatchNumber;

	VDFilterFrameRequestError *mpError;
	VDFilterFrameBuffer *mpResultBuffer;
	VDFilterFrameRequestAllocator *mpAllocator;

	IVDRefCount	*mpExtraData;
};

///////////////////////////////////////////////////////////////////////////
//
//	VDFilterFrameRequestAllocator
//
///////////////////////////////////////////////////////////////////////////

class VDFilterFrameRequestAllocator : public vdrefcounted<IVDRefCount> {
	VDFilterFrameRequestAllocator(const VDFilterFrameRequestAllocator&);
	VDFilterFrameRequestAllocator& operator=(const VDFilterFrameRequestAllocator&);
public:
	VDFilterFrameRequestAllocator();
	~VDFilterFrameRequestAllocator();

	void Shutdown();

	void Allocate(VDFilterFrameRequest **buffer);
	void AllocateClientRequest(VDFilterFrameClientRequest **buffer);

	void OnFrameRequestIdle(VDFilterFrameRequest *buf);
	void OnFrameClientRequestIdle(VDFilterFrameClientRequest *buf);

#ifdef _DEBUG
	void ValidateState();
#else
	inline void ValidateState() {}
#endif

protected:
	typedef vdlist<VDFilterFrameRequestAllocatorNode> Requests;
	Requests mActiveRequests;
	Requests mIdleRequests;

	typedef vdlist<VDFilterFrameClientRequestAllocatorNode> ClientRequests;
	ClientRequests mActiveClientRequests;
	ClientRequests mIdleClientRequests;
};

#endif	// f_VD2_FILTERFRAMEREQUEST_H
