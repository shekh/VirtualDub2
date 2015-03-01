//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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

#ifndef f_AVIOUTPUTSTRIPED_H
#define f_AVIOUTPUTSTRIPED_H

#include "AVIOutput.h"
#include "AVIStripeSystem.h"

class AVIOutputStripeState;
class IVDMediaOutputAVIFile;

class AVIOutputStriped : public AVIOutput {
private:
	AVIStripeSystem			*stripesys;
	IVDMediaOutputAVIFile	**stripe_files;
	AVIOutputStripeState	*stripe_data;
	int						stripe_count;
	int						stripe_order;

	IVDMediaOutputAVIFile	*index_file;

	enum { CACHE_SIZE = 256 };

	AVIStripeIndexEntry		audio_index_cache[CACHE_SIZE];
	DWORD					audio_index_flags[CACHE_SIZE];
	LONG					audio_index_count[CACHE_SIZE];
	AVIStripeIndexEntry		video_index_cache[CACHE_SIZE];
	DWORD					video_index_flags[CACHE_SIZE];
	LONG					video_index_count[CACHE_SIZE];
	int audio_index_cache_point;
	int video_index_cache_point;
	bool f1GbMode;

	void FlushCache(BOOL fAudio);

public:
	AVIOutputStriped(AVIStripeSystem *);
	virtual ~AVIOutputStriped();

	void disable_os_caching();
	void set_1Gb_limit();

	IVDMediaOutputStream *createVideoStream();
	IVDMediaOutputStream *createAudioStream();

	bool init(const wchar_t *szFile);
	void finalize();

	void writeChunk(bool is_audio, uint32 flags, const void *pBuffer, uint32 cbBuffer,
											uint32 lSampleFirst, uint32 lSampleCount);
};

#endif
