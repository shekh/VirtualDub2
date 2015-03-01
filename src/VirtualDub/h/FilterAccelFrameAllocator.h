#ifndef f_VD2_FILTERACCELFRAMEALLOCATOR_H
#define f_VD2_FILTERACCELFRAMEALLOCATOR_H

#include "FilterFrameAllocator.h"

class VDFilterAccelEngine;

///////////////////////////////////////////////////////////////////////////
//
//	VDFilterFrameAllocator
//
///////////////////////////////////////////////////////////////////////////

class VDFilterAccelFrameAllocator : public vdrefcounted<IVDFilterFrameAllocator> {
	VDFilterAccelFrameAllocator(const VDFilterAccelFrameAllocator&);
	VDFilterAccelFrameAllocator& operator=(const VDFilterAccelFrameAllocator&);
public:
	VDFilterAccelFrameAllocator();
	~VDFilterAccelFrameAllocator();

	uint32 GetFrameSize() const { return mSizeRequired; }

	void Init(uint32 minFrames, uint32 maxFrames, VDFilterAccelEngine *accelEngine);
	void Shutdown();

	void AddSizeRequirement(uint32 bytes);
	void AddBorderRequirement(uint32 w, uint32 h);

	void Trim();
	bool Allocate(VDFilterFrameBuffer **buffer);

	void OnFrameBufferIdle(VDFilterFrameBuffer *buf);
	void OnFrameBufferActive(VDFilterFrameBuffer *buf);

protected:
	uint32	mSizeRequired;
	uint32	mBorderWRequired;
	uint32	mBorderHRequired;
	uint32	mMinFrames;
	uint32	mMaxFrames;
	uint32	mAllocatedFrames;
	uint32	mAllocatedBytes;
	uint32	mActiveFrames;
	uint32	mActiveBytes;

	uint32	mTrimCounter;
	uint32	mTrimPeriod;
	uint32	mCurrentWatermark;

	VDFilterAccelEngine *mpAccelEngine;

	typedef vdlist<VDFilterFrameBufferAllocatorNode> Buffers;
	Buffers mActiveBuffers;
	Buffers mIdleBuffers;
};

#endif	// f_VD2_FILTERACCELFRAMEALLOCATOR_H
