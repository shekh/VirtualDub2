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

#ifdef VD_CPU_X86
	extern "C" void asm_reduceby2_32(
			void *dst,
			void *src,
			unsigned long width,
			unsigned long height,
			unsigned long srcstride,
			unsigned long dststride);
#endif

static int reduce_run(const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
#ifdef VD_CPU_X86
	asm_reduceby2_32(fa->dst.data, fa->src.data, fa->dst.w, fa->dst.h, fa->src.pitch, fa->dst.pitch);
#else
	const uint32 *src = (const uint32 *)fa->src.mpPixmap->data;
	const ptrdiff_t srcpitch = fa->src.mpPixmap->pitch;
	const ptrdiff_t srcpitch2 = srcpitch * 2;
	uint32 *dst = (uint32 *)fa->dst.mpPixmap->data;
	const ptrdiff_t dstpitch = fa->dst.mpPixmap->pitch;
	const uint32 w = fa->dst.mpPixmap->w;
	const uint32 w2 = w >> 1;
	uint32 h = fa->dst.mpPixmap->h;

	do {
		const uint32 *src1 = src;
		const uint32 *src2 = (const uint32 *)((const char *)src + srcpitch);

		for(uint32 x = 0; x < w; ++x) {
			const uint32 p0 = src1[0];
			const uint32 p1 = src1[1];
			const uint32 p2 = src2[0];
			const uint32 p3 = src2[1];
			src1 += 2;
			src2 += 2;

			const uint32 lo0 = p0 & 0xff030303;
			const uint32 lo1 = p1 & 0xff030303;
			const uint32 lo2 = p2 & 0xff030303;
			const uint32 lo3 = p3 & 0xff030303;

			const uint32 losum = lo0 + lo1 + lo2 + lo3;

			const uint32 result = (p0 + p1 + p2 + p3 - losum + ((losum + 2) & 0x0c0c0c)) >> 2;
			dst[x] = result;
		}
		
		src = (const uint32 *)((const char *)src + srcpitch2);
		dst = (uint32 *)((const char *)dst + dstpitch);
	} while(--h);
#endif
	
	return 0;
}

static long reduce_param(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;
	VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	pxldst.w /= 2;
	pxldst.h /= 2;

	return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

extern const VDXFilterDefinition g_VDVFReduce2={
	0,0,NULL,
	"2:1 reduction",
	"Reduces the size of an image by 2:1 in both directions. A 2x2 non-overlapping matrix is used.\n\n[Assembly optimized] [MMX optimized]",
	NULL,NULL,
	0,
	NULL,NULL,
	reduce_run,
	reduce_param,
};