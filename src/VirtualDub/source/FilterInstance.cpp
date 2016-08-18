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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

#include "stdafx.h"
#include <vd2/system/debug.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/int128.h>
#include <vd2/system/linearalloc.h>
#include <vd2/system/protscope.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "ScriptInterpreter.h"
#include "FilterFrame.h"
#include "FilterFrameAllocatorManager.h"
#include "FilterFrameBufferAccel.h"
#include "FilterFrameRequest.h"
#include "FilterAccelContext.h"
#include "FilterAccelEngine.h"
#include "FilterInstance.h"
#include "filters.h"
#include "project.h"

extern FilterFunctions g_VDFilterCallbacks;
extern VDProject *g_project;

/////////////////////////////////////

namespace {
	static const uint32 kFrameAxisLimit = 1048576;			// 1Mpx
	static const uint32 kFrameSizeLimit = 0x08000000;		// 128MB

	void StructCheck() {
		VDASSERTCT(sizeof(VDPixmap)-sizeof(FilterModPixmapInfo) == sizeof(VDXPixmap));
		//VDASSERTCT(sizeof(VDPixmapLayout)-sizeof(VDPixmapFormatEx) == sizeof(VDXPixmapLayout));
		VDASSERTCT(offsetof(VDPixmapLayout, pitch3)+sizeof(((VDPixmapLayout*)0)->pitch3) == sizeof(VDXPixmapLayout));
				
		VDASSERTCT(offsetof(VDPixmap, data) == offsetof(VDXPixmap, data));
		VDASSERTCT(offsetof(VDPixmap, pitch) == offsetof(VDXPixmap, pitch));
		VDASSERTCT(offsetof(VDPixmap, format) == offsetof(VDXPixmap, format));
		VDASSERTCT(offsetof(VDPixmap, w) == offsetof(VDXPixmap, w));
		VDASSERTCT(offsetof(VDPixmap, h) == offsetof(VDXPixmap, h));
				
		VDASSERTCT(offsetof(VDPixmapLayout, data) == offsetof(VDXPixmapLayout, data));
		VDASSERTCT(offsetof(VDPixmapLayout, pitch) == offsetof(VDXPixmapLayout, pitch));
		VDASSERTCT(offsetof(VDPixmapLayout, format) == offsetof(VDXPixmapLayout, format));
		VDASSERTCT(offsetof(VDPixmapLayout, w) == offsetof(VDXPixmapLayout, w));
		VDASSERTCT(offsetof(VDPixmapLayout, h) == offsetof(VDXPixmapLayout, h));
	}

	bool VDIsVDXAFormat(int format) {
		return format == nsVDXPixmap::kPixFormat_VDXA_RGB || format == nsVDXPixmap::kPixFormat_VDXA_YUV;
	}

	bool VDPixmapIsLayoutVectorAligned(const VDPixmapLayout& layout) {
		const int bufcnt = VDPixmapGetInfo(layout.format).auxbufs;

		switch(bufcnt) {
		case 2:
			if ((layout.data3 | layout.pitch3) & 15)
				return false;
			break;
		case 1:
			if ((layout.data2 | layout.pitch2) & 15)
				return false;
			break;
		case 0:
			if ((layout.data | layout.pitch) & 15)
				return false;
			break;
		}

		return true;
	}
}

///////////////////////////////////////////////////////////////////////////

VFBitmapInternal::VFBitmapInternal()
	: VBitmap()
	, dwFlags(0)
	, hdc(NULL)
	, mFrameRateHi(1)
	, mFrameRateLo(0)
	, mFrameCount(0)
	, mpPixmap(reinterpret_cast<VDXPixmap *>(&mPixmap))
	, mpPixmapLayout(reinterpret_cast<VDXPixmapLayout *>(&mPixmapLayout))
	, mAspectRatioHi(0)
	, mAspectRatioLo(0)
	, mFrameNumber(0)
	, mFrameTimestampStart(0)
	, mFrameTimestampEnd(0)
	, mCookie(0)
	, mpBuffer(NULL)
	, mVDXAHandle(0)
	, mBorderWidth(0)
	, mBorderHeight(0)
{
	this->data = NULL;
	this->pitch = 0;
	this->w = 0;
	this->h = 0;
	this->palette = NULL;
	this->depth = 0;
	this->modulo = 0;
	this->size = 0;
	this->offset = 0;

	memset(&mPixmap, 0, sizeof mPixmap);
	memset(&mPixmapLayout, 0, sizeof mPixmapLayout);
}

VFBitmapInternal::VFBitmapInternal(const VFBitmapInternal& src)
	: VBitmap(static_cast<const VBitmap&>(src))
	, dwFlags(src.dwFlags)
	, hdc(src.hdc)
	, mFrameRateHi(src.mFrameRateHi)
	, mFrameRateLo(src.mFrameRateLo)
	, mFrameCount(src.mFrameCount)
	, mpPixmap(reinterpret_cast<VDXPixmap *>(&mPixmap))
	, mpPixmapLayout(reinterpret_cast<VDXPixmapLayout *>(&mPixmapLayout))
	, mPixmap(src.mPixmap)
	, mPixmapLayout(src.mPixmapLayout)
	, mAspectRatioHi(src.mAspectRatioHi)
	, mAspectRatioLo(src.mAspectRatioLo)
	, mFrameNumber(src.mFrameNumber)
	, mFrameTimestampStart(src.mFrameTimestampStart)
	, mFrameTimestampEnd(src.mFrameTimestampEnd)
	, mCookie(src.mCookie)
	, mpBuffer(src.mpBuffer)
	, mVDXAHandle(src.mVDXAHandle)
	, mBorderWidth(src.mBorderWidth)
	, mBorderHeight(src.mBorderHeight)
{
	if (mpBuffer)
		mpBuffer->AddRef();
}

VFBitmapInternal::~VFBitmapInternal()
{
	Unbind();
}

VFBitmapInternal& VFBitmapInternal::operator=(const VFBitmapInternal& src) {
	if (this != &src) {
		static_cast<VBitmap&>(*this) = static_cast<const VBitmap&>(src);
		dwFlags			= src.dwFlags;
		hdc				= NULL;
		mFrameRateHi	= src.mFrameRateHi;
		mFrameRateLo	= src.mFrameRateLo;
		mFrameCount		= src.mFrameCount;
		mPixmap			= src.mPixmap;
		mPixmapLayout	= src.mPixmapLayout;
		mAspectRatioHi	= src.mAspectRatioHi;
		mAspectRatioLo	= src.mAspectRatioLo;
		mFrameNumber	= src.mFrameNumber;
		mFrameTimestampStart	= src.mFrameTimestampStart;
		mFrameTimestampEnd		= src.mFrameTimestampEnd;
		mCookie			= src.mCookie;
		mBorderWidth	= src.mBorderWidth;
		mBorderHeight	= src.mBorderHeight;

		mDIBSection.Shutdown();

		if (mpBuffer)
			mpBuffer->Release();

		mpBuffer = src.mpBuffer;

		if (mpBuffer)
			mpBuffer->AddRef();
	}

	return *this;
}

void VFBitmapInternal::Unbind() {
	VDASSERT(!mVDXAHandle);

	data = NULL;
	mPixmap.data = NULL;
	mPixmap.data2 = NULL;
	mPixmap.data3 = NULL;

	if (mpBuffer) {
		mpBuffer->Unlock();
		mpBuffer->Release();
		mpBuffer = NULL;
	}
}

void VFBitmapInternal::Fixup(void *base) {
	VDPixmap px = VDPixmapFromLayout(mPixmapLayout, base);
	px.info = mPixmap.info;
	mPixmap = px;
	data = (uint32 *)((pitch < 0 ? (char *)base - pitch*(h-1) : (char *)base) + offset);

	VDAssertValidPixmap(mPixmap);
}

void VFBitmapInternal::ConvertBitmapLayoutToPixmapLayout() {
	mPixmapLayout.w			= w;
	mPixmapLayout.h			= h;
	mPixmapLayout.format	= nsVDPixmap::kPixFormat_XRGB8888;
	mPixmapLayout.palette	= NULL;
	mPixmapLayout.pitch		= -pitch;
	mPixmapLayout.data		= pitch<0 ? offset : offset + pitch*(h - 1);
	mPixmapLayout.data2		= 0;
	mPixmapLayout.pitch2	= 0;
	mPixmapLayout.data3		= 0;
	mPixmapLayout.pitch3	= 0;
}

void VFBitmapInternal::ConvertPixmapLayoutToBitmapLayout() {
	w = mPixmapLayout.w;
	h = mPixmapLayout.h;
	palette = NULL;

	if (VDIsVDXAFormat(mPixmapLayout.format)) {
		depth = 0;
		pitch = 0;
		offset = 0;
		modulo = 0;
		size = 0;
		return;
	}

	VDASSERT(mPixmapLayout.pitch >= 0 || (sint64)mPixmapLayout.pitch*(mPixmapLayout.h - 1) <= mPixmapLayout.data);

	const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(mPixmapLayout.format);

	switch(mPixmapLayout.format) {
		case nsVDPixmap::kPixFormat_XRGB8888:
			depth = 32;
			break;

		default:
			depth = 0;
			break;
	}

	pitch	= -mPixmapLayout.pitch;
	offset	= mPixmapLayout.pitch < 0 ? mPixmapLayout.data + mPixmapLayout.pitch*(h - 1) : mPixmapLayout.data;

	uint32 bpr = formatInfo.qsize * ((w + formatInfo.qw - 1) / formatInfo.qw);
	modulo	= pitch - bpr;
	size	= VDPixmapLayoutGetMinSize(mPixmapLayout) - offset;

	VDASSERT((sint32)size >= 0);
}

void VFBitmapInternal::ConvertPixmapToBitmap() {
	const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(mPixmapLayout.format);
	w = mPixmap.w;
	h = mPixmap.h;

	switch(mPixmap.format) {
		case nsVDPixmap::kPixFormat_XRGB8888:
			depth = 32;
			break;

		default:
			depth = 0;
			break;
	}

	palette = NULL;
	pitch	= -mPixmap.pitch;
	data	= (Pixel *)((char *)mPixmap.data + mPixmap.pitch*(h - 1));

	uint32 bpr = formatInfo.qsize * ((w + formatInfo.qw - 1) / formatInfo.qw);
	modulo	= pitch - bpr;
}

void VFBitmapInternal::BindToDIBSection(const VDFileMappingW32 *mapping) {
	if (!mDIBSection.Init(VDAbsPtrdiff(pitch) >> 2, h, 32, mapping, offset))
		throw MyMemoryError();

	hdc = (VDXHDC)mDIBSection.GetHDC();

	mPixmap = mDIBSection.GetPixmap();
	mPixmap.w = mPixmapLayout.w;
	mPixmap.h = mPixmapLayout.h;
	ConvertPixmapToBitmap();
}

void VFBitmapInternal::BindToFrameBuffer(VDFilterFrameBuffer *buffer, bool readOnly) {
	if (mpBuffer == buffer)
		return;

	Unbind();

	if (buffer)
		buffer->AddRef();

	mpBuffer = buffer;

	if (buffer) {
		VDFilterFrameBufferAccel *accelbuf = vdpoly_cast<VDFilterFrameBufferAccel *>(buffer);

		if (accelbuf) {
			mPixmap.pitch = 0;
			mPixmap.pitch2 = 0;
			mPixmap.pitch3 = 0;
			mPixmap.data = NULL;
			mPixmap.data2 = NULL;
			mPixmap.data3 = NULL;
			mPixmap.w = accelbuf->GetWidth();
			mPixmap.h = accelbuf->GetHeight();

			data = NULL;
			palette = NULL;
			depth = 0;
			w = mPixmap.w;
			h = mPixmap.h;
			pitch = 0;
			modulo = 0;
			size = 0;
			offset = 0;
		} else {
			VDASSERT(buffer->GetSize() >= VDPixmapLayoutGetMinSize(mPixmapLayout));

			void *p = readOnly ? (void *)buffer->LockRead() : buffer->LockWrite();
			Fixup(p);
		}
	}
}

void VFBitmapInternal::CopyNullBufferParams() {
	mPixmap.w = mPixmapLayout.w;
	mPixmap.h = mPixmapLayout.h;
	mPixmap.format = mPixmapLayout.format;
	w = mPixmapLayout.w;
	h = mPixmapLayout.h;

	switch(mPixmap.format) {
		case nsVDPixmap::kPixFormat_XRGB8888:
			depth = 32;
			break;

		default:
			depth = 0;
			break;
	}
}

void VFBitmapInternal::SetFrameNumber(sint64 frame) {
	mFrameNumber = frame;

	if (mFrameRateLo) {
		const double microSecsPerFrame = (double)mFrameRateLo / (double)mFrameRateHi * 10000000.0;
		mFrameTimestampStart = VDRoundToInt64(microSecsPerFrame * frame);
		mFrameTimestampEnd = VDRoundToInt64(microSecsPerFrame * (frame + 1));
	} else {
		mFrameTimestampStart = 0;
		mFrameTimestampEnd = 0;
	}
}

