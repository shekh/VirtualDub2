#include <assert.h>
#include <stddef.h>
#include <malloc.h>

#include "VectorHeap.h"

VectorHeap::VectorHeap(long chunk_size) {
	lChunkSize = chunk_size;
	first = last = NULL;
}

VectorHeap::~VectorHeap() {
	VectorHeapHeader *vhh_cur, *vhh_next;

	vhh_cur = first;

	while(vhh_cur) {
		vhh_next = vhh_cur->next;
		free(vhh_cur);
		vhh_cur = vhh_next;
	}
}

void *VectorHeap::Allocate(long lBytes) {
	long lp;

	lBytes = (lBytes + 7) & ~7;

	if (!last || last->lSize - last->lPoint < lBytes) {
		VectorHeapHeader *vhh;

		vhh = (VectorHeapHeader *)malloc(lChunkSize);
		if (!vhh) return NULL;

		vhh->next	= NULL;
		vhh->lSize	= lChunkSize - offsetof(VectorHeapHeader, heap);
		vhh->lPoint	= lBytes;

		if (last) last->next	= vhh;
		last		= vhh;
		if (!first) first = vhh;

		return vhh->heap;
	}

	lp = last->lPoint;
	last->lPoint += lBytes;

	return &last->heap[lp];
}
