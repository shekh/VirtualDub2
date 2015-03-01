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

#ifndef f_FILTDLG_H
#define f_FILTDLG_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/vdtypes.h>
#include <vd2/system/fraction.h>

class IVDVideoSource;

struct VDVideoFiltersDialogResult {
	bool	mbDialogAccepted;
	bool	mbChangeDetected;
	bool	mbRescaleRequested;

	VDFraction	mOldFrameRate;
	sint64		mOldFrameCount;
	VDFraction	mNewFrameRate;
	sint64		mNewFrameCount;
};

VDVideoFiltersDialogResult VDShowDialogVideoFilters(VDGUIHandle h, IVDVideoSource *pVS, VDPosition initialTime);
VDVideoFiltersDialogResult VDShowDialogVideoFilters(VDGUIHandle hParent, int w, int h, int format, const VDFraction& rate, sint64 length, VDPosition initialTime);

#endif
