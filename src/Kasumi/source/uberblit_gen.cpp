//	VirtualDub - Video processing and capture application
//	Graphics support library
//	Copyright (C) 1998-2009 Avery Lee
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

#include <stdafx.h>
#include <windows.h>
#include <vd2/system/vdalloc.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "uberblit.h"
#include "uberblit_gen.h"
#include "uberblit_fill.h"
#include "uberblit_input.h"
#include "uberblit_resample.h"
#include "uberblit_resample_special.h"
#include "uberblit_ycbcr.h"
#include "uberblit_ycbcr_generic.h"
#include "uberblit_rgb.h"
#include "uberblit_rgb64.h"
#include "uberblit_swizzle.h"
#include "uberblit_pal.h"
#include "uberblit_16f.h"
#include "uberblit_v210.h"
#include "uberblit_interlace.h"

#ifdef VD_CPU_X86
	#include "uberblit_swizzle_x86.h"
	#include "uberblit_ycbcr_x86.h"
	#include "uberblit_rgb_x86.h"
	#include "uberblit_resample_special_x86.h"
#endif

void VDPixmapGenerate(void *dst, ptrdiff_t pitch, sint32 bpr, sint32 height, IVDPixmapGen *gen, int genIndex) {
	for(sint32 y=0; y<height; ++y) {
		memcpy(dst, gen->GetRow(y, genIndex), bpr);
		vdptrstep(dst, pitch);
	}
	VDCPUCleanupExtensions();
}

