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
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "resource.h"

class VDVFMotionBlur : public VDXVideoFilter {
public:
	uint32 GetParams();
	void Start();
	void End();
	void Run();

	bool Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher);

protected:
	sint64	mPrevFrame;
	VDPixmapBuffer mAccumBuffer;
};

uint32 VDVFMotionBlur::GetParams() {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;

	if (pxlsrc.format != nsVDXPixmap::kPixFormat_XRGB8888)
		return FILTERPARAM_NOT_SUPPORTED;

	return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_ALIGN_SCANLINES;
}

void VDVFMotionBlur::Start() {
	const VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	mPrevFrame = -2;

	mAccumBuffer.init(pxldst.w, pxldst.h, nsVDPixmap::kPixFormat_XRGB8888);
}

void VDVFMotionBlur::End() {
	mAccumBuffer.clear();
}

void VDVFMotionBlur::Run() {
	const VDXFBitmap& bmdst = fa->dst;
	uint32 framesToProcess = 1;
	bool doClear = false;

	if (bmdst.mFrameCount != mPrevFrame + 1) {
		doClear = true;
		framesToProcess = fa->mSourceFrameCount;
	}

	mPrevFrame = bmdst.mFrameCount;

	const VDXPixmap pxdst = *fa->dst.mpPixmap;

	for(uint32 i = 0; i < framesToProcess; ++i) {
		const VDXPixmap& pxsrc = *fa->mpSourceFrames[(framesToProcess - 1) - i]->mpPixmap;

		uint32 *dst = (uint32 *)mAccumBuffer.data;
		const uint32 *src = (const uint32 *)pxsrc.data;
		ptrdiff_t srcpitch = pxsrc.pitch;

		if (doClear) {
			doClear = false;

			uint32 h = pxdst.h;
			do {
				memcpy(dst, src, 4*mAccumBuffer.w);

				src = (uint32 *)((char *)src + pxsrc.pitch);
				dst = (uint32 *)((char *)dst + mAccumBuffer.pitch);
			} while(--h);
		} else {
			uint32 h = pxdst.h;
			do {
				for(uint32 x = 0; x < (uint32)mAccumBuffer.w; ++x) {
					uint32 a = dst[x];
					uint32 b = src[x];

					dst[x] = (a & b) + (((a ^ b) & 0xfefefefe) >> 1);
				}

				src = (uint32 *)((char *)src + pxsrc.pitch);
				dst = (uint32 *)((char *)dst + mAccumBuffer.pitch);
			} while(--h);
		}
	}

	uint32 *dst = (uint32 *)pxdst.data;
	const uint32 *src = (const uint32 *)mAccumBuffer.data;
	ptrdiff_t srcpitch = mAccumBuffer.pitch;

	uint32 h = pxdst.h;
	do {
		memcpy(dst, src, 4*pxdst.w);

		src = (uint32 *)((char *)src + mAccumBuffer.pitch);
		dst = (uint32 *)((char *)dst + pxdst.pitch);
	} while(--h);
}

bool VDVFMotionBlur::Prefetch2(sint64 frame, IVDXVideoPrefetcher *prefetcher) {
	for(int i=0; i<8; ++i)
		prefetcher->PrefetchFrame(0, frame-i, 0);

	return true;
}

extern const VDXFilterDefinition g_VDVFMotionBlur = VDXVideoFilterDefinition<VDVFMotionBlur>(
	NULL,
	"motion blur",
	"Blurs adjacent frames together.");
