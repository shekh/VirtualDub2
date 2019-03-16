#include "stdafx.h"
#include <windows.h>
#include <commctrl.h>
#include <vd2/system/filesys.h>
#include <vd2/VDLib/Dialog.h>
#include <vd2/VDLib/UIProxies.h>
#include "AccelEditDialog.h"
#include "HotKeyExControl.h"
#include "gui.h"
#include "resource.h"

extern const char g_szWarning[];

///////////////////////////////////////////////////////////////////////////

class VDDialogEditAccelerators : public VDDialogFrameW32 {
public:
	VDDialogEditAccelerators(const VDAccelToCommandEntry *commands, uint32 commandCount, VDAccelTableDefinition& table, VDAccelTableDefinition& defaultTable);
	~VDDialogEditAccelerators();

protected:
	VDZINT_PTR DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);

	void OnDataExchange(bool write);
	bool OnLoaded();
	bool OnCommand(uint32 id, uint32 extcode);
	void OnSize();
	void OnDestroy();
	void LoadTable(const VDAccelTableDefinition& table);
	void RefilterCommands(const char *pattern);
	void RefreshBoundList();
	void DestroyBoundCommands();

	void OnColumnClicked(VDUIProxyListView *source, int column);
	void OnItemSelectionChanged(VDUIProxyListView *source, int index);
	void OnHotKeyChanged(IVDUIHotKeyExControl *source, const VDUIAccelerator& accel);

	typedef vdfastvector<const VDAccelToCommandEntry *> Commands;

	Commands	mAllCommands;
	Commands	mFilteredCommands;

	struct BoundCommand : public vdrefcounted<IVDUIListViewVirtualItem>, public VDAccelTableEntry {
		void GetText(int subItem, VDStringW& s) const;
	};

	struct BoundCommandSort {
		bool operator()(const BoundCommand *x, const BoundCommand *y) const;

		bool	mbSortByKey;
		bool	mbSortReverse;
	};

	BoundCommandSort	mBoundCommandSort;

	typedef vdfastvector<BoundCommand *> BoundCommands;
	BoundCommands	mBoundCommands;
	VDAccelTableDefinition&	mBoundCommandsResult;
	const VDAccelTableDefinition&	mBoundCommandsDefault;

	VDUIProxyListView		mListViewBoundCommands;
	vdrefptr<IVDUIHotKeyExControl>	mpHotKeyControl;

	VDDelegate	mDelegateColumnClicked;
	VDDelegate	mDelegateItemSelectionChanged;
	VDDelegate	mDelegateHotKeyChanged;

	bool	mbBlockCommandUpdate;

	RECT mrInitial;
	VDDialogResizerW32		mResizer;
};

namespace {
	struct CommandSort {
		bool operator()(const VDAccelToCommandEntry *x, const VDAccelToCommandEntry *y) const {
			return _stricmp(x->mpName, y->mpName) < 0;
		}
	};
}

bool VDDialogEditAccelerators::BoundCommandSort::operator()(const BoundCommand *x, const BoundCommand *y) const {
	if (mbSortReverse) {
		const BoundCommand *t = x;
		x = y;
		y = t;
	}

	if (mbSortByKey) {
		if (x->mAccel.mModifiers != y->mAccel.mModifiers)
			return x->mAccel.mModifiers < y->mAccel.mModifiers;

		return x->mAccel.mVirtKey < y->mAccel.mVirtKey;
	} else
		return _stricmp(x->mpCommand, y->mpCommand) < 0;

}

