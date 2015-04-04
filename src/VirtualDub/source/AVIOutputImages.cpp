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

#include <vd2/system/error.h>
#include <vd2/Meia/encode_png.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Riza/bitmap.h>
#include <vd2/libav_tiff/tiff_image.h>
#include "AVIOutput.h"
#include "AVIOutputImages.h"
#include "imagejpeg.h"

class AVIOutputImages;

///////////////////////////////////////////////////////////////////////////////////////////////////

namespace {
	void CopyRow_X1R15(void *dst0, const void *src0, size_t n) {
		memcpy(dst0,src0,n*2);
	}

	void CopyRow_X8R24(void *dst0, const void *src0, size_t n) {
		memcpy(dst0,src0,n*4);
	}

	void CopyRowSetAlpha_X1R15(void *dst0, const void *src0, size_t n) {
		uint16 *dst = (uint16 *)dst0;
		const uint16 *src = (const uint16 *)src0;
		for(size_t i=0; i<n; ++i)
			dst[i] = src[i] | 0x8000;
	}

	void CopyRowSetAlpha_X8R24(void *dst0, const void *src0, size_t n) {
		uint32 *dst = (uint32 *)dst0;
		const uint32 *src = (const uint32 *)src0;
		for(size_t i=0; i<n; ++i)
			dst[i] = src[i] | 0xFF000000;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class AVIVideoImageOutputStream : public AVIOutputStream, public IVDVideoImageOutputStream {
private:
	DWORD dwFrame;
	const wchar_t *mpszPrefix;
	const wchar_t *mpszSuffix;
	int mDigits;
	int mFormat;
	int mQuality;
	char *mpPackBuffer;

	vdautoptr<IVDJPEGEncoder>		mpJPEGEncoder;
	vdautoptr<IVDImageEncoderPNG>	mpPNGEncoder;
	vdautoptr<IVDImageEncoderTIFF>	mpTIFFEncoder;
	vdfastvector<char>				mOutputBuffer;

public:
	AVIVideoImageOutputStream(const wchar_t *pszPrefix, const wchar_t *pszSuffix, int iDigits, int format, int quality);
	~AVIVideoImageOutputStream();

	void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples) {
		write(flags,pBuffer,cbBuffer,lSamples,0);
	}
	void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples, FilterModPixmapInfo* info);
	void WriteVideoImage(const VDPixmap *px);
	void partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples);
	void partialWrite(const void *pBuffer, uint32 cbBuffer) {}
	void partialWriteEnd() {}

	void *AsInterface(uint32 id) {
		if (id == IVDMediaOutputStream::kTypeID)
			return static_cast<IVDMediaOutputStream *>(this);

		if (id == IVDVideoImageOutputStream::kTypeID)
			return static_cast<IVDVideoImageOutputStream *>(this);

		return NULL;
	}
};

AVIVideoImageOutputStream::AVIVideoImageOutputStream(const wchar_t *pszPrefix, const wchar_t *pszSuffix, int iDigits, int format, int quality)
	: mpPackBuffer(NULL)
	, mpszPrefix(pszPrefix)
	, mpszSuffix(pszSuffix)
	, mDigits(iDigits)
	, mFormat(format)
	, mQuality(quality)
{
	dwFrame = 0;

	if (mFormat == AVIOutputImages::kFormatJPEG)
		mpJPEGEncoder = VDCreateJPEGEncoder();
	else if (mFormat == AVIOutputImages::kFormatPNG)
		mpPNGEncoder = VDCreateImageEncoderPNG();
	else if (mFormat == AVIOutputImages::kFormatTIFF_LZW || mFormat == AVIOutputImages::kFormatTIFF_RAW)
		mpTIFFEncoder = VDCreateImageEncoderTIFF();
}

AVIVideoImageOutputStream::~AVIVideoImageOutputStream() {
	delete[] mpPackBuffer;
}

