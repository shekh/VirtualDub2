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

///////////////////////////////////////////////////////////////////////////
//
// VirtualDub in-place crash analysis module
//
// This module is rather ugly since it attacks the broken process from a
// number of directions and produces several kinds of output to aid remote
// debugging:
//
//  * A disassembly around the failed instruction, with symbol lookups.
//  * A register dump.
//  * A detailed context stack specifying what the thread was doing
//    at the time of the failure.
//  * A call stack for the failed thread with symbols and explicit hacks
//    to tunnel through trashed or non-framed stacks.
//  * A list of recent checkpoints for all threads.
//
// Since this module runs in the context of the failed thread, there are
// a number of precautions necessary during its execution:
//
//  * No heap allocation, either explicit (malloc/new) or implicit
//    (printf).  Directly allocated virtual memory (VirtualAlloc) is
//    used instead.
//  * No stdio or iostreams.  See the first bullet.
//  * No calls to services that may acquire locks.  We may have failed
//    while holding one or suspended a thread that has one.
//  * All operations involving complex application structures are guarded
//    with SEH to avoid crashes in the crash handler.
//
// Experience has shown that the VirtualDub's crash handler is usually
// successful in producing a detailed, complete dump.  To test the crash
// handler, use the undocumented /fsck switch.  This function uses a
// direct SEH entry rather than the unhandled exception filter, so /fsck
// may be used under a debugger.
//
///////////////////////////////////////////////////////////////////////////

// DO NOT USE stdio.h!  printf() calls malloc()!
//#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>		// for alloca()

#include <windows.h>
#ifndef _M_AMD64
#include <tlhelp32.h>
#endif

#include "resource.h"
#include "crash.h"
#include "disasm.h"
#include "oshelper.h"
#include "helpfile.h"
#include "gui.h"
#include <vd2/system/thread.h>
#include <vd2/system/list.h>
#include <vd2/system/filesys.h>
#include <vd2/system/tls.h>
#include <vd2/system/debugx86.h>
#include <vd2/system/protscope.h>
#include <vd2/system/w32assist.h>

///////////////////////////////////////////////////////////////////////////

#define CODE_WINDOW (256)

///////////////////////////////////////////////////////////////////////////

#ifdef _M_AMD64
	#define PTR_08lx	"%08I64x"
#else
	#define PTR_08lx	"%08x"
#endif

extern HINSTANCE g_hInst;
extern "C" {
	extern unsigned long version_num;
	extern const char version_time[];
	extern const char version_buildmachine[];
}

static CodeDisassemblyWindow *g_pcdw;

struct VDDebugInfoContext {
	void *pRawBlock;

	int nBuildNumber;

	const unsigned char *pRVAHeap;
	unsigned	nFirstRVA;

	const char *pClassNameHeap;
	const char *pFuncNameHeap;
	const unsigned long (*pSegments)[2];
	int		nSegments;
};


static VDDebugInfoContext g_debugInfo;

VDStringW	g_VDCrashDumpPathW;
VDStringA	g_VDCrashDumpPathA;

///////////////////////////////////////////////////////////////////////////

void VDSetCrashDumpPath(const wchar_t *s) {
	g_VDCrashDumpPathW = VDMakePath(s, L"crashinfo.txt");
	g_VDCrashDumpPathA = VDTextWToA(g_VDCrashDumpPathW);
}

///////////////////////////////////////////////////////////////////////////

namespace {
	enum VDCrashResponse {
		kVDCrashResponse_Exit,
		kVDCrashResponse_Debug,
		kVDCrashResponse_DisplayAdvancedDialog
	};
}

VDCrashResponse VDDisplayFriendlyCrashDialog(HWND hwndParent, HANDLE hThread, const EXCEPTION_POINTERS *pExc, const char *pszScopeInfo, bool allowForcedExit);
bool VDDisplayAdvancedCrashDialog(HWND hwndParent, HANDLE hThread, const EXCEPTION_POINTERS *pExc, const char *pszScopeInfo, bool allowForcedExit);

///////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG

void checkfpustack(const char *file, const int line) throw() {
#ifndef _M_AMD64
	static const char szFPUProblemCaption[]="FPU/MMX internal problem";
	static const char szFPUProblemMessage[]="The FPU stack wasn't empty!  Tagword = %04x\nFile: %s, line %d";
	static bool seenmsg=false;

	char	buf[128];
	unsigned short tagword;

	if (seenmsg)
		return;

	__asm fnstenv buf

	tagword = *(unsigned short *)(buf + 8);

	if (tagword != 0xffff) {
		wsprintf(buf, szFPUProblemMessage, tagword, file, line);
		MessageBox(NULL, buf, szFPUProblemCaption, MB_OK);
		seenmsg=true;
	}
#endif
}

extern "C" void *_ReturnAddress();
#pragma intrinsic(_ReturnAddress)

#ifdef VD_CPU_AMD64
	#define ENCODED_RETURN_ADDRESS ((int)_ReturnAddress() - (int)&__ImageBase)
#else
	#define ENCODED_RETURN_ADDRESS ((int)_ReturnAddress())
#endif

void *operator new(size_t bytes) {
	static const char fname[]="stack trace";

	return _malloc_dbg(bytes, _NORMAL_BLOCK, fname, ENCODED_RETURN_ADDRESS);
}

void *operator new(size_t bytes, const std::nothrow_t&) {
	static const char fname[]="stack trace";

	return _malloc_dbg(bytes, _NORMAL_BLOCK, fname, ENCODED_RETURN_ADDRESS);
}

void *operator new[](size_t bytes) {
	static const char fname[]="stack trace";

	return _malloc_dbg(bytes, _NORMAL_BLOCK, fname, ENCODED_RETURN_ADDRESS);
}

void *operator new[](size_t bytes, const std::nothrow_t&) {
	static const char fname[]="stack trace";

	return _malloc_dbg(bytes, _NORMAL_BLOCK, fname, ENCODED_RETURN_ADDRESS);
}

#endif

#if 0
void __declspec(naked) stackcheck(void *&sp) {
	static const char g_szStackHemorrhage[]="WARNING: Thread is hemorrhaging stack space!\n";

	__asm {
		mov		eax,[esp+4]
		mov		ecx,[eax]
		or		ecx,ecx
		jnz		started
		mov		[eax],esp
		ret
started:
		sub		ecx,esp
		mov		eax,ecx
		sar		ecx,31
		xor		eax,ecx
		sub		eax,ecx
		cmp		eax,128
		jb		ok
		push	offset g_szStackHemorrhage
		call	dword ptr [OutputDebugString]
		int		3
ok:
		ret
	}
}
#endif

namespace {
#ifdef _M_AMD64
	static const uint8 kVDSUEFPatch[]={
		0x33, 0xC0,				// XOR EAX, EAX
		0xC3					// RET
	};
#else
	static const uint8 kVDSUEFPatch[]={
		0x33, 0xC0,				// XOR EAX, EAX
		0xC2, 0x04, 0x00		// RET 4
	};
#endif

	bool g_VDSUEFPatched;
	uint8 g_VDSUEFPatchSave[sizeof kVDSUEFPatch];
}

void VDPatchSetUnhandledExceptionFilter() {
	// Some DLLs, most notably MSCOREE.DLL, steal the UnhandledExceptionFilter hook.
	// Bad DLL! We patch SetUnhandledExceptionFilter() to prevent this.
	if (g_VDSUEFPatched)
		return;

	g_VDSUEFPatched = true;

	// don't attempt to patch system DLLs on Windows 98
	if (VDIsWindowsNT()) {
		HMODULE hmodKernel32 = GetModuleHandleA("kernel32");
		FARPROC fpSUEF = GetProcAddress(hmodKernel32, "SetUnhandledExceptionFilter");

		DWORD oldProtect;
		if (VirtualProtect(fpSUEF, sizeof kVDSUEFPatch, PAGE_EXECUTE_READWRITE, &oldProtect)) {
			memcpy(g_VDSUEFPatchSave, fpSUEF, sizeof kVDSUEFPatch);
			memcpy(fpSUEF, kVDSUEFPatch, sizeof kVDSUEFPatch);
			VirtualProtect(fpSUEF, sizeof kVDSUEFPatch, oldProtect, &oldProtect);
		}
	}
}

void VDUnpatchSetUnhandledExceptionFilter() {
	if (!g_VDSUEFPatched)
		return;

	g_VDSUEFPatched = false;

	HMODULE hmodKernel32 = GetModuleHandleA("kernel32");
	FARPROC fpSUEF = GetProcAddress(hmodKernel32, "SetUnhandledExceptionFilter");

	DWORD oldProtect;
	if (VirtualProtect(fpSUEF, sizeof kVDSUEFPatch, PAGE_EXECUTE_READWRITE, &oldProtect)) {
		if (!memcmp(fpSUEF, kVDSUEFPatch, sizeof kVDSUEFPatch)) {
			memcpy(fpSUEF, g_VDSUEFPatchSave, sizeof kVDSUEFPatch);
		}

		VirtualProtect(fpSUEF, sizeof kVDSUEFPatch, oldProtect, &oldProtect);
	}
}

//////////////////////////////////////////////////////////////////////////////
//
//	Module list helpers
//
//////////////////////////////////////////////////////////////////////////////

