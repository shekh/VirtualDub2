//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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

#include "stdafx.h"
#include <new>
#include "VirtualDub.h"


#include "convert.h"
#include "VBitmap.h"
#include "Histogram.h"
#include <vd2/system/cpuaccel.h>
#include <vd2/system/memory.h>

#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/pixmapops.h>

VDPixmap VDAsPixmap(const VBitmap& bm) {
	VDPixmap pxm = {
		(char *)bm.data + bm.pitch * (bm.h - 1),
		bm.depth == 8 ? bm.palette : NULL,
		bm.w,
		bm.h,
		-bm.pitch
	};

	switch(bm.depth) {
	case 8:		pxm.format = nsVDPixmap::kPixFormat_Pal8; break;
	case 16:	pxm.format = nsVDPixmap::kPixFormat_XRGB1555; break;
	case 24:	pxm.format = nsVDPixmap::kPixFormat_RGB888; break;
	case 32:	pxm.format = nsVDPixmap::kPixFormat_XRGB8888; break;
	default:
		VDASSERT(false);
	}

	return pxm;
}

VDPixmap VDAsPixmap565(const VBitmap& bm) {
	VDPixmap pxm = {
		(char *)bm.data + bm.pitch * (bm.h - 1),
		bm.depth == 8 ? bm.palette : NULL,
		bm.w,
		bm.h,
		-bm.pitch
	};

	switch(bm.depth) {
	case 8:		pxm.format = nsVDPixmap::kPixFormat_Pal8; break;
	case 16:	pxm.format = nsVDPixmap::kPixFormat_RGB565; break;
	case 24:	pxm.format = nsVDPixmap::kPixFormat_RGB888; break;
	case 32:	pxm.format = nsVDPixmap::kPixFormat_XRGB8888; break;
	default:
		VDASSERT(false);
	}

	return pxm;
}

VBitmap VDAsVBitmap(const VDPixmap& px) {
	VBitmap vbm;

	vbm.data		= (Pixel *)(vdptroffset(px.data, px.pitch * (px.h - 1)));
	vbm.palette		= (Pixel *)px.palette;
	vbm.w			= px.w;
	vbm.h			= px.h;
	vbm.pitch		= -px.pitch;
	vbm.offset		= px.pitch > 0 ? 0 : px.pitch * (px.h - 1);
	vbm.size		= vdptrdiffabs(px.pitch) * px.h;

	switch(px.format) {
		case nsVDPixmap::kPixFormat_Pal8:
			vbm.modulo = vbm.pitch - px.w;
			vbm.depth = 8;
			break;
		case nsVDPixmap::kPixFormat_XRGB1555:
			vbm.modulo = vbm.pitch - 2*px.w;
			vbm.depth = 16;
			break;
		case nsVDPixmap::kPixFormat_RGB888:
			vbm.modulo = vbm.pitch - 3*px.w;
			vbm.depth = 24;
			break;
		case nsVDPixmap::kPixFormat_XRGB8888:
			vbm.modulo = vbm.pitch - 4*px.w;
			vbm.depth = 32;
			break;
		default:
			VDASSERT(false);
			vbm.modulo = 0;
			vbm.pitch = 0;
	}

	return vbm;
}

extern "C" void __cdecl asm_bitmap_xlat1(Pixel32 *dst, Pixel32 *src,
		PixOffset dpitch, PixOffset spitch,
		PixDim w,
		PixDim h,
		const Pixel8 *tbl);

extern "C" void __cdecl asm_bitmap_xlat3(Pixel32 *dst, Pixel32 *src,
		PixOffset dpitch, PixOffset spitch,
		PixDim w,
		PixDim h,
		const Pixel32 *tbl);

///////////////////////////////////////////////////////////////////////////

VBitmap::VBitmap(void *lpData, BITMAPINFOHEADER *bmih) {
	init(lpData, bmih);
}

VBitmap::VBitmap(void *data, PixDim w, PixDim h, int depth) {
	init(data, w, h, depth);
}

///////////////////////////////////////////////////////////////////////////

VBitmap& VBitmap::init(void *lpData, BITMAPINFOHEADER *bmih) {
	data			= (Pixel *)lpData;
	palette			= (Pixel *)(bmih+1);
	depth			= bmih->biBitCount;
	w				= bmih->biWidth;
	h				= bmih->biHeight;
	offset			= 0;
	AlignTo4();

	return *this;
}

