//	VirtualDub - Video processing and capture application
//	Video decoding library
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

#include <vd2/system/error.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/zip.h>
#include <vd2/system/binary.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Meia/decode_png.h>
#include "common_png.h"
#include <stdio.h>

using namespace nsVDPNG;

///////////////////////////////////////////////////////////////////////////

namespace {
	unsigned long PNGDecodeNetwork32(const uint8 *src) {
		return (src[0]<<24) + (src[1]<<16) + (src[2]<<8) + src[3];
	}

	void PNGPredictSub(uint8 *row, int rowbytes, int bpp) {
		for(int i=bpp; i<rowbytes; ++i)
			row[i] += row[i-bpp];
	}

	void PNGPredictUp(uint8 *row, const uint8 *prevrow, int rowbytes, int bpp) {
		if (prevrow)
			for(int i=0; i<rowbytes; ++i)
				row[i] += prevrow[i];
	}

	void PNGPredictAverage(uint8 *row, const uint8 *prevrow, int rowbytes, int bpp) {
		if (prevrow) {
			for(int i=0; i<bpp; ++i)
				row[i] += prevrow[i]>>1;
			for(int j=bpp; j<rowbytes; ++j)
				row[j] += (prevrow[j] + row[j-bpp])>>1;
		} else {
			for(int j=bpp; j<rowbytes; ++j)
				row[j] += row[j-bpp]>>1;
		}
	}

	void PNGPredictPaeth(uint8 *row, const uint8 *prevrow, int rowbytes, int bpp) {
		int a, b, c, pa, pb, pc, p;
		if (prevrow) {
			for(int i=0; i<bpp; ++i)
				row[i] += prevrow[i];
			for(int j=bpp; j<rowbytes; ++j)
			{
				a = row[j-bpp];
				b = prevrow[j];
				c = prevrow[j-bpp];
				p = b - c;
				pc = a - c;
				pa = abs(p);
				pb = abs(pc);
				pc = abs(p + pc);
				row[j] += ((pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c);
			}
		} else {
			for(int j=bpp; j<rowbytes; ++j)
				row[j] += row[j-bpp];
		}
	}
}

#define notabc(c) ((c) < 65 || (c) > 122 || ((c) > 90 && (c) < 97))

#define ROWBYTES(pixel_bits, width) \
    ((pixel_bits) >= 8 ? \
    ((width) * (((uint32)(pixel_bits)) >> 3)) : \
    (( ((width) * ((uint32)(pixel_bits))) + 7) >> 3) )

class VDImageDecoderPNG : public IVDImageDecoderPNG {
public:
	PNGDecodeError Decode(const void *src0, uint32 size);
	const VDPixmap& GetFrameBuffer() { return mFrameBuffer; }
	bool IsAlphaPresent() const { return mbAlphaPresent; }

protected:
	VDPixmapBuffer	mFrameBuffer;
	bool mbAlphaPresent;

private:
	void Compose(uint8 * dst, uint8 * src, uint32 dstbytes, uint32 w, uint32 h, uint32 bop);

