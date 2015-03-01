//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2007 Avery Lee
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

#ifndef f_AUDIOSOURCEAVI_H
#define f_AUDIOSOURCEAVI_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/vdstl.h>
#include "InputFileAVI.h"
#include "AudioSource.h"

class VDAudioSourceAVISourced : public AudioSource, public VDAVIStreamSource {
public:
	VDAudioSourceAVISourced(InputFileAVI *pParent) : VDAVIStreamSource(pParent) {}

	virtual int GetPreloadSamples() = 0;

protected:
	InputFileAVI *mpParent;
};

class AudioSourceAVI : public VDAudioSourceAVISourced {
private:
	const int mStreamIndex;

	IAVIReadHandler *pAVIFile;
	IAVIReadStream *pAVIStream;
	bool bQuiet;

	bool _isKey(VDPosition lSample);

	~AudioSourceAVI();

public:
	AudioSourceAVI(InputFileAVI *pParent, IAVIReadHandler *pAVIFile, int streamIndex, bool bAutomated);

	void setRate(const VDFraction& f) { streamInfo.dwRate = f.getHi(); streamInfo.dwScale = f.getLo(); }

	void Reinit();
	int GetPreloadSamples();
	bool isStreaming();

	void streamBegin(bool fRealTime, bool bForceReset);
	void streamEnd();

	bool init();
	int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lSamplesRead, uint32 *lBytesRead);

	virtual VDPosition TimeToPositionVBR(VDTime us) const;
	virtual VDTime PositionToTimeVBR(VDPosition samples) const;
	virtual VBRMode GetVBRMode() const;
};

class AudioSourceDV : public VDAudioSourceAVISourced {
public:
	AudioSourceDV(InputFileAVI *pParent, IAVIReadStream *pAVIStream, bool bAutomated);

	void Reinit();
	int GetPreloadSamples() { return 0; }
	bool isStreaming();

	void streamBegin(bool fRealTime, bool bForceReset);
	void streamEnd();

	ErrorMode getDecodeErrorMode() { return mErrorMode; }
	void setDecodeErrorMode(ErrorMode mode) { mErrorMode = mode; }
	bool isDecodeErrorModeSupported(ErrorMode mode) { return mode == kErrorModeConceal || mode == kErrorModeReportAll; }

	bool init();
	int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lSamplesRead, uint32 *lBytesRead);

protected:
	~AudioSourceDV();

	struct CacheLine;

	const CacheLine *LoadSet(VDPosition set);
	void FlushCache();
	const sint32 *GetGatherTab(bool pal, bool sixteenBit);
	bool _isKey(VDPosition lSample);

	vdblock<uint8> mTempBuffer;

	IAVIReadStream *mpStream;
	bool bQuiet;
	ErrorMode	mErrorMode;
	uint32	mSamplesPerSet;
	VDPosition mLastFrame;
	VDPosition	mRawFrames;
	VDPosition	mRawStart;
	VDPosition	mRawEnd;

	bool mbGatherTabInited;
	bool mbGatherTabInitedAsPAL;
	bool mbGatherTabInitedAs16Bit;

	vdblock<sint32>	mGatherTab;

	enum { kCacheLines = 4 };

	VDPosition	mCacheLinePositions[kCacheLines];

	struct CacheLine {
		sint16	mRawData[1959*10][2];
		sint16	mResampledData[1920*10][2];
		uint32	mRawSamples;
	} mCache[kCacheLines];
};

#endif