VDDialogEditAccelerators::VDDialogEditAccelerators(const VDAccelToCommandEntry *commands, uint32 commandCount, VDAccelTableDefinition& table, VDAccelTableDefinition& defaultTable)
	: VDDialogFrameW32(IDD_CONFIGURE_ACCELS)
	, mAllCommands(commandCount)
	, mBoundCommandsResult(table)
	, mBoundCommandsDefault(defaultTable)
	, mbBlockCommandUpdate(false)
{
	memset(&mrInitial, 0, sizeof mrInitial);

	mBoundCommandSort.mbSortByKey = true;
	mBoundCommandSort.mbSortReverse = false;

	for(uint32 i=0; i<commandCount; ++i)
		mAllCommands[i] = &commands[i];

	std::sort(mAllCommands.begin(), mAllCommands.end(), CommandSort());

	mListViewBoundCommands.OnColumnClicked() += mDelegateColumnClicked.Bind(this, &VDDialogEditAccelerators::OnColumnClicked);
	mListViewBoundCommands.OnItemSelectionChanged() += mDelegateItemSelectionChanged.Bind(this, &VDDialogEditAccelerators::OnItemSelectionChanged);
}

VDDialogEditAccelerators::~VDDialogEditAccelerators() {
	DestroyBoundCommands();
}

VDZINT_PTR VDDialogEditAccelerators::DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam) {
	switch(msg) {
		case WM_GETMINMAXINFO:
			if (mrInitial.right > mrInitial.left) {
				LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;

				lpmmi->ptMinTrackSize.x = mrInitial.right - mrInitial.left;
				lpmmi->ptMinTrackSize.y = mrInitial.bottom - mrInitial.top;
				return TRUE;
			}

			break;
	}

	return VDDialogFrameW32::DlgProc(msg, wParam, lParam);
}

void VDDialogEditAccelerators::OnDataExchange(bool write) {
	if (write) {
		size_t n = mBoundCommands.size();

		VDAccelTableDefinition newTable;
		
		for(size_t i=0; i<n; ++i) {
			const BoundCommand& ent = *mBoundCommands[i];

			newTable.Add(ent);
		}

		mBoundCommandsResult.Swap(newTable);
	} else {
		LoadTable(mBoundCommandsResult);
	}
}

bool VDDialogEditAccelerators::OnLoaded() {
	VDSetDialogDefaultIcons(mhdlg);

	GetWindowRect(mhdlg, &mrInitial);

	mpHotKeyControl = VDGetUIHotKeyExControl((VDGUIHandle)GetControl(IDC_HOTKEY));
	if (mpHotKeyControl)
		mpHotKeyControl->OnChange() += mDelegateHotKeyChanged(this, &VDDialogEditAccelerators::OnHotKeyChanged);

	mResizer.Init(mhdlg);
	mResizer.Add(IDOK, VDDialogResizerW32::kBR);
	mResizer.Add(IDCANCEL, VDDialogResizerW32::kBR);
	mResizer.Add(IDC_ADD, VDDialogResizerW32::kBR);
	mResizer.Add(IDC_REMOVE, VDDialogResizerW32::kBR);
	mResizer.Add(IDC_RESET, VDDialogResizerW32::kBL);
	mResizer.Add(IDC_HOTKEY, VDDialogResizerW32::kBC);
	mResizer.Add(IDC_STATIC_QUICKSEARCH, VDDialogResizerW32::kBL);
	mResizer.Add(IDC_STATIC_SHORTCUT, VDDialogResizerW32::kBL);
	mResizer.Add(IDC_STATIC_AVAILABLECOMMANDS, VDDialogResizerW32::kAnchorX2_C);
	mResizer.Add(IDC_STATIC_BOUNDCOMMANDS, VDDialogResizerW32::kAnchorX1_C | VDDialogResizerW32::kAnchorX2_R);
	mResizer.Add(IDC_AVAILCOMMANDS, VDDialogResizerW32::kAnchorX2_C | VDDialogResizerW32::kAnchorY2_B);
	mResizer.Add(IDC_BOUNDCOMMANDS, VDDialogResizerW32::kAnchorX1_C
		| VDDialogResizerW32::kAnchorX2_R
		| VDDialogResizerW32::kAnchorY2_B
		);
	mResizer.Add(IDC_FILTER, VDDialogResizerW32::kAnchorY1_B | VDDialogResizerW32::kAnchorX2_C | VDDialogResizerW32::kAnchorY2_B);
	mResizer.Add(IDC_HOTKEY, VDDialogResizerW32::kBC);

	AddProxy(&mListViewBoundCommands, IDC_BOUNDCOMMANDS);

	mListViewBoundCommands.SetFullRowSelectEnabled(true);
	mListViewBoundCommands.InsertColumn(0, L"Command", 50);
	mListViewBoundCommands.InsertColumn(1, L"Shortcut", 50);
	mListViewBoundCommands.AutoSizeColumns();

	RefilterCommands("*");

	VDDialogFrameW32::OnLoaded();

	SetFocusToControl(IDC_FILTER);
	return true;
}

