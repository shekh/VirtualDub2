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

#define READSRC(byteoff, lineoff) (*(unsigned long *)((char *)src + pitch*(lineoff) + (byteoff*4)))

#ifdef VD_CPU_X86
	extern "C" void asm_reduce2hq_run(
			void *dst,
			void *src,
			unsigned long width,
			unsigned long height,
			unsigned long srcstride,
			unsigned long dststride);
#else
	namespace {
		void Average(
			void *dst,
			const void *src,
			uint32 width,
			uint32 height,
			ptrdiff_t srcpitch,
			ptrdiff_t dstpitch
			)
		{
			do {
				uint32 *dstp = (uint32 *)dst;
				const uint32 *srcp1 = (const uint32 *)src;
				const uint32 *srcp2 = (const uint32 *)((const char *)src + srcpitch);
				const uint32 *srcp3 = (const uint32 *)((const char *)src + srcpitch * 2);

				for(uint32 x = 0; x < width; ++x) {
					const uint32 p0 = srcp1[-1];
					const uint32 p1 = srcp1[0];
					const uint32 p2 = srcp1[1];
					const uint32 p3 = srcp2[-1];
					const uint32 p4 = srcp2[0];
					const uint32 p5 = srcp2[1];
					const uint32 p6 = srcp3[-1];
					const uint32 p7 = srcp3[0];
					const uint32 p8 = srcp3[1];
					srcp1 += 2;
					srcp2 += 2;
					srcp3 += 2;

					const uint32 sum = (p4*2 + p1 + p3 + p5 + p7) * 2 + p0 + p2 + p6 + p8;
					const uint32 losum
						= ((p4 & 0xff030303)*2
							+ (p1 & 0xff070707)
							+ (p3 & 0xff070707)
							+ (p5 & 0xff070707)
							+ (p7 & 0xff070707)
							)*2
						+ (p0 & 0xff0f0f0f)
						+ (p2 & 0xff0f0f0f)
						+ (p6 & 0xff0f0f0f)
						+ (p8 & 0xff0f0f0f);

					dstp[x] = (sum - losum + ((losum + 0x080808) & 0xf0f0f0)) >> 4;
				}

				src = (const char *)src + srcpitch * 2;
				dst = (char *)dst + dstpitch;
			} while(--height);
		}
	}
#endif

static int reduce2hq_run(const VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	unsigned long w,h;
	unsigned long *src = (unsigned long *)fa->src.data, *dst = (unsigned long *)fa->dst.data;
	ptrdiff_t pitch = fa->src.pitch;
	unsigned long rb, g;

	if (!(fa->src.h & 1)) {
		src -= pitch>>2;

		if (!(fa->src.w & 1)) {
			rb	=   ((READSRC( 0,1)&0xff00ff) + (READSRC( 0,2)&0xff00ff)
					+(READSRC( 1,1)&0xff00ff) + (READSRC( 1,2)&0xff00ff));
			g	=   ((READSRC( 0,1)&0x00ff00) + (READSRC( 0,2)&0x00ff00)
					+(READSRC( 1,1)&0x00ff00) + (READSRC( 1,2)&0x00ff00));
			*dst ++ = ((rb/9) & 0x00ff0000) | ((rb & 0x0000ffff)/9) | ((g/9) & 0x00ff00);
			++src;
		}
		++src;

		w = (fa->src.w-1)/2;
		do {
			rb	=   ((READSRC(-1,1)&0xff00ff) + (READSRC(-1,2)&0xff00ff)
					+(READSRC( 0,1)&0xff00ff) + (READSRC( 0,2)&0xff00ff)
					+(READSRC( 1,1)&0xff00ff) + (READSRC( 1,2)&0xff00ff));
			g	=   ((READSRC(-1,1)&0x00ff00) + (READSRC(-1,2)&0x00ff00)
					+(READSRC( 0,1)&0x00ff00) + (READSRC( 0,2)&0x00ff00)
					+(READSRC( 1,1)&0x00ff00) + (READSRC( 1,2)&0x00ff00));
			*dst ++ = ((rb/9) & 0x00ff0000) | ((rb & 0x0000ffff)/9) | ((g/9) & 0x00ff00);
			src += 2;
		} while(--w);

		src += (fa->src.modulo + fa->src.pitch)>>2;
		dst += fa->dst.modulo>>2;
	}

#ifdef VD_CPU_X86
	asm_reduce2hq_run(
		fa->src.w&1 ? dst : dst+1,
		fa->src.w&1 ? src+1 : src+2,
		(fa->src.w-1)/2,
		(fa->src.h-1)/2,
		fa->src.pitch,
		fa->dst.pitch);
#else
	Average(
		fa->src.w&1 ? dst : dst+1,
		fa->src.w&1 ? src+1 : src+2,
		(fa->src.w-1)/2,
		(fa->src.h-1)/2,
		fa->src.pitch,
		fa->dst.pitch);
#endif

	if (!(fa->src.w & 1)) {
		h = (fa->src.h-1)/2;
		do {
			rb	=   (((READSRC( 0,0)&0xff00ff) + (READSRC( 0,1)&0xff00ff) + (READSRC( 0,2)&0xff00ff))*2
					+(READSRC( 1,0)&0xff00ff) + (READSRC( 1,1)&0xff00ff) + (READSRC( 1,2)&0xff00ff));
			g	=   (((READSRC( 0,0)&0x00ff00) + (READSRC( 0,1)&0x00ff00) + (READSRC( 0,2)&0x00ff00))*2
					+(READSRC( 1,0)&0x00ff00) + (READSRC( 1,1)&0x00ff00) + (READSRC( 1,2)&0x00ff00));
			*dst = ((rb/9) & 0x00ff0000) | ((rb & 0x0000ffff)/9) | ((g/9) & 0x00ff00);

			src += fa->src.pitch>>1;
			dst += fa->dst.pitch>>2;
		} while(--h);
	}

	return 0;
}

static long reduce2hq_param(VDXFilterActivation *fa, const VDXFilterFunctions *ff) {
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;
	VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	pxldst.w /= 2;
	pxldst.h /= 2;

	return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SUPPORTS_ALTFORMATS;
}

extern const VDXFilterDefinition g_VDVFReduce2HQ={
	0,0,NULL,
	"2:1 reduction (high quality)",
	"Reduces the size of each frame by 2:1 in both directions. A 3x3 overlapping matrix is used.\n\n[Assembly optimized] [MMX optimized]",
	NULL,NULL,
	0,
	NULL,NULL,
	reduce2hq_run,
	reduce2hq_param,
};