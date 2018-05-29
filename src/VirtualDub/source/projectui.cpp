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

#include <stdafx.h>
#include <windows.h>
#include <commctrl.h>
#include <vfw.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/binary.h>
#include <vd2/system/filesys.h>
#include <vd2/system/process.h>
#include <vd2/system/registry.h>
#include <vd2/system/strutil.h>
#include <vd2/system/w32assist.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Dita/basetypes.h>
#include <vd2/Dita/services.h>
#include <vd2/Dita/controls.h>
#include <vd2/Dita/resources.h>
#include <vd2/Dita/w32control.h>
#include <vd2/Dita/w32accel.h>
#include <vd2/plugin/vdinputdriver.h>
#include <vd2/VDDisplay/display.h>
#include <vd2/VDLib/Dialog.h>
#include "projectui.h"
#include "resource.h"
#include "PositionControl.h"
#include "command.h"
#include "prefs.h"
#include "gui.h"
#include "oshelper.h"
#include "VideoSource.h"
#include "AudioSource.h"
#include "HexViewer.h"
#include "Dub.h"
#include "DubOutput.h"
#include "DubStatus.h"
#include "job.h"
#include "script.h"
#include "optdlg.h"
#include "auxdlg.h"
#include "FilterInstance.h"
#include "filters.h"
#include "filtdlg.h"
#include "mrulist.h"
#include "InputFile.h"
#include "VideoWindow.h"
#include "AccelEditDialog.h"
#include "ExternalEncoderProfile.h"
#include "tool.h"

///////////////////////////////////////////////////////////////////////////

namespace {
	enum {
		kFileDialog_WAVAudioIn		= 'wavi',
		kFileDialog_WAVAudioOut		= 'wavo',
		kFileDialog_RawAudioOut		= 'rwao',
		kFileDialog_RawVideoOut		= 'rwvo',
		kFileDialog_FLMOut			= 'flmo',
		kFileDialog_GIFOut			= 'gifo',
		kFileDialog_PNGOut			= 'pngo',
		kFileDialog_ExtOut			= 'exto',
		kFileDialog_Project 		= 'proj',
	};

	enum {
		kVDST_ProjectUI = 7,
		kVDST_RawVideoFormats = 11
	};

	enum {
		kVDM_TitleIdle,
		kVDM_TitleFileLoaded,
		kVDM_TitleRunning,
		kVDM_Undo,
		kVDM_Redo
	};

	enum {
		kMenuPos_Go					= 2
	};
}

#define MYWM_DEFERRED_COMMAND (WM_USER + 101)
#define MYWM_DEFERRED_PREVIEWRESTART (WM_USER + 102)
#define MYWM_DEFERRED_FILECOMMAND (WM_USER + 103)

///////////////////////////////////////////////////////////////////////////

extern const char g_szError[];
extern const char g_szWarning[];

extern bool g_bEnableVTuneProfiling;
extern bool g_bAutoTest;

extern HINSTANCE g_hInst;
extern VDProject *g_project;
extern vdrefptr<VDProjectUI> g_projectui;
extern HWND g_hWnd;

extern vdrefptr<AudioSource>	inputAudio;
extern COMPVARS2 g_Vcompression;

static bool				g_vertical				= FALSE;
PanCenteringMode g_panCentering = kPanCenter;

extern DubSource::ErrorMode	g_videoErrorMode;
extern DubSource::ErrorMode	g_audioErrorMode;

extern bool				g_fDropFrames;
extern bool				g_fDropSeeking;
extern bool				g_fSwapPanes;
extern bool				g_bExit;

extern bool g_fJobMode;

extern wchar_t g_szInputAVIFile[MAX_PATH];
extern wchar_t g_szInputWAVFile[MAX_PATH];

vdfastvector<VDAccelToCommandEntry> kCommandList;

extern void VDCPUTest();

// need to do this directly in Dita....
extern const char g_szRegKeyPersistence[]="Persistence";

///////////////////////////////////////////////////////////////////////////

extern IVDPositionControlCallback *VDGetPositionControlCallbackTEMP() {
	return static_cast<IVDPositionControlCallback *>(&*g_projectui);
}

extern char PositionFrameTypeCallback(HWND hwnd, void *pvData, long pos);

extern void ChooseCompressor(HWND hwndParent, COMPVARS2 *lpCompVars);
extern WAVEFORMATEX *AudioChooseCompressor(HWND hwndParent, WAVEFORMATEX *, WAVEFORMATEX *, VDString& shortNameHint, vdblock<char>& config, bool enable_plugin=true);
extern void VDDisplayLicense(HWND hwndParent, bool conditional);

extern void OpenInput(bool append=false, bool audio=false);
extern void SaveAVI(HWND, bool, bool queueAsBatch);
extern void SaveSegmentedAVI(HWND, bool queueAsBatch);
extern void OpenImageSeq(HWND hwnd);
extern void SaveImageSeq(HWND, bool queueAsBatch);
extern void SaveImage(HWND, VDPosition frame, VDPixmap* px);
extern void SaveWAV(HWND, bool queueAsBatch);
extern void SaveConfiguration(HWND);
extern void SaveProject(HWND, bool reset_path);
extern void CreateExtractSparseAVI(HWND hwndParent, bool bExtract);

extern const VDStringW& VDPreferencesGetTimelineFormat();
int VDPreferencesGetMRUSize();
int VDPreferencesGetHistoryClearCounter();
bool VDPreferencesGetConfirmExit();

int VDRenderSetVideoSourceInputFormat(IVDVideoSource *vsrc, VDPixmapFormatEx format);

void VDUIDisplayDialogConfigureExternalEncoders(VDGUIHandle h);
void VDUIShowDialogPlugins(VDGUIHandle h);

///////////////////////////////////////////////////////////////////////////
#define MENU_TO_HELP(x) ID_##x, IDS_##x

UINT iMainMenuHelpTranslator[]={
	MENU_TO_HELP(FILE_OPENAVI),
	MENU_TO_HELP(FILE_APPENDSEGMENT),
	MENU_TO_HELP(FILE_PREVIEWINPUT),
	MENU_TO_HELP(FILE_PREVIEWOUTPUT),
	MENU_TO_HELP(FILE_PREVIEWAVI),
	MENU_TO_HELP(FILE_SAVEAVI),
	MENU_TO_HELP(FILE_SAVECOMPATIBLEAVI),
	MENU_TO_HELP(FILE_SAVEIMAGESEQ),
	MENU_TO_HELP(FILE_SAVESEGMENTEDAVI),
	MENU_TO_HELP(FILE_CLOSEAVI),
	MENU_TO_HELP(FILE_CAPTUREAVI),
	MENU_TO_HELP(FILE_STARTSERVER),
	MENU_TO_HELP(FILE_AVIINFO),
	MENU_TO_HELP(FILE_SAVEWAV),
	MENU_TO_HELP(FILE_QUIT),
	MENU_TO_HELP(FILE_LOADCONFIGURATION),
	MENU_TO_HELP(FILE_SAVECONFIGURATION),

	MENU_TO_HELP(VIDEO_SEEK_START),
	MENU_TO_HELP(VIDEO_SEEK_END),
	MENU_TO_HELP(VIDEO_SEEK_PREV),
	MENU_TO_HELP(VIDEO_SEEK_NEXT),
	MENU_TO_HELP(VIDEO_SEEK_KEYPREV),
	MENU_TO_HELP(VIDEO_SEEK_KEYNEXT),
	MENU_TO_HELP(VIDEO_SEEK_SELSTART),
	MENU_TO_HELP(VIDEO_SEEK_SELEND),
	MENU_TO_HELP(VIDEO_SEEK_PREVDROP),
	MENU_TO_HELP(VIDEO_SEEK_NEXTDROP),
	MENU_TO_HELP(EDIT_JUMPTO),
	MENU_TO_HELP(EDIT_DELETE),
	MENU_TO_HELP(EDIT_SETSELSTART),
	MENU_TO_HELP(EDIT_SETSELEND),

	MENU_TO_HELP(VIDEO_FILTERS),
	MENU_TO_HELP(VIDEO_FRAMERATE),
	MENU_TO_HELP(VIDEO_COLORDEPTH),
	MENU_TO_HELP(VIDEO_COMPRESSION),
	MENU_TO_HELP(VIDEO_CLIPPING),
	MENU_TO_HELP(VIDEO_MODE_DIRECT),
	MENU_TO_HELP(VIDEO_MODE_FASTRECOMPRESS),
	MENU_TO_HELP(VIDEO_MODE_NORMALRECOMPRESS),
	MENU_TO_HELP(VIDEO_MODE_FULL),
	MENU_TO_HELP(AUDIO_CONVERSION),
	MENU_TO_HELP(AUDIO_INTERLEAVE),
	MENU_TO_HELP(AUDIO_COMPRESSION),
	MENU_TO_HELP(AUDIO_SOURCE_NONE),
	MENU_TO_HELP(AUDIO_SOURCE_WAV),
	MENU_TO_HELP(AUDIO_MODE_DIRECT),
	MENU_TO_HELP(AUDIO_MODE_FULL),
	MENU_TO_HELP(OPTIONS_PREFERENCES),
	MENU_TO_HELP(OPTIONS_PERFORMANCE),
	MENU_TO_HELP(OPTIONS_DISPLAYINPUTVIDEO),
	MENU_TO_HELP(OPTIONS_DISPLAYOUTPUTVIDEO),
	MENU_TO_HELP(OPTIONS_DISPLAYDECOMPRESSEDOUTPUT),
	MENU_TO_HELP(OPTIONS_SHOWSTATUSWINDOW),
	MENU_TO_HELP(OPTIONS_VERTICALDISPLAY),
	MENU_TO_HELP(OPTIONS_SYNCTOAUDIO),
	MENU_TO_HELP(OPTIONS_DROPFRAMES),
	MENU_TO_HELP(OPTIONS_ENABLEDIRECTDRAW),

	MENU_TO_HELP(TOOLS_HEXVIEWER),
	MENU_TO_HELP(TOOLS_CREATESPARSEAVI),
	MENU_TO_HELP(TOOLS_EXPANDSPARSEAVI),

	MENU_TO_HELP(HELP_CONTENTS),
	MENU_TO_HELP(HELP_ABOUT),
	NULL,NULL,
};

namespace {
	static const VDAccelToCommandEntry kCommandList_init[]={
		{ ID_FILE_QUIT,					"File.Quit" },
		{ ID_FILE_OPENAVI,				"File.Open" },
		{ ID_FILE_REOPEN,				"File.ReOpen" },
		{ ID_FILE_APPENDSEGMENT,		"File.Append" },
		{ ID_FILE_PREVIEWINPUT,			"File.PreviewInput" },
		{ ID_FILE_PREVIEWOUTPUT,		"File.PreviewOutput" },
		{ ID_FILE_PREVIEWAVI,			"File.Preview" },
		{ ID_FILE_RUNVIDEOANALYSISPASS,	"File.RunVideoAnalysisPass" },
		{ ID_FILE_SAVEAVI,				"File.SaveAVI" },
		{ ID_FILE_SAVECOMPATIBLEAVI,	"File.SaveCompatibleAVI" },
		{ ID_FILE_SAVEIMAGESEQ,			"File.SaveImageSequence" },
		{ ID_FILE_SAVEIMAGE,			"File.SaveImage" },
		{ ID_FILE_SAVESEGMENTEDAVI,		"File.SaveSegmentedAVI" },
		{ ID_FILE_SAVEFILMSTRIP,		"File.SaveFilmstrip" },
		{ ID_FILE_SAVEANIMATEDGIF,		"File.SaveAnimatedGIF" },
		{ ID_FILE_SAVEANIMATEDPNG,		"File.SaveAnimatedPNG" },
		{ ID_FILE_SAVERAWAUDIO,			"File.SaveRawAudio" },
		{ ID_FILE_SAVERAWVIDEO,			"File.SaveRawVideo" },
		{ ID_FILE_EXPORTEXTERNALENCODER,	"File.ExportViaEncoder" },
		{ ID_FILE_SAVEWAV,				"File.SaveWAV" },
		{ ID_FILE_CLOSEAVI,				"File.Close" },
		{ ID_FILE_STARTSERVER,			"App.SwitchToServerMode" },
		{ ID_FILE_CAPTUREAVI,			"App.SwitchToCaptureMode" },
		{ ID_FILE_SAVECONFIGURATION,	"File.SaveConfiguration" },
		{ ID_FILE_LOADCONFIGURATION,	"File.LoadConfiguration" },
		{ ID_FILE_SAVEPROJECT,			"File.SaveProject" },
		{ ID_FILE_SAVEPROJECTAS,		"File.SaveProjectAs" },
		{ ID_FILE_LOADPROJECT,			"File.LoadProject" },
		{ ID_FILE_RUNSCRIPT,			"App.RunScript" },
		{ ID_FILE_JOBCONTROL,			"Jobs.OpenJobControlDialog" },
		{ ID_FILE_AVIINFO,				"File.Information" },
		{ ID_FILE_SETTEXTINFO,			"File.SetTextInfo" },
		{ ID_FILE_NEWINSTANCE,			"File.NewInstance" },
		{ ID_FILE_OPENPREVIOUS,			"File.OpenPrevious" },
		{ ID_FILE_OPENNEXT,				"File.OpenNext" },
		{ ID_FILE_CLOSEANDDELETE,		"File.CloseAndDelete" },
		{ ID_QUEUEBATCHOPERATION_SAVEASAVI,	"Jobs.SaveAsAVI" },
		{ ID_QUEUEBATCHOPERATION_SAVECOMPATIBLEAVI,	"Jobs.SaveAsOldAVI" },
		{ ID_QUEUEBATCHOPERATION_SAVESEGMENTEDAVI,	"Jobs.SaveSegmentedAVI" },
		{ ID_QUEUEBATCHOPERATION_SAVEIMAGESEQUENCE,	"Jobs.SaveImageSequence" },
		{ ID_QUEUEBATCHOPERATION_EXPORTUSINGEXTERNALENCODER,	"Jobs.ExportViaEncoder" },
		{ ID_QUEUEBATCHOPERATION_RUNVIDEOANALYSISPASS,	"Jobs.RunVideoAnalysisPass" },
		{ ID_QUEUEBATCHOPERATION_SAVEWAV,	"Jobs.SaveWAV" },
		{ ID_QUEUEBATCHOPERATION_EXPORTRAWAUDIO,	"Jobs.SaveRawAudio" },
		{ ID_QUEUEBATCHOPERATION_EXPORTRAWVIDEO,	"Jobs.SaveRawVideo" },
		{ ID_FILE_BATCHWIZARD,			"Jobs.ShowBatchWizardDialog" },
		{ ID_EDIT_UNDO,					"Edit.Undo" },
		{ ID_EDIT_REDO,					"Edit.Redo" },
		{ ID_EDIT_CUT,					"Edit.Cut" },
		{ ID_EDIT_COPY,					"Edit.Copy" },
		{ ID_EDIT_PASTE,				"Edit.Paste" },
		{ ID_EDIT_DELETE,				"Edit.Delete" },
		{ ID_EDIT_CLEAR,				"Edit.Clear" },
		{ ID_EDIT_SELECTALL,			"Edit.SelectAll" },
		{ ID_EDIT_CROPTOSELECTION,		"Edit.CropToSelection" },
		{ ID_EDIT_RESET,				"Edit.Reset" },
		{ ID_EDIT_JUMPTO,				"Edit.ShowJumpToLocationDialog" },
		{ ID_EDIT_ZOOMRANGE,			"Edit.ToggleZoom" },
		{ ID_VIDEO_SEEK_START,			"Edit.GoToStart" },
		{ ID_VIDEO_SEEK_END,			"Edit.GoToEnd" },
		{ ID_VIDEO_SEEK_PREV,			"Edit.GoToPrevFrame" },
		{ ID_VIDEO_SEEK_NEXT,			"Edit.GoToNextFrame" },
		{ ID_VIDEO_SEEK_FPREV,			"Edit.GoToPrevFast" },
		{ ID_VIDEO_SEEK_FNEXT,			"Edit.GoToNextFast" },
		{ ID_VIDEO_SEEK_FSPREV,			"Edit.GoToPrevFastSticky" },
		{ ID_VIDEO_SEEK_FSNEXT,			"Edit.GoToNextFastSticky" },
		{ ID_VIDEO_SEEK_PREVONESEC,		"Edit.GoToPrevUnit" },
		{ ID_VIDEO_SEEK_NEXTONESEC,		"Edit.GoToNextUnit" },
		{ ID_EDIT_PREVRANGE,			"Edit.GoToPrevRange" },
		{ ID_EDIT_NEXTRANGE,			"Edit.GoToNextRange" },
		{ ID_VIDEO_SEEK_KEYPREV,		"Edit.GoToPrevKey" },
		{ ID_VIDEO_SEEK_KEYNEXT,		"Edit.GoToNextKey" },
		{ ID_VIDEO_SEEK_SELSTART,		"Edit.GoToSelectionStart" },
		{ ID_VIDEO_SEEK_SELEND,			"Edit.GoToSelectionEnd" },
		{ ID_VIDEO_SEEK_PREVDROP,		"Edit.GoToPrevDrop" },
		{ ID_VIDEO_SEEK_NEXTDROP,		"Edit.GoToNextDrop" },
		{ ID_VIDEO_SEEK_PREVSCENE,		"Edit.GoToPrevScene" },
		{ ID_VIDEO_SEEK_NEXTSCENE,		"Edit.GoToNextScene" },
		{ ID_VIDEO_SEEK_STOP,			"Edit.SeekStop" },
		{ ID_EDIT_MASK,					"Edit.MaskSelection" },
		{ ID_EDIT_UNMASK,				"Edit.UnmaskSelection" },
		{ ID_EDIT_SETSELSTART,			"Edit.SetSelectionStart" },
		{ ID_EDIT_SETSELEND,			"Edit.SetSelectionEnd" },
		{ ID_EDIT_SETMARKER,			"Edit.ToggleMarker" },
		{ ID_EDIT_CLEARMARKERS,			"Edit.ClearMarkers" },
		{ ID_EDIT_MARKERSFROMKEYS,		"Edit.MarkersFromKeys" },
		{ ID_VIEW_POSITIONCONTROL,		"View.TogglePositionControl" },
		{ ID_VIEW_MAXIMIZE,				"View.ToggleMaximize" },
		{ ID_VIEW_STATUSBAR,			"View.ToggleStatusBar" },
		{ ID_VIEW_CURVEEDITOR,			"View.ToggleCurveEditor" },
		{ ID_VIEW_AUDIODISPLAY,			"View.ToggleAudioDisplay" },
		{ ID_VIEW_FULLSCREEN,			"View.ToggleFullScreen" },
		{ ID_OPTIONS_SHOWLOG,			"View.ShowLogWindow" },
		{ ID_OPTIONS_SHOWPROFILER,		"View.ShowProfilerWindow" },
		{ ID_PANELAYOUT_INPUTPANEONLY,	"View.PaneLayout.ShowInputOnly" },
		{ ID_PANELAYOUT_OUTPUTPANEONLY,	"View.PaneLayout.ShowOutputOnly" },
		{ ID_PANELAYOUT_BOTHPANES,		"View.PaneLayout.ShowBoth" },
		{ ID_PANELAYOUT_AUTOSIZE,		"View.PaneLayout.ToggleAutoSize" },
		{ ID_VIDEO_SCANFORERRORS,		"Video.ScanForErrors" },
		{ ID_VIDEO_FILTERS,				"Video.ShowFiltersDialog" },
		{ ID_VIDEO_FRAMERATE,			"Video.ShowFrameRateDialog" },
		{ ID_VIDEO_COLORDEPTH,			"Video.ShowFormatDialog" },
		{ ID_VIDEO_CLIPPING,			"Video.ShowRangeDialog" },
		{ ID_VIDEO_COMPRESSION,			"Video.ShowCompressionDialog" },
		{ ID_VIDEO_MODE_DIRECT,			"Video.SetModeDirect" },
		{ ID_VIDEO_MODE_FASTRECOMPRESS,	"Video.SetModeFastRecompress" },
		{ ID_VIDEO_MODE_NORMALRECOMPRESS,	"Video.SetModeRecompress" },
		{ ID_VIDEO_MODE_FULL,			"Video.SetModeFull" },
		{ ID_VIDEO_SMARTRENDERING,		"Video.ToggleSmartRendering" },
		{ ID_VIDEO_PRESERVEEMPTYFRAMES,	"Video.TogglePreserveEmptyFrames" },
		{ ID_VIDEO_COPYSOURCEFRAME,		"Video.CopySourceFrameImage" },
		{ ID_VIDEO_COPYOUTPUTFRAME,		"Video.CopyOutputFrameImage" },
		{ ID_VIDEO_COPYSOURCEFRAMENUMBER,			"Video.CopySourceFrameNumber" },
		{ ID_VIDEO_COPYOUTPUTFRAMENUMBER,			"Video.CopyOutputFrameNumber" },
		{ ID_VIDEO_ERRORMODE,			"Video.ShowErrorModeDialog" },
		{ ID_AUDIO_ADVANCEDFILTERING,	"Audio.ToggleAdvancedFiltering" },
		{ ID_AUDIO_FILTERS,				"Audio.ShowFiltersDialog" },
		{ ID_AUDIO_CONVERSION,			"Audio.ShowConversionDialog" },
		{ ID_AUDIO_INTERLEAVE,			"Audio.ShowInterleaveDialog" },
		{ ID_AUDIO_COMPRESSION,			"Audio.ShowCompressionDialog" },
		{ ID_AUDIO_VOLUME,				"Audio.ShowVolumeDialog" },
		{ ID_AUDIO_SOURCE_NONE,			"Audio.SetSourceNone" },
		{ ID_AUDIO_SOURCE_WAV,			"Audio.ShowSourceFileDialog" },
		{ ID_AUDIO_MODE_DIRECT,			"Audio.SetModeDirect" },
		{ ID_AUDIO_MODE_FULL,			"Audio.SetModeFull" },
		{ ID_AUDIO_ERRORMODE,			"Audio.ShowErrorModeDialog" },
		{ ID_OPTIONS_PERFORMANCE,		"Options.ShowPerformanceDialog" },
		{ ID_OPTIONS_PREFERENCES,		"Options.ShowPreferencesDialog" },
		{ ID_OPTIONS_KEYBOARDSHORTCUTS,	"Options.ShowShortcutsDialog" },
		{ ID_OPTIONS_PLUGINS,			"Options.ShowPluginsDialog" },
		{ ID_OPTIONS_DISPLAYINPUTVIDEO,	"View.ToggleInputPane" },
		{ ID_OPTIONS_DISPLAYOUTPUTVIDEO,	"View.ToggleOutputPane" },
		{ ID_OPTIONS_DISPLAYDECOMPRESSEDOUTPUT,	"View.ToggleDecompressedOutput" },
		{ ID_OPTIONS_SHOWSTATUSWINDOW,	"View.ToggleStatusWindow" },
		{ ID_OPTIONS_VERTICALDISPLAY,	"View.ToggleVerticalDisplay" },
		{ ID_PAN_CENTER,				"View.PanCentering.Center" },
		{ ID_PAN_TOPLEFT,				"View.PanCentering.TopLeft" },
		{ ID_OPTIONS_SYNCTOAUDIO,		"Options.ToggleSyncToAudio" },
		{ ID_OPTIONS_ENABLEDIRECTDRAW,	"Options.ToggleVideoOverlay" },
		{ ID_OPTIONS_DROPFRAMES,		"Options.ToggleFrameDropping" },
		{ ID_OPTIONS_DROPSEEKING,		"Options.ToggleSeekDropping" },
		{ ID_OPTIONS_SWAPPANES,			"View.ToggleSwapPanes" },
		{ ID_OPTIONS_PREVIEWPROGRESSIVE,	"View.SetPreviewProgressive" },
		{ ID_OPTIONS_PREVIEWWEAVETFF,	"View.SetPreviewWeaveTFF" },
		{ ID_OPTIONS_PREVIEWWEAVEBFF,	"View.SetPreviewWeaveBFF" },
		{ ID_OPTIONS_PREVIEWBOBTFF,		"View.SetPreviewBobTFF" },
		{ ID_OPTIONS_PREVIEWBOBBFF,		"View.SetPreviewBobBFF" },
		{ ID_OPTIONS_PREVIEWNONTFF,		"View.SetPreviewFieldsTFF" },
		{ ID_OPTIONS_PREVIEWNONBFF,		"View.SetPreviewFieldsBFF" },
		{ ID_TOOLS_HEXVIEWER,			"Tools.OpenHexEditor" },
		{ ID_TOOLS_CREATESPARSEAVI,		"Tools.CreateSparseAVI" },
		{ ID_TOOLS_EXPANDSPARSEAVI,		"Tools.ExpandSparseAVI" },
		{ ID_TOOLS_BENCHMARKRESAMPLER,	"Tools.BenchmarkResampler" },
		{ ID_TOOLS_BENCHMARKANALYZEPASS,"Tools.BenchmarkAnalyzePass" },
		{ ID_TOOLS_CREATEPALETTIZEDAVI,	"Tools.CreatePalettizedAVI" },
		{ ID_TOOLS_CREATETESTVIDEO,		"Tools.CreateTestVideo" },
		{ ID_HELP_LICENSE,				"Help.ShowLicense" },
		{ ID_HELP_CONTENTS,				"Help.ShowContents" },
		{ ID_HELP_ABOUT,				"Help.ShowAbout" },
		//{ ID_DUBINPROGRESS_ABORTFAST,	"Render.AbortWithoutDialog" },
		{ ID_DUBINPROGRESS_ABORT,		"Render.Abort" },
		{ ID_EXPORT_STREAM_COPY,		"Export.StreamCopy" },
	};
}

void ResetCommandList() {
	kCommandList.clear();
	kCommandList.insert(kCommandList.begin(),&kCommandList_init[0],&kCommandList_init[sizeof(kCommandList_init)/sizeof(kCommandList_init[0])]);
}

///////////////////////////////////////////////////////////////////////////

static void VDCheckMenuItemW32(HMENU hMenu, UINT opt, bool en) {
	CheckMenuItem(hMenu, opt, en ? (MF_BYCOMMAND|MF_CHECKED) : (MF_BYCOMMAND|MF_UNCHECKED));
}

static void VDEnableMenuItemW32(HMENU hMenu, UINT opt, bool en) {
	EnableMenuItem(hMenu, opt, en ? (MF_BYCOMMAND|MF_ENABLED) : (MF_BYCOMMAND|MF_GRAYED));
}

class VDUIPaneSetWindow: public VDUIWindow {
public:
	VDProjectUI* projectUI;

