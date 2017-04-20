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

#include <stdafx.h>
#include <windows.h>
#include <vd2/system/filesys.h>
#include <vd2/system/file.h>
#include <vd2/system/thread.h>
#include <vd2/system/atomic.h>
#include <vd2/system/time.h>
#include <vd2/system/strutil.h>
#include <vd2/system/VDScheduler.h>
#include <vd2/system/w32assist.h>
#include <vd2/Dita/services.h>
#include <vd2/Dita/resources.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Riza/bitmap.h>
#include "project.h"
#include "VideoSource.h"
#include "AudioSource.h"
#include "InputFile.h"
#include "prefs.h"
#include "dub.h"
#include "DubOutput.h"
#include "DubStatus.h"
#include "filter.h"
#include "command.h"
#include "job.h"
#include "jobControl.h"
#include "server.h"
#include "capture.h"
#include "script.h"
#include "SceneDetector.h"
#include "oshelper.h"
#include "resource.h"
#include "uiframe.h"
#include "filters.h"
#include "FilterFrameRequest.h"
#include "gui.h"
#include "tool.h"

///////////////////////////////////////////////////////////////////////////

namespace {
	enum {
		kVDST_Project = 9
	};

	enum {
		kVDM_ReopenChangesImminent,			// The new video file has fewer video frames than the current file. Switching to it will result in changes to the edit list. Do you want to continue?
		kVDM_DeleteFrame,					// delete frame %lld (Undo/Redo)
		kVDM_DeleteFrames,					// delete %lld frames at %lld (Undo/Redo)
		kVDM_CutFrame,						// cut frame %lld (Undo/Redo)
		kVDM_CutFrames,						// cut %lld frames at %lld (Undo/Redo)
		kVDM_MaskFrame,						// mask frame %lld (Undo/Redo)
		kVDM_MaskFrames,					// mask %lld frames at %lld (Undo/Redo)
		kVDM_Paste,							// paste (Undo/Redo)
		kVDM_ScanForErrors,					// scan for errors
		kVDM_ResetTimeline,					// reset timeline
		kVDM_Crop							// crop
	};

	enum {
		kUndoLimit = 50,
		kRedoLimit = 50
	};
}

///////////////////////////////////////////////////////////////////////////

extern const char g_szError[];
extern const char g_szWarning[];

extern HINSTANCE g_hInst;

extern VDProject *g_project;
extern InputFileOptions	*g_pInputOpts;
extern COMPVARS2 g_Vcompression;

DubSource::ErrorMode	g_videoErrorMode			= DubSource::kErrorModeReportAll;
DubSource::ErrorMode	g_audioErrorMode			= DubSource::kErrorModeReportAll;

vdrefptr<AudioSource>	inputAudio;

extern bool				g_fDropFrames;
extern bool				g_fSwapPanes;
extern bool				g_bExit;

extern bool g_fJobMode;

extern wchar_t g_szInputAVIFile[MAX_PATH];
extern wchar_t g_szInputWAVFile[MAX_PATH];

extern char g_serverName[256];

extern uint32 VDPreferencesGetRenderThrottlePercent();
extern int VDPreferencesGetVideoCompressionThreadCount();
extern bool VDPreferencesGetFilterAccelEnabled();
extern bool VDPreferencesGetRenderBackgroundPriority();
extern bool VDPreferencesGetAutoRecoverEnabled();
extern bool VDPreferencesGetTimelineWarnReloadTruncation();

int VDRenderSetVideoSourceInputFormat(IVDVideoSource *vsrc, VDPixmapFormatEx format);

extern bool FiltersEditorDisplayFrame(IVDVideoSource *pVS);

///////////////////////////////////////////////////////////////////////////

namespace {

	void CopyFrameToClipboard(HWND hwnd, const VDPixmap& px) {
		if (OpenClipboard(hwnd)) {
			if (EmptyClipboard()) {
				HANDLE hMem;
				void *lpvMem;

				VDPixmapLayout layout;
				uint32 imageSize = VDMakeBitmapCompatiblePixmapLayout(layout, px.w, px.h, nsVDPixmap::kPixFormat_RGB888, 0);

				vdstructex<VDAVIBitmapInfoHeader> bih;
				VDMakeBitmapFormatFromPixmapFormat(bih, nsVDPixmap::kPixFormat_RGB888, 0, px.w, px.h);

				uint32 headerSize = bih.size();

				if (hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, headerSize + imageSize)) {
					if (lpvMem = GlobalLock(hMem)) {
						memcpy(lpvMem, bih.data(), headerSize);

						VDPixmapBlt(VDPixmapFromLayout(layout, (char *)lpvMem + headerSize), px);

						GlobalUnlock(lpvMem);
						SetClipboardData(CF_DIB, hMem);
						CloseClipboard();
						return;
					}
					GlobalFree(hMem);
				}
			}
			CloseClipboard();
		}
	}
}

///////////////////////////////////////////////////////////////////////////

class VDProjectAutoSave {
public:
	VDProjectAutoSave(VDProject *proj);
	~VDProjectAutoSave();

	void Save();
	void Delete();

protected:
	VDProject	*const mpProject;
	VDStringW	mAutoSavePath;
	VDStringW	mDataSubdir;
	VDFile		mAutoSaveFile;
};

VDProjectAutoSave::VDProjectAutoSave(VDProject *proj)
	: mpProject(proj)
{
}

VDProjectAutoSave::~VDProjectAutoSave() {
	Delete();
}

void VDProjectAutoSave::Save() {
	VDStringW fileName;
	VDStringW path;

	const uint32 signature = VDCreateAutoSaveSignature();
	for(int counter = 1; counter <= 100; ++counter) {
		fileName.sprintf(L"VirtualDub_AutoSave_%x_%u.vdproject", signature, counter);

		path = VDMakePath(VDGetDataPath(), fileName.c_str());

		if (!VDDoesPathExist(path.c_str()))
			break;

		path.clear();
	}

	if (path.empty())
		return;

	try {
		// Note that we intentionally KEEP THIS FILE OPEN. This prevents other instances of VirtualDub
		// from trying to recover while we're active!
		mAutoSaveFile.open(path.c_str(), nsVDFile::kWrite | nsVDFile::kDenyAll | nsVDFile::kCreateAlways);
		mpProject->SaveData(path,mDataSubdir);
		mpProject->SaveScript(mAutoSaveFile,mDataSubdir,false);

		mAutoSaveFile.flushNT();
	} catch(...) {
		return;
	}

	mAutoSavePath = path;
}

void VDProjectAutoSave::Delete() {
	try {
		if (!mAutoSavePath.empty()) {
			mAutoSaveFile.seek(0);
			mAutoSaveFile.truncate();
			mAutoSaveFile.close();

			VDRemoveFile(mAutoSavePath.c_str());
			mpProject->DeleteData(mAutoSavePath,mDataSubdir);
		}
	} catch(...) {
		// whatever it is, eat it -- we do NOT want to terminate due to
		// throwing from the dtor.
	}
}

///////////////////////////////////////////////////////////////////////////

class VDFilterSystemMessageLoopScheduler : public vdrefcounted<IVDFilterSystemScheduler> {
public:
	VDFilterSystemMessageLoopScheduler();

	void Reschedule();
	bool Block();

protected:
	DWORD		mThreadId;
};

VDFilterSystemMessageLoopScheduler::VDFilterSystemMessageLoopScheduler()
	: mThreadId(::GetCurrentThreadId())
{
}

void VDFilterSystemMessageLoopScheduler::Reschedule() {
	PostThreadMessage(mThreadId, WM_NULL, 0, 0);
}

bool VDFilterSystemMessageLoopScheduler::Block() {
	return true;
}

///////////////////////////////////////////////////////////////////////////

class VDProjectTimelineTimingSource : public vdrefcounted<IVDTimelineTimingSource> {
public:
	VDProjectTimelineTimingSource(IVDTimelineTimingSource *pTS, VDProject *pProject);
	~VDProjectTimelineTimingSource();

	sint64 GetStart();
	sint64 GetLength();
	const VDFraction GetRate();
	sint64 GetPrevKey(sint64 pos);
	sint64 GetNextKey(sint64 pos);
	sint64 GetNearestKey(sint64 pos);
	bool IsKey(sint64 pos);
	bool IsNullSample(sint64 pos);

protected:
	bool CheckFilters();

	vdrefptr<IVDTimelineTimingSource> mpTS;
	VDProject *mpProject;
};

VDProjectTimelineTimingSource::VDProjectTimelineTimingSource(IVDTimelineTimingSource *pTS, VDProject *pProject)
	: mpTS(pTS)
	, mpProject(pProject)
{
}

VDProjectTimelineTimingSource::~VDProjectTimelineTimingSource() {
}

sint64 VDProjectTimelineTimingSource::GetStart() {
	return 0;
}

sint64 VDProjectTimelineTimingSource::GetLength() {
	return CheckFilters() ? filters.GetOutputFrameCount() : mpTS->GetLength();
}

const VDFraction VDProjectTimelineTimingSource::GetRate() {
	return CheckFilters() ? filters.GetOutputFrameRate() : mpTS->GetRate();
}

sint64 VDProjectTimelineTimingSource::GetPrevKey(sint64 pos) {
	if (!CheckFilters())
		return mpTS->GetPrevKey(pos);

	while(--pos >= 0) {
		sint64 pos2 = filters.GetSourceFrame(pos);
		if (pos2 < 0 || mpTS->IsKey(pos2))
			break;
	}

	return pos;
}

sint64 VDProjectTimelineTimingSource::GetNextKey(sint64 pos) {
	if (!CheckFilters())
		return mpTS->GetNextKey(pos);

	sint64 len = filters.GetOutputFrameCount();

	while(++pos < len) {
		sint64 pos2 = filters.GetSourceFrame(pos);
		if (pos2 < 0 || mpTS->IsKey(pos2))
			return pos;
	}

	return -1;
}

sint64 VDProjectTimelineTimingSource::GetNearestKey(sint64 pos) {
	if (!CheckFilters())
		return mpTS->GetPrevKey(pos);

	while(pos >= 0) {
		sint64 pos2 = filters.GetSourceFrame(pos);
		if (pos2 < 0 || mpTS->IsKey(pos2))
			break;
		--pos;
	}

	return pos;
}

bool VDProjectTimelineTimingSource::IsKey(sint64 pos) {
	if (!CheckFilters())
		return mpTS->IsKey(pos);

	sint64 pos2 = filters.GetSourceFrame(pos);
	return pos2 < 0 || mpTS->IsKey(pos2);
}

bool VDProjectTimelineTimingSource::IsNullSample(sint64 pos) {
	if (!CheckFilters())
		return mpTS->IsKey(pos);

	sint64 pos2 = filters.GetSourceFrame(pos);
	return pos2 >= 0 && mpTS->IsNullSample(pos2);
}

bool VDProjectTimelineTimingSource::CheckFilters() {
	if (filters.isRunning())
		return true;

	try {
		mpProject->StartFilters();
	} catch(const MyError&) {
		// eat the error for now
	}

	return filters.isRunning();
}

///////////////////////////////////////////////////////////////////////////

VDProject::VDProject()
	: mhwnd(NULL)
	, mpCB(NULL)
	, mpSceneDetector(0)
	, mSceneShuttleMode(0)
	, mSceneShuttleAdvance(0)
	, mSceneShuttleCounter(0)
	, mpDubStatus(0)
	, mposCurrentFrame(0)
	, mposSelectionStart(0)
	, mposSelectionEnd(0)
	, mbPositionCallbackEnabled(false)
	, mbFilterChainLocked(false)
	, mbTimelineRateDirty(true)
	, mDesiredOutputFrame(-1)
	, mDesiredTimelineFrame(-1)
	, mDesiredNextInputFrame(-1)
	, mDesiredNextOutputFrame(-1)
	, mDesiredNextTimelineFrame(-1)
	, mLastDisplayedInputFrame(-1)
	, mLastDisplayedTimelineFrame(-1)
	, mbUpdateInputFrame(false)
	, mbPendingInputFrameValid(false)
	, mbPendingOutputFrameValid(false)
	, mbUpdateLong(false)
	, mFramesDecoded(0)
	, mLastDecodeUpdate(0)
	, mDisplayFrameLocks(0)
	, mbDisplayFrameDeferred(false)
	, mPreviewRestartMode(kPreviewRestart_None)
	, mVideoInputFrameRate(0,0)
	, mVideoOutputFrameRate(0,0)
	, mVideoTimelineFrameRate(0,0)
	, mVideoMarkedPostFiltersFrameRate(0,0)
	, mVideoMarkedPostFiltersFrameCount(0)
	, mAudioSourceMode(kVDAudioSourceMode_Source)
{
	filterModTimeline.project = this;
	mProjectLoading = false;
}

