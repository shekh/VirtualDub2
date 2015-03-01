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
#include <vd2/system/vdalloc.h>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofilt.h>
#include <vd2/plugin/vdvideoaccel.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/Kasumi/pixmap.h>

#include "Blur.h"

#include "VFBlur.inl"

class VDVFilterBlurBase : public VDXVideoFilter {
public:
	uint32 GetParams();
	void End();
	void Run();

protected:
	vdautoptr<VEffect> mpEffect;
};

uint32 VDVFilterBlurBase::GetParams() {
	const VDXPixmapLayout& pxlsrc = *fa->src.mpPixmapLayout;
	VDXPixmapLayout& pxldst = *fa->dst.mpPixmapLayout;

	switch(pxlsrc.format) {
		case nsVDXPixmap::kPixFormat_XRGB8888:
			pxldst.data = pxlsrc.data;
			pxldst.pitch = pxlsrc.pitch;
			return FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM;

		case nsVDXPixmap::kPixFormat_VDXA_RGB:
		case nsVDXPixmap::kPixFormat_VDXA_YUV:
			fa->src.mBorderWidth = 1;
			fa->src.mBorderHeight = 1;
			return FILTERPARAM_SWAP_BUFFERS | FILTERPARAM_SUPPORTS_ALTFORMATS | FILTERPARAM_PURE_TRANSFORM;

		default:
			return FILTERPARAM_NOT_SUPPORTED;
	}
}

void VDVFilterBlurBase::End() {
	mpEffect = NULL;
}

void VDVFilterBlurBase::Run() {
	if (mpEffect)
		mpEffect->run((VDPixmap&)*fa->dst.mpPixmap);
}

///////////////////////////////////////////////////////////////////////////////

class VDVFilterBlur : public VDVFilterBlurBase {
public:
	VDVFilterBlur();

	void Start();

	void StartAccel(IVDXAContext *vdxa);
	void RunAccel(IVDXAContext *vdxa);
	void EndAccel(IVDXAContext *vdxa);

protected:
	uint32 mAccelFP;
};

VDVFilterBlur::VDVFilterBlur()
	: mAccelFP(0)
{
}

void VDVFilterBlur::Start() {
	mpEffect = VCreateEffectBlur((VDPixmapLayout&)*fa->dst.mpPixmapLayout);
	if (!mpEffect)
		ff->ExceptOutOfMemory();
}

void VDVFilterBlur::StartAccel(IVDXAContext *vdxa) {
	mAccelFP = vdxa->CreateFragmentProgram(kVDXAPF_D3D9ByteCodePS20, kVDFilterBlurPS, sizeof kVDFilterBlurPS);
}

void VDVFilterBlur::RunAccel(IVDXAContext *vdxa) {
	uint32 hsrc = fa->src.mVDXAHandle;
	vdxa->SetSampler(0, hsrc, kVDXAFilt_Bilinear);
	vdxa->SetTextureMatrix(0, hsrc, -0.5f, -0.5f, NULL);
	vdxa->SetTextureMatrix(1, hsrc, +0.5f, -0.5f, NULL);
	vdxa->SetTextureMatrix(2, hsrc, -0.5f, +0.5f, NULL);
	vdxa->SetTextureMatrix(3, hsrc, +0.5f, +0.5f, NULL);
	vdxa->DrawRect(fa->dst.mVDXAHandle, mAccelFP, NULL);
}

void VDVFilterBlur::EndAccel(IVDXAContext *vdxa) {
	if (mAccelFP) {
		vdxa->DestroyObject(mAccelFP);
		mAccelFP = 0;
	}
}

///////////////////////////////////////////////////////////////////////////////

class VDVFilterBlurHi : public VDVFilterBlurBase {
public:
	VDVFilterBlurHi();

	void Start();

	void StartAccel(IVDXAContext *vdxa);
	void RunAccel(IVDXAContext *vdxa);
	void EndAccel(IVDXAContext *vdxa);

protected:
	uint32 mAccelFP;
};

VDVFilterBlurHi::VDVFilterBlurHi()
	: mAccelFP(0)
{
}

void VDVFilterBlurHi::Start() {
	mpEffect = VCreateEffectBlurHi((VDPixmapLayout&)*fa->dst.mpPixmapLayout);
	if (!mpEffect)
		ff->ExceptOutOfMemory();
}

void VDVFilterBlurHi::StartAccel(IVDXAContext *vdxa) {
	mAccelFP = vdxa->CreateFragmentProgram(kVDXAPF_D3D9ByteCodePS20, kVDFilterBlurMorePS, sizeof kVDFilterBlurMorePS);
}

void VDVFilterBlurHi::RunAccel(IVDXAContext *vdxa) {
	uint32 hsrc = fa->src.mVDXAHandle;
	vdxa->SetSampler(0, hsrc, kVDXAFilt_Bilinear);

	//	1	4	6	4	1
	//	4	16	24	16	4
	//	6	24	36	24	6
	//	4	16	24	16	4
	//	1	4	6	4	1
	//	/256

	VDXATextureDesc desc;
	vdxa->GetTextureDesc(hsrc, desc);

	const float m[12]={
		(float)desc.mImageWidth * desc.mInvTexWidth,
		0.0f,
		0.0f,
		(float)desc.mImageWidth * desc.mInvTexWidth,

		0.0f,
		(float)desc.mImageHeight * desc.mInvTexHeight,
		(float)desc.mImageHeight * desc.mInvTexHeight,
		0.0f,

		-1.2f * desc.mInvTexWidth,
		-1.2f * desc.mInvTexHeight,
		0.0f,
		0.0f
	};

	vdxa->SetTextureMatrix(0, NULL, 0, 0, m);
	vdxa->SetTextureMatrix(1, hsrc, +1.2f, -1.2f, NULL);
	vdxa->SetTextureMatrix(2, hsrc, -1.2f, +1.2f, NULL);
	vdxa->SetTextureMatrix(3, hsrc, +1.2f, +1.2f, NULL);
	vdxa->SetTextureMatrix(4, hsrc, +1.2f,  0.0f, NULL);
	vdxa->SetTextureMatrix(5, hsrc, -1.2f,  0.0f, NULL);
	vdxa->SetTextureMatrix(6, hsrc,  0.0f, +1.2f, NULL);
	vdxa->SetTextureMatrix(7, hsrc,  0.0f, -1.2f, NULL);
	vdxa->DrawRect(fa->dst.mVDXAHandle, mAccelFP, NULL);
}

void VDVFilterBlurHi::EndAccel(IVDXAContext *vdxa) {
	if (mAccelFP) {
		vdxa->DestroyObject(mAccelFP);
		mAccelFP = 0;
	}
}

///////////////////////////////////////////////////////////////////////////////

extern const VDXFilterDefinition g_VDVFBlur = VDXVideoFilterDefinition<VDVFilterBlur>(
		NULL,
		"blur",
		"Applies a radius-1 gaussian blur to the image.");

extern const VDXFilterDefinition g_VDVFBlurHi = VDXVideoFilterDefinition<VDVFilterBlurHi>(
		NULL,
		"blur more",
		"Applies a radius-2 gaussian blur to the image.");

// warning C4505: 'VDXVideoFilter::[thunk]: __thiscall VDXVideoFilter::`vcall'{24,{flat}}' }'' : unreferenced local function has been removed
#pragma warning(disable: 4505)
