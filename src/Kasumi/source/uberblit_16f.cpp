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
#include "uberblit_gen.h"
#include <emmintrin.h>

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

void VDPixmapGen_8_To_16::Compute(void *dst0, sint32 y) {
	uint16 *dst = (uint16 *)dst0;
	const uint8 *src = (const uint8 *)mpSrc->GetRow(y, mSrcIndex);
	int w = mWidth;
	int w0 = mWidth & ~15;
	w -= w0;

	__m128i zero = _mm_setzero_si128();
	{for(int i=0; i<w0/16; i++){
		__m128i a = _mm_loadu_si128((__m128i*)src);
		__m128i b0 = _mm_unpacklo_epi8(a,zero);
		__m128i b1 = _mm_unpackhi_epi8(a,zero);
		b0 = _mm_or_si128(b0,_mm_slli_si128(b0,1));
		b1 = _mm_or_si128(b1,_mm_slli_si128(b1,1));
		_mm_storeu_si128((__m128i*)dst,b0);
		_mm_storeu_si128((__m128i*)(dst+8),b1);
		dst += 16;
		src += 16;
	}}

	{for(int i=0; i<w; i++){
		dst[0] = (src[0]<<8) + src[0];
		dst++;
		src++;
	}}
}

void VDPixmapGen_16_To_8::Start() {
	StartWindow(mWidth);
}

uint32 VDPixmapGen_16_To_8::GetType(uint32 output) const {
	return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_8;
}

void VDPixmapGen_16_To_8::Compute(void *dst0, sint32 y) {
	const uint16 *src = (const uint16 *)mpSrc->GetRow(y, mSrcIndex);
	uint8 *dst = (uint8 *)dst0;

	const int n0 = mWidth & ~15;
	const int n1 = mWidth-n0;

	__m128i mm = _mm_set1_epi16(m);

	for(sint32 i=0; i<n0/16; ++i) {
		__m128i c0 = _mm_loadu_si128((const __m128i*)src);
		__m128i c1 = _mm_loadu_si128((const __m128i*)(src+8));
		__m128i a0 = _mm_mullo_epi16(c0,mm);
		__m128i a1 = _mm_mullo_epi16(c1,mm);
		a0 = _mm_srli_epi16(a0,15);
		a1 = _mm_srli_epi16(a1,15);
		c0 = _mm_mulhi_epu16(c0,mm);
		c1 = _mm_mulhi_epu16(c1,mm);
		c0 = _mm_adds_epu16(c0,a0);
		c1 = _mm_adds_epu16(c1,a1);
		__m128i v = _mm_packus_epi16(c0,c1);
		_mm_storeu_si128((__m128i*)dst,v);
		src+=16;
		dst+=16;
	}

	for(sint32 i=0; i<n1; ++i) {
		uint16 c = src[0];
		if(c>ref) c=255; else c=(c*m+0x8000)>>16;
		*dst = (uint8)c;
		src++;
		dst++;
	}
}

void VDPixmapGen_Y16_Normalize::Compute(void *dst0, sint32 y) {
	if (do_normalize)
		ComputeNormalize(dst0,y);
	else
		memcpy(dst0,mpSrc->GetRow(y, mSrcIndex),mWidth*2);
}

void VDPixmapGen_Y16_Normalize::ComputeNormalize(void *dst0, sint32 y) {
	uint16 *dst = (uint16 *)dst0;
	const uint16 *src = (const uint16 *)mpSrc->GetRow(y, mSrcIndex);

	int w = mWidth;
	int w0 = mWidth & ~7;
	if(!scale_down) w0 = 0;
	w -= w0;

	uint16 s = 0x10000-ref;
	__m128i sat = _mm_set1_epi16(s);
	__m128i mm = _mm_set1_epi16(m);
	{for(int x=0; x<w0/8; x++) {
		__m128i c = _mm_loadu_si128((const __m128i*)src);
		c = _mm_adds_epu16(c,sat);
		c = _mm_sub_epi16(c,sat);
		__m128i a = _mm_mullo_epi16(c,mm);
		a = _mm_srli_epi16(a,15);
		c = _mm_mulhi_epu16(c,mm);
		c = _mm_adds_epu16(c,a);
		_mm_storeu_si128((__m128i*)dst,c);
		src+=8;
		dst+=8;
	}}

	{for(int x=0; x<w; x++) {
		uint16 v = *src;
		src++;

		if(v>ref) v=max_value; else v=(v*m)>>16;

		*dst = v;
		dst++;
	}}
}

void ExtraGen_YUV_Normalize::Create(VDPixmapUberBlitterGenerator& gen, const VDPixmapLayout& dst) {
	VDPixmapGen_Y16_Normalize* normalize0 = new VDPixmapGen_Y16_Normalize;
	VDPixmapGen_Y16_Normalize* normalize1 = new VDPixmapGen_Y16_Normalize;
	VDPixmapGen_Y16_Normalize* normalize2 = new VDPixmapGen_Y16_Normalize;
	normalize0->max_value = max_value;
	normalize1->max_value = max_value;
	normalize2->max_value = max_value;
	gen.addToEnd(normalize0,2);
	gen.addToEnd(normalize1,1);
	gen.addToEnd(normalize2,0);
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

void VDPixmap_bitmap_to_XYUV64(VDPixmap& dst, const VDPixmap& src, int variant) {
	// Y416, msb aligned
	dst.info.ref_r = 0xFF00;
	dst.ext.format_swizzle = 1;
}