static const char *CrashGetModuleBaseName(HMODULE hmod, char *pszBaseName) {
	char szPath1[MAX_PATH];
	char szPath2[MAX_PATH];

	__try {
		DWORD dw;
		char *pszFile, *period = NULL;

		if (!GetModuleFileName(hmod, szPath1, sizeof szPath1))
			return NULL;

		dw = GetFullPathName(szPath1, sizeof szPath2, szPath2, &pszFile);

		if (!dw || dw>sizeof szPath2)
			return NULL;

		strcpy(pszBaseName, pszFile);

		pszFile = pszBaseName;

		while(*pszFile++)
			if (pszFile[-1]=='.')
				period = pszFile-1;

		if (period)
			*period = 0;
	} __except(1) {
		return NULL;
	}

	return pszBaseName;
}

//////////////////////////////////////////////////////////////////////////////

struct ModuleInfo {
	const char *name;
	uintptr base, size;
};

// ARRGH.  Where's psapi.h?!?

struct Win32ModuleInfo {
	LPVOID base;
	DWORD size;
	LPVOID entry;
};

typedef BOOL (__stdcall *PENUMPROCESSMODULES)(HANDLE, HMODULE *, DWORD, LPDWORD);
typedef DWORD (__stdcall *PGETMODULEBASENAME)(HANDLE, HMODULE, LPTSTR, DWORD);
typedef BOOL (__stdcall *PGETMODULEINFORMATION)(HANDLE, HMODULE, Win32ModuleInfo *, DWORD);

#ifndef _M_AMD64
typedef HANDLE (__stdcall *PCREATETOOLHELP32SNAPSHOT)(DWORD, DWORD);
typedef BOOL (WINAPI *PMODULE32FIRST)(HANDLE, LPMODULEENTRY32);
typedef BOOL (WINAPI *PMODULE32NEXT)(HANDLE, LPMODULEENTRY32);
#endif

static ModuleInfo *CrashGetModules(void *&ptr) {
	void *pMem = VirtualAlloc(NULL, 65536, MEM_COMMIT, PAGE_READWRITE);

	if (!pMem) {
		ptr = NULL;
		return NULL;
	}

	// This sucks.  If we're running under Windows 9x, we must use
	// TOOLHELP.DLL to get the module list.  Under Windows NT, we must
	// use PSAPI.DLL.  With Windows 2000, we can use both (but prefer
	// PSAPI.DLL).

	HMODULE hmodPSAPI = LoadLibrary("psapi.dll");

	if (hmodPSAPI) {
		// Using PSAPI.DLL.  Call EnumProcessModules(), then GetModuleFileNameEx()
		// and GetModuleInformation().

		PENUMPROCESSMODULES pEnumProcessModules = (PENUMPROCESSMODULES)GetProcAddress(hmodPSAPI, "EnumProcessModules");
		PGETMODULEBASENAME pGetModuleBaseName = (PGETMODULEBASENAME)GetProcAddress(hmodPSAPI, "GetModuleBaseNameA");
		PGETMODULEINFORMATION pGetModuleInformation = (PGETMODULEINFORMATION)GetProcAddress(hmodPSAPI, "GetModuleInformation");
		HMODULE *pModules, *pModules0 = (HMODULE *)((char *)pMem + 0xF000);
		DWORD cbNeeded;

		if (pEnumProcessModules && pGetModuleBaseName && pGetModuleInformation
			&& pEnumProcessModules(GetCurrentProcess(), pModules0, 0x1000, &cbNeeded)) {

			ModuleInfo *pMod, *pMod0;
			char *pszHeap = (char *)pMem, *pszHeapLimit;

			if (cbNeeded > 0x1000) cbNeeded = 0x1000;

			pModules = (HMODULE *)((char *)pMem + 0x10000 - cbNeeded);
			memmove(pModules, pModules0, cbNeeded);

			pMod = pMod0 = (ModuleInfo *)((char *)pMem + 0x10000 - sizeof(ModuleInfo) * (cbNeeded / sizeof(HMODULE) + 1));
			pszHeapLimit = (char *)pMod;

			do {
				HMODULE hCurMod = *pModules++;
				Win32ModuleInfo mi;

				if (pGetModuleBaseName(GetCurrentProcess(), hCurMod, pszHeap, pszHeapLimit - pszHeap)
					&& pGetModuleInformation(GetCurrentProcess(), hCurMod, &mi, sizeof mi)) {

					char *period = NULL;

					pMod->name = pszHeap;

					while(*pszHeap++)
						if (pszHeap[-1] == '.')
							period = pszHeap-1;

					if (period) {
						*period = 0;
						pszHeap = period+1;
					}

					pMod->base = (uintptr)mi.base;
					pMod->size = mi.size;
					++pMod;
				}
			} while((cbNeeded -= sizeof(HMODULE *)) > 0);

			pMod->name = NULL;

			FreeLibrary(hmodPSAPI);
			ptr = pMem;
			return pMod0;
		}

		FreeLibrary(hmodPSAPI);
	}
#ifndef _M_AMD64
	else {
		// No PSAPI.  Use the ToolHelp functions in KERNEL.

		HMODULE hmodKERNEL32 = LoadLibrary("kernel32.dll");

		PCREATETOOLHELP32SNAPSHOT pCreateToolhelp32Snapshot = (PCREATETOOLHELP32SNAPSHOT)GetProcAddress(hmodKERNEL32, "CreateToolhelp32Snapshot");
		PMODULE32FIRST pModule32First = (PMODULE32FIRST)GetProcAddress(hmodKERNEL32, "Module32First");
		PMODULE32NEXT pModule32Next = (PMODULE32NEXT)GetProcAddress(hmodKERNEL32, "Module32Next");
		HANDLE hSnap;

		if (pCreateToolhelp32Snapshot && pModule32First && pModule32Next) {
			if ((HANDLE)-1 != (hSnap = pCreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0))) {
				ModuleInfo *pModInfo = (ModuleInfo *)((char *)pMem + 65536);
				char *pszHeap = (char *)pMem;
				MODULEENTRY32 me;

				--pModInfo;
				pModInfo->name = NULL;

				me.dwSize = sizeof(MODULEENTRY32);

				if (pModule32First(hSnap, &me))
					do {
						if (pszHeap+strlen(me.szModule) >= (char *)(pModInfo - 1))
							break;

						strcpy(pszHeap, me.szModule);

						--pModInfo;
						pModInfo->name = pszHeap;

						char *period = NULL;

						while(*pszHeap++);
							if (pszHeap[-1]=='.')
								period = pszHeap-1;

						if (period) {
							*period = 0;
							pszHeap = period+1;
						}

						pModInfo->base = (unsigned long)me.modBaseAddr;
						pModInfo->size = me.modBaseSize;

					} while(pModule32Next(hSnap, &me));

				CloseHandle(hSnap);

				FreeLibrary(hmodKERNEL32);

				ptr = pMem;
				return pModInfo;
			}
		}

		FreeLibrary(hmodKERNEL32);
	}
#endif

	VirtualFree(pMem, 0, MEM_RELEASE);

	ptr = NULL;
	return NULL;
}

///////////////////////////////////////////////////////////////////////////
//
//	info from Portable Executable/Common Object File Format (PE/COFF) spec

typedef unsigned short ushort;
typedef unsigned long ulong;

struct PEHeader {
	ulong		signature;
	ushort		machine;
	ushort		sections;
	ulong		timestamp;
	ulong		symbol_table;
	ulong		symbols;
	ushort		opthdr_size;
	ushort		characteristics;
};

struct PESectionHeader {
	char		name[8];
	ulong		virtsize;
	ulong		virtaddr;
	ulong		rawsize;
	ulong		rawptr;
	ulong		relocptr;
	ulong		linenoptr;
	ushort		reloc_cnt;
	ushort		lineno_cnt;
	ulong		characteristics;
};

struct PEExportDirectory {
	ulong		flags;
	ulong		timestamp;
	ushort		major;
	ushort		minor;
	ulong		nameptr;
	ulong		ordbase;
	ulong		addrtbl_cnt;
	ulong		nametbl_cnt;
	ulong		addrtbl_ptr;
	ulong		nametbl_ptr;
	ulong		ordtbl_ptr;
};

struct PE32OptionalHeader {
	ushort		magic;					// 0
	char		major_linker_ver;		// 2
	char		minor_linker_ver;		// 3
	ulong		codesize;				// 4
	ulong		idatasize;				// 8
	ulong		udatasize;				// 12
	ulong		entrypoint;				// 16
	ulong		codebase;				// 20
	ulong		database;				// 24
	ulong		imagebase;				// 28
	ulong		section_align;			// 32
	ulong		file_align;				// 36
	ushort		majoros;				// 40
	ushort		minoros;				// 42
	ushort		majorimage;				// 44
	ushort		minorimage;				// 46
	ushort		majorsubsys;			// 48
	ushort		minorsubsys;			// 50
	ulong		reserved;				// 52
	ulong		imagesize;				// 56
	ulong		hdrsize;				// 60
	ulong		checksum;				// 64
	ushort		subsystem;				// 68
	ushort		characteristics;		// 70
	ulong		stackreserve;			// 72
	ulong		stackcommit;			// 76
	ulong		heapreserve;			// 80
	ulong		heapcommit;				// 84
	ulong		loaderflags;			// 88
	ulong		dictentries;			// 92

	// Not part of header, but it's convienent here

	ulong		export_RVA;				// 96
	ulong		export_size;			// 100
};

