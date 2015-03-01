//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2006 Avery Lee
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
#include <vd2/system/binary.h>
#include <vd2/system/error.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "imageiff.h"

class VDImageDecoderIFF : public IVDImageDecoderIFF {
public:
	VDImageDecoderIFF();
	~VDImageDecoderIFF();

	const VDPixmap& Decode(const void *src, uint32 srclen);

protected:
	void Parse(const uint8 *src, uint32 srclen, uint32 alignment);
	void ParseFORM(const uint8 *src, uint32 srclen, uint32 alignment);
	void ParseTBHD(const uint8 *src, uint32 srclen);
	void ParseRGBA(const uint8 *src, uint32 srclen);

	bool	mbFoundTBHD;
	uint32	mWidth;
	uint32	mHeight;
	uint32	mImagePixelsFilled;

	VDPixmapBuffer	mBuffer;
};

IVDImageDecoderIFF *VDCreateImageDecoderIFF() {
	return new VDImageDecoderIFF;
}

VDImageDecoderIFF::VDImageDecoderIFF() {
}

VDImageDecoderIFF::~VDImageDecoderIFF() {
}

const VDPixmap& VDImageDecoderIFF::Decode(const void *src, uint32 srclen) {
	mWidth = 0;
	mHeight = 0;
	mbFoundTBHD = false;
	mImagePixelsFilled = 0;

	Parse((const uint8 *)src, srclen, 2);

	// Check that we covered exactly all of the pixels in the image.
	if (mImagePixelsFilled != mWidth * mHeight)
		throw MyError("Cannot read IFF file: The configuration of image tiles in the file is invalid.");

	return mBuffer;
}

void VDImageDecoderIFF::Parse(const uint8 *src, uint32 srclen, uint32 alignment) {
	while(srclen > 8) {
		uint32 fcc = VDReadUnalignedU32(src);
		uint32 size = VDReadUnalignedBEU32(src + 4);
		src += 8;
		srclen -= 8;

		// check for a child truncated by its parent
		if (size > srclen)
			return;

		// process the chunk
		switch(fcc) {
			case VDMAKEFOURCC('F', 'O', 'R', 'M'):
				ParseFORM(src, size, 2);
				break;

			case VDMAKEFOURCC('F', 'O', 'R', '4'):
				ParseFORM(src, size, 4);
				break;

			case VDMAKEFOURCC('F', 'O', 'R', '8'):
				ParseFORM(src, size, 8);
				break;

			case VDMAKEFOURCC('T', 'B', 'H', 'D'):
				ParseTBHD(src, size);
				break;

			case VDMAKEFOURCC('R', 'G', 'B', 'A'):
				ParseRGBA(src, size);
				break;
		}

		// skip data and respect alignment
		size += alignment - 1;
		size &= ~(alignment - 1);

		if (srclen < size)
			return;

		src += size;
		srclen -= size;
	}
}

void VDImageDecoderIFF::ParseFORM(const uint8 *src, uint32 srclen, uint32 alignment) {
	// bail if FORM is malformed or is too small to contain a chunk
	if (srclen < 12)
		return;

	// process the FORM type and determine if we should recurse
	uint32 formfcc = VDReadUnalignedU32(src);
	src += 4;
	srclen -= 4;

	// recurse into 'CIMG' (MayaIFF color image)
	if (formfcc == VDMAKEFOURCC('C', 'I', 'M', 'G'))
		Parse(src, srclen, alignment);

	// recurse into 'TBMP' (MayaIFF tiled bitmap)
	if (formfcc == VDMAKEFOURCC('T', 'B', 'M', 'P'))
		Parse(src, srclen, alignment);
}

