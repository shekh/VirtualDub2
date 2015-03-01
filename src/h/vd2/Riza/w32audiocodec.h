//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2004 Avery Lee
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

#ifndef f_VD2_RIZA_W32AUDIOCODEC_H
#define f_VD2_RIZA_W32AUDIOCODEC_H

#include <vd2/system/vdstl.h>
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <msacm.h>
#include <vector>
#include <vd2/Riza/audiocodec.h>

class VDAudioCodecW32 : public IVDAudioCodec {
public:
	VDAudioCodecW32();
	~VDAudioCodecW32();

	bool Init(const WAVEFORMATEX *pSrcFormat, const WAVEFORMATEX *pDstFormat, bool isCompression, const char *pShortNameDriverHint, bool throwOnError);
	void Shutdown();

	bool IsEnded() const { return mbEnded; }

	unsigned	GetInputLevel() const { return mBufferHdr.cbSrcLength; }
	unsigned	GetInputSpace() const { return mInputBuffer.size() - mBufferHdr.cbSrcLength; }
	unsigned	GetOutputLevel() const { return mBufferHdr.cbDstLengthUsed - mOutputReadPt; }
	const VDWaveFormat *GetOutputFormat() const { return mDstFormat.data(); }
	unsigned	GetOutputFormatSize() const { return mDstFormat.size(); }

	void		Restart();
	bool		Convert(bool flush, bool requireOutput);

	void		*LockInputBuffer(unsigned& bytes);
	void		UnlockInputBuffer(unsigned bytes);
	const void	*LockOutputBuffer(unsigned& bytes);
	void		UnlockOutputBuffer(unsigned bytes);
	unsigned	CopyOutput(void *dst, unsigned bytes);

protected:
	HACMDRIVER		mhDriver;
	HACMSTREAM		mhStream;
	vdstructex<VDWaveFormat>	mSrcFormat;
	vdstructex<VDWaveFormat>	mDstFormat;
	ACMSTREAMHEADER mBufferHdr;
	char mDriverName[64];
	char mDriverFilename[64];

	unsigned		mOutputReadPt;
	bool			mbFirst;
	bool			mbFlushing;
	bool			mbEnded;

	vdblock<char>	mInputBuffer;
	vdblock<char>	mOutputBuffer;
};

#endif
