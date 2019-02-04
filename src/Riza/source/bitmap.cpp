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

#include <vd2/Riza/bitmap.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <../Kasumi/h/uberblit_rgb64.h>

void fourcc_codec_info(uint32 a, bool& useAlpha) {
	useAlpha = false;

	// utvideo alpha sources
	if (a==VDMAKEFOURCC('U', 'L', 'R', 'A')) useAlpha = true;
	if (a==VDMAKEFOURCC('U', 'Q', 'R', 'A')) useAlpha = true;

	// magicyuv alpha sources
	if (a==VDMAKEFOURCC('M', '8', 'R', 'A')) useAlpha = true;
	if (a==VDMAKEFOURCC('M', '8', 'Y', 'A')) useAlpha = true;
	if (a==VDMAKEFOURCC('M', '0', 'R', 'A')) useAlpha = true;
	if (a==VDMAKEFOURCC('M', '2', 'R', 'A')) useAlpha = true;
	if (a==VDMAKEFOURCC('M', '4', 'R', 'A')) useAlpha = true;
}

void fourcc_codec_input(uint32 a, int& format, int& variant) {
	using namespace nsVDPixmap;
	format = 0;
	variant = 0;
	if (a==VDMAKEFOURCC('M', '0', 'R', 'G')) { format = kPixFormat_RGB_Planar16; variant = kBitmapVariant_G3_0_10; }
	if (a==VDMAKEFOURCC('M', '2', 'R', 'G')) { format = kPixFormat_RGB_Planar16; variant = kBitmapVariant_G3_0_12; }
	if (a==VDMAKEFOURCC('M', '4', 'R', 'G')) { format = kPixFormat_RGB_Planar16; variant = kBitmapVariant_G3_0_14; }
	if (a==VDMAKEFOURCC('M', '0', 'R', 'A')) { format = kPixFormat_RGBA_Planar16; variant = kBitmapVariant_G4_0_10; }
	if (a==VDMAKEFOURCC('M', '2', 'R', 'A')) { format = kPixFormat_RGBA_Planar16; variant = kBitmapVariant_G4_0_12; }
	if (a==VDMAKEFOURCC('M', '4', 'R', 'A')) { format = kPixFormat_RGBA_Planar16; variant = kBitmapVariant_G4_0_14; }
}

void fourcc_codec_output(uint32 a, int& format, int& variant) {
	format = 0;
	variant = 0;
	fourcc_codec_input(a,format,variant);
}

uint32 fourcc_toupper(uint32 a) {
	char* s = (char*)&a;
	for(int i=0; i<4; i++){
		if(s[i]>='a' && s[i]<='z')
			s[i] = s[i]-'a'+'A';
	}
	return a;
}

VDString print_fourcc(uint32 a) {
	uint8 fcc[4];
	*(uint32 *)fcc = a;
	VDString buf;
	for(int i=0; i<4; ++i) {
		uint8 c = fcc[i];
		if (isalpha(c) || isdigit(c))
			buf += c;
		else
			buf.append_sprintf("[%d]", c);
	}
	return buf;
}

VDStringW printW_fourcc(uint32 a) {
	uint8 fcc[4];
	*(uint32 *)fcc = a;
	VDStringW buf;
	for(int i=0; i<4; ++i) {
		uint8 c = fcc[i];
		if (isalpha(c) || isdigit(c))
			buf += c;
		else
			buf.append_sprintf(L"[%d]", c);
	}
	return buf;
}

int VDBitmapFormatToPixmapFormat(const VDAVIBitmapInfoHeader& hdr) {
	int variant;

	return VDBitmapFormatToPixmapFormat(hdr, variant);
}

// see libavcodec\raw.c for some defined FOURCC's
// see libavformat\riff.c, libavformat\isom.c for some more

