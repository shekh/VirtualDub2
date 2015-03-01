#include "stdafx.h"
#include <vd2/system/math.h>
#include <vd2/system/bitmath.h>
#include "FilterAccelContext.h"
#include "FilterAccelEngine.h"

VDFilterAccelContext::VDFilterAccelContext()
	: mpParent(NULL)
	, mNextFreeHandle(0)
	, mbErrorState(false)
	, mRefCount(0)
{
}

VDFilterAccelContext::~VDFilterAccelContext() {
	Shutdown();
}

int VDFilterAccelContext::AddRef() {
	return ++mRefCount;
}

int VDFilterAccelContext::Release() {
	int rc = --mRefCount;

	if (!rc)
		delete this;

	return 0;
}

void *VDXAPIENTRY VDFilterAccelContext::AsInterface(uint32 iid) {
	return NULL;
}

bool VDFilterAccelContext::Init(VDFilterAccelEngine& eng) {
	mpEngine = &eng;
	mpParent = eng.GetContext();

	float *xf = &mUVTransforms[0][0];
	for(int i=0; i<8; ++i) {
		xf[0] = 1.0f;
		xf[1] = 0.0f;
		xf[2] = 0.0f;
		xf[3] = 0.0f;
		xf[4] = 1.0f;
		xf[5] = 0.0f;
		xf += 6;
	}

	return true;
}

void VDFilterAccelContext::Shutdown() {
	uint32 n = (uint32)mHandles.size();
	for(uint32 i=0; i<n; ++i) {
		const HandleEntry& ent = mHandles[i];

		if (ent.mpObject)
			DestroyObject(ent.mFullHandle);
	}
}

bool VDFilterAccelContext::Restore() {
	for(Handles::const_iterator it(mHandles.begin()), itEnd(mHandles.end()); it != itEnd; ++it) {
		const HandleEntry& ent = *it;

		if (ent.mpObject) {
			if (!ent.mpObject->Restore())
				return false;
		}
	}

	return true;
}

uint32 VDFilterAccelContext::RegisterRenderTarget(IVDTSurface *surf, uint32 rw, uint32 rh, uint32 bw, uint32 bh) {
	if (mbErrorState)
		return 0;

	HandleEntry *ent = AllocHandleEntry(kHTRenderTarget);
	if (!ent)
		return 0;

	ent->mpObject = surf;
	surf->AddRef();

	VDTSurfaceDesc desc;
	surf->GetDesc(desc);

	ent->mSurfaceW = desc.mWidth;
	ent->mSurfaceH = desc.mHeight;
	ent->mImageW = rw;
	ent->mImageH = rh;
	ent->mRenderBorderW = bw + 1;		// Extra pixel to accommodate clamp to 0/1.
	ent->mRenderBorderH = bh + 1;
	ent->mbWrap = false;

	return ent->mFullHandle;
}

uint32 VDFilterAccelContext::RegisterTexture(IVDTTexture2D *tex, uint32 imageW, uint32 imageH) {
	if (mbErrorState)
		return 0;

	HandleEntry *ent = AllocHandleEntry(kHTTexture);
	if (!ent)
		return 0;

	ent->mpObject = tex;
	tex->AddRef();

	VDTTextureDesc texDesc;
	tex->GetDesc(texDesc);

	ent->mImageW = imageW;
	ent->mImageH = imageH;
	ent->mSurfaceW = texDesc.mWidth;
	ent->mSurfaceH = texDesc.mHeight;
	ent->mRenderBorderW = 0;
	ent->mRenderBorderH = 0;
	ent->mbWrap = false;

	return ent->mFullHandle;
}