bool VDDialogEditAccelerators::OnCommand(uint32 id, uint32 extcode) {
	if (id == IDC_AVAILCOMMANDS) {
		if (extcode==LBN_SELCHANGE) {
			if (mpHotKeyControl)
				mpHotKeyControl->Clear();
			mbBlockCommandUpdate = true;
			mListViewBoundCommands.SetSelectedIndex(-1);
			mbBlockCommandUpdate = false;
			EnableControl(IDC_REMOVE, false);
				EnableControl(IDC_ADD, false);

			int selIdx = LBGetSelectedIndex(IDC_AVAILCOMMANDS);
			if ((size_t)selIdx < mFilteredCommands.size()) {
				const VDAccelToCommandEntry *ace = mFilteredCommands[selIdx];
				EnableControl(IDC_ADD, true);

				BoundCommands::const_iterator it(mBoundCommands.begin()), itEnd(mBoundCommands.end());
				int index = 0;

				for(; it != itEnd; ++it, ++index) {
					BoundCommand *bc = *it;

					if (bc->mCommandId==ace->mId) {
						if (mpHotKeyControl)
							mpHotKeyControl->SetAccelerator(bc->mAccel);
						mbBlockCommandUpdate = true;
						mListViewBoundCommands.SetSelectedIndex(index);
						mbBlockCommandUpdate = false;
						mListViewBoundCommands.EnsureItemVisible(index);
						EnableControl(IDC_REMOVE, true);
						break;
					}
				}
			}
		}
	} else if (id == IDC_FILTER) {
		if (extcode == EN_CHANGE) {
			VDStringA s("*");
			s += VDTextWToA(GetControlValueString(id)).c_str();
			s += '*';

			RefilterCommands(s.c_str());
			return true;
		}
	} else if (id == IDC_ADD) {
		VDUIAccelerator accel;

		int selIdx = LBGetSelectedIndex(IDC_AVAILCOMMANDS);

		if ((size_t)selIdx < mFilteredCommands.size()) {
			const VDAccelToCommandEntry *ace = mFilteredCommands[selIdx];

			if (mpHotKeyControl) {
				mpHotKeyControl->GetAccelerator(accel);

				// Look for a conflicting command.
				for(BoundCommands::iterator it(mBoundCommands.begin()), itEnd(mBoundCommands.end()); it != itEnd; ++it) {
					BoundCommand *obc = *it;

					if (obc->mAccel == accel) {
						VDStringW keyName;
						VDUIGetAcceleratorString(accel, keyName);

						VDStringA msg;
						msg.sprintf("The key %ls is already bound to %hs. Rebind it to %hs?", keyName.c_str(), obc->mpCommand, ace->mpName);

						if (IDOK != MessageBox(mhdlg, msg.c_str(), g_szWarning, MB_OKCANCEL | MB_ICONEXCLAMATION))
							return true;

						mBoundCommands.erase(it);
						obc->Release();
					}
				}

				vdrefptr<BoundCommand> bc(new_nothrow BoundCommand);
				
				if (bc) {
					bc->mpCommand = ace->mpName;
					bc->mCommandId = ace->mId;
					bc->mAccel = accel;
					mBoundCommands.push_back(bc);
					int index = mBoundCommands.size()-1;
					mListViewBoundCommands.InsertVirtualItem(index, bc);
					mbBlockCommandUpdate = true;
					mListViewBoundCommands.SetSelectedIndex(index);
					mbBlockCommandUpdate = false;
					bc.release();
					RefreshBoundList();
					EnableControl(IDC_REMOVE, true);
				}
			}
		}

		return true;
	} else if (id == IDC_REMOVE) {
		int selIdx = mListViewBoundCommands.GetSelectedIndex();

		if ((unsigned)selIdx < mBoundCommands.size()) {
			BoundCommand *bc = mBoundCommands[selIdx];

			mBoundCommands.erase(mBoundCommands.begin() + selIdx);

			bc->Release();

			RefreshBoundList();
			mbBlockCommandUpdate = true;
			mListViewBoundCommands.SetSelectedIndex(-1);
			mbBlockCommandUpdate = false;
			EnableControl(IDC_REMOVE, false);
		}

		return true;
	} else if (id == IDC_RESET) {
		if (IDOK == MessageBox(mhdlg, "Really reset?", g_szWarning, MB_OKCANCEL | MB_ICONEXCLAMATION))
			LoadTable(mBoundCommandsDefault);

		return true;
	}

	return false;
}

