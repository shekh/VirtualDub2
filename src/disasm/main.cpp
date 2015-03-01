//	disasm - Disassembly module compiler for VirtualDub
//	Copyright (C) 2002 Avery Lee, All Rights Reserved
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

#pragma warning(disable: 4786)

#include <string>
#include <list>
#include <vector>
#include <set>
#include <utility>
#include <algorithm>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <conio.h>
#include <crtdbg.h>
#include <windows.h>

#include "ruleset.h"
#include "utils.h"
#include "runtime_dasm.h"
#include "runtime_tracedec.h"

void parse_ia(tRuleSystem& rsys, FILE *f);

tRuleSystem		g_RuleSystem;

void *VDDisasmDecompress(void *_dst, const unsigned char *src, int src_len);
void *VDTracedecDecompress(void *_dst, const unsigned char *src, int src_len);
bool VDTraceObjectCode(VDTracedecContext *pvdc, const uint8 *source, int bytes, VDTracedecResult& res);
void dump_ia(std::vector<char>& dst, const tRuleSystem& rulesys);
void dump_tracedec(std::vector<char>& dst, const tRuleSystem& rulesys);

///////////////////////////////////////////////////////////////////////////

#ifdef _M_AMD64
static const char test1[]={
	0x50,
	0x51,
	0x52,
	0x53,
	0x54,
	0x55,
	0x56,
	0x57,

	0x48, 0x50,
	0x4c, 0x50,
	0x4a, 0x50,
	0x49, 0x50,

	0x66, 0x44, 0x50,
	0x66, 0x42, 0x50,
	0x66, 0x41, 0x50,

	0x58,
	0x59,
	0x5a,
	0x5b,
	0x5c,
	0x5d,
	0x5e,
	0x5f,
};
#else
#ifdef _MSC_VER
	#pragma warning(disable: 4730)		// warning C4730: 'test1' : mixing _m64 and floating point expressions may result in incorrect code
#endif
void __declspec(naked) test1() {
	__asm {
		__emit 0x83
		__emit 0xc3
		__emit 0x01

		__emit 0x0f
		__emit 0x18
		__emit 0x05
		__emit 0x40
		__emit 0x07
		__emit 0x90
		__emit 0x02

		prefetchnta [eax]
		prefetcht0 [eax]
		prefetcht1 [eax]
		prefetcht2 [eax]

		pavgusb		mm0,[eax]
		prefetch	[eax]
		prefetchw	[eax]
		pswapd		mm1, mm0
		push		[eax]
		push		word ptr [eax]

		cvtsi2ss	xmm4, ecx
		cvtsi2ss	xmm4, [ecx]
		cvtpi2ps	xmm4, mm2
		cvtpi2ps	xmm4, [ecx]

		cvtss2si	eax, xmm4
		cvtss2si	eax, [ecx]
		cvtps2pi	mm2, xmm4
		cvtps2pi	mm2, [ecx]

		cvttss2si	eax, xmm4
		cvttss2si	eax, [ecx]
		cvttps2pi	mm2, xmm4
		cvttps2pi	mm2, [ecx]

		cvtsi2sd	xmm4, ecx
		cvtsi2sd	xmm4, [ecx]
		cvtpi2pd	xmm4, mm2
		cvtpi2pd	xmm4, [ecx]

		cvtsd2si	eax, xmm4
		cvtsd2si	eax, [ecx]
		cvtpd2pi	mm2, xmm4
		cvtpd2pi	mm2, [ecx]

		cvttsd2si	eax, xmm4
		cvttsd2si	eax, [ecx]
		cvttpd2pi	mm2, xmm4
		cvttpd2pi	mm2, [ecx]

		movq		xmm0, qword ptr [eax]

__emit 0x66
__emit 0x0f
__emit 0x6f
__emit 0x2d
__emit 0xf0
__emit 0x42
__emit 0x0e
__emit 0x10

		rep movsw
		lock rep movs es:word ptr [edi], cs:word ptr [esi]

		lock mov cs:dword ptr [eax+ecx*4+12300000h], 12345678h

		__emit 0x2e
		jc x1

		__emit 0x3e
		jc y1

		call esi

		shl ecx,1

		ret
x1:
y1:
		nop

		fldcw word ptr [esp]

	}
}
#endif

