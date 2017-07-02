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

#ifndef f_OSHELPER_H
#define f_OSHELPER_H

#include <vd2/system/win32/miniwindows.h>
#include <vd2/system/VDString.h>

void Draw3DRect(VDZHDC hDC, int x, int y, int dx, int dy, bool inverted);

void VDShowHelp(VDZHWND hwnd, const wchar_t *filename = 0);

bool IsFilenameOnFATVolume(const wchar_t *pszFilename);

VDZHWND VDGetAncestorW32(VDZHWND hwnd, uint32 gaFlags);
VDStringW VDLoadStringW32(uint32 uID, bool doSubstitutions);
void VDSubstituteStrings(VDStringW& s);

void VDSetDataPath(const wchar_t *path);
const wchar_t *VDGetDataPath();
VDStringW VDGetLocalAppDataPath();

void VDCopyTextToClipboard(const wchar_t *s);

// Creates a 32-bit signature from the current date, time, and process ID. Not
// guaranteed to be unique, but reasonably non-conflicting.
uint32 VDCreateAutoSaveSignature();

void LaunchURL(const char *pURL);

enum VDSystemShutdownMode
{
	kVDSystemShutdownMode_Shutdown,
	kVDSystemShutdownMode_Hibernate,
	kVDSystemShutdownMode_Sleep
};

bool VDInitiateSystemShutdownWithUITimeout(VDSystemShutdownMode mode, const wchar_t *reason, uint32 timeout);
bool VDInitiateSystemShutdown(VDSystemShutdownMode mode);

class VDCPUUsageReader {
public:
	VDCPUUsageReader();
	~VDCPUUsageReader();

	void Init();
	void Shutdown();

	int read();

private:
	bool fNTMethod;
	VDZHKEY hkeyKernelCPU;

	uint64 kt_last;
	uint64 ut_last;
	uint64 skt_last;
	uint64 sut_last;
};

void VDEnableSampling(bool bEnable);

struct VDSamplingAutoProfileScope {
	VDSamplingAutoProfileScope() {
		VDEnableSampling(true);
	}
	~VDSamplingAutoProfileScope() {
		VDEnableSampling(false);
	}
};

#endif