	VDUIPaneSetWindow(VDProjectUI* ui){ projectUI = ui; }
	virtual void OnResize() { projectUI->RepositionPanes(); }
};

LRESULT WINAPI status_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	VDProjectUI* owner = (VDProjectUI*)GetWindowLongPtr(wnd,GWLP_USERDATA);

	switch(msg){
	case WM_SIZE:
		return 0;
	}

	return CallWindowProc(owner->prevStatusProc,wnd,msg,wparam,lparam);
}

struct max_enum_data{
	HWND max;
	vdfastvector<HWND> list;
};

BOOL CALLBACK max_enum_proc(HWND hwnd, LPARAM lParam)
{
	max_enum_data* data = (max_enum_data*)lParam;

	HWND w1 = GetWindow(hwnd,GW_OWNER);
	if(w1==g_hWnd && hwnd!=data->max){
		char buf[1024];
		GetWindowText(hwnd,buf,1024);
		data->list.insert(data->list.begin(),hwnd);
	}
	return TRUE;
}

void max_save_on_close(HWND wnd)
{
	HMONITOR mon = MonitorFromWindow(wnd,MONITOR_DEFAULTTONEAREST);
	HMONITOR mon1 = MonitorFromWindow(g_hWnd,MONITOR_DEFAULTTONEAREST);
	if (mon==mon1) {
		VDUIDeleteWindowPlacementW32("FullscreenPane");
	} else {
		VDUISaveWindowPlacementW32(wnd, "FullscreenPane");
	}
}

INT_PTR CALLBACK max_host_proc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if(msg==WM_KEYDOWN && wparam==VK_ESCAPE){
		HWND prev = GetWindow(wnd,GW_CHILD);
		if (prev) {
			IVDVideoWindow* window = VDGetIVideoWindow(prev);
			window->ToggleFullscreen();
		}
		return 0;
	}

	if(msg>=WM_KEYFIRST && msg<=WM_KEYLAST){
		MSG m = {0};
		m.hwnd = g_hWnd;
		m.message = msg;
		m.wParam = wparam;
		m.lParam = lparam;
		m.time = GetMessageTime();
		VDUIFrame::TranslateAcceleratorMessage(m);
		SetWindowLongPtr(wnd,DWLP_MSGRESULT,true);
		SendMessage(g_hWnd,msg,wparam,lparam);
		return true;
	}

	switch(msg){
	case WM_SHOWWINDOW:
		if (wparam==FALSE)
			max_save_on_close(wnd);
		break;

	case WM_DESTROY:
		{
			HWND prev = GetWindow(wnd,GW_CHILD);
			if (prev) SetParent(prev,0);
		}
		break;

	case WM_CLOSE:
		max_save_on_close(wnd);
		SendMessage(g_hWnd,WM_CLOSE,0,0);
		break;

	case WM_WINDOWPOSCHANGING:
		{
			WINDOWPOS *pwp = ((WINDOWPOS *)lparam);
			POINT xy = {pwp->x,pwp->y};
			HMONITOR mon;
			if(pwp->flags & SWP_NOMOVE)
				mon = MonitorFromWindow((HWND)wnd,MONITOR_DEFAULTTONEAREST);
			else
				mon = MonitorFromPoint(xy,MONITOR_DEFAULTTONEAREST);
			if(pwp->flags & SWP_SHOWWINDOW){
				VDUISavedWindowPlacement sp;
				if (!VDUIGetWindowPlacementW32(sp, "FullscreenPane")) {
					mon = MonitorFromWindow(g_hWnd,MONITOR_DEFAULTTONEAREST);
				}
			}
			MONITORINFO info = {sizeof(MONITORINFO)};
			GetMonitorInfo(mon,&info);
			RECT r = info.rcMonitor;
			pwp->x = r.left;
			pwp->y = r.top;
			pwp->cx = r.right-r.left;
			pwp->cy = r.bottom-r.top;
			pwp->flags &= ~(SWP_NOSIZE|SWP_NOMOVE);
			pwp->flags |= SWP_FRAMECHANGED|SWP_NOOWNERZORDER;
		}
		break;

	case WM_ACTIVATE:
		if(LOWORD(wparam)==WA_ACTIVE || LOWORD(wparam)==WA_CLICKACTIVE){
			max_enum_data data;
			data.max = wnd;
			EnumWindows(max_enum_proc, (LPARAM)&data);
			if(data.list.size() && data.list[0]!=wnd){
				for(int i=0; i<data.list.size(); i++){
					HWND w1 = data.list[i];
					if(w1!=wnd) SetWindowPos(w1,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
				}
			}
		}
		break;

	case DM_GETDEFID:
		return 0;
		
	case WM_SIZE:
		{
			HWND child = GetWindow(wnd,GW_CHILD);
			if (child) {
				RECT r;
				GetClientRect(wnd, &r);
				SetWindowPos(child, NULL, 0, 0, r.right, r.bottom, SWP_NOZORDER|SWP_NOACTIVATE);
			}
			return TRUE;
		}
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////

VDProjectUI::VDProjectUI()
	: mpWndProc(&VDProjectUI::MainWndProc)
	, mhwndPosition(NULL)
	, mhwndInputFrame(NULL)
	, mhwndOutputFrame(NULL)
	, mhwndInputDisplay(NULL)
	, mhwndOutputDisplay(NULL)
	, mhwndMaxDisplay(NULL)
	, mhwndFilters(NULL)
	, mpInputDisplay(NULL)
	, mpOutputDisplay(NULL)
	, mhwndStatus(NULL)
	, mhwndCurveEditor(NULL)
	, mhwndAudioDisplay(NULL)
	, mAudioDisplayPosNext(-1)
	, mbAudioDisplayReadActive(false)
	, mhMenuNormal(NULL)
	, mhMenuSourceList(NULL)
	, mhMenuDub(NULL)
	, mhMenuDisplay(NULL)
	, mhMenuExport(NULL)
	, mhMenuTools(NULL)
	, mhAccelDub(NULL)
	, mhAccelMain(NULL)
	, mhAccelPreview(NULL)
	, mOldWndProc(NULL)
	, mbDubActive(false)
	, mbLockPreviewRestart(false)
	, mPaneLayoutMode(kPaneLayoutDual)
	, mbPaneLayoutBusy(false)
	, mbAutoSizePanes(false)
	, mbPanesNeedUpdate(false)
	, mMRUList(0, "MRU List")
{
	edit_token = 0;
	mMRUList.set_capacity(VDPreferencesGetMRUSize());
	mMRUList.load();
	mbFiltersPreview = false;
	mbMaximize = false;
	mbMaximizeChanging = false;
}

VDProjectUI::~VDProjectUI() {
}

bool VDProjectUI::Attach(VDGUIHandle hwnd) {
	if (mhwnd)
		Detach();

	if (!VDProject::Attach(hwnd)) {
		Detach();
		return false;
	}

	mThreadId = VDGetCurrentThreadID();

	VDUIFrame *pFrame = VDUIFrame::GetFrame((HWND)mhwnd);

	pFrame->Attach(this);

	LoadSettings();
	
	// Load menus.
	if (!(mhMenuNormal	= LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_MAIN_MENU    )))) {
		Detach();
		return false;
	}

	mhMenuExport = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_FILE_EXPORTMENU));
	if (!mhMenuExport) {
		Detach();
		return false;
	}

	mhMenuTools = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_TOOLS_MENU));
	if (!mhMenuTools) {
		Detach();
		return false;
	}

	mhMenuSourceList = CreatePopupMenu();
	if (!mhMenuSourceList) {
		Detach();
		return false;
	}

	MENUITEMINFO mii = { sizeof(MENUITEMINFO) };
	mii.fMask = MIIM_SUBMENU;
	mii.hSubMenu = mhMenuSourceList;
	if (!SetMenuItemInfo(mhMenuNormal, ID_AUDIO_SOURCE_AVI, FALSE, &mii)) {
		DestroyMenu(mhMenuSourceList);
		mhMenuSourceList = NULL;
		Detach();
		return false;
	}

	UpdateAudioSourceMenu();

	if (!(mhMenuDub		= LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_DUB_MENU     )))) {
		Detach();
		return false;
	}
	if (!(mhMenuDisplay = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_DISPLAY_MENU )))) {
		Detach();
		return false;
	}

	mMRUListPosition = GetMenuItemCount(GetSubMenu(mhMenuNormal, 0)) - 2;

	// Load accelerators.

	UpdateAccelMain();
	UpdateAccelDub();
	UpdateAccelPreview();

	mhwndStatus = CreateStatusWindow(WS_CHILD|WS_VISIBLE, "", (HWND)mhwnd, IDC_STATUS_WINDOW);
	if (!mhwndStatus) {
		Detach();
		return false;
	}

	prevStatusProc = (WNDPROC)GetWindowLongPtr(mhwndStatus,GWLP_WNDPROC);
	SetWindowLongPtr(mhwndStatus,GWLP_USERDATA,(LPARAM)this);
	SetWindowLongPtr(mhwndStatus,GWLP_WNDPROC,(LPARAM)status_proc);

	SendMessage(mhwndStatus, SB_SIMPLE, TRUE, 0);

	mbPositionControlVisible = true;
	mbStatusBarVisible = true;

	// Create position window.
	mhwndPosition = CreateWindowEx(0, POSITIONCONTROLCLASS, "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | PCS_PLAYBACK | PCS_MARK | PCS_SCENE, 0, 0, 200, 64, (HWND)mhwnd, (HMENU)IDC_POSITION, g_hInst, NULL);
	if (!mhwndPosition) {
		Detach();
		return false;
	}

	mpPosition = VDGetIPositionControl((VDGUIHandle)mhwndPosition);

	SetWindowPos(mhwndPosition, NULL, 0, 0, 200, mpPosition->GetNiceHeight(), SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);

	mpPosition->SetFrameTypeCallback(this);

	// Create video windows.
	mhwndInputFrame = CreateWindow(VIDEOWINDOWCLASS, "", WS_CHILD|WS_CLIPSIBLINGS|WS_CLIPCHILDREN, 0, 0, 64, 64, (HWND)mhwnd, (HMENU)1, g_hInst, NULL);
	mhwndOutputFrame = CreateWindow(VIDEOWINDOWCLASS, "", WS_CHILD|WS_CLIPSIBLINGS|WS_CLIPCHILDREN, 0, 0, 64, 64, (HWND)mhwnd, (HMENU)2, g_hInst, NULL);

	if (!mhwndInputFrame || !mhwndOutputFrame) {
		Detach();
		return false;
	}

	mhwndInputDisplay = (HWND)VDCreateDisplayWindowW32(0, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0, 0, 64, 64, (VDGUIHandle)mhwndInputFrame);
	mhwndOutputDisplay = (HWND)VDCreateDisplayWindowW32(0, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0, 0, 64, 64, (VDGUIHandle)mhwndOutputFrame);

	if (!mhwndInputDisplay || !mhwndOutputDisplay) {
		Detach();
		return false;
	}

	mhwndMaxDisplay = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_PREVIEW), (HWND)mhwnd, max_host_proc, (LPARAM)this);
	if (!mhwndMaxDisplay) {
		Detach();
		return false;
	}
	SetWindowLong(mhwndMaxDisplay,GWL_STYLE,WS_CLIPCHILDREN);
	max_dlg_node.hdlg = mhwndMaxDisplay;
	max_dlg_node.mhAccel = 0;
	max_dlg_node.hook = true;
	guiAddModelessDialog(&max_dlg_node);
	VDUIRestoreWindowPlacementW32(mhwndMaxDisplay, "FullscreenPane", SW_HIDE);

	mpInputDisplay = VDGetIVideoDisplay((VDGUIHandle)mhwndInputDisplay);
	mpOutputDisplay = VDGetIVideoDisplay((VDGUIHandle)mhwndOutputDisplay);

	mpInputDisplay->SetCallback(this);
	mpOutputDisplay->SetCallback(this);

	ResetCentering();
	IVDVideoWindow *pInputWindow = VDGetIVideoWindow(mhwndInputFrame);
	IVDVideoWindow *pOutputWindow = VDGetIVideoWindow(mhwndOutputFrame);
	pInputWindow->SetChild(mhwndInputDisplay);
	pInputWindow->SetDisplay(mpInputDisplay);
	pInputWindow->SetMaxDisplayHost(mhwndMaxDisplay);
	pOutputWindow->SetChild(mhwndOutputDisplay);
	pOutputWindow->SetDisplay(mpOutputDisplay);
	pOutputWindow->SetMaxDisplayHost(mhwndMaxDisplay);
	pInputWindow->SetAutoSize(mbAutoSizePanes);
	pOutputWindow->SetAutoSize(mbAutoSizePanes);
	pInputWindow->InitSourcePAR();
	pOutputWindow->InitSourcePAR();

	// Create window layout.
	VDUIParameters parms;
	parms.SetB(nsVDUI::kUIParam_Child, true);
	mpUIPeer = VDUICreatePeer(mhwnd);

	parms.Clear();
	parms.SetB(nsVDUI::kUIParam_Child, true);
	mpUIBase = VDCreateUIBaseWindow();
	mpUIBase->SetAlignment(nsVDUI::kFill, nsVDUI::kFill);
	mpUIPeer->AddChild(mpUIBase);
	mpUIBase->Create(&parms);
	vdpoly_cast<IVDUIBase *>(mpUIBase)->SetCallback(this, false);

	// HACK
	HWND hwndBase = vdpoly_cast<IVDUIWindowW32 *>(mpUIBase)->GetHandleW32();
	SetWindowLong(hwndBase, GWL_STYLE, GetWindowLong(hwndBase, GWL_STYLE) | WS_CLIPCHILDREN);

	mpUISplitSet = VDCreateUISplitSet();
	mpUIBase->AddChild(mpUISplitSet);
	parms.Clear();
	parms.SetB(nsVDUI::kUIParam_IsVertical, true);
	mpUISplitSet->SetAlignment(nsVDUI::kFill, nsVDUI::kFill);
	mpUISplitSet->Create(&parms);

	mpUIPaneSet = new VDUIPaneSetWindow(this);
	mpUISplitSet->AddChild(mpUIPaneSet);
	parms.Clear();
	parms.SetB(nsVDUI::kUIParam_IsVertical, false);
	mpUIPaneSet->SetAlignment(nsVDUI::kFill, nsVDUI::kFill);
	mpUIPaneSet->Create(&parms);

	vdrefptr<IVDUIWindow> leftwinPeer(VDUICreatePeer((VDGUIHandle)mhwndInputFrame));
	vdrefptr<IVDUIWindow> rightwinPeer(VDUICreatePeer((VDGUIHandle)mhwndInputFrame));
	mpUIPaneSet->AddChild(leftwinPeer);
	mpUIPaneSet->AddChild(rightwinPeer);

	parms.Clear();
	mpUIPaneSet->Create(&parms);

	SetMenu((HWND)mhwnd, mhMenuNormal);
	OnSize();
	UpdateMRUList();

	SetUICallback(this);

	UISourceFileUpdated();		// reset title bar
	UIDubParametersUpdated();	// reset timeline parameters
	UITimelineUpdated();		// reset the timeline
	UIVideoSourceUpdated();		// necessary because filters can be changed in capture mode

	DragAcceptFiles((HWND)mhwnd, TRUE);

	VDToolsAttach((HWND)mhwnd);

	return true;
}

void VDProjectUI::UpdateAccelMain() {
	HACCEL haccel = LoadAccelerators(g_hInst, MAKEINTRESOURCE(IDR_IDLE_KEYS));

	if (haccel)
		VDUIExtractAcceleratorTableW32(mAccelTableDefault, haccel, kCommandList.begin(), kCommandList.size());

	VDRegistryAppKey accelKey("Accelerators\\Main", false);

	bool success = false;

	if (accelKey.isReady()) {
		try {
			mAccelTableDef.Load(accelKey, kCommandList.begin(), kCommandList.size());
			success = true;
		} catch(const MyError&) {
			// eat the error
		}
	}

	if (!success)
		mAccelTableDef = mAccelTableDefault;

	VDUIUpdateMenuAcceleratorsW32(mhMenuNormal, mAccelTableDef);

	mhAccelMain = VDUIBuildAcceleratorTableW32(mAccelTableDef);

	VDUIFrame *pFrame = VDUIFrame::GetFrame((HWND)mhwnd);
	pFrame->SetAccelTable(mhAccelMain);
}

void VDProjectUI::UpdateAccelPreview() {
	if (mhAccelPreview)
		DestroyAcceleratorTable(mhAccelPreview);

	int merge_list[] = {
		ID_FILE_SAVEIMAGE,
		ID_VIDEO_COPYOUTPUTFRAME,
		ID_FILE_SAVEPROJECT,
		ID_VIDEO_FILTERS,
		ID_OPTIONS_SHOWPROFILER,

		ID_VIDEO_SEEK_NEXTSCENE,
		ID_VIDEO_SEEK_PREVSCENE,
		ID_VIDEO_SEEK_STOP,
		ID_VIDEO_SEEK_START,
		ID_VIDEO_SEEK_END,
		ID_VIDEO_SEEK_KEYNEXT,
		ID_VIDEO_SEEK_KEYPREV,
		ID_VIDEO_SEEK_NEXT,
		ID_VIDEO_SEEK_PREV,
		ID_VIDEO_SEEK_FNEXT,
		ID_VIDEO_SEEK_FPREV,
		ID_VIDEO_SEEK_FSNEXT,
		ID_VIDEO_SEEK_FSPREV,
		ID_VIDEO_SEEK_NEXTONESEC,
		ID_VIDEO_SEEK_PREVONESEC,
		ID_VIDEO_SEEK_SELSTART,
		ID_VIDEO_SEEK_SELEND,
		ID_VIDEO_SEEK_PREVDROP,
		ID_VIDEO_SEEK_NEXTDROP,

		ID_EDIT_JUMPTO,
		ID_EDIT_ZOOMRANGE,
		ID_EDIT_PREVRANGE,
		ID_EDIT_NEXTRANGE,
		ID_EDIT_SETMARKER,
		ID_EDIT_SETSELSTART,
		ID_EDIT_SETSELEND,
		ID_EDIT_CLEAR,
		ID_EDIT_SELECTALL,
	};

	HACCEL haccel = LoadAccelerators(g_hInst, MAKEINTRESOURCE(IDR_PREVIEW_KEYS));
	VDAccelTableDefinition def;
	VDUIMergeAcceleratorTableW32(def, haccel, merge_list, sizeof(merge_list)/sizeof(merge_list[0]), mAccelTableDef);
	mhAccelPreview = VDUIBuildAcceleratorTableW32(def);
}

void VDProjectUI::UpdateAccelDub() {
	if (mhAccelDub)
		DestroyAcceleratorTable(mhAccelDub);

	int merge_list[] = {
		ID_FILE_PREVIEWINPUT,
		ID_FILE_PREVIEWOUTPUT,
		ID_OPTIONS_DISPLAYINPUTVIDEO,
		ID_OPTIONS_DISPLAYOUTPUTVIDEO,
		ID_OPTIONS_SHOWLOG,
		ID_OPTIONS_SHOWPROFILER,

		ID_VIDEO_SEEK_NEXTSCENE,
		ID_VIDEO_SEEK_PREVSCENE,
		ID_VIDEO_SEEK_STOP,
		ID_VIDEO_SEEK_START,
		ID_VIDEO_SEEK_END,
		ID_VIDEO_SEEK_KEYNEXT,
		ID_VIDEO_SEEK_KEYPREV,
		ID_VIDEO_SEEK_NEXT,
		ID_VIDEO_SEEK_PREV,
		ID_VIDEO_SEEK_FNEXT,
		ID_VIDEO_SEEK_FPREV,
		ID_VIDEO_SEEK_FSNEXT,
		ID_VIDEO_SEEK_FSPREV,
		ID_VIDEO_SEEK_NEXTONESEC,
		ID_VIDEO_SEEK_PREVONESEC,
		ID_VIDEO_SEEK_SELSTART,
		ID_VIDEO_SEEK_SELEND,
		ID_VIDEO_SEEK_PREVDROP,
		ID_VIDEO_SEEK_NEXTDROP,

		ID_EDIT_JUMPTO,
		ID_EDIT_SETSELEND,
		ID_EDIT_SETSELSTART,
		ID_EDIT_PREVRANGE,
		ID_EDIT_NEXTRANGE,
		ID_EDIT_SETMARKER,

		ID_VIEW_FULLSCREEN,
	};

	HACCEL haccel = LoadAccelerators(g_hInst, MAKEINTRESOURCE(IDR_DUB_KEYS));
	VDAccelTableDefinition def;
	VDUIMergeAcceleratorTableW32(def, haccel, merge_list, sizeof(merge_list)/sizeof(merge_list[0]), mAccelTableDef);
	mhAccelDub = VDUIBuildAcceleratorTableW32(def);
}

void VDProjectUI::Detach() {
	VDToolsDetach((HWND)mhwnd);

	DragAcceptFiles((HWND)mhwnd, FALSE);

	SetUICallback(NULL);

	mpPosition = NULL;

	CloseAudioDisplay();
	CloseCurveEditor();

	if (mpUIPeer)
		mpUIPeer->Shutdown();

	mpUIPeer = NULL;
	mpUIBase = NULL;
	mpUIInputFrame = NULL;
	mpUIOutputFrame = NULL;

	if (mhwndCurveEditor) {
		mpCurveEditor = NULL;
		DestroyWindow(mhwndCurveEditor);
		mhwndCurveEditor = NULL;
	}

	if (mhwndStatus) {
		DestroyWindow(mhwndStatus);
		mhwndStatus = NULL;
	}

	if (mpInputDisplay) {
		if (mhwndInputFrame)
			VDGetIVideoWindow(mhwndInputFrame)->SetDisplay(NULL);
		mpInputDisplay->Destroy();
		mhwndInputDisplay = NULL;
		mpInputDisplay = NULL;
	}

	if (mpOutputDisplay) {
		if (mhwndOutputFrame)
			VDGetIVideoWindow(mhwndOutputFrame)->SetDisplay(NULL);
		mpOutputDisplay->Destroy();
		mhwndOutputDisplay = NULL;
		mpOutputDisplay = NULL;
	}

	if (mhwndInputFrame) {
		DestroyWindow(mhwndInputFrame);
		mhwndInputFrame = NULL;
	}

	if (mhwndOutputFrame) {
		DestroyWindow(mhwndOutputFrame);
		mhwndOutputFrame = NULL;
	}

	if (mhwndMaxDisplay) {
		max_dlg_node.Remove();
		DestroyWindow(mhwndMaxDisplay);
		mhwndMaxDisplay = NULL;
	}

	if (mhwndPosition) {
		DestroyWindow(mhwndPosition);
		mhwndPosition = NULL;
	}

	mpUIPeer = NULL;

	// Hmm... no destroy for accelerators.

	if (mhMenuDisplay) {
		DestroyMenu(mhMenuDisplay);
		mhMenuDisplay = NULL;
	}

	if (mhMenuExport) {
		DestroyMenu(mhMenuExport);
		mhMenuExport = NULL;
	}

	if (mhMenuTools) {
		DestroyMenu(mhMenuTools);
		mhMenuTools = NULL;
	}

	mhMenuSourceList = NULL;	// already destroyed via main menu

	if (mhMenuDub) {
		DestroyMenu(mhMenuDub);
		mhMenuDub = NULL;
	}

	if (mhMenuNormal) {
		DestroyMenu(mhMenuNormal);
		mhMenuNormal = NULL;
	}

	SaveSettings();

	if (mhwnd) {
		VDUIFrame *pFrame = VDUIFrame::GetFrame((HWND)mhwnd);

		pFrame->Detach();
	}

	extern void VDCloseProfileWindow();
	VDCloseProfileWindow();

	VDProject::Detach();
}

bool VDProjectUI::Tick() {
	bool activity = false;

	if (mpAudioDisplay)
		activity = TickAudioDisplay();

	if (mbPanesNeedUpdate) {
		mbPanesNeedUpdate = false;

		if (!mbDubActive)
			DisplayFrame();
	}

	if (!mPendingCommands.empty()) {
		PendingCommands::const_iterator it(mPendingCommands.begin()), itEnd(mPendingCommands.end());
		for(; it!=itEnd; ++it) {
			int id = *it;

			PostMessage((HWND)mhwnd, MYWM_DEFERRED_COMMAND, id, 0);
		}
		mPendingCommands.clear();
	}

	if (mPreviewRestartMode && !mbLockPreviewRestart) {
		PostMessage((HWND)mhwnd, MYWM_DEFERRED_PREVIEWRESTART, 0, 0);
	}

	return activity;
}

void VDProjectUI::SetTitle(int nTitleString, int nArgs, ...) {
	const void *args[16];

	VDASSERT(nArgs < 16);

	VDStringW versionW(VDLoadStringW32(IDS_TITLE_NOFILE, true));
	const wchar_t *pVersion = versionW.c_str();
	args[0] = &pVersion;

	va_list val;
	va_start(val, nArgs);
	for(int i=0; i<nArgs; ++i)
		args[i+1] = va_arg(val, const void *);
	va_end(val);

	const VDStringW title(VDaswprintf(VDLoadString(0, kVDST_ProjectUI, nTitleString), nArgs+1, args));

	if (GetVersion() < 0x80000000) {
		SetWindowTextW((HWND)mhwnd, title.c_str());
	} else {
		SetWindowTextA((HWND)mhwnd, VDTextWToA(title).c_str());
	}
}

void VDProjectUI::SetPaneLayout(PaneLayoutMode layout) {
	if (layout == mPaneLayoutMode)
		return;

	mPaneLayoutMode = layout;

	bool videoPresent = inputVideo != NULL;
	bool showInput = mPaneLayoutMode != kPaneLayoutOutput && videoPresent;
	bool showOutput = mPaneLayoutMode != kPaneLayoutInput && videoPresent;

	IVDVideoWindow *w1 = VDGetIVideoWindow(mhwndInputFrame);
	IVDVideoWindow *w2 = VDGetIVideoWindow(mhwndOutputFrame);
	if (!showInput && w1->IsFullscreen()) w1->ToggleFullscreen();
	if (!showOutput && w2->IsFullscreen()) w2->ToggleFullscreen();

	::ShowWindow(mhwndInputFrame, showInput);
	::ShowWindow(mhwndOutputFrame, showOutput);

	RepositionPanes(true);

	mpInputDisplay->Reset();
	mpOutputDisplay->Reset();

	RefreshInputPane();
	RefreshOutputPane();
}

bool VDProjectUI::IsInputPaneUsed() {
	if (!g_dubOpts.video.fShowInputFrame)
		return false;
	if (mPaneLayoutMode == kPaneLayoutOutput)
		return false;
	return true;
}