int VDBitmapFormatToPixmapFormat(const VDAVIBitmapInfoHeader& hdr, int& variant) {
	using namespace nsVDPixmap;

	variant = 1;

	switch(hdr.biCompression) {
	case VDAVIBitmapInfoHeader::kCompressionRGB:
		if (hdr.biPlanes == 1) {
			if (hdr.biBitCount == 1)
				return kPixFormat_Pal1;
			else if (hdr.biBitCount == 2)
				return kPixFormat_Pal2;
			else if (hdr.biBitCount == 4)
				return kPixFormat_Pal4;
			else if (hdr.biBitCount == 8) {
				if (hdr.biClrUsed != 256)
					return kPixFormat_Pal8;

				const VDAVIRGBQuad *palette = (const VDAVIRGBQuad *)((const char *)&hdr + hdr.biSize);

				for(int i=0; i<256; ++i) {
					if (palette->rgbRed != i || palette->rgbGreen != i || palette->rgbBlue != i || palette->rgbReserved)
						return kPixFormat_Pal8;

					++palette;
				}

				return kPixFormat_Y8_FR;
			} else if (hdr.biBitCount == 16)
				return kPixFormat_XRGB1555;
			else if (hdr.biBitCount == 24)
				return kPixFormat_RGB888;
			else if (hdr.biBitCount == 32)
				return kPixFormat_XRGB8888;
		}
		break;
	case VDAVIBitmapInfoHeader::kCompressionBitfields:
		{
			const uint32 *bitfields = (const uint32 *)(&hdr + 1);
			const int bits = hdr.biBitCount;
			const uint32 r = bitfields[0];
			const uint32 g = bitfields[1];
			const uint32 b = bitfields[2];

			if (bits == 16 && r == 0x7c00 && g == 0x03e0 && b == 0x001f)
				return kPixFormat_XRGB1555;
			else if (bits == 16 && r == 0xf800 && g == 0x07e0 && b == 0x001f)
				return kPixFormat_RGB565;
			else if (bits == 32 && r == 0xff0000 && g == 0x00ff00 && b == 0x0000ff)
				return kPixFormat_XRGB8888;
		}
		break;

	case VDMAKEFOURCC('U', 'Y', 'V', 'Y'):
		return kPixFormat_YUV422_UYVY;

	case VDMAKEFOURCC('Y', 'U', 'Y', 'V'):
	case VDMAKEFOURCC('Y', 'U', 'Y', '2'):
		return kPixFormat_YUV422_YUYV;

	case VDMAKEFOURCC('Y', 'V', '2', '4'):	// Avisynth format
		return kPixFormat_YUV444_Planar;

	case VDMAKEFOURCC('Y', 'V', '1', '6'):
		return kPixFormat_YUV422_Planar;

	case VDMAKEFOURCC('Y', 'V', '1', '2'):
		return kPixFormat_YUV420_Planar;

	case VDMAKEFOURCC('I', '4', '2', '0'):
		variant = kBitmapVariant_I420;
		return kPixFormat_YUV420_Planar;

	case VDMAKEFOURCC('I', 'Y', 'U', 'V'):
		variant = kBitmapVariant_IYUV;
		return kPixFormat_YUV420_Planar;

	case VDMAKEFOURCC('Y', 'V', 'U', '9'):
		return kPixFormat_YUV410_Planar;

	case VDMAKEFOURCC('Y', '8', ' ', ' '):
	case VDMAKEFOURCC('Y', '8', '0', '0'):
		return kPixFormat_Y8;

	case VDMAKEFOURCC('Y', '1', 0, 16):
		return kPixFormat_Y16;

	// 420_Planar16

	case VDMAKEFOURCC('Y', '3', 11, 16):
		return kPixFormat_YUV420_Planar16;

	case VDMAKEFOURCC('Y', '3', 11, 10):
		variant = kBitmapVariant_Y3_11_10;
		return kPixFormat_YUV420_Planar16;

	// P016,P010

	case VDMAKEFOURCC('P', '0', '1', '6'):
		return kPixFormat_YUV420_P016;

	case VDMAKEFOURCC('P', '0', '1', '0'):
		return kPixFormat_YUV420_P010;

	// v210

	case VDMAKEFOURCC('v', '2', '1', '0'):
		return kPixFormat_YUV422_V210;

	// 422_Planar16

	case VDMAKEFOURCC('Y', '3', 10, 16):
		return kPixFormat_YUV422_Planar16;

	case VDMAKEFOURCC('Y', '3', 10, 10):
		variant = kBitmapVariant_Y3_10_10;
		return kPixFormat_YUV422_Planar16;

	// P216,P210

	case VDMAKEFOURCC('P', '2', '1', '6'):
		return kPixFormat_YUV422_P216;

	case VDMAKEFOURCC('P', '2', '1', '0'):
		return kPixFormat_YUV422_P210;

	// 444_Planar16

	case VDMAKEFOURCC('Y', '3', 0, 16):
		return kPixFormat_YUV444_Planar16;

	case VDMAKEFOURCC('Y', '3', 0, 10):
		variant = kBitmapVariant_Y3_0_10;
		return kPixFormat_YUV444_Planar16;

	// v410

	case VDMAKEFOURCC('v', '4', '1', '0'):
		return kPixFormat_YUV444_V410;

	// Y410

	case VDMAKEFOURCC('Y', '4', '1', '0'):
		return kPixFormat_YUV444_Y410;

	// r210

	case VDMAKEFOURCC('r', '2', '1', '0'):
		return kPixFormat_R210;

	// R10k

	case VDMAKEFOURCC('R', '1', '0', 'k'):
		return kPixFormat_R10K;

	// ----

	case VDMAKEFOURCC('v', '3', '0', '8'):
		return kPixFormat_YUV444_V308;

	case VDMAKEFOURCC('Y', '4', '1', '6'):
		return kPixFormat_YUVA444_Y416;

	case VDMAKEFOURCC('H', 'D', 'Y', 'C'):
		return kPixFormat_YUV422_UYVY_709;

	case VDMAKEFOURCC('N', 'V', '1', '2'):
		return kPixFormat_YUV420_NV12;

	case VDMAKEFOURCC('b', '6', '4', 'a'):
		return kPixFormat_B64A;

	case VDMAKEFOURCC('B', 'R', 'A', 64):
		return kPixFormat_XRGB64;

	// ----

	case VDMAKEFOURCC('G', '3', 0, 8):
		return kPixFormat_RGB_Planar;

	case VDMAKEFOURCC('G', '4', 0, 8):
		return kPixFormat_RGBA_Planar;

	case VDMAKEFOURCC('G', '3', 0, 16):
		return kPixFormat_RGB_Planar16;

	case VDMAKEFOURCC('G', '3', 0, 14):
		variant = kBitmapVariant_G3_0_14;
		return kPixFormat_RGB_Planar16;

	case VDMAKEFOURCC('G', '3', 0, 12):
		variant = kBitmapVariant_G3_0_12;
		return kPixFormat_RGB_Planar16;

	case VDMAKEFOURCC('G', '3', 0, 10):
		variant = kBitmapVariant_G3_0_10;
		return kPixFormat_RGB_Planar16;

	case VDMAKEFOURCC('G', '4', 0, 16):
		return kPixFormat_RGBA_Planar16;

	case VDMAKEFOURCC('G', '4', 0, 14):
		variant = kBitmapVariant_G4_0_14;
		return kPixFormat_RGBA_Planar16;

	case VDMAKEFOURCC('G', '4', 0, 12):
		variant = kBitmapVariant_G4_0_12;
		return kPixFormat_RGBA_Planar16;

	case VDMAKEFOURCC('G', '4', 0, 10):
		variant = kBitmapVariant_G4_0_10;
		return kPixFormat_RGBA_Planar16;
	}

	return 0;
}

