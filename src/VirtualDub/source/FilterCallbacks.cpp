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

#include "stdafx.h"
#include <vd2/system/cpuaccel.h>
#include "filters.h"
#include "resource.h"
#include "version.h"

extern HINSTANCE g_hInst;

static bool VDFilterCallbackIsFPUEnabled() {
	return !!FPU_enabled;
}

static bool VDFilterCallbackIsMMXEnabled() {
	return !!MMX_enabled;
}

static void VDFilterCallbackThrowExcept(const char *format, ...) {
	va_list val;
	MyError e;

	va_start(val, format);
	e.vsetf(format, val);
	va_end(val);

	throw e;
}

static void VDFilterCallbackThrowExceptMemory() {
	throw MyMemoryError();
}

// This is really disgusting...

struct VDXFilterVTbls {
	void *pvtblVBitmap;
};

static void VDFilterCallbackInitVTables(VDXFilterVTbls *pvtbls) {
	VBitmap tmp;
	pvtbls->pvtblVBitmap = *(void **)&tmp;
}

static long VDFilterCallbackGetCPUFlags() {
	return CPUGetEnabledExtensions();
}

static long VDFilterCallbackGetHostVersionInfo(char *buf, int len) {
	char tbuf[256];

	LoadString(g_hInst, IDS_TITLE_INITIAL, tbuf, sizeof tbuf);
	_snprintf(buf, len, tbuf, version_num,
#ifdef _DEBUG
		"debug"
#else
		"release"
#endif
		);

	return version_num;
}

VDXFilterFunctions g_VDFilterCallbacks={
	FilterAdd,
	FilterRemove,
	VDFilterCallbackIsFPUEnabled,
	VDFilterCallbackIsMMXEnabled,
	VDFilterCallbackInitVTables,
	VDFilterCallbackThrowExceptMemory,
	VDFilterCallbackThrowExcept,
	VDFilterCallbackGetCPUFlags,
	VDFilterCallbackGetHostVersionInfo,
};

FilterModInitFunctions g_FilterModCallbacks={
	FilterModAdd,
};
