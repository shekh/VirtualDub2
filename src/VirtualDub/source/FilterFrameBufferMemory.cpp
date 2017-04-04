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
#include <vd2/system/error.h>
#include "FilterFrameBufferMemory.h"

namespace {
	const uint32 max_align = 64;
}

///////////////////////////////////////////////////////////////////////////

VDFilterFrameBufferMemory::VDFilterFrameBufferMemory()
	: mpBuffer(NULL)
	, mBufferSize(0)
	, mbVirtAlloc(false)
{
}

VDFilterFrameBufferMemory::~VDFilterFrameBufferMemory() {
	Shutdown();
}

void VDFilterFrameBufferMemory::Init(uint32 size) {
	VDASSERT(!mpBuffer);

	if (size >= 262144) {
		mbVirtAlloc = true;
		mpBuffer = VDAlignedVirtualAlloc(size);
	} else {
		mbVirtAlloc = false;
		mpBuffer = VDAlignedMalloc(size, max_align);
	}

	if (!mpBuffer)
		throw MyMemoryError(size);

	mBufferSize = size;
}

void VDFilterFrameBufferMemory::Shutdown() {
	VDFilterFrameBuffer::Shutdown();

	if (mpBuffer) {
		if (mbVirtAlloc)
			VDAlignedVirtualFree(mpBuffer);
		else
			VDAlignedFree(mpBuffer);
		mpBuffer = NULL;
		mBufferSize = 0;
	}
}
