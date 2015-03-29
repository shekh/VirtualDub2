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

#ifndef f_FILTERINSTANCE_H
#define f_FILTERINSTANCE_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/fraction.h>
#include <vd2/system/list.h>
#include <vd2/system/VDString.h>
#include <vd2/system/refcount.h>
#include <vd2/system/unknown.h>
#include <vd2/system/profile.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/blitter.h>
#include <vd2/VDLib/win32/DIBSection.h>
#include <vd2/VDLib/win32/FileMapping.h>
#include <vd2/VDLib/ParameterCurve.h>
#include <vd2/plugin/vdvideofilt.h>
#include "VBitmap.h"
#include "ScriptValue.h"
#include "FilterFrameQueue.h"
#include "FilterFrameAllocatorProxy.h"
#include "FilterFrameCache.h"
#include "FilterFrameSharingPredictor.h"

//////////////////

class IVDVideoDisplay;
class IVDPositionControl;
struct VDWaveFormat;
class VDTimeline;
struct VDXWaveFormat;
struct VDXFilterDefinition;

class VDScriptValue;
class IVDScriptInterpreter;

class FilterDefinitionInstance;

class VDFilterFrameBuffer;
class VDFilterFrameRequest;
class IVDFilterFrameClientRequest;

class VDFilterFrameAllocatorManager;

class VDFilterAccelContext;
class VDFilterAccelEngine;
class VDFilterAccelEngineDispatchQueue;
class VDFilterAccelEngineMessage;

class VDTextOutputStream;
class VDFixedLinearAllocator;

///////////////////

struct VDFilterStreamDesc {
	VDPixmapLayout	mLayout;
	VDFraction		mAspectRatio;
	VDFraction		mFrameRate;
	sint64			mFrameCount;
};

class VFBitmapInternal : public VBitmap {
public:
	VFBitmapInternal();
	VFBitmapInternal(const VFBitmapInternal&);
	~VFBitmapInternal();

	VFBitmapInternal& operator=(const VFBitmapInternal&);

	VDFilterFrameBuffer *GetBuffer() const { return mpBuffer; }

	void Unbind();
	void Fixup(void *base);
	void ConvertBitmapLayoutToPixmapLayout();
	void ConvertPixmapLayoutToBitmapLayout();
	void ConvertPixmapToBitmap();
	void BindToDIBSection(const VDFileMappingW32 *mapping);
	void BindToFrameBuffer(VDFilterFrameBuffer *buffer, bool readOnly);
	void CopyNullBufferParams();

	void SetFrameNumber(sint64 frame);

	VDFilterStreamDesc GetStreamDesc() const;

public:
	// Must match layout of VFBitmap!
	enum {
		NEEDS_HDC		= 0x00000001L,
	};

	uint32	dwFlags;
	VDXHDC	hdc;

	uint32	mFrameRateHi;
	uint32	mFrameRateLo;
	sint64	mFrameCount;

	VDXPixmapLayout	*mpPixmapLayout;
	const VDXPixmap	*mpPixmap;

	uint32	mAspectRatioHi;
	uint32	mAspectRatioLo;

	sint64	mFrameNumber;				///< Current frame number (zero based).
	sint64	mFrameTimestampStart;		///< Starting timestamp of frame, in 100ns units.
	sint64	mFrameTimestampEnd;			///< Ending timestamp of frame, in 100ns units.
	sint64	mCookie;					///< Cookie supplied when frame was requested.

	uint32	mVDXAHandle;				///< Acceleration handle to be used with VDXA routines.

	uint32	mBorderWidth;
	uint32	mBorderHeight;

public:
	VDDIBSectionW32	mDIBSection;
	VDPixmap		mPixmap;
	VDPixmapLayout	mPixmapLayout;

protected:
	VDFilterFrameBuffer	*mpBuffer;
};

class FilterModPixmap: public IFilterModPixmap {
public:
	virtual FilterModPixmapInfo* GetPixmapInfo(VDXPixmap* pixmap) {
		return &((VDPixmap*)pixmap)->info;
	}
};