void VDProjectUI::OpenAsk() {
	OpenInput();
}

void VDProjectUI::AppendAsk() {
	if (!inputAVI)
		return;

	OpenInput(true);
}

void VDProjectUI::SaveAVIAsk(bool batchMode) {
	::SaveAVI((HWND)mhwnd, false, batchMode);
	JobUnlockDubber();
}

void VDProjectUI::SaveCompatibleAVIAsk(bool batchMode) {
	::SaveAVI((HWND)mhwnd, true, batchMode);
}

void VDProjectUI::SaveImageSequenceAsk(bool batchMode) {
	SaveImageSeq((HWND)mhwnd, batchMode);
}

void VDProjectUI::SaveImageAsk() {
	VDPosition frame = GetCurrentFrame();

	mLastDisplayedInputFrame = -1;
	mLastDisplayedTimelineFrame = -1;
	DisplayFrame();

	if (mpOutputDisplay) {
		if (filters.isRunning() && mpCurrentOutputFrame) {
			VDFilterFrameBuffer *buf = mpCurrentOutputFrame->GetResultBuffer();
			VDPixmap px = VDPixmapFromLayout(filters.GetOutputLayout(), (void *)buf->LockRead());
			px.info = buf->info;
			buf->Unlock();

			SaveImage((HWND)mhwnd, frame, &px);
		}
	} else if (mpInputDisplay) {
		if (inputVideo && mpCurrentInputFrame) {
			VDFilterFrameBuffer *buf = mpCurrentInputFrame->GetResultBuffer();
			VDPixmap px = VDPixmapFromLayout(mpVideoFrameSource->GetOutputLayout(), (void *)buf->LockRead());
			px.info = buf->info;
			buf->Unlock();

			SaveImage((HWND)mhwnd, frame, &px);
		}
	}
}

void VDProjectUI::SaveSegmentedAVIAsk(bool batchMode) {
	SaveSegmentedAVI((HWND)mhwnd, batchMode);
}

void VDProjectUI::SaveWAVAsk(bool batchMode) {
	if (!inputAudio)
		throw MyError("No input audio stream to extract.");

	static const wchar_t kWAVFileFilters[]=
			L"Windows audio (*.wav)\0"					L"*.wav\0"
			L"All files (*.*)\0"						L"*.*\0"
			;

	const VDStringW filename(VDGetSaveFileName(kFileDialog_WAVAudioOut, mhwnd, L"Save WAV File", kWAVFileFilters, g_prefs.main.fAttachExtension ? L"wav" : NULL));

	if (!filename.empty()) {
		if (batchMode)
			JobAddConfigurationSaveAudio(this, &g_dubOpts, g_szInputAVIFile, mInputDriverName.c_str(), inputAVI->GetFileFlags(), &inputAVI->listFiles, filename.c_str(), false, true);
		else
			SaveWAV(filename.c_str());
	}
}

void VDProjectUI::SaveFilmstripAsk() {
	if (!inputVideo)
		throw MyError("No input video stream to process.");

	const VDStringW filename(VDGetSaveFileName(kFileDialog_FLMOut, mhwnd, L"Save Filmstrip file", L"Adobe Filmstrip (*.flm)\0*.flm\0", g_prefs.main.fAttachExtension ? L"flm" : NULL));
	if (!filename.empty()) {
		SaveFilmstrip(filename.c_str(), false);
	}
}

class ProjectState: public IProjectState {
	virtual bool GetSelection(sint64& start, sint64& end) {
		start = g_project->GetSelectionStartFrame();
		end = g_project->GetSelectionEndFrame();
		if(end==-1) return false;
		return true;
	}
};

void VDProjectUI::ExportViaDriverTool(const char* name) {
	int api_version = inputAVI->GetInputDriverApiVersion();
	if (api_version<6) return;
	IFilterModFileTool* tool;
	inputAVI->GetFileTool(&tool);
	if (tool) {
		ProjectState state;
		{for(int id=0;;id++) {
			char name2[128];
			if (!tool->GetExportCommandName(id,name2,128)) break;
			if (strcmp(name,name2)==0) {
				tool->ExecuteExport(id,(VDXHWND)mhwnd,&state);
				break;
			}
		}}
		tool->Release();
	}
}

void VDProjectUI::ExportViaDriverTool(int id) {
	IFilterModFileTool* tool;
	inputAVI->GetFileTool(&tool);
	if (tool) {
		ProjectState state;
		tool->ExecuteExport(id,(VDXHWND)mhwnd,&state);
		tool->Release();
	}
}

namespace {
	class VDOutputFileAnimatedGIFOptionsDialog : public VDDialogBase {
	public:
		VDStringW mFileName;
		int mLoopCount;

	public:
		VDOutputFileAnimatedGIFOptionsDialog() : mLoopCount(0) {}

		bool HandleUIEvent(IVDUIBase *pBase, IVDUIWindow *pWin, uint32 id, eEventType type, int item) {
			if (type == kEventAttach) {
				mpBase = pBase;
				SetCaption(100, VDGetLastLoadSavePath(kFileDialog_GIFOut).c_str());

				VDRegistryAppKey appKey("Persistence");
				int loopCount = appKey.getInt("AnimGIF: Loop count", 0);

				SetValue(200, loopCount == 1 ? 0 : loopCount == 0 ? 1 : 2);

				int loopCountValue = loopCount < 2 ? 2 : loopCount;
				SetCaption(101, VDswprintf(L"%d", 1, &loopCountValue).c_str());

				pBase->ExecuteAllLinks();
			} else if (type == kEventSelect) {
				if (id == 10) {
					mFileName = GetCaption(100);

					int loopMode = GetValue(200);
					if (loopMode == 0)
						mLoopCount = 1;
					else if (loopMode == 1)
						mLoopCount = 0;
					else {
						const VDStringW caption(GetCaption(101));

						unsigned loops;
						if (1 != swscanf(caption.c_str(), L" %u", &loops)) {
							mpBase->GetControl(101)->SetFocus();
							::MessageBeep(MB_ICONEXCLAMATION);
							return true;
						}

						mLoopCount = loops;
					}

					VDRegistryAppKey appKey("Persistence");
					appKey.setInt("AnimGIF: Loop count", mLoopCount);

					pBase->EndModal(true);
					return true;
				} else if (id == 11) {
					pBase->EndModal(false);
					return true;
				} else if (id == 300) {
					const VDStringW filename(VDGetSaveFileName(kFileDialog_GIFOut, (VDGUIHandle)vdpoly_cast<IVDUIWindowW32 *>(pBase)->GetHandleW32(), L"Save animated GIF", L"Animated GIF (*.gif)\0*.gif\0", g_prefs.main.fAttachExtension ? L"gif" : NULL));

					if (!filename.empty())
						SetCaption(100, filename.c_str());
				}
			}
			return false;
		}
	};
}

namespace {
	class VDOutputFileAnimatedPNGOptionsDialog : public VDDialogBase {
	public:
		VDStringW mFileName;
		int mLoopCount;
		int mAlpha;
		int mGrayscale;

	public:
		VDOutputFileAnimatedPNGOptionsDialog() : mLoopCount(0), mAlpha(0) , mGrayscale(0) {}

		bool HandleUIEvent(IVDUIBase *pBase, IVDUIWindow *pWin, uint32 id, eEventType type, int item) {
			if (type == kEventAttach) {
				mpBase = pBase;
				SetCaption(100, VDGetLastLoadSavePath(kFileDialog_PNGOut).c_str());

				VDRegistryAppKey appKey("Persistence");
				int loopCount = appKey.getInt("AnimPNG: Loop count", 0);

				SetValue(200, loopCount == 1 ? 0 : loopCount == 0 ? 1 : 2);

				int loopCountValue = loopCount < 2 ? 2 : loopCount;
				SetCaption(101, VDswprintf(L"%d", 1, &loopCountValue).c_str());

				int alpha = appKey.getInt("AnimPNG: Alpha channel", 0);
				SetValue(400, alpha);

				int grayscale = appKey.getInt("AnimPNG: Grayscale", 0);
				SetValue(500, grayscale);

				pBase->ExecuteAllLinks();
			} else if (type == kEventSelect) {
				if (id == 10) {
					mFileName = GetCaption(100);

					int loopMode = GetValue(200);
					if (loopMode == 0)
						mLoopCount = 1;
					else if (loopMode == 1)
						mLoopCount = 0;
					else {
						const VDStringW caption(GetCaption(101));

						unsigned loops;
						if (1 != swscanf(caption.c_str(), L" %u", &loops)) {
							mpBase->GetControl(101)->SetFocus();
							::MessageBeep(MB_ICONEXCLAMATION);
							return true;
						}

						mLoopCount = loops;
					}

					mAlpha = GetValue(400);
					mGrayscale = GetValue(500);

					VDRegistryAppKey appKey("Persistence");
					appKey.setInt("AnimPNG: Loop count", mLoopCount);
					appKey.setInt("AnimPNG: Alpha channel", mAlpha);
					appKey.setInt("AnimPNG: Grayscale", mGrayscale);

					pBase->EndModal(true);
					return true;
				} else if (id == 11) {
					pBase->EndModal(false);
					return true;
				} else if (id == 300) {
					const VDStringW filename(VDGetSaveFileName(kFileDialog_PNGOut, (VDGUIHandle)vdpoly_cast<IVDUIWindowW32 *>(pBase)->GetHandleW32(), L"Save animated PNG", L"Animated PNG (*.png)\0*.png\0", g_prefs.main.fAttachExtension ? L"png" : NULL));

					if (!filename.empty())
						SetCaption(100, filename.c_str());
				}
			}
			return false;
		}
	};
}

void VDProjectUI::SaveAnimatedGIFAsk() {
	if (!inputVideo)
		throw MyError("No input video stream to process.");

	vdautoptr<IVDUIWindow> peer(VDUICreatePeer(mhwnd));

	IVDUIWindow *pWin = VDCreateDialogFromResource(3001, peer);
	VDOutputFileAnimatedGIFOptionsDialog dlg;

	IVDUIBase *pBase = vdpoly_cast<IVDUIBase *>(pWin);
	
	pBase->SetCallback(&dlg, false);
	int result = pBase->DoModal();

	peer->Shutdown();

	if (result) {
		SaveAnimatedGIF(dlg.mFileName.c_str(), dlg.mLoopCount, false);
	}
}

void VDProjectUI::SaveAnimatedPNGAsk() {
	if (!inputVideo)
		throw MyError("No input video stream to process.");

	vdautoptr<IVDUIWindow> peer(VDUICreatePeer(mhwnd));

	IVDUIWindow *pWin = VDCreateDialogFromResource(3002, peer);
	VDOutputFileAnimatedPNGOptionsDialog dlg;

	IVDUIBase *pBase = vdpoly_cast<IVDUIBase *>(pWin);

	pBase->SetCallback(&dlg, false);
	int result = pBase->DoModal();

	peer->Shutdown();

	if (result) {
		SaveAnimatedPNG(dlg.mFileName.c_str(), dlg.mLoopCount, dlg.mAlpha, dlg.mGrayscale, false);
	}
}

void VDProjectUI::SaveRawAudioAsk(bool batchMode) {
	if (!inputAudio)
		throw MyError("No input audio stream to extract.");

	const VDStringW filename(VDGetSaveFileName(kFileDialog_RawAudioOut, mhwnd, L"Save raw audio", L"All types\0*.bin;*.mp3\0Raw audio (*.bin)\0*.bin\0MPEG layer III audio (*.mp3)\0*.mp3\0", NULL));
	if (!filename.empty()) {
		if (batchMode)
			JobAddConfigurationSaveAudio(this, &g_dubOpts, g_szInputAVIFile, mInputDriverName.c_str(), inputAVI->GetFileFlags(), &inputAVI->listFiles, filename.c_str(), true, true);
		else
			SaveRawAudio(filename.c_str(), false);
	}
}

class VDUIDialogSaveRawVideoFormat : public VDDialogFrameW32 {
public:
	VDUIDialogSaveRawVideoFormat(VDAVIOutputRawVideoFormat& format);

protected:
	virtual bool OnLoaded();
	virtual void OnDataExchange(bool write);

	VDAVIOutputRawVideoFormat& mFormat;

	static const int kFormats[];
};

const int VDUIDialogSaveRawVideoFormat::kFormats[] = {
	nsVDPixmap::kPixFormat_XRGB1555,
	nsVDPixmap::kPixFormat_RGB565,
	nsVDPixmap::kPixFormat_RGB888,
	nsVDPixmap::kPixFormat_XRGB8888,
	nsVDPixmap::kPixFormat_Y8,
	nsVDPixmap::kPixFormat_Y8_FR,
	nsVDPixmap::kPixFormat_YUV422_UYVY,
	nsVDPixmap::kPixFormat_YUV422_UYVY_709,
	nsVDPixmap::kPixFormat_YUV422_UYVY_FR,
	nsVDPixmap::kPixFormat_YUV422_UYVY_709_FR,
	nsVDPixmap::kPixFormat_YUV422_YUYV,
	nsVDPixmap::kPixFormat_YUV422_YUYV_709,
	nsVDPixmap::kPixFormat_YUV422_YUYV_FR,
	nsVDPixmap::kPixFormat_YUV422_YUYV_709_FR,
	nsVDPixmap::kPixFormat_YUV444_Planar,
	nsVDPixmap::kPixFormat_YUV444_Planar_FR,
	nsVDPixmap::kPixFormat_YUV444_Planar_709,
	nsVDPixmap::kPixFormat_YUV444_Planar_709_FR,
	nsVDPixmap::kPixFormat_YUV422_Planar,
	nsVDPixmap::kPixFormat_YUV422_Planar_FR,
	nsVDPixmap::kPixFormat_YUV422_Planar_709,
	nsVDPixmap::kPixFormat_YUV422_Planar_709_FR,
	nsVDPixmap::kPixFormat_YUV422_Planar_Centered,
	nsVDPixmap::kPixFormat_YUV420_Planar,
	nsVDPixmap::kPixFormat_YUV420_Planar_FR,
	nsVDPixmap::kPixFormat_YUV420_Planar_709,
	nsVDPixmap::kPixFormat_YUV420_Planar_709_FR,
	nsVDPixmap::kPixFormat_YUV420_Planar_Centered,
	nsVDPixmap::kPixFormat_YUV411_Planar,
	nsVDPixmap::kPixFormat_YUV411_Planar_FR,
	nsVDPixmap::kPixFormat_YUV411_Planar_709,
	nsVDPixmap::kPixFormat_YUV411_Planar_709_FR,
	nsVDPixmap::kPixFormat_YUV410_Planar,
	nsVDPixmap::kPixFormat_YUV410_Planar_FR,
	nsVDPixmap::kPixFormat_YUV410_Planar_709,
	nsVDPixmap::kPixFormat_YUV410_Planar_709_FR
};

VDUIDialogSaveRawVideoFormat::VDUIDialogSaveRawVideoFormat(VDAVIOutputRawVideoFormat& format)
	: VDDialogFrameW32(IDD_OUTPUT_RAWVIDEO)
	, mFormat(format)
{
}

bool VDUIDialogSaveRawVideoFormat::OnLoaded() {
	for(size_t i=0; i<sizeof(kFormats)/sizeof(kFormats[0]); ++i) {
		int format = kFormats[i];

		CBAddString(IDC_OUTPUT_FORMAT, VDLoadString(0, kVDST_RawVideoFormats, format));
	}

	SetFocusToControl(IDC_OUTPUT_FORMAT);
	VDDialogFrameW32::OnLoaded();
	return true;
}

void VDUIDialogSaveRawVideoFormat::OnDataExchange(bool write) {
	if (write) {
		uint32 c = GetControlValueUint32(IDC_ALIGNMENT);
		if (!c || c > 64 || (c & (c-1)) || mbValidationFailed) {
			FailValidation(IDC_ALIGNMENT);
			return;
		}

		mFormat.mScanlineAlignment = c;
		mFormat.mOutputFormat = kFormats[CBGetSelectedIndex(IDC_OUTPUT_FORMAT)];
		mFormat.mbSwapChromaPlanes = IsButtonChecked(IDC_PLANEORDER_CRCB);
		mFormat.mbBottomUp = IsButtonChecked(IDC_VORIENT_BOTTOMUP);
	} else {
		const int *p = std::find(kFormats, kFormats + sizeof(kFormats)/sizeof(kFormats[0]), mFormat.mOutputFormat);
		if (p == kFormats + sizeof(kFormats)/sizeof(kFormats[0]))
			CBSetSelectedIndex(IDC_OUTPUT_FORMAT, 0);
		else
			CBSetSelectedIndex(IDC_OUTPUT_FORMAT, p - kFormats);

		CheckButton(IDC_PLANEORDER_CBCR, !mFormat.mbSwapChromaPlanes);
		CheckButton(IDC_PLANEORDER_CRCB, mFormat.mbSwapChromaPlanes);

		SetControlTextF(IDC_ALIGNMENT, L"%u", mFormat.mScanlineAlignment);

		CheckButton(IDC_VORIENT_TOPDOWN, !mFormat.mbBottomUp);
		CheckButton(IDC_VORIENT_BOTTOMUP, mFormat.mbBottomUp);
	}
}

void VDProjectUI::SaveRawVideoAsk(bool batchMode) {
	if (!inputVideo)
		throw MyError("No video stream to export.");

	VDRegistryAppKey key(g_szRegKeyPersistence);
	VDAVIOutputRawVideoFormat format = {};
	format.mOutputFormat = key.getInt("Export raw video: Output format", nsVDPixmap::kPixFormat_YUV420_Planar);
	format.mScanlineAlignment = key.getInt("Export raw video: Scanline alignment", 4);
	format.mbSwapChromaPlanes = key.getBool("Export raw video: Swap chroma planes", true);
	format.mbBottomUp = key.getBool("Export raw video: Use bottom-up orientation", false);

	VDUIDialogSaveRawVideoFormat dlg(format);
	if (!dlg.ShowDialog((VDGUIHandle)mhwnd))
		return;

	key.setInt("Export raw video: Output format", format.mOutputFormat);
	key.setInt("Export raw video: Scanline alignment", format.mScanlineAlignment);
	key.setBool("Export raw video: Swap chroma planes", format.mbSwapChromaPlanes);
	key.setBool("Export raw video: Use bottom-up orientation", format.mbBottomUp);

	const VDStringW filename(VDGetSaveFileName(kFileDialog_RawVideoOut, mhwnd, L"Save raw video", L"All types\0*.bin\0Raw YUV (*.yuv)\0*.yuv\0", NULL));
	if (!filename.empty()) {
		if (batchMode)
			JobAddConfigurationSaveVideo(this, &g_dubOpts, g_szInputAVIFile, mInputDriverName.c_str(), inputAVI->GetFileFlags(), &inputAVI->listFiles, filename.c_str(), true, format);
		else
			SaveRawVideo(filename.c_str(), format, false);
	}
}

class VDUIDialogExportViaEncoder : public VDDialogFrameW32 {
public:
	VDUIDialogExportViaEncoder();

	void SetSelectedSet(VDExtEncSet *eset) { mpSelectedSet = eset; }
	VDExtEncSet *GetSelectedSet() const { return mpSelectedSet; }

protected:
	bool OnLoaded();
	void OnDestroy();
	void OnDataExchange(bool write);

	vdrefptr<VDExtEncSet> mpSelectedSet;
	typedef vdfastvector<VDExtEncSet *> Sets;
	Sets mSets;

	struct SetSortByName {
		bool operator()(const VDExtEncSet *x, const VDExtEncSet *y) const {
			return vdwcsicmp(x->mName.c_str(), y->mName.c_str()) < 0;
		}
	};
};

VDUIDialogExportViaEncoder::VDUIDialogExportViaEncoder()
	: VDDialogFrameW32(IDD_OUTPUT_CLI)
{
}

bool VDUIDialogExportViaEncoder::OnLoaded() {
	uint32 n = VDGetExternalEncoderSetCount();
	for(uint32 i=0; i<n; ++i) {
		vdrefptr<VDExtEncSet> eset;
		if (VDGetExternalEncoderSetByIndex(i, ~eset)) {
			mSets.push_back(eset);
			eset.release();
		}
	}

	std::sort(mSets.begin(), mSets.end(), SetSortByName());

	for(uint32 i=0; i<n; ++i) {
		VDExtEncSet *eset = mSets[i];

		LBAddString(IDC_LIST, eset->mName.c_str());
	}

	OnDataExchange(false);
	SetFocusToControl(IDC_LIST);
	return true;
}

void VDUIDialogExportViaEncoder::OnDestroy() {
	while(!mSets.empty()) {
		mSets.back()->Release();
		mSets.pop_back();
	}
}

void VDUIDialogExportViaEncoder::OnDataExchange(bool write) {
	if (write) {
		int idx = LBGetSelectedIndex(IDC_LIST);

		if ((unsigned)idx < mSets.size())
			mpSelectedSet = mSets[idx];
		else
			mpSelectedSet = NULL;
	} else {
		Sets::iterator it(std::find(mSets.begin(), mSets.end(), mpSelectedSet));
		if (it != mSets.end())
			LBSetSelectedIndex(IDC_LIST, it - mSets.begin());
		else
			LBSetSelectedIndex(IDC_LIST, -1);
	}
}

bool VDGetExternalEncoderSetAsk(HWND parent, VDExtEncSet **pp) {
	VDRegistryAppKey key(g_szRegKeyPersistence);
	
	VDUIDialogExportViaEncoder dlg;

	VDStringW encName;
	if (key.getString("CLI Export: Encoder set", encName)) {
		vdrefptr<VDExtEncSet> eset;

		if (VDGetExternalEncoderSetByName(encName.c_str(), ~eset))
			dlg.SetSelectedSet(eset);
	}

	if (!dlg.ShowDialog((VDGUIHandle)parent))
		return false;

	VDExtEncSet *eset = dlg.GetSelectedSet();
	if (eset) {
		key.setString("CLI Export: Encoder set", eset->mName.c_str());
		*pp = eset;
		eset->AddRef();
		return true;
	}

	return false;
}

void VDProjectUI::ExportViaEncoderAsk(bool batch) {
	if (!inputVideo)
		throw MyError("No input video stream to process.");

	vdrefptr<VDExtEncSet> eset;
	if (!VDGetExternalEncoderSetAsk((HWND)mhwnd, ~eset)) return;

	VDStringW filterSpec;
	const wchar_t *ext = NULL;

	if (!eset->mFileDesc.empty())
		filterSpec += eset->mFileDesc;
	else {
		filterSpec += L"Output file (*.";
		if (!eset->mFileExt.empty())
			filterSpec += eset->mFileExt;
		else
			filterSpec += L"*";
		filterSpec += L")";
	}

	filterSpec += (wchar_t)0;

	filterSpec += L"*.";
	if (!eset->mFileExt.empty()) {
		filterSpec += eset->mFileExt;
		ext = eset->mFileExt.c_str();
	} else
		filterSpec += L"*";

	filterSpec += (wchar_t)0;
	
	const VDStringW filename(VDGetSaveFileName(kFileDialog_ExtOut, mhwnd, L"Export via external encoder", filterSpec.c_str(), ext));
	if (!filename.empty()) {
		if (batch)
			JobAddConfigurationExportViaEncoder(this, &g_dubOpts, g_szInputAVIFile, mInputDriverName.c_str(), inputAVI->GetFileFlags(), &inputAVI->listFiles, filename.c_str(), true, eset->mName.c_str());
		else
			ExportViaEncoder(filename.c_str(), eset->mName.c_str(), false);
	}
}

void VDProjectUI::SaveConfigurationAsk() {
	SaveConfiguration((HWND)mhwnd);
}

void VDProjectUI::LoadConfigurationAsk() {
	try {
		RunScript(NULL, (void *)mhwnd);
	} catch(const MyError& e) {
		e.post((HWND)mhwnd, g_szError);
	}
}

void VDProjectUI::LoadProjectAsk() {
	try {
		RunProject(NULL, (void *)mhwnd);
	} catch(const MyError& e) {
		e.post((HWND)mhwnd, g_szError);
	}
}

void VDProjectUI::SetVideoFiltersAsk() {
	VDPosition initialTime = -1;
	
	if (mVideoTimelineFrameRate.getLo() && mVideoTimelineFrameRate.getHi())
		initialTime = mVideoTimelineFrameRate.scale64ir(GetCurrentFrame()*1000000);

	int edit_instance = -1;
	if (edit_token){
		int v;
		if(swscanf(edit_token,L"%d",&v)) edit_instance = v;
	}

	LockFilterChain(true);
	VDVideoFiltersDialogResult result = VDShowDialogVideoFilters(mhwnd, inputVideo, initialTime, edit_instance, &mhwndFilters);
	LockFilterChain(false);

	if (result.mbDialogAccepted && result.mbRescaleRequested) {
		AdjustTimelineForFilterChanges(result.mOldFrameRate, result.mOldFrameCount, result.mNewFrameRate, result.mNewFrameCount);
	}

	UpdateDubParameters();
	UpdateFilterList();
}

void VDProjectUI::SetVideoFramerateOptionsAsk() {
	extern bool VDDisplayVideoFrameRateDialog(VDGUIHandle hParent, DubOptions& opts, IVDVideoSource *pVS, AudioSource *pAS);

	if (VDDisplayVideoFrameRateDialog(mhwnd, g_dubOpts, inputVideo, inputAudio))
		UpdateDubParameters();
}

void VDProjectUI::SetVideoDepthOptionsAsk() {
	bool changed = VDDisplayVideoDepthDialog(mhwnd, g_dubOpts.video.mInputFormat, DepthDialog_input, -1, 0);
	if (changed && inputVideo) {
		StopFilters();
		VDRenderSetVideoSourceInputFormat(inputVideo, g_dubOpts.video.mInputFormat);
		mLastDisplayedInputFrame = -1;
		mLastDisplayedTimelineFrame = -1;
		DisplayFrame();
	}
}

void VDProjectUI::SetVideoRangeOptionsAsk() {
	extern bool VDDisplayVideoRangeDialog(VDGUIHandle hParent, DubOptions& opts, const VDFraction& frameRate, VDPosition frameCount, VDPosition& startSel, VDPosition& endSel);

	if (inputVideo) {
		UpdateTimelineRate();

		VDPosition len = mTimeline.GetLength();
		VDPosition startSel = 0;
		VDPosition endSel = len;

		if (IsSelectionPresent()) {
			startSel = GetSelectionStartFrame();
			endSel = GetSelectionEndFrame();
		}

		if (VDDisplayVideoRangeDialog(mhwnd, g_dubOpts, mVideoTimelineFrameRate, len, startSel, endSel)) {
			SetSelection(startSel, endSel);
		}
	}
}

