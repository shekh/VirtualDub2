//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2002 Avery Lee
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
#include <windows.h>
#include "VBitmap.h"
#include <vd2/system/error.h>
#include <vd2/system/file.h>
#include <vd2/system/vdalloc.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Meia/decode_png.h>
#include <vd2/Dita/resources.h>
#include "imagejpegdec.h"

namespace {
	enum { kVDST_PNGDecodeErrors = 100 };
}

void ConvertOldHeader(BITMAPINFOHEADER& newhdr, const BITMAPCOREHEADER& oldhdr) {
	newhdr.biSize			= sizeof(BITMAPINFOHEADER);
	newhdr.biWidth			= oldhdr.bcWidth;
	newhdr.biHeight			= oldhdr.bcHeight;
	newhdr.biPlanes			= oldhdr.bcPlanes;
	newhdr.biCompression	= BI_RGB;
	newhdr.biBitCount		= oldhdr.bcBitCount;
	newhdr.biSizeImage		= ((newhdr.biWidth * newhdr.biBitCount + 31)>>5)*4 * abs(newhdr.biHeight);
	newhdr.biXPelsPerMeter	= 0;
	newhdr.biYPelsPerMeter	= 0;

	int palents = 0;
	if (oldhdr.bcBitCount <= 8)
		palents = 1 << oldhdr.bcBitCount;

	newhdr.biClrUsed		= palents;
	newhdr.biClrImportant	= palents;
}

bool DecodeBMPHeader(const void *pBuffer, long cbBuffer, int& w, int& h, bool& bHasAlpha) {
	const BITMAPFILEHEADER *pbfh = (const BITMAPFILEHEADER *)pBuffer;

	// Check file header.

	if (cbBuffer < sizeof(BITMAPFILEHEADER) + sizeof(DWORD) || pbfh->bfType != 'MB')
		return false;

	// Some apps don't get bfSize correct, and seems no one else validates it. Boo.
	// if (pbfh->bfSize > cbBuffer || pbfh->bfOffBits > cbBuffer)

	if (pbfh->bfOffBits > cbBuffer)
		throw MyError("Image file is too short.");

	const BITMAPINFOHEADER *pbih = (const BITMAPINFOHEADER *)((char *)pBuffer + sizeof(BITMAPFILEHEADER));

	if (pbih->biSize + sizeof(BITMAPFILEHEADER) > cbBuffer)
		throw MyError("Image file is too short.");

	BITMAPINFOHEADER bihTemp;

	if (pbih->biSize == sizeof(BITMAPCOREHEADER)) {
		const BITMAPCOREHEADER *pbch = (const BITMAPCOREHEADER *)pbih;

		ConvertOldHeader(bihTemp, *pbch);
		pbih = &bihTemp;
	}

	do {
		if (pbih->biSize < sizeof(BITMAPINFOHEADER) || pbih->biPlanes > 1 || pbih->biCompression != BI_RGB)
			break;

		int depthM1 = pbih->biBitCount - 1;
		if (depthM1 & ~31)
			break;

		static const uint32 kValidMask
				= (1 << ( 1 - 1))
				| (1 << ( 2 - 1))
				| (1 << ( 4 - 1))
				| (1 << ( 8 - 1))
				| (1 << (16 - 1))
				| (1 << (24 - 1))
				| (1 << (32 - 1));

		if (!(kValidMask & (1 << depthM1)))
			break;

		// Verify that the image is all there.
		if (pbfh->bfOffBits + ((pbih->biWidth*pbih->biBitCount+31)>>5)*4*abs(pbih->biHeight) > cbBuffer)
			throw MyError("Bitmap file is incomplete.");

		w = pbih->biWidth;
		h = abs(pbih->biHeight);
		bHasAlpha = false;

		return true;
	} while(0);

	throw MyError("Bitmap file is in an unsupported format (not 1/2/4/8/16/24/32 bit).");
}

