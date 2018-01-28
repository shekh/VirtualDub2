#include <stdafx.h>
#include <vd2/Kasumi/blitter.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <../Kasumi/h/uberblit_rgb64.h>
#include <../Kasumi/h/uberblit_16f.h>

void IVDPixmapBlitter::SetComment(const VDPixmap& dst, const VDPixmap& src) {
	profiler_comment = VDPixmapFormatPrintSpec(src) + " -> " + VDPixmapFormatPrintSpec(dst);
}

void IVDPixmapBlitter::SetComment(const VDPixmapLayout& dst, const VDPixmapLayout& src) {
	profiler_comment = VDPixmapFormatPrintSpec(src.formatEx) + " -> " + VDPixmapFormatPrintSpec(dst.formatEx);
}

VDPixmapCachedBlitter::VDPixmapCachedBlitter()
	: mSrcWidth(0)
	, mSrcHeight(0)
	, mSrcFormat(0)
	, mDstWidth(0)
	, mDstHeight(0)
	, mDstFormat(0)
	, mpCachedBlitter(NULL)
{
}

VDPixmapCachedBlitter::~VDPixmapCachedBlitter() {
	Invalidate();
}

void VDPixmapCachedBlitter::Update(const VDPixmap& dst, const VDPixmap& src) {
	VDASSERT(src.w == dst.w && src.h == dst.h);

	VDPixmapFormatEx dstFormat = dst;
	VDPixmapFormatEx srcFormat = src;

	if (!mpCachedBlitter ||
		dst.w != mDstWidth ||
		dst.h != mDstHeight ||
		!dstFormat.fullEqual(mDstFormat) ||
		src.w != mSrcWidth ||
		src.h != mSrcHeight ||
		!srcFormat.fullEqual(mSrcFormat))
	{
		profiler_comment.clear();
		if (mpCachedBlitter)
			delete mpCachedBlitter;
		mpCachedBlitter = VDPixmapCreateBlitter(dst, src);
		if (!mpCachedBlitter)
			return;

		profiler_comment = mpCachedBlitter->profiler_comment;
		mDstWidth = dst.w;
		mDstHeight = dst.h;
		mDstFormat = dstFormat;
		mSrcWidth = src.w;
		mSrcHeight = src.h;
		mSrcFormat = srcFormat;
	}
}

void VDPixmapCachedBlitter::Blit(const VDPixmap& dst, const VDPixmap& src) {
	Update(dst, src);
	mpCachedBlitter->Blit(dst, src);
}

void VDPixmapCachedBlitter::Invalidate() {
	if (mpCachedBlitter) {
		profiler_comment.clear();
		delete mpCachedBlitter;
		mpCachedBlitter = NULL;
	}
}

IVDPixmapExtraGen* VDPixmapCreateNormalizer(int format, FilterModPixmapInfo& out_info) {
	switch (format) {
	case nsVDPixmap::kPixFormat_XRGB8888:
		{
			ExtraGen_X8R8G8B8_Normalize* normalize = new ExtraGen_X8R8G8B8_Normalize;
			return normalize;
		}
	case nsVDPixmap::kPixFormat_XRGB64:
	case nsVDPixmap::kPixFormat_B64A:
		{
			ExtraGen_X16R16G16B16_Normalize* normalize = new ExtraGen_X16R16G16B16_Normalize;
			normalize->max_value = out_info.ref_r;
			return normalize;
		}
	case nsVDPixmap::kPixFormat_YUV420_Planar16:
	case nsVDPixmap::kPixFormat_YUV422_Planar16:
	case nsVDPixmap::kPixFormat_YUV444_Planar16:
	case nsVDPixmap::kPixFormat_YUV420_Alpha_Planar16:
	case nsVDPixmap::kPixFormat_YUV422_Alpha_Planar16:
	case nsVDPixmap::kPixFormat_YUV444_Alpha_Planar16:
	case nsVDPixmap::kPixFormat_YUV422_P216:
	case nsVDPixmap::kPixFormat_YUV420_P016:
	case nsVDPixmap::kPixFormat_YUV422_YU64:
		{
			ExtraGen_YUV_Normalize* normalize = new ExtraGen_YUV_Normalize;
			normalize->max_value = out_info.ref_r;
			return normalize;
		}
	case nsVDPixmap::kPixFormat_YUV422_P210:
	case nsVDPixmap::kPixFormat_YUV420_P010:
		{
			ExtraGen_YUV_Normalize* normalize = new ExtraGen_YUV_Normalize;
			normalize->max_value = out_info.ref_r;
			normalize->mask = 0xFFC0;
			return normalize;
		}
	case nsVDPixmap::kPixFormat_YUV420_Alpha_Planar:
	case nsVDPixmap::kPixFormat_YUV422_Alpha_Planar:
	case nsVDPixmap::kPixFormat_YUV444_Alpha_Planar:
		{
			ExtraGen_A8_Normalize* normalize = new ExtraGen_A8_Normalize;
			return normalize;
		}
	case nsVDPixmap::kPixFormat_YUVA444_Y416:
		{
			ExtraGen_X16R16G16B16_Normalize* normalize = new ExtraGen_X16R16G16B16_Normalize;
			normalize->max_value = out_info.ref_r;
			return normalize;
		}
	}

  return 0;
}