void VDPixmapGenerateFast(void *dst, ptrdiff_t pitch, sint32 height, IVDPixmapGen *gen) {
	for(sint32 y=0; y<height; ++y) {
		gen->ProcessRow(dst, y);
		vdptrstep(dst, pitch);
	}
	VDCPUCleanupExtensions();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

IVDPixmapBlitter *VDCreatePixmapUberBlitterDirectCopy(const VDPixmap& dst, const VDPixmap& src) {
	IVDPixmapBlitter* r = new VDPixmapUberBlitterDirectCopy;
	r->profiler_comment = VDString("copy ") + VDPixmapFormatPrintSpec(src.format);
	return r;
}

IVDPixmapBlitter *VDCreatePixmapUberBlitterDirectCopy(const VDPixmapLayout& dst, const VDPixmapLayout& src) {
	IVDPixmapBlitter* r = new VDPixmapUberBlitterDirectCopy;
	r->profiler_comment = VDString("copy ") + VDPixmapFormatPrintSpec(src.format);
	return r;
}

VDPixmapUberBlitterDirectCopy::VDPixmapUberBlitterDirectCopy() {
}

VDPixmapUberBlitterDirectCopy::~VDPixmapUberBlitterDirectCopy() {
}

void VDPixmapUberBlitterDirectCopy::Blit(const VDPixmap& dst, const VDPixmap& src) {
	Blit(dst, NULL, src);
	FilterModPixmapInfo& info = (const_cast<VDPixmap&>(dst)).info;
	info = src.info;
}

void VDPixmapUberBlitterDirectCopy::Blit(const VDPixmap& dst, const vdrect32 *rDst, const VDPixmap& src) {
	VDASSERT(dst.format == src.format);

	const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(dst.format);

	void *p = dst.data;
	void *p2 = dst.data2;
	void *p3 = dst.data3;
	void *p4 = dst.data4;
	int w = std::min<int>(dst.w, src.w);
	int h = std::min<int>(dst.h, src.h);

	if (formatInfo.qchunky)  {
		w = (w + formatInfo.qw - 1) / formatInfo.qw;
		h = -(-h >> formatInfo.qhbits);
	}

	int w2 = -(-dst.w >> formatInfo.auxwbits);
	int h2 = -(-dst.h >> formatInfo.auxhbits);

	if (rDst) {
		int x1 = rDst->left;
		int y1 = rDst->top;
		int x2 = rDst->right;
		int y2 = rDst->bottom;

		VDASSERT(x1 >= 0 && y1 >= 0 && x2 <= w && y2 <= h && x2 >= x1 && y2 >= y1);

		if (x2 < x1 || y2 < y1)
			return;

		p = vdptroffset(dst.data, dst.pitch * y1 + x1 * formatInfo.qsize);
		w = x2 - x1;
		h = y2 - y1;

		if (formatInfo.auxbufs >= 1) {
			VDASSERT(!((x1|x2) & ((1 << formatInfo.auxwbits) - 1)));
			VDASSERT(!((y1|y2) & ((1 << formatInfo.auxhbits) - 1)));

			int ax1 = x1 >> formatInfo.auxwbits;
			int ay1 = y1 >> formatInfo.auxhbits;
			int ax2 = x2 >> formatInfo.auxwbits;
			int ay2 = y2 >> formatInfo.auxhbits;

			p2 = vdptroffset(dst.data2, dst.pitch2 * ay1 + ax1);
			w2 = ax2 - ax1;
			h2 = ay2 - ay1;

			if (formatInfo.auxbufs >= 2)
				p3 = vdptroffset(dst.data3, dst.pitch3 * ay1 + ax1);
		}

		if (formatInfo.auxbufs >= 3) {
			p4 = vdptroffset(dst.data4, dst.pitch4 * y1 + x1*formatInfo.aux4size);
		}
	}

	uint32 bpr = formatInfo.qsize * w;

	VDMemcpyRect(p, dst.pitch, src.data, src.pitch, bpr, h);

	if (formatInfo.auxbufs >= 1) {
		VDMemcpyRect(p2, dst.pitch2, src.data2, src.pitch2, w2 * formatInfo.auxsize, h2);

		if (formatInfo.auxbufs >= 2)
			VDMemcpyRect(p3, dst.pitch3, src.data3, src.pitch3, w2 * formatInfo.auxsize, h2);
	}

	if (formatInfo.auxbufs >= 3)
		VDMemcpyRect(p4, dst.pitch4, src.data4, src.pitch4, w * formatInfo.aux4size, h);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

VDPixmapUberBlitter::VDPixmapUberBlitter() {
}

VDPixmapUberBlitter::~VDPixmapUberBlitter() {
	while(!mGenerators.empty()) {
		delete mGenerators.back();
		mGenerators.pop_back();
	}
}

void VDPixmapUberBlitter::Blit(const VDPixmap& dst, const VDPixmap& src) {
	using namespace nsVDXPixmap;
	FilterModPixmapInfo& info = (const_cast<VDPixmap&>(dst)).info;
	info.alpha_type = FilterModPixmapInfo::kAlphaInvalid;
	Blit(dst, NULL, src);
	info.copy_frame(src.info);
	if (!VDPixmapFormatHasAlpha(dst.format))
		info.alpha_type = FilterModPixmapInfo::kAlphaInvalid;
}

void VDPixmapUberBlitter::Blit(const VDPixmap& dst, const vdrect32 *rDst, const VDPixmap& src) {
	for(Sources::const_iterator it(mSources.begin()), itEnd(mSources.end()); it!=itEnd; ++it) {
		const SourceEntry& se = *it;
		const void *p;
		ptrdiff_t pitch;

		switch(se.mSrcPlane) {
			case 0:
				p = src.data;
				pitch = src.pitch;
				break;
			case 1:
				p = src.data2;
				pitch = src.pitch2;
				break;
			case 2:
				p = src.data3;
				pitch = src.pitch3;
				break;
			case 3:
				p = src.data4;
				pitch = src.pitch4;
				break;
			default:
				VDASSERT(false);
				break;
		}

		se.mpSrc->SetSource((const char *)p + pitch*se.mSrcY + se.mSrcX, pitch, src.palette);
	}

	if (mOutputs[2].mpSrc) {
		if (mbIndependentPlanes)
			Blit3Separated(dst, rDst, src.info);
		else if (mbIndependentChromaPlanes)
			Blit3Split(dst, rDst, src.info);
		else
			Blit3(dst, rDst, src.info);

		if (mOutputs[3].mpSrc)
			BlitAlpha(dst, rDst, src.info);

	} else if (mOutputs[1].mpSrc) {
		if (mbIndependentPlanes)
			Blit2Separated(dst, rDst, src.info);
		else
			Blit2(dst, rDst, src.info);
	} else
		Blit(dst, rDst, src.info);
}

void VDPixmapUberBlitter::BlitAlpha(const VDPixmap& px, const vdrect32 *rDst, const FilterModPixmapInfo& src) {
	const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(px.format);
	IVDPixmapGen *gen = mOutputs[3].mpSrc;
	int idx = mOutputs[3].mSrcIndex;

	FilterModPixmapInfo buf;
	FilterModPixmapInfo& info = (const_cast<VDPixmap&>(px)).info;
	gen->AddWindowRequest(0, 0);
	gen->TransformPixmapInfo(src,buf);
	gen->Start();
	info.ref_a = buf.ref_a;
	info.copy_alpha(buf);

	uint32 height = px.h;
	uint32 bpr = formatInfo.aux4size * px.w;
	uint8 *dst = (uint8 *)px.data4;
	ptrdiff_t pitch = px.pitch4;
	uint32 y2 = 0;
	for(uint32 y=0; y<height; ++y) {
		memcpy(dst, gen->GetRow(y, idx), bpr);
		vdptrstep(dst, pitch);
	}

	VDCPUCleanupExtensions();
}

void VDPixmapUberBlitter::Blit(const VDPixmap& dst, const vdrect32 *rDst, const FilterModPixmapInfo& src) {
	const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(dst.format);

	mOutputs[0].mpSrc->AddWindowRequest(0, 0);
	mOutputs[0].mpSrc->TransformPixmapInfo(src,(const_cast<VDPixmap&>(dst)).info);
	mOutputs[0].mpSrc->Start();

	void *p = dst.data;
	int w = dst.w;
	int h = dst.h;

	if (formatInfo.qchunky) {
		w = (w + formatInfo.qw - 1) / formatInfo.qw;
		h = -(-h >> formatInfo.qhbits);
	}

	if (rDst) {
		int x1 = rDst->left;
		int y1 = rDst->top;
		int x2 = rDst->right;
		int y2 = rDst->bottom;

		if (formatInfo.qchunky) {
			x1 = x1 / formatInfo.qw;
			y1 = y1 / formatInfo.qh;
			x2 = (x2 + formatInfo.qw - 1) / formatInfo.qw;
			y2 = (y2 + formatInfo.qh - 1) / formatInfo.qh;
		}

		VDASSERT(x1 >= 0 && y1 >= 0 && x2 <= w && y2 <= h && x2 >= x1 && y2 >= y1);

		if (x2 < x1 || y2 < y1)
			return;

		p = vdptroffset(dst.data, dst.pitch * y1 + x1 * formatInfo.qsize);
		w = x2 - x1;
		h = y2 - y1;
	}

	uint32 bpr = formatInfo.qsize * w;

	if (mOutputs[0].mSrcIndex == 0)
		VDPixmapGenerateFast(p, dst.pitch, h, mOutputs[0].mpSrc);
	else
		VDPixmapGenerate(p, dst.pitch, bpr, h, mOutputs[0].mpSrc, mOutputs[0].mSrcIndex);
}

void VDPixmapUberBlitter::Blit3(const VDPixmap& px, const vdrect32 *rDst, const FilterModPixmapInfo& src) {
	const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(px.format);

	int p0 = 0;
	int p1 = 1;
	int p2 = 2;
	if (!mbRGB) {
		p0 = 1;
		p1 = 2;
		p2 = 0;
	}
	
	IVDPixmapGen *gen  = mOutputs[p0].mpSrc;
	IVDPixmapGen *gen1 = mOutputs[p1].mpSrc;
	IVDPixmapGen *gen2 = mOutputs[p2].mpSrc;
	int idx  = mOutputs[p0].mSrcIndex;
	int idx1 = mOutputs[p1].mSrcIndex;
	int idx2 = mOutputs[p2].mSrcIndex;

	FilterModPixmapInfo unused;

	gen->AddWindowRequest(0, 0);
	gen->TransformPixmapInfo(src,(const_cast<VDPixmap&>(px)).info);
	gen->Start();
	gen1->AddWindowRequest(0, 0);
	gen1->TransformPixmapInfo(src,unused);
	gen1->Start();
	gen2->AddWindowRequest(0, 0);
	gen2->TransformPixmapInfo(src,unused);
	gen2->Start();

	uint32 auxstep = 0x80000000UL >> formatInfo.auxhbits;
	uint32 auxaccum = 0;

	auxstep += auxstep;

	int qw = px.w;
	int qh = px.h;

	if (formatInfo.qchunky) {
		qw = (qw + formatInfo.qw - 1) / formatInfo.qw;
		qh = -(-qh >> formatInfo.qhbits);
	}

	uint32 height = qh;
	uint32 bpr = formatInfo.qsize * qw;
	uint32 bpr2 = formatInfo.auxsize * -(-px.w >> formatInfo.auxwbits);
	uint8 *dst = (uint8 *)px.data;
	uint8 *dst2 = (uint8 *)px.data2;
	uint8 *dst3 = (uint8 *)px.data3;
	ptrdiff_t pitch = px.pitch;
	ptrdiff_t pitch2 = px.pitch2;
	ptrdiff_t pitch3 = px.pitch3;
	uint32 y2 = 0;
	for(uint32 y=0; y<height; ++y) {
		memcpy(dst, gen->GetRow(y, idx), bpr);
		vdptrstep(dst, pitch);

		if (!auxaccum) {
			memcpy(dst2, gen1->GetRow(y2, idx1), bpr2);
			vdptrstep(dst2, pitch2);
			memcpy(dst3, gen2->GetRow(y2, idx2), bpr2);
			vdptrstep(dst3, pitch3);
			++y2;
		}

		auxaccum += auxstep;
	}

	VDCPUCleanupExtensions();
}

void VDPixmapUberBlitter::Blit3Split(const VDPixmap& px, const vdrect32 *rDst, const FilterModPixmapInfo& src) {
	const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(px.format);

	int p0 = 0;
	int p1 = 1;
	int p2 = 2;
	if (!mbRGB) {
		p0 = 1;
		p1 = 2;
		p2 = 0;
	}
	
	IVDPixmapGen *gen  = mOutputs[p0].mpSrc;
	IVDPixmapGen *gen1 = mOutputs[p1].mpSrc;
	IVDPixmapGen *gen2 = mOutputs[p2].mpSrc;
	int idx  = mOutputs[p0].mSrcIndex;
	int idx1 = mOutputs[p1].mSrcIndex;
	int idx2 = mOutputs[p2].mSrcIndex;

	FilterModPixmapInfo unused;

	gen->AddWindowRequest(0, 0);
	gen->TransformPixmapInfo(src,(const_cast<VDPixmap&>(px)).info);
	gen->Start();
	gen1->AddWindowRequest(0, 0);
	gen1->TransformPixmapInfo(src,unused);
	gen1->Start();
	gen2->AddWindowRequest(0, 0);
	gen2->TransformPixmapInfo(src,unused);
	gen2->Start();

	uint32 auxstep = 0x80000000UL >> formatInfo.auxhbits;
	uint32 auxaccum = 0;

	auxstep += auxstep;

	int qw = px.w;
	int qh = px.h;

	if (formatInfo.qchunky) {
		qw = (qw + formatInfo.qw - 1) / formatInfo.qw;
		qh = -(-qh >> formatInfo.qhbits);
	}

	uint32 height = qh;
	uint32 bpr = formatInfo.qsize * qw;
	uint8 *dst = (uint8 *)px.data;
	ptrdiff_t pitch = px.pitch;

	if (idx == 0) {
		for(uint32 y=0; y<height; ++y) {
			gen->ProcessRow(dst, y);
			vdptrstep(dst, pitch);
		}
	} else {
		for(uint32 y=0; y<height; ++y) {
			memcpy(dst, gen->GetRow(y, idx), bpr);
			vdptrstep(dst, pitch);
		}
	}

	uint32 bpr2 = -(-px.w >> formatInfo.auxwbits) * formatInfo.auxsize;
	uint8 *dst2 = (uint8 *)px.data2;
	uint8 *dst3 = (uint8 *)px.data3;
	ptrdiff_t pitch2 = px.pitch2;
	ptrdiff_t pitch3 = px.pitch3;
	uint32 y2 = 0;
	for(uint32 y=0; y<height; ++y) {
		if (!auxaccum) {
			memcpy(dst2, gen1->GetRow(y2, idx1), bpr2);
			vdptrstep(dst2, pitch2);
			memcpy(dst3, gen2->GetRow(y2, idx2), bpr2);
			vdptrstep(dst3, pitch3);
			++y2;
		}

		auxaccum += auxstep;
	}

	VDCPUCleanupExtensions();
}

void VDPixmapUberBlitter::Blit3Separated(const VDPixmap& px, const vdrect32 *rDst, const FilterModPixmapInfo& src) {
	const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(px.format);

	int p0 = 0;
	int p1 = 1;
	int p2 = 2;
	if (!mbRGB) {
		p0 = 1;
		p1 = 2;
		p2 = 0;
	}

	IVDPixmapGen *gen  = mOutputs[p0].mpSrc;
	IVDPixmapGen *gen1 = mOutputs[p1].mpSrc;
	IVDPixmapGen *gen2 = mOutputs[p2].mpSrc;
	int idx  = mOutputs[p0].mSrcIndex;
	int idx1 = mOutputs[p1].mSrcIndex;
	int idx2 = mOutputs[p2].mSrcIndex;

	FilterModPixmapInfo unused;

	gen->AddWindowRequest(0, 0);
	gen->TransformPixmapInfo(src,(const_cast<VDPixmap&>(px)).info);
	gen->Start();
	gen1->AddWindowRequest(0, 0);
	gen1->TransformPixmapInfo(src,unused);
	gen1->Start();
	gen2->AddWindowRequest(0, 0);
	gen2->TransformPixmapInfo(src,unused);
	gen2->Start();

	int qw = px.w;
	int qh = px.h;

	if (formatInfo.qchunky) {
		qw = (qw + formatInfo.qw - 1) / formatInfo.qw;
		qh = -(-qh >> formatInfo.qhbits);
	}

	uint32 height = qh;
	uint32 bpr = formatInfo.qsize * qw;
	uint8 *dst = (uint8 *)px.data;
	ptrdiff_t pitch = px.pitch;

	if (idx == 0) {
		for(uint32 y=0; y<height; ++y) {
			gen->ProcessRow(dst, y);
			vdptrstep(dst, pitch);
		}
	} else {
		for(uint32 y=0; y<height; ++y) {
			memcpy(dst, gen->GetRow(y, idx), bpr);
			vdptrstep(dst, pitch);
		}
	}

	uint32 bpr2 = -(-px.w >> formatInfo.auxwbits) * formatInfo.auxsize;
	uint32 h2 = -(-px.h >> formatInfo.auxhbits);
	uint8 *dst2 = (uint8 *)px.data2;
	ptrdiff_t pitch2 = px.pitch2;
	if (idx1 == 0) {
		for(uint32 y2=0; y2<h2; ++y2) {
			gen1->ProcessRow(dst2, y2);
			vdptrstep(dst2, pitch2);
		}
	} else {
		for(uint32 y2=0; y2<h2; ++y2) {
			memcpy(dst2, gen1->GetRow(y2, idx1), bpr2);
			vdptrstep(dst2, pitch2);
		}
	}

	uint8 *dst3 = (uint8 *)px.data3;
	ptrdiff_t pitch3 = px.pitch3;
	if (idx2 == 0) {
		for(uint32 y2=0; y2<h2; ++y2) {
			gen2->ProcessRow(dst3, y2);
			vdptrstep(dst3, pitch3);
		}
	} else {
		for(uint32 y2=0; y2<h2; ++y2) {
			memcpy(dst3, gen2->GetRow(y2, idx2), bpr2);
			vdptrstep(dst3, pitch3);
		}
	}

	VDCPUCleanupExtensions();
}

void VDPixmapUberBlitter::Blit2(const VDPixmap& px, const vdrect32 *rDst, const FilterModPixmapInfo& src) {
	const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(px.format);
	IVDPixmapGen *gen = mOutputs[0].mpSrc;
	int idx = mOutputs[0].mSrcIndex;
	IVDPixmapGen *gen1 = mOutputs[1].mpSrc;
	int idx1 = mOutputs[1].mSrcIndex;

	FilterModPixmapInfo unused;

	gen->AddWindowRequest(0, 0);
	gen->TransformPixmapInfo(src,(const_cast<VDPixmap&>(px)).info);
	gen->Start();
	gen1->AddWindowRequest(0, 0);
	gen1->TransformPixmapInfo(src,unused);
	gen1->Start();

	uint32 auxstep = 0x80000000UL >> formatInfo.auxhbits;
	uint32 auxaccum = 0;

	auxstep += auxstep;

	int qw = px.w;
	int qh = px.h;

	if (formatInfo.qchunky) {
		qw = (qw + formatInfo.qw - 1) / formatInfo.qw;
		qh = -(-qh >> formatInfo.qhbits);
	}

	uint32 height = qh;
	uint32 bpr = formatInfo.qsize * qw;
	uint32 bpr2 = formatInfo.auxsize * -(-px.w >> formatInfo.auxwbits);
	uint8 *dst = (uint8 *)px.data;
	uint8 *dst2 = (uint8 *)px.data2;
	ptrdiff_t pitch = px.pitch;
	ptrdiff_t pitch2 = px.pitch2;
	uint32 y2 = 0;
	for(uint32 y=0; y<height; ++y) {
		memcpy(dst, gen->GetRow(y, idx), bpr);
		vdptrstep(dst, pitch);

		if (!auxaccum) {
			memcpy(dst2, gen1->GetRow(y2, idx1), bpr2);
			vdptrstep(dst2, pitch2);
			++y2;
		}

		auxaccum += auxstep;
	}

	VDCPUCleanupExtensions();
}

void VDPixmapUberBlitter::Blit2Separated(const VDPixmap& px, const vdrect32 *rDst, const FilterModPixmapInfo& src) {
	const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(px.format);
	IVDPixmapGen *gen = mOutputs[0].mpSrc;
	int idx = mOutputs[0].mSrcIndex;
	IVDPixmapGen *gen1 = mOutputs[1].mpSrc;
	int idx1 = mOutputs[1].mSrcIndex;

	FilterModPixmapInfo unused;

	gen->AddWindowRequest(0, 0);
	gen->TransformPixmapInfo(src,(const_cast<VDPixmap&>(px)).info);
	gen->Start();
	gen1->AddWindowRequest(0, 0);
	gen1->TransformPixmapInfo(src,unused);
	gen1->Start();

	int qw = px.w;
	int qh = px.h;

	if (formatInfo.qchunky) {
		qw = (qw + formatInfo.qw - 1) / formatInfo.qw;
		qh = -(-qh >> formatInfo.qhbits);
	}

	uint32 height = qh;
	uint32 bpr = formatInfo.qsize * qw;
	uint8 *dst = (uint8 *)px.data;
	ptrdiff_t pitch = px.pitch;

	if (idx == 0) {
		for(uint32 y=0; y<height; ++y) {
			gen->ProcessRow(dst, y);
			vdptrstep(dst, pitch);
		}
	} else {
		for(uint32 y=0; y<height; ++y) {
			memcpy(dst, gen->GetRow(y, idx), bpr);
			vdptrstep(dst, pitch);
		}
	}

	uint32 bpr2 = -(-px.w >> formatInfo.auxwbits) * formatInfo.auxsize;
	uint32 h2 = -(-px.h >> formatInfo.auxhbits);
	uint8 *dst2 = (uint8 *)px.data2;
	ptrdiff_t pitch2 = px.pitch2;
	if (idx1 == 0) {
		for(uint32 y2=0; y2<h2; ++y2) {
			gen1->ProcessRow(dst2, y2);
			vdptrstep(dst2, pitch2);
		}
	} else {
		for(uint32 y2=0; y2<h2; ++y2) {
			memcpy(dst2, gen1->GetRow(y2, idx1), bpr2);
			vdptrstep(dst2, pitch2);
		}
	}

	VDCPUCleanupExtensions();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
VDPixmapUberBlitterGenerator::VDPixmapUberBlitterGenerator() {
}

VDPixmapUberBlitterGenerator::~VDPixmapUberBlitterGenerator() {
	while(!mGenerators.empty()) {
		delete mGenerators.back();
		mGenerators.pop_back();
	}
}

void VDPixmapUberBlitterGenerator::debug_dump() {
	VDString r = dump();
	OutputDebugString(r.c_str());
}

VDString VDPixmapUberBlitterGenerator::dump() {
	VDString r;
	{for(int i=0; i<(int)mStack.size(); i++){
		StackEntry& s = mStack[i];
		r.append_sprintf("[entry %d]:%d<-",i,s.mSrcIndex);
		r += dump_gen(s.mpSrc);
		r += "\n";
	}}
	return r;
}

VDString VDPixmapUberBlitterGenerator::dump_gen(IVDPixmapGen* src) {
	VDString r;
	while (src) {
		IVDPixmapGen** px = std::find(mGenerators.begin(),mGenerators.end(),src);
		int x = px-mGenerators.begin();
    int plane = -1;
    {for(int i=0; i<(int)mSources.size(); i++){
      SourceEntry& se = mSources[i];
      if (se.mpGen==src) plane = se.mSrcPlane;
    }}
    if (plane!=-1)
		  r.append_sprintf("[%s #%d plane %d]",src->dump_name(),x,plane);
    else
		  r.append_sprintf("[%s #%d]",src->dump_name(),x);

		if (!src->dump_src(1)) {
			src = src->dump_src(0);
			if (src) r += "<-";
		} else {
			r += "(";
			{for(int i=0; ;i++){
				IVDPixmapGen* src1 = src->dump_src(i);
				if(!src1) break;
				if(i>0) r += ", ";
				r += dump_gen(src1);
			}}
			r += ")";
			break;
		}
	}
	return r;
}

void VDPixmapUberBlitterGenerator::swap(int index) {
	std::swap(mStack.back(), (&mStack.back())[-index]);
}

void VDPixmapUberBlitterGenerator::swap(VDPixmapGenWindowBasedOneSourceSimple* extra, int srcIndex) {
	StackEntry& e = mStack[srcIndex];
	extra->Init(e.mpSrc, e.mSrcIndex);

	mGenerators.push_back(extra);
	MarkDependency(extra, e.mpSrc);
	e = StackEntry(extra, 0);
}

void VDPixmapUberBlitterGenerator::move_to_end(int index) {
	StackEntry e = mStack[index];
	mStack.erase(mStack.begin()+index);
	mStack.push_back(e);
}

void VDPixmapUberBlitterGenerator::move_to(int index) {
	StackEntry e = mStack.back();
	mStack.pop_back();
	mStack.insert(mStack.begin()+index,e);
}

void VDPixmapUberBlitterGenerator::dup() {
	mStack.push_back(mStack.back());
}

void VDPixmapUberBlitterGenerator::pop() {
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::pop(StackEntry& se) {
	se = mStack.back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::push(StackEntry& se) {
	mStack.push_back(se);
}

void VDPixmapUberBlitterGenerator::ldsrc(int srcIndex, int srcPlane, int x, int y, uint32 w, uint32 h, uint32 type, uint32 bpr) {
	VDPixmapGenSrc *src = srcPlane==3 ? new VDPixmapGenSrcAlpha : new VDPixmapGenSrc;

	src->Init(w, h, type, bpr);
	src->SetPlane(srcPlane);

	mGenerators.push_back(src);
	mStack.push_back(StackEntry(src, 0));

	SourceEntry se;
	se.mpSrc = src;
	se.mpGen = src;
	se.mSrcIndex = srcIndex;
	se.mSrcPlane = srcPlane;
	se.mSrcX = x;
	se.mSrcY = y;
	mSources.push_back(se);
}

void VDPixmapUberBlitterGenerator::ldconst(uint8 fill, uint32 bpr, uint32 w, uint32 h, uint32 type) {
	VDPixmapGenFill8 *src = new VDPixmapGenFill8;

	src->Init(fill, bpr, w, h, type);

	mGenerators.push_back(src);
	mStack.push_back(StackEntry(src, 0));
}

void VDPixmapUberBlitterGenerator::ldconstF(float fill, uint32 bpr, uint32 w, uint32 h, uint32 type) {
	VDPixmapGenFillF *src = new VDPixmapGenFillF;

	src->Init(fill, bpr, w, h, type);

	mGenerators.push_back(src);
	mStack.push_back(StackEntry(src, 0));
}

void VDPixmapUberBlitterGenerator::extract_8in16(int offset, uint32 w, uint32 h) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_8In16 *src = NULL;
	
#if VD_CPU_X86
	if (MMX_enabled) {
		if (offset == 0)
			src = new VDPixmapGen_8In16_Even_MMX;
		else if (offset == 1)
			src = new VDPixmapGen_8In16_Odd_MMX;
	}
#endif
	if (!src)
		src = new VDPixmapGen_8In16;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, offset, w, h);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::extract_8in32(int offset, uint32 w, uint32 h, bool alpha) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_8In32 *src = NULL;

#if VD_CPU_X86
	if (MMX_enabled) {
		if ((unsigned)offset < 4)
			src = new VDPixmapGen_8In32_MMX;
	}
#endif

	if (!src)
		src = new VDPixmapGen_8In32;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, offset, w, h);
	src->alpha = alpha;

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::extract_16in32(int offset, uint32 w, uint32 h) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_16In32 *src = new VDPixmapGen_16In32;
	src->Init(args[0].mpSrc, args[0].mSrcIndex, offset, w, h);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::extract_16in64(int offset, uint32 w, uint32 h, bool alpha) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_16In64 *src = new VDPixmapGen_16In64;
	src->Init(args[0].mpSrc, args[0].mSrcIndex, offset, w, h);
	src->alpha = alpha;

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::extract_32in128(int offset, uint32 w, uint32 h, bool alpha) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_32In128 *src = new VDPixmapGen_32In128;
	src->Init(args[0].mpSrc, args[0].mSrcIndex, offset, w, h);
	src->alpha = alpha;

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::swap_8in16(uint32 w, uint32 h, uint32 bpr) {
	StackEntry *args = &mStack.back();

#if VD_CPU_X86
	VDPixmapGen_Swap8In16 *src = MMX_enabled ? new VDPixmapGen_Swap8In16_MMX : new VDPixmapGen_Swap8In16;
#else
	VDPixmapGen_Swap8In16 *src = new VDPixmapGen_Swap8In16;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex, w, h, bpr);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_Pal1_to_8888(int srcIndex) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_Pal1_To_X8R8G8B8 *src = new VDPixmapGen_Pal1_To_X8R8G8B8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);
	src->SetPlane(0);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);

	SourceEntry se;
	se.mpSrc = src;
	se.mpGen = src;
	se.mSrcIndex = srcIndex;
	se.mSrcPlane = 0;
	se.mSrcX = 0;
	se.mSrcY = 0;
	mSources.push_back(se);
}

