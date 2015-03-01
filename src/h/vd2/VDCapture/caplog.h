//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2005 Avery Lee
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

#ifndef f_VD2_RIZA_CAPLOG_H
#define f_VD2_RIZA_CAPLOG_H

#include <vd2/system/vdtypes.h>
#include <vd2/VDCapture/capdriver.h>

struct VDCaptureLogStatistics {
	double	mFrameRate;
	double	mVideoStartTime;
	double	mAudioStartTime;
};

class VDINTERFACE IVDCaptureLogFilter : public IVDCaptureDriverCallback {
public:
	virtual ~IVDCaptureLogFilter() {}

	virtual void SetChildCallback(IVDCaptureDriverCallback *pChild) = 0;

	virtual void WriteLog(const wchar_t *pszName) = 0;
};

IVDCaptureLogFilter *VDCreateCaptureLogFilter();


#endif