class FilterInstanceAutoDeinit;

struct VDFilterThreadContext {
	int					tmp[16];
	void				*ESPsave;
};

class VDFilterActivationImpl {		// clone of VDXFilterActivation
	VDFilterActivationImpl& operator=(const VDFilterActivationImpl&);
public:
	VDFilterActivationImpl();
	VDFilterActivationImpl(const VDFilterActivationImpl&);

	const VDXFilterDefinition *filter;
	void *filter_data;
	VDXFBitmap&	dst;
	VDXFBitmap&	src;
	VDXFBitmap	*_reserved0;
	VDXFBitmap	*const last;
	uint32		x1;
	uint32		y1;
	uint32		x2;
	uint32		y2;

	VDXFilterStateInfo	*pfsi;
	IVDXFilterPreview	*ifp;
	IVDXFilterPreview2	*ifp2;			// (V11+)

	uint32		mSourceFrameCount;
	VDXFBitmap *const *mpSourceFrames;	// (V14+)
	VDXFBitmap *const *mpOutputFrames;	// (V14+)

	IVDXAContext	*mpVDXA;			// (V15+)

	uint32		mSourceStreamCount;		// (V16+)
	VDXFBitmap *const *mpSourceStreams;	// (V16+)

private:
	char mSizeCheckSentinel;

	VDXFBitmap *mOutputFrameArray[1];

protected:
	void SetSourceStreamCount(uint32 n);

	VFBitmapInternal	mRealSrc;
	VFBitmapInternal	mRealDst;
	VFBitmapInternal	mRealLast;

	typedef vdfastvector<VDXFBitmap *> SourceStreamPtrArray;
	typedef vdvector<VFBitmapInternal> SourceStreamArray;

	SourceStreamPtrArray	mSourceStreamPtrArray;
	SourceStreamArray		mSourceStreamArray;

	FilterModActivation fma;
};

class VDFilterScriptWrapper : public VDScriptObject {
	VDFilterScriptWrapper& operator=(const VDFilterScriptWrapper&);
public:
	VDFilterScriptWrapper();
	VDFilterScriptWrapper(const VDFilterScriptWrapper&);

	void Init(const VDXScriptObject *src, VDScriptFunction voidfunc, VDScriptFunction intfunc, VDScriptFunction varfunc);

	int GetFuncIndex(const VDScriptFunctionDef *fdef) const;

protected:
	vdfastvector<VDScriptFunctionDef>	mFuncList;
};

class IVDFilterFrameEngine {
public:
	virtual void Schedule() = 0;
	virtual void ScheduleProcess() = 0;
};

class IVDFilterFrameSource : public IVDRefUnknown {
public:
	virtual const char *GetDebugDesc() const = 0;

	virtual void RegisterSourceAllocReqs(uint32 index, VDFilterFrameAllocatorProxy *prev) = 0;
	virtual void RegisterAllocatorProxies(VDFilterFrameAllocatorManager *manager) = 0;
	virtual VDFilterFrameAllocatorProxy *GetOutputAllocatorProxy() = 0;

	virtual bool IsAccelerated() const = 0;

	virtual bool CreateRequest(sint64 outputFrame, bool writable, uint32 batchNumber, IVDFilterFrameClientRequest **req) = 0;
	virtual bool GetDirectMapping(sint64 outputFrame, sint64& sourceFrame, int& sourceIndex) = 0;
	virtual sint64 GetSourceFrame(sint64 outputFrame) = 0;
	virtual sint64 GetSymbolicFrame(sint64 outputFrame, IVDFilterFrameSource *source) = 0;
	virtual sint64 GetNearestUniqueFrame(sint64 outputFrame) = 0;
	virtual const VDPixmapLayout& GetOutputLayout() = 0;

	virtual void InvalidateAllCachedFrames() = 0;

	virtual void DumpStatus(VDTextOutputStream& os) = 0;

	virtual void Start(IVDFilterFrameEngine *engine) = 0;
	virtual void Stop() = 0;

