#include <string.h>
#include <vd2/system/vdtypes.h>

#include "ScriptError.h"

#include "StringHeap.h"
 
VDScriptStringHeap::VDScriptStringHeap() {
}

VDScriptStringHeap::~VDScriptStringHeap() {
	Clear();
}

void VDScriptStringHeap::Clear() {
	tStrings::iterator it(mStrings.begin()), itEnd(mStrings.end());

	for(; it!=itEnd; ++it) {
		char *s = *it;

		free(s-1);
	}

	mStrings.clear();
}

void VDScriptStringHeap::BeginGC() {
	tStrings::iterator it(mStrings.begin()), itEnd(mStrings.end());

	for(; it!=itEnd; ++it) {
		char *s = *it;
		s[-1] = 0;
	}
}

void VDScriptStringHeap::Mark(char *s) {
	s[-1] = 1;
}

int VDScriptStringHeap::EndGC() {
	tStrings::iterator it(mStrings.begin()), itEnd(mStrings.end());
	int n = 0;

	while(it != itEnd) {
		char *s = *it;

		if (!s[-1]) {
			free(s-1);
			it = mStrings.erase(it);
			++n;
		} else
			++it;
	}

	return n;
}

char **VDScriptStringHeap::Allocate(int len) {
	char *s = (char *)malloc(len+2);
	if (!s)
		SCRIPT_ERROR(OUT_OF_STRING_SPACE);
	
	mStrings.push_back(s + 1);
	return &mStrings.back();
}
