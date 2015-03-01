//	Asuka - VirtualDub Build/Post-Mortem Utility
//	Copyright (C) 2005 Avery Lee
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
#include <vector>
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vd2/system/vdalloc.h>
#include <vd2/system/VDString.h>
#include <vd2/system/text.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/filesys.h>

#include "symbols.h"
#include "utils.h"

vdfastvector<char> fnambuf;
vdfastvector<char> cnambuf;

///////////////////////////////////////////////////////////////////////////

void parsename(sint64 rva, char *buf0) {
	char *buf = buf0;
	char *func_name = NULL;
	char *class_name = NULL;
	char c;
	int special_func = 0;

	if (*buf++ != '?') {
		func_name = buf;
	} else {

		if (*buf == '?') {
			// ??0
			// ??1
			// ??_G
			// ??_E

			++buf;
			c=*buf++;

			special_func = 31;
			if (c=='0')
				special_func = 1;		// constructor
			else if (c=='1')
				special_func = 2;		// destructor
			else if (c=='_') {
				c = *buf++;

				if (c == 'G')
					special_func = 3;		// scalar deleting destructor
				else if (c == 'E')
					special_func = 4;		// vector deleting destructor?
			}
		} else {
			func_name = buf;

			while(*buf != '@') {
				if (!*buf) {
					printf("    unrecognizable name: %s\n", buf0);
					throw "bad decorated name";
				}

				++buf;
			}

			if (buf == func_name)
				printf("    warning: empty function name generated from: '%s'\n", func_name);

			*buf++ = 0;
		}

		// Look for a class name.

		if (*buf != '@') {
			if (!*buf) {
				printf("    unrecognizable name: %s\n", buf0);
				throw "bad decorated name";
			}

			class_name = buf;

			while(*buf != '@') {
				if (!*buf)
					throw "bad decorated name (class)";

				++buf;
			}

			if (buf == class_name)
				printf("    warning: empty class name generated from: '%s'\n", class_name);

			*buf++ = 0;
		}
	}

	// write out to buffers

	if (class_name) {
		int idx = 0;
		bool found = false;

		if (!cnambuf.empty()) {
			char *csptr = &cnambuf[0];
			char *csend = csptr + cnambuf.size();
			while(csptr < csend) {
				if (!strcmp(csptr, class_name)) {
					found = true;
					break;
				}
				while(*csptr++);
				++idx;
			}
		}

		if (!found)
			cnambuf.insert(cnambuf.end(), class_name, class_name + strlen(class_name) + 1);

		fnambuf.push_back(1 + (idx / 128));
		fnambuf.push_back(1 + (idx % 128));

		if (special_func)
			fnambuf.push_back(special_func);
	}

	if (func_name) {
		if (!*func_name)
			printf("    warning: writing out empty function name\n");
		fnambuf.insert(fnambuf.end(), func_name, func_name + strlen(func_name) + 1);
	} else {
		if (!class_name)
			printf("    warning: function name absent: '%s'\n", buf);
		fnambuf.push_back(0);
	}
}

void VDNORETURN help_mapconv() {
	printf("usage: mapconv <listing-file> <output-name> <disassembler module>\n");
	exit(5);
}

