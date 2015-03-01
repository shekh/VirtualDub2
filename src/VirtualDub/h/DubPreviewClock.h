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

#ifndef f_VD2_DUBPREVIEWCLOCK_H
#define f_VD2_DUBPREVIEWCLOCK_H

#include <vd2/system/event.h>
#include <vd2/system/time.h>

class IVDAsyncBlitter;
class IVDDubPreviewTimer {
public:
	virtual uint32 GetPreviewTime() = 0;
};

class VDDubPreviewClock : public IVDTimerCallback {
	VDDubPreviewClock(VDDubPreviewClock&);
	VDDubPreviewClock& operator=(const VDDubPreviewClock&);
public:
	VDDubPreviewClock();
	~VDDubPreviewClock();

	void Init(IVDDubPreviewTimer *timer, IVDAsyncBlitter *blitter, double frameRate, double frameMultiplicationFactor);
	void Shutdown();

	VDEvent<VDDubPreviewClock, uint32>& OnClockUpdated() {
		return mEventClockUpdated;
	}

protected:
	void TimerCallback();

	uint32 ReadClock() const;

	IVDDubPreviewTimer *mpTimer;
	IVDAsyncBlitter *mpBlitter;
	uint32	mBaseTime;
	double	mTicksToFrames;
	VDCallbackTimer		mFrameTimer;

	VDEvent<VDDubPreviewClock, uint32> mEventClockUpdated;
};

#endif	// f_VD2_DUBPREVIEWCLOCK_H
