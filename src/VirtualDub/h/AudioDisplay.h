//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2006 Avery Lee
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

#ifndef f_VD2_AUDIODISPLAY_H
#define f_VD2_AUDIODISPLAY_H

#include <vd2/system/unknown.h>
#include <vd2/system/event.h>

#define AUDIODISPLAYCONTROLCLASS (g_szAudioDisplayControlName)

extern const char g_szAudioDisplayControlName[];

struct VDUIAudioDisplaySelectionRange {
	VDPosition mStart;
	VDPosition mEnd;
};

enum {
	afmt_none,
	afmt_u8,
	afmt_s16,
	afmt_float,
};

class IVDUIAudioDisplayControl : public IVDRefUnknown {
public:
	enum { kTypeID = 'uiad' };

	enum Mode {
		kModeWaveform,
		kModeSpectrogram,
		kModeCount
	};

	virtual Mode GetMode() = 0;
	virtual void SetMode(Mode mode) = 0;

	virtual int GetZoom() = 0;
	virtual void SetZoom(int samplesPerPixel) = 0;

	virtual bool GetMonoMode() = 0;
	virtual void SetMonoMode(bool v) = 0;

	virtual void ClearFailureMessage() = 0;
	virtual void SetFailureMessage(const wchar_t *s) = 0;

	virtual void SetFormat(double samplingRate, int channelCount) = 0;
	virtual void SetFrameMarkers(sint64 mn, sint64 mx, double start, double rate) = 0;
	virtual void SetSelectedFrameRange(VDPosition start, VDPosition end) = 0;
	virtual void ClearSelectedFrameRange() = 0;
	virtual void SetPosition(VDPosition pos, VDPosition hpos) = 0;
	virtual void Rescan(bool redraw=true) = 0;

	virtual VDPosition GetReadPosition() = 0;
	virtual bool ProcessAudio(const void *src, int count, const VDWaveFormat *wfex) = 0;

	virtual VDEvent<IVDUIAudioDisplayControl, VDPosition>& AudioRequiredEvent() = 0;
	virtual VDEvent<IVDUIAudioDisplayControl, VDUIAudioDisplaySelectionRange>& SetSelectStartEvent() = 0;
	virtual VDEvent<IVDUIAudioDisplayControl, VDUIAudioDisplaySelectionRange>& SetSelectTrackEvent() = 0;
	virtual VDEvent<IVDUIAudioDisplayControl, VDUIAudioDisplaySelectionRange>& SetSelectEndEvent() = 0;
	virtual VDEvent<IVDUIAudioDisplayControl, VDPosition>& SetPositionEvent() = 0;
	virtual VDEvent<IVDUIAudioDisplayControl, sint32>& TrackAudioOffsetEvent() = 0;
	virtual VDEvent<IVDUIAudioDisplayControl, sint32>& SetAudioOffsetEvent() = 0;
};

IVDUIAudioDisplayControl *VDGetIUIAudioDisplayControl(VDGUIHandle h);

#endif