uint32 VDFilterAccelContext::CreateTexture2D(uint32 width, uint32 height, uint32 mipCount, VDXAFormat format, bool wrap, const VDXAInitData2D *initData) {
	if (mbErrorState)
		return 0;

	if (!width || !height) {
		ReportLogicError("Invalid width or height passed to CreateTexture2D().");
		return 0;
	}

	if (format != kVDXAF_A8R8G8B8)
		return 0;

	uint32 texWidth = VDCeilToPow2(width);
	uint32 texHeight = VDCeilToPow2(height);

	vdrefptr<IVDTTexture2D> tex;
	if (!mpParent->CreateTexture2D(texWidth, texHeight, kVDTF_B8G8R8A8, mipCount, kVDTUsage_Default, NULL, ~tex))
		return 0;

	if (initData) {
		for(uint32 i=0; i<mipCount; ++i) {
			VDTLockData2D lockData;
			if (!tex->Lock(i, NULL, lockData))
				return 0;

			const uint32 srcw = std::max<uint32>(1, width >> i);
			const uint32 srch = std::max<uint32>(1, height >> i);
			const uint32 dstw = std::max<uint32>(1, texWidth >> i);
			const uint32 dsth = std::max<uint32>(1, texHeight >> i);

			const char *src = (const char *)initData[i].mpData;
			ptrdiff_t srcPitch = initData[i].mPitch;

			char *dst = (char *)lockData.mpData;

			for(uint32 y=0; y<srch; ++y) {
				memcpy(dst, src, 4*srcw);

				if (srcw < dstw) {
					uint32 *dstr = (uint32 *)(dst + 4*srcw);
					const uint32 fill = *(uint32 *)(src + 4*(srcw - 1));
					for(uint32 x = srcw; x < dstw; ++x)
						dstr[x] = fill;
				}

				src += srcPitch;
				dst += lockData.mPitch;
			}

			if (srch < dsth)
				VDMemcpyRect(dst, lockData.mPitch, dst - lockData.mPitch, lockData.mPitch, dstw*4, dsth - srch);

			tex->Unlock(i);
		}
	}

	HandleEntry *ent = AllocHandleEntry(kHTTexture);
	if (!ent)
		return 0;

	ent->mpObject = tex.release();
	ent->mSurfaceW = texWidth;
	ent->mSurfaceH = texHeight;
	ent->mImageW = width;
	ent->mImageH = height;
	ent->mRenderBorderW = 0;
	ent->mRenderBorderH = 0;
	ent->mbWrap = wrap;

	return ent->mFullHandle;
}

uint32 VDFilterAccelContext::CreateRenderTexture(uint32 width, uint32 height, uint32 borderWidth, uint32 borderHeight, VDXAFormat format, bool wrap) {
	if (mbErrorState)
		return 0;

	if (!width || !height) {
		ReportLogicError("Invalid width or height passed to CreateRenderTexture().");
		return 0;
	}

	if (wrap) {
		if ((width & (width - 1)) || (height & (height - 1))) {
			ReportLogicError("Wrapped textures must be powers of two in width and height.");
			return 0;
		}
	}

	if (format != kVDXAF_Unknown && format != kVDXAF_A8R8G8B8)
		return 0;

	uint32 texWidth = VDCeilToPow2(width);
	uint32 texHeight = VDCeilToPow2(height);

	vdrefptr<IVDTTexture2D> tex;
	if (!mpParent->CreateTexture2D(texWidth, texHeight, kVDTF_B8G8R8A8, 1, kVDTUsage_Render, NULL, ~tex))
		return 0;

	HandleEntry *ent = AllocHandleEntry(kHTRenderTexture);
	if (!ent)
		return 0;

	ent->mpObject = tex.release();
	ent->mSurfaceW = texWidth;
	ent->mSurfaceH = texHeight;
	ent->mImageW = width;
	ent->mImageH = height;
	ent->mRenderBorderW = borderWidth + 1;		// Extra pixel to accommodate clamp to 0/1.
	ent->mRenderBorderH = borderHeight + 1;
	ent->mbWrap = wrap;

	return ent->mFullHandle;
}

uint32 VDFilterAccelContext::CreateFragmentProgram(VDXAProgramFormat programFormat, const void *data, uint32 length) {
	if (mbErrorState)
		return 0;

	if (programFormat != kVDXAPF_D3D9ByteCodePS20)
		return 0;

	if (length < 4 || (length & 3)) {
		ReportLogicError("Invalid fragment program length.");
		return 0;
	}

	const uint32 versionKey = *(const uint32 *)data;

	if (versionKey != 0xFFFF0200) {
		ReportLogicError("Fragment program must use the ps_2_0 profile.");
		return 0;
	}

	vdrefptr<IVDTFragmentProgram> fp;
	if (!mpParent->CreateFragmentProgram(kVDTPF_D3D9ByteCode, VDTDataView(data, length), ~fp))
		return 0;

	uint32 h = AllocHandle(fp, kHTFragmentProgram);
	if (h)
		fp.release();

	return h;
}

void VDFilterAccelContext::DestroyObject(uint32 handle) {
	if (!handle)
		return;

	if (handle < 0x10000) {
		ReportLogicError("Invalid handle passed to DestroyObject().");
		return;
	}

	uint32 hidx = handle & 0xffff;
	if (hidx > mHandles.size()) {
		ReportLogicError("Invalid handle passed to DestroyObject().");
		return;
	}

	HandleEntry& ent = mHandles[hidx - 1];
	if (ent.mFullHandle != handle) {
		ReportLogicError("Invalid handle passed to DestroyObject().");
		return;
	}

	ent.mpObject->Release();
	ent.mpObject = NULL;

	ent.mFullHandle = mNextFreeHandle;
	mNextFreeHandle = hidx;
}