void VDPixmapUberBlitterGenerator::conv_Pal2_to_8888(int srcIndex) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_Pal2_To_X8R8G8B8 *src = new VDPixmapGen_Pal2_To_X8R8G8B8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);
	src->SetPlane(0);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);

	SourceEntry se;
	se.mpSrc = src;
	se.mpGen = src;
	se.mSrcIndex = srcIndex;
	se.mSrcPlane = 0;
	se.mSrcX = 0;
	se.mSrcY = 0;
	mSources.push_back(se);
}

void VDPixmapUberBlitterGenerator::conv_Pal4_to_8888(int srcIndex) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_Pal4_To_X8R8G8B8 *src = new VDPixmapGen_Pal4_To_X8R8G8B8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);
	src->SetPlane(0);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);

	SourceEntry se;
	se.mpSrc = src;
	se.mpGen = src;
	se.mSrcIndex = srcIndex;
	se.mSrcPlane = 0;
	se.mSrcX = 0;
	se.mSrcY = 0;
	mSources.push_back(se);
}

void VDPixmapUberBlitterGenerator::conv_Pal8_to_8888(int srcIndex) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_Pal8_To_X8R8G8B8 *src = new VDPixmapGen_Pal8_To_X8R8G8B8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);
	src->SetPlane(0);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);

	SourceEntry se;
	se.mpSrc = src;
	se.mpGen = src;
	se.mSrcIndex = srcIndex;
	se.mSrcPlane = 0;
	se.mSrcX = 0;
	se.mSrcY = 0;
	mSources.push_back(se);
}

