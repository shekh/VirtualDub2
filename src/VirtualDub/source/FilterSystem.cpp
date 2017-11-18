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

#include <bitset>
#include <vd2/system/bitmath.h>
#include <vd2/system/debug.h>
#include <vd2/system/error.h>
#include <vd2/system/linearalloc.h>
#include <vd2/system/profile.h>
#include <vd2/system/protscope.h>
#include <vd2/system/VDScheduler.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "VBitmap.h"
#include "crash.h"
#include "misc.h"

#include "filters.h"

#include "FilterInstance.h"
#include "FilterSystem.h"
#include "FilterFrame.h"
#include "FilterFrameAllocator.h"
#include "FilterFrameAllocatorManager.h"
#include "FilterFrameConverter.h"
#include "FilterFrameRequest.h"
#include "FilterAccelEngine.h"
#include "FilterAccelUploader.h"
#include "FilterAccelDownloader.h"
#include "FilterAccelConverter.h"

#define VDDEBUG_FILTERSYS_DETAIL (void)sizeof
//#define VDDEBUG_FILTERSYS_DETAIL VDDEBUG

extern FilterFunctions g_filterFuncs;

namespace {
	bool IsVDXAFormat(int format) {
		return format == nsVDXPixmap::kPixFormat_VDXA_RGB || format == nsVDXPixmap::kPixFormat_VDXA_YUV;
	}

	const uint32 max_align = 64;
	const uint32 vdxa_align = 16;
}

///////////////////////////////////////////////////////////////////////////

class VDFilterSystemDefaultScheduler : public vdrefcounted<IVDFilterSystemScheduler> {
public:
	void Reschedule();
	bool Block();

protected:
	VDSignal	mPendingTasks;
};

void VDFilterSystemDefaultScheduler::Reschedule() {
	mPendingTasks.signal();
}

bool VDFilterSystemDefaultScheduler::Block() {
	mPendingTasks.wait();
	return true;
}

///////////////////////////////////////////////////////////////////////////

class VDFilterSystemProcessProxy : public VDSchedulerNode {
public:
	VDFilterSystemProcessNode* engine;
	int index;

	virtual bool Service();
};

class VDFilterSystemProcessNode : public IVDFilterFrameEngine {
public:
	VDFilterSystemProcessNode(IVDFilterFrameSource *src, IVDFilterSystemScheduler *rootScheduler, int threads);
	~VDFilterSystemProcessNode();

	void AddToScheduler(VDScheduler* p);
	void RemoveFromScheduler();
	void Unblock();

	bool Service(int index);
	bool ServiceSync();

	virtual void Schedule();
	virtual void ScheduleProcess(int index);

protected:
	IVDFilterFrameSource *const mpSource;
	IVDFilterSystemScheduler *const mpRootScheduler;
	VDScheduler *mpScheduler;

	VDAtomicInt mbActive;
	VDAtomicInt mbBlocked;
	//VDAtomicInt mbBlockedPending;
	const bool mbAccelerated;

	VDFilterSystemProcessProxy* proxy;
	int proxy_count;
};

VDFilterSystemProcessNode::VDFilterSystemProcessNode(IVDFilterFrameSource *src, IVDFilterSystemScheduler *rootScheduler, int threads)
	: mpSource(src)
	, mpRootScheduler(rootScheduler)
	, mpScheduler(0)
	, mbActive(false)
	, mbBlocked(true)
	, mbAccelerated(src->IsAccelerated())
{
	//if (src->IsAccelerated()) threads = 1;
	proxy_count = src->AllocateNodes(threads);
	proxy = new VDFilterSystemProcessProxy[proxy_count];
	{for(int i=0; i<proxy_count; i++){
		proxy[i].engine = this;
		proxy[i].index = i;
	}}
}

VDFilterSystemProcessNode::~VDFilterSystemProcessNode() {
	delete[] proxy;
}

void VDFilterSystemProcessNode::AddToScheduler(VDScheduler* p) {
	mpScheduler = p;
	{for(int i=0; i<proxy_count; i++) p->Add(&proxy[i]); }
}

void VDFilterSystemProcessNode::RemoveFromScheduler() {
	if(mpScheduler){
		{for(int i=0; i<proxy_count; i++) mpScheduler->Remove(&proxy[i]); }
		mpScheduler = 0;
	}
}

void VDFilterSystemProcessNode::Unblock() {
	mbBlocked = false;

	if (mpScheduler) {
		{for(int i=0; i<proxy_count; i++) proxy[i].Reschedule(); }
	}
}

bool VDFilterSystemProcessProxy::Service() {
	return engine->Service(index); 
}

bool VDFilterSystemProcessNode::Service(int index) {
	if (index>=proxy_count)
		return false;

	if (mbBlocked)
		return false;

	VDPROFILEBEGIN("Run filter");
	bool activity = false;
	switch(mpSource->RunProcess(index)) {
		case IVDFilterFrameSource::kRunResult_Running:
		case IVDFilterFrameSource::kRunResult_IdleWasActive:
		case IVDFilterFrameSource::kRunResult_BlockedWasActive:
			activity = true;
			break;
	}
	VDPROFILEEND();

	return activity;
}

bool VDFilterSystemProcessNode::ServiceSync() {
	if (mbAccelerated || !mbActive.xchg(false))
		return false;

	bool activity = false;
	switch(mpSource->RunProcess(0)) {
		case IVDFilterFrameSource::kRunResult_Running:
		case IVDFilterFrameSource::kRunResult_IdleWasActive:
		case IVDFilterFrameSource::kRunResult_BlockedWasActive:
			activity = true;
			VDDEBUG_FILTERSYS_DETAIL("FilterSystem[%s]: Processing produced activity.\n", mpSource->GetDebugDesc());
			break;

		default:
			VDDEBUG_FILTERSYS_DETAIL("FilterSystem[%s]: Processing was idle.\n", mpSource->GetDebugDesc());
			break;
	}

	if (activity)
		mbActive = true;

	return activity;
}

void VDFilterSystemProcessNode::Schedule() {
	mpRootScheduler->Reschedule();
}

void VDFilterSystemProcessNode::ScheduleProcess(int index) {
	if (index>=proxy_count)
		return;

	VDDEBUG_FILTERSYS_DETAIL("FilterSystem[%s]: Process rescheduling request.\n", mpSource->GetDebugDesc());
	if (mpScheduler) {
		if (!mbBlocked) {
			proxy[index].Reschedule();
		}
	} else {
		mbActive = true;
		Schedule();
	}
}

///////////////////////////////////////////////////////////////////////////

struct FilterSystem::Bitmaps {
	vdrefptr<IVDFilterFrameSource> mpSource;
	vdrefptr<IVDFilterSystemScheduler> mpScheduler;
	IVDFilterFrameSource *mpTailSource;

	VFBitmapInternal	mInitialBitmap;
	VDPixmapLayout		mFinalLayout;

	VDFilterFrameAllocatorManager	mAllocatorManager;

	vdrefptr<VDFilterAccelEngine>	mpAccelEngine;

	vdautoptr<VDScheduler> mpProcessScheduler;
	vdautoptr<VDSchedulerThreadPool> mpProcessSchedulerThreadPool;
	VDSignal			mProcessSchedulerSignal;
};

///////////////////////////////////////////////////////////////////////////

FilterSystem::FilterSystem()
	: mbFiltersInited(false)
	, mbFiltersError(false)
	, mbAccelEnabled(false)
	, mbAccelDebugVisual(false)
	, mbTrimmedChain(false)
	, mThreadsRequested(-1)
	, mProcessNodes(1)
	, mThreadPriority(VDThread::kPriorityDefault)
	, mOutputFrameRate(0, 0)
	, mOutputFrameCount(0)
	, mpBitmaps(new Bitmaps)
	, lpBuffer(NULL)
	, lRequiredSize(0)
{
}

