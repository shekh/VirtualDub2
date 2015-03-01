//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2005 Avery Lee
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

#include "stdafx.h"
#include <windows.h>
#include <richedit.h>
#include <malloc.h>
#include <stdio.h>
#include <vd2/system/thread.h>
#include <vd2/system/refcount.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/w32assist.h>
#include "backface.h"

extern HINSTANCE g_hInst;

#pragma warning(disable: 4073)
#pragma init_seg(lib)

#if VD_BACKFACE_ENABLED

///////////////////////////////////////////////////////////////////////////

class IVDBackfaceStream {
public:
	virtual void operator<<(const char *s) = 0;
};

class VDBackfaceClass : public vdlist_node {
public:
	VDBackfaceClass(const char *shortname, const char *longname);

	const char *mpShortName;
	const char *mpLongName;
	uint32	mObjectCount;
	VDBackfaceObjectNode mObjects;
};

VDBackfaceClass::VDBackfaceClass(const char *shortname, const char *longname)
	: mpShortName(shortname)
	, mpLongName(longname)
	, mObjectCount(0)
{
	mObjects.mpObjNext = mObjects.mpObjPrev = &mObjects;
}

class VDBackfaceService : public IVDBackfaceOutput {
public:
	VDBackfaceService();
	~VDBackfaceService();

	VDBackfaceClass *AddClass(const char *shortname, const char *longname);

	void AddObject(VDBackfaceObjectBase *pObject);
	void RemoveObject(VDBackfaceObjectBase *pObject);

	void DumpStatus(IVDBackfaceStream& out);
	void Execute(IVDBackfaceStream& out, char *cmd);

	void operator<<(const char *s);
	void operator()(const char *format, ...);
	VDStringA GetTag(VDBackfaceObjectBase *p);
	VDStringA GetBlurb(VDBackfaceObjectBase *p);

protected:
	VDBackfaceClass *GetClassByName(const VDStringSpanA& name);
	VDBackfaceObjectBase *GetObjectByName(const char *name);

	VDCriticalSection	mLock;

	uint32	mObjectCount;
	uint32	mClassCount;
	uint32	mNextInstance;

	VDStringA	*mpCaptureString;

	IVDBackfaceStream *mpOutput;

	vdlist<VDBackfaceClass>	mClasses;
};

VDBackfaceService g_VDBackfaceService;

VDBackfaceService::VDBackfaceService()
	: mObjectCount(0)
	, mClassCount(0)
	, mNextInstance(1)
	, mpCaptureString(NULL)
{
}

VDBackfaceService::~VDBackfaceService() {
	while(!mClasses.empty()) {
		VDBackfaceClass *pNode = mClasses.back();
		mClasses.pop_back();

		delete pNode;
	}
}

VDBackfaceClass *VDBackfaceService::AddClass(const char *shortname, const char *longname) {
	vdsynchronized(mLock) {
		VDBackfaceClass *pNode = new VDBackfaceClass(shortname, longname);

		mClasses.push_back(pNode);
		++mClassCount;

		return pNode;
	}
}

void VDBackfaceService::AddObject(VDBackfaceObjectBase *pObject) {
	vdsynchronized(mLock) {
		++mObjectCount;

		pObject->mInstance = mNextInstance++;

		VDBackfaceClass& cl = *pObject->mpClass;

		++cl.mObjectCount;

		cl.mObjects.mpObjNext->mpObjPrev = pObject;
		pObject->mpObjNext = cl.mObjects.mpObjNext;
		pObject->mpObjPrev = &cl.mObjects;
		cl.mObjects.mpObjNext = pObject;
	}
}

void VDBackfaceService::RemoveObject(VDBackfaceObjectBase *pObject) {
	vdsynchronized(mLock) {
		--mObjectCount;

		--pObject->mpClass->mObjectCount;
		pObject->mpObjNext->mpObjPrev = pObject->mpObjPrev;
		pObject->mpObjPrev->mpObjNext = pObject->mpObjNext;
	}
}

void VDBackfaceService::DumpStatus(IVDBackfaceStream& out) {
	vdsynchronized(mLock) {
		mpOutput = &out;

		operator<<("Backface status:");
		operator()("    %d objects being tracked", mObjectCount);
		operator()("    %d classes being tracked", mClassCount);

		out << "backface> ";
	}
}

