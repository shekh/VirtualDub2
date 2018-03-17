#include "stdafx.h"
#include <commctrl.h>
#include <vd2/system/filesys.h>
#include <vd2/system/registry.h>
#include <vd2/system/text.h>
#include <vd2/system/w32assist.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/Dita/services.h>
#include "LogWindow.h"
#include "JobControl.h"
#include "job.h"
#include "gui.h"
#include "dub.h"
#include "command.h"
#include "resource.h"

namespace {
	enum {
		kFileDialog_JobList			= 'jobs',
		kFileDialog_ProcessDirIn	= 'jpdi',
		kFileDialog_ProcessDirOut	= 'jpdo'
	};
}

static const char g_szRegKeyShutdownWhenFinished[] = "Shutdown after jobs finish";
static const char g_szRegKeyShutdownMode[] = "Shutdown mode";

///////////////////////////////////////////////////////////////////////////////

extern HINSTANCE g_hInst;
extern HWND g_hwndJobs;
extern const char g_szError[];

extern VDJobQueue g_VDJobQueue;

///////////////////////////////////////////////////////////////////////////////

namespace {
	void JobProcessDirectory(HWND hDlg) {
		const VDStringW srcDir(VDGetDirectory(kFileDialog_ProcessDirIn, (VDGUIHandle)hDlg, L"Select source directory"));

		if (!srcDir.empty()) {
			const VDStringW dstDir(VDGetDirectory(kFileDialog_ProcessDirOut, (VDGUIHandle)hDlg, L"Select target directory"));

			if (!dstDir.empty())
				JobAddBatchDirectory(srcDir.c_str(), dstDir.c_str());
		}
	}
}

class VDUIJobErrorDialog : public VDDialogFrameW32 {
public:
	VDUIJobErrorDialog(const VDJob& job);

	bool OnLoaded();

protected:
	const VDJob&	mJob;
};

VDUIJobErrorDialog::VDUIJobErrorDialog(const VDJob& job)
	: VDDialogFrameW32(IDD_JOBERROR)
	, mJob(job)
{
}

bool VDUIJobErrorDialog::OnLoaded() {
	VDSetWindowTextFW32(mhdlg, L"VirtualDub2 - Job \"%hs\"", mJob.GetName());

	const char *text = mJob.GetError();
	size_t len = strlen(text);
	vdblock<char> buf2(len*2+1);

	char *dst = buf2.data();

	while(char c = *text++) {
		if (c == '\r' || c == '\n') {
			if (*text == (c ^ ('\r' ^ '\n')))
				++text;
			else {
				dst[0] = '\r';
				dst[1] = '\n';
				dst += 2;
				continue;
			}
		}

		*dst++ = c;
	}

	*dst = 0;

	SetDlgItemText(mhdlg, IDC_ERROR, buf2.data());

	return VDDialogFrameW32::OnLoaded();
}

///////////////////////////////////////////////////////////////////////////////

class VDUIJobLogDialog : public VDDialogFrameW32 {
public:
	VDUIJobLogDialog(const VDJob::tLogEntries& logents);

protected:
	bool OnLoaded();

	const VDJob::tLogEntries& mLogEnts;
};

VDUIJobLogDialog::VDUIJobLogDialog(const VDJob::tLogEntries& logents)
	: VDDialogFrameW32(IDD_JOBLOG)
	, mLogEnts(logents)
{
}

bool VDUIJobLogDialog::OnLoaded() {
	IVDLogWindowControl *pLogWin = VDGetILogWindowControl(GetDlgItem(mhdlg, IDC_LOG));

	for(VDAutoLogger::tEntries::const_iterator it(mLogEnts.begin()), itEnd(mLogEnts.end()); it!=itEnd; ++it) {
		const VDAutoLogger::Entry& ent = *it;
		pLogWin->AddEntry(ent.severity, ent.text);
	}

	return VDDialogFrameW32::OnLoaded();
}

///////////////////////////////////////////////////////////////////////////////

class VDUIJobControlDialog : public VDDialogFrameW32, public IVDJobQueueStatusCallback {
public:
	VDUIJobControlDialog();
	~VDUIJobControlDialog();

protected:
	bool OnLoaded();
	void OnSize();
	void OnDestroy();
	bool OnCommand(uint32 id, uint32 extcode);
	VDZINT_PTR DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);

	bool OnMenuHit(uint32 id);

	void UpdateSelectedJobEnables(const VDJob *vdjcheck);

	void GetJobListDispInfoA(NMLVDISPINFOA *nldi);
	void GetJobListDispInfoW(NMLVDISPINFOW *nldi);

	void OnJobQueueStatusChanged(VDJobQueueStatus status);
	void OnJobAdded(const VDJob& job, int index);
	void OnJobRemoved(const VDJob& job, int index);
	void OnJobUpdated(const VDJob& job, int index);
	void OnJobStarted(const VDJob& job);
	void OnJobEnded(const VDJob& job);
	void OnJobProgressUpdate(const VDJob& job, float completion);
	void OnJobQueueReloaded();

	bool fUpdateDisable;
	RECT rInitial;
	bool	mbLocked;

	VDStringW	mStandardCaption;

	VDDialogResizerW32 mResizer;
};

