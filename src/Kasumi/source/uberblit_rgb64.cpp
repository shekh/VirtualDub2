#include <stdafx.h>
#include <vd2/system/math.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "uberblit_rgb64.h"
#include "uberblit_gen.h"
#include "uberblit_input.h"
#include <emmintrin.h>

void VDPixmapGen_X8R8G8B8_To_X16R16G16B16::Compute(void *dst0, sint32 y) {
	uint16 *dst = (uint16 *)dst0;
	const uint8 *src = (const uint8 *)mpSrc->GetRow(y, mSrcIndex);
	int w = mWidth;
	int w0 = mWidth & ~3;
	w -= w0;

	__m128i zero = _mm_setzero_si128();
	{for(int i=0; i<w0/4; i++){
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
		dst[1] = (src[1]<<8) + src[1];
		dst[2] = (src[2]<<8) + src[2];
		dst[3] = (src[3]<<8) + src[3];
		dst += 4;
		src += 4;
	}}
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

void VDPixmapGen_X16R16G16B16_To_R210::Compute(void *dst0, sint32 y) {
	const uint8* src = (const uint8*)mpSrc->GetRow(y, mSrcIndex);
	uint8 *dst = (uint8 *)dst0;

	for(sint32 i=0; i<mWidth; ++i) {
		uint16 r = ((uint16*)src)[2];
		uint16 g = ((uint16*)src)[1];
		uint16 b = ((uint16*)src)[0];
		src += 8;

		uint32 c = b + (g<<10) + (r<<20);
		*(uint32*)dst = _byteswap_ulong(c);
		dst+=4;
	}
}

void VDPixmapGen_X16R16G16B16_To_R10K::Compute(void *dst0, sint32 y) {
	const uint8* src = (const uint8*)mpSrc->GetRow(y, mSrcIndex);
	uint8 *dst = (uint8 *)dst0;

	for(sint32 i=0; i<mWidth; ++i) {
		uint16 r = ((uint16*)src)[2];
		uint16 g = ((uint16*)src)[1];
		uint16 b = ((uint16*)src)[0];
		src += 8;

		uint32 c = (b<<2) + (g<<12) + (r<<22);
		*(uint32*)dst = _byteswap_ulong(c);
		dst+=4;
	}
}

void VDPixmapGen_X16R16G16B16_To_B48R::Compute(void *dst0, sint32 y) {
	const uint8* src = (const uint8*)mpSrc->GetRow(y, mSrcIndex);
	uint16 *dst = (uint16 *)dst0;

	for(sint32 i=0; i<mWidth; ++i) {
		uint16 r = ((uint16*)src)[2];
		uint16 g = ((uint16*)src)[1];
		uint16 b = ((uint16*)src)[0];
		src += 8;

		dst[0] = _byteswap_ushort(r);
		dst[1] = _byteswap_ushort(g);
		dst[2] = _byteswap_ushort(b);
		dst+=3;
	}
}

void VDPixmapGen_B64A_To_X16R16G16B16::Compute(void *dst0, sint32 y) {
	uint16 *dst = (uint16 *)dst0;
	const uint16 *src = (const uint16 *)mpSrc->GetRow(y, mSrcIndex);

	{for(int i=0; i<mWidth/2; i++){
		__m128i c = _mm_loadu_si128((__m128i*)src);
		__m128i c0 = _mm_srli_epi16(c,8);
		__m128i c1 = _mm_slli_epi16(c,8);
		c = _mm_or_si128(c0,c1);
		c = _mm_shufflehi_epi16(_mm_shufflelo_epi16(c,0x1B),0x1B); // 0 1 2 3
		_mm_storeu_si128((__m128i*)dst,c);
		src += 8;
		dst += 8;
	}}

	if (mWidth & 1) {
		dst[0] = _byteswap_ushort(src[3]);
		dst[1] = _byteswap_ushort(src[2]);
		dst[2] = _byteswap_ushort(src[1]);
		dst[3] = _byteswap_ushort(src[0]);
	}
}

void VDPixmapGen_B48R_To_X16R16G16B16::Compute(void *dst0, sint32 y) {
	uint16 *dst = (uint16 *)dst0;
	const uint16 *src = (const uint16 *)mpSrc->GetRow(y, mSrcIndex);

	{for(int i=0; i<mWidth; i++){
		uint16 r = _byteswap_ushort(src[0]);
		uint16 g = _byteswap_ushort(src[1]);
		uint16 b = _byteswap_ushort(src[2]);

		dst[0] = b;
		dst[1] = g;
		dst[2] = r;
		dst[3] = 0;
		dst += 4;
		src += 3;
	}}
}

void VDPixmapGen_R210_To_X16R16G16B16::Compute(void *dst0, sint32 y) {
	uint16 *dst = (uint16 *)dst0;
	const uint32 *src = (const uint32 *)mpSrc->GetRow(y, mSrcIndex);

	{for(int i=0; i<mWidth; i++){
		uint32 c = _byteswap_ulong(*src);
		uint32 r = (c>>20) & 0x3FF;
		uint32 g = (c>>10) & 0x3FF;
		uint32 b = c & 0x3FF;

		dst[0] = b;
		dst[1] = g;
		dst[2] = r;
		dst[3] = 0;
		dst += 4;
		src++;
	}}
}

void VDPixmapGen_R10K_To_X16R16G16B16::Compute(void *dst0, sint32 y) {
	uint16 *dst = (uint16 *)dst0;
	const uint32 *src = (const uint32 *)mpSrc->GetRow(y, mSrcIndex);

	{for(int i=0; i<mWidth; i++){
		uint32 c = _byteswap_ulong(*src);
		uint32 r = (c>>22) & 0x3FF;
		uint32 g = (c>>12) & 0x3FF;
		uint32 b = (c>>2) & 0x3FF;

		dst[0] = b;
		dst[1] = g;
		dst[2] = r;
		dst[3] = 0;
		dst += 4;
		src++;
	}}
}

///////////////////////////////////////////////////////////////////////////////

void VDPixmapGen_X16R16G16B16_Normalize::TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
	mpSrc->TransformPixmapInfo(src,dst);
	if (VDPixmap_X16R16G16B16_IsNormalized(dst,max_r)) {
		set_normalize(dst, false);
	} else {
		ref_r = dst.ref_r;
		ref_g = dst.ref_g;
		ref_b = dst.ref_b;
		ref_a = dst.ref_a;
		set_normalize(dst, true);
	}
}