void VDPixmapUberBlitterGenerator::pointh(float xoffset, float xfactor, uint32 w) {
	StackEntry *args = &mStack.back();

	if (xoffset != 0.5f || xfactor != 1.0f) {
		VDPixmapGenResampleRow *src = new VDPixmapGenResampleRow;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, w, xoffset, xfactor, nsVDPixmap::kFilterPoint, 0, false);

		mGenerators.push_back(src);
		MarkDependency(src, args[0].mpSrc);
		args[0] = StackEntry(src, 0);
	}
}

void VDPixmapUberBlitterGenerator::pointv(float yoffset, float yfactor, uint32 h) {
	StackEntry *args = &mStack.back();

	if (yoffset != 0.5f || yfactor != 1.0f) {
		VDPixmapGenResampleCol *src = new VDPixmapGenResampleCol;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, h, yoffset, yfactor, nsVDPixmap::kFilterPoint, 0, false);

		mGenerators.push_back(src);
		MarkDependency(src, args[0].mpSrc);
		args[0] = StackEntry(src, 0);
	}
}

void VDPixmapUberBlitterGenerator::linearh(float xoffset, float xfactor, uint32 w, bool interpOnly) {
	StackEntry *args = &mStack.back();
	IVDPixmapGen *src = args[0].mpSrc;
	int srcIndex = args[0].mSrcIndex;

	sint32 srcw = src->GetWidth(srcIndex);
	if (xoffset == 0.5f && xfactor == 1.0f && srcw == w)
		return;

	if (xoffset == 0.5f && (src->GetType(srcIndex) & kVDPixType_Mask) == kVDPixType_8) {
		if (xfactor == 2.0f && w == ((srcw + 1) >> 1)) {
			VDPixmapGenResampleRow_d2_p0_lin_u8 *out = new VDPixmapGenResampleRow_d2_p0_lin_u8;

			out->Init(src, srcIndex);
			mGenerators.push_back(out);
			MarkDependency(out, src);
			args[0] = StackEntry(out, 0);
			return;
		}

		if (xfactor == 4.0f && w == ((srcw + 3) >> 2)) {
			VDPixmapGenResampleRow_d4_p0_lin_u8 *out = new VDPixmapGenResampleRow_d4_p0_lin_u8;

			out->Init(src, srcIndex);
			mGenerators.push_back(out);
			MarkDependency(out, src);
			args[0] = StackEntry(out, 0);
			return;
		}

		if (xfactor == 0.5f && w == srcw*2) {
#if VD_CPU_X86
			VDPixmapGenResampleRow_x2_p0_lin_u8 *out = ISSE_enabled ? new VDPixmapGenResampleRow_x2_p0_lin_u8_ISSE : new VDPixmapGenResampleRow_x2_p0_lin_u8;
#else
			VDPixmapGenResampleRow_x2_p0_lin_u8 *out = new VDPixmapGenResampleRow_x2_p0_lin_u8;
#endif

			out->Init(src, srcIndex);
			mGenerators.push_back(out);
			MarkDependency(out, src);
			args[0] = StackEntry(out, 0);
			return;
		}

		if (xfactor == 0.25f && w == srcw*4) {
#if VD_CPU_X86
			VDPixmapGenResampleRow_x4_p0_lin_u8 *out = MMX_enabled ? new VDPixmapGenResampleRow_x4_p0_lin_u8_MMX : new VDPixmapGenResampleRow_x4_p0_lin_u8;
#else
			VDPixmapGenResampleRow_x4_p0_lin_u8 *out = new VDPixmapGenResampleRow_x4_p0_lin_u8;
#endif

			out->Init(src, srcIndex);
			mGenerators.push_back(out);
			MarkDependency(out, src);
			args[0] = StackEntry(out, 0);
			return;
		}
	}

	if (xoffset == 0.5f && (src->GetType(srcIndex) & kVDPixType_Mask) == kVDPixType_16_LE) {
		if (xfactor == 0.5f && w == srcw*2) {
			VDPixmapGenResampleRow_x2_p0_lin_u16 *out = new VDPixmapGenResampleRow_x2_p0_lin_u16;

			out->Init(src, srcIndex);
			mGenerators.push_back(out);
			MarkDependency(out, src);
			args[0] = StackEntry(out, 0);
			return;
		}
	}

	VDPixmapGenResampleRow *out = new VDPixmapGenResampleRow;

	out->Init(args[0].mpSrc, args[0].mSrcIndex, w, xoffset, xfactor, nsVDPixmap::kFilterLinear, 0, interpOnly);

	mGenerators.push_back(out);
	MarkDependency(out, src);
	args[0] = StackEntry(out, 0);
}