struct PE32PlusOptionalHeader {
	ushort		magic;					// 0
	char		major_linker_ver;		// 2
	char		minor_linker_ver;		// 3
	ulong		codesize;				// 4
	ulong		idatasize;				// 8
	ulong		udatasize;				// 12
	ulong		entrypoint;				// 16
	ulong		codebase;				// 20
	__int64		imagebase;				// 24
	ulong		section_align;			// 32
	ulong		file_align;				// 36
	ushort		majoros;				// 40
	ushort		minoros;				// 42
	ushort		majorimage;				// 44
	ushort		minorimage;				// 46
	ushort		majorsubsys;			// 48
	ushort		minorsubsys;			// 50
	ulong		reserved;				// 52
	ulong		imagesize;				// 56
	ulong		hdrsize;				// 60
	ulong		checksum;				// 64
	ushort		subsystem;				// 68
	ushort		characteristics;		// 70
	__int64		stackreserve;			// 72
	__int64		stackcommit;			// 80
	__int64		heapreserve;			// 88
	__int64		heapcommit;				// 96
	ulong		loaderflags;			// 104
	ulong		dictentries;			// 108

	// Not part of header, but it's convienent here

	ulong		export_RVA;				// 112
	ulong		export_size;			// 116
};

static const char *CrashLookupExport(HMODULE hmod, unsigned long addr, unsigned long &fnbase) {
	char *pBase = (char *)hmod;

	// The PEheader offset is at hmod+0x3c.  Add the size of the optional header
	// to step to the section headers.

	PEHeader *pHeader = (PEHeader *)(pBase + ((long *)hmod)[15]);

	if (pHeader->signature != 'EP')
		return NULL;

#if 0
	PESectionHeader *pSHdrs = (PESectionHeader *)((char *)pHeader + sizeof(PEHeader) + pHeader->opthdr_size);

	// Scan down the section headers and look for ".edata"

	int i;

	for(i=0; i<pHeader->sections; i++) {
		MessageBox(NULL, pSHdrs[i].name, "section", MB_OK);
		if (!memcmp(pSHdrs[i].name, ".edata", 6))
			break;
	}

	if (i >= pHeader->sections)
		return NULL;
#endif

	// Verify the optional structure.

	PEExportDirectory *pExportDir;

	if (pHeader->opthdr_size < 104)
		return NULL;

	switch(*(short *)((char *)pHeader + sizeof(PEHeader))) {
	case 0x10b:		// PE32
		{
			PE32OptionalHeader *pOpt = (PE32OptionalHeader *)((char *)pHeader + sizeof(PEHeader));

			if (pOpt->dictentries < 1)
				return NULL;

			pExportDir = (PEExportDirectory *)(pBase + pOpt->export_RVA);
		}
		break;
	case 0x20b:		// PE32+
		{
			PE32PlusOptionalHeader *pOpt = (PE32PlusOptionalHeader *)((char *)pHeader + sizeof(PEHeader));

			if (pOpt->dictentries < 1)
				return NULL;

			pExportDir = (PEExportDirectory *)(pBase + pOpt->export_RVA);
		}
		break;

	default:
		return NULL;
	}

	// Hmmm... no exports?

	if ((char *)pExportDir == pBase)
		return NULL;

	// Find the location of the export information.

	ulong *pNameTbl = (ulong *)(pBase + pExportDir->nametbl_ptr);
	ulong *pAddrTbl = (ulong *)(pBase + pExportDir->addrtbl_ptr);
	ushort *pOrdTbl = (ushort *)(pBase + pExportDir->ordtbl_ptr);

	// Scan exports.

	const char *pszName = NULL;
	ulong bestdelta = 0xFFFFFFFF;
	int i;

	addr -= (ulong)pBase;

	for(i=0; i<pExportDir->nametbl_cnt; i++) {
		ulong fnaddr;
		int idx;

		idx = pOrdTbl[i];
		fnaddr = pAddrTbl[idx];

		if (addr >= fnaddr) {
			ulong delta = addr - fnaddr;

			if (delta < bestdelta) {
				bestdelta = delta;
				fnbase = fnaddr;

				if (pNameTbl[i])
					pszName = pBase + pNameTbl[i];
				else {
					static char buf[8];

					wsprintf(buf, "ord%d", pOrdTbl[i]);
					pszName = buf;
				}

			}
		}
	}

	return pszName;
}

static bool LookupModuleByAddress(ModuleInfo& mi, char *szTemp, const ModuleInfo *pMods, uintptr addr, MEMORY_BASIC_INFORMATION *pmeminfo) {
	mi.name = NULL;

	if (pMods) {
		while(pMods->name) {
			if ((uintptr)(addr - pMods->base) < pMods->size)
				break;

			++pMods;
		}

		mi = *pMods;
	} else {
		MEMORY_BASIC_INFORMATION meminfo;

		if (!pmeminfo) {
			if (VirtualQuery((void *)addr, &meminfo, sizeof meminfo))
				pmeminfo = &meminfo;
		}

		// Well, something failed, or we didn't have either PSAPI.DLL or ToolHelp
		// to play with.  So we'll use a nastier method instead.

		if (pmeminfo) {
			mi.base = (uintptr)pmeminfo->AllocationBase;
			mi.name = CrashGetModuleBaseName((HMODULE)mi.base, szTemp);
		}
	}

	return mi.name != 0;
}

///////////////////////////////////////////////////////////////////////////

static bool IsExecutableProtection(DWORD dwProtect) {
	MEMORY_BASIC_INFORMATION meminfo;

	// Windows NT/2000 allows Execute permissions, but Win9x seems to
	// rip it off.  So we query the permissions on our own code block,
	// and use it to determine if READONLY/READWRITE should be
	// considered 'executable.'

	VirtualQuery(IsExecutableProtection, &meminfo, sizeof meminfo);

	switch((unsigned char)dwProtect) {
	case PAGE_READONLY:				// *sigh* Win9x...
	case PAGE_READWRITE:			// *sigh*
		return meminfo.Protect==PAGE_READONLY || meminfo.Protect==PAGE_READWRITE;

	case PAGE_EXECUTE:
	case PAGE_EXECUTE_READ:
	case PAGE_EXECUTE_READWRITE:
	case PAGE_EXECUTE_WRITECOPY:
		return true;
	}
	return false;
}



///////////////////////////////////////////////////////////////////////////

__declspec(thread) VDProtectedAutoScope *volatile g_pVDProtectedScopeLink;

VDProtectedAutoScope *VDGetProtectedScopeLink() {
	return g_pVDProtectedScopeLink;
}

void VDSetProtectedScopeLink(VDProtectedAutoScope *p) {
	g_pVDProtectedScopeLink = p;
}

void VDInitProtectedScopeHook() {
	g_pVDGetProtectedScopeLink = VDGetProtectedScopeLink;
	g_pVDSetProtectedScopeLink = VDSetProtectedScopeLink;
}

///////////////////////////////////////////////////////////////////////////
//
//	Nina's per-thread debug logs are really handy, so I back-ported
//	them to 1.x.  These are lightweight in comparison, however.
//

VirtualDubThreadState __declspec(thread) g_PerThreadState;
__declspec(thread)
struct {
	ListNode node;
	VirtualDubThreadState *pState;
} g_PerThreadStateNode;

class VirtualDubThreadStateNode : public ListNode2<VirtualDubThreadStateNode> {
public:
	VirtualDubThreadState *pState;
};

static CRITICAL_SECTION g_csPerThreadState;
static List2<VirtualDubThreadStateNode> g_listPerThreadState;
static LONG g_nThreadsTrackedMinusOne = -1;

void VDThreadInitHandler(bool bInitThread, const char *debugName) {
	if (bInitThread) {
		DWORD dwThreadId = GetCurrentThreadId();

		if (!InterlockedIncrement(&g_nThreadsTrackedMinusOne)) {
			InitializeCriticalSection(&g_csPerThreadState);
		}

		EnterCriticalSection(&g_csPerThreadState);

		if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), (HANDLE *)&g_PerThreadState.hThread, NULL, FALSE, DUPLICATE_SAME_ACCESS)) {

			g_PerThreadState.pszThreadName = debugName;
			g_PerThreadState.dwThreadId = dwThreadId;

			g_PerThreadStateNode.pState = &g_PerThreadState;
			g_listPerThreadState.AddTail((ListNode2<VirtualDubThreadStateNode> *)&g_PerThreadStateNode);
		}

		LeaveCriticalSection(&g_csPerThreadState);
	} else {
		EnterCriticalSection(&g_csPerThreadState);

		((ListNode2<VirtualDubThreadStateNode> *)&g_PerThreadStateNode)->Remove();

		LeaveCriticalSection(&g_csPerThreadState);

		CloseHandle((HANDLE)g_PerThreadState.hThread);

		InterlockedDecrement(&g_nThreadsTrackedMinusOne);
	}
}

///////////////////////////////////////////////////////////////////////////

class VDVirtAllocBlock {
public:
	VDVirtAllocBlock(unsigned size) : mpBlock(VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE)) {}
	~VDVirtAllocBlock() {
		VirtualFree(mpBlock, 0, MEM_RELEASE);
	}

	void *addr() { return mpBlock; }

protected:
	void *mpBlock;
};

///////////////////////////////////////////////////////////////////////////

