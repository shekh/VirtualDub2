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

#ifndef f_VD2_FILTERFRAMEQUEUE_H
#define f_VD2_FILTERFRAMEQUEUE_H

#include <vd2/system/vdstl.h>
#include "FilterFrameRequest.h"

class VDFilterFrameRequest;
class VDTextOutputStream;

class VDFilterFrameQueue {
	VDFilterFrameQueue(const VDFilterFrameQueue&);
	VDFilterFrameQueue& operator=(const VDFilterFrameQueue&);
public:
	VDFilterFrameQueue();
	~VDFilterFrameQueue();

	void Shutdown();

	bool GetRequest(sint64 frame, VDFilterFrameRequest **req);
	void CompleteRequests(sint64 frame, VDFilterFrameBuffer *buf);

	void CreateRequest(VDFilterFrameRequest **req);
	bool PeekNextRequest(const uint32 *batchNumberLimit, VDFilterFrameRequest **req);
	bool GetNextRequest(const uint32 *batchNumberLimit, VDFilterFrameRequest **req);

	void Add(VDFilterFrameRequest *req);
	bool Remove(VDFilterFrameRequest *req);

	void DumpStatus(VDTextOutputStream& os);

#ifdef _DEBUG
	void ValidateState();
#else
	inline void ValidateState() {}
#endif

protected:
	typedef vdfastdeque<VDFilterFrameRequest *> Requests;
	Requests mRequests;

	VDFilterFrameRequestAllocator mAllocator;
};

#endif	// f_VD2_FILTERFRAMEQUEUE_H
