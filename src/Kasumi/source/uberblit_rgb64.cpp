#include <stdafx.h>
#include <vd2/system/math.h>
#include "uberblit_rgb64.h"
#include <emmintrin.h>

void VDPixmapGen_X8R8G8B8_To_X16R16G16B16::Compute(void *dst0, sint32 y) {
	uint16 *dst = (uint16 *)dst0;
	const uint8 *src = (const uint8 *)mpSrc->GetRow(y, mSrcIndex);
	sint32 w = mWidth;

	VDCPUCleanupExtensions();

	for(sint32 i=0; i<w; ++i) {
		dst[0] = src[0]<<8;
		dst[1] = src[1]<<8;
		dst[2] = src[2]<<8;
		dst[3] = src[3]<<8;
		dst += 4;
		src += 4;
	}
}

void VDPixmapGen_X16R16G16B16_To_X32B32G32R32F::Compute(void *dst0, sint32 y) {
	VDCPUCleanupExtensions();

	const void* src0 = mpSrc->GetRow(y, mSrcIndex);

	float *dst = (float *)dst0;
	const uint16 *src = (const uint16 *)src0;
	sint32 w = mWidth;

	for(sint32 i=0; i<w; ++i) {
		uint16 r = src[2];
		uint16 g = src[1];
		uint16 b = src[0];
		uint16 a = src[3];
		src += 4;

		dst[0] = r*mr;
		dst[1] = g*mg;
		dst[2] = b*mb;
		dst[3] = a*ma;
		dst += 4;
	}
}

inline uint16 ClampedRoundToUInt16(float x) {
	int v = VDRoundToIntFast(x);
	if(v<0) v = 0;
	if(v>0xFFFF) v = 0xFFFF;
	return v;
}

void VDPixmapGen_X32B32G32R32F_To_X16R16G16B16::Compute(void *dst0, sint32 y) {
	VDCPUCleanupExtensions();

	const void* src0 = mpSrc->GetRow(y, mSrcIndex);

	uint16 *dst = (uint16 *)dst0;
	const float *src = (const float *)src0;
	sint32 w = mWidth;

	for(sint32 i=0; i<w; ++i) {
		float r = src[0];
		float g = src[1];
		float b = src[2];
		float a = src[3];
		src += 4;

		dst[2] = ClampedRoundToUInt16(r*mr);
		dst[1] = ClampedRoundToUInt16(g*mg);
		dst[0] = ClampedRoundToUInt16(b*mb);
		dst[3] = ClampedRoundToUInt16(a*ma);
		dst += 4;
	}
}

