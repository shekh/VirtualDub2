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
#include <vd2/system/vectors.h>
#include "uberblit_ycbcr_generic.h"
#include <emmintrin.h>

extern const VDPixmapGenYCbCrBasis g_VDPixmapGenYCbCrBasis_601 = {
	0.299f,
	0.114f,
	{
		0.0f,   -0.3441363f,   1.772f,
		1.402f, -0.7141363f,   0.0f,
	}
};

extern const VDPixmapGenYCbCrBasis g_VDPixmapGenYCbCrBasis_709 = {
	0.2126f,
	0.0722f,
	{
		0.0f,     -0.1873243f,    1.8556f,
		1.5748f,  -0.4681243f,    0.0f,
	}
};

////////////////////////////////////////////////////////////////////////////

VDPixmapGenYCbCrToRGB32Generic::VDPixmapGenYCbCrToRGB32Generic(const VDPixmapGenYCbCrBasis& basis, bool studioRGB) {
	float scale;
	float scale2;
	float bias;

	if (studioRGB) {
		// warning: this path is not used / not tested
		scale = 4096.0f * (255.0f / 219.0f);
		scale2 = 4096.0f * (255.0f / 224.0f);
		bias = -16.0f / 255.0f;
	} else {
		scale = 4096.0f;
		scale2 = 4096.0f;
		bias = 0.0f;
	}

	mCoY = VDRoundToInt32(scale);
	mCoRCr = VDRoundToInt32(basis.mToRGB[1][0] * scale2);
	mCoGCr = VDRoundToInt32(basis.mToRGB[1][1] * scale2);
	mCoGCb = VDRoundToInt32(basis.mToRGB[0][1] * scale2);
	mCoBCb = VDRoundToInt32(basis.mToRGB[0][2] * scale2);
	mBiasR = VDRoundToInt32(bias*scale/256) - 128*mCoRCr/256 + 8;
	mBiasG = VDRoundToInt32(bias*scale/256) - 128*(mCoGCr + mCoGCb)/256 + 8;
	mBiasB = VDRoundToInt32(bias*scale/256) - 128*mCoBCb/256 + 8;
}

