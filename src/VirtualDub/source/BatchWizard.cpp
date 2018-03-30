#include "stdafx.h"
#include <hash_map>
#include <shellapi.h>
#include <commctrl.h>
#include <vd2/system/filesys.h>
#include <vd2/system/hash.h>
#include <vd2/system/registry.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/w32assist.h>
#include <vd2/Dita/services.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/VDLib/UIProxies.h>
#include "ExternalEncoderProfile.h"
#include "resource.h"
#include "gui.h"
#include "job.h"

namespace {
	enum {
		kFileDialog_ProcessDirIn	= 'jpdi',
		kFileDialog_BatchOutputDir	= 'bout'
	};
}

extern DubOptions g_dubOpts;
extern HINSTANCE g_hInst;
extern const char g_szError[];
extern const char g_szWarning[];

///////////////////////////////////////////////////////////////////////////////

class VDUIBatchWizardNameFilter : public VDDialogFrameW32 {
public:
	VDUIBatchWizardNameFilter();
	~VDUIBatchWizardNameFilter();

	bool IsMatchCaseEnabled() const { return mbCaseSensitive; }

	const wchar_t *GetSearchString() const { return mSearchString.c_str(); }
	const wchar_t *GetReplaceString() const { return mReplaceString.c_str(); }

protected:
	bool OnLoaded();
	void OnDataExchange(bool write);

	VDStringW	mSearchString;
	VDStringW	mReplaceString;
	bool		mbCaseSensitive;
};

VDUIBatchWizardNameFilter::VDUIBatchWizardNameFilter()
	: VDDialogFrameW32(IDD_BATCH_WIZARD_NAMEFILTER)
	, mbCaseSensitive(false)
{
	VDRegistryAppKey key("Persistence");

	mbCaseSensitive = key.getBool("Batch Wizard: Match case", false);
}

VDUIBatchWizardNameFilter::~VDUIBatchWizardNameFilter() {
}

bool VDUIBatchWizardNameFilter::OnLoaded() {
	SetFocusToControl(IDC_SEARCHSTR);
	return true;
}

void VDUIBatchWizardNameFilter::OnDataExchange(bool write) {
	ExchangeControlValueString(write, IDC_SEARCHSTR, mSearchString);
	ExchangeControlValueString(write, IDC_REPLACESTR, mReplaceString);
	ExchangeControlValueBoolCheckbox(write, IDC_MATCHCASE, mbCaseSensitive);

	if (write) {
		VDRegistryAppKey key("Persistence");

		key.setBool("Batch Wizard: Match case", mbCaseSensitive);
	}
}

///////////////////////////////////////////////////////////////////////////////

class VDUIBatchWizard : public VDDialogFrameW32 {
public:
	VDUIBatchWizard();
	~VDUIBatchWizard();

protected:
	VDZINT_PTR DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);
	static LRESULT CALLBACK ListProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	bool OnLoaded();
	void SetOutputAbsolute();
	void OnDestroy();
	void OnSize();
	bool OnCommand(uint32 id, uint32 extcode);
	void OnDropFiles(IVDUIDropFileList *dropFileList);
	bool CheckAndConfirmConflicts();
	void DeleteSelected();

	bool mbOutputRelative;

	VDDialogResizerW32	mResizer;
	VDUIProxyListView	mList;
	WNDPROC list_proc;

	HMENU	mhmenuPopups;
};

VDUIBatchWizard::VDUIBatchWizard()
	: VDDialogFrameW32(IDD_BATCH_WIZARD)
	, mbOutputRelative(false)
	, mhmenuPopups(LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_BATCHWIZARD_MENU)))
{
}

VDUIBatchWizard::~VDUIBatchWizard() {
	if (mhmenuPopups)
		DestroyMenu(mhmenuPopups);
}

VDZINT_PTR VDUIBatchWizard::DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam) {
	switch(msg) {
	case WM_NOTIFY:
		SetWindowLongPtr(mhdlg, DWLP_MSGRESULT, mMsgDispatcher.Dispatch_WM_NOTIFY(wParam, lParam));
		return TRUE;
	}

	return VDDialogFrameW32::DlgProc(msg, wParam, lParam);
}

