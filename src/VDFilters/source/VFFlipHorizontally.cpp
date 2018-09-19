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
#include <vd2/VDXFrame/VideoFilter.h>

class VDVFFlipHorizontally : public VDXVideoFilter {
public:
	uint32 GetParams();
	void Run();
	void Run32(void* d, void* s, int dpitch, int spitch, int w, int h);
	void Run64(void* d, void* s, int dpitch, int spitch, int w, int h);
	void Run8(void* d, void* s, int dpitch, int spitch, int w, int h);
	void Run16(void* d, void* s, int dpitch, int spitch, int w, int h);
};

uint32 VDVFFlipHorizontally::GetParams() {
	using namespace vd2;
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;

	switch(pxlsrc.format) {
		case kPixFormat_XRGB8888:
		case kPixFormat_XRGB64:
		case kPixFormat_RGB_Planar:
		case kPixFormat_RGBA_Planar:
		case kPixFormat_RGB_Planar16:
		case kPixFormat_RGBA_Planar16:
		case kPixFormat_RGB_Planar32F:
		case kPixFormat_RGBA_Planar32F:
		case kPixFormat_Y8:
		case kPixFormat_Y8_FR:
		case kPixFormat_Y16:
		case kPixFormat_YUV444_Planar:
		case kPixFormat_YUV422_Planar:
		case kPixFormat_YUV420_Planar:
		case kPixFormat_YUV444_Alpha_Planar:
		case kPixFormat_YUV422_Alpha_Planar:
		case kPixFormat_YUV420_Alpha_Planar:
			break;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
	}

	return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SWAP_BUFFERS;
}

void VDVFFlipHorizontally::Run() {
	using namespace vd2;
	const VDXPixmapAlpha& s = (const VDXPixmapAlpha&)*fa->src.mpPixmap;
	const VDXPixmapAlpha& d = (const VDXPixmapAlpha&)*fa->dst.mpPixmap;

	int format = ExtractBaseFormat(d.format);
	int w2 = ExtractWidth2(d.format, s.w);
	int h2 = ExtractHeight2(d.format, s.h);

	switch (format) {
	case kPixFormat_RGBA_Planar:
	case kPixFormat_YUV444_Alpha_Planar:
	case kPixFormat_YUV422_Alpha_Planar:
	case kPixFormat_YUV420_Alpha_Planar:
		Run8(d.data4, s.data4, d.pitch4, s.pitch4, s.w, s.h);
		break;
	case kPixFormat_RGBA_Planar16:
	case kPixFormat_YUV444_Alpha_Planar16:
	case kPixFormat_YUV422_Alpha_Planar16:
	case kPixFormat_YUV420_Alpha_Planar16:
		Run16(d.data4, s.data4, d.pitch4, s.pitch4, s.w, s.h);
		break;
	case kPixFormat_RGBA_Planar32F:
		Run32(d.data4, s.data4, d.pitch4, s.pitch4, s.w, s.h);
		break;
	}

	switch (format) {
	case kPixFormat_XRGB8888:
		Run32(d.data, s.data, d.pitch, s.pitch, s.w, s.h);
		break;
	case kPixFormat_XRGB64:
		Run64(d.data, s.data, d.pitch, s.pitch, s.w, s.h);
		break;
	case kPixFormat_Y8:
	case kPixFormat_Y8_FR:
		Run8(d.data, s.data, d.pitch, s.pitch, s.w, s.h);
		break;
	case kPixFormat_Y16:
		Run16(d.data, s.data, d.pitch, s.pitch, s.w, s.h);
		break;
	case kPixFormat_RGB_Planar:
	case kPixFormat_RGBA_Planar:
		Run8(d.data, s.data, d.pitch, s.pitch, s.w, s.h);
		Run8(d.data2, s.data2, d.pitch2, s.pitch2, s.w, s.h);
		Run8(d.data3, s.data3, d.pitch3, s.pitch3, s.w, s.h);
		break;
	case kPixFormat_RGB_Planar16:
	case kPixFormat_RGBA_Planar16:
		Run16(d.data, s.data, d.pitch, s.pitch, s.w, s.h);
		Run16(d.data2, s.data2, d.pitch2, s.pitch2, s.w, s.h);
		Run16(d.data3, s.data3, d.pitch3, s.pitch3, s.w, s.h);
		break;
	case kPixFormat_RGB_Planar32F:
	case kPixFormat_RGBA_Planar32F:
		Run32(d.data, s.data, d.pitch, s.pitch, s.w, s.h);
		Run32(d.data2, s.data2, d.pitch2, s.pitch2, s.w, s.h);
		Run32(d.data3, s.data3, d.pitch3, s.pitch3, s.w, s.h);
		break;
	case kPixFormat_YUV444_Planar:
	case kPixFormat_YUV422_Planar:
	case kPixFormat_YUV420_Planar:
	case kPixFormat_YUV444_Alpha_Planar:
	case kPixFormat_YUV422_Alpha_Planar:
	case kPixFormat_YUV420_Alpha_Planar:
		Run8(d.data, s.data, d.pitch, s.pitch, s.w, s.h);
		Run8(d.data2, s.data2, d.pitch2, s.pitch2, w2, h2);
		Run8(d.data3, s.data3, d.pitch3, s.pitch3, w2, h2);
		break;
	case kPixFormat_YUV444_Planar16:
	case kPixFormat_YUV422_Planar16:
	case kPixFormat_YUV420_Planar16:
	case kPixFormat_YUV444_Alpha_Planar16:
	case kPixFormat_YUV422_Alpha_Planar16:
	case kPixFormat_YUV420_Alpha_Planar16:
		Run16(d.data, s.data, d.pitch, s.pitch, s.w, s.h);
		Run16(d.data2, s.data2, d.pitch2, s.pitch2, w2, h2);
		Run16(d.data3, s.data3, d.pitch3, s.pitch3, w2, h2);
		break;
	}
}

