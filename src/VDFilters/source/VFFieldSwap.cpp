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

#include <vd2/system/memory.h>
#include <vd2/VDXFrame/VideoFilter.h>

class VDVideoFilterFieldSwap : public VDXVideoFilter {
public:
	uint32 GetParams();
	void Run();

protected:
	static void SwapPlaneScanlines(void *p, ptrdiff_t pitch, uint32 vecw, uint32 h);
};

uint32 VDVideoFilterFieldSwap::GetParams() {
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;

	pxldst = pxlsrc;

	fa->dst.offset = fa->src.offset;

	switch(pxldst.format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
		case nsVDXPixmap::kPixFormat_Y8:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_FR:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_709:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_709_FR:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_FR:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_709:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_709_FR:
		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_YUV411_Planar:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709_FR:
		case nsVDXPixmap::kPixFormat_YUV410_Planar:
		case nsVDXPixmap::kPixFormat_YUV410_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV410_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV410_Planar_709_FR:
			return FILTERPARAM_ALIGN_SCANLINES | FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
	}
}

void VDVideoFilterFieldSwap::Run() {
	const VDXPixmap& pxdst = *fa->dst.mpPixmap;

	switch(pxdst.format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
			SwapPlaneScanlines(pxdst.data, pxdst.pitch, (pxdst.w + 3) >> 2, pxdst.h);
			break;

		case nsVDXPixmap::kPixFormat_Y8:
			SwapPlaneScanlines(pxdst.data, pxdst.pitch, (pxdst.w + 15) >> 4, pxdst.h);
			break;

		case nsVDXPixmap::kPixFormat_YUV422_UYVY:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_FR:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_709:
		case nsVDXPixmap::kPixFormat_YUV422_UYVY_709_FR:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_FR:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_709:
		case nsVDXPixmap::kPixFormat_YUV422_YUYV_709_FR:
			SwapPlaneScanlines(pxdst.data, pxdst.pitch, (pxdst.w + 7) >> 3, pxdst.h);
			break;

		case nsVDXPixmap::kPixFormat_YUV444_Planar:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV444_Planar_709_FR:
			SwapPlaneScanlines(pxdst.data, pxdst.pitch, (pxdst.w + 15) >> 4, pxdst.h);
			SwapPlaneScanlines(pxdst.data2, pxdst.pitch2, (pxdst.w + 15) >> 4, pxdst.h);
			SwapPlaneScanlines(pxdst.data3, pxdst.pitch3, (pxdst.w + 15) >> 4, pxdst.h);
			break;

		case nsVDXPixmap::kPixFormat_YUV422_Planar:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV422_Planar_709_FR:
			SwapPlaneScanlines(pxdst.data, pxdst.pitch, (pxdst.w + 15) >> 4, pxdst.h);
			SwapPlaneScanlines(pxdst.data2, pxdst.pitch2, (pxdst.w + 31) >> 5, pxdst.h >> 1);
			SwapPlaneScanlines(pxdst.data3, pxdst.pitch3, (pxdst.w + 31) >> 5, pxdst.h >> 1);
			break;

		case nsVDXPixmap::kPixFormat_YUV411_Planar:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV411_Planar_709_FR:
			SwapPlaneScanlines(pxdst.data, pxdst.pitch, (pxdst.w + 15) >> 4, pxdst.h);
			SwapPlaneScanlines(pxdst.data2, pxdst.pitch2, (pxdst.w + 63) >> 6, pxdst.h);
			SwapPlaneScanlines(pxdst.data3, pxdst.pitch3, (pxdst.w + 63) >> 6, pxdst.h);
			break;

		case nsVDXPixmap::kPixFormat_YUV410_Planar:
		case nsVDXPixmap::kPixFormat_YUV410_Planar_FR:
		case nsVDXPixmap::kPixFormat_YUV410_Planar_709:
		case nsVDXPixmap::kPixFormat_YUV410_Planar_709_FR:
			SwapPlaneScanlines(pxdst.data, pxdst.pitch, (pxdst.w + 15) >> 4, pxdst.h);
			SwapPlaneScanlines(pxdst.data2, pxdst.pitch2, (pxdst.w + 63) >> 6, pxdst.h >> 2);
			SwapPlaneScanlines(pxdst.data3, pxdst.pitch3, (pxdst.w + 63) >> 6, pxdst.h >> 2);
			break;
	}
}

void VDVideoFilterFieldSwap::SwapPlaneScanlines(void *p, ptrdiff_t pitch, uint32 vecw, uint32 h) {
	char *dst1 = (char *)p;
	char *dst2 = dst1 + pitch;
	ptrdiff_t step = pitch + pitch;
	size_t rowbytes = vecw << 4;
	uint32 rowpairs = h >> 1;

	for(uint32 y=0; y<rowpairs; ++y) {
		VDSwapMemory(dst1, dst2, rowbytes);
		dst1 += step;
		dst2 += step;
	}
}

extern const VDXFilterDefinition g_VDVFFieldSwap = VDXVideoFilterDefinition<VDVideoFilterFieldSwap>(
	NULL,
	"field swap",
	"Swaps interlaced fields in the image.");

#ifdef _MSC_VER
	#pragma warning(disable: 4505)	// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{48,{flat}}' }'' : unreferenced local function has been removed
#endif