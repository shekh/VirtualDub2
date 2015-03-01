#ifndef f_VD2_FILTERACCELCONTEXT_H
#define f_VD2_FILTERACCELCONTEXT_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/atomic.h>
#include <vd2/Tessa/Context.h>
#include <vd2/plugin/vdvideoaccel.h>

class VDFilterAccelEngine;

class VDFilterAccelContext : public IVDXAContext {
public:
	VDFilterAccelContext();
	~VDFilterAccelContext();

	int VDXAPIENTRY AddRef();
	int VDXAPIENTRY Release();
	void *VDXAPIENTRY AsInterface(uint32 iid);

	bool Init(VDFilterAccelEngine& eng);
	void Shutdown();

	bool Restore();

	uint32 RegisterRenderTarget(IVDTSurface *surf, uint32 rw, uint32 rh, uint32 bw, uint32 bh);
	uint32 RegisterTexture(IVDTTexture2D *tex, uint32 imageW, uint32 imageH);

	uint32 VDXAPIENTRY CreateTexture2D(uint32 width, uint32 height, uint32 mipCount, VDXAFormat format, bool wrap, const VDXAInitData2D *initData);
	uint32 VDXAPIENTRY CreateRenderTexture(uint32 width, uint32 height, uint32 borderWidth, uint32 borderHeight, VDXAFormat format, bool wrap);
	uint32 VDXAPIENTRY CreateFragmentProgram(VDXAProgramFormat programFormat, const void *data, uint32 length);
	void VDXAPIENTRY DestroyObject(uint32 handle);

	void VDXAPIENTRY GetTextureDesc(uint32 handle, VDXATextureDesc& desc);

	void VDXAPIENTRY SetTextureMatrix(uint32 coordIndex, uint32 textureHandle, float xoffset, float yoffset, const float uvMatrix[12]);
	void VDXAPIENTRY SetTextureMatrixDual(uint32 coordIndex, uint32 textureHandle, float xoffset, float yoffset, float xoffset2, float yoffset2);
	void VDXAPIENTRY SetSampler(uint32 samplerIndex, uint32 textureHandle, VDXAFilterMode filter);
	void VDXAPIENTRY SetFragmentProgramConstF(uint32 startIndex, uint32 count, const float *data);
	void VDXAPIENTRY DrawRect(uint32 renderTargetHandle, uint32 fragmentProgram, const VDXRect *destRect);
	void VDXAPIENTRY FillRects(uint32 renderTargetHandle, uint32 rectCount, const VDXRect *rects, uint32 colorARGB);

protected:
	enum {
		kHTFragmentProgram	= 0x00010000,
		kHTRenderTarget		= 0x00020000,
		kHTTexture			= 0x00030000,
		kHTRenderTexture	= 0x00040000,
		kHTTypeMask			= 0xFFFF0000
	};

	struct HandleEntry {
		uint32	mFullHandle;
		IVDTResource *mpObject;

		uint32	mImageW;
		uint32	mImageH;
		uint32	mSurfaceW;
		uint32	mSurfaceH;
		uint32	mRenderBorderW;
		uint32	mRenderBorderH;
		bool	mbWrap;
	};

	uint32 AllocHandle(IVDTResource *obj, uint32 handleType);
	HandleEntry *AllocHandleEntry(uint32 handleType);

	IVDTResource *DecodeHandle(uint32 handle, uint32 handleType) const;
	const HandleEntry *DecodeHandleEntry(uint32 handle, uint32 handleType) const;

	void ReportLogicError(const char *msg);

	IVDTContext *mpParent;
	VDFilterAccelEngine *mpEngine;

	typedef vdfastvector<HandleEntry> Handles;
	Handles mHandles;
	uint32	mNextFreeHandle;

	bool	mbErrorState;

	float	mUVTransforms[8][12];

	VDAtomicInt	mRefCount;
};

#endif	// f_VD2_FILTERACCELCONTEXT_H