void VDVFFlipHorizontally::Run32(void* d, void* s, int dpitch, int spitch, int w, int h) {
	{for(int y=0; y<h; y++){
		uint32 *src = (uint32*)(size_t(s) + spitch*y);
		uint32 *dst = (uint32*)(size_t(d) + dpitch*y + w*4);

		{for(int x=0; x<w; x++){
			dst--;
			*dst = *src;
			src++;
		}}
	}}
}

void VDVFFlipHorizontally::Run64(void* d, void* s, int dpitch, int spitch, int w, int h) {
	{for(int y=0; y<h; y++){
		uint64 *src = (uint64*)(size_t(s) + spitch*y);
		uint64 *dst = (uint64*)(size_t(d) + dpitch*y + w*8);

		{for(int x=0; x<w; x++){
			dst--;
			*dst = *src;
			src++;
		}}
	}}
}

void VDVFFlipHorizontally::Run8(void* d, void* s, int dpitch, int spitch, int w, int h) {
	{for(int y=0; y<h; y++){
		uint8 *src = (uint8*)(size_t(s) + spitch*y);
		uint8 *dst = (uint8*)(size_t(d) + dpitch*y + w);

		{for(int x=0; x<w; x++){
			dst--;
			*dst = *src;
			src++;
		}}
	}}
}

void VDVFFlipHorizontally::Run16(void* d, void* s, int dpitch, int spitch, int w, int h) {
	{for(int y=0; y<h; y++){
		uint16 *src = (uint16*)(size_t(s) + spitch*y);
		uint16 *dst = (uint16*)(size_t(d) + dpitch*y + w*2);

		{for(int x=0; x<w; x++){
			dst--;
			*dst = *src;
			src++;
		}}
	}}
}

extern const VDXFilterDefinition g_VDVFFlipHorizontally = VDXVideoFilterDefinition<VDVFFlipHorizontally>(
	NULL,
	"flip horizontally",
	"Horizontally flips an image.");