VBitmap& VBitmap::init(void *data, PixDim w, PixDim h, int depth) {
	this->data		= (Pixel32 *)data;
	this->palette	= NULL;
	this->depth		= depth;
	this->w			= w;
	this->h			= h;
	this->offset	= 0;
	AlignTo8();

	return *this;
}

void VBitmap::MakeBitmapHeader(BITMAPINFOHEADER *bih) const {
	bih->biSize				= sizeof(BITMAPINFOHEADER);
	bih->biBitCount			= (WORD)depth;
	bih->biPlanes			= 1;
	bih->biCompression		= BI_RGB;

	if (pitch == ((w*bih->biBitCount + 31)/32) * 4)
		bih->biWidth		= w;
	else
		bih->biWidth		= pitch*8 / depth;

	bih->biHeight			= h;
	bih->biSizeImage		= pitch*h;
	bih->biClrUsed			= 0;
	bih->biClrImportant		= 0;
	bih->biXPelsPerMeter	= 0;
	bih->biYPelsPerMeter	= 0;
}

void VBitmap::MakeBitmapHeaderNoPadding(BITMAPINFOHEADER *bih) const {
	bih->biSize				= sizeof(BITMAPINFOHEADER);
	bih->biBitCount			= (WORD)depth;
	bih->biPlanes			= 1;
	bih->biCompression		= BI_RGB;
	bih->biWidth			= w;
	bih->biHeight			= h;
	bih->biSizeImage		= (((w*bih->biBitCount + 31)/32) * 4)*h;
	bih->biClrUsed			= 0;
	bih->biClrImportant		= 0;
	bih->biXPelsPerMeter	= 0;
	bih->biYPelsPerMeter	= 0;
}

void VBitmap::AlignTo4() {
	pitch		= PitchAlign4();
	modulo		= Modulo();
	size		= Size();
}

void VBitmap::AlignTo8() {
	pitch		= PitchAlign8();
	modulo		= Modulo();
	size		= Size();
}

///////////////////////////////////////////////////////////////////////////

bool VBitmap::dualrectclip(PixCoord& x2, PixCoord& y2, const VBitmap *src, PixCoord& x1, PixCoord& y1, PixDim& dx, PixDim& dy) const {
	if (dx == -1) dx = src->w;
	if (dy == -1) dy = src->h;

	// clip to source bitmap

	if (x1 < 0) { dx+=x1; x2-=x1; x1=0; }
	if (y1 < 0) { dy+=y1; y2-=y1; y1=0; }
	if (x1+dx > src->w) dx=src->w-x1;
	if (y1+dy > src->h) dy=src->h-y1;

	// clip to destination bitmap

	if (x2 < 0) { dx+=x2; x1-=x2; x2=0; }
	if (y2 < 0) { dy+=y2; y1-=y2; y2=0; }
	if (x2+dx > w) dx=w-x2;
	if (y2+dy > h) dy=h-y2;

	// anything left to blit?

	if (dx<=0 || dy<=0)
		return false;

	return true;
}

///////////////////////////////////////////////////////////////////////////

void VBitmap::BitBlt(PixCoord x2, PixCoord y2, const VBitmap *src, PixDim x1, PixDim y1, PixDim dx, PixDim dy) const {
	if (dx == -1) dx = src->w;
	if (dy == -1) dy = src->h;

	VDVERIFY(VDPixmapBlt(VDAsPixmap(*this), x2, y2, VDAsPixmap(*src), x1, y1, dx, dy));
}

void VBitmap::BitBltDither(PixCoord x2, PixCoord y2, const VBitmap *src, PixDim x1, PixDim y1, PixDim dx, PixDim dy, bool to565) const {

	// Right now, we can only dither 32->16

	if (src->depth != 32 || depth != 16) {
		BitBlt(x2, y2, src, x1, y1, dx, dy);
		return;
	}

	// Do the blit

	Pixel *dstp, *srcp;

	if (!dualrectclip(x2, y2, src, x1, y1, dx, dy))
		return;

	// compute coordinates

	srcp = src->Address(x1, y1+dy-1);
	dstp = Address(x2, y2+dy-1);

	// do the blit

#ifdef _M_IX86
	if (to565)
		DIBconvert_32_to_16_565_dithered(dstp, pitch, srcp, src->pitch, dx, dy);
	else
		DIBconvert_32_to_16_dithered(dstp, pitch, srcp, src->pitch, dx, dy);
#else
#pragma vdpragma_TODO("fixme")
#endif
}

