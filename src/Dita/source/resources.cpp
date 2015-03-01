//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2004 Avery Lee
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

#include <stdafx.h>
#include <vector>
#include <algorithm>
#include <string>
#include <map>

#include <vd2/system/vdtypes.h>
#include <vd2/system/log.h>
#include <vd2/Dita/resources.h>

namespace {
	typedef std::map<int, int> tStringSet;
	typedef std::map<int, tStringSet> tStringSetList;
	typedef std::map<int, std::vector<unsigned char> > tDialogList;
	typedef std::map<int, std::vector<unsigned char> > tDialogTemplateList;

	struct VDModuleResources {
		std::vector<wchar_t>	stringHeap;
		tStringSetList			stringTable;
		tDialogList				dialogs;
		tDialogTemplateList		dlgTemplates;
	};

	typedef std::map<int, VDModuleResources> tModuleResourceList;

	// Don't make this a static object -- VC7's STL is unsafe with static containers.
	tModuleResourceList *g_pModuleResources;
}

///////////////////////////////////////////////////////////////////////////

void VDInitResourceSystem() {
	if (!g_pModuleResources)
		g_pModuleResources = new tModuleResourceList;
}

void VDDeinitResourceSystem() {
	delete g_pModuleResources;
	g_pModuleResources = NULL;
}

///////////////////////////////////////////////////////////////////////////

// This is not a fully compliant SCSU decoder.

void VDDecompressSCSU(std::vector<wchar_t>& heap, const unsigned char *src, int len) {
	enum {
		kSQn		= 0x01,		// 01-08 ch		Quote from window n

		kSDX		= 0x0B,		// 0B hb lb		Define extended

		kSQU		= 0x0E,		// 0E hb lb		Quote Unicode
		kSCU		= 0x0F,		//				Switch to Unicode mode

		kSCn		= 0x10,		// 10-17		Change to window n
		kSDn		= 0x18,		// 18-1F b		Define window n as OffsetTable[b]

		kUCn		= 0xE0,		// E0-E7		Change to window n
		kUDn		= 0xE8,		// E8-EF b		Define window n as OffsetTable[b]
		kUQU		= 0xF0,		// F0			Quote
		kUDX		= 0xF1,		// F1			Define extended

	};

	const unsigned static_windows[8]={0x0000,0x0080,0x0100,0x0300,0x2000,0x2080,0x2100,0x3000};
	unsigned dynamic_windows[8]={0x0080,0x00c0,0x0400,0x0600,0x0900,0x3040,0x30A0,0xFF00};
	int current_dynwnd = 0;
	const unsigned char *srclimit = src + len;

	while(src < srclimit) {
		unsigned char c = *src++;

		switch(c) {
		case kSQn+0:
		case kSQn+1:
		case kSQn+2:
		case kSQn+3:
		case kSQn+4:
		case kSQn+5:
		case kSQn+6:
		case kSQn+7:
			{
				unsigned char d = *src++;
				heap.push_back(d < 0x80 ? static_windows[c-kSQn]+d : dynamic_windows[c-kSQn]+(d-0x80));
			}
			break;

		case kSQU:
			heap.push_back(((unsigned)src[0] << 8) + src[1]);
			src += 2;
			break;

		case kSCn+0:
		case kSCn+1:
		case kSCn+2:
		case kSCn+3:
		case kSCn+4:
		case kSCn+5:
		case kSCn+6:
		case kSCn+7:
			current_dynwnd = (c - kSCn);
			break;

		case kSDn+0:
		case kSDn+1:
		case kSDn+2:
		case kSDn+3:
		case kSDn+4:
		case kSDn+5:
		case kSDn+6:
		case kSDn+7:
			{
				unsigned char w = *src++;
				unsigned window = w<<7;

				if (w >= 0x68) {
					if (w < 0xA8)
						window += 0xAC00;
					else switch(w) {
						case 0xF9: window = 0x00C0; break;
						case 0xFA: window = 0x0250; break;
						case 0xFB: window = 0x0370; break;
						case 0xFC: window = 0x0530; break;
						case 0xFD: window = 0x3040; break;
						case 0xFE: window = 0x30A0; break;
						case 0xFF: window = 0xFF60; break;
					}
				}

				dynamic_windows[c - kSDn] = window;
				current_dynwnd = c-kSDn;
			}
			break;

		default:
			heap.push_back(c < 0x80 ? c : dynamic_windows[current_dynwnd] + (c-0x80));
			break;
		}
	}

	heap.push_back(0);
}