FilterSystem::~FilterSystem() {
	DeinitFilters();
	DeallocateBuffers();

	if (mpBitmaps)
		delete mpBitmaps;
}

void FilterSystem::SetAccelEnabled(bool enable) {
	mbAccelEnabled = enable;
}

void FilterSystem::SetVisualAccelDebugEnabled(bool enable) {
	mbAccelDebugVisual = enable;
}

void FilterSystem::SetAsyncThreadCount(sint32 threadsToUse) {
	mThreadsRequested = threadsToUse;
}

void FilterSystem::SetAsyncThreadPriority(int priority) {
	mThreadPriority = priority;

	if (mpBitmaps->mpProcessSchedulerThreadPool)
		mpBitmaps->mpProcessSchedulerThreadPool->SetPriority(mThreadPriority);
}

struct FilterSystem::PrepareState {
	typedef vdhashmap<VDStringA, VFBitmapInternal *> NamedInputs;
	VFBitmapInternal *baseInput;
	VFBitmapInternal *prevInput;
	NamedInputs namedInputs;
};

// prepareLinearChain(): init bitmaps in a linear filtering system
void FilterSystem::prepareLinearChain(VDFilterChainDesc *desc, uint32 src_width, uint32 src_height, VDPixmapFormatEx src_format, const VDFraction& sourceFrameRate, sint64 sourceFrameCount, const VDFraction& sourcePixelAspect) {
	if (mbFiltersInited)
		return;

	DeallocateBuffers();

	uint32 alignReq = 16;
	bool align_changed;

	VFBitmapInternal *lastInput = 0;

	for(;;){
		VDPixmapCreateLinearLayout(mpBitmaps->mInitialBitmap.mPixmapLayout, src_format, src_width, src_height, alignReq);
		mpBitmaps->mInitialBitmap.mPixmapLayout.formatEx = src_format;

		if (VDPixmapGetInfo(src_format).palsize > 0)
			mpBitmaps->mInitialBitmap.mPixmapLayout.palette = mPalette;

		if (src_format == nsVDPixmap::kPixFormat_XRGB8888)
			VDPixmapLayoutFlipV(mpBitmaps->mInitialBitmap.mPixmapLayout);

		mpBitmaps->mInitialBitmap.mFrameRateHi		= sourceFrameRate.getHi();
		mpBitmaps->mInitialBitmap.mFrameRateLo		= sourceFrameRate.getLo();
		mpBitmaps->mInitialBitmap.mFrameCount		= sourceFrameCount;
		mpBitmaps->mInitialBitmap.ConvertPixmapLayoutToBitmapLayout();
		mpBitmaps->mInitialBitmap.mAspectRatioHi	= sourcePixelAspect.getHi();
		mpBitmaps->mInitialBitmap.mAspectRatioLo	= sourcePixelAspect.getLo();

		lRequiredSize = 0;
		mbFiltersUseAcceleration = false;
		align_changed = false;

		PrepareState state;
		state.baseInput = &mpBitmaps->mInitialBitmap;
		state.prevInput = state.baseInput;
		state.namedInputs[VDStringA("$input")] = state.baseInput;

		for(VDFilterChainDesc::Entries::const_iterator it(desc->mEntries.begin()), itEnd(desc->mEntries.end());
			it != itEnd;
			++it)
		{
			VDFilterChainEntry *ent = *it;
    	FilterInstance *fa = ent->mpInstance;
			if (!fa->IsEnabled())
				continue;

			prepareLinearEntry(state,ent,mbAccelEnabled,alignReq);
			uint32 align = ent->mpInstance->GetAlignReq();
			if (align>alignReq) {
				alignReq = align;
				align_changed = true;
				break;
			}

	    lRequiredSize += fa->mPrepareInfo.mLastFrameSizeRequired;

	    state.prevInput = &fa->GetOutputStream();
	    if (!ent->mOutputName.empty())
		    state.namedInputs[ent->mOutputName] = state.prevInput;

	    if (IsVDXAFormat(state.prevInput->mPixmapLayout.format))
		    mbFiltersUseAcceleration = true;

			if (fa->IsTerminal())
				break;
		}

		if (!align_changed) {
			lastInput = state.prevInput;
			break;
		}
	}

	// 2/3) Temp buffers
	VFBitmapInternal& final = *lastInput;
	mOutputPixelAspect.Assign(final.mAspectRatioHi, final.mAspectRatioLo);
	mOutputFrameRate.Assign(final.mFrameRateHi, final.mFrameRateLo);
	mOutputFrameCount = final.mFrameCount;
	mpBitmaps->mFinalLayout = final.mPixmapLayout;

	if (IsVDXAFormat(mpBitmaps->mFinalLayout.format)) {
		int format;

		if (mpBitmaps->mFinalLayout.format == nsVDXPixmap::kPixFormat_VDXA_RGB)
			format = nsVDXPixmap::kPixFormat_XRGB8888;
		else if (mpBitmaps->mFinalLayout.format == nsVDXPixmap::kPixFormat_VDXA_YUV)
			format = nsVDXPixmap::kPixFormat_YUV444_Planar;

		VDPixmapCreateLinearLayout(mpBitmaps->mFinalLayout, format, mpBitmaps->mFinalLayout.w, mpBitmaps->mFinalLayout.h, vdxa_align);
	}
}