int VDGetPixmapToBitmapVariants(int format) {
	if (format == nsVDPixmap::kPixFormat_YUV420_Planar)
		return 3;

	if (format == nsVDPixmap::kPixFormat_Y8)
		return 2;

	if (format == nsVDPixmap::kPixFormat_Y8_FR)
		return 2;

	if (format == nsVDPixmap::kPixFormat_YUV420_Planar16)
		return 2;

	if (format == nsVDPixmap::kPixFormat_YUV422_Planar16)
		return 2;

	if (format == nsVDPixmap::kPixFormat_YUV444_Planar16)
		return 2;

	if (format == nsVDPixmap::kPixFormat_RGB_Planar16)
		return 4;

	if (format == nsVDPixmap::kPixFormat_RGBA_Planar16)
		return 4;

	return 1;
}

bool VDMakeBitmapFormatFromPixmapFormat(vdstructex<VDAVIBitmapInfoHeader>& dst, const vdstructex<VDAVIBitmapInfoHeader>& src, int format, int variant) {
	return VDMakeBitmapFormatFromPixmapFormat(dst, src, format, variant, src->biWidth, src->biHeight);
}

bool VDMakeBitmapFormatFromPixmapFormat(vdstructex<VDAVIBitmapInfoHeader>& dst, const vdstructex<VDAVIBitmapInfoHeader>& src, int format, int variant, uint32 w, uint32 h) {
	if (format == nsVDPixmap::kPixFormat_Pal8) {
		if (src->biBitCount > 8)
			return false;

		uint32 clrUsed = src->biClrUsed;
		uint32 clrImportant = src->biClrImportant;

		if (clrUsed == 0) {
			if (src->biCompression != VDAVIBitmapInfoHeader::kCompressionRGB && src->biCompression != VDAVIBitmapInfoHeader::kCompressionRLE4 && src->biCompression != VDAVIBitmapInfoHeader::kCompressionRLE8)
				return false;

			clrUsed = 1 << src->biBitCount;
		}
		if (clrUsed >= 256)
			clrUsed = 0;				// means 'max for type'
		if (clrImportant >= clrUsed)
			clrImportant = 0;			// means 'all required'

		uint32 clrEntries = clrUsed;
		if (!clrEntries)
			clrEntries = 256;

		dst.resize(sizeof(VDAVIBitmapInfoHeader) + sizeof(VDAVIRGBQuad) * clrEntries);
		dst->biSize				= sizeof(VDAVIBitmapInfoHeader);
		dst->biWidth			= w;
		dst->biHeight			= h;
		dst->biPlanes			= 1;
		dst->biBitCount			= 8;
		dst->biCompression		= VDAVIBitmapInfoHeader::kCompressionRGB;
		dst->biSizeImage		= ((w+3)&~3)*h;
		dst->biXPelsPerMeter	= src->biXPelsPerMeter;
		dst->biYPelsPerMeter	= src->biYPelsPerMeter;
		dst->biClrUsed			= src->biClrUsed;
		dst->biClrImportant		= src->biClrImportant;

		uint32 clrTableSize = sizeof(VDAVIRGBQuad)*clrEntries;
		memcpy((char *)dst.data() + sizeof(VDAVIBitmapInfoHeader), (const char *)src.data() + src->biSize, clrTableSize);

		return true;
	}

	if (!VDMakeBitmapFormatFromPixmapFormat(dst, format, variant, w, h))
		return false;

	dst->biXPelsPerMeter = src->biXPelsPerMeter;
	dst->biYPelsPerMeter = src->biYPelsPerMeter;

	return true;
}

