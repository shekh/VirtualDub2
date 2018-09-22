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

#include "stdafx.h"

#include <windows.h>
#include <vfw.h>

#include <vd2/system/error.h>
#include <vd2/Riza/audiocodec.h>

#include "AVIOutputWAV.h"

extern uint32 VDPreferencesGetFileAsyncDefaultMode();

namespace
{
	static const uint8 kGuidRIFF[16]={
		// {66666972-912E-11CF-A5D6-28DB04C10000}
		0x72, 0x69, 0x66, 0x66, 0x2E, 0x91, 0xCF, 0x11, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00
	};

	static const uint8 kGuidWAVE[16]={
		// {65766177-ACF3-11D3-8CD1-00C04F8EDB8A}
		0x77, 0x61, 0x76, 0x65, 0xF3, 0xAC, 0xD3, 0x11, 0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A
	};

	static const uint8 kGuidfmt[16]={
		// {20746D66-ACF3-11D3-8CD1-00C04F8EDB8A}
		0x66, 0x6D, 0x74, 0x20, 0xF3, 0xAC, 0xD3, 0x11, 0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A
	};

	static const uint8 kGuidfact[16]={
		// {74636166-ACF3-11D3-8CD1-00C04F8EDB8A}
		0x66, 0x61, 0x63, 0x74, 0xF3, 0xAC, 0xD3, 0x11, 0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A
	};

	static const uint8 kGuiddata[16]={
		// {61746164-ACF3-11D3-8CD1-00C04F8EDB8A}
		0x64, 0x61, 0x74, 0x61, 0xF3, 0xAC, 0xD3, 0x11, 0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A
	};
}

//////////////////////////////////////////////////////////////////////
//
// AVIAudioOutputStreamWAV
//
//////////////////////////////////////////////////////////////////////

class AVIAudioOutputStreamWAV : public AVIOutputStream {
public:
	AVIAudioOutputStreamWAV(AVIOutputWAV *pParent);

	void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples);
	void partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples);
	void partialWrite(const void *pBuffer, uint32 cbBuffer);
	void partialWriteEnd();

protected:
	AVIOutputWAV *const mpParent;
};

AVIAudioOutputStreamWAV::AVIAudioOutputStreamWAV(AVIOutputWAV *pParent) : mpParent(pParent) {
}

void AVIAudioOutputStreamWAV::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples) {
	mpParent->write(pBuffer, cbBuffer);
}

void AVIAudioOutputStreamWAV::partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples) {

}

void AVIAudioOutputStreamWAV::partialWrite(const void *pBuffer, uint32 cbBuffer) {
	mpParent->write(pBuffer, cbBuffer);
}

void AVIAudioOutputStreamWAV::partialWriteEnd() {
}

//////////////////////////////////////////////////////////////////////
//
// AVIOutputWAV
//
//////////////////////////////////////////////////////////////////////

AVIOutputWAV::AVIOutputWAV() {
	mbHeaderOpen			= false;
	mbAutoWriteWAVE64	= true;
	mbWriteWAVE64		= false;
	mBytesWritten		= 0;
	mBufferSize			= 65536;
}

AVIOutputWAV::~AVIOutputWAV() {
}

IVDMediaOutputStream *AVIOutputWAV::createVideoStream() {
	return NULL;
}

IVDMediaOutputStream *AVIOutputWAV::createAudioStream() {
	VDASSERT(!audioOut);
	if (!(audioOut = new_nothrow AVIAudioOutputStreamWAV(this)))
		throw MyMemoryError();
	return audioOut;
}

bool AVIOutputWAV::init(const wchar_t *pwszFile) {
	if (!audioOut) return false;

	mpFileAsync = VDCreateFileAsync((IVDFileAsync::Mode)VDPreferencesGetFileAsyncDefaultMode());
	mpFileAsync->Open(pwszFile, 2, mBufferSize >> 1);

	WriteHeader(true);

	mBytesWritten = 0;

	mbHeaderOpen = true;
	mbPipeMode = false;

	return true;
}

bool AVIOutputWAV::init(VDFileHandle h, bool pipeMode) {
	if (!audioOut) return false;

	mpFileAsync = VDCreateFileAsync((IVDFileAsync::Mode)VDPreferencesGetFileAsyncDefaultMode());
	mpFileAsync->OpenPipe(h, 2, mBufferSize >> 1);

	mbPipeMode = pipeMode;
	mbAutoWriteWAVE64 = false;

	WriteHeader(true);

	mBytesWritten = 0;

	mbHeaderOpen = true;

	return true;
}

void AVIOutputWAV::finalize() {
	if (!mpFileAsync->IsOpen())
		return;

	mpFileAsync->FastWriteEnd();

	if (mbAutoWriteWAVE64 && mBytesWritten > 0x7FFFFFFF)
		mbWriteWAVE64 = true;

	if (mbHeaderOpen && !mbPipeMode) {
		WriteHeader(false);
		mbHeaderOpen = false;
		mpFileAsync->Truncate(mBytesWritten + mHeaderSize);
	}

	mpFileAsync->Close();
}

void AVIOutputWAV::write(const void *pBuffer, uint32 cbBuffer) {
	mpFileAsync->FastWrite(pBuffer, cbBuffer);
	mBytesWritten += cbBuffer;
}

