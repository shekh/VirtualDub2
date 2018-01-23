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
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "uberblit.h"
#include "uberblit_gen.h"
#include "uberblit_ycbcr_generic.h"

uint32 VDPixmapGetFormatTokenFromFormat(int format) {
	using namespace nsVDPixmap;
	switch(format) {
	case kPixFormat_Pal1:			return kVDPixType_1 | kVDPixSamp_444 | kVDPixSpace_Pal;
	case kPixFormat_Pal2:			return kVDPixType_2 | kVDPixSamp_444 | kVDPixSpace_Pal;
	case kPixFormat_Pal4:			return kVDPixType_4 | kVDPixSamp_444 | kVDPixSpace_Pal;
	case kPixFormat_Pal8:			return kVDPixType_8 | kVDPixSamp_444 | kVDPixSpace_Pal;
	case kPixFormat_XRGB1555:		return kVDPixType_1555_LE | kVDPixSamp_444 | kVDPixSpace_BGR;
	case kPixFormat_RGB565:			return kVDPixType_565_LE | kVDPixSamp_444 | kVDPixSpace_BGR;
	case kPixFormat_RGB888:			return kVDPixType_888 | kVDPixSamp_444 | kVDPixSpace_BGR;
	case kPixFormat_XRGB8888:		return kVDPixType_8888 | kVDPixSamp_444 | kVDPixSpace_BGR;
	case kPixFormat_Y8:				return kVDPixType_8 | kVDPixSamp_444 | kVDPixSpace_Y_601;
	case kPixFormat_YUV422_UYVY:	return kVDPixType_B8G8_R8G8 | kVDPixSamp_422 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV422_YUYV:	return kVDPixType_G8B8_G8R8 | kVDPixSamp_422 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV444_XVYU:	return kVDPixType_8888 | kVDPixSamp_444 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV444_Planar:	return kVDPixType_8_8_8 | kVDPixSamp_444 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV422_Planar:	return kVDPixType_8_8_8 | kVDPixSamp_422 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV422_Planar_16F:	return kVDPixType_16F_16F_16F_LE | kVDPixSamp_422 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV420_Planar:	return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG2 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV411_Planar:	return kVDPixType_8_8_8 | kVDPixSamp_411 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV410_Planar:	return kVDPixType_8_8_8 | kVDPixSamp_410 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV422_Planar_Centered:	return kVDPixType_8_8_8 | kVDPixSamp_422_JPEG | kVDPixSpace_YCC_601;
	case kPixFormat_YUV420_Planar_Centered:	return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG1 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV422_V210:	return kVDPixType_V210 | kVDPixSamp_422 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV422_UYVY_709:	return kVDPixType_B8G8_R8G8 | kVDPixSamp_422 | kVDPixSpace_YCC_709;
	case kPixFormat_YUV420_NV12:	return kVDPixType_8_B8R8 | kVDPixSamp_420_MPEG2 | kVDPixSpace_YCC_601;
	case kPixFormat_Y8_FR:				return kVDPixType_8 | kVDPixSamp_444 | kVDPixSpace_Y_601_FR;
	case kPixFormat_YUV422_YUYV_709:	return kVDPixType_G8B8_G8R8 | kVDPixSamp_422 | kVDPixSpace_YCC_709;
	case kPixFormat_YUV444_Planar_709:	return kVDPixType_8_8_8 | kVDPixSamp_444 | kVDPixSpace_YCC_709;
	case kPixFormat_YUV422_Planar_709:	return kVDPixType_8_8_8 | kVDPixSamp_422 | kVDPixSpace_YCC_709;
	case kPixFormat_YUV420_Planar_709:	return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG2 | kVDPixSpace_YCC_709;
	case kPixFormat_YUV411_Planar_709:	return kVDPixType_8_8_8 | kVDPixSamp_411 | kVDPixSpace_YCC_709;
	case kPixFormat_YUV410_Planar_709:	return kVDPixType_8_8_8 | kVDPixSamp_410 | kVDPixSpace_YCC_709;
	case kPixFormat_YUV422_UYVY_FR:		return kVDPixType_B8G8_R8G8 | kVDPixSamp_422 | kVDPixSpace_YCC_601_FR;
	case kPixFormat_YUV422_YUYV_FR:		return kVDPixType_G8B8_G8R8 | kVDPixSamp_422 | kVDPixSpace_YCC_601_FR;
	case kPixFormat_YUV444_Planar_FR:	return kVDPixType_8_8_8 | kVDPixSamp_444 | kVDPixSpace_YCC_601_FR;
	case kPixFormat_YUV422_Planar_FR:	return kVDPixType_8_8_8 | kVDPixSamp_422 | kVDPixSpace_YCC_601_FR;
	case kPixFormat_YUV420_Planar_FR:	return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG2 | kVDPixSpace_YCC_601_FR;
	case kPixFormat_YUV411_Planar_FR:	return kVDPixType_8_8_8 | kVDPixSamp_411 | kVDPixSpace_YCC_601_FR;
	case kPixFormat_YUV410_Planar_FR:	return kVDPixType_8_8_8 | kVDPixSamp_410 | kVDPixSpace_YCC_601_FR;
	case kPixFormat_YUV422_UYVY_709_FR:	return kVDPixType_B8G8_R8G8 | kVDPixSamp_422 | kVDPixSpace_YCC_709_FR;
	case kPixFormat_YUV422_YUYV_709_FR:	return kVDPixType_G8B8_G8R8 | kVDPixSamp_422 | kVDPixSpace_YCC_709_FR;
	case kPixFormat_YUV444_Planar_709_FR:	return kVDPixType_8_8_8 | kVDPixSamp_444 | kVDPixSpace_YCC_709_FR;
	case kPixFormat_YUV422_Planar_709_FR:	return kVDPixType_8_8_8 | kVDPixSamp_422 | kVDPixSpace_YCC_709_FR;
	case kPixFormat_YUV420_Planar_709_FR:	return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG2 | kVDPixSpace_YCC_709_FR;
	case kPixFormat_YUV411_Planar_709_FR:	return kVDPixType_8_8_8 | kVDPixSamp_411 | kVDPixSpace_YCC_709_FR;
	case kPixFormat_YUV410_Planar_709_FR:	return kVDPixType_8_8_8 | kVDPixSamp_410 | kVDPixSpace_YCC_709_FR;
	case kPixFormat_YUV420i_Planar:			return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG2INT | kVDPixSpace_YCC_601;
	case kPixFormat_YUV420i_Planar_FR:		return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG2INT | kVDPixSpace_YCC_601_FR;
	case kPixFormat_YUV420i_Planar_709:		return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG2INT | kVDPixSpace_YCC_709;
	case kPixFormat_YUV420i_Planar_709_FR:	return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG2INT | kVDPixSpace_YCC_709_FR;
	case kPixFormat_YUV420it_Planar:		return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG2INT1 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV420it_Planar_FR:		return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG2INT1 | kVDPixSpace_YCC_601_FR;
	case kPixFormat_YUV420it_Planar_709:	return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG2INT1 | kVDPixSpace_YCC_709;
	case kPixFormat_YUV420it_Planar_709_FR:	return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG2INT1 | kVDPixSpace_YCC_709_FR;
	case kPixFormat_YUV420ib_Planar:		return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG2INT2 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV420ib_Planar_FR:		return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG2INT2 | kVDPixSpace_YCC_601_FR;
	case kPixFormat_YUV420ib_Planar_709:	return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG2INT2 | kVDPixSpace_YCC_709;
	case kPixFormat_YUV420ib_Planar_709_FR:	return kVDPixType_8_8_8 | kVDPixSamp_420_MPEG2INT2 | kVDPixSpace_YCC_709_FR;
	case kPixFormat_XRGB64:		return kVDPixType_16x4_LE | kVDPixSamp_444 | kVDPixSpace_BGR;
	case kPixFormat_YUV444_Planar16:	return kVDPixType_16_16_16_LE | kVDPixSamp_444 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV422_Planar16:	return kVDPixType_16_16_16_LE | kVDPixSamp_422 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV420_Planar16:	return kVDPixType_16_16_16_LE | kVDPixSamp_420_MPEG2 | kVDPixSpace_YCC_601;
	case kPixFormat_Y16:				return kVDPixType_16_LE | kVDPixSamp_444 | kVDPixSpace_Y_601;
	case kPixFormat_YUVA444_Y416:		return kVDPixType_16x4_LE | kVDPixSamp_444 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV444_V410:		return kVDPixType_V410 | kVDPixSamp_444 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV444_Y410:		return kVDPixType_Y410 | kVDPixSamp_444 | kVDPixSpace_YCC_601;
	case kPixFormat_R210:				return kVDPixType_R210 | kVDPixSamp_444 | kVDPixSpace_BGR;
	case kPixFormat_R10K:				return kVDPixType_R10K | kVDPixSamp_444 | kVDPixSpace_BGR;
	case kPixFormat_YUV444_V308:		return kVDPixType_V308 | kVDPixSamp_444 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV422_P210:
	case kPixFormat_YUV422_P216:
		return kVDPixType_16_16x2_LE | kVDPixSamp_422 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV420_P010:
	case kPixFormat_YUV420_P016:
		return kVDPixType_16_16x2_LE | kVDPixSamp_420_MPEG2 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV444_Alpha_Planar:	return kVDPixType_8_8_8_8 | kVDPixSamp_444 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV422_Alpha_Planar:	return kVDPixType_8_8_8_8 | kVDPixSamp_422 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV420_Alpha_Planar:	return kVDPixType_8_8_8_8 | kVDPixSamp_420_MPEG2 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV444_Alpha_Planar16:	return kVDPixType_16_16_16_16_LE | kVDPixSamp_444 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV422_Alpha_Planar16:	return kVDPixType_16_16_16_16_LE | kVDPixSamp_422 | kVDPixSpace_YCC_601;
	case kPixFormat_YUV420_Alpha_Planar16:	return kVDPixType_16_16_16_16_LE | kVDPixSamp_420_MPEG2 | kVDPixSpace_YCC_601;
	default:
		VDASSERT(false);
		return 0;
	}
}