void DecodeBMP(const void *pBuffer, long cbBuffer, const VDPixmap& vb) {
	// Blit the image to the framebuffer.

	const BITMAPFILEHEADER *pbfh = (const BITMAPFILEHEADER *)pBuffer;
	const BITMAPINFOHEADER *pbih = (const BITMAPINFOHEADER *)((char *)pBuffer + sizeof(BITMAPFILEHEADER));

	BITMAPINFOHEADER bihTemp;

	if (pbih->biSize == sizeof(BITMAPCOREHEADER)) {
		const BITMAPCOREHEADER *pbch = (const BITMAPCOREHEADER *)pbih;

		ConvertOldHeader(bihTemp, *pbch);
		pbih = &bihTemp;
	}

	using namespace nsVDPixmap;

	VDPixmap px;

	px.data		= (char *)pBuffer + pbfh->bfOffBits;
	px.data2	= NULL;
	px.data3	= NULL;

	px.pitch	= ((pbih->biWidth * pbih->biBitCount + 31) >> 5) * 4;
	px.pitch2	= 0;
	px.pitch3	= 0;

	switch(pbih->biBitCount) {
	case 1:		px.format = kPixFormat_Pal1; break;
	case 2:		px.format = kPixFormat_Pal2; break;
	case 4:		px.format = kPixFormat_Pal4; break;
	case 8:		px.format = kPixFormat_Pal8; break;
	case 16:	px.format = kPixFormat_XRGB1555; break;
	case 24:	px.format = kPixFormat_RGB888; break;
	case 32:	px.format = kPixFormat_XRGB8888; break;
	}

	px.w		= pbih->biWidth;

	if (pbih->biHeight < 0) {
		px.h = -pbih->biHeight;
	} else {
		px.h = pbih->biHeight;

		VDPixmapFlipV(px);
	}

	uint32 pal[256];
	px.palette = pal;

	if (pbih->biBitCount <= 8) {
		int palsize = pbih->biClrUsed;
		if (!palsize)
			palsize = 1<<pbih->biBitCount;

		memcpy(pal, (char *)pbih + sizeof(BITMAPINFOHEADER), 4 * std::min<size_t>(palsize, 256));
	}

	VDVERIFY(VDPixmapBlt(vb, px));
}

struct TGAHeader {
	unsigned char	IDLength;
	unsigned char	CoMapType;
	unsigned char	ImgType;
	unsigned char	IndexLo, IndexHi;
	unsigned char	LengthLo, LengthHi;
	unsigned char	CoSize;
	unsigned char	X_OrgLo, X_OrgHi;
	unsigned char	Y_OrgLo, Y_OrgHi;
	unsigned char	WidthLo, WidthHi;
	unsigned char	HeightLo, HeightHi;
	unsigned char	PixelSize;
	unsigned char	AttBits;
};

bool DecodeTGAHeader(const void *pBuffer, long cbBuffer, int& w, int& h, bool& bHasAlpha) {
	const TGAHeader& hdr = *(const TGAHeader *)pBuffer;

	if (cbBuffer < sizeof(TGAHeader))
		return false;		// too short

	// Look for the TARGA signature at the end of the file.  If we
	// find this we know the file is TARGA and can apply strict
	// checks.  Otherwise, assume old TARGA and apply loose checks.
	bool bVerified = (!memcmp((const char *)pBuffer + cbBuffer - 18, "TRUEVISION-XFILE.", 18));

	if (hdr.ImgType != 2 && hdr.ImgType != 10) {
		if (bVerified)
			throw MyError("TARGA file must be true-color or RLE true-color.");
		return false;		// not true-color
	}

	if (hdr.PixelSize != 16 && hdr.PixelSize != 24 && hdr.PixelSize != 32) {
		if (bVerified)
			throw MyError("TARGA file must be 16-bit, 24-bit, or 32-bit.");
		return false;		// not 24-bit pixels
	}

	if (hdr.AttBits & 0x10) {
		if (bVerified)
			throw MyError("Right-to-left TARGA files not supported.");
		return false;		// right-to-left not supported
	}

	switch(hdr.AttBits & 0xf) {
	case 0:		// Zero alpha bits is always valid.
		break;
	case 1:		// One alpha bit is permitted for 16-bit.
		if (hdr.PixelSize != 16)
			throw MyError("TARGA decoder: 1-bit alpha supported only with 16-bit RGB.");
		break;
	case 8:		// 8-bit alpha is permitted for 32-bit.
		if (hdr.PixelSize != 32)
			throw MyError("TARGA decoder: 8-bit alpha supported only with 32-bit RGB.");
		break;
	}

	w = hdr.WidthLo + (hdr.WidthHi << 8);
	h = hdr.HeightLo + (hdr.HeightHi << 8);
	bHasAlpha = (hdr.AttBits & 0xf) > 0;

	return true;
}

