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

#include <vd2/system/file.h>
#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/vdstl.h>

#include "PositionControl.h"

#include "InputFile.h"
#include "InputFileImages.h"
#include "AudioSource.h"
#include "VideoSource.h"
#include "AVIOutput.h"
#include "AVIOutputWAV.h"
#include "AVIOutputImages.h"
#include "AVIOutputStriped.h"
#include "Dub.h"
#include "DubOutput.h"
#include "AudioFilterSystem.h"
#include "FrameSubset.h"
#include "ProgressDialog.h"
#include "oshelper.h"

#include "mpeg.h"
#include "gui.h"
#include "prefs.h"
#include "command.h"
#include "project.h"
#include "resource.h"

///////////////////////////////////////////////////////////////////////////

extern HWND					g_hWnd;
extern DubOptions			g_dubOpts;

extern wchar_t g_szInputWAVFile[MAX_PATH];

extern DubSource::ErrorMode	g_videoErrorMode;
extern DubSource::ErrorMode	g_audioErrorMode;

vdrefptr<InputFile>		inputAVI;
VDStringW g_inputDriver;
InputFileOptions	*g_pInputOpts			= NULL;

vdrefptr<IVDVideoSource>	inputVideo;
extern vdrefptr<AudioSource>	inputAudio;

IDubber				*g_dubber				= NULL;

COMPVARS2			g_Vcompression;
VDWaveFormat		*g_ACompressionFormat		= NULL;
uint32				g_ACompressionFormatSize	= 0;
VDStringA			g_ACompressionFormatHint;
vdblock<char>		g_ACompressionConfig;

VDAudioFilterGraph	g_audioFilterGraph;

VDStringW		g_FileOutDriver;
VDStringA		g_FileOutFormat;
VDStringW		g_AudioOutDriver;
VDStringA		g_AudioOutFormat;

extern VDProject *g_project;


bool				g_drawDecompressedFrame	= FALSE;
bool				g_showStatusWindow		= TRUE;

extern uint32& VDPreferencesGetRenderOutputBufferSize();
extern bool VDPreferencesGetRenderBackgroundPriority();

///////////////////////////////////////////////////////////////////////////

void AppendAVI(const wchar_t *pszFile) {
	if (inputAVI) {
		IVDStreamSource *pVSS = inputVideo->asStream();
		VDPosition lTail = pVSS->getEnd();
		VDStringW filename(g_project->ExpandProjectPath(pszFile));

		if (inputAVI->Append(filename.c_str())) {
			inputVideo->streamAppendReinit();
			if (inputAudio) inputAudio->streamAppendReinit();
			g_project->BeginTimelineUpdate();
			FrameSubset& s = g_project->GetTimeline().GetSubset();

			s.insert(s.end(), FrameSubsetNode(lTail, pVSS->getEnd() - lTail, false, 0));
			g_project->EndTimelineUpdate();
		}
	}
}

int AppendAVIAutoscanEnum(const wchar_t *pszFile) {
	wchar_t buf[MAX_PATH];
	wchar_t *s = buf, *t;
	int count = 0;

	wcscpy(buf, pszFile);

	t = VDFileSplitExt(VDFileSplitPath(s));

	if (t>buf)
		--t;

	for(;;) {
		if (!VDDoesPathExist(buf))
			break;
		
		++count;

		s = t;

		for(;;) {
			if (s<buf || !isdigit(*s)) {
				memmove(s+2, s+1, sizeof(wchar_t) * wcslen(s));
				s[1] = L'1';
				++t;
			} else {
				if (*s == L'9') {
					*s-- = L'0';
					continue;
				}
				++*s;
			}
			break;
		}
	}

  return count;
}