///////////////////////////////////////////////////////////////////////////

bool VDLoadResources(int moduleID, const void *src0, int length) {
	const unsigned char *src = (const unsigned char *)src0;
	const unsigned char *srclimit = src + length;

	if (!g_pModuleResources)
		g_pModuleResources = new tModuleResourceList;

	// All VirtualDub resource files start with the following tag:
	//	[xx|yy]
	//
	// xx is the write version, yy is the compatible version.

	if (src[0] != '[' || src[3] != '|' || src[6] != ']')
		return false;

	int write_version = (src[1]-'0')*10 + (src[2] - '0');
	int compat_version = (src[4]-'0')*10 + (src[5] - '0');

	if (compat_version > 1)
		return false;	// resource is too new for us to load

	// Okay, accept it.  Begin processing chunks until we hit the end.

	VDModuleResources& module = (*g_pModuleResources)[moduleID];

	src += 64;

	while(src < srclimit) {
		uint32 ckid = *(uint32 *)(src + 0);
		uint32 cklen = *(uint32 *)(src + 4);
		src += 8;

		if (ckid == 'SRTS') {
			//	Load strings.
			//
			//	[uint16] string set count
			//	repeat {
			//		[uint16] string set id
			//		[uint16] string count in string set
			//		repeat {
			//			[uint16]		string ID
			//			[uint8/uint16]	length of compressed string
			//			[variable]		string encoded using SCSU (Standard Compression Scheme for Unicode)
			//		}
			//	}

			const unsigned char *src2 = src;

			int strsets = *(uint16 *)src2;
			src2 += 2;

			while(strsets--) {
				uint16 setid	= ((uint16 *)src2)[0];
				uint16 strings	= ((uint16 *)src2)[1];
				tStringSet& strset = module.stringTable[setid];

				src2 += 4;

				while(strings--) {
					uint16 strid = *(uint16 *)src2;
					src2 += 2;

					int size = *(uint8 *)src2++;

					if (size & 0x80) {
						VDASSERT(!(*src2 & 0x80));
						size = ((size & 0x7f) << 7) + *(uint8 *)src2++;}

					// decompress string

					strset[strid] = module.stringHeap.size();
					VDDecompressSCSU(module.stringHeap, src2, size);

					src2 += size;
				}
			}

		} else if (ckid == 'SGLD') {
			//	Load dialogs.
			//
			//	[uint16] dialog count
			//	repeat {
			//		[uint16] dialog ID
			//		[variable] dialog bytecode
			//	}

			const unsigned char *src2 = src;
			int dialogs = *(uint16 *)src2;
			src2 += 2;

			while(dialogs--) {
				uint16 dlgid = *(uint16 *)src2;
				src2 += 2;

				uint16 size = *(uint16 *)src2;
				src2 += 2;

				std::vector<unsigned char>& dialog = module.dialogs[dlgid];

				dialog.resize(size);
				std::copy(src2, src2+size, dialog.begin());
				src2 += size;
			}
		} else if (ckid == 'SPTD') {
			//	Load dialog templates.
			//
			//	[uint16] dialog count
			//	repeat {
			//		[uint16] dialog ID
			//		[variable] dialog bytecode
			//	}

			const unsigned char *src2 = src;
			int dialogs = *(uint16 *)src2;
			src2 += 2;

			while(dialogs--) {
				uint16 dlgid = *(uint16 *)src2;
				src2 += 2;

				uint16 size = *(uint16 *)src2;
				src2 += 2;

				std::vector<unsigned char>& dialog = module.dlgTemplates[dlgid];

				dialog.resize(size);
				std::copy(src2, src2+size, dialog.begin());
				src2 += size;
			}
		}

		src += (cklen + 3) & ~3;
	}

	return true;
}