void FilterSystem::prepareLinearEntry(PrepareState& state, VDFilterChainEntry *ent, bool accelEnabled, uint32 alignReq) {
  typedef PrepareState::NamedInputs NamedInputs;
	FilterInstance *fa = ent->mpInstance;

	const VDXFilterDefinition *fdef = fa->GetDefinition();
	vdvector<VFBitmapInternal *> inputSrcs;
	uint32 inputCount;
	if (ent->mSources.empty()) {
		inputSrcs.clear();
		if (fdef->mSourceCountLowMinus1 < 0) {
			inputCount = 0;
		} else {
			inputCount = 1;
			inputSrcs.resize(1, state.prevInput);
		}
	} else {
		inputCount = ent->mSources.size();

		inputSrcs.resize(inputCount);

		for(uint32 i=0; i<inputCount; ++i) {
			const VDStringA& srcName = ent->mSources[i];

			if (srcName == "$prev")
				inputSrcs[i] = state.prevInput;
			else {
				NamedInputs::const_iterator it(state.namedInputs.find(srcName));

				if (it == state.namedInputs.end()) {
					// We cannot throw out of this routine, so we use the previous
					// input instead.
					inputSrcs[i] = state.prevInput;
				} else
					inputSrcs[i] = it->second;
			}
		}
	}

	// check if the pin count is correct; if it is not, we need to fudge
	if (inputCount < (fdef->mSourceCountLowMinus1 + 1) || inputCount > (fdef->mSourceCountHighMinus1 + 1)) {
		inputSrcs.resize(fdef->mSourceCountLowMinus1 + 1, inputSrcs.empty() ? state.baseInput : inputSrcs.back());
	}

	// check if we need to blit
	VDFilterPrepareInfo& prepareInfo = fa->mPrepareInfo;
	VDFilterPrepareInfo2& prepareInfo2 = fa->mPrepareInfo2;

	prepareInfo2.mStreams.resize(inputCount);
	for(uint32 i=0; i<inputCount; ++i) {
		prepareInfo2.mStreams[i].mbConvertOnEntry = false;
		prepareInfo2.mStreams[i].srcFormat = 0;
	}

	vdvector<VFBitmapInternal> inputs;
	inputs.clear();
	inputs.resize(inputCount);

	for(uint32 i = 0; i < inputCount; ++i)
		inputs[i] = *inputSrcs[i];

	const bool altFormatCheckRequired = fa->IsAltFormatCheckRequired();
	if (altFormatCheckRequired) {
		VFBitmapInternal& bmTemp = inputs.front();

		if (bmTemp.mPixmapLayout.format != nsVDPixmap::kPixFormat_XRGB8888 || bmTemp.mPixmapLayout.pitch > 0
			|| VDPixmapGetInfo(bmTemp.mPixmapLayout.format).palsize) {

			VDPixmapCreateLinearLayout(bmTemp.mPixmapLayout, nsVDPixmap::kPixFormat_XRGB8888, bmTemp.w, bmTemp.h, alignReq);
			VDPixmapLayoutFlipV(bmTemp.mPixmapLayout);
			bmTemp.ConvertPixmapLayoutToBitmapLayout();

			prepareInfo2.mStreams[0].mbConvertOnEntry = true;
			prepareInfo2.mStreams[0].srcFormat = bmTemp.mPixmapLayout.formatEx;
		}
	}

	fa->PrepareReset();

	uint32 flags = fa->Prepare(inputs.data(), inputCount, prepareInfo, alignReq);

	if (flags == FILTERPARAM_NOT_SUPPORTED || (flags & FILTERPARAM_SUPPORTS_ALTFORMATS)) {
		using namespace nsVDPixmap;

		VDASSERTCT(kPixFormat_Max_Standard == kPixFormat_R10K + 1);

		std::bitset<nsVDPixmap::kPixFormat_Max_Standard> formatMask;

		formatMask.set(kPixFormat_XRGB1555);
		formatMask.set(kPixFormat_RGB565);
		formatMask.set(kPixFormat_RGB888);
		formatMask.set(kPixFormat_XRGB8888);
		formatMask.set(kPixFormat_Y8);
		formatMask.set(kPixFormat_Y8_FR);
		formatMask.set(kPixFormat_YUV422_UYVY);
		formatMask.set(kPixFormat_YUV422_YUYV);
		formatMask.set(kPixFormat_YUV444_Planar);
		formatMask.set(kPixFormat_YUV422_Planar);
		formatMask.set(kPixFormat_YUV420_Planar);
		formatMask.set(kPixFormat_YUV420i_Planar);
		formatMask.set(kPixFormat_YUV420it_Planar);
		formatMask.set(kPixFormat_YUV420ib_Planar);
		formatMask.set(kPixFormat_YUV411_Planar);
		formatMask.set(kPixFormat_YUV410_Planar);
		formatMask.set(kPixFormat_YUV422_UYVY_709);
		formatMask.set(kPixFormat_YUV422_YUYV_709);
		formatMask.set(kPixFormat_YUV444_Planar_709);
		formatMask.set(kPixFormat_YUV422_Planar_709);
		formatMask.set(kPixFormat_YUV420_Planar_709);
		formatMask.set(kPixFormat_YUV420i_Planar_709);
		formatMask.set(kPixFormat_YUV420it_Planar_709);
		formatMask.set(kPixFormat_YUV420ib_Planar_709);
		formatMask.set(kPixFormat_YUV411_Planar_709);
		formatMask.set(kPixFormat_YUV410_Planar_709);
		formatMask.set(kPixFormat_YUV422_UYVY_FR);
		formatMask.set(kPixFormat_YUV422_YUYV_FR);
		formatMask.set(kPixFormat_YUV444_Planar_FR);
		formatMask.set(kPixFormat_YUV422_Planar_FR);
		formatMask.set(kPixFormat_YUV420_Planar_FR);
		formatMask.set(kPixFormat_YUV420i_Planar_FR);
		formatMask.set(kPixFormat_YUV420it_Planar_FR);
		formatMask.set(kPixFormat_YUV420ib_Planar_FR);
		formatMask.set(kPixFormat_YUV411_Planar_FR);
		formatMask.set(kPixFormat_YUV410_Planar_FR);
		formatMask.set(kPixFormat_YUV422_UYVY_709_FR);
		formatMask.set(kPixFormat_YUV422_YUYV_709_FR);
		formatMask.set(kPixFormat_YUV444_Planar_709_FR);
		formatMask.set(kPixFormat_YUV422_Planar_709_FR);
		formatMask.set(kPixFormat_YUV420_Planar_709_FR);
		formatMask.set(kPixFormat_YUV420i_Planar_709_FR);
		formatMask.set(kPixFormat_YUV420it_Planar_709_FR);
		formatMask.set(kPixFormat_YUV420ib_Planar_709_FR);
		formatMask.set(kPixFormat_YUV411_Planar_709_FR);
		formatMask.set(kPixFormat_YUV410_Planar_709_FR);
		formatMask.set(kPixFormat_XRGB64);
		formatMask.set(kPixFormat_YUV444_Planar16);
		formatMask.set(kPixFormat_YUV422_Planar16);
		formatMask.set(kPixFormat_YUV420_Planar16);
		formatMask.set(kPixFormat_Y16);

		static const int kStaticOrder[]={
			kPixFormat_YUV444_Planar,
			kPixFormat_YUV422_Planar,
			kPixFormat_YUV422_UYVY,
			kPixFormat_YUV422_YUYV,
			kPixFormat_YUV420_Planar,
			kPixFormat_YUV411_Planar,
			kPixFormat_YUV410_Planar,
			kPixFormat_XRGB8888
		};

		int staticOrderIndex = 0;

		// test an invalid format and make sure the filter DOESN'T accept it
		for(uint32 i = 0; i < inputCount; ++i)
			inputs[i] = *inputSrcs[i];

		inputs[0].mPixmapLayout.format = 255;

		flags = fa->Prepare(inputs.data(), inputCount, prepareInfo, alignReq);

		inputs[0] = *inputSrcs[0];

		VDPixmapFormatEx originalFormat = inputs[0].mPixmapLayout.formatEx;
		int format = originalFormat;
		if (flags != FILTERPARAM_NOT_SUPPORTED) {
			formatMask.reset();
			VDASSERT(fa->GetInvalidFormatHandlingState());
		} else {
			bool conversionRequired = true;

			if (accelEnabled && fa->IsAcceleratable()) {
				static const int kFormats[]={
					nsVDXPixmap::kPixFormat_VDXA_RGB,
					nsVDXPixmap::kPixFormat_VDXA_YUV,
					nsVDXPixmap::kPixFormat_VDXA_RGB
				};

				const int *formats = kFormats + 1;

				switch(originalFormat) {
					case nsVDXPixmap::kPixFormat_RGB565:
					case nsVDXPixmap::kPixFormat_XRGB1555:
					case nsVDXPixmap::kPixFormat_RGB888:
					case nsVDXPixmap::kPixFormat_XRGB8888:
					case nsVDXPixmap::kPixFormat_VDXA_RGB:
						formats = kFormats;
						break;
				}

				for(int i=0; i<2; ++i) {
					format = formats[i];

					bool conversionFound = false;
					for(uint32 j = 0; j < inputCount; ++j) {
						inputs[j] = *inputSrcs[j];

						if (inputs[j].mPixmapLayout.format != format)
							conversionFound = true;

						VDPixmapCreateLinearLayout(inputs[j].mPixmapLayout, nsVDPixmap::kPixFormat_XRGB8888, inputs[j].w, inputs[j].h, alignReq);
						inputs[j].mPixmapLayout.format = format;
						inputs[j].mPixmapLayout.formatEx = format;
						inputs[j].ConvertPixmapLayoutToBitmapLayout();
					}

					flags = fa->Prepare(inputs.data(), inputCount, prepareInfo, alignReq);

					if (flags != FILTERPARAM_NOT_SUPPORTED) {
						// clear the format mask so we don't try any more formats
						formatMask.reset();

						conversionRequired = conversionFound;
						break;
					}
				}

				if (formatMask.any()) {
					// failed - restore original first format to try
					format = originalFormat;
				}
			}

			// check if the incoming format is VDXA and we haven't already handled the situation --
			// if so we must convert it to the equivalent.
			if (formatMask.any()) {
				switch(originalFormat) {
					case nsVDXPixmap::kPixFormat_VDXA_RGB:
						format = nsVDXPixmap::kPixFormat_XRGB8888;
						break;
					case nsVDXPixmap::kPixFormat_VDXA_YUV:
						format = nsVDXPixmap::kPixFormat_YUV444_Planar;
						break;
				}
			}

			while(format && formatMask.any()) {
				if (formatMask.test(format)) {
					if (format == originalFormat) {
						for(uint32 j = 0; j < inputCount; ++j)
							inputs[j] = *inputSrcs[j];

						flags = fa->Prepare(inputs.data(), inputCount, prepareInfo, alignReq);

						if (flags != FILTERPARAM_NOT_SUPPORTED) {
							conversionRequired = false;
							break;
						}
					}

					for(uint32 j = 0; j < inputCount; ++j) {
						inputs[j] = *inputSrcs[j];

						VDPixmapFormatEx formatEx = format;
						if (VDPixmapFormatMatrixType(format)==1)
							formatEx = VDPixmapFormatCombine(format,VDPixmapFormatNormalize(originalFormat));
						VDPixmapCreateLinearLayout(inputs[j].mPixmapLayout, formatEx, inputs[j].w, inputs[j].h, alignReq);

						if (altFormatCheckRequired && format == nsVDPixmap::kPixFormat_XRGB8888)
							VDPixmapLayoutFlipV(inputs[j].mPixmapLayout);

						inputs[j].ConvertPixmapLayoutToBitmapLayout();
					}

					flags = fa->Prepare(inputs.data(), inputCount, prepareInfo, alignReq);

					if (flags != FILTERPARAM_NOT_SUPPORTED)
						break;

					formatMask.reset(format);
				}

				switch(format) {
				case kPixFormat_YUV422_UYVY:
					if (formatMask.test(kPixFormat_YUV422_YUYV))
						format = kPixFormat_YUV422_YUYV;
					else
						format = kPixFormat_YUV422_Planar;
					break;

				case kPixFormat_YUV422_YUYV:
					if (formatMask.test(kPixFormat_YUV422_UYVY))
						format = kPixFormat_YUV422_UYVY;
					else
						format = kPixFormat_YUV422_Planar;
					break;

				case kPixFormat_YUV422_UYVY_709:
					if (formatMask.test(kPixFormat_YUV422_YUYV_709))
						format = kPixFormat_YUV422_YUYV_709;
					else
						format = kPixFormat_YUV422_Planar_709;
					break;

				case kPixFormat_YUV422_YUYV_709:
					if (formatMask.test(kPixFormat_YUV422_UYVY_709))
						format = kPixFormat_YUV422_UYVY_709;
					else
						format = kPixFormat_YUV422_Planar_709;
					break;

				case kPixFormat_YUV422_UYVY_FR:
					if (formatMask.test(kPixFormat_YUV422_YUYV_FR))
						format = kPixFormat_YUV422_YUYV_FR;
					else
						format = kPixFormat_YUV422_Planar_FR;
					break;

				case kPixFormat_YUV422_YUYV_709_FR:
					if (formatMask.test(kPixFormat_YUV422_UYVY_709_FR))
						format = kPixFormat_YUV422_UYVY_709_FR;
					else
						format = kPixFormat_YUV422_Planar_709_FR;
					break;

				case kPixFormat_Y8:
				case kPixFormat_YUV422_Planar:
					format = kPixFormat_YUV444_Planar;
					break;

				case kPixFormat_YUV420_Planar:
				case kPixFormat_YUV411_Planar:
					format = kPixFormat_YUV422_Planar;
					break;

				case kPixFormat_YUV410_Planar:
				case kPixFormat_YUV420_NV12:
					format = kPixFormat_YUV420_Planar;
					break;

				case kPixFormat_YUV422_V210:
					format = kPixFormat_YUV422_Planar16;
					//format = kPixFormat_YUV422_Planar;
					break;

				case kPixFormat_XYUV64:
				case kPixFormat_YUV444_V410:
				case kPixFormat_YUV444_Y410:
					format = kPixFormat_YUV444_Planar16;
					break;

				case kPixFormat_Y8_FR:
				case kPixFormat_YUV422_Planar_FR:
					format = kPixFormat_YUV444_Planar_FR;
					break;

				case kPixFormat_YUV420_Planar_FR:
				case kPixFormat_YUV411_Planar_FR:
					format = kPixFormat_YUV422_Planar_FR;
					break;

				case kPixFormat_YUV410_Planar_FR:
					format = kPixFormat_YUV420_Planar_FR;
					break;

				case kPixFormat_YUV422_Planar_709:
					format = kPixFormat_YUV444_Planar_709;
					break;

				case kPixFormat_YUV420_Planar_709:
				case kPixFormat_YUV411_Planar_709:
					format = kPixFormat_YUV422_Planar_709;
					break;

				case kPixFormat_YUV410_Planar_709:
					format = kPixFormat_YUV420_Planar_709;
					break;

				case kPixFormat_YUV422_Planar_709_FR:
					format = kPixFormat_YUV444_Planar_709_FR;
					break;

				case kPixFormat_YUV420_Planar_709_FR:
				case kPixFormat_YUV411_Planar_709_FR:
					format = kPixFormat_YUV422_Planar_709_FR;
					break;

				case kPixFormat_YUV410_Planar_709_FR:
					format = kPixFormat_YUV420_Planar_709_FR;
					break;

				case kPixFormat_R210:
				case kPixFormat_R10K:
					format = kPixFormat_XRGB64;
					break;

				case kPixFormat_XRGB1555:
				case kPixFormat_RGB565:
				case kPixFormat_RGB888:
				case kPixFormat_XRGB64:
					if (formatMask.test(kPixFormat_XRGB8888)) {
						format = kPixFormat_XRGB8888;
						break;
					}

					// fall through

				default:
					if (staticOrderIndex < sizeof(kStaticOrder)/sizeof(kStaticOrder[0]))
						format = kStaticOrder[staticOrderIndex++];
					else if (formatMask.test(kPixFormat_XRGB8888))
						format = kPixFormat_XRGB8888;
					else {
						for(size_t i=1; i<nsVDPixmap::kPixFormat_Max_Standard; ++i) {
							if (formatMask.test(i)) {
								format = i;
								break;
							}
						}
					}
					break;
				}
			}

			if (conversionRequired) {
				for(uint32 j = 0; j < inputCount; ++j) {
					prepareInfo2.mStreams[j].mbConvertOnEntry = true;
					prepareInfo2.mStreams[j].srcFormat = originalFormat;
				}
			}
		}
	}

	if (fa->GetInvalidFormatState()) {
		for(uint32 j = 0; j < inputCount; ++j)
			prepareInfo2.mStreams[j].mbConvertOnEntry = false;

		flags = 0;
	}
}

