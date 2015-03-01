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

#ifndef _f_IVIDEOCOMPRESSOR_H
#define _f_IVIDEOCOMPRESSOR_H

#include <vfw.h>

class IVideoCompressor {
public:
	virtual ~IVideoCompressor() {};

	virtual LRESULT About				(HWND hwnd)											=0;
	virtual LRESULT Compress				(ICCOMPRESS *icc, DWORD cbSize)						=0;
	virtual LRESULT CompressBegin		(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)		=0;
	virtual LRESULT CompressEnd			()													=0;
	virtual LRESULT CompressFramesInfo	(ICCOMPRESSFRAMES *icf, DWORD cbSize)				=0;
	virtual LRESULT CompressGetFormat	(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)		=0;
	virtual LRESULT CompressGetSize		(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)		=0;
	virtual LRESULT CompressQuery		(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)		=0;
	virtual LRESULT Configure			(HWND hwnd)											=0;
	virtual LRESULT Decompress			(ICDECOMPRESS *icd, DWORD cbSize)					=0;
	virtual LRESULT DecompressBegin		(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)		=0;
	virtual LRESULT DecompressEnd		()													=0;
	virtual LRESULT DecompressGetFormat	(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)		=0;
	virtual LRESULT DecompressGetPalette	(BITMAPINFOHEADER *lpbiInput, BITMAPINFOHEADER *lpbiOutput)		=0;
	virtual LRESULT DecompressQuery		(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)		=0;
	virtual LRESULT DecompressSetPalette	(BITMAPINFOHEADER *lpbiPalette)						=0;
	virtual LRESULT DecompressEx			(ICDECOMPRESSEX *icdex, DWORD cbSize)				=0;
	virtual LRESULT DecompressExBegin	(ICDECOMPRESSEX *icdex, DWORD cbSize)				=0;
	virtual LRESULT DecompressExEnd		()													=0;
	virtual LRESULT DecompressExQuery	(ICDECOMPRESSEX *icdex, DWORD cbSize)				=0;
	virtual LRESULT Draw					(ICDRAW *icdraw, DWORD cbSize)						=0;
	virtual LRESULT DrawBegin			(ICDRAWBEGIN *icdrwBgn, DWORD cbSize)				=0;
	virtual LRESULT DrawChangePalette	(BITMAPINFO *lpbiInput)								=0;
	virtual LRESULT DrawEnd				()													=0;
	virtual LRESULT DrawFlush			()													=0;
	virtual LRESULT DrawGetPalette		()													=0;
	virtual LRESULT DrawGetTime			(DWORD *lplTime)									=0;
	virtual LRESULT DrawQuery			(BITMAPINFO *lpbiInput)								=0;
	virtual LRESULT DrawRealize			(HDC hdc, BOOL fBackground)							=0;
	virtual LRESULT DrawRenderBuffer		()													=0;
	virtual LRESULT DrawSetTime			(DWORD lpTime)										=0;
	virtual LRESULT DrawStart			()													=0;
	virtual LRESULT DrawStartPlay		(DWORD lFrom, DWORD lTo)							=0;
	virtual LRESULT DrawStop				()													=0;
	virtual LRESULT DrawStopPlay			()													=0;
	virtual LRESULT DrawSuggestFormat	(ICDRAWSUGGEST *icdrwSuggest, DWORD cbSize)			=0;
	virtual LRESULT DrawWindow			(RECT *prc)											=0;
	virtual LRESULT Get					(LPVOID pv, DWORD cbSize)							=0;
	virtual LRESULT GetBuffersWanted		(DWORD *lpdwBuffers)								=0;
	virtual LRESULT GetDefaultKeyFrameRate(DWORD *lpdwICValue)								=0;
	virtual LRESULT GetDefaultQuality	(DWORD *lpdwICValue)								=0;
	virtual LRESULT GetInfo				(ICINFO *lpicinfo, DWORD cbSize)					=0;
	virtual LRESULT GetQuality			(DWORD *lpdwICValue)								=0;
	virtual LRESULT GetState				(LPVOID pv, DWORD cbSize)							=0;
	virtual LRESULT SetStatusProc			(ICSETSTATUSPROC *icsetstatusProc, DWORD cbSize)	=0;
	virtual LRESULT SetQuality			(DWORD *lpdwICValue)								=0;
	virtual LRESULT SetState				(LPVOID pv, DWORD cbSize)							=0;

	virtual LRESULT Default				(DWORD dwDriverID, HDRVR hDriver, UINT uiMessage, LPARAM lParam1, LPARAM lParam2) = 0;
};

#endif
