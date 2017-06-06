//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#include "stdafx.h"
#include <wtypes.h>
#include <winnt.h>
#include <vd2/system/win32/intrin.h>
#include <vd2/system/cpuaccel.h>

static long g_lCPUExtensionsEnabled;
static long g_lCPUExtensionsAvailable;

extern "C" {
	bool FPU_enabled, MMX_enabled, SSE_enabled, ISSE_enabled, SSE2_enabled;
};

#define IS_BIT_SET(bitfield, bit) ((bitfield) & (1<<(bit)) ? true : false)

#if (!defined(VD_CPU_X86) && !defined(VD_CPU_AMD64)) || defined(__MINGW32__)
long CPUCheckForExtensions() {
	return 0;
}
#else

#if defined(_MSC_VER) && (_MSC_FULL_VER >= 160040219)
unsigned __int64 get_xcr0(){
	return _xgetbv(0);  // VS2010 SP1 required.
}
#elif defined(_M_IX86)
unsigned __int64 __cdecl get_xcr0(){
		uint32 r0;
		uint32 r1;
	__asm {
		xor ecx, ecx
		__emit 0x0f		;xgetbv
		__emit 0x01
		__emit 0xd0
		mov dword ptr r0, eax
		mov dword ptr r1, edx
	}
	return (((unsigned __int64)r1) << 32) | r0;
}
#else
extern "C" bool get_xcr0();
#endif

long CPUCheckForExtensions() {
	long result = 0;
	int cpuinfo[4];

	__cpuid(cpuinfo, 1);
	if (IS_BIT_SET(cpuinfo[3], 0))
		result |= CPUF_SUPPORTS_FPU;
	if (IS_BIT_SET(cpuinfo[3], 23))
		result |= CPUF_SUPPORTS_MMX;
	if (IS_BIT_SET(cpuinfo[3], 25))
		result |= CPUF_SUPPORTS_SSE | CPUF_SUPPORTS_INTEGER_SSE;
	if (IS_BIT_SET(cpuinfo[3], 26))
		result |= CPUF_SUPPORTS_SSE2;
	if (IS_BIT_SET(cpuinfo[2], 0))
		result |= CPUF_SUPPORTS_SSE3;
	if (IS_BIT_SET(cpuinfo[2], 9))
		result |= CPUF_SUPPORTS_SSSE3;
	if (IS_BIT_SET(cpuinfo[2], 19))
		result |= CPUF_SUPPORTS_SSE41;
	if (IS_BIT_SET(cpuinfo[2], 20))
		result |= CPUF_SUPPORTS_SSE42;
	if (IS_BIT_SET(cpuinfo[2], 22))
		result |= CPUF_SUPPORTS_MOVBE;
	if (IS_BIT_SET(cpuinfo[2], 23))
		result |= CPUF_SUPPORTS_POPCNT;
	if (IS_BIT_SET(cpuinfo[2], 25))
		result |= CPUF_SUPPORTS_AES;
	if (IS_BIT_SET(cpuinfo[2], 29))
		result |= CPUF_SUPPORTS_F16C;
	// AVX
	bool xgetbv_supported = IS_BIT_SET(cpuinfo[2], 27);
	bool avx_supported = IS_BIT_SET(cpuinfo[2], 28);
	if (xgetbv_supported && avx_supported)
	{
		unsigned long long xgetbv0 = get_xcr0();
		if ((xgetbv0 & 0x6ull) == 0x6ull) {
			result |= CPUF_SUPPORTS_AVX;
			if (IS_BIT_SET(cpuinfo[2], 12))
				result |= CPUF_SUPPORTS_FMA3;
			__cpuid(cpuinfo, 7);
			if (IS_BIT_SET(cpuinfo[1], 5))
				result |= CPUF_SUPPORTS_AVX2;
		}
		if((xgetbv0 & (0x7ull << 5)) && // OPMASK: upper-256 enabled by OS
			 (xgetbv0 & (0x3ull << 1))) { // XMM/YMM enabled by OS
			// Verify that XCR0[7:5] = ‘111b’ (OPMASK state, upper 256-bit of ZMM0-ZMM15 and
			// ZMM16-ZMM31 state are enabled by OS)
			// and that XCR0[2:1] = ‘11b’ (XMM state and YMM state are enabled by OS).
			__cpuid(cpuinfo, 7);
			if (IS_BIT_SET(cpuinfo[1], 16))
				result |= CPUF_SUPPORTS_AVX512F;
			if (IS_BIT_SET(cpuinfo[1], 17))
				result |= CPUF_SUPPORTS_AVX512DQ;
			if (IS_BIT_SET(cpuinfo[1], 21))
				result |= CPUF_SUPPORTS_AVX512IFMA;
			if (IS_BIT_SET(cpuinfo[1], 26))
				result |= CPUF_SUPPORTS_AVX512PF;
			if (IS_BIT_SET(cpuinfo[1], 27))
				result |= CPUF_SUPPORTS_AVX512ER;
			if (IS_BIT_SET(cpuinfo[1], 28))
				result |= CPUF_SUPPORTS_AVX512CD;
			if (IS_BIT_SET(cpuinfo[1], 30))
				result |= CPUF_SUPPORTS_AVX512BW;
			if (IS_BIT_SET(cpuinfo[1], 31))
				result |= CPUF_SUPPORTS_AVX512VL;
			if (IS_BIT_SET(cpuinfo[2], 1)) // [2]!
				result |= CPUF_SUPPORTS_AVX512VBMI;
		}
	}

	// 3DNow!, 3DNow!, ISSE, FMA4
	__cpuid(cpuinfo, 0x80000000);   
	if (cpuinfo[0] >= 0x80000001)
	{
		__cpuid(cpuinfo, 0x80000001);

		if (IS_BIT_SET(cpuinfo[3], 31))
			result |= CPUF_SUPPORTS_3DNOW;

		if (IS_BIT_SET(cpuinfo[3], 30))
			result |= CPUF_SUPPORTS_3DNOW_EXT;

		if (IS_BIT_SET(cpuinfo[3], 22))
			result |= CPUF_SUPPORTS_INTEGER_SSE;

		if (result & CPUF_SUPPORTS_AVX) {
			if (IS_BIT_SET(cpuinfo[2], 16))
				result |= CPUF_SUPPORTS_FMA4;
		}
	}

	return result;	
}
#endif

long CPUEnableExtensions(long lEnableFlags) {
	g_lCPUExtensionsEnabled = lEnableFlags;

	MMX_enabled = !!(g_lCPUExtensionsEnabled & CPUF_SUPPORTS_MMX);
	FPU_enabled = !!(g_lCPUExtensionsEnabled & CPUF_SUPPORTS_FPU);
	SSE_enabled = !!(g_lCPUExtensionsEnabled & CPUF_SUPPORTS_SSE);
	ISSE_enabled = !!(g_lCPUExtensionsEnabled & CPUF_SUPPORTS_INTEGER_SSE);
	SSE2_enabled = !!(g_lCPUExtensionsEnabled & CPUF_SUPPORTS_SSE2);

	return g_lCPUExtensionsEnabled;
}

long CPUGetAvailableExtensions() {
	return g_lCPUExtensionsAvailable;
}

long CPUGetEnabledExtensions() {
	return g_lCPUExtensionsEnabled;
}

void VDCPUCleanupExtensions() {
#if defined(VD_CPU_X86)
	if (ISSE_enabled)
		_mm_sfence();

	if (MMX_enabled)
		_mm_empty();
#elif defined(VD_CPU_AMD64)
	_mm_sfence();
#endif
}