void VBitmap::BitBlt565(PixCoord x2, PixCoord y2, const VBitmap *src, PixDim x1, PixDim y1, PixDim dx, PixDim dy) const {
	if (dx == -1) dx = src->w;
	if (dy == -1) dy = src->h;

	VDPixmapBlt(VDAsPixmap(*this), x2, y2, VDAsPixmap565(*src), x1, y1, dx, dy);
}

bool VBitmap::BitBltXlat1(PixCoord x2, PixCoord y2, const VBitmap *src, PixCoord x1, PixCoord y1, PixDim dx, PixDim dy, const Pixel8 *tbl) const {
	if (depth != 32)
		return false;

	if (!dualrectclip(x2, y2, src, x1, y1, dx, dy))
		return false;

	// do the translate

#ifdef _M_IX86
	asm_bitmap_xlat1(
			this->Address32(x2, y2+dy-1)+dx,
			src ->Address32(x1, y1+dy-1)+dx,
			this->pitch,
			src->pitch,
			-4*dx, dy, tbl);
#else
#pragma vdpragma_TODO("fixme")
#endif

	return true;
}

bool VBitmap::BitBltXlat3(PixCoord x2, PixCoord y2, const VBitmap *src, PixCoord x1, PixCoord y1, PixDim dx, PixDim dy, const Pixel32 *tbl) const {
	if (depth != 32)
		return false;

	if (!dualrectclip(x2, y2, src, x1, y1, dx, dy))
		return false;

	// do the translate

#ifdef _M_IX86
	asm_bitmap_xlat3(
			this->Address32(x2, y2+dy-1)+dx,
			src ->Address32(x1, y1+dy-1)+dx,
			this->pitch,
			src->pitch,
			-4*dx, dy, tbl);
#else
#pragma vdpragma_TODO("fixme")
#endif

	return true;
}

///////////////////////////////////////////////////////////////////////////

bool VBitmap::StretchBltNearestFast(PixCoord x2, PixCoord y2, PixDim dx, PixDim dy,
						const VBitmap *src, double x1, double y1, double dx1, double dy1) const {

	return VDPixmapStretchBltNearest(VDAsPixmap(*this), x2<<16, y2<<16, (x2+dx)<<16, (y2+dy)<<16,
				VDAsPixmap(*src), VDRoundToLong(x1*65536.0), VDRoundToLong(y1*65536.0), VDRoundToLong((x1+dx1)*65536.0), VDRoundToLong((y1+dy1)*65536.0));
}

bool VBitmap::StretchBltBilinearFast(PixCoord x2, PixCoord y2, PixDim dx, PixDim dy,
						const VBitmap *src, double x1, double y1, double dx1, double dy1) const {


	return VDPixmapStretchBltBilinear(VDAsPixmap(*this), x2<<16, y2<<16, (x2+dx)<<16, (y2+dy)<<16,
				VDAsPixmap(*src), VDRoundToLong(x1*65536.0), VDRoundToLong(y1*65536.0), VDRoundToLong((x1+dx1)*65536.0), VDRoundToLong((y1+dy1)*65536.0));
}

bool VBitmap::RectFill(PixCoord x, PixCoord y, PixDim dx, PixDim dy, Pixel32 c) const {

	if (depth != 32)
		return false;

	// Do the blit

	Pixel32 *dstp;

	if (dx == -1) dx = w;
	if (dy == -1) dy = h;

	// clip to destination bitmap

	if (x < 0) { dx+=x; x=0; }
	if (y < 0) { dy+=y; y=0; }
	if (x+dx > w) dx=w-x;
	if (y+dy > h) dy=h-y;

	// anything left to fill?

	if (dx<=0 || dy<=0) return false;

	// compute coordinates

	dstp = Address32(x, y+dy-1);

	// do the fill

	do {
		VDMemset32(dstp, c, dx);

		dstp = (Pixel32 *)((char *)dstp + pitch);
	} while(--dy);

	return true;
}