// initLinearChain(): prepare for a linear filtering system
void FilterSystem::initLinearChain(IVDFilterSystemScheduler *scheduler, uint32 filterStateFlags, VDFilterChainDesc *desc, IVDFilterFrameSource *src0, uint32 src_width, uint32 src_height, VDPixmapFormatEx src_format, const uint32 *palette, const VDFraction& sourceFrameRate, sint64 sourceFrameCount, const VDFraction& sourcePixelAspect) {
	DeinitFilters();
	DeallocateBuffers();

	if (!scheduler)
		scheduler = new VDFilterSystemDefaultScheduler;

	mpBitmaps->mpScheduler = scheduler;

	mFilterStateFlags = filterStateFlags;

	// buffers required:
	//
	// 1) Input buffer (8/16/24/32 bits)
	// 2) Temp buffer #1 (32 bits)
	// 3) Temp buffer #2 (32 bits)
	//
	// All temporary buffers must be aligned on an 8-byte boundary, and all
	// pitches must be a multiple of 8 bytes.  The exceptions are the source
	// and destination buffers, which may have pitches that are only 4-byte
	// multiples.

	int palSize = VDPixmapGetInfo(src_format).palsize;
	if (palette && palSize)
		memcpy(mPalette, palette, palSize*sizeof(uint32));

	prepareLinearChain(desc, src_width, src_height, src_format, sourceFrameRate, sourceFrameCount, sourcePixelAspect);

	if (mbFiltersUseAcceleration) {
		mpBitmaps->mpAccelEngine = new VDFilterAccelEngine;

		if (!mpBitmaps->mpAccelEngine->Init(mbAccelDebugVisual))
			throw MyError("Cannot start filter chain: The 3D accelerator device is not available.");
	}

	AllocateBuffers(lRequiredSize);

	VDFixedLinearAllocator lastFrameAlloc(lpBuffer, lRequiredSize);

	mpBitmaps->mAllocatorManager.Shutdown();

	mpBitmaps->mpSource = src0;
	mpBitmaps->mpSource->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager);

	mpBitmaps->mInitialBitmap.mPixmap.clear();

	StreamTail inputTail;
	inputTail.mpSrc = src0;
	inputTail.mpProxy = src0->GetOutputAllocatorProxy();

	StreamTail prevTail(inputTail);
	vdfastvector<StreamTail> tails;

	typedef vdhashmap<VDStringA, StreamTail> Tailset; 
	Tailset tailset;

	tailset[VDStringA("$input")] = inputTail;

	for(VDFilterChainDesc::Entries::const_iterator it(desc->mEntries.begin()), itEnd(desc->mEntries.end());
		it != itEnd;
		++it)
	{
		VDFilterChainEntry *ent = *it;
		FilterInstance *fa = ent->mpInstance;

		if (!fa->IsEnabled())
			continue;

		const VDXFilterDefinition *fdef = fa->GetDefinition();
		uint32 inputCount;
		if (ent->mSources.empty()) {
			inputCount = 1;

			tails.clear();

			if (fdef->mSourceCountLowMinus1 >= 0)
				tails.resize(1, prevTail);
		} else {
			inputCount = ent->mSources.size();

			tails.resize(inputCount);

			for(uint32 i=0; i<inputCount; ++i) {
				const VDStringA& srcName = ent->mSources[i];

				if (srcName.empty())
					throw MyError("Cannot start filters: an instance of filter \"%s\" has an unconnected input.", fa->GetName());
				else if (srcName == "$prev")
					tails[i] = prevTail;
				else {
					Tailset::const_iterator it(tailset.find(srcName));

					if (it == tailset.end())
						throw MyError("Cannot start filters: an instance of filter \"%s\" references an unknown output name: %s", fa->GetName(), srcName.c_str());

 					tails[i] = it->second;
				}
			}
		}

		if (inputCount < (uint32)(fdef->mSourceCountLowMinus1 + 1)) {
			throw MyError("Cannot start filters: an instance of filter \"%s\" has too few inputs (expected %u, got %u)", fa->GetName(), fdef->mSourceCountLowMinus1 + 1, inputCount);
		} else if (inputCount > (uint32)(fdef->mSourceCountHighMinus1 + 1)) {
			throw MyError("Cannot start filters: an instance of filter \"%s\" has too many inputs (expected %u, got %u)", fa->GetName(), fdef->mSourceCountHighMinus1 + 1, inputCount);
		}

		fa->CheckValidConfiguration();

		const VDFilterPrepareInfo& prepareInfo = fa->mPrepareInfo;
		const VDFilterPrepareInfo2& prepareInfo2 = fa->mPrepareInfo2;

		for(uint32 i = 0; i < inputCount; ++i) {
			StreamTail& tail = *(tails.end() - inputCount + i);
			const VDFilterPrepareStreamInfo& streamInfo = prepareInfo.mStreams[i];
			const VDFilterPrepareStreamInfo2& streamInfo2 = prepareInfo2.mStreams[i];

			const VDPixmapLayout& srcLayout0 = tail.mpSrc->GetOutputLayout();
			const VDPixmapLayout& extSrcLayout = streamInfo.mExternalSrc.mPixmapLayout;
			int srcFormat = srcLayout0.format;

			bool normalizeRequired = false;
			if (streamInfo.mbNormalizeOnEntry) {
				int dstFormat = extSrcLayout.format;
				switch (dstFormat) {
				case nsVDPixmap::kPixFormat_XRGB64:
				case nsVDPixmap::kPixFormat_YUV420_Planar16:
				case nsVDPixmap::kPixFormat_YUV422_Planar16:
				case nsVDPixmap::kPixFormat_YUV444_Planar16:
					normalizeRequired = true;
					break;
				}
			}

			if (streamInfo2.mbConvertOnEntry || (IsVDXAFormat(srcFormat) && fa->IsCroppingEnabled())) {
				int dstFormat = extSrcLayout.format;

				vdrect32 srcCrop(fa->GetCropInsets());
				vdrect32 srcRect(srcCrop.left, srcCrop.top, srcLayout0.w - srcCrop.right, srcLayout0.h - srcCrop.bottom);

				if (IsVDXAFormat(dstFormat)) {
					if (IsVDXAFormat(srcFormat)) {		// VDXA -> VDXA
						AppendAccelConversionFilter(tail, dstFormat);
					} else {	// CPU -> VDXA
						// convert to 8888/888 if necessary
						if (srcFormat != nsVDPixmap::kPixFormat_XRGB8888 && srcFormat != nsVDPixmap::kPixFormat_YUV444_Planar) {
							vdrefptr<VDFilterFrameConverter> conv(new VDFilterFrameConverter);

							VDPixmapLayout layout;

							if (dstFormat == nsVDXPixmap::kPixFormat_VDXA_YUV)
								VDPixmapCreateLinearLayout(layout, nsVDPixmap::kPixFormat_YUV444_Planar, extSrcLayout.w, extSrcLayout.h, vdxa_align);
							else
								VDPixmapCreateLinearLayout(layout, nsVDPixmap::kPixFormat_XRGB8888, extSrcLayout.w, extSrcLayout.h, vdxa_align);

							AppendConversionFilter(tail, layout);
						}

						// upload to accelerator
						AppendAccelUploadFilter(tail, srcRect);

						// apply conversion on VDXA side if necessary
						if (tail.mpSrc->GetOutputLayout().format != dstFormat)
							AppendAccelConversionFilter(tail, dstFormat);
					}
				} else {	// VDXA, CPU -> CPU
					bool cpuConversionRequired = true;
					
					if (IsVDXAFormat(srcFormat)) {		// VDXA -> CPU
						const VDPixmapLayout& prevLayout = tail.mpSrc->GetOutputLayout();

						int targetFormat;
						if (prevLayout.format == nsVDXPixmap::kPixFormat_VDXA_RGB)
							targetFormat = nsVDPixmap::kPixFormat_XRGB8888;
						else
							targetFormat = nsVDPixmap::kPixFormat_YUV444_Planar;

						VDPixmapLayout layout;
						if (dstFormat == targetFormat) {
							layout = extSrcLayout;
							cpuConversionRequired = false;
						} else
							VDPixmapCreateLinearLayout(layout, targetFormat, prevLayout.w, prevLayout.h, max_align);

						AppendAccelDownloadFilter(tail, layout);
					}

					if (cpuConversionRequired)
						AppendConversionFilter(tail, extSrcLayout, normalizeRequired);
				}
			} else if (normalizeRequired) {
				AppendConversionFilter(tail, extSrcLayout, true);
			}

			if (streamInfo.mAlignOnEntry) {
				const VDFilterStreamDesc& croppedDesc = streamInfo.mExternalSrcCropped.GetStreamDesc();
				const VDFilterStreamDesc& preAlignDesc = streamInfo.mExternalSrcPreAlign.GetStreamDesc();

				AppendAlignmentFilter(tail, croppedDesc.mLayout, preAlignDesc.mLayout);
			}
		}

		FilterEntry& fe = mFilters.push_back();
		fe.mpFilter = fa;
		fe.mSources.resize(inputCount);

		for(uint32 i = 0; i < inputCount; ++i) {
			StreamTail& tail = *(tails.end() - inputCount + i);

			fe.mSources[i] = tail.mpSrc;
		}

		fa->InitSharedBuffers(lastFrameAlloc);
		fa->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager);

		for(uint32 i = 0; i < inputCount; ++i) {
			StreamTail& tail = *(tails.end() - inputCount + i);

			fa->RegisterSourceAllocReqs(i, tail.mpProxy);
		}

		prevTail.mpSrc = fa;
		prevTail.mpProxy = fa->GetOutputAllocatorProxy();

		if (!ent->mOutputName.empty())
			tailset[ent->mOutputName] = prevTail;

		if (fa->IsTerminal()) break;
	}

	// check if the last format is accelerated, and add a downloader if necessary
	StreamTail& finalTail = prevTail;
	const VDPixmapLayout& finalLayout = finalTail.mpSrc->GetOutputLayout();
	if (finalLayout.format == nsVDXPixmap::kPixFormat_VDXA_RGB) {
		VDPixmapLayout layout;
		VDPixmapCreateLinearLayout(layout, nsVDPixmap::kPixFormat_XRGB8888, finalLayout.w, finalLayout.h, vdxa_align);

		AppendAccelDownloadFilter(finalTail, layout);
	} else if (finalLayout.format == nsVDXPixmap::kPixFormat_VDXA_YUV) {
		VDPixmapLayout layout;
		VDPixmapCreateLinearLayout(layout, nsVDPixmap::kPixFormat_YUV444_Planar, finalLayout.w, finalLayout.h, vdxa_align);

		AppendAccelDownloadFilter(finalTail, layout);
	}
}