const VDPixmapSamplingInfo& VDPixmapGetSamplingInfo(uint32 samplingToken) {
	static const VDPixmapSamplingInfo kPixmapSamplingInfo[]={
		/* Null			*/ { false, {  0,  0, 0, 0 }, {  0,  0, 0, 0 } },
		/* 444			*/ { false, {  0,  0, 0, 0 }, {  0,  0, 0, 0 } },
		/* 422			*/ { false, { -4,  0, 1, 0 }, { -4,  0, 1, 0 } },
		/* 422_JPEG		*/ { false, {  0,  0, 1, 0 }, {  0,  0, 1, 0 } },
		/* 420_MPEG2	*/ { false, { -4,  0, 1, 1 }, { -4,  0, 1, 1 } },
		/* 420_MPEG2INT	*/ { true , { -4, -2, 1, 1 }, { -4, -2, 1, 1 }, { -4, +2, 1, 1 }, { -4, +2, 1, 1 } },
		/* 420_MPEG2INT1*/ { false, { -4, -2, 1, 1 }, { -4, -2, 1, 1 } },
		/* 420_MPEG2INT2*/ { false, { -4, +2, 1, 1 }, { -4, +2, 1, 1 } },
		/* 420_MPEG1	*/ { false, {  0,  0, 1, 1 }, {  0,  0, 1, 1 } },
		/* 420_DVPAL	*/ { true , { -4,  0, 1, 1 }, { -4,  0, 1, 1 } },
		/* 411			*/ { false, { -6,  0, 2, 0 }, { -6,  0, 2, 0 } },
		/* 410			*/ { false, { -6,  0, 2, 2 }, { -6,  0, 2, 2 } },
	};

	uint32 index = (samplingToken & kVDPixSamp_Mask) >> kVDPixSamp_Bits;

	return index >= sizeof(kPixmapSamplingInfo)/sizeof(kPixmapSamplingInfo[0]) ? kPixmapSamplingInfo[0] : kPixmapSamplingInfo[index];
}

namespace {
	uint32 GetChromaPlaneBpr(uint32 w, uint32 srcToken) {
		switch(srcToken & kVDPixType_Mask) {
			case kVDPixType_1:
			case kVDPixType_2:
			case kVDPixType_4:
			case kVDPixType_8:
			case kVDPixType_555_LE:
			case kVDPixType_565_LE:
			case kVDPixType_1555_LE:
			case kVDPixType_16F_LE:
			case kVDPixType_16_LE:
			case kVDPixType_888:
			case kVDPixType_8888:
			case kVDPixType_16Fx4_LE:
			case kVDPixType_32F_LE:
			case kVDPixType_32Fx4_LE:
			case kVDPixType_B8G8_R8G8:
			case kVDPixType_G8B8_G8R8:
			case kVDPixType_V210:
			case kVDPixType_V410:
			case kVDPixType_Y410:
			case kVDPixType_V308:
			case kVDPixType_8_B8R8:
			case kVDPixType_B8R8:
			case kVDPixType_16x4_LE:
			case kVDPixType_R210:
			case kVDPixType_R10K:
			default:
				return 0;

			case kVDPixType_8_8_8:
			case kVDPixType_8_8_8_8:
				return w;

			case kVDPixType_16F_16F_16F_LE:
			case kVDPixType_16_16_16_LE:
			case kVDPixType_16_16_16_16_LE:
				return w*2;

			case kVDPixType_32F_32F_32F_LE:
				return w*4;
		}
	}

	void BlitterConvertSampling(VDPixmapUberBlitterGenerator& gen, const VDPixmapSamplingInfo& dstInfo, const VDPixmapSamplingInfo& srcInfo, sint32 cw, sint32 ch);
	void BlitterConvertPlaneSampling(VDPixmapUberBlitterGenerator& gen, const VDPixmapPlaneSamplingInfo& dstInfo, const VDPixmapPlaneSamplingInfo& srcInfo, sint32 cw, sint32 ch);

	uint32 BlitterConvertSampling(VDPixmapUberBlitterGenerator& gen, uint32 srcToken, uint32 dstSamplingToken, sint32 w, sint32 h) {
		// if the source type is 16F, we have to convert to 32F
		if ((srcToken & kVDPixType_Mask) == kVDPixType_16F_16F_16F_LE) {
			// 0 1 2
			gen.conv_16F_to_32F();
			gen.swap(1);
			// 1 0 2
			gen.conv_16F_to_32F();
			gen.swap(2);
			// 2 0 1
			gen.conv_16F_to_32F();
			gen.swap(2);
			gen.swap(1);
			srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_32F_32F_32F_LE;
		}

		// look up sampling info
		const VDPixmapSamplingInfo& srcInfo = VDPixmapGetSamplingInfo(srcToken);
		const VDPixmapSamplingInfo& dstInfo = VDPixmapGetSamplingInfo(dstSamplingToken);

		// Check if we have an interlacing mismatch. If so, then we have to convert up to
		// full resolution vertically in order to split or merge fields.
		const sint32 cw = -(-w >> dstInfo.mPlane1Cr.mXBits);
		const sint32 ch = -(-h >> dstInfo.mPlane1Cr.mYBits);
		const uint32 cbpr = GetChromaPlaneBpr(cw, srcToken);

		if (dstInfo.mbInterlaced || srcInfo.mbInterlaced) {
			const sint32 src_cw = -(-w >> srcInfo.mPlane1Cr.mXBits);

			const sint32 ch1 = (ch + 1) >> 1;
			const sint32 ch2 = ch >> 1;

			if (dstInfo.mbInterlaced) {
				if (srcInfo.mbInterlaced) {
					// interlaced -> interlaced: split fields, resample, merge fields
					//
					// cr y cb
					gen.split_fields(cbpr);

					// cr-odd cr-even y cb
					BlitterConvertPlaneSampling(gen, dstInfo.mPlane2Cr, srcInfo.mPlane2Cr, cw, ch2);

					// cr-odd' cr-even y cb
					gen.swap(1);

					// cr-even cr-odd' y cb
					BlitterConvertPlaneSampling(gen, dstInfo.mPlane1Cr, srcInfo.mPlane1Cr, cw, ch1);

					// cr-even' cr-odd' y cb
					gen.swap(1);

					// cr-odd' cr-even' y cb
					gen.merge_fields(cw, ch, cbpr);

					// cr' y cb
					gen.swap(2);

					// cb' y cr'
					gen.split_fields(cbpr);

					// cb-odd cb-even y cr'
					BlitterConvertPlaneSampling(gen, dstInfo.mPlane2Cb, srcInfo.mPlane2Cb, cw, ch2);

					// cb-odd' cb-even y cr'
					gen.swap(1);

					// cb-even cb-odd' y cr'
					BlitterConvertPlaneSampling(gen, dstInfo.mPlane1Cb, srcInfo.mPlane1Cb, cw, ch1);

					// cb-even' cb-odd' y cr'
					gen.swap(1);

					// cb-odd' cb-even' y cr'
					gen.merge_fields(cw, ch, cbpr);

					// cb' y cr'
					gen.swap(2);

					// cr' y cb'
				} else {
					// non-interlaced -> interlaced
					VDPixmapPlaneSamplingInfo crPlaneInt(srcInfo.mPlane1Cr);
					VDPixmapPlaneSamplingInfo crPlaneFieldInt(srcInfo.mPlane1Cr);
					VDPixmapPlaneSamplingInfo cbPlaneInt(srcInfo.mPlane1Cb);
					VDPixmapPlaneSamplingInfo cbPlaneFieldInt(srcInfo.mPlane1Cb);

					crPlaneInt.mX = dstInfo.mPlane1Cr.mX;
					crPlaneInt.mXBits = dstInfo.mPlane1Cr.mXBits;
					crPlaneInt.mY = 0;
					crPlaneInt.mYBits = 0;
					crPlaneFieldInt.mX = dstInfo.mPlane1Cr.mX;
					crPlaneFieldInt.mXBits = dstInfo.mPlane1Cr.mXBits;
					crPlaneFieldInt.mY = 0;
					crPlaneFieldInt.mYBits = 0;

					cbPlaneInt.mX = dstInfo.mPlane1Cb.mX;
					cbPlaneInt.mXBits = dstInfo.mPlane1Cb.mXBits;
					cbPlaneFieldInt.mX = dstInfo.mPlane1Cb.mX;
					cbPlaneFieldInt.mXBits = dstInfo.mPlane1Cb.mXBits;
					cbPlaneFieldInt.mY = 0;
					cbPlaneFieldInt.mYBits = 0;

					// cr y cb
					BlitterConvertPlaneSampling(gen, crPlaneInt, srcInfo.mPlane1Cr, cw, h);

					// cr' y cb
					gen.split_fields(cbpr);
					BlitterConvertPlaneSampling(gen, dstInfo.mPlane1Cr, crPlaneFieldInt, cw, ch1);
					gen.swap(1);
					BlitterConvertPlaneSampling(gen, dstInfo.mPlane2Cr, crPlaneFieldInt, cw, ch2);
					gen.swap(1);
					gen.merge_fields(cw, ch, cbpr);

					gen.swap(2);
					BlitterConvertPlaneSampling(gen, cbPlaneInt, srcInfo.mPlane1Cb, cw, h);
					gen.split_fields(cbpr);
					gen.swap(1);
					BlitterConvertPlaneSampling(gen, dstInfo.mPlane1Cb, cbPlaneFieldInt, cw, ch1);
					gen.swap(1);
					BlitterConvertPlaneSampling(gen, dstInfo.mPlane1Cb, cbPlaneFieldInt, cw, ch2);
					gen.merge_fields(cw, ch, cbpr);
					gen.swap(2);
				}
			} else {
				sint32 src_cbpr = src_cw;

				// interlaced -> non-interlaced
				VDPixmapPlaneSamplingInfo crPlaneFieldInt(srcInfo.mPlane1Cr);
				VDPixmapPlaneSamplingInfo crPlaneInt(srcInfo.mPlane1Cr);
				VDPixmapPlaneSamplingInfo cbPlaneFieldInt(srcInfo.mPlane1Cb);
				VDPixmapPlaneSamplingInfo cbPlaneInt(srcInfo.mPlane1Cb);

				crPlaneFieldInt.mY = 0;
				crPlaneFieldInt.mYBits = 0;
				crPlaneInt.mY = 0;
				crPlaneInt.mYBits = 0;
				cbPlaneFieldInt.mY = 0;
				cbPlaneFieldInt.mYBits = 0;
				cbPlaneInt.mY = 0;
				cbPlaneInt.mYBits = 0;

				// cr y cb
				gen.split_fields(src_cbpr);
				BlitterConvertPlaneSampling(gen, crPlaneFieldInt, srcInfo.mPlane1Cr, src_cw, (h + 1) >> 1);
				gen.swap(1);
				BlitterConvertPlaneSampling(gen, crPlaneFieldInt, srcInfo.mPlane2Cr, src_cw, h >> 1);
				gen.swap(1);
				gen.merge_fields(src_cw, h, src_cbpr);
				BlitterConvertPlaneSampling(gen, dstInfo.mPlane1Cr, crPlaneInt, cw, ch);
				gen.swap(2);

				// cr' y cb
				gen.split_fields(src_cbpr);
				BlitterConvertPlaneSampling(gen, cbPlaneFieldInt, srcInfo.mPlane1Cb, src_cw, (h + 1) >> 1);
				gen.swap(1);
				BlitterConvertPlaneSampling(gen, cbPlaneFieldInt, srcInfo.mPlane2Cb, src_cw, h >> 1);
				gen.swap(1);
				gen.merge_fields(src_cw, h, src_cbpr);
				BlitterConvertPlaneSampling(gen, dstInfo.mPlane1Cb, cbPlaneInt, cw, ch);
				gen.swap(2);
			}
		} else {
			// non-interlaced -> non-interlaced
			BlitterConvertSampling(gen, dstInfo, srcInfo, cw, ch);
		}

		return (srcToken & ~kVDPixSamp_Mask) | (dstSamplingToken & kVDPixSamp_Mask);
	}

