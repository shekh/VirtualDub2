#include "stdafx.h"
#include <vd2/system/profile.h>
#include "FilterFrame.h"
#include "FilterFrameBufferMemory.h"
#include "FilterFrameAllocatorMemory.h"
#include "FilterFrameCache.h"

VDFilterFrameAllocatorMemory::VDFilterFrameAllocatorMemory()
	: mSizeRequired(0)
	, mMinFrames(0)
	, mMaxFrames(0x7fffffff)
	, mAllocatedFrames(0)
	, mAllocatedBytes(0)
	, mActiveFrames(0)
	, mActiveBytes(0)
	, mTrimCounter(0)
	, mTrimPeriod(0)
{
}

VDFilterFrameAllocatorMemory::~VDFilterFrameAllocatorMemory() {
	Shutdown();
}

void VDFilterFrameAllocatorMemory::Init(uint32 minFrames, uint32 maxFrames) {
	mMinFrames = minFrames;
	mMaxFrames = maxFrames;

	VDASSERT(mActiveBuffers.empty());
	VDASSERT(mIdleBuffers.empty());

	for(int i=0; i<mMinFrames; ++i) {
		vdrefptr<VDFilterFrameBufferMemory> buf(new VDFilterFrameBufferMemory);

		buf->Init(mSizeRequired);
		buf->SetAllocator(this);

		mIdleBuffers.push_back(buf);
		buf.release();
	}

	mAllocatedFrames = mMinFrames;
	mAllocatedBytes = mMinFrames * mSizeRequired;
	mActiveFrames = 0;
	mActiveBytes = 0;
	mTrimCounter = 0;
	mTrimPeriod = 50;
	mCurrentWatermark = 0;

	VDRTProfiler *profiler = VDGetRTProfiler();
	if (profiler) {
		profiler->RegisterCounterU32("Allocated frames", &mAllocatedFrames);
		profiler->RegisterCounterU32("Allocated bytes", &mAllocatedBytes);
		profiler->RegisterCounterU32("Active frames", &mActiveFrames);
		profiler->RegisterCounterU32("Active bytes", &mActiveBytes);
	}
}

void VDFilterFrameAllocatorMemory::Shutdown() {
	VDRTProfiler *profiler = VDGetRTProfiler();
	if (profiler) {
		profiler->UnregisterCounter(&mAllocatedFrames);
		profiler->UnregisterCounter(&mAllocatedBytes);
		profiler->UnregisterCounter(&mActiveFrames);
		profiler->UnregisterCounter(&mActiveBytes);
	}

	mSizeRequired = 0;
	mAllocatedFrames = 0;
	mAllocatedBytes = 0;
	mActiveFrames = 0;
	mActiveBytes = 0;

	mIdleBuffers.splice(mIdleBuffers.end(), mActiveBuffers);

	Trim();
}

void VDFilterFrameAllocatorMemory::AddSizeRequirement(uint32 bytes) {
	if (mSizeRequired < bytes)
		mSizeRequired = bytes;
}

void VDFilterFrameAllocatorMemory::Trim() {
	while(!mIdleBuffers.empty()) {
		VDFilterFrameBuffer *buf = static_cast<VDFilterFrameBuffer *>(mIdleBuffers.back());
		mIdleBuffers.pop_back();

		buf->SetAllocator(NULL);
		buf->Release();
	}
}

bool VDFilterFrameAllocatorMemory::Allocate(VDFilterFrameBuffer **buffer) {
	vdrefptr<VDFilterFrameBufferMemory> buf;

	if (mIdleBuffers.empty()) {
		if (mAllocatedFrames >= mMaxFrames)
			return false;

		buf = new VDFilterFrameBufferMemory;
		buf->Init(mSizeRequired);

		buf->AddRef();
		mActiveBuffers.push_back(buf);

		// must do this AFTER refcount hits 2 to avoid OnFrameBufferActive() callback
		buf->SetAllocator(this);

		++mAllocatedFrames;
		mAllocatedBytes += mSizeRequired;
		++mActiveFrames;
		mActiveBytes += mSizeRequired;
	} else {
		// The implicit AddRef() here will knock it out of the idle buffers list to
		// the active list.
		buf = static_cast<VDFilterFrameBufferMemory *>(mIdleBuffers.front());

		buf->EvictFromCaches();
	}

	if (mCurrentWatermark < mAllocatedFrames)
		mCurrentWatermark = mAllocatedFrames;

	if (++mTrimCounter >= mTrimPeriod) {
		if (mAllocatedFrames > mCurrentWatermark && !mIdleBuffers.empty()) {
			VDFilterFrameBuffer *trimBuf = static_cast<VDFilterFrameBuffer *>(mIdleBuffers.front());
			mIdleBuffers.pop_front();
			--mAllocatedFrames;
			mAllocatedBytes -= mSizeRequired;

			trimBuf->SetAllocator(NULL);
			trimBuf->Release();
		}

		mTrimCounter = 0;
		mCurrentWatermark = 0;
	}

	*buffer = buf.release();
	return true;
}

void VDFilterFrameAllocatorMemory::OnFrameBufferIdle(VDFilterFrameBuffer *buf) {
	mIdleBuffers.splice(mIdleBuffers.end(), mActiveBuffers, buf);

	--mActiveFrames;
	mActiveBytes -= mSizeRequired;
}

void VDFilterFrameAllocatorMemory::OnFrameBufferActive(VDFilterFrameBuffer *buf) {
	mActiveBuffers.splice(mActiveBuffers.begin(), mIdleBuffers, buf);

	++mActiveFrames;
	mActiveBytes += mSizeRequired;
}