LRESULT VDUIBatchWizard::ListProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDUIBatchWizard* p = (VDUIBatchWizard*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
	switch(msg) {
	case WM_GETDLGCODE:
		{
			MSG* msg = (MSG*)lParam;
			if (!msg) return 0;
			if (msg->message==WM_KEYDOWN && (wParam==VK_DELETE || wParam==VK_UP || wParam==VK_DOWN))
				return DLGC_WANTMESSAGE;
		}
		return 0;

	case WM_KEYDOWN:
		if (wParam==VK_DELETE) {
			p->DeleteSelected();
			return true;
		}
		break;
	}

	return CallWindowProc(p->list_proc, hwnd, msg, wParam, lParam);
}

bool VDUIBatchWizard::OnLoaded() {
	VDSetDialogDefaultIcons(mhdlg);
	mResizer.Init(mhdlg);
	mResizer.Add(IDC_OUTPUTFOLDER, VDDialogResizerW32::kTC);
	mResizer.Add(IDC_BROWSEOUTPUTFOLDER, VDDialogResizerW32::kTR);
	mResizer.Add(IDC_LIST, VDDialogResizerW32::kMC);
	mResizer.Add(IDC_FILTEROUTPUTNAMES, VDDialogResizerW32::kBL);
	mResizer.Add(IDC_RENAMEFILES, VDDialogResizerW32::kBR);
	mResizer.Add(IDC_ADDTOQUEUE, VDDialogResizerW32::kBR);
	mResizer.Add(IDOK, VDDialogResizerW32::kBR);
	DragAcceptFiles(mhdlg, TRUE);

	VDUIRestoreWindowPlacementW32(mhdlg, "Batch wizard", SW_SHOWNORMAL);

	AddProxy(&mList, IDC_LIST);
	mList.SetFullRowSelectEnabled(true);
	list_proc = (WNDPROC)GetWindowLongPtr(GetDlgItem(mhdlg,IDC_LIST), GWLP_WNDPROC);
	SetWindowLongPtr(GetDlgItem(mhdlg,IDC_LIST), GWLP_USERDATA, (LPARAM)this);
	SetWindowLongPtr(GetDlgItem(mhdlg,IDC_LIST), GWLP_WNDPROC, (LPARAM)ListProc);

	mList.InsertColumn(0, L"Source file", 100);
	mList.InsertColumn(1, L"Output name", 100);

	VDRegistryAppKey key("Persistence");
	mbOutputRelative = key.getBool("Batch Wizard: Output relative", true);

	DirspecEntry* fsent = VDGetDirSpec(kFileDialog_BatchOutputDir);
	if (!fsent || !fsent->szFile[0]) mbOutputRelative = true;

	if (mbOutputRelative)
		CheckButton(IDC_OUTPUT_RELATIVE, true);
	else
		CheckButton(IDC_OUTPUT_ABSOLUTE, true);
	if (!mbOutputRelative) SetOutputAbsolute();

	return false;
}

void VDUIBatchWizard::SetOutputAbsolute() {
	VDStringW s;
	GetControlText(IDC_OUTPUTFOLDER, s);

	if (s.empty()) {
		DirspecEntry* fsent = VDGetDirSpec(kFileDialog_BatchOutputDir);
		if (fsent)
			SetControlText(IDC_OUTPUTFOLDER, fsent->szFile);
	}
}

void VDUIBatchWizard::OnDestroy() {
	VDUISaveWindowPlacementW32(mhdlg, "Batch wizard");
	VDRegistryAppKey key("Persistence");
	key.setBool("Batch Wizard: Output relative", mbOutputRelative);

	mList.Clear();

	VDDialogFrameW32::OnDestroy();
}

void VDUIBatchWizard::OnSize() {
	mResizer.Relayout();
}

class VDUIBatchWizardItem : public vdrefcounted<IVDUIListViewVirtualItem> {
public:
	VDUIBatchWizardItem(const wchar_t *fn);

