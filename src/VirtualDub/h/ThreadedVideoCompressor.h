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

#ifndef f_VD2_THREADEDVIDEOCOMPRESSOR_H
#define f_VD2_THREADEDVIDEOCOMPRESSOR_H

#include <vd2/system/event.h>
#include <vd2/system/thread.h>
#include <vd2/system/refcount.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/VDDisplay/display.h>

class VDRTProfileChannel;
class IVDVideoCompressor;

///////////////////////////////////////////////////////////////////////////
//
//	VDRenderBufferAllocator<T>
//
///////////////////////////////////////////////////////////////////////////

template<class T>
class VDRenderBufferAllocator : public vdrefcounted<IVDRefCount> {
public:
	VDRenderBufferAllocator();
	~VDRenderBufferAllocator();

	void Init(int count);
	void Shutdown();

	bool AllocFrame(int timeout, T **ppFrame);
	bool FreeFrame(T *buffer);

protected:
	vdfastvector<T *>	mBuffers;

	int					mOutstandingBufferCount;
	VDSemaphore			mBufferCount;
	VDCriticalSection	mMutex;
	bool mbActive;
};

template<class T>
VDRenderBufferAllocator<T>::VDRenderBufferAllocator()
	: mBufferCount(0)
	, mOutstandingBufferCount(0)
	, mbActive(true)
{
}

template<class T>
VDRenderBufferAllocator<T>::~VDRenderBufferAllocator() {
	Shutdown();
}

template<class T>
void VDRenderBufferAllocator<T>::Init(int count) {
	vdsynchronized(mMutex) {
		// Prereserve the buffers array to decrease the chances that we'll hit OOM while
		// trying to free a buffer in a dtor (leads to terminate).
		mBuffers.reserve(count);

		mBufferCount.Reset(0);
		mOutstandingBufferCount = count;
		mbActive = true;
	}
}

template<class T>
void VDRenderBufferAllocator<T>::Shutdown() {
	vdsynchronized(mMutex) {
		mbActive = false;
		mBufferCount.Post();

		while(!mBuffers.empty()) {
			T *buf = mBuffers.back();
			mBuffers.pop_back();

			delete buf;
		}
	}
}

template<class T>
bool VDRenderBufferAllocator<T>::AllocFrame(int timeout, T **ppFrame) {
	T *buf = NULL;

	if (!mBufferCount.Wait(timeout))
		return false;

	vdsynchronized(mMutex) {
		if (!mbActive) {
			mBufferCount.Post();
			return false;
		}

		VDASSERT(!mBuffers.empty());

		buf = mBuffers.back();
		mBuffers.pop_back();

		++mOutstandingBufferCount;
	}

	*ppFrame = buf;
	buf->AddRef();
	return true;
}

template<class T>
bool VDRenderBufferAllocator<T>::FreeFrame(T *buffer) {
	bool active = false;
	vdsynchronized(mMutex) {
		active = mbActive;
		if (active)
			mBuffers.push_back(buffer);

		--mOutstandingBufferCount;
	}

	if (active)
		mBufferCount.Post();

	return active;
}

///////////////////////////////////////////////////////////////////////////////

class VDRenderOutputBuffer;

class VDRenderOutputBufferTracker : public VDRenderBufferAllocator<VDRenderOutputBuffer> {
public:
	void Init(void *base, const VDPixmap& px);
	void Init(int count, const VDPixmapLayout& layout);
};

///////////////////////////////////////////////////////////////////////////

class VDRenderOutputBuffer : public VDVideoDisplayFrame {
public:
	VDRenderOutputBuffer(VDRenderOutputBufferTracker *tracker);
	~VDRenderOutputBuffer();

	void Init(void *base, const VDPixmap& px);
	void Init(const VDPixmapLayout& layout);

	virtual int Release();

	void *mpBase;

public:
	const vdrefptr<VDRenderOutputBufferTracker> mpTracker;
	VDPixmapBuffer mBuffer;
};

///////////////////////////////////////////////////////////////////////////

class VDRenderPostCompressionBuffer;

class VDRenderPostCompressionBufferAllocator : public VDRenderBufferAllocator<VDRenderPostCompressionBuffer> {
public:
	void Init(int count, uint32 auxsize);
};

class VDRenderPostCompressionBuffer : public vdrefcounted<IVDRefCount> {
public:
	VDRenderPostCompressionBuffer(VDRenderPostCompressionBufferAllocator *tracker);
	~VDRenderPostCompressionBuffer();

	virtual int Release();

	uint32	mOutputSize;
	bool	mbOutputIsKey;
	vdfastvector<char>	mOutputBuffer;

protected:
	const vdrefptr<VDRenderPostCompressionBufferAllocator> mpTracker;
};

class VDThreadedVideoCompressorSlave;

class VDThreadedVideoCompressor {
public:
	enum FlushStatus {
		kFlushStatusNone				= 0,
		kFlushStatusLoopingDetected		= 1,
		kFlushStatusAll					= 1
	};

	VDThreadedVideoCompressor();
	~VDThreadedVideoCompressor();

	FlushStatus GetFlushStatus();

	bool IsAsynchronous() const { return mThreadCount > 0; }

	void SetPriority(int priority);

	void Init(int threads, IVDVideoCompressor *pBaseCompressor);
	void Shutdown();

	void SetFlush(bool flush, VDRenderOutputBuffer *holdBuffer);

	void SkipFrame();
	void Restart();

	bool ExchangeBuffer(VDRenderOutputBuffer *buffer, VDRenderPostCompressionBuffer **ppOutBuffer);

	VDEvent<VDThreadedVideoCompressor, bool>& OnFrameComplete() {
		return mEventFrameComplete;
	}

public:
	void RunSlave(IVDVideoCompressor *compressor, VDPixmapBuffer& repack_buffer);

protected:
	typedef vdfastdeque<uint32> FrameTrackingQueue;

	bool ProcessFrame(VDRenderOutputBuffer *pBuffer, IVDVideoCompressor *pCompressor, VDRTProfileChannel *pProfileChannel, sint32 frameNumber, FrameTrackingQueue *frameTrackingQueue, VDPixmapBuffer* repack_buffer);
	void FlushInputQueue();
	void FlushOutputQueue();

	VDThreadedVideoCompressorSlave *mpThreads;
	int mThreadCount;
	bool mbClientFlushInProgress;

	IVDVideoCompressor *mpBaseCompressor;

	vdrefptr<VDRenderPostCompressionBufferAllocator> mpAllocator;
	vdrefptr<VDRenderOutputBuffer> mpFlushBuffer;

	VDSemaphore mBarrier;

	VDAtomicInt	mFrameSkipCounter;

	VDCriticalSection mMutex;
	int mFramesSubmitted;
	int mFramesProcessed;
	int mFramesBufferedInFlush;
	bool mbFlushInProgress;
	bool mbLoopDetectedDuringFlush;
	uint32	mNextInputFrameNumber;
	uint32	mNextOutputFrameNumber;
	uint32	mNextOutputAllocIndex;

	bool mbInErrorState;
	vdfastdeque<VDRenderOutputBuffer *> mInputBuffer;

	struct OutputEntry {
		VDRenderPostCompressionBuffer *mpBuffer;
		bool mbCompleted;
	};

	vdfastdeque<OutputEntry> mOutputBuffer;
	VDSemaphore mInputBufferCount;
	int mPriority;

	MyError mError;

	VDEvent<VDThreadedVideoCompressor, bool> mEventFrameComplete;

	vdfastvector<IVDVideoCompressor *> mClonedCodecs;
};

#endif
