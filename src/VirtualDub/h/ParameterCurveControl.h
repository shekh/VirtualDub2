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

#ifndef f_VD2_PARAMETERCURVECONTROL_H
#define f_VD2_PARAMETERCURVECONTROL_H

#include <vd2/system/unknown.h>
#include <vd2/system/event.h>

#define VDPARAMETERCURVECONTROLCLASS (g_VDParameterCurveControlClass)

extern const char g_VDParameterCurveControlClass[];

class VDParameterCurve;

class IVDUIParameterCurveControl : public IVDRefUnknown {
public:
	enum { kTypeID = 'uipc' };

	enum Status {
		kStatus_Nothing,
		kStatus_Focused,
		kStatus_PointHighlighted,
		kStatus_PointDrag
	};

	virtual VDParameterCurve *GetCurve() = 0;
	virtual void SetCurve(VDParameterCurve *pCurve) = 0;

	virtual void SetPosition(VDPosition pos) = 0;
	virtual void SetSelectedPoint(int x) = 0;
	virtual int GetSelectedPoint() = 0;
	virtual void DeletePoint(int x) = 0;
	virtual void SetValue(int x, double v) = 0;

	// events
	virtual VDEvent<IVDUIParameterCurveControl, int>& CurveUpdatedEvent() = 0;
	virtual VDEvent<IVDUIParameterCurveControl, Status>& StatusUpdatedEvent() = 0;
};

IVDUIParameterCurveControl *VDGetIUIParameterCurveControl(VDGUIHandle h);

#endif