bool VBitmap::Histogram(PixCoord x, PixCoord y, PixCoord dx, PixCoord dy, long *pHisto, int iHistoType) const {
	static const unsigned short pixmasks[3]={
		0x7c00,
		0x03e0,
		0x001f,
	};

	if (depth != 32 && depth != 24 && depth != 16)
		return false;

	// Do the blit

	Pixel32 *dstp;

	if (dx == -1) dx = w;
	if (dy == -1) dy = h;

	// clip to bitmap

	if (x < 0) { dx+=x; x=0; }
	if (y < 0) { dy+=y; y=0; }
	if (x+dx > w) dx=w-x;
	if (y+dy > h) dy=h-y;

	// anything left to histogram?

	if (dx<=0 || dy<=0) return false;

	// compute coordinates

	dstp = Address(x, y+dy-1);

	// do the histogram

#ifdef _M_IX86
	switch(iHistoType) {
	case HISTO_LUMA:
		switch(depth) {
		case 16:	asm_histogram16_run(dstp, dx, dy, pitch, pHisto, 0x7FFF); break;
		case 24:	asm_histogram_gray24_run(dstp, dx, dy, pitch, pHisto); break;
		case 32:	asm_histogram_gray_run(dstp, dx, dy, pitch, pHisto); break;
		default:	__assume(false);
		}
		break;

	case HISTO_GRAY:
		// FIXME: Really lame algorithm.

		switch(depth) {
		case 16:
			asm_histogram16_run(dstp, dx, dy, pitch, pHisto, pixmasks[0]);
			asm_histogram16_run(dstp, dx, dy, pitch, pHisto, pixmasks[1]);
			asm_histogram16_run(dstp, dx, dy, pitch, pHisto, pixmasks[2]);
			break;
		case 24:
			asm_histogram_color24_run((char *)dstp + 0, dx, dy, pitch, pHisto);
			asm_histogram_color24_run((char *)dstp + 1, dx, dy, pitch, pHisto);
			asm_histogram_color24_run((char *)dstp + 2, dx, dy, pitch, pHisto);
			break;
		case 32:
			asm_histogram_color_run((char *)dstp + 0, dx, dy, pitch, pHisto);
			asm_histogram_color_run((char *)dstp + 1, dx, dy, pitch, pHisto);
			asm_histogram_color_run((char *)dstp + 2, dx, dy, pitch, pHisto);
			break;
		default:	__assume(false);
		}
		break;

	case HISTO_RED:
	case HISTO_GREEN:
	case HISTO_BLUE:
		switch(depth) {
		case 16:	asm_histogram16_run(dstp, dx, dy, pitch, pHisto, pixmasks[iHistoType-HISTO_RED]); break;
		case 24:	asm_histogram_color24_run((char *)dstp + (2-(iHistoType-HISTO_RED)), dx, dy, pitch, pHisto); break;
		case 32:	asm_histogram_color_run((char *)dstp + (2-(iHistoType-HISTO_RED)), dx, dy, pitch, pHisto); break;
		default:	__assume(false);
		}
		break;
	}
#else
#pragma vdpragma_TODO("fixme")
#endif

	return true;
}


///////////////////////////////////////////////////////////////////////////