void VDPixmapGen_Y416_Normalize::TransformPixmapInfo(const FilterModPixmapInfo& src, FilterModPixmapInfo& dst) {
	mpSrc->TransformPixmapInfo(src,dst);
	if (VDPixmap_Y416_IsNormalized(dst)) {
		set_normalize(dst, false);
	} else {
		ref_r = dst.ref_r;
		ref_g = dst.ref_r;
		ref_b = dst.ref_r;
		ref_a = dst.ref_a;
		if(dst.colorRangeMode==vd2::kColorRangeMode_Full)
			max_r = 0xFFFF;
		else
			max_r = 0xFF00;
		set_normalize(dst, true);
	}
}

void VDPixmapGen_X16R16G16B16_Normalize::set_normalize(FilterModPixmapInfo& dst, bool v) {
	if (!v) {
		do_normalize = false;
	} else {
		do_normalize = true;
		mr = (uint64(max_r)*0x20000/ref_r+1)/2;
		mg = (uint64(max_r)*0x20000/ref_g+1)/2;
		mb = (uint64(max_r)*0x20000/ref_b+1)/2;
		if(dst.alpha_type==FilterModPixmapInfo::kAlphaInvalid)
			ma = 0;
		else
			ma = (uint64(max_a)*0x20000/ref_a+1)/2;
		dst.ref_r = max_r;
		dst.ref_g = max_r;
		dst.ref_b = max_r;
		dst.ref_a = max_a;
		scale_down = true;
		if (mr>=0x10000 || mg>=0x10000 || mb>=0x10000 || ma>=0x10000) scale_down = false;

		if (isChroma) {
			int n1 = vd2::chroma_neutral(max_r);
			int n0 = vd2::chroma_neutral(ref_r);
			bias = n1 - ((n0*mr+0x8000)>>16);
		} else {
			bias = 0;
		}
	}

	a_mask = 0;
	if(dst.alpha_type==FilterModPixmapInfo::kAlphaInvalid && wipe_alpha){
		ref_a = 0;
		a_mask = max_a;
	}
}