VDProject::~VDProject() {
#if 0
	// We have to issue prestops first to make sure that all threads have
	// spinners running.
	for(int i=0; i<mThreadCount; ++i)
		mpSchedulerThreads[i].PreStop();

	delete[] mpSchedulerThreads;
#endif
}

bool VDProject::Attach(VDGUIHandle hwnd) {	
	mhwnd = hwnd;
	return true;
}

void VDProject::Detach() {
	if (mpSceneDetector) {
		delete mpSceneDetector;
		mpSceneDetector = NULL;
	}

	if (mpDubStatus) {
		delete mpDubStatus;
		mpDubStatus = 0;
	}

	mhwnd = NULL;
}

void VDProject::SetUICallback(IVDProjectUICallback *pCB) {
	mpCB = pCB;
}

void VDProject::BeginTimelineUpdate(const wchar_t *undostr) {
	if (!undostr)
		ClearUndoStack();
	else {
		if (mUndoStack.size()+1 > kUndoLimit)
			mUndoStack.pop_back();

		mUndoStack.push_front(UndoEntry(mTimeline.GetSubset(), undostr, mposCurrentFrame, mposSelectionStart, mposSelectionEnd));
	}

	mRedoStack.clear();
}

void VDProject::EndTimelineUpdate() {
	UpdateDubParameters();
	if (mpCB)
		mpCB->UITimelineUpdated();
}

void VDProject::BeginLoading() {
	mProjectLoading = true;
}

void VDProject::EndLoading() {
	if (mProjectLoading)
		ClearUndoStack();
	mProjectLoading = false;
	EndTimelineUpdate();
	UpdateFilterList();
}

bool VDProject::Undo() {
	if (mUndoStack.empty())
		return false;

	UndoEntry& ue = mUndoStack.front();

	mTimeline.GetSubset().swap(ue.mSubset);

	if (mRedoStack.size()+1 > kRedoLimit)
		mRedoStack.pop_back();

	mRedoStack.splice(mRedoStack.begin(), mUndoStack, mUndoStack.begin());

	EndTimelineUpdate();
	MoveToFrame(ue.mFrame);
	SetSelection(mposSelectionStart, mposSelectionEnd, false);
	return true;
}

bool VDProject::Redo() {
	if (mRedoStack.empty())
		return false;

	UndoEntry& ue = mRedoStack.front();

	mTimeline.GetSubset().swap(ue.mSubset);

	if (mUndoStack.size()+1 > kUndoLimit)
		mUndoStack.pop_back();

	mUndoStack.splice(mUndoStack.begin(), mRedoStack, mRedoStack.begin());

	EndTimelineUpdate();
	MoveToFrame(ue.mFrame);
	SetSelection(mposSelectionStart, mposSelectionEnd, false);
	return true;
}

void VDProject::ClearUndoStack() {
	mUndoStack.clear();
	mRedoStack.clear();
}

const wchar_t *VDProject::GetCurrentUndoAction() {
	if (mUndoStack.empty())
		return NULL;

	const UndoEntry& ue = mUndoStack.front();

	return ue.mDescription.c_str();
}

const wchar_t *VDProject::GetCurrentRedoAction() {
	if (mRedoStack.empty())
		return NULL;

	const UndoEntry& ue = mRedoStack.front();

	return ue.mDescription.c_str();
}

bool VDProject::Tick() {
	bool active = false;

	if (inputVideo && mSceneShuttleMode) {
		if (!mpSceneDetector)
			mpSceneDetector = new_nothrow SceneDetector();

		if (mpSceneDetector) {
			mpSceneDetector->SetThresholds(g_prefs.scene.iCutThreshold, g_prefs.scene.iFadeThreshold);

			SceneShuttleStep();
			active = true;
		} else
			SceneShuttleStop();
	} else {
		if (mpSceneDetector) {
			delete mpSceneDetector;
			mpSceneDetector = NULL;
		}
	}

	if (UpdateFrame())
		active = true;

	return active;
}

VDPosition VDProject::GetCurrentFrame() {
	return mposCurrentFrame;
}

VDPosition VDProject::GetFrameCount() {
	return mTimeline.GetLength();
}

VDFraction VDProject::GetInputFrameRate() {
	return mVideoInputFrameRate;
}

VDFraction VDProject::GetTimelineFrameRate() {
	VDASSERT(!mbTimelineRateDirty);
	return mVideoTimelineFrameRate;
}

void VDProject::ClearSelection(bool notifyUser) {
	mposSelectionStart = 0;
	mposSelectionEnd = -1;
	g_dubOpts.video.mSelectionStart.mOffset = 0;
	g_dubOpts.video.mSelectionEnd.mOffset = -1;
	if (mpCB)
		mpCB->UISelectionUpdated(notifyUser);
}

bool VDProject::IsSelectionEmpty() {
	return mposSelectionStart >= mposSelectionEnd;
}

bool VDProject::IsSelectionPresent() {
	return mposSelectionStart <= mposSelectionEnd;
}

VDPosition VDProject::GetSelectionStartFrame() {
	return mposSelectionStart;
}

VDPosition VDProject::GetSelectionEndFrame() {
	return mposSelectionEnd;
}

bool VDProject::IsClipboardEmpty() {
	return mClipboard.empty();
}

bool VDProject::IsSceneShuttleRunning() {
	return mSceneShuttleMode != 0;
}

void VDProject::SetPositionCallbackEnabled(bool enable) {
	mbPositionCallbackEnabled = enable;
}

void VDProject::Cut() {
	Copy();
	DeleteInternal(true, false);
}

void VDProject::Copy() {
	FrameSubset& s = mTimeline.GetSubset();
	mClipboard.assign(s, mposSelectionStart, mposSelectionEnd - mposSelectionStart);
}

void VDProject::Paste() {
	FrameSubset& s = mTimeline.GetSubset();

	BeginTimelineUpdate(VDLoadString(0, kVDST_Project, kVDM_Paste));
	if (!IsSelectionEmpty())
		DeleteInternal(false, true);
	s.insert(mposCurrentFrame, mClipboard);
	EndTimelineUpdate();
}

void VDProject::Delete() {
	DeleteInternal(false, false);
}

void VDProject::DeleteInternal(bool tagAsCut, bool noTag) {
	VDPosition pos = GetCurrentFrame();
	VDPosition start = GetSelectionStartFrame();
	VDPosition end = GetSelectionEndFrame();

	FrameSubset& s = mTimeline.GetSubset();
	VDPosition len = 1;

	if (IsSelectionEmpty())
		start = pos;
	else
		len = end-start;

	if (!noTag) {
		if (tagAsCut) {
			if (len > 1)
				BeginTimelineUpdate(VDswprintf(VDLoadString(0, kVDST_Project, kVDM_CutFrames), 2, &start, &len).c_str());
			else
				BeginTimelineUpdate(VDswprintf(VDLoadString(0, kVDST_Project, kVDM_CutFrame), 1, &start).c_str());
		} else {
			if (len > 1)
				BeginTimelineUpdate(VDswprintf(VDLoadString(0, kVDST_Project, kVDM_DeleteFrames), 2, &start, &len).c_str());
			else
				BeginTimelineUpdate(VDswprintf(VDLoadString(0, kVDST_Project, kVDM_DeleteFrame), 1, &start).c_str());
		}
	}

	s.deleteRange(start, len);

	if (!noTag)
		EndTimelineUpdate();

	ClearSelection(false);
	MoveToFrame(start);
}

void VDProject::CropToSelection() {
	FrameSubset& s = mTimeline.GetSubset();

	if (!IsSelectionEmpty()) {
		VDPosition start = GetSelectionStartFrame();
		VDPosition end = GetSelectionEndFrame();

		BeginTimelineUpdate(VDLoadString(0, kVDST_Project, kVDM_Crop));
		s.clip(start, end-start);
		EndTimelineUpdate();

		ClearSelection(false);
		MoveToFrame(0);
	}
}

void VDProject::MaskSelection(bool bNewMode) {
	VDPosition pos = GetCurrentFrame();
	VDPosition start = GetSelectionStartFrame();
	VDPosition end = GetSelectionEndFrame();

	FrameSubset& s = mTimeline.GetSubset();
	VDPosition len = 1;

	if (IsSelectionEmpty())
		start = pos;
	else
		len = end-start;

	if (len) {
		if (len > 1)
			BeginTimelineUpdate(VDswprintf(VDLoadString(0, kVDST_Project, kVDM_MaskFrames), 2, &start, &len).c_str());
		else
			BeginTimelineUpdate(VDswprintf(VDLoadString(0, kVDST_Project, kVDM_MaskFrame), 1, &start).c_str());

		s.setRange(start, len, bNewMode, 0);

		EndTimelineUpdate();
	}
}

void VDProject::LockDisplayFrame() {
	++mDisplayFrameLocks;
}

void VDProject::UnlockDisplayFrame() {
	VDASSERT(mDisplayFrameLocks > 0);

	if (mDisplayFrameLocks) {
		--mDisplayFrameLocks;

		if (mbDisplayFrameDeferred) {
			mbDisplayFrameDeferred = false;

			DisplayFrame();
		}
	}
}

void VDProject::DisplayFrame(bool bDispInput, bool bDispOutput, bool forceInput, bool forceOutput) {
	VDPosition pos = mposCurrentFrame;
	VDPosition timeline_pos = pos;

	if (!mpCB)
		return;

	if (!inputVideo)
		return;

	if (g_dubber)
		return;

	if (mDisplayFrameLocks) {
		mbDisplayFrameDeferred = true;
		return;
	}

	const bool showInputFrame = bDispInput && (g_dubOpts.video.fShowInputFrame || forceInput);
	const bool showOutputFrame = bDispOutput && (g_dubOpts.video.fShowOutputFrame || forceOutput);
	if (!showInputFrame && !showOutputFrame)
		return;

	try {
		sint64 outpos = mTimeline.TimelineToSourceFrame(pos);

		if (!g_filterChain.IsEmpty() && outpos >= 0) {
			if (!filters.isRunning()) {
				StartFilters();
			}

			pos = filters.GetSourceFrame(outpos);
		} else {
			pos = outpos;
		}

		IVDStreamSource *pVSS = inputVideo->asStream();
		if (pos < 0)
			pos = pVSS->getEnd();

		bool updateRequired = false;

		if (showInputFrame && mLastDisplayedInputFrame != pos) {
			mLastDisplayedInputFrame = pos;
			updateRequired = true;
		}

		if (showOutputFrame && mLastDisplayedTimelineFrame != timeline_pos) {
			mLastDisplayedTimelineFrame = timeline_pos;
			updateRequired = true;
		}

		if (updateRequired) {
			if (pos >= pVSS->getEnd()) {
				mDesiredOutputFrame = -1;
				mDesiredNextInputFrame = -1;
				mDesiredNextOutputFrame = -1;

				mpPendingInputFrame = NULL;
				mpCurrentInputFrame = NULL;
				mpPendingOutputFrame = NULL;
				mpCurrentOutputFrame = NULL;
				mbPendingInputFrameValid = false;
				mbPendingOutputFrameValid = false;

				if (showInputFrame)
					mpCB->UIRefreshInputFrame(NULL);
				if (showOutputFrame)
					mpCB->UIRefreshOutputFrame(NULL);
			} else {
				mDesiredOutputFrame = outpos;
				mDesiredTimelineFrame = timeline_pos;
				mDesiredNextInputFrame = -1;
				mDesiredNextOutputFrame = -1;
				mDesiredNextTimelineFrame = -1;

				mbUpdateInputFrame	= bDispInput;
				mbUpdateLong		= false;
				mFramesDecoded		= 0;
				mLastDecodeUpdate	= VDGetCurrentTick();

				UpdateTimelineRate();

				if (!filters.isRunning())
					StartFilters();

				if (filters.isRunning()) {
					if (showInputFrame)
						mpVideoFrameSource->CreateRequest(pos, false, 0, ~mpPendingInputFrame);

					if (showOutputFrame) {
						if (mDesiredOutputFrame >= 0) {
							filters.RequestFrame(mDesiredOutputFrame, 0, ~mpPendingOutputFrame);
						} else {
							mpPendingOutputFrame = NULL;
						}

						mbPendingOutputFrameValid = true;
					}
				}

				mbUpdateInputFrame = true;

				UpdateFrame();
			}
		}

	} catch(const MyError& e) {
		const char *src = e.gets();
		char *dst = _strdup(src);

		if (!dst)
			guiSetStatus("%s", 255, e.gets());
		else {
			for(char *t = dst; *t; ++t)
				if (*t == '\n')
					*t = ' ';

			guiSetStatus("%s", 255, dst);
			free(dst);
		}
		SceneShuttleStop();
	}
}