void VDBackfaceService::Execute(IVDBackfaceStream& out, char *s) {
	vdsynchronized(mLock) {
		mpOutput = &out;

		operator<<(s);

		const char *argv[64];
		int argc = 0;

		while(argc < 63) {
			while(*s && *s == ' ')
				++s;

			if (!*s)
				break;

			argv[argc++] = s;

			if (*s == '"') {
				while(*s && *s != '"')
					++s;
			} else {
				while(*s && *s != ' ')
					++s;
			}

			if (!*s)
				break;

			*s++ = 0;
		}

		argv[argc] = NULL;

		if (argc) {
			VDStringSpanA cmd(argv[0]);

			if (cmd == "lc") {
				operator<<("");
				operator<<("Count    Class Name");
				operator<<("---------------------------------------------");

				vdlist<VDBackfaceClass>::iterator it(mClasses.begin()), itEnd(mClasses.end());
				for(; it!=itEnd; ++it) {
					VDBackfaceClass& cl = **it;

					operator()("%5d    %-8s (%s)", cl.mObjectCount, cl.mpShortName, cl.mpLongName);
				}
			} else if (cmd == "lo") {
				const char *cname = argv[1];
				
				if (cname) {
					VDBackfaceClass *cl = GetClassByName(VDStringSpanA(cname));

					if (!cl)
						operator()("Unknown class \"%s.\"", cname);
					else {
						VDBackfaceObjectNode *p = cl->mObjects.mpObjNext;

						operator<<("");
						operator<<("Pointer Instance  Desc");
						operator<<("---------------------------------------------");
						for(; p != &cl->mObjects; p = p->mpObjNext) {
							VDBackfaceObjectBase *pObj = static_cast<VDBackfaceObjectBase *>(p);

							operator()("%p  #%-5d  %s", pObj, pObj->mInstance, GetBlurb(pObj).c_str());
						}
					}
				}
			} else if (cmd == "dump") {
				const char *objname = argv[1];

				if (objname) {
					VDBackfaceObjectBase *obj = GetObjectByName(objname);

					if (obj) {
						operator()("Object %s | %p | %s:", objname, obj, GetBlurb(obj).c_str());
						obj->BackfaceDumpObject(*this);
					} else {
						operator()("Unknown object %s.", objname);
					}
				}
			} else if (cmd == "?") {
				operator<<("lc                     List classes");
				operator<<("lo <class>             List objects by class");
				operator<<("dump <class>:<inst>    Dump object");
			} else {
				operator<<("Unrecognized command -- ? for help.");
			}
		}

		out << ("backface> ");
	}
}

void VDBackfaceService::operator<<(const char *s) {
	if (mpCaptureString)
		mpCaptureString->assign(s);
	else {
		*mpOutput << s;
		*mpOutput << "\n";
	}
}

void VDBackfaceService::operator()(const char *format, ...) {
	char buf[3072];
	va_list val;

	va_start(val, format);
	if ((unsigned)_vsnprintf(buf, 3072, format, val) < 3072) {
		if (mpCaptureString)
			mpCaptureString->assign(buf);
		else {
			*mpOutput << buf;
			*mpOutput << "\n";
		}
	} else if (mpCaptureString)
		mpCaptureString->clear();
	va_end(val);
}

VDStringA VDBackfaceService::GetTag(VDBackfaceObjectBase *p) {
	VDStringA tag;
	if (p)
		tag.sprintf("%s:%u", p->mpClass->mpShortName, p->mInstance);
	else
		tag = "null";
	return tag;
}

VDStringA VDBackfaceService::GetBlurb(VDBackfaceObjectBase *pObject) {
	VDStringA s;
	VDStringA *pOldCapture = mpCaptureString;
	mpCaptureString = &s;
	pObject->BackfaceDumpBlurb(*this);
	mpCaptureString = pOldCapture;
	return s;
}

VDBackfaceClass *VDBackfaceService::GetClassByName(const VDStringSpanA& name) {
	vdlist<VDBackfaceClass>::iterator it(mClasses.begin()), itEnd(mClasses.end());

	for(; it!=itEnd; ++it) {
		VDBackfaceClass *p = *it;

		if (name == p->mpShortName || name == p->mpLongName)
			return p;
	}

	return NULL;
}