VDUIJobControlDialog::VDUIJobControlDialog()
	: VDDialogFrameW32(IDD_JOBCONTROL)
{
}

VDUIJobControlDialog::~VDUIJobControlDialog() {
}

bool VDUIJobControlDialog::OnLoaded() {
	VDSetDialogDefaultIcons(mhdlg);

	mStandardCaption = VDGetWindowTextW32(mhdlg);

	static const char *const szColumnNames[]={ "Name","Source","Dest","Start","End","Status" };
	HWND hwndItem = GetDlgItem(mhdlg, IDC_JOBS);
	for (int i=0; i<6; i++) {
		LV_COLUMN lvc;
		lvc.mask = LVCF_FMT | LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
		lvc.fmt = LVCFMT_LEFT;
		lvc.cx = 1;
		lvc.pszText = (LPSTR)szColumnNames[i];

		ListView_InsertColumn(hwndItem, i, &lvc);
	}

	static const float kRelativeColumnWidths[]={ 50,100,100,50,50,100 };
	VDUISetListViewColumnsW32(hwndItem, kRelativeColumnWidths, 6);

	mResizer.Init(mhdlg);
	mResizer.Add(IDOK,				VDDialogResizerW32::kTR);
	mResizer.Add(IDC_MOVE_UP,		VDDialogResizerW32::kTR);
	mResizer.Add(IDC_MOVE_DOWN,		VDDialogResizerW32::kTR);
	mResizer.Add(IDC_POSTPONE,		VDDialogResizerW32::kTR);
	mResizer.Add(IDC_DELETE,		VDDialogResizerW32::kTR);
	mResizer.Add(IDC_START,			VDDialogResizerW32::kTR);
	mResizer.Add(IDC_ABORT,			VDDialogResizerW32::kTR);
	mResizer.Add(IDC_RELOAD,		VDDialogResizerW32::kTR);
	mResizer.Add(IDC_AUTOSTART,		VDDialogResizerW32::kTR);
	mResizer.Add(IDC_JOBS,			VDDialogResizerW32::kMC);
	mResizer.Add(IDC_CURRENTJOB,	VDDialogResizerW32::kBL);
	mResizer.Add(IDC_PROGRESS,		VDDialogResizerW32::kBC);
	mResizer.Add(IDC_PERCENT,		VDDialogResizerW32::kBR);

	GetWindowRect(mhdlg, &rInitial);
	VDUIRestoreWindowPlacementW32(mhdlg, "Job control", SW_SHOWNORMAL);
	VDUIRestoreListViewColumnsW32(hwndItem, "Job control: Columns");

	fUpdateDisable = false;

	ListView_SetExtendedListViewStyleEx(hwndItem, LVS_EX_FULLROWSELECT , LVS_EX_FULLROWSELECT);

	RECT rLV;
	GetClientRect(hwndItem, &rLV);

	OnJobQueueReloaded();
	OnJobQueueStatusChanged(g_VDJobQueue.GetQueueStatus());

	CheckButton(IDC_AUTOSTART, g_VDJobQueue.IsAutoRunEnabled());

	SendDlgItemMessage(mhdlg, IDC_PROGRESS, PBM_SETRANGE, 0, MAKELPARAM(0, 16384));

	if (g_dubber || g_VDJobQueue.IsRunInProgress()) {
		EnableControl(IDC_PROGRESS, false);
		EnableControl(IDC_PERCENT, false);
	} else {
		EnableControl(IDC_PROGRESS, true);
		EnableControl(IDC_PERCENT, true);
	}

	g_hwndJobs = mhdlg;

	g_VDJobQueue.SetCallback(this);

	return VDDialogFrameW32::OnLoaded();
}

void VDUIJobControlDialog::OnSize() {
	mResizer.Relayout();
}

void VDUIJobControlDialog::OnDestroy() {
	HWND hwndItem = GetDlgItem(mhdlg, IDC_JOBS);
	VDUISaveListViewColumnsW32(hwndItem, "Job control: Columns");
	VDUISaveWindowPlacementW32(mhdlg, "Job control");
	g_VDJobQueue.SetCallback(NULL);
	g_hwndJobs = NULL;

	try {
		g_VDJobQueue.Flush();
	} catch(const MyError&) {
		// ignore flush error
	}
}