void tool_mapconv(const vdfastvector<const char *>& args, const vdfastvector<const char *>& switches, bool amd64) {
	FILE *fo;
	int i;

	if (args.size() < 3)
		help_mapconv();

	// Begin parsing file

	vdautoptr<IVDSymbolSource> syms(VDCreateSymbolSourceLinkMap());
	vdfastvector<VDSymbol> rvabuf;
	syms->Init(VDTextAToW(args[0]).c_str());

	if (!(fo=fopen(args[1], "wb")))
		fail("    can't open output file \"%s\"\n", args[1]);

	int disasm_size = 0;
	{
		FILE *fd;

		if (!(fd=fopen(args[2], "rb")))
			fail("    can't open disassembler module \"%s\"\n", args[2]);

		void *buf = malloc(32768);
		int act;

		while((act = fread(buf, 1, 32768, fd)) > 0) {
			disasm_size += act;
			fwrite(buf, act, 1, fo);
		}

		free(buf);
		fclose(fd);
	}

	// delete all entries that take no space -- these often crop up with inline classes

	const uint32 code_segs(syms->GetCodeGroupMask());

	printf("%x\n", code_segs);

	syms->GetAllSymbols(rvabuf);

	int j=0;
	for(i=0; size_t(i)<rvabuf.size()-1; ++i) {
		VDSymbol& sym = rvabuf[i];

		if (!(code_segs & (1<<sym.group)))
			continue;

		if (!strncmp(sym.name, "__ehhandler$", 12) || !strncmp(sym.name, "__unwindfunclet$", 16))
			continue;

		if (rvabuf[i+1].rva - sym.rva) {
			rvabuf[j++] = sym;
		}
	}
	rvabuf[j++] = rvabuf[i];	// last entry is always copied

	printf("    Deleted %d zero-size and EH stub entries from RVA table\n", i+1-j);
	rvabuf.resize(j);

	size_t rvabufsize = rvabuf.size();
	for(i=0; size_t(i)<rvabufsize; i++) {
		VDSymbol& sym = rvabuf[i];

		parsename(sym.rva, sym.name);
	}
	
	int segcnt = syms->GetSectionCount();
	vdfastvector<uint32> segbuf(segcnt * 2);
	for(i=0; i<segcnt; i++) {
		const VDSection *sect = syms->GetSection(i);
		segbuf[i*2+0] = (uint32)(sect->mAbsStart);
		segbuf[i*2+1] = sect->mLength;
		printf("        #%-2d  %08lx-%08lx\n", i+1, sect->mStart, sect->mStart + sect->mLength - 1);
	}

	printf("        Raw statistics:\n");
	printf("            Disassembler:     %ld bytes\n", disasm_size);
	printf("            RVA bytes:        %ld\n", rvabuf.size()*4);
	printf("            Class name bytes: %ld\n", cnambuf.size());
	printf("            Func name bytes:  %ld\n", fnambuf.size());

	printf("    Packing RVA data..."); fflush(stdout);

	vdfastvector<VDSymbol>::iterator itRVA = rvabuf.begin(), itRVAEnd = rvabuf.end();
	vdfastvector<char> rvaout;
	sint64 firstrva = (*itRVA++).rva;
	sint64 lastrva = firstrva;

	for(; itRVA != itRVAEnd; ++itRVA) {
		sint64 rvadiff = (*itRVA).rva - lastrva;

		lastrva += rvadiff;

		if (rvadiff & 0xF0000000) rvaout.push_back((char)(0x80 | ((rvadiff>>28) & 0x7F)));
		if (rvadiff & 0xFFE00000) rvaout.push_back((char)(0x80 | ((rvadiff>>21) & 0x7F)));
		if (rvadiff & 0xFFFFC000) rvaout.push_back((char)(0x80 | ((rvadiff>>14) & 0x7F)));
		if (rvadiff & 0xFFFFFF80) rvaout.push_back((char)(0x80 | ((rvadiff>> 7) & 0x7F)));
		rvaout.push_back((char)(rvadiff & 0x7F));
	}

	printf("%ld bytes\n", rvaout.size());

	// dump data

	long t;

	static const char header[64]="[01|01] VirtualDub symbolic debug information\r\n\x1A";

	fwrite(header, 64, 1, fo);

	t = get_version();
	fwrite(&t, 4, 1, fo);

	t = rvaout.size() + 4;
	fwrite(&t, 4, 1, fo);

	t = cnambuf.size();
	fwrite(&t, 4, 1, fo);

	t = fnambuf.size();
	fwrite(&t, 4, 1, fo);

	t = segcnt;
	fwrite(&t, 4, 1, fo);

	fwrite(&firstrva, 4, 1, fo);
	fwrite(rvaout.data(), rvaout.size(), 1, fo);
	fwrite(cnambuf.data(), cnambuf.size(), 1, fo);
	fwrite(fnambuf.data(), fnambuf.size(), 1, fo);
	fwrite(segbuf.data(), segcnt*8, 1, fo);

	// really all done

	if (fclose(fo))
		throw "output file close failed";

	FILE *fLock = fopen("autobuild.lock", "r");
	if (fLock) {
		fclose(fLock);
	} else {
		ProjectSetup ps;

		if (VDDoesPathExist(L"build.cfg")) {
			ps.Read(L"build.cfg");
		} else {
			ps.Query();
			ps.Write(L"build.cfg");
		}

		inc_version(ps.mCounterTag.empty() ? NULL : ps.mCounterTag.c_str());
		write_version(ps.mCounterTag.empty() ? NULL : ps.mCounterTag.c_str());
	}
}