static const struct ExceptionLookup {
	DWORD	code;
	const char *name;
} exceptions[]={
	{	EXCEPTION_ACCESS_VIOLATION,			"Access Violation"		},
	{	EXCEPTION_BREAKPOINT,				"Breakpoint"			},
	{	EXCEPTION_FLT_DENORMAL_OPERAND,		"FP Denormal Operand"	},
	{	EXCEPTION_FLT_DIVIDE_BY_ZERO,		"FP Divide-by-Zero"		},
	{	EXCEPTION_FLT_INEXACT_RESULT,		"FP Inexact Result"		},
	{	EXCEPTION_FLT_INVALID_OPERATION,	"FP Invalid Operation"	},
	{	EXCEPTION_FLT_OVERFLOW,				"FP Overflow",			},
	{	EXCEPTION_FLT_STACK_CHECK,			"FP Stack Check",		},
	{	EXCEPTION_FLT_UNDERFLOW,			"FP Underflow",			},
	{	EXCEPTION_INT_DIVIDE_BY_ZERO,		"Integer Divide-by-Zero",	},
	{	EXCEPTION_INT_OVERFLOW,				"Integer Overflow",		},
	{	EXCEPTION_PRIV_INSTRUCTION,			"Privileged Instruction",	},
	{	EXCEPTION_ILLEGAL_INSTRUCTION,		"Illegal instruction"	},
	{	EXCEPTION_INVALID_HANDLE,			"Invalid handle"		},
	{	EXCEPTION_STACK_OVERFLOW,			"Stack overflow"		},
	{	0xe06d7363,							"Unhandled Microsoft C++ Exception",	},
			// hmm... '_msc'... gee, who would have thought?
	{	NULL	},
};

long VDDebugInfoLookupRVA(VDDebugInfoContext *pctx, unsigned rva, char *buf, int buflen);
bool VDDebugInfoInitFromMemory(VDDebugInfoContext *pctx, const void *_src);
bool VDDebugInfoInitFromFile(VDDebugInfoContext *pctx, const char *pszFilename);
void VDDebugInfoDeinit(VDDebugInfoContext *pctx);

static void SpliceProgramPath(char *buf, int bufsiz, const char *fn) {
	char tbuf[MAX_PATH];
	char *pszFile;

	GetModuleFileName(NULL, tbuf, sizeof tbuf);
	GetFullPathName(tbuf, bufsiz, buf, &pszFile);
	strcpy(pszFile, fn);
}

static long CrashSymLookup(VDDisassemblyContext *pctx, unsigned long virtAddr, char *buf, int buf_len) {
	if (!g_debugInfo.pRVAHeap)
		return -1;

	return VDDebugInfoLookupRVA(&g_debugInfo, virtAddr, buf, buf_len);
}

class VDProtectedScopeBuffer : public IVDProtectedScopeOutput, public VDVirtAllocBlock {
public:
	VDProtectedScopeBuffer(unsigned size) : VDVirtAllocBlock(size), mpNext((char *)mpBlock), mpBlockEnd((char *)mpBlock) {
		// if there is no memory available, collapse the end pointer
		if (mpBlockEnd)
			mpBlockEnd += size;
	}

	void write(const char *s) {
		size_t l = strlen(s);

		if (mpBlockEnd - mpNext < l + 1)		// '+1' is so we always have space for a null
			return;		// not enough memory

		memcpy(mpNext, s, l);
		mpNext += l;
	}

	void writef(const char *s, ...) {
		char buf[1024];
		va_list val;
		va_start(val, s);
		wvsprintf(buf, s, val);
		va_end(val);
		write(buf);
	}

	const char *c_str() {
		if (!mpBlock)
			return "[out of memory]";

		*mpNext = 0;

		return (const char *)mpBlock;
	}

protected:
	char *mpBlockEnd;
	char *mpNext;
};

static void VDDebugDumpCrashContext(EXCEPTION_POINTERS *pExc, IVDProtectedScopeOutput& out) {
	const char *pszExceptionName;
	const EXCEPTION_RECORD& exr = *pExc->ExceptionRecord;

	switch(exr.ExceptionCode) {
	case EXCEPTION_ACCESS_VIOLATION:
		pszExceptionName = "An out-of-bounds memory access (access violation) occurred";
		break;
	case EXCEPTION_BREAKPOINT:
		pszExceptionName = "A stray breakpoint occurred";
		break;
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
		pszExceptionName = "An integer division by zero occurred";
		break;
	case EXCEPTION_PRIV_INSTRUCTION:
		pszExceptionName = "A privileged instruction or unaligned SSE/SSE2 access occurred";
		break;
	case EXCEPTION_ILLEGAL_INSTRUCTION:
#ifdef _M_IX86
		switch(VDGetInstructionTypeX86((void *)pExc->ContextRecord->Eip)) {
		case kX86Inst3DNow:		pszExceptionName = "A 3DNow! (K6/Athlon) instruction not supported by the CPU was executed";				break;
		case kX86InstMMX:		pszExceptionName = "An MMX instruction not supported by the CPU was executed";								break;
		case kX86InstMMX2:		pszExceptionName = "An integer SSE (Pentium III/Athlon) instruction not supported by the CPU was executed";	break;
		case kX86InstSSE:		pszExceptionName = "A floating-point SSE (Pentium III/Athlon XP) instruction not supported by the CPU was executed";	break;
		case kX86InstSSE2:		pszExceptionName = "An SSE2 (Pentium 4/Athlon 64) instruction not supported by the CPU was executed";					break;
		case kX86InstP6:		pszExceptionName = "A Pentium Pro instruction not supported by the CPU was executed";						break;
		case kX86InstUnknown:	pszExceptionName = "An instruction not supported by the CPU was executed";									break;
		}
#else
		pszExceptionName = "An instruction not supported by the CPU was executed";
#endif
		break;
	default:
		pszExceptionName = "An exception occurred";
	}

	out.write(pszExceptionName);

	ModuleInfo mi;
	char szName[MAX_PATH];

#ifdef _M_AMD64
	if (LookupModuleByAddress(mi, szName, NULL, (uint32)pExc->ContextRecord->Rip, NULL))
#else
	if (LookupModuleByAddress(mi, szName, NULL, (uint32)pExc->ContextRecord->Eip, NULL))
#endif
		out.writef(" in module '%.64s'.", mi.name);
	else
#ifdef _M_AMD64
		out.writef(" at " PTR_08lx ".", pExc->ContextRecord->Rip);
#else
		out.writef(" at " PTR_08lx ".", pExc->ContextRecord->Eip);
#endif

	if (exr.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && exr.NumberParameters >= 2) {
		if (exr.ExceptionInformation[0])
			out.writef("..\r\n...writing address %p.", (void *)exr.ExceptionInformation[1]);
		else
			out.writef("..\r\n...reading address %p.", (void *)exr.ExceptionInformation[1]);
	}

	__try {
		for(VDProtectedAutoScope *pScope = g_pVDGetProtectedScopeLink(); pScope; pScope = pScope->mpLink) {
			out.write("..\r\n...while ");
			pScope->Write(out);
			out.writef(" (%.64s:%d).", VDFileSplitPath(pScope->mpFile), pScope->mLine);
		}
	} __except(EXCEPTION_EXECUTE_HANDLER) {
	}
}

class VDCrashUIThread : public VDThread {
public:
	VDCrashUIThread(HANDLE hThread, const EXCEPTION_POINTERS *pExc, const char *scopeinfo, bool allowForcedExit)
		: mhThread(hThread)
		, mpExc(pExc)
		, mpScopeInfo(scopeinfo)
		, mbAllowForcedExit(allowForcedExit)
	{
	}

	void ThreadRun();

protected:
	HANDLE mhThread;
	const EXCEPTION_POINTERS *mpExc;
	const char *mpScopeInfo;
	const bool mbAllowForcedExit;
};

void VDCrashUIThread::ThreadRun() {
	switch(VDDisplayFriendlyCrashDialog(NULL, mhThread, mpExc, mpScopeInfo, mbAllowForcedExit)) {
		case kVDCrashResponse_Debug:
			break;
		case kVDCrashResponse_DisplayAdvancedDialog:
			VDDisplayAdvancedCrashDialog(NULL, mhThread, mpExc, mpScopeInfo, mbAllowForcedExit);
			break;
		case kVDCrashResponse_Exit:
			if (!mbAllowForcedExit)
				return;
			TerminateProcess(GetCurrentProcess(), mpExc->ExceptionRecord->ExceptionCode);
			break;
	}
}