uint32 VDPixmapGenYCbCrToRGB32Generic::GetType(uint32 output) const {
	return (mpSrcY->GetType(mSrcIndexY) & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixType_8888 | kVDPixSpace_BGR;
}

void VDPixmapGenYCbCrToRGB32Generic::Compute(void *dst0, sint32 y) {
	uint8 *dst = (uint8 *)dst0;
	const uint8 *srcY = (const uint8 *)mpSrcY->GetRow(y, mSrcIndexY);
	const uint8 *srcCb = (const uint8 *)mpSrcCb->GetRow(y, mSrcIndexCb);
	const uint8 *srcCr = (const uint8 *)mpSrcCr->GetRow(y, mSrcIndexCr);

	const __m128i zero = _mm_setzero_si128();
	const __m128i consty = _mm_set1_epi16(mCoY);
	const __m128i constrr = _mm_set1_epi16(mCoRCr);
	const __m128i constrg = _mm_set1_epi16(-mCoGCr);
	const __m128i constbg = _mm_set1_epi16(-mCoGCb);
	const __m128i constbb = _mm_set1_epi16(mCoBCb);
	const __m128i constr0 = _mm_set1_epi16(-mBiasR);
	const __m128i constg0 = _mm_set1_epi16(mBiasG);
	const __m128i constb0 = _mm_set1_epi16(-mBiasB);

	const int n0 = mWidth/16;
	const int n1 = mWidth-n0*16;

	{for(int i=0; i<n0; i++){
		{
			__m128i y = _mm_loadu_si128((__m128i*)srcY);
			__m128i cb = _mm_loadu_si128((__m128i*)srcCb);
			__m128i cr = _mm_loadu_si128((__m128i*)srcCr);

			y = _mm_unpacklo_epi8(zero,y);
			y = _mm_mulhi_epu16(y,consty);

			__m128i r = _mm_unpacklo_epi8(zero,cr);
			r = _mm_mulhi_epu16(r,constrr);
			r = _mm_add_epi16(r,y);
			r = _mm_subs_epu16(r,constr0);
			r = _mm_srli_epi16(r,4);
			r = _mm_packus_epi16(r,zero);

			__m128i r1 = _mm_unpacklo_epi8(zero,cr);
			r1 = _mm_mulhi_epu16(r1,constrg);
			__m128i g = _mm_add_epi16(y,constg0);
			g = _mm_subs_epu16(g,r1);
			__m128i b1 = _mm_unpacklo_epi8(zero,cb);
			b1 = _mm_mulhi_epu16(b1,constbg);
			g = _mm_subs_epu16(g,b1);
			g = _mm_srli_epi16(g,4);
			g = _mm_packus_epi16(g,zero);

			__m128i b = _mm_unpacklo_epi8(zero,cb);
			b = _mm_mulhi_epu16(b,constbb);
			b = _mm_add_epi16(b,y);
			b = _mm_subs_epu16(b,constb0);
			b = _mm_srli_epi16(b,4);
			b = _mm_packus_epi16(b,zero);

			__m128i bg = _mm_unpacklo_epi8(b,g);
			__m128i ra = _mm_unpacklo_epi8(r,zero);
			__m128i bgr0 = _mm_unpacklo_epi16(bg,ra);
			__m128i bgr1 = _mm_unpackhi_epi16(bg,ra);
			_mm_storeu_si128((__m128i*)dst,bgr0);
			dst += 16;
			_mm_storeu_si128((__m128i*)dst,bgr1);
			dst += 16;
		}

		{
			__m128i y = _mm_loadu_si128((__m128i*)srcY);
			__m128i cb = _mm_loadu_si128((__m128i*)srcCb);
			__m128i cr = _mm_loadu_si128((__m128i*)srcCr);

			y = _mm_unpackhi_epi8(zero,y);
			y = _mm_mulhi_epu16(y,consty);

			__m128i r = _mm_unpackhi_epi8(zero,cr);
			r = _mm_mulhi_epu16(r,constrr);
			r = _mm_add_epi16(r,y);
			r = _mm_subs_epu16(r,constr0);
			r = _mm_srli_epi16(r,4);
			r = _mm_packus_epi16(r,zero);

			__m128i r1 = _mm_unpackhi_epi8(zero,cr);
			r1 = _mm_mulhi_epu16(r1,constrg);
			__m128i g = _mm_add_epi16(y,constg0);
			g = _mm_subs_epu16(g,r1);
			__m128i b1 = _mm_unpackhi_epi8(zero,cb);
			b1 = _mm_mulhi_epu16(b1,constbg);
			g = _mm_subs_epu16(g,b1);
			g = _mm_srli_epi16(g,4);
			g = _mm_packus_epi16(g,zero);

			__m128i b = _mm_unpackhi_epi8(zero,cb);
			b = _mm_mulhi_epu16(b,constbb);
			b = _mm_add_epi16(b,y);
			b = _mm_subs_epu16(b,constb0);
			b = _mm_srli_epi16(b,4);
			b = _mm_packus_epi16(b,zero);

			__m128i bg = _mm_unpacklo_epi8(b,g);
			__m128i ra = _mm_unpacklo_epi8(r,zero);
			__m128i bgr0 = _mm_unpacklo_epi16(bg,ra);
			__m128i bgr1 = _mm_unpackhi_epi16(bg,ra);
			_mm_storeu_si128((__m128i*)dst,bgr0);
			dst += 16;
			_mm_storeu_si128((__m128i*)dst,bgr1);
			dst += 16;
		}

		srcY += 16;
		srcCb += 16;
		srcCr += 16;
	}}

	const sint32 coY = mCoY;
	const sint32 coRCr = mCoRCr;
	const sint32 coGCr = -mCoGCr;
	const sint32 coGCb = -mCoGCb;
	const sint32 coBCb = mCoBCb;
	const sint32 biasR = mBiasR;
	const sint32 biasG = mBiasG;
	const sint32 biasB = mBiasB;

	// intermediate shifts are necessary to make it equivalent to sse version
	for(sint32 i=0; i<n1; ++i) {
		sint32 y = srcY[i];
		sint32 cb = srcCb[i];
		sint32 cr = srcCr[i];

		y = (y*coY)>>8;

		sint32 r = biasR + y + ((coRCr * cr)>>8);
		sint32 g = biasG + y - ((coGCr * cr)>>8) - ((coGCb * cb)>>8);
		sint32 b = biasB + y + ((coBCb * cb)>>8);

		// clip low
		r &= ~r >> 31;
		g &= ~g >> 31;
		b &= ~b >> 31;

		// clip high
		sint32 clipR = 0xfff - r;
		sint32 clipG = 0xfff - g;
		sint32 clipB = 0xfff - b;
		r |= clipR >> 31;
		g |= clipG >> 31;
		b |= clipB >> 31;

		dst[0] = (uint8)(b >> 4);
		dst[1] = (uint8)(g >> 4);
		dst[2] = (uint8)(r >> 4);
		dst[3] = 0xff;

		dst += 4;
	}
}

////////////////////////////////////////////////////////////////////////////

VDPixmapGenYCbCrToRGB64Generic::VDPixmapGenYCbCrToRGB64Generic(const VDPixmapGenYCbCrBasis& basis, bool studioRGB) {
	this->basis = basis;
	this->studioRGB = studioRGB;
}

void VDPixmapGenYCbCrToRGB64Generic::UpdateParams() {
	float scale;
	float scale2;
	float bias;

	if (studioRGB) {
		scale = 255.0f / 219.0f;
		scale2 = 255.0f / 224.0f;
		bias = -16.0f / 255.0f;
	} else {
		scale = 1.0f;
		scale2 = 1.0f;
		bias = 0.0f;
	}

	int max_value = 0xFFFF;
	float m = float(max_value)/ref_r;

	mCoY = scale;
	mCoRCr = basis.mToRGB[1][0] * scale2;
	mCoGCr = basis.mToRGB[1][1] * scale2;
	mCoGCb = basis.mToRGB[0][1] * scale2;
	mCoBCb = basis.mToRGB[0][2] * scale2;
	mCoY *= m;
	mCoRCr *= m;
	mCoGCr *= m;
	mCoGCb *= m;
	mCoBCb *= m;
	mBiasR = bias*scale*max_value - (128.0f / 255.0f)*mCoRCr*ref_r;
	mBiasG = bias*scale*max_value - (128.0f / 255.0f)*(mCoGCr + mCoGCb)*ref_r;
	mBiasB = bias*scale*max_value - (128.0f / 255.0f)*mCoBCb*ref_r;
}

uint32 VDPixmapGenYCbCrToRGB64Generic::GetType(uint32 output) const {
	return (mpSrcY->GetType(mSrcIndexY) & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixType_16x4_LE | kVDPixSpace_BGR;
}

void VDPixmapGenYCbCrToRGB64Generic::Compute(void *dst0, sint32 y) {
	uint16 *dst = (uint16 *)dst0;
	const uint16 *srcY = (const uint16 *)mpSrcY->GetRow(y, mSrcIndexY);
	const uint16 *srcCb = (const uint16 *)mpSrcCb->GetRow(y, mSrcIndexCb);
	const uint16 *srcCr = (const uint16 *)mpSrcCr->GetRow(y, mSrcIndexCr);

	VDCPUCleanupExtensions();
	// requires round-to-nearest (standard)
	int prev_csr = _mm_getcsr();
	VDASSERT((prev_csr & 0x6000)==0);

	const __m128 const_my = _mm_set1_ps(mCoY);
	const __m128 const_rcr = _mm_set1_ps(mCoRCr);
	const __m128 const_gcr = _mm_set1_ps(mCoGCr);
	const __m128 const_gcb = _mm_set1_ps(mCoGCb);
	const __m128 const_bcb = _mm_set1_ps(mCoBCb);
	const __m128 const_biasR = _mm_set1_ps(mBiasR-0x8000);
	const __m128 const_biasG = _mm_set1_ps(mBiasG-0x8000);
	const __m128 const_biasB = _mm_set1_ps(mBiasB-0x8000);
	const __m128i zero = _mm_setzero_si128();
	const __m128i bias = _mm_set1_epi16((uint16)0x8000);

	sint32 w0 = (mWidth<4) ? 0 : (mWidth-4) & ~3;
	sint32 w1 = mWidth-w0;

	for(sint32 i=0; i<w0/4; ++i) {
		__m128i y = _mm_loadu_si128((__m128i*)srcY);
		y = _mm_unpacklo_epi16(y,zero);
		__m128 yf = _mm_cvtepi32_ps(y);

		__m128i cb = _mm_loadu_si128((__m128i*)srcCb);
		cb = _mm_unpacklo_epi16(cb,zero);
		__m128 cbf = _mm_cvtepi32_ps(cb);

		__m128i cr = _mm_loadu_si128((__m128i*)srcCr);
		cr = _mm_unpacklo_epi16(cr,zero);
		__m128 crf = _mm_cvtepi32_ps(cr);

		yf = _mm_mul_ps(yf,const_my);

		__m128 Rf = _mm_mul_ps(crf,const_rcr);
		Rf = _mm_add_ps(Rf,yf);
		Rf = _mm_add_ps(Rf,const_biasR);

		__m128 Bf = _mm_mul_ps(cbf,const_bcb);
		Bf = _mm_add_ps(Bf,yf);
		Bf = _mm_add_ps(Bf,const_biasB);

		crf = _mm_mul_ps(crf,const_gcr);
		cbf = _mm_mul_ps(cbf,const_gcb);
		__m128 Gf = _mm_add_ps(crf,cbf);
		Gf = _mm_add_ps(Gf,yf);
		Gf = _mm_add_ps(Gf,const_biasG);

		__m128i R = _mm_cvtps_epi32(Rf);
		__m128i G = _mm_cvtps_epi32(Gf);
		__m128i B = _mm_cvtps_epi32(Bf);
		R = _mm_packs_epi32(R,R);
		G = _mm_packs_epi32(G,G);
		B = _mm_packs_epi32(B,B);
		__m128i BG = _mm_unpacklo_epi16(B,G);
		__m128i R0 = _mm_unpacklo_epi16(R,zero);
		BG = _mm_add_epi16(BG,bias);
		R0 = _mm_add_epi16(R0,bias);
		__m128i BGR0_0 = _mm_unpacklo_epi32(BG,R0);
		__m128i BGR0_1 = _mm_unpackhi_epi32(BG,R0);
		_mm_storeu_si128((__m128i*)dst,BGR0_0);
		_mm_storeu_si128((__m128i*)(dst+8),BGR0_1);

		dst += 16;
		srcY += 4;
		srcCb += 4;
		srcCr += 4;
	}

	for(sint32 i=0; i<w1; ++i) {
		int y = *srcY;
		__m128 yf = _mm_cvtsi32_ss(_mm_setzero_ps(),y);

		int cb = *srcCb;
		__m128 cbf = _mm_cvtsi32_ss(_mm_setzero_ps(),cb);

		int cr = *srcCr;
		__m128 crf = _mm_cvtsi32_ss(_mm_setzero_ps(),cr);

		yf = _mm_mul_ss(yf,const_my);

		__m128 Rf = _mm_mul_ss(crf,const_rcr);
		Rf = _mm_add_ss(Rf,yf);
		Rf = _mm_add_ss(Rf,const_biasR);

		__m128 Bf = _mm_mul_ss(cbf,const_bcb);
		Bf = _mm_add_ss(Bf,yf);
		Bf = _mm_add_ss(Bf,const_biasB);

		crf = _mm_mul_ss(crf,const_gcr);
		cbf = _mm_mul_ss(cbf,const_gcb);
		__m128 Gf = _mm_add_ss(crf,cbf);
		Gf = _mm_add_ss(Gf,yf);
		Gf = _mm_add_ss(Gf,const_biasG);

		int R = _mm_cvtss_si32(Rf) + 0x8000;
		int G = _mm_cvtss_si32(Gf) + 0x8000;
		int B = _mm_cvtss_si32(Bf) + 0x8000;
		if(R<0) R = 0; if(R>0xFFFF) R = 0xFFFF;
		if(G<0) G = 0; if(G>0xFFFF) G = 0xFFFF;
		if(B<0) B = 0; if(B>0xFFFF) B = 0xFFFF;
		dst[0] = uint16(B);
		dst[1] = uint16(G);
		dst[2] = uint16(R);
		dst[3] = 0;

		dst += 4;
		srcY++;
		srcCb++;
		srcCr++;
	}
}

////////////////////////////////////////////////////////////////////////////

VDPixmapGenYCbCrToRGB32FGeneric::VDPixmapGenYCbCrToRGB32FGeneric(const VDPixmapGenYCbCrBasis& basis, bool studioRGB) {
	float scale;
	float scale2;
	float bias;

	if (studioRGB) {
		scale = 255.0f / 219.0f;
		scale2 = 255.0f / 224.0f;
		bias = -16.0f / 255.0f;
	} else {
		scale = 1.0f;
		scale2 = 1.0f;
		bias = 0.0f;
	}

	mCoY = scale;
	mCoRCr = basis.mToRGB[1][0] * scale2;
	mCoGCr = basis.mToRGB[1][1] * scale2;
	mCoGCb = basis.mToRGB[0][1] * scale2;
	mCoBCb = basis.mToRGB[0][2] * scale2;
	mBiasR = bias*scale - (128.0f / 255.0f)*mCoRCr;
	mBiasG = bias*scale - (128.0f / 255.0f)*(mCoGCr + mCoGCb);
	mBiasB = bias*scale - (128.0f / 255.0f)*mCoBCb;
}

uint32 VDPixmapGenYCbCrToRGB32FGeneric::GetType(uint32 output) const {
	return (mpSrcY->GetType(mSrcIndexY) & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixType_32Fx4_LE | kVDPixSpace_BGR;
}

void VDPixmapGenYCbCrToRGB32FGeneric::Compute(void *dst0, sint32 y) {
	float *dst = (float *)dst0;
	const float *srcY = (const float *)mpSrcY->GetRow(y, mSrcIndexY);
	const float *srcCb = (const float *)mpSrcCb->GetRow(y, mSrcIndexCb);
	const float *srcCr = (const float *)mpSrcCr->GetRow(y, mSrcIndexCr);

	VDCPUCleanupExtensions();

	const float coRCr = mCoRCr;
	const float coGCr = mCoGCr;
	const float coGCb = mCoGCb;
	const float coBCb = mCoBCb;
	const float biasR = mBiasR;
	const float biasG = mBiasG;
	const float biasB = mBiasB;

	for(sint32 i=0; i<mWidth; ++i) {
		float y = srcY[i];
		float cb = srcCb[i];
		float cr = srcCr[i];

		y *= mCoY;

		dst[0] = biasR + y + coRCr * cr;
		dst[1] = biasG + y + coGCr * cr + coGCb * cb;
		dst[2] = biasB + y + coBCb * cb;
		dst[3] = 1.0f;
		dst += 4;
	}
}

////////////////////////////////////////////////////////////////////////////

VDPixmapGenRGB32ToYCbCrGeneric::VDPixmapGenRGB32ToYCbCrGeneric(const VDPixmapGenYCbCrBasis& basis, bool studioRGB, uint32 colorSpace)
	: mColorSpace(colorSpace)
{
	float scale;
	float scale2;
	float bias;

	if (studioRGB) {
		// warning: this path is not used / not tested
		scale = 255.0f / 219.0f;
		scale2 = 112.0f / 255.0f;
		bias = -16.0f;
	} else {
		scale = 1.0f;
		scale2 = 0.5f;
		bias = 0.0f;
	}


	// compute Y coefficients
	float coYR  = basis.mKr;
	float coYG  = (1.0f - basis.mKr - basis.mKb);
	float coYB  = basis.mKb;

	mCoYR  = VDRoundToInt32(scale * coYR  * 65536.0f);
	mCoYG  = VDRoundToInt32(scale * coYG  * 65536.0f);
	mCoYB  = VDRoundToInt32(scale * coYB  * 65536.0f);
	mCoYA  = 0x8000;

	// Cb = 0.5 * (B-Y) / (1-Kb)
	const float coCb = scale2 / (1.0f - basis.mKb);
	float coCbR = (0.0f - coYR) * coCb;
	float coCbG = (0.0f - coYG) * coCb;
	float coCbB = (1.0f - coYB) * coCb;
	float coCbA = (coCbR + coCbG + coCbB) * bias;

	// Cr = 0.5 * (R-Y) / (1-Kr)
	const float coCr = scale2 / (1.0f - basis.mKr);
	float coCrR = (1.0f - coYR) * coCr;
	float coCrG = (0.0f - coYG) * coCr;
	float coCrB = (0.0f - coYB) * coCr;
	float coCrA = (coCrR + coCrG + coCrB) * bias;

	mCoCbR = VDRoundToInt32(coCbR * 65536.0f);
	mCoCbG = VDRoundToInt32(coCbG * 65536.0f);
	mCoCbB = VDRoundToInt32(coCbB * 65536.0f);
	mCoCbA = VDRoundToInt32(coCbA * 65536.0f) + 0x808000;

	mCoCrR = VDRoundToInt32(coCrR * 65536.0f);
	mCoCrG = VDRoundToInt32(coCrG * 65536.0f);
	mCoCrB = VDRoundToInt32(coCrB * 65536.0f);
	mCoCrA = VDRoundToInt32(coCrA * 65536.0f) + 0x808000;
}

uint32 VDPixmapGenRGB32ToYCbCrGeneric::GetType(uint32 output) const {
	return (mpSrc->GetType(mSrcIndex) & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixType_8 | mColorSpace;
}

void VDPixmapGenRGB32ToYCbCrGeneric::Compute(void *dst0, sint32 y) {
	uint8 *dstCr = (uint8 *)dst0;
	uint8 *dstY = dstCr + mWindowPitch;
	uint8 *dstCb = dstY + mWindowPitch;

	const uint8 *srcRGB = (const uint8 *)mpSrc->GetRow(y, mSrcIndex);

	const sint32 coYR = mCoYR;
	const sint32 coYG = mCoYG;
	const sint32 coYB = mCoYB;
	const sint32 coCbR = mCoCbR;
	const sint32 coCbG = mCoCbG;
	const sint32 coCbB = mCoCbB;
	const sint32 coCrR = mCoCrR;
	const sint32 coCrG = mCoCrG;
	const sint32 coCrB = mCoCrB;
	const sint32 coYA = mCoYA;
	const sint32 coCbA = mCoCbA;
	const sint32 coCrA = mCoCrA;
	
	const sint32 w = mWidth;
	for(sint32 i=0; i<w; ++i) {
		int r = (int)srcRGB[2];
		int g = (int)srcRGB[1];
		int b = (int)srcRGB[0];
		srcRGB += 4;			

		// Normally, this can be optimized by encoding the chroma channels as
		// (R-Y) and (B-Y) differences. However, us working in fixed point complicates
		// things here, so for now we do a full 4x3 matrix.

		sint32 y16  = coYR  * r + coYG  * g + coYB  * b + coYA;
		sint32 cb16 = coCbR * r + coCbG * g + coCbB * b + coCbA;
		sint32 cr16 = coCrR * r + coCrG * g + coCrB * b + coCrA;

		// Quite annoyingly, we have to clip chroma on the high end since the transformation
		// targets [0,1] instead of [0,1). This occurs due to the bias by +0.5 to make
		// reference black for chroma fall on 128 instead of 127.5. The resulting
		// transformation is the one used for JFIF and also for TIFF with the full
		// range ReferenceBlack/ReferenceWhite values.

		cb16 |= (0xffffff - cb16) >> 31;
		cr16 |= (0xffffff - cr16) >> 31;

		*dstCb++ = (uint8)(cb16 >> 16);
		*dstY ++ = (uint8)( y16 >> 16);
		*dstCr++ = (uint8)(cr16 >> 16);
	}
}

////////////////////////////////////////////////////////////////////////////

VDPixmapGenRGB32FToYCbCrGeneric::VDPixmapGenRGB32FToYCbCrGeneric(const VDPixmapGenYCbCrBasis& basis, bool studioRGB, uint32 colorSpace)
	: mColorSpace(colorSpace)
{
	float scale;
	float scale2;
	float bias;

	if (studioRGB) {
		scale = 219.0f / 255.0f;
		scale2 = 112.0f / 255.0f;
		bias = 16.0f / 255.0f;
	} else {
		scale = 1.0f;
		scale2 = 0.5f;
		bias = 0.0f;
	}

	// compute Y coefficients
	float coYR  = basis.mKr;
	float coYG  = (1.0f - basis.mKr - basis.mKb);
	float coYB  = basis.mKb;
	mCoYR = coYR*scale;
	mCoYG = coYG*scale;
	mCoYB = coYB*scale;
	mCoYA = bias;

	// Cb = 0.5 * (B-Y) / (1-Kb)
	const float coCb = scale2 / (1.0f - basis.mKb);
	float coCbR = (0.0f - coYR) * coCb;
	float coCbG = (0.0f - coYG) * coCb;
	float coCbB = (1.0f - coYB) * coCb;
	float coCbA = (coCbR + coCbG + coCbB) * bias;
	mCoCbR = coCbR;
	mCoCbG = coCbG;
	mCoCbB = coCbB;
	mCoCbA = coCbA + (128.0f / 255.0f);

	// Cr = 0.5 * (R-Y) / (1-Kr)
	const float coCr = scale2 / (1.0f - basis.mKr);
	float coCrR = (1.0f - coYR) * coCr;
	float coCrG = (0.0f - coYG) * coCr;
	float coCrB = (0.0f - coYB) * coCr;
	float coCrA = (coCrR + coCrG + coCrB) * bias;
	mCoCrR = coCrR;
	mCoCrG = coCrG;
	mCoCrB = coCrB;
	mCoCrA = coCrA + (128.0f / 255.0f);
}

uint32 VDPixmapGenRGB32FToYCbCrGeneric::GetType(uint32 output) const {
	return (mpSrc->GetType(mSrcIndex) & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixType_32F_LE | mColorSpace;
}

void VDPixmapGenRGB32FToYCbCrGeneric::Compute(void *dst0, sint32 y) {
	float *dstCr = (float *)dst0;
	float *dstY  = vdptroffset(dstCr, mWindowPitch);
	float *dstCb = vdptroffset(dstY, mWindowPitch);

	const float *srcRGB = (const float *)mpSrc->GetRow(y, mSrcIndex);

	VDCPUCleanupExtensions();

	const float coYR = mCoYR;
	const float coYG = mCoYG;
	const float coYB = mCoYB;
	const float coYA = mCoYA;
	const float coCbR = mCoCbR;
	const float coCbG = mCoCbG;
	const float coCbB = mCoCbB;
	const float coCbA = mCoCbA;
	const float coCrR = mCoCrR;
	const float coCrG = mCoCrG;
	const float coCrB = mCoCrB;
	const float coCrA = mCoCrA;

	const sint32 w = mWidth;
	for(sint32 i=0; i<w; ++i) {
		float r = srcRGB[0];
		float g = srcRGB[1];
		float b = srcRGB[2];
		srcRGB += 4;

		*dstY++  = coYR  * r + coYG  * g + coYB  * b + coYA;
		*dstCb++ = coCbR * r + coCbG * g + coCbB * b + coCbA;
		*dstCr++ = coCrR * r + coCrG * g + coCrB * b + coCrA;
	}
}

////////////////////////////////////////////////////////////////////////////

VDPixmapGenYCbCrToYCbCrGeneric::VDPixmapGenYCbCrToYCbCrGeneric(const VDPixmapGenYCbCrBasis& dstBasis, bool dstLimitedRange, const VDPixmapGenYCbCrBasis& srcBasis, bool srcLimitedRange, uint32 colorSpace)
	: mColorSpace(colorSpace)
{
	vdfloat3x3 dstToRGB;
	dstToRGB.m[0] = vdfloat3c(1, 1, 1);
	dstToRGB.m[1] = vdfloat3c(dstBasis.mToRGB[0]);
	dstToRGB.m[2] = vdfloat3c(dstBasis.mToRGB[1]);

	if (dstLimitedRange) {
		dstToRGB.m[0] *= (255.0f / 219.0f);
		dstToRGB.m[1] *= (128.0f / 112.0f);
		dstToRGB.m[2] *= (128.0f / 112.0f);
	}

	vdfloat3x3 srcToRGB;
	srcToRGB.m[0] = vdfloat3c(1, 1, 1);
	srcToRGB.m[1] = vdfloat3c(srcBasis.mToRGB[0]);
	srcToRGB.m[2] = vdfloat3c(srcBasis.mToRGB[1]);

	if (srcLimitedRange) {
		srcToRGB.m[0] *= (255.0f / 219.0f);
		srcToRGB.m[1] *= (128.0f / 112.0f);
		srcToRGB.m[2] *= (128.0f / 112.0f);
	}

	vdfloat3x3 xf(srcToRGB * ~dstToRGB);

	// We should get a transform that looks like this:
	//
	//	            |k 0 0|
	//	[y cb cr 1] |a c e| = [y' cb' cr]
	//	            |b d f|
	//				|x y z|

	VDASSERT(fabsf(xf.m[0].v[1]) < 1e-5f);
	VDASSERT(fabsf(xf.m[0].v[2]) < 1e-5f);

	mCoYY   = VDRoundToInt32(xf.m[0].v[0] * 65536.0f);
	mCoYCb  = VDRoundToInt32(xf.m[1].v[0] * 65536.0f);
	mCoYCr  = VDRoundToInt32(xf.m[2].v[0] * 65536.0f);
	mCoCbCb = VDRoundToInt32(xf.m[1].v[1] * 65536.0f);
	mCoCbCr = VDRoundToInt32(xf.m[2].v[1] * 65536.0f);
	mCoCrCb = VDRoundToInt32(xf.m[1].v[2] * 65536.0f);
	mCoCrCr = VDRoundToInt32(xf.m[2].v[2] * 65536.0f);

	vdfloat3c srcBias(0, 128.0f/255.0f, 128.0f/255.0f);
	if (srcLimitedRange)
		srcBias.set(16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f);

	vdfloat3c dstBias(0, 128.0f/255.0f, 128.0f/255.0f);
	if (dstLimitedRange)
		dstBias.set(16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f);

	vdfloat3 bias = -srcBias * xf + dstBias;

	mCoYA  = VDRoundToInt32(bias.x * 255.0f * 65536.0f) + 0x8000;
	mCoCbA = VDRoundToInt32(bias.y * 255.0f * 65536.0f) + 0x8000;
	mCoCrA = VDRoundToInt32(bias.z * 255.0f * 65536.0f) + 0x8000;
}

void VDPixmapGenYCbCrToYCbCrGeneric::Start() {
	mpSrcY->Start();
	mpSrcCb->Start();
	mpSrcCr->Start();

	StartWindow(mWidth, 3);
}

const void *VDPixmapGenYCbCrToYCbCrGeneric::GetRow(sint32 y, uint32 index) {
	return (const uint8 *)VDPixmapGenYCbCrToRGBBase::GetRow(y, index) + mWindowPitch * index;
}

uint32 VDPixmapGenYCbCrToYCbCrGeneric::GetType(uint32 output) const {
	return (mpSrcY->GetType(mSrcIndexY) & ~kVDPixSpace_Mask) | mColorSpace;
}

void VDPixmapGenYCbCrToYCbCrGeneric::Compute(void *dst0, sint32 ypos) {
	uint8 *dstCr = (uint8 *)dst0;
	uint8 *dstY  = dstCr + mWindowPitch;
	uint8 *dstCb = dstY + mWindowPitch;

	const uint8 *srcY  = (const uint8 *)mpSrcY ->GetRow(ypos, mSrcIndexY );
	const uint8 *srcCb = (const uint8 *)mpSrcCb->GetRow(ypos, mSrcIndexCb);
	const uint8 *srcCr = (const uint8 *)mpSrcCr->GetRow(ypos, mSrcIndexCr);

	const sint32 coYY   = mCoYY;
	const sint32 coYCb  = mCoYCb;
	const sint32 coYCr  = mCoYCr;
	const sint32 coYA   = mCoYA;
	const sint32 coCbCb = mCoCbCb;
	const sint32 coCbCr = mCoCbCr;
	const sint32 coCbA  = mCoCbA;
	const sint32 coCrCb = mCoCrCb;
	const sint32 coCrCr = mCoCrCr;
	const sint32 coCrA  = mCoCrA;

	for(sint32 i=0; i<mWidth; ++i) {
		sint32 y = srcY[i];
		sint32 cb = srcCb[i];
		sint32 cr = srcCr[i];

		sint32 y2  = y*coYY  + cb*coYCb  + cr*coYCr  + coYA;
		sint32 cb2 =           cb*coCbCb + cr*coCbCr + coCbA;
		sint32 cr2 =           cb*coCrCb + cr*coCrCr + coCrA;

		y2 &= ~y2 >> 31;
		cb2 &= ~cb2 >> 31;
		cr2 &= ~cr2 >> 31;

		y2 |= (0xffffff - y2) >> 31;
		cb2 |= (0xffffff - cb2) >> 31;
		cr2 |= (0xffffff - cr2) >> 31;

		*dstY++  = (uint8)(y2 >> 16);
		*dstCb++ = (uint8)(cb2 >> 16);
		*dstCr++ = (uint8)(cr2 >> 16);
	}
}

////////////////////////////////////////////////////////////////////////////

VDPixmapGenYCbCrToYCbCrGeneric_32F::VDPixmapGenYCbCrToYCbCrGeneric_32F(const VDPixmapGenYCbCrBasis& dstBasis, bool dstLimitedRange, const VDPixmapGenYCbCrBasis& srcBasis, bool srcLimitedRange, uint32 colorSpace)
	: mColorSpace(colorSpace)
{
	vdfloat3x3 dstToRGB;
	dstToRGB.m[0] = vdfloat3c(1, 1, 1);
	dstToRGB.m[1] = vdfloat3c(dstBasis.mToRGB[0]);
	dstToRGB.m[2] = vdfloat3c(dstBasis.mToRGB[1]);

	if (dstLimitedRange) {
		dstToRGB.m[0] *= (255.0f / 219.0f);
		dstToRGB.m[1] *= (255.0f / 224.0f);
		dstToRGB.m[2] *= (255.0f / 224.0f);
	}

	vdfloat3x3 srcToRGB;
	srcToRGB.m[0] = vdfloat3c(1, 1, 1);
	srcToRGB.m[1] = vdfloat3c(srcBasis.mToRGB[0]);
	srcToRGB.m[2] = vdfloat3c(srcBasis.mToRGB[1]);

	if (srcLimitedRange) {
		srcToRGB.m[0] *= (255.0f / 219.0f);
		srcToRGB.m[1] *= (255.0f / 224.0f);
		srcToRGB.m[2] *= (255.0f / 224.0f);
	}

	vdfloat3x3 xf(srcToRGB * ~dstToRGB);

	// We should get a transform that looks like this:
	//
	//	            |k 0 0|
	//	[y cb cr 1] |a c e| = [y' cb' cr]
	//	            |b d f|
	//				|x y z|

	VDASSERT(fabsf(xf.m[0].v[1]) < 1e-5f);
	VDASSERT(fabsf(xf.m[0].v[2]) < 1e-5f);

	mCoYY   = xf.m[0].v[0];
	mCoYCb  = xf.m[1].v[0];
	mCoYCr  = xf.m[2].v[0];
	mCoCbCb = xf.m[1].v[1];
	mCoCbCr = xf.m[2].v[1];
	mCoCrCb = xf.m[1].v[2];
	mCoCrCr = xf.m[2].v[2];

	vdfloat3c srcBias(0, 128.0f/255.0f, 128.0f/255.0f);
	if (srcLimitedRange)
		srcBias.set(16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f);

	vdfloat3c dstBias(0, 128.0f/255.0f, 128.0f/255.0f);
	if (dstLimitedRange)
		dstBias.set(16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f);

	vdfloat3 bias = -srcBias * xf + dstBias;

	mCoYA  = bias.x;
	mCoCbA = bias.y;
	mCoCrA = bias.z;
}

void VDPixmapGenYCbCrToYCbCrGeneric_32F::Start() {
	mpSrcY->Start();
	mpSrcCb->Start();
	mpSrcCr->Start();

	StartWindow(mWidth * sizeof(float), 3);
}

const void *VDPixmapGenYCbCrToYCbCrGeneric_32F::GetRow(sint32 y, uint32 index) {
	return (const uint8 *)VDPixmapGenYCbCrToRGBBase::GetRow(y, index) + mWindowPitch * index;
}

uint32 VDPixmapGenYCbCrToYCbCrGeneric_32F::GetType(uint32 output) const {
	return (mpSrcY->GetType(mSrcIndexY) & ~kVDPixSpace_Mask) | mColorSpace;
}

void VDPixmapGenYCbCrToYCbCrGeneric_32F::Compute(void *dst0, sint32 ypos) {
	float *dstCr = (float *)dst0;
	float *dstY  = vdptroffset(dstCr, mWindowPitch);
	float *dstCb = vdptroffset(dstY, mWindowPitch);

	const float *srcY  = (const float *)mpSrcY ->GetRow(ypos, mSrcIndexY );
	const float *srcCb = (const float *)mpSrcCb->GetRow(ypos, mSrcIndexCb);
	const float *srcCr = (const float *)mpSrcCr->GetRow(ypos, mSrcIndexCr);

	VDCPUCleanupExtensions();

	const float coYY   = mCoYY;
	const float coYCb  = mCoYCb;
	const float coYCr  = mCoYCr;
	const float coYA   = mCoYA;
	const float coCbCb = mCoCbCb;
	const float coCbCr = mCoCbCr;
	const float coCbA  = mCoCbA;
	const float coCrCb = mCoCrCb;
	const float coCrCr = mCoCrCr;
	const float coCrA  = mCoCrA;

	for(sint32 i=0; i<mWidth; ++i) {
		float y  = srcY [i];
		float cb = srcCb[i];
		float cr = srcCr[i];

		*dstY++  = y*coYY + cb*coYCb  + cr*coYCr  + coYA;
		*dstCb++ =          cb*coCbCb + cr*coCbCr + coCbA;
		*dstCr++ =          cb*coCrCb + cr*coCrCr + coCrA;
	}
}
