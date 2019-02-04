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
		*dst = ClampedRoundToUInt16(*src*m + bias);
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
	if (invalid) return;

	float *dst = (float *)dst0;
	const uint16 *src = (const uint16 *)mpSrc->GetRow(y, mSrcIndex);
	uint32 w = mWidth;

	for(uint32 i=0; i<w; ++i){
		*dst = *src*m + bias;
		dst++;
		src++;
	}
}

///////////////////////////////////////////////////////////////////////////////

int VDPixmapGen_8_To_16::ComputeSpan(uint16* dst, const uint8* src, int n) {
	if (invalid) return n;
	n = n & ~15;
	if (n==0) return 0;

	__m128i zero = _mm_setzero_si128();
	{for(int i=0; i<n/16; i++){
		__m128i a = _mm_loadu_si128((__m128i*)src);
		__m128i b0 = _mm_unpacklo_epi8(zero,a);
		__m128i b1 = _mm_unpackhi_epi8(zero,a);
		_mm_storeu_si128((__m128i*)dst,b0);
		_mm_storeu_si128((__m128i*)(dst+8),b1);
		dst += 16;
		src += 16;
	}}

	return n;
}

int VDPixmapGen_A8_To_A16::ComputeSpan(uint16* dst, const uint8* src, int n) {
	if (invalid) return n;
	n = n & ~15;
	if (n==0) return 0;

	__m128i zero = _mm_setzero_si128();
	{for(int i=0; i<n/16; i++){
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

	return n;
}

///////////////////////////////////////////////////////////////////////////////

void VDPixmapGen_16_To_8::Start() {
	StartWindow(mWidth);
}

uint32 VDPixmapGen_16_To_8::GetType(uint32 output) const {
	return (mpSrc->GetType(mSrcIndex) & ~kVDPixType_Mask) | kVDPixType_8;
}

void VDPixmapGen_16_To_8::TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
	FilterModPixmapInfo buf;
	mpSrc->TransformPixmapInfo(src,buf);
	dst.copy_frame(buf);
	ref = buf.ref_r;
	m = (0xFF*0x20000/ref+1)/2;
	// not using chroma bias: too small
	invalid = false;
}

void VDPixmapGen_A16_To_A8::TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
	FilterModPixmapInfo buf;
	mpSrc->TransformPixmapInfo(src,buf);
	dst.copy_alpha(buf);
	ref = buf.ref_a;
	invalid = !dst.alpha_type;
	if (!invalid) m = (0xFF*0x20000/ref+1)/2;
}

int VDPixmapGen_16_To_8::ComputeSpan(uint8* dst, const uint16* src, int n) {
	if (invalid) return n;
	n = n & ~15;
	if (n==0) return 0;

	__m128i mm = _mm_set1_epi16(m);

	for(sint32 i=0; i<n/16; ++i) {
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

	return n;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void VDPixmapGen_Y16_Normalize::TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
	mpSrc->TransformPixmapInfo(src,dst);
	int mask = vd2::depth_mask(max_value);
	s = 0xFFFF-mask;
	if (dst.ref_r==max_value) {
		do_normalize = false;
		ref = dst.ref_r;
		m = 1;
		bias = 0;
	} else {
		do_normalize = true;
		ref = dst.ref_r;
		m = (uint64(max_value)*0x20000/ref+1)/2;
		int ext = mask*0x10000/m;
		if (ext>0xFFFF) ext = 0xFFFF;
		while (ext<0xFFFF && ((uint64(ext+1)*m+0x8000)>>16)<=mask) ext++;
		s = 0xFFFF-ext;
		if (isChroma) {
			int n1 = vd2::chroma_neutral(max_value);
			int n0 = vd2::chroma_neutral(ref);
			bias = n1 - ((n0*m+0x8000)>>16);
		} else {
			bias = 0;
		}
		dst.ref_r = max_value;
	}
}

void VDPixmapGen_A16_Normalize::TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
	mpSrc->TransformPixmapInfo(src,dst);

	a_mask = 0;
	if(dst.alpha_type==FilterModPixmapInfo::kAlphaInvalid){
		a_mask = max_value;
		do_normalize = false;
		ref = 0;
		m = 1;
		bias = 0;
	} else {
		if (dst.ref_a==max_value) {
			do_normalize = false;
			ref = dst.ref_a;
		} else {
			do_normalize = true;
			ref = dst.ref_a;
			m = (uint64(max_value)*0x20000/ref+1)/2;
			dst.ref_a = max_value;
			bias = 0;
		}
	}
	s = 0xFFFF-ref;
}

int VDPixmapGen_Y16_Normalize::ComputeSpan(uint16* dst, const uint16* src, int n) {
	if (!do_normalize && round==1) {
		memcpy(dst,src,n*2);
		return n;
	}
	n = n & ~7;
	if (n==0) return 0;
	if (do_normalize && (bias!=0 || round>1))
		ComputeNormalizeBias(dst,src,n);
	else if (do_normalize)
		ComputeNormalize(dst,src,n);
	else
		ComputeMask(dst,src,n);
	return n;
}