VDFilterStreamDesc VFBitmapInternal::GetStreamDesc() const {
	VDFilterStreamDesc desc = {};

	desc.mLayout = mPixmapLayout;
	desc.mAspectRatio = VDFraction(mAspectRatioHi, mAspectRatioLo);
	desc.mFrameRate = VDFraction(mFrameRateHi, mFrameRateLo);
	desc.mFrameCount = mFrameCount;

	return desc;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDFilterActivationImpl
//
///////////////////////////////////////////////////////////////////////////

VDFilterActivationImpl::VDFilterActivationImpl()
	: filter		(NULL)
	, filter_data	(NULL)
	, dst			((VDXFBitmap&)mRealDst)
	, src			((VDXFBitmap&)mRealSrc)
	, _reserved0	(NULL)
	, last			((VDXFBitmap *)&mRealLast)
	, x1			(0)
	, y1			(0)
	, x2			(0)
	, y2			(0)
	, pfsi			(NULL)
	, ifp			(NULL)
	, ifp2			(NULL)
	, mSourceFrameCount(0)
	, mpSourceFrames(NULL)
	, mpOutputFrames(mOutputFrameArray)
	, mpVDXA		(NULL)
	, mRealSrc      ()
	, mRealDst      ()
	, mRealLast     ()
{
	VDASSERT(((char *)&mSizeCheckSentinel - (char *)this) == sizeof(VDXFilterActivation));

	mOutputFrameArray[0] = &dst;

	SetSourceStreamCount(1);

	fma_ref = &fma;
	fma.filter = 0;
	fma.filterMod = 0;
	fma.filter_data = 0;
	fma.fmpreview = 0;
}

VDFilterActivationImpl::VDFilterActivationImpl(const VDFilterActivationImpl& src)
	: filter		(NULL)
	, filter_data	(NULL)
	, dst			((VDXFBitmap&)mRealDst)
	, src			((VDXFBitmap&)mRealSrc)
	, _reserved0	(NULL)
	, last			((VDXFBitmap *)&mRealLast)
	, x1			(0)
	, y1			(0)
	, x2			(0)
	, y2			(0)
	, pfsi			(NULL)
	, ifp			(NULL)
	, ifp2			(NULL)
	, mSourceFrameCount(0)
	, mpSourceFrames(NULL)
	, mpOutputFrames(mOutputFrameArray)
	, mpVDXA		(NULL)
	, mRealSrc      (src.mRealSrc)
	, mRealDst      (src.mRealDst)
	, mRealLast     (src.mRealLast)
{
	VDASSERT(((char *)&mSizeCheckSentinel - (char *)this) == sizeof(VDXFilterActivation));

	mOutputFrameArray[0] = &dst;

	SetSourceStreamCount(1);

	fma_ref = &fma;
	fma.filter = 0;
	fma.filterMod = 0;
	fma.filter_data = 0;
	fma.fmpreview = 0;
}

void VDFilterActivationImpl::SetSourceStreamCount(uint32 n) {
	mSourceStreamArray.resize(n - 1);
	mSourceStreamPtrArray.resize(n);

	mSourceStreamPtrArray[0] = (VDXFBitmap *)&mRealSrc;
	for(uint32 i = 1; i < n; ++i)
		mSourceStreamPtrArray[i] = (VDXFBitmap *)&mSourceStreamArray[i - 1];

	mSourceStreamCount = n;
	mpSourceStreams = mSourceStreamPtrArray.data();
}

///////////////////////////////////////////////////////////////////////////
//
//	FilterInstanceAutoDeinit
//
///////////////////////////////////////////////////////////////////////////

class FilterInstanceAutoDeinit : public vdrefcounted<IVDRefCount> {};

///////////////////////////////////////////////////////////////////////////
class FilterInstance::VideoPrefetcher : public IVDXVideoPrefetcher {
public:
	VideoPrefetcher(uint32 sourceCount);

	bool operator==(const VideoPrefetcher& other) const;
	bool operator!=(const VideoPrefetcher& other) const;

	void Clear();
	void TransformToNearestUnique(const vdvector<vdrefptr<IVDFilterFrameSource> >& src);
	void Finalize();

	int VDXAPIENTRY AddRef() { return 2; }
	int VDXAPIENTRY Release() { return 1; }

	void * VDXAPIENTRY AsInterface(uint32 iid) {
		if (iid == IVDXUnknown::kIID)
			return static_cast<IVDXUnknown *>(this);
		else if (iid == IVDXVideoPrefetcher::kIID)
			return static_cast<IVDXVideoPrefetcher *>(this);

		return NULL;
	}

	virtual void VDXAPIENTRY PrefetchFrame(sint32 srcIndex, sint64 frame, uint64 cookie);
	virtual void VDXAPIENTRY PrefetchFrameDirect(sint32 srcIndex, sint64 frame);
	virtual void VDXAPIENTRY PrefetchFrameSymbolic(sint32 srcIndex, sint64 frame);

	struct PrefetchInfo {
		sint64 mFrame;
		uint64 mCookie;
		uint32 mSrcIndex;

		bool operator==(const PrefetchInfo& other) const {
			return mFrame == other.mFrame && mCookie == other.mCookie && mSrcIndex == other.mSrcIndex;
		}

		bool operator!=(const PrefetchInfo& other) const {
			return mFrame != other.mFrame || mCookie != other.mCookie || mSrcIndex != other.mSrcIndex;
		}
	};

	typedef vdfastfixedvector<PrefetchInfo, 32> SourceFrames;
	SourceFrames mSourceFrames;

	sint64	mDirectFrame;
	sint64	mSymbolicFrame;
	uint32	mDirectFrameSrcIndex;
	uint32	mSymbolicFrameSrcIndex;
	uint32	mSourceCount;
	const char *mpError;
};

FilterInstance::VideoPrefetcher::VideoPrefetcher(uint32 sourceCount)
	: mDirectFrame(-1)
	, mDirectFrameSrcIndex(0)
	, mSymbolicFrame(-1)
	, mSymbolicFrameSrcIndex(0)
	, mSourceCount(sourceCount)
	, mpError(NULL)
{
}

bool FilterInstance::VideoPrefetcher::operator==(const VideoPrefetcher& other) const {
	return mDirectFrame == other.mDirectFrame && mSourceFrames == other.mSourceFrames;
}

bool FilterInstance::VideoPrefetcher::operator!=(const VideoPrefetcher& other) const {
	return mDirectFrame != other.mDirectFrame || mSourceFrames != other.mSourceFrames;
}

void FilterInstance::VideoPrefetcher::Clear() {
	mDirectFrame = -1;
	mSymbolicFrame = -1;
	mSourceFrames.clear();
}

void FilterInstance::VideoPrefetcher::TransformToNearestUnique(const vdvector<vdrefptr<IVDFilterFrameSource> >& srcs) {
	if (mDirectFrame >= 0)
		mDirectFrame = srcs[mDirectFrameSrcIndex]->GetNearestUniqueFrame(mDirectFrame);

	for(SourceFrames::iterator it(mSourceFrames.begin()), itEnd(mSourceFrames.end()); it != itEnd; ++it) {
		PrefetchInfo& info = *it;

		info.mFrame = srcs[info.mSrcIndex]->GetNearestUniqueFrame(info.mFrame);
	}
}

void FilterInstance::VideoPrefetcher::Finalize() {
	if (mSymbolicFrame < 0 && !mSourceFrames.empty()) {
		vdint128 accum(0);
		int count = 0;

		for(SourceFrames::const_iterator it(mSourceFrames.begin()), itEnd(mSourceFrames.end()); it != itEnd; ++it) {
			const PrefetchInfo& info = *it;
			accum += info.mFrame;
			++count;
		}

		accum += (count >> 1);
		mSymbolicFrame = accum / count;
	}
}

void VDXAPIENTRY FilterInstance::VideoPrefetcher::PrefetchFrame(sint32 srcIndex, sint64 frame, uint64 cookie) {
	if (srcIndex >= mSourceCount) {
		mpError = "An invalid source index was specified in a prefetch operation.";
		return;
	}

	if (frame < 0)
		frame = 0;

	PrefetchInfo& info = mSourceFrames.push_back();
	info.mFrame = frame;
	info.mCookie = cookie;
	info.mSrcIndex = srcIndex;
}

void VDXAPIENTRY FilterInstance::VideoPrefetcher::PrefetchFrameDirect(sint32 srcIndex, sint64 frame) {
	if (srcIndex >= mSourceCount) {
		mpError = "An invalid source index was specified in a prefetch operation.";
		return;
	}

	if (mDirectFrame >= 0) {
		mpError = "PrefetchFrameDirect() was called multiple times.";
		return;
	}

	if (frame < 0)
		frame = 0;

	mDirectFrame = frame;
	mDirectFrameSrcIndex = srcIndex;
	mSymbolicFrame = frame;
	mSymbolicFrameSrcIndex = srcIndex;
}

void VDXAPIENTRY FilterInstance::VideoPrefetcher::PrefetchFrameSymbolic(sint32 srcIndex, sint64 frame) {
	if (srcIndex >= mSourceCount) {
		mpError = "An invalid source index was specified in a prefetch operation.";
		return;
	}

	if (mSymbolicFrame >= 0) {
		mpError = "PrefetchFrameSymbolic() was called after a symbolic frame was already set.";
		return;
	}

	if (frame < 0)
		frame = 0;

	mSymbolicFrame = frame;
	mSymbolicFrameSrcIndex = srcIndex;
}

///////////////////////////////////////////////////////////////////////////
//
//	FilterInstance
//
///////////////////////////////////////////////////////////////////////////

class VDFilterThreadContextSwapper {
public:
	VDFilterThreadContextSwapper(void *pNewContext) {
#ifdef _M_IX86
		// 0x14: NT_TIB.ArbitraryUserPointer
		mpOldContext = __readfsdword(0x14);
		__writefsdword(0x14, (unsigned long)pNewContext);
#endif
	}

	~VDFilterThreadContextSwapper() {
#ifdef _M_IX86
		__writefsdword(0x14, mpOldContext);
#endif
	}

protected:
	unsigned long	mpOldContext;
};

///////////////////////////////////////////////////////////////////////////

VDFilterConfiguration::VDFilterConfiguration()
	: mbPreciseCrop(true)
	, mCropX1(0)
	, mCropY1(0)
	, mCropX2(0)
	, mCropY2(0)
	, mbEnabled(true)
	, mbForceSingleFB(false)
{
}

bool VDFilterConfiguration::IsCroppingEnabled() const {
	return (mCropX1 | mCropY1 | mCropX2 | mCropY2) != 0;
}

bool VDFilterConfiguration::IsPreciseCroppingEnabled() const {
	return mbPreciseCrop;
}

vdrect32 VDFilterConfiguration::GetCropInsets() const {
	return vdrect32(mCropX1, mCropY1, mCropX2, mCropY2);
}

void VDFilterConfiguration::SetCrop(int x1, int y1, int x2, int y2, bool precise) {
	mCropX1 = x1;
	mCropY1 = y1;
	mCropX2 = x2;
	mCropY2 = y2;
	mbPreciseCrop = precise;
}

///////////////////////////////////////////////////////////////////////////

VDFilterScriptWrapper::VDFilterScriptWrapper() {
	mpName		= NULL;
	obj_list	= NULL;
	Lookup		= NULL;
	func_list	= NULL;
	pNextObject	= NULL;
}

VDFilterScriptWrapper::VDFilterScriptWrapper(const VDFilterScriptWrapper& src)
	: mFuncList(src.mFuncList)
{
	mpName		= NULL;
	obj_list	= NULL;
	Lookup		= NULL;
	func_list	= mFuncList.data();
	pNextObject	= NULL;
}

void VDFilterScriptWrapper::Init(const VDXScriptObject *src, VDScriptFunction voidfunc, VDScriptFunction intfunc, VDScriptFunction varfunc) {
	if (!src)
		return;

	const ScriptFunctionDef *pf = src->func_list;

	if (pf) {
		for(; pf->func_ptr; ++pf) {
			VDScriptFunctionDef def;

			def.arg_list	= pf->arg_list;
			def.name		= pf->name;

			switch(def.arg_list[0]) {
			default:
			case '0':
				def.func_ptr	= voidfunc;
				break;
			case 'i':
				def.func_ptr	= intfunc;
				break;
			case 'v':
				def.func_ptr	= varfunc;
				break;
			}

			mFuncList.push_back(def);
		}

		VDScriptFunctionDef def_end = {NULL};
		mFuncList.push_back(def_end);

		func_list	= mFuncList.data();
	}
}

int VDFilterScriptWrapper::GetFuncIndex(const VDScriptFunctionDef *fdef) const {
	return fdef - mFuncList.data();
}

///////////////////////////////////////////////////////////////////////////

class FilterInstance::SamplingInfo : public vdrefcounted<IVDRefCount> {
public:
	VDXFilterPreviewSampleCallback mpCB;
	void *mpCBData;
	IFilterModPreviewSample* handler;
	int result;
};

FilterInstance::FilterInstance(const FilterInstance& fi)
	: VDFilterConfiguration(fi)
	, mbInvalidFormat	(fi.mbInvalidFormat)
	, mbInvalidFormatHandling(fi.mbInvalidFormatHandling)
	, mbExcessiveFrameSize(fi.mbExcessiveFrameSize)
	, mbAccelerated		(fi.mbAccelerated)
	, mFlags			(fi.mFlags)
	, mbStarted			(fi.mbStarted)
	, mbFirstFrame		(fi.mbFirstFrame)
	, mFilterName		(fi.mFilterName)
	, mpAutoDeinit		(fi.mpAutoDeinit)
	, mpLogicError		(fi.mpLogicError)
	, mScriptWrapper	(fi.mScriptWrapper)
	, mpFDInst			(fi.mpFDInst)
	, mpRequestInProgress(NULL)
	, mbRequestFramePending(false)
	, mbRequestFrameBeingProcessed(false)
	, mpAccelEngine		(NULL)
	, mpAccelContext	(NULL)
	, mPrepareInfo()
	, mPrepareInfo2()
	, fmProject			(fi.fmProject)
{
	if (mpAutoDeinit)
		mpAutoDeinit->AddRef();
	else
		mbStarted = false;

	filter = const_cast<FilterDefinition *>(&fi.mpFDInst->Attach());
	filter_data = fi.filter_data;

	fmProject.inst = this;
	fma.fmproject = &fmProject;
	fma.fmtimeline = &g_project->filterModTimeline;
	fma.fmsystem = &g_project->filterModSystem;
	fma.fmpixmap = &g_project->filterModPixmap;

	fma.filter = filter;
	fma.filterMod = const_cast<FilterModDefinition *>(&fi.mpFDInst->GetFilterModDef());
	fma.filter_data = filter_data;

	mAPIVersion = fi.mpFDInst->GetAPIVersion();
	VDASSERT(mAPIVersion);
}

FilterInstance::FilterInstance(FilterDefinitionInstance *fdi)
	: mbInvalidFormat(true)
	, mbInvalidFormatHandling(false)
	, mbExcessiveFrameSize(false)
	, mbAccelerated(false)
	, mFlags(0)
	, mbStarted(false)
	, mbFirstFrame(false)
	, mpFDInst(fdi)
	, mpAutoDeinit(NULL)
	, mpLogicError(NULL)
	, mpRequestInProgress(NULL)
	, mbRequestFramePending(false)
	, mbRequestFrameCompleted(false)
	, mbRequestFrameBeingProcessed(false)
	, mpAccelEngine(NULL)
	, mpAccelContext(NULL)
	, mPrepareInfo()
	, mPrepareInfo2()
{
	filter = const_cast<FilterDefinition *>(&fdi->Attach());

	fmProject.inst = this;
	fma.fmproject = &fmProject;
	fma.fmtimeline = &g_project->filterModTimeline;
	fma.fmsystem = &g_project->filterModSystem;
	fma.fmpixmap = &g_project->filterModPixmap;

	fma.filter = filter;
	fma.filterMod = const_cast<FilterModDefinition *>(&fdi->GetFilterModDef());
	mAPIVersion = fdi->GetAPIVersion();
	VDASSERT(mAPIVersion);

	src.hdc = NULL;
	dst.hdc = NULL;
	last->hdc = NULL;
	x1 = 0;
	y1 = 0;
	x2 = 0;
	y2 = 0;

	if (filter->inst_data_size) {
		if (!(filter_data = allocmem(filter->inst_data_size)))
			throw MyMemoryError();

		memset(filter_data, 0, filter->inst_data_size);

		if (filter->initProc) {
			try {
				vdrefptr<FilterInstanceAutoDeinit> autoDeinit;
				
				if (!filter->copyProc && !filter->copyProc2 && filter->deinitProc)
					autoDeinit = new FilterInstanceAutoDeinit;

				VDFilterThreadContextSwapper autoContextSwap(&mThreadContext);
				if (filter->initProc(AsVDXFilterActivation(), &g_VDFilterCallbacks)) {
					if (filter->deinitProc)
						filter->deinitProc(AsVDXFilterActivation(), &g_VDFilterCallbacks);

					freemem(filter_data);
					throw MyError("Filter failed to initialize.");
				}

				mpAutoDeinit = autoDeinit.release();
			} catch(const MyError& e) {
				throw MyError("Cannot initialize filter '%s': %s", filter->name, e.gets());
			}
		}

		if (fma.filterMod->activateProc) {
			fma.filter_data = filter_data;
			try {
				VDFilterThreadContextSwapper autoContextSwap(&mThreadContext);
				fma.filterMod->activateProc(&fma, &g_VDFilterCallbacks);
			} catch(const MyError& e) {
				throw MyError("Cannot initialize filter '%s': %s", filter->name, e.gets());
			}
		}

		mFilterName = VDTextAToW(filter->name);

	} else
		filter_data = NULL;

	mScriptWrapper.Init(filter->script_obj, ScriptFunctionThunkVoid, ScriptFunctionThunkInt, ScriptFunctionThunkVariadic);
}

FilterInstance::~FilterInstance() {
	VDASSERT(!mpRequestInProgress);

	VDFilterThreadContextSwapper autoContextSwap(&mThreadContext);

	if (mpAutoDeinit) {
		if (!mpAutoDeinit->Release())
			filter->deinitProc(AsVDXFilterActivation(), &g_VDFilterCallbacks);
		mpAutoDeinit = NULL;
	} else if (filter->deinitProc) {
		filter->deinitProc(AsVDXFilterActivation(), &g_VDFilterCallbacks);
	}

	freemem(filter_data);

	mpFDInst->Detach();

	VDFilterFrameRequestError *oldError = mpLogicError.xchg(NULL);
	if (oldError)
		oldError->Release();
}

void *FilterInstance::AsInterface(uint32 iid) {
	if (iid == FilterInstance::kTypeID)
		return static_cast<FilterInstance *>(this);

	return NULL;
}

FilterInstance *FilterInstance::Clone() {
	FilterInstance *fi = new FilterInstance(*this);

	if (!fi) throw MyMemoryError();

	if (fi->filter_data) {
		const VDXFilterDefinition *def = fi->filter;
		fi->filter_data = allocmem(def->inst_data_size);

		if (!fi->filter_data) {
			delete fi;
			throw MyMemoryError();
		}

		VDFilterThreadContextSwapper autoContextSwap(&mThreadContext);
		if (def->copyProc2)
			def->copyProc2(AsVDXFilterActivation(), &g_VDFilterCallbacks, fi->filter_data, fi->AsVDXFilterActivation(), &g_VDFilterCallbacks);
		else if (def->copyProc)
			def->copyProc(AsVDXFilterActivation(), &g_VDFilterCallbacks, fi->filter_data);
		else
			memcpy(fi->filter_data, filter_data, def->inst_data_size);

		if (fma.filterMod->activateProc) {
			fi->fma.filter_data = fi->filter_data;
			fma.filterMod->activateProc(&fi->fma, &g_VDFilterCallbacks);
		}
	}

	return fi;
}

const char *FilterInstance::GetName() const {
	return filter->name;
}

const VDXFilterDefinition *FilterInstance::GetDefinition() const { return &mpFDInst->GetDef(); }

bool FilterInstance::IsConfigurable() const {
	return filter->configProc != NULL;
}

bool FilterInstance::IsInPlace() const {
	return (mFlags & FILTERPARAM_SWAP_BUFFERS) == 0;
}

bool FilterInstance::IsAcceleratable() const {
	return filter->accelRunProc != NULL;
}

bool FilterInstance::Configure(VDXHWND parent, IVDXFilterPreview2 *ifp2, IFilterModPreview *ifmpreview) {
	VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);
	bool success;

	this->ifp = ifp2;
	this->ifp2 = ifp2;
	this->fma.fmpreview = ifmpreview;

	vdprotected1("configuring filter \"%s\"", const char *, filter->name) {
		VDFilterThreadContextSwapper autoSwap(&mThreadContext);

		success = !filter->configProc(AsVDXFilterActivation(), &g_VDFilterCallbacks, parent);
	}

	this->ifp2 = NULL;
	this->ifp = NULL;
	this->fma.fmpreview = NULL;

	return success;
}

