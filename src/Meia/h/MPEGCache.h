#ifndef f_VD2_MEIA_MPEGCACHE_H
#define f_VD2_MEIA_MPEGCACHE_H

#include <vector>
#include <vd2/system/vdtypes.h>

class VDMPEGCache {
public:
	VDMPEGCache(int nBlocks, int nBlockLen);
	~VDMPEGCache();

	bool Read(void *pData, int64 bytepos, int len, int offset);
	void Write(const void *pData, int64 bytepos, int len);

private:
	struct CacheEntry {
		int64			pos;		// The assumption is that zero is never a valid start for a pack's data
		int				len;		// length of this cache block
		int				next;
		int				age;
	};

	char *mpCacheMemory;
	unsigned		mNextAgeValue;
	int				mBlockSize;

	std::vector<CacheEntry> mCacheBlocks;

	int Evict();
};

#endif
