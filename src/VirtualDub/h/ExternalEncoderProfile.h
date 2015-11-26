//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2010 Avery Lee
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

#ifndef f_EXTERNALENCODERPROFILE_H
#define f_EXTERNALENCODERPROFILE_H

#include <vd2/system/VDString.h>
#include <vd2/system/refcount.h>

enum VDExtEncType {
	kVDExtEncType_Video,
	kVDExtEncType_Audio,
	kVDExtEncType_Mux,
	kVDExtEncTypeCount
};

enum VDExtEncInputFormat {
	kVDExtEncInputFormat_Raw,
	kVDExtEncInputFormat_WAV,
	kVDExtEncInputFormatCount
};

class VDExtEncProfile : public vdrefcount {
public:
	VDExtEncProfile();

	VDStringW	mName;
	VDStringW	mProgram;
	VDStringW	mCommandArguments;
	VDStringW	mOutputFilename;
	VDExtEncType	mType;

	VDExtEncInputFormat	mInputFormat;
	VDStringW	mPixelFormat;

	bool	mbCheckReturnCode;
	bool	mbLogStdout;
	bool	mbLogStderr;
	bool	mbPredeleteOutputFile;
	bool	mbBypassCompression;
};

class VDExtEncSet : public vdrefcount {
public:
	VDExtEncSet();

	VDStringW	mName;
	VDStringW	mVideoEncoder;
	VDStringW	mAudioEncoder;
	VDStringW	mMultiplexer;
	VDStringW	mFileDesc;
	VDStringW	mFileExt;
	bool		mbProcessPartialOutput;
	bool		mbUseOutputAsTemp;
};

uint32 VDGetExternalEncoderProfileCount();
bool VDGetExternalEncoderProfileByIndex(uint32 idx, VDExtEncProfile **pp);
bool VDGetExternalEncoderProfileByName(const wchar_t *name, VDExtEncProfile **pp);
void VDAddExternalEncoderProfile(VDExtEncProfile *profile);
void VDRemoveExternalEncoderProfile(VDExtEncProfile *profile);

uint32 VDGetExternalEncoderSetCount();
bool VDGetExternalEncoderSetByIndex(uint32 idx, VDExtEncSet **pp);
bool VDGetExternalEncoderSetByName(const wchar_t *name, VDExtEncSet **pp);
void VDAddExternalEncoderSet(VDExtEncSet *eset);
void VDRemoveExternalEncoderSet(VDExtEncSet *eset);

void VDLoadExternalEncoderProfiles();
void VDSaveExternalEncoderProfiles();
void VDShutdownExternalEncoderProfiles();

#endif
