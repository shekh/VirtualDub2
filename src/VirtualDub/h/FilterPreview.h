//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2007 Avery Lee
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

#ifndef f_VD2_FILTERPREVIEW_H
#define f_VD2_FILTERPREVIEW_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <windows.h>
#include <vd2/system/error.h>
#include <vd2/system/list.h>
#include <vd2/plugin/vdvideofilt.h>
#include "FilterSystem.h"
#include "gui.h"

class FilterInstance;
class IVDPositionControl;
class IVDVideoDisplay;
class VDTimeline;

class IVDVideoFilterPreviewDialog : public IVDRefCount {
public:
	virtual IVDXFilterPreview2 *AsIVDXFilterPreview2() = 0;
	virtual void SetInitialTime(VDTime t) = 0;
};

bool VDCreateVideoFilterPreviewDialog(VDFilterChainDesc *, FilterInstance *, IVDVideoFilterPreviewDialog **);

#endif
