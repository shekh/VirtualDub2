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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

#ifndef f_VD2_FILTERACCELCONVERTER_H
#define f_VD2_FILTERACCELCONVERTER_H

#include <vd2/system/vectors.h>
#include "FilterInstance.h"
#include "FilterFrameManualSource.h"

class VDFilterAccelEngine;
class VDFilterFrameBufferAccel;

class VDFilterAccelConverter : public VDFilterFrameManualSource {
	VDFilterAccelConverter(const VDFilterAccelConverter&);
	VDFilterAccelConverter& operator=(const VDFilterAccelConverter&);
public:
	VDFilterAccelConverter();
	~VDFilterAccelConverter();

	void Init(VDFilterAccelEngine *engine, IVDFilterFrameSource *source, const VDPixmapLayout& outputLayout, const vdrect32 *srcRect);
	void Start(IVDFilterFrameEngine *frameEngine);
	void Stop();

	bool GetDirectMapping(sint64 outputFrame, sint64& sourceFrame, int& sourceIndex);
	sint64 GetSourceFrame(sint64 outputFrame);
	sint64 GetSymbolicFrame(sint64 outputFrame, IVDFilterFrameSource *source);
	sint64 GetNearestUniqueFrame(sint64 outputFrame);

	RunResult RunRequests(const uint32 *batchNumberLimit, int index);
	RunResult RunProcess(int index);

protected:
	bool InitNewRequest(VDFilterFrameRequest *req, sint64 outputFrame, bool writable, uint32 batchNumber);

	VDFilterAccelEngine	*mpEngine;
	IVDFilterFrameSource *mpSource;
	IVDFilterFrameEngine *mpFrameEngine;
	VDPixmapLayout		mSourceLayout;
	vdrect32			mSourceRect;

	vdrefptr<VDFilterFrameRequest> mpRequest;

	enum ProcessStatus {
		kProcess_Idle,
		kProcess_Pending,
		kProcess_Succeeded,
		kProcess_Failed
	};

	VDAtomicInt mProcessStatus;
	VDFilterFrameBufferAccel *mpLockedDst;
	VDFilterFrameBufferAccel *mpLockedSrc;
};

#endif
