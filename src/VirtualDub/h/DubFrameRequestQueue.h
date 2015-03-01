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

#ifndef f_VD2_DUBFRAMEREQUESTQUEUE_H
#define f_VD2_DUBFRAMEREQUESTQUEUE_H

#include <vd2/system/thread.h>
#include <vd2/system/event.h>

struct VDDubFrameRequest {
	sint64	mSrcFrame;
	bool	mbDirect;
};

class VDDubFrameRequestQueue {
	VDDubFrameRequestQueue(const VDDubFrameRequestQueue&);
	VDDubFrameRequestQueue& operator=(const VDDubFrameRequestQueue&);

public:
	VDDubFrameRequestQueue();
	~VDDubFrameRequestQueue();

	const VDSignal& GetNotEmptySignal() const;
	uint32 GetQueueLength();

	void Shutdown();

	void AddRequest(const VDDubFrameRequest& request);
	bool RemoveRequest(VDDubFrameRequest& request);

	VDEvent<VDDubFrameRequestQueue, bool>& OnLowWatermark() {
		return mLowWatermarkEvent;
	}

protected:
	typedef vdfastdeque<VDDubFrameRequest> Queue;
	Queue mQueue;

	VDSignal mNotEmpty;
	VDCriticalSection mMutex;

	VDEvent<VDDubFrameRequestQueue, bool> mLowWatermarkEvent;
};

#endif