void AppendAVIAutoscan(const wchar_t *pszFile, bool skip_first) {
	wchar_t buf[MAX_PATH];
	wchar_t *s = buf, *t;
	int count = 0;
	VDStringW last;

	if (!inputAVI)
		return;

	IVDStreamSource *pVSS = inputVideo->asStream();
	VDPosition originalCount = pVSS->getEnd();

	wcscpy(buf, pszFile);

	t = VDFileSplitExt(VDFileSplitPath(s));

	if (t>buf)
		--t;

	try {
		for(;;) {
			if (!VDDoesPathExist(buf))
				break;
			
			if (!skip_first) {
				if (!inputAVI->Append(buf))
					break;

			last = buf;
				inputVideo->streamAppendReinit();
				if (inputAudio) inputAudio->streamAppendReinit();
				++count;
			}

			skip_first = false;
			s = t;

			for(;;) {
				if (s<buf || !isdigit(*s)) {
					memmove(s+2, s+1, sizeof(wchar_t) * wcslen(s));
					s[1] = L'1';
					++t;
				} else {
					if (*s == L'9') {
						*s-- = L'0';
						continue;
					}
					++*s;
				}
				break;
			}
		}
	} catch(const MyError& e) {
		// if the first segment failed, turn the warning into an error
		if (!count)
			throw;

		// log append errors, but otherwise eat them
		VDLog(kVDLogWarning, VDTextAToW(e.gets()));
	}

	guiSetStatus("Appended %d segments (last was \"%s\")", 255, count, VDTextWToA(last).c_str());

	if (count) {
		FrameSubset& s = g_project->GetTimeline().GetSubset();
		g_project->BeginTimelineUpdate();
		s.insert(s.end(), FrameSubsetNode(originalCount, pVSS->getEnd() - originalCount, false, 0));
		g_project->EndTimelineUpdate();
	}
}

void SaveWAV(const wchar_t *szFilename, bool auto_w64, bool fProp, DubOptions *quick_opts) {
	if (!inputVideo)
		throw MyError("No input file to process.");

	if (!inputAudio)
		throw MyError("No audio stream to process.");

	VDAVIOutputWAVSystem wavout(szFilename, auto_w64);
	g_project->RunOperation(&wavout, TRUE, quick_opts, 0, fProp);
}

///////////////////////////////////////////////////////////////////////////

void SavePlugin(const wchar_t *szFilename, IVDOutputDriver* driver, const char* format, bool fProp, DubOptions *quick_opts, bool removeAudio, bool removeVideo) {
	VDAVIOutputPluginSystem fileout(szFilename);

	fileout.SetDriver(driver,format);
	fileout.SetTextInfo(g_project->GetTextInfo());

	int type = 0;
	if (removeVideo){ type = 1; fileout.fAudioOnly = true; }
	if (removeAudio) type = 3;

	g_project->RunOperation(&fileout, type, quick_opts, g_prefs.main.iDubPriority, fProp, 0, 0, VDPreferencesGetRenderBackgroundPriority());
}

void SaveAVI(const wchar_t *szFilename, bool fProp, DubOptions *quick_opts, bool fCompatibility, bool removeAudio) {
	VDAVIOutputFileSystem fileout;

	fileout.Set1GBLimit(g_prefs.fAVIRestrict1Gb != 0);
	fileout.SetCaching(false);
	fileout.SetIndexing(!fCompatibility);
	fileout.SetFilename(szFilename);
	fileout.SetBuffer(VDPreferencesGetRenderOutputBufferSize());
	fileout.SetTextInfo(g_project->GetTextInfo());

	g_project->RunOperation(&fileout, removeAudio ? 3:FALSE, quick_opts, g_prefs.main.iDubPriority, fProp, 0, 0, VDPreferencesGetRenderBackgroundPriority());
}

void SaveStripedAVI(const wchar_t *szFile) {
	if (!inputVideo)
		throw MyError("No input video stream to process.");

	VDAVIOutputStripedSystem outstriped(szFile);

	outstriped.Set1GBLimit(g_prefs.fAVIRestrict1Gb != 0);

	g_project->RunOperation(&outstriped, FALSE, NULL, g_prefs.main.iDubPriority, false, 0, 0, VDPreferencesGetRenderBackgroundPriority());
}

void SaveStripeMaster(const wchar_t *szFile) {
	if (!inputVideo)
		throw MyError("No input video stream to process.");

	VDAVIOutputStripedSystem outstriped(szFile);

	outstriped.Set1GBLimit(g_prefs.fAVIRestrict1Gb != 0);

	g_project->RunOperation(&outstriped, 2, NULL, g_prefs.main.iDubPriority, false, 0, 0, VDPreferencesGetRenderBackgroundPriority());
}

