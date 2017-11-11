#include <stdafx.h>
#include <vd2/Kasumi/blitter.h>
#include <vd2/Kasumi/pixmaputils.h>

void IVDPixmapBlitter::SetComment(const VDPixmap& dst, const VDPixmap& src) {
	profiler_comment = VDPixmapFormatPrintSpec(src) + " -> " + VDPixmapFormatPrintSpec(dst);
}

void IVDPixmapBlitter::SetComment(const VDPixmapLayout& dst, const VDPixmapLayout& src) {
	VDPixmapFormatEx dst2 = VDPixmapFormatCombine(dst.format, dst.formatEx);
	VDPixmapFormatEx src2 = VDPixmapFormatCombine(src.format, src.formatEx);
	profiler_comment = VDPixmapFormatPrintSpec(src2) + " -> " + VDPixmapFormatPrintSpec(dst2);
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
		dst.ext != mDstExt ||
		src.w != mSrcWidth ||
		src.h != mSrcHeight ||
		!srcFormat.fullEqual(mSrcFormat) ||
		src.ext != mSrcExt)
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
		mDstExt = dst.ext;
		mSrcWidth = src.w;
		mSrcHeight = src.h;
		mSrcFormat = srcFormat;
		mSrcExt = src.ext;
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
