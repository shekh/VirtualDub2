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

#ifndef f_VD2_FILTERFRAMECACHE_H
#define f_VD2_FILTERFRAMECACHE_H

#include <vd2/system/vdstl.h>

#include "FilterFrame.h"

class VDFilterFrameBuffer;
class VDFilterFrameCache;

struct VDFilterFrameBufferCacheHashNode : public vdlist_node {
	VDFilterFrameBuffer *mpBuffer;
	sint64 mKey;
};

struct VDFilterFrameBufferCacheNode : public VDFilterFrameBufferCacheHashNode, public VDFilterFrameBufferCacheLinkNode {};

class VDFilterFrameCache {
	VDFilterFrameCache(const VDFilterFrameCache&);
	VDFilterFrameCache& operator=(const VDFilterFrameCache&);
public:
	VDFilterFrameCache();
	~VDFilterFrameCache();

	void Flush();

	void Add(VDFilterFrameBuffer *buf, sint64 key);
	void Remove(VDFilterFrameBuffer *buf);

	bool Lookup(sint64 key, VDFilterFrameBuffer **buffer);
	void Evict(VDFilterFrameBufferCacheLinkNode *cacheLink);

	void InvalidateAllFrames();

protected:
	VDFilterFrameBufferCacheNode *AllocateNode();
	void FreeNode(VDFilterFrameBufferCacheNode *node);

	enum { kBufferHashTableSize = 64 };
	typedef vdlist<VDFilterFrameBufferCacheHashNode> HashNodes;
	HashNodes mHashTable[kBufferHashTableSize];

	HashNodes mFreeNodes;
};

#endif	// f_VD2_FILTERFRAMECACHE_H

