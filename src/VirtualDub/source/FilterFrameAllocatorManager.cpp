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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "stdafx.h"
#include "FilterFrameAllocatorManager.h"
#include "FilterFrameAllocatorProxy.h"
#include "FilterFrameAllocatorMemory.h"
#include "FilterAccelFrameAllocator.h"

struct VDFilterFrameAllocatorManager::ProxyEntrySort {
	bool operator()(ProxyEntry *x, ProxyEntry *y) const {
		return x->mMinSize < y->mMinSize;
	}
};

VDFilterFrameAllocatorManager::VDFilterFrameAllocatorManager() {
	VDASSERTCT(sizeof(mProxies)/sizeof(mProxies[0]) == VDFilterFrameAllocatorProxy::kAccelModeCount);
}

VDFilterFrameAllocatorManager::~VDFilterFrameAllocatorManager() {
	Shutdown();
}

void VDFilterFrameAllocatorManager::Shutdown() {
	for(int mode=0; mode<3; ++mode) {
		Proxies& proxies = mProxies[mode];

		while(!proxies.empty()) {
			ProxyEntry& ent = proxies.back();

			if (ent.mpAllocator)
				ent.mpAllocator->Release();

			proxies.pop_back();
		}
	}
}

void VDFilterFrameAllocatorManager::AddAllocatorProxy(VDFilterFrameAllocatorProxy *proxy) {
	ProxyEntry& ent = mProxies[proxy->GetAccelerationRequirement()].push_back();

	ent.mpProxy = proxy;
	ent.mpAllocator = NULL;
	ent.mMinSize = 0;
	ent.mMaxSize = 0;
	ent.mBorderWidth = 0;
	ent.mBorderHeight = 0;
	ent.mpParent = NULL;
}

void VDFilterFrameAllocatorManager::AssignAllocators(VDFilterAccelEngine *accelEngine) {
	AssignAllocators(NULL, 0);
	AssignAllocators(accelEngine, 1);
	AssignAllocators(accelEngine, 2);
}

void VDFilterFrameAllocatorManager::AssignAllocators(VDFilterAccelEngine *accelEngine, int accelMode) {
	Proxies& proxies = mProxies[accelMode];

	// Push down all size requirements through links.
	for(Proxies::iterator it(proxies.begin()), itEnd(proxies.end()); it != itEnd; ++it) {
		ProxyEntry& ent = *it;
		VDFilterFrameAllocatorProxy *proxy = ent.mpProxy;
		VDFilterFrameAllocatorProxy *link = NULL;
		VDFilterFrameAllocatorProxy *linkNext;
		
		linkNext = proxy->GetLink();

		if (linkNext) {
			do {
				link = linkNext;
				linkNext = link->GetLink();
			} while(linkNext);
		}

		if (link) {
			proxy->Link(link);
			link->AddRequirements(proxy);
		}
	}

	// Push size requirements back up through links. This has to be a separate pass
	// since multiple proxies may have the same link and we won't know the requirement
	// on the master.
	for(Proxies::iterator it(proxies.begin()), itEnd(proxies.end()); it != itEnd; ++it) {
		ProxyEntry& ent = *it;
		VDFilterFrameAllocatorProxy *proxy = ent.mpProxy;
		VDFilterFrameAllocatorProxy *link = proxy->GetLink();
		
		if (link)
			proxy->AddRequirements(link);
	}

	for(Proxies::iterator it(proxies.begin()), itEnd(proxies.end()); it != itEnd; ++it) {
		ProxyEntry& ent = *it;

		uint32 req = ent.mpProxy->GetSizeRequirement();
		ent.mMinSize = req;
		ent.mMaxSize = req;
		ent.mBorderWidth = ent.mpProxy->GetBorderWidth();
		ent.mBorderHeight = ent.mpProxy->GetBorderHeight();
	}

	typedef vdfastvector<ProxyEntry *> ProxyRefs;
	ProxyRefs proxyRefs(proxies.size());

	int n = proxies.size();
	for(int i=0; i<n; ++i) {
		proxyRefs[i] = &proxies[i];
	}

	std::sort(proxyRefs.begin(), proxyRefs.end(), ProxyEntrySort());

	for(;;) {
		int bestMerge = -1;
		float bestMergeRatio = 0.75f;

		for(int i=0; i<n-1; ++i) {
			ProxyEntry& ent1 = *proxyRefs[i];
			ProxyEntry& ent2 = *proxyRefs[i+1];

			if (accelMode) {
				if (ent1.mMinSize == ent2.mMaxSize) {
					bestMerge = i;
					break;
				}
			} else {
				VDASSERT(ent1.mMaxSize <= ent2.mMinSize);

				float mergeRatio = 0.f;

				if (ent2.mMaxSize)
					mergeRatio = (float)ent1.mMinSize / (float)ent2.mMaxSize;

				if (mergeRatio > bestMergeRatio) {
					bestMerge = i;
					bestMergeRatio = mergeRatio;
				}
			}
		}

		if (bestMerge < 0)
			break;

		ProxyEntry& dst = *proxyRefs[bestMerge];
		ProxyEntry& src = *proxyRefs[bestMerge+1];

		VDASSERT(src.mMaxSize >= dst.mMaxSize);

		src.mpParent = &dst;
		dst.mMaxSize = src.mMaxSize;

		if (dst.mBorderWidth < src.mBorderWidth)
			dst.mBorderWidth = src.mBorderWidth;

		if (dst.mBorderHeight < src.mBorderHeight)
			dst.mBorderHeight = src.mBorderHeight;

		proxyRefs.erase(proxyRefs.begin() + bestMerge + 1);
		--n;
	}

	// init allocators
	for(int i=0; i<n; ++i) {
		ProxyEntry& ent = *proxyRefs[i];

		if (accelMode) {
			vdrefptr<VDFilterAccelFrameAllocator> allocMem(new VDFilterAccelFrameAllocator);
			allocMem->AddSizeRequirement(ent.mMaxSize);
			allocMem->AddBorderRequirement(ent.mBorderWidth, ent.mBorderHeight);
			allocMem->Init(0, 0x7fffffff, accelEngine);

			ent.mpAllocator = allocMem.release();
		} else {
			vdrefptr<VDFilterFrameAllocatorMemory> allocMem(new VDFilterFrameAllocatorMemory);
			allocMem->AddSizeRequirement(ent.mMaxSize);
			allocMem->Init(0, 0x7fffffff);

			ent.mpAllocator = allocMem.release();
		}
	}

	// set allocators for all proxies
	for(Proxies::iterator it(proxies.begin()), itEnd(proxies.end()); it != itEnd; ++it) {
		ProxyEntry& ent = *it;
		ProxyEntry *src = &ent;

		while(src->mpParent)
			src = src->mpParent;

		ent.mpProxy->SetAllocator(src->mpAllocator);
	}
}
