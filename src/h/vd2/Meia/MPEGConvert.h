//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#ifndef f_VD2_MEIA_MPEGCONVERT_H
#define f_VD2_MEIA_MPEGCONVERT_H

#include <stddef.h>

// VDMPEGConverterSet
//
// Groups of blitter routines for converting MPEG-1's native YV12 colorspace to various more
// useful formats.  The source format used by the MPEG-1 decoder is a series of byte planes
// for Y, U (Cb), and V (Cr), all interleaved as follows:
//
// <----------------Y0----------------><-------U0------->
// <----------------Y1----------------><-------V0------->
// <----------------Y2----------------><-------U2------->
// <----------------Y3----------------><-------V2------->
//
// The blitter routines need not know that interleaving is occurring in this fashion, however,
// as the above can all be specified by playing with the bitmap strides.  All scanlines
// must be aligned to a 16-byte boundary, however.  Since MPEG-1 is macroblock-aligned
// regardless of the actual size of the encoded bitmap, this is not a problem in practice.
//
// All bitmap conversions are performed top-down, that is, the top-left pixel is pointed to
// by all four of the dst, srcY, srcCr (V), and srcCb (U) pointers.  Color space is regular
// CCIR.601, so some preconversion is necessary to accommodate the JPEG color space.
//
// o  All scanlines must be aligned to a 16 byte boundary.
// o  No surfaces may overlap, although they may be interleaved as described above.
// o  Color space is CCIR 601 (16-235 range for Y).
// o  Alpha channel is random for RGB15 (1555) and RGB32 (8888) formats.
// o  Chroma upsampling is not performed for speed.
// o  All converters must support odd heights.  The way the converters handle this efficiently
//    is that they convert pairs of scanlines at a time, and then on the last line, the
//    destination and source Y scanline pointers are collapsed together, causing the odd
//    scanline to be converted twice.

typedef void (*tVDMPEGConverter)(void *dst, ptrdiff_t dpitch, const unsigned char *srcY, ptrdiff_t Ypitch, const unsigned char *srcCr, const unsigned char *srcCb, ptrdiff_t Cpitch, int mbw, int height);

struct VDMPEGConverterSet {
	tVDMPEGConverter	DecodeUYVY;
	tVDMPEGConverter	DecodeYUYV;
	tVDMPEGConverter	DecodeYVYU;
	tVDMPEGConverter	DecodeY41P;
	tVDMPEGConverter	DecodeRGB15;
	tVDMPEGConverter	DecodeRGB16;
	tVDMPEGConverter	DecodeRGB24;
	tVDMPEGConverter	DecodeRGB32;
};

extern const VDMPEGConverterSet g_VDMPEGConvert_reference;
extern const VDMPEGConverterSet g_VDMPEGConvert_scalar;
extern const VDMPEGConverterSet g_VDMPEGConvert_mmx;
extern const VDMPEGConverterSet g_VDMPEGConvert_isse;

#endif
