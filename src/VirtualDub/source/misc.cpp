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

#include <ctype.h>

#include <wtypes.h>
#include <mmsystem.h>

#include "misc.h"
#include <vd2/system/cpuaccel.h>
#include <vd2/system/debug.h>
#include <vd2/system/filesys.h>
#include <vd2/system/log.h>
#include <vd2/system/registry.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>

#ifdef _M_IX86
	long __declspec(naked) MulDivTrunc(long a, long b, long c) {
		__asm {
			mov eax,[esp+4]
			imul dword ptr [esp+8]
			idiv dword ptr [esp+12]
			ret
		}
	}

	unsigned __declspec(naked) __stdcall MulDivUnsigned(unsigned a, unsigned b, unsigned c) {
		__asm {
			mov		eax,[esp+4]
			mov		ecx,[esp+12]
			mul		dword ptr [esp+8]
			shr		ecx,1
			add		eax,ecx
			adc		edx,0
			div		dword ptr [esp+12]
			ret		12
		}
	}
#else
	long MulDivTrunc(long a, long b, long c) {
		return (long)(((sint64)a * b) / c);
	}

	unsigned __stdcall MulDivUnsigned(unsigned a, unsigned b, unsigned c) {
		return (unsigned)(((uint64)a * b + 0x80000000) / c);
	}
#endif

int NearestLongValue(long v, const long *array, int array_size) {
	int i;

	for(i=1; i<array_size; i++)
		if (v*2 < array[i-1]+array[i])
			break;

	return i-1;
}

bool isEqualFOURCC(FOURCC fccA, FOURCC fccB) {
	int i;

	for(i=0; i<4; i++) {
		if (tolower((unsigned char)fccA) != tolower((unsigned char)fccB))
			return false;

		fccA>>=8;
		fccB>>=8;
	}

	return true;
}

bool isValidFOURCC(FOURCC fcc) {
	return isprint((unsigned char)(fcc>>24))
		&& isprint((unsigned char)(fcc>>16))
		&& isprint((unsigned char)(fcc>> 8))
		&& isprint((unsigned char)(fcc    ));
}

FOURCC toupperFOURCC(FOURCC fcc) {
	return(toupper((unsigned char)(fcc>>24)) << 24)
		| (toupper((unsigned char)(fcc>>16)) << 16)
		| (toupper((unsigned char)(fcc>> 8)) <<  8)
		| (toupper((unsigned char)(fcc    ))      );
}

#if defined(WIN32) && defined(_M_IX86)

	class VDExternalCallTrap : public IVDExternalCallTrap {
	public:
		virtual void OnMMXTrap(const wchar_t *context, const char *file, int line) {
			if (!context) {
				VDLog(kVDLogError, VDswprintf(L"Internal error: MMX state was active before entry to external code at (%hs:%d). "
											L"This indicates an uncaught bug either in an external driver or in VirtualDub itself "
											L"that could cause application instability.  Please report this problem to the author!",
											2,
											&file,
											&line
											));
			} else {
				if (sbDisableFurtherWarnings)
					return;

				VDLog(kVDLogWarning, VDswprintf(L"%ls returned to VirtualDub with the CPU's MMX unit still active. "
												L"This indicates a bug in that module which could cause application instability. "
												L"Please check with the module vendor for an updated version which addresses this problem. "
												L"(Trap location: %hs:%d)",
												3,
												&context,
												&file,
												&line));
				sbDisableFurtherWarnings = true;
			}
		}

		virtual void OnFPUTrap(const wchar_t *context, const char *file, int line, uint16 fpucw) {
			if (!context) {
#if 0			// Far too many drivers get this wrong... #@&*($@&#(*$
				VDLog(kVDLogError, VDswprintf(L"Internal error: Floating-point state was bad before entry to external code at (%hs:%d). "
											L"This indicates an uncaught bug either in an external driver or in VirtualDub itself "
											L"that could cause application instability.  Please report this problem to the author!\n"
											L"(FPU tag word = %04x)",
											3,
											&file,
											&line,
											&fpucw
											));
#endif
			} else {
				if (sbDisableFurtherWarnings)
					return;
#if 0			// Far too many drivers get this wrong... #@&*($@&#(*$
				VDLog(kVDLogWarning, VDswprintf(L"%ls returned to VirtualDub with the floating-point unit in an abnormal state. "
												L"This indicates a bug in that module which could cause application instability. "
												L"Please check with the module vendor for an updated version which addresses this problem. "
												L"(Trap location: %hs:%d, FPUCW = %04x)",
												4,
												&mpContext,
												&mpFile,
												&mLine,
												&fpucw));
				sbDisableFurtherWarnings = true;
#endif
			}
		}

		virtual void OnSSETrap(const wchar_t *context, const char *file, int line, uint32 mxcsr) {
			if (!context) {
				VDLog(kVDLogError, VDswprintf(L"Internal error: SSE state was bad before entry to external code at (%hs:%d). "
											L"This indicates an uncaught bug either in an external driver or in VirtualDub itself "
											L"that could cause application instability.  Please report this problem to the author!\n"
											L"(MXCSR = %08x)",
											3,
											&file,
											&line,
											&mxcsr
											));
			} else {
				if (sbDisableFurtherWarnings)
					return;

				VDLog(kVDLogWarning, VDswprintf(L"%ls returned to VirtualDub with the SSE floating-point unit in an abnormal state. "
												L"This indicates a bug in that module which could cause application instability. "
												L"Please check with the module vendor for an updated version which addresses this problem. "
												L"(Trap location: %hs:%d, MXCSR = %08x)",
												4,
												&context,
												&file,
												&line,
												&mxcsr));
				sbDisableFurtherWarnings = true;
			}
		}

		static bool sbDisableFurtherWarnings;
	} g_excallTrap;

	bool VDExternalCallTrap::sbDisableFurtherWarnings = false;

	void VDInitExternalCallTrap() {
		VDSetExternalCallTrap(&g_excallTrap);
	}
