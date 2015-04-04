#include <stdafx.h>
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
		src += 4;

		dst[0] = r*mr;
		dst[1] = g*mg;
		dst[2] = b*mb;
		dst[3] = 1.0f;
		dst += 4;
	}
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
		src += 4;

		dst[2] = uint16(r*mr);
		dst[1] = uint16(g*mg);
		dst[0] = uint16(b*mb);
		dst[3] = 0;
		dst += 4;
	}
}

void VDPixmapGen_X16R16G16B16_To_X8R8G8B8::Compute(void *dst0, sint32 y) {
	VDCPUCleanupExtensions();

	const void* src0 = mpSrc->GetRow(y, mSrcIndex);

	if((mWidth & 1)==0 && (size_t(dst0) & 0xF)==0) {
		__m128i *dst = (__m128i *)dst0;
		const __m128i *src = (const __m128i *)src0;
		sint32 w = mWidth/4;

		if(unorm_mode){
			__m128i mm = _mm_set_epi16(0,mr,mg,mb,0,mr,mg,mb);

			for(sint32 i=0; i<w; ++i) {
				__m128i c0 = _mm_load_si128(src);
				__m128i c1 = _mm_load_si128(src+1);
				c0 = _mm_mulhi_epu16(c0,mm);
				c1 = _mm_mulhi_epu16(c1,mm);
				__m128i v = _mm_packus_epi16(c0,c1);
				_mm_store_si128(dst,v);
				src+=2;
				dst++;
			}

		} else {

			for(sint32 i=0; i<w; ++i) {
				__m128i c0 = _mm_load_si128(src);
				__m128i c1 = _mm_load_si128(src+1);
				c0 = _mm_srli_epi16(c0,8);
				c1 = _mm_srli_epi16(c1,8);
				__m128i v = _mm_packus_epi16(c0,c1);
				_mm_store_si128(dst,v);
				src+=2;
				dst++;
			}
		}

	} else {

		uint32 *dst = (uint32 *)dst0;
		const uint16 *src = (const uint16 *)src0;
		sint32 w = mWidth;

		if(unorm_mode){
			for(sint32 i=0; i<w; ++i) {
				uint16 r = src[2];
				uint16 g = src[1];
				uint16 b = src[0];
				uint16 a = src[3];
				src += 4;

				if(r>ref_r) r=255; else r=(r*mr)>>16;
				if(g>ref_g) g=255; else g=(g*mg)>>16;
				if(b>ref_b) b=255; else b=(b*mb)>>16;
				if(a>ref_a) a=255; else a=(a*ma)>>16;

				uint32 ir = r << 16;
				uint32 ig = g << 8;
				uint32 ib = b;
				uint32 ia = a << 24;

				dst[i] = ir + ig + ib + ia;
			}
		} else {
			for(sint32 i=0; i<w; ++i) {
				uint16 r = src[2];
				uint16 g = src[1];
				uint16 b = src[0];
				uint16 a = src[3];
				src += 4;

				uint32 ir = (r>>8) << 16;
				uint32 ig = (g>>8) << 8;
				uint32 ib = (b>>8);
				uint32 ia = a << 24;

				dst[i] = ir + ig + ib + ia;
			}
		}
	}
}

void VDPixmap_X16R16G16B16_Normalize(VDPixmap& pxdst, const VDPixmap& pxsrc) {
	int ref_r = pxsrc.info.ref_r;
	int ref_g = pxsrc.info.ref_g;
	int ref_b = pxsrc.info.ref_b;
	int ref_a = pxsrc.info.ref_a;
	uint32 mr = 0xFFFF0000/ref_r;
	uint32 mg = 0xFFFF0000/ref_g;
	uint32 mb = 0xFFFF0000/ref_b;
	uint32 ma = 0xFFFF0000/ref_a;
	pxdst.info = pxsrc.info;
	pxdst.info.ref_r = 0xFFFF;
	pxdst.info.ref_g = 0xFFFF;
	pxdst.info.ref_b = 0xFFFF;
	pxdst.info.ref_a = 0xFFFF;

	{for(sint32 y=0; y<pxsrc.h; y++) {
		const uint16 *src = (const uint16 *)(size_t(pxsrc.data) + pxsrc.pitch*y);
		uint16 *dst = (uint16 *)(size_t(pxdst.data) + pxdst.pitch*y);

		{for(sint32 x=0; x<pxsrc.w; x++) {
			uint16 r = src[2];
			uint16 g = src[1];
			uint16 b = src[0];
			uint16 a = src[3];
			src += 4;

			if(r>ref_r) r=0xFFFF; else r=(r*mr)>>16;
			if(g>ref_g) g=0xFFFF; else g=(g*mg)>>16;
			if(b>ref_b) b=0xFFFF; else b=(b*mb)>>16;
			if(a>ref_a) a=0xFFFF; else a=(a*ma)>>16;

			dst[2] = r;
			dst[1] = g;
			dst[0] = b;
			dst[3] = a;
			dst += 4;
		}}
	}}
}