void FilterInstance::PrepareReset() {
	mbInvalidFormatHandling = false;
}

uint32 FilterInstance::Prepare(const VFBitmapInternal *inputs, uint32 numInputs, VDFilterPrepareInfo& prepareInfo) {
	SetSourceStreamCount(numInputs);

	const VFBitmapInternal& input = inputs[0];

	bool testingInvalidFormat = input.mpPixmapLayout->format == 255;
	bool invalidCrop = false;

	mbInvalidFormat	= false;
	mbExcessiveFrameSize = false;

	// Init all of the strams.
	prepareInfo.mStreams.resize(numInputs);

	for(uint32 streamIndex = 0; streamIndex < numInputs; ++streamIndex) {
		VDFilterPrepareStreamInfo& streamInfo = prepareInfo.mStreams[streamIndex];
		VDPixmapLayout& preAlignLayout = streamInfo.mExternalSrcPreAlign.mPixmapLayout;
		const VFBitmapInternal& streamInput = inputs[streamIndex];

		streamInfo.mExternalSrc		= streamInput;
		streamInfo.mExternalSrcPreAlign	= streamInput;
		if (testingInvalidFormat) {
			preAlignLayout.format = nsVDXPixmap::kPixFormat_XRGB8888;
		} else {
			// Only stream 0 is cropped.
			if (!streamIndex) {
				const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(VDIsVDXAFormat(preAlignLayout.format) ? nsVDPixmap::kPixFormat_XRGB8888 : preAlignLayout.format);
				int xmask = ~((1 << (formatInfo.qwbits + formatInfo.auxwbits)) - 1);
				int ymask = ~((1 << (formatInfo.qhbits + formatInfo.auxhbits)) - 1);

				if (mbPreciseCrop) {
					if ((mCropX1 | mCropY1) & ~xmask)
						invalidCrop = true;

					if ((mCropX2 | mCropY2) & ~ymask)
						invalidCrop = true;
				}

				int qx1 = (mCropX1 & xmask) >> formatInfo.qwbits;
				int qy1 = (mCropY1 & ymask) >> formatInfo.qhbits;
				int qx2 = (mCropX2 & xmask) >> formatInfo.qwbits;
				int qy2 = (mCropY2 & ymask) >> formatInfo.qhbits;

				int qw = input.w >> formatInfo.qwbits;
				int qh = input.h >> formatInfo.qhbits;

				// Clamp the crop rect at this point to avoid going below 1x1.
				// We will throw an exception later during init.
				if (qx1 >= qw)
					qx1 = qw - 1;

				if (qy1 >= qh)
					qy1 = qh - 1;

				if (qx1 + qx2 >= qw)
					qx2 = (qw - qx1) - 1;

				if (qy1 + qy2 >= qh)
					qy2 = (qh - qy1) - 1;

				VDASSERT(qx1 >= 0 && qy1 >= 0 && qx2 >= 0 && qy2 >= 0);
				VDASSERT(qx1 + qx2 < qw);
				VDASSERT(qy1 + qy2 < qh);

				if (preAlignLayout.format == nsVDXPixmap::kPixFormat_VDXA_RGB) {
					preAlignLayout.format = nsVDPixmap::kPixFormat_XRGB8888;
					preAlignLayout = VDPixmapLayoutOffset(preAlignLayout, qx1 << formatInfo.qwbits, qy1 << formatInfo.qhbits);
					preAlignLayout.format = nsVDXPixmap::kPixFormat_VDXA_RGB;
				} else if (preAlignLayout.format == nsVDXPixmap::kPixFormat_VDXA_YUV) {
					preAlignLayout.format = nsVDPixmap::kPixFormat_XRGB8888;
					preAlignLayout = VDPixmapLayoutOffset(preAlignLayout, qx1 << formatInfo.qwbits, qy1 << formatInfo.qhbits);
					preAlignLayout.format = nsVDXPixmap::kPixFormat_VDXA_YUV;
				} else {
					preAlignLayout = VDPixmapLayoutOffset(preAlignLayout, qx1 << formatInfo.qwbits, qy1 << formatInfo.qhbits);
				}

				preAlignLayout.w -= (qx1+qx2) << formatInfo.qwbits;
				preAlignLayout.h -= (qy1+qy2) << formatInfo.qhbits;
			}
		}

		streamInfo.mExternalSrcPreAlign.ConvertPixmapLayoutToBitmapLayout();
		streamInfo.mbAlignOnEntry = false;
	}

	pfsi	= &mfsi;
	memset(&mfsi, 0, sizeof mfsi);
	mSourceFrameCount = 0;
	mpSourceFrames = NULL;

	uint32 flags = FILTERPARAM_SWAP_BUFFERS;

	for(;;) {
		for(uint32 streamIndex = 0; streamIndex < numInputs; ++streamIndex) {
			VDFilterPrepareStreamInfo& streamInfo = prepareInfo.mStreams[streamIndex];
			VFBitmapInternal& src = streamIndex ? mSourceStreamArray[streamIndex - 1] : mRealSrc;

			src = streamInfo.mExternalSrcPreAlign;

			if (streamInfo.mbAlignOnEntry) {
				VDPixmapCreateLinearLayout(src.mPixmapLayout, src.mPixmapLayout.format, src.mPixmapLayout.w, src.mPixmapLayout.h, 16);
				src.ConvertPixmapLayoutToBitmapLayout();
			}

			src.dwFlags	= 0;
			src.hdc		= NULL;
			src.mBorderWidth = 0;
			src.mBorderHeight = 0;

			streamInfo.mExternalSrcCropped = src;
		}

		mExternalSrcCropped	= mRealSrc;

		mRealLast			= mRealSrc;
		mRealLast.dwFlags	= 0;
		mRealLast.hdc		= NULL;

		mRealDst			= mRealSrc;
		mRealDst.dwFlags	= 0;
		mRealDst.hdc		= NULL;
		mRealDst.mBorderWidth = 0;
		mRealDst.mBorderHeight = 0;

		if (testingInvalidFormat) {
			mRealSrc.mPixmapLayout.format = 255;
			mRealDst.mPixmapLayout.format = 255;
		}

		// V16+: default to flexible output pitch
		if (mAPIVersion >= 16) {
			mRealDst.depth = 0;
			mRealDst.mPixmapLayout.pitch = 0;
		}

		if (filter->paramProc) {
			VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);

			vdprotected1("preparing filter \"%s\"", const char *, filter->name) {
				VDFilterThreadContextSwapper autoSwap(&mThreadContext);

				flags = filter->paramProc(AsVDXFilterActivation(), &g_VDFilterCallbacks);
			}

			// Remove flags banned starting with V16+
			if (mAPIVersion >= 16 && flags != FILTERPARAM_NOT_SUPPORTED) {
				if (flags & FILTERPARAM_NEEDS_LAST) {
					flags &= ~FILTERPARAM_NEEDS_LAST;

					SetLogicError("A V16+ filter cannot request a last-frame buffer.");
				}

				if (!(flags & FILTERPARAM_SUPPORTS_ALTFORMATS)) {
					flags |= FILTERPARAM_SUPPORTS_ALTFORMATS;

					SetLogicError("A V16+ filter must set the ALTFORMATS flag.");
				}
			}

		} else {
			// We won't hit this ordinarily because the invalid format test isn't done unless FILTERPARAM_SUPPORTS_ALTFORMATS,
			// but we'll do it anyway.
			if (testingInvalidFormat)
				flags = FILTERPARAM_NOT_SUPPORTED;
		}

		if (fma.filterMod && fma.filterMod->paramProc) {
			vdprotected1("preparing filter \"%s\"", const char *, filter->name) {
				VDFilterThreadContextSwapper autoSwap(&mThreadContext);

				mFilterModFlags = fma.filterMod->paramProc(AsVDXFilterActivation(), &g_VDFilterCallbacks);
			}
		} else {
			mFilterModFlags = 0;
			if(mFilterName==L"Deshaker v3.0" || mFilterName==L"Deshaker v3.1") {
				VDStringA buf;
				GetSettingsString(buf);
				if(buf==" (Pass 1)") mFilterModFlags |= FILTERMODPARAM_TERMINAL;
			}
		}

		if (invalidCrop)
			flags = FILTERPARAM_NOT_SUPPORTED;

		// we can't allow single FB mode with VDXA
		if (VDIsVDXAFormat(mRealSrc.mPixmapLayout.format) && mbForceSingleFB)
			flags = FILTERPARAM_NOT_SUPPORTED;

		if (flags == FILTERPARAM_NOT_SUPPORTED) {
			mbInvalidFormat = true;
			break;
		}

		// check if alignment is required
		if (flags & FILTERPARAM_ALIGN_SCANLINES) {
			bool alignmentViolationDetected = false;

			for(uint32 streamIndex = 0; streamIndex < numInputs; ++streamIndex) {
				VDFilterPrepareStreamInfo& streamInfo = prepareInfo.mStreams[streamIndex];
				VFBitmapInternal& src = streamIndex ? mSourceStreamArray[streamIndex - 1] : mRealSrc;

				if (!VDIsVDXAFormat(src.mPixmapLayout.format) && !VDPixmapIsLayoutVectorAligned(src.mPixmapLayout)) {
					VDASSERT(!streamInfo.mbAlignOnEntry);
					streamInfo.mbAlignOnEntry = true;
					alignmentViolationDetected = true;
				}
			}

			if (alignmentViolationDetected)
				continue;
		}

		break;
	}

	if (testingInvalidFormat) {
		mRealSrc.mPixmapLayout.format = nsVDXPixmap::kPixFormat_XRGB8888;
		mRealDst.mPixmapLayout.format = nsVDXPixmap::kPixFormat_XRGB8888;
	}

	mFlags = flags;
	mLag = (uint32)mFlags >> 16;

	mbAccelerated = false;
	if (VDIsVDXAFormat(mRealSrc.mPixmapLayout.format))
		mbAccelerated = true;

	if (mRealDst.depth) {
		// check if the frame size is excessive and clamp if so
		if ((mRealDst.w | mRealDst.h) < 0 || mRealDst.w > kFrameAxisLimit || mRealDst.h > kFrameAxisLimit || (uint64)mRealDst.w * mRealDst.h > kFrameSizeLimit) {
			mRealDst.offset = 0;
			mRealDst.w = 128;
			mRealDst.h = 128;
			mRealDst.offset = 0;
			mRealDst.pitch = 512;
			mbExcessiveFrameSize = true;
		}

		mRealDst.modulo	= mRealDst.pitch - 4*mRealDst.w;
		mRealDst.size	= mRealDst.offset + vdptrdiffabs(mRealDst.pitch) * mRealDst.h;
		VDASSERT((sint32)mRealDst.size >= 0);

		mRealDst.ConvertBitmapLayoutToPixmapLayout();
	} else {
		// check if the frame size is excessive and clamp if so
		if ((mRealDst.mPixmapLayout.w | mRealDst.mPixmapLayout.h) < 0 || mRealDst.mPixmapLayout.w > kFrameAxisLimit || mRealDst.mPixmapLayout.h > kFrameAxisLimit || (uint64)mRealDst.mPixmapLayout.w * mRealDst.mPixmapLayout.h > kFrameSizeLimit) {
			mRealDst.mPixmapLayout.w = 128;
			mRealDst.mPixmapLayout.h = 128;
			mRealDst.mPixmapLayout.data = 0;
			mRealDst.mPixmapLayout.pitch = 512;
			mbExcessiveFrameSize = true;
		}

		if (!mRealDst.mPixmapLayout.pitch) {
			if (VDIsVDXAFormat(mRealDst.mPixmapLayout.format)) {
				mRealDst.mPixmapLayout.data = 0;
				mRealDst.mPixmapLayout.data2 = 0;
				mRealDst.mPixmapLayout.data3 = 0;
				mRealDst.mPixmapLayout.pitch = 0;
				mRealDst.mPixmapLayout.pitch2 = 0;
				mRealDst.mPixmapLayout.pitch3 = 0;
			} else {
				VDPixmapCreateLinearLayout(mRealDst.mPixmapLayout, mRealDst.mPixmapLayout.format, mRealDst.mPixmapLayout.w, mRealDst.mPixmapLayout.h, 16);

				if (mRealDst.mPixmapLayout.format == nsVDPixmap::kPixFormat_XRGB8888)
					VDPixmapLayoutFlipV(mRealDst.mPixmapLayout);
			}
		}

		mRealDst.ConvertPixmapLayoutToBitmapLayout();
	}

	*mRealLast.mpPixmapLayout = *mRealSrc.mpPixmapLayout;
	mRealLast.ConvertPixmapLayoutToBitmapLayout();

	mfsi.lMicrosecsPerSrcFrame	= VDRoundToInt((double)mRealSrc.mFrameRateLo / (double)mRealSrc.mFrameRateHi * 1000000.0);
	mfsi.lMicrosecsPerFrame		= VDRoundToInt((double)mRealDst.mFrameRateLo / (double)mRealDst.mFrameRateHi * 1000000.0);

	if (testingInvalidFormat) {
		if (flags != FILTERPARAM_NOT_SUPPORTED)
			mbInvalidFormatHandling = true;
	} else {
		if (mRealSrc.dwFlags & VDXFBitmap::NEEDS_HDC) {
			if (mRealSrc.mPixmap.format && mRealSrc.mPixmap.format != nsVDXPixmap::kPixFormat_XRGB8888)
				flags = FILTERPARAM_NOT_SUPPORTED;
		}

		if (mRealDst.dwFlags & VDXFBitmap::NEEDS_HDC) {
			if (mRealDst.mPixmap.format && mRealDst.mPixmap.format != nsVDXPixmap::kPixFormat_XRGB8888)
				flags = FILTERPARAM_NOT_SUPPORTED;
		}
	}

	if (mbInvalidFormat) {
		mRealSrc = input;
		mRealDst = input;
	}

	prepareInfo.mLastFrameSizeRequired = 0;

	if (flags & FILTERPARAM_NEEDS_LAST)
		prepareInfo.mLastFrameSizeRequired = mRealLast.size + mRealLast.offset;

	return flags;
}