void SaveSegmentedAVI(const wchar_t *szFilename, bool fProp, DubOptions *quick_opts, long lSpillThreshold, long lSpillFrameThreshold, int digits) {
	if (!inputVideo)
		throw MyError("No input file to process.");

	if (digits < 1 || digits > 10)
		throw MyError("Invalid digit count: %d", digits);

	VDAVIOutputFileSystem outfile;

	outfile.SetIndexing(false);
	outfile.SetCaching(false);
	outfile.SetBuffer(VDPreferencesGetRenderOutputBufferSize());

	const VDStringW filename(szFilename);
	outfile.SetFilenamePattern(VDFileSplitExtLeft(filename).c_str(), VDFileSplitExtRight(filename).c_str(), digits);

	g_project->RunOperation(&outfile, FALSE, quick_opts, g_prefs.main.iDubPriority, fProp, lSpillThreshold, lSpillFrameThreshold, VDPreferencesGetRenderBackgroundPriority());
}

void SaveImageSequence(const wchar_t *szPrefix, const wchar_t *szSuffix, int minDigits, bool fProp, DubOptions *quick_opts, int targetFormat, int quality) {
	VDAVIOutputImagesSystem outimages;

	outimages.SetFilenamePattern(szPrefix, szSuffix, minDigits);
	outimages.SetFormat(targetFormat, quality);
		
	g_project->RunOperation(&outimages, FALSE, quick_opts, g_prefs.main.iDubPriority, fProp, 0, 0, VDPreferencesGetRenderBackgroundPriority());
}

///////////////////////////////////////////////////////////////////////////


void SetSelectionStart(long ms) {
}

void SetSelectionEnd(long ms) {
}

void ScanForUnreadableFrames(FrameSubset *pSubset, IVDVideoSource *pVideoSource) {
	IVDStreamSource *pVSS = pVideoSource->asStream();
	const VDPosition lFirst = pVSS->getStart();
	const VDPosition lLast = pVSS->getEnd();
	VDPosition lFrame = lFirst;
	vdblock<char>	buffer;

	IVDStreamSource::ErrorMode oldErrorMode(pVSS->getDecodeErrorMode());
	pVSS->setDecodeErrorMode(IVDStreamSource::kErrorModeReportAll);

	try {
		ProgressDialog pd(g_hWnd, "Frame scan", "Scanning for unreadable frames", VDClampToSint32(lLast-lFrame), true);
		bool bLastValid = true;
		VDPosition lRangeFirst;
		long lDeadFrames = 0;
		long lMaskedFrames = 0;

		pd.setValueFormat("Frame %d of %d");

		pVideoSource->streamBegin(false, true);

		const uint32 padSize = pVideoSource->streamGetDecodePadding();

		while(lFrame <= lLast) {
			uint32 lActualBytes, lActualSamples;
			int err;
			bool bValid;

			pd.advance(VDClampToSint32(lFrame - lFirst));
			pd.check();

			do {
				bValid = false;

				if (!bLastValid && !pVideoSource->isKey(lFrame))
					break;

				if (lFrame < lLast) {
					err = pVSS->read(lFrame, 1, NULL, 0, &lActualBytes, &lActualSamples);

					if (err)
						break;

					if (buffer.empty() || buffer.size() < lActualBytes + padSize)
						buffer.resize(((lActualBytes + !lActualBytes + padSize + 65535) & ~65535));

					err = pVSS->read(lFrame, 1, buffer.data(), buffer.size() - padSize, &lActualBytes, &lActualSamples);

					if (err)
						break;

					pVideoSource->streamFillDecodePadding(buffer.data(), lActualBytes);

					try {
						pVideoSource->streamGetFrame(buffer.data(), lActualBytes, FALSE, lFrame, lFrame);
					} catch(...) {
						++lDeadFrames;
						break;
					}
				}

				bValid = true;
			} while(false);

			if (!bValid)
				++lMaskedFrames;

			if (bValid ^ bLastValid) {
				if (!bValid)
					lRangeFirst = lFrame;
				else
					pSubset->setRange(lRangeFirst, lFrame - lRangeFirst, true, 0);

				bLastValid = bValid;
			}

			++lFrame;
		}

		pVSS->streamEnd();

		guiSetStatus("%ld frames masked (%ld frames bad, %ld frames good but undecodable)", 255, lMaskedFrames, lDeadFrames, lMaskedFrames-lDeadFrames);

	} catch(...) {
		pVSS->setDecodeErrorMode(oldErrorMode);
		pVideoSource->invalidateFrameBuffer();
		throw;
	}
	pVSS->setDecodeErrorMode(oldErrorMode);
	pVideoSource->invalidateFrameBuffer();

	g_project->DisplayFrame();
}
