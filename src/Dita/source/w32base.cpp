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
#include <stdafx.h>

#include <vd2/system/w32assist.h>
#include <vd2/Dita/w32base.h>
#include <set>

VDUIBaseWindowW32::VDUIBaseWindowW32()
	: mNextNativeID(1337)
	, mpCB(NULL)
	, mbCBAutoDelete(false)
	, mbAutoDestroy(false)
	, mbTick(false)
	, mpModal(NULL)
{
}

VDUIBaseWindowW32::~VDUIBaseWindowW32() {
	if (mpCB && mbCBAutoDelete)
		delete mpCB;
}

void *VDUIBaseWindowW32::AsInterface(uint32 id) {
	switch(id) {
		case IVDUIBase::kTypeID: return static_cast<IVDUIBase *>(this); break;
	}

	return VDUICustomControlW32::AsInterface(id);
}

void VDUIBaseWindowW32::Shutdown() {
	DispatchEvent(this, mID, IVDUICallback::kEventDestroy, 0);
	SetCallback(NULL, false);
	VDUICustomControlW32::Shutdown();
}

bool VDUIBaseWindowW32::Create(IVDUIParameters *pParams) {
	bool is_child = pParams->GetB(nsVDUI::kUIParam_Child, false);

	mPadding = pParams->GetI(nsVDUI::kUIParam_Spacing, is_child ? 0 : 7);

	DWORD flags = 0;

	if (pParams->GetB(nsVDUI::kUIParam_SystemMenus, false))
		flags |= WS_OVERLAPPEDWINDOW;

	return VDUICustomControlW32::Create(pParams, !is_child, flags);
}

void VDUIBaseWindowW32::Destroy() {
	if (VDINLINEASSERTFALSE(mpModal)) {
		EndModal(-1);
		return;
	}

	VDUICustomControlW32::Destroy();

	if (mbAutoDestroy) {
		mbAutoDestroy = false;
		if (mpParent)
			mpParent->RemoveChild(this);
		Release();
	}
}

void VDUIBaseWindowW32::SetAutoDestroy(bool enable) {
	if (enable)
		AddRef();
	if (mbAutoDestroy)
		Release();
	mbAutoDestroy = enable;
}

void VDUIBaseWindowW32::SetTickEnable(bool enable) {
	mbTick = enable;
}

vduirect VDUIBaseWindowW32::MapUnitsToPixels(vduirect r) {
	RECT rwin = {r.left, r.top, r.right, r.bottom};

	MapDialogRect(mhwnd, &rwin);

	return vduirect(rwin.left, rwin.top, rwin.right, rwin.bottom);
}

vduisize VDUIBaseWindowW32::MapUnitsToPixels(vduisize s) {
	RECT rwin = {0,0,s.w,s.h};

	MapDialogRect(mhwnd, &rwin);

	return vduisize(rwin.right, rwin.bottom);
}

void VDUIBaseWindowW32::AddControl(IVDUIWindow *pWin) {
	const uint32 id = pWin->GetID();
	std::pair<tControls::iterator, tControls::iterator> result(mControls.equal_range(id));

	for(; result.first != result.second; ++result.first)
		if ((*result.first).second == pWin)
			return;

	mControls.insert(result.first, tControls::value_type(id, pWin));
}

void VDUIBaseWindowW32::RemoveControl(IVDUIWindow *pWin) {
	const uint32 id = pWin->GetID();
	std::pair<tControls::iterator, tControls::iterator> result(mControls.equal_range(id));

	while(result.first != result.second)
		if ((*result.first).second == pWin)
			mControls.erase(result.first++);
		else
			++result.first;
}

IVDUIWindow *VDUIBaseWindowW32::GetControl(uint32 id) {
	tControls::iterator it(mControls.find(id));

	if (it != mControls.end())
		return (*it).second;

	return NULL;
}

uint32 VDUIBaseWindowW32::GetNextNativeID() {
	return mNextNativeID++;
}

void VDUIBaseWindowW32::SetCallback(IVDUICallback *pCB, bool autoDelete) {
	if (mpCB) {
		DispatchEvent(this, GetID(), IVDUICallback::kEventDetach, 0);
		if (mbCBAutoDelete)
			delete mpCB;
	}
	mpCB = pCB;
	mbCBAutoDelete = autoDelete;
	DispatchEvent(this, GetID(), IVDUICallback::kEventAttach, 0);
}

void VDUIBaseWindowW32::FinalizeDialog() {
	Relayout();

	if (mhwnd && GetFocus() == mhwnd) {
		// must init focus to a control or else shortcuts don't work at first
		HWND hwndFirstFocus = GetNextDlgTabItem(mhwnd, NULL, FALSE);
		if (hwndFirstFocus)
			::SetFocus(hwndFirstFocus);
	}
}

