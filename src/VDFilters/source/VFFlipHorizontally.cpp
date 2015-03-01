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
};

uint32 VDVFFlipHorizontally::GetParams() {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_SWAP_BUFFERS;
}

void VDVFFlipHorizontally::Run() {
	uint32 *src = fa->src.data, *srct;
	uint32 *dst = fa->dst.data-1;
	unsigned long h, w;

	h = fa->dst.h;
	do {
		srct = src;
		w = fa->dst.w;
		do {
			dst[w] = *srct++;
		} while(--w);
		src = (uint32 *)((char *)src + fa->src.pitch);
		dst = (uint32 *)((char *)dst + fa->dst.pitch);
	} while(--h);
}

extern const VDXFilterDefinition g_VDVFFlipHorizontally = VDXVideoFilterDefinition<VDVFFlipHorizontally>(
	NULL,
	"flip horizontally",
	"Horizontally flips an image.");