void VDDialogEditAccelerators::OnSize() {
	mResizer.Relayout();
}

void VDDialogEditAccelerators::OnDestroy() {
	mListViewBoundCommands.Clear();
}

void VDDialogEditAccelerators::RefilterCommands(const char *pattern) {
	mFilteredCommands.clear();

	LBClear(IDC_AVAILCOMMANDS);

	Commands::const_iterator it(mAllCommands.begin()), itEnd(mAllCommands.end());
	for(; it != itEnd; ++it) {
		const VDAccelToCommandEntry& ent = **it;

		if (VDFileWildMatch(pattern, ent.mpName)) {
			const VDStringW s(VDTextAToW(ent.mpName));

			mFilteredCommands.push_back(&ent);
			LBAddString(IDC_AVAILCOMMANDS, s.c_str());
		}
	}

	if (mFilteredCommands.size()>0)
		LBSetSelectedIndex(IDC_AVAILCOMMANDS, 0);
	else
		LBSetSelectedIndex(IDC_AVAILCOMMANDS, -1);
	OnCommand(IDC_AVAILCOMMANDS, LBN_SELCHANGE);
}

void VDDialogEditAccelerators::LoadTable(const VDAccelTableDefinition& table) {
	size_t n = table.GetSize();

	DestroyBoundCommands();
	mBoundCommands.reserve(n);

	for(size_t i=0; i<n; ++i) {
		vdrefptr<BoundCommand> bc(new_nothrow BoundCommand);
		if (!bc)
			break;

		const VDAccelTableEntry& ent = table[i];

		static_cast<VDAccelTableEntry&>(*bc) = ent;

		mBoundCommands.push_back(bc.release());
	}

	RefreshBoundList();
}

void VDDialogEditAccelerators::RefreshBoundList() {
	int visIdx = mListViewBoundCommands.GetVisibleTopIndex();
	int selIdx = mListViewBoundCommands.GetSelectedIndex();
	BoundCommand *sel_bc = 0;
	if (selIdx!=-1) sel_bc = mBoundCommands[selIdx];

	std::sort(mBoundCommands.begin(), mBoundCommands.end(), mBoundCommandSort);

	mListViewBoundCommands.Clear();

	BoundCommands::const_iterator it(mBoundCommands.begin()), itEnd(mBoundCommands.end());
	int index = 0;

	for(; it != itEnd; ++it) {
		BoundCommand *bc = *it;
		if (bc==sel_bc) selIdx = index;
		mListViewBoundCommands.InsertVirtualItem(index++, bc);
	}

	mListViewBoundCommands.AutoSizeColumns();
	mbBlockCommandUpdate = true;
	mListViewBoundCommands.SetSelectedIndex(selIdx);
	mbBlockCommandUpdate = false;
	if (selIdx==-1)
		mListViewBoundCommands.SetVisibleTopIndex(visIdx);
	else
		mListViewBoundCommands.SetVisibleTopIndex(selIdx);
}