int VDPixmapGen_X16R16G16B16_Normalize::ComputeSpan(uint16* dst, const uint16* src, int n) {
	if (!do_normalize && !a_mask) {
		memcpy(dst,src,n*2);
		return n;
	}
	n = n & ~7;
	if (n==0) return 0;
	if (do_normalize && bias!=0)
		ComputeNormalizeBias(dst,src,n);
	else if (do_normalize)
		ComputeNormalize(dst,src,n);
	else if(a_mask)
		ComputeWipeAlpha(dst,src,n);
	return n;
}

void VDPixmapGen_X16R16G16B16_Normalize::ComputeNormalize(uint16* dst, const uint16* src, int n) {
	uint16 sr = 0xFFFF-ref_r;
	uint16 sg = 0xFFFF-ref_g;
	uint16 sb = 0xFFFF-ref_b;
	uint16 sa = 0xFFFF-ref_a;
	__m128i sat = _mm_set_epi16(sa,sr,sg,sb,sa,sr,sg,sb);
	__m128i mm = _mm_set_epi16(ma,mr,mg,mb,ma,mr,mg,mb);
	__m128i mask = _mm_set_epi16(a_mask,0,0,0,a_mask,0,0,0);

	if (scale_down) {
		{for(sint32 x=0; x<n/8; x++) {
			__m128i c = _mm_loadu_si128((const __m128i*)src);
			c = _mm_adds_epu16(c,sat);
			c = _mm_sub_epi16(c,sat);
			__m128i a = _mm_mullo_epi16(c,mm);
			a = _mm_srli_epi16(a,15);
			c = _mm_mulhi_epu16(c,mm);
			c = _mm_adds_epu16(c,a);
			c = _mm_or_si128(c,mask);
			_mm_storeu_si128((__m128i*)dst,c);
			src+=8;
			dst+=8;
		}}
	} else {
		__m128i mmh = _mm_set_epi16(ma>>16,mr>>16,mg>>16,mb>>16,ma>>16,mr>>16,mg>>16,mb>>16);
		{for(sint32 x=0; x<n/8; x++) {
			__m128i c = _mm_loadu_si128((const __m128i*)src);
			c = _mm_adds_epu16(c,sat);
			c = _mm_sub_epi16(c,sat);
			__m128i a = _mm_mullo_epi16(c,mm);
			a = _mm_srli_epi16(a,15);
			__m128i b = _mm_mullo_epi16(c,mmh);
			c = _mm_mulhi_epu16(c,mm);
			c = _mm_adds_epu16(c,a);
			c = _mm_adds_epu16(c,b);
			c = _mm_or_si128(c,mask);
			_mm_storeu_si128((__m128i*)dst,c);
			src+=8;
			dst+=8;
		}}
	}
}