	uint8  pal[256][4];
	uint32 hasTRNS;
	uint16 trns1, trns2, trns3;
	uint8  coltype, depth, bpp;
	uint32 rowbytes;
};

IVDImageDecoderPNG *VDCreateImageDecoderPNG() {
	return new VDImageDecoderPNG;
}

void VDImageDecoderPNG::Compose(uint8 * dst, uint8 * src, uint32 dstbytes, uint32 w, uint32 h, uint32 bop) {
	uint32   i, j;
	uint32   r, g, b, a;
	uint32   r2, g2, b2, a2;
	uint8    col;
	uint8  * sp;
	uint32 * dp;
	uint8  * row = src;
	uint8  * out = dst;
	uint8  * prev_row = NULL;
	uint32   u, v, al;
	uint32   step = (depth+7)/8;

	const uint32 mask4[2]={240,15};
	const uint32 shift4[2]={4,0};

	const uint32 mask2[4]={192,48,12,3};
	const uint32 shift2[4]={6,4,2,0};

	const uint32 mask1[8]={128,64,32,16,8,4,2,1};
	const uint32 shift1[8]={7,6,5,4,3,2,1,0};

	for (j=0; j<h; j++)
	{
		switch (*row++)
		{
			case 0: break;
			case 1: PNGPredictSub(row, rowbytes, bpp); break;
			case 2: PNGPredictUp(row, prev_row, rowbytes, bpp); break;
			case 3: PNGPredictAverage(row, prev_row, rowbytes, bpp); break;
			case 4: PNGPredictPaeth(row, prev_row, rowbytes, bpp); break;
		}
		sp = row;
		dp = (uint32*)out;

		if (coltype == 6) // RGBA
		{
			if (bop == 0) // SOURCE
			{
				for (i=0; i<w; i++)
				{
					r = *sp; sp += step;
					g = *sp; sp += step;
					b = *sp; sp += step;
					a = *sp; sp += step;
					*dp++ = (a != 0) ? (a << 24) + (r << 16) + (g << 8) + b : 0;
				}
			}
			else // OVER
			{
				for (i=0; i<w; i++)
				{
					r = *sp; sp += step;
					g = *sp; sp += step;
					b = *sp; sp += step;
					a = *sp; sp += step;
					if (a == 255)
						*dp++ = (a << 24) + (r << 16) + (g << 8) + b;
					else
					if (a != 0)
					{
						if (a2 = (*dp)>>24)
						{
							u = a*255;
							v = (255-a)*a2;
							al = u + v;
							r2 = (((*dp)>>16)&255);
							g2 = (((*dp)>>8)&255);
							b2 = ((*dp)&255);
							r = (r*u + r2*v)/al;
							g = (g*u + g2*v)/al;
							b = (b*u + b2*v)/al;
							a = al/255;
						}
						*dp++ = (a << 24) + (r << 16) + (g << 8) + b;
					}
					else
						dp++;
				}
			}
		}
		else
		if (coltype == 4) // GA
		{
			if (bop == 0) // SOURCE
			{
				for (i=0; i<w; i++)
				{
					g = *sp; sp += step;
					a = *sp; sp += step;
					*dp++ = (a != 0) ? (a << 24) + (g << 16) + (g << 8) + g : 0;
				}
			}
			else // OVER
			{
				for (i=0; i<w; i++)
				{
					g = *sp; sp += step;
					a = *sp; sp += step;
					if (a == 255)
					{
						*dp++ = (a << 24) + (g << 16) + (g << 8) + g;
					}
					else
					if (a != 0)
					{
						if (a2 = (*dp)>>24)
						{
							u = a*255;
							v = (255-a)*a2;
							al = u + v;
							g2 = ((*dp)&255);
							g = (g*u + g2*v)/al;
							a = al/255;
						}
						*dp++ = (a << 24) + (g << 16) + (g << 8) + g;
					}
					else
						dp++;
				}
			}
		}
		else
		if (coltype == 3) // INDEXED
		{
			for (i=0; i<w; i++)
			{
				switch (depth)
				{
					case 8: col = sp[i]; break;
					case 4: col = (sp[i>>1] & mask4[i&1]) >> shift4[i&1]; break;
					case 2: col = (sp[i>>2] & mask2[i&3]) >> shift2[i&3]; break;
					case 1: col = (sp[i>>3] & mask1[i&7]) >> shift1[i&7]; break;
				}

				r = pal[col][0];
				g = pal[col][1];
				b = pal[col][2];
				a = pal[col][3];

				if (bop == 0) // SOURCE
				{
					*dp++ = (a << 24) + (r << 16) + (g << 8) + b;
				}
				else // OVER
				{
					if (a == 255)
						*dp++ = (a << 24) + (r << 16) + (g << 8) + b;
					else
					if (a != 0)
					{
						if (a2 = (*dp)>>24)
						{
							u = a*255;
							v = (255-a)*a2;
							al = u + v;
							r2 = (((*dp)>>16)&255);
							g2 = (((*dp)>>8)&255);
							b2 = ((*dp)&255);
							r = (r*u + r2*v)/al;
							g = (g*u + g2*v)/al;
							b = (b*u + b2*v)/al;
							a = al/255;
						}
						*dp++ = (a << 24) + (r << 16) + (g << 8) + b;
					}
					else
						dp++;
				}
			}
		}
		else
		if (coltype == 2) // RGB
		{
			if (bop == 0) // SOURCE
			{
				if (depth == 8)
				{
					for (i=0; i<w; i++)
					{
						r = *sp++;
						g = *sp++;
						b = *sp++;
						if (hasTRNS && r==trns1 && g==trns2 && b==trns3)
							*dp++ = 0;
						else
							*dp++ = 0xFF000000 + (r << 16) + (g << 8) + b;
					}
				}
				else
				{
					for (i=0; i<w; i++, sp+=6)
					{
						r = *sp;
						g = *(sp+2);
						b = *(sp+4);
						if (hasTRNS && VDReadUnalignedBEU16(sp)==trns1 && VDReadUnalignedBEU16(sp+2)==trns2 && VDReadUnalignedBEU16(sp+4)==trns3)
							*dp++ = 0;
						else
							*dp++ = 0xFF000000 + (r << 16) + (g << 8) + b;
					}
				}
			}
			else // OVER
			{
				if (depth == 8)
				{
					for (i=0; i<w; i++, sp+=3, dp++)
						if ((*sp != trns1) || (*(sp+1) != trns2) || (*(sp+2) != trns3))
							*dp = 0xFF000000 + (*sp << 16) + (*(sp+1) << 8) + *(sp+2);
				}
				else
				{
					for (i=0; i<w; i++, sp+=6, dp++)
						if ((VDReadUnalignedBEU16(sp) != trns1) || (VDReadUnalignedBEU16(sp+2) != trns2) || (VDReadUnalignedBEU16(sp+4) != trns3))
							*dp = 0xFF000000 + (*sp << 16) + (*(sp+2) << 8) + *(sp+4);
				}
			}
		}
		else
		if (coltype == 0) // Gray
		{
			if (bop == 0) // SOURCE
			{
				switch (depth)
				{
					case 16: for (i=0; i<w; i++) { if (hasTRNS && VDReadUnalignedBEU16(sp)==trns1) *dp++ = 0; else *dp++ = 0xFF000000 + (*sp << 16) + (*sp << 8) + *sp; sp+=2; }  break;
					case 8:  for (i=0; i<w; i++) { if (hasTRNS && *sp==trns1)                      *dp++ = 0; else *dp++ = 0xFF000000 + (*sp << 16) + (*sp << 8) + *sp; sp++;  }  break;
					case 4:  for (i=0; i<w; i++) { g = (sp[i>>1] & mask4[i&1]) >> shift4[i&1]; if (hasTRNS && g==trns1) *dp++ = 0; else *dp++ = 0xFF000000 + g*0x111111; } break;
					case 2:  for (i=0; i<w; i++) { g = (sp[i>>2] & mask2[i&3]) >> shift2[i&3]; if (hasTRNS && g==trns1) *dp++ = 0; else *dp++ = 0xFF000000 + g*0x555555; } break;
					case 1:  for (i=0; i<w; i++) { g = (sp[i>>3] & mask1[i&7]) >> shift1[i&7]; if (hasTRNS && g==trns1) *dp++ = 0; else *dp++ = 0xFF000000 + g*0xFFFFFF; } break;
				}
			}
			else // OVER
			{
				switch (depth)
				{
					case 16: for (i=0; i<w; i++, dp++) { if (VDReadUnalignedBEU16(sp) != trns1) { *dp = 0xFF000000 + (*sp << 16) + (*sp << 8) + *sp; } sp+=2; } break;
					case 8:  for (i=0; i<w; i++, dp++) { if (*sp != trns1)                      { *dp = 0xFF000000 + (*sp << 16) + (*sp << 8) + *sp; } sp++;  } break;
					case 4:  for (i=0; i<w; i++, dp++) { g = (sp[i>>1] & mask4[i&1]) >> shift4[i&1]; if (g != trns1) *dp = 0xFF000000 + g*0x111111; } break;
					case 2:  for (i=0; i<w; i++, dp++) { g = (sp[i>>2] & mask2[i&3]) >> shift2[i&3]; if (g != trns1) *dp = 0xFF000000 + g*0x555555; } break;
					case 1:  for (i=0; i<w; i++, dp++) { g = (sp[i>>3] & mask1[i&7]) >> shift1[i&7]; if (g != trns1) *dp = 0xFF000000 + g*0xFFFFFF; } break;
				}
			}
		}

		prev_row = row;
		row += rowbytes;
		out += dstbytes;
	}
}

PNGDecodeError VDImageDecoderPNG::Decode(const void *src0, uint32 size) {
	const uint8 *src = (const uint8 *)src0;
	const uint8 *const src_end = src+size;
	uint32 i, w, h, w0, h0, x0, y0;
	uint32 len, chunk, crc, frames, num_fctl, num_idat;
	uint8  compr, filter, interl, pixeldepth, dop, bop;

	frames = 1;
	num_fctl = 0;
	num_idat = 0;
	hasTRNS = 0;
	x0 = 0;
	y0 = 0;
	bop = 0;

	for (i=0; (i<256); i++)
	{
		pal[i][0] = i;
		pal[i][1] = i;
		pal[i][2] = i;
		pal[i][3] = 0xFF;
	}

	if (size < 8)
		return kPNGDecodeNotPNG;

	if (memcmp(src, kPNGSignature, 8))
		return kPNGDecodeNotPNG;

	src += 8;

	// decode chunks
	VDCRCChecker checker;

	if (src + 25 > src_end)
		return kPNGDecodeBadHeader;

	len		= PNGDecodeNetwork32(src);
	chunk	= PNGDecodeNetwork32(src + 4);

	if ((len != 13) || (chunk != 'IHDR'))
		return kPNGDecodeBadHeader;

	w = w0  = PNGDecodeNetwork32(src+8);
	h = h0  = PNGDecodeNetwork32(src+12);
	depth   = src[16];
	coltype = src[17];
	compr   = src[18];
	filter  = src[19];
	interl  = src[20];
	crc = PNGDecodeNetwork32(src + 21);

	checker.Init(VDCRCChecker::kCRC32);
	checker.Process(src + 4, len + 4);
	if (checker.CRC() != crc)
		return kPNGDecodeChecksumFailed;

	if (depth != 1 && depth != 2 && depth != 4 && depth != 8 && depth != 16)
		return kPNGDecodeUnsupported;
	if (coltype != 0 && coltype != 2 && coltype != 3 && coltype != 4 && coltype != 6)
		return kPNGDecodeUnsupported;
	if (coltype == 3 && depth > 8)
		return kPNGDecodeUnsupported;
	if (coltype != 0 && coltype != 3 && depth < 8)
		return kPNGDecodeUnsupported;
	if (compr != 0)
		return kPNGDecodeUnsupportedCompressionAlgorithm;
	if (filter != 0)
		return kPNGDecodeUnsupportedFilterAlgorithm;
	if (interl > 1)
		return kPNGDecodeUnsupportedInterlacingAlgorithm;

	pixeldepth = depth;
	if (coltype == 2)
		pixeldepth = depth*3;
	else
	if (coltype == 4)
		pixeldepth = depth*2;
	else
	if (coltype == 6)
		pixeldepth = depth*4;

	bpp = (pixeldepth + 7) >> 3;
	rowbytes = ROWBYTES(pixeldepth, w);

	mbAlphaPresent = (coltype >= 4);

	mFrameBuffer.init(w, h, nsVDPixmap::kPixFormat_XRGB8888);
	vdblock<uint8> dstbuf((rowbytes + 1) * h);
	vdfastvector<uint8> packeddata;

	src += 25;

	while (src + 8 <= src_end) {
		len   = PNGDecodeNetwork32(src);
		chunk = PNGDecodeNetwork32(src + 4);

		if (src_end < src + len + 12)
			return kPNGDecodeTruncatedChunk;

		crc   = PNGDecodeNetwork32(src + len + 8);

		// verify the crc
		checker.Init(VDCRCChecker::kCRC32);
		checker.Process(src + 4, len + 4);
		if (checker.CRC() != crc)
			return kPNGDecodeChecksumFailed;

		if (chunk == 'PLTE') {
			if (len%3)
				return kPNGDecodeBadPalette;

			for (i=0; (i<len/3 && i<256); i++)
			{
				pal[i][0] = src[8+i*3];
				pal[i][1] = src[8+i*3+1];
				pal[i][2] = src[8+i*3+2];
				pal[i][3] = 0xFF;
			}
		} else if (chunk == 'tRNS') {
			if (coltype == 0)
			{
				trns1 = VDReadUnalignedBEU16(&src[8]);
			}
			else
			if (coltype == 2)
			{
				trns1 = VDReadUnalignedBEU16(&src[8]);
				trns2 = VDReadUnalignedBEU16(&src[10]);
				trns3 = VDReadUnalignedBEU16(&src[12]);
			}
			else
			if (coltype == 3)
			{
				for (i=0; (i<len && i<256); i++)
					pal[i][3] = src[8+i];
			}
			hasTRNS = 1;
			mbAlphaPresent = true;
		} else if (chunk == 'acTL') {
			frames = PNGDecodeNetwork32(src + 8);
			mFrameBuffer.init(w, h*frames, nsVDPixmap::kPixFormat_XRGB8888);
		} else if (chunk == 'fcTL') {
			if ((num_fctl == num_idat) && (num_idat > 0))
			{
				if (dop == 2) // PREV
					VDMemcpyRect((char *)mFrameBuffer.data + num_idat*h*mFrameBuffer.pitch, mFrameBuffer.pitch,
					             (char *)mFrameBuffer.data + (num_idat-1)*h*mFrameBuffer.pitch, mFrameBuffer.pitch, w*4, h);

				VDMemoryStream packedStream(&packeddata[2], packeddata.size() - 6);
				VDZipStream unpackedStream(&packedStream, packeddata.size() - 6, false);

				try {
					unpackedStream.Read(dstbuf.data(), (rowbytes + 1) * h0);
				} catch(const MyError&) {
					return kPNGDecodeDecompressionFailed;
				}

				// check image data
				uint32 adler32 = VDReadUnalignedBEU32(packeddata.data() + packeddata.size() - 4);
				if (adler32 != VDAdler32Checker::Adler32(dstbuf.data(), (rowbytes + 1) * h0))
					return kPNGDecodeChecksumFailed;

				Compose((uint8 *)mFrameBuffer.data + (y0+(num_idat-1)*h)*mFrameBuffer.pitch + x0*4, dstbuf.data(), mFrameBuffer.pitch, w0, h0, bop);

				if (dop != 2) // PREV
				{
					VDMemcpyRect((char *)mFrameBuffer.data + num_idat*h*mFrameBuffer.pitch, mFrameBuffer.pitch, (char *)mFrameBuffer.data + (num_idat-1)*h*mFrameBuffer.pitch, mFrameBuffer.pitch, w*4, h);

					if (dop == 1) // BACK
						VDMemset32Rect((char *)mFrameBuffer.data + (y0+num_idat*h)*mFrameBuffer.pitch + x0*4, mFrameBuffer.pitch, 0, w0, h0);
				}
			}

			w0 = PNGDecodeNetwork32(src + 12);
			h0 = PNGDecodeNetwork32(src + 16);
			x0 = PNGDecodeNetwork32(src + 20);
			y0 = PNGDecodeNetwork32(src + 24);
			dop = src[32];
			bop = src[33];

			if (num_fctl == 0)
			{
				bop = 0;
				if (dop == 2) // PREV
					dop = 1;  // BACK
			}

			if (!(coltype & 4) && !(hasTRNS))
				bop = 0;

			rowbytes = ROWBYTES(pixeldepth, w0);
			num_fctl++;
		} else if (chunk == 'IDAT') {
			if (num_fctl > num_idat)
			{
				packeddata.clear();
				num_idat++;
			}
			packeddata.resize(packeddata.size()+len);
			memcpy(&packeddata[packeddata.size()-len], src+8, len);
		} else if (chunk == 'fdAT') {
			if (num_fctl > num_idat)
			{
				packeddata.clear();
				num_idat++;
			}
			packeddata.resize(packeddata.size()+len-4);
			memcpy(&packeddata[packeddata.size()-len+4], src+12, len-4);
		} else if (chunk == 'IEND') {
			break;
		} else {
			if (notabc(src[4]))
				return kPNGDecodeTruncatedChunk;
			if (notabc(src[5]))
				return kPNGDecodeTruncatedChunk;
			if (notabc(src[6]))
				return kPNGDecodeTruncatedChunk;
			if (notabc(src[7]))
				return kPNGDecodeTruncatedChunk;
		}

		src += len+12;
	}

	if (num_idat == 0)
		num_idat++;

	VDMemoryStream packedStream(&packeddata[2], packeddata.size() - 6);
	VDZipStream unpackedStream(&packedStream, packeddata.size() - 6, false);

	try {
		unpackedStream.Read(dstbuf.data(), (rowbytes + 1) * h0);
	} catch(const MyError&) {
		return kPNGDecodeDecompressionFailed;
	}

	// check image data
	uint32 adler32 = VDReadUnalignedBEU32(packeddata.data() + packeddata.size() - 4);
	if (adler32 != VDAdler32Checker::Adler32(dstbuf.data(), (rowbytes + 1) * h0))
		return kPNGDecodeChecksumFailed;

	Compose((uint8 *)mFrameBuffer.data + (y0+(num_idat-1)*h)*mFrameBuffer.pitch + x0*4, dstbuf.data(), mFrameBuffer.pitch, w0, h0, bop);

	return kPNGDecodeOK;
}

const char *PNGGetErrorString(PNGDecodeError err) {
	switch(err) {
	case kPNGDecodeOK:									return "No error.";
	case kPNGDecodeNotPNG:								return "Not a PNG file.";
	case kPNGDecodeTruncatedChunk:						return "A chunk in the PNG file is damaged.";
	case kPNGDecodeBadHeader:							return "The PNG header is invalid.";
	case kPNGDecodeUnsupportedCompressionAlgorithm:		return "The compression algorithm used by the PNG file is not supported.";
	case kPNGDecodeUnsupportedInterlacingAlgorithm:		return "The interlacing algorithm used by the PNG file is not supported.";
	case kPNGDecodeUnsupportedFilterAlgorithm:			return "The filtering algorithm used by the PNG file is not supported.";
	case kPNGDecodeBadPalette:							return "The PNG palette structure is bad.";
	case kPNGDecodeDecompressionFailed:					return "A decompression error occurred while unpacking the raw PNG image data.";
	case kPNGDecodeBadFilterMode:						return "The image data specifies an unknown PNG filtering mode.";
	case kPNGDecodeUnknownRequiredChunk:				return "The PNG file contains data that is required to decode the image but which this decoder does not support.";
	case kPNGDecodeChecksumFailed:						return "A chunk in the PNG file is corrupted (bad checksum).";
	case kPNGDecodeUnsupported:							return "The PNG file appears to be valid, but uses an encoding mode that is not supported.";
	default:											return "?";
	}
}

bool VDDecodePNGHeader(const void *src0, uint32 size, int& w, int& h, bool& hasalpha, int& frames) {
	const uint8 *src = (const uint8 *)src0;
	const uint8 *const src_end = src+size;

	if (size < 45)
		return false;

	if (memcmp(src, kPNGSignature, 8))
		return false;

	src += 8;

	uint32 len   = PNGDecodeNetwork32(src);
	uint32 chunk = PNGDecodeNetwork32(src+4);

	if ((len != 13) || (chunk != 'IHDR'))
		return false;

	w = PNGDecodeNetwork32(src+8);
	h = PNGDecodeNetwork32(src+12);
	uint8  coltype = src[17];
	uint8  hastrns = 0;
	frames  = 1;

	src += 25;

	while(src < src_end) {
		if (notabc(src[4])) break;
		if (notabc(src[5])) break;
		if (notabc(src[6])) break;
		if (notabc(src[7])) break;
		len   = PNGDecodeNetwork32(src);
		chunk = PNGDecodeNetwork32(src + 4);
		src += 8;
		if (src_end < src+len+4)
			break;

		if (chunk == 'tRNS') {
			hastrns = 1;
		} else if (chunk == 'acTL') {
			frames = PNGDecodeNetwork32(src);
		} else if (chunk == 'IDAT') {
			break;
		} else if (chunk == 'fDAT') {
			break;
		} else if (chunk == 'IEND') {
			break;
		}
		src += len+4;
		if (src_end < src+8)
			break;
	}

	if (frames > 1)
		h *= frames;

	hasalpha = (coltype >= 4) || (hastrns);

	return true;
}
