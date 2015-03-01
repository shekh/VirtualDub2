#include "stdafx.h"
#include "vd2/VDLib/win32/FileMapping.h"
#include <windows.h>

VDFileMappingW32::VDFileMappingW32()
	: mpHandle(NULL)
{
}

VDFileMappingW32::~VDFileMappingW32() {
	Shutdown();
}

bool VDFileMappingW32::Init(uint32 bytes) {
	if (mpHandle)
		Shutdown();

	mpHandle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, bytes, NULL);
	if (!mpHandle)
		return false;

	return true;
}

void VDFileMappingW32::Shutdown() {
	if (mpHandle) {
		CloseHandle(mpHandle);
		mpHandle = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////

VDFileMappingViewW32::VDFileMappingViewW32()
	: mpView(NULL)
{
}

VDFileMappingViewW32::~VDFileMappingViewW32() {
	Shutdown();
}

bool VDFileMappingViewW32::Init(const VDFileMappingW32& mapping, uint64 offset, uint32 size) {
	Shutdown();

	HANDLE h = mapping.GetHandle();
	if (!h)
		return false;

	mpView = MapViewOfFile(h, FILE_MAP_WRITE, (uint32)(offset >> 32), (uint32)offset, size);
	if (!mpView)
		return false;

	return true;
}

void VDFileMappingViewW32::Shutdown() {
	if (mpView) {
		UnmapViewOfFile(mpView);
		mpView = NULL;
	}
}
