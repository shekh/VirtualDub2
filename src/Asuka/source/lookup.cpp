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
#include <vd2/system/vdalloc.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/text.h>
#include <vd2/system/VDString.h>
#include <stdlib.h>
#include <vector>
#include "symbols.h"
#include "utils.h"

void VDNORETURN help_lookup() {
	printf("usage: lookup <map file> <address>\n");
	exit(5);
}

void tool_lookup(const vdfastvector<const char *>& args, const vdfastvector<const char *>& switches, bool amd64) {
	if (args.size() < 2)
		help_lookup();

	char *s;
	sint64 addr = _strtoi64(args[1], &s, 16);

	if (*s)
		fail("lookup: invalid address \"%s\"", args[0]);

	vdautoptr<IVDSymbolSource> pss(VDCreateSymbolSourceLinkMap());

	pss->Init(VDTextAToW(args[0]).c_str());

	const VDSymbol *sym = pss->LookupSymbol(addr);

	if (!sym)
		fail("symbol not found for address %08x", addr);

	const char *fn;
	int line;

	if (pss->LookupLine(addr, fn, line))
		printf("%08I64x   %s + %x [%s:%d]\n", addr, sym->name, addr-sym->rva, fn, line);
	else
		printf("%08I64x   %s + %x\n", addr, sym->name, addr-sym->rva);
}