void VDPixmapGen_X16R16G16B16_To_X8R8G8B8::Compute(void *dst0, sint32 y) {
	VDCPUCleanupExtensions();

	const uint8* src = (const uint8*)mpSrc->GetRow(y, mSrcIndex);
	uint8 *dst = (uint8 *)dst0;

	const int n2 = ((16-size_t(dst)) & 0xF)/4;
	const int n0 = (mWidth-n2)/4;
	const int n1 = mWidth-n2-n0*4;

	for(sint32 i=0; i<n2; ++i) {
		uint16 r = ((uint16*)src)[2];
		uint16 g = ((uint16*)src)[1];
		uint16 b = ((uint16*)src)[0];
		uint16 a = ((uint16*)src)[3];
		src += 8;

		if(r>ref_r) r=255; else r=(r*mr+0x8000)>>16;
		if(g>ref_g) g=255; else g=(g*mg+0x8000)>>16;
		if(b>ref_b) b=255; else b=(b*mb+0x8000)>>16;
		if(a>ref_a) a=255; else a=(a*ma+0x8000)>>16;

		uint32 ir = r << 16;
		uint32 ig = g << 8;
		uint32 ib = b;
		uint32 ia = a << 24;

		*(uint32*)dst = ir + ig + ib + ia;
		dst+=4;
	}

	__m128i mm = _mm_set_epi16(ma,mr,mg,mb,ma,mr,mg,mb);

	for(sint32 i=0; i<n0; ++i) {
		__m128i c0 = _mm_loadu_si128((const __m128i*)src);
		__m128i c1 = _mm_loadu_si128((const __m128i*)(src+16));
		__m128i a0 = _mm_mullo_epi16(c0,mm);
		__m128i a1 = _mm_mullo_epi16(c1,mm);
		a0 = _mm_srli_epi16(a0,15);
		a1 = _mm_srli_epi16(a1,15);
		c0 = _mm_mulhi_epu16(c0,mm);
		c1 = _mm_mulhi_epu16(c1,mm);
		c0 = _mm_adds_epu16(c0,a0);
		c1 = _mm_adds_epu16(c1,a1);
		__m128i v = _mm_packus_epi16(c0,c1);
		_mm_store_si128((__m128i*)dst,v);
		src+=32;
		dst+=16;
	}

	for(sint32 i=0; i<n1; ++i) {
		uint16 r = ((uint16*)src)[2];
		uint16 g = ((uint16*)src)[1];
		uint16 b = ((uint16*)src)[0];
		uint16 a = ((uint16*)src)[3];
		src += 8;

		if(r>ref_r) r=255; else r=(r*mr+0x8000)>>16;
		if(g>ref_g) g=255; else g=(g*mg+0x8000)>>16;
		if(b>ref_b) b=255; else b=(b*mb+0x8000)>>16;
		if(a>ref_a) a=255; else a=(a*ma+0x8000)>>16;

		uint32 ir = r << 16;
		uint32 ig = g << 8;
		uint32 ib = b;
		uint32 ia = a << 24;

		*(uint32*)dst = ir + ig + ib + ia;
		dst+=4;
	}
}

void VDPixmapGen_X16R16G16B16_Normalize::Compute(void *dst0, sint32 y) {
  if (do_normalize)
    ComputeAll(dst0,y);
  else if(a_mask)
    ComputeWipeAlpha(dst0,y);
  else
    mpSrc->ProcessRow(dst0,y);
}

void VDPixmapGen_X16R16G16B16_Normalize::ComputeAll(void *dst0, sint32 y) {
	const uint16* src = (const uint16*)mpSrc->GetRow(y, mSrcIndex);
	uint16* dst = (uint16*)dst0;

	const int n2 = ((16-size_t(dst)) & 0xF)/8;
	const int n0 = (mWidth-n2)/2;
	const int n1 = mWidth-n2-n0*2;

	{for(sint32 x=0; x<n2; x++) {
		uint16 r = src[2];
		uint16 g = src[1];
		uint16 b = src[0];
		uint16 a = src[3];
		src += 4;

		if(r>ref_r) r=max_value; else r=(r*mr+0x8000)>>16;
		if(g>ref_g) g=max_value; else g=(g*mg+0x8000)>>16;
		if(b>ref_b) b=max_value; else b=(b*mb+0x8000)>>16;
		if(a>ref_a) a=max_value; else a=(a*ma+0x8000)>>16;

		dst[2] = r;
		dst[1] = g;
		dst[0] = b;
		dst[3] = a | a_mask;
		dst += 4;
	}}

	uint16 sr = 0x10000-ref_r;
	uint16 sg = 0x10000-ref_g;
	uint16 sb = 0x10000-ref_b;
	uint16 sa = 0x10000-ref_a;
	__m128i sat = _mm_set_epi16(sa,sr,sg,sb,sa,sr,sg,sb);
	__m128i mm = _mm_set_epi16(ma,mr,mg,mb,ma,mr,mg,mb);
	__m128i mask = _mm_set_epi16(a_mask,0,0,0,a_mask,0,0,0);
	{for(sint32 x=0; x<n0; x++) {
		__m128i c = _mm_loadu_si128((const __m128i*)src);
		c = _mm_adds_epu16(c,sat);
		c = _mm_sub_epi16(c,sat);
		__m128i a = _mm_mullo_epi16(c,mm);
		a = _mm_srli_epi16(a,15);
		c = _mm_mulhi_epu16(c,mm);
		c = _mm_adds_epu16(c,a);
		c = _mm_or_si128(c,mask);
		_mm_store_si128((__m128i*)dst,c);
		src+=8;
		dst+=8;
	}}

	{for(sint32 x=0; x<n1; x++) {
		uint16 r = src[2];
		uint16 g = src[1];
		uint16 b = src[0];
		uint16 a = src[3];
		src += 4;

		if(r>ref_r) r=max_value; else r=(r*mr+0x8000)>>16;
		if(g>ref_g) g=max_value; else g=(g*mg+0x8000)>>16;
		if(b>ref_b) b=max_value; else b=(b*mb+0x8000)>>16;
		if(a>ref_a) a=max_value; else a=(a*ma+0x8000)>>16;

		dst[2] = r;
		dst[1] = g;
		dst[0] = b;
		dst[3] = a | a_mask;
		dst += 4;
	}}
}