void VDImageDecoderIFF::ParseTBHD(const uint8 *src, uint32 srclen) {
	if (srclen < 20)
		throw MyError("Cannot read IFF file: Invalid tiled bitmap header.");

	// TBHD [32 bytes]
	// +0	uint32	width
	// +4	uint32	height
	// +8	uint16	unknown (saw 1)
	// +10	uint16	unknown (saw 1)
	// +12	uint16	unknown (saw 0)
	// +14	uint16	unknown (saw 3)
	// +16	uint32	tile count
	// +20	uint32	unknown (saw 1)
	// +24	uint32	unknown (saw 0)
	// +28	uint32	unknown (saw 0)

	mbFoundTBHD = true;
	mWidth	= VDReadUnalignedBEU32(src + 0);
	mHeight	= VDReadUnalignedBEU32(src + 4);

	mBuffer.init(mWidth, mHeight, nsVDPixmap::kPixFormat_XRGB8888);
}

void VDImageDecoderIFF::ParseRGBA(const uint8 *src, uint32 srclen) {
	if (!mbFoundTBHD)
		throw MyError("Cannot read IFF file: A tiled bitmap header (TBHD) was not found before the image tiles.");

	do {
		if (srclen < 8)
			break;

		uint16 x1 = VDReadUnalignedBEU16(src + 0);
		uint16 y1 = VDReadUnalignedBEU16(src + 2);
		uint16 x2 = VDReadUnalignedBEU16(src + 4);
		uint16 y2 = VDReadUnalignedBEU16(src + 6);

		src += 8;
		srclen -= 8;

		// validate tile rect
		if (x1 > x2 || y1 > y2)
			break;

		if (x2 >= mWidth || y2 >= mHeight)
			break;

		// determine compression by size
		ptrdiff_t pitch = mBuffer.pitch;
		uint8 *dstrow0 = (uint8 *)mBuffer.data + pitch*(mHeight - 1 - y1) + 4*x1;
		uint8 *dstrow = dstrow0;
		uint32 h = (uint32)(y2-y1)+1;
		uint32 w = (uint32)(x2-x1)+1;

		mImagePixelsFilled += w*h;

		uint32 planesize = w*h;
		if (srclen == planesize*4) {
			for(int y=0; y<h; ++y) {
				uint8 *dst = dstrow;
				for(int x=0; x<w; ++x) {
					dst[0] = src[1];
					dst[1] = src[2];
					dst[2] = src[3];
					dst[3] = src[0];
					dst += 4;
					src += 4;
				}

				dstrow -= pitch;
			}
			return;
		}

		// decompress tile
		uint32 limit = w*4;
		uint32 x = 0;
		uint32 y = 0;
		uint32 plane = 0;

		static const int kPlaneOrder[4]={3,0,1,2};		// ABGR

		for(;;) {
			if (srclen < 2)
				return;

			uint32 c = *src++;
			--srclen;

			if (c & 0x80) {
				c -= 0x7f;

				uint8 fill = *src++;
				--srclen;

				while(c--) {
					if (x >= limit) {
						dstrow -= pitch;
						x = 0;
						++y;
						if (y >= h) {
							y = 0;
							++plane;
							dstrow = dstrow0 + kPlaneOrder[plane];
							if (plane >= 4)
								throw MyError("Cannot read IFF file: An RLE decoding error occurred while reading tiled image data.");
						}
					}

					dstrow[x] = fill;
					x += 4;
				}
			} else {
				++c;

				if (srclen < c)
					return;

				srclen -= c;

				while(c--) {
					if (x >= limit) {
						dstrow -= pitch;
						x = 0;
						++y;
						if (y >= h) {
							y = 0;
							++plane;
							dstrow = dstrow0 + kPlaneOrder[plane];
							if (plane >= 4)
								throw MyError("Cannot read IFF file: An RLE decoding error occurred while reading tiled image data.");
						}
					}

					dstrow[x] = *src++;
					x += 4;
				}
			}
		}

		// check that we got everything just right
		if (x != limit || y != h-1 || plane != 3)
			throw MyError("Cannot read IFF file: An RLE decoding error occurred while reading tiled image data.");

		return;
	} while(false);

	throw MyError("Cannot read IFF file: Invalid tiled RGBA block.");
}