LONG __stdcall CrashHandler(EXCEPTION_POINTERS *pExc, bool allowForcedExit) {
	VDUnpatchSetUnhandledExceptionFilter();
	SetUnhandledExceptionFilter(NULL);

	/////////////////////////
	//
	// QUICKLY: SUSPEND ALL THREADS THAT AREN'T US.

	EnterCriticalSection(&g_csPerThreadState);

	try {
		DWORD dwCurrentThread = GetCurrentThreadId();

		for(List2<VirtualDubThreadStateNode>::fwit it = g_listPerThreadState.begin(); it; ++it) {
			const VirtualDubThreadState *pState = it->pState;

			if (pState->dwThreadId && pState->dwThreadId != dwCurrentThread) {
				SuspendThread((HANDLE)pState->hThread);
			}
		}
	} catch(...) {
	}

	LeaveCriticalSection(&g_csPerThreadState);

	// Unwind protected scopes.

	VDProtectedScopeBuffer	scopeinfo(65536);
	VDDebugDumpCrashContext(pExc, scopeinfo);

	/////////////////////////

	static char buf[CODE_WINDOW+16];
	HANDLE hprMe = GetCurrentProcess();
	void *lpBaseAddress = pExc->ExceptionRecord->ExceptionAddress;
	char *lpAddr = (char *)((uintptr)lpBaseAddress & -32);

	memset(buf, 0, sizeof buf);

	if ((unsigned long)lpAddr > CODE_WINDOW/2)
		lpAddr -= CODE_WINDOW/2;
	else
		lpAddr = NULL;

	if (!ReadProcessMemory(hprMe, lpAddr, buf, CODE_WINDOW, NULL)) {
		int i;

		for(i=0; i<CODE_WINDOW; i+=32)
			if (!ReadProcessMemory(hprMe, lpAddr+i, buf+i, 32, NULL))
				memset(buf+i, 0, 32);
	}

	CodeDisassemblyWindow cdw(buf, CODE_WINDOW, (char *)(buf-lpAddr), lpAddr);

	g_pcdw = &cdw;

	cdw.setFaultAddress(lpBaseAddress);

	// Attempt to read debug file.

	bool bSuccess;

	if (cdw.vdc.pExtraData) {
		bSuccess = VDDebugInfoInitFromMemory(&g_debugInfo, cdw.vdc.pExtraData);
	} else {
#ifdef __INTEL_COMPILER		// P4 build
		SpliceProgramPath(buf, sizeof buf, "VeedubP4.vdi");
		bSuccess = VDDebugInfoInitFromFile(&g_debugInfo, buf);
#elif defined(_M_AMD64)
		SpliceProgramPath(buf, sizeof buf, "Veedub64.vdi");
		bSuccess = VDDebugInfoInitFromFile(&g_debugInfo, buf);
#else						// General build
		SpliceProgramPath(buf, sizeof buf, "VirtualDub.vdi");
		bSuccess = VDDebugInfoInitFromFile(&g_debugInfo, buf);
		if (!bSuccess) {
			SpliceProgramPath(buf, sizeof buf, "VirtualD.vdi");
			bSuccess = VDDebugInfoInitFromFile(&g_debugInfo, buf);
		}
#endif
	}

	cdw.vdc.pSymLookup = CrashSymLookup;

	cdw.parse();

	// Disconnect the modeless dialog hook.
	VDDeinstallModelessDialogHookW32();

	// Display "friendly" crash dialog.
	HANDLE hProcess = GetCurrentProcess();
	HANDLE hThread;

	VDVERIFY(DuplicateHandle(hProcess, GetCurrentThread(), hProcess, &hThread, 0, FALSE, DUPLICATE_SAME_ACCESS));

	VDCrashUIThread crashThread(hThread, pExc, scopeinfo.c_str(), allowForcedExit);

	crashThread.ThreadStart();
	crashThread.ThreadWait();

	VDDebugInfoDeinit(&g_debugInfo);

	// resume all threads so we can clear deadlocks

	EnterCriticalSection(&g_csPerThreadState);

	try {
		DWORD dwCurrentThread = GetCurrentThreadId();

		for(List2<VirtualDubThreadStateNode>::fwit it = g_listPerThreadState.begin(); it; ++it) {
			const VirtualDubThreadState *pState = it->pState;

			if (pState->dwThreadId && pState->dwThreadId != dwCurrentThread) {
				ResumeThread((HANDLE)pState->hThread);
			}
		}
	} catch(...) {
	}

	LeaveCriticalSection(&g_csPerThreadState);

	// invoke normal OS handler

	if (allowForcedExit)
		return UnhandledExceptionFilter(pExc);

	return EXCEPTION_EXECUTE_HANDLER;
}

LONG __stdcall CrashHandlerHook(EXCEPTION_POINTERS *pExc) {
	return CrashHandler(pExc, true);
}

///////////////////////////////////////////////////////////////////////////

class VDDebugCrashTextOutput {
public:
	virtual void Write(const char *s) = 0;

	void WriteF(const char *format, ...) {
		char buf[256];
		va_list val;

		va_start(val, format);
		wvsprintf(buf, format, val);
		va_end(val);

		Write(buf);
	}
};

class VDDebugCrashTextOutputFile : public VDDebugCrashTextOutput {
public:
	VDDebugCrashTextOutputFile(const char *pszFilename, const wchar_t *pwszFilename)
		: mhFile(
			VDIsWindowsNT() ? CreateFileW(pwszFilename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
							: CreateFileA(pszFilename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)
		)
		, mNext(0)
		, mbError(mhFile == INVALID_HANDLE_VALUE)
	{
	}

	~VDDebugCrashTextOutputFile() {
		Finalize();
	}

	void Write(const char *s) {
		while(char c = *s++) {
			if (c == '\n')
				Put('\r');
			Put(c);
		}
	}

	void Finalize() {
		if (mhFile != INVALID_HANDLE_VALUE) {
			Flush();
			if (!CloseHandle(mhFile))
				mbError = true;
			mhFile = INVALID_HANDLE_VALUE;
		}
	}

	bool operator!() const { return mbError; }

protected:
	void Put(char c) {
		if (mNext >= kBufferSize)
			Flush();

		mBuffer[mNext++] = c;
	}

	void Flush() {
		if (mhFile != INVALID_HANDLE_VALUE) {
			DWORD actual;
			if (!WriteFile(mhFile, mBuffer, mNext, &actual, NULL))
				mbError = true;
		}
		mNext = 0;
	}

	enum { kBufferSize = 256 };

	HANDLE			mhFile;
	unsigned		mNext;
	char			mBuffer[kBufferSize];
	bool			mbError;
};

class VDDebugCrashTextOutputWindow : public VDDebugCrashTextOutput {
public:
	VDDebugCrashTextOutputWindow(HWND hwnd) : mhwnd(hwnd), mNext(0) {}
	~VDDebugCrashTextOutputWindow() {
		Flush();
	}

protected:
	void Write(const char *s) {
		while(char c = *s++) {
			if (mNext >= kBufferSize - 1)
				break;

			if (c == '\n')
				c = ' ';

			mBuffer[mNext++] = c;
		}
	}

protected:
	void Flush() {
		if (mNext) {
			mBuffer[mNext] = 0;
			SetWindowText(mhwnd, mBuffer);
			mNext = 0;
		}
	}

	enum { kBufferSize = 256 };
	const HWND	mhwnd;
	unsigned	mNext;
	char		mBuffer[kBufferSize];
};

class VDDebugCrashTextOutputListbox : public VDDebugCrashTextOutput {
public:
	VDDebugCrashTextOutputListbox(HWND hwnd) : mhwnd(hwnd), mNext(0) {}
	~VDDebugCrashTextOutputListbox() {
		Flush();
	}

protected:
	void Write(const char *s) {
		while(char c = *s++) {
			if (mNext >= kBufferSize - 1)
				Flush();

			if (c == '\n') {
				Flush();
				continue;
			}

			mBuffer[mNext++] = c;
		}
	}

protected:
	void Flush() {
		if (mNext) {
			mBuffer[mNext] = 0;
			SendMessage(mhwnd, LB_ADDSTRING, 0, (LPARAM)mBuffer);
			mNext = 0;
		}
	}

	enum { kBufferSize = 256 };
	const HWND	mhwnd;
	unsigned	mNext;
	char		mBuffer[kBufferSize];
};

static void VDDebugCrashDumpRegisters(VDDebugCrashTextOutput& out, const EXCEPTION_POINTERS *const pExc) {
	const CONTEXT *const pContext = (const CONTEXT *)pExc->ContextRecord;

#ifdef _M_IX86
	out.WriteF("EAX = %08lx\n", pContext->Eax);
	out.WriteF("EBX = %08lx\n", pContext->Ebx);
	out.WriteF("ECX = %08lx\n", pContext->Ecx);
	out.WriteF("EDX = %08lx\n", pContext->Edx);
	out.WriteF("EBP = %08lx\n", pContext->Ebp);
	out.WriteF("ESI = %08lx\n", pContext->Esi);
	out.WriteF("EDI = %08lx\n", pContext->Edi);
	out.WriteF("ESP = %08lx\n", pContext->Esp);
	out.WriteF("EIP = %08lx\n", pContext->Eip);
	out.WriteF("EFLAGS = %08lx\n", pContext->EFlags);
	out.WriteF("FPUCW = %04x\n", pContext->FloatSave.ControlWord);
	out.WriteF("FPUTW = %04x\n", pContext->FloatSave.TagWord);
		
#if 0
	// extract out MMX registers

	int tos = (pContext->FloatSave.StatusWord & 0x3800)>>11;

	out.Write("\n");
	for(int i=0; i<8; i++) {
		long *pReg = (long *)(pContext->FloatSave.RegisterArea + 10*((i-tos) & 7));

		out.WriteF("MM%c = %08lx%08lx\n", i+'0', pReg[1], pReg[0]);
	}
#endif

#elif defined(_M_AMD64)
	out.WriteF("RAX = %16I64x\n", pContext->Rax);
	out.WriteF("RBX = %16I64x\n", pContext->Rbx);
	out.WriteF("RCX = %16I64x\n", pContext->Rcx);
	out.WriteF("RDX = %16I64x\n", pContext->Rdx);
	out.WriteF("RSI = %16I64x\n", pContext->Rsi);
	out.WriteF("RDI = %16I64x\n", pContext->Rdi);
	out.WriteF("RBP = %16I64x\n", pContext->Rbp);
	out.WriteF("R8  = %16I64x\n", pContext->R8);
	out.WriteF("R9  = %16I64x\n", pContext->R9);
	out.WriteF("R10 = %16I64x\n", pContext->R10);
	out.WriteF("R11 = %16I64x\n", pContext->R11);
	out.WriteF("R12 = %16I64x\n", pContext->R12);
	out.WriteF("R13 = %16I64x\n", pContext->R13);
	out.WriteF("R14 = %16I64x\n", pContext->R14);
	out.WriteF("R15 = %16I64x\n", pContext->R15);
	out.WriteF("RSP = %16I64x\n", pContext->Rsp);
	out.WriteF("RIP = %16I64x\n", pContext->Rip);
	out.WriteF("EFLAGS = %08lx\n", pContext->EFlags);
	out.Write("\n");
#else
	#error Need platform-specific register dump
#endif
}

static void VDDebugCrashDumpBombReason(VDDebugCrashTextOutput& out, const EXCEPTION_POINTERS *const pExc) {
	const EXCEPTION_RECORD *const pRecord = (const EXCEPTION_RECORD *)pExc->ExceptionRecord;
	const struct ExceptionLookup *pel = exceptions;

	while(pel->code) {
		if (pel->code == pRecord->ExceptionCode)
			break;

		++pel;
	}

	// Unfortunately, EXCEPTION_ACCESS_VIOLATION doesn't seem to provide
	// us with the read/write flag and virtual address as the docs say...
	// *sigh*

	if (!pel->code) {
		out.WriteF("Crash reason: unknown exception 0x%08lx\n", pRecord->ExceptionCode);
	} else {
		out.WriteF("Crash reason: %s\n", pel->name);
	}
}

static void VDDebugCrashDumpMemoryRegion(VDDebugCrashTextOutput& out, uintptr base, const char *name, int dwords) {
	HANDLE hProcess = GetCurrentProcess();
	uint32 *savemem = (uint32 *)alloca(4 * dwords);

	base &= ~4;

	// eliminate easy ones
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);