bool VDProject::UpdateFrame(bool updateInputFrame) {
	if (!mpCB)
		return false;

	if (!inputVideo) {
		if (mbUpdateLong)
			guiSetStatus("", 255);

		mDesiredOutputFrame = -1;
		mDesiredTimelineFrame = -1;
		mDesiredNextInputFrame = -1;
		mDesiredNextOutputFrame = -1;
		mDesiredNextTimelineFrame = -1;
		return false;
	}

	uint32 startTime = VDGetCurrentTick();

	bool workCompleted;

	try {
		for(;;) {
			workCompleted = false;

			if (mpPendingInputFrame && mpPendingInputFrame->IsCompleted()) {
				mpCurrentInputFrame = mpPendingInputFrame;
				mpPendingInputFrame = NULL;

				if (mpCB && updateInputFrame) {
					if (mpCurrentInputFrame->IsSuccessful()) {
						VDFilterFrameBuffer *buf = mpCurrentInputFrame->GetResultBuffer();
						VDPixmap px(VDPixmapFromLayout(filters.GetInputLayout(), (void *)buf->LockRead()));
						px.info = buf->info;
						mpCB->UIRefreshInputFrame(&px);
						buf->Unlock();
					} else {
						mpCB->UIRefreshInputFrame(NULL);
					}
				}
			}

			if (mbPendingOutputFrameValid) {
				if (mpPendingOutputFrame)
					workCompleted = (filters.Run(NULL, false) != FilterSystem::kRunResult_Idle);

				if (!mpPendingOutputFrame || mpPendingOutputFrame->IsCompleted()) {
					mpCurrentOutputFrame = mpPendingOutputFrame;
					mpPendingOutputFrame = NULL;
					mbPendingOutputFrameValid = false;

					if (mbUpdateLong) {
						guiSetStatus("", 255);
						mbUpdateLong = false;
					}

					mDesiredOutputFrame = mDesiredNextOutputFrame;
					mDesiredTimelineFrame = mDesiredNextTimelineFrame;
					mDesiredNextInputFrame = -1;
					mDesiredNextOutputFrame = -1;
					mFramesDecoded = 0;

					if (mpCB) {
						if (!mpCurrentOutputFrame) {
							mpCB->UIRefreshOutputFrame(NULL);
						} else if (mpCurrentOutputFrame->IsSuccessful()) {
							VDFilterFrameBuffer *buf = mpCurrentOutputFrame->GetResultBuffer();
							VDPixmap px(VDPixmapFromLayout(filters.GetOutputLayout(), (void *)buf->LockRead()));
							px.info = buf->info;
							mpCB->UIRefreshOutputFrame(&px);
							buf->Unlock();
						} else {
							const VDFilterFrameRequestError *err = mpCurrentOutputFrame->GetError();

							throw MyError("%s", err ? err->mError.c_str() : "An unknown error occurred during filter processing.");
						}
					}

					workCompleted = true;
				}
			}

			if (mpVideoFrameSource) {
				switch(mpVideoFrameSource->RunRequests(NULL,0)) {
					case IVDFilterFrameSource::kRunResult_Running:
					case IVDFilterFrameSource::kRunResult_IdleWasActive:
					case IVDFilterFrameSource::kRunResult_BlockedWasActive:
						workCompleted = true;
						break;
				}
			}

			if (!workCompleted)
				return false;

			uint32 nCurrentTime = VDGetCurrentTick();

			if (nCurrentTime - mLastDecodeUpdate > 500) {
				mLastDecodeUpdate = nCurrentTime;
				mbUpdateLong = true;

				if (mpPendingOutputFrame)
					guiSetStatus("Decoding frame %lu...", 255, (unsigned long)mpPendingOutputFrame->GetFrameNumber());
			}

			if (nCurrentTime - startTime > 100)
				break;
		}
	} catch(const MyError& e) {
		guiSetStatus("%s", 255, e.gets());

		SceneShuttleStop();
		mDesiredOutputFrame = -1;
		mDesiredTimelineFrame = -1;
		mDesiredNextInputFrame = -1;
		mDesiredNextOutputFrame = -1;
		mDesiredNextTimelineFrame = -1;
	}

	return workCompleted;
}

bool VDProject::RefilterFrame(VDPosition timelinePos) {
	if (!inputVideo)
        return false;

	try {
		StartFilters();
		if (!filters.isRunning())
			return false;
	} catch(const MyError& e) {
		guiSetStatus("%s", 255, e.gets());
		return false;
	}

	VDPosition outputFrame = mTimeline.TimelineToSourceFrame(timelinePos);

	if (outputFrame >= 0) {
		filters.InvalidateCachedFrames(NULL);

		filters.RequestFrame(outputFrame, 0, ~mpPendingOutputFrame);
		mbPendingOutputFrameValid = true;

		while(UpdateFrame())
			;
	} else {
		mpPendingOutputFrame = NULL;
		mpCurrentOutputFrame = NULL;
		mbPendingOutputFrameValid = false;
	}

	if (mpCurrentOutputFrame == NULL) {
		if (mpCB)
			mpCB->UIRefreshOutputFrame(NULL);
		return false;
	} else {
		if (mpCB) {
			VDFilterFrameBuffer *buf = mpCurrentOutputFrame->GetResultBuffer();
			VDPixmap px(VDPixmapFromLayout(filters.GetOutputLayout(), (void *)buf->LockRead()));
			px.info = buf->info;
			mpCB->UIRefreshOutputFrame(&px);
			buf->Unlock();
		}
		return true;
	}
}

void VDProject::LockFilterChain(bool enableLock) {
	mbFilterChainLocked = enableLock;
}

///////////////////////////////////////////////////////////////////////////

void VDProject::SaveScript(VDFile& f, const VDStringW& dataSubdir, bool relative) {
  if (inputAVI)
	  JobWriteProjectScript(f, this, relative, dataSubdir, &g_dubOpts, g_szInputAVIFile, mInputDriverName.c_str(), &inputAVI->listFiles);
  else
	  JobWriteProjectScript(f, this, false, VDStringW(), &g_dubOpts, 0, 0, 0);
}

void VDProject::Quit() {
	VDUIFrame *pFrame = VDUIFrame::GetFrame((HWND)mhwnd);

	if (VDINLINEASSERT(pFrame))
		pFrame->Destroy();
}

void VDProject::CmdOpen(const wchar_t *token) {
	VDStringW filename(VDGetFullPath(token));
	size_t l = filename.length();
	if (l > 10) {
		if (!_wcsicmp(filename.c_str() + l - 10, L".vdproject")) {
			OpenProject(filename.c_str());
			return;
		}
	}

	mProjectFilename.clear();
	Open(token);
}

void VDProject::OpenProject(const wchar_t *pFilename, bool readOnly) {
	VDJobQueue jproject;
	jproject.LoadProject(pFilename);
	if (jproject.ListSize()==1) {
		VDJob* job = jproject.ListGet(0);
		VDStringA subDir(job->GetProjectSubdir());
		SaveProjectPath(VDStringW(pFilename), VDTextU8ToW(subDir), readOnly);
		mProjectName = job->GetName();
		RunScriptMemory(job->GetScript(), true);
	}
}

void VDProject::OpenJob(const wchar_t *pFilename, VDJob* job) {
	VDStringA subDir(job->GetProjectSubdir());
	SaveProjectPath(VDStringW(pFilename), VDTextU8ToW(subDir), true);
	RunScriptMemory(job->GetScript(), true);
}

void VDProject::SaveProjectPath(const VDStringW& path, const VDStringW& dataSubdir, bool readOnly) {
	mProjectReadonly = readOnly;
	mProjectFilename = path;
	mProjectSubdir = dataSubdir;
}

VDStringA CreateDataPrefix() {
	char buf[4];
	static char list[] = "0123456789abcdefghijkmnpqrstuvwxyz";
	for(int i=0; i<4; i++) {
		int x = rand() % 34;
		buf[i] = list[x];
	}

	return VDStringA(buf,4);
}

bool ComparePrefix(const VDStringW& name, const VDStringA& prefix) {
	int size = prefix.length();
	if (name.length()<size+2)
		return false;
	if (name[0]!='.')
		return false;
	if (name[size+1]!='.')
		return false;
	{for(int i=0; i<size; i++){
		if (name[i+1]!=prefix[i])
			return false;
	}}
	return true;
}

bool LooksLikePrefix(const VDStringW& name) {
	int size = 4;
	if (name.length()<size+2)
		return false;
	if (name[0]!='.')
		return false;
	if (name[size+1]!='.')
		return false;
	return true;
}

void EmptyDataDirectory(const VDStringW& dst) {
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW((dst+L"\\*.*").c_str(),&fd);
	if (h!=INVALID_HANDLE_VALUE) {
		do {
			VDStringW name(fd.cFileName);
			if(name==L".") continue;
			if(name==L"..") continue;
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				continue;
			} else {
				DeleteFileW((dst+L"\\"+name).c_str());
			}
			
		} while(FindNextFileW(h,&fd));
		FindClose(h);
	}
}

bool CleanupDataDir(const VDStringW& dst, const VDStringW& src, vdfastvector<FilterInstance*>& used_prefix) {
	bool empty = true;

	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW((src+L"\\*.*").c_str(),&fd);
	if (h!=INVALID_HANDLE_VALUE) {
		do {
			VDStringW name(fd.cFileName);
			if(name==L".") continue;
			if(name==L"..") continue;

			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				empty = false;
				continue;
			} else {
				bool used = false;
				{for(FilterInstance** p=used_prefix.begin(); p<used_prefix.end(); p++){
					if (ComparePrefix(name,(*p)->fmProject.dataPrefix)) {
						used = true;
						break;
					}
				}}
				if (!LooksLikePrefix(name))
					used = true;

				if (used && !dst.empty())
					CopyFileW((src+L"\\"+name).c_str(),(dst+L"\\"+name).c_str(),false);

				if (!used && dst.empty())
					DeleteFileW((src+L"\\"+name).c_str());

				if (used)
					empty = false;
			}
			
		} while(FindNextFileW(h,&fd));
		FindClose(h);
	}

	return empty;
}

void VDProject::DeleteData(const VDStringW& path, const VDStringW& dataSubdir) {
	if (!dataSubdir.empty()) {
		VDStringW src = VDFileSplitPathLeft(path);
		src += dataSubdir;
		EmptyDataDirectory(src);
		if (VDDoesPathExist(src.c_str()))
			VDRemoveDirectory(src.c_str());
	}
}