void VDFilterAccelContext::GetTextureDesc(uint32 handle, VDXATextureDesc& desc) {
	desc.mImageWidth = 1;
	desc.mImageHeight = 1;
	desc.mTexWidth = 1;
	desc.mTexHeight = 1;
	desc.mInvTexWidth = 1.0f;
	desc.mInvTexHeight = 1.0f;

	if (!handle)
		return;

	const HandleEntry *texent = DecodeHandleEntry(handle, kHTTexture);

	if (!texent)
		texent = DecodeHandleEntry(handle, kHTRenderTexture);

	if (!texent) {
		ReportLogicError("Invalid texture handle passed to GetTextureSizeConstant().");
		return;
	}

	desc.mImageWidth = texent->mImageW;
	desc.mImageHeight = texent->mImageH;
	desc.mTexWidth = texent->mSurfaceW;
	desc.mTexHeight = texent->mSurfaceH;
	desc.mInvTexWidth = 1.0f / (float)texent->mSurfaceW;
	desc.mInvTexHeight = 1.0f / (float)texent->mSurfaceH;
}

void VDFilterAccelContext::SetTextureMatrix(uint32 coordIndex, uint32 textureHandle, float xoffset, float yoffset, const float uvMatrix[12]) {
	if (coordIndex >= 8) {
		ReportLogicError("Invalid texture coordinate index passed to SetTextureMatrix().");
		return;
	}

	const HandleEntry *texent = NULL;
	float *xf = mUVTransforms[coordIndex];

	if (textureHandle) {
		if (uvMatrix) {
			ReportLogicError("Cannot pass both texture handle and matrix to SetTextureMatrix().");
			return;
		}

		texent = DecodeHandleEntry(textureHandle, kHTTexture);

		if (!texent)
			texent = DecodeHandleEntry(textureHandle, kHTRenderTexture);

		if (!texent) {
			ReportLogicError("Invalid texture handle passed to SetTextureMatrix().");
			return;
		}

		xf[0] = (float)texent->mImageW / (float)texent->mSurfaceW;
		xf[1] = 0.0f;
		xf[2] = 0.0f;
		xf[3] = 0.0f;
		xf[4] = 0.0f;
		xf[5] = (float)texent->mImageH / (float)texent->mSurfaceH;
		xf[6] = 0.0f;
		xf[7] = 0.0f;
		xf[8] = xoffset / (float)texent->mSurfaceW;
		xf[9] = yoffset / (float)texent->mSurfaceH;
		xf[10] = 0.0f;
		xf[11] = 0.0f;
		return;
	}

	if (uvMatrix) {
		if (!VDVerifyFiniteFloats(uvMatrix, 12)) {
			ReportLogicError("Invalid constant data passed to SetTextureMatrix().");
			return;
		}
	} else {
		static const float kIdent[]={
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f
		};

		uvMatrix = kIdent;
	}

	memcpy(xf, uvMatrix, sizeof(float) * 12);
}

void VDFilterAccelContext::SetTextureMatrixDual(uint32 coordIndex, uint32 textureHandle, float xoffset, float yoffset, float xoffset2, float yoffset2) {
	if (coordIndex >= 8) {
		ReportLogicError("Invalid texture coordinate index passed to SetTextureMatrixDual().");
		return;
	}

	const HandleEntry *texent = NULL;
	float *xf = mUVTransforms[coordIndex];

	texent = DecodeHandleEntry(textureHandle, kHTTexture);

	if (!texent)
		texent = DecodeHandleEntry(textureHandle, kHTRenderTexture);

	if (!texent) {
		ReportLogicError("Invalid texture handle passed to SetTextureMatrix().");
		return;
	}

	xf[0] = (float)texent->mImageW / (float)texent->mSurfaceW;
	xf[1] = 0.0f;
	xf[2] = 0.0f;
	xf[3] = (float)texent->mImageW / (float)texent->mSurfaceW;
	xf[4] = 0.0f;
	xf[5] = (float)texent->mImageH / (float)texent->mSurfaceH;
	xf[6] = (float)texent->mImageH / (float)texent->mSurfaceH;
	xf[7] = 0.0f;
	xf[8] = xoffset / (float)texent->mSurfaceW;
	xf[9] = yoffset / (float)texent->mSurfaceH;
	xf[10] = yoffset2 / (float)texent->mSurfaceH;
	xf[11] = xoffset2 / (float)texent->mSurfaceW;
}