bool VDUIJobControlDialog::OnCommand(uint32 id, uint32 extcode) {
	OnMenuHit(id);
	
	if (extcode == BN_CLICKED) {
		HWND hwndItem = GetDlgItem(mhdlg, IDC_JOBS);
		int index = ListView_GetNextItem(hwndItem, -1, LVNI_ALL | LVNI_SELECTED);
		VDJob *vdj = NULL;

		if (index >= 0)
			vdj = g_VDJobQueue.ListGet(index);

		switch(id) {
		case IDOK:
			Destroy();
			return true;

		case IDC_DELETE:
			if (vdj) {
				// Do not delete jobs that are in progress!

				if (vdj->GetState() != VDJob::kStateInProgress) {
					fUpdateDisable = true;
					g_VDJobQueue.Delete(vdj, false);
					delete vdj;
					g_VDJobQueue.SetModified();
					fUpdateDisable = false;
					if (g_VDJobQueue.ListSize() > 0)
						ListView_SetItemState(hwndItem, index==g_VDJobQueue.ListSize() ? index-1 : index, LVIS_SELECTED, LVIS_SELECTED);
				}
			}

			return TRUE;

		case IDC_POSTPONE:
			if (vdj) {
				// Do not postpone jobs in progress

				int state = vdj->GetState();
				if (state != VDJob::kStateInProgress) {
					if (state == VDJob::kStatePostponed)
						vdj->SetState(VDJob::kStateWaiting);
					else
						vdj->SetState(VDJob::kStatePostponed);

					vdj->Refresh();

					g_VDJobQueue.SetModified();
				}
			}

			return TRUE;

		case IDC_MOVE_UP:
			if (!vdj || index <= 0)
				return TRUE;

			g_VDJobQueue.Swap(index-1, index);

			ListView_SetItemState(hwndItem, index  , 0, LVIS_SELECTED | LVIS_FOCUSED);
			ListView_SetItemState(hwndItem, index-1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			ListView_RedrawItems(hwndItem, index-1, index);

			g_VDJobQueue.SetModified();

			return TRUE;

		case IDC_MOVE_DOWN:
			if (!vdj || index >= g_VDJobQueue.ListSize()-1)
				return TRUE;

			g_VDJobQueue.Swap(index+1, index);
			
			ListView_SetItemState(hwndItem, index  , 0, LVIS_SELECTED | LVIS_FOCUSED);
			ListView_SetItemState(hwndItem, index+1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			ListView_RedrawItems(hwndItem, index, index+1);

			g_VDJobQueue.SetModified();

			return TRUE;

		case IDC_START:
			if (g_VDJobQueue.IsRunInProgress()) {
				g_VDJobQueue.RunAllStop();
				EnableControl(IDC_START, FALSE);
			} else
				g_VDJobQueue.RunAllStart();
			return TRUE;

		case IDC_ABORT:
			if (g_VDJobQueue.IsRunInProgress()) {
				g_VDJobQueue.RunAllStop();
				EnableControl(IDC_START, false);
				EnableControl(IDC_ABORT, false);
				if (g_dubber) g_dubber->Abort();
			}

			return TRUE;

		case IDC_RELOAD:
			if (!vdj)
				return TRUE;

			if (!vdj->IsReloadMarkerPresent())
				MessageBox(mhdlg, "This job was created with an older version of VirtualDub and cannot be reloaded.", g_szError, MB_ICONERROR|MB_OK);
			else
				vdj->Reload();

			return TRUE;

		case IDC_AUTOSTART:
			g_VDJobQueue.SetAutoRunEnabled(IsButtonChecked(IDC_AUTOSTART));
			return TRUE;
		}
	}

	return false;
}

bool VDUIJobControlDialog::OnMenuHit(uint32 id) {
	static const wchar_t fileFilters[]=
		L"VirtualDub job list (*.jobs)\0"			L"*.jobs\0"
		L"VirtualDub script (*.syl, *.vdscript)\0"	L"*.syl;*.vdscript\0"
		L"All files (*.*)\0"						L"*.*\0";

	try {
		switch(id) {

			case ID_FILE_LOADJOBLIST:
				{
					VDStringW filename(VDGetLoadFileName(kFileDialog_JobList, (VDGUIHandle)mhdlg, L"Load job list", fileFilters, NULL));

					if (!filename.empty())
						g_VDJobQueue.ListLoad(filename.c_str(), false);
				}
				break;

			case ID_FILE_SAVEJOBLIST:
				{
					VDStringW filename(VDGetSaveFileName(kFileDialog_JobList, (VDGUIHandle)mhdlg, L"Save job list", fileFilters, NULL));

					if (!filename.empty())
						g_VDJobQueue.Flush(filename.c_str());
				}
				break;

			case ID_FILE_USELOCALJOBQUEUE:
				if (g_VDJobQueue.IsRunInProgress())
					MessageBox(mhdlg, "Cannot switch job queues while a job is in progress.", g_szError, MB_ICONERROR | MB_OK);
				else
					g_VDJobQueue.SetJobFilePath(NULL, false, false);
				break;

			case ID_FILE_USEREMOTEJOBQUEUE:
				if (g_VDJobQueue.IsRunInProgress())
					MessageBox(mhdlg, "Cannot switch job queues while a job is in progress.", g_szError, MB_ICONERROR | MB_OK);
				else {
					const VDFileDialogOption opts[]={
						{ VDFileDialogOption::kConfirmFile, 0, NULL, 0, 0},
						{0}
					};

					int optvals[]={ false };

					VDStringW filename(VDGetSaveFileName(kFileDialog_JobList, (VDGUIHandle)mhdlg, L"Use shared job list", fileFilters, NULL, opts, optvals));

					if (!filename.empty()) {
						if (!_wcsicmp(filename.c_str(), g_VDJobQueue.GetDefaultJobFilePath())) {
							DWORD res = MessageBox(mhdlg,
								"Using the same job file that is normally used for local job queue operation is not recommended as "
								"it can cause job queue corruption.\n"
								"\n"
								"Are you sure you want to use this file for the remote queue too?",
								"VirtualDub Warning",
								MB_ICONEXCLAMATION | MB_YESNO);

							if (res != IDYES)
								break;
						}

						g_VDJobQueue.SetJobFilePath(filename.c_str(), true, true);
					}
				}
				break;

			case ID_EDIT_CLEARLIST:
				if (IDOK != MessageBox(mhdlg, "Really clear job list?", "VirtualDub job system", MB_OKCANCEL | MB_ICONEXCLAMATION))
					break;

				g_VDJobQueue.ListClear(false);
				break;

			case ID_EDIT_DELETEDONEJOBS:
				for(uint32 i=0; i<g_VDJobQueue.ListSize();) {
					VDJob *vdj = g_VDJobQueue.ListGet(i);

					if (vdj->GetState() == VDJob::kStateCompleted) {
						g_VDJobQueue.Delete(vdj, false);
						delete vdj;
					} else
						++i;
				}

				break;

			case ID_EDIT_FAILEDTOWAITING:
				g_VDJobQueue.Transform(VDJob::kStateAborted, VDJob::kStateWaiting);
				g_VDJobQueue.Transform(VDJob::kStateError, VDJob::kStateWaiting);
				break;

			case ID_EDIT_WAITINGTOPOSTPONED:
				g_VDJobQueue.Transform(VDJob::kStateWaiting, VDJob::kStatePostponed);
				break;

			case ID_EDIT_POSTPONEDTOWAITING:
				g_VDJobQueue.Transform(VDJob::kStatePostponed, VDJob::kStateWaiting);
				break;

			case ID_EDIT_DONETOWAITING:
				g_VDJobQueue.Transform(VDJob::kStateCompleted, VDJob::kStateWaiting);
				break;

			case ID_EDIT_PROCESSDIRECTORY:
				JobProcessDirectory(mhdlg);
				break;

			case ID_WHENFINISHED_DONOTHING:
				{
					VDRegistryAppKey appKey("");

					appKey.setBool(g_szRegKeyShutdownWhenFinished, false);
				}
				break;

			case ID_WHENFINISHED_SHUTDOWN:
				{
					VDRegistryAppKey appKey("");

					appKey.setBool(g_szRegKeyShutdownWhenFinished, true);
					appKey.setInt(g_szRegKeyShutdownMode, 0);
				}
				break;

			case ID_WHENFINISHED_HIBERNATE:
				{
					VDRegistryAppKey appKey("");

					appKey.setBool(g_szRegKeyShutdownWhenFinished, true);
					appKey.setInt(g_szRegKeyShutdownMode, 1);
				}
				break;
			case ID_WHENFINISHED_SLEEP:
				{
					VDRegistryAppKey appKey("");

					appKey.setBool(g_szRegKeyShutdownWhenFinished, true);
					appKey.setInt(g_szRegKeyShutdownMode, 2);
				}
				break;
		}
	} catch(const MyError& e) {
		e.post(mhdlg, "Job system error");
	}

	return true;
}

VDZINT_PTR VDUIJobControlDialog::DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam) {
	int index;

	switch(msg) {
	case WM_COMMAND:
		// we have to filter out WM_COMMAND messages from the list view edit control
		// because some moron on the Windows team used IDOK as the edit control identifier
		if (lParam) {
			switch(HIWORD(wParam)) {
				case EN_SETFOCUS:
				case EN_KILLFOCUS:
				case EN_CHANGE:
				case EN_UPDATE:
				case EN_ERRSPACE:
				case EN_MAXTEXT:
				case EN_HSCROLL:
				case EN_VSCROLL:
					return FALSE;
			}
		}

		// fall through to default handler
		break;

	case WM_NOTIFY:
		{
			NMHDR *nm = (NMHDR *)lParam;

			if (nm->idFrom == IDC_JOBS) {
				NMLVDISPINFO *nldi = (NMLVDISPINFO *)nm;
				NMLISTVIEW *nmlv;
				VDJob *vdj;

				switch(nm->code) {
				case LVN_GETDISPINFOA:
					GetJobListDispInfoA(nldi);
					return TRUE;
				case LVN_GETDISPINFOW:
					GetJobListDispInfoW((NMLVDISPINFOW *)nldi);
					return TRUE;
				case LVN_ENDLABELEDITA:
					SetWindowLongPtr(mhdlg, DWLP_MSGRESULT, TRUE);
					vdj = g_VDJobQueue.ListGet(nldi->item.iItem);

					if (vdj && nldi->item.pszText)
						vdj->SetName(nldi->item.pszText);

					return TRUE;
				case LVN_ENDLABELEDITW:
					SetWindowLongPtr(mhdlg, DWLP_MSGRESULT, TRUE);
					vdj = g_VDJobQueue.ListGet(nldi->item.iItem);

					if (vdj && nldi->item.pszText)
						vdj->SetName(VDTextWToA(((NMLVDISPINFOW *)nldi)->item.pszText).c_str());

					return TRUE;
				case LVN_ITEMCHANGED:

					if (fUpdateDisable) return TRUE;

					nmlv = (NMLISTVIEW *)lParam;
					vdj = g_VDJobQueue.ListGet(nmlv->iItem);

					UpdateSelectedJobEnables(NULL);
					return TRUE;

				case LVN_KEYDOWN:
					switch(((LPNMLVKEYDOWN)lParam)->wVKey) {
					case VK_DELETE:
						SendMessage(mhdlg, WM_COMMAND, IDC_DELETE, (LPARAM)GetDlgItem(mhdlg, IDC_DELETE));
					}
					return TRUE;

				case NM_DBLCLK:

					//	Previous state		Next state		Action
					//	--------------		----------		------
					//	Error				Waiting			Show error message
					//	Done (warnings)		Done			Show log
					//	Done				Waiting
					//	Postponed			Waiting
					//	Aborted				Waiting
					//	All others			Postponed

					index = ListView_GetNextItem(GetDlgItem(mhdlg, IDC_JOBS), -1, LVNI_ALL | LVNI_SELECTED);
					if (index>=0) {
						vdj = g_VDJobQueue.ListGet(index);

						switch(vdj->GetState()) {
						case VDJob::kStateError:
							if (VDUIJobErrorDialog(*vdj).ShowDialog((VDGUIHandle)mhdlg)) {
								vdj->SetState(VDJob::kStateWaiting);
								vdj->Refresh();
								g_VDJobQueue.SetModified();
							}
							break;
						case VDJob::kStateCompleted:
							if (!vdj->mLogEntries.empty()) {
								if (!VDUIJobLogDialog(vdj->mLogEntries).ShowDialog((VDGUIHandle)mhdlg))
									break;
							}
						case VDJob::kStateAborted:
							vdj->SetState(VDJob::kStateWaiting);
							vdj->Refresh();
							g_VDJobQueue.SetModified();
							break;

						case VDJob::kStateInProgress:
							if (!vdj->IsLocal()) {
								vdj->SetState(VDJob::kStateAborting);
								vdj->Refresh();
							}
							g_VDJobQueue.SetModified();
							break;

						case VDJob::kStateAborting:
							if (!vdj->IsLocal()) {
								VDStringA msg;

								msg.sprintf("This job may be running on a different instance of VirtualDub on the machine named %hs. Are you sure you want to reset it to Waiting status?", vdj->GetRunnerName());
								if (IDOK == MessageBox(mhdlg, msg.c_str(), "VirtualDub Warning", MB_ICONEXCLAMATION | MB_OKCANCEL)) {
									vdj->SetState(VDJob::kStateWaiting);
									vdj->Refresh();
								}
							}
							break;
						default:
							SendMessage(mhdlg, WM_COMMAND, MAKELONG(IDC_POSTPONE, BN_CLICKED), (LPARAM)GetDlgItem(mhdlg, IDC_POSTPONE));
						}
					}

					return TRUE;
				}
			}
		}
		break;

	case WM_INITMENU:
		{
			HMENU hmenu = (HMENU)wParam;
			VDRegistryAppKey key;
			bool bShutdownWhenFinished = key.getBool(g_szRegKeyShutdownWhenFinished);
			int shutdownMode = key.getInt(g_szRegKeyShutdownMode);

			VDCheckRadioMenuItemByCommandW32(hmenu, ID_WHENFINISHED_DONOTHING, !bShutdownWhenFinished);
			VDCheckRadioMenuItemByCommandW32(hmenu, ID_WHENFINISHED_SHUTDOWN, bShutdownWhenFinished && shutdownMode == 0);
			VDCheckRadioMenuItemByCommandW32(hmenu, ID_WHENFINISHED_HIBERNATE, bShutdownWhenFinished && shutdownMode == 1);
			VDCheckRadioMenuItemByCommandW32(hmenu, ID_WHENFINISHED_SLEEP, bShutdownWhenFinished && shutdownMode == 2);

			bool isRemoteQueue = g_VDJobQueue.IsAutoUpdateEnabled();
			VDCheckRadioMenuItemByCommandW32(hmenu, ID_FILE_USELOCALJOBQUEUE, !isRemoteQueue);
			VDCheckRadioMenuItemByCommandW32(hmenu, ID_FILE_USEREMOTEJOBQUEUE, isRemoteQueue);
		}
		return 0;

	case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;

			lpmmi->ptMinTrackSize.x = rInitial.right - rInitial.left;
			lpmmi->ptMinTrackSize.y = rInitial.bottom - rInitial.top;
		}
		return TRUE;

	}

	return VDDialogFrameW32::DlgProc(msg, wParam, lParam);
}

