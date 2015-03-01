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

#ifndef f_VIDEOSEQUENCECOMPRESSOR_H
#define f_VIDEOSEQUENCECOMPRESSOR_H

#include <windows.h>
#include <vfw.h>
#include <vd2/system/VDString.h>

class VideoSequenceCompressor {
public:
	VideoSequenceCompressor();
	~VideoSequenceCompressor();

	void init(HIC hic, BITMAPINFO *pbiInput, BITMAPINFO *pbiOutput, long lQ, long lKeyRate);
	void setDataRate(long lDataRate, long lUsPerFrame, long lFrameCount);
	void start();
	void dropFrame();
	void *packFrame(void *pBits, bool *pfKeyframe, long *plSize);
	void finish();

	long getMaxSize() {
		return lMaxPackedSize;
	}

private:
	void PackFrameInternal(DWORD frameSize, DWORD q, void *pBits, DWORD dwFlagsIn, DWORD& dwFlagsOut, sint32& bytes);

	HIC			hic;
	DWORD		dwFlags;
	DWORD		mVFWExtensionMessageID;
	BITMAPINFO	*pbiInput, *pbiOutput;
	char		*pOutputBuffer, *pPrevBuffer;
	long		lFrameNum, lKeyRate, lQuality;
	long		lKeyRateCounter;
	long		lMaxFrameSize;
	long		lMaxPackedSize;
	bool		fCompressionStarted;
	long		lSlopSpace;
	long		lKeySlopSpace;

	// crunch emulation
	sint32		mQualityLo;
	sint32		mQualityLast;
	sint32		mQualityHi;

	void		*pConfigData;
	int			cbConfigData;

	VDStringA	mCodecName;
	VDStringW	mDriverName;
};

#endif