#else
	void VDInitExternalCallTrap() {}
#endif

char *strCify(const char *s, char* buf) {
	char c,*t = buf;

	while(c=*s++) {
		if (!isprint((unsigned char)c))
			t += sprintf(t, "\\x%02x", (int)c & 0xff);
		else {
			if (c=='"' || c=='\\')
				*t++ = '\\';
			*t++ = c;
		}
	}
	*t=0;

	return buf;
}

char *strCify(const char *s) {
	static char buf[2048];
  return strCify(s, buf);
}

VDStringA strCify(const VDStringA& s) {
	static char buf[2048];
  return VDStringA(strCify(s.c_str(), buf));
}

VDStringA VDEncodeScriptString(const VDStringSpanA& sa) {
	VDStringA out;

	// this is not very fast, but it's only used during script serialization
	for(VDStringA::const_iterator it(sa.begin()), itEnd(sa.end()); it != itEnd; ++it) {
		char c = *it;
		char buf[16];

		if (!isprint((unsigned char)c)) {
			sprintf(buf, "\\x%02x", (int)c & 0xff);
			out.append(buf);
		} else {
			if (c == '"' || c=='\\')
				out += '\\';
			out += c;
		}
	}

	return out;
}

VDStringA VDEncodeScriptString(const VDStringW& sw) {
	return VDEncodeScriptString(VDTextWToU8(sw));
}

HMODULE VDLoadVTuneDLLW32() {
	VDRegistryKey key("SOFTWARE\\Intel Corporation\\VTune(TM) Performance Environment\\6.0", true);

	if (key.isReady()) {
		VDStringW path;
		if (key.getString("SharedBaseInstallDir", path)) {
			const VDStringW path2(VDMakePath(path.c_str(), L"Analyzer\\Bin\\VTuneAPI.dll"));

			return LoadLibraryW(path2.c_str());
		}
	}

	return NULL;
}

extern bool g_bEnableVTuneProfiling;
void VDEnableSampling(bool bEnable) {
	if (g_bEnableVTuneProfiling) {
		static HMODULE hmodVTuneAPI = VDLoadVTuneDLLW32();
		if (!hmodVTuneAPI)
			return;

		static void (__cdecl *pVTunePauseSampling)() = (void(__cdecl*)())GetProcAddress(hmodVTuneAPI, "VTPauseSampling");
		static void (__cdecl *pVTuneResumeSampling)() = (void(__cdecl*)())GetProcAddress(hmodVTuneAPI, "VTResumeSampling");

		(bEnable ? pVTuneResumeSampling : pVTunePauseSampling)();
	}
}