void FilterSystem::ReadyFilters() {
	if (mbFiltersInited)
		return;

	mbFiltersError = false;
	mbTrimmedChain = false;
	mProcessNodes = 1;

	if (mThreadsRequested >= 0) {
		mpBitmaps->mpProcessScheduler = new VDScheduler;
		mpBitmaps->mpProcessScheduler->setSignal(&mpBitmaps->mProcessSchedulerSignal);

		uint32 threadsToUse = VDGetLogicalProcessorCount();

		if (mThreadsRequested > 0) {
			threadsToUse = mThreadsRequested;

			if (threadsToUse > 32)
				threadsToUse = 32;
		} else {
			if (threadsToUse > 4)
				threadsToUse = 4;
		}

		mProcessNodes = threadsToUse;
		mpBitmaps->mpProcessSchedulerThreadPool = new VDSchedulerThreadPool;
		mpBitmaps->mpProcessSchedulerThreadPool->SetPriority(mThreadPriority);
		mpBitmaps->mpProcessSchedulerThreadPool->Start(mpBitmaps->mpProcessScheduler, mProcessNodes);
	}

	mpBitmaps->mAllocatorManager.AssignAllocators(mpBitmaps->mpAccelEngine);

	IVDFilterFrameSource *pLastSource = mpBitmaps->mpSource;

	mActiveFilters.clear();
	mActiveFilters.reserve(mFilters.size());

	VDScheduler *accelScheduler = NULL;
	if (mpBitmaps->mpAccelEngine)
		accelScheduler = mpBitmaps->mpAccelEngine->GetScheduler();

	try {
		for(Filters::const_iterator it(mFilters.begin()), itEnd(mFilters.end()); it != itEnd; ++it) {
			IVDFilterFrameSource *src = it->mpFilter;

			ActiveFilterEntry& afe = mActiveFilters.push_back();

			afe.mpFrameSource = src;
			afe.mpProcessNode = new_nothrow VDFilterSystemProcessNode(src, mpBitmaps->mpScheduler, mProcessNodes);
			if (!afe.mpProcessNode)
				throw MyMemoryError();

			if (src->IsAccelerated())
				afe.mpProcessNode->AddToScheduler(accelScheduler);
			else if (mpBitmaps->mpProcessScheduler)
				afe.mpProcessNode->AddToScheduler(mpBitmaps->mpProcessScheduler);

			FilterInstance *fa = vdpoly_cast<FilterInstance *>(src);
			if (fa)
				fa->Start(mFilterStateFlags, it->mSources.data(), afe.mpProcessNode, mpBitmaps->mpAccelEngine);
			else
				src->Start(afe.mpProcessNode);

			afe.mpProcessNode->Unblock();

			pLastSource = src;

			if(fa && fa->IsTerminal()) {
				mbTrimmedChain = true;
				break;
			}
		}
	} catch(const MyError&) {
		// roll back previously initialized filters (similar to deinit)
		while(!mActiveFilters.empty()) {
			ActiveFilterEntry& afe = mActiveFilters.back();

			// Remove the process node from the scheduler first so that we know RunProcess() isn't
			// being called.
			if (afe.mpProcessNode) {
				afe.mpProcessNode->RemoveFromScheduler();
			}

			// Stop the frame source.
			afe.mpFrameSource->Stop();

			if (afe.mpProcessNode) {
				delete afe.mpProcessNode;
				afe.mpProcessNode = NULL;
			}

			mActiveFilters.pop_back();

		}

		throw;
	}

	mbFiltersInited = true;
	mpBitmaps->mpTailSource = pLastSource;
}