void AVIVideoImageOutputStream::WriteVideoImage(const VDPixmap *px) {
	if (mFormat != AVIOutputImages::kFormatTIFF_LZW && mFormat != AVIOutputImages::kFormatTIFF_RAW) {
		throw MyError("The current output video format is not supported by the selected output path.");
	}

	wchar_t szFileName[MAX_PATH];
	swprintf(szFileName, MAX_PATH, L"%ls%0*d%ls", mpszPrefix, mDigits, dwFrame++, mpszSuffix);

	using namespace nsVDFile;
	VDFile mFile(szFileName, kWrite | kDenyNone | kCreateAlways | kSequential);

	if (mFormat == AVIOutputImages::kFormatTIFF_LZW || mFormat == AVIOutputImages::kFormatTIFF_RAW) {
		bool pack_lzw = mFormat == AVIOutputImages::kFormatTIFF_LZW;
		bool alpha = px->info.alpha_type!=FilterModPixmapInfo::kAlphaInvalid;

		void *p;
		uint32 len;
		mpTIFFEncoder->Encode(*px, p, len, pack_lzw, alpha);

		mFile.write(p, len);
		free(p);
	}

	mFile.close();
}

void AVIVideoImageOutputStream::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples, FilterModPixmapInfo* info) {
	wchar_t szFileName[MAX_PATH];

	const BITMAPINFOHEADER& bih = *(const BITMAPINFOHEADER *)getFormat();

	if (mFormat != AVIOutputImages::kFormatBMP && (bih.biCompression != BI_RGB ||
			(bih.biBitCount != 16 && bih.biBitCount != 24 && bih.biBitCount != 32))) {
		throw MyError("Output settings must be 16/24/32-bit RGB, uncompressed in order to save a TARGA, JPEG, or PNG sequence.");
	}

	swprintf(szFileName, MAX_PATH, L"%ls%0*d%ls", mpszPrefix, mDigits, dwFrame++, mpszSuffix);

	using namespace nsVDFile;
	VDFile mFile(szFileName, kWrite | kDenyNone | kCreateAlways | kSequential);

	if (mFormat == AVIOutputImages::kFormatJPEG) {
		mOutputBuffer.clear();

		mpJPEGEncoder->Init(mQuality, true);

		switch(bih.biBitCount) {
		case 16:
			mpJPEGEncoder->Encode(mOutputBuffer, (const char *)pBuffer + ((bih.biWidth*2+3)&~3) * (bih.biHeight-1), -((2*bih.biWidth+3) & ~3), IVDJPEGEncoder::kFormatRGB15, bih.biWidth, bih.biHeight);
			break;
		case 24:
			mpJPEGEncoder->Encode(mOutputBuffer, (const char *)pBuffer + ((bih.biWidth*3+3)&~3) * (bih.biHeight-1), -((3*bih.biWidth+3) & ~3), IVDJPEGEncoder::kFormatRGB24, bih.biWidth, bih.biHeight);
			break;
		case 32:
			mpJPEGEncoder->Encode(mOutputBuffer, (const char *)pBuffer + bih.biWidth * (bih.biHeight-1) * 4, -4*bih.biWidth, IVDJPEGEncoder::kFormatRGB32, bih.biWidth, bih.biHeight);
			break;
		default:
			VDNEVERHERE;
		}

		mFile.write(mOutputBuffer.data(), mOutputBuffer.size());
	} else if (mFormat == AVIOutputImages::kFormatPNG) {
		VDPixmapLayout pxl;

		switch(bih.biBitCount) {
		case 16:
			VDMakeBitmapCompatiblePixmapLayout(pxl, bih.biWidth, bih.biHeight, nsVDPixmap::kPixFormat_XRGB1555, 0);
			break;
		case 24:
			VDMakeBitmapCompatiblePixmapLayout(pxl, bih.biWidth, bih.biHeight, nsVDPixmap::kPixFormat_RGB888, 0);
			break;
		case 32:
			VDMakeBitmapCompatiblePixmapLayout(pxl, bih.biWidth, bih.biHeight, nsVDPixmap::kPixFormat_XRGB8888, 0);
			break;
		default:
			VDNEVERHERE;
		}

		const void *p;
		uint32 len;
		mpPNGEncoder->Encode(VDPixmapFromLayout(pxl, (void *)pBuffer), p, len, mQuality < 50);

		mFile.write(p, len);
	} else if (mFormat == AVIOutputImages::kFormatTGA || mFormat == AVIOutputImages::kFormatTGAUncompressed) {
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
		} hdr;

		hdr.IDLength	= 0;
		hdr.CoMapType	= 0;		// no color map

		if (mFormat == AVIOutputImages::kFormatTGA)
			hdr.ImgType		= 10;		// run-length, true-color image
		else
			hdr.ImgType		= 2;		// run-length, true-color image

		hdr.IndexLo		= 0;		// color map start = 0
		hdr.IndexHi		= 0;
		hdr.LengthLo	= 0;		// color map length = 0
		hdr.LengthHi	= 0;
		hdr.CoSize		= 0;
		hdr.X_OrgLo		= 0;
		hdr.X_OrgHi		= 0;
		hdr.Y_OrgLo		= 0;
		hdr.Y_OrgHi		= 0;
		hdr.WidthLo		= (unsigned char)(bih.biWidth & 0xff);
		hdr.WidthHi		= (unsigned char)(bih.biWidth >> 8);
		hdr.HeightLo	= (unsigned char)(bih.biHeight & 0xff);
		hdr.HeightHi	= (unsigned char)(bih.biHeight >> 8);
		hdr.PixelSize	= (unsigned char)(bih.biBitCount);
		hdr.AttBits		= (unsigned char)(bih.biBitCount==16 ? 1 : bih.biBitCount==32 ? 8 : 0);		// origin is bottom-left, x alpha bits

		// Do we have a pack buffer yet?

		int packrowsize = 129 * ((bih.biWidth * (bih.biBitCount>>3) + 127)>>7);

		if (!mpPackBuffer) {
			mpPackBuffer = new char[(bih.biHeight + 2) * packrowsize];
			if (!mpPackBuffer)
				throw MyMemoryError();
		}

		// Begin RLE packing.
		char *dstbase = (char *)mpPackBuffer + packrowsize;
		char *dst = dstbase;
		const int pelsize = bih.biBitCount >> 3;
		int srcpitch = (pelsize * bih.biWidth+3)&~3;

		bool wipe_alpha = true;
		if (info && info->alpha_type!=FilterModPixmapInfo::kAlphaInvalid)
			wipe_alpha = false;

		if (mFormat == AVIOutputImages::kFormatTGA) {
			const char *src = (const char *)pBuffer;
			char *dstlimit = dstbase + pelsize * bih.biWidth * bih.biHeight;
			int y;

			for(y=0; y<bih.biHeight; ++y) {
				const char *rlesrc = src;

				// copy row into scan buffer and perform necessary masking
				if (pelsize == 2) {
					if (wipe_alpha)
						CopyRowSetAlpha_X1R15(mpPackBuffer, src, bih.biWidth);
					else
						CopyRow_X1R15(mpPackBuffer, src, bih.biWidth);

					rlesrc = (const char *)mpPackBuffer;
				} else if (pelsize == 4) {
					if (wipe_alpha)
						CopyRowSetAlpha_X8R24(mpPackBuffer, src, bih.biWidth);
					else
						CopyRow_X8R24(mpPackBuffer, src, bih.biWidth);

					rlesrc = (const char *)mpPackBuffer;
				}

				// RLE pack row
				const char *rlesrcend = rlesrc + pelsize * bih.biWidth;
				const char *rlecompare = rlesrc;
				int literalbytes = pelsize;
				const char *literalstart = rlesrc;

				rlesrc += pelsize;

				do {
					while(rlesrc < rlesrcend && *rlecompare != *rlesrc) {
						++rlecompare;
						++rlesrc;
						++literalbytes;
					}

					int runbytes = 0;
					while(rlesrc < rlesrcend && *rlecompare == *rlesrc) {
						++rlecompare;
						++rlesrc;
						++runbytes;
					}

					int round;
					
					if (pelsize == 3) {
						round = 3 - literalbytes % 3;
						if (round == 3)
							round = 0;
					} else
						round = -literalbytes & (pelsize-1);

					if (runbytes < round) {
						literalbytes += runbytes;
					} else {
						literalbytes += round;
						runbytes -= round;

						int q = runbytes / pelsize;

						if (q > 2 || rlesrc >= rlesrcend) {
							int lq = literalbytes / pelsize;

							while(lq > 128) {
								*dst++ = 0x7f;
								memcpy(dst, literalstart, 128*pelsize);
								dst += 128*pelsize;
								literalstart += 128*pelsize;
								lq -= 128;
							}

							if (lq) {
								*dst++ = (char)(lq-1);
								memcpy(dst, literalstart, lq*pelsize);
								dst += lq*pelsize;
							}

							literalbytes = runbytes - q*pelsize;
							literalstart = rlesrc - literalbytes;

							while (q > 128) {
								*dst++ = (char)0xff;
								for(int i=0; i<pelsize; ++i)
									*dst++ = rlesrc[i-runbytes];
								q -= 128;
							}

							if (q) {
								*dst++ = (char)(0x7f + q);
								for(int i=0; i<pelsize; ++i)
									*dst++ = rlesrc[i-runbytes];
							}

						} else {
							literalbytes += runbytes;
						}
					}

					VDASSERT(rlesrc<rlesrcend || literalbytes <= 0);
				} while(rlesrc < rlesrcend);

				if (dst >= dstlimit) {
					hdr.ImgType = 2;
					break;
				}

				src += srcpitch;
			}
		}

		mFile.write(&hdr, 18);

		if (hdr.ImgType == 10) {		// RLE
			mFile.write(dstbase, dst-dstbase);
		} else if (pelsize == 2) {
			const char *src = (const char *)pBuffer;
			for(LONG y=0; y<bih.biHeight; ++y) {
				if (wipe_alpha)
					CopyRowSetAlpha_X1R15(mpPackBuffer, src, bih.biWidth);
				else
					CopyRow_X1R15(mpPackBuffer, src, bih.biWidth);

				mFile.write(mpPackBuffer, pelsize*bih.biWidth);

				src += srcpitch;
			}
		} else if (pelsize == 4) {
			const char *src = (const char *)pBuffer;
			for(LONG y=0; y<bih.biHeight; ++y) {
				if (wipe_alpha)
					CopyRowSetAlpha_X8R24(mpPackBuffer, src, bih.biWidth);
				else
					CopyRow_X8R24(mpPackBuffer, src, bih.biWidth);

				mFile.write(mpPackBuffer, pelsize*bih.biWidth);

				src += srcpitch;
			}
		} else {
			const char *src = (const char *)pBuffer;
			for(LONG y=0; y<bih.biHeight; ++y) {
				mFile.write(src, pelsize*bih.biWidth);

				src += srcpitch;
			}
		}

		mFile.write("\0\0\0\0\0\0\0\0TRUEVISION-XFILE.\0", 26);
	} else {
		BITMAPFILEHEADER bfh;
		bfh.bfType		= 'MB';
		bfh.bfSize		= sizeof(BITMAPFILEHEADER)+getFormatLen()+cbBuffer;
		bfh.bfReserved1	= 0;
		bfh.bfReserved2	= 0;
		bfh.bfOffBits	= sizeof(BITMAPFILEHEADER)+getFormatLen();

		mFile.write(&bfh, sizeof(BITMAPFILEHEADER));
		mFile.write(getFormat(), getFormatLen());
		mFile.write(pBuffer, cbBuffer);
	}

	mFile.close();
}

