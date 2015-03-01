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
#include <vd2/Dita/w32peer.h>

IVDUIWindow *VDUICreatePeer(VDGUIHandle h) {
	return new VDUIPeerW32((HWND)h);
}

VDUIPeerW32::VDUIPeerW32()
	: mhwnd(NULL)
{
}

VDUIPeerW32::VDUIPeerW32(HWND hwnd)
	: mhwnd(hwnd)
{
}

void VDUIPeerW32::Attach(HWND hwnd) {
	mhwnd = hwnd;
}

void VDUIPeerW32::Detach() {
	mhwnd = NULL;
}

void *VDUIPeerW32::AsInterface(uint32 id) {
	if (id == VDUIPeerW32::kTypeID) return static_cast<VDUIPeerW32 *>(this);
	if (id == IVDUIWindowW32::kTypeID) return static_cast<IVDUIWindowW32 *>(this);

	return VDUIWindow::AsInterface(id);
}

void VDUIPeerW32::RelayoutChildren() {
	tChildren::iterator it(mChildren.begin()), itEnd(mChildren.end());
	const vduirect r(GetClientArea());

	for(; it!=itEnd; ++it) {
		IVDUIWindow *pWin = *it;

		pWin->PostLayout(r);
	}
}

void VDUIPeerW32::SetFocus() {
	if (mhwnd)
		::SetFocus(mhwnd);
}

void VDUIPeerW32::SetCaption(const wchar_t *caption) {
	VDUIWindow::SetCaption(caption);

	if (mhwnd)
		VDSetWindowTextW32(mhwnd, mCaption.c_str());
}

vduirect VDUIPeerW32::GetArea() {
	RECT r;

	HWND hwndParent = ::GetParent(mhwnd);
	GetWindowRect(mhwnd, &r);

	if (hwndParent)
		MapWindowPoints(hwndParent, NULL, (LPPOINT)&r, 2);

	return vduirect(r.left, r.top, r.right, r.bottom);
}

void VDUIPeerW32::SetArea(const vduirect& pos) {
	SetWindowPos(mhwnd, NULL, pos.left, pos.top, pos.width(), pos.height(), SWP_NOZORDER|SWP_NOACTIVATE);
	VDUIWindow::SetArea(pos);
}

vduirect VDUIPeerW32::GetClientArea() const {
	RECT r;

	GetClientRect(mhwnd, &r);

	return vduirect(r.left, r.top, r.right, r.bottom);
}

void VDUIPeerW32::PropagateVisible(bool vis) {
	ShowWindow(mhwnd, vis && mbVisible ? SW_SHOW : SW_HIDE);
}

void VDUIPeerW32::PropagateEnabled(bool ena) {
	EnableWindow(mhwnd, ena && mbEnabled);
}

VDUIPeerW32 *VDUIPeerW32::GetParentPeerW32() const {
	for(IVDUIWindow *pWin = mpParent; pWin; pWin = pWin->GetParent()) {
		VDUIPeerW32 *pPeer = vdpoly_cast<VDUIPeerW32 *>(pWin);

		if (pPeer && pPeer->IsOwnerW32())
			return pPeer;
	}

	return NULL;
}

HWND VDUIPeerW32::GetParentW32() const {
	VDUIPeerW32 *pParentW32 = GetParentPeerW32();

	return pParentW32 ? pParentW32->GetHandleW32() : NULL;
}

bool VDUIPeerW32::IsOwnerW32() const {
	return !(GetWindowLong(mhwnd, GWL_STYLE) & WS_CHILD);
}

void VDUIPeerW32::RegisterCallbackW32(VDUIPeerW32 *pChild) {
	mCallbacks[pChild->GetHandleW32()] = pChild;
}

void VDUIPeerW32::UnregisterCallbackW32(VDUIPeerW32 *pChild) {
	mCallbacks.erase(pChild->GetHandleW32());
}

void VDUIPeerW32::UpdateCaptionW32() {
	mCaption = VDGetWindowTextW32(mhwnd);
}