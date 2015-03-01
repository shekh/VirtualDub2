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

#ifndef f_MONOBITMAP_H
#define f_MONOBITMAP_H

#include <windows.h>

class MonoBitmap {
private:
	struct {
		BITMAPINFO bi;
		COLORREF crFore;
	} bi;

	HBITMAP		hbm;
	HDC			hdcCompat;
	HBRUSH		hbrBlack;
	void *		lpvBits;
	int			iPitch;

public:
	MonoBitmap(HDC hdcRef, int width, int height, COLORREF crFore, COLORREF crBack);
	~MonoBitmap();

	void	Clear();
	void *	getBits() { return lpvBits; }
	HDC		getDC() { return hdcCompat; }
	int		getPitch() { return iPitch; }
	void	BitBlt(HDC hdcDest, LONG x, LONG y);
	void	BitBlt(HDC hdcDest, LONG dx, LONG dy, LONG sx, LONG sy, LONG w, LONG h);
};

#endif
