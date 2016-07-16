//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2009 Avery Lee
//
//	Animated PNG support by Max Stepin
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

#ifndef f_AVIOUTPUTAPNG_H
#define f_AVIOUTPUTAPNG_H

#include <vd2/system/VDString.h>
#include "AVIOutput.h"

class VideoSource;

class IVDAVIOutputAPNG {
public:
	virtual ~IVDAVIOutputAPNG() {}
	virtual AVIOutput *AsAVIOutput() = 0;
	virtual void SetFramesCount(int framesCount) = 0;
	virtual void SetLoopCount(int loopCount) = 0;
	virtual void SetAlpha(int alpha) = 0;
	virtual void SetGrayscale(int grayscale) = 0;
    virtual void SetRate(int rate) = 0;
    virtual void SetScale(int scale) = 0;
};

IVDAVIOutputAPNG *VDCreateAVIOutputAPNG();

#endif
