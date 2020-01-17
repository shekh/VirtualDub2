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

#ifndef f_VIRTUALDUB_JOB_H
#define f_VIRTUALDUB_JOB_H

#include "DubOutput.h"

class DubOptions;
class InputFilenameNode;
template<class T> class List2;
struct VDAVIOutputRawVideoFormat;

class VDFile;
class VDProject;

void OpenJobWindow();
void CloseJobWindow();
bool InitJobSystem();
void DeinitJobSystem();
bool JobPollAutoRun();
void JobSetQueueFile(const wchar_t *filename, bool distributed, bool autorun);
void JobFlushFilterConfig();

struct JobRequest{
	const VDProject* project;
	const DubOptions *opt;
	const wchar_t *pszInputDriver;
	int inputFlags;
	List2<InputFilenameNode> *pListAppended;

	VDStringW fileInput;
	VDStringW fileOutput;
	bool bIncludeEditList;

	VDStringW dataSubdir;

	JobRequest() {
		project = 0;
		opt = 0;
		pszInputDriver = 0;
		inputFlags = 0;
		pListAppended = 0;

		bIncludeEditList = true;
	}
};

struct JobRequestVideo: public JobRequest{
	bool fCompatibility;
	long lSegmentCount;
	long lSpillThreshold;
	long lSpillFrameThreshold;
	int spillDigits;

	JobRequestVideo() {
		fCompatibility = false;
		lSegmentCount = 0;
		lSpillThreshold = 0;
		lSpillFrameThreshold = 0;
		spillDigits = 0;
	}
};

struct JobRequestAudio: public JobRequest{
	bool raw;
	bool auto_w64;

	JobRequestAudio() {
		raw = false;
		auto_w64 = true;
	}
};

struct JobRequestImages: public JobRequest{
	VDStringW filePrefix;
	VDStringW fileSuffix;
	int minDigits;
	int startDigit;
	int imageFormat;
	int quality;
};

struct JobRequestRawVideo: public JobRequest{
	VDAVIOutputRawVideoFormat format;
};

struct JobRequestExtVideo: public JobRequest{
	VDStringW encSetName;
};

void SetProject(JobRequest& req, const VDProject* project);
void JobAddConfiguration(JobRequestVideo& req);
void JobAddConfigurationImages(JobRequestImages& req);
void JobAddConfigurationSaveAudio(JobRequestAudio& req);
void JobAddConfigurationSaveAudioPlugin(JobRequest& req);
void JobAddConfigurationSaveRawVideo(JobRequestRawVideo& req);
void JobAddConfigurationExportViaEncoder(JobRequestExtVideo& req);
void JobAddConfigurationRunVideoAnalysisPass(JobRequest& req);

void JobWriteProjectScript(VDFile& f, const VDProject* project, bool project_relative, const VDStringW& dataSubdir, DubOptions *opt, const wchar_t *srcFile, const wchar_t *srcInputDriver, int inputFlags, List2<InputFilenameNode> *pListAppended);
void JobWriteConfiguration(const wchar_t *filename, DubOptions *, bool bIncludeEditList = true, bool bIncludeTextInfo = true);
void JobLockDubber();
void JobUnlockDubber();
void JobPositionCallback(VDPosition start, VDPosition cur, VDPosition end, int progress, bool fast_update, void *cookie);
void JobClearList();
bool JobRunList();
void JobAddBatchFile(const wchar_t *srcDir, const wchar_t *dstDir);
void JobAddBatchDirectory(const wchar_t *srcDir, const wchar_t *dstDir);

#endif
