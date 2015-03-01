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

#include "stdafx.h"
#include "MonoBitmap.h"

MonoBitmap::MonoBitmap(HDC hdcRef, int width, int height, COLORREF crFore, COLORREF crBack) {
	HDC hdcDisplay = NULL;

	if (!hdcRef)
		hdcRef = hdcDisplay = CreateDC("DISPLAY", NULL, NULL, NULL);

	hdcCompat = CreateCompatibleDC(hdcRef);

	iPitch = ((width + 31)/32)*4;

	bi.bi.bmiHeader.biSize			= sizeof(BITMAPINFOHEADER);
	bi.bi.bmiHeader.biWidth			= width;
	bi.bi.bmiHeader.biHeight		= height;
	bi.bi.bmiHeader.biPlanes		= 1;
	bi.bi.bmiHeader.biBitCount		= 1;
	bi.bi.bmiHeader.biCompression	= BI_RGB;
	bi.bi.bmiHeader.biSizeImage		= iPitch*height;
	bi.bi.bmiHeader.biXPelsPerMeter	= 100;
	bi.bi.bmiHeader.biYPelsPerMeter	= 100;
	bi.bi.bmiHeader.biClrUsed		= 2;
	bi.bi.bmiHeader.biClrImportant	= 2;
	bi.bi.bmiColors[0].rgbBlue	= GetBValue(crBack);
	bi.bi.bmiColors[0].rgbGreen	= GetGValue(crBack);
	bi.bi.bmiColors[0].rgbRed	= GetRValue(crBack);
	bi.bi.bmiColors[1].rgbBlue	= GetBValue(crFore);
	bi.bi.bmiColors[1].rgbGreen	= GetGValue(crFore);
	bi.bi.bmiColors[1].rgbRed	= GetRValue(crFore);

	hbm = CreateDIBSection(hdcCompat, &bi.bi, DIB_RGB_COLORS, &lpvBits, NULL, 0);
	SelectObject(hdcCompat, hbm);

	if (hdcDisplay) DeleteDC(hdcDisplay);

}

MonoBitmap::~MonoBitmap() {
	DeleteDC(hdcCompat);
	DeleteObject(hbm);
}

void MonoBitmap::Clear() {
	GdiFlush();
	memset(lpvBits, 0, bi.bi.bmiHeader.biSizeImage);
}

void MonoBitmap::BitBlt(HDC hdcDest, LONG x, LONG y) {
	BitBlt(hdcDest, x, y, 0, 0, bi.bi.bmiHeader.biWidth, bi.bi.bmiHeader.biHeight);
}

void MonoBitmap::BitBlt(HDC hdcDest, LONG dx, LONG dy, LONG sx, LONG sy, LONG w, LONG h) {
	::BitBlt(hdcDest,
		dx, dy,
		w, h,
		hdcCompat,
		sx, sy,
		SRCCOPY);
}