void VDProjectUI::SetVideoCompressionAsk() {
	if (!(g_Vcompression.dwFlags & ICMF_COMPVARS_VALID)) {
		g_Vcompression.clear();
		g_Vcompression.dwFlags |= ICMF_COMPVARS_VALID;
		g_Vcompression.lQ = 10000;
	}

	g_Vcompression.cbSize = sizeof(COMPVARS);

	ChooseCompressor((HWND)mhwnd, &g_Vcompression);
}

void VDProjectUI::SetVideoErrorModeAsk() {
	extern DubSource::ErrorMode VDDisplayErrorModeDialog(VDGUIHandle hParent, IVDStreamSource::ErrorMode oldMode, const char *pszSettingsKey, IVDStreamSource *pSource);
	g_videoErrorMode = VDDisplayErrorModeDialog(mhwnd, g_videoErrorMode, "Edit: Video error mode", inputVideo ? inputVideo->asStream() : NULL);

	if (inputVideo)
		inputVideo->asStream()->setDecodeErrorMode(g_videoErrorMode);
}

void VDProjectUI::SetAudioFiltersAsk() {
	extern void VDDisplayAudioFilterDialog(VDGUIHandle, VDAudioFilterGraph&, AudioSource *pAS);
	VDDisplayAudioFilterDialog(mhwnd, g_audioFilterGraph, inputAudio);
}

void VDProjectUI::SetAudioConversionOptionsAsk() {
	extern bool VDDisplayAudioConversionDialog(VDGUIHandle hParent, DubOptions& opts, AudioSource *pSource);
	if (VDDisplayAudioConversionDialog(mhwnd, g_dubOpts, inputAudio)) {
		SetAudioSource();
		UIAudioSourceUpdated();
	}
}

void VDProjectUI::SetAudioInterleaveOptionsAsk() {
	ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_INTERLEAVE), (HWND)mhwnd, AudioInterleaveDlgProc);
}

void VDProjectUI::SetAudioCompressionAsk(HWND parent) {
	if (!inputAudio)
		g_ACompressionFormat = (VDWaveFormat *)AudioChooseCompressor(parent, (WAVEFORMATEX *)g_ACompressionFormat, NULL, g_ACompressionFormatHint, g_ACompressionConfig);
	else {

		WAVEFORMATEX wfex = {0};

		const VDWaveFormat* inputFormat = inputAudio->getWaveFormat();
		memcpy(&wfex, inputFormat, sizeof(PCMWAVEFORMAT));

		if (is_audio_float(inputFormat)) {
			wfex.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
		} else {
			wfex.wFormatTag = WAVE_FORMAT_PCM;

			// Say 16-bit if the source was compressed.
			if (!is_audio_pcm(inputFormat))
				wfex.wBitsPerSample = 16;
		}

		switch(g_dubOpts.audio.newPrecision) {
		case DubAudioOptions::P_8BIT:	wfex.wBitsPerSample = 8; break;
		case DubAudioOptions::P_16BIT:	wfex.wBitsPerSample = 16; break;
		}

		switch(g_dubOpts.audio.newChannels) {
		case DubAudioOptions::C_MONO:	wfex.nChannels = 1; break;
		case DubAudioOptions::C_MONOLEFT:	wfex.nChannels = 1; break;
		case DubAudioOptions::C_MONORIGHT:	wfex.nChannels = 1; break;
		case DubAudioOptions::C_STEREO:	wfex.nChannels = 2; break;
		}

		if (g_dubOpts.audio.new_rate) {
			long samp_frac;

			samp_frac = MulDiv(wfex.nSamplesPerSec, 0x80000L, g_dubOpts.audio.new_rate);

			wfex.nSamplesPerSec = MulDiv(wfex.nSamplesPerSec, 0x80000L, samp_frac);
		}

		wfex.nBlockAlign		= (WORD)((wfex.wBitsPerSample+7)/8 * wfex.nChannels);
		wfex.nAvgBytesPerSec	= wfex.nSamplesPerSec * wfex.nBlockAlign;

		g_ACompressionFormat = (VDWaveFormat *)AudioChooseCompressor(parent, (WAVEFORMATEX *)g_ACompressionFormat, &wfex, g_ACompressionFormatHint, g_ACompressionConfig);

	}

	if (g_ACompressionFormat) {
		g_ACompressionFormatSize = sizeof(VDWaveFormat) + g_ACompressionFormat->mExtraSize;
	}
}

void VDProjectUI::SetAudioVolumeOptionsAsk() {
	bool VDDisplayAudioVolumeDialog(VDGUIHandle hParent, DubOptions& opts);

	VDDisplayAudioVolumeDialog(mhwnd, g_dubOpts);
}

void VDProjectUI::SetAudioSourceWAVAsk() {
	OpenInput(false, true);
}

void VDProjectUI::SetAudioErrorModeAsk() {
	extern DubSource::ErrorMode VDDisplayErrorModeDialog(VDGUIHandle hParent, IVDStreamSource::ErrorMode oldMode, const char *pszSettingsKey, IVDStreamSource *pSource);
	g_audioErrorMode = VDDisplayErrorModeDialog(mhwnd, g_audioErrorMode, "Edit: Audio error mode", inputAudio);

	SetAudioErrorMode(g_audioErrorMode);
}

void VDProjectUI::JumpToFrameAsk() {
	if (inputAVI) {
		extern VDPosition VDDisplayJumpToPositionDialog(VDGUIHandle hParent, VDPosition currentFrame, IVDVideoSource *pVS, const VDFraction& trueRate);

		VDPosition pos = VDDisplayJumpToPositionDialog(mhwnd, GetCurrentFrame(), inputVideo, mVideoInputFrameRate);

		if (pos >= 0) {
			if (mbZoomEnabled && ((pos<mposZoomStart) || (pos>mposZoomEnd))) ToggleZoomRange();
			MoveToFrame(pos);
		}
	}
}

void VDProjectUI::OpenNewInstance() {
	VDLaunchProgram(VDGetProgramFilePath().c_str());
}

void VDProjectUI::CloseAndDelete() {
	if (!inputAVI || !VDDoesPathExist(g_szInputAVIFile))
		return;

	VDStringA s;
	s.sprintf("Are you sure you want to delete the file \"%ls\"?", g_szInputAVIFile);

	if (IDOK == MessageBoxA((HWND)mhwnd, s.c_str(), g_szWarning, MB_ICONEXCLAMATION | MB_OKCANCEL)) {
		VDStringW fn(g_szInputAVIFile);

		Close();

		VDRemoveFile(fn.c_str());
	}
}

void VDProjectUI::OpenPrevious() {
	OpenPreviousNext(false);
}

void VDProjectUI::OpenNext() {
	OpenPreviousNext(true);
}

void VDProjectUI::OpenPreviousNext(bool next) {
	typedef vdvector<VDStringW> Patterns;
	Patterns patterns;
	VDGetInputDriverFilePatterns(IVDInputDriver::kF_Video, patterns);

	VDStringW fileToOpen;
	VDStringW bestFileName;

	const wchar_t *fnref = VDFileSplitPath(g_szInputAVIFile);
	VDDirectoryIterator it(VDMakePath(VDFileSplitPathLeft(VDStringW(g_szInputAVIFile)).c_str(), L"*.*").c_str());
	while(it.Next()) {
		bool matched = false;

		if (it.IsDirectory())
			continue;

		const wchar_t *fn = it.GetName();
		for(Patterns::const_iterator itPat(patterns.begin()), itPatEnd(patterns.end());
			itPat != itPatEnd;
			++itPat)
		{
			if (VDFileWildMatch(itPat->c_str(), fn)) {
				matched = true;
				break;
			}
		}

		if (matched) {
			if (next) {
				// Look for least file > current
				if (vdwcsicmp(fn, fnref) > 0 && (bestFileName.empty() || vdwcsicmp(fn, bestFileName.c_str()) < 0)) {
					fileToOpen = it.GetFullPath();
					bestFileName = fn;
				}
			} else {
				// Look for greatest file < current
				if (vdwcsicmp(fn, fnref) < 0 && (bestFileName.empty() || vdwcsicmp(fn, bestFileName.c_str()) > 0)) {
					fileToOpen = it.GetFullPath();
					bestFileName = fn;
				}
			}
		}
	}

	if (!fileToOpen.empty())
		Open(fileToOpen.c_str());
}

bool VDProjectUI::QueueCommand(int cmd) {
	if (g_dubber) {
		if (!g_dubber->IsPreviewing())
			return false;

		switch(cmd) {
		case kVDProjectCmd_GoToStart:
		case kVDProjectCmd_GoToEnd:
		case kVDProjectCmd_GoToSelectionStart:
		case kVDProjectCmd_GoToSelectionEnd:
		case kVDProjectCmd_ScrubBegin:
		case kVDProjectCmd_ScrubEnd:
		case kVDProjectCmd_ScrubUpdate:
		case kVDProjectCmd_GoToNextUnit:
		case kVDProjectCmd_GoToNextKey:
		case kVDProjectCmd_GoToNextRange:
		case kVDProjectCmd_ZoomRange:
			// allow restart
			g_dubber->Abort(false);
			SetPositionCallbackEnabled(false);
			break;
		default:
			AbortPreviewing();
			break;
		}

		mPreviewRestartPosition = mpPosition->GetPosition();
		mPendingCommands.push_back(cmd);
	} else {
		SceneShuttleStop();
		ExecuteCommand(cmd);
	}

	return true;
}

void VDProjectUI::ExecuteCommand(int cmd) {
	switch(cmd) {
		case kVDProjectCmd_GoToAsk:
			JumpToFrameAsk();
			break;
		case kVDProjectCmd_GoToStart:
			MoveToStart();
			break;
		case kVDProjectCmd_GoToEnd:
			MoveToEnd();
			break;
		case kVDProjectCmd_GoToPrevFrame:
			MoveToPrevious();
			RedrawWindow(mhwndPosition,0,0,RDW_UPDATENOW);
			break;
		case kVDProjectCmd_GoToNextFrame:
			MoveToNext();
			RedrawWindow(mhwndPosition,0,0,RDW_UPDATENOW);
			break;
		case kVDProjectCmd_GoToPrevUnit:
			MoveBackSome();
			break;
		case kVDProjectCmd_GoToNextUnit:
			MoveForwardSome();
			break;
		case kVDProjectCmd_GoToPrevKey:
			MoveToPreviousKey();
			break;
		case kVDProjectCmd_GoToNextKey:
			MoveToNextKey();
			break;
		case kVDProjectCmd_GoToPrevDrop:
			MoveToPreviousDrop();
			break;
		case kVDProjectCmd_GoToNextDrop:
			MoveToNextDrop();
			break;
		case kVDProjectCmd_GoToPrevRange:
			MoveToPreviousRange();
			break;
		case kVDProjectCmd_GoToNextRange:
			MoveToNextRange();
			break;
		case kVDProjectCmd_GoToSelectionStart:
			MoveToSelectionStart();
			break;
		case kVDProjectCmd_GoToSelectionEnd:
			MoveToSelectionEnd();
			break;
		case kVDProjectCmd_ScrubBegin:
			OnPositionNotify(PCN_BEGINTRACK);
			break;
		case kVDProjectCmd_ScrubEnd:
			OnPositionNotify(PCN_ENDTRACK);
			break;
		case kVDProjectCmd_ScrubUpdate:
			OnPositionNotify(PCN_THUMBPOSITION);
			break;
		case kVDProjectCmd_ScrubUpdatePrev:
			OnPositionNotify(PCN_THUMBPOSITIONPREV);
			break;
		case kVDProjectCmd_ScrubUpdateNext:
			OnPositionNotify(PCN_THUMBPOSITIONNEXT);
			break;
		case kVDProjectCmd_SetSelectionStart:
			SetSelectionStart();
			break;
		case kVDProjectCmd_SetSelectionEnd:
			SetSelectionEnd();
			break;
		case kVDProjectCmd_ZoomRange:
			ToggleZoomRange();
			break;
	}
	mPreviewRestartPosition = mpPosition->GetPosition();
}

bool VDProjectUI::MenuHit(UINT id) {
	bool bFilterReinitialize = !g_dubber;
	bool bJobActive = !!g_dubber;

	if (bFilterReinitialize) {
		switch(id) {
		case ID_VIDEO_SEEK_START:
		case ID_VIDEO_SEEK_END:
		case ID_VIDEO_SEEK_PREV:
		case ID_VIDEO_SEEK_NEXT:
		case ID_VIDEO_SEEK_FPREV:
		case ID_VIDEO_SEEK_FNEXT:
		case ID_VIDEO_SEEK_FSPREV:
		case ID_VIDEO_SEEK_FSNEXT:
		case ID_VIDEO_SEEK_PREVONESEC:
		case ID_VIDEO_SEEK_NEXTONESEC:
		case ID_VIDEO_SEEK_KEYPREV:
		case ID_VIDEO_SEEK_KEYNEXT:
		case ID_VIDEO_SEEK_SELSTART:
		case ID_VIDEO_SEEK_SELEND:
		case ID_VIDEO_SEEK_PREVDROP:
		case ID_VIDEO_SEEK_NEXTDROP:
		case ID_EDIT_JUMPTO:
		case ID_EDIT_ZOOMRANGE:
		case ID_VIDEO_COPYSOURCEFRAME:
		case ID_VIDEO_COPYOUTPUTFRAME:
		case ID_FILE_SAVEIMAGE:
		case ID_FILE_SAVEPROJECT:
		case ID_VIDEO_COPYSOURCEFRAMENUMBER:
		case ID_VIDEO_COPYOUTPUTFRAMENUMBER:
		case ID_PANELAYOUT_INPUTPANEONLY:
		case ID_PANELAYOUT_OUTPUTPANEONLY:
		case ID_PANELAYOUT_BOTHPANES:
		case ID_PANELAYOUT_AUTOSIZE:
			break;
		default:
			StopFilters();
			break;
		}
	}

	if (!bJobActive) {
		JobLockDubber();
		DragAcceptFiles((HWND)mhwnd, FALSE);
	}

	try {
		switch(id) {
		case ID_FILE_QUIT:						Quit();						break;
		case ID_FILE_OPENAVI:					OpenAsk();						break;
		case ID_FILE_REOPEN:					Reopen();					break;
		case ID_FILE_APPENDSEGMENT:				AppendAsk();					break;
		case ID_FILE_PREVIEWINPUT:
			if (g_dubber) {
				AbortPreviewing();
			} else {
				SceneShuttleStop();
				mPreviewRestartPosition = mpPosition->GetPosition();
				PreviewInput();
			}
			break;
		case ID_FILE_PREVIEWOUTPUT:
			if (g_dubber) {
				AbortPreviewing();
			} else {
				SceneShuttleStop();
				mPreviewRestartPosition = mpPosition->GetPosition();
				PreviewOutput();
			}
			break;
		case ID_FILE_PREVIEWAVI:				PreviewAll();					break;
		case ID_FILE_RUNVIDEOANALYSISPASS:		RunNullVideoPass();			break;
		case ID_FILE_SAVEAVI:					SaveAVIAsk(false);			break;
		case ID_FILE_SAVECOMPATIBLEAVI:			SaveCompatibleAVIAsk(false);	break;
		case ID_FILE_SAVEIMAGESEQ:				SaveImageSequenceAsk(false);		break;
		case ID_FILE_SAVEIMAGE:					SaveImageAsk();					break;
		case ID_FILE_SAVESEGMENTEDAVI:			SaveSegmentedAVIAsk(false);		break;
		case ID_FILE_SAVEFILMSTRIP:				SaveFilmstripAsk();				break;
		case ID_FILE_SAVEANIMATEDGIF:			SaveAnimatedGIFAsk();			break;
		case ID_FILE_SAVEANIMATEDPNG:			SaveAnimatedPNGAsk();			break;
		case ID_FILE_SAVERAWAUDIO:				SaveRawAudioAsk(false);			break;
		case ID_FILE_SAVERAWVIDEO:				SaveRawVideoAsk(false);			break;
		case ID_FILE_SAVEWAV:					SaveWAVAsk(false);				break;
		case ID_FILE_EXPORTEXTERNALENCODER:		ExportViaEncoderAsk(false);		break;
		case ID_EXPORT_DRIVERTOOL0:
		case ID_EXPORT_DRIVERTOOL1:
		case ID_EXPORT_DRIVERTOOL2:
		case ID_EXPORT_DRIVERTOOL3:
			ExportViaDriverTool(id-ID_EXPORT_DRIVERTOOL0);
			break;
		case ID_EXPORT_STREAM_COPY:	
			ExportViaDriverTool("Export.StreamCopy");
			break;
		case ID_FILE_CLOSEAVI:					Close();						break;
		case ID_FILE_STARTSERVER:				StartServer();					break;
		case ID_FILE_CAPTUREAVI:
			{
				VDUIFrame *pFrame = VDUIFrame::GetFrame((HWND)mhwnd);

				pFrame->SetNextMode(1);
				pFrame->Detach();
			}
			break;
		case ID_FILE_SAVECONFIGURATION:			SaveConfigurationAsk();		break;
		case ID_FILE_SAVEPROJECT:			SaveProject((HWND)mhwnd,false);		break;
		case ID_FILE_SAVEPROJECTAS:			SaveProject((HWND)mhwnd,true);		break;
		case ID_FILE_LOADCONFIGURATION:
		case ID_FILE_RUNSCRIPT:
			LoadConfigurationAsk();
			break;
		case ID_FILE_LOADPROJECT:
			LoadProjectAsk();
			break;
		case ID_FILE_JOBCONTROL:				OpenJobWindow();							break;
		case ID_FILE_AVIINFO:					ShowInputInfo();					break;
		case ID_FILE_SETTEXTINFO:
			// ugh
			extern void VDDisplayFileTextInfoDialog(VDGUIHandle hParent, std::list<std::pair<uint32, VDStringA> >&);
			VDDisplayFileTextInfoDialog(mhwnd, mTextInfo);
			break;

		case ID_FILE_NEWINSTANCE:
			OpenNewInstance();
			break;

		case ID_FILE_CLOSEANDDELETE:
			CloseAndDelete();
			break;

		case ID_FILE_OPENPREVIOUS:
			OpenPrevious();
			break;

		case ID_FILE_OPENNEXT:
			OpenNext();
			break;

		case ID_QUEUEBATCHOPERATION_SAVEASAVI:
			SaveAVIAsk(true);
			break;

		case ID_QUEUEBATCHOPERATION_SAVECOMPATIBLEAVI:
			SaveCompatibleAVIAsk(true);
			break;

		case ID_QUEUEBATCHOPERATION_SAVESEGMENTEDAVI:
			SaveSegmentedAVIAsk(true);
			break;

		case ID_QUEUEBATCHOPERATION_SAVEIMAGESEQUENCE:
			SaveImageSequenceAsk(true);
			break;

		case ID_QUEUEBATCHOPERATION_RUNVIDEOANALYSISPASS:
			QueueNullVideoPass();
			break;

		case ID_QUEUEBATCHOPERATION_SAVEWAV:
			SaveWAVAsk(true);
			break;

		case ID_QUEUEBATCHOPERATION_EXPORTRAWAUDIO:
			SaveRawAudioAsk(true);
			break;

		case ID_QUEUEBATCHOPERATION_EXPORTRAWVIDEO:
			SaveRawVideoAsk(true);
			break;

		case ID_QUEUEBATCHOPERATION_EXPORTUSINGEXTERNALENCODER:
			ExportViaEncoderAsk(true);
			break;

		case ID_FILE_BATCHWIZARD:
			extern void VDUIDisplayBatchWizard(VDGUIHandle hParent);
			VDUIDisplayBatchWizard(mhwnd);
			break;

		case ID_EDIT_UNDO:						Undo();						break;
		case ID_EDIT_REDO:						Redo();						break;

		case ID_EDIT_CUT:						Cut();						break;
		case ID_EDIT_COPY:						Copy();						break;
		case ID_EDIT_PASTE:						Paste();					break;
		case ID_EDIT_DELETE:					Delete();					break;
		case ID_EDIT_CLEAR:						ClearSelection();			break;
		case ID_EDIT_SELECTALL:
			SetSelectionStart(0);
			SetSelectionEnd(GetFrameCount());
			break;
		case ID_EDIT_CROPTOSELECTION:			CropToSelection();			break;

		case ID_VIEW_POSITIONCONTROL:
			mbPositionControlVisible = !mbPositionControlVisible;
			if (mhwndPosition) {
				ShowWindow(mhwndPosition, mbPositionControlVisible ? SW_SHOWNA : SW_HIDE);
				OnSize();
			}
			break;

		case ID_VIEW_STATUSBAR:
			mbStatusBarVisible = !mbStatusBarVisible;
			if (mhwndStatus) {
				ShowWindow(mhwndStatus, mbStatusBarVisible ? SW_SHOWNA : SW_HIDE);
				OnSize();
			}
			break;

		case ID_VIEW_MAXIMIZE:
			mbMaximize = !mbMaximize;
			if(mbMaximize) ShowWindow((HWND)mhwnd,SW_SHOWMAXIMIZED);
			UpdateMaximize();
			break;

		case ID_VIEW_CURVEEDITOR:
			if (mpCurveEditor)
				CloseCurveEditor();
			else
				OpenCurveEditor();
			break;

		case ID_VIEW_AUDIODISPLAY:
			if (mpAudioDisplay)
				CloseAudioDisplay();
			else
				OpenAudioDisplay();
			break;

		case ID_VIEW_FULLSCREEN:
			ToggleFullscreen();
			break;

		case ID_PANELAYOUT_INPUTPANEONLY:
			SetPaneLayout(kPaneLayoutInput);
			break;

		case ID_PANELAYOUT_OUTPUTPANEONLY:
			SetPaneLayout(kPaneLayoutOutput);
			break;

		case ID_PANELAYOUT_BOTHPANES:
			SetPaneLayout(kPaneLayoutDual);
			break;

		case ID_PANELAYOUT_AUTOSIZE:
			mbAutoSizePanes = !mbAutoSizePanes;
			RepositionPanes(true);
			break;

		case ID_VIDEO_SEEK_START:
			QueueCommand(kVDProjectCmd_GoToStart);
			break;

		case ID_VIDEO_SEEK_END:
			QueueCommand(kVDProjectCmd_GoToEnd);
			break;

		case ID_VIDEO_SEEK_PREV:				QueueCommand(kVDProjectCmd_GoToPrevFrame); break;
		case ID_VIDEO_SEEK_NEXT:				QueueCommand(kVDProjectCmd_GoToNextFrame); break;
		case ID_VIDEO_SEEK_PREVONESEC:			QueueCommand(kVDProjectCmd_GoToPrevUnit); break;
		case ID_VIDEO_SEEK_NEXTONESEC:			QueueCommand(kVDProjectCmd_GoToNextUnit); break;
		case ID_VIDEO_SEEK_KEYPREV:				QueueCommand(kVDProjectCmd_GoToPrevKey); break;
		case ID_VIDEO_SEEK_KEYNEXT:				QueueCommand(kVDProjectCmd_GoToNextKey); break;
		case ID_VIDEO_SEEK_SELSTART:
			if (IsSelectionPresent())
				QueueCommand(kVDProjectCmd_GoToSelectionStart);	
			break;
		case ID_VIDEO_SEEK_SELEND:
			if (IsSelectionPresent())
				QueueCommand(kVDProjectCmd_GoToSelectionEnd);
			break;
		case ID_VIDEO_SEEK_PREVDROP:			QueueCommand(kVDProjectCmd_GoToPrevDrop);		break;
		case ID_VIDEO_SEEK_NEXTDROP:			QueueCommand(kVDProjectCmd_GoToNextDrop);		break;
		case ID_EDIT_PREVRANGE:					QueueCommand(kVDProjectCmd_GoToPrevRange);		break;
		case ID_EDIT_NEXTRANGE:					QueueCommand(kVDProjectCmd_GoToNextRange);		break;
		case ID_EDIT_JUMPTO:					QueueCommand(kVDProjectCmd_GoToAsk);			break;
		case ID_EDIT_ZOOMRANGE:					QueueCommand(kVDProjectCmd_ZoomRange);			break;
		case ID_VIDEO_SEEK_STOP:
			SceneShuttleStop();
			AbortPreviewing();
			break;
		case ID_VIDEO_SEEK_PREVSCENE:
			if (IsSceneShuttleRunning())
				SceneShuttleStop();
			else {
				if (AbortPreviewing()) StartSceneShuttleReverse();
			}
			break;
		case ID_VIDEO_SEEK_NEXTSCENE:
			if (IsSceneShuttleRunning())
				SceneShuttleStop();
			else {
				if (AbortPreviewing()) StartSceneShuttleForward();
			}
			break;
		case ID_VIDEO_SEEK_FPREV:
			if (AbortPreviewing()) StartShuttle(-1,false);
			break;
		case ID_VIDEO_SEEK_FNEXT:
			if (AbortPreviewing()) StartShuttle(+1,false);
			break;
		case ID_VIDEO_SEEK_FSPREV:
			if (IsSceneShuttleRunning())
				SceneShuttleStop();
			else {
				if (AbortPreviewing()) StartShuttle(-1,true);
			}
			break;
		case ID_VIDEO_SEEK_FSNEXT:
			if (IsSceneShuttleRunning())
				SceneShuttleStop();
			else {
				if (AbortPreviewing()) StartShuttle(+1,true);
			}
			break;
		case ID_VIDEO_SCANFORERRORS:			ScanForErrors();			break;
		case ID_EDIT_RESET:						ResetTimelineWithConfirmation();		break;

		case ID_VIDEO_FILTERS:					SetVideoFiltersAsk();				break;
		case ID_VIDEO_FRAMERATE:				SetVideoFramerateOptionsAsk();		break;
		case ID_VIDEO_COLORDEPTH:				SetVideoDepthOptionsAsk();			break;
		case ID_VIDEO_CLIPPING:					SetVideoRangeOptionsAsk();			break;
		case ID_VIDEO_COMPRESSION:				SetVideoCompressionAsk();			break;
		case ID_VIDEO_MODE_DIRECT:				SetVideoMode(DubVideoOptions::M_NONE);			break;
		case ID_VIDEO_MODE_FASTRECOMPRESS:		SetVideoMode(DubVideoOptions::M_FASTREPACK);			break;
		case ID_VIDEO_MODE_NORMALRECOMPRESS:	SetVideoMode(DubVideoOptions::M_SLOWREPACK);			break;
		case ID_VIDEO_MODE_FULL:				SetVideoMode(DubVideoOptions::M_FULL);			break;
		case ID_VIDEO_SMARTRENDERING:			g_dubOpts.video.mbUseSmartRendering = !g_dubOpts.video.mbUseSmartRendering; break;
		case ID_VIDEO_PRESERVEEMPTYFRAMES:		g_dubOpts.video.mbPreserveEmptyFrames = !g_dubOpts.video.mbPreserveEmptyFrames; break;
		case ID_VIDEO_COPYSOURCEFRAME:			CopySourceFrameToClipboard();		break;
		case ID_VIDEO_COPYOUTPUTFRAME:			CopyOutputFrameToClipboard();		break;
		case ID_VIDEO_COPYSOURCEFRAMENUMBER:	CopySourceFrameNumberToClipboard();		break;
		case ID_VIDEO_COPYOUTPUTFRAMENUMBER:	CopyOutputFrameNumberToClipboard();		break;
		case ID_VIDEO_ERRORMODE:				SetVideoErrorModeAsk();			break;
		case ID_EDIT_MASK:						MaskSelection(true);							break;
		case ID_EDIT_UNMASK:					MaskSelection(false);						break;
		case ID_EDIT_SETSELSTART:
			if (!g_dubber || g_dubber->IsPreviewing())
				SetSelectionStart();
			else
				QueueCommand(kVDProjectCmd_SetSelectionStart);
			break;
		case ID_EDIT_SETSELEND:
			if (!g_dubber || g_dubber->IsPreviewing())
				SetSelectionEnd();
			else
				QueueCommand(kVDProjectCmd_SetSelectionEnd);
			break;
		case ID_EDIT_SETMARKER:
			if (!g_dubber || g_dubber->IsPreviewing())
				SetMarker();
			break;
		case ID_EDIT_CLEARMARKERS:
			mTimeline.ClearMarker();
			mpPosition->SetTimeline(mTimeline);
			break;
		case ID_EDIT_MARKERSFROMKEYS:
			if (inputVideo) {
				VDPosition pos = 0;
				while (1) {
					pos = inputVideo->nextKey(pos);
					if(pos==-1) break;
					mTimeline.SetMarkerSrc(pos);
				}

				if (mpCB)
					mpCB->UITimelineUpdated();
			}
			break;

		case ID_AUDIO_ADVANCEDFILTERING:
			g_dubOpts.audio.bUseAudioFilterGraph = !g_dubOpts.audio.bUseAudioFilterGraph;
			SetAudioSource();
			break;

		case ID_AUDIO_FILTERS:					SetAudioFiltersAsk();				break;

		case ID_AUDIO_CONVERSION:				SetAudioConversionOptionsAsk();	break;
		case ID_AUDIO_INTERLEAVE:				SetAudioInterleaveOptionsAsk();	break;
		case ID_AUDIO_COMPRESSION:				SetAudioCompressionAsk((HWND)mhwnd);			break;

		case ID_AUDIO_VOLUME:					SetAudioVolumeOptionsAsk();		break;

		case ID_AUDIO_SOURCE_NONE:				SetAudioSourceNone();			break;
		case ID_AUDIO_SOURCE_WAV:				SetAudioSourceWAVAsk();			break;

		case ID_AUDIO_MODE_DIRECT:				SetAudioMode(DubAudioOptions::M_NONE);			break;
		case ID_AUDIO_MODE_FULL:				SetAudioMode(DubAudioOptions::M_FULL);			break;

		case ID_AUDIO_ERRORMODE:			SetAudioErrorModeAsk();			break;

		case ID_OPTIONS_SHOWLOG:
			extern void VDOpenLogWindow();
			VDOpenLogWindow();
			break;

		case ID_OPTIONS_SHOWPROFILER:
			extern void VDOpenProfileWindow(int);
			VDOpenProfileWindow(g_dubber!=0 ? 2:1);
			break;

		case ID_OPTIONS_PERFORMANCE:
			extern void VDShowPerformanceDialog(VDGUIHandle h);
			VDShowPerformanceDialog((VDGUIHandle)mhwnd);
			break;

		case ID_OPTIONS_EXTERNALENCODERS:
			VDUIDisplayDialogConfigureExternalEncoders((VDGUIHandle)mhwnd);
			break;

		case ID_OPTIONS_PREFERENCES:
			extern int VDShowPreferencesDialog(VDGUIHandle h);
			{
				int clearCount = VDPreferencesGetHistoryClearCounter();

				int result = VDShowPreferencesDialog((VDGUIHandle)mhwnd);
				VDCPUTest();
				mMRUList.set_capacity(VDPreferencesGetMRUSize());

				if (clearCount != VDPreferencesGetHistoryClearCounter()) {
					mMRUList.clear_history();
				}

				UpdateMRUList();

				if (result & PREFERENCES_OPTF) {
					StopFilters();
					VDPosition timelinePos = GetCurrentFrame();
					RefilterFrame(timelinePos);
					// also could reset cached blitters
				}

				if (result & PREFERENCES_DISPLAY)
					UiDisplayPreferencesUpdated();
			}
			break;

		case ID_OPTIONS_KEYBOARDSHORTCUTS:
			if (VDShowDialogEditAccelerators((VDGUIHandle)mhwnd, kCommandList.begin(), kCommandList.size(), mAccelTableDef, mAccelTableDefault)) {
				HACCEL acc = VDUIBuildAcceleratorTableW32(mAccelTableDef);
				VDUIFrame *pFrame = VDUIFrame::GetFrame((HWND)mhwnd);
				pFrame->SetAccelTable(acc);

				if (mhAccelMain)
					DestroyAcceleratorTable(mhAccelMain);

				VDUIUpdateMenuAcceleratorsW32(mhMenuNormal, mAccelTableDef);
				DrawMenuBar((HWND)mhwnd);

				mhAccelMain = acc;

				UpdateAccelPreview();

				VDRegistryAppKey accelKey("Accelerators\\Main", true);

				try {
					mAccelTableDef.Save(accelKey);
				} catch(const MyError&) {
					// eat the error
				}
			}
			break;

		case ID_OPTIONS_PLUGINS:
			VDUIShowDialogPlugins((VDGUIHandle)mhwnd);
			break;

		case ID_OPTIONS_DISPLAYINPUTVIDEO:
			if (mpDubStatus)
				mpDubStatus->ToggleFrame(false);
			else
				g_dubOpts.video.fShowInputFrame = !g_dubOpts.video.fShowInputFrame;
			break;
		case ID_OPTIONS_DISPLAYOUTPUTVIDEO:
			if (mpDubStatus)
				mpDubStatus->ToggleFrame(true);
			else
				g_dubOpts.video.fShowOutputFrame = !g_dubOpts.video.fShowOutputFrame;
			break;
		case ID_OPTIONS_DISPLAYDECOMPRESSEDOUTPUT:
			g_drawDecompressedFrame = !g_drawDecompressedFrame;
			break;
		case ID_OPTIONS_SHOWSTATUSWINDOW:
			if (mpDubStatus)
				mpDubStatus->ToggleStatus();
			else
				g_showStatusWindow = !g_showStatusWindow;
			break;
		case ID_OPTIONS_VERTICALDISPLAY:
			g_vertical = !g_vertical;
			RepositionPanes(true);
			break;
		case ID_PAN_CENTER:
			g_panCentering = kPanCenter;
			ResetCentering();
			break;
		case ID_PAN_TOPLEFT:
			g_panCentering = kPanTopLeft;
			ResetCentering();
			break;
		case ID_OPTIONS_SYNCTOAUDIO:
			g_dubOpts.video.fSyncToAudio = !g_dubOpts.video.fSyncToAudio;
			break;
		case ID_OPTIONS_ENABLEDIRECTDRAW:
			g_dubOpts.perf.useDirectDraw = !g_dubOpts.perf.useDirectDraw;
			break;
		case ID_OPTIONS_DROPFRAMES:
			g_fDropFrames = !g_fDropFrames;
			break;
		case ID_OPTIONS_DROPSEEKING:
			g_fDropSeeking = !g_fDropSeeking;
			break;
		case ID_OPTIONS_SWAPPANES:
			g_fSwapPanes = !g_fSwapPanes;
			SwapFullscreen();
			RepositionPanes(true);
			break;

		case ID_OPTIONS_PREVIEWPROGRESSIVE:	g_dubOpts.video.previewFieldMode = DubVideoOptions::kPreviewFieldsProgressive; break;
		case ID_OPTIONS_PREVIEWWEAVETFF:	g_dubOpts.video.previewFieldMode = DubVideoOptions::kPreviewFieldsWeaveTFF; break;
		case ID_OPTIONS_PREVIEWWEAVEBFF:	g_dubOpts.video.previewFieldMode = DubVideoOptions::kPreviewFieldsWeaveBFF; break;
		case ID_OPTIONS_PREVIEWBOBTFF:		g_dubOpts.video.previewFieldMode = DubVideoOptions::kPreviewFieldsBobTFF; break;
		case ID_OPTIONS_PREVIEWBOBBFF:		g_dubOpts.video.previewFieldMode = DubVideoOptions::kPreviewFieldsBobBFF; break;
		case ID_OPTIONS_PREVIEWNONTFF:		g_dubOpts.video.previewFieldMode = DubVideoOptions::kPreviewFieldsNonIntTFF; break;
		case ID_OPTIONS_PREVIEWNONBFF:		g_dubOpts.video.previewFieldMode = DubVideoOptions::kPreviewFieldsNonIntBFF; break;

		case ID_TOOLS_HEXVIEWER:
			HexEdit(NULL, NULL, false);
			break;

		case ID_TOOLS_CREATESPARSEAVI:
			CreateExtractSparseAVI((HWND)mhwnd, false);
			break;

		case ID_TOOLS_EXPANDSPARSEAVI:
			CreateExtractSparseAVI((HWND)mhwnd, true);
			break;

		case ID_TOOLS_BENCHMARKRESAMPLER:
			extern void VDBenchmarkResampler(VDGUIHandle);
			VDBenchmarkResampler(mhwnd);
			break;

		case ID_TOOLS_BENCHMARKANALYZEPASS:
			extern void VDOpenProfileWindow(int);
			VDOpenProfileWindow(1);
			RunNullVideoPass(true);
			break;

		case ID_TOOLS_CREATEPALETTIZEDAVI:
			extern void VDCreateTestPal8Video(VDGUIHandle);
			VDCreateTestPal8Video(mhwnd);
			break;

		case ID_TOOLS_CREATETESTVIDEO:
			{
				extern IVDInputDriver *VDCreateInputDriverTest();

				vdrefptr<IVDInputDriver> pDriver(VDCreateInputDriverTest());
				vdrefptr<InputFile> inputAVI;
				inputAVI = pDriver->CreateInputFile(0);
				InputFileOptions* opt = inputAVI->promptForOptions(mhwnd);
				if (opt) {
					VDString data;
					int len = opt->write(0,0);
					data.resize(len);
					opt->write(&data[0],len);
					delete opt;
					Open(L"", pDriver, false, false, false, &data[0], len);
				}
			}
			break;

		case ID_HELP_LICENSE:
			VDDisplayLicense((HWND)mhwnd, false);
			break;

		case ID_HELP_CONTENTS:
			VDShowHelp((HWND)mhwnd);
			break;
		case ID_HELP_ABOUT:
			DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUT), (HWND)mhwnd, AboutDlgProc);
			break;

		case ID_DUBINPROGRESS_ABORT:			AbortOperation();			break;

		default:
			if (id >= ID_AUDIO_SOURCE_AVI_0 && id <= ID_AUDIO_SOURCE_AVI_0+99) {
				SetAudioSourceNormal(id - ID_AUDIO_SOURCE_AVI_0);
			} else if (id >= ID_MRU_FILE0 && id <= ID_MRU_FILE0+99) {
				const int index = id - ID_MRU_FILE0;
				VDStringW name(mMRUList[index]);

				if (!name.empty()) {
					const bool bExtendedOpen = (signed short)GetAsyncKeyState(VK_SHIFT) < 0;

					VDAutoLogDisplay logDisp;
					g_project->Open(name.c_str(), NULL, bExtendedOpen, false, 1);
					logDisp.Post(mhwnd);
				}
				break;
			} else if (id >= ID_PLUGIN_TOOL && id <= ID_PLUGIN_TOOL+99) {
				VDToolExecuteCommand(id,(HWND)mhwnd);
			}
			break;
		}
	} catch(const MyError& e) {
		HWND parent = (HWND)mhwnd;
		if (mpDubStatus && mpDubStatus->IsNormalWindow()) parent = mpDubStatus->GetHwnd();
		e.post(parent, g_szError);
	}

	if (!bJobActive) {
		JobUnlockDubber();
		DragAcceptFiles((HWND)mhwnd, TRUE);
	}

	return true;
}

