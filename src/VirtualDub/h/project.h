#ifndef f_PROJECT_H
#define f_PROJECT_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/VDScheduler.h>
#include <vd2/system/fraction.h>
#include <vd2/system/event.h>
#include "FrameSubset.h"
#include "FilterFrameVideoSource.h"
#include "filter.h"
#include "timeline.h"
#include <list>
#include <utility>
#include <vector>

class SceneDetector;
class IVDDubberOutputSystem;
class IVDInputDriver;
class IDubStatusHandler;
class DubOptions;
class InputFileOptions;
class VDProjectSchedulerThread;
class AudioSource;
class IDubber;
struct VDAVIOutputRawVideoFormat;
struct VDAVIOutputCLITemplate;
class VDFile;
class VDProject;

enum VDAudioSourceMode {
	kVDAudioSourceMode_None		= 0,
	kVDAudioSourceMode_External	= 1,
	kVDAudioSourceMode_Source	= 2
};

class IVDProjectUICallback {
public:
	virtual void UIRefreshInputFrame(const VDPixmap *px) = 0;
	virtual void UIRefreshOutputFrame(const VDPixmap *px) = 0;
	virtual void UISetDubbingMode(bool bActive, bool bIsPreview) = 0;
	virtual bool UIRunDubMessageLoop() = 0;
	virtual void UIAbortDubMessageLoop() = 0;		// Note: multithreaded
	virtual void UICurrentPositionUpdated() = 0;
	virtual void UISelectionUpdated(bool notifyUser) = 0;
	virtual void UITimelineUpdated() = 0;
	virtual void UIShuttleModeUpdated() = 0;
	virtual void UISourceFileUpdated() = 0;
	virtual void UIAudioSourceUpdated() = 0;
	virtual void UIVideoSourceUpdated() = 0;
	virtual void UIVideoFiltersUpdated() = 0;
	virtual void UIDubParametersUpdated() = 0;
};

enum {
	kVDProjectCmd_Null,
	kVDProjectCmd_GoToStart,
	kVDProjectCmd_GoToEnd,
	kVDProjectCmd_GoToPrevFrame,
	kVDProjectCmd_GoToNextFrame,
	kVDProjectCmd_GoToPrevUnit,
	kVDProjectCmd_GoToNextUnit,
	kVDProjectCmd_GoToPrevKey,
	kVDProjectCmd_GoToNextKey,
	kVDProjectCmd_GoToPrevDrop,
	kVDProjectCmd_GoToNextDrop,
	kVDProjectCmd_GoToSelectionStart,
	kVDProjectCmd_GoToSelectionEnd,
	kVDProjectCmd_ScrubBegin,
	kVDProjectCmd_ScrubEnd,
	kVDProjectCmd_ScrubUpdate,
	kVDProjectCmd_ScrubUpdatePrev,
	kVDProjectCmd_ScrubUpdateNext,
	kVDProjectCmd_SetSelectionStart,
	kVDProjectCmd_SetSelectionEnd
};

class FilterModTimeline: public IFilterModTimeline {
public:
	VDProject* project;

	FilterModTimeline(){ project=0; }
	virtual int64 GetTimelinePos();
	virtual int64 TimelineToFilterSource(int64 frame);
	virtual int64 FilterSourceToTimeline(int64 frame);
};

class FilterModSystem: public IFilterModSystem {
public:
	virtual bool CreateVideoFilter(VDXHWND hParent, FilterReturnInfo& a);
	virtual bool FindVideoFilter(const char* name, FilterReturnInfo& a);
};

class VDProject {
public:
	VDProject();
	~VDProject();

	virtual bool Attach(VDGUIHandle hwnd);
	virtual void Detach();

	void SetUICallback(IVDProjectUICallback *pCB);

	VDTimeline& GetTimeline() { return mTimeline; }
	void BeginTimelineUpdate(const wchar_t *undostr = 0);
	void EndTimelineUpdate();

	// This is a workaround for the way that script can change options. We don't want
	// to handle it fully automagically as that will cause the filter system to get
	// hit a lot.
	void MarkTimelineRateDirty() { mbTimelineRateDirty = true; }
	void UpdateTimelineRate();

	bool Undo();
	bool Redo();
	void ClearUndoStack();
	const wchar_t *GetCurrentUndoAction();
	const wchar_t *GetCurrentRedoAction();

	bool Tick();

	VDPosition GetCurrentFrame();
	VDPosition GetFrameCount();
	VDFraction GetInputFrameRate();
	VDFraction GetTimelineFrameRate();