struct FilterInstance::StopStartMessage : public VDFilterAccelEngineMessage {
	FilterInstance *mpThis;
	MyError mError;
};

void FilterInstance::Start(IVDFilterFrameEngine *engine) {
	throw MyError("This function is not supported.");
}

void FilterInstance::Start(uint32 flags, IVDFilterFrameSource *const *pSources, IVDFilterFrameEngine *engine, VDFilterAccelEngine *accelEngine) {
	if (mbStarted)
		return;

	mProfileCacheFilterName = 0;

	if (mbAccelerated && accelEngine)
		mpAccelEngine = accelEngine;

	if (GetInvalidFormatHandlingState())
		throw MyError("Cannot start filters: Filter \"%s\" is not handling image formats correctly.",
			GetName());

	if (GetInvalidFormatState())
		throw MyError("Cannot start filters: An instance of filter \"%s\" cannot process its input of %ux%d (%s).",
			GetName(),
			mRealSrc.mpPixmapLayout->w,
			mRealSrc.mpPixmapLayout->h,
			VDPixmapGetInfo(mRealSrc.mpPixmapLayout->format).name);

	if (GetExcessiveFrameSizeState())
		throw MyError("Cannot start filters: An instance of filter \"%s\" is producing too big of a frame.", GetName());

	mfsi.flags = flags;

	mFsiDelayRing.resize(GetFrameDelay());
	mDelayRingPos = 0;

	mLastResultFrame = -1;

	// Note that we set this immediately so we can Stop() the filter, even if
	// it fails.
	mbStarted = true;

	// allocate blend buffer
	if (GetAlphaParameterCurve()) {
		if (!(mFlags & FILTERPARAM_SWAP_BUFFERS)) {
			// if this is an in-place filter and we have a blend curve, allocate other buffer as well.
			vdrefptr<VDFilterFrameBuffer> blendbuf;
			if (!mResultAllocator.Allocate(~blendbuf))
				throw MyError("Cannot start filter '%s': Unable to allocate blend buffer.", filter->name);

			mBlendTemp.mPixmapLayout = mRealDst.mPixmapLayout;
			mBlendTemp.ConvertPixmapLayoutToBitmapLayout();
			mBlendTemp.BindToFrameBuffer(blendbuf, false);
		}
	}

	if (mbAccelerated) {
		mRealSrc.CopyNullBufferParams();
		mRealDst.CopyNullBufferParams();
	} else {
		if (mFlags & FILTERPARAM_NEEDS_LAST) {
			vdrefptr<VDFilterFrameBuffer> last;
			mSourceAllocator.Allocate(~last);
			mRealLast.BindToFrameBuffer(last, false);
		}

		// Older filters use the fa->src/dst/last fields directly and need buffers
		// bound in order to start correctly.
		
		if (mAPIVersion < 16 || mbForceSingleFB) {
			if (!mRealSrc.hdc) {
				vdrefptr<VDFilterFrameBuffer> tempSrc;
				mSourceAllocator.Allocate(~tempSrc);

				mRealSrc.BindToFrameBuffer(tempSrc, true);

				if (!(mFlags & FILTERPARAM_SWAP_BUFFERS) && !mRealDst.hdc)
					mRealDst.BindToFrameBuffer(tempSrc, false);
			}

			if ((mFlags & FILTERPARAM_SWAP_BUFFERS) && !mRealDst.hdc) {
				vdrefptr<VDFilterFrameBuffer> tempDst;
				mResultAllocator.Allocate(~tempDst);
				mRealDst.BindToFrameBuffer(tempDst, true);
			}
		}
	}

	try {
		if (mbAccelerated) {
			StopStartMessage msg;
			msg.mpCallback = StartFilterCallback;
			msg.mpThis = this;

			mpAccelEngine->SyncCall(&msg);

			if (msg.mError.gets())
				throw MyError(msg.mError);
		} else {
			StartInner();
		}
	} catch(const MyError&) {
		if (!mRealSrc.hdc)
			mRealSrc.Unbind();

		if (!mRealDst.hdc)
			mRealDst.Unbind();

		Stop();
		throw;
	}

	if (!mRealSrc.hdc && !mbForceSingleFB)
		mRealSrc.Unbind();

	if (!mRealDst.hdc && !mbForceSingleFB)
		mRealDst.Unbind();

	mSourceAllocator.TrimAllocator();

	mSources.assign(pSources, pSources + mSourceStreamCount);
	mbCanStealSourceBuffer = IsInPlace() && !mRealSrc.hdc;
	mSharingPredictor.Clear();

	if (mpLogicError)
		throw MyError("Cannot start filter '%s': %s", filter->name, mpLogicError->mError.c_str());

	VDASSERT(mRealDst.mBorderWidth < 10000000 && mRealDst.mBorderHeight < 10000000);

	mbRequestFramePending = false;
	mbRequestFrameCompleted = false;
	mbRequestFrameBeingProcessed = false;

	mpEngine = engine;

	VDASSERT(!mRealDst.GetBuffer());
	VDASSERT(!mExternalDst.GetBuffer());
}

