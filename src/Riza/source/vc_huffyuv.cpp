//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2008 Avery Lee
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
#include <vd2/system/vdalloc.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Riza/bitmap.h>
#include <vd2/Riza/videocodec.h>
#include <vd2/Meia/decode_huffyuv.h>

class VDVideoDecompressorHuffyuv : public IVDVideoDecompressor {
public:
	VDVideoDecompressorHuffyuv(uint32 w, uint32 h, uint32 depth, const uint8 *extradata, uint32 extralen);
	~VDVideoDecompressorHuffyuv();

	bool QueryTargetFormat(int format);
	bool QueryTargetFormat(const void *format);
	bool SetTargetFormat(int format);
	bool SetTargetFormat(const void *format);
	int GetTargetFormat() { return mFormat; }
	int GetTargetFormatVariant() { return 0; }
	const uint32 *GetTargetFormatPalette() { return NULL; }
	void Start();
	void Stop();
	void DecompressFrame(void *dst, const void *src, uint32 srcSize, bool keyframe, bool preroll);
	const void *GetRawCodecHandlePtr();
	const wchar_t *GetName();

protected:
	int	mFormat;
	int	mWidth;
	int	mHeight;

	vdautoptr<IVDVideoDecoderHuffyuv> mpDecoder;
};

IVDVideoDecompressor *VDCreateVideoDecompressorHuffyuv(uint32 w, uint32 h, uint32 depth, const uint8 *extradata, uint32 extralen) {
	return new VDVideoDecompressorHuffyuv(w, h, depth, extradata, extralen);
}

VDVideoDecompressorHuffyuv::VDVideoDecompressorHuffyuv(uint32 w, uint32 h, uint32 depth, const uint8 *extradata, uint32 extralen)
	: mFormat(0)
	, mWidth(w)
	, mHeight(h)
	, mpDecoder(VDCreateVideoDecoderHuffyuv())
{
	mpDecoder->Init(w, h, depth, extradata, extralen);
}

VDVideoDecompressorHuffyuv::~VDVideoDecompressorHuffyuv() {
}

bool VDVideoDecompressorHuffyuv::QueryTargetFormat(int format) {
	return format > 0;
}

bool VDVideoDecompressorHuffyuv::QueryTargetFormat(const void *format) {
	const VDAVIBitmapInfoHeader& hdr = *(const VDAVIBitmapInfoHeader *)format;

	if (hdr.biWidth != mWidth || hdr.biHeight != mHeight)
		return false;

	int pxformat = VDBitmapFormatToPixmapFormat(hdr);

	return QueryTargetFormat(pxformat);
}

bool VDVideoDecompressorHuffyuv::SetTargetFormat(int format) {
	if (!format)
		format = nsVDPixmap::kPixFormat_YUV422_YUYV;

	if (QueryTargetFormat(format)) {
		mFormat = format;
		return true;
	}

	return false;
}

bool VDVideoDecompressorHuffyuv::SetTargetFormat(const void *format) {
	const VDAVIBitmapInfoHeader& hdr = *(const VDAVIBitmapInfoHeader *)format;

	if (hdr.biWidth != mWidth || hdr.biHeight != mHeight)
		return false;

	int pxformat = VDBitmapFormatToPixmapFormat(hdr);

	if (QueryTargetFormat(pxformat)) {
		mFormat = pxformat;
		return true;
	}

	return false;
}

void VDVideoDecompressorHuffyuv::Start() {
	if (!mFormat)
		throw MyError("Cannot find compatible target format for video decompression.");
}

void VDVideoDecompressorHuffyuv::Stop() {
}

void VDVideoDecompressorHuffyuv::DecompressFrame(void *dst, const void *src, uint32 srcSize, bool keyframe, bool preroll) {
	if (!mFormat)
		throw MyError("Cannot find compatible target format for video decompression.");

	mpDecoder->DecompressFrame(src, srcSize);

	// blit time!

	VDPixmap pxsrc(mpDecoder->GetFrameBuffer());

	VDPixmapLayout dstlayout;
	VDMakeBitmapCompatiblePixmapLayout(dstlayout, mWidth, mHeight, mFormat, 0);
	VDPixmap pxdst(VDPixmapFromLayout(dstlayout, dst));

	VDPixmapBlt(pxdst, pxsrc);
}

const void *VDVideoDecompressorHuffyuv::GetRawCodecHandlePtr() {
	return NULL;
}

const wchar_t *VDVideoDecompressorHuffyuv::GetName() {
	return L"Internal Huffyuv decoder";
}
