//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#ifndef f_AF_POLYPHASE_H
#define f_AF_POLYPHASE_H

#include <vector>
#include <vd2/system/VDRingBuffer.h>
#include "af_base.h"

class VDAudioFilterSymmetricFIR : public VDAudioFilterBase {
protected:
	VDAudioFilterSymmetricFIR();
	~VDAudioFilterSymmetricFIR();

	uint32 Prepare();
	uint32 Run();
	void Start();

	sint64 Seek(sint64);

	virtual void GenerateFilter(int freq) = 0;

	std::vector<sint16, vdaligned_alloc<sint16> >		mFilterBank;
	int mFilterSize;

	std::vector<sint16>		mFIRBuffer;
	int mFIRBufferChannelStride;
	int mFIRBufferReadPoint;
	int mFIRBufferWritePoint;
	int mFIRBufferLimit;
	int mMaxQuantum;

	VDRingBuffer<char>		mOutputBuffer;
};

class VDAudioFilterPolyphase : public VDAudioFilterBase {
protected:
	VDAudioFilterPolyphase();
	~VDAudioFilterPolyphase();

	uint32 Prepare();
	uint32 Run();
	void Start();

	sint64 Seek(sint64);

	virtual int GenerateFilterBank(int freq) = 0;

	std::vector<sint16, vdaligned_alloc<sint16> >		mFilterBank;
	int mFilterSize;
	uint32 mCurrentPhase;

	std::vector<sint16>		mFIRBuffer;
	int mFIRBufferChannelStride;
	int mFIRBufferPoint;
	int mFIRBufferLimit;

	VDRingBuffer<char>		mOutputBuffer;

	uint64		mRatio;		// 32:32
};

#endif