void FilterInstance::StartFilterCallback(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message) {
	StopStartMessage& msg = *static_cast<StopStartMessage *>(message);
	
	try {
		msg.mpThis->StartInner();
	} catch(MyError& err) {
		msg.mError.swap(err);
	}
}

void FilterInstance::StartInner() {
	if (mpAccelEngine) {
		mpAccelContext = new VDFilterAccelContext;
		mpAccelContext->AddRef();
		mpAccelContext->Init(*mpAccelEngine);
		mpVDXA = mpAccelContext;
	}

	if (filter->startProc) {
		int rcode;
		try {
			VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);

			vdprotected1("starting filter \"%s\"", const char *, filter->name) {
				VDFilterThreadContextSwapper autoSwap(&mThreadContext);

				rcode = filter->startProc(AsVDXFilterActivation(), &g_VDFilterCallbacks);
			}
		} catch(const MyError& e) {
			throw MyError("Cannot start filter '%s': %s", filter->name, e.gets());
		}

		if (rcode)
			throw MyError("Cannot start filter '%s': Unknown failure.", filter->name);

		mbFirstFrame = true;
	}
}

void FilterInstance::Stop() {
	if (!mbStarted)
		return;

	mbStarted = false;

	if (mbRequestFramePending || mbRequestFrameCompleted) {
		EndFrame();

		mbRequestFramePending = false;
		mbRequestFrameCompleted = false;
	} else {
		VDASSERT(!mRealDst.GetBuffer());
		VDASSERT(!mExternalDst.GetBuffer());
	}

	if (mpRequestInProgress)
		CloseRequest(false);

	if (mbAccelerated) {
		StopStartMessage msg;
		msg.mpCallback = StopFilterCallback;
		msg.mpThis = this;

		mpAccelEngine->SyncCall(&msg);

		if (msg.mError.gets())
			throw MyError(msg.mError);
	} else {
		StopInner();
	}

	mFsiDelayRing.clear();

	mBlendTemp.Unbind();

	mRealLast.mDIBSection.Shutdown();
	mRealDst.mDIBSection.Shutdown();
	mRealSrc.mDIBSection.Shutdown();
	mRealLast.Unbind();
	mRealDst.Unbind();
	mRealSrc.Unbind();
	mFileMapping.Shutdown();

	mFrameQueueWaiting.Shutdown();
	mFrameQueueInProgress.Shutdown();
	mFrameCache.Flush();

	mSources.clear();
	mSourceAllocator.Clear();
	mResultAllocator.Clear();

	mpSourceConversionBlitter = NULL;

	mpAccelEngine = NULL;
	mpEngine = NULL;

	VDASSERT(!mRealDst.GetBuffer());
	VDASSERT(!mExternalDst.GetBuffer());
}

void FilterInstance::StopFilterCallback(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message) {
	StopStartMessage& msg = *static_cast<StopStartMessage *>(message);
	
	try {
		msg.mpThis->StopInner();
	} catch(MyError& err) {
		msg.mError.swap(err);
	}
}

void FilterInstance::StopInner() {
	if (filter->endProc) {
		VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);
		vdprotected1("stopping filter \"%s\"", const char *, filter->name) {
			VDFilterThreadContextSwapper autoSwap(&mThreadContext);

			filter->endProc(AsVDXFilterActivation(), &g_VDFilterCallbacks);
		}
	}

	if (mpAccelContext) {
		mpVDXA = NULL;
		mpAccelContext->Release();
		mpAccelContext = NULL;
	}
}

const char *FilterInstance::GetDebugDesc() const {
	return filter->name;
}

VDFilterFrameAllocatorProxy *FilterInstance::GetOutputAllocatorProxy() {
	return &mResultAllocator;
}

void FilterInstance::RegisterSourceAllocReqs(uint32 index, VDFilterFrameAllocatorProxy *prev) {
	VFBitmapInternal& src = index ? mSourceStreamArray[index - 1] : mRealSrc;
	prev->AddBorderRequirement(src.mBorderWidth, src.mBorderHeight);

	mSourceAllocator.Link(prev);

	if (!(mFlags & FILTERPARAM_SWAP_BUFFERS))
		mResultAllocator.Link(prev);
}

void FilterInstance::RegisterAllocatorProxies(VDFilterFrameAllocatorManager *mgr) {
	mSourceAllocator.Clear();

	mgr->AddAllocatorProxy(&mSourceAllocator);

	if (!mbAccelerated)
		mSourceAllocator.AddSizeRequirement(mRealSrc.size + mRealSrc.offset);

	uint32 dstSizeRequired = mRealDst.size + mRealDst.offset;
	mResultAllocator.Clear();

	if (mbAccelerated) {
		mResultAllocator.SetAccelerationRequirement(VDFilterFrameAllocatorProxy::kAccelModeRender);
		mResultAllocator.AddSizeRequirement((mRealDst.h << 16) + mRealDst.w);
	} else {
		mResultAllocator.AddSizeRequirement(dstSizeRequired);
	}

	mgr->AddAllocatorProxy(&mResultAllocator);
}

bool FilterInstance::CreateRequest(sint64 outputFrame, bool writable, uint32 batchNumber, IVDFilterFrameClientRequest **req) {
	VDASSERT(mbStarted);

	if (outputFrame < 0)
		outputFrame = 0;

	if (mRealDst.mFrameCount > 0 && outputFrame >= mRealDst.mFrameCount)
		outputFrame = mRealDst.mFrameCount - 1;

	mSharingPredictor.OnRequest(outputFrame);

	vdrefptr<VDFilterFrameRequest> r;
	bool cached = false;
	bool newRequest = false;

	if (!mFrameQueueWaiting.GetRequest(outputFrame, ~r) && !mFrameQueueInProgress.GetRequest(outputFrame, ~r)) {
		newRequest = true;

		mFrameQueueWaiting.CreateRequest(~r);

		VDFilterFrameRequestTiming timing;
		timing.mSourceFrame = GetSourceFrame(outputFrame);
		timing.mOutputFrame = outputFrame;
		r->SetTiming(timing);
		r->SetBatchNumber(batchNumber);

		vdrefptr<VDFilterFrameBuffer> buf;

		if (mFrameCache.Lookup(outputFrame, ~buf)) {
			cached = true;

			r->SetResultBuffer(buf);
		} else {
			VideoPrefetcher vp((uint32)mSources.size());

			if (mLag) {
				for(uint32 i=0; i<=mLag; ++i) {
					VDPosition laggedSrc = outputFrame + i;

					if (laggedSrc >= 0)
						GetPrefetchInfo(laggedSrc, vp, false);
				}
			} else {
				GetPrefetchInfo(outputFrame, vp, false);
			}

			if (!mSources.empty()) {
				if (vp.mDirectFrame >= 0) {
					r->SetSourceCount(1);

					vdrefptr<IVDFilterFrameClientRequest> srcreq;
					mSources[vp.mDirectFrameSrcIndex]->CreateRequest(vp.mDirectFrame, false, batchNumber, ~srcreq);

					srcreq->Start(NULL, 0, vp.mDirectFrameSrcIndex);

					r->SetSourceRequest(0, srcreq);
				} else {
					r->SetSourceCount(vp.mSourceFrames.size());

					uint32 idx = 0;
					for(VideoPrefetcher::SourceFrames::const_iterator it(vp.mSourceFrames.begin()), itEnd(vp.mSourceFrames.end()); it != itEnd; ++it) {
						const VideoPrefetcher::PrefetchInfo& prefetchInfo = *it;
						bool writable = (idx == 0) && IsInPlace();

						vdrefptr<IVDFilterFrameClientRequest> srcreq;
						mSources[prefetchInfo.mSrcIndex]->CreateRequest(prefetchInfo.mFrame, writable, batchNumber, ~srcreq);

						srcreq->Start(NULL, prefetchInfo.mCookie, prefetchInfo.mSrcIndex);

						r->SetSourceRequest(idx++, srcreq);
					}
				}
			}
		}
	}

	vdrefptr<IVDFilterFrameClientRequest> creq;
	r->CreateClient(writable, ~creq);

	if (mSharingPredictor.IsSharingPredicted(outputFrame))
		r->SetStealable(false);

	if (cached)
		r->MarkComplete(true);
	else if (newRequest)
		mFrameQueueWaiting.Add(r);

	*req = creq.release();
	return true;
}

bool FilterInstance::CacheLookup(VDPosition frame, VDFilterFrameBuffer** buf) {
  return mFrameCache.Lookup(frame,buf);
}

int FilterInstance::GetSamplingRequestResult(IVDFilterFrameClientRequest *req) {
	VDFilterFrameClientRequest *vdreq = (VDFilterFrameClientRequest*)req;
	SamplingInfo *sampInfo = (SamplingInfo *)vdreq->GetExtraInfo();
	return sampInfo ? sampInfo->result : 0;
}

bool FilterInstance::CreateSamplingRequest(sint64 outputFrame, VDXFilterPreviewSampleCallback sampleCB, void *sampleCBData, IFilterModPreviewSample* sampleHandler, uint32 batchNumber, IVDFilterFrameClientRequest **req) {
	VDASSERT(mbStarted);

	if (outputFrame < 0)
		outputFrame = 0;

	if (mRealDst.mFrameCount > 0 && outputFrame >= mRealDst.mFrameCount)
		outputFrame = mRealDst.mFrameCount - 1;

	vdrefptr<SamplingInfo> sampInfo(new SamplingInfo);
	sampInfo->mpCB = sampleCB;
	sampInfo->mpCBData = sampleCBData;
	sampInfo->handler = sampleHandler;
	sampInfo->result = 0;

	vdrefptr<VDFilterFrameRequest> r;

	mFrameQueueWaiting.CreateRequest(~r);

	VDFilterFrameRequestTiming timing;
	timing.mSourceFrame = GetSourceFrame(outputFrame);
	timing.mOutputFrame = outputFrame;
	r->SetTiming(timing);
	r->SetCacheable(false);

  if (sampleCB || sampleHandler)
	  r->SetExtraInfo(sampInfo);

	vdrefptr<VDFilterFrameBuffer> buf;

	VideoPrefetcher vp((uint32)mSources.size());

	GetPrefetchInfo(outputFrame, vp, false);

	if (!mSources.empty()) {
		r->SetSourceCount(vp.mSourceFrames.size());

		uint32 idx = 0;
		for(VideoPrefetcher::SourceFrames::const_iterator it(vp.mSourceFrames.begin()), itEnd(vp.mSourceFrames.end()); it != itEnd; ++it) {
			const VideoPrefetcher::PrefetchInfo& prefetchInfo = *it;
			bool writable = (idx == 0) && IsInPlace();

			vdrefptr<IVDFilterFrameClientRequest> srcreq;
			mSources[prefetchInfo.mSrcIndex]->CreateRequest(prefetchInfo.mFrame, writable, batchNumber, ~srcreq);

			srcreq->Start(NULL, prefetchInfo.mCookie, prefetchInfo.mSrcIndex);

			r->SetSourceRequest(idx++, srcreq);
		}
	}

	vdrefptr<IVDFilterFrameClientRequest> creq;
	r->CreateClient(false, ~creq);

	mFrameQueueWaiting.Add(r);

	*req = creq.release();
	return true;
}

FilterInstance::RunResult FilterInstance::RunRequests(const uint32 *batchNumberLimit, int index) {
	if (index>0) return kRunResult_Idle;
  
	if (mbRequestFramePending)
		return kRunResult_Blocked;

	bool activity = false;
	while(!mpRequestInProgress) {
		if (!mFrameQueueWaiting.GetNextRequest(batchNumberLimit, &mpRequestInProgress))
			return activity ? kRunResult_IdleWasActive : kRunResult_Idle;

		mFrameQueueInProgress.Add(mpRequestInProgress);
		OpenRequest();
		activity = true;

		// OpenRequest() will initiate a frame immediately when sampling is enabled.
		if (mbRequestFramePending)
			return kRunResult_Blocked;
	}

	AdvanceRequest();
	return kRunResult_Running;
}

FilterInstance::RunResult FilterInstance::RunProcess(int index) {
	if (index>0) return kRunResult_Idle;

	if (mbRequestFramePending) {
		VDPROFILEBEGINDYNAMICEX(mProfileCacheFilterName, filter->name, (uint32)mRequestCurrentFrame);
		RunFilter();
		VDPROFILEEND();

		mpEngine->Schedule();
		return kRunResult_BlockedWasActive;
	}

	return kRunResult_Blocked;
}