void VDProjectUI::RepaintMainWindow(HWND hWnd) {
	PAINTSTRUCT ps;
	HDC hDC;

	hDC = BeginPaint(hWnd, &ps);
	EndPaint(hWnd, &ps);
}

void VDProjectUI::ShowMenuHelp(WPARAM wParam) {
	if (HIWORD(wParam)==0xFFFF) {
		UINotifySelection();
		return;
	}

	if (LOWORD(wParam) >= ID_MRU_FILE0 && LOWORD(wParam) <= ID_MRU_FILE3) {
		HWND hwndStatus = GetDlgItem((HWND)mhwnd, IDC_STATUS_WINDOW);
		char name[1024];

		if ((HIWORD(wParam) & MF_POPUP) || (HIWORD(wParam) & MF_SYSMENU)) {
			SendMessage(hwndStatus, SB_SETTEXT, 0, (LPARAM)"");
			return;
		}

		strcpy(name, "[SHIFT for options] Load file ");

		const VDStringW filename(mMRUList[LOWORD(wParam) - ID_MRU_FILE0]);

		if (!filename.empty()) {
			VDTextWToA(name+30, sizeof name - 30, filename.c_str(), filename.length() + 1);
			SendMessage(hwndStatus, SB_SETTEXT, 255, (LPARAM)name);
		} else
			SendMessage(hwndStatus, SB_SETTEXT, 255, (LPARAM)"");
	} else
		guiMenuHelp((HWND)mhwnd, wParam, 255, iMainMenuHelpTranslator);
}

void VDProjectUI::UpdateMainMenu(HMENU hMenu) {

	VDCheckMenuItemW32(hMenu, ID_VIEW_POSITIONCONTROL, mbPositionControlVisible);
	VDCheckMenuItemW32(hMenu, ID_VIEW_STATUSBAR, mbStatusBarVisible);
	VDCheckMenuItemW32(hMenu, ID_VIEW_CURVEEDITOR, mpCurveEditor != NULL);
	VDCheckMenuItemW32(hMenu, ID_VIEW_AUDIODISPLAY, mpAudioDisplay != NULL);
	VDCheckMenuItemW32(hMenu, ID_VIEW_MAXIMIZE, mbMaximize);
	VDCheckMenuItemW32(hMenu, ID_VIEW_FULLSCREEN, IsWindowVisible(mhwndMaxDisplay)==TRUE);

	VDCheckRadioMenuItemByCommandW32(hMenu, ID_PANELAYOUT_INPUTPANEONLY, mPaneLayoutMode == kPaneLayoutInput);
	VDCheckRadioMenuItemByCommandW32(hMenu, ID_PANELAYOUT_OUTPUTPANEONLY, mPaneLayoutMode == kPaneLayoutOutput);
	VDCheckRadioMenuItemByCommandW32(hMenu, ID_PANELAYOUT_BOTHPANES, mPaneLayoutMode == kPaneLayoutDual);

	VDCheckMenuItemW32(hMenu, ID_PANELAYOUT_AUTOSIZE, mbAutoSizePanes);

	int audioSourceMode = GetAudioSourceMode();
	VDCheckRadioMenuItemByCommandW32(hMenu, ID_AUDIO_SOURCE_NONE, audioSourceMode == kVDAudioSourceMode_None);
	VDCheckRadioMenuItemByCommandW32(hMenu, ID_AUDIO_SOURCE_WAV, audioSourceMode == kVDAudioSourceMode_External);

	CheckMenuRadioItem(hMenu, ID_AUDIO_SOURCE_AVI_0, ID_AUDIO_SOURCE_AVI_0+99, ID_AUDIO_SOURCE_AVI_0 + (audioSourceMode - kVDAudioSourceMode_Source), MF_BYCOMMAND);

	CheckMenuRadioItem(hMenu, ID_VIDEO_MODE_DIRECT, ID_VIDEO_MODE_FULL, ID_VIDEO_MODE_DIRECT+g_dubOpts.video.mode, MF_BYCOMMAND);
	CheckMenuRadioItem(hMenu, ID_AUDIO_MODE_DIRECT, ID_AUDIO_MODE_FULL, ID_AUDIO_MODE_DIRECT+g_dubOpts.audio.mode, MF_BYCOMMAND);
	CheckMenuRadioItem(hMenu, ID_OPTIONS_PREVIEWPROGRESSIVE, ID_OPTIONS_PREVIEWNONBFF,
		ID_OPTIONS_PREVIEWPROGRESSIVE+g_dubOpts.video.previewFieldMode, MF_BYCOMMAND);

	VDCheckMenuItemW32(hMenu, ID_VIDEO_SMARTRENDERING,				g_dubOpts.video.mbUseSmartRendering);
	VDCheckMenuItemW32(hMenu, ID_VIDEO_PRESERVEEMPTYFRAMES,			g_dubOpts.video.mbPreserveEmptyFrames);

	VDEnableMenuItemW32(hMenu, ID_VIDEO_SMARTRENDERING, (g_dubOpts.video.mode != DubAudioOptions::M_NONE));
	VDEnableMenuItemW32(hMenu, ID_VIDEO_PRESERVEEMPTYFRAMES, (g_dubOpts.video.mode != DubAudioOptions::M_NONE));

	VDCheckMenuItemW32(hMenu, ID_AUDIO_ADVANCEDFILTERING,			g_dubOpts.audio.bUseAudioFilterGraph);

	VDCheckMenuItemW32(hMenu, ID_OPTIONS_DISPLAYINPUTVIDEO,			g_dubOpts.video.fShowInputFrame);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_DISPLAYOUTPUTVIDEO,		g_dubOpts.video.fShowOutputFrame);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_DISPLAYDECOMPRESSEDOUTPUT,	g_drawDecompressedFrame);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_SHOWSTATUSWINDOW,			g_showStatusWindow);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_VERTICALDISPLAY,			g_vertical);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_SYNCTOAUDIO,				g_dubOpts.video.fSyncToAudio);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_ENABLEDIRECTDRAW,			g_dubOpts.perf.useDirectDraw);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_DROPFRAMES,				g_fDropFrames);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_DROPSEEKING,				g_fDropSeeking);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_SWAPPANES,					g_fSwapPanes);
	CheckMenuRadioItem(hMenu, ID_PAN_CENTER, ID_PAN_TOPLEFT, ID_PAN_CENTER+g_panCentering, MF_BYCOMMAND);

	const bool bAVISourceExists = (inputAVI && inputAVI->Append(NULL));
	VDEnableMenuItemW32(hMenu,ID_FILE_APPENDSEGMENT			, bAVISourceExists);

	const bool bSourceFileExists = (inputAVI != 0);
	VDEnableMenuItemW32(hMenu, ID_FILE_REOPEN				, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_PREVIEWAVI			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_PREVIEWINPUT			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_PREVIEWOUTPUT		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_RUNVIDEOANALYSISPASS	, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_SAVEAVI				, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_SAVECOMPATIBLEAVI	, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_SAVEIMAGESEQ			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_SAVEIMAGE			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_SAVESEGMENTEDAVI		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_SAVEWAV				, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_SAVEFILMSTRIP		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_SAVEANIMATEDGIF		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_SAVEANIMATEDPNG		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_SAVERAWAUDIO			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_SAVERAWVIDEO			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_CLOSEAVI				, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_STARTSERVER			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_AVIINFO				, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_SETTEXTINFO			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_EXPORTEXTERNALENCODER, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_FILE_EXPORT, bSourceFileExists);

	HMENU hmenuFile = GetSubMenu(hMenu, 0);
	if (bSourceFileExists) {
		HMENU hmenuExport = GetSubMenu(mhMenuExport, 0);
		MENUITEMINFOA info = {0};
		info.cbSize = sizeof(info);
		info.fMask = MIIM_SUBMENU;
		info.hSubMenu = hmenuExport;
		SetMenuItemInfo(hmenuFile,ID_FILE_EXPORT,false,&info);

		RemoveMenu(hmenuExport, ID_EXPORT_DRIVERTOOL0, MF_BYCOMMAND);
		RemoveMenu(hmenuExport, ID_EXPORT_DRIVERTOOL1, MF_BYCOMMAND);
		RemoveMenu(hmenuExport, ID_EXPORT_DRIVERTOOL2, MF_BYCOMMAND);
		RemoveMenu(hmenuExport, ID_EXPORT_DRIVERTOOL3, MF_BYCOMMAND);

		int pos = 0;
		while (1) {
			info.fMask = MIIM_ID;
			if (!GetMenuItemInfo(hmenuExport, pos, TRUE, &info)) break;
			pos++;
		}

		IFilterModFileTool* tool;
		inputAVI->GetFileTool(&tool);
		if (tool) {
			{for(int i=0; i<3; i++){
				char name[256];
				bool enabled = true;
				if (!tool->GetExportMenuInfo(i,name,sizeof(name),&enabled)) continue;

				MENUITEMINFOA mii = {0};
				mii.cbSize = sizeof(mii);
				mii.fMask = MIIM_TYPE | MIIM_STATE | MIIM_ID;
				mii.fType = MFT_STRING;
				mii.fState = enabled ? 0 : MFS_DISABLED;
				mii.wID	= ID_EXPORT_DRIVERTOOL0;
				mii.dwTypeData	= name;
				InsertMenuItemA(hmenuExport, pos, TRUE, &mii);
				pos++;
			}}

			tool->Release();
		}

		VDUIUpdateMenuAcceleratorsW32(hmenuExport, mAccelTableDef);

	} else {
		MENUITEMINFOA info = {0};
		info.cbSize = sizeof(info);
		info.fMask = MIIM_SUBMENU;
		info.hSubMenu = GetSubMenu(mhMenuExport, 0);
		SetMenuItemInfo(hmenuFile,ID_FILE_EXPORT,false,&info);
	}

	{
		if (mhMenuTools)
			DestroyMenu(mhMenuTools);
		mhMenuTools = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_TOOLS_MENU));
		HMENU hmenuTools = GetSubMenu(mhMenuTools, 0);
		{for(int i=0; i<GetMenuItemCount(hmenuTools); i++){
			if (GetMenuItemID(hmenuTools,i)==ID_TOOLS_PLUGIN) {
				int pos = i;
				RemoveMenu(hmenuTools, pos, MF_BYPOSITION);
				VDToolInsertMenu(hmenuTools, pos);
				break;
			}
		}}

		VDUIUpdateMenuAcceleratorsW32(mhMenuTools, mAccelTableDef);

		VDEnableMenuItemW32(hmenuTools, ID_TOOLS_BENCHMARKANALYZEPASS, bSourceFileExists);

		MENUITEMINFOA info = {0};
		info.cbSize = sizeof(info);
		info.fMask = MIIM_SUBMENU;
		info.hSubMenu = hmenuTools;
		SetMenuItemInfo(hMenu,ID_TOOLS,false,&info);
	}

	VDEnableMenuItemW32(hMenu, ID_QUEUEBATCHOPERATION_SAVEASAVI				, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_QUEUEBATCHOPERATION_SAVECOMPATIBLEAVI		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_QUEUEBATCHOPERATION_SAVESEGMENTEDAVI		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_QUEUEBATCHOPERATION_SAVEIMAGESEQUENCE		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_QUEUEBATCHOPERATION_RUNVIDEOANALYSISPASS	, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_QUEUEBATCHOPERATION_SAVEWAV				, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_QUEUEBATCHOPERATION_EXPORTRAWAUDIO		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_QUEUEBATCHOPERATION_EXPORTRAWVIDEO        , bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_QUEUEBATCHOPERATION_EXPORTUSINGEXTERNALENCODER, bSourceFileExists);

	const bool bSelectionExists = bSourceFileExists && IsSelectionPresent();

	VDEnableMenuItemW32(hMenu, ID_EDIT_CUT					, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_EDIT_COPY					, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_EDIT_PASTE				, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_EDIT_DELETE				, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_EDIT_CLEAR				, bSelectionExists);
	VDEnableMenuItemW32(hMenu, ID_EDIT_SETSELSTART			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_EDIT_SETSELEND			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_EDIT_MASK					, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_EDIT_UNMASK				, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_EDIT_RESET				, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_EDIT_CROPTOSELECTION		, bSelectionExists);
	VDEnableMenuItemW32(hMenu, ID_EDIT_SETMARKER			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_EDIT_CLEARMARKERS			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_EDIT_MARKERSFROMKEYS		, bSourceFileExists);

	const wchar_t *undoAction = GetCurrentUndoAction();
	const wchar_t *redoAction = GetCurrentRedoAction();

	VDEnableMenuItemW32(hMenu, ID_EDIT_UNDO, bSourceFileExists && undoAction);
	VDEnableMenuItemW32(hMenu, ID_EDIT_REDO, bSourceFileExists && redoAction);

	if (!undoAction)
		undoAction = L"";
	if (!redoAction)
		redoAction = L"";

	const VDStringW undoPrevCmd(VDGetMenuItemTextByCommandW32(hMenu, ID_EDIT_UNDO));
	const VDStringW redoPrevCmd(VDGetMenuItemTextByCommandW32(hMenu, ID_EDIT_REDO));

	VDStringW undoCmd(VDswprintf(VDLoadString(0, kVDST_ProjectUI, kVDM_Undo), 1, &undoAction));
	VDStringW redoCmd(VDswprintf(VDLoadString(0, kVDST_ProjectUI, kVDM_Redo), 1, &redoAction));

	VDStringW::size_type undoAccel = undoPrevCmd.find('\t');
	VDStringW::size_type redoAccel = redoPrevCmd.find('\t');

	if (undoAccel != VDStringW::npos)
		undoCmd.append(undoPrevCmd, undoAccel, VDStringW::npos);

	if (redoAccel != VDStringW::npos)
		redoCmd.append(redoPrevCmd, redoAccel, VDStringW::npos);

	VDSetMenuItemTextByCommandW32(hMenu, ID_EDIT_UNDO, undoCmd.c_str());
	VDSetMenuItemTextByCommandW32(hMenu, ID_EDIT_REDO, redoCmd.c_str());

	VDEnableMenuItemW32(hMenu, ID_EDIT_SELECTALL			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_START			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_END			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_PREV			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_NEXT			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_FPREV			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_FNEXT			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_FSPREV			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_FSNEXT			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_KEYPREV		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_KEYNEXT		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_PREVONESEC		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_NEXTONESEC		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_PREVDROP		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_NEXTDROP		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_EDIT_PREVRANGE			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_EDIT_NEXTRANGE			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_SELSTART		, bSelectionExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_SELEND			, bSelectionExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_PREVSCENE		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_NEXTSCENE		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_VIDEO_SEEK_STOP			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu, ID_EDIT_JUMPTO				, bSourceFileExists);

	bool zoom = mbZoomEnabled;
	if (mposZoomEnd>mposZoomStart) zoom = true;
	if (mposSelectionEnd>mposSelectionStart) zoom = true;
	VDEnableMenuItemW32(hMenu, ID_EDIT_ZOOMRANGE, zoom);
	VDCheckMenuItemW32(hMenu, ID_EDIT_ZOOMRANGE, mbZoomEnabled);

	VDEnableMenuItemW32(hMenu, ID_VIDEO_CLIPPING			, inputVideo != 0);

	VDEnableMenuItemW32(hMenu,ID_VIDEO_COPYSOURCEFRAME		, inputVideo != 0);
	VDEnableMenuItemW32(hMenu,ID_VIDEO_COPYOUTPUTFRAME		, inputVideo != 0);
	VDEnableMenuItemW32(hMenu,ID_VIDEO_COPYSOURCEFRAMENUMBER		, inputVideo != 0);
	VDEnableMenuItemW32(hMenu,ID_VIDEO_COPYOUTPUTFRAMENUMBER		, inputVideo != 0);
	VDEnableMenuItemW32(hMenu,ID_VIDEO_SCANFORERRORS		, inputVideo != 0);

	const bool bAudioProcessingEnabled			= (g_dubOpts.audio.mode == DubAudioOptions::M_FULL);
	const bool bUseFixedFunctionAudioPipeline	= bAudioProcessingEnabled && !g_dubOpts.audio.bUseAudioFilterGraph;
	const bool bUseProgrammableAudioPipeline	= bAudioProcessingEnabled && g_dubOpts.audio.bUseAudioFilterGraph;

	VDEnableMenuItemW32(hMenu,ID_AUDIO_ADVANCEDFILTERING	, bAudioProcessingEnabled);
	VDEnableMenuItemW32(hMenu,ID_AUDIO_COMPRESSION			, bAudioProcessingEnabled);
	VDEnableMenuItemW32(hMenu,ID_AUDIO_CONVERSION			, bUseFixedFunctionAudioPipeline);
	VDEnableMenuItemW32(hMenu,ID_AUDIO_VOLUME				, bUseFixedFunctionAudioPipeline);
	VDEnableMenuItemW32(hMenu,ID_AUDIO_FILTERS				, bUseProgrammableAudioPipeline);

	const bool bVideoFullProcessingEnabled = (g_dubOpts.video.mode >= DubVideoOptions::M_FULL);
	VDEnableMenuItemW32(hMenu,ID_VIDEO_FILTERS				, bVideoFullProcessingEnabled);

	//const bool bVideoConversionEnabled = (g_dubOpts.video.mode >= DubVideoOptions::M_SLOWREPACK);
	//VDEnableMenuItemW32(hMenu,ID_VIDEO_COLORDEPTH			, bVideoConversionEnabled);

	const bool bVideoCompressionEnabled = (g_dubOpts.video.mode >= DubVideoOptions::M_FASTREPACK);
	VDEnableMenuItemW32(hMenu,ID_VIDEO_COMPRESSION			, bVideoCompressionEnabled);
}