void VDProject::SaveData(const VDStringW& path, VDStringW& dataSubdir, bool make_unique) const {
	JobFlushFilterConfig();

	vdfastvector<FilterInstance*> used_prefix;
	bool data_empty = true;

	{for(VDFilterChainDesc::Entries::const_iterator it(g_filterChain.mEntries.begin()), itEnd(g_filterChain.mEntries.end()); it != itEnd; ++it) {
		FilterInstance *fa = (*it)->mpInstance;
		if (!fa->fmProject.dataPrefix.empty()) 
			used_prefix.push_back(fa);
		if (!fa->fmProject.data.empty())
			data_empty = false;
	}}

	bool move_project = !VDFileIsPathEqual(path.c_str(), mProjectFilename.c_str());
	bool alloc_dir = move_project;
	if (!move_project) {
		if (mProjectSubdir.empty() && !data_empty) alloc_dir = true;
	}

	if (!alloc_dir) {
		dataSubdir = mProjectSubdir;
		if (!dataSubdir.empty()) {
			VDStringW src = VDFileSplitPathLeft(mProjectFilename);
			src += dataSubdir;
			if (!CleanupDataDir(VDStringW(), src, used_prefix)) data_empty = false;
			if (data_empty) {
				if (VDDoesPathExist(src.c_str()))
					VDRemoveDirectory(src.c_str());
				dataSubdir.clear();
			}
		}
	} else {
		VDStringW base2 = VDFileSplitPathLeft(path);
		if (dataSubdir.empty())
			dataSubdir = VDFileSplitExtLeft(VDFileSplitPathRight(path));

		if (make_unique) {
			const uint32 signature = VDCreateAutoSaveSignature();
			for(int counter = 1; counter <= 100; ++counter) {
				VDStringW s;
				s.sprintf(L"_%x_%u", signature, counter);
				VDStringW test = base2+dataSubdir+s+L".vd";
				if (!VDDoesPathExist(test.c_str())){
					dataSubdir += s;
					break;
				}
			}
		}

		dataSubdir += L".vd";
		VDStringW dst = base2+dataSubdir;
		if ((!data_empty || !mProjectSubdir.empty()) && !VDDoesPathExist(dst.c_str()))
			VDCreateDirectory(dst.c_str());

		EmptyDataDirectory(dst);

		if (!mProjectSubdir.empty()) {
			VDStringW src = VDFileSplitPathLeft(mProjectFilename);
			src += mProjectSubdir;
			if (!CleanupDataDir(dst, src, used_prefix)) data_empty = false;
		}

		if (data_empty) {
			if (VDDoesPathExist(dst.c_str()))
				VDRemoveDirectory(dst.c_str());
			dataSubdir.clear();
		}
	}

	if (!data_empty) {
		VDStringW dst = VDFileSplitPathLeft(path);
		dst += dataSubdir;

		{for(VDFilterChainDesc::Entries::const_iterator it(g_filterChain.mEntries.begin()), itEnd(g_filterChain.mEntries.end()); it != itEnd; ++it) {
			FilterInstance *fa = (*it)->mpInstance;
			if (!fa->fmProject.data.empty()) {
				if (fa->fmProject.dataPrefix.empty()) {
					VDStringA s;
					while (1) {
						s = CreateDataPrefix();
						bool failed = false;
						{for(FilterInstance** p=used_prefix.begin(); p<used_prefix.end(); p++){
							if ((*p)->fmProject.dataPrefix==s) {
								failed = true;
								break;
							}
						}}
						if (!failed) break;
					}

					fa->fmProject.dataPrefix = s;
					used_prefix.push_back(fa);
				}

				VDStringW name = dst;
				name += L"\\.";
				name += VDTextU8ToW(fa->fmProject.dataPrefix);
				name += L".";

				{for(FilterModProject::Data** p=fa->fmProject.data.begin(); p<fa->fmProject.data.end(); p++){
					VDStringW filename = name + (*p)->id;
					size_t size = (*p)->data.size();
					if (size==0) {
						DeleteFileW(filename.c_str());
					} else {
						VDFile file;
						file.open(filename.c_str(),nsVDFile::kWrite|nsVDFile::kCreateAlways);
						file.write((*p)->data.begin(),size);
					}
				}}
			}
		}}
	}
}

VDStringW VDProject::BuildProjectPath(const wchar_t* path) const {
	if (!mProjectFilename.empty()) {
		VDStringW base = VDFileSplitPathLeft(mProjectFilename);

		if (!mProjectSubdir.empty()) {
			VDStringW data = base;
			data += mProjectSubdir;
			VDStringW rel_data = VDFileGetRelativePath(data.c_str(),path,false);
			if (!rel_data.empty())
				return VDStringW(L"$(DATA)\\") + rel_data;
		}

		VDStringW rel_base = VDFileGetRelativePath(base.c_str(),path,true);
		if (!rel_base.empty())
			return VDStringW(L"$(PROJECT)\\") + rel_base;
	}

	return VDStringW(path);
}

VDStringW VDProject::ExpandProjectPath(const wchar_t* path) const {
	if (!mProjectFilename.empty()) {
		VDStringW base = VDFileSplitPathLeft(mProjectFilename);

		if (wcsncmp(path,L"$(PROJECT)",10)==0)
			return VDFileResolvePath(base.c_str(),path+10);

		if (wcsncmp(path,L"$(DATA)",7)==0) {
			VDStringW data = base + mProjectSubdir;
			return VDFileResolvePath(data.c_str(),path+7);
		}
	}

	return VDGetFullPath(path);
}

void VDProject::Open(const wchar_t *pFilename, IVDInputDriver *pSelectedDriver, bool fExtendedOpen, bool fQuiet, bool fAutoscan, const char *pInputOpts, uint32 inputOptsLen) {
	Close();

	try {
		// attempt to determine input file type

		VDStringW filename(ExpandProjectPath(pFilename));

		if (!pSelectedDriver) {
			pSelectedDriver = VDAutoselectInputDriverForFile(filename.c_str(), IVDInputDriver::kF_Video);
			mInputDriverName.clear();
		} else {
			mInputDriverName = pSelectedDriver->GetSignatureName();
		}

		// open file

		inputAVI = pSelectedDriver->CreateInputFile((fQuiet?IVDInputDriver::kOF_Quiet:0) + (fAutoscan?IVDInputDriver::kOF_AutoSegmentScan:0));
		if (!inputAVI) throw MyMemoryError();

		// Extended open?
		if (!(pSelectedDriver->GetFlags() & IVDInputDriver::kF_SupportsOpts))
			fExtendedOpen = false;

		if (fExtendedOpen || (pSelectedDriver->GetFlags() & IVDInputDriver::kF_PromptForOpts)) {
			g_pInputOpts = inputAVI->promptForOptions(mhwnd);
			if (!g_pInputOpts)
				throw MyUserAbortError();
		} else if (pInputOpts)
			g_pInputOpts = inputAVI->createOptions(pInputOpts, inputOptsLen);

		if (g_pInputOpts)
			inputAVI->setOptions(g_pInputOpts);

		inputAVI->Init(filename.c_str());

		mInputAudioSources.clear();

		{
			vdrefptr<AudioSource> pTempAS;
			for(int i=0; inputAVI->GetAudioSource(i, ~pTempAS); ++i) {
				mInputAudioSources.push_back(pTempAS);
				pTempAS->setDecodeErrorMode(g_audioErrorMode);
			}
		}

		if (!inputAVI->GetVideoSource(0, ~inputVideo))
			throw MyError("File \"%ls\" does not have a video stream.", filename.c_str());


		VDRenderSetVideoSourceInputFormat(inputVideo, g_dubOpts.video.mInputFormat);

		IVDStreamSource *pVSS = inputVideo->asStream();
		pVSS->setDecodeErrorMode(g_videoErrorMode);

		// How many items did we get?

		{
			InputFilenameNode *pnode = inputAVI->listFiles.AtHead();
			InputFilenameNode *pnode_next;
			int nFiles = 0;

			while(pnode_next = pnode->NextFromHead()) {
				++nFiles;
				pnode = pnode_next;
			}

			if (nFiles > 1)
				guiSetStatus("Autoloaded %d segments (last was \"%ls\")", 255, nFiles, pnode->NextFromTail()->name);
		}

		// Retrieve info text

		inputAVI->GetTextInfo(mTextInfo);

		// Set current filename

		wcscpy(g_szInputAVIFile, filename.c_str());

		vdrefptr<IVDTimelineTimingSource> pTS;
		VDCreateTimelineTimingSourceVS(inputVideo, ~pTS);
		pTS = new VDProjectTimelineTimingSource(pTS, this);
		mTimeline.SetTimingSource(pTS);
		mTimeline.SetFromSource();
		mTimeline.ClearMarker();

		// invalidate currently displayed frames
		mLastDisplayedInputFrame = -1;
		mLastDisplayedTimelineFrame = -1;

		ClearSelection(false);
		mpCB->UITimelineUpdated();

		if (mAudioSourceMode >= kVDAudioSourceMode_Source)
			mAudioSourceMode = kVDAudioSourceMode_Source;
		SetAudioSource();
		UpdateDubParameters();
		mpCB->UISourceFileUpdated();
		mpCB->UIVideoSourceUpdated();
		mpCB->UIAudioSourceUpdated();
		FiltersEditorDisplayFrame(inputVideo);
		MoveToFrame(0);
	} catch(const MyError&) {
		Close();
		throw;
	}

	VDToolsHandleFileOpen(g_szInputAVIFile, pSelectedDriver);
}

void VDProject::Reopen() {
	if (!inputAVI)
		return;

	// attempt to determine input file type

	VDStringW filename(VDGetFullPath(g_szInputAVIFile));

	IVDInputDriver *pSelectedDriver;
	if (mInputDriverName.empty()) {
		pSelectedDriver = VDAutoselectInputDriverForFile(filename.c_str(), IVDInputDriver::kF_Video);
	} else {
		pSelectedDriver = VDGetInputDriverByName(mInputDriverName.c_str());
		if (!pSelectedDriver)
			throw MyError("The input driver \"%ls\" is no more available.", mInputDriverName);
	}

	// open file

	vdrefptr<InputFile> newInput(pSelectedDriver->CreateInputFile(0));
	if (!newInput)
		throw MyMemoryError();

	// Extended open?

	if (g_pInputOpts)
		newInput->setOptions(g_pInputOpts);

	// Open new source

	newInput->Init(filename.c_str());

	vdrefptr<IVDVideoSource> pVS;
	vdrefptr<AudioSource> pAS;
	newInput->GetVideoSource(0, ~pVS);

	VDRenderSetVideoSourceInputFormat(pVS, g_dubOpts.video.mInputFormat);

	IVDStreamSource *pVSS = pVS->asStream();
	pVSS->setDecodeErrorMode(g_videoErrorMode);

	// Check for an irrevocable change to the edit list. Irrevocable changes will occur if
	// there are any ranges other than the last that extend beyond the new length.

	const VDPosition oldFrameCount = inputVideo->asStream()->getLength();
	const VDPosition newFrameCount = pVS->asStream()->getLength();

	FrameSubset& fs = mTimeline.GetSubset();

	if (newFrameCount < oldFrameCount && VDPreferencesGetTimelineWarnReloadTruncation()) {
		FrameSubset::const_iterator it(fs.begin()), itEnd(fs.end());

		if (it != itEnd) {
			--itEnd;

			for(; it!=itEnd; ++it) {
				const FrameSubsetNode& fsn = *it;

				if (fsn.start + fsn.len > newFrameCount) {
					sint64 oldCount = oldFrameCount;
					sint64 newCount = newFrameCount;

					VDStringA msg(VDTextWToA(VDswprintf(VDLoadString(0, kVDST_Project, kVDM_ReopenChangesImminent), 2, &newCount, &oldCount)));

					if (IDCANCEL == MessageBox((HWND)mhwnd, msg.c_str(), g_szError, MB_OKCANCEL))
						return;

					break;
				}
			}
		}
	}

	// Swap the sources.

	inputAudio = NULL;
	inputAVI = newInput;
	inputVideo = pVS;

	mInputAudioSources.clear();

	{
		vdrefptr<AudioSource> pTempAS;
		for(int i=0; inputAVI->GetAudioSource(i, ~pTempAS); ++i) {
			mInputAudioSources.push_back(pTempAS);
			pTempAS->setDecodeErrorMode(g_audioErrorMode);
		}
	}

	wcscpy(g_szInputAVIFile, filename.c_str());

	// Update vars.

	vdrefptr<IVDTimelineTimingSource> pTS;
	VDCreateTimelineTimingSourceVS(inputVideo, ~pTS);
	pTS = new VDProjectTimelineTimingSource(pTS, this);
	mTimeline.SetTimingSource(pTS);

	ClearUndoStack();

	if (oldFrameCount > newFrameCount)
		fs.trimInputRange(newFrameCount);
	else if (oldFrameCount < newFrameCount)
		fs.addRange(oldFrameCount, newFrameCount - oldFrameCount, false, 0);

	mpCB->UITimelineUpdated();
	SetAudioSource();
	UpdateDubParameters();
	mpCB->UISourceFileUpdated();
	mpCB->UIAudioSourceUpdated();
	mpCB->UIVideoSourceUpdated();
	FiltersEditorDisplayFrame(inputVideo);

	// Invalidate currently displayed frames.
	mLastDisplayedInputFrame = -1;
	mLastDisplayedTimelineFrame = -1;

	if (newFrameCount < oldFrameCount) {
		if (!IsSelectionEmpty() && mposSelectionEnd > newFrameCount)
			SetSelectionEnd(newFrameCount, false);

		if (mposCurrentFrame > newFrameCount)
			MoveToFrame(newFrameCount);
	}

	// redisplay current frame
	DisplayFrame();

	mpCB->UICurrentPositionUpdated();

	guiSetStatus("Reloaded \"%ls\" (%I64d frames).", 255, filename.c_str(), newFrameCount);
}