VDBackfaceObjectBase *VDBackfaceService::GetObjectByName(const char *name) {
	VDStringSpanA names(name);
	VDStringSpanA::size_type colonPos = names.find(':');

	if (colonPos == VDStringSpanA::npos)
		return NULL;

	const char *s = name + colonPos + 1;
	unsigned index;

	if (!*s || 1 != sscanf(s, "%u", &index))
		return NULL;

	VDBackfaceClass *cl = GetClassByName(names.subspan(0, colonPos));
	if (!cl)
		return NULL;

	VDBackfaceObjectNode *p = cl->mObjects.mpObjNext;
	for(; p != &cl->mObjects; p = p->mpObjNext) {
		VDBackfaceObjectBase *pObj = static_cast<VDBackfaceObjectBase *>(p);

		if (pObj->mInstance == index)
			return pObj;
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////

VDBackfaceObjectBase::VDBackfaceObjectBase(const VDBackfaceObjectBase& src)
	: mpClass(src.mpClass)
{
	g_VDBackfaceService.AddObject(this);
}

VDBackfaceObjectBase::~VDBackfaceObjectBase() {
	g_VDBackfaceService.RemoveObject(this);
}

VDBackfaceClass *VDBackfaceObjectBase::BackfaceInitClass(const char *shortname, const char *longname) {
	return g_VDBackfaceService.AddClass(shortname, longname);
}

void VDBackfaceObjectBase::BackfaceInitObject(VDBackfaceClass *pClass) {
	mpClass = pClass;
	g_VDBackfaceService.AddObject(this);
}

void VDBackfaceObjectBase::BackfaceDumpObject(IVDBackfaceOutput&) {
}

void VDBackfaceObjectBase::BackfaceDumpBlurb(IVDBackfaceOutput&) {
}

///////////////////////////////////////////////////////////////////////////

class VDBackfaceConsole : public IVDBackfaceStream {
public:
	VDBackfaceConsole();
	~VDBackfaceConsole();

	int AddRef();
	int Release();

	void Init();

protected:
	static ATOM RegisterWindowClass();
	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	bool OnCreate();
	void OnDestroy();
	void OnSize();
	LRESULT OnNotify(NMHDR *);

	void operator<<(const char *s);

	HWND	mhwnd;
	HWND	mhwndLog;
	HWND	mhwndEdit;
	HFONT	mFont;
	HMODULE	mhmodRichEdit;
	VDAtomicInt	mRefCount;
};

VDBackfaceConsole::VDBackfaceConsole()
	: mRefCount(0)
	, mFont(NULL)
	, mhmodRichEdit(NULL)
{
}

VDBackfaceConsole::~VDBackfaceConsole() {
	if (mFont)
		DeleteObject(mFont);
}

int VDBackfaceConsole::AddRef() {
	return ++mRefCount;
}

int VDBackfaceConsole::Release() {
	int rv = --mRefCount;

	if (!rv)
		delete this;

	return 0;
}

void VDBackfaceConsole::Init() {
	static ATOM a = RegisterWindowClass();

	CreateWindow((LPCTSTR)a, "Backface", WS_OVERLAPPEDWINDOW|WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, g_hInst, this);
}

ATOM VDBackfaceConsole::RegisterWindowClass() {
	WNDCLASS wc;
	wc.style			= 0;
	wc.lpfnWndProc		= StaticWndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= sizeof(void *);
	wc.hInstance		= g_hInst;
	wc.hIcon			= NULL;
	wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground	= NULL;
	wc.lpszMenuName		= NULL;
	wc.lpszClassName	= "Backface";
	return RegisterClass(&wc);
}

LRESULT CALLBACK VDBackfaceConsole::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDBackfaceConsole *pThis;
	
	if (msg == WM_NCCREATE) {
		pThis = (VDBackfaceConsole *)(((const CREATESTRUCT *)lParam)->lpCreateParams);
		pThis->AddRef();
		pThis->mhwnd = hwnd;
		SetWindowLongPtr(hwnd, 0, (LONG_PTR)pThis);
	} else {
		pThis = (VDBackfaceConsole *)GetWindowLongPtr(hwnd, 0);

		if (pThis) {
			if (msg != WM_NCCREATE) {
				pThis->AddRef();
				LRESULT lr = pThis->WndProc(msg, wParam, lParam);
				pThis->Release();
				return lr;
			}

			pThis->mhwnd = NULL;
			pThis->Release();
		}
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT VDBackfaceConsole::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_CREATE:
		return OnCreate() ? 0 : -1;
	case WM_DESTROY:
		OnDestroy();
		break;
	case WM_SIZE:
		OnSize();
		return 0;
	case WM_NOTIFY:
		return OnNotify((NMHDR *)lParam);
	case WM_SETFOCUS:
		SetFocus(mhwndEdit);
		return 0;
	}

	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

bool VDBackfaceConsole::OnCreate() {
	mhmodRichEdit = LoadLibrary("riched32");

	if (!mFont) {
		mFont = CreateFont(-10, 0, 0, 0, 0, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_CHARACTER_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Lucida Console");
		if (!mFont)
			return false;
	}

	mhwndLog = CreateWindowEx(WS_EX_CLIENTEDGE, "RICHEDIT", "", ES_READONLY|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL|WS_VISIBLE|WS_CHILD, 0, 0, 0, 0, mhwnd, (HMENU)100, g_hInst, NULL);
	if (!mhwndLog)
		return false;

	SendMessage(mhwndLog, WM_SETFONT, (WPARAM)mFont, NULL);

	mhwndEdit = CreateWindowEx(WS_EX_CLIENTEDGE, "RICHEDIT", "", WS_VISIBLE|WS_CHILD, 0, 0, 0, 0, mhwnd, (HMENU)100, g_hInst, NULL);
	if (!mhwndEdit)
		return false;

	SendMessage(mhwndEdit, EM_SETEVENTMASK, 0, ENM_KEYEVENTS);
	SendMessage(mhwndEdit, WM_SETFONT, (WPARAM)mFont, NULL);

	OnSize();

	g_VDBackfaceService.DumpStatus(*this);

	return true;
}

void VDBackfaceConsole::OnDestroy() {
	if (mhwndLog) {
		DestroyWindow(mhwndLog);
		mhwndLog = NULL;
	}

	if (mhwndEdit) {
		DestroyWindow(mhwndEdit);
		mhwndEdit = NULL;
	}

	if (mhmodRichEdit) {
		FreeLibrary(mhmodRichEdit);
		mhmodRichEdit = NULL;
	}
}

void VDBackfaceConsole::OnSize() {
	RECT r;

	if (GetClientRect(mhwnd, &r)) {
		int h = 20;

		SetWindowPos(mhwndLog, NULL, 0, 0, r.right, r.bottom > h ? r.bottom - h : 0, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS);
		SetWindowPos(mhwndEdit, NULL, 0, r.bottom - h, r.right, h, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS);
	}
}

LRESULT VDBackfaceConsole::OnNotify(NMHDR *pHdr) {
	if (pHdr->hwndFrom == mhwndEdit) {
		if (pHdr->code == EN_MSGFILTER) {
			const MSGFILTER& mf = *(const MSGFILTER *)pHdr;

			if (mf.msg == WM_CHAR) {
				if (mf.wParam == '\r') {
					int len = GetWindowTextLength(mhwndEdit);
					if (len) {
						char *buf = (char *)_alloca(len+1);
						buf[0] = 0;

						if (GetWindowText(mhwndEdit, buf, len+1)) {
							SetWindowText(mhwndEdit, "");
							g_VDBackfaceService.Execute(*this, buf);
						}
					}
					return true;
				}
			}

			return false;
		}
	}

	return 0;
}

void VDBackfaceConsole::operator<<(const char *s) {
	SendMessage(mhwndLog, EM_SETSEL, -1, -1);
	SendMessage(mhwndLog, EM_REPLACESEL, FALSE, (LPARAM)s);
}

///////////////////////////////////////////////////////////////////////////

void VDBackfaceOpenConsole() {
	vdrefptr<VDBackfaceConsole> pConsole(new VDBackfaceConsole);

	pConsole->Init();
}

///////////////////////////////////////////////////////////////////////////

#else

void VDBackfaceOpenConsole() {}

#endif