void VDPixmapGen_X16R16G16B16_Normalize::ComputeNormalizeBias(uint16* dst, const uint16* src, int n) {
	uint16 sr = 0xFFFF-ref_r;
	uint16 sg = 0xFFFF-ref_g;
	uint16 sb = 0xFFFF-ref_b;
	uint16 sa = 0xFFFF-ref_a;
	__m128i sat = _mm_set_epi16(sa,sr,sg,sb,sa,sr,sg,sb);
	__m128i mm = _mm_set_epi16(ma,mr,mg,mb,ma,mr,mg,mb);
	__m128i mask = _mm_set_epi16(a_mask,0,0,0,a_mask,0,0,0);
	int bv1 = bias>0 ? +bias:0;
	int bv2 = bias<0 ? -bias:0;
	__m128i bias1 = _mm_set_epi16(0,bv1,0,bv1,0,bv1,0,bv1);
	__m128i bias2 = _mm_set_epi16(0,bv2,0,bv2,0,bv2,0,bv2);

	if (scale_down) {
		{for(sint32 x=0; x<n/8; x++) {
			__m128i c = _mm_loadu_si128((const __m128i*)src);
			c = _mm_adds_epu16(c,sat);
			c = _mm_sub_epi16(c,sat);
			__m128i a = _mm_mullo_epi16(c,mm);
			a = _mm_srli_epi16(a,15);
			c = _mm_mulhi_epu16(c,mm);
			c = _mm_adds_epu16(c,a);
			c = _mm_adds_epu16(c,bias1);
			c = _mm_subs_epu16(c,bias2);
			c = _mm_or_si128(c,mask);
			_mm_storeu_si128((__m128i*)dst,c);
			src+=8;
			dst+=8;
		}}
	} else {
		__m128i mmh = _mm_set_epi16(ma>>16,mr>>16,mg>>16,mb>>16,ma>>16,mr>>16,mg>>16,mb>>16);
		{for(sint32 x=0; x<n/8; x++) {
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
			c = _mm_or_si128(c,mask);
			_mm_storeu_si128((__m128i*)dst,c);
			src+=8;
			dst+=8;
		}}
	}
}

void VDPixmapGen_X16R16G16B16_Normalize::ComputeWipeAlpha(uint16* dst, const uint16* src, int n) {
	__m128i mask1 = _mm_set_epi16(0,-1,-1,-1,0,-1,-1,-1);
	__m128i mask2 = _mm_set_epi16(a_mask,0,0,0,a_mask,0,0,0);
	{for(sint32 x=0; x<n/8; x++) {
		__m128i c = _mm_loadu_si128((const __m128i*)src);
		c = _mm_and_si128(c,mask1);
		c = _mm_or_si128(c,mask2);
		_mm_storeu_si128((__m128i*)dst,c);
		src+=8;
		dst+=8;
	}}
}

void ExtraGen_X16R16G16B16_Normalize::Create(VDPixmapUberBlitterGenerator& gen, const VDPixmapLayout& dst) {
	if (dst.format==nsVDPixmap::kPixFormat_XRGB64) {
		VDPixmapGen_X16R16G16B16_Normalize* normalize = new VDPixmapGen_X16R16G16B16_Normalize;
		normalize->max_r = max_value;
		normalize->max_a = max_value;
		gen.swap(normalize);
	}
	if (dst.format==nsVDPixmap::kPixFormat_YUVA444_Y416) {
		VDPixmapGen_Y416_Normalize* normalize = new VDPixmapGen_Y416_Normalize;
		gen.swap(normalize);
	}
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

void ExtraGen_X8R8G8B8_Normalize::Create(VDPixmapUberBlitterGenerator& gen, const VDPixmapLayout& dst) {
	VDPixmapGen_X8R8G8B8_Normalize* normalize = new VDPixmapGen_X8R8G8B8_Normalize;
	gen.swap(normalize);
}

void VDPixmap_X16R16G16B16_Normalize(VDPixmap& pxdst, const VDPixmap& pxsrc, uint32 max_value) {
	VDPixmapGenSrc src;
	src.Init(pxsrc.w,pxsrc.h,kVDPixType_16x4_LE,pxsrc.w*8);
	src.SetSource(pxsrc.data,pxsrc.pitch,0);

	FilterModPixmapInfo info;
	FilterModPixmapInfo info2;
	VDPixmapGen_X16R16G16B16_Normalize gen;
	gen.Init(&src,0);
	gen.TransformPixmapInfo(pxsrc.info,pxdst.info);
	gen.AddWindowRequest(1,1);
	gen.Start();

	uint8* dst = (uint8*)pxdst.data;
	{for(sint32 y=0; y<pxsrc.h; y++) {
		gen.ProcessRow(dst,y);
		dst += pxdst.pitch;
	}}
}

