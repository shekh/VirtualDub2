//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#if 0
#ifndef f_VIDEOSOURCEADAPTER_H
#define f_VIDEOSOURCEADAPTER_H

#include "VideoSource.h"

class VideoSourceAdapter : public VideoSource {
public:
	VideoSourceAdapter();
	~VideoSourceAdapter();

	virtual bool setDecompressedFormat(int depth);
	virtual bool setDecompressedFormat(BITMAPINFOHEADER *pbih);

	BITMAPINFOHEADER *getDecompressedFormat() {
		return bmihDecompressedFormat;
	}

	virtual void streamSetDesiredFrame(long frame_num);
	virtual long streamGetNextRequiredFrame(BOOL *is_preroll);
	virtual int	streamGetRequiredCount(long *pSize);
	virtual void *streamGetFrame(void *inputBuffer, long data_len, BOOL is_key, BOOL is_preroll, long frame_num) = NULL;

	virtual void streamBegin(bool fRealTime, bool bForceReset);

	virtual void invalidateFrameBuffer();
	virtual	BOOL isFrameBufferValid() = NULL;

	virtual void *getFrame(LONG frameNum) = NULL;

	virtual char getFrameTypeChar(long lFrameNum) = 0;

	virtual eDropType getDropType(long lFrameNum)=0;

	virtual bool isKeyframeOnly();
	virtual bool isType1();

	virtual long	streamToDisplayOrder(long sample_num) { return sample_num; }
	virtual long	displayToStreamOrder(long display_num) { return display_num; }

	virtual bool isDecodable(long sample_num) = 0;
};

#endif
#endif