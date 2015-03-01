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
#include <vd2/system/w32assist.h>
#include <vd2/system/vdstl.h>
#include <windows.h>
#include <commctrl.h>
#include <vector>

#include <vd2/Dita/w32control.h>

#ifdef _MSC_VER
	#pragma comment(lib, "comctl32")
#endif

class VDUIListViewW32 : public VDUIControlW32, public IVDUIListView, public IVDUIList {
public:
	VDUIListViewW32();

	void *AsInterface(uint32 id);

	bool Create(IVDUIParameters *);
	void PreLayoutBaseW32(const VDUILayoutSpecs&);

	int GetValue();
	void SetValue(int);

protected:
	void Clear();
	int GetItemCount();
	uintptr GetItemData(int index);
	void SetItemData(int index, uintptr value);
	int AddItem(const wchar_t *text, uintptr data);

	void SetListCallback(IVDUIListCallback *cb);
	void SetItemText(int item, int subitem, const wchar_t *text);
	void AddColumn(const wchar_t *name, int width, int affinity);
	void SetItemChecked(int item, bool checked);
	bool IsItemChecked(int item);
	void OnNotifyCallback(const NMHDR *);
	void OnResize();

	struct Column {
		int mWidth;
		int mAffinity;
	};

	int mSelected;
	int		mTotalAffinity;
	int		mTotalWidth;
	bool	mbCheckable;
	vdfastvector<Column>	mColumns;

	IVDUIListCallback *mpListCB;

	// Temporary storage for strings that have been returned to the list view. We must
	// triple buffer these according to the docs for NMLVDISPINFO.
	int mListTextIndex;
	VDStringW	mListTextW[3];
	VDStringA	mListTextA[3];
};

extern IVDUIWindow *VDCreateUIListView() { return new VDUIListViewW32; }

VDUIListViewW32::VDUIListViewW32()
	: mSelected(-1)
	, mTotalAffinity(0)
	, mTotalWidth(0)
	, mpListCB(NULL)
	, mListTextIndex(0)
{
	InitCommonControls();
}

void *VDUIListViewW32::AsInterface(uint32 id) {
	if (id == IVDUIListView::kTypeID) return static_cast<IVDUIListView *>(this);
	if (id == IVDUIList::kTypeID) return static_cast<IVDUIList *>(this);

	return VDUIControlW32::AsInterface(id);
}

bool VDUIListViewW32::Create(IVDUIParameters *pParameters) {
	mbCheckable = pParameters->GetB(nsVDUI::kUIParam_Checkable, false);

	DWORD dwFlags = LVS_REPORT | WS_TABSTOP;

	if (pParameters->GetB(nsVDUI::kUIParam_NoHeader, false))
		dwFlags |= LVS_NOCOLUMNHEADER;

	if (!CreateW32(pParameters, WC_LISTVIEW, dwFlags))
		return false;

	ListView_SetExtendedListViewStyle(mhwnd, LVS_EX_FULLROWSELECT | ListView_GetExtendedListViewStyle(mhwnd));

	if (mbCheckable) {
		const int cx = GetSystemMetrics(SM_CXMENUCHECK);
		const int cy = GetSystemMetrics(SM_CYMENUCHECK);

		if (HBITMAP hbm = CreateBitmap(cx, cy, 1, 1, NULL)) {
			if (HDC hdc = CreateCompatibleDC(NULL)) {
				if (HGDIOBJ hbmOld = SelectObject(hdc, hbm)) {
					bool success = false;

					RECT r = { 0, 0, cx, cy };

					SetBkColor(hdc, PALETTEINDEX(0));
					ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &r, "", 0, NULL);
					DrawFrameControl(hdc, &r, DFC_BUTTON, DFCS_BUTTONCHECK|DFCS_CHECKED);

					SelectObject(hdc, hbmOld);

					if (HIMAGELIST himl = ImageList_Create(cx, cy, ILC_COLOR, 1, 1)) {
						if (ImageList_Add(himl, hbm, NULL) >= 0)
							ListView_SetImageList(mhwnd, himl, LVSIL_STATE);
						else
							ImageList_Destroy(himl);
					}
				}

				DeleteDC(hdc);
			}

			DeleteObject(hbm);
		}
	}

	return true;
}

void VDUIListViewW32::PreLayoutBaseW32(const VDUILayoutSpecs& parentConstraints) {
}

int VDUIListViewW32::GetValue() {
	return mSelected;
}

void VDUIListViewW32::SetValue(int value) {
	if (mSelected != value) {
		mSelected = value;		// prevents recursion

		if (value >= 0)
			ListView_SetItemState(mhwnd, value, LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED);
	}
}

