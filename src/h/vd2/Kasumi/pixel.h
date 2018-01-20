//	VirtualDub - Video processing and capture application
//	Graphics support library
//	Copyright (C) 1998-2009 Avery Lee
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

#ifndef f_VD2_KASUMI_PIXEL_H
#define f_VD2_KASUMI_PIXEL_H

#ifndef f_VD2_SYSTEM_VDTYPES_H
	#include <vd2/system/vdtypes.h>
#endif

struct VDPixmap;
struct VDPixmapFormatEx;

struct VDSample{
	int format;
	float r,g,b,a;
	int sr,sg,sb,sa;
	int sy,scb,scr;
};

void VDPixmapSample(const VDPixmap& px, sint32 x, sint32 y, VDSample& s);
uint32 VDPixmapSample(const VDPixmap& px, sint32 x, sint32 y);
uint32 VDPixmapInterpolateSampleRGB24(const VDPixmap& px, sint32 x, sint32 y);

inline uint8 VDPixmapSample8(const void *data, ptrdiff_t pitch, sint32 x, sint32 y) {
	return ((const uint8 *)data)[pitch*y + x];
}

inline uint16 VDPixmapSample16U(const void *data, ptrdiff_t pitch, sint32 x, sint32 y) {
	return ((const uint16 *)data)[pitch/2*y + x];
}

inline uint8 VDPixmapSample16(const void *data, ptrdiff_t pitch, sint32 x, sint32 y, uint32 ref) {
	uint16 v = ((const uint16 *)data)[pitch/2*y + x];
	if (v>ref) return 255;
	return (uint8)((v*0xFF0/ref+8)>>4);
}

uint8 VDPixmapInterpolateSample8(const void *data, ptrdiff_t pitch, uint32 w, uint32 h, sint32 x_256, sint32 y_256);
uint16 VDPixmapInterpolateSample16U(const void *data, ptrdiff_t pitch, uint32 w, uint32 h, sint32 x_256, sint32 y_256, uint32 ref, int xx=1);
uint16 VDPixmapInterpolateSample2x16U(const void *data, ptrdiff_t pitch, uint32 w, uint32 h, sint32 x_256, sint32 y_256, uint32 ref);
uint8 VDPixmapInterpolateSample16(const void *data, ptrdiff_t pitch, uint32 w, uint32 h, sint32 x_256, sint32 y_256, uint32 ref);
uint8 VDPixmapInterpolateSample2x16(const void *data, ptrdiff_t pitch, uint32 w, uint32 h, sint32 x_256, sint32 y_256, uint32 ref);

uint32 VDConvertYCbCrToRGB(uint8 y, uint8 cb, uint8 cr, bool use709, bool useFullRange);
void VDConvertYCbCrToRGB(int y, int cb, int cr, int ref, VDPixmapFormatEx& format, float& r, float& g, float& b);
uint32 VDPackRGB(float r, float g, float b);
uint32 VDPackRGBA(float r, float g, float b, float a);
uint32 VDPackRGBA8(float r, float g, float b, uint8 a);
uint32 VDConvertRGBToYCbCr(uint32 c, bool use709=false, bool useFullRange=false);
uint32 VDConvertRGBToYCbCr(uint8 r, uint8 g, uint8 b, bool use709, bool useFullRange);

#endif