	void BlitterConvertSampling(VDPixmapUberBlitterGenerator& gen, const VDPixmapSamplingInfo& dstInfo, const VDPixmapSamplingInfo& srcInfo, sint32 cw, sint32 ch) {
		gen.swap(2);
		BlitterConvertPlaneSampling(gen, dstInfo.mPlane1Cb, srcInfo.mPlane1Cb, cw, ch);
		gen.swap(2);
		BlitterConvertPlaneSampling(gen, dstInfo.mPlane1Cr, srcInfo.mPlane1Cr, cw, ch);
	}

	void BlitterConvertPlaneSampling(VDPixmapUberBlitterGenerator& gen, const VDPixmapPlaneSamplingInfo& dstInfo, const VDPixmapPlaneSamplingInfo& srcInfo, sint32 cw, sint32 ch) {
		// convert destination chroma origin to luma space
		int c_x = ((8 + dstInfo.mX) << dstInfo.mXBits) - 8;
		int c_y = ((8 + dstInfo.mY) << dstInfo.mYBits) - 8;

		// convert luma chroma location to source chroma space
		c_x = ((8 + c_x) >> srcInfo.mXBits) - 8 - srcInfo.mX;
		c_y = ((8 + c_y) >> srcInfo.mYBits) - 8 - srcInfo.mY;

		float cxo = c_x / 16.0f + 0.5f;
		float cxf = ((16 << dstInfo.mXBits) >> srcInfo.mXBits) / 16.0f;
		float cyf = ((16 << dstInfo.mYBits) >> srcInfo.mYBits) / 16.0f;

		gen.linear(cxo, cxf, cw, c_y / 16.0f + 0.5f, cyf, ch);
	}