int VDPixmapGen_A16_Normalize::ComputeSpan(uint16* dst, const uint16* src, int n) {
	if (!a_mask) {
		return VDPixmapGen_Y16_Normalize::ComputeSpan(dst,src,n);
	}

	n = n & ~7;
	if (n==0) return 0;
	ComputeWipeAlpha(dst, n);
	return n;
}

void VDPixmapGen_A8_Normalize::Compute(void *dst0, sint32 y) {
	if (a_mask)
		ComputeWipeAlpha(dst0,y);
	else
		memcpy(dst0,mpSrc->GetRow(y, mSrcIndex),mWidth);
}

void VDPixmapGen_A16_Normalize::ComputeWipeAlpha(uint16* dst, int n) {
	__m128i cmask = _mm_set1_epi16(a_mask);
	{for(int x=0; x<n/8; x++) {
		_mm_storeu_si128((__m128i*)dst,cmask);
		dst+=8;
	}}
}

void VDPixmapGen_A8_Normalize::ComputeWipeAlpha(void *dst0, sint32 y) {
	uint8 *dst = (uint8 *)dst0;

	int w = mWidth;
	int w0 = w & ~15;
	w -= w0;

	__m128i cmask = _mm_set1_epi8(a_mask);
	{for(int x=0; x<w0/16; x++) {
		_mm_storeu_si128((__m128i*)dst,cmask);
		dst+=16;
	}}

	{for(int x=0; x<w; x++) {
		*dst = a_mask;
		dst++;
	}}
}

void VDPixmapGen_Y16_Normalize::ComputeMask(uint16* dst, const uint16* src, int n) {
	__m128i rn = _mm_set1_epi16(round/2);
	__m128i sat = _mm_set1_epi16(s);
	__m128i rmask = _mm_set1_epi16(-round);
	{for(int x=0; x<n/8; x++) {
		__m128i c = _mm_loadu_si128((const __m128i*)src);
		c = _mm_adds_epu16(c,rn);
		c = _mm_adds_epu16(c,sat);
		c = _mm_sub_epi16(c,sat);
		c = _mm_and_si128(c,rmask);
		_mm_storeu_si128((__m128i*)dst,c);
		src+=8;
		dst+=8;
	}}
}