	const wchar_t *GetFileName() const { return mFileName.c_str(); }
	const wchar_t *GetOutputName() const { return mOutputName.c_str(); }

	void SetOutputName(const wchar_t *s) { mOutputName = s; }

	void GetText(int subItem, VDStringW& s) const;

protected:
	VDStringW	mFileName;
	VDStringW	mOutputName;
};

VDUIBatchWizardItem::VDUIBatchWizardItem(const wchar_t *fn)
	: mFileName(fn)
	, mOutputName(VDFileSplitPath(fn))
{
}

void VDUIBatchWizardItem::GetText(int subItem, VDStringW& s) const {
	switch(subItem) {
		case 0:
			s = mFileName;
			break;

		case 1:
			s = VDFileSplitExtLeft(mOutputName) + L".***";
			break;
	}
}

void VDUIBatchWizard::DeleteSelected() {
	int x = mList.GetSelectedIndex();
	if (x!=-1)
		mList.DeleteItem(x);
	int n = mList.GetItemCount();
	if (n>x)
		mList.SetSelectedIndex(x);
	else if (n>0)
		mList.SetSelectedIndex(n-1);
}

bool VDUIBatchWizard::OnCommand(uint32 id, uint32 extcode) {
	switch(id) {
		case IDC_OUTPUT_RELATIVE:
			if (extcode == BN_CLICKED)
				mbOutputRelative = true;
			break;

		case IDC_OUTPUT_ABSOLUTE:
			if (extcode == BN_CLICKED) {
				mbOutputRelative = false;
				SetOutputAbsolute();
			}
			break;

		case IDC_OUTPUTFOLDER:
			if (extcode = EN_CHANGE) {
				VDStringW s;
				GetControlText(IDC_OUTPUTFOLDER, s);

				if (!s.empty()) {
					DirspecEntry* fsent = VDGetDirSpec(kFileDialog_BatchOutputDir);
					if (fsent)
						wcscpy(fsent->szFile, s.c_str());
					CheckButton(IDC_OUTPUT_RELATIVE, false);
					CheckButton(IDC_OUTPUT_ABSOLUTE, true);
					mbOutputRelative = false;
				}
			}
			break;

		case IDC_BROWSEOUTPUTFOLDER:
			{
				const VDStringW s(VDGetDirectory(kFileDialog_BatchOutputDir, (VDGUIHandle)mhdlg, L"Select output directory"));

				if (!s.empty()) {
					SetControlText(IDC_OUTPUTFOLDER, s.c_str());
					CheckButton(IDC_OUTPUT_RELATIVE, false);
					CheckButton(IDC_OUTPUT_ABSOLUTE, true);
					mbOutputRelative = false;
				}
			}
			return true;

		case IDC_ADDFILES:
			{
				DirspecEntry& fsent = *VDGetDirSpec(kFileDialog_ProcessDirIn);
				if (!&fsent) return true;

				OPENFILENAMEW ofn = {0};
				int buf_size = 1024*1024;
				wchar_t* szFile = (wchar_t*)malloc(buf_size*2);
				if (!szFile) return true;
				szFile[0]=0;

				ofn.lStructSize = sizeof(OPENFILENAMEW);
				ofn.hwndOwner = mhdlg;
				ofn.lpstrFilter = L"All files (*.*)\0*.*\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile = szFile;
				ofn.nMaxFile = buf_size;
				ofn.lpstrInitialDir = fsent.szFile;
				ofn.lpstrTitle = L"Select files";
				ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_ALLOWMULTISELECT;

				if (GetOpenFileNameW(&ofn)) {
					wchar_t* p = szFile;
					VDStringW dir(p);
					p += dir.length()+1;
					if (!*p) {
						vdrefptr<VDUIBatchWizardItem> item(new VDUIBatchWizardItem(dir.c_str()));
						mList.InsertVirtualItem(-1, item);
						wcscpy(fsent.szFile, VDFileSplitPathLeft(dir).c_str());
					} else {
						while(1) {
							if (!*p) break;
							VDStringW name(p);
							VDStringW path = VDMakePath(dir.c_str(), name.c_str());
							vdrefptr<VDUIBatchWizardItem> item(new VDUIBatchWizardItem(path.c_str()));
							mList.InsertVirtualItem(-1, item);
							p += name.length()+1;
						}
						wcscpy(fsent.szFile, dir.c_str());
					}
					mList.AutoSizeColumns();
				}

				free(szFile);
			}
			return true;

		case IDC_RENAMEFILES:
			if (!CheckAndConfirmConflicts())
				return true;
			{
				int n = mList.GetItemCount();
				int failed = 0;
				int succeeded = 0;

				const VDStringW outputPath(GetControlValueString(IDC_OUTPUTFOLDER));

				for(int i=0; i<n;) {
					VDUIBatchWizardItem *item = static_cast<VDUIBatchWizardItem *>(mList.GetVirtualItem(i));
					if (item) {
						const wchar_t *srcPath = item->GetFileName();
						const wchar_t *srcName = VDFileSplitPath(srcPath);
						const wchar_t *dstName = item->GetOutputName();
						const VDStringW& dstPath = VDMakePath(VDFileSplitPathLeft(VDStringW(srcPath)).c_str(), dstName);

						bool success = true;
						bool noop = false;

						// check if the filenames are actually the same (no-op).
						if (!wcscmp(srcName, dstName))
							noop = true;
						
						if (!noop) {
							VDStringW tempPath;

							// if the names differ only by case, we must rename through an intermediate name
							if (!_wcsicmp(srcName, dstName)) {
								tempPath = dstPath;
								tempPath.append_sprintf(L"-temp%08x", GetCurrentProcessId() ^ GetTickCount());

								success = !VDDoesPathExist(tempPath.c_str());
							} else {
								success = !VDDoesPathExist(dstPath.c_str());
							}

							if (success) {
								if (VDIsWindowsNT()) {
									if (!tempPath.empty()) {
										success = (0 != MoveFileW(srcPath, tempPath.c_str()));
										if (success) {
											success = (0 != MoveFileW(tempPath.c_str(), dstPath.c_str()));

											if (!success)
												MoveFileW(tempPath.c_str(), srcPath);
										}
									} else {
										success = (0 != MoveFileW(srcPath, dstPath.c_str()));
									}
								} else {
									VDStringA srcPathA(VDTextWToA(srcPath));
									VDStringA dstPathA(VDTextWToA(dstPath));

									if (!tempPath.empty()) {
										const VDStringA tempPathA(VDTextWToA(tempPath));

										success = (0 != MoveFileA(srcPathA.c_str(), tempPathA.c_str()));
										if (success) {
											success = (0 != MoveFileA(tempPathA.c_str(), dstPathA.c_str()));
											if (!success)
												MoveFileA(tempPathA.c_str(), srcPathA.c_str());
										}
									} else {
										success = (0 != MoveFileA(srcPathA.c_str(), dstPathA.c_str()));
									}
								}
							}
						}

						if (success) {
							mList.DeleteItem(i);
							--n;
							++succeeded;
						} else {
							++i;
							++failed;
						}
					}
				}

				VDStringA message;

				message.sprintf("%u file(s) renamed.", succeeded);

				if (failed)
					message.append_sprintf("\n\n%u file(s) could not be renamed and have been left in the list.", failed);

				MessageBox(mhdlg, message.c_str(), "VirtualDub notice", (failed ? MB_ICONWARNING : MB_ICONINFORMATION)|MB_OK);
			}
			return true;

		case IDC_ADDTOQUEUE:
			if (mhmenuPopups) {
				HWND hwndChild = GetDlgItem(mhdlg, IDC_ADDTOQUEUE);
				RECT r;
				if (GetWindowRect(hwndChild, &r))
					TrackPopupMenu(GetSubMenu(mhmenuPopups, 0), TPM_LEFTALIGN | TPM_TOPALIGN, r.left, r.bottom, 0, mhdlg, NULL);
			}
			return true;

		case IDC_FILTEROUTPUTNAMES:
			{
				VDUIBatchWizardNameFilter nameFilter;
				if (nameFilter.ShowDialog((VDGUIHandle)mhdlg)) {
					const bool matchCase = nameFilter.IsMatchCaseEnabled();
					const wchar_t *searchStr = nameFilter.GetSearchString();
					const wchar_t *replaceStr = nameFilter.GetReplaceString();
					int n = mList.GetItemCount();
					VDStringW s;
					size_t searchLen = wcslen(searchStr);
					size_t replaceLen = wcslen(replaceStr);

					if (searchLen) {
						for(int i=0; i<n; ++i) {
							VDUIBatchWizardItem *item = static_cast<VDUIBatchWizardItem *>(mList.GetVirtualItem(i));
							if (item) {
								s = item->GetOutputName();

								int pos = 0;
								bool found = false;
								for(;;) {
									const wchar_t *base = s.c_str();
									const wchar_t *t = NULL;
									
									if (matchCase)
										t = wcsstr(base + pos, searchStr);
									else {
										const wchar_t *start = base + pos;
										size_t left = wcslen(start);

										if (left >= searchLen) {
											const wchar_t *limit = start + left - searchLen + 1;
											for(const wchar_t *t2 = start; t2 != limit; ++t2) {
												if (!_wcsnicmp(t2, searchStr, searchLen)) {
													t = t2;
													break;
												}
											}
										}
									}

									if (!t)
										break;

									found = true;

									size_t offset = t - base;
									s.replace(offset, searchLen, replaceStr, replaceLen);

									pos = offset + replaceLen;
								}

								if (found) {
									item->SetOutputName(s.c_str());
									mList.RefreshItem(i);
								}
							}
						}
					}					
				}
			}
			return true;

		case ID_ADDTOQUEUE_RESAVEASAVI:
			if (!CheckAndConfirmConflicts())
				return true;

			{
				int n = mList.GetItemCount();

				const VDStringW outputPath(GetControlValueString(IDC_OUTPUTFOLDER));
				VDStringW outputFileName;

				for(int i=0; i<n; ++i) {
					VDUIBatchWizardItem *item = static_cast<VDUIBatchWizardItem *>(mList.GetVirtualItem(i));
					if (item) {
						const wchar_t *outputName = item->GetOutputName();

						if (mbOutputRelative)
							outputFileName = VDMakePath(VDFileSplitPathLeft(VDStringW(item->GetFileName())).c_str(), outputName);
						else
							outputFileName = VDMakePath(outputPath.c_str(), outputName);

						outputFileName = VDFileSplitExtLeft(outputFileName) + L".avi";

						JobAddBatchFile(item->GetFileName(), outputFileName.c_str());
					}
				}

				mList.Clear();
			}
			return true;

		case ID_ADDTOQUEUE_EXTRACTAUDIOASWAV:
		case ID_ADDTOQUEUE_EXTRACTRAWAUDIO:
			if (!CheckAndConfirmConflicts())
				return true;

			{
				const bool raw = (id == ID_ADDTOQUEUE_EXTRACTRAWAUDIO);
				const int n = mList.GetItemCount();

				const VDStringW outputPath(GetControlValueString(IDC_OUTPUTFOLDER));
				VDStringW outputFileName;

				for(int i=0; i<n; ++i) {
					VDUIBatchWizardItem *item = static_cast<VDUIBatchWizardItem *>(mList.GetVirtualItem(i));
					if (item) {
						const wchar_t *outputName = item->GetOutputName();

						if (mbOutputRelative)
							outputFileName = VDMakePath(VDFileSplitPathLeft(VDStringW(item->GetFileName())).c_str(), outputName);
						else
							outputFileName = VDMakePath(outputPath.c_str(), outputName);

						if (id == ID_ADDTOQUEUE_EXTRACTAUDIOASWAV)
							outputFileName = VDFileSplitExtLeft(outputFileName) + L".wav";

						JobAddConfigurationSaveAudio(0, &g_dubOpts, item->GetFileName(), NULL, 0, NULL, outputFileName.c_str(), raw, false);
					}
				}

				mList.Clear();
			}
			return true;

		case ID_ADDTOQUEUE_RUNVIDEOANALYSISPASS:
			if (!CheckAndConfirmConflicts())
				return true;

			{
				const int n = mList.GetItemCount();

				for(int i=0; i<n; ++i) {
					VDUIBatchWizardItem *item = static_cast<VDUIBatchWizardItem *>(mList.GetVirtualItem(i));
					if (item) {
						JobAddConfigurationRunVideoAnalysisPass(0, &g_dubOpts, item->GetFileName(), NULL, 0, NULL, false);
					}
				}

				mList.Clear();
			}
			return true;

		case ID_ADDTOQUEUE_CLI:
			if (!CheckAndConfirmConflicts())
				return true;

			{
				const int n = mList.GetItemCount();
				const VDStringW outputPath(GetControlValueString(IDC_OUTPUTFOLDER));
				VDStringW outputFileName;

				vdrefptr<VDExtEncSet> eset;
				if (!VDGetExternalEncoderSetAsk(mhdlg, ~eset)) return true;

				for(int i=0; i<n; ++i) {
					VDUIBatchWizardItem *item = static_cast<VDUIBatchWizardItem *>(mList.GetVirtualItem(i));
					if (item) {
						const wchar_t *outputName = item->GetOutputName();

						if (mbOutputRelative)
							outputFileName = VDMakePath(VDFileSplitPathLeft(VDStringW(item->GetFileName())).c_str(), outputName);
						else
							outputFileName = VDMakePath(outputPath.c_str(), outputName);

						if (!eset->mFileExt.empty())
							outputFileName = VDFileSplitExtLeft(outputFileName) + eset->mFileExt;

						JobAddConfigurationExportViaEncoder(0, &g_dubOpts, item->GetFileName(), NULL, 0, NULL, outputFileName.c_str(), false, eset->mName.c_str());
					}
				}
			}

			mList.Clear();
			return true;
	}

	return false;
}

