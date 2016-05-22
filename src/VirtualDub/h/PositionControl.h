//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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

#ifndef f_POSITIONCONTROL_H
#define f_POSITIONCONTROL_H

#include <windows.h>
#include <commctrl.h>
#include <vd2/system/refcount.h>
#include <vd2/system/event.h>

#define POSITIONCONTROLCLASS (szPositionControlName)

#ifndef f_POSITIONCONTROL_CPP
extern const char szPositionControlName[];
#endif

typedef char (*PosCtlFTCallback)(HWND hwnd, void *data, long pos);

class VDFraction;
class VDTimeline;

class IVDPositionControlCallback {
public:
	virtual bool		GetFrameString(wchar_t *buf, size_t buflen, VDPosition frame) = 0;
};

struct VDPositionControlEventData {
	enum EventType {
		kEventNone,
		kEventJump,
		kEventJumpToStart,
		kEventJumpToPrev,
		kEventJumpToNext,
		kEventJumpToPrevPage,
		kEventJumpToNextPage,
		kEventJumpToPrevKey,
		kEventJumpToNextKey,
		kEventJumpToEnd,
		kEventTracking,
		kEventCount
	};

	VDPosition	mPosition;
	EventType	mEventType;	
};

class IVDPositionControl : public IVDRefCount {
public:
	virtual int			GetNiceHeight() = 0;

	virtual void		SetFrameTypeCallback(IVDPositionControlCallback *pCB) = 0;
	virtual void		SetRange(VDPosition lo, VDPosition hi, bool updateNow = true) = 0;
	virtual VDPosition	GetRangeBegin() = 0;
	virtual VDPosition	GetRangeEnd() = 0;
	virtual VDPosition	GetPosition() = 0;
	virtual void		SetPosition(VDPosition pos) = 0;
	virtual void		SetDisplayedPosition(VDPosition pos) = 0;
	virtual bool		GetSelection(VDPosition& start, VDPosition& end) = 0;
	virtual void		SetSelection(VDPosition start, VDPosition end, bool updateNow = true) = 0;
	virtual void		SetTimeline(VDTimeline& t) = 0;
	virtual void		SetFrameRate(const VDFraction& frameRate) = 0;
	virtual void		SetAutoPositionUpdate(bool autoUpdate) = 0;
	virtual void		SetAutoStep(bool autoStep) = 0;

	virtual void		ResetShuttle() = 0;

	virtual VDEvent<IVDPositionControl, VDPositionControlEventData>&	PositionUpdated() = 0;
};

IVDPositionControl *VDGetIPositionControl(VDGUIHandle h);

#define PCS_PLAYBACK			(0x00000001L)
#define PCS_MARK				(0x00000002L)
#define	PCS_SCENE				(0x00000004L)

#define PCN_THUMBTRACK			(NM_FIRST+0)
#define PCN_THUMBPOSITION		(NM_FIRST+1)
#define PCN_THUMBPOSITIONPREV	(NM_FIRST+2)
#define PCN_THUMBPOSITIONNEXT	(NM_FIRST+3)
#define PCN_PAGELEFT			(NM_FIRST+4)
#define PCN_PAGERIGHT			(NM_FIRST+5)
#define PCN_BEGINTRACK			(NM_FIRST+6)
#define PCN_ENDTRACK			(NM_FIRST+7)

#define PCN_STOP				(0)
#define PCN_PLAY				(1)
#define	PCN_PLAYPREVIEW			(10)
#define PCN_MARKIN				(2)
#define PCN_MARKOUT				(3)
#define PCN_START				(4)
#define PCN_BACKWARD			(5)
#define PCN_FORWARD				(6)
#define PCN_END					(7)
#define PCN_KEYPREV				(8)
#define PCN_KEYNEXT				(9)
#define	PCN_SCENEREV			(11)
#define	PCN_SCENEFWD			(12)
#define	PCN_SCENESTOP			(13)

ATOM RegisterPositionControl();

#endif
