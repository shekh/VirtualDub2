//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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

#ifndef f_PROGRESSDIALOG_H
#define f_PROGRESSDIALOG_H

#include <windows.h>

class ProgressDialog {
private:
	HWND hwndProgressBar, hwndDialog, hwndValue, mhwndParent;
	long newval, curval, maxval;
	int mSparseCount;
	int mSparseInterval;
	DWORD dwLastTime;
	bool fAbortEnabled;
	bool fAbort;
	bool mbPreviouslyEnabled;

	const char *lpszTitle, *lpszCaption, *lpszValueFormat;

	static INT_PTR CALLBACK ProgressDlgProc(HWND, UINT, WPARAM, LPARAM);
public:
	ProgressDialog(HWND hwndParent, const char *szTitle, const char *szCaption, long maxval, bool fAbortEnabled);
	~ProgressDialog();

	void setValueFormat(const char *);
	void setCaption(const char *);
	void setLimit(long);

	void advance(long newval) {
		this->newval = newval;
	}

	void check();
	void close();
};

#endif