void VDFilterAccelContext::SetSampler(uint32 samplerIndex, uint32 textureHandle, VDXAFilterMode filter) {
	if (samplerIndex >= 16) {
		ReportLogicError("Invalid sampler index passed to SetSampler().");
		return;
	}

	bool wrap = false;
	IVDTTexture *tex = NULL;
	if (textureHandle) {
		const HandleEntry *texent = DecodeHandleEntry(textureHandle, kHTTexture);
		const HandleEntry *rtent = DecodeHandleEntry(textureHandle, kHTRenderTexture);

		if (texent) {
			tex = static_cast<IVDTTexture *>(texent->mpObject);
			wrap = texent->mbWrap;
		} else if (rtent) {
			tex = static_cast<IVDTTexture *>(rtent->mpObject);
			wrap = rtent->mbWrap;
		} else {
			ReportLogicError("Invalid texture handle passed to SetSampler().");
			return;
		}

	}

	mpParent->SetTextures(samplerIndex, 1, &tex);
	mpEngine->SetSamplerState(samplerIndex, wrap, filter);
}

void VDFilterAccelContext::SetFragmentProgramConstF(uint32 startIndex, uint32 count, const float *data) {
	if (!data) {
		ReportLogicError("Constant pointer cannot be NULL.");
		return;
	}

	if (!VDVerifyFiniteFloats(data, 4*count)) {
		ReportLogicError("Invalid constant data passed to SetFragmentProgramConstF().");
		return;
	}

	mpParent->SetFragmentProgramConstF(startIndex, count, data);
}

void VDFilterAccelContext::DrawRect(uint32 renderTargetHandle, uint32 fragmentProgram, const VDXRect *destRect) {
	IVDTFragmentProgram *fp = static_cast<IVDTFragmentProgram *>(DecodeHandle(fragmentProgram, kHTFragmentProgram));
	if (!fp) {
		ReportLogicError("Invalid fragment program handle.");
		return;
	}

	const HandleEntry *sent = DecodeHandleEntry(renderTargetHandle, kHTRenderTarget);
	IVDTSurface *s;
	if (sent) {
		s = static_cast<IVDTSurface *>(sent->mpObject);
	} else {
		sent = DecodeHandleEntry(renderTargetHandle, kHTRenderTexture);
		if (sent) {
			IVDTTexture2D *tex = vdpoly_cast<IVDTTexture2D *>(sent->mpObject);
			if (!tex) {
				ReportLogicError("Render target textures must be 2D textures.");
				return;
			}

			s = tex->GetLevelSurface(0);
		} else {
			ReportLogicError("Invalid render target handle.");
			return;
		}
	}

	VDXRect wholeRect;
	float u0 = 0;
	float v0 = 0;
	float u1 = 1;
	float v1 = 1;

	if (!destRect) {
		wholeRect.left = 0;
		wholeRect.top = 0;
		wholeRect.right = sent->mImageW;
		wholeRect.bottom = sent->mImageH;
		destRect = &wholeRect;
	} else {
		wholeRect = *destRect;

		destRect = &wholeRect;
		if (wholeRect.right <= wholeRect.left)
			return;

		if (wholeRect.bottom <= wholeRect.top)
			return;

		uint32 dx = wholeRect.right - wholeRect.left;
		uint32 dy = wholeRect.bottom - wholeRect.top;
		float idx = 1.0f / (float)dx;
		float idy = 1.0f / (float)dy;

		if (wholeRect.left < 0) {
			u0 = idx * -(float)wholeRect.left;
			wholeRect.left = 0;

			if (wholeRect.right <= 0)
				return;
		}

		if (wholeRect.top < 0) {
			v0 = idy * -(float)wholeRect.top;
			wholeRect.top = 0;

			if (wholeRect.bottom <= 0)
				return;
		}

		if (wholeRect.right > (int)sent->mImageW) {
			u1 = 1.0f - idx*(wholeRect.right - (int)sent->mImageW);
			wholeRect.right = (int)sent->mImageW;
		}

		if (wholeRect.bottom > (int)sent->mImageH) {
			v1 = 1.0f - idy*(wholeRect.bottom - (int)sent->mImageH);
			wholeRect.bottom = (int)sent->mImageH;
		}
	}

	VDTSurfaceDesc rtdesc;
	s->GetDesc(rtdesc);
	float invW = 1.0f / (float)rtdesc.mWidth;
	float invH = 1.0f / (float)rtdesc.mHeight;

	float posx[3]={
		(float)wholeRect.left,
		(float)wholeRect.right,
		(float)(wholeRect.right >= sent->mImageW ? sent->mImageW + sent->mRenderBorderW : wholeRect.right)
	};

	float posy[3]={
		(float)wholeRect.top,
		(float)wholeRect.bottom,
		(float)(wholeRect.bottom >= sent->mImageH ? sent->mImageH + sent->mRenderBorderH : wholeRect.bottom)
	};

	VDFilterAccelVertex vxs[9];
	for(int i=0; i<9; ++i) {
		VDFilterAccelVertex& vx = vxs[i];
		int x = i % 3;
		int y = i / 3;
		float u = x < 1 ? u0 : u1;
		float v = y < 1 ? v0 : v1;

		vx.x = (posx[x] * 2.0f) * invW - 1.0f;
		vx.y = 1.0f - (posy[y] * 2.0f) * invH;
		vx.z = 0.5f;

		for(int j=0; j<8; ++j) {
			float *uv = vx.uv[j];
			const float *xf = mUVTransforms[j];

			uv[0] = u*xf[0] + v*xf[4] + xf[8];
			uv[1] = u*xf[1] + v*xf[5] + xf[9];
			uv[2] = u*xf[2] + v*xf[6] + xf[10];
			uv[3] = u*xf[3] + v*xf[7] + xf[11];
		}
	}


	mpParent->SetRenderTarget(0, s);

	const VDTViewport vp = { 0, 0, rtdesc.mWidth, rtdesc.mHeight, 0.0f, 1.0f };
	mpParent->SetViewport(vp);

	mpEngine->DrawQuads(NULL, fp, vxs, 9, sizeof vxs[0], VDFilterAccelEngine::kQuadPattern_3x3);
}

