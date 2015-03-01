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

#ifndef f_AVIOUTPUTWAV_H
#define f_AVIOUTPUTWAV_H

#include <vd2/system/fileasync.h>
#include <vd2/system/vdalloc.h>

#include "AVIOutput.h"

class AVIOutputWAV : public AVIOutput {
public:
	AVIOutputWAV();
	~AVIOutputWAV();

	IVDMediaOutputStream *createVideoStream();
	IVDMediaOutputStream *createAudioStream();

	void setBufferSize(sint32 nBytes) {
		mBufferSize = nBytes;
	}

	bool init(const wchar_t *szFile);
	bool init(VDFileHandle h, bool pipeMode);
	void finalize();

	void write(const void *pBuffer, uint32 cbBuffer);

private:
	void WriteHeader(bool initial);

	vdautoptr<IVDFileAsync>	mpFileAsync;
	bool		mbHeaderOpen;
	bool		mbWriteWAVE64;
	bool		mbAutoWriteWAVE64;
	bool		mbPipeMode;
	uint64		mBytesWritten;
	uint32		mHeaderSize;
	sint32		mBufferSize;
};

#endif