void VDPixmapUberBlitterGenerator::linearv(float yoffset, float yfactor, uint32 h, bool interpOnly) {
	StackEntry *args = &mStack.back();
	IVDPixmapGen *src = args[0].mpSrc;
	int srcIndex = args[0].mSrcIndex;

	sint32 srch = src->GetHeight(srcIndex);
	if (yoffset == 0.5f && yfactor == 1.0f && srch == h)
		return;

	if ((src->GetType(srcIndex) & kVDPixType_Mask) == kVDPixType_8) {
		if (yoffset == 1.0f && yfactor == 2.0f && h == ((srch + 1) >> 1)) {
			VDPixmapGenResampleCol_x2_phalf_lin_u8 *out = new VDPixmapGenResampleCol_x2_phalf_lin_u8;

			out->Init(src, srcIndex);
			mGenerators.push_back(out);
			MarkDependency(out, src);
			args[0] = StackEntry(out, 0);
			return;
		}

		if (yoffset == 2.0f && yfactor == 4.0f && h == ((srch + 2) >> 2)) {
			VDPixmapGenResampleCol_x4_p1half_lin_u8 *out = new VDPixmapGenResampleCol_x4_p1half_lin_u8;

			out->Init(src, srcIndex);
			mGenerators.push_back(out);
			MarkDependency(out, src);
			args[0] = StackEntry(out, 0);
			return;
		}

		if (yoffset == 0.25f && yfactor == 0.5f && h == srch*2) {
#if VD_CPU_X86
			VDPixmapGenResampleCol_d2_pnqrtr_lin_u8 *out = ISSE_enabled ? new VDPixmapGenResampleCol_d2_pnqrtr_lin_u8_ISSE : new VDPixmapGenResampleCol_d2_pnqrtr_lin_u8;
#else
			VDPixmapGenResampleCol_d2_pnqrtr_lin_u8 *out = new VDPixmapGenResampleCol_d2_pnqrtr_lin_u8;
#endif

			out->Init(src, srcIndex);
			mGenerators.push_back(out);
			MarkDependency(out, src);
			args[0] = StackEntry(out, 0);
			return;
		}

		if (yoffset == 0.125f && yfactor == 0.25f && h == srch*4) {
#if VD_CPU_X86
			VDPixmapGenResampleCol_d4_pn38_lin_u8 *out = ISSE_enabled ? new VDPixmapGenResampleCol_d4_pn38_lin_u8_ISSE : new VDPixmapGenResampleCol_d4_pn38_lin_u8;
#else
			VDPixmapGenResampleCol_d4_pn38_lin_u8 *out = new VDPixmapGenResampleCol_d4_pn38_lin_u8;
#endif

			out->Init(src, srcIndex);
			mGenerators.push_back(out);
			MarkDependency(out, src);
			args[0] = StackEntry(out, 0);
			return;
		}
	}

	if ((src->GetType(srcIndex) & kVDPixType_Mask) == kVDPixType_16_LE) {
		if (yoffset == 0.25f && yfactor == 0.5f && h == srch*2) {
			VDPixmapGenResampleCol_d2_pnqrtr_lin_u16 *out = new VDPixmapGenResampleCol_d2_pnqrtr_lin_u16;

			out->Init(src, srcIndex);
			mGenerators.push_back(out);
			MarkDependency(out, src);
			args[0] = StackEntry(out, 0);
			return;
		}
	}

	VDPixmapGenResampleCol *out = new VDPixmapGenResampleCol;

	out->Init(src, srcIndex, h, yoffset, yfactor, nsVDPixmap::kFilterLinear, 0, interpOnly);

	mGenerators.push_back(out);
	MarkDependency(out, src);
	args[0] = StackEntry(out, 0);
}

void VDPixmapUberBlitterGenerator::linear(float xoffset, float xfactor, uint32 w, float yoffset, float yfactor, uint32 h) {
	linearh(xoffset, xfactor, w, false);
	linearv(yoffset, yfactor, h, false);
}

void VDPixmapUberBlitterGenerator::cubich(float xoffset, float xfactor, uint32 w, float splineFactor, bool interpOnly) {
	StackEntry *args = &mStack.back();

	if (xoffset != 0.5f || xfactor != 1.0f) {
		VDPixmapGenResampleRow *src = new VDPixmapGenResampleRow;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, w, xoffset, xfactor, nsVDPixmap::kFilterCubic, splineFactor, interpOnly);

		mGenerators.push_back(src);
		MarkDependency(src, args[0].mpSrc);
		args[0] = StackEntry(src, 0);
	}
}

void VDPixmapUberBlitterGenerator::cubicv(float yoffset, float yfactor, uint32 h, float splineFactor, bool interpOnly) {
	StackEntry *args = &mStack.back();

	if (yoffset != 0.5f || yfactor != 1.0f) {
		VDPixmapGenResampleCol *src = new VDPixmapGenResampleCol;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, h, yoffset, yfactor, nsVDPixmap::kFilterCubic, splineFactor, interpOnly);

		mGenerators.push_back(src);
		MarkDependency(src, args[0].mpSrc);
		args[0] = StackEntry(src, 0);
	}
}

void VDPixmapUberBlitterGenerator::cubic(float xoffset, float xfactor, uint32 w, float yoffset, float yfactor, uint32 h, float splineFactor) {
	cubich(xoffset, xfactor, w, splineFactor, false);
	cubicv(yoffset, yfactor, h, splineFactor, false);
}

void VDPixmapUberBlitterGenerator::lanczos3h(float xoffset, float xfactor, uint32 w) {
	StackEntry *args = &mStack.back();

	if (xoffset != 0.5f || xfactor != 1.0f) {
		VDPixmapGenResampleRow *src = new VDPixmapGenResampleRow;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, w, xoffset, xfactor, nsVDPixmap::kFilterLanczos3, 0, false);

		mGenerators.push_back(src);
		MarkDependency(src, args[0].mpSrc);
		args[0] = StackEntry(src, 0);
	}
}

void VDPixmapUberBlitterGenerator::lanczos3v(float yoffset, float yfactor, uint32 h) {
	StackEntry *args = &mStack.back();

	if (yoffset != 0.5f || yfactor != 1.0f) {
		VDPixmapGenResampleCol *src = new VDPixmapGenResampleCol;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, h, yoffset, yfactor, nsVDPixmap::kFilterLanczos3, 0, false);

		mGenerators.push_back(src);
		MarkDependency(src, args[0].mpSrc);
		args[0] = StackEntry(src, 0);
	}
}

void VDPixmapUberBlitterGenerator::lanczos3(float xoffset, float xfactor, uint32 w, float yoffset, float yfactor, uint32 h) {
	lanczos3h(xoffset, xfactor, w);
	lanczos3v(yoffset, yfactor, h);
}

void VDPixmapUberBlitterGenerator::conv_555_to_8888() {
	StackEntry *args = &mStack.back();
#ifdef VD_CPU_X86
	VDPixmapGen_X1R5G5B5_To_X8R8G8B8 *src = MMX_enabled ? new VDPixmapGen_X1R5G5B5_To_X8R8G8B8_MMX : new VDPixmapGen_X1R5G5B5_To_X8R8G8B8;
#else
	VDPixmapGen_X1R5G5B5_To_X8R8G8B8 *src = new VDPixmapGen_X1R5G5B5_To_X8R8G8B8;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_565_to_8888() {
	StackEntry *args = &mStack.back();
#ifdef VD_CPU_X86
	VDPixmapGen_R5G6B5_To_X8R8G8B8 *src = MMX_enabled ? new VDPixmapGen_R5G6B5_To_X8R8G8B8_MMX : new VDPixmapGen_R5G6B5_To_X8R8G8B8;
#else
	VDPixmapGen_R5G6B5_To_X8R8G8B8 *src = new VDPixmapGen_R5G6B5_To_X8R8G8B8;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_888_to_8888() {
	StackEntry *args = &mStack.back();
#ifdef VD_CPU_X86
	VDPixmapGen_R8G8B8_To_A8R8G8B8 *src = MMX_enabled ? new VDPixmapGen_R8G8B8_To_X8R8G8B8_MMX : new VDPixmapGen_R8G8B8_To_A8R8G8B8;
#else
	VDPixmapGen_R8G8B8_To_A8R8G8B8 *src = new VDPixmapGen_R8G8B8_To_A8R8G8B8;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_8_to_32F() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_8_To_32F *src = new VDPixmapGen_8_To_32F;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_8_to_16() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_8_To_16 *src = new VDPixmapGen_8_To_16;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_a8_to_a16() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_A8_To_A16 *src = new VDPixmapGen_A8_To_A16;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_r8_to_r16() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_R8_To_R16 *src = new VDPixmapGen_R8_To_R16;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_16F_to_32F() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_16F_To_32F *src = new VDPixmapGen_16F_To_32F;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_16_to_32F(bool chroma) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_16_To_32F *src = new VDPixmapGen_16_To_32F(chroma);

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_16_to_8() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_16_To_8 *src = new VDPixmapGen_16_To_8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_a16_to_a8() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_A16_To_A8 *src = new VDPixmapGen_A16_To_A8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_V210_to_32F() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_V210_To_32F *src = new VDPixmapGen_V210_To_32F;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.push_back(StackEntry(src, 1));
	mStack.push_back(StackEntry(src, 2));
}

void VDPixmapUberBlitterGenerator::conv_V210_to_P16() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_V210_To_P16 *src = new VDPixmapGen_V210_To_P16;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.push_back(StackEntry(src, 1));
	mStack.push_back(StackEntry(src, 2));
}

void VDPixmapUberBlitterGenerator::conv_YU64_to_P16() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_YU64_To_P16 *src = new VDPixmapGen_YU64_To_P16;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.push_back(StackEntry(src, 1));
	mStack.push_back(StackEntry(src, 2));
}

