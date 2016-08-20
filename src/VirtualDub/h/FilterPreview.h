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
class IVDPixmapViewDialog;

typedef void (__cdecl *PixmapViewDestroyCallback)(IVDPixmapViewDialog* view, void *pData);

class IVDPixmapViewDialog : public IVDRefCount {
public:
	virtual void Display(VDXHWND hwndParent, const wchar_t* title) = 0;
	virtual void Destroy() = 0;
	virtual void SetImage(VDPixmap& px) = 0;
	virtual void SetDestroyCallback(PixmapViewDestroyCallback cb, void* cbData) = 0;
};

class IVDVideoFilterPreviewDialog : public IVDRefCount {
public:
	virtual IVDXFilterPreview2 *AsIVDXFilterPreview2() = 0;
	virtual IFilterModPreview *AsIFilterModPreview() = 0;
	virtual void SetInitialTime(VDTime t) = 0;
	virtual void SetFilterList(HWND w) = 0;
	virtual HWND GetHwnd() = 0;
	virtual void RedoFrame2() = 0;
};

bool VDCreatePixmapViewDialog(IVDPixmapViewDialog **);
bool VDCreateVideoFilterPreviewDialog(FilterSystem *, VDFilterChainDesc *, FilterInstance *, IVDVideoFilterPreviewDialog **);

#endif