///////////////////////////////////////////////////////////////////////////

long symLookup(unsigned long virtAddr, char *buf, int buf_len) {
	unsigned long offs;

	if ((offs = (virtAddr - (unsigned long)symLookup)) < 256) {
		strcpy(buf, "symLookup");
		return (long)offs;
	}

	if ((offs = (virtAddr - (unsigned long)VDDisassemble)) < 256) {
		strcpy(buf, "VDDisassemble");
		return (long)offs;
	}

	if ((offs = (virtAddr - (unsigned long)printf)) < 16) {
		strcpy(buf, "printf");
		return (long)offs;
	}

	return -1;
}

int main(int argc, char **argv) {
	FILE *f = fopen(argc>1?argv[1]:"ia32.txt", "r");
	parse_ia(g_RuleSystem, f);
	fclose(f);

#if 0
	std::vector<char> tr;
	dump_tracedec(tr, g_RuleSystem);

	if (f = fopen(argc>2?argv[2]:"ia32trace.bin", "wb")) {
		fwrite(&tr[0], tr.size(), 1, f);
		fclose(f);
	}

	std::vector<char> ruleTestHeap;
	ruleTestHeap.resize(*(long *)&tr[68]);
	void *dst_end = VDTracedecDecompress(&ruleTestHeap[0], (const unsigned char *)&tr[72], *(long *)&tr[64]);

	VDTracedecContext tdec;
	tdec.pRuleSystem = (const unsigned char **)&ruleTestHeap[0];
	tdec.physToVirtOffset = 0;

	void *src = (void *)0x401000;
	VDTracedecResult res;

	std::set<ptrdiff_t> traces;
	std::vector<ptrdiff_t> tracesToGo;

	tracesToGo.push_back((ptrdiff_t)0x401000);

	while(!tracesToGo.empty()) {
		ptrdiff_t traceStart = tracesToGo.back();
		tracesToGo.pop_back();

		printf("Tracing at %p\n", (void *)traceStart);
		while(VDTraceObjectCode(&tdec, (const uint8 *)traceStart, 0xFFFFFF, res)) {
			if (res.mbIsCall)
				printf("Call: %p\n", (void *)traceStart);
			else if (res.mbIsJcc)
				printf("Jcc: %p\n", (void *)traceStart);
			else if (res.mbIsJmp) {
				printf("Jmp: %p\n", (void *)traceStart);
				break;
			} else if (res.mbIsReturn) {
				printf("Return: %p\n", (void *)traceStart);
				break;
			}

			if (res.mbTargetValid) {
				ptrdiff_t newTrace = res.mBranchTarget;

				if (traces.insert(newTrace).second)
					tracesToGo.push_back(newTrace);
			}

			traceStart += res.mInsnLength;
		}
	}
#else

	std::vector<char> dst;
	dump_ia(dst, g_RuleSystem);

	if (f = fopen(argc>2?argv[2]:"ia32.bin", "wb")) {
		fwrite(&dst[0], dst.size(), 1, f);
		fclose(f);
	}

	std::vector<char> ruleTestHeap;
	ruleTestHeap.resize(*(long *)&dst[68]);
	void *dst_end = VDDisasmDecompress(&ruleTestHeap[0], (const unsigned char *)&dst[72], *(long *)&dst[64]);

	VDDisassemblyContext vdc;

	vdc.pRuleSystem = (const unsigned char **)&ruleTestHeap[0];
	vdc.pSymLookup = symLookup;
	vdc.physToVirtOffset = 0;

	VDDisassemble(&vdc, (const uint8 *)test1, 1024);
#endif

	_getch();

	return 0;
}
