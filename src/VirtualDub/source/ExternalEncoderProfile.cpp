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

#include "stdafx.h"
#include <vd2/system/registry.h>
#include "ExternalEncoderProfile.h"

VDExtEncProfile::VDExtEncProfile()
	: mOutputFilename(L"%(outputname).audio")
	, mType(kVDExtEncType_Audio)
	, mInputFormat(kVDExtEncInputFormat_Raw)
	, mbCheckReturnCode(true)
	, mbLogStdout(true)
	, mbLogStderr(false)
	, mbPredeleteOutputFile(false)
	, mbBypassCompression(false)
{
}

VDExtEncSet::VDExtEncSet()
	: mbProcessPartialOutput(false)
	, mbUseOutputAsTemp(false)
{
}

///////////////////////////////////////////////////////////////////////////

namespace {
	typedef vdfastvector<VDExtEncProfile *> ExtEncProfiles;
	typedef vdfastvector<VDExtEncSet *> ExtEncSets;

	ExtEncProfiles g_VDExtEncProfiles;
	ExtEncSets g_VDExtEncSets;
}

uint32 VDGetExternalEncoderProfileCount() {
	return g_VDExtEncProfiles.size();
}

bool VDGetExternalEncoderProfileByIndex(uint32 idx, VDExtEncProfile **pp) {
	if (idx >= g_VDExtEncProfiles.size())
		return false;

	*pp = g_VDExtEncProfiles[idx];
	(*pp)->AddRef();
	return true;
}

bool VDGetExternalEncoderProfileByName(const wchar_t *name, VDExtEncProfile **pp) {
	for(ExtEncProfiles::const_iterator it(g_VDExtEncProfiles.begin()), itEnd(g_VDExtEncProfiles.end());
		it != itEnd; ++it)
	{
		VDExtEncProfile *profile = *it;

		if (profile->mName == name) {
			if (pp) {
				*pp = profile;
				profile->AddRef();
			}
			return true;
		}
	}

	return false;
}

void VDAddExternalEncoderProfile(VDExtEncProfile *profile) {
	ExtEncProfiles::iterator it(std::find(g_VDExtEncProfiles.begin(), g_VDExtEncProfiles.end(), profile));

	if (it == g_VDExtEncProfiles.end()) {
		g_VDExtEncProfiles.push_back(profile);
		profile->AddRef();
	}
}

void VDRemoveExternalEncoderProfile(VDExtEncProfile *profile) {
	ExtEncProfiles::iterator it(std::find(g_VDExtEncProfiles.begin(), g_VDExtEncProfiles.end(), profile));

	if (it != g_VDExtEncProfiles.end()) {
		*it = g_VDExtEncProfiles.back();
		g_VDExtEncProfiles.pop_back();
		profile->Release();
	}
}

uint32 VDGetExternalEncoderSetCount() {
	return g_VDExtEncSets.size();
}

bool VDGetExternalEncoderSetByIndex(uint32 idx, VDExtEncSet **pp) {
	if (idx >= g_VDExtEncSets.size())
		return false;

	*pp = g_VDExtEncSets[idx];
	(*pp)->AddRef();
	return true;
}

bool VDGetExternalEncoderSetByName(const wchar_t *name, VDExtEncSet **pp) {
	for(ExtEncSets::const_iterator it(g_VDExtEncSets.begin()), itEnd(g_VDExtEncSets.end());
		it != itEnd; ++it)
	{
		VDExtEncSet *eset = *it;

		if (eset->mName == name) {
			if (pp) {
				*pp = eset;
				eset->AddRef();
			}
			return true;
		}
	}

	return false;
}

void VDAddExternalEncoderSet(VDExtEncSet *eset) {
	ExtEncSets::iterator it(std::find(g_VDExtEncSets.begin(), g_VDExtEncSets.end(), eset));

	if (it == g_VDExtEncSets.end()) {
		g_VDExtEncSets.push_back(eset);
		eset->AddRef();
	}
}

void VDRemoveExternalEncoderSet(VDExtEncSet *eset) {
	ExtEncSets::iterator it(std::find(g_VDExtEncSets.begin(), g_VDExtEncSets.end(), eset));

	if (it != g_VDExtEncSets.end()) {
		*it = g_VDExtEncSets.back();
		g_VDExtEncSets.pop_back();
		eset->Release();
	}
}

