#include <string.h>

#include "MPEGCache.h"

VDMPEGCache::VDMPEGCache(int nBlocks, int nBlockLen)
	: mpCacheMemory(new char[nBlockLen * nBlocks])
	, mNextAgeValue(0)
	, mBlockSize(nBlockLen)
{
	VDASSERT(mpCacheMemory);

	for(int i=0; i<nBlocks; ++i) {
		CacheEntry e;

		e.pos = 0;
		e.len = 0;
		e.next = -1;
		e.age = 0;

		mCacheBlocks.push_back(e);
	}
}

VDMPEGCache::~VDMPEGCache() {
	delete[] mpCacheMemory;
}

bool VDMPEGCache::Read(void *pData, int64 bytepos, int len, int offset) {
	VDASSERTPTR(this);
	VDASSERTPTR(pData);
	VDASSERT(bytepos != 0);
	VDASSERT(len > 0);

	if (mpCacheMemory) {
		int nBlocks = mCacheBlocks.size();

		for(int i=0; i<nBlocks; ++i) {
			if (mCacheBlocks[i].pos == bytepos) {
				++mNextAgeValue;

//				VDDEBUG2("Cache hit  : %16I64x + %-5d (%5d)\n", bytepos, offset, len);

				while(len > 0) {

					VDASSERT(i>=0);

					int blen = mCacheBlocks[i].len - offset;

					if (blen > len)
						blen = len;

					VDASSERT(blen > 0);

					memcpy(pData, mpCacheMemory + mBlockSize * i + offset, blen);

					pData = (char *)pData + blen;
					len -= blen;

					mCacheBlocks[i].age = mNextAgeValue;

					i = mCacheBlocks[i].next;
					offset = 0;
				}

				VDASSERT(!len);

				return true;
			}
		}
	}

//	VDDEBUG2("Cache miss : %16I64x + %-5d (%5d)\n", bytepos, offset, len);

	return false;
}

void VDMPEGCache::Write(const void *pData, int64 bytepos, int len) {
	VDASSERTPTR(this);
	VDASSERTPTR(pData);
	VDASSERT(bytepos != 0);
	VDASSERT(len > 0);

	if (!mpCacheMemory || (size_t)len > mCacheBlocks.size()*mBlockSize)
		return;

//	VDDEBUG2("Cache store: %16I64x +       (%5d)\n", bytepos, len);

	++mNextAgeValue;

	int victim = Evict(), next_victim;

	while(len > 0) {
		int blen = mBlockSize>len ? mBlockSize : len;
		len -= blen;

		// Age must be set before next evict to avoid getting the same block
		// twice

		mCacheBlocks[victim].age = mNextAgeValue;

		next_victim = -1;
		if (len)
			next_victim = Evict();

		memcpy(mpCacheMemory + mBlockSize*victim, pData, blen);

		mCacheBlocks[victim].pos = bytepos;
		mCacheBlocks[victim].len = blen;
		mCacheBlocks[victim].next = next_victim;

		// Only write out a non-zero byte position for the first block.

		bytepos = 0;
		pData = (char *)pData + blen;
		victim = next_victim;
	}
}

int VDMPEGCache::Evict() {
	int nBlocks = mCacheBlocks.size();

	// Evict the block with the oldest age

	unsigned oldest_age = 0;
	int oldest_age_idx = 0;		// all else failing, evict block 0

	for(int i=0; i<nBlocks; ++i) {
		unsigned age_delta = mNextAgeValue - mCacheBlocks[i].age;

		if (age_delta > oldest_age) {
			oldest_age_idx = i;
			oldest_age = age_delta;
		}
	}

	return oldest_age_idx;
}

