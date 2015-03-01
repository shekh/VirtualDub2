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

#ifndef f_VD2_FILTERFRAMEALLOCATORMANAGER_H
#define f_VD2_FILTERFRAMEALLOCATORMANAGER_H

#include <vd2/system/vdstl.h>

class IVDFilterFrameAllocator;
class VDFilterFrameAllocatorProxy;
class VDFilterAccelEngine;

class VDFilterFrameAllocatorManager {
	VDFilterFrameAllocatorManager(const VDFilterFrameAllocatorManager&);
	VDFilterFrameAllocatorManager& operator=(const VDFilterFrameAllocatorManager&);
public:
	VDFilterFrameAllocatorManager();
	~VDFilterFrameAllocatorManager();

	void Shutdown();

	void AddAllocatorProxy(VDFilterFrameAllocatorProxy *proxy);
	void AssignAllocators(VDFilterAccelEngine *accelEngine);

protected:
	void AssignAllocators(VDFilterAccelEngine *accelEngine, int mode);

	struct ProxyEntry {
		VDFilterFrameAllocatorProxy *mpProxy;
		IVDFilterFrameAllocator *mpAllocator;
		uint32 mMinSize;
		uint32 mMaxSize;
		uint32 mBorderWidth;
		uint32 mBorderHeight;
		ProxyEntry *mpParent;
	};

	struct ProxyEntrySort;

	typedef vdfastvector<ProxyEntry> Proxies;

	Proxies		mProxies[3];
};

#endif	// f_VD2_FILTERFRAMEALLOCATORMANAGER_H