////////////////////////////////////////////////////////////////////////////

struct VDUIBaseWindowW32::ModalData {
	int		mReturnValue;
	bool	mbOwnerWasEnabled;
	HWND	mhwndOwner;
};

int VDUIBaseWindowW32::DoModal() {
	if (VDINLINEASSERTFALSE(mpModal))
		return -1;

	ModalData data;

	mpModal = &data;

	HWND hwndParent = GetParentW32();		// Cannot use GetParent() as it isn't correct until after messages have been dispatched and the window hops to its real parent.

	data.mbOwnerWasEnabled = false;
	data.mhwndOwner = hwndParent;

	if (data.mhwndOwner) {
		data.mbOwnerWasEnabled = !(GetWindowLong(data.mhwndOwner, GWL_STYLE) & WS_DISABLED);

		if (data.mbOwnerWasEnabled)
			EnableWindow(data.mhwndOwner, FALSE);
	}

	MSG msg;
	while(mpModal) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				PostQuitMessage(msg.wParam);
				break;
			}

			if (IsDialogMessage(mhwnd, &msg))
				continue;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}

		if (!mpModal)
			break;

		if (mbTick) {
			if (mpCB && mpCB->HandleUIEvent(this, NULL, 0, IVDUICallback::kEventTick, 0))
				continue;
		}

		WaitMessage();
	}

	mpModal = NULL;

	Shutdown();

	return data.mReturnValue;
}

void VDUIBaseWindowW32::EndModal(int value) {
	if (VDINLINEASSERTFALSE(!mpModal))
		return;

	if (mpModal->mhwndOwner && mpModal->mbOwnerWasEnabled)
		EnableWindow(mpModal->mhwndOwner, TRUE);

	if (mhwnd) {
		// Set the popup flag on the window so it has a valid parent. This is needed so
		// that on destruction the window manager reactivates the owner.
		SetWindowLong(mhwnd, GWL_STYLE, GetWindowLong(mhwnd, GWL_STYLE) | WS_POPUP);
	}

	mpModal->mReturnValue = value;
	mpModal = NULL;
}

void VDUIBaseWindowW32::Link(uint32 id, nsVDUI::LinkTarget target, const uint8 *src, size_t len) {
	int expressions = *src++;

	while(expressions-- > 0) {
		tLinkList::iterator it(mLinkList.insert(tLinkList::value_type(id, LinkEntry())));
		LinkEntry& linkEnt = (*it).second;

		int refs = *src++;
		linkEnt.mLinkSources.reserve(refs);

		while(refs-->0) {
			const uint32 id = src[0] + ((uint32)src[1]<<8) + ((uint32)src[2]<<16) + ((uint32)src[3]<<24);
			src += 4;

			linkEnt.mLinkSources.push_back(id);
		}

		const int bclen = src[0] + ((uint32)src[1]<<8);
		src += 2;
		linkEnt.mByteCode.assign(src, src+bclen+1);
		src += bclen+1;
		linkEnt.mTarget = target;
	}

	mbLinkUpdateMapDirty = true;
}

void VDUIBaseWindowW32::ProcessActivation(IVDUIWindow *pWin, uint32 id) {
	ExecuteLinks(id);
}

void VDUIBaseWindowW32::ProcessValueChange(IVDUIWindow *pWin, uint32 id) {
	ExecuteLinks(id);
}

bool VDUIBaseWindowW32::DispatchEvent(IVDUIWindow *pWin, uint32 id, IVDUICallback::eEventType ev, int item) {
	if (mpCB)
		return mpCB->HandleUIEvent(this, pWin, id, ev, item);

	return false;
}

namespace {
	BOOL CALLBACK SetApplicationIconOnDialog(HMODULE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam) {
		HWND hdlg = (HWND)lParam;
		HANDLE hLargeIcon = LoadImage((HINSTANCE)hModule, lpszName, IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED);
		if (hLargeIcon)
			SendMessage(hdlg, WM_SETICON, ICON_BIG, (LPARAM)hLargeIcon);

		HANDLE hSmallIcon = LoadImage((HINSTANCE)hModule, lpszName, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
		if (hSmallIcon)
			SendMessage(hdlg, WM_SETICON, ICON_SMALL, (LPARAM)hSmallIcon);

		return FALSE;
	}
}

LRESULT VDUIBaseWindowW32::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_INITDIALOG:
			{
				RECT r;
				GetWindowRect(mhwnd, &r);
				MapWindowPoints(mhwnd, ::GetParent(mhwnd), (LPPOINT)&r, 2);
				mArea.left = r.left;
				mArea.top = r.top;
				mArea.right = r.right;
				mArea.bottom = r.bottom;

				DispatchEvent(this, mID, IVDUICallback::kEventCreate, 0);

				DWORD dwStyle = GetWindowLong(mhwnd, GWL_STYLE);

				if (dwStyle & WS_THICKFRAME) {
					EnumResourceNames(VDGetLocalModuleHandleW32(), RT_GROUP_ICON, SetApplicationIconOnDialog, (LONG_PTR)mhwnd);
				}

				if (!(dwStyle & DS_CONTROL))
					Relayout();

				ExecuteAllLinks();
			}
			return FALSE;
   