	typedef std::list<std::pair<uint32, VDStringA> > tTextInfo;
	tTextInfo& GetTextInfo() { return mTextInfo; }
	const tTextInfo& GetTextInfo() const { return mTextInfo; }

	void ClearSelection(bool notifyUser = true);
	bool IsSelectionEmpty();
	bool IsSelectionPresent();
	void SetSelectionStart();
	void SetSelectionStart(VDPosition pos, bool notifyUser = true);
	void SetSelectionEnd();
	void SetSelectionEnd(VDPosition pos, bool notifyUser = true);
	void SetSelection(VDPosition start, VDPosition end, bool notifyUser = true);
	VDPosition GetSelectionStartFrame();
	VDPosition GetSelectionEndFrame();

	bool IsClipboardEmpty();
	bool IsSceneShuttleRunning();

	void SetPositionCallbackEnabled(bool enable);

	void Cut();
	void Copy();
	void Paste();
	void Delete();
	void DeleteInternal(bool tagAsCut, bool noTag);
	void CropToSelection();
	void MaskSelection(bool bMasked);

	void LockDisplayFrame();
	void UnlockDisplayFrame();
	void DisplayFrame(bool bDispInput = true, bool bDispOutput = true, bool forceInput = false, bool forceOutput = false);

	void RunOperation(IVDDubberOutputSystem *pOutputSystem, int fAudioOnly, DubOptions *pOptions, int iPriority=0, bool fPropagateErrors = false, long lSpillThreshold = 0, long lSpillFrameThreshold = 0, bool backgroundPriority = false);

	////////////////////

	void AutoSave(VDFile& f);

	void Quit();
	void Open(const wchar_t *pFilename, IVDInputDriver *pSelectedDriver = 0, bool fExtendedOpen = false, bool fQuiet = false, bool fAutoscan = false, const char *pInputOpts = 0, uint32 inputOptsLen = 0);
	void Reopen();
	void OpenWAV(const wchar_t *pFilename, IVDInputDriver *pSelectedDriver = NULL, bool automated = false, bool extOpts = false, const void *optdata = NULL, int optlen = 0);
	void CloseWAV();
	void PreviewInput();
	void PreviewOutput();
	void PreviewAll();
	void Preview(DubOptions *options);
	void PreviewRestart();
	void RunNullVideoPass();
	void QueueNullVideoPass();
	void CloseAVI();			// to be removed later....
	void Close();

	void SaveAVI(const wchar_t *filename, bool compat, bool addAsJob);
	void SaveFilmstrip(const wchar_t *pFilename, bool propagateErrors);
	void SaveAnimatedGIF(const wchar_t *pFilename, int loopCount, bool propagateErrors, DubOptions *optsOverride = NULL);
	void SaveRawAudio(const wchar_t *pFilename, bool propagateErrors, DubOptions *optsOverride = NULL);
	void SaveRawVideo(const wchar_t *pFilename, const VDAVIOutputRawVideoFormat& format, bool propagateErrors, DubOptions *optsOverride = NULL);
	void ExportViaEncoder(const wchar_t *filename, const wchar_t *encSetName, bool propagateErrors, DubOptions *optsOverride = NULL);

	void StartServer(const char *name = NULL);
	void ShowInputInfo();
	void SetVideoMode(int mode);
	void CopySourceFrameToClipboard();
	void CopyOutputFrameToClipboard();
	void CopySourceFrameNumberToClipboard();
	void CopyOutputFrameNumberToClipboard();

	const wchar_t *GetAudioSourceDriverName() const { return mAudioInputDriverName.c_str(); }
	const InputFileOptions *GetAudioSourceOptions() const { return mpAudioInputOptions; }
	int GetAudioSourceCount() const;
	int GetAudioSourceMode() const;
	void SetAudioSourceNone();
	void SetAudioSourceNormal(int index);

	void SetAudioMode(int mode);
	void SetAudioErrorMode(int errorMode);
	void MoveToFrame(VDPosition pos);
	void MoveToStart();
	void MoveToPrevious();
	void MoveToNext();
	void MoveToEnd();
	void MoveToSelectionStart();
	void MoveToSelectionEnd();
	void MoveToNearestKey(VDPosition pos);
	void MoveToNearestKeyNext(VDPosition pos);
	void MoveToPreviousKey();
	void MoveToNextKey();
	void MoveBackSome();
	void MoveForwardSome();
	void StartSceneShuttleReverse();
	void StartSceneShuttleForward();
	void MoveToPreviousRange();
	void MoveToNextRange();
	void MoveToPreviousDrop();
	void MoveToNextDrop();
	void ResetTimeline();
	void ResetTimelineWithConfirmation();
	void ScanForErrors();
	void AbortOperation();

