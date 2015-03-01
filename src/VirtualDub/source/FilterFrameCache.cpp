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
#include "FilterFrameCache.h"
#include "FilterFrame.h"

VDFilterFrameCache::VDFilterFrameCache() {
}

VDFilterFrameCache::~VDFilterFrameCache() {
	Flush();
}

void VDFilterFrameCache::Flush() {
	InvalidateAllFrames();

	while(!mFreeNodes.empty()) {
		VDFilterFrameBufferCacheNode *node = static_cast<VDFilterFrameBufferCacheNode *>(mFreeNodes.back());
		mFreeNodes.pop_back();

		delete node;
	}
}

bool VDFilterFrameCache::Lookup(sint64 key, VDFilterFrameBuffer **buffer) {
	int htidx = (unsigned)key % (kBufferHashTableSize - 1);
	HashNodes& hblist = mHashTable[htidx];

	for(HashNodes::iterator it(hblist.begin()), itEnd(hblist.end()); it != itEnd; ++it) {
		VDFilterFrameBufferCacheHashNode *hnode = *it;

		if (hnode->mKey == key) {
			VDFilterFrameBuffer *buf = hnode->mpBuffer;
			VDFilterFrameBufferCacheNode *node = static_cast<VDFilterFrameBufferCacheNode *>(hnode);

			VDASSERT(node->mpCache == this);
			VDASSERT(buf->GetCacheReference(this) == node);

			buf->AddRef();
			*buffer = buf;
			return true;
		}
	}

	return false;
}

void VDFilterFrameCache::Evict(VDFilterFrameBufferCacheLinkNode *cacheLink) {
	VDASSERT(cacheLink->mpCache == this);

	VDFilterFrameBufferCacheNode *node = static_cast<VDFilterFrameBufferCacheNode *>(cacheLink);
	node->mpBuffer->RemoveCacheReference(cacheLink);

	VDFilterFrameBufferCacheHashNode *hnode = static_cast<VDFilterFrameBufferCacheHashNode *>(node);
	HashNodes::unlink(*hnode);
	hnode->mListNodePrev = NULL;
	hnode->mListNodeNext = NULL;
	hnode->mpBuffer = NULL;

	mFreeNodes.push_back(node);
}

void VDFilterFrameCache::Add(VDFilterFrameBuffer *buf, sint64 key) {
	VDFilterFrameBufferCacheNode *hnode = AllocateNode();

	hnode->mKey = key;
	hnode->mpBuffer = buf;

	int htidx = (unsigned)hnode->mKey % (kBufferHashTableSize - 1);

	mHashTable[htidx].push_back(hnode);
	buf->AddCacheReference(hnode);
}

void VDFilterFrameCache::Remove(VDFilterFrameBuffer *buf) {
	VDFilterFrameBufferCacheNode *hnode = static_cast<VDFilterFrameBufferCacheNode *>(buf->GetCacheReference(this));

	if (hnode)
		Evict(hnode);
}

void VDFilterFrameCache::InvalidateAllFrames() {
	for(int htidx = 0; htidx < kBufferHashTableSize; ++htidx) {
		HashNodes& hblist = mHashTable[htidx];

		while(!hblist.empty()) {
			VDFilterFrameBufferCacheNode *hnode = static_cast<VDFilterFrameBufferCacheNode *>(hblist.back());

			Evict(hnode);
		}
	}
}

VDFilterFrameBufferCacheNode *VDFilterFrameCache::AllocateNode() {
	if (mFreeNodes.empty()) {
		VDFilterFrameBufferCacheNode *node = new VDFilterFrameBufferCacheNode;
		node->mpCache = this;
		return node;
	}

	VDFilterFrameBufferCacheNode *node = static_cast<VDFilterFrameBufferCacheNode *>(mFreeNodes.front());
	mFreeNodes.pop_front();

	VDASSERT(!node->mpBuffer);

	return node;
}

void VDFilterFrameCache::FreeNode(VDFilterFrameBufferCacheNode *node) {
	node->mpBuffer = NULL;

	mFreeNodes.push_front(node);
}