	enum RunResult {
		kRunResult_Idle,
		kRunResult_IdleWasActive,
		kRunResult_Running,
		kRunResult_Blocked,
		kRunResult_BlockedWasActive,
		kRunResult_BatchLimited,
		kRunResult_BatchLimitedWasActive
	};

	virtual RunResult RunRequests(const uint32 *batchNumberLimit) = 0;
	virtual RunResult RunProcess() = 0;
};

class VDFilterConfiguration {
public:
	VDFilterConfiguration();

	bool	IsEnabled() const { return mbEnabled; }
	void	SetEnabled(bool enabled) { mbEnabled = enabled; }

	bool	IsForceSingleFBEnabled() const { return mbForceSingleFB; }
	void	SetForceSingleFBEnabled(bool en) { mbForceSingleFB = en; }

	bool	IsCroppingEnabled() const;
	bool	IsPreciseCroppingEnabled() const;
	vdrect32 GetCropInsets() const;
	void	SetCrop(int x1, int y1, int x2, int y2, bool precise);

	VDParameterCurve *GetAlphaParameterCurve() const { return mpAlphaCurve; }
	void SetAlphaParameterCurve(VDParameterCurve *p) { mpAlphaCurve = p; }

protected:
	bool	mbEnabled;
	bool	mbForceSingleFB;
	bool	mbPreciseCrop;
	int		mCropX1;
	int		mCropY1;
	int		mCropX2;
	int		mCropY2;

	vdrefptr<VDParameterCurve> mpAlphaCurve;
};

struct VDFilterPrepareStreamInfo {
	VFBitmapInternal	mExternalSrc;			// [prepare only] post convert (incoming)
	VFBitmapInternal	mExternalSrcPreAlign;	// [prepare only] cropped
	VFBitmapInternal	mExternalSrcCropped;	// [prepare only] post-align
	bool	mbAlignOnEntry;
};

struct VDFilterPrepareInfo {
	typedef vdvector<VDFilterPrepareStreamInfo> Streams;
	Streams mStreams;

	uint32	mLastFrameSizeRequired;
};

struct VDFilterPrepareStreamInfo2 {
	bool	mbConvertOnEntry;
};

struct VDFilterPrepareInfo2 {
	typedef vdfastvector<VDFilterPrepareStreamInfo2> Streams;
	Streams mStreams;
};

class FilterInstance : protected VDFilterActivationImpl, public VDFilterConfiguration, public vdrefcounted<IVDFilterFrameSource> {
	FilterInstance& operator=(const FilterInstance&);		// outlaw copy assignment

public:
	enum { kTypeID = 'vfin' };

	FilterInstance(const FilterInstance& fi);
	FilterInstance(FilterDefinitionInstance *);

	void *AsInterface(uint32 iid);

	FilterInstance *Clone();

	bool	IsInPlace() const;
	bool	IsAcceleratable() const;
	bool	IsAccelerated() const { return mbAccelerated; }

	const char *GetName() const;
	const VDXFilterDefinition *GetDefinition() const;
	uint32	GetFlags() const { return mFlags; }
	uint32	GetFrameDelay() const { return mFlags >> 16; }

	bool	GetInvalidFormatState() const { return mbInvalidFormat; }
	bool	GetInvalidFormatHandlingState() const { return mbInvalidFormatHandling; }
	bool	GetExcessiveFrameSizeState() const { return mbExcessiveFrameSize; }

	bool	IsConfigurable() const;
	bool	Configure(VDXHWND parent, IVDXFilterPreview2 *ifp2, IFilterModPreview *ifmpreview);

	void	PrepareReset();
	uint32	Prepare(const VFBitmapInternal *inputs, uint32 numInputs, VDFilterPrepareInfo& prepareInfo);

	void	SetEngine(IVDFilterFrameEngine *engine);
	void	Start(IVDFilterFrameEngine *engine);
	void	Start(uint32 flags, IVDFilterFrameSource *const *pSources, IVDFilterFrameEngine *engine, VDFilterAccelEngine *accelEngine);
	void	Stop();

	const char *GetDebugDesc() const;

