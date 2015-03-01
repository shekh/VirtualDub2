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

#ifndef f_VD2_FILTERFRAMEBUFFERMEMORY_H
#define f_VD2_FILTERFRAMEBUFFERMEMORY_H

#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>
#include "FilterFrame.h"

///////////////////////////////////////////////////////////////////////////
//
//	VDFilterFrameBufferMemory
//
///////////////////////////////////////////////////////////////////////////

class VDFilterFrameBufferMemory	: public VDFilterFrameBuffer
{
public:
	VDFilterFrameBufferMemory();

	void Init(uint32 size);
	void Shutdown();

	void *LockWrite() { return mpBuffer; }
	const void *LockRead() const { return mpBuffer; }
	void Unlock() {}

	uint32 GetSize() const { return mBufferSize; }

protected:
	~VDFilterFrameBufferMemory();

	void *mpBuffer;
	uint32 mBufferSize;
	bool	mbVirtAlloc;
};

#endif	// f_VD2_FILTERFRAMEBUFFERMEMORY_H
