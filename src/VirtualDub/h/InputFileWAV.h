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

#ifndef f_INPUTFILEWAV_H
#define f_INPUTFILEWAV_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/file.h>
#include <vd2/system/refcount.h>
#include <vd2/Riza/audiocodec.h>
#include "AudioSource.h"
#include "InputFile.h"

class VDInputFileWAV : public InputFile {
public:
	VDInputFileWAV();
	~VDInputFileWAV();

	void Init(const wchar_t *szFile);

	bool GetVideoSource(int index, IVDVideoSource **ppSrc);
	bool GetAudioSource(int index, AudioSource **ppSrc);

public:
	const vdstructex<VDWaveFormat>& GetFormat() const { return mWaveFormat; }
	sint64	GetDataLength() const { return mDataLength; }
	uint32	GetBytesPerSample() const { return mBytesPerSample; }
	void	ReadSpan(sint64 pos, void *buffer, uint32 len);

private:
	void ParseWAVE();
	void ParseWAVE64();

	VDFileStream		mFile;
	VDBufferedStream	mBufferedFile;

	sint64			mDataStart;
	sint64			mDataLength;
	uint32			mBytesPerSample;

	vdstructex<VDWaveFormat>	mWaveFormat;
};

class VDAudioSourceWAV : public AudioSource {
public:
	VDAudioSourceWAV(VDInputFileWAV *parent);
	~VDAudioSourceWAV();

	virtual int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lSamplesRead, uint32 *lBytesRead);

protected:
	vdrefptr<VDInputFileWAV>	mpParent;

	VDPosition		mCurrentSample;
	uint32			mBytesPerSample;
};

#endif