void VDPixmapUberBlitterGenerator::conv_P16_to_YU64() {
	StackEntry *args = &*(mStack.end() - 3);
	VDPixmapGen_P16_To_YU64 *src = new VDPixmapGen_P16_To_YU64;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::conv_V410_to_32F() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_V410_To_32F *src = new VDPixmapGen_V410_To_32F;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.push_back(StackEntry(src, 1));
	mStack.push_back(StackEntry(src, 2));
}

void VDPixmapUberBlitterGenerator::conv_V410_to_P16() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_V410_To_P16 *src = new VDPixmapGen_V410_To_P16;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.push_back(StackEntry(src, 1));
	mStack.push_back(StackEntry(src, 2));
}

void VDPixmapUberBlitterGenerator::conv_Y410_to_32F() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_Y410_To_32F *src = new VDPixmapGen_Y410_To_32F;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.push_back(StackEntry(src, 1));
	mStack.push_back(StackEntry(src, 2));
}

void VDPixmapUberBlitterGenerator::conv_Y410_to_P16() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_Y410_To_P16 *src = new VDPixmapGen_Y410_To_P16;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.push_back(StackEntry(src, 1));
	mStack.push_back(StackEntry(src, 2));
}

void VDPixmapUberBlitterGenerator::conv_V308_to_P8() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_V308_To_P8 *src = new VDPixmapGen_V308_To_P8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.push_back(StackEntry(src, 1));
	mStack.push_back(StackEntry(src, 2));
}