void VDProjectUI::UpdateAudioSourceMenu() {
	for(int i = GetMenuItemCount(mhMenuSourceList)-1; i>=0; --i)
		DeleteMenu(mhMenuSourceList, i, MF_BYPOSITION);

	int count = GetAudioSourceCount();

	if (!count)
		VDAppendMenuW32(mhMenuSourceList, MF_GRAYED, 0, L"None");
	else {
		VDStringW s;

		for(int i=0; i<count; ++i) {
			s.sprintf(L"Stream %d", i+1);
			VDAppendMenuW32(mhMenuSourceList, MF_ENABLED, ID_AUDIO_SOURCE_AVI_0 + i, s.c_str());
		}
	}
}

void VDProjectUI::UpdateDubMenu(HMENU hMenu) {
	bool fShowStatusWindow = mpDubStatus->isVisible();
	bool fShowInputFrame = mpDubStatus->isFrameVisible(false);
	bool fShowOutputFrame = mpDubStatus->isFrameVisible(true);

	VDCheckMenuItemW32(hMenu, ID_OPTIONS_DISPLAYINPUTVIDEO, fShowInputFrame);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_DISPLAYOUTPUTVIDEO, fShowOutputFrame);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_SHOWSTATUSWINDOW, fShowStatusWindow);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_VERTICALDISPLAY,			g_vertical);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_SWAPPANES,					g_fSwapPanes);
}

LRESULT VDProjectUI::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	return (this->*mpWndProc)(msg, wParam, lParam);
}

LRESULT VDProjectUI::MainWndProc( UINT msg, WPARAM wParam, LPARAM lParam) {
	static HWND hwndItem0 = NULL;

    switch (msg) {
 
	case WM_INITMENU:
		UpdateMainMenu((HMENU)wParam);
		break;

	case WM_ACTIVATE:
		if (LOWORD(wParam)==WA_ACTIVE || LOWORD(wParam)==WA_CLICKACTIVE) {
			if (IsWindowVisible(mhwndMaxDisplay)) {
				HMONITOR mon1 = MonitorFromWindow((HWND)mhwnd, MONITOR_DEFAULTTONEAREST);
				HMONITOR mon2 = MonitorFromWindow(mhwndMaxDisplay, MONITOR_DEFAULTTONEAREST);
				if (mon1==mon2) {
					SetActiveWindow(mhwndMaxDisplay);
					return 0;
				}
			}
		}
		break;

	case WM_GETMINMAXINFO:
		if (lParam) {
			OnGetMinMaxInfo(*(MINMAXINFO *)lParam);
			return 0;
		}
		break;

	case WM_COMMAND:           // message: command from application menu
		if (lParam) {
			switch(LOWORD(wParam)) {
			case IDC_POSITION:
				if (inputVideo) {
					try {
						switch(HIWORD(wParam)) {
						case PCN_PLAY:
							if (g_dubber) {
								AbortPreviewing();
							} else {
								SceneShuttleStop();
								mPreviewRestartPosition = mpPosition->GetPosition();
								PreviewInput();
							}
							break;
						case PCN_PLAYPREVIEW:
							if (g_dubber) {
								AbortPreviewing();
							} else {
								SceneShuttleStop();
								mPreviewRestartPosition = mpPosition->GetPosition();
								PreviewOutput();
							}
							break;
						case PCN_MARKIN:
							MenuHit(ID_EDIT_SETSELSTART);
							break;
						case PCN_MARKOUT:
							MenuHit(ID_EDIT_SETSELEND);
							break;
						case PCN_START:
							MenuHit(ID_VIDEO_SEEK_START);
							break;
						case PCN_BACKWARD:
							MenuHit(ID_VIDEO_SEEK_PREV);
							break;
						case PCN_FORWARD:
							MenuHit(ID_VIDEO_SEEK_NEXT);
							break;
						case PCN_END:
							MenuHit(ID_VIDEO_SEEK_END);
							break;
						case PCN_KEYPREV:
							MenuHit(ID_VIDEO_SEEK_KEYPREV);
							break;
						case PCN_KEYNEXT:
							MenuHit(ID_VIDEO_SEEK_KEYNEXT);
							break;
						case PCN_SCENEREV:
							if (!AbortPreviewing()) return -1;
							StartSceneShuttleReverse();
							break;
						case PCN_SCENEFWD:
							if (!AbortPreviewing()) return -1;
							StartSceneShuttleForward();
							break;
						case PCN_STOP:
							AbortOperation();
							SceneShuttleStop();
							break;
						case PCN_SCENESTOP:
							SceneShuttleStop();
							break;
						}
					} catch(const MyError& e) {
						e.post((HWND)mhwnd, g_szError);
					}
				}
				return 0;
			}
		} else if (MenuHit(LOWORD(wParam)))
			return 0;

		break;

	case WM_WINDOWPOSCHANGING:
		// seems to fix borderless sizing issues
		if(!(((WINDOWPOS*)lParam)->flags & SWP_NOMOVE) && !mbMaximizeChanging){
			int style = GetWindowLong((HWND)mhwnd,GWL_STYLE);
			SetWindowLong((HWND)mhwnd,GWL_STYLE,style | WS_CAPTION);
		}
		break;

	case WM_SIZE:
		if (!IsIconic((HWND)mhwnd)) {
			UpdateMaximize();
			OnSize();
		}
		return 0;

	case WM_DESTROY:                  // message: window being destroyed
		PostQuitMessage(0);
		break;

	case WM_PAINT:
		RepaintMainWindow((HWND)mhwnd);
		return 0;

	case WM_MENUSELECT:
		ShowMenuHelp(wParam);
		return 0;

	case WM_NOTIFY:
		{
			LPNMHDR nmh = (LPNMHDR)lParam;

			switch(nmh->idFrom) {
			case IDC_POSITION:
				OnPositionNotify(nmh->code);
				break;

			case 1:
			case 2:
				switch(nmh->code) {
				case VWN_RESIZED:
					if (nmh->idFrom == 1) {
						GetClientRect(nmh->hwndFrom, &mrInputFrame);
					} else {
						GetClientRect(nmh->hwndFrom, &mrOutputFrame);
					}

					if (!mbPaneLayoutBusy)
						RepositionPanes();
					break;
				case VWN_REQUPDATE:
					if (nmh->idFrom == 1) {
						RefreshInputPane();
					} else {
						RefreshOutputPane();
					}
					break;
				}
				break;
			}
		}
		return 0;

	case WM_KEYDOWN:
		switch((int)wParam) {
		case VK_F12:
			guiOpenDebug();
			break;
		}
		return 0;

  case WM_KEYUP:
  case WM_SYSKEYUP:
    if (mShuttleMode && !mStickyShuttle) {
      SceneShuttleStop();
    }
    return 0;

	case WM_DROPFILES:
		HandleDragDrop((HDROP)wParam);
		DragFinish((HDROP)wParam);
		return 0;

	case WM_MOUSEWHEEL:
		// Windows forwards all mouse wheel messages down to us, which we then forward
		// to the position control.  Obviously for this to be safe the position control
		// MUST eat the message, which it currently does.
		return SendMessage(mhwndPosition, WM_MOUSEWHEEL, wParam, lParam);

	case WM_CLOSE:
		if (VDPreferencesGetConfirmExit()) {
			if (IDOK != MessageBox((HWND)mhwnd, "Are you sure you want to exit?", "VirtualDub warning", MB_ICONQUESTION | MB_OKCANCEL))
				return 0;
		}
		break;

	case WM_USER+100:		// display update request
		if (!g_dubber) {
			IVDVideoDisplay *pDisp = wParam ? mpOutputDisplay : mpInputDisplay;

			if (!wParam)
				RefreshInputPane();
			else
				RefreshOutputPane();

			pDisp->Cache();
		}
		break;

	case MYWM_DEFERRED_COMMAND:
		ExecuteCommand((int)wParam);
		return 0;

	case MYWM_DEFERRED_PREVIEWRESTART:
		try {
			PreviewRestart();
		} catch(const MyError& e) {
			e.post((HWND)mhwnd, g_szError);
		}
		return 0;

	case MYWM_DEFERRED_FILECOMMAND:
		{
			FileNameCommand* cmd = (FileNameCommand*)lParam;
			try {
				if (cmd->reopen) {
					StopFilters();
					g_project->Reopen();
				} else {
					IVDInputDriver *pDriver = VDGetInputDriverByName(cmd->driverName.c_str());
					g_project->Open(cmd->fileName.c_str(), pDriver);
				}
			} catch(const MyError& e) {
				if (!VDToolCatchError(cmd,e))
					e.post((HWND)mhwnd, g_szError);
			}
			delete cmd;
		}
		return 0;
	}

	return VDUIFrame::GetFrame((HWND)mhwnd)->DefProc((HWND)mhwnd, msg, wParam, lParam);
}

LRESULT VDProjectUI::DubWndProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
	case WM_INITMENU:
		UpdateDubMenu((HMENU)wParam);
		break;

	case WM_GETMINMAXINFO:
		if (lParam) {
			OnGetMinMaxInfo(*(MINMAXINFO *)lParam);
			return 0;
		}
		break;

	case WM_COMMAND:
		if (lParam) {
			switch(LOWORD(wParam)) {
			case IDC_POSITION:
				return MainWndProc(msg,wParam,lParam);
			}
		} else if (!MenuHit(LOWORD(wParam)))
			return VDUIFrame::GetFrame((HWND)mhwnd)->DefProc((HWND)mhwnd, msg, wParam, lParam);
		break;

	case WM_CLOSE:
		if (VDPreferencesGetConfirmExit()) {
			if (IDOK != MessageBox((HWND)mhwnd, "Are you sure you want to exit?", "VirtualDub warning", MB_ICONQUESTION | MB_OKCANCEL))
				return 0;
		}

		if (g_dubber->IsPreviewing() || g_bEnableVTuneProfiling) {
			g_dubber->Abort();
			g_bExit = true;
		} else {
			if (mpDubStatus && !mpDubStatus->isVisible())
				mpDubStatus->ToggleStatus();

			if (IDYES == MessageBox((HWND)mhwnd,
					"A dub operation is currently in progress. Forcing VirtualDub to abort "
					"will leave the output file unusable and may have undesirable side effects. "
					"Do you really want to do this?"
					,"VirtualDub warning", MB_YESNO))

					ExitProcess(1000);
		}
		break;

	case WM_SIZE:
		OnSize();
		break;

	case WM_PAINT:
		RepaintMainWindow((HWND)mhwnd);
		return TRUE;

	case WM_NOTIFY:
		{
			LPNMHDR nmh = (LPNMHDR)lParam;

			switch(nmh->idFrom) {
			case IDC_POSITION:
				switch(nmh->code) {
				case PCN_BEGINTRACK:
					if (!QueueCommand(kVDProjectCmd_ScrubBegin)) return -1;
					break;
				case PCN_ENDTRACK:
					if (!QueueCommand(kVDProjectCmd_ScrubEnd)) return -1;
					break;
				case PCN_THUMBPOSITION:
				case PCN_THUMBTRACK:
				case PCN_PAGELEFT:
				case PCN_PAGERIGHT:
					if (!QueueCommand(kVDProjectCmd_ScrubUpdate)) return -1;
					break;
				case PCN_THUMBPOSITIONPREV:
					if (!QueueCommand(kVDProjectCmd_ScrubUpdatePrev)) return -1;
					break;
				case PCN_THUMBPOSITIONNEXT:
					if (!QueueCommand(kVDProjectCmd_ScrubUpdateNext)) return -1;
					break;
				}

				return 0;

			case 1:
			case 2:
				switch(nmh->code) {
				case VWN_RESIZED:
					if (nmh->idFrom == 1) {
						GetClientRect(nmh->hwndFrom, &mrInputFrame);
					} else {
						GetClientRect(nmh->hwndFrom, &mrOutputFrame);
					}

					if (!mbPaneLayoutBusy)
						RepositionPanes();
					break;
				case VWN_REQUPDATE:
					// eat it
					break;
				}
				return 0;
			}
		}
		break;

	case WM_USER+100:		// display update request
		// Don't pay attention to these. If we re-init the displays while a render is
		// running because the panes are disabled, then they won't update properly
		// when re-enabled since the rendering engine calls Update().
		break;

	case MYWM_DEFERRED_FILECOMMAND:
		{
			FileNameCommand* cmd = (FileNameCommand*)lParam;
			delete cmd;
  		}
		return 0;

	default:
		return VDUIFrame::GetFrame((HWND)mhwnd)->DefProc((HWND)mhwnd, msg, wParam, lParam);
    }
    return (0);
}

void VDProjectUI::HandleDragDrop(HDROP hdrop) {
	UINT fileCount = DragQueryFile(hdrop, 0xFFFFFFFF, NULL, 0);

	if (fileCount < 1)
		return;

	bool isAppend = (GetKeyState(VK_CONTROL) < 0);

	if (!isAppend && fileCount > 1) {
		MyError("You have dropped %u files. Multiple files are not allowed except for appending (when holding Control).", fileCount).post((HWND)mhwnd, g_szError);
		return;
	}

	typedef vdvector<VDStringW> Filenames;
	Filenames filenames;

	for(UINT i=0; i<fileCount; ++i) {
		if (!VDIsWindowsNT()) {
			char szName[MAX_PATH];
			if (DragQueryFile(hdrop, i, szName, sizeof szName)) {
				const VDStringW s(VDTextAToW(szName));

				if (!s.empty())
					filenames.push_back_as(s);
			}
		} else {
			wchar_t szNameW[MAX_PATH];
			typedef UINT (APIENTRY *tpDragQueryFileW)(HDROP, UINT, LPWSTR, UINT);

			if (HMODULE hmod = GetModuleHandle("shell32"))
				if (const tpDragQueryFileW pDragQueryFileW = (tpDragQueryFileW)GetProcAddress(hmod, "DragQueryFileW")) {
					if (pDragQueryFileW(hdrop, i, szNameW, sizeof szNameW / sizeof szNameW[0])) {
						if (szNameW[0])
							filenames.push_back_as(szNameW);
					}
				}

		}
	}

	if (!filenames.empty()) {
		try {
			VDAutoLogDisplay logDisp;

			if (isAppend) {
				for(Filenames::const_iterator it(filenames.begin()), itEnd(filenames.end()); it != itEnd; ++it) {
					const VDStringW& filename = *it;

					AppendAVI(filename.c_str());
				}
			} else {
				CmdOpen(filenames.front().c_str());
			}

			logDisp.Post(mhwnd);
		} catch(const MyError& e) {
			e.post((HWND)mhwnd, g_szError);
		}
	}
}

void VDProjectUI::OnGetMinMaxInfo(MINMAXINFO& mmi) {
	int minHeight = 0;

	if (mbStatusBarVisible && mhwndStatus) {
		RECT r;
		if (GetWindowRect(mhwndStatus, &r))
			minHeight += r.bottom - r.top;
	}

	if (mbPositionControlVisible && mhwndPosition) {
		RECT r;
		if (GetWindowRect(mhwndPosition, &r))
			minHeight += r.bottom - r.top;
	}

	RECT r2;
	r2.left = 0;
	r2.right = 0;
	r2.top = 0;
	r2.bottom = minHeight;
	AdjustWindowRect(&r2, ::GetWindowLong((HWND)mhwnd, GWL_STYLE), ::GetMenu((HWND)mhwnd) != NULL);

	minHeight = r2.bottom - r2.top;

	if (mmi.ptMinTrackSize.y < minHeight)
		mmi.ptMinTrackSize.y = minHeight;
}

void VDProjectUI::UpdateMaximize() {
	int style0 = GetWindowLong((HWND)mhwnd,GWL_STYLE);
	int style1 = style0;

	if(mbMaximize && IsZoomed((HWND)mhwnd)){
		style1 &= ~(WS_CAPTION|WS_SYSMENU|WS_THICKFRAME);
	} else {
		style1 |= (WS_CAPTION|WS_SYSMENU|WS_THICKFRAME);
	}

	if(style1!=style0){
		HMONITOR mon = MonitorFromWindow((HWND)mhwnd,MONITOR_DEFAULTTONEAREST);
		MONITORINFO info = {sizeof(MONITORINFO)};
		GetMonitorInfo(mon,&info);
		RECT rr = info.rcWork;
		int x = rr.left;
		int y = rr.top;
		int w = rr.right-rr.left;
		int h = rr.bottom-rr.top;
		mbMaximizeChanging = true;
		SetWindowLong((HWND)mhwnd,GWL_STYLE,style1);
		SetWindowPos((HWND)mhwnd,0,x,y,w,h,SWP_NOACTIVATE|SWP_NOZORDER|SWP_FRAMECHANGED);
		mbMaximizeChanging = false;
	}
}

void VDProjectUI::OnSize() {
	RECT rClient;

	GetClientRect((HWND)mhwnd, &rClient);

	int w = rClient.right;
	int y = rClient.bottom;
	bool dock_status = mbMaximize && w>1200;
	if (!mbPositionControlVisible) dock_status = false;
	if (!mpPosition) dock_status = false;

	if (mbStatusBarVisible && !dock_status) {
		HWND hwnd = mhwndStatus;
		RECT r;
		GetWindowRect(hwnd, &r);
		int dy = r.bottom - r.top;
		y -= dy;
		SetWindowPos(hwnd, NULL, 0, y, w, dy, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOCOPYBITS);
	}
	if (mbPositionControlVisible) {
		HWND hwnd = mhwndPosition;
		RECT r;
		GetWindowRect(hwnd, &r);
		int dy = r.bottom - r.top;
		SetWindowPos(hwnd, NULL, 0, y-dy, w, dy, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOCOPYBITS);

		if (dock_status) {
			RECT fr;
			RECT sr;
			GetWindowRect(GetDlgItem(mhwndPosition,501),&fr);
			GetWindowRect(mhwndStatus, &sr);
			int sx = fr.right-r.left+10;
			int sdy = sr.bottom - sr.top;
			SetWindowPos(mhwndStatus, NULL, sx, y-sdy, w-sx, sdy, SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOCOPYBITS);
		}

		y -= dy;
	}

	if (mpUIBase)
		mpUIBase->Layout(vduirect(0, 0, rClient.right, y));

	RepositionPanes(true);

	int nParts = SendMessage(mhwndStatus, SB_GETPARTS, 0, 0);
	if (nParts > 1) {
		enum { kMaxStatusParts = 8 };
		INT aWidth[kMaxStatusParts];
		VDASSERT(nParts <= kMaxStatusParts);

		RECT rStatus;
		GetWindowRect(mhwndStatus, &rStatus);
		int xCoord = (rStatus.right-rStatus.left) - (rStatus.bottom-rStatus.top);

		aWidth[nParts-2] = xCoord;

		for(int i=nParts-3; i>=0; i--) {
			xCoord -= 60;
			aWidth[i] = xCoord;
		}
		aWidth[nParts-1] = -1;

		SendMessage(mhwndStatus, SB_SETPARTS, nParts, (LPARAM)aWidth);
	}
}

void VDProjectUI::OnPositionNotify(int code) {
	VDPosition pos;
	switch(code) {
	case PCN_BEGINTRACK:
		guiSetStatus("Seeking: hold SHIFT to snap to keyframes", 255);
		mpPosition->SetAutoPositionUpdate(false);
		mbLockPreviewRestart = true;
		break;
	case PCN_ENDTRACK:
		//guiSetStatus("", 255);
		UINotifySelection();
		mPreviewRestartPosition = mpPosition->GetPosition();
		mpPosition->SetAutoPositionUpdate(true);
		mbLockPreviewRestart = false;
		break;
	case PCN_THUMBPOSITION:
	case PCN_THUMBPOSITIONPREV:
	case PCN_THUMBPOSITIONNEXT:
	case PCN_THUMBTRACK:
	case PCN_PAGELEFT:
	case PCN_PAGERIGHT:
		pos = mpPosition->GetPosition();

		if (inputVideo) {
			if (GetKeyState(VK_SHIFT)<0) {
				if (code == PCN_THUMBPOSITIONNEXT)
					MoveToNearestKeyNext(pos);
				else
					MoveToNearestKey(pos);

				pos = GetCurrentFrame();

				if (code == PCN_THUMBTRACK)
					mpPosition->SetDisplayedPosition(pos);
				else
					mpPosition->SetPosition(pos);
			} else if (pos >= 0) {
				if (code == PCN_THUMBTRACK)
					mpPosition->SetDisplayedPosition(pos);

				MoveToFrame(pos);
			}
		}
		break;
	}
}

void VDProjectUI::ToggleFullscreen() {
	IVDVideoWindow *w1 = VDGetIVideoWindow(mhwndInputFrame);
	IVDVideoWindow *w2 = VDGetIVideoWindow(mhwndOutputFrame);

	if (w1->IsFullscreen()) {
		w1->ToggleFullscreen();
		return;
	}

	if (w2->IsFullscreen()) {
		w2->ToggleFullscreen();
		return;
	}

	switch(mPaneLayoutMode) {
	case kPaneLayoutInput:
		w1->ToggleFullscreen();
		break;

	case kPaneLayoutOutput:
	case kPaneLayoutDual:
		w2->ToggleFullscreen();
		break;
	}
}

void VDProjectUI::SwapFullscreen() {
	IVDVideoWindow *w1 = VDGetIVideoWindow(mhwndInputFrame);
	IVDVideoWindow *w2 = VDGetIVideoWindow(mhwndOutputFrame);

	switch(mPaneLayoutMode) {
	case kPaneLayoutDual:
		if (w1->IsFullscreen()) {
			w2->ToggleFullscreen();
			return;
		}
		if (w2->IsFullscreen()) {
			w1->ToggleFullscreen();
			return;
		}
		break;
	}
}

void VDProjectUI::ResetCentering() {
	IVDVideoWindow *w1 = VDGetIVideoWindow(mhwndInputFrame);
	IVDVideoWindow *w2 = VDGetIVideoWindow(mhwndOutputFrame);
	w1->SetPanCentering(g_panCentering);
	w2->SetPanCentering(g_panCentering);
}