		case WM_GETMINMAXINFO:
			{
				MINMAXINFO *pmmi = (MINMAXINFO *)lParam;
   
				pmmi->ptMinTrackSize.x = mLayoutSpecs.minsize.w;
				pmmi->ptMinTrackSize.y = mLayoutSpecs.minsize.h;
   
				if ((mAlignX & nsVDUI::kAlignTypeMask) != nsVDUI::kFill)
					pmmi->ptMaxTrackSize.x = mLayoutSpecs.minsize.w;
   
				if ((mAlignY & nsVDUI::kAlignTypeMask) != nsVDUI::kFill)
					pmmi->ptMaxTrackSize.y = mLayoutSpecs.minsize.h;
			}
			return 0;

		case WM_SIZE:
			if (!(GetWindowLong(mhwnd, GWL_STYLE) & WS_CHILD)) {
				vduirect r(GetClientArea());

				if (r.size() != GetArea().size()) {
					r.left   += mInsets.left;
					r.top    += mInsets.top;
					r.right  -= mInsets.right;
					r.bottom -= mInsets.bottom;

					tChildren::iterator it(mChildren.begin()), itEnd(mChildren.end());

					for(; it!=itEnd; ++it) {
						IVDUIWindow *pWin = *it;

						pWin->PostLayout(r);
					}
				}
			}
			return 0;

		case WM_CLOSE:
			if (DispatchEvent(this, mID, IVDUICallback::kEventClose, 0))
				return 0;
			goto handle_cancel;

		case WM_NOTIFYFORMAT:
			return VDIsWindowsNT() ? NFR_UNICODE : NFR_ANSI;

		case WM_COMMAND:
			// special handling for commands that have have keyboard equivalents in
			// the dialog manager; we never assign these native IDs so it's safe to
			// shortcut them
			if (LOWORD(wParam) == IDOK) {
				DispatchEvent(this, 10, IVDUICallback::kEventSelect, 0);
				return 0;
			} else if (LOWORD(wParam) == IDCANCEL) {
handle_cancel:
				if (!DispatchEvent(this, 11, IVDUICallback::kEventSelect, 0)) {
					if (mpModal)
						EndModal(-1);
					else
						Shutdown();
				}
				return 0;
			} else if (!lParam) {
				// dispatch menu/accelerator commands
				DispatchEvent(this, LOWORD(wParam), IVDUICallback::kEventSelect, 0);
				return 0;
			}
			break;
	}

	return VDUICustomControlW32::WndProc(msg, wParam, lParam);
}

void VDUIBaseWindowW32::Relayout() {
	VDUILayoutSpecs constraints;

	constraints.minsize.w = GetSystemMetrics(SM_CXMAXIMIZED);
	constraints.minsize.h = GetSystemMetrics(SM_CYMAXIMIZED);

	PreLayout(constraints);

	vduirect r(GetArea());

	if ((mAlignX & nsVDUI::kAlignTypeMask) != nsVDUI::kFill) {
		if (r.width() > mLayoutSpecs.minsize.w)
			r.right = r.left + mLayoutSpecs.minsize.w;
	}

	if ((mAlignY & nsVDUI::kAlignTypeMask) != nsVDUI::kFill) {
		if (r.height() > mLayoutSpecs.minsize.h)
			r.bottom = r.top + mLayoutSpecs.minsize.h;
	}

	PostLayout(r);
}

