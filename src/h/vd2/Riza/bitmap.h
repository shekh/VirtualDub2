//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2004 Avery Lee
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

#ifndef f_VD2_RIZA_BITMAP_H
#define f_VD2_RIZA_BITMAP_H

#include <vd2/system/vdstl.h>
#include <vd2/Riza/avi.h>

struct VDPixmap;

///////////////////////////////////////////////////////////////////////////
//
//	pixmap <-> bitmap format converters
//
//	pixmap format:	describes channel layout, color space, subsampling
//	variant:		describes variations in layout and FOURCC that may
//					correspond to the same pixmap format
//	bitmap format:	Win32 VDAVIBitmapInfoHeader
//
struct VDPixmapLayout;
struct FilterModPixmapInfo;
class VDStringA;
class VDStringW;

enum {
	kBitmapVariant_YV12 = 1,
	kBitmapVariant_I420 = 2,
	kBitmapVariant_IYUV = 3,
};
enum {
	kBitmapVariant_Y8 = 1,
	kBitmapVariant_Y800 = 2,
};
enum {
	kBitmapVariant_Y8_Pal = 1,
	kBitmapVariant_Y8_FR = 2,
};
enum {
	kBitmapVariant_Y1_0_16 = 1,
};
enum {
	kBitmapVariant_G3_0_16 = 1,
	kBitmapVariant_G3_0_14 = 2,
	kBitmapVariant_G3_0_12 = 3,
	kBitmapVariant_G3_0_10 = 4,
};
enum {
	kBitmapVariant_G4_0_16 = 1,
	kBitmapVariant_G4_0_14 = 2,
	kBitmapVariant_G4_0_12 = 3,
	kBitmapVariant_G4_0_10 = 4,
};
enum {
	kBitmapVariant_Y3_11_16 = 1,
	kBitmapVariant_Y3_11_10 = 2,
};
enum {
	kBitmapVariant_Y3_10_16 = 1,
	kBitmapVariant_Y3_10_10 = 2,
};
enum {
	kBitmapVariant_Y3_0_16 = 1,
	kBitmapVariant_Y3_0_10 = 2,
};

int VDGetPixmapToBitmapVariants(int format);
int VDBitmapFormatToPixmapFormat(const VDAVIBitmapInfoHeader& hdr);
int VDBitmapFormatToPixmapFormat(const VDAVIBitmapInfoHeader& hdr, int& variant);
bool VDMakeBitmapFormatFromPixmapFormat(vdstructex<VDAVIBitmapInfoHeader>& dst, const vdstructex<VDAVIBitmapInfoHeader>& src, int format, int variant);
bool VDMakeBitmapFormatFromPixmapFormat(vdstructex<VDAVIBitmapInfoHeader>& dst, const vdstructex<VDAVIBitmapInfoHeader>& src, int format, int variant, uint32 w, uint32 h);
bool VDMakeBitmapFormatFromPixmapFormat(vdstructex<VDAVIBitmapInfoHeader>& dst, int format, int variant, uint32 w, uint32 h, bool allowNonstandardMappings = false);
uint32 VDMakeBitmapCompatiblePixmapLayout(VDPixmapLayout& layout, sint32 w, sint32 h, int format, int variant, const uint32 *palette = NULL, int imageSize=0);
bool VDGetPixmapLayoutForBitmapFormat(const VDAVIBitmapInfoHeader& hdr, uint32 hdrsize, VDPixmapLayout& layout);
VDPixmap VDGetPixmapForBitmap(const VDAVIBitmapInfoHeader& hdr, const void *data);
void VDSetPixmapInfoForBitmap(FilterModPixmapInfo& info, int format, int variant=0);
void VDAdjustPixmapInfoForRange(FilterModPixmapInfo& info, int format);

uint32 fourcc_toupper(uint32 a);

VDStringA print_fourcc(uint32 a);
VDStringW printW_fourcc(uint32 a);

void fourcc_codec_info(uint32 a, bool& useAlpha);
void fourcc_codec_input(uint32 a, int& format, int& variant);
void fourcc_codec_output(uint32 a, int& format, int& variant);

#endif