	if (base < (uintptr)sysInfo.lpMinimumApplicationAddress || base > (uintptr)sysInfo.lpMaximumApplicationAddress)
		return;

	int count = 0;
	while(count < dwords) {
		SIZE_T actual;
		if (!ReadProcessMemory(hProcess, (LPCVOID)(base + 4*count), savemem+count, 4, &actual))
			break;

		++count;
	}

	for(int i=0; i<count; i+=8) {
		out.WriteF("%-4s  " PTR_08lx ":", name, base + 4*i);
		for(int j=0; j<8 && i+j < count; ++j)
			out.WriteF(" %08x", savemem[i+j]);
		out.Write("\n");

		name = "";
	}
}
static void VDDebugCrashDumpPointers(VDDebugCrashTextOutput& out, const EXCEPTION_POINTERS *pExc) {
	const CONTEXT *const pContext = (const CONTEXT *)pExc->ContextRecord;

#ifdef _M_IX86
	VDDebugCrashDumpMemoryRegion(out, pContext->Eax, "EAX", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->Ebx, "EBX", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->Ecx, "ECX", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->Edx, "EDX", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->Esi, "ESI", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->Edi, "EDI", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->Esp, "ESP", 32);
	VDDebugCrashDumpMemoryRegion(out, pContext->Ebp, "EBP", 32);
#elif defined(_M_AMD64)
	VDDebugCrashDumpMemoryRegion(out, pContext->Rax, "RAX", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->Rbx, "RBX", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->Rcx, "RCX", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->Rdx, "RDX", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->Rsi, "RSI", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->Rdi, "RDI", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->Rsp, "RSP", 32);
	VDDebugCrashDumpMemoryRegion(out, pContext->Rbp, "RBP", 32);
	VDDebugCrashDumpMemoryRegion(out, pContext->R8, "R8", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->R9, "R9", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->R10, "R10", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->R11, "R11", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->R12, "R12", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->R13, "R13", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->R14, "R14", 8);
	VDDebugCrashDumpMemoryRegion(out, pContext->R15, "R15", 8);
#else
	#error Need platform-specific register dump
#endif
}

#if 0
static void VDDebugCrashDumpThreadStacks(VDDebugCrashTextOutput& out) {
	EnterCriticalSection(&g_csPerThreadState);

	__try {
		for(List2<VirtualDubThreadStateNode>::fwit it = g_listPerThreadState.begin(); it; ++it) {
			const VirtualDubThreadState *pState = it->pState;

			out.WriteF("Thread %08lx (%s)\n", pState->dwThreadId, pState->pszThreadName?pState->pszThreadName:"unknown");

			for(int i=0; i<CHECKPOINT_COUNT; ++i) {
				const VirtualDubCheckpoint& cp = pState->cp[(pState->nNextCP+i) & (CHECKPOINT_COUNT-1)];

				if (cp.file)
					out.WriteF("\t%s(%d)\n", cp.file, cp.line);
			}
		}
	} __except(EXCEPTION_EXECUTE_HANDLER) {
	}

	LeaveCriticalSection(&g_csPerThreadState);
}
#endif

static const char *GetNameFromHeap(const char *heap, int idx) {
	while(idx--)
		while(*heap++);

	return heap;
}



///////////////////////////////////////////////////////////////////////////

bool VDDebugInfoInitFromMemory(VDDebugInfoContext *pctx, const void *_src) {
	const unsigned char *src = (const unsigned char *)_src;

	pctx->pRVAHeap = NULL;

	// Check type string

	if (!memcmp((char *)src + 6, "] VirtualDub disasm", 19))
		src += *(long *)(src + 64) + 72;

	if (src[0] != '[' || src[3] != '|')
		return false;

	if (memcmp((char *)src + 6, "] VirtualDub symbolic debug information", 39))
		return false;

	// Check version number

//	int write_version = (src[1]-'0')*10 + (src[2] - '0');
	int compat_version = (src[4]-'0')*10 + (src[5] - '0');

	if (compat_version > 1)
		return false;	// resource is too new for us to load

	// Extract fields

	src += 64;

	pctx->nBuildNumber		= *(int *)src;
	pctx->pRVAHeap			= (const unsigned char *)(src + 24);
	pctx->nFirstRVA			= *(const long *)(src + 20);
	pctx->pClassNameHeap	= (const char *)pctx->pRVAHeap - 4 + *(const long *)(src + 4);
	pctx->pFuncNameHeap		= pctx->pClassNameHeap + *(const long *)(src + 8);
	pctx->pSegments			= (unsigned long (*)[2])(pctx->pFuncNameHeap + *(const long *)(src + 12));
	pctx->nSegments			= *(const long *)(src + 16);

	return true;
}

void VDDebugInfoDeinit(VDDebugInfoContext *pctx) {
	if (pctx->pRawBlock) {
		VirtualFree(pctx->pRawBlock, 0, MEM_RELEASE);
		pctx->pRawBlock = NULL;
	}
}