void VDUIBaseWindowW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	VDUILayoutSpecs rcConstraints(parentConstraints);
	RECT rcBorders = {0,0,0,0};
	RECT rcPad = {mPadding,mPadding,mPadding,mPadding};
	bool bModeless = (0 != (GetWindowLong(mhwnd, GWL_STYLE) & DS_CONTROL));

	// If the dialog is modal, compute the insets for the dialog.

	if (!bModeless) {
		MapDialogRect(mhwnd, &rcPad);

		mInsets = rcPad;

		AdjustWindowRectEx(&rcBorders, GetWindowLong(mhwnd, GWL_STYLE), GetMenu(mhwnd) != NULL, GetWindowLong(mhwnd, GWL_STYLE));

		// enlarge borders by pads
		rcBorders.left		= rcBorders.left   - rcPad.left;
		rcBorders.top		= rcBorders.top    - rcPad.top;
		rcBorders.right		= rcBorders.right  + rcPad.right;
		rcBorders.bottom	= rcBorders.bottom + rcPad.bottom;
	} else
		mInsets = rcBorders;

	// Shrink constraints by insets.

	mLayoutSpecs.minsize.w = rcBorders.right - rcBorders.left;
	mLayoutSpecs.minsize.h = rcBorders.bottom - rcBorders.top;

	rcConstraints.minsize.w -= mLayoutSpecs.minsize.w;
	rcConstraints.minsize.h -= mLayoutSpecs.minsize.h;

	// Layout children.

	tChildren::iterator it(mChildren.begin()), itEnd(mChildren.end());
	vduisize minsize(0, 0);

	for(; it!=itEnd; ++it) {
		IVDUIWindow *pWin = *it;

		pWin->PreLayout(rcConstraints);

		const VDUILayoutSpecs& prispecs = pWin->GetLayoutSpecs();

		if (minsize.w < prispecs.minsize.w)
			minsize.w = prispecs.minsize.w;
		if (minsize.h < prispecs.minsize.h)
			minsize.h = prispecs.minsize.h;
	}

	mLayoutSpecs.minsize.w += minsize.w;
	mLayoutSpecs.minsize.h += minsize.h;
}

void VDUIBaseWindowW32::PostLayoutBase(const vduirect& target) {
	VDUIControlW32::PostLayoutBase(target);

	vduirect rc(GetClientArea());

	rc.left += mInsets.left;
	rc.top += mInsets.top;
	rc.right -= mInsets.right;
	rc.bottom -= mInsets.bottom;

	tChildren::iterator it(mChildren.begin()), itEnd(mChildren.end());
	vduisize minsize(0, 0);

	for(; it!=itEnd; ++it) {
		IVDUIWindow *pWin = *it;

		pWin->PostLayout(rc);
	}
}

void VDUIBaseWindowW32::RebuildLinkUpdateMap() {
	if (!mbLinkUpdateMapDirty)
		return;

	mbLinkUpdateMapDirty = false;

	mLinkUpdateMap.clear();
	for(tLinkList::const_iterator it(mLinkList.begin()), itEnd(mLinkList.end()); it != itEnd; ++it) {
		const tLinkList::value_type& data = *it;
		const LinkEntry& linkEntry = data.second;

		for(std::vector<uint32>::const_iterator itLink(linkEntry.mLinkSources.begin()), itLinkEnd(linkEntry.mLinkSources.end()); itLink != itLinkEnd; ++itLink) {
			const uint32 sourceID = *itLink;

			mLinkUpdateMap.insert(tLinkUpdateMap::value_type(sourceID, &data));
		}
	}
}

void VDUIBaseWindowW32::ExecuteLinks(uint32 id) {
	std::list<uint32> queue;
	queue.push_back(id);

	ExecuteLinks(queue);
}

void VDUIBaseWindowW32::ExecuteAllLinks() {
	uint32 last = 0;

	std::list<uint32> queue;
	for(tControls::const_iterator it(mControls.begin()), itEnd(mControls.end()); it!=itEnd; ++it) {
		uint32 id = (*it).first;

		if (last != id) {
			last = id;

			queue.push_back(id);
		}
	}

	ExecuteLinks(queue);
}

void VDUIBaseWindowW32::ExecuteLinks(std::list<uint32>& queue) {
	if (mbLinkUpdateMapDirty)
		RebuildLinkUpdateMap();

	std::set<uint32> visited;
	while(!queue.empty()) {
		uint32 id = queue.front();
		queue.pop_front();

		std::pair<tLinkUpdateMap::iterator, tLinkUpdateMap::iterator> links(mLinkUpdateMap.equal_range(id));
		std::vector<IVDUIWindow *> srcWindows;

		for(; links.first != links.second; ++links.first) {
			const tLinkList::value_type *pLinkListEntry = (*links.first).second;
			const uint32 targetID = pLinkListEntry->first;
			const LinkEntry& linkEntry = pLinkListEntry->second;
			const int sources = linkEntry.mLinkSources.size();

			IVDUIWindow *pTarget = GetControl(targetID);

			if (!pTarget)
				continue;

			if (visited.insert(targetID).second)
				queue.push_back(targetID);

			srcWindows.resize(sources);
			for(int i=0; i<sources; ++i)
				srcWindows[i] = GetControl(linkEntry.mLinkSources[i]);

			const uint32 result = VDUIExecuteRuntimeExpression(&linkEntry.mByteCode.front(), &srcWindows.front());

			switch(linkEntry.mTarget) {
				case nsVDUI::kLinkTarget_Enable:
					pTarget->SetEnabled(result != 0);
					break;

				case nsVDUI::kLinkTarget_Value:
					pTarget->SetValue(result);
					break;
			}
		}
	}
}
