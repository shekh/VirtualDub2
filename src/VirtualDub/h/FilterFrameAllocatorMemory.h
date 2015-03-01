#ifndef f_VD2_FILTERFRAMEALLOCATORMEMORY_H
#define f_VD2_FILTERFRAMEALLOCATORMEMORY_H

#include "FilterFrameAllocator.h"
#include "FilterFrame.h"

///////////////////////////////////////////////////////////////////////////
//
//	VDFilterFrameAllocatorMemory
//
///////////////////////////////////////////////////////////////////////////

class VDFilterFrameAllocatorMemory : public vdrefcounted<IVDFilterFrameAllocator> {
	VDFilterFrameAllocatorMemory(const VDFilterFrameAllocatorMemory&);
	VDFilterFrameAllocatorMemory& operator=(const VDFilterFrameAllocatorMemory&);
public:
	VDFilterFrameAllocatorMemory();
	~VDFilterFrameAllocatorMemory();

	uint32 GetFrameSize() const { return mSizeRequired; }

	void Init(uint32 minFrames, uint32 maxFrames);
	void Shutdown();

	void AddSizeRequirement(uint32 bytes);

	void Trim();
	bool Allocate(VDFilterFrameBuffer **buffer);

	void OnFrameBufferIdle(VDFilterFrameBuffer *buf);
	void OnFrameBufferActive(VDFilterFrameBuffer *buf);

protected:
	uint32	mSizeRequired;
	uint32	mMinFrames;
	uint32	mMaxFrames;
	uint32	mAllocatedFrames;
	uint32	mAllocatedBytes;
	uint32	mActiveFrames;
	uint32	mActiveBytes;

	uint32	mTrimCounter;
	uint32	mTrimPeriod;
	uint32	mCurrentWatermark;

	typedef vdlist<VDFilterFrameBufferAllocatorNode> Buffers;
	Buffers mActiveBuffers;
	Buffers mIdleBuffers;
};

#endif	// f_VD2_FILTERFRAMEALLOCATORMEMORY_H