	// hack
	void StopFilters();
	void PrepareFilters();
	void StartFilters();
	void UpdateFilterList();

	// Mark off the current filter chain output rate/count, and then later check if it
	// has changed and update the timeline accordingly.
	void BeginFilterUpdates();
	void EndFilterUpdates();

	void SceneShuttleStop();

	FilterModTimeline filterModTimeline;
	FilterModSystem filterModSystem;
	FilterModPixmap filterModPixmap;

protected:
	void AdjustTimelineForFilterChanges(const VDFraction& oldRate, sint64 oldFrameCount, const VDFraction& newRate, sint64 newFrameCount);
	void SceneShuttleStep();
	bool UpdateFrame(bool updateInputFrame = true);
	bool RefilterFrame(VDPosition timelinePos);
	void LockFilterChain(bool enableLock);

	static void StaticPositionCallback(VDPosition start, VDPosition cur, VDPosition end, int progress, void *cookie);

	void UpdateDubParameters(bool forceUpdate = false);
	void SetAudioSource();

	void OnDubAbort(IDubber *src, const bool& userAbort);

	VDGUIHandle		mhwnd;

	IVDProjectUICallback *mpCB;

	SceneDetector	*mpSceneDetector;
	int		mSceneShuttleMode;
	int		mSceneShuttleAdvance;
	int		mSceneShuttleCounter;

	FrameSubset		mClipboard;
	VDTimeline		mTimeline;

	struct UndoEntry {
		FrameSubset	mSubset;
		VDStringW	mDescription;
		VDPosition	mFrame;
		VDPosition	mSelStart;
		VDPosition	mSelEnd;

		UndoEntry(const FrameSubset& s, const wchar_t *desc, VDPosition pos, VDPosition selStart, VDPosition selEnd) : mSubset(s), mDescription(desc), mFrame(pos), mSelStart(selStart), mSelEnd(selEnd) {}
	};
	std::list<UndoEntry>	mUndoStack;
	std::list<UndoEntry>	mRedoStack;

	IDubStatusHandler	*mpDubStatus;

	VDPosition	mposCurrentFrame;
	VDPosition	mposSelectionStart;
	VDPosition	mposSelectionEnd;
	bool		mbPositionCallbackEnabled;

	bool		mbFilterChainLocked;
	bool		mbTimelineRateDirty;

	VDXFilterStateInfo mfsi;
	VDPosition		mDesiredOutputFrame;
	VDPosition		mDesiredTimelineFrame;
	VDPosition		mDesiredNextInputFrame;
	VDPosition		mDesiredNextOutputFrame;
	VDPosition		mDesiredNextTimelineFrame;
	VDPosition		mLastDisplayedInputFrame;
	VDPosition		mLastDisplayedTimelineFrame;
	bool			mbUpdateInputFrame;

	vdrefptr<IVDFilterFrameClientRequest> mpCurrentInputFrame;
	vdrefptr<IVDFilterFrameClientRequest> mpPendingInputFrame;
	vdrefptr<IVDFilterFrameClientRequest> mpCurrentOutputFrame;
	vdrefptr<IVDFilterFrameClientRequest> mpPendingOutputFrame;
	bool			mbPendingInputFrameValid;
	bool			mbPendingOutputFrameValid;

	bool			mbUpdateLong;
	int				mFramesDecoded;
	uint32			mLastDecodeUpdate;

	uint32			mDisplayFrameLocks;
	bool			mbDisplayFrameDeferred;

	enum PreviewRestartMode {
		kPreviewRestart_None,
		kPreviewRestart_Input,
		kPreviewRestart_Output,
		kPreviewRestart_All
	} mPreviewRestartMode;

	vdblock<char>	mVideoSampleBuffer;

	VDFraction		mVideoInputFrameRate;
	VDFraction		mVideoOutputFrameRate;
	VDFraction		mVideoTimelineFrameRate;

	sint64			mVideoMarkedPostFiltersFrameCount;
	VDFraction		mVideoMarkedPostFiltersFrameRate;

	vdrefptr<VDFilterFrameVideoSource>	mpVideoFrameSource;

	std::list<std::pair<uint32, VDStringA> >	mTextInfo;

	int				mAudioSourceMode;
	typedef std::vector<vdrefptr<AudioSource> > AudioSources;
	AudioSources 			mInputAudioSources;
	vdrefptr<AudioSource>	mpInputAudioExt;

	VDStringW		mInputDriverName;
	VDStringW		mAudioInputDriverName;
	vdautoptr<InputFileOptions>	mpAudioInputOptions;

	VDDelegate		mStoppedDelegate;
};

#endif