	uint32 BlitterConvertType(VDPixmapUberBlitterGenerator& gen, uint32 srcToken, uint32 dstToken, sint32 w, sint32 h) {
		uint32 dstType = dstToken & kVDPixType_Mask;

		while((srcToken ^ dstToken) & kVDPixType_Mask) {
			uint32 srcType = srcToken & kVDPixType_Mask;
			uint32 targetType = dstType;

	type_reconvert:
			switch(targetType) {
				case kVDPixType_1555_LE:
					switch(srcType) {
						case kVDPixType_565_LE:
							gen.conv_565_to_555();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_1555_LE;
							break;

						case kVDPixType_8888:
							gen.conv_8888_to_555();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_1555_LE;
							break;
						case kVDPixType_B8G8_R8G8:
						case kVDPixType_G8B8_G8R8:
							targetType = kVDPixType_8_8_8;
							goto type_reconvert;
						default:
							targetType = kVDPixType_8888;
							goto type_reconvert;
					}
					break;

				case kVDPixType_565_LE:
					switch(srcType) {
						case kVDPixType_1555_LE:
							gen.conv_555_to_565();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_565_LE;
							break;
						case kVDPixType_8888:
							gen.conv_8888_to_565();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_565_LE;
							break;
						case kVDPixType_B8G8_R8G8:
						case kVDPixType_G8B8_G8R8:
							targetType = kVDPixType_8_8_8;
							goto type_reconvert;
						default:
							targetType = kVDPixType_8888;
							goto type_reconvert;
					}
					break;

				case kVDPixType_888:
					switch(srcType) {
						case kVDPixType_8888:
							gen.conv_8888_to_888();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_888;
							break;
						default:
							targetType = kVDPixType_8888;
							goto type_reconvert;
					}
					break;

				case kVDPixType_8888:
					switch(srcType) {
						case kVDPixType_1555_LE:
							gen.conv_555_to_8888();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8888;
							break;
						case kVDPixType_565_LE:
							gen.conv_565_to_8888();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8888;
							break;
						case kVDPixType_888:
							gen.conv_888_to_8888();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8888;
							break;
						case kVDPixType_32Fx4_LE:
							gen.conv_X32F_to_8888();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8888;
							break;
						case kVDPixType_8_8_8:
							if ((srcToken & kVDPixSamp_Mask) != kVDPixSamp_444)
								srcToken = BlitterConvertSampling(gen, srcToken, kVDPixSamp_444, w, h);
							gen.interleave_X8R8G8B8();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8888;
							break;
						case kVDPixType_16x4_LE:
							gen.conv_X16_to_8888();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8888;
							break;
						case kVDPixType_R210:
						case kVDPixType_R10K:
							targetType = kVDPixType_16x4_LE;
							goto type_reconvert;
						default:
							VDASSERT(false);
							break;
					}
					break;

				case kVDPixType_8:
					switch(srcType) {
						case kVDPixType_8_8_8:
							gen.pop();
							gen.swap(1);
							gen.pop();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8;
							break;

						case kVDPixType_16F_LE:
							targetType = kVDPixType_32F_LE;
							goto type_reconvert;

						case kVDPixType_16_LE:
							gen.conv_16_to_8();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8;
							break;

						case kVDPixType_32F_LE:
							gen.conv_32F_to_8();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8;
							break;

						default:
							targetType = kVDPixType_8_8_8;
							goto type_reconvert;
					}
					break;

				case kVDPixType_16_LE:
					switch(srcType) {
						case kVDPixType_16_16_16_LE:
							gen.pop();
							gen.swap(1);
							gen.pop();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_LE;
							break;

						case kVDPixType_16F_LE:
							targetType = kVDPixType_32F_LE;
							goto type_reconvert;

						case kVDPixType_8:
							gen.conv_8_to_16();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_LE;
							break;

						case kVDPixType_32F_LE:
							gen.conv_32F_to_16();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_LE;
							break;

						default:
							targetType = kVDPixType_16_16_16_LE;
							goto type_reconvert;
					}
					break;

				case kVDPixType_8_8_8:
					switch(srcType) {
						case kVDPixType_B8G8_R8G8:
							gen.dup();
							gen.dup();
							gen.extract_8in32(2, (w + 1) >> 1, h);
							gen.swap(2);
							gen.extract_8in16(1, w, h);
							gen.swap(1);
							gen.extract_8in32(0, (w + 1) >> 1, h);
							srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSamp_Mask)) | kVDPixType_8_8_8 | kVDPixSamp_422;
							break;
						case kVDPixType_G8B8_G8R8:
							gen.dup();
							gen.dup();
							gen.extract_8in32(3, (w + 1) >> 1, h);
							gen.swap(2);
							gen.extract_8in16(0, w, h);
							gen.swap(1);
							gen.extract_8in32(1, (w + 1) >> 1, h);
							srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSamp_Mask)) | kVDPixType_8_8_8 | kVDPixSamp_422;
							break;
						case kVDPixType_V308:
							gen.conv_V308_to_P8();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8_8_8;
							break;
						case kVDPixType_16F_16F_16F_LE:
						case kVDPixType_V210:
						case kVDPixType_V410:
						case kVDPixType_Y410:
							targetType = kVDPixType_32F_32F_32F_LE;
							goto type_reconvert;
						case kVDPixType_32F_32F_32F_LE:
							// 0 1 2
							gen.conv_32F_to_8();
							gen.swap(1);
							// 1 0 2
							gen.conv_32F_to_8();
							gen.swap(2);
							// 2 0 1
							gen.conv_32F_to_8();
							gen.swap(2);
							gen.swap(1);
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8_8_8;
							break;
						case kVDPixType_16_16_16_LE:
							// 0 1 2
							gen.conv_16_to_8();
							gen.swap(1);
							// 1 0 2
							gen.conv_16_to_8();
							gen.swap(2);
							// 2 0 1
							gen.conv_16_to_8();
							gen.swap(2);
							gen.swap(1);
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8_8_8;
							break;
						case kVDPixType_16x4_LE:
							targetType = kVDPixType_16_16_16_LE;
							goto type_reconvert;
							break;
						case kVDPixType_8_B8R8:
							{
								const VDPixmapSamplingInfo& sampInfo = VDPixmapGetSamplingInfo(srcToken);
								int cw = -(-w >> sampInfo.mPlane1Cr.mXBits);
								int ch = -(-h >> sampInfo.mPlane1Cr.mYBits);

								gen.dup();
								gen.extract_8in16(1, cw, ch);
								gen.swap(2);
								gen.swap(1);
								gen.extract_8in16(0, cw, ch);
								srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8_8_8;
							}
							break;
						default:
							VDASSERT(false);
							break;
					}
					break;

				case kVDPixType_B8G8_R8G8:
					switch(srcType) {
					case kVDPixType_8_8_8:
						if ((srcToken ^ dstToken) & kVDPixSamp_Mask)
							srcToken = BlitterConvertSampling(gen, srcToken, dstToken, w, h);

						gen.interleave_B8G8_R8G8();
						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSamp_Mask)) | kVDPixType_B8G8_R8G8;
						break;
					case kVDPixType_G8B8_G8R8:
						gen.swap_8in16(w, h, ((w + 1) & ~1)*2);
						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSamp_Mask)) | kVDPixType_B8G8_R8G8;
						break;
					default:
						targetType = kVDPixType_8_8_8;
						goto type_reconvert;
					}
					break;

				case kVDPixType_G8B8_G8R8:
					switch(srcType) {
					case kVDPixType_8_8_8:
						if ((srcToken ^ dstToken) & kVDPixSamp_Mask)
							srcToken = BlitterConvertSampling(gen, srcToken, dstToken, w, h);

						gen.interleave_G8B8_G8R8();
						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSamp_Mask)) | kVDPixType_G8B8_G8R8;
						break;
					case kVDPixType_B8G8_R8G8:
						gen.swap_8in16(w, h, ((w + 1) & ~1)*2);
						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSamp_Mask)) | kVDPixType_G8B8_G8R8;
						break;
					default:
						targetType = kVDPixType_8_8_8;
						goto type_reconvert;
					}
					break;

				case kVDPixType_16F_16F_16F_LE:
					switch(srcType) {
						case kVDPixType_32F_32F_32F_LE:
							// 0 1 2
							gen.conv_32F_to_16F();
							gen.swap(1);
							// 1 0 2
							gen.conv_32F_to_16F();
							gen.swap(2);
							// 2 0 1
							gen.conv_32F_to_16F();
							gen.swap(2);
							gen.swap(1);
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16F_16F_16F_LE;
							break;

						default:
							targetType = kVDPixType_32F_32F_32F_LE;
							goto type_reconvert;
					}
					break;

				case kVDPixType_16_16x2_LE:
					switch(srcType) {
						case kVDPixType_16_16_16_LE:
							gen.swap(1);
							gen.swap(2);
							gen.interleave_B16R16();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_16x2_LE;
							break;

						default:
							targetType = kVDPixType_16_16_16_LE;
							goto type_reconvert;
					}
					break;

				case kVDPixType_16_16_16_LE:
					switch(srcType) {
						case kVDPixType_8_8_8:
							// 0 1 2
							gen.conv_8_to_16();
							gen.swap(1);
							// 1 0 2
							gen.conv_8_to_16();
							gen.swap(2);
							// 2 0 1
							gen.conv_8_to_16();
							gen.swap(2);
							gen.swap(1);
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_16_16_LE;
							break;

						case kVDPixType_16x4_LE:
							gen.dup();
							gen.dup();
							gen.extract_16in64(3, w, h);
							gen.swap(2);
							gen.extract_16in64(1, w, h);
							gen.swap(1);
							gen.extract_16in64(2, w, h);
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_16_16_LE;
							break;

						case kVDPixType_32F_32F_32F_LE:
							// 0 1 2
							gen.conv_32F_to_16();
							gen.swap(1);
							// 1 0 2
							gen.conv_32F_to_16();
							gen.swap(2);
							// 2 0 1
							gen.conv_32F_to_16();
							gen.swap(2);
							gen.swap(1);
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_16_16_LE;
							break;

						case kVDPixType_V210:
							gen.conv_V210_to_P16();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_16_16_LE;
							break;

						case kVDPixType_V410:
							gen.conv_V410_to_P16();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_16_16_LE;
							break;

						case kVDPixType_Y410:
							gen.conv_Y410_to_P16();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_16_16_LE;
							break;

						default:
							targetType = kVDPixType_32F_32F_32F_LE;
							goto type_reconvert;
					}
					break;

				case kVDPixType_32F_32F_32F_LE:
					switch(srcType) {
						case kVDPixType_8_8_8:
							// 0 1 2
							gen.conv_8_to_32F();
							gen.swap(1);
							// 1 0 2
							gen.conv_8_to_32F();
							gen.swap(2);
							// 2 0 1
							gen.conv_8_to_32F();
							gen.swap(2);
							gen.swap(1);
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_32F_32F_32F_LE;
							break;

						case kVDPixType_16F_16F_16F_LE:
							// 0 1 2
							gen.conv_16F_to_32F();
							gen.swap(1);
							// 1 0 2
							gen.conv_16F_to_32F();
							gen.swap(2);
							// 2 0 1
							gen.conv_16F_to_32F();
							gen.swap(2);
							gen.swap(1);
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_32F_32F_32F_LE;
							break;

						case kVDPixType_16_16_16_LE:
							// 0 1 2
							gen.conv_16_to_32F();
							gen.swap(1);
							// 1 0 2
							gen.conv_16_to_32F();
							gen.swap(2);
							// 2 0 1
							gen.conv_16_to_32F();
							gen.swap(2);
							gen.swap(1);
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_32F_32F_32F_LE;
							break;

						case kVDPixType_16x4_LE:
							targetType = kVDPixType_16_16_16_LE;
							goto type_reconvert;

						case kVDPixType_B8G8_R8G8:
						case kVDPixType_G8B8_G8R8:
						case kVDPixType_8_B8R8:
						case kVDPixType_V308:
							targetType = kVDPixType_8_8_8;
							goto type_reconvert;

						case kVDPixType_V210:
							gen.conv_V210_to_32F();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_32F_32F_32F_LE;
							break;

						case kVDPixType_V410:
							gen.conv_V410_to_32F();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_32F_32F_32F_LE;
							break;

						case kVDPixType_Y410:
							gen.conv_Y410_to_32F();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_32F_32F_32F_LE;
							break;

						default:
							VDASSERT(false);
					}
					break;

				case kVDPixType_V308:
					switch(srcType) {
						case kVDPixType_8_8_8:
							if ((srcToken & kVDPixSamp_Mask) != kVDPixSamp_444)
								srcToken = BlitterConvertSampling(gen, srcToken, kVDPixSamp_444, w, h);

							gen.conv_P8_to_V308();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_V308;
							break;

						default:
							targetType = kVDPixType_8_8_8;
							goto type_reconvert;
					}
					break;

				case kVDPixType_V210:
					switch(srcType) {
						case kVDPixType_32F_32F_32F_LE:
							if ((srcToken & kVDPixSamp_Mask) != kVDPixSamp_422)
								srcToken = BlitterConvertSampling(gen, srcToken, kVDPixSamp_422, w, h);

							gen.conv_32F_to_V210();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_V210;
							break;

						case kVDPixType_16F_16F_16F_LE:
						case kVDPixType_16_16_16_LE:
						case kVDPixType_16x4_LE:
							targetType = kVDPixType_32F_32F_32F_LE;
							goto type_reconvert;

						case kVDPixType_8_8_8:
							if ((srcToken & kVDPixSamp_Mask) != kVDPixSamp_422)
								srcToken = BlitterConvertSampling(gen, srcToken, kVDPixSamp_422, w, h);

							targetType = kVDPixType_32F_32F_32F_LE;
							goto type_reconvert;

						case kVDPixType_B8G8_R8G8:
						case kVDPixType_G8B8_G8R8:
						case kVDPixType_8_B8R8:
							targetType = kVDPixType_8_8_8;
							goto type_reconvert;

						default:
							targetType = kVDPixType_32F_32F_32F_LE;
							goto type_reconvert;
					}
					break;

				case kVDPixType_V410:
					switch(srcType) {
						case kVDPixType_32F_32F_32F_LE:
							if ((srcToken & kVDPixSamp_Mask) != kVDPixSamp_444)
								srcToken = BlitterConvertSampling(gen, srcToken, kVDPixSamp_444, w, h);

							gen.conv_32F_to_V410();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_V410;
							break;

						case kVDPixType_16F_16F_16F_LE:
						case kVDPixType_16_16_16_LE:
						case kVDPixType_16x4_LE:
							targetType = kVDPixType_32F_32F_32F_LE;
							goto type_reconvert;

						case kVDPixType_8_8_8:
							if ((srcToken & kVDPixSamp_Mask) != kVDPixSamp_444)
								srcToken = BlitterConvertSampling(gen, srcToken, kVDPixSamp_444, w, h);

							targetType = kVDPixType_32F_32F_32F_LE;
							goto type_reconvert;

						case kVDPixType_B8G8_R8G8:
						case kVDPixType_G8B8_G8R8:
						case kVDPixType_8_B8R8:
							targetType = kVDPixType_8_8_8;
							goto type_reconvert;

						default:
							targetType = kVDPixType_32F_32F_32F_LE;
							goto type_reconvert;
					}
					break;

				case kVDPixType_Y410:
					switch(srcType) {
						case kVDPixType_32F_32F_32F_LE:
							if ((srcToken & kVDPixSamp_Mask) != kVDPixSamp_444)
								srcToken = BlitterConvertSampling(gen, srcToken, kVDPixSamp_444, w, h);

							gen.conv_32F_to_Y410();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_Y410;
							break;

						case kVDPixType_16F_16F_16F_LE:
						case kVDPixType_16_16_16_LE:
						case kVDPixType_16x4_LE:
							targetType = kVDPixType_32F_32F_32F_LE;
							goto type_reconvert;

						case kVDPixType_8_8_8:
							if ((srcToken & kVDPixSamp_Mask) != kVDPixSamp_444)
								srcToken = BlitterConvertSampling(gen, srcToken, kVDPixSamp_444, w, h);

							targetType = kVDPixType_32F_32F_32F_LE;
							goto type_reconvert;

						case kVDPixType_B8G8_R8G8:
						case kVDPixType_G8B8_G8R8:
						case kVDPixType_8_B8R8:
							targetType = kVDPixType_8_8_8;
							goto type_reconvert;

						default:
							targetType = kVDPixType_32F_32F_32F_LE;
							goto type_reconvert;
					}
					break;

				case kVDPixType_32F_LE:
					switch(srcType) {
						case kVDPixType_8:
							gen.conv_8_to_32F();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_32F_LE;
							break;
						case kVDPixType_16F_LE:
							gen.conv_16F_to_32F();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_32F_LE;
							break;
						case kVDPixType_16_LE:
							gen.conv_16_to_32F();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_32F_LE;
							break;
						default:
							VDASSERT(false);
					}
					break;

				case kVDPixType_8_B8R8:
					switch(srcType) {
						case kVDPixType_8_8_8:
							gen.swap(1);
							gen.swap(2);
							gen.interleave_B8R8();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8_B8R8;
							break;
						case kVDPixType_16_16_16_LE:
						case kVDPixType_32F_32F_32F_LE:
							targetType = kVDPixType_8_8_8;
							goto type_reconvert;
						default:
							VDASSERT(false);
							break;
					}
					break;

				case kVDPixType_32Fx4_LE:
					switch(srcType) {
						case kVDPixType_8888:
							gen.conv_8888_to_X32F();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_32Fx4_LE;
							break;
						default:
							targetType = kVDPixType_8888;
							goto type_reconvert;
					}
					break;

				case kVDPixType_16x4_LE:
					switch(srcType) {
						case kVDPixType_8888:
							gen.conv_8888_to_X16();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16x4_LE;
							break;
						case kVDPixType_32Fx4_LE:
							gen.conv_X32F_to_X16();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16x4_LE;
							break;
						case kVDPixType_16_16_16_LE:
							if ((srcToken & kVDPixSamp_Mask) != kVDPixSamp_444)
								srcToken = BlitterConvertSampling(gen, srcToken, kVDPixSamp_444, w, h);
							gen.interleave_Y416();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16x4_LE;
							break;
						case kVDPixType_1:
						case kVDPixType_2:
						case kVDPixType_4:
						case kVDPixType_8:
						case kVDPixType_555_LE:
						case kVDPixType_565_LE:
						case kVDPixType_1555_LE:
						case kVDPixType_888:
							targetType = kVDPixType_8888;
							goto type_reconvert;
						case kVDPixType_8_8_8:
						case kVDPixType_32F_32F_32F_LE:
						default:
							targetType = kVDPixType_16_16_16_LE;
							goto type_reconvert;
					}
					break;

				case kVDPixType_R210:
					switch(srcType) {
						case kVDPixType_16x4_LE:
							gen.conv_X16_to_R210();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_R210;
							break;
						default:
							targetType = kVDPixType_16x4_LE;
							goto type_reconvert;
					}
					break;

				case kVDPixType_R10K:
					switch(srcType) {
						case kVDPixType_16x4_LE:
							gen.conv_X16_to_R10K();
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_R10K;
							break;
						default:
							targetType = kVDPixType_16x4_LE;
							goto type_reconvert;
					}
					break;

				default:
					VDASSERT(false);
					break;
			}
		}

		return srcToken;
	}
}

