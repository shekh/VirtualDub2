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

#ifndef f_PROJECTUI_H
#define f_PROJECTUI_H

#include <windows.h>
#include <shellapi.h>
#include <vd2/system/event.h>
#include <vd2/system/thread.h>
#include <vd2/VDDisplay/display.h>

#include "project.h"
#include "MRUList.h"
#include "PositionControl.h"
#include "uiframe.h"
#include "ParameterCurveControl.h"
#include "AudioDisplay.h"
#include "AccelEditDialog.h"

class IVDPositionControl;
class IVDUIWindow;
class IVDUIAudioDisplayControl;

class VDProjectUI : public VDProject, public vdrefcounted<IVDUIFrameClient>, protected IVDVideoDisplayCallback, public IVDPositionControlCallback, public IVDProjectUICallback, public IVDUICallback {
public:
	VDProjectUI();
	~VDProjectUI();

	bool Attach(VDGUIHandle hwnd);
	void Detach();

	bool Tick();

	void SetTitle(int nTitleString, int nArgs, ...);

	enum PaneLayoutMode {
		kPaneLayoutDual,
		kPaneLayoutInput,
		kPaneLayoutOutput,
		kPaneLayoutModeCount
	};

	void SetPaneLayout(PaneLayoutMode layout);
	bool IsInputPaneUsed();

	void OpenAsk();
	void AppendAsk();
	void SaveAVIAsk(bool batchMode);
	void SaveCompatibleAVIAsk(bool batchMode);
	void SaveImageSequenceAsk(bool batchMode);
	void SaveImageAsk();
	void SaveSegmentedAVIAsk(bool batchMode);
	void SaveWAVAsk(bool batchMode);
	void SaveFilmstripAsk();
	void SaveAnimatedGIFAsk();
	void SaveAnimatedPNGAsk();
	void SaveRawAudioAsk(bool batchMode);
	void SaveRawVideoAsk(bool batchMode);
	void ExportViaEncoderAsk(bool batchMode);
	void ExportViaDriverTool(int id);
	void ExportViaDriverTool(const char* id);
	void SaveConfigurationAsk();
	void LoadConfigurationAsk();
	void LoadProjectAsk();
	void SetVideoFiltersAsk();
	void SetVideoFramerateOptionsAsk();
	void SetVideoDepthOptionsAsk();
	void SetVideoRangeOptionsAsk();
	void SetVideoCompressionAsk();
	void SetVideoErrorModeAsk();
	void SetAudioFiltersAsk();
	void SetAudioConversionOptionsAsk();
	void SetAudioInterleaveOptionsAsk();
	void SetAudioCompressionAsk(HWND parent);
	void SetAudioVolumeOptionsAsk();
	void SetAudioSourceWAVAsk();
	void SetAudioErrorModeAsk();
	void JumpToFrameAsk();

	void OpenNewInstance();
	void CloseAndDelete();
	void OpenPrevious();
	void OpenNext();

	void DisplayPreview(bool v);
	HWND GetHwnd(){ return (HWND)mhwnd; }
	HACCEL GetAccelPreview(){ return mhAccelPreview; }
	void RepositionPanes(bool reset=false);
	void ResetCentering();
	void ToggleFullscreen();
	void SwapFullscreen();

public:
	const wchar_t *edit_token;
	WNDPROC prevStatusProc;
	void UpdateAccelMain();

protected:
	void OpenPreviousNext(bool next);

	bool QueueCommand(int cmd);
	void ExecuteCommand(int cmd);

	LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT MainWndProc( UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT DubWndProc(UINT msg, WPARAM wParam, LPARAM lParam);
	bool HotkeyProc(UINT msg, WPARAM wParam, LPARAM lParam);
	virtual bool Intercept_WM_KEYUP(WPARAM wParam, LPARAM lParam) { return HotkeyProc(WM_KEYUP, wParam, lParam); }
	virtual bool Intercept_WM_SYSKEYUP(WPARAM wParam, LPARAM lParam) { return HotkeyProc(WM_KEYUP, wParam, lParam); }
	void OnGetMinMaxInfo(MINMAXINFO& mmi);
	void OnPositionNotify(int cmd);
	void OnSize();
	void HandleDragDrop(HDROP hdrop);
	void OnPreferencesChanged();
	bool MenuHit(UINT id);
	void RepaintMainWindow(HWND hWnd);
	void ShowMenuHelp(WPARAM wParam);
	bool DoFrameRightClick(LPARAM lParam);
	void UpdateMainMenu(HMENU hMenu);
	void UpdateAudioSourceMenu();
	void UpdateDubMenu(HMENU hMenu);
	void UpdateVideoFrameLayout();
	void UpdateAccelPreview();
	void UpdateAccelDub();

	void OpenAudioDisplay();
	void CloseAudioDisplay();
	bool TickAudioDisplay();
	void UpdateAudioDisplay();
	void UpdateAudioDisplayPosition();

	void OpenCurveEditor();
	void CloseCurveEditor();
	void UpdateCurveList();
	void UpdateCurveEditorPosition();
	void UpdateMaximize();

	void UIRefreshInputFrame(const VDPixmap *px);
	void UIRefreshOutputFrame(const VDPixmap *px);
	void UISetDubbingMode(bool bActive, bool bIsPreview);
	bool UIRunDubMessageLoop();
	void UIAbortDubMessageLoop();
	void UICurrentPositionUpdated(bool fast_update=false);
	void UISelectionUpdated(bool notifyUser);
	void UINotifySelection();
	void UITimelineUpdated();
	void UIMarkerUpdated();
	void UIShuttleModeUpdated();
	void UISourceFileUpdated();
	void UIAudioSourceUpdated();
	void UIVideoSourceUpdated();
	void UIVideoFiltersUpdated();
	void UIDubParametersUpdated();
	void UiDisplayPreferencesUpdated();

	void UpdateMRUList();
	void SetStatus(const wchar_t *s);

	void DisplayRequestUpdate(IVDVideoDisplay *pDisp);
	void RefreshInputPane();
	void RefreshOutputPane();

	bool GetFrameString(wchar_t *buf, size_t buflen, VDPosition dstFrame);

	void LoadSettings();
	void SaveSettings();

	bool HandleUIEvent(IVDUIBase *pBase, IVDUIWindow *pWin, uint32 id, eEventType type, int item);
	void OnCurveUpdated(IVDUIParameterCurveControl *source, const int& args);
	void OnCurveStatusUpdated(IVDUIParameterCurveControl *source, const IVDUIParameterCurveControl::Status& status);
	void OnAudioDisplayUpdateRequired(IVDUIAudioDisplayControl *source, const VDPosition& pos);
	void OnAudioDisplaySetSelect(IVDUIAudioDisplayControl *source, const VDUIAudioDisplaySelectionRange& pos);
	void OnAudioDisplayTrackAudioOffset(IVDUIAudioDisplayControl *source, const sint32& offset);
	void OnAudioDisplaySetAudioOffset(IVDUIAudioDisplayControl *source, const sint32& offset);

	LRESULT (VDProjectUI::*mpWndProc)(UINT, WPARAM, LPARAM);

	HWND		mhwndPosition;
	vdrefptr<IVDPositionControl> mpPosition;
	HWND		mhwndStatus;
	HWND		mhwndInputFrame;
	HWND		mhwndOutputFrame;
	HWND		mhwndInputDisplay;
	HWND		mhwndOutputDisplay;
	HWND		mhwndMaxDisplay;
	ModelessDlgNode	max_dlg_node;
	IVDVideoDisplay	*mpInputDisplay;
	IVDVideoDisplay	*mpOutputDisplay;

	HWND		mhwndFilters;

	vdrefptr<IVDUIParameterCurveControl> mpCurveEditor;
	HWND		mhwndCurveEditor;

	vdrefptr<IVDUIAudioDisplayControl> mpAudioDisplay;
	HWND		mhwndAudioDisplay;
	VDPosition	mAudioDisplayPosNext;
	bool		mbAudioDisplayReadActive;

	HMENU		mhMenuNormal;
	HMENU		mhMenuSourceList;
	HMENU		mhMenuDub;
	HMENU		mhMenuDisplay;
	HMENU		mhMenuExport;
	HMENU		mhMenuTools;
	int			mMRUListPosition;
	HACCEL		mhAccelDub;
	HACCEL		mhAccelMain;
	HACCEL		mhAccelPreview;

	RECT		mrInputFrame;
	RECT		mrOutputFrame;
	bool		mbInputFrameValid;
	bool		mbOutputFrameValid;

	WNDPROC		mOldWndProc;
	bool		mbDubActive;
	bool		mbPositionControlVisible;
	bool		mbStatusBarVisible;
	bool		mbFiltersPreview;
	bool		mbMaximize;
	bool		mbMaximizeChanging;

	bool		mbLockPreviewRestart;

	PaneLayoutMode	mPaneLayoutMode;
	bool		mbPaneLayoutBusy;
	bool		mbAutoSizeInput;
	bool		mbAutoSizeOutput;
	bool		mbPanesNeedUpdate;
	float		mInputZoom;
	float		mOutputZoom;

	bool		mbShowAudio;

	VDThreadID	mThreadId;

	MRUList		mMRUList;

	typedef vdfastvector<int> PendingCommands;
	PendingCommands		mPendingCommands;

	vdrefptr<IVDUIWindow>	mpUIPeer;
	vdrefptr<IVDUIWindow>	mpUIBase;
	vdrefptr<IVDUIWindow>	mpUIPaneSet;
	vdrefptr<IVDUIWindow>	mpUISplitSet;
	vdrefptr<IVDUIWindow>	mpUICurveSet;
	vdrefptr<IVDUIWindow>	mpUICurveSplitBar;
	vdrefptr<IVDUIWindow>	mpUICurveEditor;
	vdrefptr<IVDUIWindow>	mpUIInputFrame;
	vdrefptr<IVDUIWindow>	mpUIOutputFrame;
	vdrefptr<IVDUIWindow>	mpUICurveComboBox;

	vdrefptr<IVDUIWindow>	mpUIAudioSplitBar;
	vdrefptr<IVDUIWindow>	mpUIAudioDisplay;

	VDAccelTableDefinition	mAccelTableDef;
	VDAccelTableDefinition	mAccelTableDefault;

	VDDelegate mCurveUpdatedDelegate;
	VDDelegate mCurveStatusUpdatedDelegate;
	VDDelegate mAudioDisplayUpdateRequiredDelegate;
	VDDelegate mAudioDisplaySetSelectStartDelegate;
	VDDelegate mAudioDisplaySetSelectTrackDelegate;
	VDDelegate mAudioDisplaySetSelectEndDelegate;
	VDDelegate mAudioDisplayTrackAudioOffsetDelegate;
	VDDelegate mAudioDisplaySetAudioOffsetDelegate;
};

#endif