void VDProject::OpenWAV(const wchar_t *szFile, IVDInputDriver *pSelectedDriver, bool automated, bool extOpts, const void *optdata, int optlen) {
	if (!pSelectedDriver) {
		pSelectedDriver = VDAutoselectInputDriverForFile(szFile, IVDInputDriver::kF_Audio);
		mAudioInputDriverName.clear();
	} else {
		mAudioInputDriverName = pSelectedDriver->GetSignatureName();
	}
	mpAudioInputOptions = NULL;

	vdrefptr<InputFile> ifile(pSelectedDriver->CreateInputFile(IVDInputDriver::kOF_Quiet));
	if (!ifile)
		throw MyMemoryError();

	if (pSelectedDriver) {
		if (pSelectedDriver->GetFlags() & IVDInputDriver::kF_PromptForOpts)
			extOpts = true;
	}

	if (!(pSelectedDriver->GetFlags() & IVDInputDriver::kF_SupportsOpts))
		extOpts = false;

	if (!automated && extOpts) {
		mpAudioInputOptions = ifile->promptForOptions((VDGUIHandle)mhwnd);
		if (!mpAudioInputOptions)
			throw MyUserAbortError();

		ifile->setOptions(mpAudioInputOptions);

		// force input driver name if we have options, since they have to match
		if (mAudioInputDriverName.empty())
			mAudioInputDriverName = pSelectedDriver->GetSignatureName();
	} else if (optdata) {
		mpAudioInputOptions = ifile->createOptions(optdata, optlen);
		if (mpAudioInputOptions)
			ifile->setOptions(mpAudioInputOptions);
	}

	ifile->Init(szFile);

	vdrefptr<AudioSource> pNewAudio;
	if (!ifile->GetAudioSource(0, ~pNewAudio))
		throw MyError("The file \"%ls\" does not contain an audio track.", szFile);

	pNewAudio->setDecodeErrorMode(g_audioErrorMode);

	vdwcslcpy(g_szInputWAVFile, szFile, sizeof(g_szInputWAVFile)/sizeof(g_szInputWAVFile[0]));

	mAudioSourceMode = kVDAudioSourceMode_External;
	inputAudio = mpInputAudioExt = pNewAudio;
	SetAudioSource();
	if (mpCB)
		mpCB->UIAudioSourceUpdated();
}

void VDProject::CloseWAV() {
	mpAudioInputOptions = NULL;

	if (mpInputAudioExt) {
		if (inputAudio == mpInputAudioExt) {
			inputAudio = NULL;
			mAudioSourceMode = kVDAudioSourceMode_None;
		}
		mpInputAudioExt = NULL;
	}
}

void VDProject::PreviewInput() {
	UpdateTimelineRate();

	VDPosition start = GetCurrentFrame();
	DubOptions dubOpt(g_dubOpts);

	LONG preload = inputAudio && inputAudio->getWaveFormat()->mTag != WAVE_FORMAT_PCM ? 1000 : 500;

	if (dubOpt.audio.preload > preload)
		dubOpt.audio.preload = preload;

	dubOpt.audio.enabled				= TRUE;
	dubOpt.audio.interval				= 1;
	dubOpt.audio.is_ms					= FALSE;
	dubOpt.video.mSelectionStart.mOffset = start;
	if (start>=dubOpt.video.mSelectionEnd.mOffset)
		dubOpt.video.mSelectionEnd.mOffset = mTimeline.GetLength();

	dubOpt.audio.fStartAudio			= TRUE;
	dubOpt.audio.new_rate				= 0;
	dubOpt.audio.newPrecision			= DubAudioOptions::P_NOCHANGE;
	dubOpt.audio.newChannels			= DubAudioOptions::C_NOCHANGE;
	dubOpt.audio.mVolume				= -1.0f;
	dubOpt.audio.bUseAudioFilterGraph	= false;

	dubOpt.video.mOutputFormat			= dubOpt.video.mInputFormat;

	dubOpt.video.mode					= DubVideoOptions::M_SLOWREPACK;
	dubOpt.video.fShowInputFrame		= TRUE;
	dubOpt.video.fShowOutputFrame		= FALSE;
	dubOpt.video.frameRateDecimation	= 1;
	dubOpt.video.mSelectionEnd.mOffset	= -1;
	dubOpt.video.mbUseSmartRendering	= false;

	dubOpt.audio.mode					= DubAudioOptions::M_FULL;

	dubOpt.fShowStatus = false;
	dubOpt.fMoveSlider = true;

	if (start < mTimeline.GetLength()) {
		mPreviewRestartMode = kPreviewRestart_Input;
		Preview(&dubOpt);
	}
}

void VDProject::PreviewOutput() {
	UpdateTimelineRate();

	VDPosition start = GetCurrentFrame();
	DubOptions dubOpt(g_dubOpts);

	const VDPixmapLayout& layout = (dubOpt.video.mode == DubVideoOptions::M_FULL) ? filters.GetOutputLayout() : filters.GetInputLayout();
	dubOpt.video.mOutputFormat = layout.format;

	long preload = inputAudio && inputAudio->getWaveFormat()->mTag != WAVE_FORMAT_PCM ? 1000 : 500;

	if (dubOpt.audio.preload > preload)
		dubOpt.audio.preload = preload;

	dubOpt.audio.enabled				= TRUE;
	dubOpt.audio.interval				= 1;
	dubOpt.audio.is_ms					= FALSE;

	if (dubOpt.video.mode != DubVideoOptions::M_FULL) dubOpt.video.mode = DubVideoOptions::M_SLOWREPACK;
	dubOpt.video.mSelectionStart.mOffset = start;
	if (start>=dubOpt.video.mSelectionEnd.mOffset)
		dubOpt.video.mSelectionEnd.mOffset = mTimeline.GetLength();
	dubOpt.video.mbUseSmartRendering	= false;

	dubOpt.fShowStatus = false;
	dubOpt.fMoveSlider = true;

	if (start < mTimeline.GetLength()) {
		mPreviewRestartMode = kPreviewRestart_Output;
		Preview(&dubOpt);
	}
}

void VDProject::PreviewAll() {
	mPreviewRestartMode = kPreviewRestart_All;
	Preview(NULL);
}

void VDProject::Preview(DubOptions *options) {
	if (!inputVideo)
		throw MyError("No input video stream to process.");

	DubOptions opts(options ? *options : g_dubOpts);
	opts.audio.enabled = true;

	if (!options) {
		opts.video.fShowDecompressedFrame = g_drawDecompressedFrame;
		opts.fShowStatus = !!g_showStatusWindow;
		opts.fMoveSlider = true;
	}

	VDAVIOutputPreviewSystem outpreview;
	RunOperation(&outpreview, false, &opts, g_prefs.main.iPreviewPriority, true, 0, 0);
}

void VDProject::PreviewRestart() {
	if (mPreviewRestartMode) {
		PreviewRestartMode restartMode = mPreviewRestartMode;
		mPreviewRestartMode = kPreviewRestart_None;

		switch(restartMode) {
			case kPreviewRestart_Input:
				PreviewInput();
				break;
			case kPreviewRestart_Output:
				PreviewOutput();
				break;
			case kPreviewRestart_All:
				PreviewAll();
				break;
		}
	}
}

void VDProject::RunNullVideoPass() {
	if (!inputVideo)
		throw MyError("No input file to process.");

	VDAVIOutputNullVideoSystem nullout;
	RunOperation(&nullout, FALSE, NULL, g_prefs.main.iDubPriority, true, 0, 0, VDPreferencesGetRenderBackgroundPriority());
}

void VDProject::QueueNullVideoPass() {
	if (!inputVideo)
		throw MyError("No input file to process.");

	JobAddConfigurationRunVideoAnalysisPass(this, &g_dubOpts, g_szInputAVIFile, mInputDriverName.c_str(), &inputAVI->listFiles, true);
}

void VDProject::CloseAVI() {
	// kill current seek
	mDesiredOutputFrame = -1;
	mDesiredTimelineFrame = -1;
	mDesiredNextInputFrame = -1;
	mDesiredNextOutputFrame = -1;
	mDesiredNextTimelineFrame = -1;

	StopFilters();		// needs to happen before we take down the video source

	mTimeline.SetTimingSource(NULL);

	if (g_pInputOpts) {
		delete g_pInputOpts;
		g_pInputOpts = NULL;
	}

	while(!mInputAudioSources.empty()) {
		if (inputAudio == mInputAudioSources.back())
			inputAudio = NULL;

		mInputAudioSources.pop_back();
	}

	inputVideo = NULL;
	inputAVI = NULL;

	mTextInfo.clear();

	ClearUndoStack();
}

void VDProject::Close() {
	CloseAVI();
	if (mpCB) {
		mpCB->UIVideoSourceUpdated();
		mpCB->UIAudioSourceUpdated();
		mpCB->UISourceFileUpdated();
	}
}

void VDProject::SaveAVI(const wchar_t *filename, bool compat, bool addAsJob, bool removeAudio) {
	if (!inputVideo)
		throw MyError("No input file to process.");

	if (addAsJob) {
		DubOptions opts = g_dubOpts;
		opts.removeAudio = removeAudio;
		JobAddConfiguration(this, &opts, g_szInputAVIFile, mInputDriverName.c_str(), filename, compat, &inputAVI->listFiles, 0, 0, true, 0);
	} else {
		::SaveAVI(filename, false, NULL, compat, removeAudio);
	}
}

void VDProject::SavePlugin(const wchar_t *filename, IVDOutputDriver* driver, const char* format, bool addAsJob, bool removeAudio) {
	if (!inputVideo)
		throw MyError("No input file to process.");

	if (addAsJob) {
		DubOptions opts = g_dubOpts;
		opts.removeAudio = removeAudio;
		JobAddConfiguration(this, &opts, g_szInputAVIFile, mInputDriverName.c_str(), filename, false, &inputAVI->listFiles, 0, 0, true, 0);
	} else {
		::SavePlugin(filename, driver, format, NULL, removeAudio);
	}
}

void VDProject::SaveFilmstrip(const wchar_t *pFilename, bool propagateErrors) {
	if (!inputVideo)
		throw MyError("No input file to process.");

	VDAVIOutputFilmstripSystem out(pFilename);
	RunOperation(&out, TRUE, NULL, 0, propagateErrors);
}

void VDProject::SaveAnimatedGIF(const wchar_t *pFilename, int loopCount, bool propagateErrors, DubOptions *optsOverride) {
	if (!inputVideo)
		throw MyError("No input file to process.");

	VDAVIOutputGIFSystem out(pFilename);
	out.SetLoopCount(loopCount);
	RunOperation(&out, TRUE, optsOverride, 0, propagateErrors);
}