void VDFilterAccelContext::FillRects(uint32 renderTargetHandle, uint32 rectCount, const VDXRect *rects, uint32 colorARGB) {
	const HandleEntry *sent = DecodeHandleEntry(renderTargetHandle, kHTRenderTarget);
	IVDTSurface *s;
	if (sent) {
		s = static_cast<IVDTSurface *>(sent->mpObject);
	} else {
		sent = DecodeHandleEntry(renderTargetHandle, kHTRenderTexture);
		if (sent) {
			IVDTTexture2D *tex = vdpoly_cast<IVDTTexture2D *>(sent->mpObject);
			if (!tex) {
				ReportLogicError("Render target textures must be 2D textures.");
				return;
			}

			s = tex->GetLevelSurface(0);
		} else {
			ReportLogicError("Invalid render target handle.");
			return;
		}
	}

	mpParent->SetRenderTarget(0, s);

	VDTSurfaceDesc desc;
	s->GetDesc(desc);
	const VDTViewport vp = { 0, 0, desc.mWidth, desc.mHeight, 0.0f, 1.0f };
	mpParent->SetViewport(vp);

	mpEngine->FillRects(rectCount, rects, colorARGB, sent->mImageW, sent->mImageH, sent->mRenderBorderW, sent->mRenderBorderH);
}

uint32 VDFilterAccelContext::AllocHandle(IVDTResource *obj, uint32 handleType) {
	HandleEntry *ent = AllocHandleEntry(handleType);
	if (!ent)
		return 0;

	ent->mpObject = obj;
	return ent->mFullHandle;
}

VDFilterAccelContext::HandleEntry *VDFilterAccelContext::AllocHandleEntry(uint32 handleType) {
	uint32 hidx = mNextFreeHandle;

	if (!hidx) {
		hidx = (uint32)mHandles.size() + 1;
		if (hidx >= 0x10000)
			return NULL;

		HandleEntry dummy = {0, NULL};
		mHandles.push_back(dummy);
	}

	HandleEntry& ent = mHandles[hidx - 1];
	mNextFreeHandle = ent.mFullHandle;
	ent.mFullHandle = handleType + hidx;

	return &ent;
}

IVDTResource *VDFilterAccelContext::DecodeHandle(uint32 handle, uint32 handleType) const {
	const HandleEntry *ent = DecodeHandleEntry(handle, handleType);
	if (!ent)
		return NULL;

	return ent->mpObject;
}

const VDFilterAccelContext::HandleEntry *VDFilterAccelContext::DecodeHandleEntry(uint32 handle, uint32 handleType) const {
	if ((handle & kHTTypeMask) != handleType)
		return NULL;

	uint32 hidx = handle & 0xffff;
	if (!hidx)
		return NULL;

	if (hidx > (uint32)mHandles.size())
		return NULL;

	const HandleEntry& ent = mHandles[hidx - 1];
	return &ent;
}

void VDFilterAccelContext::ReportLogicError(const char *msg) {
	mbErrorState = true;
}
