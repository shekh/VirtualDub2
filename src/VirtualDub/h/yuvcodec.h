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

#ifndef f_YUVDRIVER_H
#define f_YUVDRIVER_H

#include "IVideoDriver.h"
#include "CVideoCompressor.h"

class YUVCodecDriver : public IVideoDriver {
public:
	virtual ~YUVCodecDriver();

	BOOL	Load(HDRVR hDriver);
	void	Free(HDRVR hDriver);
	DWORD	Open(HDRVR hDriver, char *szDescription, LPVIDEO_OPEN_PARMS lpVideoOpenParms);
	void	Disable(HDRVR hDriver);
	void	Enable(HDRVR hDriver);
	LRESULT	Default(DWORD dwDriverID, HDRVR hDriver, UINT uiMessage, LPARAM lParam1, LPARAM lParam2);
};

class YUVCodec : public CVideoCompressor {
public:
	YUVCodec();
	~YUVCodec();

	LRESULT Compress(ICCOMPRESS *icc, DWORD cbSize);
	LRESULT CompressFramesInfo(ICCOMPRESSFRAMES *icf, DWORD cbSize);
	LRESULT CompressGetFormat(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput);
	LRESULT CompressGetSize(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput);
	LRESULT CompressQuery(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput);
	LRESULT DecompressGetFormat(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput);
	LRESULT DecompressEx(ICDECOMPRESSEX *icdex, DWORD cbSize);
	LRESULT DecompressExQuery(ICDECOMPRESSEX *icdex, DWORD cbSize);
	LRESULT GetInfo(ICINFO *lpicinfo, DWORD cbSize);
};

extern YUVCodecDriver g_YUVCodecDriver;

#endif
