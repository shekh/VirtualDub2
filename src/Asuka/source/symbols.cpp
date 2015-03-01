#include "stdafx.h"
#include <vd2/system/file.h>
#include <vd2/system/zip.h>
#include "symbols.h"
#include <string>
#include <list>
#include <map>

class VDSymbolSourceLinkMap : public IVDSymbolSource {
public:
	~VDSymbolSourceLinkMap();

	void Init(const wchar_t *filename);
	const VDSymbol *LookupSymbol(sint64 addr);
	const VDSection *LookupSection(sint64 addr);
	void GetAllSymbols(vdfastvector<VDSymbol>&);
	uint32 GetCodeGroupMask();
	int GetSectionCount();
	const VDSection *GetSection(int sec);
	bool LookupLine(sint64 addr, const char *& filename, int& lineno);

protected:
	void Init(IVDStream *pStream);

	uint32		mCodeSegments;

	typedef vdfastvector<VDSymbol> tSymbols;
	tSymbols	mSymbols;
	typedef std::vector<VDSection> tSections;
	tSections mSections;

	typedef std::list<std::string> tLineStrings;
	tLineStrings mLineStrings;

	typedef std::pair<const char *, int> tLineInfo;
	typedef std::map<sint64, tLineInfo> tLineMap;
	tLineMap mLineMap;
};

IVDSymbolSource *VDCreateSymbolSourceLinkMap() {
	return new VDSymbolSourceLinkMap;
}

namespace {
	bool findline(VDTextStream& textStream, const char *s) {
		while(const char *line = textStream.GetNextLine()) {
			if (strstr(line, s))
				return true;
		}

		return false;
	}

	struct SymbolSort {
		bool operator()(const VDSymbol& s1, const VDSymbol& s2) const {
			return s1.rva < s2.rva;
		}
	};
}

VDSymbolSourceLinkMap::~VDSymbolSourceLinkMap() {
	while(!mSymbols.empty()) {
		VDSymbol& ent = mSymbols.back();

		free(ent.name);
		mSymbols.pop_back();
	}
}

void VDSymbolSourceLinkMap::Init(const wchar_t *filename) {
	if (const wchar_t *b = wcschr(filename, L'!')) {
		std::wstring fn(filename, b - filename);

		VDFileStream fileStream(fn.c_str());
		VDZipArchive zipArchive;

		zipArchive.Init(&fileStream);

		sint32 n = zipArchive.GetFileCount();
		VDStringA name(VDTextWToA(b+1));

		for(int i=0; i<n; ++i) {
			const VDZipArchive::FileInfo& fileInfo = zipArchive.GetFileInfo(i);

			if (fileInfo.mFileName == name) {
				IVDStream *pStream = zipArchive.OpenRawStream(i);

				VDZipStream zipStream(pStream, fileInfo.mCompressedSize, !fileInfo.mbPacked);
				Init(&zipStream);
				return;
			}
		}

		throw "Can't find file in zip archive.";
	}

	VDFileStream fileStream(filename);
	Init(&fileStream);
}