void VDPixmapUberBlitterGenerator::conv_P8_to_V308() {
	StackEntry *args = &*(mStack.end() - 3);
	VDPixmapGen_P8_To_V308 *src = new VDPixmapGen_P8_To_V308;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::conv_8888_to_X32F() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_X8R8G8B8_To_X32B32G32R32F *src = new VDPixmapGen_X8R8G8B8_To_X32B32G32R32F;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_8888_to_555() {
	StackEntry *args = &mStack.back();
#ifdef VD_CPU_X86
	VDPixmapGen_X8R8G8B8_To_X1R5G5B5 *src = MMX_enabled ? new VDPixmapGen_X8R8G8B8_To_X1R5G5B5_MMX : new VDPixmapGen_X8R8G8B8_To_X1R5G5B5;
#else
	VDPixmapGen_X8R8G8B8_To_X1R5G5B5 *src = new VDPixmapGen_X8R8G8B8_To_X1R5G5B5;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_555_to_565() {
	StackEntry *args = &mStack.back();
#ifdef VD_CPU_X86
	VDPixmapGen_X1R5G5B5_To_R5G6B5 *src = MMX_enabled ? new VDPixmapGen_X1R5G5B5_To_R5G6B5_MMX : new VDPixmapGen_X1R5G5B5_To_R5G6B5;
#else
	VDPixmapGen_X1R5G5B5_To_R5G6B5 *src = new VDPixmapGen_X1R5G5B5_To_R5G6B5;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_565_to_555() {
	StackEntry *args = &mStack.back();
#ifdef VD_CPU_X86
	VDPixmapGen_R5G6B5_To_X1R5G5B5 *src = MMX_enabled ? new VDPixmapGen_R5G6B5_To_X1R5G5B5_MMX : new VDPixmapGen_R5G6B5_To_X1R5G5B5;
#else
	VDPixmapGen_R5G6B5_To_X1R5G5B5 *src = new VDPixmapGen_R5G6B5_To_X1R5G5B5;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_8888_to_565() {
	StackEntry *args = &mStack.back();
#ifdef VD_CPU_X86
	VDPixmapGen_X8R8G8B8_To_R5G6B5 *src = MMX_enabled ? new VDPixmapGen_X8R8G8B8_To_R5G6B5_MMX : new VDPixmapGen_X8R8G8B8_To_R5G6B5;
#else
	VDPixmapGen_X8R8G8B8_To_R5G6B5 *src = new VDPixmapGen_X8R8G8B8_To_R5G6B5;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_8888_to_888() {
	StackEntry *args = &mStack.back();
#ifdef VD_CPU_X86
	VDPixmapGen_X8R8G8B8_To_R8G8B8 *src = MMX_enabled ? new VDPixmapGen_X8R8G8B8_To_R8G8B8_MMX : new VDPixmapGen_X8R8G8B8_To_R8G8B8;
#else
	VDPixmapGen_X8R8G8B8_To_R8G8B8 *src = new VDPixmapGen_X8R8G8B8_To_R8G8B8;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_32F_to_8() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_32F_To_8 *src = new VDPixmapGen_32F_To_8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_a32F_to_a8() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_a32F_To_a8 *src = new VDPixmapGen_a32F_To_a8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_X32F_to_8888() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_X32B32G32R32F_To_X8R8G8B8 *src = new VDPixmapGen_X32B32G32R32F_To_X8R8G8B8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_X16_to_8888() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_X16R16G16B16_To_X8R8G8B8 *src = new VDPixmapGen_X16R16G16B16_To_X8R8G8B8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_8888_to_X16() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_X8R8G8B8_To_X16R16G16B16 *src = new VDPixmapGen_X8R8G8B8_To_X16R16G16B16;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_X32F_to_X16() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_X32B32G32R32F_To_X16R16G16B16 *src = new VDPixmapGen_X32B32G32R32F_To_X16R16G16B16;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_X16_to_X32F() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_X16R16G16B16_To_X32B32G32R32F *src = new VDPixmapGen_X16R16G16B16_To_X32B32G32R32F;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_X16_to_R210() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_X16R16G16B16_Normalize *narg = new VDPixmapGen_X16R16G16B16_Normalize;
	narg->max_r = 0x3FF;
	narg->wipe_alpha = false;
	narg->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(narg);
	MarkDependency(narg, args[0].mpSrc);
	args[0] = StackEntry(narg, 0);

	VDPixmapGen_X16R16G16B16_To_R210 *src = new VDPixmapGen_X16R16G16B16_To_R210;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_X16_to_R10K() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_X16R16G16B16_Normalize *narg = new VDPixmapGen_X16R16G16B16_Normalize;
	narg->max_r = 0x3FF;
	narg->wipe_alpha = false;
	narg->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(narg);
	MarkDependency(narg, args[0].mpSrc);
	args[0] = StackEntry(narg, 0);

	VDPixmapGen_X16R16G16B16_To_R10K *src = new VDPixmapGen_X16R16G16B16_To_R10K;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_R210_to_X16() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_R210_To_X16R16G16B16 *src = new VDPixmapGen_R210_To_X16R16G16B16;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_R10K_to_X16() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_R10K_To_X16R16G16B16 *src = new VDPixmapGen_R10K_To_X16R16G16B16;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_32F_to_16F() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_32F_To_16F *src = new VDPixmapGen_32F_To_16F;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_32F_to_16(bool chroma) {
	StackEntry *args = &mStack.back();
	VDPixmapGen_32F_To_16 *src = new VDPixmapGen_32F_To_16(chroma);

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_32F_to_r16() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_32F_To_r16 *src = new VDPixmapGen_32F_To_r16;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::conv_32F_to_V210() {
	StackEntry *args = &*(mStack.end() - 3);
	VDPixmapGen_32F_To_V210 *src = new VDPixmapGen_32F_To_V210;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::conv_32F_to_V410() {
	StackEntry *args = &*(mStack.end() - 3);
	VDPixmapGen_32F_To_V410 *src = new VDPixmapGen_32F_To_V410;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::conv_32F_to_Y410() {
	StackEntry *args = &*(mStack.end() - 3);
	VDPixmapGen_32F_To_Y410 *src = new VDPixmapGen_32F_To_Y410;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::convd_8888_to_555() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_X8R8G8B8_To_X1R5G5B5_Dithered *src = new VDPixmapGen_X8R8G8B8_To_X1R5G5B5_Dithered;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::convd_8888_to_565() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_X8R8G8B8_To_R5G6B5_Dithered *src = new VDPixmapGen_X8R8G8B8_To_R5G6B5_Dithered;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::convd_32F_to_8() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_32F_To_8_Dithered *src = new VDPixmapGen_32F_To_8_Dithered;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::convd_X32F_to_8888() {
	StackEntry *args = &mStack.back();
	VDPixmapGen_X32B32G32R32F_To_X8R8G8B8_Dithered *src = new VDPixmapGen_X32B32G32R32F_To_X8R8G8B8_Dithered;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
}

void VDPixmapUberBlitterGenerator::interleave_B8G8_R8G8() {
	StackEntry *args = &mStack.back() - 2;
	VDPixmapGen_B8x3_To_B8G8_R8G8 *src = NULL;
	
#if VD_CPU_X86
	if (MMX_enabled)
		src = new VDPixmapGen_B8x3_To_B8G8_R8G8_MMX;
#endif

	if (!src)
		src = new VDPixmapGen_B8x3_To_B8G8_R8G8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::interleave_G8B8_G8R8() {
	StackEntry *args = &mStack.back() - 2;
	VDPixmapGen_B8x3_To_G8B8_G8R8 *src = NULL;
	
#if VD_CPU_X86
	if (MMX_enabled)
		src = new VDPixmapGen_B8x3_To_G8B8_G8R8_MMX;
#endif

	if (!src)
		src = new VDPixmapGen_B8x3_To_G8B8_G8R8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::interleave_X8R8G8B8(uint32 type) {
	if (type==kVDPixType_8_8_8) {
		StackEntry *args = &mStack.back() - 2;
		VDPixmapGen_B8x3_To_X8R8G8B8 *src = new VDPixmapGen_B8x3_To_X8R8G8B8;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

		mGenerators.push_back(src);
		MarkDependency(src, args[0].mpSrc);
		MarkDependency(src, args[1].mpSrc);
		MarkDependency(src, args[2].mpSrc);
		args[0] = StackEntry(src, 0);
		mStack.pop_back();
		mStack.pop_back();
		return;
	}
	if (type==kVDPixType_8_8_8_8) {
		StackEntry *args = &mStack.back() - 3;
		VDPixmapGen_B8x4_To_A8R8G8B8 *src = new VDPixmapGen_B8x4_To_A8R8G8B8;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex, args[3].mpSrc, args[3].mSrcIndex);

		mGenerators.push_back(src);
		MarkDependency(src, args[0].mpSrc);
		MarkDependency(src, args[1].mpSrc);
		MarkDependency(src, args[2].mpSrc);
		MarkDependency(src, args[3].mpSrc);
		args[0] = StackEntry(src, 0);
		mStack.pop_back();
		mStack.pop_back();
		mStack.pop_back();
		return;
	}
	VDASSERT(false);
}

void VDPixmapUberBlitterGenerator::interleave_RGB64(uint32 type) {
	if (type==kVDPixType_16_16_16_LE) {
		StackEntry *args = &mStack.back() - 2;
		VDPixmapGen_B16x3_To_RGB64 *src = new VDPixmapGen_B16x3_To_RGB64;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

		mGenerators.push_back(src);
		MarkDependency(src, args[0].mpSrc);
		MarkDependency(src, args[1].mpSrc);
		MarkDependency(src, args[2].mpSrc);
		args[0] = StackEntry(src, 0);
		mStack.pop_back();
		mStack.pop_back();
		return;
	}
	if (type==kVDPixType_16_16_16_16_LE) {
		StackEntry *args = &mStack.back() - 3;
		VDPixmapGen_B16x4_To_RGB64 *src = new VDPixmapGen_B16x4_To_RGB64;

		src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex, args[3].mpSrc, args[3].mSrcIndex);

		mGenerators.push_back(src);
		MarkDependency(src, args[0].mpSrc);
		MarkDependency(src, args[1].mpSrc);
		MarkDependency(src, args[2].mpSrc);
		MarkDependency(src, args[3].mpSrc);
		args[0] = StackEntry(src, 0);
		mStack.pop_back();
		mStack.pop_back();
		mStack.pop_back();
		return;
	}
	VDASSERT(false);
}

void VDPixmapUberBlitterGenerator::X8R8G8B8_to_A8R8G8B8() {
	StackEntry *args = &mStack.back() - 1;
	VDPixmapGen_X8R8G8B8_To_A8R8G8B8 *src = new VDPixmapGen_X8R8G8B8_To_A8R8G8B8;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::interleave_Y416() {
	StackEntry *args = &mStack.back() - 2;
	VDPixmapGen_B16x3_To_Y416 *src = new VDPixmapGen_B16x3_To_Y416;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::X16R16G16B16_to_A16R16G16B16() {
	StackEntry *args = &mStack.back() - 1;
	VDPixmapGen_X16R16G16B16_To_A16R16G16B16 *src = new VDPixmapGen_X16R16G16B16_To_A16R16G16B16;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::interleave_B16R16() {
	StackEntry *args = &mStack.back() - 1;
	VDPixmapGen_B16x2_To_B16R16 *src = new VDPixmapGen_B16x2_To_B16R16;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::interleave_B8R8() {
	StackEntry *args = &mStack.back() - 1;

#if VD_CPU_X86
	VDPixmapGen_B8x2_To_B8R8 *src = MMX_enabled ? new VDPixmapGen_B8x2_To_B8R8_MMX : new VDPixmapGen_B8x2_To_B8R8;
#else
	VDPixmapGen_B8x2_To_B8R8 *src = new VDPixmapGen_B8x2_To_B8R8;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::merge_fields(uint32 w, uint32 h, uint32 bpr) {
	StackEntry *args = &mStack.back() - 1;

	VDPixmapGen_MergeFields *src = new VDPixmapGen_MergeFields;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, w, h, bpr);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::split_fields(uint32 bpr) {
	StackEntry *args = &mStack.back();

	VDPixmapGen_SplitFields *src = new VDPixmapGen_SplitFields;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, bpr);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);

	args[0] = StackEntry(src, 0);
	mStack.push_back(StackEntry(src, 1));
}

void VDPixmapUberBlitterGenerator::ycbcr601_to_rgb32() {
	StackEntry *args = &mStack.back() - 2;

#ifdef VD_CPU_X86
	VDPixmapGenYCbCr601ToRGB32 *src = MMX_enabled ? new VDPixmapGenYCbCr601ToRGB32_MMX : new VDPixmapGenYCbCr601ToRGB32;
#else
	VDPixmapGenYCbCr601ToRGB32 *src = new VDPixmapGenYCbCr601ToRGB32;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::ycbcr709_to_rgb32() {
	StackEntry *args = &mStack.back() - 2;

	VDPixmapGenYCbCr709ToRGB32 *src = new VDPixmapGenYCbCr709ToRGB32;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::rgb32_to_ycbcr601() {
	StackEntry *args = &mStack.back();
#ifdef VD_CPU_X86
	VDPixmapGenRGB32ToYCbCr601 *src = SSE2_enabled ? new VDPixmapGenRGB32ToYCbCr601_SSE2 : new VDPixmapGenRGB32ToYCbCr601;
#else
	VDPixmapGenRGB32ToYCbCr601 *src = new VDPixmapGenRGB32ToYCbCr601;
#endif

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.push_back(StackEntry(src, 1));
	mStack.push_back(StackEntry(src, 2));
}

void VDPixmapUberBlitterGenerator::rgb32_to_ycbcr709() {
	StackEntry *args = &mStack.back();
	VDPixmapGenRGB32ToYCbCr709 *src = new VDPixmapGenRGB32ToYCbCr709;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.push_back(StackEntry(src, 1));
	mStack.push_back(StackEntry(src, 2));
}

void VDPixmapUberBlitterGenerator::ycbcr601_to_rgb32_32f() {
	StackEntry *args = &mStack.back() - 2;

	VDPixmapGenYCbCr601ToRGB32F *src = new VDPixmapGenYCbCr601ToRGB32F;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::ycbcr709_to_rgb32_32f() {
	StackEntry *args = &mStack.back() - 2;

	VDPixmapGenYCbCr709ToRGB32F *src = new VDPixmapGenYCbCr709ToRGB32F;

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::rgb32_to_ycbcr601_32f() {
	StackEntry *args = &mStack.back();
	VDPixmapGenRGB32FToYCbCr601 *src = new VDPixmapGenRGB32FToYCbCr601;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.push_back(StackEntry(src, 1));
	mStack.push_back(StackEntry(src, 2));
}

void VDPixmapUberBlitterGenerator::rgb32_to_ycbcr709_32f() {
	StackEntry *args = &mStack.back();
	VDPixmapGenRGB32FToYCbCr709 *src = new VDPixmapGenRGB32FToYCbCr709;

	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.push_back(StackEntry(src, 1));
	mStack.push_back(StackEntry(src, 2));
}

/*
void VDPixmapUberBlitterGenerator::ycbcr601_to_ycbcr709() {
	StackEntry *args = &mStack.back() - 2;

	IVDPixmapGen *src;
	if ((args[0].mpSrc->GetType(args[0].mSrcIndex) & kVDPixType_Mask) == kVDPixType_32F_LE) {
		VDPixmapGenYCbCr601ToYCbCr709_32F *src2 = new VDPixmapGenYCbCr601ToYCbCr709_32F;

		src2->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);
		src = src2;
	} else {
		VDPixmapGenYCbCr601ToYCbCr709 *src2 = new VDPixmapGenYCbCr601ToYCbCr709;

		src2->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);
		src = src2;
	}

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	args[1] = StackEntry(src, 1);
	args[2] = StackEntry(src, 2);
}

void VDPixmapUberBlitterGenerator::ycbcr709_to_ycbcr601() {
	StackEntry *args = &mStack.back() - 2;

	IVDPixmapGen *src;
	if ((args[0].mpSrc->GetType(args[0].mSrcIndex) & kVDPixType_Mask) == kVDPixType_32F_LE) {
		VDPixmapGenYCbCr709ToYCbCr601_32F *src2 = new VDPixmapGenYCbCr709ToYCbCr601_32F;

		src2->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);
		src = src2;
	} else {
		VDPixmapGenYCbCr709ToYCbCr601 *src2 = new VDPixmapGenYCbCr709ToYCbCr601;

		src2->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);
		src = src2;
	}

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	args[1] = StackEntry(src, 1);
	args[2] = StackEntry(src, 2);
}
*/

void VDPixmapUberBlitterGenerator::ycbcr_to_rgb32_generic(const VDPixmapGenYCbCrBasis& basis, bool studioRGB) {
	StackEntry *args = &mStack.back() - 2;

	VDPixmapGenYCbCrToRGB32Generic *src = new VDPixmapGenYCbCrToRGB32Generic(basis, studioRGB);

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::ycbcr_to_rgb64_generic(const VDPixmapGenYCbCrBasis& basis, bool studioRGB) {
	StackEntry *args = &mStack.back() - 2;

	VDPixmapGenYCbCrToRGB64Generic *src = new VDPixmapGenYCbCrToRGB64Generic(basis, studioRGB);

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::ycbcr_to_rgb32f_generic(const VDPixmapGenYCbCrBasis& basis, bool studioRGB) {
	StackEntry *args = &mStack.back() - 2;

	VDPixmapGenYCbCrToRGB32FGeneric *src = new VDPixmapGenYCbCrToRGB32FGeneric(basis, studioRGB);

	src->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.pop_back();
	mStack.pop_back();
}

void VDPixmapUberBlitterGenerator::rgb32_to_ycbcr_generic(const VDPixmapGenYCbCrBasis& basis, bool studioRGB, uint32 colorSpace) {
	StackEntry *args = &mStack.back();

	VDPixmapGenRGB32ToYCbCrGeneric *src = new VDPixmapGenRGB32ToYCbCrGeneric(basis, studioRGB, colorSpace);
	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.push_back(StackEntry(src, 1));
	mStack.push_back(StackEntry(src, 2));
}

void VDPixmapUberBlitterGenerator::rgb32f_to_ycbcr_generic(const VDPixmapGenYCbCrBasis& basis, bool studioRGB, uint32 colorSpace) {
	StackEntry *args = &mStack.back();

	VDPixmapGenRGB32FToYCbCrGeneric *src = new VDPixmapGenRGB32FToYCbCrGeneric(basis, studioRGB, colorSpace);
	src->Init(args[0].mpSrc, args[0].mSrcIndex);

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	args[0] = StackEntry(src, 0);
	mStack.push_back(StackEntry(src, 1));
	mStack.push_back(StackEntry(src, 2));
}

void VDPixmapUberBlitterGenerator::ycbcr_to_ycbcr_generic(const VDPixmapGenYCbCrBasis& basisDst, bool dstLimitedRange, const VDPixmapGenYCbCrBasis& basisSrc, bool srcLimitedRange, uint32 colorSpace) {
	StackEntry *args = &mStack.back() - 2;

	IVDPixmapGen *src;
	if ((args[0].mpSrc->GetType(args[0].mSrcIndex) & kVDPixType_Mask) == kVDPixType_32F_LE) {
		VDPixmapGenYCbCrToYCbCrGeneric_32F *src2 = new VDPixmapGenYCbCrToYCbCrGeneric_32F(basisDst, dstLimitedRange, basisSrc, srcLimitedRange, colorSpace);

		src2->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);
		src = src2;
	} else {
		VDPixmapGenYCbCrToYCbCrGeneric *src2 = new VDPixmapGenYCbCrToYCbCrGeneric(basisDst, dstLimitedRange, basisSrc, srcLimitedRange, colorSpace);

		src2->Init(args[0].mpSrc, args[0].mSrcIndex, args[1].mpSrc, args[1].mSrcIndex, args[2].mpSrc, args[2].mSrcIndex);
		src = src2;
	}

	mGenerators.push_back(src);
	MarkDependency(src, args[0].mpSrc);
	MarkDependency(src, args[1].mpSrc);
	MarkDependency(src, args[2].mpSrc);
	args[0] = StackEntry(src, 0);
	args[1] = StackEntry(src, 1);
	args[2] = StackEntry(src, 2);
}

IVDPixmapBlitter *VDPixmapUberBlitterGenerator::create() {
	vdautoptr<VDPixmapUberBlitter> blitter(new VDPixmapUberBlitter);

	int numStackEntries = (int)mStack.size();

	for(int i=0; i<4; ++i) {
		if (i < numStackEntries) {
			blitter->mOutputs[i].mpSrc = mStack[i].mpSrc;
			blitter->mOutputs[i].mSrcIndex = mStack[i].mSrcIndex;
		} else {
			blitter->mOutputs[i].mpSrc = NULL;
			blitter->mOutputs[i].mSrcIndex = 0;
		}
	}

	mStack.clear();

	blitter->mbRGB = mbRGB;
	blitter->mbIndependentAlpha = false;
	if (numStackEntries >= 4) {
		blitter->mbIndependentAlpha = true;
	}

	// If this blitter has three outputs, determine if outputs 1 and 2 are independent
	// from output 0.
	blitter->mbIndependentChromaPlanes = true;
	blitter->mbIndependentPlanes = true;
	if (numStackEntries >= 3) {
		int numGens = mGenerators.size();
		vdfastvector<uint8> genflags(numGens, 0);

		enum {
			kFlagStateful = 0x80,
			kFlagY = 0x01,
			kFlagCb = 0x02,
			kFlagCr = 0x04,
			kFlagYCbCr = 0x07
		};

		for(int i=0; i<3; ++i)
			genflags[std::find(mGenerators.begin(), mGenerators.end(), blitter->mOutputs[i].mpSrc) - mGenerators.begin()] |= (1 << i);

		for(int i=0; i<numGens; ++i) {
			IVDPixmapGen *gen = mGenerators[i];

			if (gen->IsStateful())
				genflags[i] |= kFlagStateful;
		}

		while(!mDependencies.empty()) {
			const Dependency& dep = mDependencies.back();

			genflags[dep.mSrcIdx] |= (genflags[dep.mDstIdx] & ~kFlagStateful);

			mDependencies.pop_back();
		}

		for(int i=0; i<numGens; ++i) {
			uint8 flags = genflags[i];

			if (!(flags & kFlagStateful))
				continue;

			switch(flags & kFlagYCbCr) {
				case 0:
				case kFlagY:
				case kFlagCb:
				case kFlagCr:
					break;
				case kFlagCr | kFlagCb:
					blitter->mbIndependentPlanes = false;
					break;
				case kFlagCb | kFlagY:
				case kFlagCr | kFlagY:
				case kFlagCr | kFlagCb | kFlagY:
					blitter->mbIndependentPlanes = false;
					blitter->mbIndependentChromaPlanes = false;
					break;
			}
		}
	} else if (numStackEntries >= 2) {
		int numGens = mGenerators.size();
		vdfastvector<uint8> genflags(numGens, 0);

		enum {
			kFlagStateful = 0x80,
			kFlagY = 0x01,
			kFlagC = 0x02,
			kFlagYC = 0x03
		};

		for(int i=0; i<2; ++i)
			genflags[std::find(mGenerators.begin(), mGenerators.end(), blitter->mOutputs[i].mpSrc) - mGenerators.begin()] |= (1 << i);

		for(int i=0; i<numGens; ++i) {
			IVDPixmapGen *gen = mGenerators[i];

			if (gen->IsStateful())
				genflags[i] |= kFlagStateful;
		}

		while(!mDependencies.empty()) {
			const Dependency& dep = mDependencies.back();

			genflags[dep.mSrcIdx] |= (genflags[dep.mDstIdx] & ~kFlagStateful);

			mDependencies.pop_back();
		}

		for(int i=0; i<numGens; ++i) {
			uint8 flags = genflags[i];

			if (!(flags & kFlagStateful))
				continue;

			switch(flags & kFlagYC) {
				case kFlagYC:
					blitter->mbIndependentPlanes = false;
					blitter->mbIndependentChromaPlanes = false;
					break;
			}
		}
	}

	blitter->mGenerators.swap(mGenerators);
	blitter->mSources.swap(mSources);
	return blitter.release();
}

void VDPixmapUberBlitterGenerator::MarkDependency(IVDPixmapGen *dst, IVDPixmapGen *src) {
	Generators::const_iterator it1(std::find(mGenerators.begin(), mGenerators.end(), dst));
	Generators::const_iterator it2(std::find(mGenerators.begin(), mGenerators.end(), src));

	VDASSERT(it1 != mGenerators.end());
	VDASSERT(it2 != mGenerators.end());

	int idx1 = it1 - mGenerators.begin();
	int idx2 = it2 - mGenerators.begin();

	Dependency dep = { idx1, idx2 };

	mDependencies.push_back(dep);
}