bool FilterSystem::RequestFrame(sint64 outputFrame, uint32 batchNumber, IVDFilterFrameClientRequest **creq) {
	if (!mpBitmaps->mpTailSource)
		return false;

	return mpBitmaps->mpTailSource->CreateRequest(outputFrame, false, batchNumber, creq);
}

void LogRunResultSync(IVDFilterFrameSource::RunResult rr, IVDFilterFrameSource* source) {
	VDDEBUG_FILTERSYS_DETAIL("FilterSystem[%s]: Sync processing occurred.\n", source->GetDebugDesc());
}

void LogRunResult(IVDFilterFrameSource::RunResult rr, IVDFilterFrameSource* source) {
	switch(rr) {
		case IVDFilterFrameSource::kRunResult_BatchLimited:
			VDDEBUG_FILTERSYS_DETAIL("FilterSystem[%s]: Run -> BatchLimited.\n", source->GetDebugDesc());
			break;

		case IVDFilterFrameSource::kRunResult_BatchLimitedWasActive:
			VDDEBUG_FILTERSYS_DETAIL("FilterSystem[%s]: Run -> BatchLimited+Activity.\n", source->GetDebugDesc());
			break;

		case IVDFilterFrameSource::kRunResult_Blocked:
			VDDEBUG_FILTERSYS_DETAIL("FilterSystem[%s]: Run -> Blocked.\n", source->GetDebugDesc());
			break;

		case IVDFilterFrameSource::kRunResult_BlockedWasActive:
			VDDEBUG_FILTERSYS_DETAIL("FilterSystem[%s]: Run -> Blocked+Activity.\n", source->GetDebugDesc());
			break;

		case IVDFilterFrameSource::kRunResult_Idle:
			VDDEBUG_FILTERSYS_DETAIL("FilterSystem[%s]: Run -> Idle.\n", source->GetDebugDesc());
			break;

		case IVDFilterFrameSource::kRunResult_IdleWasActive:
			VDDEBUG_FILTERSYS_DETAIL("FilterSystem[%s]: Run -> Idle+Activity.\n", source->GetDebugDesc());
			break;

		case IVDFilterFrameSource::kRunResult_Running:
			VDDEBUG_FILTERSYS_DETAIL("FilterSystem[%s]: Run -> Running.\n", source->GetDebugDesc());
			break;
	}
}

