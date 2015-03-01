#include "stdafx.h"
#if 0

#include <windows.h>
#include <stdarg.h>

#include "DynamicCode.h"

DynamicCode::DynamicCode(const DynamicCodeBlock *pdcb, long *params) : pDynamicBlock(NULL) {
	const long *pReloc = pdcb->entrypts + pdcb->nEntryPoints;
	long *pEntryPoints;
	long nEntryOffset = ((pdcb->nEntryPoints*4+31)&-32);

	pDynamicBlock = (void **)VirtualAlloc(NULL, nEntryOffset + pdcb->cbCode, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

	if (pDynamicBlock) {
		int i;

		memcpy(pDynamicBlock, pdcb->entrypts, pdcb->nEntryPoints * 4);

		pEntryPoints = (long *)pDynamicBlock;

		for(i=0; i<pdcb->nEntryPoints; i++)
			*pEntryPoints++ += (long)pDynamicBlock + nEntryOffset;

		memcpy((char *)pDynamicBlock + nEntryOffset, pdcb->pCode, pdcb->cbCode);

		for(i=0; i<pdcb->nRelocs; i++) {
			long varsrc = *pReloc++, src = varsrc & 0x3fff;
			long *target = (long *)((char *)pDynamicBlock + nEntryOffset + ((unsigned)varsrc>>16));
			int addend = 0, mult = 1;

			if (varsrc & 0x8000)
				addend = *pReloc++;

			if (varsrc & 0x4000)
				mult = *pReloc++;

			if (!src)
				*target += (long)pDynamicBlock + nEntryOffset - (long)pdcb->pCode;
			else
				*target = params[src-1]*mult + addend;
		}
	}
}

DynamicCode::~DynamicCode() {
	if (pDynamicBlock)
		VirtualFree(pDynamicBlock, 0, MEM_RELEASE);
}

#endif