void VDProject::SaveAnimatedPNG(const wchar_t *pFilename, int loopCount, int alpha, int grayscale, bool propagateErrors, DubOptions *optsOverride) {
	if (!inputVideo)
		throw MyError("No input file to process.");

	int mSaveVideoOutputFormat = g_dubOpts.video.mOutputFormat;

	if (alpha)
		g_dubOpts.video.mOutputFormat = nsVDPixmap::kPixFormat_XRGB8888;

	VDAVIOutputAPNGSystem out(pFilename);
	out.SetLoopCount(loopCount);
	out.SetAlpha(alpha);
	out.SetGrayscale(grayscale);
	RunOperation(&out, TRUE, optsOverride, 0, propagateErrors);

	if (alpha)
		g_dubOpts.video.mOutputFormat = mSaveVideoOutputFormat;
}

void VDProject::SaveRawAudio(const wchar_t *pFilename, bool propagateErrors, DubOptions *optsOverride) {
	if (!inputVideo)
		throw MyError("No input file to process.");

	if (!inputAudio)
		throw MyError("No audio stream to process.");

	VDAVIOutputRawSystem out(pFilename);
	RunOperation(&out, TRUE, optsOverride, 0, propagateErrors);
}

void VDProject::SaveRawVideo(const wchar_t *pFilename, const VDAVIOutputRawVideoFormat& format, bool propagateErrors, DubOptions *optsOverride) {
	if (!inputVideo)
		throw MyError("No input file to process.");

	VDAVIOutputRawVideoSystem out(pFilename, format);
	RunOperation(&out, TRUE, optsOverride, 0, propagateErrors);
}

void VDProject::ExportViaEncoder(const wchar_t *filename, const wchar_t *setName, bool propagateErrors, DubOptions *optsOverride) {
	if (!inputVideo)
		throw MyError("No input file to process.");

	VDAVIOutputCLISystem out(filename, setName);
	RunOperation(&out, TRUE, optsOverride, 0, propagateErrors);
}

void VDProject::StartServer(const char *serverName) {
	if (!inputVideo)
		throw MyError("No input file to process.");

	VDGUIHandle hwnd = mhwnd;

	if (serverName)
		vdstrlcpy(g_serverName, serverName, sizeof(g_serverName)/sizeof(g_serverName[0]));
	else
		g_serverName[0] = 0;

	VDUIFrame *pFrame = VDUIFrame::GetFrame((HWND)hwnd);

	pFrame->SetNextMode(3);
	pFrame->Detach();
}

void VDProject::ShowInputInfo() {
	if (inputAVI) {
		LockDisplayFrame();
		inputAVI->InfoDialog(mhwnd);
		UnlockDisplayFrame();
	}
}

void VDProject::SetVideoMode(int mode) {
	g_dubOpts.video.mode = (char)mode;
}

void VDProject::CopySourceFrameToClipboard() {
	if (!inputVideo || !mpCurrentInputFrame)
		return;

	VDFilterFrameBuffer *buf = mpCurrentInputFrame->GetResultBuffer();
	VDPixmap px = VDPixmapFromLayout(mpVideoFrameSource->GetOutputLayout(), (void *)buf->LockRead());
	px.info = buf->info;
	CopyFrameToClipboard(px);
	buf->Unlock();
}

void VDProject::CopyOutputFrameToClipboard() {
	if (!filters.isRunning() || !mpCurrentOutputFrame)
		return;

	VDFilterFrameBuffer *buf = mpCurrentOutputFrame->GetResultBuffer();
	VDPixmap px = VDPixmapFromLayout(filters.GetOutputLayout(), (void *)buf->LockRead());
	px.info = buf->info;
	CopyFrameToClipboard(px);
	buf->Unlock();
}

void VDProject::CopyFrameToClipboard(VDPixmap& px) {
	::CopyFrameToClipboard((HWND)mhwnd, px);
}

void VDProject::CopySourceFrameNumberToClipboard() {
	if (!filters.isRunning())
		StartFilters();

	sint64 pos = filters.GetSourceFrame(mTimeline.TimelineToSourceFrame(GetCurrentFrame()));

	VDStringW s;
	s.sprintf(L"%lld", pos);
	VDCopyTextToClipboard(s.c_str());
}

void VDProject::CopyOutputFrameNumberToClipboard() {
	VDStringW s;
	s.sprintf(L"%lld", GetCurrentFrame());
	VDCopyTextToClipboard(s.c_str());
}

int VDProject::GetAudioSourceCount() const {
	return (int)mInputAudioSources.size();
}

int VDProject::GetAudioSourceMode() const {
	return mAudioSourceMode;
}

void VDProject::SetAudioSourceNone() {
	mAudioSourceMode = kVDAudioSourceMode_None;
	CloseWAV();
	SetAudioSource();
	if (mpCB)
		mpCB->UIAudioSourceUpdated();
}

void VDProject::SetAudioSourceNormal(int index) {
	CloseWAV();
	mAudioSourceMode = kVDAudioSourceMode_Source + index;
	SetAudioSource();
	if (mpCB)
		mpCB->UIAudioSourceUpdated();
}

void VDProject::SetAudioMode(int mode) {
	g_dubOpts.audio.mode = (char)mode;
	SetAudioSource();
	if (mpCB)
		mpCB->UIAudioSourceUpdated();
}

void VDProject::SetAudioErrorMode(int errorMode0) {
	DubSource::ErrorMode errorMode = (DubSource::ErrorMode)errorMode0;

	AudioSources::iterator it(mInputAudioSources.begin()), itEnd(mInputAudioSources.end());
	for(; it!=itEnd; ++it) {
		AudioSource *as = *it;
	
		as->setDecodeErrorMode(errorMode);
	}

	if (mpInputAudioExt)
		mpInputAudioExt->setDecodeErrorMode(errorMode);
}

void VDProject::SetSelectionStart() {
	if (inputAVI)
		SetSelectionStart(GetCurrentFrame());
}

void VDProject::SetSelectionStart(VDPosition pos, bool notifyUser) {
	if (inputAVI) {
		UpdateTimelineRate();

		if (pos < 0)
			pos = 0;
		if (pos > GetFrameCount())
			pos = GetFrameCount();
		mposSelectionStart = pos;
		if (mposSelectionEnd < mposSelectionStart) {
			mposSelectionEnd = mposSelectionStart;
			g_dubOpts.video.mSelectionEnd.mOffset = pos;
		}

		g_dubOpts.video.mSelectionStart.mOffset = pos;

		if (mpCB)
			mpCB->UISelectionUpdated(notifyUser);
	}
}

void VDProject::SetSelectionEnd() {
	if (inputAVI)
		SetSelectionEnd(GetCurrentFrame());
}

void VDProject::SetSelectionEnd(VDPosition pos, bool notifyUser) {
	if (inputAVI) {
		UpdateTimelineRate();

		if (pos < 0)
			pos = 0;
		if (pos > GetFrameCount())
			pos = GetFrameCount();

		mposSelectionEnd = pos;
		if (mposSelectionStart > mposSelectionEnd) {
			mposSelectionStart = mposSelectionEnd;
			g_dubOpts.video.mSelectionStart.mOffset = pos;
		}

		g_dubOpts.video.mSelectionEnd.mOffset = pos;

		if (mpCB)
			mpCB->UISelectionUpdated(notifyUser);
	}
}

void VDProject::SetSelection(VDPosition start, VDPosition end, bool notifyUser) {
	if (end < start)
		ClearSelection(notifyUser);
	else {
		UpdateTimelineRate();

		const VDPosition count = GetFrameCount();
		if (start < 0)
			start = 0;
		if (start > count)
			start = count;
		if (end < 0)
			end = 0;
		if (end > count)
			end = count;

		mposSelectionStart = start;
		mposSelectionEnd = end;

		g_dubOpts.video.mSelectionStart.mOffset = start;
		g_dubOpts.video.mSelectionEnd.mOffset = end;

		if (mpCB)
			mpCB->UISelectionUpdated(notifyUser);
	}
}

void VDProject::SetMarker() {
	if (inputAVI)
		SetMarker(GetCurrentFrame());
}

void VDProject::SetMarker(VDPosition pos) {
	if (inputAVI) {
		mTimeline.ToggleMarker(pos);

		if (mpCB)
			mpCB->UITimelineUpdated();
	}
}

void VDProject::MoveToFrame(VDPosition frame) {
	if (inputVideo) {
		frame = std::max<VDPosition>(0, std::min<VDPosition>(frame, mTimeline.GetLength()));

		mposCurrentFrame = frame;
		mbPositionCallbackEnabled = false;

		if (!g_dubber && !mProjectLoading) {
			DisplayFrame();
			while(UpdateFrame());
		}

		if (mpCB)
			mpCB->UICurrentPositionUpdated();
	}
}

void VDProject::MoveToStart() {
	if (inputVideo)
		MoveToFrame(0);
}

void VDProject::MoveToPrevious() {
	if (inputVideo)
		MoveToFrame(GetCurrentFrame() - 1);
}

void VDProject::MoveToNext() {
	if (inputVideo)
		MoveToFrame(GetCurrentFrame() + 1);
}

void VDProject::MoveToEnd() {
	if (inputVideo)
		MoveToFrame(mTimeline.GetEnd());
}

void VDProject::MoveToSelectionStart() {
	if (inputVideo && IsSelectionPresent()) {
		VDPosition pos = GetSelectionStartFrame();

		if (pos >= 0)
			MoveToFrame(pos);
	}
}

void VDProject::MoveToSelectionEnd() {
	if (inputVideo && IsSelectionPresent()) {
		VDPosition pos = GetSelectionEndFrame();

		if (pos >= 0)
			MoveToFrame(pos);
	}
}

void VDProject::MoveToNearestKey(VDPosition pos) {
	if (!inputVideo)
		return;


	MoveToFrame(mTimeline.GetNearestKey(pos));
}

void VDProject::MoveToNearestKeyNext(VDPosition pos) {
	if (!inputVideo)
		return;


	MoveToFrame(mTimeline.GetNearestKeyNext(pos));
}

void VDProject::MoveToPreviousKey() {
	if (!inputVideo)
		return;

	VDPosition pos = mTimeline.GetPrevKey(GetCurrentFrame());

	if (pos < 0)
		pos = 0;

	MoveToFrame(pos);
}

void VDProject::MoveToNextKey() {
	if (!inputVideo)
		return;

	VDPosition pos = mTimeline.GetNextKey(GetCurrentFrame());

	if (pos < 0)
		pos = mTimeline.GetEnd();

	MoveToFrame(pos);
}

void VDProject::MoveBackSome() {
	if (inputVideo)
		MoveToFrame(GetCurrentFrame() - 50);
}

void VDProject::MoveForwardSome() {
	if (inputVideo)
		MoveToFrame(GetCurrentFrame() + 50);
}

void VDProject::StartSceneShuttleReverse() {
	if (!inputVideo)
		return;
	mSceneShuttleMode = -1;
	if (mpCB)
		mpCB->UIShuttleModeUpdated();
}

void VDProject::StartSceneShuttleForward() {
	if (!inputVideo)
		return;
	mSceneShuttleMode = +1;
	if (mpCB)
		mpCB->UIShuttleModeUpdated();
}

void VDProject::MoveToPreviousRange() {
	if (inputAVI) {
		VDPosition pos = mTimeline.GetPrevEdit(GetCurrentFrame());
		VDPosition mpos = mTimeline.GetPrevMarker(GetCurrentFrame());

		if (mpos >=0 && (mpos>pos || pos==-1)) {
			MoveToFrame(mpos);
			guiSetStatus("Marker", 255);
			return;
		}

		if (pos >= 0) {
			MoveToFrame(pos);

			sint64 len;
			bool masked;
			int source;
			sint64 start = mTimeline.GetSubset().lookupRange(pos, len, masked, source);
			guiSetStatus("Previous output frame %I64d-%I64d: included source range %I64d-%I64d%s", 255, pos, pos+len-1, start, start+len-1, masked ? " (masked)" : "");
			return;
		}
	}
	MoveToFrame(0);
	guiSetStatus("No previous edit.", 255);
}