void VDUnloadResources(int moduleID) {
	g_pModuleResources->erase(moduleID);
}

///////////////////////////////////////////////////////////////////////////

void VDLoadStaticStringTableA(int moduleID, int tableID, const char *const *pStrings) {
	VDModuleResources& modres = (*g_pModuleResources)[moduleID];
	tStringSet& sset = modres.stringTable[tableID];
	int id = 0;

	while(const char *s = *pStrings++) {
		VDStringW ws(VDTextAToW(s));

		int pos = modres.stringHeap.size();

		modres.stringHeap.resize(pos + ws.size() + 1);
		modres.stringHeap[pos + ws.size()] = 0;
		ws.copy(&modres.stringHeap[pos], ws.size());

		sset[id++] = pos;
	}
}

void VDLoadStaticStringTableW(int moduleID, int tableID, const wchar_t *const *pStrings) {
	VDModuleResources& modres = (*g_pModuleResources)[moduleID];
	tStringSet& sset = modres.stringTable[tableID];
	int id = 0;

	while(const wchar_t *ws = *pStrings++) {
		size_t wslen = wcslen(ws);

		int pos = modres.stringHeap.size();

		modres.stringHeap.resize(pos + wslen + 1);
		modres.stringHeap[pos + wslen] = 0;
		std::copy(ws, ws+wslen, &modres.stringHeap[pos]);

		sset[id++] = pos;
	}
}

///////////////////////////////////////////////////////////////////////////

const wchar_t *VDTryLoadString(int moduleID, int table, int id) {
	tModuleResourceList::iterator itModule = (*g_pModuleResources).find(moduleID);

	if (itModule != (*g_pModuleResources).end()) {
		const VDModuleResources& module = (*itModule).second;
		tStringSetList::const_iterator itSS = module.stringTable.find(table);

		if (itSS != module.stringTable.end()) {
			const tStringSet& strset = (*itSS).second;
			tStringSet::const_iterator itS = strset.find(id);

			if (itS != strset.end()) {
				return &module.stringHeap[(*itS).second];
			}
		}
	}

	return NULL;
}

const wchar_t *VDLoadString(int moduleID, int table, int id) {
	const wchar_t *s = VDTryLoadString(moduleID, table, id);

	return s ? s : L"";
}

const unsigned char *VDLoadDialog(int moduleID, int id) {
	tModuleResourceList::iterator itModule = (*g_pModuleResources).find(moduleID);

	if (itModule != (*g_pModuleResources).end()) {
		const VDModuleResources& module = (*itModule).second;
		tDialogList::const_iterator itD = module.dialogs.find(id);

		if (itD != module.dialogs.end()) {
			return &(*itD).second[0];
		}
	}

	return NULL;
}

const unsigned char *VDLoadTemplate(int moduleID, int id) {
	tModuleResourceList::iterator itModule = (*g_pModuleResources).find(moduleID);

	if (itModule != (*g_pModuleResources).end()) {
		const VDModuleResources& module = (*itModule).second;
		tDialogList::const_iterator itD = module.dlgTemplates.find(id);

		if (itD != module.dlgTemplates.end()) {
			return &(*itD).second[0];
		}
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////

void VDLogAppMessage(int loglevel, int table, int id) {
	VDLog(loglevel, VDStringW(VDLoadString(0, table, id)));
}

void VDLogAppMessage(int loglevel, int table, int id, int args, ...) {
	va_list val;
	va_start(val, args);
	VDLog(loglevel, VDvswprintf(VDLoadString(0, table, id), args, val));
	va_end(val);
}

void VDLogAppMessageLimited(int& count, int loglevel, int table, int id, int args, ...) {
	++count;
	if (count == 100) {
		VDLog(loglevel, VDStringW(L"(More than 100 warnings detected for this operation; ignoring remainder in interest of speed.)"));
	} else if (count < 100) {
		va_list val;
		va_start(val, args);
		VDLog(loglevel, VDvswprintf(VDLoadString(0, table, id), args, val));
		va_end(val);
	}
}
