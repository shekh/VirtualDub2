#ifndef f_SYLIA_VECTORHEAP_H
#define f_SYLIA_VECTORHEAP_H

class VectorHeapHeader {
public:
	VectorHeapHeader *next;
	long lSize, lPoint;
	char heap[1];
};

class VectorHeap {
private:
	VectorHeapHeader *first, *last;
	long lChunkSize;

public:
	VectorHeap(long chunk_size);
	~VectorHeap();

	void *Allocate(long);
};

#endif