void VDUIJobControlDialog::GetJobListDispInfoA(NMLVDISPINFOA *nldi) {
	NMLVDISPINFOW nldiw;

	nldiw.hdr				= nldi->hdr;
    nldiw.item.mask			= nldi->item.mask;
    nldiw.item.iItem		= nldi->item.iItem;
    nldiw.item.iSubItem		= nldi->item.iSubItem;
    nldiw.item.state		= nldi->item.state;
    nldiw.item.stateMask	= nldi->item.stateMask;
    nldiw.item.iImage		= nldi->item.iImage;
    nldiw.item.lParam		= nldi->item.lParam;

	wchar_t bufw[512];
	bufw[0] = 0;
    nldiw.item.pszText		= bufw;
    nldiw.item.cchTextMax	= 512;

	GetJobListDispInfoW(&nldiw);

	VDTextWToA(nldi->item.pszText, nldi->item.cchTextMax, nldiw.item.pszText);
}

void VDUIJobControlDialog::GetJobListDispInfoW(NMLVDISPINFOW *nldi) {
	VDJob *vdj = g_VDJobQueue.ListGet(nldi->item.iItem);
	SYSTEMTIME st;
	SYSTEMTIME ct;
	static const wchar_t *const dow[]={L"Sun",L"Mon",L"Tue",L"Wed",L"Thu",L"Fri",L"Sat"};

	if (!(nldi->item.mask & LVIF_TEXT))
		return;

	nldi->item.mask			= LVIF_TEXT;
	nldi->item.pszText[0]	= 0;

	if (!vdj)
		return;

	uint64 *ft = &vdj->mDateEnd;

	switch(nldi->item.iSubItem) {
	case 0:
		VDTextAToW(nldi->item.pszText, nldi->item.cchTextMax, vdj->GetName());
		break;
	case 1:		// file in
		VDTextAToW(nldi->item.pszText, nldi->item.cchTextMax, VDFileSplitPath(vdj->GetInputFile()));
		break;
	case 2:		// file out
		VDTextAToW(nldi->item.pszText, nldi->item.cchTextMax, VDFileSplitPath(vdj->GetOutputFile()));
		break;
	case 3:		// time in
		ft = &vdj->mDateStart;
	case 4:		// time out

		{
			FILETIME ft2, ftl;
			ft2.dwLowDateTime = (uint32)*ft;
			ft2.dwHighDateTime = (uint32)(*ft >> 32);

			FileTimeToLocalFileTime(&ft2, &ftl);

			FileTimeToSystemTime(&ftl, &st);
		}

		GetLocalTime(&ct);
		if (!*ft)
			nldi->item.pszText = L"-";
		else if (ct.wYear != st.wYear
			|| ct.wMonth != st.wMonth
			|| ct.wDay != st.wDay) {

			swprintf(nldi->item.pszText, nldi->item.cchTextMax, L"%s %d %d:%02d%c"
						,dow[st.wDayOfWeek]
						,st.wDay
						,st.wHour==12||!st.wHour ? 12 : st.wHour%12
						,st.wMinute
						,st.wHour>=12 ? 'p' : 'a');
		} else {
			swprintf(nldi->item.pszText, nldi->item.cchTextMax, L"%d:%02d%c"
						,st.wHour==12||!st.wHour ? 12 : st.wHour%12
						,st.wMinute
						,st.wHour>=12 ? 'p' : 'a');
		}
		break;
	case 5:		// status
		switch(vdj->GetState()) {
		case VDJob::kStateWaiting:		nldi->item.pszText = L"Waiting"		; break;
		case VDJob::kStateInProgress:
			if (vdj->mRunnerName.empty() || vdj->IsLocal())
				nldi->item.pszText = L"In progress";
			else
				swprintf(nldi->item.pszText, nldi->item.cchTextMax, L"In progress (%hs:%d)", vdj->mRunnerName.c_str(), (uint32)vdj->GetRunnerId());
			break;
		case VDJob::kStateStarting:
			if (vdj->mRunnerName.empty() || vdj->IsLocal())
				nldi->item.pszText = L"Starting";
			else
				swprintf(nldi->item.pszText, nldi->item.cchTextMax, L"Starting (%hs:%d)", vdj->mRunnerName.c_str(), (uint32)vdj->GetRunnerId());
			break;
		case VDJob::kStateAborting:
			if (vdj->mRunnerName.empty() || vdj->IsLocal())
				nldi->item.pszText = L"Aborting";
			else
				swprintf(nldi->item.pszText, nldi->item.cchTextMax, L"Aborting (%hs:%d)", vdj->mRunnerName.c_str(), (uint32)vdj->GetRunnerId());
			break;
		case VDJob::kStateCompleted:
			if (vdj->mRunnerName.empty() || vdj->IsLocal())
				swprintf(nldi->item.pszText, nldi->item.cchTextMax, L"Done%hs", vdj->mLogEntries.empty() ? "" : " (warnings)");
			else
				swprintf(nldi->item.pszText, nldi->item.cchTextMax, L"Done%hs (%hs:%d)", vdj->mLogEntries.empty() ? "" : " (warnings)", vdj->mRunnerName.c_str(), (uint32)vdj->GetRunnerId());
			break;
		case VDJob::kStatePostponed:
			nldi->item.pszText = L"Postponed";
			break;
		case VDJob::kStateAborted:
			if (vdj->mRunnerName.empty() || vdj->IsLocal())
				nldi->item.pszText = L"Aborted";
			else
				swprintf(nldi->item.pszText, nldi->item.cchTextMax, L"Aborted (%hs:%d)", vdj->mRunnerName.c_str(), (uint32)vdj->GetRunnerId());
			break;
		case VDJob::kStateError:
			if (vdj->mRunnerName.empty() || vdj->IsLocal())
				nldi->item.pszText = L"Error";
			else
				swprintf(nldi->item.pszText, nldi->item.cchTextMax, L"Error (%hs:%d)", vdj->mRunnerName.c_str(), (uint32)vdj->GetRunnerId());
			break;
		}
		break;
	}
}

