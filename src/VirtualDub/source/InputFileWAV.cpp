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

#include <process.h>

#include <windows.h>
#include <vfw.h>
#include <commdlg.h>

#include "InputFile.h"
#include "InputFileWAV.h"
#include "AudioSource.h"
#include "VideoSource.h"
#include "VideoSourceAVI.h"
#include <vd2/system/debug.h>
#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/Dita/resources.h>
#include <vd2/Dita/services.h>
#include <vd2/Riza/audioformat.h>
#include "AVIStripeSystem.h"
#include "AVIReadHandler.h"

#include "gui.h"
#include "oshelper.h"
#include "prefs.h"
#include "misc.h"

#include "resource.h"

extern uint32& VDPreferencesGetRenderWaveBufferSize();

/////////////////////////////////////////////////////////////////////

// TODO: Merge this with defs from AVIOutputWAV.cpp.
namespace
{
	static const uint8 kGuidRIFF[16]={
		// {66666972-912E-11CF-A5D6-28DB04C10000}
		0x72, 0x69, 0x66, 0x66, 0x2E, 0x91, 0xCF, 0x11, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00
	};

	static const uint8 kGuidLIST[16]={
		// {7473696C-912E-11CF-A5D6-28DB04C10000}
		0x6C, 0x69, 0x73, 0x74, 0x2E, 0x91, 0xCF, 0x11, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00
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

//////////////////////////////////////////////////////////////////////////////

VDAudioSourceWAV::VDAudioSourceWAV(VDInputFileWAV *parent)
	: mpParent(parent)
	, mBytesPerSample(parent->GetBytesPerSample())
{
	mSampleFirst	= 0;
	mSampleLast		= parent->GetDataLength() / mBytesPerSample;

	const vdstructex<VDWaveFormat>& format = parent->GetFormat();

	memcpy(allocFormat(format.size()), format.data(), format.size());

	streamInfo.fccType					= streamtypeAUDIO;
	streamInfo.fccHandler				= 0;
	streamInfo.dwFlags					= 0;
	streamInfo.wPriority				= 0;
	streamInfo.wLanguage				= 0;
	streamInfo.dwInitialFrames			= 0;
	streamInfo.dwScale					= mBytesPerSample;
	streamInfo.dwRate					= getWaveFormat()->mDataRate;
	streamInfo.dwStart					= 0;
	streamInfo.dwLength					= VDClampToUint32(mSampleLast);
	streamInfo.dwSuggestedBufferSize	= 0;
	streamInfo.dwQuality				= 0xffffffff;
	streamInfo.dwSampleSize				= mBytesPerSample;
}

VDAudioSourceWAV::~VDAudioSourceWAV() {
}

int VDAudioSourceWAV::_read(VDPosition lStart, uint32 lCount, void *buffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead) {
	uint32 bytes = lCount * mBytesPerSample;

	if (bytes > cbBuffer) {
		bytes = cbBuffer - cbBuffer % mBytesPerSample;
		lCount = bytes / mBytesPerSample;
	}
	
	if (buffer) {
		mpParent->ReadSpan(mBytesPerSample*lStart, buffer, bytes);
	}

	*lSamplesRead = lCount;
	*lBytesRead = bytes;

	return IVDStreamSource::kOK;
}

/////////////////////////////////////////////////////////////////////

VDInputFileWAV::VDInputFileWAV()
	: mBufferedFile(&mFile, VDPreferencesGetRenderWaveBufferSize())
{
}

VDInputFileWAV::~VDInputFileWAV() {
}

void VDInputFileWAV::Init(const wchar_t *szFile) {
	mFile.open(szFile);

	// Read the first 12 bytes of the file. They must always be RIFF <size> WAVE for a WAVE
	// file. We deliberately ignore the length of the RIFF and only use the length of the data
	// chunk.
	uint32 ckinfo[10];

	mBufferedFile.Read(ckinfo, 12);
	if (ckinfo[0] == mmioFOURCC('R', 'I', 'F', 'F') && ckinfo[2] == mmioFOURCC('W', 'A', 'V', 'E')) {
		ParseWAVE();
		goto ok;
	} else if (ckinfo[0] == mmioFOURCC('r', 'i', 'f', 'f')) {
		mBufferedFile.Read(ckinfo+3, 40 - 12);

		if (!memcmp(ckinfo, kGuidRIFF, 16) && !memcmp(ckinfo + 6, kGuidWAVE, 16)) {
			ParseWAVE64();
			goto ok;
		}
	}

	throw MyError("\"%ls\" is not a WAVE file.", mBufferedFile.GetNameForError());

ok:
	mBytesPerSample	= mWaveFormat->mBlockSize;
}

bool VDInputFileWAV::GetVideoSource(int index, IVDVideoSource **ppSrc) {
	return false;
}

bool VDInputFileWAV::GetAudioSource(int index, AudioSource **ppSrc) {
	if (index)
		return false;

	*ppSrc = new VDAudioSourceWAV(this);
	if (!*ppSrc)
		return false;

	(*ppSrc)->AddRef();
	return true;
}

void VDInputFileWAV::ReadSpan(sint64 pos, void *buffer, uint32 len) {
	mBufferedFile.Seek(mDataStart + pos);
	mBufferedFile.Read(buffer, len);
}

void VDInputFileWAV::ParseWAVE() {
	// iteratively open chunks
	static const uint32 kFoundFormat = 1;
	static const uint32 kFoundData = 2;

	uint32 notFoundYet = kFoundFormat | kFoundData;
	while(notFoundYet != 0) {
		uint32 ckinfo[2];

		// read chunk and chunk id
		if (8 != mBufferedFile.ReadData(ckinfo, 8))
			throw MyError("\"%ls\" is incomplete and could not be opened as a WAVE file.", mBufferedFile.GetNameForError());

		uint32 size = ckinfo[1];
		uint32 sizeToSkip = (size + 1) & ~1;	// RIFF chunks are dword aligned.

		switch(ckinfo[0]) {
			case mmioFOURCC('f', 'm', 't', ' '):
				if (size > 0x100000)
					throw MyError("\"%ls\" contains a format block that is too large (%u bytes).", mBufferedFile.GetNameForError(), size);

				mWaveFormat.resize(size);
				mBufferedFile.Read(mWaveFormat.data(), size);
				sizeToSkip -= size;
				notFoundYet &= ~kFoundFormat;
				break;

			case mmioFOURCC('d', 'a', 't', 'a'):
				mDataStart = mBufferedFile.Pos();

				// truncate length if it extends beyond file
				mDataLength = std::min<sint64>(size, mBufferedFile.Length() - mDataStart);
				notFoundYet &= ~kFoundData;
				break;

			case mmioFOURCC('L', 'I', 'S', 'T'):
				if (size < 4)
					throw MyError("\"%ls\" contains a structural error at position %08llx and cannot be loaded.", mBufferedFile.GetNameForError(), mBufferedFile.Pos() - 8);
				sizeToSkip = 4;
				break;
		}

		mBufferedFile.Skip(sizeToSkip);
	}
}

void VDInputFileWAV::ParseWAVE64() {
	// iteratively open chunks
	static const uint32 kFoundFormat = 1;
	static const uint32 kFoundData = 2;

	uint32 notFoundYet = kFoundFormat | kFoundData;
	while(notFoundYet != 0) {
		struct {
			uint8 guid[16];
			uint64 size;
		} ck;

		// read chunk and chunk id
		if (24 != mBufferedFile.ReadData(&ck, 24))
			break;

		// unlike RIFF, WAVE64 includes the chunk header in the chunk size.
		if (ck.size < 24)
			throw MyError("\"%ls\" contains a structural error at position %08llx and cannot be loaded.", mBufferedFile.GetNameForError(), mBufferedFile.Pos() - 8);

		sint64 sizeToSkip = (ck.size + 7 - 24) & ~7;		// WAVE64 chunks are 8-byte aligned.

		if (!memcmp(ck.guid, kGuidfmt, 16)) {
			if (ck.size > 0x100000)
				throw MyError("\"%ls\" contains a format block that is too large (%llu bytes).", mBufferedFile.GetNameForError(), (unsigned long long)ck.size);

			mWaveFormat.resize((uint32)ck.size - 24);
			mBufferedFile.Read(mWaveFormat.data(), mWaveFormat.size());
			sizeToSkip -= mWaveFormat.size();
			notFoundYet &= ~kFoundFormat;
		} else if (!memcmp(ck.guid, kGuiddata, 16)) {
			mDataStart = mBufferedFile.Pos();

			// truncate length if it extends beyond file
			mDataLength = std::min<sint64>(ck.size - 24, mBufferedFile.Length() - mDataStart);
		} else if (!memcmp(ck.guid, kGuidLIST, 16)) {
			sizeToSkip = 8;
		}

		mBufferedFile.Skip(sizeToSkip);
	}
}

/////////////////////////////////////////////////////////////////////

class VDInputDriverWAV : public vdrefcounted<IVDInputDriver> {
public:
	const wchar_t *GetSignatureName() { return L"Wave input driver (internal)"; }

	int GetDefaultPriority() {
		return -4;
	}

	uint32 GetFlags() { return kF_Audio; }

	const wchar_t *GetFilenamePattern() {
		return L"Wave file (*.wav,*.w64)\0*.wav;*.w64\0";
	}

	bool DetectByFilename(const wchar_t *pszFilename) {
		size_t l = wcslen(pszFilename);
		if (l > 4) {
			if (!_wcsicmp(pszFilename + l - 4, L".wav"))
				return true;

			if (!_wcsicmp(pszFilename + l - 4, L".w64"))
				return true;
		}

		return false;
	}

	DetectionConfidence DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) {
		if (nHeaderSize >= 12) {
			if (!memcmp(pHeader, "RIFF", 4) && !memcmp((char*)pHeader+8, "WAVE", 4))
				return kDC_High;

			if (nHeaderSize >= 40) {
				if (!memcmp(pHeader, kGuidRIFF, 16) && !memcmp((const char *)pHeader + 24, kGuidWAVE, 16))
					return kDC_High;
			}
		}

		return kDC_None;
	}

	InputFile *CreateInputFile(uint32 flags) {
		VDInputFileWAV *pf = new_nothrow VDInputFileWAV;

		if (!pf)
			throw MyMemoryError();

		return pf;
	}
};

IVDInputDriver *VDCreateInputDriverWAV() {
	return new VDInputDriverWAV;
}
