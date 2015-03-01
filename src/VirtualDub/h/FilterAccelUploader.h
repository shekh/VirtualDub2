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

#ifndef f_VD2_FILTERACCELUPLOADER_H
#define f_VD2_FILTERACCELUPLOADER_H

#include "FilterInstance.h"
#include "FilterFrameManualSource.h"
#include <vd2/Kasumi/blitter.h>

class VDFilterAccelEngine;
class VDFilterFrameBufferAccel;

class VDFilterAccelUploader : public VDFilterFrameManualSource {
	VDFilterAccelUploader(const VDFilterAccelUploader&);
	VDFilterAccelUploader& operator=(const VDFilterAccelUploader&);
public:
	VDFilterAccelUploader();
	~VDFilterAccelUploader();

	bool IsAccelerated() const { return true; }

	void Init(VDFilterAccelEngine *engine, IVDFilterFrameSource *source, const VDPixmapLayout& outputLayout, const VDPixmapLayout *sourceLayoutOverride);
	void Start(IVDFilterFrameEngine *frameEngine);
	void Stop();

	bool GetDirectMapping(sint64 outputFrame, sint64& sourceFrame, int& sourceIndex);
	sint64 GetSourceFrame(sint64 outputFrame);
	sint64 GetSymbolicFrame(sint64 outputFrame, IVDFilterFrameSource *source);
	sint64 GetNearestUniqueFrame(sint64 outputFrame);

	RunResult RunRequests(const uint32 *batchNumberLimit);
	RunResult RunProcess();

protected:
	bool InitNewRequest(VDFilterFrameRequest *req, sint64 outputFrame, bool writable, uint32 batchNumber);

	VDFilterAccelEngine	*mpEngine;
	IVDFilterFrameSource *mpSource;
	IVDFilterFrameEngine *mpFrameEngine;
	VDPixmapLayout		mSourceLayout;

	vdautoptr<IVDPixmapBlitter> mpBlitter;
	vdrefptr<VDFilterFrameRequest> mpRequest;

	enum ProcessStatus {
		kProcess_Idle,
		kProcess_Pending,
		kProcess_Succeeded,
		kProcess_Failed
	};

	VDAtomicInt mProcessStatus;
	VDFilterFrameBufferAccel *mpLockedDst;
	const void *mpLockedSrc;
};

#endif
