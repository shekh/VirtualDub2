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

#ifndef f_VD2_FILTERFRAMEBUFFERACCEL_H
#define f_VD2_FILTERFRAMEBUFFERACCEL_H

#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>
#include "FilterFrame.h"

class IVDTTexture2D;
class VDFilterAccelEngine;

///////////////////////////////////////////////////////////////////////////
//
//	VDFilterFrameBufferAccel
//
///////////////////////////////////////////////////////////////////////////

class VDFilterFrameBufferAccel : public VDFilterFrameBuffer
{
public:
	enum { kTypeID = 'fbxa' };

	VDFilterFrameBufferAccel();

	void *AsInterface(uint32 iid);

	void Init(VDFilterAccelEngine *accelEngine, uint32 width, uint32 height, uint32 borderWidth, uint32 borderHeight);
	void Shutdown();

	uint32 GetWidth() const { return mWidth; }
	uint32 GetHeight() const { return mHeight; }
	uint32 GetBorderWidth() const { return mBorderWidth; }
	uint32 GetBorderHeight() const { return mBorderHeight; }

	IVDTTexture2D *GetTexture() const { return mpTexture; }
	void SetTexture(IVDTTexture2D *tex);

	void *LockWrite();
	const void *LockRead() const;
	void Unlock();

	uint32 GetSize() const;

public:
	void Decommit();

protected:
	~VDFilterFrameBufferAccel();

	VDFilterAccelEngine *mpAccelEngine;
	IVDTTexture2D *mpTexture;
	uint32	mWidth;
	uint32	mHeight;
	uint32	mBorderWidth;
	uint32	mBorderHeight;
};

#endif	// f_VD2_FILTERFRAMEBUFFERACCEL_H
