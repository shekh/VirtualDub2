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

class DubOptions;
class InputFilenameNode;
template<class T> class List2;
struct VDAVIOutputRawVideoFormat;

class VDFile;

void OpenJobWindow();
void CloseJobWindow();
bool InitJobSystem();
void DeinitJobSystem();
bool JobPollAutoRun();
void JobSetQueueFile(const wchar_t *filename, bool distributed, bool autorun);
void JobAddConfiguration(const DubOptions *, const wchar_t *szFileInput, const wchar_t *pszInputDriver, const wchar_t *szFileOutput, bool fUseCompatibility, List2<InputFilenameNode> *pListAppended, long lSpillThreshold, long lSpillFrameThreshold, bool bIncludeEditList, int digits);
void JobAddConfigurationImages(const DubOptions *opt, const wchar_t *szFileInput, const wchar_t *pszInputDriver, const wchar_t *szFileOutputPrefix, const wchar_t *szFileOutputSuffix, int minDigits, int imageFormat, int quality, List2<InputFilenameNode> *pListAppended);
void JobAddConfigurationSaveAudio(const DubOptions *opt, const wchar_t *srcFile, const wchar_t *srcInputDriver, List2<InputFilenameNode> *pListAppended, const wchar_t *dstFile, bool raw, bool includeEditList);
void JobAddConfigurationSaveVideo(const DubOptions *opt, const wchar_t *srcFile, const wchar_t *srcInputDriver, List2<InputFilenameNode> *pListAppended, const wchar_t *dstFile, bool includeEditList, const VDAVIOutputRawVideoFormat& format);
void JobAddConfigurationExportViaEncoder(const DubOptions *opt, const wchar_t *srcFile, const wchar_t *srcInputDriver, List2<InputFilenameNode> *pListAppended, const wchar_t *dstFile, bool includeEditList, const wchar_t *encSetName);
void JobAddConfigurationRunVideoAnalysisPass(const DubOptions *opt, const wchar_t *srcFile, const wchar_t *srcInputDriver, List2<InputFilenameNode> *pListAppended, bool includeEditList);
void JobWriteAutoSave(VDFile& f, DubOptions *opt, const wchar_t *srcFile, const wchar_t *srcInputDriver, List2<InputFilenameNode> *pListAppended);
void JobWriteConfiguration(const wchar_t *filename, DubOptions *, bool bIncludeEditList = true, bool bIncludeTextInfo = true);
void JobLockDubber();
void JobUnlockDubber();
void JobPositionCallback(VDPosition start, VDPosition cur, VDPosition end, int progress, void *cookie);
void JobClearList();
bool JobRunList();
void JobAddBatchFile(const wchar_t *srcDir, const wchar_t *dstDir);
void JobAddBatchDirectory(const wchar_t *srcDir, const wchar_t *dstDir);

#endif