static void BitBltAlpha(const VDPixmap& dst, int dx, int dy, const VDPixmap& src, int sx, int sy, int w, int h, bool bSrcHasAlpha) {
	if (src.format == nsVDPixmap::kPixFormat_XRGB1555 && dst.format == nsVDPixmap::kPixFormat_XRGB8888 && bSrcHasAlpha) {
		const uint16 *psrc = (const uint16 *)vdptroffset(src.data, src.pitch * sy) + sx;
		uint32 *pdst = (uint32 *)vdptroffset(dst.data, dst.pitch * dy) + dx;

		if (w && h) do {
			for(int x=0; x<w; ++x) {
				uint32 px = psrc[x];
				uint32 px2 = ((px & 0x7c00) << 9) + ((px & 0x03e0) << 6) + ((px & 0x001f) << 3);

				pdst[x] = px2 + ((px2 & 0xe0e0e0)>>5) + (px&0x8000?0xff000000:0);
			}

			vdptrstep(psrc, src.pitch);
			vdptrstep(pdst, dst.pitch);
		} while(--h);
	} else
		VDPixmapBlt(dst, dx, dy, src, sx, sy, w, h);
}

void DecodeTGA(const void *pBuffer, long cbBuffer, const VDPixmap& vb) {
	const TGAHeader& hdr = *(const TGAHeader *)pBuffer;
	const unsigned char *src = (const unsigned char *)pBuffer + sizeof(hdr) + hdr.IDLength;
	const unsigned char *srcLimit = (const unsigned char *)pBuffer + cbBuffer;
	const int w = hdr.WidthLo + (hdr.WidthHi << 8);
	const int h = hdr.HeightLo + (hdr.HeightHi << 8);

	VDPixmap vbSrc;
	int bpp = (hdr.PixelSize+7) >> 3;		// TARGA doesn't have a 565 mode, only 555 and 1555
	bool bSrcHasAlpha = (hdr.AttBits&0xf) != 0;

	const int kFormats[3]={
		nsVDPixmap::kPixFormat_XRGB1555,
		nsVDPixmap::kPixFormat_RGB888,
		nsVDPixmap::kPixFormat_XRGB8888,
	};

	if (hdr.ImgType == 2) {
		vbSrc.data = (uint32 *)src;
		vbSrc.w = w;
		vbSrc.h = h;
		vbSrc.format = kFormats[bpp - 2];
		vbSrc.pitch = bpp * vbSrc.w;

		if (!(hdr.AttBits & 0x20)) {
			vdptrstep(vbSrc.data, vbSrc.pitch * (h-1));
			vbSrc.pitch = -vbSrc.pitch;
		}

		BitBltAlpha(vb, 0, 0, vbSrc, 0, 0, w, h, bSrcHasAlpha);

	} else if (hdr.ImgType == 10) {
		vdblock<uint8> rowbuffer(bpp * w + 1);
		unsigned char *rowbuf = rowbuffer.data();

		vbSrc.data = rowbuf;
		vbSrc.w = w;
		vbSrc.h = 1;
		vbSrc.format = kFormats[bpp - 2];
		vbSrc.pitch = 0;

		// This version allows RLE packets to span rows (illegal).
		unsigned char *dst = rowbuf;
		unsigned char *dstEnd = rowbuf + bpp*w;

		for(int y=0; y<h; ) {
			if (src >= srcLimit)
				throw MyError("TARGA RLE decoding error");
			unsigned c = *src++;
			const unsigned char *copysrc;

			// we always copy one pixel
			dst[0] = src[0];
			dst[1] = src[1];
			src += 2;
			dst += 2;
			for(int k=0; k<bpp-2; ++k)
				*dst++ = *src++;

			if (c & 0x80)				// run
				copysrc = dst - bpp;
			else {						// lit
				copysrc = src;
				src += bpp * c;
			}

			c &= 0x7f;

			if (dst == dstEnd) {
				if (hdr.AttBits & 0x20)
					BitBltAlpha(vb, 0, y, vbSrc, 0, 0, w, 1, bSrcHasAlpha);
				else
					BitBltAlpha(vb, 0, h-1-y, vbSrc, 0, 0, w, 1, bSrcHasAlpha);

				dst = rowbuf;
				if (++y >= h) {
					if (c)
						throw MyError("TARGA RLE decoding error");
					break;
				}
			}

			if (c) {
				c *= bpp;

				do {
					*dst++ = *copysrc++;

					if (copysrc == dstEnd)
						copysrc = rowbuf;
					if (dst == dstEnd) {
						if (hdr.AttBits & 0x20)
							BitBltAlpha(vb, 0, y, vbSrc, 0, 0, w, 1, bSrcHasAlpha);
						else
							BitBltAlpha(vb, 0, h-1-y, vbSrc, 0, 0, w, 1, bSrcHasAlpha);
						dst = rowbuf;
						if (++y >= h) {
							if (c > 1)
								throw MyError("TARGA RLE decoding error");
							break;
						}
					}
				} while(--c);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////

bool VDIsJPEGHeader(const void *pv, uint32 len) {
	const uint8 *buf = (const uint8 *)pv;

	if (len >= 32 && buf[0] == 0xFF && buf[1] == 0xD8) {	// x'FF' SOI
		if (buf[2] == 0xFF && buf[3] == 0xE0) {		// x'FF' APP0
			// Hmm... might be a JPEG image.  Check for JFIF tag.
			if (buf[6] == 'J' && buf[7] == 'F' && buf[8] == 'I' && buf[9] == 'F')
				return true;
		}
		if (buf[2] == 0xFF && buf[3] == 0xE1) {		// x'FF' APP1
			// Hmm... might be a JPEG image.  Check for JFIF tag.
			if (buf[6] == 'E' && buf[7] == 'x' && buf[8] == 'i' && buf[9] == 'f')
				return true;
		}

		// Might be a bare JPEG. Look for the start of a second marker and xFFD9 at the end.
		if (buf[2] == 0xFF && buf[len - 2] == 0xFF && buf[len - 1] == 0xD9)
			return true;
	}
	return false;
}

bool VDIsMayaIFFHeader(const void *pv, uint32 len) {
	const uint8 *buf = (const uint8 *)pv;

	if (buf[0] == 'F' && buf[1] == 'O' && buf[2] == 'R' && buf[3] == '4' &&
		buf[8] == 'C' && buf[9] == 'I' && buf[10] == 'M' && buf[11] == 'G')
	{
		return true;
	}

	return false;
}

void DecodeImage(const void *pBuffer, long cbBuffer, VDPixmapBuffer& vb, int desired_format, bool& bHasAlpha) {
	int w, h;

	bool bIsPNG = false;
	bool bIsJPG = false;
	bool bIsBMP = false;
	bool bIsTGA = false;

	bIsPNG = VDDecodePNGHeader(pBuffer, cbBuffer, w, h, bHasAlpha);
	if (!bIsPNG) {
		bIsJPG = VDIsJPEGHeader(pBuffer, cbBuffer);
		if (!bIsJPG) {
			bIsBMP = DecodeBMPHeader(pBuffer, cbBuffer, w, h, bHasAlpha);
			if (!bIsBMP) {
				bIsTGA = DecodeTGAHeader(pBuffer, cbBuffer, w, h, bHasAlpha);
			}
		}
	}

	if (!bIsBMP && !bIsTGA && !bIsJPG && !bIsPNG)
		throw MyError("Image file must be in PNG, Windows BMP, truecolor TARGA format, or sequential JPEG format.");

	vdautoptr<IVDJPEGDecoder> pDecoder;

	if (bIsJPG) {
		pDecoder = VDCreateJPEGDecoder();
		pDecoder->Begin(pBuffer, cbBuffer);
		pDecoder->DecodeHeader(w, h);
	}

	vb.init(w, h, desired_format);

	if (bIsJPG) {
		int format;

		switch(vb.format) {
			case nsVDPixmap::kPixFormat_XRGB1555:	format = IVDJPEGDecoder::kFormatXRGB1555;	break;
			case nsVDPixmap::kPixFormat_RGB888:		format = IVDJPEGDecoder::kFormatRGB888;		break;
			case nsVDPixmap::kPixFormat_XRGB8888:	format = IVDJPEGDecoder::kFormatXRGB8888;	break;
		}

		pDecoder->DecodeImage(vb.data, vb.pitch, format);
		pDecoder->End();
	}

	if (bIsBMP)
		DecodeBMP(pBuffer, cbBuffer, vb);

	if (bIsTGA)
		DecodeTGA(pBuffer, cbBuffer, vb);

	if (bIsPNG) {
		vdautoptr<IVDImageDecoderPNG> pPNGDecoder(VDCreateImageDecoderPNG());

		PNGDecodeError err = pPNGDecoder->Decode(pBuffer, cbBuffer);
		if (err) {
			if (err == kPNGDecodeOutOfMemory)
				throw MyMemoryError();

			vdfastvector<wchar_t> errBuf;

			throw MyError("Error decoding PNG image: %ls", VDLoadString(0, kVDST_PNGDecodeErrors, err));
		}

		VDPixmapBlt(vb, pPNGDecoder->GetFrameBuffer());
	}
}

void DecodeImage(const char *pszFile, VDPixmapBuffer& buf, int desired_format, bool& bHasAlpha) {
	VDFile f;

	f.open(pszFile);

	sint64 flen = f.size();
	if (flen > 0x7FFFFFFF)
		throw MyError("Image file \"%s\" is too large to read (>2GB!).\n");

	vdblock<uint8> buffer;
	buffer.resize((uint32)flen);

	f.read(buffer.data(), buffer.size());
	f.close();

	DecodeImage(buffer.data(), buffer.size(), buf, desired_format, bHasAlpha);
}