#ifdef _M_IX86
extern "C" void asm_convert_yuy2_bgr16(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_bgr16_MMX(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_bgr24(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_bgr24_MMX(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_bgr32(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_bgr32_MMX(void *dst, void *src, int w, int h, long dstmod, long srcmod);

extern "C" void asm_convert_yuy2_fullscale_bgr16(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_fullscale_bgr16_MMX(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_fullscale_bgr24(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_fullscale_bgr24_MMX(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_fullscale_bgr32(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_fullscale_bgr32_MMX(void *dst, void *src, int w, int h, long dstmod, long srcmod);
#endif

bool VBitmap::BitBltFromYUY2(PixCoord x2, PixCoord y2, const VBitmap *src, PixCoord x1, PixCoord y1, PixDim dx, PixDim dy) const {
	VDPixmap srcbm = {0};

	srcbm.data		= src->data;
	srcbm.pitch		= src->pitch;
	srcbm.w			= src->w;
	srcbm.h			= src->h;
	srcbm.format	= nsVDPixmap::kPixFormat_YUV422_YUYV;

	return VDPixmapBlt(VDAsPixmap(*this), x2, y2, srcbm, x1, y1, dx, dy);
}

bool VBitmap::BitBltFromYUY2Fullscale(PixCoord x2, PixCoord y2, const VBitmap *src, PixCoord x1, PixCoord y1, PixDim dx, PixDim dy) const {
	unsigned char *srcp, *dstp;

	if (depth != 32 && depth != 24 && depth != 16)
		return false;

	if (!dualrectclip(x2, y2, src, x1, y1, dx, dy))
		return false;

	// We only support even widths.

	if (x2 & 1) {
		++x2;
		--dx;
	}

	if (dx & 1)
		--dx;

	if (dx<=0)
		return false;

	// compute coordinates

	srcp = (unsigned char *)src->Address16i(x1, y1+dy-1);
	dstp = (unsigned char *)Address(x2, y2+dy-1);

	// Do blit

	dx >>= 1;

#ifdef _M_IX86
	PixOffset srcmod = -src->pitch;
	PixOffset dstmod = pitch;

	switch(depth) {
	case 16:
		srcmod -= 4*dx;
		dstmod -= 4*dx;

		if (MMX_enabled)
			asm_convert_yuy2_fullscale_bgr16_MMX(dstp, srcp, dx, dy, dstmod, srcmod);
		else
			asm_convert_yuy2_fullscale_bgr16(dstp, srcp, dx, dy, dstmod, srcmod);

		break;
	case 24:
		srcmod -= 4*dx;
		dstmod -= 6*dx;

		if (MMX_enabled)
			asm_convert_yuy2_fullscale_bgr24_MMX(dstp, srcp, dx, dy, dstmod, srcmod);
		else
			asm_convert_yuy2_fullscale_bgr24(dstp, srcp, dx, dy, dstmod, srcmod);

		break;
	case 32:
		srcmod -= 4*dx;
		dstmod -= 8*dx;

		if (MMX_enabled)
			asm_convert_yuy2_fullscale_bgr32_MMX(dstp, srcp, dx, dy, dstmod, srcmod);
		else
			asm_convert_yuy2_fullscale_bgr32(dstp, srcp, dx, dy, dstmod, srcmod);

		break;
	}
#else
#pragma vdpragma_TODO("fixme")
#endif

	return true;
}

///////////////////////////////////////////////////////////////////////////

typedef unsigned char YUVPixel;

extern "C" void asm_YUVtoRGB32_row(
		void *ARGB1_pointer,
		void *ARGB2_pointer,
		YUVPixel *Y1_pointer,
		YUVPixel *Y2_pointer,
		YUVPixel *U_pointer,
		YUVPixel *V_pointer,
		long width
		);

extern "C" void asm_YUVtoRGB24_row(
		void *ARGB1_pointer,
		void *ARGB2_pointer,
		YUVPixel *Y1_pointer,
		YUVPixel *Y2_pointer,
		YUVPixel *U_pointer,
		YUVPixel *V_pointer,
		long width
		);

extern "C" void asm_YUVtoRGB16_row(
		void *ARGB1_pointer,
		void *ARGB2_pointer,
		YUVPixel *Y1_pointer,
		YUVPixel *Y2_pointer,
		YUVPixel *U_pointer,
		YUVPixel *V_pointer,
		long width
		);

bool VBitmap::BitBltFromI420(PixCoord x2, PixCoord y2, const VBitmap *src, PixCoord x1, PixCoord y1, PixDim dx, PixDim dy) const {
	VDPixmap srcbm = {0};

	srcbm.data		= src->data;
	srcbm.pitch		= src->pitch;
	srcbm.w			= src->w;
	srcbm.h			= src->h;
	srcbm.format	= nsVDPixmap::kPixFormat_YUV420_Planar;
	srcbm.data2		= (char *)srcbm.data + src->w * src->h;
	srcbm.pitch2	= (src->w+1)>>1;
	srcbm.data3		= (char *)srcbm.data2 + srcbm.pitch2 * ((src->h+1)>>1);

	return VDPixmapBlt(VDAsPixmap(*this), x2, y2, srcbm, x1, y1, dx, dy);
}
