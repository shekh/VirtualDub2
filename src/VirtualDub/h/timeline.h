//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2004 Avery Lee
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


#ifndef f_TIMELINE_H
#define f_TIMELINE_H

#ifdef _MSC_VER
	#pragma once
#endif

#ifndef f_VD2_SYSTEM_REFCOUNT_H
	#include <vd2/system/refcount.h>
#endif

#ifndef f_FRAMESUBSET_H
	#include "FrameSubset.h"
#endif

class VDFraction;
class IVDVideoSource;

class IVDTimelineTimingSource : public IVDRefCount {
public:
	virtual sint64 GetStart() = 0;
	virtual sint64 GetLength() = 0;
	virtual const VDFraction GetRate() = 0;
	virtual sint64 GetPrevKey(sint64 pos) = 0;
	virtual sint64 GetNextKey(sint64 pos) = 0;
	virtual sint64 GetNearestKey(sint64 pos) = 0;
	virtual bool IsKey(sint64 pos) = 0;
	virtual bool IsNullSample(sint64 pos) = 0;
};

class VDTimeline {
public:
	VDTimeline();
	~VDTimeline();

	FrameSubset&	GetSubset() { return mSubset; }

	void SetTimingSource(IVDTimelineTimingSource *pT) { mpTiming = pT; }
	void SetFromSource();

	VDPosition		GetStart()			{ return 0; }
	VDPosition		GetEnd()			{ return mSubset.getTotalFrames(); }
	VDPosition		GetLength()			{ return mSubset.getTotalFrames(); }
	VDPosition		GetNearestKey(VDPosition pos);
	VDPosition		GetNearestKeyNext(sint64 pos);
	VDPosition		GetPrevKey(VDPosition pos);
	VDPosition		GetNextKey(VDPosition pos);
	VDPosition		GetPrevDrop(VDPosition pos);
	VDPosition		GetNextDrop(VDPosition pos);
	VDPosition		GetPrevEdit(VDPosition pos);
	VDPosition		GetNextEdit(VDPosition pos);

	VDPosition		TimelineToSourceFrame(VDPosition pos);

	void	Rescale(const VDFraction& oldRate, sint64 oldLength, const VDFraction& newRate, sint64 newLength);

protected:
	FrameSubset	mSubset;

	vdrefptr<IVDTimelineTimingSource> mpTiming;
};

void VDCreateTimelineTimingSourceVS(IVDVideoSource *pVS, IVDTimelineTimingSource **ppTS);

#endif