void VDProject::MoveToNextRange() {
	if (inputAVI) {
		VDPosition pos = mTimeline.GetNextEdit(GetCurrentFrame());
		VDPosition mpos = mTimeline.GetNextMarker(GetCurrentFrame());

		if (mpos >=0 && (mpos<pos || pos==-1)) {
			MoveToFrame(mpos);
			guiSetStatus("Marker", 255);
			return;
		}

		if (pos >= 0) {
			MoveToFrame(pos);

			sint64 len;
			bool masked;
			int source;
			sint64 start = mTimeline.GetSubset().lookupRange(pos, len, masked, source);
			guiSetStatus("Next output frame %I64d-%I64d: included source range %I64d-%I64d%s", 255, pos, pos+len-1, start, start+len-1, masked ? " (masked)" : "");
			return;
		}
	}
	MoveToFrame(GetFrameCount());
	guiSetStatus("No next edit.", 255);
}

void VDProject::MoveToPreviousDrop() {
	if (inputAVI) {
		VDPosition pos = mTimeline.GetPrevDrop(GetCurrentFrame());

		if (pos >= 0)
			MoveToFrame(pos);
		else
			guiSetStatus("No previous dropped frame found.", 255);
	}
}

void VDProject::MoveToNextDrop() {
	if (inputAVI) {
		VDPosition pos = mTimeline.GetNextDrop(GetCurrentFrame());

		if (pos >= 0)
			MoveToFrame(pos);
		else
			guiSetStatus("No next dropped frame found.", 255);
	}
}

void VDProject::ResetTimeline() {
	if (inputAVI) {
		BeginTimelineUpdate(VDLoadString(0, kVDST_Project, kVDM_ResetTimeline));

		mTimeline.SetFromSource();

		EndTimelineUpdate();
	}
}

void VDProject::ResetTimelineWithConfirmation() {
	if (inputAVI) {
		if (IDOK == MessageBox((HWND)mhwnd, "Discard edits and reset timeline?", g_szWarning, MB_OKCANCEL|MB_TASKMODAL|MB_SETFOREGROUND|MB_ICONEXCLAMATION)) {
			ResetTimeline();
		}
	}
}

void VDProject::ScanForErrors() {
	if (inputVideo) {
		BeginTimelineUpdate(VDLoadString(0, kVDST_Project, kVDM_ScanForErrors));
		ScanForUnreadableFrames(&mTimeline.GetSubset(), inputVideo);
		EndTimelineUpdate();
	}
}

void VDProject::RunOperation(IVDDubberOutputSystem *pOutputSystem, BOOL fAudioOnly, DubOptions *pOptions, int iPriority, bool fPropagateErrors, long lSpillThreshold, long lSpillFrameThreshold, bool backgroundPriority) {

	if (!inputAVI)
		throw MyError("No source has been loaded to process.");

	VDProjectAutoSave autoSave(this);

	if (!g_fJobMode && VDPreferencesGetAutoRecoverEnabled() && !pOutputSystem->IsRealTime()) {
		autoSave.Save();
	}

	bool fError = false;
	bool bUserAbort = false;
	MyError prop_err;
	DubOptions *opts;

	vdautoptr<VDAVIOutputSegmentedSystem> segmentedOutput;

	{
		const wchar_t *pOpType = pOutputSystem->IsRealTime() ? L"preview" : L"dub";
		VDLog(kVDLogMarker, VDswprintf(L"Beginning %ls operation.", 1, &pOpType));
	}

	DubOptions tempOpts(pOptions ? *pOptions : g_dubOpts);

	mbPositionCallbackEnabled = true;

	try {
		VDAutoLogDisplay disp;

		UpdateDubParameters(true);
		StopFilters();

		// Create a dubber.

		opts = &tempOpts;
		if (!pOptions) {
			opts->video.fShowDecompressedFrame = g_drawDecompressedFrame;
			opts->fShowStatus = !!g_showStatusWindow;
		}
		opts->perf.fDropFrames = g_fDropFrames;
		opts->mThrottlePercent = pOutputSystem->IsRealTime() ? 100 : VDPreferencesGetRenderThrottlePercent();
		opts->video.mMaxVideoCompressionThreads = VDPreferencesGetVideoCompressionThreadCount();

		if (pOutputSystem->IsVideoImageOutputRequired()) {
			if (opts->video.mode == DubVideoOptions::M_NONE)
				opts->video.mode = DubVideoOptions::M_FASTREPACK;
		}

		if (!(g_dubber = CreateDubber(opts)))
			throw MyMemoryError();

		// Create dub status window

		delete mpDubStatus;
		mpDubStatus = CreateDubStatusHandler();

		if (opts->fMoveSlider) {
			if (g_fJobMode) {
				mpDubStatus->SetPositionCallback(JobPositionCallback, this);
			} else {
				if (pOutputSystem->IsRealTime())
					mpDubStatus->SetPositionCallback(StaticFastPositionCallback, this);
				else
					mpDubStatus->SetPositionCallback(StaticPositionCallback, this);
			}
		}

		// Initialize the dubber.

		if (opts->audio.bUseAudioFilterGraph)
			g_dubber->SetAudioFilterGraph(g_audioFilterGraph);

		g_dubber->SetStatusHandler(mpDubStatus);

		if (!pOutputSystem->IsRealTime() && g_ACompressionFormat)
			g_dubber->SetAudioCompression((const VDWaveFormat *)g_ACompressionFormat, g_ACompressionFormatSize, g_ACompressionFormatHint.c_str(), g_ACompressionConfig);

		// As soon as we call Init(), this value is no longer ours to free.

		if (mpCB)
			mpCB->UISetDubbingMode(true, pOutputSystem->IsRealTime());

		IVDVideoSource *vsrc = inputVideo;
		AudioSource *asrc = inputAudio;
		if (fAudioOnly == 3) asrc = 0;
		COMPVARS2 *comp = &g_Vcompression;
		if(filters.isTrimmedChain() && pOutputSystem->IsNull()) comp = 0;

		if (lSpillThreshold) {
			segmentedOutput = new VDAVIOutputSegmentedSystem(pOutputSystem, opts->audio.is_ms, opts->audio.is_ms ? opts->audio.interval * 0.001 : opts->audio.interval, (double)opts->audio.preload / 500.0, (sint64)lSpillThreshold << 20, lSpillFrameThreshold);
			g_dubber->Init(&vsrc, 1, &asrc, asrc ? 1 : 0, segmentedOutput, comp, &mTimeline.GetSubset(), mVideoTimelineFrameRate);
		} else {
			if (fAudioOnly == 2)
				g_dubber->SetPhantomVideoMode();

			g_dubber->Init(&vsrc, 1, &asrc, asrc ? 1 : 0, pOutputSystem, comp, &mTimeline.GetSubset(), mVideoTimelineFrameRate);
		}

		if (!pOptions && mhwnd)
			RedrawWindow((HWND)mhwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);

		g_dubber->Stopped() += mStoppedDelegate(this, &VDProject::OnDubAbort);
		g_dubber->Go(iPriority);
		g_dubber->SetBackground(backgroundPriority);

		if (mpCB)
			bUserAbort = !mpCB->UIRunDubMessageLoop();
		else {
			MSG msg;
			while(g_dubber->isRunning()) {
				BOOL result = GetMessage(&msg, NULL, 0, 0);

				if (result == (BOOL)-1)
					break;

				if (!result) {
					PostQuitMessage(msg.wParam);
					break;
				}

				TranslateMessage(&msg); 
				DispatchMessage(&msg); 
			}
		}

		g_dubber->Stop();

		if (g_dubber->isAbortedByUser()) {
			bUserAbort = true;
			mPreviewRestartMode = kPreviewRestart_None;
		} else {
			if (!g_dubber->IsAborted())
				mPreviewRestartMode = kPreviewRestart_None;

			if (!fPropagateErrors)
				disp.Post(mhwnd);
		}

	} catch(char *s) {
		mPreviewRestartMode = kPreviewRestart_None;
		if (fPropagateErrors) {
			prop_err.setf(s);
			fError = true;
		} else
			MyError(s).post((HWND)mhwnd, g_szError);
	} catch(MyError& err) {
		mPreviewRestartMode = kPreviewRestart_None;
		if (fPropagateErrors) {
			prop_err.TransferFrom(err);
			fError = true;
		} else
			err.post((HWND)mhwnd,g_szError);
	}

	if (g_dubber)
		g_dubber->SetStatusHandler(NULL);

	if (mpDubStatus) {
		if (!fError || !mpDubStatus->IsNormalWindow()){
			delete mpDubStatus;
			mpDubStatus = NULL;
		} else {
			mpDubStatus->DeferDestroy((void**)&mpDubStatus);
		}
	}

	_CrtCheckMemory();

	delete g_dubber;
	g_dubber = NULL;

	VDRenderSetVideoSourceInputFormat(inputVideo, g_dubOpts.video.mInputFormat);

	if (mpCB)
		mpCB->UISetDubbingMode(false, false);

	VDLog(kVDLogMarker, VDStringW(L"Ending operation."));

	if (g_bExit)
		PostQuitMessage(0);
	else if (fPropagateErrors) {
		if (fError)
			throw prop_err;
		else if (bUserAbort)
			throw MyUserAbortError();
	}
}

void VDProject::AbortOperation() {
	if (g_dubber)
		g_dubber->Abort();
}

void VDProject::StopFilters() {
	mpCurrentInputFrame = NULL;
	mpCurrentOutputFrame = NULL;
	mpPendingInputFrame = NULL;
	mpPendingOutputFrame = NULL;

	filters.DeinitFilters();
	filters.DeallocateBuffers();

	mpVideoFrameSource = NULL;
}

void VDProject::PrepareFilters() {
	if (filters.isRunning() || !inputVideo)
		return;

	IVDStreamSource *pVSS = inputVideo->asStream();

	DubVideoStreamInfo vInfo;
	InitVideoStreamValuesStatic(vInfo, inputVideo, inputAudio, &g_dubOpts, &mTimeline.GetSubset(), NULL, NULL);

	VDFraction framerate(vInfo.mFrameRatePreFilter);
	const VDPixmap& px = inputVideo->getTargetFormat();
	const VDFraction& srcPAR = inputVideo->getPixelAspectRatio();

	filters.prepareLinearChain(&g_filterChain, px.w, px.h, px, framerate, pVSS->getLength(), srcPAR);
}

void VDProject::StartFilters() {
	if (filters.isRunning() || !inputVideo || mbFilterChainLocked)
		return;
	IVDStreamSource *pVSS = inputVideo->asStream();

	DubVideoStreamInfo vInfo;
	InitVideoStreamValuesStatic(vInfo, inputVideo, inputAudio, &g_dubOpts, &mTimeline.GetSubset(), NULL, NULL);

	VDFraction framerate(vInfo.mFrameRatePreFilter);
	const VDPixmap& px = inputVideo->getTargetFormat();

	if (px.format) {
		const VDFraction& srcPAR = inputVideo->getPixelAspectRatio();
		filters.prepareLinearChain(&g_filterChain, px.w, px.h, px, framerate, pVSS->getLength(), srcPAR);

		mpVideoFrameSource = new VDFilterFrameVideoSource;
		mpVideoFrameSource->Init(inputVideo, filters.GetInputLayout());

		filters.SetVisualAccelDebugEnabled(false);
		filters.SetAccelEnabled(VDPreferencesGetFilterAccelEnabled());
		filters.SetAsyncThreadCount(-1);

		// We explicitly use the stream length here as we're interested in the *uncut* filtered length.
		vdrefptr<IVDFilterSystemScheduler> fss(new VDFilterSystemMessageLoopScheduler);

		filters.initLinearChain(fss, VDXFilterStateInfo::kStatePreview, &g_filterChain, mpVideoFrameSource, px.w, px.h, px, px.palette, framerate, pVSS->getLength(), srcPAR);

		filters.ReadyFilters();
	}
}

void VDProject::UpdateFilterList() {
	mLastDisplayedTimelineFrame = -1;
	DisplayFrame();
	if (mpCB)
		mpCB->UIVideoFiltersUpdated();
}

///////////////////////////////////////////////////////////////////////////

