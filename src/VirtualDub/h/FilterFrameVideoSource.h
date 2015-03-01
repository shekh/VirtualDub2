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

#ifndef f_VD2_FILTERFRAMEVIDEOSOURCE_H
#define f_VD2_FILTERFRAMEVIDEOSOURCE_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/blitter.h>
#include "FilterFrameManualSource.h"

class IVDVideoSource;
class IVDStreamSource;

class VDFilterFrameVideoSource : public VDFilterFrameManualSource {
	VDFilterFrameVideoSource(const VDFilterFrameVideoSource&);
	VDFilterFrameVideoSource& operator=(const VDFilterFrameVideoSource&);

public:
	VDFilterFrameVideoSource();
	~VDFilterFrameVideoSource();

	void Init(IVDVideoSource *vs, const VDPixmapLayout& layout);

	RunResult RunRequests(const uint32 *batchNumberLimit);

public:	// IVDFilterFrameSource
	virtual sint64 GetNearestUniqueFrame(sint64 outputFrame);

protected:
	IVDVideoSource *mpVS;
	IVDStreamSource *mpSS;
	VDFilterFrameRequest *mpRequest;

	uint32		mDecodePadding;
	VDPosition	mTargetSample;
	bool		mbFirstSample;

	vdfastvector<uint8> mBuffer;
	vdautoptr<IVDPixmapBlitter>	mpBlitter;
};

#endif