void VDProjectUI::RepositionPanes(bool reset) {
	VDASSERT(!mbPaneLayoutBusy);
	mbPaneLayoutBusy = true;

	HWND panes[2];
	int n = 0;

	RECT rWork;
	rWork.left = 0;
	rWork.top = 0;
	vdsize32 size = mpUIPaneSet->GetArea().size();
	rWork.right = size.w;
	rWork.bottom = size.h;

	switch(mPaneLayoutMode) {
		case kPaneLayoutInput:
			panes[n++] = mhwndInputFrame;
			break;

		case kPaneLayoutOutput:
			panes[n++] = mhwndOutputFrame;
			break;

		case kPaneLayoutDual:
			if (g_fSwapPanes) {
				panes[n++] = mhwndOutputFrame;
				panes[n++] = mhwndInputFrame;
			} else {
				panes[n++] = mhwndInputFrame;
				panes[n++] = mhwndOutputFrame;
			}
			break;
	}

	const int n2 = n;
	n = 0;

	for(int i=0; i<n2; ++i) {
		HWND h = panes[i];
		if (h && GetParent(h)==(HWND)mhwnd) {
			panes[n++] = h;
			IVDVideoWindow *w = VDGetIVideoWindow(h);
			w->SetWorkArea(rWork, true);
			w->SyncMonitorChange();
			if (reset) {
				w->SetAutoSize(mbAutoSizePanes);
			} else {
				if (!w->GetAutoSize())
					mbAutoSizePanes = false;
			}
		}
	}

	if (mbAutoSizePanes) {
		vdsize32 size = mpUIPaneSet->GetArea().size();

		if (size.w && size.h) {
			double weightTotal = 0;
			double weights[2];

			for(int i=0; i<n; ++i) {
				IVDVideoWindow *w = VDGetIVideoWindow(panes[i]);

				int srcw;
				int srch;
				w->GetSourceSize(srcw, srch);

				if (g_vertical) {
					weights[i] = (double)(srch + 8);
					weightTotal += (double)(srch + 8);
				} else {
					const VDFraction srcpar = w->GetSourcePAR();

					double srcw2 = (double)srcw;
					if (srcpar.getLo())
						srcw2 *= srcpar.asDouble();

					weights[i] = (double)(srcw2 + 8);
					weightTotal += (double)(srcw2 + 8);
				}
			}

			if (weightTotal > 0) {
				for(int i=0; i<n; ++i) {
					IVDVideoWindow *w = VDGetIVideoWindow(panes[i]);
					vdsize32 s1 = size;
					if (g_vertical)
						s1.h = VDFloorToInt((double)size.h * weights[i] / weightTotal);
					else
						s1.w = VDFloorToInt((double)size.w * weights[i] / weightTotal);

					w->SetZoom(w->GetMaxZoomForArea(s1.w, s1.h), n==1);
				}

				// move all at once
				int x = 0;
				int y = 0;

				for(int i=0; i<n; ++i) {
					HWND hwndPane = panes[i];

					RECT r;
					GetWindowRect(hwndPane, &r);

					IVDVideoWindow *w = VDGetIVideoWindow(hwndPane);
					w->Move(x, y);

					if (g_vertical)
						y += r.bottom - r.top;
					else
						x += r.right - r.left;
				}
			}
		}
	} else {
		// move & size in order
		int x = 0;
		int y = 0;

		for(int i=0; i<n; ++i) {
			HWND hwndPane = panes[i];

			IVDVideoWindow *w = VDGetIVideoWindow(hwndPane);
			w->Move(x, y);
			if (reset || i>0) w->Resize(true);

			RECT r;
			GetWindowRect(hwndPane, &r);

			if (g_vertical)
				y += r.bottom - r.top;
			else
				x += r.right - r.left;
		}
	}

	mbPaneLayoutBusy = false;
}

void VDProjectUI::UpdateVideoFrameLayout() {
	bool inputVisible = (mPaneLayoutMode != kPaneLayoutOutput) && inputVideo;
	bool outputVisible = (mPaneLayoutMode != kPaneLayoutInput) && inputVideo;

	if (inputVideo) {
		const VDAVIBitmapInfoHeader *formatIn = inputVideo->getImageFormat();

		if (formatIn) {
			const VDPixmap& px = inputVideo->getTargetFormat();
			int w0 = px.w;
			int h0 = px.h;
			int w = w0;
			int h = h0;
			VDFraction inputPAR(inputVideo->getPixelAspectRatio());

			IVDVideoWindow *inputWin = VDGetIVideoWindow(mhwndInputFrame);
			inputWin->SetSourcePAR(inputPAR);
			inputWin->SetSourceSize(w, h);

			// figure out output size too

			int w2 = w0, h2 = h0;
			VDFraction outputPAR(inputPAR);

			if (!g_filterChain.IsEmpty()) {
				if (!filters.isRunning()) {
					IVDStreamSource *pVSS = inputVideo->asStream();
					VDPixmapFormatEx format = px;
					if (!px.format)	format = nsVDPixmap::kPixFormat_XRGB8888;
					filters.prepareLinearChain(&g_filterChain, w0, h0, format, pVSS->getRate(), pVSS->getLength(), inputVideo->getPixelAspectRatio());
				}

				const VDPixmapLayout& output = filters.GetOutputLayout();
				w2 = output.w;
				h2 = output.h;
				outputPAR = filters.GetOutputPixelAspect();
			}

			IVDVideoWindow *outputWin = VDGetIVideoWindow(mhwndOutputFrame);
			outputWin->SetSourcePAR(outputPAR);
			outputWin->SetSourceSize(w2, h2);
		}
	}

	if (inputVisible && !mbFiltersPreview)
		ShowWindow(mhwndInputFrame, SW_SHOWNOACTIVATE);
	else {
		IVDVideoWindow *w = VDGetIVideoWindow(mhwndInputFrame);
		if (w->IsFullscreen()) w->ToggleFullscreen();
		ShowWindow(mhwndInputFrame, SW_HIDE);
	}

	if (outputVisible && !mbFiltersPreview)
		ShowWindow(mhwndOutputFrame, SW_SHOWNOACTIVATE);
	else {
		IVDVideoWindow *w = VDGetIVideoWindow(mhwndOutputFrame);
		if (w->IsFullscreen()) w->ToggleFullscreen();
		ShowWindow(mhwndOutputFrame, SW_HIDE);
	}
}

void VDProjectUI::OpenAudioDisplay() {
	if (mpAudioDisplay)
		return;

	mpUIAudioSplitBar = VDCreateUISplitBar();
	mpUISplitSet->AddChild(mpUIAudioSplitBar);
	VDUIParameters parms;
	parms.Clear();
	parms.SetB(nsVDUI::kUIParam_IsVertical, false);
	mpUIAudioSplitBar->SetAlignment(nsVDUI::kFill, nsVDUI::kTop);
	mpUIAudioSplitBar->Create(&parms);

	HWND hwndParent = vdpoly_cast<IVDUIWindowW32 *>(mpUIBase)->GetHandleW32();
	mhwndAudioDisplay = CreateWindowEx(WS_EX_STATICEDGE, g_szAudioDisplayControlName, "", WS_CHILD|WS_VISIBLE, 0, 0, 0, 0, (HWND)hwndParent, NULL, GetModuleHandle(NULL), NULL);
	mpUIAudioDisplay = VDUICreatePeer((VDGUIHandle)mhwndAudioDisplay);
	mpUIAudioDisplay->SetAlignment(nsVDUI::kFill, nsVDUI::kFill);
	mpUISplitSet->AddChild(mpUIAudioDisplay);

	const vduirect rPeer(mpUIPeer->GetClientArea());
	mpUIAudioDisplay->SetArea(vduirect(0, 0, rPeer.width(), rPeer.height() / 3));

	mpAudioDisplay = VDGetIUIAudioDisplayControl((VDGUIHandle)mhwndAudioDisplay);

	mpAudioDisplay->AudioRequiredEvent() += mAudioDisplayUpdateRequiredDelegate(this, &VDProjectUI::OnAudioDisplayUpdateRequired);
	mpAudioDisplay->SetSelectStartEvent() += mAudioDisplaySetSelectStartDelegate(this, &VDProjectUI::OnAudioDisplaySetSelect);
	mpAudioDisplay->SetSelectTrackEvent() += mAudioDisplaySetSelectTrackDelegate(this, &VDProjectUI::OnAudioDisplaySetSelect);
	mpAudioDisplay->SetSelectEndEvent() += mAudioDisplaySetSelectEndDelegate(this, &VDProjectUI::OnAudioDisplaySetSelect);
	mpAudioDisplay->TrackAudioOffsetEvent() += mAudioDisplayTrackAudioOffsetDelegate(this, &VDProjectUI::OnAudioDisplayTrackAudioOffset);
	mpAudioDisplay->SetAudioOffsetEvent() += mAudioDisplaySetAudioOffsetDelegate(this, &VDProjectUI::OnAudioDisplaySetAudioOffset);

	if (IsSelectionPresent())
		mpAudioDisplay->SetSelectedFrameRange(GetSelectionStartFrame(), GetSelectionEndFrame());

	UpdateAudioDisplay();
	OnSize();

	UpdateAudioDisplayPosition();

	VDRegistryAppKey key(g_szRegKeyPersistence);
	int zoom = key.getInt("Audio display: zoom", -1);
	if (zoom >= 1)
		mpAudioDisplay->SetZoom(zoom);
	int mode = key.getInt("Audio display: mode", -1);
	if ((unsigned)mode < (unsigned)IVDUIAudioDisplayControl::kModeCount)
		mpAudioDisplay->SetMode((IVDUIAudioDisplayControl::Mode)mode);
}

void VDProjectUI::CloseAudioDisplay() {
	if (mpUIAudioDisplay) {
		mpUISplitSet->RemoveChild(mpUIAudioSplitBar);
		mpUIAudioSplitBar->Shutdown();
		mpUIAudioSplitBar = NULL;
		mpUISplitSet->RemoveChild(mpUIAudioDisplay);
		mpUIAudioDisplay->Shutdown();
		mpUIAudioDisplay = NULL;
	}

	if (mpAudioDisplay) {
		VDRegistryAppKey key(g_szRegKeyPersistence);
		key.setInt("Audio display: zoom", mpAudioDisplay->GetZoom());
		key.setInt("Audio display: mode", mpAudioDisplay->GetMode());
		mpAudioDisplay = NULL;
	}

	if (mhwndAudioDisplay) {
		DestroyWindow(mhwndAudioDisplay);
		mhwndAudioDisplay = NULL;
	}

	// strange redraw problem....
	if (mpUIBase)
		InvalidateRect(vdpoly_cast<IVDUIWindowW32 *>(mpUIBase)->GetHandleW32(), NULL, TRUE);

	OnSize();
}

bool VDProjectUI::TickAudioDisplay() {
	if (!mbAudioDisplayReadActive)
		return false;

	char buf[4000];

	uint32 actualBytes = 0, actualSamples = 0;

	if (!inputAudio || !inputVideo) {
		UpdateAudioDisplay();
		return false;
	}

	const VDWaveFormat *wfex = inputAudio->getWaveFormat();
	if (!is_audio_pcm8(wfex) && !is_audio_pcm16(wfex) && !is_audio_float(wfex)) {
		UpdateAudioDisplay();
		return false;
	}

	uint32 maxlen = sizeof buf;
	sint64 apos = mAudioDisplayPosNext;

	const FrameSubset& subset = GetTimeline().GetSubset();

	IVDStreamSource *pVSS = inputVideo->asStream();
	double audioToVideoFactor = pVSS->getRate().asDouble() / inputAudio->getRate().asDouble();
	double vframef = mAudioDisplayPosNext * audioToVideoFactor;
	sint64 vframe = VDFloorToInt64(vframef);
	double vframeoff = vframef - vframe;

	for(;;) {
		sint64 len = 1;
		sint64 vframesrc = subset.lookupRange(vframe, len);

		if (vframesrc < 0) {
			sint64 vend;
			if (g_dubOpts.audio.fEndAudio || subset.empty() || (vend = subset.back().end()) != pVSS->getEnd()) {
				mbAudioDisplayReadActive = false;
				return false;
			}

			apos = VDRoundToInt64((double)(vend + vframef - subset.getTotalFrames()) / audioToVideoFactor);
		} else {
			apos = VDRoundToInt64((double)(vframesrc + vframeoff) / audioToVideoFactor);
			sint64 alimit = VDRoundToInt64((double)(vframesrc + len) / audioToVideoFactor);

			maxlen = VDClampToUint32(alimit - apos);
		}

		// check for roundoff errors when we're very close to the end of a frame
		if (maxlen)
			break;

		++vframe;
		vframeoff = 0;

	}

	apos -= inputAudio->msToSamples(g_dubOpts.audio.offset);

	// avoid Avisynth buffer overflow bug
	uint32 nBlockAlign = wfex->mBlockSize;
	if (nBlockAlign)
		maxlen = std::min<uint32>(maxlen, sizeof buf / nBlockAlign);

	if (apos < 0) {
		uint32 count = maxlen;

		if (apos + count > 0)
			count = -(sint32)apos;

		if (wfex->mSampleBits == 8)
			VDMemset8(buf, 0x80, wfex->mChannels * count);
		if (wfex->mSampleBits == 16)
			VDMemset16(buf, 0, wfex->mChannels * count);
		if (wfex->mSampleBits == 32)
			VDMemset32(buf, 0, wfex->mChannels * count);

		actualSamples = count;
		actualBytes = wfex->mBlockSize * count;
	} else {
		if (inputAudio->read(apos, maxlen, buf, sizeof buf, &actualBytes, &actualSamples) || !actualSamples) {
			mbAudioDisplayReadActive = false;
			return false;
		}
	}
	mAudioDisplayPosNext += actualSamples;

	bool needMore = false;
	if (wfex->mSampleBits == 8)
		needMore = mpAudioDisplay->ProcessAudio8U((const uint8 *)buf, actualSamples, 1, wfex->mBlockSize);
	if (wfex->mSampleBits == 16)
		needMore = mpAudioDisplay->ProcessAudio16S((const sint16 *)buf, actualSamples, 2, wfex->mBlockSize);
	if (wfex->mSampleBits == 32)
		needMore = mpAudioDisplay->ProcessAudioF((const float *)buf, actualSamples, 4, wfex->mBlockSize);

	if (needMore)
		return true;

	mbAudioDisplayReadActive = false;
	return false;
}

void VDProjectUI::UpdateAudioDisplay() {
	if (!mpAudioDisplay)
		return;

	// Right now, we can't display the audio display if there is no _video_ track, since we have
	// frame markers. Besides, there wouldn't be any way for you to move.
	if (!inputAudio || !inputVideo) {
		mpAudioDisplay->SetFailureMessage(L"Audio display is disabled because there is no audio track.");
		mbAudioDisplayReadActive = false;
		return;
	}

	const VDWaveFormat *wfex = inputAudio->getWaveFormat();
	if (!is_audio_pcm(wfex) && !is_audio_float(wfex)) {
		mpAudioDisplay->SetFailureMessage(L"Audio display is disabled because the audio track is compressed.");
		mbAudioDisplayReadActive = false;
		return;
	}

	if (!is_audio_pcm8(wfex) && !is_audio_pcm16(wfex) && !is_audio_float(wfex)) {
		mpAudioDisplay->SetFailureMessage(L"Audio display is disabled because the audio track uses an unsupported PCM format.");
		mbAudioDisplayReadActive = false;
		return;
	}

	mpAudioDisplay->SetFormat((double)wfex->mSamplingRate, wfex->mChannels);
	mpAudioDisplay->ClearFailureMessage();
}

void VDProjectUI::UpdateAudioDisplayPosition() {
	if (inputAudio && mpAudioDisplay) {
		IVDStreamSource *pVSS = inputVideo->asStream();
		VDPosition pos = GetCurrentFrame();
		VDPosition cenpos = inputAudio->TimeToPositionVBR(pVSS->PositionToTimeVBR(pos));

		double audioPerVideoSamples = inputAudio->getRate().asDouble() / pVSS->getRate().asDouble();

		mpAudioDisplay->SetFrameMarkers(0, VDCeilToInt64(inputAudio->getLength() / audioPerVideoSamples), 0.0, audioPerVideoSamples);
		//mpAudioDisplay->SetHighlightedFrameMarker(pos);
		mpAudioDisplay->SetPosition(cenpos,pos);
		mAudioDisplayPosNext = mpAudioDisplay->GetReadPosition();
	}
}

void VDProjectUI::OpenCurveEditor() {
	if (mpCurveEditor)
		return;

	mpUICurveSplitBar = VDCreateUISplitBar();
	mpUISplitSet->AddChild(mpUICurveSplitBar);
	VDUIParameters parms;
	parms.Clear();
	parms.SetB(nsVDUI::kUIParam_IsVertical, false);
	mpUICurveSplitBar->SetAlignment(nsVDUI::kFill, nsVDUI::kTop);
	mpUICurveSplitBar->Create(&parms);

	mpUICurveSet = VDCreateUISet();
	mpUISplitSet->AddChild(mpUICurveSet);

	parms.SetB(nsVDUI::kUIParam_IsVertical, true);
	mpUICurveSet->SetAlignment(nsVDUI::kFill, nsVDUI::kFill);
	mpUICurveSet->Create(&parms);

	mpUICurveComboBox = VDCreateUIComboBox();
	mpUICurveSet->AddChild(mpUICurveComboBox);
	parms.Clear();
	mpUICurveComboBox->SetAlignment(nsVDUI::kFill, nsVDUI::kTop);
	mpUICurveComboBox->Create(&parms);
	mpUICurveComboBox->SetID(100);

	HWND hwndParent = vdpoly_cast<IVDUIWindowW32 *>(mpUIBase)->GetHandleW32();
	mhwndCurveEditor = CreateWindowEx(WS_EX_STATICEDGE, g_VDParameterCurveControlClass, "", WS_CHILD|WS_VISIBLE, 0, 0, 0, 0, (HWND)hwndParent, NULL, GetModuleHandle(NULL), NULL);
	mpUICurveEditor = VDUICreatePeer((VDGUIHandle)mhwndCurveEditor);
	mpUICurveEditor->SetAlignment(nsVDUI::kFill, nsVDUI::kFill);
	mpUICurveSet->AddChild(mpUICurveEditor);

	const vduirect rPeer(mpUIPeer->GetClientArea());
	mpUICurveSet->SetArea(vduirect(0, 0, rPeer.width(), rPeer.height() / 3));

	mpCurveEditor = VDGetIUIParameterCurveControl((VDGUIHandle)mhwndCurveEditor);
	mpCurveEditor->CurveUpdatedEvent() += mCurveUpdatedDelegate(this, &VDProjectUI::OnCurveUpdated);
	mpCurveEditor->StatusUpdatedEvent() += mCurveStatusUpdatedDelegate(this, &VDProjectUI::OnCurveStatusUpdated);

	UpdateCurveList();
	UpdateCurveEditorPosition();
	OnSize();
}

void VDProjectUI::CloseCurveEditor() {
	if (mpUICurveEditor) {
		mpUISplitSet->RemoveChild(mpUICurveSplitBar);
		mpUICurveSplitBar->Shutdown();
		mpUICurveSplitBar = NULL;
		mpUISplitSet->RemoveChild(mpUICurveSet);
		mpUICurveSet->Shutdown();
		mpUICurveEditor = NULL;
		mpUICurveComboBox = NULL;
		mpUICurveSet = NULL;
	}

	mpCurveEditor = NULL;
	if (mhwndCurveEditor) {
		DestroyWindow(mhwndCurveEditor);
		mhwndCurveEditor = NULL;
	}

	OnSize();
}

void VDProjectUI::UpdateCurveList() {
	VDParameterCurve *pcSelected = NULL;

	if (mpCurveEditor)
		pcSelected = mpCurveEditor->GetCurve();

	IVDUIList *pList = vdpoly_cast<IVDUIList *>(mpUICurveComboBox);
	if (pList) {
		mpUICurveComboBox->SetValue(-1);
		pList->Clear();

		bool curvesFound = false;
		int index = 1;
		int comboIndex = 0;
		int currentSelect = -1;

		for(VDFilterChainDesc::Entries::const_iterator it(g_filterChain.mEntries.begin()), itEnd(g_filterChain.mEntries.end());
			it != itEnd;
			++it)
		{
			FilterInstance *fa = (*it)->mpInstance;

			VDParameterCurve *pc = fa->GetAlphaParameterCurve();
			if (pc) {
				const char *name = fa->GetName();
				pList->AddItem(VDswprintf(L"Video filter %d: %hs (Opacity curve)", 2, &index, &name).c_str(), (uintptr)index);
				curvesFound = true;

				if (pc == pcSelected)
					currentSelect = comboIndex;
				else if (currentSelect < 0)
					currentSelect = 0;

				++comboIndex;
			}

			++index;
		}

		if (!curvesFound)
			pList->AddItem(L"There are no video filters with parameter curves.", NULL);

		mpUICurveComboBox->SetEnabled(curvesFound);

		if (currentSelect >= 0) {
			mpUICurveComboBox->SetValue(currentSelect);
		} else {
			mpCurveEditor->SetCurve(NULL);
			mpUICurveComboBox->SetValue(0);
			currentSelect = 0;
		}

		HandleUIEvent(NULL, mpUICurveComboBox, 100, kEventSelect, currentSelect);
	}
}

void VDProjectUI::UpdateCurveEditorPosition() {
	if (!mpCurveEditor)
		return;

	int selIndex = mpUICurveComboBox->GetValue();

	if (selIndex >= 0)
		selIndex = vdpoly_cast<IVDUIList *>(mpUICurveComboBox)->GetItemData(selIndex) - 1;

	if (selIndex >= 0) {
		if (!filters.isRunning()) {
			try {
				StartFilters();

				if (!filters.isRunning())
					return;
			} catch(const MyError&) {
				return;
			}
		}

		FilterInstance *selected = NULL;

		for(VDFilterChainDesc::Entries::const_iterator it(g_filterChain.mEntries.begin()), itEnd(g_filterChain.mEntries.end());
			it != itEnd;
			++it)
		{
			FilterInstance *fa = (*it)->mpInstance;

			if (!selIndex--)
				selected = fa;
		}

		if (selected) {
			sint64 timelineFrame = GetCurrentFrame();

			if (timelineFrame >= mTimeline.GetLength())
				--timelineFrame;

			sint64 outFrame = mTimeline.TimelineToSourceFrame(timelineFrame);

			if (outFrame >= 0) {
				sint64 symFrame = filters.GetSymbolicFrame(outFrame, selected);

				if (symFrame >= 0)
					mpCurveEditor->SetPosition(symFrame);
			}
		}
	}
}

void VDProjectUI::UIRefreshInputFrame(const VDPixmap *px) {
	if (mPaneLayoutMode == kPaneLayoutOutput)
		return;

	IVDVideoDisplay *pDisp = VDGetIVideoDisplay((VDGUIHandle)mhwndInputDisplay);
	if (px) {
		pDisp->SetSource(true, *px);
	} else {
		pDisp->SetSourceSolidColor(0xFF000000 + (VDSwizzleU32(GetSysColor(COLOR_APPWORKSPACE) & 0xFFFFFF) >> 8));
	}
}

void VDProjectUI::UIRefreshOutputFrame(const VDPixmap *px) {
	if (mPaneLayoutMode == kPaneLayoutInput)
		return;

	IVDVideoDisplay *pDisp = VDGetIVideoDisplay((VDGUIHandle)mhwndOutputDisplay);
	if (px) {
		pDisp->SetSource(true, *px);
	} else {
		pDisp->SetSourceSolidColor(0xFF000000 + (VDSwizzleU32(GetSysColor(COLOR_APPWORKSPACE) & 0xFFFFFF) >> 8));
	}
}

void VDProjectUI::UISetDubbingMode(bool bActive, bool bIsPreview) {
	mbDubActive = bActive;

	if (bActive) {
		if (!bIsPreview) mpPosition->SetMessage(L"(dub in progress)");
		UpdateVideoFrameLayout();

		mpInputDisplay->SetAccelerationMode(IVDVideoDisplay::kAccelResetInForeground);
		mpOutputDisplay->SetAccelerationMode(IVDVideoDisplay::kAccelResetInForeground);

		if (mPaneLayoutMode == kPaneLayoutInput || mPaneLayoutMode == kPaneLayoutDual)
			g_dubber->SetInputDisplay(mpInputDisplay);
		else
			g_dubber->SetInputDisplay(NULL);

		if (mPaneLayoutMode == kPaneLayoutOutput || mPaneLayoutMode == kPaneLayoutDual)
			g_dubber->SetOutputDisplay(mpOutputDisplay);
		else
			g_dubber->SetOutputDisplay(NULL);

		SetMenu((HWND)mhwnd, mhMenuDub);

		VDUIFrame *pFrame = VDUIFrame::GetFrame((HWND)mhwnd);
		pFrame->SetAccelTable(mhAccelDub);

		mpWndProc = &VDProjectUI::DubWndProc;
	} else if (mPreviewRestartMode==kPreviewRestart_None) {
		SetMenu((HWND)mhwnd, mhMenuNormal);
		UpdateMRUList();

		VDUIFrame *pFrame = VDUIFrame::GetFrame((HWND)mhwnd);
		pFrame->SetAccelTable(mhAccelMain);

		mpWndProc = &VDProjectUI::MainWndProc;

		if (inputAVI) {
			const wchar_t *s = VDFileSplitPath(g_szInputAVIFile);

			SetTitle(kVDM_TitleFileLoaded, 1, &s);
		} else {
			SetTitle(kVDM_TitleIdle, 0);
		}

		// reset video displays
		mLastDisplayedInputFrame = -1;
		mLastDisplayedTimelineFrame = -1;
		mpInputDisplay->SetAccelerationMode(IVDVideoDisplay::kAccelOnlyInForeground);
		mpOutputDisplay->SetAccelerationMode(IVDVideoDisplay::kAccelOnlyInForeground);
		DisplayFrame();
		UICurrentPositionUpdated(false);
	} else {
		mpWndProc = &VDProjectUI::MainWndProc;
		mLastDisplayedInputFrame = -1;
		mLastDisplayedTimelineFrame = -1;
		mpInputDisplay->SetAccelerationMode(IVDVideoDisplay::kAccelOnlyInForeground);
		mpOutputDisplay->SetAccelerationMode(IVDVideoDisplay::kAccelOnlyInForeground);
		DisplayFrame();
	}
}

bool VDProjectUI::UIRunDubMessageLoop() {
	MSG msg;

	VDSamplingAutoProfileScope autoProfileScope;

	/*
	while(g_dubber->isRunning()) {
		// TODO: PerfHUD 5 doesn't hook GetMessage() and doesn't work unless you use PeekMessage().
		//       Confirm if this is OK to switch.
#if 0
		BOOL result = GetMessage(&msg, (HWND) NULL, 0, 0);

		if (result == (BOOL)-1)
			break;

		if (!result) {
			PostQuitMessage(msg.wParam);
			return false;
		}
#else
		if (!PeekMessage(&msg, (HWND) NULL, 0, 0, QS_ALLINPUT)) {
			if (!PeekMessage(&msg, (HWND) NULL, 0, 0, PM_REMOVE)) {
				WaitMessage();
				continue;
			}

			if (msg.message == WM_QUIT) {
				PostQuitMessage(msg.wParam);
				break;
			}
		}
#endif

		if (guiCheckDialogs(&msg))
			continue;

		if (VDUIFrame::TranslateAcceleratorMessage(msg))
			continue;

		TranslateMessage(&msg); 
		DispatchMessage(&msg); 
	}
	*/

	bool dubRunning = true;

	while(1) {
		if (dubRunning) {
			dubRunning = g_dubber->isRunning();
			// assuming if it is not running the messages still need to reach displays
		} else {
			bool finished = true;
			if (mpInputDisplay->IsFramePending()) finished = false;
			if (mpOutputDisplay->IsFramePending()) finished = false;
			if (finished) break;
			// process until posted buffers are actually painted on screen
		}

		if(!PeekMessage(&msg,0,0,0,PM_NOREMOVE)){
			WaitMessage();
		}

		bool process_pane0 = false;
		bool process_pane1 = false;
		unsigned int MYWM_PROCESSNEXTFRAME = WM_USER+0x105;

		while(PeekMessage(&msg,0,0,0,PM_NOREMOVE)){
			BOOL result = GetMessage(&msg, (HWND) NULL, 0, 0);

			if (result == (BOOL)-1)
				break;

			if (!result) {
				PostQuitMessage(msg.wParam);
				return false;
			}

			if (guiCheckDialogs(&msg))
				continue;

			if (VDUIFrame::TranslateAcceleratorMessage(msg))
				continue;

			//! workaround:
			// for some reason these messages are generated in huge bursts and hog message queue
			if(msg.hwnd==mhwndInputDisplay && msg.message==MYWM_PROCESSNEXTFRAME){
				process_pane0 = true;
				continue;
			}
			if(msg.hwnd==mhwndOutputDisplay && msg.message==MYWM_PROCESSNEXTFRAME){
				process_pane1 = true;
				continue;
			}

			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}

		if(process_pane0) SendMessage(mhwndInputDisplay, MYWM_PROCESSNEXTFRAME, 0, 0);
		if(process_pane1) SendMessage(mhwndOutputDisplay, MYWM_PROCESSNEXTFRAME, 0, 0);
	}

	return true;
}

