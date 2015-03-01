//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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
#include "plugins.h"

extern const VDPluginInfo
	apluginDef_input,
	apluginDef_lowpass,
	apluginDef_highpass,
	apluginDef_butterfly,
	apluginDef_stereosplit,
	apluginDef_stereomerge,
	apluginDef_playback,
	apluginDef_resample,
	apluginDef_output,
	apluginDef_sink,
	apluginDef_pitchshift,
	apluginDef_pitchscale,
	apluginDef_stretch,
	apluginDef_discard,
	apluginDef_centercut,
	apluginDef_centermix,
	apluginDef_gain,
	apluginDef_stereochorus,
	apluginDef_split,
	apluginDef_mix,
	apluginDef_newrate,
	apluginDef_formatconv,
	apluginDef_timestretch;

static const VDPluginInfo *const g_builtin_audio_filters[]={
	&apluginDef_input,
	&apluginDef_lowpass,
	&apluginDef_highpass,
	&apluginDef_butterfly,
	&apluginDef_stereosplit,
	&apluginDef_stereomerge,
	&apluginDef_playback,
	&apluginDef_resample,
	&apluginDef_output,
	&apluginDef_sink,
	&apluginDef_pitchshift,
	&apluginDef_pitchscale,
	&apluginDef_stretch,
	&apluginDef_discard,
	&apluginDef_centercut,
	&apluginDef_centermix,
	&apluginDef_gain,
	&apluginDef_stereochorus,
	&apluginDef_split,
	&apluginDef_mix,
	&apluginDef_newrate,
	&apluginDef_formatconv,
	&apluginDef_timestretch,
	NULL
};

void VDInitBuiltinAudioFilters() {
	VDAddInternalPlugins(g_builtin_audio_filters);
}
