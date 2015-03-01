#pragma include_alias("stdafx.h", "test.h")
#pragma include_alias("AVIReadIndex.h", "..\VirtualDub\h\AVIReadIndex.h")
#include "..\VirtualDub\source\AVIReadIndex.cpp"

namespace {
	struct Chunk {
		sint64 pos;
		uint32 len;
	};

	void AddChunks(VDAVIReadIndex& idx, const Chunk *chunks, size_t n) {
		while(n--) {
			idx.AddChunk(chunks->pos, chunks->len);
			++chunks;
		}
	}

	template<size_t N>
	void AddChunks(VDAVIReadIndex& idx, const Chunk (&chunks)[N]) {
		AddChunks(idx, chunks, N);
	}
}

DEFINE_TEST(AVIReadIndex) {
	VDAVIReadIndex ari;

	////////////////////////////////////////////////////////////////////////
	//
	// VBR (video)
	//
	////////////////////////////////////////////////////////////////////////

	static const Chunk kChunks1[]={
		{ 100, 100 },
		{ 200, 0x80000001 },
		{ 300, 0 },
		{ 400, 0x80000002 },
		{ 500, 5 }
	};

	ari.Init(0);
	AddChunks(ari, kChunks1);
	ari.Finalize();

	TEST_ASSERT(ari.GetChunkCount() == 5);
	TEST_ASSERT(ari.GetSampleCount() == 5);

	VDAVIReadIndexIterator it;
	sint64 chunkPos;
	uint32 offset;
	uint32 byteSize;

	ari.FindSampleRange(it, 0, 1); ari.GetNextSampleRange(it, chunkPos, offset, byteSize);
	TEST_ASSERT(chunkPos == 100);
	TEST_ASSERT(offset == 0);
	TEST_ASSERT(byteSize == 100);

	ari.FindSampleRange(it, 1, 1); ari.GetNextSampleRange(it, chunkPos, offset, byteSize);
	TEST_ASSERT(chunkPos == 200);
	TEST_ASSERT(offset == 0);
	TEST_ASSERT(byteSize == 1);

	ari.FindSampleRange(it, 2, 1); ari.GetNextSampleRange(it, chunkPos, offset, byteSize);
	TEST_ASSERT(chunkPos == 300);
	TEST_ASSERT(offset == 0);
	TEST_ASSERT(byteSize == 0);

	ari.FindSampleRange(it, 3, 1); ari.GetNextSampleRange(it, chunkPos, offset, byteSize);
	TEST_ASSERT(chunkPos == 400);
	TEST_ASSERT(offset == 0);
	TEST_ASSERT(byteSize == 2);

	ari.FindSampleRange(it, 4, 1); ari.GetNextSampleRange(it, chunkPos, offset, byteSize);
	TEST_ASSERT(chunkPos == 500);
	TEST_ASSERT(offset == 0);
	TEST_ASSERT(byteSize == 5);

	TEST_ASSERT(ari.NearestKey(-1) == -1);
	TEST_ASSERT(ari.NearestKey(0) == -1);
	TEST_ASSERT(ari.NearestKey(1) == 1);
	TEST_ASSERT(ari.NearestKey(2) == 1);
	TEST_ASSERT(ari.NearestKey(3) == 3);
	TEST_ASSERT(ari.NearestKey(4) == 3);
	TEST_ASSERT(ari.NearestKey(5) == 3);

	TEST_ASSERT(ari.IsKey(-1) == false);
	TEST_ASSERT(ari.IsKey(0) == false);
	TEST_ASSERT(ari.IsKey(1) == true);
	TEST_ASSERT(ari.IsKey(2) == false);
	TEST_ASSERT(ari.IsKey(3) == true);
	TEST_ASSERT(ari.IsKey(4) == false);
	TEST_ASSERT(ari.IsKey(5) == false);

	TEST_ASSERT(ari.PrevKey(-1) == -1);
	TEST_ASSERT(ari.PrevKey(0) == -1);
	TEST_ASSERT(ari.PrevKey(1) == -1);
	TEST_ASSERT(ari.PrevKey(2) == 1);
	TEST_ASSERT(ari.PrevKey(3) == 1);
	TEST_ASSERT(ari.PrevKey(4) == 3);
	TEST_ASSERT(ari.PrevKey(5) == 3);

	TEST_ASSERT(ari.NextKey(-1) == 1);
	TEST_ASSERT(ari.NextKey(0) == 1);
	TEST_ASSERT(ari.NextKey(1) == 3);
	TEST_ASSERT(ari.NextKey(2) == 3);
	TEST_ASSERT(ari.NextKey(3) == -1);
	TEST_ASSERT(ari.NextKey(4) == -1);
	TEST_ASSERT(ari.NextKey(5) == -1);

	static const Chunk kChunks2[]={
		{ 0x0000000000000000ULL, 1 },
		{ 0x00000000FFFFFFFFULL, 2 },
		{ 0x0000000100000000ULL, 3 },
		{ 0x00000001FFFFFFFFULL, 4 },
		{ 0x0000000200000000ULL, 5 },
		{ 0x00000002FFFFFFFFULL, 6 },
		{ 0x0000000300000000ULL, 7 },
		{ 0x00000003FFFFFFFFULL, 8 },
		{ 0x0000000400000000ULL, 9 },
		{ 0x00000004FFFFFFFFULL, 10 },
	};

	ari.Init(0);
	AddChunks(ari, kChunks2);
	ari.Finalize();

	TEST_ASSERT(ari.GetChunkCount() == 10);
	TEST_ASSERT(ari.GetSampleCount() == 10);

	for(int i=0; i<10; ++i) {
		const Chunk& ch = kChunks2[i];
		ari.FindSampleRange(it, i, 1); ari.GetNextSampleRange(it, chunkPos, offset, byteSize);
		TEST_ASSERT(chunkPos == ch.pos);
		TEST_ASSERT(offset == 0);
		TEST_ASSERT(byteSize == ch.len);
	}

	vdfastvector<uint32> sizes;
	vdfastvector<sint64> positions;
	vdfastvector<int> order;

	sint64 pos = 0;
	ari.Clear();
	for(int i=0; i<10000; ++i) {
		uint32 cksize = rand() ^ (rand() << 12) ^ (rand() << 24);
		uint32 ckskip = rand() ^ (rand() << 12) ^ (rand() << 24);
		
		positions.push_back(pos);
		sizes.push_back(cksize);
		order.push_back(i);

		ari.AddChunk(pos, cksize);
		pos += cksize & 0x7FFFFFFF;
		pos += ckskip;
	}

	ari.Finalize();

	std::random_shuffle(order.begin(), order.end());

	for(int i=0; i<10000; ++i) {
		int j = order[i];
		ari.FindSampleRange(it, j, 1); ari.GetNextSampleRange(it, chunkPos, offset, byteSize);

		TEST_ASSERT(chunkPos == positions[j]);
		TEST_ASSERT(offset == 0);
		TEST_ASSERT(byteSize == (sizes[j] & 0x7FFFFFFF));
	}

	////////////////////////////////////////////////////////////////////////
	//
	// CBR (sample based)
	//
	////////////////////////////////////////////////////////////////////////

	static const Chunk kChunks3[]={
		{ 0x0000000000000000ULL, 0 },
		{ 0x0000000080000000ULL, 4 },
		{ 0x0000000100000000ULL, 8 },
		{ 0x0000000180000000ULL, 12 },
		{ 0x0000000200000000ULL, 16 },
	};

	ari.Init(4);
	AddChunks(ari, kChunks3);
	ari.Finalize();

	TEST_ASSERT(ari.GetChunkCount() == 5);
	TEST_ASSERT(ari.GetSampleCount() == 10);
	TEST_ASSERT(ari.IsVBR() == false);

	for(int i=1; i<5; ++i) {
		const Chunk& ch = kChunks3[i];
		ari.FindSampleRange(it, i*(i-1)/2, 1); ari.GetNextSampleRange(it, chunkPos, offset, byteSize);
		TEST_ASSERT(chunkPos == ch.pos);
		TEST_ASSERT(offset == 0);
		TEST_ASSERT(byteSize == ch.len);
	}

	ari.FindSampleRange(it, 0, 100);
	TEST_ASSERT(true == ari.GetNextSampleRange(it, chunkPos, offset, byteSize));
	TEST_ASSERT(chunkPos == kChunks3[1].pos);
	TEST_ASSERT(offset == 0);
	TEST_ASSERT(byteSize == 4);
	TEST_ASSERT(true == ari.GetNextSampleRange(it, chunkPos, offset, byteSize));
	TEST_ASSERT(chunkPos == kChunks3[2].pos);
	TEST_ASSERT(offset == 0);
	TEST_ASSERT(byteSize == 8);
	TEST_ASSERT(true == ari.GetNextSampleRange(it, chunkPos, offset, byteSize));
	TEST_ASSERT(chunkPos == kChunks3[3].pos);
	TEST_ASSERT(offset == 0);
	TEST_ASSERT(byteSize == 12);
	TEST_ASSERT(true == ari.GetNextSampleRange(it, chunkPos, offset, byteSize));
	TEST_ASSERT(chunkPos == kChunks3[4].pos);
	TEST_ASSERT(offset == 0);
	TEST_ASSERT(byteSize == 16);
	TEST_ASSERT(false == ari.GetNextSampleRange(it, chunkPos, offset, byteSize));
	
	ari.FindSampleRange(it, 1, 100);
	TEST_ASSERT(true == ari.GetNextSampleRange(it, chunkPos, offset, byteSize));
	TEST_ASSERT(chunkPos == kChunks3[2].pos);
	TEST_ASSERT(offset == 0);
	TEST_ASSERT(byteSize == 8);

	ari.FindSampleRange(it, 2, 100);
	TEST_ASSERT(true == ari.GetNextSampleRange(it, chunkPos, offset, byteSize));
	TEST_ASSERT(chunkPos == kChunks3[2].pos);
	TEST_ASSERT(offset == 4);
	TEST_ASSERT(byteSize == 4);
	TEST_ASSERT(true == ari.GetNextSampleRange(it, chunkPos, offset, byteSize));
	TEST_ASSERT(chunkPos == kChunks3[3].pos);
	TEST_ASSERT(offset == 0);
	TEST_ASSERT(byteSize == 12);

	ari.FindSampleRange(it, 3, 100);
	TEST_ASSERT(true == ari.GetNextSampleRange(it, chunkPos, offset, byteSize));
	TEST_ASSERT(chunkPos == kChunks3[3].pos);
	TEST_ASSERT(offset == 0);
	TEST_ASSERT(byteSize == 12);

	ari.FindSampleRange(it, 4, 100);
	TEST_ASSERT(true == ari.GetNextSampleRange(it, chunkPos, offset, byteSize));
	TEST_ASSERT(chunkPos == kChunks3[3].pos);
	TEST_ASSERT(offset == 4);
	TEST_ASSERT(byteSize == 8);

	////////////////////////////////////////////////////////////////////////
	//
	// VBR (sample based)
	//
	////////////////////////////////////////////////////////////////////////

	static const Chunk kChunks4[]={
		{ 0x0000000000000000ULL, 384 },
		{ 0x0000000080000000ULL, 192 },
		{ 0x0000000100000000ULL, 576 },
		{ 0x0000000180000000ULL, 1152 },
		{ 0x0000000200000000ULL, 1024 },
	};

	ari.Init(576);
	AddChunks(ari, kChunks4);
	ari.Finalize();

	TEST_ASSERT(ari.GetChunkCount() == 5);
	TEST_ASSERT(ari.GetSampleCount() == 7);
	TEST_ASSERT(ari.IsVBR() == true);

	ari.FindSampleRange(it, 0, 100);
	TEST_ASSERT(true == ari.GetNextSampleRange(it, chunkPos, offset, byteSize));
	TEST_ASSERT(chunkPos == kChunks3[0].pos);
	TEST_ASSERT(offset == 0);
	TEST_ASSERT(byteSize == 384);

	ari.FindSampleRange(it, 1, 100);
	TEST_ASSERT(true == ari.GetNextSampleRange(it, chunkPos, offset, byteSize));
	TEST_ASSERT(chunkPos == kChunks3[1].pos);
	TEST_ASSERT(offset == 0);
	TEST_ASSERT(byteSize == 192);

	ari.FindSampleRange(it, 2, 100);
	TEST_ASSERT(true == ari.GetNextSampleRange(it, chunkPos, offset, byteSize));
	TEST_ASSERT(chunkPos == kChunks3[2].pos);
	TEST_ASSERT(offset == 0);
	TEST_ASSERT(byteSize == 576);

	ari.FindSampleRange(it, 3, 100);
	TEST_ASSERT(true == ari.GetNextSampleRange(it, chunkPos, offset, byteSize));
	TEST_ASSERT(chunkPos == kChunks3[3].pos);
	TEST_ASSERT(offset == 0);
	TEST_ASSERT(byteSize == 576);

	ari.FindSampleRange(it, 4, 100);
	TEST_ASSERT(true == ari.GetNextSampleRange(it, chunkPos, offset, byteSize));
	TEST_ASSERT(chunkPos == kChunks3[3].pos);
	TEST_ASSERT(offset == 576);
	TEST_ASSERT(byteSize == 576);

	ari.FindSampleRange(it, 5, 100);
	TEST_ASSERT(true == ari.GetNextSampleRange(it, chunkPos, offset, byteSize));
	TEST_ASSERT(chunkPos == kChunks3[4].pos);
	TEST_ASSERT(offset == 0);
	TEST_ASSERT(byteSize == 576);

	ari.FindSampleRange(it, 6, 100);
	TEST_ASSERT(true == ari.GetNextSampleRange(it, chunkPos, offset, byteSize));
	TEST_ASSERT(chunkPos == kChunks3[4].pos);
	TEST_ASSERT(offset == 576);
	TEST_ASSERT(byteSize == 448);

	return 0;
}