bool VDMakeBitmapFormatFromPixmapFormat(vdstructex<VDAVIBitmapInfoHeader>& dst, int format, int variant, uint32 w, uint32 h, bool allowNonstandardMappings) {
	using namespace nsVDPixmap;

	dst.resize(sizeof(VDAVIBitmapInfoHeader));
	dst->biSize				= sizeof(VDAVIBitmapInfoHeader);
	dst->biWidth			= w;
	dst->biHeight			= h;
	dst->biPlanes			= 1;
	dst->biXPelsPerMeter	= 0;
	dst->biYPelsPerMeter	= 0;
	dst->biClrUsed			= 0;
	dst->biClrImportant		= 0;

	if (allowNonstandardMappings) {
		switch(format) {
			case kPixFormat_YUV422_UYVY_FR:
			case kPixFormat_YUV422_UYVY_709:
			case kPixFormat_YUV422_UYVY_709_FR:
				format = kPixFormat_YUV422_UYVY;
				break;

			case kPixFormat_YUV422_YUYV_FR:
			case kPixFormat_YUV422_YUYV_709:
			case kPixFormat_YUV422_YUYV_709_FR:
				format = kPixFormat_YUV422_YUYV;
				break;

			case kPixFormat_YUV444_Planar_FR:
			case kPixFormat_YUV444_Planar_709:
			case kPixFormat_YUV444_Planar_709_FR:
				format = kPixFormat_YUV444_Planar;
				break;

			case kPixFormat_YUV422_Planar_FR:
			case kPixFormat_YUV422_Planar_709:
			case kPixFormat_YUV422_Planar_709_FR:
				format = kPixFormat_YUV422_Planar;
				break;

			case kPixFormat_YUV420_Planar_FR:
			case kPixFormat_YUV420_Planar_709:
			case kPixFormat_YUV420_Planar_709_FR:
				format = kPixFormat_YUV420_Planar;
				break;

			case kPixFormat_YUV410_Planar_FR:
			case kPixFormat_YUV410_Planar_709:
			case kPixFormat_YUV410_Planar_709_FR:
				format = kPixFormat_YUV410_Planar;
				break;

			case kPixFormat_Y8_FR:
				format = kPixFormat_Y8;
				break;
		}
	}

	switch(format) {
	case kPixFormat_XRGB1555:
		dst->biCompression	= VDAVIBitmapInfoHeader::kCompressionRGB;
		dst->biBitCount		= 16;
		dst->biSizeImage	= ((w*2+3)&~3) * h;
		break;
	case kPixFormat_RGB565:
		dst->biCompression	= VDAVIBitmapInfoHeader::kCompressionBitfields;
		dst->biBitCount		= 16;
		dst->biSizeImage	= ((w*2+3)&~3) * h;
		dst.resize(sizeof(VDAVIBitmapInfoHeader) + 3*sizeof(uint32));
		{
			uint32 *fields = (uint32 *)(dst.data() + 1);
			fields[0] = 0xf800;
			fields[1] = 0x07e0;
			fields[2] = 0x001f;
		}
		break;
	case kPixFormat_RGB888:
		dst->biCompression	= VDAVIBitmapInfoHeader::kCompressionRGB;
		dst->biBitCount		= 24;
		dst->biSizeImage	= ((w*3+3)&~3) * h;
		break;
	case kPixFormat_XRGB8888:
		dst->biCompression	= VDAVIBitmapInfoHeader::kCompressionRGB;
		dst->biBitCount		= 32;
		dst->biSizeImage	= w*4 * h;
		break;
	case kPixFormat_R210:
		dst->biCompression	= VDMAKEFOURCC('r', '2', '1', '0');
		dst->biBitCount		= 32;
		dst->biSizeImage	= w * 4 * h;
		break;
	case kPixFormat_R10K:
		dst->biCompression	= VDMAKEFOURCC('R', '1', '0', 'k');
		dst->biBitCount		= 32;
		dst->biSizeImage	= w * 4 * h;
		break;
	case kPixFormat_XRGB64:
		dst->biCompression	= VDMAKEFOURCC('B', 'R', 'A', 64);
		dst->biBitCount		= 64;
		dst->biSizeImage	= w*8 * h;
		break;
	case kPixFormat_B64A:
		dst->biCompression	= VDMAKEFOURCC('b', '6', '4', 'a');
		dst->biBitCount		= 64;
		dst->biSizeImage	= w*8 * h;
		break;
	case kPixFormat_YUV444_V308:
		dst->biCompression	= VDMAKEFOURCC('v', '3', '0', '8');
		dst->biBitCount		= 24;
		dst->biSizeImage	= w*3 * h;
		break;
	case kPixFormat_YUVA444_Y416:
		dst->biCompression	= VDMAKEFOURCC('Y', '4', '1', '6');
		dst->biBitCount		= 64;
		dst->biSizeImage	= w*8 * h;
		break;
	case kPixFormat_YUV420_Planar16:
		switch(variant) {
		case kBitmapVariant_Y3_11_10:
			dst->biCompression	= VDMAKEFOURCC('Y', '3', 11, 10);
			break;
		case kBitmapVariant_Y3_11_16:
		default:
			dst->biCompression	= VDMAKEFOURCC('Y', '3', 11, 16);
			break;
		}
		dst->biBitCount		= 24;
		dst->biSizeImage	= w*h*2 + ((w+1)>>1)*((h+1)>>1)*4;
		break;
	case kPixFormat_YUV420_P010:
		dst->biCompression	= VDMAKEFOURCC('P', '0', '1', '0');
		dst->biBitCount		= 24;
		dst->biSizeImage	= w*h*2 + ((w+1)>>1)*((h+1)>>1)*4;
		break;
	case kPixFormat_YUV420_P016:
		dst->biCompression	= VDMAKEFOURCC('P', '0', '1', '6');
		dst->biBitCount		= 24;
		dst->biSizeImage	= w*h*2 + ((w+1)>>1)*((h+1)>>1)*4;
		break;
	case kPixFormat_YUV422_Planar16:
		switch(variant) {
		case kBitmapVariant_Y3_10_10:
			dst->biCompression	= VDMAKEFOURCC('Y', '3', 10, 10);
			break;
		case kBitmapVariant_Y3_10_16:
		default:
			dst->biCompression	= VDMAKEFOURCC('Y', '3', 10, 16);
			break;
		}
		dst->biBitCount		= 32;
		dst->biSizeImage	= ((w+1)>>1) * h * 8;
		break;
	case kPixFormat_YUV422_P210:
		dst->biCompression	= VDMAKEFOURCC('P', '2', '1', '0');
		dst->biBitCount		= 32;
		dst->biSizeImage	= ((w+1)>>1) * h * 8;
		break;
	case kPixFormat_YUV422_P216:
		dst->biCompression	= VDMAKEFOURCC('P', '2', '1', '6');
		dst->biBitCount		= 32;
		dst->biSizeImage	= ((w+1)>>1) * h * 8;
		break;
	case kPixFormat_YUV422_UYVY:
		dst->biCompression	= VDMAKEFOURCC('U', 'Y', 'V', 'Y');
		dst->biBitCount		= 16;
		dst->biSizeImage	= ((w+1)&~1)*2*h;
		break;
	case kPixFormat_YUV422_YUYV:
		dst->biCompression	= VDMAKEFOURCC('Y', 'U', 'Y', '2');
		dst->biBitCount		= 16;
		dst->biSizeImage	= ((w+1)&~1)*2*h;
		break;
	case kPixFormat_YUV444_Planar:
		dst->biCompression	= VDMAKEFOURCC('Y', 'V', '2', '4');
		dst->biBitCount		= 24;
		dst->biSizeImage	= w * h * 3;
		break;
	case kPixFormat_YUV422_Planar:
		dst->biCompression	= VDMAKEFOURCC('Y', 'V', '1', '6');
		dst->biBitCount		= 16;
		dst->biSizeImage	= ((w+1)>>1) * h * 4;
		break;
	case kPixFormat_YUV420_Planar:
		switch(variant) {
		case kBitmapVariant_IYUV:
			dst->biCompression	= VDMAKEFOURCC('I', 'Y', 'U', 'V');
			break;
		case kBitmapVariant_I420:
			dst->biCompression	= VDMAKEFOURCC('I', '4', '2', '0');
			break;
		case kBitmapVariant_YV12:
		default:
			dst->biCompression	= VDMAKEFOURCC('Y', 'V', '1', '2');
			break;
		}
		dst->biBitCount		= 12;
		dst->biSizeImage	= w*h + ((w+1)>>1)*((h+1)>>1)*2;
		break;
	case kPixFormat_YUV410_Planar:
		dst->biCompression	= VDMAKEFOURCC('Y', 'V', 'U', '9');
		dst->biBitCount		= 9;
		dst->biSizeImage	= ((w+2)>>2) * ((h+2)>>2) * 18;
		break;
	case kPixFormat_Y8:
		switch(variant) {
		case kBitmapVariant_Y800:
			dst->biCompression	= VDMAKEFOURCC('Y', '8', '0', '0');
			break;
		case kBitmapVariant_Y8:
		default:
			dst->biCompression	= VDMAKEFOURCC('Y', '8', ' ', ' ');
			break;
		}
		dst->biBitCount		= 8;
		dst->biSizeImage	= ((w+3) & ~3) * h;
		break;
	case kPixFormat_YUV422_V210:
		dst->biCompression	= VDMAKEFOURCC('v', '2', '1', '0');
		dst->biBitCount		= 20;
		dst->biSizeImage	= ((w + 47) / 48) * 128 * h;
		break;
	case kPixFormat_YUV444_Planar16:
		switch(variant) {
		case kBitmapVariant_Y3_0_10:
			dst->biCompression	= VDMAKEFOURCC('Y', '3', 0, 10);
			break;
		case kBitmapVariant_Y3_0_16:
		default:
			dst->biCompression	= VDMAKEFOURCC('Y', '3', 0, 16);
			break;
		}
		dst->biBitCount		= 48;
		dst->biSizeImage	= w * h * 6;
		break;
	case kPixFormat_YUV444_V410:
		dst->biCompression	= VDMAKEFOURCC('v', '4', '1', '0');
		dst->biBitCount		= 32;
		dst->biSizeImage	= w * 4 * h;
		break;
	case kPixFormat_YUV444_Y410:
		dst->biCompression	= VDMAKEFOURCC('Y', '4', '1', '0');
		dst->biBitCount		= 32;
		dst->biSizeImage	= w * 4 * h;
		break;
	case kPixFormat_YUV422_UYVY_709:
		dst->biCompression	= VDMAKEFOURCC('H', 'D', 'Y', 'C');
		dst->biBitCount		= 16;
		dst->biSizeImage	= ((w+1)&~1) * h * 2;
		break;
	case kPixFormat_YUV420_NV12:
		dst->biCompression	= VDMAKEFOURCC('N', 'V', '1', '2');
		dst->biBitCount		= 12;
		dst->biSizeImage	= w*h + ((w+1)>>1)*((h+1)>>1)*2;
		break;
	case kPixFormat_Y8_FR:
		switch(variant) {
		case kBitmapVariant_Y8_FR:
			dst->biCompression	= VDMAKEFOURCC('Y', '8', ' ', ' ');
			dst->biBitCount		= 8;
			dst->biSizeImage	= ((w+3) & ~3) * h;
			break;
		case kBitmapVariant_Y8_Pal:
		default:
			dst->biCompression	= VDAVIBitmapInfoHeader::kCompressionRGB;
			dst->biBitCount		= 8;
			dst->biClrUsed		= 256;
			dst->biClrImportant	= 256;
			dst->biSizeImage	= ((w+3) & ~3)*h;
			dst.resize(sizeof(VDAVIBitmapInfoHeader) + 256*sizeof(VDAVIRGBQuad));

			{
				VDAVIRGBQuad *pal = (VDAVIRGBQuad *)(dst.data() + 1);

				for(int i=0; i<256; ++i) {
					pal[i].rgbRed = i;
					pal[i].rgbGreen = i;
					pal[i].rgbBlue = i;
					pal[i].rgbReserved = 0;
				}
			}
			break;
		}
		break;
	case kPixFormat_Y16:
		dst->biCompression	= VDMAKEFOURCC('Y', '1', 0, 16);
		dst->biBitCount		= 16;
		dst->biSizeImage	= w * h * 2;
		break;

	case kPixFormat_RGB_Planar:
		dst->biCompression	= VDMAKEFOURCC('G', '3', 0, 8);
		dst->biBitCount		= 24;
		dst->biSizeImage	= w * h * 3;
		break;

	case kPixFormat_RGBA_Planar:
		dst->biCompression	= VDMAKEFOURCC('G', '4', 0, 8);
		dst->biBitCount		= 32;
		dst->biSizeImage	= w * h * 4;
		break;

	case kPixFormat_RGB_Planar16:
		switch(variant) {
		case kBitmapVariant_G3_0_10:
			dst->biCompression	= VDMAKEFOURCC('G', '3', 0, 10);
			break;
		case kBitmapVariant_G3_0_12:
			dst->biCompression	= VDMAKEFOURCC('G', '3', 0, 12);
			break;
		case kBitmapVariant_G3_0_14:
			dst->biCompression	= VDMAKEFOURCC('G', '3', 0, 14);
			break;
		case kBitmapVariant_G3_0_16:
		default:
			dst->biCompression	= VDMAKEFOURCC('G', '3', 0, 16);
			break;
		}
		dst->biBitCount		= 48;
		dst->biSizeImage	= w * h * 6;
		break;

	case kPixFormat_RGBA_Planar16:
		switch(variant) {
		case kBitmapVariant_G4_0_10:
			dst->biCompression	= VDMAKEFOURCC('G', '4', 0, 10);
			break;
		case kBitmapVariant_G4_0_12:
			dst->biCompression	= VDMAKEFOURCC('G', '4', 0, 12);
			break;
		case kBitmapVariant_G4_0_14:
			dst->biCompression	= VDMAKEFOURCC('G', '4', 0, 14);
			break;
		case kBitmapVariant_G4_0_16:
		default:
			dst->biCompression	= VDMAKEFOURCC('G', '4', 0, 16);
			break;
		}
		dst->biBitCount		= 64;
		dst->biSizeImage	= w * h * 8;
		break;

	default:
		return false;
	};

	return true;
}