void VDPixmapGen_X16R16G16B16_Normalize::ComputeWipeAlpha(void *dst0, sint32 y) {
	const uint16* src = (const uint16*)mpSrc->GetRow(y, mSrcIndex);
	uint16* dst = (uint16*)dst0;

	const int n2 = ((16-size_t(dst)) & 0xF)/8;
	const int n0 = (mWidth-n2)/2;
	const int n1 = mWidth-n2-n0*2;

	{for(sint32 x=0; x<n2; x++) {
		dst[2] = src[2];
		dst[1] = src[1];
		dst[0] = src[0];
		uint16 a = src[3];
		dst[3] = a | a_mask;
		src += 4;
		dst += 4;
	}}

	__m128i mask = _mm_set_epi16(a_mask,0,0,0,a_mask,0,0,0);
	{for(sint32 x=0; x<n0; x++) {
		__m128i c = _mm_loadu_si128((const __m128i*)src);
		c = _mm_or_si128(c,mask);
		_mm_store_si128((__m128i*)dst,c);
		src+=8;
		dst+=8;
	}}

	{for(sint32 x=0; x<n1; x++) {
		dst[2] = src[2];
		dst[1] = src[1];
		dst[0] = src[0];
		uint16 a = src[3];
		dst[3] = a | a_mask;
		src += 4;
		dst += 4;
	}}
}

void VDPixmapGen_X8R8G8B8_Normalize::Compute(void *dst0, sint32 y) {
	if (a_mask)
		ComputeWipeAlpha(dst0,y);
	else
		mpSrc->ProcessRow(dst0,y);
}

void VDPixmapGen_X8R8G8B8_Normalize::ComputeWipeAlpha(void *dst0, sint32 y) {
	const uint32* src = (const uint32*)mpSrc->GetRow(y, mSrcIndex);
	uint32* dst = (uint32*)dst0;

	const int n2 = ((16-size_t(dst)) & 0xF)/4;
	const int n0 = (mWidth-n2)/4;
	const int n1 = mWidth-n2-n0*4;

	{for(sint32 x=0; x<n2; x++) {
		uint32 c = src[0];
		dst[0] = c | a_mask;
		src++;
		dst++;
	}}

	__m128i mask = _mm_set1_epi32(a_mask);
	{for(sint32 x=0; x<n0; x++) {
		__m128i c = _mm_loadu_si128((const __m128i*)src);
		c = _mm_or_si128(c,mask);
		_mm_store_si128((__m128i*)dst,c);
		src+=4;
		dst+=4;
	}}

	{for(sint32 x=0; x<n1; x++) {
		uint32 c = src[0];
		dst[0] = c | a_mask;
		src++;
		dst++;
	}}
}