bool FilterInstance::OpenRequest() {
	VDFilterFrameRequest& req = *mpRequestInProgress;

	const SamplingInfo *samplingInfo = static_cast<const SamplingInfo *>(req.GetExtraInfo());

	if (samplingInfo) {
		if (!AllocateResultBuffer(req, 0)) {
			CloseRequest(false);
			return false;
		}

		if (!BeginFrame(req, 0, 0xffff, NULL)) {
			CloseRequest(false);
			return false;
		}

		mRequestStartFrame = 0;
		mRequestCurrentFrame = 1;
		mRequestEndFrame = 0;
		mbRequestFrameCompleted = false;
		mbRequestCacheIntermediateFrames = (samplingInfo->handler!=0);
		return true;
	}

	const VDPosition targetFrame = req.GetTiming().mOutputFrame;
	VDPosition startFrame = targetFrame;
	VDPosition endFrame = targetFrame + mLag;

	bool enableIntermediateFrameCaching = false;
	if (startFrame <= mLastResultFrame + 1 && mLastResultFrame < endFrame) {
		startFrame = mLastResultFrame + 1;
		enableIntermediateFrameCaching = true;
	} else {
		mbFirstFrame = true;
	}

	mRequestStartFrame = startFrame;
	mRequestEndFrame = endFrame;
	mRequestTargetFrame = targetFrame;
	mbRequestCacheIntermediateFrames = enableIntermediateFrameCaching;
	mRequestCurrentFrame = mRequestStartFrame;
	mbRequestFrameCompleted = false;

	return true;
}

void FilterInstance::CloseRequest(bool success) {
	if (!mpRequestInProgress)
		return;

	VDASSERT(!mRealDst.GetBuffer());
	VDASSERT(!mExternalDst.GetBuffer());

	mpRequestInProgress->MarkComplete(success);
	mFrameQueueInProgress.Remove(mpRequestInProgress);
	mpRequestInProgress->Release();
	mpRequestInProgress = NULL;
}

bool FilterInstance::AdvanceRequest() {
	VDFilterFrameRequest& req = *mpRequestInProgress;

	VDASSERT(!mbRequestFramePending);

	for(;;) {
		if (!mbRequestFrameCompleted) {
			if (mRequestCurrentFrame > mRequestEndFrame)
				break;

			const uint32 sourceOffset = (uint32)mRequestCurrentFrame - (uint32)mRequestTargetFrame;
			if (!AllocateResultBuffer(req, sourceOffset))
				return false;

			if (mLag) {
				VDPosition sourceFrame = req.GetSourceRequest(sourceOffset)->GetFrameNumber();
				VDPosition clampedOutputFrame = mRequestCurrentFrame;

				if (clampedOutputFrame >= mRealDst.mFrameCount && mRealDst.mFrameCount > 0)
					clampedOutputFrame = mRealDst.mFrameCount - 1;

				VDFilterFrameRequestTiming overrideTiming;
				overrideTiming.mSourceFrame = sourceFrame;
				overrideTiming.mOutputFrame = clampedOutputFrame;

				if (!BeginFrame(req, sourceOffset, 1, &overrideTiming)) {
					CloseRequest(false);
					return false;
				}

				return true;
			} else {
				if (!BeginFrame(req, 0, 0xffff, NULL)) {
					CloseRequest(false);
					return false;
				}
				return true;
			}
		} else {
			EndFrame();

			if (!mbRequestFrameSuccess) {
				CloseRequest(false);
				return false;
			}

			VDPosition resultFrameUnlagged = mRequestCurrentFrame - mLag;
			if (mRequestCurrentFrame != mRequestEndFrame) {
				if (mbRequestCacheIntermediateFrames && resultFrameUnlagged >= 0) {
					VDFilterFrameBuffer *buf = req.GetResultBuffer();
					mFrameCache.Add(buf, resultFrameUnlagged);
					mFrameQueueWaiting.CompleteRequests(resultFrameUnlagged, buf);
				} else {
					req.SetResultBuffer(NULL);
				}
			}

			++mRequestCurrentFrame;
			mbRequestFrameCompleted = false;
		}
	}

	if (!mpRequestInProgress->GetExtraInfo())
		mFrameCache.Add(req.GetResultBuffer(), mRequestTargetFrame);

	CloseRequest(true);

	VDASSERT(!mRealDst.GetBuffer());
	VDASSERT(!mExternalDst.GetBuffer());

	return true;
}

bool FilterInstance::AllocateResultBuffer(VDFilterFrameRequest& req, int srcIndex) {
	vdrefptr<VDFilterFrameBuffer> buf;
	bool stolen = false;

	if (mbCanStealSourceBuffer && req.GetSourceCount()) {
		IVDFilterFrameClientRequest *creqsrc0 = req.GetSourceRequest(srcIndex);

		if (!creqsrc0 || creqsrc0->IsResultBufferStealable()) {
			VDFilterFrameBuffer *srcbuf = req.GetSource(srcIndex);
			uint32 srcRefs = 1;

			if (creqsrc0)
				++srcRefs;

			if (srcbuf->Steal(srcRefs)) {
				buf = srcbuf;
				stolen = true;
			}
		}
	}

	// allocate a new result buffer if the source buffer wasn't stolen
	if (!stolen && !mResultAllocator.Allocate(~buf))
		return false;

	req.SetResultBuffer(buf);
	return true;
}

struct FilterInstance::RunMessage : public VDFilterAccelEngineMessage {
	FilterInstance *mpThis;
};

bool FilterInstance::BeginFrame(VDFilterFrameRequest& request, uint32 sourceOffset, uint32 sourceCountLimit, const VDFilterFrameRequestTiming *overrideTiming) {
	VDASSERT(!mbRequestFramePending);

	VDFilterFrameRequestError *logicError = mpLogicError;
	if (logicError) {
		request.SetError(logicError);
		return false;
	}

	const VDFilterFrameRequestTiming& timing = overrideTiming ? *overrideTiming : request.GetTiming();

	uint32 sourceCount = request.GetSourceCount();
	VDASSERT(sourceOffset <= sourceCount);
	sourceCount -= sourceOffset;
	if (sourceCount > sourceCountLimit)
		sourceCount = sourceCountLimit;

	IVDFilterFrameClientRequest *creqsrc0 = sourceCount ? request.GetSourceRequest(sourceOffset) : NULL;

	if (creqsrc0) {
		vdrefptr<VDFilterFrameRequestError> err(creqsrc0->GetError());

		if (err) {
			request.SetError(err);
			return false;
		}
	}

	VDFilterFrameBuffer *src0Buffer = request.GetSource(sourceOffset);

	VDFilterFrameBuffer *resultBuffer = request.GetResultBuffer();
	bool bltSrcOnEntry = false;

	if (mRealSrc.hdc || mbForceSingleFB) {
		mExternalSrcCropped.BindToFrameBuffer(src0Buffer, true);
		bltSrcOnEntry = true;
	} else if (!IsInPlace()) {
		if (src0Buffer)
			mRealSrc.BindToFrameBuffer(src0Buffer, true);
		else
			mRealSrc.Unbind();
	} else {
		if (!resultBuffer || resultBuffer == src0Buffer) {
			resultBuffer = src0Buffer;

			mRealSrc.BindToFrameBuffer(resultBuffer, false);
		} else {
			VDFilterFrameBuffer *buf = resultBuffer;
			mRealSrc.BindToFrameBuffer(buf, false);

			if (src0Buffer) {
				mExternalSrcCropped.BindToFrameBuffer(src0Buffer, true);

				bltSrcOnEntry = true;
			}
		}
	}

	if (creqsrc0) {
		mRealSrc.SetFrameNumber(creqsrc0->GetFrameNumber());
		mRealSrc.mCookie = creqsrc0->GetCookie();
		mRealSrc.mPixmap.info = creqsrc0->GetInfo();
	} else {
		mRealSrc.SetFrameNumber(-1);
		mRealSrc.mCookie = 0;
	}

	if (!mRealDst.hdc && !mbForceSingleFB)
		mRealDst.BindToFrameBuffer(resultBuffer, false);

	mRealDst.SetFrameNumber(timing.mOutputFrame);
	mExternalDst.BindToFrameBuffer(resultBuffer, false);

	mRealDst.mPixmap.info = mRealSrc.mPixmap.info;

	mSourceFrameArray.resize(sourceCount);
	mSourceFrames.resize(sourceCount);
	mpSourceFrames = mSourceFrameArray.data();
	mSourceFrameCount = sourceCount;

	if (sourceCount)
		mSourceFrameArray[0] = (VDXFBitmap *)&mRealSrc;

	for(uint32 i=1; i<sourceCount; ++i) {
		mSourceFrameArray[i] = (VDXFBitmap *)&mSourceFrames[i];

		IVDFilterFrameClientRequest *creqsrc = request.GetSourceRequest(sourceOffset + i);
		VDFilterFrameRequestError *localError = creqsrc->GetError();

		if (localError) {
			request.SetError(localError);
			return false;
		}

		VFBitmapInternal& bm = mSourceFrames[i];
		VDFilterFrameBuffer *fb = request.GetSource(sourceOffset + i);

		const uint32 srcIndex = creqsrc->GetSrcIndex();
		const VDFilterPrepareStreamInfo& streamInfo = mPrepareInfo.mStreams[srcIndex];

		bm.mPixmapLayout = streamInfo.mExternalSrcCropped.mPixmapLayout;
		bm.BindToFrameBuffer(fb, true);

		bm.mFrameCount = streamInfo.mExternalSrcCropped.mFrameCount;
		bm.mFrameRateLo = streamInfo.mExternalSrcCropped.mFrameRateLo;
		bm.mFrameRateHi = streamInfo.mExternalSrcCropped.mFrameRateHi;
		bm.SetFrameNumber(creqsrc->GetFrameNumber());
		bm.mCookie = creqsrc->GetCookie();
	}

	mLastResultFrame = -1;
	VDPosition sourceFrame = timing.mSourceFrame;
	VDPosition outputFrame = timing.mOutputFrame;

	// If the filter has a delay ring...
	DelayInfo di;

	di.mSourceFrame		= sourceFrame;
	di.mOutputFrame		= outputFrame;

	if (!mFsiDelayRing.empty()) {
		if (mbFirstFrame)
			std::fill(mFsiDelayRing.begin(), mFsiDelayRing.end(), di);

		DelayInfo diOut = mFsiDelayRing[mDelayRingPos];
		mFsiDelayRing[mDelayRingPos] = di;

		if (++mDelayRingPos >= mFsiDelayRing.size())
			mDelayRingPos = 0;

		sourceFrame		= diOut.mSourceFrame;
		outputFrame		= diOut.mOutputFrame;
	}

	// Update FilterStateInfo structure.
	mfsi.lCurrentSourceFrame	= VDClampToSint32(di.mSourceFrame);
	mfsi.lCurrentFrame			= VDClampToSint32(di.mOutputFrame);
	mfsi.lSourceFrameMS			= VDClampToSint32(VDRoundToInt64((double)di.mSourceFrame * (double)mRealSrc.mFrameRateLo / (double)mRealSrc.mFrameRateHi * 1000.0));
	mfsi.lDestFrameMS			= VDClampToSint32(VDRoundToInt64((double)di.mOutputFrame * (double)mRealDst.mFrameRateLo / (double)mRealDst.mFrameRateHi * 1000.0));
	mfsi.mOutputFrame			= VDClampToSint32(outputFrame);

	mRequestSourceFrame = sourceFrame;
	mRequestOutputFrame = outputFrame;

	if (mpVDXA)
		mbRequestBltSrcOnEntry = false;
	else
		mbRequestBltSrcOnEntry = bltSrcOnEntry;

	mbRequestFrameCompleted = false;
	mbRequestFramePending = true;
	mpEngine->ScheduleProcess(0);
	return true;
}

void FilterInstance::EndFrame() {
	if (!mbRequestFrameCompleted)
		return;

	VDASSERT(mpRequestInProgress);

	mbRequestFrameCompleted = false;

	if (mRequestError.gets()) {
		vdrefptr<VDFilterFrameRequestError> err(new_nothrow VDFilterFrameRequestError);

		if (err)
			err->mError.sprintf("Error processing frame %lld with filter '%s': %s", mRequestOutputFrame, filter->name, mRequestError.gets());

		mpRequestInProgress->SetError(err);

		mRequestError.clear();
	}

	if (mbRequestFrameSuccess)
		mLastResultFrame = mRequestCurrentFrame;

	uint32 sourceCount = mSourceFrames.size();
	for(uint32 i=1; i<sourceCount; ++i) {
		mSourceFrames[i].Unbind();
	}

	if (!mRealSrc.hdc && !mbForceSingleFB)
		mRealSrc.Unbind();

	mExternalSrcCropped.Unbind();

	if (!mRealDst.hdc && !mbForceSingleFB)
		mRealDst.Unbind();

	mExternalDst.Unbind();
}