void VDUIJobControlDialog::UpdateSelectedJobEnables(const VDJob *vdjcheck) {
	HWND hwndItem = GetDlgItem(mhdlg, IDC_JOBS);
	if (!hwndItem)
		return;

	int index = ListView_GetNextItem(hwndItem, -1, LVNI_ALL | LVNI_SELECTED);
	VDJob *vdj = NULL;

	if (index >= 0)
		vdj = g_VDJobQueue.ListGet(index);

	if (vdjcheck && vdjcheck != vdj)
		return;

	if (vdj) {
		EnableControl(IDC_DELETE, vdj->GetState() != VDJob::kStateInProgress);
		EnableControl(IDC_POSTPONE, vdj->GetState() != VDJob::kStateInProgress);
		EnableControl(IDC_RELOAD, !g_VDJobQueue.IsRunInProgress());
		EnableControl(IDC_MOVE_UP, index > 0);
		EnableControl(IDC_MOVE_DOWN, index < g_VDJobQueue.ListSize()-1);
	} else {
		EnableControl(IDC_DELETE, false);
		EnableControl(IDC_POSTPONE, false);
		EnableControl(IDC_RELOAD, false);
		EnableControl(IDC_MOVE_UP, false);
		EnableControl(IDC_MOVE_DOWN, false);
	}
}

