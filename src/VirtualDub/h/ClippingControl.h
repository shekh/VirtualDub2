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

#ifndef f_CLIPPINGCONTROL_H
#define f_CLIPPINGCONTROL_H

#include <windows.h>

#include <vd2/system/vectors.h>

struct VDPixmap;

#define CLIPPINGCONTROLCLASS (szClippingControlName)

#ifndef f_CLIPPINGCONTROL_CPP
extern const char szClippingControlName[];
#endif

#define CCS_POSITION			(0x00000001L)
#define CCS_FRAME				(0x00000002L)

#define CCN_REFRESHFRAME		(NM_FIRST+32)

#define CCM__FIRST				(WM_USER+0x100)
#define CCM__LAST				(WM_USER+0x120)
#define CCM_SETBITMAPSIZE		(WM_USER+0x120)
#define CCM_SETCLIPBOUNDS		(WM_USER+0x121)
#define CCM_GETCLIPBOUNDS		(WM_USER+0x122)
#define CCM_BLITFRAME2			(WM_USER+0x124)

class VDINTERFACE IVDClippingControl {
public:
	virtual void SetBitmapSize(int sourceW, int sourceH) = 0;
	virtual void SetClipBounds(const vdrect32& r) = 0;
	virtual void GetClipBounds(vdrect32& r) = 0;
	virtual void AutoSize(int borderW, int borderH);
	virtual void BlitFrame(const VDPixmap *px) = 0;
	virtual void SetFillBorder(bool v) = 0;
};

struct ClippingControlBounds {
	sint32 x1, y1, x2, y2;
};

ATOM RegisterClippingControl();

class IVDPositionControl;

IVDClippingControl *VDGetIClippingControl(VDGUIHandle h);
IVDPositionControl *VDGetIPositionControlFromClippingControl(VDGUIHandle h);

#endif