void FilterInstance::RunFilter() {
	if (!mbRequestFramePending)
		return;

	mbRequestFrameSuccess = false;

	if (mbRequestFrameBeingProcessed.xchg(true)) {
#ifdef _DEBUG
		VDBREAK;
#endif
		return;
	}

	if (mpVDXA) {
		IVDTContext *tc = mpAccelEngine->GetContext();
		const uint32 counter = tc->GetDeviceLossCounter();
		bool deviceLost = false;

		if (tc->IsDeviceLost()) {
			deviceLost = true;
		} else {
			if (!ConnectAccelBuffers()) {
				mRequestError.assign("One or more source frames are no longer available.");
			} else {
				IVDTProfiler *vdtc = vdpoly_cast<IVDTProfiler *>(tc);
				if (vdtc)
					VDTBeginScopeF(vdtc, 0xe0ffe0, "Run filter '%ls'", mFilterName.c_str());
				
				try {
					RunFilterInner();
					mbRequestFrameSuccess = true;
				} catch(MyError& err) {
					mRequestError.TransferFrom(err);
				}

				if (vdtc)
					vdtc->EndScope();

				DisconnectAccelBuffers();
				mpAccelEngine->UpdateProfilingDisplay();

				if (tc->IsDeviceLost() || counter != tc->GetDeviceLossCounter())
					deviceLost = true;
			}
		}

		if (deviceLost)
			mRequestError.assign("The 3D accelerator is no longer available.");
	} else {
		try {
			RunFilterInner();
			mbRequestFrameSuccess = true;
		} catch(MyError& e) {
			mRequestError.TransferFrom(e);
		}
	}

	mbRequestFrameBeingProcessed = false;

	mbRequestFramePending = false;
	mbRequestFrameCompleted = true;

	mpEngine->Schedule();
}

void FilterInstance::RunFilterInner() {
	SamplingInfo *sampInfo = (SamplingInfo *)mpRequestInProgress->GetExtraInfo();
	VDXFilterPreviewSampleCallback const sampleCB = sampInfo ? sampInfo->mpCB : NULL;
	void *const sampleCBData = sampInfo ? sampInfo->mpCBData : NULL;
	IFilterModPreviewSample* sampleHandler = sampInfo ? sampInfo->handler : NULL;

	sint64 outputFrame = mRequestOutputFrame;
	VDASSERT(outputFrame >= 0);

	if (mbRequestBltSrcOnEntry && mRealSrc.mPixmap.data) {
		if (!mpSourceConversionBlitter)
			mpSourceConversionBlitter = VDPixmapCreateBlitter(mRealSrc.mPixmap, mExternalSrcCropped.mPixmap);

		mExternalSrcCropped.mPixmap.info = mRealSrc.mPixmap.info;
		mpSourceConversionBlitter->Blit(mRealSrc.mPixmap, mExternalSrcCropped.mPixmap);
	}

	// Compute alpha blending value.
	float alpha = 1.0f;

	VDParameterCurve *pAlphaCurve = GetAlphaParameterCurve();
	if (pAlphaCurve)
		alpha = (float)(*pAlphaCurve)((double)outputFrame).mY;

	// If this is an in-place filter with an alpha curve, save off the old image.
	bool skipFilter = false;
	bool skipBlit = false;

	const VDPixmap *blendSrc = NULL;

	if ((!sampleCB && !sampleHandler) && alpha < 254.5f / 255.0f && mRealSrc.mPixmap.data) {
		if (mFlags & FILTERPARAM_SWAP_BUFFERS) {
			if (alpha < 0.5f / 255.0f)
				skipFilter = true;

			blendSrc = &mRealSrc.mPixmap;
		} else {
			if (alpha < 0.5f / 255.0f) {
				skipFilter = true;

				if (mRealSrc.data == mRealDst.data && mRealSrc.pitch == mRealDst.pitch)
					skipBlit = true;
			}

			if (!skipBlit) {
				VDPixmapBlt(mBlendTemp.mPixmap, mRealSrc.mPixmap);
				blendSrc = &mBlendTemp.mPixmap;
			}
		}
	}

	if (!skipFilter) {
		VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);

		if (mpVDXA) {
			vdprotected1("running accelerated filter \"%s\"", const char *, filter->name) {
				VDFilterThreadContextSwapper autoSwap(&mThreadContext);

				filter->accelRunProc(AsVDXFilterActivation(), &g_VDFilterCallbacks);
			}
		} else {
			vdprotected1("running filter \"%s\"", const char *, filter->name) {
				VDFilterThreadContextSwapper autoSwap(&mThreadContext);

				if (sampleHandler) {
					sampInfo->result = sampleHandler->Run(AsVDXFilterActivation(), &g_VDFilterCallbacks);
				} else if (sampleCB) {
					sampleCB(&AsVDXFilterActivation()->src, mfsi.mOutputFrame, VDClampToSint32(mRealDst.mFrameCount), sampleCBData);
				} else {
					// Deliberately ignore the return code. It was supposed to be an error value,
					// but earlier versions didn't check it and logoaway returns true in some cases.
					filter->runProc(AsVDXFilterActivation(), &g_VDFilterCallbacks);
          if (view) view->SetImage(mRealDst.mPixmap);
          view = 0;
				}
			}
		}


		if (mRealDst.hdc || mbForceSingleFB) {
			if (mRealDst.hdc)
				::GdiFlush();

			VDPixmapBlt(mExternalDst.mPixmap, mRealDst.mPixmap);
		}
	}

	if ((!sampleCB && !sampleHandler) && !skipBlit && alpha < 254.5f / 255.0f) {
		if (alpha > 0.5f / 255.0f)
			VDPixmapBltAlphaConst(mRealDst.mPixmap, *blendSrc, 1.0f - alpha);
		else
			VDPixmapBlt(mRealDst.mPixmap, *blendSrc);
	}

	if (mFlags & FILTERPARAM_NEEDS_LAST)
		VDPixmapBlt(mRealLast.mPixmap, mRealSrc.mPixmap);

	VDFilterFrameBuffer* dstBuffer = mRealDst.GetBuffer();
	if (dstBuffer)
		dstBuffer->info = mRealDst.mPixmap.info;
	
	mbFirstFrame = false;
}

bool FilterInstance::ConnectAccelBuffers() {
	for(uint32 i=0; i<mSourceFrameCount; ++i) {
		if (!ConnectAccelBuffer((VFBitmapInternal *)mpSourceFrames[i], false)) {
			DisconnectAccelBuffers();
			return false;
		}
	}

	if (!ConnectAccelBuffer((VFBitmapInternal *)mpOutputFrames[0], true)) {
		DisconnectAccelBuffers();
		return false;
	}

	return true;
}

void FilterInstance::DisconnectAccelBuffers() {
	for(uint32 i=0; i<mSourceFrameCount; ++i)
		DisconnectAccelBuffer((VFBitmapInternal *)mpSourceFrames[i]);

	DisconnectAccelBuffer((VFBitmapInternal *)mpOutputFrames[0]);
}

bool FilterInstance::ConnectAccelBuffer(VFBitmapInternal *buf, bool bindAsRenderTarget) {
	VDFilterFrameBufferAccel *fbuf = vdpoly_cast<VDFilterFrameBufferAccel *>(buf->GetBuffer());
	if (!fbuf)
		return false;

	if (!mpAccelEngine->CommitBuffer(fbuf, bindAsRenderTarget))
		return false;

	IVDTTexture2D *tex = fbuf->GetTexture();
	if (!tex)
		return false;

	if (bindAsRenderTarget) {
		IVDTSurface *surf = tex->GetLevelSurface(0);

		buf->mVDXAHandle = mpAccelContext->RegisterRenderTarget(surf, buf->w, buf->h, fbuf->GetBorderWidth(), fbuf->GetBorderHeight());
		VDASSERT(buf->mVDXAHandle);
	} else {
		buf->mVDXAHandle = mpAccelContext->RegisterTexture(tex, buf->w, buf->h);
		VDASSERT(buf->mVDXAHandle);
	}

	return true;
}

void FilterInstance::DisconnectAccelBuffer(VFBitmapInternal *buf) {
	if (buf->mVDXAHandle) {
		mpAccelContext->DestroyObject(buf->mVDXAHandle);
		buf->mVDXAHandle = 0;
	}
}

void FilterInstance::InvalidateAllCachedFrames() {
	mFrameCache.InvalidateAllFrames();
	mLastResultFrame = -1;

	if (mbStarted && filter->eventProc) {
		vdprotected1("invalidating internal caches on filter \"%s\"", const char *, filter->name) {
			VDFilterThreadContext context;
			VDFilterThreadContextSwapper autoSwap(&context);

			filter->eventProc(AsVDXFilterActivation(), &g_VDFilterCallbacks, kVDXVFEvent_InvalidateCaches, NULL);
		}
	}
}

void FilterInstance::DumpStatus(VDTextOutputStream& os) {
	os.FormatLine("Filter \"%s\":", filter->name);
	os.PutLine("  Pending queue:");
	mFrameQueueWaiting.DumpStatus(os);
	os.PutLine();
}

bool FilterInstance::GetScriptString(VDStringA& buf) {
	buf.clear();

	if (!filter->fssProc)
		return false;

	char tbuf[4096];
	tbuf[0] = 0;

	VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);
	vdprotected1("querying filter \"%s\" for script string", const char *, filter->name) {
		VDFilterThreadContext context;
		VDFilterThreadContextSwapper autoSwap(&context);

		if (!filter->fssProc(AsVDXFilterActivation(), &g_VDFilterCallbacks, tbuf, sizeof tbuf))
			return false;
	}

	tbuf[4095] = 0;

	buf = tbuf;
	return true;
}

bool FilterInstance::GetSettingsString(VDStringA& buf) const {
	buf.clear();

	if (!filter->stringProc && !filter->stringProc2)
		return false;

	char tbuf[2048];
	tbuf[0] = 0;

	VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);
	vdprotected1("querying filter \"%s\" for settings string", const char *, filter->name) {
		VDFilterThreadContext context;
		VDFilterThreadContextSwapper autoSwap(&context);

		if (filter->stringProc2)
			filter->stringProc2(AsVDXFilterActivation(), &g_VDFilterCallbacks, tbuf, sizeof tbuf);
		else
			filter->stringProc(AsVDXFilterActivation(), &g_VDFilterCallbacks, tbuf);
	}

	tbuf[2047] = 0;

	buf = tbuf;
	return true;
}

sint64 FilterInstance::GetSourceFrame(sint64 frame) {
	VideoPrefetcher vp((uint32)mSources.size());
	GetPrefetchInfo(frame, vp, true);
	vp.Finalize();

	if (vp.mSymbolicFrame >= 0)
		return vp.mSymbolicFrame;

	return vp.mSourceFrames[0].mFrame;
}

sint64 FilterInstance::GetSymbolicFrame(sint64 outputFrame, IVDFilterFrameSource *source) {
	if (source == this)
		return outputFrame;

	VideoPrefetcher vp((uint32)mSources.size());
	GetPrefetchInfo(outputFrame, vp, true);
	vp.Finalize();

	if (vp.mSymbolicFrame >= 0)
		return mSources[vp.mSymbolicFrameSrcIndex]->GetSymbolicFrame(vp.mSymbolicFrame, source);

	return -1;
}

bool FilterInstance::IsFiltered(sint64 outputFrame) const {
	return !IsFadedOut(outputFrame);
}

bool FilterInstance::IsFadedOut(sint64 outputFrame) const {
	if (!mpAlphaCurve)
		return false;

	float alpha = (float)(*mpAlphaCurve)((double)outputFrame).mY;

	return (alpha < (0.5f / 255.0f));
}

bool FilterInstance::IsTerminal() const {
	return (mFilterModFlags & FILTERMODPARAM_TERMINAL)!=0;
}

bool FilterInstance::GetDirectMapping(sint64 outputFrame, sint64& sourceFrame, int& sourceIndex) {
	VideoPrefetcher prefetcher((uint32)mSources.size());
	GetPrefetchInfo(outputFrame, prefetcher, false);
	prefetcher.Finalize();

	// If the filter directly specifies a direct mapping, use it.
	if (prefetcher.mDirectFrame >= 0)
		return mSources[prefetcher.mDirectFrameSrcIndex]->GetDirectMapping(prefetcher.mDirectFrame, sourceFrame, sourceIndex);

	// The filter doesn't have a direct mapping. If it doesn't pull from any source frames,
	// then assume it is not mappable.
	if (prefetcher.mSourceFrames.empty())
		return false;

	// If the filter isn't faded out or we don't have the info needed to determine that, bail.
	if (!IsFadedOut(outputFrame))
		return false;

	// Filter's faded out, so assume we're going to map to the first source frame.
	return mSources[prefetcher.mSourceFrames[0].mSrcIndex]->GetDirectMapping(prefetcher.mSourceFrames[0].mFrame, sourceFrame, sourceIndex);
}

sint64 FilterInstance::GetNearestUniqueFrame(sint64 outputFrame) {
	if (!(mFlags & FILTERPARAM_PURE_TRANSFORM))
		return outputFrame;

	VideoPrefetcher prefetcher((uint32)mSources.size());
	GetPrefetchInfo(outputFrame, prefetcher, false);
	prefetcher.TransformToNearestUnique(mSources);

	VideoPrefetcher prefetcher2((uint32)mSources.size());
	while(outputFrame >= 0) {
		prefetcher2.Clear();
		GetPrefetchInfo(outputFrame - 1, prefetcher2, false);
		prefetcher2.TransformToNearestUnique(mSources);

		if (prefetcher != prefetcher2)
			break;

		--outputFrame;
	}

	return outputFrame;
}

const VDScriptObject *FilterInstance::GetScriptObject() const {
	return filter->script_obj ? &mScriptWrapper : NULL;
}