int BlitterSplitAlpha(VDPixmapUberBlitterGenerator& gen, const VDPixmapLayout& src, uint32& srcToken, int alpha_type) {
	int w = src.w;
	int h = src.h;

	switch(srcToken & kVDPixType_Mask) {
	case kVDPixType_8888:
		gen.dup();
		gen.extract_8in32(3, w, h, true);
		if (alpha_type==kVDPixType_16_LE) gen.conv_a8_to_a16();
		gen.swap(1);
		return 0;

	case kVDPixType_16x4_LE:
		gen.dup();
		gen.extract_16in64(3, w, h, true);
		if (alpha_type==kVDPixType_8) gen.conv_a16_to_a8();
		gen.swap(1);
		return 0;

	case kVDPixType_8_8_8_8:
		gen.move_to_end(0);
		if (alpha_type==kVDPixType_16_LE) gen.conv_a8_to_a16();
		gen.move_to(0);
		srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8_8_8;
		return 0;

	case kVDPixType_16_16_16_16_LE:
		gen.move_to_end(0);
		if (alpha_type==kVDPixType_8) gen.conv_a16_to_a8();
		gen.move_to(0);
		srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_16_16_LE;
		return 0;
	}

	return -1;
}

IVDPixmapBlitter *VDPixmapCreateBlitter(const VDPixmap& dst, const VDPixmap& src, IVDPixmapExtraGen* extraDst) {
	const VDPixmapLayout& dstlayout = VDPixmapToLayoutFromBase(dst, dst.data);
	const VDPixmapLayout& srclayout = VDPixmapToLayoutFromBase(src, src.data);

	return VDPixmapCreateBlitter(dstlayout, srclayout, extraDst);
}