struct FilterSystem::RunState {
	const uint32 *batchNumberLimit;
	bool runToCompletion;

	bool didSomething;
	bool blocked;
	bool batchLimited;
};

int FilterSystem::RunNode(VDFilterSystemProcessNode* node, IVDFilterFrameSource* source, int index, RunState& state) {
	IVDFilterFrameSource::RunResult rr;
	int repeat = 0;
	
	if (!mpBitmaps->mpProcessScheduler && node->ServiceSync()) {
		rr = IVDFilterFrameSource::kRunResult_Running;
		LogRunResultSync(rr,source);
	} else {
		rr = source->RunRequests(state.batchNumberLimit,index);
		LogRunResult(rr,source);

		switch(rr) {
			case IVDFilterFrameSource::kRunResult_BlockedWasActive:
				rr = IVDFilterFrameSource::kRunResult_Blocked;
				state.didSomething = true;
				repeat = 1;
				break;

			case IVDFilterFrameSource::kRunResult_IdleWasActive:
				rr = IVDFilterFrameSource::kRunResult_Idle;
				state.didSomething = true;
				repeat = 1;
				break;
		}

		if (rr == IVDFilterFrameSource::kRunResult_Blocked) {
			// If we are single threaded and we can run the processing node for this filter, do
			// that now. This is *required* for the capture filter code to work correctly with
			// converter stages.
			if (!mpBitmaps->mpProcessScheduler && node->ServiceSync()) {
				rr = IVDFilterFrameSource::kRunResult_Running;
				LogRunResultSync(rr,source);
			} else {
				if (state.runToCompletion) state.blocked = true;
			}
		}
	}

	if (rr == IVDFilterFrameSource::kRunResult_Running) {
		if (!state.runToCompletion)	return -1;
		state.didSomething = true;
		repeat = 1;
	} else if (rr == IVDFilterFrameSource::kRunResult_BatchLimited) {
		VDASSERT(state.batchNumberLimit);
		state.batchLimited = true;
	}

	return repeat;
}

FilterSystem::RunResult FilterSystem::Run(const uint32 *batchNumberLimit, bool runToCompletion) {
	if (runToCompletion)
		batchNumberLimit = NULL;

	if (mbFiltersError)
		return kRunResult_Idle;

	if (!mbFiltersInited)
		return kRunResult_Idle;

	bool activity = false;
	RunState state;
	state.batchNumberLimit = batchNumberLimit;
	state.runToCompletion = runToCompletion;

	for(;;) {
		state.didSomething = false;
		state.blocked = false;
		state.batchLimited = false;

		{for(int index=0; index<mProcessNodes; index++){
			ActiveFilters::const_iterator it(mActiveFilters.end()), itEnd(mActiveFilters.begin());
			while(it != itEnd) {
				const ActiveFilterEntry& afe = *--it;

				try {
					while(1){
						int r = RunNode(afe.mpProcessNode,afe.mpFrameSource,index,state);
						if (r==-1) return kRunResult_Running;
						if (r==0) break;
					}
				} catch(const MyError&) {
					mbFiltersError = true;
					throw;
				}
			}
		}}

		if (state.blocked && !state.didSomething) {
			mpBitmaps->mpScheduler->Block();
			continue;
		}

		if (!state.didSomething)
			break;

		activity = true;
	}

	VDDEBUG_FILTERSYS_DETAIL("FilterSystem: Exiting (%s)\n", activity ? "running" : state.batchLimited ? "batch limited" : "idle");
	return activity ? kRunResult_Running : state.batchLimited ? kRunResult_BatchLimited : kRunResult_Idle;
}

void FilterSystem::Block() {
	mpBitmaps->mpScheduler->Block();
}

void FilterSystem::InvalidateCachedFrames(FilterInstance *startingFilter) {
	ActiveFilters::const_iterator it(mActiveFilters.begin()), itEnd(mActiveFilters.end());
	bool invalidating = !startingFilter;

	for(; it != itEnd; ++it) {
		IVDFilterFrameSource *fi = it->mpFrameSource;

		if (fi == startingFilter)
			invalidating = true;

		if (invalidating)
			fi->InvalidateAllCachedFrames();
	}
}

void FilterSystem::DumpStatus(VDTextOutputStream& os) {
	ActiveFilters::const_iterator it(mActiveFilters.begin()), itEnd(mActiveFilters.end());
	for(; it != itEnd; ++it) {
		IVDFilterFrameSource *fi = it->mpFrameSource;

		fi->DumpStatus(os);
	}
}

void FilterSystem::DeinitFilters() {
	// send all filters a 'stop'
	VDScheduler *accelScheduler = NULL;
	if (mpBitmaps->mpAccelEngine)
		accelScheduler = mpBitmaps->mpAccelEngine->GetScheduler();

	while(!mActiveFilters.empty()) {
		ActiveFilterEntry& afe = mActiveFilters.back();

		IVDFilterFrameSource *fi = afe.mpFrameSource;
		afe.mpProcessNode->RemoveFromScheduler();
		fi->Stop();

		if (afe.mpProcessNode) {
			delete afe.mpProcessNode;
			afe.mpProcessNode = NULL;
		}

		mActiveFilters.pop_back();
	}

	mFilters.clear();

	if (mpBitmaps->mpProcessScheduler) {
		mpBitmaps->mpProcessScheduler->BeginShutdown();

		mpBitmaps->mpProcessSchedulerThreadPool = NULL;
		mpBitmaps->mpProcessScheduler = NULL;
	}


	mpBitmaps->mAllocatorManager.Shutdown();
	mpBitmaps->mpTailSource = NULL;
	mpBitmaps->mpScheduler = NULL;

	if (mpBitmaps->mpAccelEngine) {
		mpBitmaps->mpAccelEngine->Shutdown();
		mpBitmaps->mpAccelEngine = NULL;
	}

	mbFiltersInited = false;
}

const VDPixmapLayout& FilterSystem::GetInputLayout() const {
	return mpBitmaps->mInitialBitmap.mPixmapLayout;
}

const VDPixmapLayout& FilterSystem::GetOutputLayout() const {
	return mpBitmaps->mFinalLayout;
}

bool FilterSystem::isRunning() const {
	return mbFiltersInited;
}

