//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "stdafx.h"

#include "VirtualDub.h"

#include <windows.h>

#include "AVIPipe.h"

///////////////////////////////

AVIPipe::AVIPipe(int buffers, long roundup_size)
	: mState(0)
	, mReadPt(0)
	, mWritePt(0)
	, mLevel(0)
{
	pBuffers		= new struct AVIPipeBuffer[buffers];
	num_buffers		= buffers;
	round_size		= roundup_size;

	if (pBuffers)
		memset((void *)pBuffers, 0, sizeof(struct AVIPipeBuffer)*buffers);
}

AVIPipe::~AVIPipe() {
	if (pBuffers) {
		for(int i=0; i<num_buffers; ++i) {
			void *buf = pBuffers[i].mFrameInfo.mpData;

			if (buf)
				VirtualFree(buf, 0, MEM_RELEASE);
		}

		delete[] (void *)pBuffers;
	}
}

bool AVIPipe::isFinalized() {
	if (mState & kFlagFinalizeTriggered) {
		finalizeAck();
		return true;
	}

	return false;
}

bool AVIPipe::isFinalizeAcked() {
	return 0 != (mState & kFlagFinalizeAcknowledged);
}

bool AVIPipe::full() {
	vdsynchronized(mcsQueue) {
		if (mState & kFlagAborted)
			return false;

		return mLevel >= num_buffers;
	}

	return false;
}

void *AVIPipe::getWriteBuffer(long len, int *handle_ptr) {
	int h;

	if (!len) ++len;
	len = ((len+round_size-1) / round_size) * round_size;

	++mcsQueue;

	for(;;) {
		if (mState & kFlagAborted) {
			--mcsQueue;
			return NULL;
		}

		// try the buffer right under us
		if (mLevel < num_buffers) {
			h = mWritePt;
			if (pBuffers[h].mBufferSize >= len)
				break;
		}

		int nBufferWithoutAllocation = -1;
		int nBufferWithSmallAllocation = -1;

		h = mWritePt;
		for(int cnt = num_buffers - mLevel; cnt>0; --cnt) {
			if (!pBuffers[h].mBufferSize)
				nBufferWithoutAllocation = h;
			else if (pBuffers[h].mBufferSize < len)
				nBufferWithSmallAllocation = h;
			else
				goto buffer_found;

			if (++h >= num_buffers)
				h = 0;
		}

		if (nBufferWithoutAllocation >= 0)
			h = nBufferWithoutAllocation;
		else if (nBufferWithSmallAllocation >= 0)
			h = nBufferWithSmallAllocation;
		else {
			--mcsQueue;
			msigRead.wait();
			++mcsQueue;
			continue;
		}

buffer_found:

		if (pBuffers[h].mBufferSize < len) {
			void *buf = pBuffers[h].mFrameInfo.mpData;
			if (buf) {
				VirtualFree(buf, 0, MEM_RELEASE);
				pBuffers[h].mFrameInfo.mpData = NULL;
				pBuffers[h].mBufferSize = 0;
			}

			buf = VirtualAlloc(NULL, len, MEM_COMMIT, PAGE_READWRITE);
			if (buf) {
				pBuffers[h].mFrameInfo.mpData = buf;
				pBuffers[h].mBufferSize = len;
			}
		}

		if (h != mWritePt) {
			std::swap(pBuffers[h].mFrameInfo.mpData, pBuffers[mWritePt].mFrameInfo.mpData);
			std::swap(pBuffers[h].mBufferSize, pBuffers[mWritePt].mBufferSize);
		}
	}

	pBuffers[h].mbInUse = true;

	--mcsQueue;

	*handle_ptr = h;

	return pBuffers[h].mFrameInfo.mpData;
}

void AVIPipe::postBuffer(const VDRenderVideoPipeFrameInfo& frameInfo) {
	++mcsQueue;
	void *buf = pBuffers[mWritePt].mFrameInfo.mpData;
	pBuffers[mWritePt].mFrameInfo = frameInfo;
	pBuffers[mWritePt].mFrameInfo.mpData = buf;

	if (++mWritePt >= num_buffers)
		mWritePt = 0;
	++mLevel;
	--mcsQueue;

	msigWrite.signal();

	mEventBufferAdded.Raise(this, false);

	//	_RPT2(0,"Posted buffer %ld (ID %08lx)\n",handle,cur_write-1);
}

void AVIPipe::getDropDistances(int& total, int& indep) {
	total = 0;
	indep = 0x3FFFFFFF;

	++mcsQueue;

	int h = mReadPt;
	for(int cnt = mLevel; cnt>0; --cnt) {
		int ahead = total;

		if (pBuffers[h].mbInUse) {
			if (pBuffers[h].mFrameInfo.mDroptype == kIndependent && ahead >= 0 && ahead < indep)
				indep = ahead;
		}

		++total;
		if (++h >= num_buffers)
			h = 0;
	}

	--mcsQueue;
}

void AVIPipe::getQueueInfo(int& total, int& finals, int& allocated) {
	total = 0;
	finals = 0;
	allocated = num_buffers;

	++mcsQueue;

	int h = mReadPt;
	for(int cnt = mLevel; cnt>0; --cnt) {
		if (pBuffers[h].mbInUse) {
			++total;

			if (pBuffers[h].mFrameInfo.mbFinal)
				++finals;
		}
		if (++h >= num_buffers)
			h = 0;
	}

	--mcsQueue;
}

const VDRenderVideoPipeFrameInfo *AVIPipe::TryReadBuffer() {
	vdsynchronized(mcsQueue) {
		if (mState & kFlagAborted)
			return NULL;

		if (mLevel)
			return &pBuffers[mReadPt].mFrameInfo;
	}

	if (mState & kFlagFinalizeTriggered) {
		mState |= kFlagFinalizeAcknowledged;

		msigRead.signal();
	}

	return NULL;
}

const VDRenderVideoPipeFrameInfo *AVIPipe::getReadBuffer() {
	for(;;) {
		vdsynchronized(mcsQueue) {
			if (mState & kFlagAborted)
				return NULL;

			if (mLevel)
				return &pBuffers[mReadPt].mFrameInfo;
		}

		if (mState & kFlagFinalizeTriggered) {
			mState |= kFlagFinalizeAcknowledged;

			msigRead.signal();
			return NULL;
		}

		msigWrite.wait();
	}
}

void AVIPipe::releaseBuffer() {
	++mcsQueue;
	pBuffers[mReadPt].mbInUse = false;
	--mLevel;
	if (++mReadPt >= num_buffers)
		mReadPt = 0;
	--mcsQueue;

	msigRead.signal();
}

void AVIPipe::finalize() {
	mState |= kFlagFinalizeTriggered;
	msigWrite.signal();
}

void AVIPipe::finalizeAck() {
	mState |= kFlagFinalizeAcknowledged;
	msigRead.signal();
}

void AVIPipe::abort() {
	vdsynchronized(mcsQueue) {
		mState |= kFlagAborted;
		msigWrite.signal();
		msigRead.signal();
	}
}
