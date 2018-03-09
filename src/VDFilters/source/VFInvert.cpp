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
#include <vd2/plugin/vdvideoaccel.h>

#include "VFInvert.inl"

///////////////////////////////////

namespace {
	void VDInvertRect32(uint32 *data, long w, long h, ptrdiff_t pitch) {
		pitch -= 4*w;

		do {
			long wt = w;
			do {
				*data = *data ^ 0x00FFFFFF;
				++data;
			} while(--wt);

			data = (uint32 *)((char *)data + pitch);
		} while(--h);
	}

	void VDInvertRect64(uint32 *data, long w, long h, ptrdiff_t pitch) {
		pitch -= 8*w;

		do {
			long wt = w;
			do {
				data[0] = data[0] ^ 0xFFFFFFFF;
				data[1] = data[1] ^ 0x0000FFFF;
				data+=2;
			} while(--wt);

			data = (uint32 *)((char *)data + pitch);
		} while(--h);
	}
}

///////////////////////////////////////////////////////////////////////////////

class VDVideoFilterInvert : public VDXVideoFilter {
public:
	VDVideoFilterInvert();

	uint32 GetParams();
	void Run();

	void StartAccel(IVDXAContext *vdxa);
	void RunAccel(IVDXAContext *vdxa);
	void StopAccel(IVDXAContext *vdxa);

protected:
	uint32 mAccelFP;
};

VDVideoFilterInvert::VDVideoFilterInvert()
	: mAccelFP(0)
{
}

uint32 VDVideoFilterInvert::GetParams() {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	switch(pxlsrc.format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
			pxldst.pitch = pxlsrc.pitch;
			return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM;

		case nsVDXPixmap::kPixFormat_XRGB64:
			pxldst.pitch = pxlsrc.pitch;
			return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM | FILTERPARAM_NORMALIZE16;

		case nsVDXPixmap::kPixFormat_VDXA_RGB:
		case nsVDXPixmap::kPixFormat_VDXA_YUV:
			return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
	}
}

void VDVideoFilterInvert::Run() {
	const VDXPixmap& src = *fa->src.mpPixmap;
	if (src.format==nsVDXPixmap::kPixFormat_XRGB8888)
		VDInvertRect32((uint32*)src.data, src.w, src.h, src.pitch);
	if (src.format==nsVDXPixmap::kPixFormat_XRGB64)
		VDInvertRect64((uint32*)src.data, src.w, src.h, src.pitch);
}

void VDVideoFilterInvert::StartAccel(IVDXAContext *vdxa) {
	mAccelFP = vdxa->CreateFragmentProgram(kVDXAPF_D3D9ByteCodePS20, kVDFilterInvertPS, sizeof kVDFilterInvertPS);
}

void VDVideoFilterInvert::RunAccel(IVDXAContext *vdxa) {
	vdxa->SetTextureMatrix(0, fa->src.mVDXAHandle, 0, 0, NULL);
	vdxa->SetSampler(0, fa->src.mVDXAHandle, kVDXAFilt_Point);
	vdxa->DrawRect(fa->dst.mVDXAHandle, mAccelFP, NULL);
}

void VDVideoFilterInvert::StopAccel(IVDXAContext *vdxa) {
	if (mAccelFP) {
		vdxa->DestroyObject(mAccelFP);
		mAccelFP = 0;
	}
}

///////////////////////////////////////////////////////////////////////////////

extern const VDXFilterDefinition g_VDVFInvert = VDXVideoFilterDefinition<VDVideoFilterInvert>(
		NULL,
		"invert",
		"Inverts the colors in the image.");

#ifdef _MSC_VER
	#pragma warning(disable: 4505)	// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{48,{flat}}' }'' : unreferenced local function has been removed
#endif