void VDPixmap_X16R16G16B16_Normalize(VDPixmap& pxdst, const VDPixmap& pxsrc, uint32 max_value) {
	int ref_r = pxsrc.info.ref_r;
	int ref_g = pxsrc.info.ref_g;
	int ref_b = pxsrc.info.ref_b;
	int ref_a = pxsrc.info.ref_a;
	uint32 mr = max_value*0x10000/ref_r;
	uint32 mg = max_value*0x10000/ref_g;
	uint32 mb = max_value*0x10000/ref_b;
	uint32 ma = max_value*0x10000/ref_a;
	pxdst.info = pxsrc.info;
	pxdst.info.ref_r = max_value;
	pxdst.info.ref_g = max_value;
	pxdst.info.ref_b = max_value;
	pxdst.info.ref_a = max_value;

	{for(sint32 y=0; y<pxsrc.h; y++) {
		const uint16 *src = (const uint16 *)(size_t(pxsrc.data) + pxsrc.pitch*y);
		uint16 *dst = (uint16 *)(size_t(pxdst.data) + pxdst.pitch*y);

		{for(sint32 x=0; x<pxsrc.w; x++) {
			uint16 r = src[2];
			uint16 g = src[1];
			uint16 b = src[0];
			uint16 a = src[3];
			src += 4;

			if(r>ref_r) r=max_value; else r=(r*mr+0x8000)>>16;
			if(g>ref_g) g=max_value; else g=(g*mg+0x8000)>>16;
			if(b>ref_b) b=max_value; else b=(b*mb+0x8000)>>16;
			if(a>ref_a) a=max_value; else a=(a*ma+0x8000)>>16;

			dst[2] = r;
			dst[1] = g;
			dst[0] = b;
			dst[3] = a;
			dst += 4;
		}}
	}}
}

void VDPixmap_X16R16G16B16_to_b64a(VDPixmap& dst, const VDPixmap& src) {
	int w = (src.w+1)/2;
	int m = 0;
	if(src.info.alpha_type==FilterModPixmapInfo::kAlphaInvalid)
		m = -1;
	__m128i a_mask = _mm_set_epi16(m,0,0,0,m,0,0,0);
	{for(int y=0; y<src.h; y++){
		uint8* s = (uint8*)src.data + y*src.pitch;
		uint8* d = (uint8*)dst.data + y*dst.pitch;
		{for(int x=0; x<w; x++){
			__m128i c = _mm_load_si128((__m128i*)s);
			c = _mm_or_si128(c,a_mask);
			__m128i c0 = _mm_srli_epi16(c,8);
			__m128i c1 = _mm_slli_epi16(c,8);
			c = _mm_or_si128(c0,c1);
			c = _mm_shufflehi_epi16(_mm_shufflelo_epi16(c,0x1B),0x1B); // 0 1 2 3
			_mm_store_si128((__m128i*)d,c);
			s += 16;
			d += 16;
		}}
	}}
}

void VDPixmap_b64a_to_X16R16G16B16(VDPixmap& dst, const VDPixmap& src) {
	int w = (src.w+1)/2;
	{for(int y=0; y<src.h; y++){
		uint8* s = (uint8*)src.data + y*src.pitch;
		uint8* d = (uint8*)dst.data + y*dst.pitch;
		{for(int x=0; x<w; x++){
			__m128i c = _mm_load_si128((__m128i*)s);
			__m128i c0 = _mm_srli_epi16(c,8);
			__m128i c1 = _mm_slli_epi16(c,8);
			c = _mm_or_si128(c0,c1);
			c = _mm_shufflehi_epi16(_mm_shufflelo_epi16(c,0x1B),0x1B); // 0 1 2 3
			_mm_store_si128((__m128i*)d,c);
			s += 16;
			d += 16;
		}}
	}}

	dst.info.ref_r = 0xFFFF;
	dst.info.ref_g = 0xFFFF;
	dst.info.ref_b = 0xFFFF;
	dst.info.ref_a = 0xFFFF;
}