void AVIVideoImageOutputStream::partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples) {
	throw MyError("Partial writes are not supported for video streams.");
}

////////////////////////////////////

AVIOutputImages::AVIOutputImages(const wchar_t *szFilePrefix, const wchar_t *szFileSuffix, int digits, int format, int quality)
	: mPrefix(szFilePrefix)
	, mSuffix(szFileSuffix)
	, mDigits(digits)
	, mFormat(format)
	, mQuality(quality)
{
	VDASSERT(format == kFormatBMP || format == kFormatTGA || format == kFormatTGAUncompressed || format == kFormatJPEG || format == kFormatPNG || format == kFormatTIFF_LZW || format == kFormatTIFF_RAW);
}

AVIOutputImages::~AVIOutputImages() {
}

//////////////////////////////////

IVDMediaOutputStream *AVIOutputImages::createVideoStream() {
	VDASSERT(!videoOut);
	if (!(videoOut = new_nothrow AVIVideoImageOutputStream(mPrefix.c_str(), mSuffix.c_str(), mDigits, mFormat, mQuality)))
		throw MyMemoryError();
	return videoOut;
}

IVDMediaOutputStream *AVIOutputImages::createAudioStream() {
	return NULL;
}

bool AVIOutputImages::init(const wchar_t *szFile) {
	if (!videoOut)
		return false;

	return true;
}

void AVIOutputImages::finalize() {
}
