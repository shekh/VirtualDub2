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

#ifndef f_LOGWINDOW_H
#define f_LOGWINDOW_H

#include <windows.h>
#include <list>
#include <vd2/system/VDString.h>
#include <vd2/system/log.h>

#define LOGWINDOWCONTROLCLASS (g_szLogWindowControlName)

#ifndef f_LOGWINDOW_CPP
extern const char g_szLogWindowControlName[];
#endif

ATOM RegisterLogWindowControl();

class IVDLogWindowControl : public IVDLogger {
public:
	virtual void AttachAsLogger(bool bThisThreadOnly) = 0;
	virtual void AddEntry(int severity, const wchar_t *s) = 0;
	virtual void AddEntry(int severity, const VDStringW& s) = 0;
};

IVDLogWindowControl *VDGetILogWindowControl(HWND hwnd);

#endif
