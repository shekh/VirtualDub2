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

#ifndef f_VD2_IMAGEIFF_H
#define f_VD2_IMAGEIFF_H

#include <vd2/system/vdtypes.h>

struct VDPixmap;

class VDINTERFACE IVDImageDecoderIFF {
public:
	virtual ~IVDImageDecoderIFF() {}

	virtual const VDPixmap& Decode(const void *src, uint32 srclen) = 0;
};

IVDImageDecoderIFF *VDCreateImageDecoderIFF();

#endif
