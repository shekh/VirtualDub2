//	VirtualDub - Video processing and capture application
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

#ifndef f_INPUTFILEMP3_H
#define f_INPUTFILEMP3_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/file.h>
#include <vd2/system/refcount.h>
#include "AudioSource.h"
#include "InputFile.h"

class VDInputFileMP3 : public InputFile {
public:
	VDInputFileMP3();
	~VDInputFileMP3();

	enum BitRateMode {
		kBRM_Autodetect,
		kBRM_CBR,
		kBRM_VBR
	};

	void Init(const wchar_t *szFile);

	void setOptions(InputFileOptions *);
	InputFileOptions *promptForOptions(VDGUIHandle hwndParent);
	InputFileOptions *createOptions(const void *buf, uint32 len);

	bool GetVideoSource(int index, IVDVideoSource **ppSrc);
	bool GetAudioSource(int index, AudioSource **ppSrc);

public:
	struct FrameInfo {
		sint64	mPos;
		uint32	mSize;
		uint32	mSamples;
	};

	bool IsVBRMode() const { return mbVBRMode; }

	const vdstructex<VDWaveFormat>& GetFormat() const { return mWaveFormat; }
	sint64	GetDataLength() const { return mDataLength; }
	uint32	GetSamplesPerFrame() const { return mSamplesPerFrame; }

	uint32	GetFrameCount() const { return mFrames.size(); }
	const FrameInfo& GetFrameInfo(uint32 frame) const;

	void	ReadSpan(sint64 pos, void *buffer, uint32 len);

private:
	void ParseWave();
	void ParseWave64();

	VDFileStream		mFile;
	VDBufferedStream	mBufferedFile;

	sint64			mDataStart;
	sint64			mDataLength;
	uint32			mSamplesPerFrame;
	bool			mbVBRMode;

	typedef vdfastvector<FrameInfo> Frames;
	Frames mFrames;

	vdstructex<VDWaveFormat>	mWaveFormat;

	BitRateMode	mBitRateMode;
};

class VDAudioSourceMP3 : public AudioSource {
public:
	VDAudioSourceMP3(VDInputFileMP3 *parent);
	~VDAudioSourceMP3();

	virtual VBRMode GetVBRMode() const { return mbVBRMode ? kVBRModeVariableFrames : kVBRModeNone; }
	virtual int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lSamplesRead, uint32 *lBytesRead);

protected:
	vdrefptr<VDInputFileMP3>	mpParent;
	const bool					mbVBRMode;
};

#endif
