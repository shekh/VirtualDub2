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

#ifndef f_VIRTUALDUB_COMMAND_H
#define f_VIRTUALDUB_COMMAND_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/refcount.h>
#include <vd2/system/VDString.h>
#include <vd2/Riza/audiocodec.h>

class InputFile;
class IVDInputDriver;
class IVDOutputDriver;
class IVDVideoSource;
class AVIOutput;
class VideoSource;
class AudioSource;
class IDubber;
class DubOptions;
class FrameSubset;
struct VDAudioFilterGraph;

extern vdrefptr<InputFile>		inputAVI;
extern VDStringW  g_inputDriver;

extern vdrefptr<IVDVideoSource>	inputVideo;

extern IDubber				*g_dubber;

extern VDWaveFormat		*g_ACompressionFormat;
extern uint32			g_ACompressionFormatSize;
extern VDStringA		g_ACompressionFormatHint;
extern vdblock<char>	g_ACompressionConfig;

extern VDAudioFilterGraph	g_audioFilterGraph;

extern VDStringW		g_FileOutDriver;
extern VDStringA		g_FileOutFormat;

extern bool				g_drawDecompressedFrame;
extern bool				g_showStatusWindow;

///////////////////////////

void AppendAVI(const wchar_t *pszFile);
int AppendAVIAutoscanEnum(const wchar_t *pszFile);
void AppendAVIAutoscan(const wchar_t *pszFile, bool skip_first=false);
void SaveWAV(const wchar_t *szFilename, bool auto_w64=true, bool fProp = false, DubOptions *quick_opts=NULL);
void SaveAVI(const wchar_t *szFilename, bool fProp = false, DubOptions *quick_opts=NULL, bool fCompatibility=false, bool removeAudio=false);
void SavePlugin(const wchar_t *szFilename, IVDOutputDriver* driver, const char* format, bool fProp = false, DubOptions *quick_opts=NULL, bool removeAudio=false);
void SaveStripedAVI(const wchar_t *szFile);
void SaveStripeMaster(const wchar_t *szFile);
void SaveSegmentedAVI(const wchar_t *szFilename, bool fProp, DubOptions *quick_opts, long lSpillThreshold, long lSpillFrameThreshold, int digits);
void SaveImageSequence(const wchar_t *szPrefix, const wchar_t *szSuffix, int minDigits, bool fProp, DubOptions *quick_opts, int targetFormat, int quality);
void EnsureSubset();
void ScanForUnreadableFrames(FrameSubset *pSubset, IVDVideoSource *pVideoSource);

#endif
