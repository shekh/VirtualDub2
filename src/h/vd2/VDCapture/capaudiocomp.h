//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2004 Avery Lee
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

#ifndef f_VD2_RIZA_CAPAUDIOCOMP_H
#define f_VD2_RIZA_CAPAUDIOCOMP_H

#include <vd2/VDCapture/capdriver.h>
#include <vd2/Priss/convert.h>
#include <mmsystem.h>

/////////////////////////////////////////////////////////////////////////////

struct VDCaptureAudioCompStatus {
};

class VDINTERFACE IVDCaptureAudioCompFilter : public IVDCaptureDriverCallback {
public:
	virtual ~IVDCaptureAudioCompFilter() {}

	virtual void SetChildCallback(IVDCaptureDriverCallback *pChild) = 0;
	virtual void SetSourceSplit(bool enable) = 0;

	virtual void Init(const WAVEFORMATEX *srcFormat, const WAVEFORMATEX *dstFormat, const char *pShortNameHint) = 0;

	virtual void GetStatus(VDCaptureAudioCompStatus&) = 0;
};

IVDCaptureAudioCompFilter *VDCreateCaptureAudioCompFilter();

#endif