uint32 VDMakeBitmapCompatiblePixmapLayout(VDPixmapLayout& layout, sint32 w, sint32 h, int format, int variant, const uint32 *palette, int imageSize) {
	using namespace nsVDPixmap;
	int alignment = 4;

	if (format == kPixFormat_YUV422_V210) {
		// V210 requires 128 _byte_ (NOT bit!) alignment.
		alignment = 128;
	} else if (format == kPixFormat_R210) {
		alignment = 256;
		if (imageSize && imageSize==w*h*4) alignment = 1;
	} else if (format == kPixFormat_YUV444_V308) {
		alignment = 1;
		if (imageSize && ((w*3+3) & ~3)*h==imageSize) alignment = 4;
	} else if (format == kPixFormat_Y8) {
		if (imageSize && w*h==imageSize) alignment = 1;
	} else if (VDPixmapGetInfo(format).auxbufs > 1)
		alignment = 1;

	uint32 linspace = VDPixmapCreateLinearLayout(layout, format, w, abs(h), alignment);
	if (variant==0) variant = 1;

	switch(format) {
	case kPixFormat_Pal8:
		layout.palette = palette;
		// fall through
	case kPixFormat_XRGB1555:
	case kPixFormat_RGB888:
	case kPixFormat_RGB565:
	case kPixFormat_XRGB8888:
		// RGB can be flipped (but YUV can't)
		if (h > 0) {
			layout.data += layout.pitch * (h-1);
			layout.pitch = -layout.pitch;
		}
		break;
	case kPixFormat_Y8_FR:
		if (variant==kBitmapVariant_Y8_Pal) {
			if (h > 0) {
				layout.data += layout.pitch * (h-1);
				layout.pitch = -layout.pitch;
			}
		}
		break;
	case kPixFormat_YUV444_Planar:		// swap YV24 to match YV12
	case kPixFormat_YUV444_Planar_FR:
	case kPixFormat_YUV444_Planar_709:
	case kPixFormat_YUV444_Planar_709_FR:
	case kPixFormat_YUV422_Planar:		// swap YV16 to match YV12
	case kPixFormat_YUV422_Planar_FR:
	case kPixFormat_YUV422_Planar_709:
	case kPixFormat_YUV422_Planar_709_FR:
		std::swap(layout.data2, layout.data3);
		std::swap(layout.pitch2, layout.pitch3);
		break;
	case kPixFormat_YUV420_Planar:
	case kPixFormat_YUV420_Planar_FR:
	case kPixFormat_YUV420_Planar_709:
	case kPixFormat_YUV420_Planar_709_FR:
		if (variant==kBitmapVariant_YV12) {
			std::swap(layout.data2, layout.data3);
			std::swap(layout.pitch2, layout.pitch3);
		}
		break;
	case kPixFormat_YUV410_Planar:
	case kPixFormat_YUV410_Planar_FR:
	case kPixFormat_YUV410_Planar_709:
	case kPixFormat_YUV410_Planar_709_FR:
		std::swap(layout.data2, layout.data3);
		std::swap(layout.pitch2, layout.pitch3);
		break;
	case kPixFormat_RGB_Planar:
	case kPixFormat_RGBA_Planar:
	case kPixFormat_RGB_Planar16:
	case kPixFormat_RGBA_Planar16:
		// G3, G4: GBR
		std::swap(layout.data, layout.data2);
		std::swap(layout.pitch, layout.pitch2);
		std::swap(layout.data, layout.data3);
		std::swap(layout.pitch, layout.pitch3);
		break;
	}

	return linspace;
}

