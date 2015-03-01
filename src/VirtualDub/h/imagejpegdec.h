//	VirtualDub - Video processing and capture application
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

#ifndef f_IMAGEJPEGDEC_H
#define f_IMAGEJPEGDEC_H

#include <vd2/system/vdtypes.h>

class IVDJPEGDecoder {
public:
	enum {
		kFormatXRGB1555,
		kFormatRGB888,
		kFormatXRGB8888
	};

	virtual ~IVDJPEGDecoder() {}

	virtual void Begin(const void *src, uint32 srclen) = 0;
	virtual void DecodeHeader(int& w, int& h) = 0;
	virtual void DecodeImage(void *dst, ptrdiff_t dstpitch, int format) = 0;
	virtual void End() = 0;
};

IVDJPEGDecoder *VDCreateJPEGDecoder();

#endif
