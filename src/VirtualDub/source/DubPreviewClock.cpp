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
#include <vd2/system/time.h>
#include "AsyncBlitter.h"
#include "DubPreviewClock.h"

VDDubPreviewClock::VDDubPreviewClock()
	: mpTimer(NULL)
	, mpBlitter(NULL)
	, mTicksToFrames(1.0)
{
}

VDDubPreviewClock::~VDDubPreviewClock() {
}

void VDDubPreviewClock::Init(IVDDubPreviewTimer *timer, IVDAsyncBlitter *blitter, double frameRate, double frameMultiplicationFactor) {
	mpTimer = timer;
	mpBlitter = blitter;
	mBaseTime = VDGetAccurateTick();
	mTicksToFrames = frameRate / 1000.0 * frameMultiplicationFactor;

	sint32 resolution;
	if (frameRate > 100.0) {
		resolution = VDRoundToInt32(10000000.0 / frameRate);
	} else {
		resolution = VDRoundToInt32(10000000.0 / 4.0 / frameRate);

		if (resolution < 10000)
			resolution = 10000;
	}

	mFrameTimer.Init3(this, resolution, resolution, false);
}

void VDDubPreviewClock::Shutdown() {
	mFrameTimer.Shutdown();
}

void VDDubPreviewClock::TimerCallback() {
	const uint32 clock = ReadClock();

	if (mpBlitter)
		mpBlitter->setPulseClock(clock);

	mEventClockUpdated.Raise(this, clock);
}

uint32 VDDubPreviewClock::ReadClock() const {
	uint32 ticks;
	if (mpTimer)
		ticks = mpTimer->GetPreviewTime();
	else
		ticks = VDGetAccurateTick() - mBaseTime;

	return VDRoundToInt32((double)ticks * mTicksToFrames);
}