void VDLoadExternalEncoderProfiles() {
	VDRegistryAppKey key("External Encoders");
	VDRegistryKeyIterator keyIt(key);

	while(const char *name = keyIt.Next()) {
		if (!strncmp(name, "Profile ", 8)) {
			VDRegistryKey subKey(key, name, false);
			vdrefptr<VDExtEncProfile> profile(new VDExtEncProfile);

			if (!subKey.getString("Name", profile->mName) || VDGetExternalEncoderProfileByName(profile->mName.c_str(), NULL))
				continue;

			VDStringW cmdLine;
			if (subKey.getString("Command Line", cmdLine)) {
				size_t n = cmdLine.size();
				size_t base = 0;
				size_t split = n;

				while(base < n && cmdLine[base] == L' ')
					++base;

				if (base < n && cmdLine[base] == L'"') {
					size_t enquote = cmdLine.find(L'"', base + 1);
					size_t term = n;

					if (enquote != VDStringW::npos) {
						term = enquote;
						split = enquote + 1;
					}

					profile->mProgram.assign(cmdLine, base + 1, term - (base + 1));
				} else {
					split = cmdLine.find(L' ', base);
					profile->mProgram.assign(cmdLine, base, split - base);
				}

				while(split < n && cmdLine[split] == L' ')
					++split;

				profile->mCommandArguments.assign(cmdLine, split, n - split);
			} else {
				subKey.getString("Program", profile->mProgram);
				subKey.getString("Command Arguments", profile->mCommandArguments);
			}

			subKey.getString("Output Filename", profile->mOutputFilename);
			profile->mType = (VDExtEncType)subKey.getEnumInt("Type", kVDExtEncTypeCount, kVDExtEncType_Audio);
			subKey.getString("Pixel Format", profile->mPixelFormat);
			profile->mInputFormat = (VDExtEncInputFormat)subKey.getEnumInt("Input Format", kVDExtEncInputFormatCount, kVDExtEncInputFormat_Raw);
			profile->mbCheckReturnCode = subKey.getBool("Check Return Code", profile->mbCheckReturnCode);
			profile->mbLogStdout = subKey.getBool("Log Stdout", profile->mbLogStdout);
			profile->mbLogStderr = subKey.getBool("Log Stderr", profile->mbLogStderr);
			profile->mbPredeleteOutputFile = subKey.getBool("Predelete Output File", profile->mbPredeleteOutputFile);
			profile->mbBypassCompression = subKey.getBool("Bypass Compression", profile->mbBypassCompression);

			VDAddExternalEncoderProfile(profile);
		} else if (!strncmp(name, "Set ", 4)) {
			VDRegistryKey subKey(key, name, false);
			vdrefptr<VDExtEncSet> eset(new VDExtEncSet);

			if (!subKey.getString("Name", eset->mName) || VDGetExternalEncoderSetByName(eset->mName.c_str(), NULL))
				continue;

			subKey.getString("Video Encoder", eset->mVideoEncoder);
			subKey.getString("Audio Encoder", eset->mAudioEncoder);
			subKey.getString("Multiplexer", eset->mMultiplexer);
			subKey.getString("File Description", eset->mFileDesc);
			subKey.getString("File Extension", eset->mFileExt);
			eset->mbProcessPartialOutput = subKey.getBool("Process Partial Output", eset->mbProcessPartialOutput);
			eset->mbUseOutputAsTemp = subKey.getBool("Use Output As Temp Path", eset->mbUseOutputAsTemp);

			VDAddExternalEncoderSet(eset);
		}
	}
}

void VDSaveExternalEncoderProfiles() {
	VDRegistryAppKey key("External Encoders", true);

	// delete existing keys
	vdvector<VDStringA> keys;
	vdvector<VDStringA> values;
	{
		VDRegistryKeyIterator keyIt(key);
		while(const char *name = keyIt.Next()) {
			keys.push_back_as(name);
		}
	}

	while(!keys.empty()) {
		const char *name = keys.back().c_str();

		key.removeKey(name);
		keys.pop_back();
	}

	// write new keys
	VDStringA keyName;
	int counter = 1;
	for(ExtEncProfiles::const_iterator it(g_VDExtEncProfiles.begin()), itEnd(g_VDExtEncProfiles.end());
		it != itEnd; ++it)
	{
		VDExtEncProfile *profile = *it;
		keyName.sprintf("Profile %u", counter++);

		VDRegistryKey subKey(key, keyName.c_str(), true);

		subKey.setString("Name", profile->mName.c_str());
		subKey.setString("Program", profile->mProgram.c_str());
		subKey.setString("Command Arguments", profile->mCommandArguments.c_str());
		subKey.setString("Output Filename", profile->mOutputFilename.c_str());
		subKey.setInt("Type", (int)profile->mType);
		subKey.setString("Pixel Format", profile->mPixelFormat.c_str());
		subKey.setInt("Input Format", (int)profile->mInputFormat);
		subKey.setBool("Check Return Code", profile->mbCheckReturnCode);
		subKey.setBool("Log Stdout", profile->mbLogStdout);
		subKey.setBool("Log Stderr", profile->mbLogStderr);
		subKey.setBool("Predelete Output File", profile->mbPredeleteOutputFile);
		subKey.setBool("Bypass Compression", profile->mbBypassCompression);
	}

	counter = 1;
	for(ExtEncSets::const_iterator it(g_VDExtEncSets.begin()), itEnd(g_VDExtEncSets.end());
		it != itEnd; ++it)
	{
		VDExtEncSet *eset = *it;
		keyName.sprintf("Set %u", counter++);

		VDRegistryKey subKey(key, keyName.c_str(), true);

		subKey.setString("Name", eset->mName.c_str());
		subKey.setString("Video Encoder", eset->mVideoEncoder.c_str());
		subKey.setString("Audio Encoder", eset->mAudioEncoder.c_str());
		subKey.setString("Multiplexer", eset->mMultiplexer.c_str());
		subKey.setString("File Description", eset->mFileDesc.c_str());
		subKey.setString("File Extension", eset->mFileExt.c_str());
		subKey.setBool("Process Partial Output", eset->mbProcessPartialOutput);
		subKey.setBool("Use Output As Temp Path", eset->mbUseOutputAsTemp);
	}
}

void VDShutdownExternalEncoderProfiles() {
	while(!g_VDExtEncProfiles.empty()) {
		g_VDExtEncProfiles.back()->Release();
		g_VDExtEncProfiles.pop_back();
	}

	while(!g_VDExtEncSets.empty()) {
		g_VDExtEncSets.back()->Release();
		g_VDExtEncSets.pop_back();
	}
}