void VDUIJobControlDialog::OnJobQueueStatusChanged(VDJobQueueStatus status) {
	VDStringW title(mStandardCaption);

	if (status == kVDJQS_Running)
		title.append_sprintf(L" (%d remaining)", g_VDJobQueue.GetPendingJobCount());

	if (g_VDJobQueue.IsAutoUpdateEnabled())
		title.append_sprintf(L" [%s] (%hs:%d)", g_VDJobQueue.GetJobFilePath(), g_VDJobQueue.GetRunnerName(), (uint32)g_VDJobQueue.GetRunnerId());

	switch(status) {
		case kVDJQS_Idle:
			EnableControl(IDC_START, true);
			SetControlText(IDC_START, L"Start");
			EnableControl(IDC_ABORT, false);
			EnableControl(IDC_RELOAD, true);
			break;

		case kVDJQS_Running:
			SetControlText(IDC_START, L"Stop");
			EnableControl(IDC_START, true);
			EnableControl(IDC_ABORT, true);
			EnableControl(IDC_RELOAD, false);
			break;

		case kVDJQS_Blocked:
			SetControlText(IDC_START, L"Start");
			EnableControl(IDC_START, false);
			EnableControl(IDC_ABORT, false);
			EnableControl(IDC_RELOAD, false);
			break;
	}

	VDSetWindowTextW32(mhdlg, title.c_str());
}