void FilterInstance::GetPrefetchInfo(sint64 outputFrame, VideoPrefetcher& prefetcher, bool requireOutput) const {
	if (filter->prefetchProc2) {
		bool handled = false;

		VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);
		vdprotected1("prefetching filter \"%s\"", const char *, filter->name) {
			handled = filter->prefetchProc2(AsVDXFilterActivation(), &g_VDFilterCallbacks, outputFrame, &prefetcher);
		}

		if (handled) {
			if (prefetcher.mpError)
				SetLogicErrorF("%s", prefetcher.mpError);

			if (!requireOutput || !prefetcher.mSourceFrames.empty() || prefetcher.mDirectFrame >= 0)
				return;
		}
	} else if (filter->prefetchProc) {
		sint64 inputFrame;

		VDExternalCodeBracket bracket(mFilterName.c_str(), __FILE__, __LINE__);
		vdprotected1("prefetching filter \"%s\"", const char *, filter->name) {
			inputFrame = filter->prefetchProc(AsVDXFilterActivation(), &g_VDFilterCallbacks, outputFrame);
		}

		prefetcher.PrefetchFrame(0, inputFrame, 0);
		return;
	}

	double factor = ((double)mRealSrc.mFrameRateHi * (double)mRealDst.mFrameRateLo) / ((double)mRealSrc.mFrameRateLo * (double)mRealDst.mFrameRateHi);

	prefetcher.PrefetchFrame(0, VDFloorToInt64((outputFrame + 0.5f) * factor), 0);
}

void FilterInstance::SetLogicError(const char *s) const {
	if (mpLogicError)
		return;

	vdrefptr<VDFilterFrameRequestError> error(new_nothrow VDFilterFrameRequestError);

	if (error) {
		error->mError.sprintf("A logic error was detected in filter '%s': %s", filter->name, s);

		VDFilterFrameRequestError *oldError = mpLogicError.compareExchange(error, NULL);
		
		if (!oldError)
			error.release();
	}
}

void FilterInstance::SetLogicErrorF(const char *format, ...) const {
	if (mpLogicError)
		return;

	vdrefptr<VDFilterFrameRequestError> error(new_nothrow VDFilterFrameRequestError);

	if (error) {
		error->mError.sprintf("A logic error was detected in filter '%s': ", filter->name);

		va_list val;
		va_start(val, format);
		error->mError.append_vsprintf(format, val);
		va_end(val);

		VDFilterFrameRequestError *oldError = mpLogicError.compareExchange(error, NULL);
		
		if (!oldError)
			error.release();
	}
}

void FilterInstance::ConvertParameters(CScriptValue *dst, const VDScriptValue *src, int argc) {
	int idx = 0;
	while(argc-->0) {
		const VDScriptValue& v = *src++;

		switch(v.type) {
			case VDScriptValue::T_INT:
				*dst = CScriptValue(v.asInt());
				break;
			case VDScriptValue::T_STR:
				*dst = CScriptValue(v.asString());
				break;
			case VDScriptValue::T_LONG:
				*dst = CScriptValue(v.asLong());
				break;
			case VDScriptValue::T_DOUBLE:
				*dst = CScriptValue(v.asDouble());
				break;
			case VDScriptValue::T_VOID:
				*dst = CScriptValue();
				break;
			default:
				throw MyError("Script: Parameter %d is not of a supported type for filter configuration functions.");
				break;
		}

		++dst;
		++idx;
	}
}

void FilterInstance::ConvertValue(VDScriptValue& dst, const CScriptValue& v) {
	switch(v.type) {
		case VDScriptValue::T_INT:
			dst = VDScriptValue(v.asInt());
			break;
		case VDScriptValue::T_STR:
			dst = VDScriptValue(v.asString());
			break;
		case VDScriptValue::T_LONG:
			dst = VDScriptValue(v.asLong());
			break;
		case VDScriptValue::T_DOUBLE:
			dst = VDScriptValue(v.asDouble());
			break;
		case VDScriptValue::T_VOID:
		default:
			dst = VDScriptValue();
			break;
	}
}

namespace {
	class VDScriptInterpreterAdapter : public IScriptInterpreter{
	public:
		VDScriptInterpreterAdapter(IVDScriptInterpreter *p) : mpInterp(p) {}

		void ScriptError(int e) {
			mpInterp->ScriptError(e);
		}

		char** AllocTempString(long l) {
			return mpInterp->AllocTempString(l);
		}

	protected:
		IVDScriptInterpreter *mpInterp;
	};
}

void FilterInstance::ScriptFunctionThunkVoid(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDFilterChainEntry *ent = (VDFilterChainEntry *)argv[-1].asObjectPtr();
	FilterInstance *const thisPtr = ent->mpInstance;
	const int funcidx = thisPtr->mScriptWrapper.GetFuncIndex(isi->GetCurrentMethod());

	const ScriptFunctionDef& fd = thisPtr->filter->script_obj->func_list[funcidx];
	VDXScriptVoidFunctionPtr pf = (VDXScriptVoidFunctionPtr)fd.func_ptr;

	std::vector<CScriptValue> params(argc ? argc : 1);

	ConvertParameters(&params[0], argv, argc);

	VDScriptInterpreterAdapter adapt(isi);
	pf(&adapt, thisPtr->AsVDXFilterActivation(), &params[0], argc);
}

void FilterInstance::ScriptFunctionThunkInt(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDFilterChainEntry *ent = (VDFilterChainEntry *)argv[-1].asObjectPtr();
	FilterInstance *const thisPtr = ent->mpInstance;
	const int funcidx = thisPtr->mScriptWrapper.GetFuncIndex(isi->GetCurrentMethod());

	const ScriptFunctionDef& fd = thisPtr->filter->script_obj->func_list[funcidx];
	VDXScriptIntFunctionPtr pf = (VDXScriptIntFunctionPtr)fd.func_ptr;

	std::vector<CScriptValue> params(argc ? argc : 1);

	ConvertParameters(&params[0], argv, argc);

	VDScriptInterpreterAdapter adapt(isi);
	int rval = pf(&adapt, thisPtr->AsVDXFilterActivation(), &params[0], argc);

	argv[0] = rval;
}

void FilterInstance::ScriptFunctionThunkVariadic(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	VDFilterChainEntry *ent = (VDFilterChainEntry *)argv[-1].asObjectPtr();
	FilterInstance *const thisPtr = ent->mpInstance;
	const int funcidx = thisPtr->mScriptWrapper.GetFuncIndex(isi->GetCurrentMethod());

	const ScriptFunctionDef& fd = thisPtr->filter->script_obj->func_list[funcidx];
	ScriptFunctionPtr pf = fd.func_ptr;

	std::vector<CScriptValue> params(argc ? argc : 1);

	ConvertParameters(&params[0], argv, argc);

	VDScriptInterpreterAdapter adapt(isi);
	CScriptValue v(pf(&adapt, thisPtr->AsVDXFilterActivation(), &params[0], argc));

	ConvertValue(argv[0], v);
}

void FilterInstance::CheckValidConfiguration() {
	VDASSERT(!mRealDst.GetBuffer());
	VDASSERT(!mExternalDst.GetBuffer());

	if (GetInvalidFormatHandlingState())
		throw MyError("Cannot start filters: Filter \"%s\" is not handling image formats correctly.",
			GetName());

	if (mRealDst.w < 1 || mRealDst.h < 1)
		throw MyError("Cannot start filter chain: The output of filter \"%s\" is smaller than 1x1.", GetName());


	if (GetAlphaParameterCurve()) {
		// size check
		if (mRealSrc.w != mRealDst.w || mRealSrc.h != mRealDst.h) {
			throw MyError("Cannot start filter chain: Filter \"%s\" has a blend curve attached and has differing input and output sizes (%dx%d -> %dx%d). Input and output sizes must match."
				, GetName()
				, mRealSrc.w
				, mRealSrc.h
				, mRealDst.w
				, mRealDst.h
				);
		}

		// format check
		if (mRealSrc.mPixmapLayout.format != mRealDst.mPixmapLayout.format) {
			throw MyError("Cannot start filter chain: Filter \"%s\" has a blend curve attached and has differing input and output formats. Input and output formats must match."
				, GetName());
		}
	}
}

void FilterInstance::InitSharedBuffers(VDFixedLinearAllocator& lastFrameAlloc) {
	mExternalDst = mRealDst;

	VDFileMappingW32 *mapping = NULL;
	if (((mRealSrc.dwFlags | mRealDst.dwFlags) & VFBitmapInternal::NEEDS_HDC) && !(GetFlags() & FILTERPARAM_SWAP_BUFFERS)) {
		uint32 mapSize = std::max<uint32>(VDPixmapLayoutGetMinSize(mRealSrc.mPixmapLayout), VDPixmapLayoutGetMinSize(mRealDst.mPixmapLayout));

		if (!mFileMapping.Init(mapSize))
			throw MyMemoryError();

		mapping = &mFileMapping;
	}

	mRealSrc.hdc = NULL;
	if (mapping || (mRealSrc.dwFlags & VDXFBitmap::NEEDS_HDC))
		mRealSrc.BindToDIBSection(mapping);

	mRealDst.hdc = NULL;
	if (mapping || (mRealDst.dwFlags & VDXFBitmap::NEEDS_HDC))
		mRealDst.BindToDIBSection(mapping);

	mRealLast.hdc = NULL;
	if (GetFlags() & FILTERPARAM_NEEDS_LAST) {
		mRealLast.Fixup(lastFrameAlloc.Allocate(mRealLast.size + mRealLast.offset));

		if (mRealLast.dwFlags & VDXFBitmap::NEEDS_HDC)
			mRealLast.BindToDIBSection(NULL);
	}
}

FilterModProject::FilterModProject(const FilterModProject& a) {
	inst = 0;
	dataPrefix = a.dataPrefix;
	{for(Data* const* p=a.data.begin(); p<a.data.end(); p++){
		Data* d = new Data(**p);
		data.push_back(d);
	}}
}

FilterModProject::~FilterModProject() {
	{for(Data** p=data.begin(); p<data.end(); p++){
		delete *p;
	}}
}

bool FilterModProject::GetData(void* buf, size_t* buf_size, const wchar_t* id) {
	Data* d = 0;
	{for(Data** p=data.begin(); p<data.end(); p++){
		if((*p)->id==id) {
			d = *p;
			break;
		}
	}}

	if (!d) {
		if (dataPrefix.empty() || g_project->mProjectFilename.empty() || g_project->mProjectSubdir.empty()) {
			*buf_size = 0;
			return true;
		}

		d = new Data;
		d->id = id;
		data.push_back(d);
	}

	if (d->read_dirty) {
		VDStringW name = VDFileSplitPathLeft(g_project->mProjectFilename);
		name += g_project->mProjectSubdir;
		name += L"\\.";
		name += VDTextU8ToW(dataPrefix);
		name += L".";
		name += id;
		VDFile file;
		if (file.openNT(name.c_str())) {
			sint64 size = file.size();
			d->data.resize((size_t)size);
			file.read(d->data.begin(),(long)size);
		} else {
			d->data.clear();
		}
		d->read_dirty = false;
	}

	if(d->data.empty()) {
		*buf_size = 0;
		return true;
	}

	size_t size = d->data.size();
	if (!buf || *buf_size<size) {
		*buf_size = size;
		return false;
	}

	*buf_size = size;
	memcpy(buf,d->data.begin(),size);
	return true;
}

bool FilterModProject::SetData(const void* buf, const size_t buf_size, const wchar_t* id) {
	Data* d = 0;
	{for(Data** p=data.begin(); p<data.end(); p++){
		if((*p)->id==id) {
			d = *p;
			break;
		}
	}}

	if (!d) {
		d = new Data;
		d->id = id;
		data.push_back(d);
	}

	d->data.resize(buf_size);
	memcpy(d->data.begin(),buf,buf_size);
	d->read_dirty = false;
	d->write_dirty = true;

	return true;
}

bool FilterModProject::GetProjectData(void* buf, size_t* buf_size, const wchar_t* id) {
	*buf_size = 0;
	return false;
}

bool FilterModProject::SetProjectData(const void* buf, const size_t buf_size, const wchar_t* id) {
	return false;
}

bool FilterModProject::GetDataDir(wchar_t* buf, size_t* buf_size) {
	if (g_project->mProjectFilename.empty() || g_project->mProjectSubdir.empty()) {
		*buf_size = 0;
		return true;
	}

	VDStringW data = VDFileSplitPathLeft(g_project->mProjectFilename);
	data += g_project->mProjectSubdir;
	size_t size = (data.length()+1)*2;
	if (!buf || *buf_size<size) {
		*buf_size = size;
		return false;
	}

	*buf_size = size;
	memcpy(buf,data.c_str(),size);
	return true;
}

bool FilterModProject::GetProjectDir(wchar_t* buf, size_t* buf_size) {
	if (g_project->mProjectFilename.empty()) {
		*buf_size = 0;
		return true;
	}

	VDStringW base = VDFileSplitPathLeft(g_project->mProjectFilename);
	size_t size = (base.length()+1)*2;
	if (!buf || *buf_size<size) {
		*buf_size = size;
		return false;
	}

	*buf_size = size;
	memcpy(buf,base.c_str(),size);
	return true;
}