void AVIOutputWAV::WriteHeader(bool initial) {
	vdfastvector<char> buffer;

	buffer.reserve(256);

	uint32 audioFmtLen = audioOut->getFormatLen();
	const WAVEFORMATEX& wfex = *(const WAVEFORMATEX *)audioOut->getFormat();
	bool compressed = true;
	if (is_audio_pcm((VDWaveFormat*)&wfex) || is_audio_float((VDWaveFormat*)&wfex)) compressed = false;

	// WAV format:
	//		RIFF	<riff_size>	WAVE
	//		fmt		<fmt_size>	<fmt_data>
	//		[fact	4	<uncompressed_size>]
	//		data	<data_size>	<wave_data>
	//
	// Header overhead is 28/40 bytes for WAV and 88/120 bytes for WAVE64.
	uint32 dwHeader[22];

	if (mbWriteWAVE64) {
		memcpy(dwHeader + 0, kGuidRIFF, 16);
		if (initial) {
			dwHeader[4] = 0x7F000000;
			dwHeader[5] = 0;
		} else {
			*(uint64 *)&dwHeader[4] = mBytesWritten + mHeaderSize;
		}

		memcpy(dwHeader + 6, kGuidWAVE, 16);
		memcpy(dwHeader + 10, kGuidfmt, 16);
		dwHeader[14] = audioFmtLen + 24;
		dwHeader[15] = 0;

		memcpy(buffer.alloc(64), dwHeader, 64);
		memcpy(buffer.alloc(audioFmtLen), audioOut->getFormat(), audioFmtLen);

		if (dwHeader[14] & 7) {
			uint32 pad = -(sint32)audioFmtLen & 7;
			memset(buffer.alloc(pad), 0, pad);
		}

		// The 'fact' chunk is required for compressed WAVs and indicates the
		// number of uncompressed samples in the audio.  It is in fact rather
		// useless as it is usually computed from the ratios in the wave header
		// and the size of the compressed data -- check the output of Sound
		// Recorder -- but we must write it out anyway.

		if (compressed) {
			memcpy(dwHeader + 0, kGuidfact, 16);
			dwHeader[4] = 4 + 24;
			dwHeader[5] = 0;
			dwHeader[6] = 0;
			dwHeader[7] = 0;

			if (!initial)
				dwHeader[6] = (uint32)(mBytesWritten * wfex.nSamplesPerSec / wfex.nAvgBytesPerSec);

			memcpy(buffer.alloc(32), dwHeader, 32);
		}

		memcpy(dwHeader + 0, kGuiddata, 16);
		dwHeader[4] = 0x7E000000;
		dwHeader[5] = 0;

		if (!initial)
			*(uint64 *)&dwHeader[4] = mBytesWritten + 24;

		memcpy(buffer.alloc(24), dwHeader, 24);
	} else {
		dwHeader[0]	= FOURCC_RIFF;
		dwHeader[1] = initial ? 0x7F000000 : (uint32)(mBytesWritten + mHeaderSize - 8);
		dwHeader[2] = mmioFOURCC('W', 'A', 'V', 'E');
		dwHeader[3] = mmioFOURCC('f', 'm', 't', ' ');
		dwHeader[4] = audioFmtLen;

		memcpy(buffer.alloc(20), dwHeader, 20);
		memcpy(buffer.alloc(audioFmtLen), audioOut->getFormat(), audioFmtLen);

		if (audioFmtLen & 1)
			buffer.push_back(0);

		// The 'fact' chunk is required for compressed WAVs and indicates the
		// number of uncompressed samples in the audio.  It is in fact rather
		// useless as it is usually computed from the ratios in the wave header
		// and the size of the compressed data -- check the output of Sound
		// Recorder -- but we must write it out anyway.

		if (compressed) {
			dwHeader[0] = mmioFOURCC('f', 'a', 'c', 't');
			dwHeader[1] = 4;
			dwHeader[2] = initial ? 0 : (uint32)(mBytesWritten * wfex.nSamplesPerSec / wfex.nAvgBytesPerSec);
			memcpy(buffer.alloc(12), dwHeader, 12);
		}

		// If auto-WAVE64 mode is enabled, then it will be necessary to write additional
		// JUNK data at the beginning in order to convert to WAVE64 later. This will take
		// 60 or 80 bytes, depending on whether a fact chunk is used, not counting
		// additional bytes required for format padding.
		if (mbAutoWriteWAVE64) {
			dwHeader[0] = mmioFOURCC('J', 'U', 'N', 'K');
			dwHeader[1] = 60 - 8;

			if (compressed)
				dwHeader[1] += 20;

			// Compensate for difference between word padding and qword padding in format
			// chunk.
			dwHeader[1] += -(sint32)audioFmtLen & 6;
			char *dst = buffer.alloc(dwHeader[1] + 8);
			memcpy(dst, dwHeader, 8);
			memset(dst + 8, 0, dwHeader[1]);
		}

		dwHeader[0] = mmioFOURCC('d', 'a', 't', 'a');
		dwHeader[1] = initial ? 0x7E000000 : (uint32)mBytesWritten;

		memcpy(buffer.alloc(8), dwHeader, 8);
	}

	if (initial)
		mpFileAsync->FastWrite(buffer.data(), buffer.size());
	else
		mpFileAsync->Write(0, buffer.data(), buffer.size());

	// The header size had better not change between init() and finalize().
	VDASSERT(initial || mHeaderSize == buffer.size());

	mHeaderSize = buffer.size();
}
