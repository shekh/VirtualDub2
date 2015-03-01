//	VirtualDub - Video processing and capture application
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

#ifndef f_VD2_PLUGIN_VDVIDEOACCEL_H
#define f_VD2_PLUGIN_VDVIDEOACCEL_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <stddef.h>

#include "vdplugin.h"

struct VDXAInitData2D {
	const void *mpData;
	ptrdiff_t mPitch;
};

struct VDXATextureDesc {
	uint32	mImageWidth;
	uint32	mImageHeight;
	uint32	mTexWidth;
	uint32	mTexHeight;
	float	mInvTexWidth;
	float	mInvTexHeight;
};

enum VDXAFormat {
	kVDXAF_Unknown,
	kVDXAF_A8R8G8B8
};

enum VDXAProgramFormat {
	kVDXAPF_D3D9ByteCodePS20
};

enum VDXAFilterMode {
	kVDXAFilt_Point,
	kVDXAFilt_Bilinear,
	kVDXAFilt_BilinearMip,
	kVDXAFiltCount
};

class IVDXAContext : public IVDXUnknown {
public:
	// Create a texture.
	virtual uint32 VDXAPIENTRY CreateTexture2D(uint32 width, uint32 height, uint32 mipCount, VDXAFormat format, bool wrap, const VDXAInitData2D *initData) = 0;

	// Create a fragment program.
	virtual uint32 VDXAPIENTRY CreateFragmentProgram(VDXAProgramFormat programFormat, const void *data, uint32 length) = 0;

	virtual uint32 VDXAPIENTRY CreateRenderTexture(uint32 width, uint32 height, uint32 borderWidth, uint32 borderHeight, VDXAFormat format, bool wrap) = 0;

	// Destroy an object. If the object is bound, it is automatically unbound before being destroyed.
	virtual void VDXAPIENTRY DestroyObject(uint32 handle) = 0;

	// Query the description of a texture.
	virtual void VDXAPIENTRY GetTextureDesc(uint32 handle, VDXATextureDesc& desc) = 0;

	// Set one or more fragment program constants with an array of 4-vectors.
	virtual void VDXAPIENTRY SetFragmentProgramConstF(uint32 startIndex, uint32 count, const float *values) = 0;

	// Set a 2x3 transform matrix used by a texture coordinate interpolator to target one source rectangle.
	virtual void VDXAPIENTRY SetTextureMatrix(uint32 coordIndex, uint32 textureHandle, float xoffset, float yoffset, const float uvMatrix[12]) = 0;

	// Set a 4x3 transform matrix used by a texture coordinate interpolator to target two source rectangles.
	virtual void VDXAPIENTRY SetTextureMatrixDual(uint32 coordIndex, uint32 textureHandle, float xoffset1, float yoffset1, float xoffset2, float yoffset2) = 0;

	// A call of SetSampler(samplerIndex, 0, false, NULL) will disable a sampler. However, there is no
	// need to explicitly disable samplers, as the runtime will automatically do this whenever a bound
	// texture is destroyed, and ignore samplers not used by a fragment program.
	virtual void VDXAPIENTRY SetSampler(uint32 samplerIndex, uint32 textureHandle, VDXAFilterMode filterMode) = 0;

	// Draw a quad using the given fragment program. The rectangle size is determined by the natural size
	// of the render target. The optional destination rect is cropped against the render target if
	// required.
	virtual void VDXAPIENTRY DrawRect(uint32 renderTargetHandle, uint32 fragmentProgram, const VDXRect *destRect) = 0;

	// Fill a series of rectangles with a solid color. The rectangles are cropped to the destination as
	// necessary. If rectCount > 0 and rects != null, the entire destination is filled.
	virtual void VDXAPIENTRY FillRects(uint32 renderTargetHandle, uint32 rectCount, const VDXRect *rects, uint32 colorARGB) = 0;
};

#endif