bool VDDebugInfoInitFromFile(VDDebugInfoContext *pctx, const char *pszFilename) {
	pctx->pRawBlock = NULL;
	pctx->pRVAHeap = NULL;

	HANDLE h = CreateFile(pszFilename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (INVALID_HANDLE_VALUE == h) {
		return false;
	}

	do {
		DWORD dwFileSize = GetFileSize(h, NULL);

		if (dwFileSize == 0xFFFFFFFF)
			break;

		pctx->pRawBlock = VirtualAlloc(NULL, dwFileSize, MEM_COMMIT, PAGE_READWRITE);
		if (!pctx->pRawBlock)
			break;

		DWORD dwActual;
		if (!ReadFile(h, pctx->pRawBlock, dwFileSize, &dwActual, NULL) || dwActual != dwFileSize)
			break;

		if (VDDebugInfoInitFromMemory(pctx, pctx->pRawBlock)) {
			CloseHandle(h);
			return true;
		}

		VirtualFree(pctx->pRawBlock, 0, MEM_RELEASE);

	} while(false);

	VDDebugInfoDeinit(pctx);
	CloseHandle(h);
	return false;
}

long VDDebugInfoLookupRVA(VDDebugInfoContext *pctx, unsigned rva, char *buf, int buflen) {
	int i;

	for(i=0; i<pctx->nSegments; ++i) {
		if (rva >= pctx->pSegments[i][0] && rva < pctx->pSegments[i][0] + pctx->pSegments[i][1])
			break;
	}

	if (i >= pctx->nSegments)
		return -1;

	const unsigned char *pr = pctx->pRVAHeap;
	const unsigned char *pr_limit = (const unsigned char *)pctx->pClassNameHeap;
	int idx = 0;

	// Linearly unpack RVA deltas and find lower_bound

	rva -= pctx->nFirstRVA;

	if ((signed)rva < 0)
		return -1;

	while(pr < pr_limit) {
		unsigned char c;
		unsigned diff = 0;

		do {
			c = *pr++;

			diff = (diff << 7) | (c & 0x7f);
		} while(c & 0x80);

		rva -= diff;

		if ((signed)rva < 0) {
			rva += diff;
			break;
		}

		++idx;
	}

	// Decompress name for RVA

	if (pr < pr_limit) {
		const char *fn_name = GetNameFromHeap(pctx->pFuncNameHeap, idx);
		const char *class_name = NULL;
		const char *prefix = "";

		if (!*fn_name) {
			fn_name = "(special)";
		} else if (*fn_name < 32) {
			int class_idx;

			class_idx = ((unsigned)(unsigned char)fn_name[0] - 1)*128 + ((unsigned)(unsigned char)fn_name[1] - 1);
			class_name = GetNameFromHeap(pctx->pClassNameHeap, class_idx);

			fn_name += 2;

			if (*fn_name == 1) {
				fn_name = class_name;
			} else if (*fn_name == 2) {
				fn_name = class_name;
				prefix = "~";
			} else if (*fn_name < 32)
				fn_name = "(special)";
		}

		// ehh... where's my wsnprintf?  _snprintf() might allocate memory or locks....
		return wsprintf(buf, "%s%s%s%s", class_name?class_name:"", class_name?"::":"", prefix, fn_name) >= 0
				? rva
				: -1;
	}

	return -1;
}

///////////////////////////////////////////////////////////////////////////

#ifdef _M_AMD64
	// We need a few definitions that are normally only in the NT DDK.
	#ifndef _NTDDK_
		typedef LONG NTSTATUS;
		typedef LONG KPRIORITY;

		typedef struct _THREAD_BASIC_INFORMATION {
			NTSTATUS ExitStatus;
			PVOID TebBaseAddress;
			ULONG_PTR UniqueProcessId;
			ULONG_PTR UniqueThreadId;
			KAFFINITY AffinityMask;
			KPRIORITY BasePriority;
		} THREAD_BASIC_INFORMATION;
	#endif

	NT_TIB *VDGetThreadTibW32(HANDLE hThread, const CONTEXT *pContext) {
		typedef NTSTATUS (WINAPI *tpNtQueryInformationThread)(HANDLE ThreadHandle, int ThreadInformationClass, PVOID ThreadInformation, ULONG ThreadInformationLength, PULONG ReturnLength);

		const tpNtQueryInformationThread pNtQueryInformationThread = (tpNtQueryInformationThread)GetProcAddress(GetModuleHandle("ntdll"), "NtQueryInformationThread");
		const int ThreadBasicInformation = 0;

		VDASSERTCT(sizeof(THREAD_BASIC_INFORMATION) == 0x30);
		THREAD_BASIC_INFORMATION info;
		ULONG actual = 0;

		NTSTATUS status = pNtQueryInformationThread(hThread, ThreadBasicInformation, &info, sizeof info, &actual);
		VDVERIFY(SUCCEEDED(status));

		return (NT_TIB *)info.TebBaseAddress;
	}
#else
	NT_TIB *VDGetThreadTibW32(HANDLE hThread, const CONTEXT *pContext) {
		LDT_ENTRY ldtEnt;
		VDVERIFY(GetThreadSelectorEntry(hThread, pContext->SegFs, &ldtEnt));
		return (NT_TIB *)(ldtEnt.BaseLow + ((uintptr)ldtEnt.HighWord.Bytes.BaseMid << 16) + ((uintptr)ldtEnt.HighWord.Bytes.BaseHi << 24));
	}
#endif


static void VDDebugCrashDumpCallStack(VDDebugCrashTextOutput& out, HANDLE hThread, const EXCEPTION_POINTERS *const pExc, const void *pDebugSrc) {
	const CONTEXT *const pContext = (const CONTEXT *)pExc->ContextRecord;
	HANDLE hprMe = GetCurrentProcess();
	int limit = 100;
	char buf[512];

	if (!g_debugInfo.pRVAHeap) {
#ifdef __INTEL_COMPILER
		out.Write("Could not open debug resource file (VeedubP4.vdi).\n");
#elif defined(_M_AMD64)
		out.Write("Could not open debug resource file (Veedub64.vdi).\n");
#else
		out.Write("Could not open debug resource file (VirtualDub.vdi).\n");
#endif
		return;
	}

	if (g_debugInfo.nBuildNumber != (int)version_num) {
		out.WriteF("Incorrect VirtualDub.vdi file (build %d) for this version of VirtualDub -- call stack unavailable.\n", g_debugInfo.nBuildNumber);
		return;
	}

	// Get some module names.

	void *pModuleMem;
	ModuleInfo *pModules = CrashGetModules(pModuleMem);

	// Retrieve stack pointers.
	NT_TIB *pTib = VDGetThreadTibW32(hThread, pContext);

#ifdef _M_AMD64
	uintptr ip = pContext->Rip;
	uintptr sp = pContext->Rsp;
#else
	uintptr ip = pContext->Eip;
	uintptr sp = pContext->Esp;
#endif

	char *pStackBase = (char *)pTib->StackBase;

	uintptr data = ip;
	char *lpAddr = (char *)sp;

	// Walk up the stack.  Hopefully it wasn't fscked.
	do {
		bool fValid = true;
		int len;
		MEMORY_BASIC_INFORMATION meminfo;

		VirtualQuery((void *)data, &meminfo, sizeof meminfo);
		
		if (!IsExecutableProtection(meminfo.Protect) || meminfo.State!=MEM_COMMIT) {
//				Report(hwnd, hFile, "Rejected: %08lx (%08lx)", data, meminfo.Protect);
			fValid = false;
		}

		if (data != ip) {
			len = 7;

			*(long *)(buf + 0) = *(long *)(buf + 4) = 0;

			while(len > 0 && !ReadProcessMemory(GetCurrentProcess(), (void *)(data-len), buf+7-len, len, NULL))
				--len;

			fValid &= VDIsValidCallX86(buf+7, len);
		}
		
		if (fValid) {
			if (VDDebugInfoLookupRVA(&g_debugInfo, data, buf, sizeof buf) >= 0) {
				out.WriteF(PTR_08lx ": %s()\n", data, buf);
				--limit;
			} else {
				ModuleInfo mi;
				char szName[MAX_PATH];

				if (LookupModuleByAddress(mi, szName, pModules, data, &meminfo)) {
					unsigned long fnbase;
					const char *pExportName = CrashLookupExport((HMODULE)mi.base, data, fnbase);

					if (pExportName)
						out.WriteF(PTR_08lx ": %s!%s [" PTR_08lx "+%lx+%lx]\n", data, mi.name, pExportName, mi.base, fnbase, (long)(data-mi.base-fnbase));
					else
						out.WriteF(PTR_08lx ": %s!%08lx\n", data, mi.name, (long)(data - mi.base));
				} else
					out.WriteF(PTR_08lx ": " PTR_08lx "\n", data, data);

				--limit;
			}
		}

		if (lpAddr >= pStackBase)
			break;

		lpAddr += sizeof(void *);
	} while(limit > 0 && ReadProcessMemory(hprMe, lpAddr-sizeof(void *), &data, sizeof(void *), NULL));

	// All done, close up shop and exit.

	if (pModuleMem)
		VirtualFree(pModuleMem, 0, MEM_RELEASE);
}

void VDDebugCrashDumpDisassembly(VDDebugCrashTextOutput& out) {
	char tbuf[1024];
	int idx = 0;

	while(idx = g_pcdw->getInstruction(tbuf, idx)) {
		out.Write(tbuf);
		out.Write("\n");
	}
}

static bool DoSave(const char *pszFilename, const wchar_t *pwszFilename, HANDLE hThread, const EXCEPTION_POINTERS *pExc, const char *pszScopeInfo) {
	VDDebugCrashTextOutputFile out(pszFilename, pwszFilename);

	out.WriteF(
			"VirtualDub FilterMod crash report -- build %d ("
#ifdef DEBUG
			"debug"
#elif defined(_M_AMD64)
			"release-AMD64"
#elif defined(__INTEL_COMPILER)
			"release-P4"
#else
			"release"
#endif
			")\n"
			"--------------------------------------\n"
			"\n"
			"Disassembly:\n", version_num);

	VDDebugCrashDumpDisassembly(out);

	out.Write("\n");

	out.WriteF("Built on %s on %s using compiler version %d\n", version_buildmachine, version_time, _MSC_VER);

	out.Write("\n");

	// Detect operating system.

	OSVERSIONINFO ovi;
	ovi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

#ifndef _M_AMD64
	HMODULE hmodKernel32 = GetModuleHandle("kernel32");
#endif

	if (GetVersionEx(&ovi)) {
#ifdef _M_AMD64
		const char *build = "x64";
#else
		typedef BOOL (WINAPI *tpIsWow64Process)(HANDLE, PBOOL);
		tpIsWow64Process pIsWow64Process = (tpIsWow64Process)GetProcAddress(hmodKernel32, "IsWow64Process");
		const char *build = "x86";

		if (pIsWow64Process) {
			BOOL is64 = FALSE;

			if (pIsWow64Process(GetCurrentProcess(), &is64) && is64)
				build = "x64";
		}
#endif

		const char *version = "?";

		if (ovi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
			if (ovi.dwMinorVersion > 0)
				version = "98";
			else
				version = "95";
		} else if (ovi.dwPlatformId == VER_PLATFORM_WIN32_NT) {
			if (ovi.dwMajorVersion >= 6) {
				if (ovi.dwMinorVersion > 0)
					version = "7";
				else
					version = "Vista";
			} else if (ovi.dwMajorVersion == 5) {
				if (ovi.dwMinorVersion >= 2)
					version = "Server 2003";
				else if (ovi.dwMinorVersion >= 1)
					version = "XP";
				else
					version = "2000";
			} else {
				version = "NT";
			}
		}

		out.WriteF("Windows %d.%d (Windows %s %s build %d) [%s]\n"
			,ovi.dwMajorVersion
			,ovi.dwMinorVersion
			,version
			,build
			,ovi.dwBuildNumber & 0xffff
			,ovi.szCSDVersion);
	}

	uint64	virtualFree = 0;
	uint64	virtualTotal = 0;
	uint64	commitLimit = 0;
	uint64	physicalTotal = 0;
#ifdef _M_AMD64
	MEMORYSTATUSEX msex = { sizeof(MEMORYSTATUSEX) };
	if (GlobalMemoryStatusEx(&msex)) {
		virtualFree = msex.ullAvailVirtual;
		virtualTotal = msex.ullTotalVirtual;
		commitLimit = msex.ullTotalPageFile;
		physicalTotal = msex.ullTotalPhys;
	}
#else
	typedef BOOL (WINAPI *tpGlobalMemoryStatusEx)(LPMEMORYSTATUSEX lpBuffer);
	tpGlobalMemoryStatusEx pGlobalMemoryStatusEx = (tpGlobalMemoryStatusEx)GetProcAddress(hmodKernel32, "GlobalMemoryStatusEx");

	MEMORYSTATUSEX msex = { sizeof(MEMORYSTATUSEX) };
	if (pGlobalMemoryStatusEx && pGlobalMemoryStatusEx(&msex)) {
		virtualFree = msex.ullAvailVirtual;
		virtualTotal = msex.ullTotalVirtual;
		commitLimit = msex.ullTotalPageFile;
		physicalTotal = msex.ullTotalPhys;
	} else {
		MEMORYSTATUS ms = { sizeof(MEMORYSTATUS) };

		GlobalMemoryStatus(&ms);

		virtualFree = ms.dwAvailVirtual;
		virtualTotal = ms.dwTotalVirtual;
		commitLimit = ms.dwTotalPageFile;
		physicalTotal = ms.dwTotalPhys;
	}
#endif

	if (physicalTotal) {
		out.WriteF("Memory status: virtual free %uM/%uM, commit limit %uM, physical total %uM\n"
			, (unsigned)((virtualFree + 1048575) >> 20)
			, (unsigned)((virtualTotal + 1048575) >> 20)
			, (unsigned)((commitLimit + 1048575) >> 20)
			, (unsigned)((physicalTotal + 1048575) >> 20)
			);
	}

	out.Write("\n");

	VDDebugCrashDumpRegisters(out, pExc);

	out.Write("\n");

	VDDebugCrashDumpBombReason(out, pExc);

	out.Write("\nCrash context:\n");

	out.Write(pszScopeInfo);

	out.Write("\n\nPointer dumps:\n\n");

	VDDebugCrashDumpPointers(out, pExc);

	out.Write("\nThread call stack:\n");

	VDDebugCrashDumpCallStack(out, hThread, pExc, g_pcdw->vdc.pExtraData);

	out.Write("\n-- End of report\n");

	return !!out;
}

///////////////////////////////////////////////////////////////////////////
//
//	Crash dialogs
//
///////////////////////////////////////////////////////////////////////////

class VDCrashDialog {
protected:
	VDCrashDialog(HANDLE hThread, const EXCEPTION_POINTERS *pExc, const char *pszScopeInfo, bool allowForcedExit)
		: mhThread(hThread)
		, mpExc(pExc)
		, mpszScopeInfo(pszScopeInfo)
		, mbAllowForcedExit(allowForcedExit)
	{
	}

	bool Display2(LPCTSTR dlgid, HWND hwndParent) {
		return 0 != DialogBoxParam(g_hInst, dlgid, hwndParent, StaticDlgProc, (LPARAM)this);
	}

	static INT_PTR CALLBACK StaticDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
		VDCrashDialog *pThis = (VDCrashDialog *)GetWindowLongPtr(hdlg, DWLP_USER);

		switch(msg) {
		case WM_INITDIALOG:
			pThis = (VDCrashDialog *)lParam;
			SetWindowLongPtr(hdlg, DWLP_USER, lParam);
			pThis->mhdlg = hdlg;
			pThis->OnInit();
			return true;
		}

		return pThis ? pThis->DlgProc(msg, wParam, lParam) : FALSE;
	}

	virtual BOOL DlgProc(UINT, WPARAM, LPARAM) = 0;
	virtual void OnInit() = 0;

	void DoSave() {
		if (!g_debugInfo.pRVAHeap)
			if (IDOK != MessageBox(mhdlg,
				"VirtualDub cannot load its crash resource file, and thus the crash dump will be "
				"missing the most important part, the call stack. Crash dumps are much less useful "
				"to the author without the call stack.",
				"VirtualDub warning", MB_OK|MB_ICONEXCLAMATION))
				return;

		if (::DoSave(g_VDCrashDumpPathA.c_str(), g_VDCrashDumpPathW.c_str(), mhThread, mpExc, mpszScopeInfo)) {
			char buf[1024];

			sprintf(buf, "Save successful to: %.512s.\n", g_VDCrashDumpPathA.c_str());
			MessageBox(mhdlg, buf, "VirtualDub Notice", MB_OK | MB_ICONINFORMATION);
		} else
			MessageBox(mhdlg, "Save failed.", "VirtualDub Error", MB_OK | MB_ICONERROR);
	}

	static void DoHelp(HWND hwnd) {
		VDShowHelp(hwnd, L"crash.html");		// hopefully, we didn't crash while trying to unpack the help file....
	}

	HWND mhdlg;
	HANDLE mhThread;
	const EXCEPTION_POINTERS *mpExc;
	const char *mpszScopeInfo;
	const bool mbAllowForcedExit;
};