void VDUIJobControlDialog::OnJobAdded(const VDJob& job, int index) {
	if (!mhdlg)
		return;

	LVITEM li;

	li.mask		= LVIF_TEXT;
	li.iSubItem	= 0;
	li.iItem	= index;
	li.pszText	= LPSTR_TEXTCALLBACK;

	ListView_InsertItem(GetDlgItem(mhdlg, IDC_JOBS), &li);
}

void VDUIJobControlDialog::OnJobRemoved(const VDJob& job, int index) {
	if (!mhdlg)
		return;

	ListView_DeleteItem(GetDlgItem(mhdlg, IDC_JOBS), index);

	UpdateSelectedJobEnables(&job);
}

void VDUIJobControlDialog::OnJobUpdated(const VDJob& job, int index) {
	if (!mhdlg)
		return;

	HWND hwndItem = GetDlgItem(mhdlg, IDC_JOBS);

	if (hwndItem)
		ListView_Update(hwndItem, index);

	UpdateSelectedJobEnables(&job);
}

void VDUIJobControlDialog::OnJobStarted(const VDJob& job) {
	EnableControl(IDC_PROGRESS, true);
	EnableControl(IDC_PERCENT, true);
}

void VDUIJobControlDialog::OnJobEnded(const VDJob& job) {
	EnableControl(IDC_PROGRESS, false);
	EnableControl(IDC_PERCENT, false);
	SendDlgItemMessage(mhdlg, IDC_PROGRESS, PBM_SETPOS, 0, 0);
}

