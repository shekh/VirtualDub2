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
#include <vd2/system/refcount.h>
#include "DubFrameRequestQueue.h"

VDDubFrameRequestQueue::VDDubFrameRequestQueue() {
}

VDDubFrameRequestQueue::~VDDubFrameRequestQueue() {
	Shutdown();
}

const VDSignal& VDDubFrameRequestQueue::GetNotEmptySignal() const {
	return mNotEmpty;
}

uint32 VDDubFrameRequestQueue::GetQueueLength() {
	vdsynchronized(mMutex) {
		return mQueue.size();
	}
}

void VDDubFrameRequestQueue::Shutdown() {
	vdsynchronized(mMutex) {
		mQueue.clear();
	}
}

void VDDubFrameRequestQueue::AddRequest(const VDDubFrameRequest& request) {
	bool shouldSignal = false;

	vdsynchronized(mMutex) {
		shouldSignal = mQueue.empty();

		mQueue.push_back(request);
	}

	if (shouldSignal)
		mNotEmpty.signal();
}

bool VDDubFrameRequestQueue::RemoveRequest(VDDubFrameRequest& request) {
	vdsynchronized(mMutex) {
		if (mQueue.empty())
			return false;

		request = mQueue.front();
		mQueue.pop_front();

		mLowWatermarkEvent.Raise(this, false);
		return true;
	}
}