void VDSymbolSourceLinkMap::Init(IVDStream *pStream) {
	VDTextStream textStream(pStream);

	mCodeSegments = 0;

	if (!findline(textStream, "Start         Length"))
		throw "can't find segment list";

	while(const char *line = textStream.GetNextLine()) {
		long grp, start, len;

		if (3!=sscanf(line, "%lx:%lx %lx", &grp, &start, &len))
			break;

		if (strstr(line+49, "CODE")) {
			printf("        %04x:%08lx %08lx type code (%dKB)\n", grp, start, len, (len+1023)>>10);

			mCodeSegments |= 1<<grp;

			mSections.push_back(VDSection(start, len, grp));
		}
	}

	if (!findline(textStream, "Publics by Value"))
		throw "Can't find public symbol list.";

	textStream.GetNextLine();

	std::vector<sint64> groups;

	while(const char *line = textStream.GetNextLine()) {
		long grp, start;
		sint64 rva;
		char symname[2048];

		// workaround for junk caused by VCPPCheck insertion with goofy anonymous namespace
		if (strlen(line) > 21 && !strcmp(line+21, "?VCPPCheck@?% ")) {
			textStream.GetNextLine();
			textStream.GetNextLine();
			textStream.GetNextLine();
			textStream.GetNextLine();
			textStream.GetNextLine();
			textStream.GetNextLine();
			textStream.GetNextLine();
			continue;
		}

		if (4!=sscanf(line, "%lx:%lx %s %I64x", &grp, &start, symname, &rva))
			break;

		if (!(mCodeSegments & (1<<grp)))
			continue;

		VDSymbol entry = { rva, grp, start, _strdup(symname) };

		if (groups.size() < (size_t)(grp+1))
			groups.resize(grp+1, 0);

		if (!groups[grp])
			groups[grp] = rva - start;

		mSymbols.push_back(entry);
	}

	if (!findline(textStream, "Static symbols"))
		printf("    warning: No static symbols found!\n");
	else {
		textStream.GetNextLine();

		while(const char *line = textStream.GetNextLine()) {
			long grp, start;
			sint64 rva;
			char symname[4096];

			if (4!=sscanf(line, "%lx:%lx %s %I64x", &grp, &start, symname, &rva))
				break;

			if (!(mCodeSegments & (1<<grp)))
				continue;

			VDSymbol entry = { rva, grp, start, _strdup(symname) };

			if (groups.size() < size_t(grp+1))
				groups.resize(grp+1, 0);

			if (!groups[grp])
				groups[grp] = rva - start;

			mSymbols.push_back(entry);
		}
	}

	// parse line number information
	const char *linefn = NULL;
	int blanklines = 0;

	while(const char *line = textStream.GetNextLine()) {
		if (!line[0] && linefn) {
			if (!--blanklines)
				linefn = NULL;
		}

		if (line[0] == 'L') {
			if (!strncmp(line, "Line numbers for ", 17)) {
				const char *fnstart = line + 17;
				const char *fnend = fnstart;

				while(const char c = *fnend) {
					if (c == '/' || c == '\\' || c == ':')
						fnstart = fnend+1;

					if (c == '(')
						break;

					++fnend;
				}

				mLineStrings.push_back(std::string(fnstart, fnend));
				linefn = mLineStrings.back().c_str();

				// one blank line after the Line numbers header, one after the line numbers
				// themselves... so kill line number collection on the second blank line
				blanklines = 2;
			}
			continue;
		}

		if (linefn && line[0] == ' ') {
			int lineno[4], grp[4], offset[4];

			int count = sscanf(line, "%d %x:%x %d %x:%x %d %x:%x %d %x:%x"
				, &lineno[0], &grp[0], &offset[0]
				, &lineno[1], &grp[1], &offset[1]
				, &lineno[2], &grp[2], &offset[2]
				, &lineno[3], &grp[3], &offset[3]);

			if (count > 0) {
				count /= 3;
				for(int i=0; i<count; ++i)
					mLineMap.insert(tLineMap::value_type(groups[grp[i]] + offset[i], tLineInfo(linefn, lineno[i])));
			}
		}
	}

	// rebias sections with group offsets
	tSections::iterator it(mSections.begin()), itEnd(mSections.end());
	for(; it!=itEnd; ++it) {
		VDSection& sec = *it;

		if (size_t(sec.mGroup) < groups.size()) {
			sec.mAbsStart = groups[sec.mGroup] + sec.mStart;
		} else {
			printf("    warning: no symbol found for group %u\n", sec.mGroup);
			sec.mAbsStart = 0;
		}
	}

	// sort symbols
	std::sort(mSymbols.begin(), mSymbols.end(), SymbolSort());
}

const VDSymbol *VDSymbolSourceLinkMap::LookupSymbol(sint64 addr) {
	VDSymbol s={addr, NULL};

	tSymbols::iterator it(std::upper_bound(mSymbols.begin(), mSymbols.end(), s, SymbolSort()));

	if (it == mSymbols.begin())
		return NULL;

	return &*--it;
}

const VDSection *VDSymbolSourceLinkMap::LookupSection(sint64 addr) {
	return NULL;
}

void VDSymbolSourceLinkMap::GetAllSymbols(vdfastvector<VDSymbol>& syms) {
	syms = mSymbols;
}

uint32 VDSymbolSourceLinkMap::GetCodeGroupMask() {
	return mCodeSegments;
}

int VDSymbolSourceLinkMap::GetSectionCount() {
	return mSections.size();
}

const VDSection *VDSymbolSourceLinkMap::GetSection(int sec) {
	return &mSections[sec];
}

bool VDSymbolSourceLinkMap::LookupLine(sint64 addr, const char *& filename, int& lineno) {
	tLineMap::iterator it(mLineMap.upper_bound(addr));

	if (it != mLineMap.begin()) {
		--it;

		filename = (*it).second.first;
		lineno = (*it).second.second;
		return true;
	}

	return false;
}