void VDUIJobControlDialog::OnJobProgressUpdate(const VDJob& job, float completion) {
	if (!mhdlg)
		return;

	SendDlgItemMessage(mhdlg, IDC_PROGRESS, PBM_SETPOS, (int)(completion*16384.0f + 0.5f), 0);
	SetControlTextF(IDC_PERCENT, L"%.0f%%", completion * 100.0f);
}

void VDUIJobControlDialog::OnJobQueueReloaded() {
	HWND hwndItem = GetDlgItem(mhdlg, IDC_JOBS);
	if (!hwndItem)
		return;

	POINT pt={0,0};
	ListView_GetOrigin(hwndItem, &pt);

	int selindex = ListView_GetNextItem(hwndItem, -1, LVNI_ALL | LVNI_SELECTED);
	int newselindex = -1;

	if (selindex >= 0) {
		VDJob *vdj = g_VDJobQueue.ListGet(selindex);

		if (vdj) {
			uint64 selid = vdj->mId;

			newselindex = g_VDJobQueue.GetJobIndexById(selid);
		}
	}

	ListView_DeleteAllItems(hwndItem);
	int n = g_VDJobQueue.ListSize();
	for(int i=0; i<n; i++) {
		LVITEM li;

		li.mask		= LVIF_TEXT;
		li.iSubItem	= 0;
		li.iItem	= i;
		li.pszText	= LPSTR_TEXTCALLBACK;

		ListView_InsertItem(hwndItem, &li);
	}

	if (newselindex >= 0)
		ListView_SetItemState(hwndItem, newselindex, LVIS_SELECTED, LVIS_SELECTED);

	ListView_Scroll(hwndItem, 0, -pt.y);

	UpdateSelectedJobEnables(NULL);
}

///////////////////////////////////////////////////////////////////////////////

VDUIJobControlDialog g_VDUIJobControlDialog;

void OpenJobWindow() {
	HWND hdlg = g_VDUIJobControlDialog.GetWindowHandle();

	if (hdlg) {
		SetForegroundWindow(hdlg);
		return;
	}

	g_VDUIJobControlDialog.Create(NULL);
}

void CloseJobWindow() {
	g_VDUIJobControlDialog.Destroy();
}

