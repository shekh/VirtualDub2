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

#ifndef f_VD2_FILTERFRAMEALLOCATORPROXY_H
#define f_VD2_FILTERFRAMEALLOCATORPROXY_H

#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>

class VDFilterFrameBuffer;
class IVDFilterFrameAllocator;

///////////////////////////////////////////////////////////////////////////
//
//	VDFilterFrameAllocator
//
///////////////////////////////////////////////////////////////////////////

class VDFilterFrameAllocatorProxy {
	VDFilterFrameAllocatorProxy& operator=(const VDFilterFrameAllocatorProxy&);
public:
	enum AccelMode {
		kAccelModeNone,
		kAccelModeUpload,
		kAccelModeRender,
		kAccelModeCount
	};

	VDFilterFrameAllocatorProxy()
		: mSizeRequired(0)
		, mBorderWRequired(0)
		, mBorderHRequired(0)
		, mAccelMode(kAccelModeNone)
		, mpAllocator(NULL)
		, mpLink(NULL)
	{
	}

	VDFilterFrameAllocatorProxy(const VDFilterFrameAllocatorProxy&);
	~VDFilterFrameAllocatorProxy();

	uint32 GetSizeRequirement() const { return mSizeRequired; }
	AccelMode GetAccelerationRequirement() const { return mAccelMode; }
	uint32 GetBorderWidth() const { return mBorderWRequired; }
	uint32 GetBorderHeight() const { return mBorderHRequired; }

	void AddSizeRequirement(uint32 bytes) {
		if (mSizeRequired < bytes)
			mSizeRequired = bytes;
	}

	void AddBorderRequirement(uint32 w, uint32 h) {
		VDASSERT(w < 10000000 && h < 10000000);

		if (mBorderWRequired < w)
			mBorderWRequired = w;

		if (mBorderHRequired < h)
			mBorderHRequired = h;
	}

	void SetAccelerationRequirement(AccelMode mode) {
		mAccelMode = mode;
	}

	void AddRequirements(VDFilterFrameAllocatorProxy *src) {
		AddSizeRequirement(src->GetSizeRequirement());
		AddBorderRequirement(src->mBorderWRequired, src->mBorderHRequired);
		SetAccelerationRequirement(src->GetAccelerationRequirement());
	}

	VDFilterFrameAllocatorProxy *GetLink() const { return mpLink; }
	void Link(VDFilterFrameAllocatorProxy *prev) {
#ifdef _DEBUG
		for(VDFilterFrameAllocatorProxy *p = prev; p; p = p->mpLink) {
			VDASSERT(p != this);
		}
#endif
		mpLink = prev;
	}

	void Clear();
	void SetAllocator(IVDFilterFrameAllocator *alloc);

	void TrimAllocator();
	bool Allocate(VDFilterFrameBuffer **buffer);

protected:
	uint32	mSizeRequired;
	uint32	mBorderWRequired;
	uint32	mBorderHRequired;
	AccelMode mAccelMode;
	IVDFilterFrameAllocator *mpAllocator;
	VDFilterFrameAllocatorProxy *mpLink;
};

#endif	// f_VD2_FILTERFRAMEALLOCATORPROXY_H