bool FilterSystem::isEmpty() const {
	return mActiveFilters.empty();
}

bool FilterSystem::IsThreadingActive() const {
	return mpBitmaps->mpProcessScheduler != 0;
}

uint32 FilterSystem::GetThreadCount() const {
	return mpBitmaps->mpProcessSchedulerThreadPool->GetThreadCount();
}

bool FilterSystem::GetDirectFrameMapping(VDPosition outputFrame, VDPosition& sourceFrame, int& sourceIndex) const {
	if (mbFiltersError)
		return false;

	if (!mbFiltersInited)
		return false;

	return mpBitmaps->mpTailSource->GetDirectMapping(outputFrame, sourceFrame, sourceIndex);
}

sint64 FilterSystem::GetSourceFrame(sint64 frame) const {
	ActiveFilters::const_iterator it(mActiveFilters.end()), itEnd(mActiveFilters.begin());
	while(it != itEnd) {
		IVDFilterFrameSource *fa = (--it)->mpFrameSource;

		frame = fa->GetSourceFrame(frame);
	}

	return frame;
}

sint64 FilterSystem::GetSymbolicFrame(sint64 outframe, IVDFilterFrameSource *source) const {
	if (!mbFiltersInited)
		return outframe;

	if (mbFiltersError)
		return outframe;

	return mpBitmaps->mpTailSource->GetSymbolicFrame(outframe, source);
}

sint64 FilterSystem::GetNearestUniqueFrame(sint64 outframe) const {
	if (!mbFiltersInited)
		return outframe;

	return mpBitmaps->mpTailSource->GetNearestUniqueFrame(outframe);
}

const VDFraction FilterSystem::GetOutputFrameRate() const {
	return mOutputFrameRate;
}

const VDFraction FilterSystem::GetOutputPixelAspect() const {
	return mOutputPixelAspect;
}

sint64 FilterSystem::GetOutputFrameCount() const {
	return mOutputFrameCount;
}

/////////////////////////////////////////////////////////////////////////////
//
//	FilterSystem::private_methods
//
/////////////////////////////////////////////////////////////////////////////

void FilterSystem::AllocateBuffers(uint32 lTotalBufferNeeded) {
	DeallocateBuffers();

	if (!(lpBuffer = (unsigned char *)VirtualAlloc(NULL, lTotalBufferNeeded+8, MEM_COMMIT, PAGE_READWRITE)))
		throw MyMemoryError();

	memset(lpBuffer, 0, lTotalBufferNeeded+8);
}

void FilterSystem::DeallocateBuffers() {
	if (mpBitmaps) {
		mpBitmaps->mAllocatorManager.Shutdown();
		mpBitmaps->mpSource = NULL;
	}

	if (lpBuffer) {
		VirtualFree(lpBuffer, 0, MEM_RELEASE);

		lpBuffer = NULL;
	}
}

void FilterSystem::AppendConversionFilter(StreamTail& tail, const VDPixmapLayout& dstLayout, bool normalize16) {
	vdrefptr<VDFilterFrameConverter> conv(new VDFilterFrameConverter);

	conv->filter_index = mFilters.size();
	conv->Init(tail.mpSrc, dstLayout, NULL, normalize16);
	conv->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager);
	conv->RegisterSourceAllocReqs(0, tail.mpProxy);
	tail.mpSrc = conv;
	FilterEntry& fe = mFilters.push_back();
	fe.mpFilter = conv;
	fe.mSources.resize(1, tail.mpSrc);
	tail.mpProxy = tail.mpSrc->GetOutputAllocatorProxy();
}

void FilterSystem::AppendAlignmentFilter(StreamTail& tail, const VDPixmapLayout& dstLayout, const VDPixmapLayout& srcLayout) {
	vdrefptr<VDFilterFrameConverter> conv(new VDFilterFrameConverter);

	conv->filter_index = mFilters.size();
	conv->Init(tail.mpSrc, dstLayout, &srcLayout, false);
	conv->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager);
	conv->RegisterSourceAllocReqs(0, tail.mpProxy);
	tail.mpSrc = conv;
	tail.mpProxy = conv->GetOutputAllocatorProxy();
	FilterEntry& fe = mFilters.push_back();
	fe.mpFilter = conv;
	fe.mSources.resize(1, tail.mpSrc);
}

void FilterSystem::AppendAccelUploadFilter(StreamTail& tail, const vdrect32& srcRect) {
	vdrefptr<VDFilterAccelUploader> conv(new VDFilterAccelUploader);
	VDPixmapLayout srcLayout(tail.mpSrc->GetOutputLayout());

	VDPixmapLayout layout;
	VDPixmapCreateLinearLayout(layout, nsVDPixmap::kPixFormat_XRGB8888, srcRect.width(), srcRect.height(), vdxa_align);

	int format = nsVDXPixmap::kPixFormat_VDXA_YUV;
	bool isRGB = false;
	if (srcLayout.format == nsVDXPixmap::kPixFormat_XRGB8888) {
		format = nsVDXPixmap::kPixFormat_VDXA_RGB;
		isRGB = true;
	}

	layout.format = format;
	layout.formatEx = format;

	srcLayout.data += srcRect.top * srcLayout.pitch + (isRGB ? srcRect.left << 2 : srcRect.left);
	srcLayout.data2 += srcRect.top * srcLayout.pitch2 + srcRect.left;
	srcLayout.data3 += srcRect.top * srcLayout.pitch3 + srcRect.left;
	srcLayout.w = srcRect.width();
	srcLayout.h = srcRect.height();

	conv->Init(mpBitmaps->mpAccelEngine, tail.mpSrc, layout, &srcLayout);
	conv->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager);
	conv->RegisterSourceAllocReqs(0, tail.mpProxy);
	tail.mpSrc = conv;
	tail.mpProxy = conv->GetOutputAllocatorProxy();
	FilterEntry& fe = mFilters.push_back();
	fe.mpFilter = conv;
	fe.mSources.resize(1, tail.mpSrc);
}

void FilterSystem::AppendAccelDownloadFilter(StreamTail& tail, const VDPixmapLayout& layout) {
	vdrefptr<VDFilterAccelDownloader> conv(new VDFilterAccelDownloader);
	conv->Init(mpBitmaps->mpAccelEngine, tail.mpSrc, layout, NULL);
	conv->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager);
	conv->RegisterSourceAllocReqs(0, tail.mpProxy);
	tail.mpSrc = conv;
	tail.mpProxy = conv->GetOutputAllocatorProxy();
	FilterEntry& fe = mFilters.push_back();
	fe.mpFilter = conv;
	fe.mSources.resize(1, tail.mpSrc);
}

void FilterSystem::AppendAccelConversionFilter(StreamTail& tail, int format) {
	vdrefptr<VDFilterAccelConverter> conv(new VDFilterAccelConverter);
	const VDPixmapLayout& prevLayout = tail.mpSrc->GetOutputLayout();

	VDPixmapLayout layout;
	VDPixmapCreateLinearLayout(layout, nsVDPixmap::kPixFormat_XRGB8888, prevLayout.w, prevLayout.h, vdxa_align);

	layout.format = format;
	layout.formatEx = format;

	conv->Init(mpBitmaps->mpAccelEngine, tail.mpSrc, layout, NULL);
	conv->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager);
	conv->RegisterSourceAllocReqs(0, tail.mpProxy);
	tail.mpSrc = conv;
	tail.mpProxy = tail.mpSrc->GetOutputAllocatorProxy();

	FilterEntry& fe = mFilters.push_back();
	fe.mpFilter = conv;
	fe.mSources.resize(1, tail.mpSrc);
}