IVDPixmapBlitter *VDPixmapCreateBlitter(const VDPixmapLayout& dst, const VDPixmapLayout& src, IVDPixmapExtraGen* extraDst) {
	if (src.formatEx.fullEqual(dst.formatEx) && !extraDst) {
		return VDCreatePixmapUberBlitterDirectCopy(dst, src);
	}

	uint32 srcToken = VDPixmapGetFormatTokenFromFormat(src.format);
	uint32 dstToken = VDPixmapGetFormatTokenFromFormat(dst.format);

	VDPixmapUberBlitterGenerator gen;

	// load source channels
	int w = src.w;
	int h = src.h;

	switch(srcToken & kVDPixType_Mask) {
	case kVDPixType_1:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, (w + 7) >> 3);
		break;

	case kVDPixType_2:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, (w + 3) >> 2);
		break;

	case kVDPixType_4:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, (w + 1) >> 1);
		break;

	case kVDPixType_8:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, w);
		break;

	case kVDPixType_555_LE:
	case kVDPixType_565_LE:
	case kVDPixType_1555_LE:
	case kVDPixType_16_LE:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, w*2);
		break;

	case kVDPixType_888:
	case kVDPixType_V308:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, w*3);
		break;

	case kVDPixType_8888:
	case kVDPixType_32F_LE:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, w*4);
		break;

	case kVDPixType_16x4_LE:
		if (src.format==nsVDPixmap::kPixFormat_YUVA444_Y416) {
			gen.ldsrc(0, 0, 0, 0, w, h, srcToken, w*8);
			gen.dup();
			gen.dup();
			gen.dup();
			gen.extract_16in64(3, w, h);
			gen.swap(3);
			gen.extract_16in64(2, w, h);
			gen.swap(2);
			gen.extract_16in64(1, w, h);
			gen.swap(1);
			gen.extract_16in64(0, w, h);
			srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_16_16_16_LE;
			break;
		}

		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, w*8);
		break;

	case kVDPixType_32Fx4_LE:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, w*16);
		break;

	case kVDPixType_B8G8_R8G8:
	case kVDPixType_G8B8_G8R8:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, ((w + 1) & ~1)*2);
		break;

	case kVDPixType_8_8_8_8:
			gen.ldsrc(0, 3, 0, 0, w, h, kVDPixType_8, w);
	case kVDPixType_8_8_8:
		{
			uint32 ytoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8;
			uint32 cbtoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8;
			uint32 crtoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8;

			const VDPixmapSamplingInfo& sampInfo = VDPixmapGetSamplingInfo(srcToken);

			int cxbits = sampInfo.mPlane1Cb.mXBits;
			int cybits = sampInfo.mPlane1Cb.mYBits;
			int w2 = -(-w >> cxbits);
			int h2 = -(-h >> cybits);
			gen.ldsrc(0, 2, 0, 0, w2, h2, cbtoken, w2);
			gen.ldsrc(0, 0, 0, 0, w, h, ytoken, w);
			gen.ldsrc(0, 1, 0, 0, w2, h2, crtoken, w2);
		}
		break;

	case kVDPixType_16F_16F_16F_LE:
		{
			uint32 ytoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16F_LE;
			uint32 cbtoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16F_LE;
			uint32 crtoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16F_LE;

			const VDPixmapSamplingInfo& sampInfo = VDPixmapGetSamplingInfo(srcToken);

			int cxbits = sampInfo.mPlane1Cb.mXBits;
			int cybits = sampInfo.mPlane1Cb.mYBits;
			int w2 = -(-w >> cxbits);
			int h2 = -(-h >> cybits);
			gen.ldsrc(0, 2, 0, 0, w2, h2, cbtoken, w2 * 2);
			gen.ldsrc(0, 0, 0, 0, w, h, ytoken, w*2);
			gen.ldsrc(0, 1, 0, 0, w2, h2, crtoken, w2 * 2);
		}
		break;

	case kVDPixType_16_16_16_16_LE:
			gen.ldsrc(0, 3, 0, 0, w, h, kVDPixType_16_LE, w*2);
	case kVDPixType_16_16_16_LE:
		{
			uint32 ytoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_LE;
			uint32 cbtoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_LE;
			uint32 crtoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_LE;

			const VDPixmapSamplingInfo& sampInfo = VDPixmapGetSamplingInfo(srcToken);

			int cxbits = sampInfo.mPlane1Cb.mXBits;
			int cybits = sampInfo.mPlane1Cb.mYBits;
			int w2 = -(-w >> cxbits);
			int h2 = -(-h >> cybits);

			gen.ldsrc(0, 2, 0, 0, w2, h2, cbtoken, w2 * 2);
			gen.ldsrc(0, 0, 0, 0, w, h, ytoken, w*2);
			gen.ldsrc(0, 1, 0, 0, w2, h2, crtoken, w2 * 2);
		}
		break;

	case kVDPixType_16_16x2_LE:
		{
			uint32 ytoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_LE;
			uint32 cbtoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_LE;
			uint32 crtoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_LE;

			const VDPixmapSamplingInfo& sampInfo = VDPixmapGetSamplingInfo(srcToken);

			int cxbits = sampInfo.mPlane1Cb.mXBits;
			int cybits = sampInfo.mPlane1Cb.mYBits;
			int w2 = -(-w >> cxbits);
			int h2 = -(-h >> cybits);

			gen.ldsrc(0, 1, 0, 0, w2, h2, cbtoken, w2 * 4);
			gen.dup();
			gen.extract_16in32(1, w2, h2);
			gen.swap(1);
			gen.extract_16in32(0, w2, h2);
			gen.ldsrc(0, 0, 0, 0, w, h, ytoken, w*2);
			gen.swap(1);
			srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_16_16_LE;
		}
		break;

	case kVDPixType_32F_32F_32F_LE:
		{
			uint32 ytoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_32F_LE;
			uint32 cbtoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_32F_LE;
			uint32 crtoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_32F_LE;

			const VDPixmapSamplingInfo& sampInfo = VDPixmapGetSamplingInfo(srcToken);

			int cxbits = sampInfo.mPlane1Cb.mXBits;
			int cybits = sampInfo.mPlane1Cb.mYBits;
			int w2 = -(-w >> cxbits);
			int h2 = -(-h >> cybits);
			gen.ldsrc(0, 2, 0, 0, w2, h2, cbtoken, w2 * 4);
			gen.ldsrc(0, 0, 0, 0, w, h, ytoken, w*4);
			gen.ldsrc(0, 1, 0, 0, w2, h2, crtoken, w2 * 4);
		}
		break;

	case kVDPixType_V210:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, ((w + 5) / 6) * 4);
		break;

	case kVDPixType_V410:
	case kVDPixType_Y410:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, w * 4);
		break;

	case kVDPixType_R210:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, w * 4);
		gen.conv_R210_to_X16();
		srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16x4_LE;
		break;

	case kVDPixType_R10K:
		gen.ldsrc(0, 0, 0, 0, w, h, srcToken, w * 4);
		gen.conv_R10K_to_X16();
		srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16x4_LE;
		break;

	case kVDPixType_8_B8R8:
		{
			uint32 ytoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8;
			uint32 ctoken = (srcToken & ~kVDPixType_Mask) | kVDPixType_B8R8;

			const VDPixmapSamplingInfo& sampInfo = VDPixmapGetSamplingInfo(srcToken);

			int cxbits = sampInfo.mPlane1Cb.mXBits;
			int cybits = sampInfo.mPlane1Cb.mYBits;
			int w2 = -(-w >> cxbits);
			int h2 = -(-h >> cybits);
			gen.ldsrc(0, 0, 0, 0, w, h, ytoken, w);
			gen.ldsrc(0, 1, 0, 0, w2, h2, ctoken, w2*2);
		}
		break;

	default:
		VDASSERT(false);
	}

	if ((srcToken & kVDPixSpace_Mask)==kVDPixSpace_YCC_601) {
		if (VDPixmapFormatMatrixType(src.format)==1) {
			uint32 srcSpace = (src.formatEx.colorRangeMode==nsVDXPixmap::kColorRangeMode_Full) ? kVDPixSpace_YCC_601_FR : kVDPixSpace_YCC_601;
			if (src.formatEx.colorSpaceMode==nsVDXPixmap::kColorSpaceMode_709)
				srcSpace = (src.formatEx.colorRangeMode==nsVDXPixmap::kColorRangeMode_Full) ? kVDPixSpace_YCC_709_FR : kVDPixSpace_YCC_709;
			srcToken = (srcToken & ~kVDPixSpace_Mask) | srcSpace;
		}
	}

	if ((dstToken & kVDPixSpace_Mask)==kVDPixSpace_YCC_601) {
		if (VDPixmapFormatMatrixType(dst.format)==1) {
			uint32 dstSpace = (dst.formatEx.colorRangeMode==nsVDXPixmap::kColorRangeMode_Full) ? kVDPixSpace_YCC_601_FR : kVDPixSpace_YCC_601;
			if (dst.formatEx.colorSpaceMode==nsVDXPixmap::kColorSpaceMode_709)
				dstSpace = (dst.formatEx.colorRangeMode==nsVDXPixmap::kColorRangeMode_Full) ? kVDPixSpace_YCC_709_FR : kVDPixSpace_YCC_709;
			dstToken = (dstToken & ~kVDPixSpace_Mask) | dstSpace;
		}
	}

	int alpha_src = -1;
	int alpha_type = 0;
	// check if we need alpha bypass
	if (VDPixmapFormatHasAlphaPlane(dst.format)) {
		int proxy_dst = 0;
		switch (dstToken & kVDPixType_Mask) {
		case kVDPixType_8_8_8_8:
			proxy_dst = (dstToken & ~kVDPixType_Mask) | kVDPixType_8_8_8;
			alpha_type = kVDPixType_8;
			break;
		case kVDPixType_16_16_16_16_LE:
			proxy_dst = (dstToken & ~kVDPixType_Mask) | kVDPixType_16_16_16_LE;
			alpha_type = kVDPixType_16_LE;
			break;
		}

		if (VDPixmapFormatHasAlpha(src.format))
			alpha_src = BlitterSplitAlpha(gen, src, srcToken, alpha_type);

		dstToken = proxy_dst;

	} else if (VDPixmapFormatHasAlpha(dst.format)) {
		switch (dstToken & kVDPixType_Mask) {
		case kVDPixType_8888:
			alpha_type = kVDPixType_8;
			break;
		case kVDPixType_16x4_LE:
			alpha_type = kVDPixType_16_LE;
			break;
		}

		bool direct = false;
		if ((srcToken & kVDPixSpace_Mask)==kVDPixSpace_BGR && (dstToken & kVDPixSpace_Mask)==kVDPixSpace_BGR) direct = true;

		if (VDPixmapFormatHasAlpha(src.format) && !direct)
			alpha_src = BlitterSplitAlpha(gen, src, srcToken, alpha_type);
		
	} else {
		if (VDPixmapFormatHasAlpha(src.format)) {
			switch(srcToken & kVDPixType_Mask) {
			case kVDPixType_8_8_8_8:
				gen.move_to_end(0);
				gen.pop();
				srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8_8_8;
				break;

			case kVDPixType_16_16_16_16_LE:
				gen.move_to_end(0);
				gen.pop();
				srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_16_16_LE;
				break;
			}
		}
	}

	// check if we need a color space change
	if ((srcToken ^ dstToken) & kVDPixSpace_Mask) {
		// first, if we're dealing with an interleaved format, deinterleave it
		switch(srcToken & kVDPixType_Mask) {
		case kVDPixType_B8G8_R8G8:
			gen.dup();
			gen.dup();
			gen.extract_8in32(2, (w + 1) >> 1, h);
			gen.swap(2);
			gen.extract_8in16(1, w, h);
			gen.swap(1);
			gen.extract_8in32(0, (w + 1) >> 1, h);
			srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8_8_8;
			break;

		case kVDPixType_G8B8_G8R8:
			gen.dup();
			gen.dup();
			gen.extract_8in32(3, (w + 1) >> 1, h);
			gen.swap(2);
			gen.extract_8in16(0, w, h);
			gen.swap(1);
			gen.extract_8in32(1, (w + 1) >> 1, h);
			srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8_8_8;
			break;

		case kVDPixType_8_B8R8:
			gen.dup();
			gen.extract_8in16(1, (w + 1) >> 1, (h + 1) >> 1);
			gen.swap(2);
			gen.swap(1);
			gen.extract_8in16(0, (w + 1) >> 1, (h + 1) >> 1);
			srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8_8_8;
			break;

		case kVDPixType_V210:
			gen.conv_V210_to_P16();
			srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_16_16_LE;
			break;

		case kVDPixType_V410:
			gen.conv_V410_to_P16();
			srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_16_16_LE;
			break;

		case kVDPixType_Y410:
			gen.conv_Y410_to_P16();
			srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_16_16_16_LE;
			break;

		case kVDPixType_V308:
			gen.conv_V308_to_P8();
			srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8_8_8;
			break;

		case kVDPixType_16x4_LE:
			if ((srcToken & kVDPixSpace_Mask)==kVDPixSpace_BGR) {
				gen.conv_X16_to_X32F();
				srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_32Fx4_LE;
			} else {
				srcToken = BlitterConvertType(gen, srcToken, kVDPixType_16_16_16_LE, w, h);
			}
			break;
		}

		// if the source is subsampled, converge on 4:4:4 subsampling, but only if we actually need
		// the auxiliary channels
		const VDPixmapSamplingInfo& sampInfo = VDPixmapGetSamplingInfo(srcToken);

#if 0
		// This check is currently disabled because we currently do need the chroma planes
		// if we're doing a color space conversion, even if we are going to Y-only.
		switch(dstToken & kVDPixSpace_Mask) {
//			case kVDPixSpace_Y_601:
//			case kVDPixSpace_Y_709:
//			case kVDPixSpace_Y_601_FR:
//			case kVDPixSpace_Y_709_FR:
//				break;

			default:
#endif
				if (sampInfo.mPlane1Cb.mXBits |
					sampInfo.mPlane1Cb.mYBits |
					sampInfo.mPlane1Cb.mX |
					sampInfo.mPlane1Cb.mY |
					sampInfo.mPlane1Cr.mX |
					sampInfo.mPlane1Cr.mY)
					srcToken = BlitterConvertSampling(gen, srcToken, kVDPixSamp_444, w, h);
#if 0
				break;
		}
#endif

		// When target is high bit depth we need to upconvert as soon as possible
		int target_quality = 0;
		switch (dstToken & kVDPixType_Mask) {
		case kVDPixType_V210:
		case kVDPixType_V410:
		case kVDPixType_Y410:
		case kVDPixType_16_16_16_LE:
		case kVDPixType_16_16x2_LE:
		case kVDPixType_16x4_LE:
		case kVDPixType_16_16_16_16_LE:
			target_quality = 1;
			break;
		}

		if ((srcToken & kVDPixSpace_Mask)==kVDPixSpace_BGR && target_quality) {
			srcToken = BlitterConvertType(gen, srcToken, kVDPixType_32Fx4_LE, w, h);
			uint32 dstSpace = dstToken & kVDPixSpace_Mask;
			bool studioRGB = true;
			const VDPixmapGenYCbCrBasis* basis = &g_VDPixmapGenYCbCrBasis_601;
			if (dst.formatEx.colorSpaceMode==nsVDXPixmap::kColorSpaceMode_709)
				basis = &g_VDPixmapGenYCbCrBasis_709;
			if (dst.formatEx.colorRangeMode==nsVDXPixmap::kColorRangeMode_Full)
				studioRGB = false;
			gen.rgb32f_to_ycbcr_generic(*basis, studioRGB, dstSpace);
			srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | dstSpace | kVDPixType_32F_32F_32F_LE;
		}

		// Do this here before range/space mode is fully established
		if ((srcToken & kVDPixType_Mask)==kVDPixType_16_16_16_LE) {
			if ((dstToken & kVDPixSpace_Mask)==kVDPixSpace_BGR) {
				bool studioRGB = true;
				const VDPixmapGenYCbCrBasis* basis = &g_VDPixmapGenYCbCrBasis_601;
				if (src.formatEx.colorSpaceMode==nsVDXPixmap::kColorSpaceMode_709)
					basis = &g_VDPixmapGenYCbCrBasis_709;
				if (src.formatEx.colorRangeMode==nsVDXPixmap::kColorRangeMode_Full)
					studioRGB = false;
				gen.ycbcr_to_rgb64_generic(*basis, studioRGB);
				srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_BGR | kVDPixType_16x4_LE;
			} else {
				uint32 dstSpace = dstToken & kVDPixSpace_Mask;
				uint32 srcSpace = srcToken & kVDPixSpace_Mask;
				bool convert_float = true;
				if (srcSpace==kVDPixSpace_YCC_601 && dstSpace==kVDPixSpace_Y_601) convert_float = false;
				if (srcSpace==kVDPixSpace_YCC_601_FR && dstSpace==kVDPixSpace_Y_601_FR) convert_float = false;
				if (convert_float) srcToken = BlitterConvertType(gen, srcToken, kVDPixType_32F_32F_32F_LE, w, h);
			}
		}

		// change color spaces
		uint32 dstSpace = dstToken & kVDPixSpace_Mask;
		while((srcToken ^ dstToken) & kVDPixSpace_Mask) {
			uint32 srcSpace = srcToken & kVDPixSpace_Mask;
			uint32 targetSpace = dstSpace;

space_reconvert:
			switch(targetSpace) {
				case kVDPixSpace_BGR:
					switch(srcSpace) {
					case kVDPixSpace_YCC_709:
						switch(srcToken & kVDPixType_Mask) {
							case kVDPixType_8_8_8:
								gen.ycbcr709_to_rgb32();
								srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_BGR | kVDPixType_8888;
								break;

							case kVDPixType_16F_16F_16F_LE:
							case kVDPixType_16_16_16_LE:
								srcToken = BlitterConvertType(gen, srcToken, kVDPixType_32F_32F_32F_LE, w, h);
								gen.ycbcr709_to_rgb32_32f();
								srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_BGR | kVDPixType_32Fx4_LE;
								break;

							case kVDPixType_32F_32F_32F_LE:
								gen.ycbcr709_to_rgb32_32f();
								srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_BGR | kVDPixType_32Fx4_LE;
								break;

							default:
								VDASSERT(false);
								break;
						}
						break;

					case kVDPixSpace_YCC_601:
						switch(srcToken & kVDPixType_Mask) {
							case kVDPixType_8_8_8:
								gen.ycbcr601_to_rgb32();
								srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_BGR | kVDPixType_8888;
								break;

							case kVDPixType_16F_16F_16F_LE:
							case kVDPixType_16_16_16_LE:
								srcToken = BlitterConvertType(gen, srcToken, kVDPixType_32F_32F_32F_LE, w, h);
								gen.ycbcr601_to_rgb32_32f();
								srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_BGR | kVDPixType_32Fx4_LE;
								break;

							case kVDPixType_32F_32F_32F_LE:
								gen.ycbcr601_to_rgb32_32f();
								srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_BGR | kVDPixType_32Fx4_LE;
								break;

							default:
								VDASSERT(false);
								break;
						}
						break;

					case kVDPixSpace_YCC_709_FR:
						switch(srcToken & kVDPixType_Mask) {
							case kVDPixType_8_8_8:
								gen.ycbcr_to_rgb32_generic(g_VDPixmapGenYCbCrBasis_709, false);
								srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_BGR | kVDPixType_8888;
								break;

							default:
								VDASSERT(false);
								break;
						}
						break;

					case kVDPixSpace_YCC_601_FR:
						switch(srcToken & kVDPixType_Mask) {
							case kVDPixType_8_8_8:
								gen.ycbcr_to_rgb32_generic(g_VDPixmapGenYCbCrBasis_601, false);
								srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_BGR | kVDPixType_8888;
								break;

							default:
								VDASSERT(false);
								break;
						}
						break;

					case kVDPixSpace_Y_601:
						targetSpace = kVDPixSpace_YCC_601;
						goto space_reconvert;

					case kVDPixSpace_Y_709:
						targetSpace = kVDPixSpace_YCC_709;
						goto space_reconvert;

					case kVDPixSpace_Y_601_FR:
						targetSpace = kVDPixSpace_YCC_601_FR;
						goto space_reconvert;

					case kVDPixSpace_Y_709_FR:
						targetSpace = kVDPixSpace_YCC_709_FR;
						goto space_reconvert;

					case kVDPixSpace_Pal:
						switch(srcToken & kVDPixType_Mask) {
							case kVDPixType_1:
								gen.conv_Pal1_to_8888(0);
								srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_BGR | kVDPixType_8888;
								break;

							case kVDPixType_2:
								gen.conv_Pal2_to_8888(0);
								srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_BGR | kVDPixType_8888;
								break;

							case kVDPixType_4:
								gen.conv_Pal4_to_8888(0);
								srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_BGR | kVDPixType_8888;
								break;

							case kVDPixType_8:
								gen.conv_Pal8_to_8888(0);
								srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_BGR | kVDPixType_8888;
								break;

							default:
								VDASSERT(false);
								break;
						}
						break;

					default:
						VDASSERT(false);
						break;
					}
					break;

				case kVDPixSpace_Y_601:
					switch(srcSpace) {
						case kVDPixSpace_YCC_601:
							gen.pop();
							gen.swap(1);
							gen.pop();
							switch(srcToken & kVDPixType_Mask) {
								case kVDPixType_32F_32F_32F_LE:
									srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_32F_LE;
									break;
								case kVDPixType_16F_16F_16F_LE:
									srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_16F_LE;
									break;
								case kVDPixType_16_16_16_LE:
									srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_16_LE;
									break;
								case kVDPixType_8_8_8:
									srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_8;
									break;

								default:
									VDASSERT(false);
							}
							srcToken = BlitterConvertType(gen, srcToken, dstToken & kVDPixType_Mask, w, h);
							break;

						default:
							targetSpace = kVDPixSpace_YCC_601;
							goto space_reconvert;
					}
					break;

				case kVDPixSpace_Y_601_FR:
					switch(srcSpace) {
						case kVDPixSpace_YCC_601_FR:
							gen.pop();
							gen.swap(1);
							gen.pop();
							switch(srcToken & kVDPixType_Mask) {
								case kVDPixType_32F_32F_32F_LE:
									srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_32F_LE;
									break;
								case kVDPixType_16F_16F_16F_LE:
									srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_16F_LE;
									break;
								case kVDPixType_16_16_16_LE:
									srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_16_LE;
									break;
								case kVDPixType_8_8_8:
									srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_8;
									break;

								default:
									VDASSERT(false);
							}
							srcToken = BlitterConvertType(gen, srcToken, dstToken & kVDPixType_Mask, w, h);
							break;

						default:
							targetSpace = kVDPixSpace_YCC_601_FR;
							goto space_reconvert;
					}
					break;

				case kVDPixSpace_Y_709:
					switch(srcSpace) {
						case kVDPixSpace_YCC_709:
							gen.pop();
							gen.swap(1);
							gen.pop();
							switch(srcToken & kVDPixType_Mask) {
								case kVDPixType_32F_32F_32F_LE:
									srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_32F_LE;
									break;
								case kVDPixType_16F_16F_16F_LE:
									srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_16F_LE;
									break;
								case kVDPixType_16_16_16_LE:
									srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_16_LE;
									break;
								case kVDPixType_8_8_8:
									srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_8;
									break;

								default:
									VDASSERT(false);
							}
							srcToken = BlitterConvertType(gen, srcToken, dstToken & kVDPixType_Mask, w, h);
							break;

						default:
							targetSpace = kVDPixSpace_YCC_709;
							goto space_reconvert;
					}
					break;

				case kVDPixSpace_Y_709_FR:
					switch(srcSpace) {
						case kVDPixSpace_YCC_709_FR:
							gen.pop();
							gen.swap(1);
							gen.pop();
							switch(srcToken & kVDPixType_Mask) {
								case kVDPixType_32F_32F_32F_LE:
									srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_32F_LE;
									break;
								case kVDPixType_16F_16F_16F_LE:
									srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_16F_LE;
									break;
								case kVDPixType_16_16_16_LE:
									srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_16_LE;
									break;
								case kVDPixType_8_8_8:
									srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_8;
									break;

								default:
									VDASSERT(false);
							}
							srcToken = BlitterConvertType(gen, srcToken, dstToken & kVDPixType_Mask, w, h);
							break;

						default:
							targetSpace = kVDPixSpace_YCC_709_FR;
							goto space_reconvert;
					}
					break;

				case kVDPixSpace_YCC_601:
					switch(srcSpace) {
					case kVDPixSpace_BGR:
						if ((srcToken & kVDPixType_Mask)==kVDPixType_32Fx4_LE) {
							gen.rgb32_to_ycbcr601_32f();
							srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_YCC_601 | kVDPixType_32F_32F_32F_LE;
						} else {
							srcToken = BlitterConvertType(gen, srcToken, kVDPixType_8888, w, h);
							gen.rgb32_to_ycbcr601();
							srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_YCC_601 | kVDPixType_8_8_8;
						}
						break;
					case kVDPixSpace_Y_601:
					case kVDPixSpace_Y_709:
						if ((srcToken & kVDPixType_Mask)!=kVDPixType_8) {
							srcToken = BlitterConvertType(gen, srcToken, kVDPixType_32F_LE, w, h);
							srcToken = (srcToken & ~kVDPixSpace_Mask) | kVDPixSpace_YCC_601;

							{
								const VDPixmapSamplingInfo& sinfo = VDPixmapGetSamplingInfo(srcToken);
								int cw = ((w - 1) >> sinfo.mPlane1Cb.mXBits) + 1;
								int ch = ((h - 1) >> sinfo.mPlane1Cb.mYBits) + 1;

								gen.ldconstF(128.0/255.0, cw, cw, ch, srcToken);
							}

							gen.dup();
							gen.swap(2);
							gen.swap(1);
							srcToken = kVDPixSpace_YCC_601 | kVDPixType_32F_32F_32F_LE | (srcToken & kVDPixSamp_Mask);
							break;
						}

						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_YCC_601 | kVDPixType_8;

						{
							const VDPixmapSamplingInfo& sinfo = VDPixmapGetSamplingInfo(srcToken);
							int cw = ((w - 1) >> sinfo.mPlane1Cb.mXBits) + 1;
							int ch = ((h - 1) >> sinfo.mPlane1Cb.mYBits) + 1;

							gen.ldconst(0x80, cw, cw, ch, srcToken);
						}

						gen.dup();
						gen.swap(2);
						gen.swap(1);
						srcToken = kVDPixSpace_YCC_601 | kVDPixType_8_8_8 | (srcToken & kVDPixSamp_Mask);
						break;

					case kVDPixSpace_Y_601_FR:
						targetSpace = kVDPixSpace_YCC_601_FR;
						goto space_reconvert;

					case kVDPixSpace_Y_709_FR:
						targetSpace = kVDPixSpace_YCC_709_FR;
						goto space_reconvert;

					case kVDPixSpace_YCC_709:
						switch(srcToken & kVDPixType_Mask) {
							case kVDPixType_8_8_8:
							case kVDPixType_32F_32F_32F_LE:
								if(target_quality) srcToken = BlitterConvertType(gen, srcToken, kVDPixType_32F_32F_32F_LE, w, h);
								gen.ycbcr_to_ycbcr_generic(g_VDPixmapGenYCbCrBasis_601, true, g_VDPixmapGenYCbCrBasis_709, true, kVDPixSpace_YCC_601);
								srcToken = (srcToken & ~kVDPixSpace_Mask) | kVDPixSpace_YCC_601;
								break;

							default:
								VDASSERT(false);
								break;
						}
						break;

					case kVDPixSpace_YCC_601_FR:
						switch(srcToken & kVDPixType_Mask) {
							case kVDPixType_8_8_8:
							case kVDPixType_32F_32F_32F_LE:
								if(target_quality) srcToken = BlitterConvertType(gen, srcToken, kVDPixType_32F_32F_32F_LE, w, h);
								gen.ycbcr_to_ycbcr_generic(g_VDPixmapGenYCbCrBasis_601, true, g_VDPixmapGenYCbCrBasis_601, false, kVDPixSpace_YCC_601);
								srcToken = (srcToken & ~kVDPixSpace_Mask) | kVDPixSpace_YCC_601;
								break;

							default:
								VDASSERT(false);
								break;
						}
						break;

					case kVDPixSpace_YCC_709_FR:
						switch(srcToken & kVDPixType_Mask) {
							case kVDPixType_8_8_8:
							case kVDPixType_32F_32F_32F_LE:
								if(target_quality) srcToken = BlitterConvertType(gen, srcToken, kVDPixType_32F_32F_32F_LE, w, h);
								gen.ycbcr_to_ycbcr_generic(g_VDPixmapGenYCbCrBasis_601, true, g_VDPixmapGenYCbCrBasis_709, false, kVDPixSpace_YCC_601);
								srcToken = (srcToken & ~kVDPixSpace_Mask) | kVDPixSpace_YCC_601;
								break;

							default:
								VDASSERT(false);
								break;
						}
						break;

					case kVDPixSpace_Pal:
						targetSpace = kVDPixSpace_BGR;
						goto space_reconvert;

					default:
						VDASSERT(false);
						break;
					}
					break;

				case kVDPixSpace_YCC_709:
					switch(srcSpace) {
					case kVDPixSpace_BGR:
						srcToken = BlitterConvertType(gen, srcToken, kVDPixType_8888, w, h);
						gen.rgb32_to_ycbcr709();
						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_YCC_709 | kVDPixType_8_8_8;
						break;
					case kVDPixSpace_Y_601:
					case kVDPixSpace_Y_709:
						srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixSpace_YCC_709 | kVDPixType_8;

						{
							const VDPixmapSamplingInfo& sinfo = VDPixmapGetSamplingInfo(srcToken);
							int cw = ((w - 1) >> sinfo.mPlane1Cb.mXBits) + 1;
							int ch = ((h - 1) >> sinfo.mPlane1Cb.mYBits) + 1;

							gen.ldconst(0x80, cw, cw, ch, srcToken);
						}

						gen.dup();
						gen.swap(2);
						gen.swap(1);
						srcToken = kVDPixSpace_YCC_709 | kVDPixType_8_8_8 | (srcToken  & kVDPixSamp_Mask);
						break;
					case kVDPixSpace_YCC_601:
						switch(srcToken & kVDPixType_Mask) {
							case kVDPixType_8_8_8:
							case kVDPixType_32F_32F_32F_LE:
								if(target_quality) srcToken = BlitterConvertType(gen, srcToken, kVDPixType_32F_32F_32F_LE, w, h);
								gen.ycbcr_to_ycbcr_generic(g_VDPixmapGenYCbCrBasis_709, true, g_VDPixmapGenYCbCrBasis_601, true, kVDPixSpace_YCC_709);
								srcToken = (srcToken & ~kVDPixSpace_Mask) | kVDPixSpace_YCC_709;
								break;

							default:
								VDASSERT(false);
								break;
						}

					case kVDPixSpace_YCC_601_FR:
						switch(srcToken & kVDPixType_Mask) {
							case kVDPixType_8_8_8:
							case kVDPixType_32F_32F_32F_LE:
								if(target_quality) srcToken = BlitterConvertType(gen, srcToken, kVDPixType_32F_32F_32F_LE, w, h);
								gen.ycbcr_to_ycbcr_generic(g_VDPixmapGenYCbCrBasis_709, true, g_VDPixmapGenYCbCrBasis_601, false, kVDPixSpace_YCC_709);
								srcToken = (srcToken & ~kVDPixSpace_Mask) | kVDPixSpace_YCC_709;
								break;

							default:
								VDASSERT(false);
								break;
						}
						break;

					case kVDPixSpace_YCC_709_FR:
						switch(srcToken & kVDPixType_Mask) {
							case kVDPixType_8_8_8:
							case kVDPixType_32F_32F_32F_LE:
								if(target_quality) srcToken = BlitterConvertType(gen, srcToken, kVDPixType_32F_32F_32F_LE, w, h);
								gen.ycbcr_to_ycbcr_generic(g_VDPixmapGenYCbCrBasis_709, true, g_VDPixmapGenYCbCrBasis_709, false, kVDPixSpace_YCC_709);
								srcToken = (srcToken & ~kVDPixSpace_Mask) | kVDPixSpace_YCC_709;
								break;

							default:
								VDASSERT(false);
								break;
						}
						break;

					case kVDPixSpace_Y_601_FR:
						targetSpace = kVDPixSpace_YCC_601_FR;
						goto space_reconvert;

					case kVDPixSpace_Y_709_FR:
						targetSpace = kVDPixSpace_YCC_709_FR;
						goto space_reconvert;

					case kVDPixSpace_Pal:
						targetSpace = kVDPixSpace_BGR;
						goto space_reconvert;
						
					default:
						VDASSERT(false);
						break;
					}
					break;

				case kVDPixSpace_YCC_601_FR:
				case kVDPixSpace_YCC_709_FR:
					{
						const VDPixmapGenYCbCrBasis& dstBasis = *(targetSpace == kVDPixSpace_YCC_601_FR ? &g_VDPixmapGenYCbCrBasis_601 : &g_VDPixmapGenYCbCrBasis_709);

						switch(srcSpace) {
						case kVDPixSpace_BGR:
							srcToken = BlitterConvertType(gen, srcToken, kVDPixType_8888, w, h);
							gen.rgb32_to_ycbcr_generic(dstBasis, false, targetSpace);
							srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_8_8_8;
							break;
						case kVDPixSpace_Y_601_FR:
						case kVDPixSpace_Y_709_FR:
							srcToken = (srcToken & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | targetSpace | kVDPixType_8;

							{
								const VDPixmapSamplingInfo& sinfo = VDPixmapGetSamplingInfo(srcToken);
								int cw = ((w - 1) >> sinfo.mPlane1Cb.mXBits) + 1;
								int ch = ((h - 1) >> sinfo.mPlane1Cb.mYBits) + 1;

								gen.ldconst(0x80, cw, cw, ch, srcToken);
							}

							gen.dup();
							gen.swap(2);
							gen.swap(1);
							srcToken = (srcToken & ~kVDPixType_Mask) | kVDPixType_8_8_8;
							break;
						case kVDPixSpace_YCC_601:
							if(target_quality) srcToken = BlitterConvertType(gen, srcToken, kVDPixType_32F_32F_32F_LE, w, h);
							gen.ycbcr_to_ycbcr_generic(dstBasis, false, g_VDPixmapGenYCbCrBasis_601, true, targetSpace);
							srcToken = (srcToken & ~kVDPixSpace_Mask) | targetSpace;
							break;
						case kVDPixSpace_YCC_709:
							if(target_quality) srcToken = BlitterConvertType(gen, srcToken, kVDPixType_32F_32F_32F_LE, w, h);
							gen.ycbcr_to_ycbcr_generic(dstBasis, false, g_VDPixmapGenYCbCrBasis_709, true, targetSpace);
							srcToken = (srcToken & ~kVDPixSpace_Mask) | targetSpace;
							break;
						case kVDPixSpace_YCC_601_FR:
							if(target_quality) srcToken = BlitterConvertType(gen, srcToken, kVDPixType_32F_32F_32F_LE, w, h);
							gen.ycbcr_to_ycbcr_generic(dstBasis, false, g_VDPixmapGenYCbCrBasis_601, false, targetSpace);
							srcToken = (srcToken & ~kVDPixSpace_Mask) | targetSpace;
							break;
						case kVDPixSpace_YCC_709_FR:
							if(target_quality) srcToken = BlitterConvertType(gen, srcToken, kVDPixType_32F_32F_32F_LE, w, h);
							gen.ycbcr_to_ycbcr_generic(dstBasis, false, g_VDPixmapGenYCbCrBasis_709, false, targetSpace);
							srcToken = (srcToken & ~kVDPixSpace_Mask) | targetSpace;
							break;
						case kVDPixSpace_Pal:
							targetSpace = kVDPixSpace_BGR;
							goto space_reconvert;

						case kVDPixSpace_Y_601:
							targetSpace = kVDPixSpace_YCC_601;
							goto space_reconvert;
							
						case kVDPixSpace_Y_709:
							targetSpace = kVDPixSpace_YCC_709;
							goto space_reconvert;

						default:
							VDASSERT(false);
							break;
						}
					}
					break;

				default:
					VDASSERT(false);
					break;
			}
		}
	}

	// check if we need a type change
	//
	// Note: If the sampling is also different, we have to be careful about what types we
	// target. The type conversion may itself involve a sampling conversion, so things get
	// VERY tricky here.
	if ((srcToken ^ dstToken) & kVDPixType_Mask) {
		bool samplingDifferent = 0 != ((srcToken ^ dstToken) & kVDPixSamp_Mask);
		uint32 intermediateTypeToken = dstToken & kVDPixType_Mask;

		if (samplingDifferent) {
			switch(dstToken & kVDPixType_Mask) {
				case kVDPixType_16F_16F_16F_LE:
				case kVDPixType_16_16_16_LE:
				case kVDPixType_16x4_LE:
				case kVDPixType_16_16x2_LE:
					intermediateTypeToken = kVDPixType_32F_32F_32F_LE;
					break;
				case kVDPixType_8_B8R8:
					intermediateTypeToken = kVDPixType_8_8_8;
					break;
			}
		}

		srcToken = BlitterConvertType(gen, srcToken, (dstToken & ~kVDPixType_Mask) | intermediateTypeToken, w, h);
	}

	// convert subsampling if necessary
	switch(srcToken & kVDPixType_Mask) {
		case kVDPixType_8_8_8:
		case kVDPixType_16F_16F_16F_LE:
		case kVDPixType_16_16_16_LE:
		case kVDPixType_32F_32F_32F_LE:
			if ((srcToken ^ dstToken) & kVDPixSamp_Mask)
				srcToken = BlitterConvertSampling(gen, srcToken, dstToken, w, h);
			break;
	}

	// check if we need a type change (possible with 16F)
	srcToken = BlitterConvertType(gen, srcToken, dstToken, w, h);

	// attach alpha
	if (alpha_src!=-1) {
		switch(dstToken & kVDPixType_Mask) {
		case kVDPixType_8_8_8:
		case kVDPixType_16_16_16_LE:
			gen.move_to_end(alpha_src);
			break;

		case kVDPixType_8888:
			gen.move_to_end(alpha_src);
			gen.X8R8G8B8_to_A8R8G8B8();
			break;

		case kVDPixType_16x4_LE:
			gen.move_to_end(alpha_src);
			gen.X16R16G16B16_to_A16R16G16B16();
			break;
		}
	}

	if (extraDst) extraDst->Create(gen,dst);

	//gen.debug_dump();

	IVDPixmapBlitter* r = gen.create();
	r->SetComment(dst,src);
	return r;
}