bool VDGetPixmapLayoutForBitmapFormat(const VDAVIBitmapInfoHeader& hdr, uint32 hdrsize, VDPixmapLayout& layout) {
	int variant;

	int format = VDBitmapFormatToPixmapFormat(hdr, variant);
	if (!format)
		return false;

	VDMakeBitmapCompatiblePixmapLayout(layout, hdr.biWidth, hdr.biHeight, format, variant, 0, hdr.biSizeImage);
	return true;
}

VDPixmap VDGetPixmapForBitmap(const VDAVIBitmapInfoHeader& hdr, const void *data) {
	int variant;

	int format = VDBitmapFormatToPixmapFormat(hdr, variant);
	VDPixmapLayout layout;
	VDMakeBitmapCompatiblePixmapLayout(layout, hdr.biWidth, hdr.biHeight, format, variant, 0, hdr.biSizeImage);

	return VDPixmapFromLayout(layout, (void *)data);
}

void VDSetPixmapInfoForBitmap(FilterModPixmapInfo& info, int format, int variant) {
	using namespace nsVDPixmap;

	switch (format) {
	case kPixFormat_XRGB64:
	case kPixFormat_B64A:
		info.ref_r = 0xFFFF;
		info.ref_g = 0xFFFF;
		info.ref_b = 0xFFFF;
		info.ref_a = 0xFFFF;
		break;

	case kPixFormat_RGB_Planar16:
	case kPixFormat_RGBA_Planar16:
		info.ref_r = 0xFFFF;
		info.ref_a = 0xFFFF;
		if (variant==kBitmapVariant_G4_0_14) {
			// 14 bit
			info.ref_r = 0x3FFF;
			info.ref_a = 0x3FFF;
		}
		if (variant==kBitmapVariant_G4_0_12) {
			// 12 bit
			info.ref_r = 0xFFF;
			info.ref_a = 0xFFF;
		}
		if (variant==kBitmapVariant_G4_0_10) {
			// 10 bit
			info.ref_r = 0x3FF;
			info.ref_a = 0x3FF;
		}
		break;

	case kPixFormat_YUV420_Planar16:
	case kPixFormat_YUV422_Planar16:
	case kPixFormat_YUV444_Planar16:
		if (info.colorRangeMode==vd2::kColorRangeMode_Full)
			info.ref_r = 0xFFFF;
		else
			info.ref_r = 0xFF00;

		if (variant==kBitmapVariant_Y3_10_10) {
			// ffmpeg, 10 bit
			if (info.colorRangeMode==vd2::kColorRangeMode_Full)
				info.ref_r = 0x3FF;
			else
				info.ref_r = 0x3FC;
		}
		break;

	case kPixFormat_Y16:
	case kPixFormat_YUV420_Alpha_Planar16:
	case kPixFormat_YUV422_Alpha_Planar16:
	case kPixFormat_YUV444_Alpha_Planar16:
		if (info.colorRangeMode==vd2::kColorRangeMode_Full)
			info.ref_r = 0xFFFF;
		else
			info.ref_r = 0xFF00;
		info.ref_a = 0xFFFF;
		break;

	case kPixFormat_YUV422_P210:
	case kPixFormat_YUV420_P010:
		if (info.colorRangeMode==vd2::kColorRangeMode_Full)
			info.ref_r = 0xFFC0;
		else
			info.ref_r = 0xFF00;
		break;

	case kPixFormat_YUV422_P216:
	case kPixFormat_YUV420_P016:
		if (info.colorRangeMode==vd2::kColorRangeMode_Full)
			info.ref_r = 0xFFFF;
		else
			info.ref_r = 0xFF00;
		break;

	case kPixFormat_YUVA444_Y416:
		if (info.colorRangeMode==vd2::kColorRangeMode_Full)
			info.ref_r = 0xFFFF;
		else
			info.ref_r = 0xFF00;
		info.ref_a = 0xFFFF;
		break;
	}
}

// purpose: convert YCbCr bitmask of type 2^n-1 to FF*2^(n-8)
// do nothing otherwise
void VDAdjustPixmapInfoForRange(FilterModPixmapInfo& info, int format) {
	if (VDPixmapFormatHasYUV16(format) && info.colorRangeMode!=vd2::kColorRangeMode_Full) {
		int range = info.ref_r+1;
		int lsb = (range & -range);
		if (range==lsb) {
			int x = (lsb>>1) + (lsb>>2) + (lsb>>3) + (lsb>>4);
			info.ref_r = x + (x>>4);
		}
	}
}