void VDProject::BeginFilterUpdates() {
	StartFilters();

	if (filters.isRunning()) {
		mVideoMarkedPostFiltersFrameRate = filters.GetOutputFrameRate();
		mVideoMarkedPostFiltersFrameCount = filters.GetOutputFrameCount();
	} else {
		mVideoMarkedPostFiltersFrameRate = VDFraction(0, 0);
		mVideoMarkedPostFiltersFrameCount = 0;
	}
}

void VDProject::EndFilterUpdates() {
	StartFilters();

	VDFraction oldRate = mVideoMarkedPostFiltersFrameRate;
	sint64 oldCount = mVideoMarkedPostFiltersFrameCount;
	VDFraction newRate = filters.GetOutputFrameRate();
	sint64 newCount = filters.GetOutputFrameCount();

	if (oldRate != newRate ||
		oldCount != newCount)
	{
		mVideoMarkedPostFiltersFrameRate = newRate;
		mVideoMarkedPostFiltersFrameCount = newCount;

		if (oldRate.getLo() > 0)
			AdjustTimelineForFilterChanges(oldRate, oldCount, newRate, newCount);
	}
}

void VDProject::SceneShuttleStop() {
	if (mSceneShuttleMode) {
		mSceneShuttleMode = 0;
		mSceneShuttleAdvance = 0;
		mSceneShuttleCounter = 0;

		if (mpCB)
			mpCB->UIShuttleModeUpdated();

		if (inputVideo) {
			MoveToFrame(GetCurrentFrame());

			// We have to force the input frame to refresh since we may have omitted one of the updates.
			if (mpCurrentInputFrame && mpCurrentInputFrame->IsSuccessful()) {
				VDFilterFrameBuffer *buf = mpCurrentInputFrame->GetResultBuffer();
				VDPixmap px(VDPixmapFromLayout(filters.GetInputLayout(), (void *)buf->LockRead()));
				px.info = buf->info;
				mpCB->UIRefreshInputFrame(&px);
				buf->Unlock();
			} else {
				mpCB->UIRefreshInputFrame(NULL);
			}
		}
	}
}

void VDProject::AdjustTimelineForFilterChanges(const VDFraction& oldRate, sint64 oldFrameCount, const VDFraction& newRate, sint64 newFrameCount) {
	// rescale everything
	mTimeline.Rescale(
			oldRate,
			oldFrameCount,
			newRate,
			newFrameCount);

	mpCB->UITimelineUpdated();

	double rateConversion = newRate.asDouble() / oldRate.asDouble();

	if (IsSelectionPresent()) {
		VDPosition selStart = GetSelectionStartFrame();
		VDPosition selEnd = GetSelectionEndFrame();

		selStart = VDCeilToInt64(selStart * rateConversion - 0.5);
		selEnd = VDCeilToInt64(selEnd * rateConversion - 0.5);

		SetSelection(selStart, selEnd);
	}

	MoveToFrame(VDCeilToInt64(GetCurrentFrame() * rateConversion - 0.5));
}

void VDProject::SceneShuttleStep() {
	bool input_mode = IsInputPaneUsed();

	VDPosition sample = GetCurrentFrame() + mSceneShuttleMode;

	if (input_mode) {
		VDPosition ls2 = mTimeline.TimelineToSourceFrame(sample);
		IVDStreamSource *pVSS = inputVideo->asStream();
		if (!inputVideo || ls2 < pVSS->getStart() || ls2 >= pVSS->getEnd()) {
			SceneShuttleStop();
			return;
		}
	} else {
		if (sample<0 || sample>=mTimeline.GetLength()) {
			SceneShuttleStop();
			return;
		}
	}

	if (mSceneShuttleAdvance < 1280)
		++mSceneShuttleAdvance;

	mSceneShuttleCounter += 32;

	mposCurrentFrame = sample;

	if (input_mode)
		DisplayFrame(true, false, true, false);
	else
		DisplayFrame(false, true, false, true);

	bool updateInputFrame = false;
	if (mSceneShuttleCounter >= mSceneShuttleAdvance) {
		mSceneShuttleCounter = 0;
		updateInputFrame = true;
	}

	while(UpdateFrame(updateInputFrame));

	if (mpCB)
		mpCB->UICurrentPositionUpdated();

	if (!mpSceneDetector->Enabled())
		return;

	bool sceneBreak = false;
	if (input_mode) {
		VDFilterFrameBuffer *buf = mpCurrentInputFrame->GetResultBuffer();
		if (!buf) {
			SceneShuttleStop();
			return;
		}

		VDPixmap px = VDPixmapFromLayout(mpVideoFrameSource->GetOutputLayout(), (void *)buf->LockRead());
		px.info = buf->info;
		sceneBreak = mpSceneDetector->Submit(px);
		buf->Unlock();
	} else {
		if (!filters.isRunning()) {
			SceneShuttleStop();
			return;
		}

		VDFilterFrameBuffer *buf = mpCurrentOutputFrame->GetResultBuffer();
		if (!buf) {
			SceneShuttleStop();
			return;
		}

		VDPixmap px = VDPixmapFromLayout(filters.GetOutputLayout(), (void *)buf->LockRead());
		px.info = buf->info;
		sceneBreak = mpSceneDetector->Submit(px);
		buf->Unlock();
	}

	if (sceneBreak)
		SceneShuttleStop();
}

void VDProject::StaticPositionCallback(VDPosition start, VDPosition cur, VDPosition end, int progress, bool fast_update, void *cookie) {
	VDProject *pthis = (VDProject *)cookie;

	if (pthis->mbPositionCallbackEnabled && !fast_update) {
		VDPosition frame = std::max<VDPosition>(0, std::min<VDPosition>(cur, pthis->GetFrameCount()));

		pthis->mposCurrentFrame = frame;
		if (pthis->mpCB)
			pthis->mpCB->UICurrentPositionUpdated(false);
	}
}

void VDProject::StaticFastPositionCallback(VDPosition start, VDPosition cur, VDPosition end, int progress, bool fast_update, void *cookie) {
	VDProject *pthis = (VDProject *)cookie;

	if (pthis->mbPositionCallbackEnabled) {
		VDPosition frame = std::max<VDPosition>(0, std::min<VDPosition>(cur, pthis->GetFrameCount()));

		pthis->mposCurrentFrame = frame;
		if (pthis->mpCB)
			pthis->mpCB->UICurrentPositionUpdated(fast_update);
	}
}

void VDProject::UpdateTimelineRate() {
	if (mbTimelineRateDirty)
		UpdateDubParameters(false);
}

void VDProject::UpdateDubParameters(bool forceUpdate) {
	if (!inputVideo) {
		if (forceUpdate)
			throw MyError("Cannot initialize rendering parameters: there is no video stream.");

		return;
	}

	// work around fraction ==(0,0) bug for now
	mVideoInputFrameRate	= VDFraction(1,1);
	mVideoOutputFrameRate	= VDFraction(1,1);
	mVideoTimelineFrameRate	= VDFraction(1,1);

	DubVideoStreamInfo vInfo;

	if (inputVideo) {
		VDRenderSetVideoSourceInputFormat(inputVideo, g_dubOpts.video.mInputFormat);

		try {
			InitVideoStreamValuesStatic(vInfo, inputVideo, inputAudio, &g_dubOpts, &GetTimeline().GetSubset(), NULL, NULL);

			if (mVideoOutputFrameRate != vInfo.mFrameRatePreFilter)
				StopFilters();

			mVideoInputFrameRate	= vInfo.mFrameRateIn;
			mVideoOutputFrameRate	= vInfo.mFrameRatePreFilter;
			mVideoTimelineFrameRate = vInfo.mFrameRatePreFilter;

			StartFilters();

			InitVideoStreamValuesStatic2(vInfo, &g_dubOpts, filters.isRunning() ? &filters : NULL, VDFraction(0, 0));

			mVideoTimelineFrameRate	= vInfo.mFrameRatePostFilter;
			mbTimelineRateDirty = false;
		} catch(const MyError& e) {
			// The input stream may throw an error here trying to obtain the nearest key.
			// If so, bail.
			if (forceUpdate)
				throw MyError("Cannot initialize rendering parameters: %s", e.c_str());
		}
	}

	if (mpCB)
		mpCB->UIDubParametersUpdated();
}

void VDProject::SetAudioSource() {
	switch(mAudioSourceMode) {
		case kVDAudioSourceMode_None:
			inputAudio = NULL;
			break;

		case kVDAudioSourceMode_External:
			inputAudio = mpInputAudioExt;
			break;

		default:
			if (mAudioSourceMode >= kVDAudioSourceMode_Source) {
				int index = mAudioSourceMode - kVDAudioSourceMode_Source;

				if ((unsigned)index < mInputAudioSources.size()) {
					inputAudio = mInputAudioSources[index];
					break;
				}
			}
			inputAudio = NULL;
			break;
	}

	if (inputAudio) {
		const VDWaveFormat *fmt = inputAudio->getWaveFormat();
		bool convert = false;
		if (g_dubOpts.audio.newChannels!=DubAudioOptions::C_NOCHANGE) convert = true;
		if (g_dubOpts.audio.newPrecision!=DubAudioOptions::P_NOCHANGE) convert = true;
		if (g_dubOpts.audio.mode==DubAudioOptions::M_NONE) convert = false;
		if ((is_audio_pcm(fmt) || is_audio_float(fmt)) && convert) {
			VDWaveFormat target = *fmt;
			target.mTag = VDWaveFormat::kTagPCM;
			switch (g_dubOpts.audio.newChannels) {
			case DubAudioOptions::C_MONO:
				target.mChannels = 1;
				break;
			case DubAudioOptions::C_NOCHANGE:
				target.mChannels = 0;
				break;
			default:
				target.mChannels = 2;
			}
			switch (g_dubOpts.audio.newPrecision) {
			case DubAudioOptions::P_8BIT:
				target.mSampleBits = 8;
				break;
			case DubAudioOptions::P_16BIT:
				target.mSampleBits = 16;
				break;
			default:
				target.mSampleBits = 0;
			}
			inputAudio->SetTargetFormat(&target);
		} else {
			inputAudio->SetTargetFormat(0);
		}
	}
}

void VDProject::OnDubAbort(IDubber *, const bool&) {
	if (mpCB)
		mpCB->UIAbortDubMessageLoop();
}

int64 FilterModTimeline::GetTimelinePos() {
	return project->GetCurrentFrame();
}

int64 FilterModTimeline::TimelineToFilterSource(int64 frame) {
	return project->GetTimeline().TimelineToSourceFrame(frame);
}

int64 FilterModTimeline::FilterSourceToTimeline(int64 frame) {
	bool masked;
	return project->GetTimeline().GetSubset().revLookupFrame(frame,masked);
}

FilterDefinitionInstance *VDUIShowDialogAddFilter(VDGUIHandle hParent);

bool FilterModSystem::CreateVideoFilter(VDXHWND hParent, FilterReturnInfo& a)
{
	FilterDefinitionInstance *fdi = VDUIShowDialogAddFilter((VDGUIHandle)hParent);
	if(!fdi) return false;
	a.setName(fdi->GetName().c_str());
	a.setMaker(fdi->GetAuthor().c_str());
	a.setDesc(fdi->GetDescription().c_str());
	VDExternalModule* xm = fdi->GetModule();
	if(xm)
		a.setModulePath(xm->GetFilename().c_str());
	else
		a.setBuiltinDef(&fdi->GetDef());
	return true;
}

bool FilterModSystem::FindVideoFilter(const char* name, FilterReturnInfo& a)
{
	std::list<FilterBlurb>	filterList;

	FilterEnumerateFilters(filterList);

	for(std::list<FilterBlurb>::const_iterator it(filterList.begin()), itEnd(filterList.end()); it!=itEnd; ++it) {
		const FilterBlurb& fb = *it;

		if (strfuzzycompare(fb.name.c_str(), name)) {
			a.setName(fb.name.c_str());
			a.setMaker(fb.author.c_str());
			a.setDesc(fb.description.c_str());
			VDExternalModule* xm = fb.key->GetModule();
			if(xm)
				a.setModulePath(xm->GetFilename().c_str());
			else
				a.setBuiltinDef(&fb.key->GetDef());
			return true;
		}
	}

	return false;
}
