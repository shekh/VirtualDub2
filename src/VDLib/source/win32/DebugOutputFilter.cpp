//	VirtualDub - Video processing and capture application
//	Application helper library
//	Copyright (C) 1998-2008 Avery Lee
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
#include <windows.h>
#include <vd2/system/win32/intrin.h>
#include <vd2/system/w32assist.h>
#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/thunk.h>
#include <vd2/system/vdstl.h>

class VDDebugOutputFilterW32 {
public:
	VDDebugOutputFilterW32();
	~VDDebugOutputFilterW32();

	bool Init(const char *filter);
	void Shutdown();

protected:
	BOOL InterceptODSA(LPCSTR);
	BOOL InterceptODSW(LPCWSTR);

	inline bool IsReturnAddressFiltered(void *addr);
	static uint32 HashModuleName(const VDStringSpanA& modname);

	bool mbThunksInited;
	VDFunctionThunk	*mpThunkA;
	VDFunctionThunk	*mpThunkW;

	typedef BOOL (WINAPI *ODSA)(LPCSTR);
	typedef BOOL (WINAPI *ODSW)(LPCWSTR);

	ODSA mpOldODSA;
	ODSW mpOldODSW;

	typedef vdfastvector<uint32> ExcludedModules;
	ExcludedModules mExcludedModules;

	enum {
		kUnknown, kEnabled, kDisabled
	};

	uintptr	mReturnAddressCache[2048];
};

VDDebugOutputFilterW32::VDDebugOutputFilterW32()
	: mbThunksInited(false)
	, mpThunkA(NULL)
	, mpThunkW(NULL)
{
	memset(mReturnAddressCache, 0, sizeof(mReturnAddressCache));
}

VDDebugOutputFilterW32::~VDDebugOutputFilterW32() {
	Shutdown();
}

bool VDDebugOutputFilterW32::Init(const char *filter) {
	VDStringRefA filterRef(filter);
	VDStringRefA token;

	bool invalid = false;
	for(;;) {
		if (!filterRef.split(L',', token)) {
			if (filterRef.empty())
				break;

			token = filterRef;
			filterRef = VDStringRefA();
		}

		if (token.empty()) {
			invalid = true;
			break;
		}

		if (token[0] != '-') {
			invalid = true;
			break;
		}

		mExcludedModules.push_back(HashModuleName(token.subspan(1, VDStringSpanA::npos)));
	}

	if (invalid)
		throw MyError("Invalid debug module filter: %s", filter);

	if (!VDInitThunkAllocator())
		return false;
	mbThunksInited = true;

	mpThunkA = VDCreateFunctionThunkFromMethod(this, &VDDebugOutputFilterW32::InterceptODSA, true);
	if (!mpThunkA) {
		Shutdown();
		return false;
	}

	mpThunkW = VDCreateFunctionThunkFromMethod(this, &VDDebugOutputFilterW32::InterceptODSW, true);
	if (!mpThunkW) {
		Shutdown();
		return false;
	}

	HMODULE kernel32 = GetModuleHandle("kernel32");

	VDPatchModuleExportTableW32(kernel32, "OutputDebugStringA", NULL, mpThunkA, (void *volatile *)&mpOldODSA);
	VDPatchModuleExportTableW32(kernel32, "OutputDebugStringW", NULL, mpThunkW, (void *volatile *)&mpOldODSW);

	return true;
}

void VDDebugOutputFilterW32::Shutdown() {
	if (mpThunkA) {
		VDDestroyFunctionThunk(mpThunkA);
		mpThunkA = NULL;
	}

	if (mpThunkW) {
		VDDestroyFunctionThunk(mpThunkW);
		mpThunkW = NULL;
	}

	if (mbThunksInited) {
		mbThunksInited = false;
		VDShutdownThunkAllocator();
	}
}

BOOL VDDebugOutputFilterW32::InterceptODSA(LPCSTR s) {
	if (IsReturnAddressFiltered(_ReturnAddress()))
		return TRUE;

	return mpOldODSA(s);
}

BOOL VDDebugOutputFilterW32::InterceptODSW(LPCWSTR s) {
	if (IsReturnAddressFiltered(_ReturnAddress()))
		return TRUE;

	return mpOldODSW(s);
}

bool VDDebugOutputFilterW32::IsReturnAddressFiltered(void *retaddr) {
	uint32 raval = (uint32)(uintptr)retaddr;
	uint32 rahash = raval & 2047;
	uintptr code = mReturnAddressCache[rahash];

	if (!((code ^ raval) & ~(uintptr)0x7FF))
		return (uint8)code == kDisabled;

	MEMORY_BASIC_INFORMATION meminfo = {0};
	uintptr abase = 0;
	bool validModule = false;
	if (VirtualQuery(retaddr, &meminfo, sizeof meminfo)) {
		abase = (uintptr)meminfo.AllocationBase;

		if (abase && meminfo.Type == MEM_IMAGE)
			validModule = true;
	}

	bool active = true;

	if (validModule) {
		char path[MAX_PATH];
		if (GetModuleFileNameA((HMODULE)meminfo.AllocationBase, path, MAX_PATH)) {
			path[MAX_PATH-1] = 0;

			const char *fn = VDFileSplitPath(path);
			const char *ext = VDFileSplitExt(fn);
			uint32 modhash = HashModuleName(VDStringSpanA(fn, ext));

			ExcludedModules::const_iterator it(mExcludedModules.begin()), itEnd(mExcludedModules.end());
			for(; it!=itEnd; ++it) {
				if (*it == modhash) {
					active = false;
					break;
				}
			}
		}
	}

	uint8 mcode = (active ? kEnabled : kDisabled);
	mReturnAddressCache[rahash] = (raval & ~(uintptr)0x7FF) + mcode;

	return mcode == kDisabled;
}

uint32 VDDebugOutputFilterW32::HashModuleName(const VDStringSpanA& modname) {
	VDStringSpanA::const_iterator it(modname.begin()), itEnd(modname.end());
	uint32 hash = 0;

	for(; it != itEnd; ++it) {
		uint8 c = *it;

		if ((uint8)(c - 0x61) < 26)
			c &= 0xdf;

		hash ^= c;
		hash += (hash >> 7);
		hash ^= (hash << 13);
	}

	return hash;
}


VDDebugOutputFilterW32 g_VDDebugOutputFilterW32;

void VDInitDebugOutputFilterW32(const char *filter) {
	g_VDDebugOutputFilterW32.Init(filter);
}