void VDUIBatchWizard::OnDropFiles(IVDUIDropFileList *dropFileList) {
	VDStringW fileName;
	for(int index = 0; dropFileList->GetFileName(index, fileName); ++index) {
		const wchar_t *fn = fileName.c_str();

		vdrefptr<VDUIBatchWizardItem> item(new VDUIBatchWizardItem(fn));

		mList.InsertVirtualItem(-1, item);
	}

	mList.AutoSizeColumns();
}

bool VDUIBatchWizard::CheckAndConfirmConflicts() {
	typedef stdext::hash_multimap<uint32, const VDUIBatchWizardItem *> ConflictMap;
	ConflictMap conflictMap;

	int conflicts = 0;
	int n = mList.GetItemCount();

	for(int i=0; i<n; ++i) {
		VDUIBatchWizardItem *item = static_cast<VDUIBatchWizardItem *>(mList.GetVirtualItem(i));
		if (item) {
			const wchar_t *outputName = item->GetOutputName();
			uint32 hash = VDHashString32I(outputName);

			std::pair<ConflictMap::const_iterator, ConflictMap::const_iterator> result(conflictMap.equal_range(hash));
			for(; result.first != result.second; ++result.first) {
				const VDUIBatchWizardItem *item2 = result.first->second;

				if (!_wcsicmp(outputName, item2->GetOutputName())) {
					++conflicts;
					break;
				}
			}

			if (result.first == result.second)
				conflictMap.insert(ConflictMap::value_type(hash, item));
		}
	}

	if (conflicts) {
		VDStringA message;
		message.sprintf("%u file(s) have conflicting output names and will attempt to overwrite the results of other entries. Proceed anyway?", conflicts);
		return IDOK == MessageBox(mhdlg, message.c_str(), g_szWarning, MB_ICONWARNING | MB_OKCANCEL);
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////

void VDUIDisplayBatchWizard(VDGUIHandle hParent) {
	VDUIBatchWizard wiz;
	wiz.ShowDialog(hParent);
}