	VDFilterFrameAllocatorProxy *GetOutputAllocatorProxy();
	void	RegisterSourceAllocReqs(uint32 index, VDFilterFrameAllocatorProxy *prev);
	void	RegisterAllocatorProxies(VDFilterFrameAllocatorManager *manager);
	bool	CreateRequest(sint64 outputFrame, bool writable, uint32 batchNumber, IVDFilterFrameClientRequest **req);
	bool	CreateSamplingRequest(sint64 outputFrame, VDXFilterPreviewSampleCallback sampleCB, void *sampleCBData, uint32 batchNumber, IVDFilterFrameClientRequest **req);
	RunResult RunRequests(const uint32 *batchNumberLimit);
	RunResult RunProcess();

	void	InvalidateAllCachedFrames();

	void	DumpStatus(VDTextOutputStream& os);

	bool	GetScriptString(VDStringA& buf);
	bool	GetSettingsString(VDStringA& buf) const;

	sint64	GetSourceFrame(sint64 frame);
	sint64	GetSymbolicFrame(sint64 outputFrame, IVDFilterFrameSource *source);

	bool	IsFiltered(sint64 outputFrame) const;
	bool	IsFadedOut(sint64 outputFrame) const;
	bool  IsTerminal() const;
	bool	GetDirectMapping(sint64 outputFrame, sint64& sourceFrame, int& sourceIndex);
	sint64	GetNearestUniqueFrame(sint64 outputFrame);

	const VDScriptObject *GetScriptObject() const;

	IVDFilterFrameSource *GetSource(uint32 index) const { return index < mSources.size() ? mSources[index] : (IVDFilterFrameSource *)NULL; }

	VDFilterStreamDesc GetSourceDesc() const { return mRealSrc.GetStreamDesc(); }

	VDFilterStreamDesc GetOutputDesc() const { return mRealDst.GetStreamDesc(); }
	uint32		GetOutputFrameWidth()	const { return mRealDst.w; }
	uint32		GetOutputFrameHeight()	const { return mRealDst.h; }
	VDFraction	GetOutputFrameRate()	const { return VDFraction(mRealDst.mFrameRateHi, mRealDst.mFrameRateLo); }
	sint64		GetOutputFrameCount()	const { return mRealDst.mFrameCount; }
	const VDPixmapLayout& GetOutputLayout() { return mRealDst.mPixmapLayout; }

	sint64		GetLastSourceFrame()	const { return mfsi.lCurrentSourceFrame; }
	sint64		GetLastOutputFrame()	const { return mfsi.lCurrentFrame; }

protected:
	class SamplingInfo;
	struct StopStartMessage;

	~FilterInstance();

	const VDXFilterActivation *AsVDXFilterActivation() const { return (const VDXFilterActivation *)static_cast<const VDFilterActivationImpl *>(this); }
	VDXFilterActivation *AsVDXFilterActivation() { return (VDXFilterActivation *)static_cast<VDFilterActivationImpl *>(this); }

	class VideoPrefetcher;
	void GetPrefetchInfo(sint64 frame, VideoPrefetcher& prefetcher, bool requireOutput) const;

	void SetLogicError(const char *s) const;
	void SetLogicErrorF(const char *format, ...) const;

	static void ConvertParameters(VDXScriptValue *dst, const VDScriptValue *src, int argc);
	static void ConvertValue(VDScriptValue& dst, const VDXScriptValue& src);
	static void ScriptFunctionThunkVoid(IVDScriptInterpreter *, VDScriptValue *, int);
	static void ScriptFunctionThunkInt(IVDScriptInterpreter *, VDScriptValue *, int);
	static void ScriptFunctionThunkVariadic(IVDScriptInterpreter *, VDScriptValue *, int);

	bool	OpenRequest();
	void	CloseRequest(bool success);
	bool	AdvanceRequest();
	bool	BeginFrame(VDFilterFrameRequest& request, uint32 sourceOffset, uint32 sourceCountLimit, const VDFilterFrameRequestTiming *overrideTiming);
	void	EndFrame();

	bool	AllocateResultBuffer(VDFilterFrameRequest& request, int srcIndex);

