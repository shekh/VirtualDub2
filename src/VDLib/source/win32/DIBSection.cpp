//	VirtualDub - Video processing and capture application
//	Application helper library
//	Copyright (C) 1998-2007 Avery Lee
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
#include <vd2/VDLib/win32/DIBSection.h>
#include <vd2/VDLib/win32/FileMapping.h>
#include <vd2/Kasumi/pixmap.h>

VDDIBSectionW32::VDDIBSectionW32()
	: mpBits(NULL)
	, mhbm(NULL)
	, mhdc(NULL)
	, mhgo(NULL)
	, mbForceUnmap(false)
{
}

VDDIBSectionW32::~VDDIBSectionW32() {
	Shutdown();
}

bool VDDIBSectionW32::Init(int w, int h, int depth, const VDFileMappingW32 *mapping, uint32 mapOffset) {
	BITMAPINFO bi = {0};
	bi.bmiHeader.biSize				= sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth			= w;
	bi.bmiHeader.biHeight			= h;
	bi.bmiHeader.biPlanes			= 1;
	bi.bmiHeader.biCompression		= BI_RGB;
	bi.bmiHeader.biBitCount			= depth;
	bi.bmiHeader.biSizeImage		= ((w * depth + 31) >> 5) * 4 * abs(h);
	bi.bmiHeader.biXPelsPerMeter	= 0;
	bi.bmiHeader.biYPelsPerMeter	= 0;
	bi.bmiHeader.biClrUsed			= 0;
	bi.bmiHeader.biClrImportant		= 0;

	return Init(&bi, mapping, mapOffset);
}

bool VDDIBSectionW32::Init(const tagBITMAPINFO *bi, const VDFileMappingW32 *mapping, uint32 mapOffset) {
	Shutdown();

	HDC hdc = GetDC(NULL);
	mhdc = CreateCompatibleDC(hdc);
	if (mhdc) {
		HANDLE hMap = NULL;
		if (mapping)
			hMap = mapping->GetHandle();

		mhbm = CreateDIBSection(hdc, bi, DIB_RGB_COLORS, &mpBits, hMap, mapOffset);
		if (mhbm) {
			mbForceUnmap = (hMap != NULL);

			mhgo = SelectObject(mhdc, mhbm);
			if (mhgo) {
				ReleaseDC(NULL, hdc);

				mWidth		= bi->bmiHeader.biWidth;
				mHeight		= abs(bi->bmiHeader.biHeight);
				mDepth		= bi->bmiHeader.biBitCount;
				mPitch		= ((mWidth * mDepth + 31) >> 5) * 4;
				mpScan0		= mpBits;
				if (bi->bmiHeader.biHeight >= 0) {
					mpScan0 = (char *)mpScan0 + mPitch*(mHeight - 1);
					mPitch = -mPitch;
				}
				return true;
			}
		}
	}
	ReleaseDC(NULL, hdc);

	Shutdown();
	return false;
}

void VDDIBSectionW32::Shutdown() {
	if (mhdc) {
		if (mhbm) {
			if (mhgo) {
				SelectObject(mhdc, mhgo);
				mhgo = NULL;
			}

			DeleteObject(mhbm);
			mhbm = NULL;

			if (mbForceUnmap) {
				UnmapViewOfFile(mpBits);

				mbForceUnmap = false;
			}

			mpBits = NULL;
		}

		DeleteDC(mhdc);
		mhdc = NULL;
	}
}

VDPixmap VDDIBSectionW32::GetPixmap() const {
	VDPixmap px;

	px.data		= mpScan0;
	px.pitch	= mPitch;
	px.palette	= NULL;
	px.w		= mWidth;
	px.h		= mHeight;
	px.format	= mDepth == 16 ? nsVDPixmap::kPixFormat_XRGB1555
				: mDepth == 24 ? nsVDPixmap::kPixFormat_RGB888
				: mDepth == 32 ? nsVDPixmap::kPixFormat_XRGB8888
				: 0;
	px.data2	= NULL;
	px.pitch2	= 0;
	px.data3	= NULL;
	px.pitch3	= 0;
	return px;
}
