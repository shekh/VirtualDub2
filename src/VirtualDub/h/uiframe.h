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

#ifndef UIFRAME_H
#define UIFRAME_H

#include <windows.h>
#include <vd2/system/atomic.h>
#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>

#define VDWM_ENGINE_EVENT (WM_APP + 70)

class VDINTERFACE IVDUIFrameEngine : public IVDRefCount {
public:
	virtual void Detach() = 0;
	virtual LRESULT OnEngineEvent(WPARAM wParam, LPARAM lParam) = 0;
};

class VDINTERFACE IVDUIFrameClient : public IVDRefCount {
public:
	virtual void Detach() = 0;
	virtual LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) = 0;

	virtual bool Intercept_WM_CHAR(WPARAM wParam, LPARAM lParam) { return false; }
	virtual bool Intercept_WM_KEYDOWN(WPARAM wParam, LPARAM lParam) { return false; }
	virtual bool Intercept_WM_KEYUP(WPARAM wParam, LPARAM lParam) { return false; }
	virtual bool Intercept_WM_SYSKEYDOWN(WPARAM wParam, LPARAM lParam) { return false; }
	virtual bool Intercept_WM_SYSKEYUP(WPARAM wParam, LPARAM lParam) { return false; }
};

class VDUIFrame : public vdlist<VDUIFrame>::node {
public:
	VDUIFrame(HWND hwnd);
	~VDUIFrame();

	void Attach(IVDUIFrameClient *);
	void Detach();

	void AttachEngine(IVDUIFrameEngine *);
	void DetachEngine();

	void Destroy();

	void SetNextMode(int nextMode);
	void SetAccelTable(HACCEL hAccel);
	void SetRegistryName(const char *name);

	void SavePlacement();
	void RestorePlacement(int nCmdShow);

	LPARAM DefProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	static void DestroyAll();

	static VDUIFrame *GetFrame(HWND hwnd) {
		VDASSERT(IsWindow(hwnd));
		VDUIFrame *p = (VDUIFrame *)GetWindowLongPtr(hwnd, 0);
		VDASSERTPTR(p);
		return p;
	}
	static ATOM Register();
	static ATOM Class() { return sClass; }
	static bool TranslateAcceleratorMessage(MSG& msg);

	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
protected:
	void DetachNow(HWND hwnd, bool bClient, bool bEngine);

	vdrefptr<IVDUIFrameClient>	mpClient;
	vdrefptr<IVDUIFrameEngine>	mpEngine;
	HACCEL				mhAccel;
	LRESULT (CALLBACK *mpDefWindowProc)(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	int					mNestCount;
	int					mNextMode;
	bool				mbDetachClient;
	bool				mbDetachEngine;
	bool				mbDestroy;
	HWND				mhwnd;
	int					mRefCount;		// does not need to be atomic as winprocs are single threaded
	const char			*mpRegistryName;

	static ATOM					sClass;
	static vdlist<VDUIFrame>	sFrameList;
};

#endif
