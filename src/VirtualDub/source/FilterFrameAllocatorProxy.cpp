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
#include "FilterFrameAllocatorProxy.h"
#include "FilterFrameAllocator.h"

VDFilterFrameAllocatorProxy::VDFilterFrameAllocatorProxy(const VDFilterFrameAllocatorProxy& src)
	: mSizeRequired(src.mSizeRequired)
	, mBorderWRequired(src.mBorderWRequired)
	, mBorderHRequired(src.mBorderHRequired)
	, mAccelMode(src.mAccelMode)
	, mpAllocator(src.mpAllocator)
	, mpLink(NULL)
{
	if (mpAllocator)
		mpAllocator->AddRef();
}

VDFilterFrameAllocatorProxy::~VDFilterFrameAllocatorProxy() {
	SetAllocator(NULL);
}

VDFilterFrameAllocatorProxy& VDFilterFrameAllocatorProxy::operator=(const VDFilterFrameAllocatorProxy& src) {
	if (&src != this) {
		SetAllocator(src.mpAllocator);
		mSizeRequired = src.mSizeRequired;
		mBorderWRequired = src.mBorderWRequired;
		mBorderHRequired = src.mBorderHRequired;
		mAccelMode = src.mAccelMode;
		mpLink = src.mpLink;
	}

	return *this;
}

void VDFilterFrameAllocatorProxy::Clear() {
	SetAllocator(NULL);
	mSizeRequired = 0;
	mBorderWRequired = 0;
	mBorderHRequired = 0;
	mAccelMode = kAccelModeNone;
	mpLink = NULL;
}

void VDFilterFrameAllocatorProxy::SetAllocator(IVDFilterFrameAllocator *alloc) {
	if (alloc != mpAllocator) {
		if (alloc)
			alloc->AddRef();
		if (mpAllocator)
			mpAllocator->Release();
		mpAllocator = alloc;
	}
}

void VDFilterFrameAllocatorProxy::TrimAllocator() {
	if (mpAllocator)
		mpAllocator->Trim();
}

bool VDFilterFrameAllocatorProxy::Allocate(VDFilterFrameBuffer **buffer) {
	return mpAllocator && mpAllocator->Allocate(buffer);
}