void VDUIListViewW32::Clear() {
	ListView_DeleteAllItems(mhwnd);
}

int VDUIListViewW32::GetItemCount() {
	return (int)ListView_GetItemCount(mhwnd);
}

uintptr VDUIListViewW32::GetItemData(int index) {
	LVITEMA lvia={0};
	lvia.mask		= LVIF_PARAM;
	lvia.iItem		= index;
	lvia.iSubItem	= 0;

	if (ListView_GetItem(mhwnd, &lvia))
		return lvia.lParam;

	return 0;
}

void VDUIListViewW32::SetItemData(int index, uintptr value) {
	LVITEMA lvia={0};
	lvia.mask		= LVIF_PARAM;
	lvia.iItem		= index;
	lvia.iSubItem	= 0;
	lvia.lParam		= (LPARAM)value;

	ListView_SetItem(mhwnd, &lvia);
}

int VDUIListViewW32::AddItem(const wchar_t *text, uintptr data) {
	DWORD dwMask = LVIF_PARAM | LVIF_TEXT;

	if (mbCheckable)
		dwMask |= LVIF_STATE;

	int item;
	if (VDIsWindowsNT()) {
		LVITEMW lviw={0};

		lviw.mask		= dwMask;
		lviw.iItem		= 0x1FFFFFFF;
		lviw.iSubItem	= 0;
		lviw.state		= 0x1000;
		lviw.stateMask	= (UINT)-1;
		lviw.pszText	= (LPWSTR)text;
		lviw.lParam		= (LPARAM)data;

		item = (int)SendMessageW(mhwnd, LVM_INSERTITEMW, 0, (LPARAM)&lviw);
	} else {
		LVITEMA lvia={0};

		VDStringA textA(VDTextWToA(text));

		lvia.mask		= dwMask;
		lvia.iItem		= 0x1FFFFFFF;
		lvia.iSubItem	= 0;
		lvia.state		= 0x1000;
		lvia.stateMask	= (UINT)-1;
		lvia.pszText	= (LPSTR)textA.c_str();
		lvia.lParam		= (LPARAM)data;

		item = (int)SendMessageA(mhwnd, LVM_INSERTITEMA, 0, (LPARAM)&lvia);
	}

	return item;
}

void VDUIListViewW32::SetListCallback(IVDUIListCallback *cb) {
	mpListCB = cb;
}

void VDUIListViewW32::SetItemText(int item, int subitem, const wchar_t *text) {
	DWORD dwMask = LVIF_TEXT;

	if (VDIsWindowsNT()) {
		LVITEMW lviw={0};

		lviw.mask		= dwMask;
		lviw.iItem		= item;
		lviw.iSubItem	= subitem;
		lviw.pszText	= (LPWSTR)text;

		SendMessageW(mhwnd, LVM_SETITEMTEXTW, (WPARAM)item, (LPARAM)&lviw);
	} else {
		LVITEMA lvia={0};

		VDStringA textA(VDTextWToA(text));

		lvia.mask		= dwMask;
		lvia.iItem		= item;
		lvia.iSubItem	= subitem;
		lvia.pszText	= (LPSTR)textA.c_str();

		SendMessageA(mhwnd, LVM_SETITEMTEXTA, (WPARAM)item, (LPARAM)&lvia);
	}
}

void VDUIListViewW32::AddColumn(const wchar_t *name, int width, int affinity) {
	VDASSERT(affinity >= 0);
	VDASSERT(width >= 0);

	if (VDIsWindowsNT()) {
		LVCOLUMNW lvcw={0};

		lvcw.mask		= LVCF_TEXT | LVCF_WIDTH;
		lvcw.pszText	= (LPWSTR)name;
		lvcw.cx			= width;

		SendMessageW(mhwnd, LVM_INSERTCOLUMNW, mColumns.size(), (LPARAM)&lvcw);
	} else {
		LVCOLUMNA lvca={0};
		VDStringA nameA(VDTextWToA(name));

		lvca.mask		= LVCF_TEXT | LVCF_WIDTH;
		lvca.pszText	= (LPSTR)nameA.c_str();
		lvca.cx			= width;

		SendMessageA(mhwnd, LVM_INSERTCOLUMNA, mColumns.size(), (LPARAM)&lvca);
	}

	mColumns.push_back(Column());
	Column& col = mColumns.back();

	col.mWidth		= width;
	col.mAffinity	= affinity;

	mTotalWidth		+= width;
	mTotalAffinity	+= affinity;

	OnResize();
}

void VDUIListViewW32::SetItemChecked(int item, bool checked) {
	ListView_SetItemState(mhwnd, item, checked ? INDEXTOSTATEIMAGEMASK(1) : 0, LVIS_STATEIMAGEMASK);
	ListView_RedrawItems(mhwnd, item, item);
}

