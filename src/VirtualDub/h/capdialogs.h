//	VirtualDub - Video processing and capture application
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

#ifndef f_CAPDIALOGS_H
#define f_CAPDIALOGS_H

#include <vd2/system/vdstl.h>

enum {
	kVDCapDevOptSaveCurrentAudioFormat = 1,
	kVDCapDevOptSaveCurrentAudioCompFormat = 2,
	kVDCapDevOptSaveCurrentVideoFormat = 4,
	kVDCapDevOptSaveCurrentVideoCompFormat = 8,
	kVDCapDevOptSaveCurrentFrameRate = 0x10,
	kVDCapDevOptSaveCurrentDisplayMode = 0x20,
	kVDCapDevOptSwitchSourcesTogether = 0x40,
	kVDCapDevOptSlowOverlay = 0x80,
	kVDCapDevOptSlowPreview = 0x100
};

struct VDCaptureSettings {
	uint32	mFramePeriod;
	int		mVideoBufferCount;
	int		mAudioBufferCount;
	int		mAudioBufferSize;
	bool	mbDisplayPrerollDialog;
	bool	mbMaxPower;
	bool	mbEnablePower;
};

class IVDUICaptureVumeter;

bool VDShowCaptureSettingsDialog(VDGUIHandle hwndParent, VDCaptureSettings& parms);
bool VDShowCapturePreferencesDialog(VDGUIHandle h, VDCapturePreferences& prefs);
void VDCaptureLoadPreferences(VDCapturePreferences& prefs);
void VDCaptureSavePreferences(const VDCapturePreferences& prefs);
void VDShowCaptureChannelsDialog(VDGUIHandle h, const vdstructex<VDWaveFormat>& format, VDAudioMaskParam& param, IVDUICaptureVumeter** thunk);

#endif