	struct RunMessage;

	static void StartFilterCallback(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message);
	void StartInner();

	static void StopFilterCallback(VDFilterAccelEngineDispatchQueue *queue, VDFilterAccelEngineMessage *message);
	void StopInner();

	void	RunFilter();
	void	RunFilterInner();
	bool	ConnectAccelBuffers();
	void	DisconnectAccelBuffers();
	bool	ConnectAccelBuffer(VFBitmapInternal *buf, bool bindAsRenderTarget);
	void	DisconnectAccelBuffer(VFBitmapInternal *buf);

public:
	//friend class FilterSystem;

	void	CheckValidConfiguration();
	void	InitSharedBuffers(VDFixedLinearAllocator& lastFrameAlloc);

	bool	IsAltFormatCheckRequired() const { return mAPIVersion < 16; }

	VFBitmapInternal&	GetOutputStream() { return mRealDst; }

	VDFilterPrepareInfo		mPrepareInfo;
	VDFilterPrepareInfo2	mPrepareInfo2;

protected:
	VDFileMappingW32	mFileMapping;
	VFBitmapInternal	mExternalSrcCropped;	// cropped, aligned; different from mRealSrc if entry src blit required

	VFBitmapInternal	mExternalDst;			// external output; different from mRealDst if HDC/SFB active on output
	VFBitmapInternal	mBlendTemp;

	bool	mbAccelerated;

	bool	mbInvalidFormat;
	bool	mbInvalidFormatHandling;
	bool	mbExcessiveFrameSize;

	struct DelayInfo {
		sint64	mSourceFrame;
		sint64	mOutputFrame;
	};

	vdfastvector<DelayInfo>	mFsiDelayRing;	
	uint32		mDelayRingPos;
	VDXFilterStateInfo mfsi;

	VDFilterScriptWrapper	mScriptWrapper;

	uint32		mFlags;
	uint32		mLag;
	uint32    mFilterModFlags;
	int			mAPIVersion;

	VDPosition	mLastResultFrame;

	bool		mbStarted;
	bool		mbFirstFrame;
	bool		mbCanStealSourceBuffer;

	VDStringW	mFilterName;

	FilterInstanceAutoDeinit	*mpAutoDeinit;
	mutable VDAtomicPtr<VDFilterFrameRequestError> mpLogicError;

	vdvector<VFBitmapInternal> mSourceFrames;
	vdfastvector<VDXFBitmap *> mSourceFrameArray;

	FilterDefinitionInstance *mpFDInst;
	IVDFilterFrameEngine *mpEngine;

	typedef vdvector<vdrefptr<IVDFilterFrameSource> > Sources;
	Sources mSources;

	VDFilterFrameAllocatorProxy mSourceAllocator;
	VDFilterFrameAllocatorProxy mResultAllocator;

	VDFilterFrameQueue		mFrameQueueWaiting;
	VDFilterFrameQueue		mFrameQueueInProgress;
	VDFilterFrameCache		mFrameCache;
	VDFilterFrameSharingPredictor	mSharingPredictor;

	vdautoptr<IVDPixmapBlitter>	mpSourceConversionBlitter;

	VDFilterFrameRequest	*mpRequestInProgress;
	sint64					mRequestStartFrame;
	sint64					mRequestEndFrame;
	sint64					mRequestTargetFrame;
	sint64					mRequestCurrentFrame;
	bool					mbRequestCacheIntermediateFrames;

	VDAtomicInt				mbRequestFramePending;
	VDAtomicInt				mbRequestFrameBeingProcessed;
	VDAtomicInt				mbRequestFrameCompleted;
	bool					mbRequestFrameSuccess;
	sint64					mRequestSourceFrame;
	sint64					mRequestOutputFrame;
	bool					mbRequestBltSrcOnEntry;
	MyError					mRequestError;

	VDFilterAccelEngine		*mpAccelEngine;
	VDFilterAccelContext	*mpAccelContext;

	VDFilterThreadContext	mThreadContext;

	VDProfileEventCache		mProfileCacheFilterName;
};

#endif
