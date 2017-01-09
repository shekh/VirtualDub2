#include <stdafx.h>
#include <vd2/Kasumi/blitter.h>

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

void VDPixmapCachedBlitter::Blit(const VDPixmap& dst, const VDPixmap& src) {
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
		if (mpCachedBlitter)
			delete mpCachedBlitter;
		mpCachedBlitter = VDPixmapCreateBlitter(dst, src);
		if (!mpCachedBlitter)
			return;

		mDstWidth = dst.w;
		mDstHeight = dst.h;
		mDstFormat = dstFormat;
		mDstExt = dst.ext;
		mSrcWidth = src.w;
		mSrcHeight = src.h;
		mSrcFormat = srcFormat;
		mSrcExt = src.ext;
	}

	mpCachedBlitter->Blit(dst, src);
}

void VDPixmapCachedBlitter::Invalidate() {
	if (mpCachedBlitter) {
		delete mpCachedBlitter;
		mpCachedBlitter = NULL;
	}
}
