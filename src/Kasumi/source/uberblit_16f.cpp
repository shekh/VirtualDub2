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

#include <stdafx.h>
#include <vd2/system/halffloat.h>
#include <vd2/system/math.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "uberblit_16f.h"

///////////////////////////////////////////////////////////////////////////////

void VDPixmapGen_32F_To_16F::Start() {
	StartWindow(mWidth * sizeof(uint16));
}

uint32 VDPixmapGen_32F_To_16F::GetType(uint32 output) const {
	return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_16F_LE;
}

void VDPixmapGen_32F_To_16F::Compute(void *dst0, sint32 y) {
	uint16 *dst = (uint16 *)dst0;
	const float *src = (const float *)mpSrc->GetRow(y, mSrcIndex);
	uint32 w = mWidth;

	for(uint32 i=0; i<w; ++i)
		*dst++ = VDConvertFloatToHalf(src++);
}

///////////////////////////////////////////////////////////////////////////////

void VDPixmapGen_16F_To_32F::Start() {
	StartWindow(mWidth * sizeof(float));
}

uint32 VDPixmapGen_16F_To_32F::GetType(uint32 output) const {
	return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_32F_LE;
}

void VDPixmapGen_16F_To_32F::Compute(void *dst0, sint32 y) {
	float *dst = (float *)dst0;
	const uint16 *src = (const uint16 *)mpSrc->GetRow(y, mSrcIndex);
	uint32 w = mWidth;

	for(uint32 i=0; i<w; ++i)
		VDConvertHalfToFloat(*src++, dst++);
}

///////////////////////////////////////////////////////////////////////////////

inline uint16 ClampedRoundToUInt16(float x) {
	int v = VDRoundToIntFast(x);
	if(v<0) v = 0;
	if(v>0xFFFF) v = 0xFFFF;
	return v;
}

void VDPixmapGen_32F_To_16::Start() {
	StartWindow(mWidth * sizeof(uint16));
}

uint32 VDPixmapGen_32F_To_16::GetType(uint32 output) const {
	return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_16_LE;
}

void VDPixmapGen_32F_To_16::Compute(void *dst0, sint32 y) {
	VDCPUCleanupExtensions();

	uint16 *dst = (uint16 *)dst0;
	const float *src = (const float *)mpSrc->GetRow(y, mSrcIndex);
	uint32 w = mWidth;

	for(uint32 i=0; i<w; ++i){
		*dst = ClampedRoundToUInt16(*src*m);
		dst++;
		src++;
	}
}

///////////////////////////////////////////////////////////////////////////////

void VDPixmapGen_16_To_32F::Start() {
	StartWindow(mWidth * sizeof(float));
}

uint32 VDPixmapGen_16_To_32F::GetType(uint32 output) const {
	return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_32F_LE;
}

void VDPixmapGen_16_To_32F::Compute(void *dst0, sint32 y) {
	VDCPUCleanupExtensions();

	float *dst = (float *)dst0;
	const uint16 *src = (const uint16 *)mpSrc->GetRow(y, mSrcIndex);
	uint32 w = mWidth;

	for(uint32 i=0; i<w; ++i){
		*dst = *src*m;
		dst++;
		src++;
	}
}

void VDPixmap_YUV_Normalize(VDPixmap& pxdst, const VDPixmap& pxsrc, uint32 max_value) {
	int ref = pxsrc.info.ref_r;
	uint32 m = max_value*0x10000/ref;
	pxdst.info = pxsrc.info;
	pxdst.info.ref_r = max_value;
	const VDPixmapFormatInfo& info = VDPixmapGetInfo(pxsrc.format);
	const vdpixsize auxw = -(-pxsrc.w >> info.auxwbits);
	const vdpixsize auxh = -(-pxsrc.h >> info.auxhbits);

	{for(sint32 y=0; y<pxsrc.h; y++) {
		const uint16 *src = (const uint16 *)(size_t(pxsrc.data) + pxsrc.pitch*y);
		uint16 *dst = (uint16 *)(size_t(pxdst.data) + pxdst.pitch*y);

		{for(sint32 x=0; x<pxsrc.w; x++) {
			uint16 v = *src;
			src++;

			if(v>ref) v=max_value; else v=(v*m)>>16;

			*dst = v;
			dst++;
		}}
	}}

	{for(sint32 y=0; y<auxh; y++) {
		const uint16 *src = (const uint16 *)(size_t(pxsrc.data2) + pxsrc.pitch2*y);
		uint16 *dst = (uint16 *)(size_t(pxdst.data2) + pxdst.pitch2*y);

		{for(sint32 x=0; x<auxw; x++) {
			uint16 v = *src;
			src++;

			if(v>ref) v=max_value; else v=(v*m)>>16;

			*dst = v;
			dst++;
		}}
	}}

	{for(sint32 y=0; y<auxh; y++) {
		const uint16 *src = (const uint16 *)(size_t(pxsrc.data3) + pxsrc.pitch3*y);
		uint16 *dst = (uint16 *)(size_t(pxdst.data3) + pxdst.pitch3*y);

		{for(sint32 x=0; x<auxw; x++) {
			uint16 v = *src;
			src++;

			if(v>ref) v=max_value; else v=(v*m)>>16;

			*dst = v;
			dst++;
		}}
	}}
}

void VDPixmap_bitmap_to_YUV422_Planar16(VDPixmap& dst, const VDPixmap& src, int variant) {
	dst.info.ref_r = 0xFFFF;

  if (variant==2) {
    // ffmpeg, 10 bit
  	dst.info.ref_r = 0x3FF;
  }

  if (variant==3 || variant==4) {
    // P216/P210, msb aligned
  	dst.info.ref_r = 0xFF00;
    // next blitter should deinterleave it
    dst.ext.format_swizzle = 3;
    dst.pitch2 = src.pitch;
    dst.pitch3 = 0;
    dst.data3 = 0;
  }
}