void VDPixmapGen_Y16_Normalize::ComputeNormalize(uint16* dst, const uint16* src, int n) {
	__m128i sat = _mm_set1_epi16(s);
	__m128i mm = _mm_set1_epi16(m);

	if(m<0x10000){
		{for(int x=0; x<n/8; x++) {
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
	} else {
		__m128i mmh = _mm_set1_epi16(m >> 16);
		{for(int x=0; x<n/8; x++) {
			__m128i c = _mm_loadu_si128((const __m128i*)src);
			c = _mm_adds_epu16(c,sat);
			c = _mm_sub_epi16(c,sat);
			__m128i a = _mm_mullo_epi16(c,mm);
			a = _mm_srli_epi16(a,15);
			__m128i b = _mm_mullo_epi16(c,mmh);
			c = _mm_mulhi_epu16(c,mm);
			c = _mm_adds_epu16(c,a);
			c = _mm_adds_epu16(c,b);
			_mm_storeu_si128((__m128i*)dst,c);
			src+=8;
			dst+=8;
		}}
	}
}

void VDPixmapGen_Y16_Normalize::ComputeNormalizeBias(uint16* dst, const uint16* src, int n) {
	// round is actually used only with 0xFF00 range - safely saturates at 0xFFFF
	// bias does not produce out of range values (test_normalize_bias)
	__m128i sat = _mm_set1_epi16(s);
	__m128i mm = _mm_set1_epi16(m);
	int br = bias + round/2;
	__m128i bias1 = _mm_set1_epi16(br>0 ? +br:0);
	__m128i bias2 = _mm_set1_epi16(br<0 ? -br:0);

	if(round>1){
		__m128i rmask = _mm_set1_epi16(-round);

		if(m<0x10000){
			{for(int x=0; x<n/8; x++) {
				__m128i c = _mm_loadu_si128((const __m128i*)src);
				c = _mm_adds_epu16(c,sat);
				c = _mm_sub_epi16(c,sat);
				c = _mm_mulhi_epu16(c,mm);
				c = _mm_adds_epu16(c,bias1);
				c = _mm_subs_epu16(c,bias2);
				c = _mm_and_si128(c,rmask);
				_mm_storeu_si128((__m128i*)dst,c);
				src+=8;
				dst+=8;
			}}
		} else {
			__m128i mmh = _mm_set1_epi16(m >> 16);
			{for(int x=0; x<n/8; x++) {
				__m128i c = _mm_loadu_si128((const __m128i*)src);
				c = _mm_adds_epu16(c,sat);
				c = _mm_sub_epi16(c,sat);
				__m128i b = _mm_mullo_epi16(c,mmh);
				c = _mm_mulhi_epu16(c,mm);
				c = _mm_adds_epu16(c,b);
				c = _mm_adds_epu16(c,bias1);
				c = _mm_subs_epu16(c,bias2);
				c = _mm_and_si128(c,rmask);
				_mm_storeu_si128((__m128i*)dst,c);
				src+=8;
				dst+=8;
			}}
		}
	} else {
		if(m<0x10000){
			{for(int x=0; x<n/8; x++) {
				__m128i c = _mm_loadu_si128((const __m128i*)src);
				c = _mm_adds_epu16(c,sat);
				c = _mm_sub_epi16(c,sat);
				__m128i a = _mm_mullo_epi16(c,mm);
				a = _mm_srli_epi16(a,15);
				c = _mm_mulhi_epu16(c,mm);
				c = _mm_adds_epu16(c,a);
				c = _mm_adds_epu16(c,bias1);
				c = _mm_subs_epu16(c,bias2);
				_mm_storeu_si128((__m128i*)dst,c);
				src+=8;
				dst+=8;
			}}
		} else {
			__m128i mmh = _mm_set1_epi16(m >> 16);
			{for(int x=0; x<n/8; x++) {
				__m128i c = _mm_loadu_si128((const __m128i*)src);
				c = _mm_adds_epu16(c,sat);
				c = _mm_sub_epi16(c,sat);
				__m128i a = _mm_mullo_epi16(c,mm);
				a = _mm_srli_epi16(a,15);
				__m128i b = _mm_mullo_epi16(c,mmh);
				c = _mm_mulhi_epu16(c,mm);
				c = _mm_adds_epu16(c,a);
				c = _mm_adds_epu16(c,b);
				c = _mm_adds_epu16(c,bias1);
				c = _mm_subs_epu16(c,bias2);
				_mm_storeu_si128((__m128i*)dst,c);
				src+=8;
				dst+=8;
			}}
		}
	}
}

void ExtraGen_YUV_Normalize::Create(VDPixmapUberBlitterGenerator& gen, const VDPixmapLayout& dst) {
	//               0,  1,  2,  3
	// YUVA inputs:  Cr, Y,  Cb, A
	// YUV inputs:   Cr, Y,  Cb
	// YC inputs:    Y,  C (P210)
	// Y16 input:    Y
	if (dst.pitch4) {
		VDPixmapGen_A16_Normalize* normalize3 = new VDPixmapGen_A16_Normalize;
		normalize3->max_value = max_a_value;
		gen.swap(normalize3,3);
	}
	if (dst.pitch3) {
		VDPixmapGen_Y16_Normalize* normalize2 = new VDPixmapGen_Y16_Normalize(true);
		normalize2->max_value = max_value; normalize2->round = round;
		gen.swap(normalize2,2);

		VDPixmapGen_Y16_Normalize* normalize0 = new VDPixmapGen_Y16_Normalize;
		normalize0->max_value = max_value; normalize0->round = round;
		gen.swap(normalize0,1);

		VDPixmapGen_Y16_Normalize* normalize1 = new VDPixmapGen_Y16_Normalize(true);
		normalize1->max_value = max_value; normalize1->round = round;
		gen.swap(normalize1,0);

	} else if (dst.pitch2) {
		VDPixmapGen_Y16_Normalize* normalize1 = new VDPixmapGen_Y16_Normalize(true);
		normalize1->max_value = max_value; normalize1->round = round;
		gen.swap(normalize1,1);

		VDPixmapGen_Y16_Normalize* normalize0 = new VDPixmapGen_Y16_Normalize;
		normalize0->max_value = max_value; normalize0->round = round;
		gen.swap(normalize0,0);

	} else {
		VDPixmapGen_Y16_Normalize* normalize0 = new VDPixmapGen_Y16_Normalize;
		normalize0->max_value = max_value; normalize0->round = round;
		gen.swap(normalize0,0);
	}
}

void ExtraGen_RGB_Normalize::Create(VDPixmapUberBlitterGenerator& gen, const VDPixmapLayout& dst) {
	if (dst.pitch4) {
		VDPixmapGen_A16_Normalize* normalize3 = new VDPixmapGen_A16_Normalize;
		normalize3->max_value = max_a_value;
		gen.swap(normalize3,3);
	}
	if (dst.pitch3) {
		VDPixmapGen_Y16_Normalize* normalize2 = new VDPixmapGen_Y16_Normalize;
		VDPixmapGen_Y16_Normalize* normalize1 = new VDPixmapGen_Y16_Normalize;
		VDPixmapGen_Y16_Normalize* normalize0 = new VDPixmapGen_Y16_Normalize;
		normalize2->max_value = max_value;
		normalize1->max_value = max_value;
		normalize0->max_value = max_value;
		gen.swap(normalize2,2);
		gen.swap(normalize1,1);
		gen.swap(normalize0,0);
	}
}

void ExtraGen_A8_Normalize::Create(VDPixmapUberBlitterGenerator& gen, const VDPixmapLayout& dst) {
	if (dst.pitch4) {
		VDPixmapGen_A8_Normalize* normalize = new VDPixmapGen_A8_Normalize;
		gen.swap(normalize,3);
	}
}

/*
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
*/