void VDDialogEditAccelerators::DestroyBoundCommands() {
	mListViewBoundCommands.Clear();

	while(!mBoundCommands.empty()) {
		BoundCommand *bc = mBoundCommands.back();
		mBoundCommands.pop_back();

		bc->Release();
	}
}

void VDDialogEditAccelerators::OnColumnClicked(VDUIProxyListView *source, int column) {
	bool sortByKey = (column > 0);

	if (sortByKey == mBoundCommandSort.mbSortByKey)
		mBoundCommandSort.mbSortReverse = !mBoundCommandSort.mbSortReverse;
	else
		mBoundCommandSort.mbSortByKey = sortByKey;

	RefreshBoundList();
}

void VDDialogEditAccelerators::OnItemSelectionChanged(VDUIProxyListView *source, int index) {
	if (index < 0 || mbBlockCommandUpdate)
		return;

	const BoundCommand& bcmd = *mBoundCommands[index];

	if (mpHotKeyControl)
		mpHotKeyControl->SetAccelerator(bcmd.mAccel);

	uint32 n = mFilteredCommands.size();
	int cmdSelIndex = -1;

	for(uint32 i=0; i<n; ++i) {
		const VDAccelToCommandEntry& cent = *mFilteredCommands[i];

		if (!_stricmp(cent.mpName, bcmd.mpCommand)) {
			cmdSelIndex = i;
			break;
		}
	}

	LBSetSelectedIndex(IDC_AVAILCOMMANDS, cmdSelIndex);
	EnableControl(IDC_ADD, cmdSelIndex!=-1);
	EnableControl(IDC_REMOVE, true);
}

void VDDialogEditAccelerators::OnHotKeyChanged(IVDUIHotKeyExControl *source, const VDUIAccelerator& accel) {
	BoundCommands::const_iterator it(mBoundCommands.begin()), itEnd(mBoundCommands.end());
	int index = 0;
	const BoundCommand* bcmd = 0;

	for(; it != itEnd; ++it, ++index) {
		BoundCommand *bc = *it;

		if (bc->mAccel == accel) {
			bcmd = bc;
			break;
		}
	}

	if (bcmd) {
		mbBlockCommandUpdate = true;
		mListViewBoundCommands.SetSelectedIndex(index);
		mbBlockCommandUpdate = false;
		mListViewBoundCommands.EnsureItemVisible(index);
		EnableControl(IDC_REMOVE, true);
		/*
		uint32 n = mFilteredCommands.size();
		int cmdSelIndex = -1;

		for(uint32 i=0; i<n; ++i) {
			const VDAccelToCommandEntry& cent = *mFilteredCommands[i];

			if (!_stricmp(cent.mpName, bcmd->mpCommand)) {
				cmdSelIndex = i;
				break;
			}
		}

		LBSetSelectedIndex(IDC_AVAILCOMMANDS, cmdSelIndex);
		EnableControl(IDC_ADD, cmdSelIndex!=-1);
		*/
	} else {
		mbBlockCommandUpdate = true;
		mListViewBoundCommands.SetSelectedIndex(-1);
		mbBlockCommandUpdate = false;
		EnableControl(IDC_REMOVE, false);
		/*
		LBSetSelectedIndex(IDC_AVAILCOMMANDS, -1);
		EnableControl(IDC_ADD, false);
		*/
	}
}

///////////////////////////////////////////////////////////////////////////

void VDDialogEditAccelerators::BoundCommand::GetText(int subItem, VDStringW& s) const {
	if (subItem == 0) {
		s = VDTextAToW(mpCommand);
	} else if (subItem == 1) {
		VDUIGetAcceleratorString(mAccel, s);
	}
}

///////////////////////////////////////////////////////////////////////////

bool VDShowDialogEditAccelerators(VDGUIHandle hParent, const VDAccelToCommandEntry *commands, uint32 commandCount, VDAccelTableDefinition& accelTable, VDAccelTableDefinition& defaultTable) {
	VDDialogEditAccelerators dlg(commands, commandCount, accelTable, defaultTable);

	return dlg.ShowDialog(hParent) != 0;
}
