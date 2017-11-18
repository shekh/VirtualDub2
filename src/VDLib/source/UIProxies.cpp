#include "stdafx.h"
#include <windows.h>
#include <commctrl.h>

#include <vd2/system/w32assist.h>
#include <vd2/Dita/accel.h>
#include <vd2/VDLib/UIProxies.h>

///////////////////////////////////////////////////////////////////////////////

VDUIProxyControl::VDUIProxyControl()
	: mhwnd(NULL)
	, mRedrawInhibitCount(0)
{
}

void VDUIProxyControl::Attach(VDZHWND hwnd) {
	VDASSERT(IsWindow(hwnd));
	mhwnd = hwnd;
}

void VDUIProxyControl::Detach() {
	mhwnd = NULL;
}

void VDUIProxyControl::SetArea(const vdrect32& r) {
	if (mhwnd)
		SetWindowPos(mhwnd, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

void VDUIProxyControl::SetRedraw(bool redraw) {
	if (redraw) {
		if (!--mRedrawInhibitCount) {
			if (mhwnd)
				SendMessage(mhwnd, WM_SETREDRAW, TRUE, 0);
		}
	} else {
		if (!mRedrawInhibitCount++) {
			if (mhwnd)
				SendMessage(mhwnd, WM_SETREDRAW, FALSE, 0);
		}
	}
}

VDZLRESULT VDUIProxyControl::On_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam) {
	return 0;
}

VDZLRESULT VDUIProxyControl::On_WM_COMMAND(VDZWPARAM wParam, VDZLPARAM lParam) {
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

void VDUIProxyMessageDispatcherW32::AddControl(VDUIProxyControl *control) {
	VDZHWND hwnd = control->GetHandle();
	size_t hc = Hash(hwnd);

	mHashTable[hc].push_back(control);
}

void VDUIProxyMessageDispatcherW32::RemoveControl(VDZHWND hwnd) {
	size_t hc = Hash(hwnd);
	HashChain& hchain = mHashTable[hc];

	HashChain::iterator it(hchain.begin()), itEnd(hchain.end());
	for(; it != itEnd; ++it) {
		VDUIProxyControl *control = *it;

		if (control->GetHandle() == hwnd) {
			hchain.erase(control);
			break;
		}
	}

}

void VDUIProxyMessageDispatcherW32::RemoveAllControls(bool detach) {
	for(int i=0; i<kHashTableSize; ++i) {
		HashChain& hchain = mHashTable[i];

		if (detach) {
			HashChain::iterator it(hchain.begin()), itEnd(hchain.end());
			for(; it != itEnd; ++it) {
				VDUIProxyControl *control = *it;

				control->Detach();
			}
		}

		hchain.clear();
	}
}

VDZLRESULT VDUIProxyMessageDispatcherW32::Dispatch_WM_COMMAND(VDZWPARAM wParam, VDZLPARAM lParam) {
	VDUIProxyControl *control = GetControl((HWND)lParam);

	if (control)
		return control->On_WM_COMMAND(wParam, lParam);

	return 0;
}

VDZLRESULT VDUIProxyMessageDispatcherW32::Dispatch_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam) {
	const NMHDR *hdr = (const NMHDR *)lParam;
	VDUIProxyControl *control = GetControl(hdr->hwndFrom);

	if (control)
		return control->On_WM_NOTIFY(wParam, lParam);

	return 0;
}

size_t VDUIProxyMessageDispatcherW32::Hash(VDZHWND hwnd) const {
	return (size_t)hwnd % (size_t)kHashTableSize;
}

VDUIProxyControl *VDUIProxyMessageDispatcherW32::GetControl(VDZHWND hwnd) {
	size_t hc = Hash(hwnd);
	HashChain& hchain = mHashTable[hc];

	HashChain::iterator it(hchain.begin()), itEnd(hchain.end());
	for(; it != itEnd; ++it) {
		VDUIProxyControl *control = *it;

		if (control->GetHandle() == hwnd)
			return control;
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

VDUIProxyListView::VDUIProxyListView()
	: mChangeNotificationLocks(0)
	, mNextTextIndex(0)
{
}

void VDUIProxyListView::Detach() {
	Clear();
	VDUIProxyControl::Detach();
}

void VDUIProxyListView::AutoSizeColumns(bool expandlast) {
	const int colCount = GetColumnCount();

	SendMessage(mhwnd, WM_SETREDRAW, FALSE, 0);

	int colCacheCount = mColumnWidthCache.size();
	while(colCacheCount < colCount) {
		SendMessage(mhwnd, LVM_SETCOLUMNWIDTH, colCacheCount, LVSCW_AUTOSIZE_USEHEADER);
		mColumnWidthCache.push_back(SendMessage(mhwnd, LVM_GETCOLUMNWIDTH, colCacheCount, 0));
		++colCacheCount;
	}

	int totalWidth = 0;
	for(int col=0; col<colCount; ++col) {
		const int hdrWidth = mColumnWidthCache[col];

		SendMessage(mhwnd, LVM_SETCOLUMNWIDTH, col, LVSCW_AUTOSIZE);
		int dataWidth = SendMessage(mhwnd, LVM_GETCOLUMNWIDTH, col, 0);

		if (dataWidth < hdrWidth)
			dataWidth = hdrWidth;

		if (expandlast) {
			RECT r;
			if (GetClientRect(mhwnd, &r)) {
				int extraWidth = r.right - totalWidth;

				if (dataWidth < extraWidth)
					dataWidth = extraWidth;
			}
		}

		SendMessage(mhwnd, LVM_SETCOLUMNWIDTH, col, dataWidth);

		totalWidth += dataWidth;
	}

	SendMessage(mhwnd, WM_SETREDRAW, TRUE, 0);
	InvalidateRect(mhwnd,0,true);
}

void VDUIProxyListView::Clear() {
	if (mhwnd)
		SendMessage(mhwnd, LVM_DELETEALLITEMS, 0, 0);
}

void VDUIProxyListView::ClearExtraColumns() {
	if (!mhwnd)
		return;

	uint32 n = GetColumnCount();
	for(uint32 i=n; i > 1; --i)
		ListView_DeleteColumn(mhwnd, i - 1);

	if (!mColumnWidthCache.empty())
		mColumnWidthCache.resize(1);
}

void VDUIProxyListView::DeleteItem(int index) {
	SendMessage(mhwnd, LVM_DELETEITEM, index, 0);
}

int VDUIProxyListView::GetColumnCount() const {
	HWND hwndHeader = (HWND)SendMessage(mhwnd, LVM_GETHEADER, 0, 0);
	if (!hwndHeader)
		return 0;

	return (int)SendMessage(hwndHeader, HDM_GETITEMCOUNT, 0, 0);
}

int VDUIProxyListView::GetItemCount() const {
	return (int)SendMessage(mhwnd, LVM_GETITEMCOUNT, 0, 0);
}

int VDUIProxyListView::GetSelectedIndex() const {
	return ListView_GetNextItem(mhwnd, -1, LVNI_SELECTED);
}

void VDUIProxyListView::SetSelectedIndex(int index) {
	int curIdx = GetSelectedIndex();

	if (curIdx == index)
		return;

	if (curIdx >= 0)
		ListView_SetItemState(mhwnd, index, 0, LVIS_SELECTED|LVIS_FOCUSED);

	if (index >= 0)
		ListView_SetItemState(mhwnd, index, LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED);
}

IVDUIListViewVirtualItem *VDUIProxyListView::GetSelectedItem() const {
	int idx = GetSelectedIndex();

	if (idx < 0)
		return NULL;

	return GetVirtualItem(idx);
}

void VDUIProxyListView::GetSelectedIndices(vdfastvector<int>& indices) const {
	int idx = -1;

	indices.clear();
	for(;;) {
		idx = ListView_GetNextItem(mhwnd, idx, LVNI_SELECTED);
		if (idx < 0)
			return;

		indices.push_back(idx);
	}
}

void VDUIProxyListView::SetFullRowSelectEnabled(bool enabled) {
	ListView_SetExtendedListViewStyleEx(mhwnd, LVS_EX_FULLROWSELECT, enabled ? LVS_EX_FULLROWSELECT : 0);
}

void VDUIProxyListView::SetGridLinesEnabled(bool enabled) {
	ListView_SetExtendedListViewStyleEx(mhwnd, LVS_EX_GRIDLINES, enabled ? LVS_EX_GRIDLINES : 0);
}

void VDUIProxyListView::SetItemCheckboxesEnabled(bool enabled) {
	ListView_SetExtendedListViewStyleEx(mhwnd, LVS_EX_CHECKBOXES, enabled ? LVS_EX_CHECKBOXES : 0);
}

void VDUIProxyListView::EnsureItemVisible(int index) {
	ListView_EnsureVisible(mhwnd, index, FALSE);
}

int VDUIProxyListView::GetVisibleTopIndex() {
	return ListView_GetTopIndex(mhwnd);
}

void VDUIProxyListView::SetVisibleTopIndex(int index) {
	int n = ListView_GetItemCount(mhwnd);
	if (n > 0) {
		ListView_EnsureVisible(mhwnd, n - 1, FALSE);
		ListView_EnsureVisible(mhwnd, index, FALSE);
	}
}

IVDUIListViewVirtualItem *VDUIProxyListView::GetSelectedVirtualItem() const {
	int index = GetSelectedIndex();
	if (index < 0)
		return NULL;

	return GetVirtualItem(index);
}

IVDUIListViewVirtualItem *VDUIProxyListView::GetVirtualItem(int index) const {
	if (index < 0)
		return NULL;

	if (VDIsWindowsNT()) {
		LVITEMW itemw={};
		itemw.mask = LVIF_PARAM;
		itemw.iItem = index;
		itemw.iSubItem = 0;
		if (SendMessage(mhwnd, LVM_GETITEMA, 0, (LPARAM)&itemw))
			return (IVDUIListViewVirtualItem *)itemw.lParam;
	} else {
		LVITEMA itema={};
		itema.mask = LVIF_PARAM;
		itema.iItem = index;
		itema.iSubItem = 0;
		if (SendMessage(mhwnd, LVM_GETITEMW, 0, (LPARAM)&itema))
			return (IVDUIListViewVirtualItem *)itema.lParam;
	}

	return NULL;
}

void VDUIProxyListView::InsertColumn(int index, const wchar_t *label, int width, bool rightAligned) {
	VDASSERT(index || !rightAligned);

	if (VDIsWindowsNT()) {
		LVCOLUMNW colw = {};

		colw.mask		= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		colw.fmt		= rightAligned ? LVCFMT_RIGHT : LVCFMT_LEFT;
		colw.cx			= width;
		colw.pszText	= (LPWSTR)label;

		SendMessageW(mhwnd, LVM_INSERTCOLUMNW, (WPARAM)index, (LPARAM)&colw);
	} else {
		LVCOLUMNA cola = {};
		VDStringA labela(VDTextWToA(label));

		cola.mask		= LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
		cola.fmt		= rightAligned ? LVCFMT_RIGHT : LVCFMT_LEFT;
		cola.cx			= width;
		cola.pszText	= (LPSTR)labela.c_str();

		SendMessageA(mhwnd, LVM_INSERTCOLUMNA, (WPARAM)index, (LPARAM)&cola);
	}
}

int VDUIProxyListView::InsertItem(int item, const wchar_t *text) {
	if (item < 0)
		item = 0x7FFFFFFF;

	if (VDIsWindowsNT()) {
		LVITEMW itemw = {};

		itemw.mask		= LVIF_TEXT;
		itemw.iItem		= item;
		itemw.pszText	= (LPWSTR)text;

		return (int)SendMessageW(mhwnd, LVM_INSERTITEMW, 0, (LPARAM)&itemw);
	} else {
		LVITEMA itema = {};
		VDStringA texta(VDTextWToA(text));

		itema.mask		= LVIF_TEXT;
		itema.iItem		= item;
		itema.pszText	= (LPSTR)texta.c_str();

		return (int)SendMessageA(mhwnd, LVM_INSERTITEMA, 0, (LPARAM)&itema);
	}
}

int VDUIProxyListView::InsertVirtualItem(int item, IVDUIListViewVirtualItem *lvvi) {
	int index;

	if (item < 0)
		item = 0x7FFFFFFF;

	++mChangeNotificationLocks;
	if (VDIsWindowsNT()) {
		LVITEMW itemw = {};

		itemw.mask		= LVIF_TEXT | LVIF_PARAM;
		itemw.iItem		= item;
		itemw.pszText	= LPSTR_TEXTCALLBACKW;
		itemw.lParam	= (LPARAM)lvvi;

		index = (int)SendMessageW(mhwnd, LVM_INSERTITEMW, 0, (LPARAM)&itemw);
	} else {
		LVITEMA itema = {};

		itema.mask		= LVIF_TEXT | LVIF_PARAM;
		itema.iItem		= item;
		itema.pszText	= LPSTR_TEXTCALLBACKA;
		itema.lParam	= (LPARAM)lvvi;

		index = (int)SendMessageA(mhwnd, LVM_INSERTITEMA, 0, (LPARAM)&itema);
	}
	--mChangeNotificationLocks;

	if (index >= 0)
		lvvi->AddRef();

	return index;
}

void VDUIProxyListView::RefreshItem(int item) {
	SendMessage(mhwnd, LVM_REDRAWITEMS, item, item);
}

void VDUIProxyListView::RefreshAllItems() {
	int n = GetItemCount();

	if (n)
		SendMessage(mhwnd, LVM_REDRAWITEMS, 0, n - 1);
}

void VDUIProxyListView::EditItemLabel(int item) {
	ListView_EditLabel(mhwnd, item);
}

void VDUIProxyListView::GetItemText(int item, VDStringW& s) const {
	if (VDIsWindowsNT()) {
		LVITEMW itemw;
		wchar_t buf[512];

		itemw.iSubItem = 0;
		itemw.cchTextMax = 511;
		itemw.pszText = buf;
		buf[0] = 0;
		SendMessageW(mhwnd, LVM_GETITEMTEXTW, item, (LPARAM)&itemw);

		s = buf;
	} else {
		LVITEMA itema;
		char buf[512];

		itema.iSubItem = 0;
		itema.cchTextMax = 511;
		itema.pszText = buf;
		buf[0] = 0;
		SendMessageW(mhwnd, LVM_GETITEMTEXTA, item, (LPARAM)&itema);

		s = VDTextAToW(buf);
	}
}

void VDUIProxyListView::SetItemText(int item, int subitem, const wchar_t *text) {
	if (VDIsWindowsNT()) {
		LVITEMW itemw = {};

		itemw.mask		= LVIF_TEXT;
		itemw.iItem		= item;
		itemw.iSubItem	= subitem;
		itemw.pszText	= (LPWSTR)text;

		SendMessageW(mhwnd, LVM_SETITEMW, 0, (LPARAM)&itemw);
	} else {
		LVITEMA itema = {};
		VDStringA texta(VDTextWToA(text));

		itema.mask		= LVIF_TEXT;
		itema.iItem		= item;
		itema.iSubItem	= subitem;
		itema.pszText	= (LPSTR)texta.c_str();

		SendMessageA(mhwnd, LVM_SETITEMA, 0, (LPARAM)&itema);
	}
}

bool VDUIProxyListView::IsItemChecked(int item) {
	return ListView_GetCheckState(mhwnd, item) != 0;
}

void VDUIProxyListView::SetItemChecked(int item, bool checked) {
	ListView_SetCheckState(mhwnd, item, checked);
}

void VDUIProxyListView::SetItemCheckedVisible(int item, bool checked) {
	if (!mhwnd)
		return;

	ListView_SetItemState(mhwnd, item, INDEXTOSTATEIMAGEMASK(0), LVIS_STATEIMAGEMASK);
}

void VDUIProxyListView::SetItemImage(int item, uint32 imageIndex) {
	if (VDIsWindowsNT()) {
		LVITEMW itemw = {};

		itemw.mask		= LVIF_IMAGE;
		itemw.iItem		= item;
		itemw.iSubItem	= 0;
		itemw.iImage	= imageIndex;

		SendMessageW(mhwnd, LVM_SETITEMW, 0, (LPARAM)&itemw);
	} else {
		LVITEMA itema = {};

		itema.mask		= LVIF_IMAGE;
		itema.iItem		= item;
		itema.iSubItem	= 0;
		itema.iImage	= imageIndex;

		SendMessageA(mhwnd, LVM_SETITEMA, 0, (LPARAM)&itema);
	}
}

bool VDUIProxyListView::GetItemScreenRect(int item, vdrect32& r) const {
	r.set(0, 0, 0, 0);

	if (!mhwnd)
		return false;

	RECT nr = {LVIR_BOUNDS};
	if (!SendMessage(mhwnd, LVM_GETITEMRECT, (WPARAM)item, (LPARAM)&nr))
		return false;

	MapWindowPoints(mhwnd, NULL, (LPPOINT)&nr, 2);

	r.set(nr.left, nr.top, nr.right, nr.bottom);
	return true;
}

void VDUIProxyListView::Sort(IVDUIListViewVirtualComparer& comparer) {
	ListView_SortItems(mhwnd, SortAdapter, (LPARAM)&comparer);
}

int VDZCALLBACK VDUIProxyListView::SortAdapter(LPARAM x, LPARAM y, LPARAM cookie) {
	return ((IVDUIListViewVirtualComparer *)cookie)->Compare((IVDUIListViewVirtualItem *)x, (IVDUIListViewVirtualItem *)y);
}

VDZLRESULT VDUIProxyListView::On_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam) {
	const NMHDR *hdr = (const NMHDR *)lParam;

	switch(hdr->code) {
		case LVN_GETDISPINFOA:
			{
				NMLVDISPINFOA *dispa = (NMLVDISPINFOA *)hdr;
				IVDUIListViewVirtualItem *lvvi = (IVDUIListViewVirtualItem *)dispa->item.lParam;

				if (dispa->item.mask & LVIF_TEXT) {
					mTextW[0].clear();
					if (lvvi)
						lvvi->GetText(dispa->item.iSubItem, mTextW[0]);
					mTextA[mNextTextIndex] = VDTextWToA(mTextW[0]);
					dispa->item.pszText = (LPSTR)mTextA[mNextTextIndex].c_str();

					if (++mNextTextIndex >= 3)
						mNextTextIndex = 0;
				}
			}
			break;

		case LVN_GETDISPINFOW:
			{
				NMLVDISPINFOW *dispw = (NMLVDISPINFOW *)hdr;
				IVDUIListViewVirtualItem *lvvi = (IVDUIListViewVirtualItem *)dispw->item.lParam;

				if (dispw->item.mask & LVIF_TEXT) {
					mTextW[mNextTextIndex].clear();
					if (lvvi)
						lvvi->GetText(dispw->item.iSubItem, mTextW[mNextTextIndex]);
					dispw->item.pszText = (LPWSTR)mTextW[mNextTextIndex].c_str();

					if (++mNextTextIndex >= 3)
						mNextTextIndex = 0;
				}
			}
			break;

		case LVN_DELETEITEM:
			{
				const NMLISTVIEW *nmlv = (const NMLISTVIEW *)hdr;
				IVDUIListViewVirtualItem *lvvi = (IVDUIListViewVirtualItem *)nmlv->lParam;

				if (lvvi)
					lvvi->Release();
			}
			break;

		case LVN_COLUMNCLICK:
			{
				const NMLISTVIEW *nmlv = (const NMLISTVIEW *)hdr;

				mEventColumnClicked.Raise(this, nmlv->iSubItem);
			}
			break;

		case LVN_ITEMCHANGING:
			if (!mChangeNotificationLocks) {
				const NMLISTVIEW *nmlv = (const NMLISTVIEW *)hdr;

				if (nmlv->uChanged & LVIF_STATE) {
					uint32 deltaState = nmlv->uOldState ^ nmlv->uNewState;

					if (deltaState & LVIS_STATEIMAGEMASK) {
						VDASSERT(nmlv->iItem >= 0);

						CheckedChangingEvent event;
						event.mIndex = nmlv->iItem;
						event.mbNewVisible = (nmlv->uNewState & LVIS_STATEIMAGEMASK) != 0;
						event.mbNewChecked = (nmlv->uNewState & 0x2000) != 0;
						event.mbAllowChange = true;
						mEventItemCheckedChanging.Raise(this, &event);

						if (!event.mbAllowChange)
							return TRUE;
					}
				}
			}
			break;

		case LVN_ITEMCHANGED:
			if (!mChangeNotificationLocks) {
				const NMLISTVIEW *nmlv = (const NMLISTVIEW *)hdr;

				if (nmlv->uChanged & LVIF_STATE) {
					uint32 deltaState = nmlv->uOldState ^ nmlv->uNewState;

					if (deltaState & LVIS_SELECTED) {
						int selIndex = ListView_GetNextItem(mhwnd, -1, LVNI_ALL | LVNI_SELECTED);

						mEventItemSelectionChanged.Raise(this, selIndex);
					}

					if (deltaState & LVIS_STATEIMAGEMASK) {
						VDASSERT(nmlv->iItem >= 0);
						mEventItemCheckedChanged.Raise(this, nmlv->iItem);
					}
				}
			}
			break;

		case LVN_ENDLABELEDITA:
			{
				const NMLVDISPINFOA *di = (const NMLVDISPINFOA *)hdr;
				if (di->item.pszText) {
					const VDStringW label(VDTextAToW(di->item.pszText));
					LabelChangedEvent event = {
						true,
						di->item.iItem,
						label.c_str()
					};

					mEventItemLabelEdited.Raise(this, &event);

					if (!event.mbAllowEdit)
						return FALSE;
				}
			}
			return TRUE;

		case LVN_ENDLABELEDITW:
			{
				const NMLVDISPINFOW *di = (const NMLVDISPINFOW *)hdr;

				if (di->item.pszText) {
					LabelChangedEvent event = {
						true,
						di->item.iItem,
						di->item.pszText
					};

					mEventItemLabelEdited.Raise(this, &event);

					if (!event.mbAllowEdit)
						return FALSE;
				}
			}
			return TRUE;

		case LVN_BEGINDRAG:
			{
				const NMLISTVIEW *nmlv = (const NMLISTVIEW *)hdr;

				mEventItemBeginDrag.Raise(this, nmlv->iItem);
			}
			return 0;

		case LVN_BEGINRDRAG:
			{
				const NMLISTVIEW *nmlv = (const NMLISTVIEW *)hdr;

				mEventItemBeginRDrag.Raise(this, nmlv->iItem);
			}
			return 0;

		case NM_RCLICK:
			{
				const NMITEMACTIVATE *nmia = (const NMITEMACTIVATE *)hdr;

				if (nmia->iItem >= 0) {
					ContextMenuEvent event;
					event.mIndex = nmia->iItem;

					POINT pt = nmia->ptAction;
					ClientToScreen(mhwnd, &pt);
					event.mX = pt.x;
					event.mY = pt.y;
					mEventItemContextMenu.Raise(this, event);
				}
			}
			return 0;

		case NM_DBLCLK:
			{
				int selIndex = ListView_GetNextItem(mhwnd, -1, LVNI_ALL | LVNI_SELECTED);

				mEventItemDoubleClicked.Raise(this, selIndex);
			}
			return 0;
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////

VDUIProxyHotKeyControl::VDUIProxyHotKeyControl() {
}

VDUIProxyHotKeyControl::~VDUIProxyHotKeyControl() {
}

bool VDUIProxyHotKeyControl::GetAccelerator(VDUIAccelerator& accel) const {
	if (!mhwnd)
		return false;

	uint32 v = SendMessage(mhwnd, HKM_GETHOTKEY, 0, 0);

	accel.mVirtKey = (uint8)v;
	accel.mModifiers = 0;
	
	const uint8 mods = (uint8)(v >> 8);
	if (mods & HOTKEYF_SHIFT)
		accel.mModifiers |= VDUIAccelerator::kModShift;

	if (mods & HOTKEYF_CONTROL)
		accel.mModifiers |= VDUIAccelerator::kModCtrl;

	if (mods & HOTKEYF_ALT)
		accel.mModifiers |= VDUIAccelerator::kModAlt;

	if (mods & HOTKEYF_EXT)
		accel.mModifiers |= VDUIAccelerator::kModExtended;

	return true;
}

void VDUIProxyHotKeyControl::SetAccelerator(const VDUIAccelerator& accel) {
	uint32 mods = 0;

	if (accel.mModifiers & VDUIAccelerator::kModShift)
		mods |= HOTKEYF_SHIFT;

	if (accel.mModifiers & VDUIAccelerator::kModCtrl)
		mods |= HOTKEYF_CONTROL;

	if (accel.mModifiers & VDUIAccelerator::kModAlt)
		mods |= HOTKEYF_ALT;

	if (accel.mModifiers & VDUIAccelerator::kModExtended)
		mods |= HOTKEYF_EXT;

	SendMessage(mhwnd, HKM_SETHOTKEY, accel.mVirtKey + (mods << 8), 0);
}

VDZLRESULT VDUIProxyHotKeyControl::On_WM_COMMAND(VDZWPARAM wParam, VDZLPARAM lParam) {
	if (HIWORD(wParam) == EN_CHANGE) {
		VDUIAccelerator accel;
		GetAccelerator(accel);
		mEventHotKeyChanged.Raise(this, accel);
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////

VDUIProxyTabControl::VDUIProxyTabControl() {
}

VDUIProxyTabControl::~VDUIProxyTabControl() {
}

void VDUIProxyTabControl::AddItem(const wchar_t *s) {
	if (!mhwnd)
		return;

	int n = TabCtrl_GetItemCount(mhwnd);
	if (VDIsWindowsNT()) {
		TCITEMW tciw = { TCIF_TEXT };

		tciw.pszText = (LPWSTR)s;

		SendMessageW(mhwnd, TCM_INSERTITEMW, n, (LPARAM)&tciw);
	} else {
		TCITEMA tcia = { TCIF_TEXT };
		VDStringA sa(VDTextWToA(s));

		tcia.pszText = (LPSTR)sa.c_str();

		SendMessageA(mhwnd, TCM_INSERTITEMA, n, (LPARAM)&tcia);
	}
}

void VDUIProxyTabControl::DeleteItem(int index) {
	if (mhwnd)
		SendMessage(mhwnd, TCM_DELETEITEM, index, 0);
}

vdsize32 VDUIProxyTabControl::GetControlSizeForContent(const vdsize32& sz) const {
	if (!mhwnd)
		return vdsize32(0, 0);

	RECT r = { 0, 0, sz.w, sz.h };
	TabCtrl_AdjustRect(mhwnd, TRUE, &r);

	return vdsize32(r.right - r.left, r.bottom - r.top);
}

vdrect32 VDUIProxyTabControl::GetContentArea() const {
	if (!mhwnd)
		return vdrect32(0, 0, 0, 0);

	RECT r = {0};
	GetWindowRect(mhwnd, &r);

	HWND hwndParent = GetParent(mhwnd);
	if (hwndParent)
		MapWindowPoints(NULL, hwndParent, (LPPOINT)&r, 2);

	TabCtrl_AdjustRect(mhwnd, FALSE, &r);

	return vdrect32(r.left, r.top, r.right, r.bottom);
}

int VDUIProxyTabControl::GetSelection() const {
	if (!mhwnd)
		return -1;

	return (int)SendMessage(mhwnd, TCM_GETCURSEL, 0, 0);
}

void VDUIProxyTabControl::SetSelection(int index) {
	if (mhwnd)
		SendMessage(mhwnd, TCM_SETCURSEL, index, 0);
}

VDZLRESULT VDUIProxyTabControl::On_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam) {
	if (((const NMHDR *)lParam)->code == TCN_SELCHANGE) {
		mSelectionChanged.Raise(this, GetSelection());
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////

VDUIProxyListBoxControl::VDUIProxyListBoxControl() {
}

VDUIProxyListBoxControl::~VDUIProxyListBoxControl() {
}

void VDUIProxyListBoxControl::Clear() {
	if (!mhwnd)
		return;

	SendMessage(mhwnd, LB_RESETCONTENT, 0, 0);
}

int VDUIProxyListBoxControl::AddItem(const wchar_t *s, uintptr_t cookie) {
	if (!mhwnd)
		return -1;

	int idx;
	if (VDIsWindowsNT())
		idx = SendMessageW(mhwnd, LB_ADDSTRING, 0, (LPARAM)s);
	else
		idx = SendMessageA(mhwnd, LB_ADDSTRING, 0, (LPARAM)VDTextWToA(s).c_str());

	if (idx >= 0)
		SendMessage(mhwnd, LB_SETITEMDATA, idx, (LPARAM)cookie);

	return idx;
}

uintptr VDUIProxyListBoxControl::GetItemData(int index) const {
	if (index < 0)
		return 0;

	return SendMessage(mhwnd, LB_GETITEMDATA, index, 0);
}

int VDUIProxyListBoxControl::GetSelection() const {
	if (!mhwnd)
		return -1;

	return (int)SendMessage(mhwnd, LB_GETCURSEL, 0, 0);
}

void VDUIProxyListBoxControl::SetSelection(int index) {
	if (mhwnd)
		SendMessage(mhwnd, LB_SETCURSEL, index, 0);
}

void VDUIProxyListBoxControl::MakeSelectionVisible() {
	if (!mhwnd)
		return;

	int idx = SendMessage(mhwnd, LB_GETCURSEL, 0, 0);

	if (idx >= 0)
		SendMessage(mhwnd, LB_SETCURSEL, idx, 0);
}

void VDUIProxyListBoxControl::SetTabStops(const int *units, uint32 n) {
	if (!mhwnd)
		return;

	vdfastvector<INT> v(n);

	for(uint32 i=0; i<n; ++i)
		v[i] = units[i];

	SendMessage(mhwnd, LB_SETTABSTOPS, n, (LPARAM)v.data());
}

VDZLRESULT VDUIProxyListBoxControl::On_WM_COMMAND(VDZWPARAM wParam, VDZLPARAM lParam) {
	if (HIWORD(wParam) == LBN_SELCHANGE) {
		mSelectionChanged.Raise(this, GetSelection());
	} else if (HIWORD(wParam) == LBN_DBLCLK) {
		int sel = GetSelection();

		if (sel >= 0)
			mEventItemDoubleClicked.Raise(this, GetSelection());
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////

VDUIProxyComboBoxControl::VDUIProxyComboBoxControl() {
}

VDUIProxyComboBoxControl::~VDUIProxyComboBoxControl() {
}

void VDUIProxyComboBoxControl::AddItem(const wchar_t *s) {
	if (!mhwnd)
		return;

	if (VDIsWindowsNT())
		SendMessageW(mhwnd, CB_ADDSTRING, 0, (LPARAM)s);
	else
		SendMessageA(mhwnd, CB_ADDSTRING, 0, (LPARAM)VDTextWToA(s).c_str());
}

int VDUIProxyComboBoxControl::GetSelection() const {
	if (!mhwnd)
		return -1;

	return (int)SendMessage(mhwnd, CB_GETCURSEL, 0, 0);
}

void VDUIProxyComboBoxControl::SetSelection(int index) {
	if (mhwnd)
		SendMessage(mhwnd, CB_SETCURSEL, index, 0);
}

VDZLRESULT VDUIProxyComboBoxControl::On_WM_COMMAND(VDZWPARAM wParam, VDZLPARAM lParam) {
	if (HIWORD(wParam) == CBN_SELCHANGE) {
		mSelectionChanged.Raise(this, GetSelection());
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////

const VDUIProxyTreeViewControl::NodeRef VDUIProxyTreeViewControl::kNodeRoot = (NodeRef)TVI_ROOT;
const VDUIProxyTreeViewControl::NodeRef VDUIProxyTreeViewControl::kNodeFirst = (NodeRef)TVI_FIRST;
const VDUIProxyTreeViewControl::NodeRef VDUIProxyTreeViewControl::kNodeLast = (NodeRef)TVI_LAST;

VDUIProxyTreeViewControl::VDUIProxyTreeViewControl()
	: mNextTextIndex(0)
{
}

VDUIProxyTreeViewControl::~VDUIProxyTreeViewControl() {
}

IVDUITreeViewVirtualItem *VDUIProxyTreeViewControl::GetSelectedVirtualItem() const {
	if (!mhwnd)
		return NULL;

	HTREEITEM hti = TreeView_GetSelection(mhwnd);

	if (!hti)
		return NULL;

	if (VDIsWindowsNT()) {
		TVITEMW itemw = {0};

		itemw.mask = LVIF_PARAM;
		itemw.hItem = hti;

		SendMessageW(mhwnd, TVM_GETITEMW, 0, (LPARAM)&itemw);
		return (IVDUITreeViewVirtualItem *)itemw.lParam;
	} else {
		TVITEMA itema = {0};

		itema.mask = LVIF_PARAM;
		itema.hItem = hti;

		SendMessageA(mhwnd, TVM_GETITEMA, 0, (LPARAM)&itema);
		return (IVDUITreeViewVirtualItem *)itema.lParam;
	}
}

void VDUIProxyTreeViewControl::Clear() {
	if (mhwnd) {
		TreeView_DeleteAllItems(mhwnd);
	}
}

void VDUIProxyTreeViewControl::DeleteItem(NodeRef ref) {
	if (mhwnd) {
		TreeView_DeleteItem(mhwnd, (HTREEITEM)ref);
	}
}

VDUIProxyTreeViewControl::NodeRef VDUIProxyTreeViewControl::AddItem(NodeRef parent, NodeRef insertAfter, const wchar_t *label) {
	if (!mhwnd)
		return NULL;

	if (VDIsWindowsNT()) {
		TVINSERTSTRUCTW isw = { 0 };

		isw.hParent = (HTREEITEM)parent;
		isw.hInsertAfter = (HTREEITEM)insertAfter;
		isw.item.mask = TVIF_TEXT | TVIF_PARAM;
		isw.item.pszText = (LPWSTR)label;
		isw.item.lParam = NULL;

		return (NodeRef)SendMessageW(mhwnd, TVM_INSERTITEMW, 0, (LPARAM)&isw);
	} else {
		TVINSERTSTRUCTA isa;
		VDStringA sa(VDTextWToA(label));

		isa.hParent = (HTREEITEM)parent;
		isa.hInsertAfter = (HTREEITEM)insertAfter;
		isa.item.mask = TVIF_TEXT | TVIF_PARAM;
		isa.item.pszText = (LPSTR)sa.c_str();
		isa.item.lParam = NULL;

		return (NodeRef)SendMessageA(mhwnd, TVM_INSERTITEMA, 0, (LPARAM)&isa);
	}
}

VDUIProxyTreeViewControl::NodeRef VDUIProxyTreeViewControl::AddVirtualItem(NodeRef parent, NodeRef insertAfter, IVDUITreeViewVirtualItem *item) {
	if (!mhwnd)
		return NULL;

	HTREEITEM hti;

	if (VDIsWindowsNT()) {
		TVINSERTSTRUCTW isw = { 0 };

		isw.hParent = (HTREEITEM)parent;
		isw.hInsertAfter = (HTREEITEM)insertAfter;
		isw.item.mask = TVIF_PARAM | TVIF_TEXT;
		isw.item.lParam = (LPARAM)item;
		isw.item.pszText = LPSTR_TEXTCALLBACKW;

		hti = (HTREEITEM)SendMessageW(mhwnd, TVM_INSERTITEMW, 0, (LPARAM)&isw);
	} else {
		TVINSERTSTRUCTA isa;

		isa.hParent = (HTREEITEM)parent;
		isa.hInsertAfter = (HTREEITEM)insertAfter;
		isa.item.mask = TVIF_PARAM | TVIF_TEXT;
		isa.item.lParam = (LPARAM)item;
		isa.item.pszText = LPSTR_TEXTCALLBACKA;

		hti = (HTREEITEM)SendMessageA(mhwnd, TVM_INSERTITEMA, 0, (LPARAM)&isa);
	}

	if (hti) {
		if (parent != kNodeRoot) {
			TreeView_Expand(mhwnd, (HTREEITEM)parent, TVE_EXPAND);
		}

		item->AddRef();
	}

	return (NodeRef)hti;
}

void VDUIProxyTreeViewControl::MakeNodeVisible(NodeRef node) {
	if (mhwnd) {
		TreeView_EnsureVisible(mhwnd, (HTREEITEM)node);
	}
}

void VDUIProxyTreeViewControl::SelectNode(NodeRef node) {
	if (mhwnd) {
		TreeView_SelectItem(mhwnd, (HTREEITEM)node);
	}
}

void VDUIProxyTreeViewControl::RefreshNode(NodeRef node) {
	if (mhwnd) {
		if (VDIsWindowsNT()) {
			TVITEMW itemw = {0};

			itemw.mask = LVIF_PARAM;
			itemw.hItem = (HTREEITEM)node;

			SendMessageW(mhwnd, TVM_GETITEMW, 0, (LPARAM)&itemw);

			if (itemw.lParam)
				SendMessageW(mhwnd, TVM_SETITEMW, 0, (LPARAM)&itemw);
		} else {
			TVITEMA itema = {0};

			itema.mask = LVIF_PARAM;
			itema.hItem = (HTREEITEM)node;

			SendMessageA(mhwnd, TVM_GETITEMA, 0, (LPARAM)&itema);

			if (itema.lParam)
				SendMessageA(mhwnd, TVM_SETITEMA, 0, (LPARAM)&itema);
		}
	}
}

VDZLRESULT VDUIProxyTreeViewControl::On_WM_NOTIFY(VDZWPARAM wParam, VDZLPARAM lParam) {
	const NMHDR *hdr = (const NMHDR *)lParam;

	switch(hdr->code) {
		case TVN_GETDISPINFOA:
			{
				NMTVDISPINFOA *dispa = (NMTVDISPINFOA *)hdr;
				IVDUITreeViewVirtualItem *lvvi = (IVDUITreeViewVirtualItem *)dispa->item.lParam;

				mTextW[0].clear();
				lvvi->GetText(mTextW[0]);
				mTextA[mNextTextIndex] = VDTextWToA(mTextW[0]);
				dispa->item.pszText = (LPSTR)mTextA[mNextTextIndex].c_str();

				if (++mNextTextIndex >= 3)
					mNextTextIndex = 0;
			}
			break;

		case TVN_GETDISPINFOW:
			{
				NMTVDISPINFOW *dispw = (NMTVDISPINFOW *)hdr;
				IVDUITreeViewVirtualItem *lvvi = (IVDUITreeViewVirtualItem *)dispw->item.lParam;

				mTextW[mNextTextIndex].clear();
				lvvi->GetText(mTextW[mNextTextIndex]);
				dispw->item.pszText = (LPWSTR)mTextW[mNextTextIndex].c_str();

				if (++mNextTextIndex >= 3)
					mNextTextIndex = 0;
			}
			break;

		case TVN_DELETEITEMA:
			{
				const NMTREEVIEWA *nmtv = (const NMTREEVIEWA *)hdr;
				IVDUITreeViewVirtualItem *lvvi = (IVDUITreeViewVirtualItem *)nmtv->itemOld.lParam;

				if (lvvi)
					lvvi->Release();
			}
			break;

		case TVN_DELETEITEMW:
			{
				const NMTREEVIEWW *nmtv = (const NMTREEVIEWW *)hdr;
				IVDUITreeViewVirtualItem *lvvi = (IVDUITreeViewVirtualItem *)nmtv->itemOld.lParam;

				if (lvvi)
					lvvi->Release();
			}
			break;

		case TVN_SELCHANGEDA:
		case TVN_SELCHANGEDW:
			mEventItemSelectionChanged.Raise(this, 0);
			break;

		case NM_DBLCLK:
			{
				bool handled = false;
				mEventItemDoubleClicked.Raise(this, &handled);
				return handled;
			}

	}

	return 0;
}
