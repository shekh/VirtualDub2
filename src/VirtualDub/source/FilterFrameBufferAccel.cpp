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
#include <vd2/Tessa/Context.h>
#include "FilterAccelEngine.h"
#include "FilterFrameBufferAccel.h"

///////////////////////////////////////////////////////////////////////////

VDFilterFrameBufferAccel::VDFilterFrameBufferAccel()
	: mpAccelEngine(NULL)
	, mpTexture(NULL)
	, mWidth(0)
	, mHeight(0)
	, mBorderWidth(0)
	, mBorderHeight(0)
{
}

VDFilterFrameBufferAccel::~VDFilterFrameBufferAccel() {
	Shutdown();
}

void *VDFilterFrameBufferAccel::AsInterface(uint32 iid) {
	if (iid == VDFilterFrameBufferAccel::kTypeID)
		return static_cast<VDFilterFrameBufferAccel *>(this);

	return VDFilterFrameBuffer::AsInterface(iid);
}

void VDFilterFrameBufferAccel::Init(VDFilterAccelEngine *accelEngine, uint32 width, uint32 height, uint32 borderWidth, uint32 borderHeight) {
	VDASSERT(width && height);
	mpAccelEngine = accelEngine;
	mWidth = width;
	mHeight = height;
	mBorderWidth = borderWidth;
	mBorderHeight = borderHeight;
}

void VDFilterFrameBufferAccel::Shutdown() {
	if (mpTexture)
		mpAccelEngine->DecommitBuffer(this);

	VDFilterFrameBuffer::Shutdown();
}

void VDFilterFrameBufferAccel::SetTexture(IVDTTexture2D *tex) {
	if (tex)
		tex->AddRef();

	if (mpTexture)
		mpTexture->Release();

	mpTexture = tex;
}

void *VDFilterFrameBufferAccel::LockWrite() {
	return NULL;
}

const void *VDFilterFrameBufferAccel::LockRead() const {
	return NULL;
}

void VDFilterFrameBufferAccel::Unlock() {
}

uint32 VDFilterFrameBufferAccel::GetSize() const {
	return 0;
}

void VDFilterFrameBufferAccel::Decommit() {
	if (mpTexture) {
		mpTexture->Release();
		mpTexture = NULL;
	}
}
