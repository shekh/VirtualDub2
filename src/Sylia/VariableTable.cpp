#include <string.h>
#include <stddef.h>

#include "ScriptError.h"
#include "StringHeap.h"

#include "VariableTable.h"

VariableTable::VariableTable(int ht_size) : varheap(16384) {
	long i;

	lHashTableSize	= ht_size;
	lpHashTable		= new VariableTableEntry *[ht_size];

	for(i=0; i<lHashTableSize; i++)
		lpHashTable[i] = 0;
}

VariableTable::~VariableTable() {
	delete[] lpHashTable;
}


long VariableTable::Hash(const char *szName) {
	unsigned hc = 0;
	char c;

	while(c=*szName++)
		hc = (hc + 17) * (unsigned char)c;

	return (long)(hc % lHashTableSize);
}

void VariableTable::MarkStrings(VDScriptStringHeap& heap) {
	for(long hc=0; hc<lHashTableSize; ++hc) {
		VariableTableEntry *vte = lpHashTable[hc];

		while(vte) {
			const VDScriptValue& val = vte->v;

			if (val.isString())
				heap.Mark(*val.asString());

			vte = vte->next;
		}
	}
}

VariableTableEntry *VariableTable::Lookup(const char *szName) {
	long lHashVal = Hash(szName);
	VariableTableEntry *vte = lpHashTable[lHashVal];

	while(vte) {
		if (!strcmp(vte->szName, szName))
			return vte;

		vte = vte->next;
	}

	return NULL;
}

VariableTableEntry *VariableTable::Declare(const char *szName) {
	VariableTableEntry *vte;
	long lHashVal = Hash(szName);
	long lNameLen;

	lNameLen	= strlen(szName);

	vte			= (VariableTableEntry *)varheap.Allocate(sizeof(VariableTableEntry) + lNameLen);
	if (!vte)
		SCRIPT_ERROR(OUT_OF_MEMORY);

	vte->next	= lpHashTable[lHashVal];
	vte->v		= VDScriptValue();
	strcpy(vte->szName, szName);

	lpHashTable[lHashVal] = vte;

	return vte;
}
