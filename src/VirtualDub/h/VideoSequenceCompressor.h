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
#include <vd2/system/Fraction.h>
#include <vd2/Kasumi/pixmap.h>

struct EncoderHIC;
struct VDPacketInfo;

class VideoSequenceCompressor {
public:
	VideoSequenceCompressor();
	~VideoSequenceCompressor();
	void SetDriver(EncoderHIC* driver, uint32 kilobytesPerSecond, long quality, long keyrate, bool ownHandle);
	void GetOutputFormat(const void *inputFormat, vdstructex<tagBITMAPINFOHEADER>& outputFormat);
	void GetOutputFormat(const VDPixmapLayout *inputFormat, vdstructex<tagBITMAPINFOHEADER>& outputFormat);
	const void *GetOutputFormat();
	uint32 GetOutputFormatSize();
	int GetInputFormat(FilterModPixmapInfo* info) {
		if (info) {
			info->copy_ref(mInputInfo);
			info->copy_alpha(mInputInfo);
		}
		return mInputLayout.format; 
	}

	void init(EncoderHIC* driver, long lQ, long lKeyRate);
	void Start(const void *inputFormat, uint32 inputFormatSize, const void *outputFormat, uint32 outputFormatSize, const VDFraction& frameRate, VDPosition frameCount);
	void Start(const VDPixmapLayout& layout, FilterModPixmapInfo& info, const void *outputFormat, uint32 outputFormatSize, const VDFraction& frameRate, VDPosition frameCount);
	void internalStart(const void *outputFormat, uint32 outputFormatSize, const VDFraction& frameRate, VDPosition frameCount);
	void dropFrame();
	bool packFrame(void *dst, const void *src, uint32& size, VDPacketInfo& packetInfo);
	void Stop();

	long getMaxSize() {
		return lMaxPackedSize;
	}

	void* createResultBuffer();

private:
	void PackFrameInternal(void* dst, DWORD frameSize, DWORD q, const void *src, DWORD dwFlagsIn, DWORD& dwFlagsOut, VDPacketInfo& packetInfo, sint32& bytes);

	EncoderHIC	*driver;
	bool		mbOwnHandle;
	DWORD		dwFlags;
	DWORD		mVFWExtensionMessageID;
	vdstructex<BITMAPINFOHEADER>	mInputFormat;
	vdstructex<BITMAPINFOHEADER>	mOutputFormat;
	VDPixmapLayout  	mInputLayout;
	FilterModPixmapInfo mInputInfo;
	VDFraction	mFrameRate;
	VDPosition	mFrameCount;
	char		*pPrevBuffer;
	long		lFrameSent;
	long		lFrameDone;
	long		lKeyRate;
	long		lQuality;
	long		lDataRate;
	long		lKeyRateCounter;
	long		lMaxFrameSize;
	long		lMaxPackedSize;
	bool		fCompressionStarted;
	long		lSlopSpace;
	long		lKeySlopSpace;

	bool		mbKeyframeOnly;
	bool		mbCompressionRestarted;

	// crunch emulation
	sint32		mQualityLo;
	sint32		mQualityLast;
	sint32		mQualityHi;

	void		*pConfigData;
	int			cbConfigData;

	VDStringW	mCodecName;
	VDStringW	mDriverName;
};

#endif