void VDProjectUI::UIAbortDubMessageLoop() {
	PostThreadMessage(mThreadId, WM_NULL, 0, 0);
}

void VDProjectUI::UICurrentPositionUpdated(bool fast_update) {
	VDPosition pos(GetCurrentFrame());

	mpPosition->SetPosition(pos);

	if (!fast_update) {
		if (mhwndCurveEditor) {
			UpdateCurveEditorPosition();
			RedrawWindow(mhwndCurveEditor,0,0,RDW_UPDATENOW);
		}

		if (mpAudioDisplay && !mbDubActive) {
			UpdateAudioDisplayPosition();
			for(int i=0; i<10; i++) if(!TickAudioDisplay()) break;
			RedrawWindow(mhwndAudioDisplay,0,0,RDW_UPDATENOW);
		}
	}
}

void VDProjectUI::UITimelineUpdated() {
	if (!inputAVI) return;

	if (mbZoomEnabled) {
		mpPosition->SetRangeZoom(true);
		mpPosition->SetRange(mposZoomStart, mposZoomEnd);
	} else {
		mpPosition->SetRangeZoom(false);
		mpPosition->SetRange(0, g_project->GetFrameCount());
	}

	VDPosition start(GetSelectionStartFrame());
	VDPosition end(GetSelectionEndFrame());
	mpPosition->SetSelection(start, end);
	mpPosition->SetTimeline(mTimeline);

	if (mpAudioDisplay) {
		UpdateAudioDisplay();
		mpAudioDisplay->Rescan();
	}
}

void VDProjectUI::UISelectionUpdated(bool notifyUser) {
	VDPosition start(GetSelectionStartFrame());
	VDPosition end(GetSelectionEndFrame());
	mpPosition->SetSelection(start, end);

	if (mpAudioDisplay) {
		if (IsSelectionPresent())
			mpAudioDisplay->SetSelectedFrameRange(start, end);
		else
			mpAudioDisplay->ClearSelectedFrameRange();
	}

	if (notifyUser)
		UINotifySelection();
}

void VDProjectUI::UINotifySelection() {
	VDPosition start(GetSelectionStartFrame());
	VDPosition end(GetSelectionEndFrame());
	if (start <= end)
		guiSetStatus("Selecting frames %u-%u (%u frames)", 255, (unsigned)start, (unsigned)end, (unsigned)(end - start));
	else
		guiSetStatus("", 255);
}

void VDProjectUI::UIShuttleModeUpdated() {
	if (!mSceneShuttleMode && !mShuttleMode)
		mpPosition->ResetShuttle();
}

void VDProjectUI::UISourceFileUpdated() {
	if (inputAVI) {
		if (g_szInputAVIFile[0] && !g_bAutoTest)
			mMRUList.add(g_szInputAVIFile);

		UpdateMRUList();

		VDStringW fileName(VDFileSplitExtLeft(VDStringW(VDFileSplitPath(g_szInputAVIFile))));

		VDSetLastLoadSaveFileName(VDFSPECKEY_SAVEVIDEOFILE, (fileName + L".avi").c_str());
		VDSetLastLoadSaveFileName(kFileDialog_WAVAudioOut, (fileName + L".wav").c_str());
		VDSetLastLoadSaveFileName(kFileDialog_Project, (fileName + L".vdproject").c_str());
		VDSetLastLoadSaveFileName(VDFSPECKEY_SAVEIMAGEFILE, (fileName + L".png").c_str());

		bool isMP3 = inputAudio && inputAudio->getWaveFormat()->mTag == 0x55;
		VDSetLastLoadSaveFileName(kFileDialog_RawAudioOut, (fileName + (isMP3 ? L".mp3" : L".bin")).c_str());

		VDSetLastLoadSaveFileName(kFileDialog_GIFOut, (fileName + L".gif").c_str());
		VDSetLastLoadSaveFileName(kFileDialog_PNGOut, (fileName + L".png").c_str());

		const wchar_t *s = VDFileSplitPath(g_szInputAVIFile);

		SetTitle(kVDM_TitleFileLoaded, 1, &s);
	} else
		SetTitle(kVDM_TitleIdle, 0);

	UpdateAudioSourceMenu();
}

void VDProjectUI::UIAudioSourceUpdated() {
	if (mpAudioDisplay) {
		UpdateAudioDisplay();
		mpAudioDisplay->Rescan();
	}
}

void VDProjectUI::UIVideoSourceUpdated() {
	
	UpdateVideoFrameLayout();
}

void VDProjectUI::UIVideoFiltersUpdated() {
	UpdateVideoFrameLayout();

	UpdateCurveList();
}

void VDProjectUI::UIDubParametersUpdated() {
	mpPosition->SetFrameRate(mVideoInputFrameRate);
}

void VDProjectUI::UiDisplayPreferencesUpdated() {
	mpInputDisplay->Reset();
	mpOutputDisplay->Reset();
	DisplayFrame();
}

void VDProjectUI::UpdateMRUList() {
	HMENU hmenuFile = GetSubMenu(GetMenu((HWND)mhwnd), 0);
	union {
		MENUITEMINFOA a;
		MENUITEMINFOW w;
	} mii;
	char name2[MAX_PATH];
	int index=0;

#define WIN95_MENUITEMINFO_SIZE (offsetof(MENUITEMINFO, cch) + sizeof(UINT))

	memset(&mii, 0, sizeof mii);
#ifdef _WIN64
	mii.a.cbSize	= sizeof(MENUITEMINFO);		// AMD64 has hidden padding in the struct that keeps the above from working.... screw it
#else
	mii.a.cbSize	= WIN95_MENUITEMINFO_SIZE;
#endif
	for(;;) {
		mii.a.fMask			= MIIM_TYPE;
		mii.a.dwTypeData		= name2;
		mii.a.cch				= sizeof name2;

		if (!GetMenuItemInfo(hmenuFile, mMRUListPosition, TRUE, &mii.a))
			break;

		if (mii.a.fType & MFT_SEPARATOR)
			break;

		RemoveMenu(hmenuFile, mMRUListPosition, MF_BYPOSITION);
	}

	for(;;) {
		VDStringW name(mMRUList[index]);

		if (name.empty())
			break;

		// collapse name while it is too long

		if (name.length() > 60) {
			const wchar_t *t = name.c_str();
			size_t rootidx = VDFileSplitRoot(t) - t;
			size_t diridx = VDFileSplitFirstDir(t+rootidx) - t;
			size_t limitidx = VDFileSplitFirstDir(t+diridx) - t;

			if (diridx > rootidx && limitidx > diridx) {
				name.replace(rootidx, diridx-rootidx-1, L"...", 3);

				rootidx += 4;

				while(name.length() > 60) {
					const wchar_t *t = name.c_str();
					diridx = VDFileSplitFirstDir(t+rootidx) - t;
					limitidx = VDFileSplitFirstDir(t+diridx) - t;
					if (diridx <= rootidx || limitidx <= diridx)
						break;

					name.erase(rootidx, diridx-rootidx);
				}
			}
		}

		mii.a.fMask		= MIIM_TYPE | MIIM_STATE | MIIM_ID;
		mii.a.fType		= MFT_STRING;
		mii.a.fState	= MFS_ENABLED;
		mii.a.wID		= ID_MRU_FILE0 + index;

		const wchar_t *s = name.c_str();

		VDStringW name2;
		if (index < 10) {
			int shortcut = (index+1) % 10;
			name2.sprintf(L"&%d %s", shortcut, s);
		} else {
			name2 = s;
		}

		if (GetVersion() & 0x80000000) {
			VDStringA name2A(VDTextWToA(name2.c_str()));

			mii.a.dwTypeData	= (char *)name2A.c_str();
			mii.a.cch			= name2A.size() + 1;

			if (!InsertMenuItemA(hmenuFile, mMRUListPosition+index, TRUE, &mii.a))
				break;
		} else {
			mii.w.dwTypeData	= (wchar_t *)name2.c_str();
			mii.w.cch			= name2.size() + 1;

			if (!InsertMenuItemW(hmenuFile, mMRUListPosition+index, TRUE, &mii.w))
				break;
		}

		++index;
	}

	if (!index) {
		mii.a.fMask			= MIIM_TYPE | MIIM_STATE | MIIM_ID;
		mii.a.fType			= MFT_STRING;
		mii.a.fState		= MFS_GRAYED;
		mii.a.wID			= ID_MRU_FILE0;
		mii.a.dwTypeData	= "Recent file list";
		mii.a.cch			= sizeof name2;

		InsertMenuItem(hmenuFile, mMRUListPosition+index, TRUE, &mii.a);
	}

	DrawMenuBar((HWND)mhwnd);
}

void VDProjectUI::SetStatus(const wchar_t *s) {
	if (IsWindowUnicode(mhwndStatus)) {
		SendMessage(mhwndStatus, SB_SETTEXTW, 255, (LPARAM)s);
	} else {
		SendMessage(mhwndStatus, SB_SETTEXTA, 255, (LPARAM)VDTextWToA(s).c_str());
	}
}

void VDProjectUI::DisplayRequestUpdate(IVDVideoDisplay *pDisp) {
	PostMessage((HWND)mhwnd, WM_USER + 100, pDisp == mpOutputDisplay, 0);
}

void VDProjectUI::RefreshInputPane() {
	if (mbDubActive)
		return;

	if (!mpCurrentInputFrame) {
		mLastDisplayedInputFrame = -1;
		mbPanesNeedUpdate = true;
	}

	if (mpCurrentInputFrame && mpCurrentInputFrame->IsSuccessful()) {
		VDFilterFrameBuffer *srcFrame = mpCurrentInputFrame->GetResultBuffer();
		VDPixmap px(VDPixmapFromLayout(filters.GetInputLayout(), (void *)srcFrame->LockRead()));
		px.info = srcFrame->info;
		UIRefreshInputFrame(&px);
		srcFrame->Unlock();
	} else
		UIRefreshInputFrame(NULL);
}

void VDProjectUI::RefreshOutputPane() {
	if (mbDubActive)
		return;

	if (!mpCurrentOutputFrame) {
		mLastDisplayedTimelineFrame = -1;
		mbPanesNeedUpdate = true;
	}

	if (mpCurrentOutputFrame && mpCurrentOutputFrame->IsSuccessful()) {
		VDFilterFrameBuffer *dstFrame = mpCurrentOutputFrame->GetResultBuffer();
		VDPixmap px(VDPixmapFromLayout(filters.GetOutputLayout(), (void *)dstFrame->LockRead()));
		px.info = dstFrame->info;
		UIRefreshOutputFrame(&px);
		dstFrame->Unlock();
	} else
		UIRefreshOutputFrame(NULL);
}

//////////////////////////////////////////////////////////////////////

bool VDProjectUI::GetFrameString(wchar_t *buf, size_t buflen, VDPosition dstFrame) {
	if (!inputVideo || !mVideoInputFrameRate.getLo())
		return false;

	const VDStringW& format = VDPreferencesGetTimelineFormat();
	const wchar_t *s = format.data();
	const wchar_t *end = s + format.length();

	try {
		bool bMasked = false;
		int source;
		VDPosition srcFrame = mTimeline.GetSubset().lookupFrame(dstFrame, bMasked, source);

		srcFrame = filters.GetSourceFrame(srcFrame);

		VDPosition srcStreamFrame;
		
		IVDStreamSource *pVSS = inputVideo->asStream();
		if (srcFrame < 0)
			srcFrame = srcStreamFrame = pVSS->getLength();
		else
			srcStreamFrame = inputVideo->displayToStreamOrder(srcFrame);

		const VDFraction srcRate = pVSS->getRate();

		VDPosition dstTime = mVideoTimelineFrameRate.scale64ir(dstFrame * 1000);
		VDPosition srcTime = srcRate.scale64ir(srcFrame * 1000);

		while(s != end) {
			if (*s != '%') {
				const wchar_t *t = s;

				while(s != end && *s != '%')
					++s;

				const size_t len = s - t;

				if (len > buflen)
					return false;

				memcpy(buf, t, len*sizeof(wchar_t));
				buf += len;
				buflen -= len;
				continue;
			}

			++s;

			// check for end
			bool use_end = false;
			if (s != end && *s == '>') {
				++s;

				use_end = true;
			}

			// check for zero-fill
			bool zero_fill = false;

			if (s != end && *s == '0') {
				++s;

				zero_fill = true;
			}

			// check for width
			unsigned width = 0;
			while(s != end && *s >= '0' && *s <= '9')
				width = width*10 + (*s++ - '0');

			// parse code
			if (s == end || !buflen)
				return false;

			wchar_t c = *s++;
			VDPosition formatFrame = dstFrame;
			VDPosition formatTime = dstTime;
			
			if (iswupper(c)) {
				formatFrame = srcFrame;
				formatTime = srcTime;

				if (use_end) {
					formatFrame = pVSS->getLength();
					formatTime = srcRate.scale64ir(formatFrame * 1000);
				}
			} else {
				if (use_end) {
					formatFrame = mTimeline.GetSubset().getTotalFrames();
					formatTime = mVideoTimelineFrameRate.scale64ir(formatFrame * 1000);
				}
			}

			unsigned actual = 0;
			switch(c) {
			case 'f':
			case 'F':
				actual = _snwprintf(buf, buflen, zero_fill ? L"%0*I64u" : L"%*I64u", width, formatFrame);
				break;
			case 'h':
			case 'H':
				actual = _snwprintf(buf, buflen, zero_fill ? L"%0*u" : L"%*u", width, (unsigned)(formatTime / 3600000));
				break;
			case 'm':
			case 'M':
				actual = _snwprintf(buf, buflen, zero_fill ? L"%0*u" : L"%*u", width, (unsigned)((formatTime / 60000) % 60));
				break;
			case 's':
			case 'S':
				actual = _snwprintf(buf, buflen, zero_fill ? L"%0*u" : L"%*u", width, (unsigned)((formatTime / 1000) % 60));
				break;
			case 't':
			case 'T':
				actual = _snwprintf(buf, buflen, zero_fill ? L"%0*u" : L"%*u", width, formatTime % 1000);
				break;
			case 'p':
				actual = _snwprintf(buf, buflen, zero_fill ? L"%0*u" : L"%*u", width,
					(unsigned)formatFrame - (unsigned)VDCeilToInt64(mVideoTimelineFrameRate.asDouble() * (double)(formatTime - formatTime % 1000) / 1000.0));
				break;
			case 'P':
				actual = _snwprintf(buf, buflen, zero_fill ? L"%0*u" : L"%*u", width,
					(unsigned)srcFrame - (unsigned)VDCeilToInt64(srcRate.asDouble() * (double)(srcTime - srcTime % 1000) / 1000.0));
				break;
			case 'B':
				{
					sint64 bytepos = inputVideo->getSampleBytePosition(srcStreamFrame);

					if (bytepos >= 0)
						actual = _snwprintf(buf, buflen, zero_fill ? L"%0*I64x" : L"%*I64x", width, bytepos);
					else
						actual = _snwprintf(buf, buflen, L"%*s", width, L"N/A");
				}
				break;
			case 'L':
				{
					uint32 bytes;

					// we can't safely call this while a dub is occurring because it would cause I/O on
					// two threads
					if (!mbDubActive && !pVSS->read(srcStreamFrame, 1, NULL, 0, &bytes, NULL))
						actual = _snwprintf(buf, buflen, zero_fill ? L"%0*u" : L"%*u", width, bytes);
					else
						actual = _snwprintf(buf, buflen, L"%*s", width, L"N/A");
				}
				break;
			case 'D':
				{
					VDPosition nearestKey = -1;
					
					// we can't safely call this while a dub is occurring because it would cause I/O on
					// two threads
					if (!mbDubActive) {
						nearestKey = inputVideo->nearestKey(srcFrame);

						if (nearestKey > srcFrame)
							nearestKey = inputVideo->prevKey(nearestKey);
					}

					if (nearestKey >= 0)
						actual = _snwprintf(buf, buflen, zero_fill ? L"%0*d" : L"%*d", width, srcFrame - nearestKey);
					else
						actual = _snwprintf(buf, buflen, L"%*s", width, L"N/A");
				}
				break;
			case 'c':
				if (bMasked) {
					*buf = 'M';
					actual = 1;
					break;
				}
			case 'C':
				if (srcFrame >= 0 && srcFrame < pVSS->getLength())
					*buf = inputVideo->getFrameTypeChar(srcFrame);
				else
					*buf = ' ';
				actual = 1;
				break;

			case '%':
				if (!buflen--)
					return false;

				*buf = L'%';
				actual = 1;
				break;

			default:
				return false;
			}

			if (actual > buflen)
				return false;

			buf += actual;
			buflen -= actual;
		}

		if (!buflen)
			return false;

		*buf = 0;
	} catch(const MyError&) {
		return false;
	}

	return true;
}

void VDProjectUI::LoadSettings() {
	VDRegistryAppKey key("Persistence");

	g_vertical							= key.getBool("Vertical display", g_vertical);
	g_drawDecompressedFrame				= key.getBool("Show decompressed frame", g_drawDecompressedFrame);
	g_fSwapPanes						= key.getBool("Swap panes", g_fSwapPanes);
	g_fDropFrames						= key.getBool("Preview frame skipping", g_fDropFrames);
	g_fDropSeeking						= key.getBool("Seek frame skipping", g_fDropSeeking);
	g_showStatusWindow					= key.getBool("Show status window", g_showStatusWindow);
	g_panCentering						= (PanCenteringMode)key.getInt("Pan centering", g_panCentering);
	g_dubOpts.video.fShowInputFrame		= key.getBool("Update input pane", g_dubOpts.video.fShowInputFrame);
	g_dubOpts.video.fShowOutputFrame	= key.getBool("Update output pane", g_dubOpts.video.fShowOutputFrame);
	g_dubOpts.video.fSyncToAudio		= key.getBool("Preview audio sync", g_dubOpts.video.fSyncToAudio);
	g_dubOpts.perf.useDirectDraw		= key.getBool("Accelerate preview", g_dubOpts.perf.useDirectDraw);
	mPaneLayoutMode						= (PaneLayoutMode)key.getEnumInt("Pane layout mode", kPaneLayoutModeCount, mPaneLayoutMode);
	mbAutoSizePanes						= key.getBool("Auto-size panes", mbAutoSizePanes);
	mbMaximize							= key.getBool("Maximize main layout", mbMaximize);

	// these are only saved from the Video Depth dialog.
	VDRegistryAppKey keyPrefs("Preferences");
	int format;

	format = keyPrefs.getInt("Input format", g_dubOpts.video.mInputFormat);
	if ((unsigned)format < nsVDPixmap::kPixFormat_Max_Standard)
		g_dubOpts.video.mInputFormat = format;

	format = keyPrefs.getInt("Output format", g_dubOpts.video.mOutputFormat);
	if ((unsigned)format < nsVDPixmap::kPixFormat_Max_Standard)
		g_dubOpts.video.mOutputFormat = format;

	g_dubOpts.video.mInputFormat.colorSpaceMode = (vd2::ColorSpaceMode)keyPrefs.getInt("Input space", g_dubOpts.video.mInputFormat.colorSpaceMode);
	g_dubOpts.video.mInputFormat.colorRangeMode = (vd2::ColorRangeMode)keyPrefs.getInt("Input range", g_dubOpts.video.mInputFormat.colorRangeMode);

	g_dubOpts.video.mOutputFormat.colorSpaceMode = (vd2::ColorSpaceMode)keyPrefs.getInt("Output space", g_dubOpts.video.mOutputFormat.colorSpaceMode);
	g_dubOpts.video.mOutputFormat.colorRangeMode = (vd2::ColorRangeMode)keyPrefs.getInt("Output range", g_dubOpts.video.mOutputFormat.colorRangeMode);
	g_dubOpts.video.outputReference = keyPrefs.getInt("Output reference", 1);
}

void VDProjectUI::SaveSettings() {
	VDRegistryAppKey key("Persistence");		// we don't use Preferences because these are invisibly saved

	key.setBool("Vertical display", g_vertical);
	key.setBool("Show decompressed frame", g_drawDecompressedFrame);
	key.setBool("Swap panes", g_fSwapPanes);
	key.setBool("Preview frame skipping", g_fDropFrames);
	key.setBool("Seek frame skipping", g_fDropSeeking);
	key.setBool("Show status window", g_showStatusWindow);
	key.setInt("Pan centering", g_panCentering);
	key.setBool("Update input pane", g_dubOpts.video.fShowInputFrame);
	key.setBool("Update output pane", g_dubOpts.video.fShowOutputFrame);
	key.setBool("Preview audio sync", g_dubOpts.video.fSyncToAudio);
	key.setBool("Accelerate preview", g_dubOpts.perf.useDirectDraw);
	key.setInt("Pane layout mode", mPaneLayoutMode);
	key.setBool("Auto-size panes", mbAutoSizePanes);
	key.setBool("Maximize main layout", mbMaximize);
}

bool VDProjectUI::HandleUIEvent(IVDUIBase *pBase, IVDUIWindow *pWin, uint32 id, eEventType type, int item) {
	switch(type) {
		case IVDUICallback::kEventSelect:
			if (id == 100) {
				VDParameterCurve *pc = NULL;
				if (item >= 0) {
					int id = (int)vdpoly_cast<IVDUIList *>(pWin)->GetItemData(item);

					for(VDFilterChainDesc::Entries::const_iterator it(g_filterChain.mEntries.begin()), itEnd(g_filterChain.mEntries.end());
						it != itEnd;
						++it)
					{
						FilterInstance *fa = (*it)->mpInstance;

						if (!--id) {
							pc = fa->GetAlphaParameterCurve();
							break;
						}
					}
				}

				mpCurveEditor->SetCurve(pc);
			}
			break;
	}
	return false;
}

void VDProjectUI::OnCurveUpdated(IVDUIParameterCurveControl *source, const int& args) {
	if (!inputVideo) {
		UIRefreshOutputFrame(false);
		return;
	}

	try {
		VDPosition timelinePos = GetCurrentFrame();

		RefilterFrame(timelinePos);
	} catch(const MyError&) {
		// do nothing
	}
}

void VDProjectUI::OnCurveStatusUpdated(IVDUIParameterCurveControl *source, const IVDUIParameterCurveControl::Status& status) {
	switch(status) {
		case IVDUIParameterCurveControl::kStatus_Nothing:
			SetStatus(L"");
			break;
		case IVDUIParameterCurveControl::kStatus_Focused:
			SetStatus(L"Parameter curve editor: Move mouse to an existing point; Shift+Left to add point; Shift+Right to toggle between line/curve");
			break;
		case IVDUIParameterCurveControl::kStatus_PointDrag:
			SetStatus(L"Parameter curve editor: Dragging point. Release left mouse button to place.");
			break;
		case IVDUIParameterCurveControl::kStatus_PointHighlighted:
			SetStatus(L"Parameter curve editor: Left+Drag to drag point; Ctrl+Left to delete point.");
			break;
	}
}

void VDProjectUI::OnAudioDisplayUpdateRequired(IVDUIAudioDisplayControl *source, const VDPosition& pos) {
	mAudioDisplayPosNext = pos;
	mbAudioDisplayReadActive = true;
}

void VDProjectUI::OnAudioDisplaySetSelect(IVDUIAudioDisplayControl *source, const VDUIAudioDisplaySelectionRange& range) {
	IVDStreamSource *pVSS = inputVideo->asStream();
	VDPosition pos1 = pVSS->TimeToPositionVBR(inputAudio->PositionToTimeVBR(range.mStart));
	VDPosition pos2 = pVSS->TimeToPositionVBR(inputAudio->PositionToTimeVBR(range.mEnd));

	if (pos1 > pos2)
		std::swap(pos1, pos2);

	ClearSelection();
	SetSelectionEnd(pos2);
	SetSelectionStart(pos1);
}

void VDProjectUI::OnAudioDisplayTrackAudioOffset(IVDUIAudioDisplayControl *source, const sint32& offset) {
	// ignore
}

void VDProjectUI::OnAudioDisplaySetAudioOffset(IVDUIAudioDisplayControl *source, const sint32& offset) {
	g_dubOpts.audio.offset += (long)inputAudio->samplesToMs(offset);

	source->Rescan();
}

void VDProjectUI::DisplayPreview(bool v)
{
	mbFiltersPreview = v;

	if (v) {
		ShowWindow(mhwndPosition, SW_HIDE);
	} else {
		UITimelineUpdated();
		ShowWindow(mhwndPosition, mbPositionControlVisible ? SW_SHOWNA : SW_HIDE);
	}
	if (mhwndFilters) ShowWindow(mhwndFilters, v ? SW_HIDE:SW_SHOWNOACTIVATE);
	mpUIBase->SetVisible(!v);
	
	if (v) {
		if (GetParent(mhwndInputFrame)==(HWND)mhwnd) ::ShowWindow(mhwndInputFrame, SW_HIDE);
		if (GetParent(mhwndOutputFrame)==(HWND)mhwnd) ::ShowWindow(mhwndOutputFrame, SW_HIDE);
	} else {
		bool videoPresent = inputVideo != NULL;
		::ShowWindow(mhwndInputFrame, mPaneLayoutMode != kPaneLayoutOutput && videoPresent);
		::ShowWindow(mhwndOutputFrame, mPaneLayoutMode != kPaneLayoutInput && videoPresent);
	}
}