bool VDUIListViewW32::IsItemChecked(int item) {
	UINT oldState = ListView_GetItemState(mhwnd, item, -1);

	return 0 != (oldState & 0x1000);
}

void VDUIListViewW32::OnNotifyCallback(const NMHDR *pHdr) {
   	if (pHdr->code == LVN_ITEMCHANGED) {
   		const NMLISTVIEW *plvn = (const NMLISTVIEW *)pHdr;
   
   		if ((plvn->uOldState|plvn->uNewState) & LVIS_SELECTED) {
   			int iSel = (int)SendMessage(mhwnd, LVM_GETNEXTITEM, -1, LVNI_ALL|LVNI_SELECTED);
   
   			if (iSel != mSelected) {
   				mSelected = iSel;
   
				mpBase->ProcessValueChange(this, mID);
   				mpBase->DispatchEvent(this, mID, IVDUICallback::kEventSelect, mSelected);
   			}
   		}
	} else if (pHdr->code == LVN_GETDISPINFO) {
		NMLVDISPINFOA& lvdi = *(NMLVDISPINFOA *)pHdr;
		LVITEMA& lvi = lvdi.item;

		VDASSERTCT(sizeof(NMLVDISPINFOW) == sizeof(NMLVDISPINFOA));

		if (mpListCB && (lvi.mask & LVIF_TEXT)) {
			if (mpListCB->GetListText(lvi.iItem, lvi.iSubItem, mListTextW[mListTextIndex])) {
				if (VDIsWindowsNT()) {
					NMLVDISPINFOW& lvdiw = *(NMLVDISPINFOW *)pHdr;
					LVITEMW& lviw = lvdiw.item;

					lviw.pszText = const_cast<WCHAR *>(mListTextW[mListTextIndex].c_str());
				} else {
					mListTextA[mListTextIndex] = VDTextWToA(mListTextW[mListTextIndex]);

					lvi.pszText = const_cast<CHAR *>(mListTextA[mListTextIndex].c_str());
				}

				if (++mListTextIndex >= 3)
					mListTextIndex = 0;
			}
		}
	} else if (mbCheckable) {
		if (pHdr->code == LVN_KEYDOWN && ((const NMLVKEYDOWN *)pHdr)->wVKey == VK_SPACE) {
			int idx = -1;
			bool first = true;
			bool select = false;

			while((idx = ListView_GetNextItem(mhwnd, idx, LVNI_SELECTED)) >= 0) {
				if (first) {
					UINT oldState = ListView_GetItemState(mhwnd, idx, -1);

					select = !(oldState & INDEXTOSTATEIMAGEMASK(1));
					first = false;
				}

				ListView_SetItemState(mhwnd, idx, select ? INDEXTOSTATEIMAGEMASK(1) : 0, LVIS_STATEIMAGEMASK);
				ListView_RedrawItems(mhwnd, idx, idx);
			}
		} else if (pHdr->code == NM_CLICK || pHdr->code == NM_DBLCLK) {
 			DWORD pos = GetMessagePos();

			LVHITTESTINFO lvhi = {0};

			lvhi.pt.x = (SHORT)LOWORD(pos);
			lvhi.pt.y = (SHORT)HIWORD(pos);

			ScreenToClient(mhwnd, &lvhi.pt);

			int idx = ListView_HitTest(mhwnd, &lvhi);

			if (idx >= 0) {
				UINT oldState = ListView_GetItemState(mhwnd, idx, -1);

				ListView_SetItemState(mhwnd, idx, oldState ^ INDEXTOSTATEIMAGEMASK(1), LVIS_STATEIMAGEMASK);
				ListView_RedrawItems(mhwnd, idx, idx);
			}
		}
	}
}

void VDUIListViewW32::OnResize() {
	const int ncols = mColumns.size();
	const Column *pcol = mColumns.data();
	RECT r;

	GetClientRect(mhwnd, &r);

	int spaceLeft = r.right - mTotalWidth;
	int affinityLeft = mTotalAffinity;

	for(int i=0; i<ncols; ++i) {
		const Column& col = *pcol++;
		int width = col.mWidth;

		if (affinityLeft && col.mAffinity) {
			int extra = (spaceLeft * col.mAffinity + affinityLeft - 1) / affinityLeft;

			affinityLeft -= col.mAffinity;
			spaceLeft -= extra;
			width += extra;
		}	

		SendMessageA(mhwnd, LVM_SETCOLUMNWIDTH, i, MAKELPARAM((int)width, 0));
	}
}