class VDCrashDialogAdvanced : public VDCrashDialog {
public:
	VDCrashDialogAdvanced(HANDLE hThread, const EXCEPTION_POINTERS *pExc, const char *pszScopeInfo, bool allowForcedExit)
		: VDCrashDialog(hThread, pExc, pszScopeInfo, allowForcedExit) {}

	bool Display(HWND hwndParent) {
		return Display2(MAKEINTRESOURCE(IDD_DISASM_CRASH), hwndParent);
	}

protected:
	BOOL DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
		switch(msg) {
			case WM_COMMAND:
				switch(LOWORD(wParam)) {
				case IDCANCEL: case IDOK:
					EndDialog(mhdlg, FALSE);
					return TRUE;
				case IDC_SAVE:
					DoSave();
					return TRUE;
				case IDC_HELP2:
					DoHelp(mhdlg);
					return TRUE;
				}
				break;

			case WM_MEASUREITEM:
				return g_pcdw->DoMeasureItem(lParam);

			case WM_DRAWITEM:
				return g_pcdw->DoDrawItem(lParam);
		}

		return FALSE;
	}

	void OnInit() {
		HWND hwndList1 = GetDlgItem(mhdlg, IDC_ASMBOX);
		HWND hwndList2 = GetDlgItem(mhdlg, IDC_REGDUMP);
		HWND hwndList3 = GetDlgItem(mhdlg, IDC_CALL_STACK);
		HWND hwndReason = GetDlgItem(mhdlg, IDC_STATIC_BOMBREASON);

		g_pcdw->DoInitListbox(hwndList1);

		SendMessage(hwndList2, WM_SETFONT, SendMessage(hwndList1, WM_GETFONT, 0, 0), MAKELPARAM(TRUE, 0));
		SendMessage(hwndList3, WM_SETFONT, SendMessage(hwndList1, WM_GETFONT, 0, 0), MAKELPARAM(TRUE, 0));

		// stdc++ doesn't allow temporaries to be lvalues like VC++ does, so we
		// have to declare all the output drivers out-of-band... @*&#$&(#*$

		{
			VDDebugCrashTextOutputWindow out1(hwndReason);
			VDDebugCrashDumpBombReason(out1, mpExc);
		}

		{
			VDDebugCrashTextOutputListbox out2(hwndList1);
			VDDebugCrashDumpDisassembly(out2);
		}

		{
			VDDebugCrashTextOutputListbox out3(hwndList2);
			VDDebugCrashDumpRegisters(out3, mpExc);
		}

		{
			VDDebugCrashTextOutputListbox out4(hwndList3);
			VDDebugCrashDumpCallStack(out4, mhThread, mpExc, g_pcdw->vdc.pExtraData);
		}
	}
};

class VDCrashDialogFriendly : public VDCrashDialog {
public:
	VDCrashDialogFriendly(HANDLE hThread, const EXCEPTION_POINTERS *pExc, const char *pszScopeInfo, bool allowForcedExit)
		: VDCrashDialog(hThread, pExc, pszScopeInfo, allowForcedExit) {}

	VDCrashResponse Display(HWND hwndParent) {
		return (VDCrashResponse)DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_CRASH), hwndParent, StaticDlgProc, (LPARAM)static_cast<VDCrashDialog *>(this));
	}

protected:
	void OnInit() {
		static const char sszHelpText[]=
				"Oops -- VirtualDub has crashed. Details are listed below which may "
				"help you pinpoint the problem. If a third party driver is implicated, "
				"try using another driver and see if the problem goes away.";

		SendDlgItemMessage(mhdlg, IDC_CRASH_HELP, WM_SETTEXT, 0, (LPARAM)sszHelpText);

		HWND hwndInfo = GetDlgItem(mhdlg, IDC_CRASH_DETAILS);

		if (hwndInfo)
			SetWindowText(hwndInfo, mpszScopeInfo);

		if (!mbAllowForcedExit)
			SetWindowText(GetDlgItem(mhdlg, IDOK), "OK");
	}

	BOOL DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
		switch(msg) {
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				EndDialog(mhdlg, kVDCrashResponse_Exit);
				return TRUE;
			case IDCANCEL:
				EndDialog(mhdlg, kVDCrashResponse_DisplayAdvancedDialog);
				return TRUE;
			case IDC_SAVE:
				DoSave();
				return TRUE;
			case IDC_HELP2:
				DoHelp(mhdlg);
				return TRUE;
			case IDC_DEBUG:
				EndDialog(mhdlg, kVDCrashResponse_Debug);
				return TRUE;
			}
		}
		return FALSE;
	}
};

VDCrashResponse VDDisplayFriendlyCrashDialog(HWND hwndParent, HANDLE hThread, const EXCEPTION_POINTERS *pExc, const char *pszScopeInfo, bool allowForcedExit) {
	return VDCrashDialogFriendly(hThread, pExc, pszScopeInfo, allowForcedExit).Display(hwndParent);
}

bool VDDisplayAdvancedCrashDialog(HWND hwndParent, HANDLE hThread, const EXCEPTION_POINTERS *pExc, const char *pszScopeInfo, bool allowForcedExit) {
	return VDCrashDialogAdvanced(hThread, pExc, pszScopeInfo, allowForcedExit).Display(hwndParent);
}

