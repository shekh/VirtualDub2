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

#ifndef f_VIRTUALDUB_FILTERSYSTEM_H
#define f_VIRTUALDUB_FILTERSYSTEM_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/list.h>
#include <vd2/system/fraction.h>
#include <vd2/system/refcount.h>
#include <vd2/system/vectors.h>
#include "filter.h"

class VDFilterChainDesc;
class FilterInstance;
class VDXFilterStateInfo;
class FilterSystemBitmap;
class VFBitmapInternal;
struct VDPixmap;
struct VDPixmapLayout;
struct VDPixmapFormatEx;
class IVDFilterFrameSource;
class IVDFilterFrameClientRequest;
class VDFilterFrameRequest;
class VDFilterSystemProcessNode;
class VDTextOutputStream;
class VDFilterFrameAllocatorProxy;

class IVDFilterSystemScheduler : public IVDRefCount {
public:
	virtual void Reschedule() = 0;
	virtual bool Block() = 0;
};

class FilterSystem {
	FilterSystem(const FilterSystem&);
	FilterSystem& operator=(const FilterSystem&);
public:
	FilterSystem();
	~FilterSystem();

	void SetAccelEnabled(bool enable);
	void SetVisualAccelDebugEnabled(bool enable);

	/// Set the number of async threads to use. -1 disables, 0 is auto.
	void SetAsyncThreadCount(sint32 threadsToUse);
	void SetAsyncThreadPriority(int priority);

	void prepareLinearChain(VDFilterChainDesc *desc, uint32 src_width, uint32 src_height, VDPixmapFormatEx src_format, const VDFraction& sourceFrameRate, sint64 sourceFrameCount, const VDFraction& sourcePixelAspect);
	void initLinearChain(IVDFilterSystemScheduler *scheduler, uint32 filterStateFlags, VDFilterChainDesc *desc, IVDFilterFrameSource *src, uint32 src_width, uint32 src_height, VDPixmapFormatEx src_format, const uint32 *palette, const VDFraction& sourceFrameRate, sint64 sourceFrameCount, const VDFraction& sourcePixelAspect);
	void ReadyFilters();

	bool RequestFrame(sint64 outputFrame, uint32 batchNumber, IVDFilterFrameClientRequest **creq);

	enum RunResult {
		kRunResult_Idle,		// All filters are idle.
		kRunResult_Running,		// There are still filters to run, and some can be run on this thread.
		kRunResult_Blocked,		// There are still filters to run, but all are waiting for asynchronous operation.
		kRunResult_BatchLimited	// There are still filters to run, but all ready ones are blocked due to the specified batch limit.
	};

	RunResult Run(const uint32 *batchNumberLimit, bool runToCompletion);
	void Block();

	void InvalidateCachedFrames(FilterInstance *startingFilter);
	
	void DumpStatus(VDTextOutputStream& os);

	void DeinitFilters();
	void DeallocateBuffers();
	const VDPixmapLayout& GetInputLayout() const;
	const VDPixmapLayout& GetOutputLayout() const;
	bool isRunning() const;
	bool isEmpty() const;
	bool isTrimmedChain() const { return mbTrimmedChain; }
	bool IsThreadingActive() const;
	uint32 GetThreadCount() const;

	bool GetDirectFrameMapping(VDPosition outputFrame, VDPosition& sourceFrame, int& sourceIndex) const;
	sint64	GetSourceFrame(sint64 outframe) const;
	sint64	GetSymbolicFrame(sint64 outframe, IVDFilterFrameSource *source) const;
	sint64	GetNearestUniqueFrame(sint64 outframe) const;

	const VDFraction GetOutputFrameRate() const;
	const VDFraction GetOutputPixelAspect() const;
	sint64	GetOutputFrameCount() const;

private:
	void AllocateVBitmaps(int count);
	void AllocateBuffers(uint32 lTotalBufferNeeded);

	struct Bitmaps;

	struct StreamTail {
		IVDFilterFrameSource *mpSrc;
		VDFilterFrameAllocatorProxy *mpProxy;
	};

	void AppendConversionFilter(StreamTail& tail, const VDPixmapLayout& dstLayout);
	void AppendAlignmentFilter(StreamTail& tail, const VDPixmapLayout& dstLayout, const VDPixmapLayout& srcLayout);
	void AppendAccelUploadFilter(StreamTail& tail, const vdrect32& srcCrop);
	void AppendAccelDownloadFilter(StreamTail& tail, const VDPixmapLayout& layout);
	void AppendAccelConversionFilter(StreamTail& tail, int format);

	bool	mbFiltersInited;
	bool	mbFiltersError;
	bool	mbFiltersUseAcceleration;
	bool	mbAccelDebugVisual;
	bool	mbAccelEnabled;
	bool	mbTrimmedChain;
	sint32	mThreadsRequested;
	sint32	mProcessNodes;
	int		mThreadPriority;

	VDFraction	mOutputFrameRate;
	VDFraction	mOutputPixelAspect;
	sint64		mOutputFrameCount;

	Bitmaps *mpBitmaps;

	unsigned char *lpBuffer;
	long lRequiredSize;
	uint32	mFilterStateFlags;

	struct FilterEntry {
		vdrefptr<IVDFilterFrameSource> mpFilter;
		vdfastvector<IVDFilterFrameSource *> mSources;
	};

	typedef vdvector<FilterEntry> Filters;
	Filters mFilters;

	struct ActiveFilterEntry {
		IVDFilterFrameSource *mpFrameSource;
		VDFilterSystemProcessNode *mpProcessNode;
	};

	typedef vdfastvector<ActiveFilterEntry> ActiveFilters;
	ActiveFilters mActiveFilters;

	uint32	mPalette[256];

	struct RunState;
	int RunNode(VDFilterSystemProcessNode* node, IVDFilterFrameSource* source, int index, RunState& state);
};

#endif
