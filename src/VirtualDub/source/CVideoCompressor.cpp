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

#include "CVideoCompressor.h"

CVideoCompressor::~CVideoCompressor() {
}

LRESULT CVideoCompressor::About(HWND hwnd) {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::CompressBegin(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput) {
	return CompressQuery(lpbiInput, lpbiOutput);
}

LRESULT CVideoCompressor::CompressEnd() {
	return ICERR_OK;
}

LRESULT CVideoCompressor::Configure(HWND hwnd) {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::Decompress(ICDECOMPRESS *icd, DWORD cbSize) {
	ICDECOMPRESSEX icdex;

	icdex.dwFlags		= icd->dwFlags;
	icdex.lpbiSrc		= icd->lpbiInput;
	icdex.lpSrc			= icd->lpInput;
	icdex.lpbiDst		= icd->lpbiOutput;
	icdex.lpDst			= icd->lpOutput;
	icdex.xDst			= 0;
	icdex.yDst			= 0;
	icdex.dxDst			= -1;
	icdex.dyDst			= -1;
	icdex.xSrc			= 0;
	icdex.ySrc			= 0;
	icdex.dxSrc			= -1;
	icdex.dySrc			= -1;

	return DecompressEx(&icdex, sizeof icdex);
}

LRESULT CVideoCompressor::DecompressBegin(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput) {
	return DecompressQuery(lpbiInput, lpbiOutput);
}

LRESULT CVideoCompressor::DecompressEnd() {
	return ICERR_OK;
}

LRESULT CVideoCompressor::DecompressGetPalette(BITMAPINFOHEADER *lpbiInput, BITMAPINFOHEADER *lpbiOutput) {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::DecompressQuery(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput) {
	ICDECOMPRESSEX icdex;

	icdex.dwFlags		= 0;
	icdex.lpbiSrc		= (BITMAPINFOHEADER *)lpbiInput;
	icdex.lpSrc			= NULL;
	icdex.lpbiDst		= (BITMAPINFOHEADER *)lpbiOutput;
	icdex.lpDst			= NULL;
	icdex.xDst			= 0;
	icdex.yDst			= 0;
	icdex.dxDst			= -1;
	icdex.dyDst			= -1;
	icdex.xSrc			= 0;
	icdex.ySrc			= 0;
	icdex.dxSrc			= -1;
	icdex.dySrc			= -1;

	return DecompressExQuery(&icdex, sizeof icdex);
}

LRESULT CVideoCompressor::DecompressSetPalette	(BITMAPINFOHEADER *lpbiPalette) {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::DecompressExBegin(ICDECOMPRESSEX *icdex, DWORD cbSize) {
	return DecompressExQuery(icdex, cbSize);
}

LRESULT CVideoCompressor::DecompressExEnd() {
	return ICERR_OK;
}

LRESULT CVideoCompressor::Draw(ICDRAW *icdraw, DWORD cbSize) {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::DrawBegin(ICDRAWBEGIN *icdrwBgn, DWORD cbSize) {
	return ICERR_UNSUPPORTED;
}
LRESULT CVideoCompressor::DrawChangePalette(BITMAPINFO *lpbiInput) {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::DrawEnd() {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::DrawFlush() {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::DrawGetPalette() {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::DrawGetTime(DWORD *lplTime) {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::DrawQuery(BITMAPINFO *lpbiInput) {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::DrawRealize(HDC hdc, BOOL fBackground) {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::DrawRenderBuffer() {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::DrawSetTime(DWORD lpTime) {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::DrawStart() {
	return 0;
}

LRESULT CVideoCompressor::DrawStartPlay(DWORD lFrom, DWORD lTo) {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::DrawStop() {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::DrawStopPlay() {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::DrawSuggestFormat(ICDRAWSUGGEST *icdrwSuggest, DWORD cbSize) {
	return icdrwSuggest->lpbiSuggest ? sizeof(BITMAPINFOHEADER) : ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::DrawWindow(RECT *prc) {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::Get(LPVOID pv, DWORD cbSize) {
	return 0;
}

LRESULT CVideoCompressor::GetBuffersWanted(DWORD *lpdwBuffers) {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::GetDefaultKeyFrameRate(DWORD *lpdwICValue) {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::GetDefaultQuality(DWORD *lpdwICValue) {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::GetQuality(DWORD *lpdwICValue) {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::GetState(LPVOID pv, DWORD cbSize) {
	return cbSize ? 1 : 0;
}

LRESULT CVideoCompressor::SetStatusProc(ICSETSTATUSPROC *icsetstatusProc, DWORD cbSize) {
	return ICERR_OK;
}

LRESULT CVideoCompressor::SetQuality(DWORD *lpdwICValue) {
	return ICERR_UNSUPPORTED;
}

LRESULT CVideoCompressor::SetState(LPVOID pv, DWORD cbSize) {
	return cbSize;
}

LRESULT CVideoCompressor::Default(DWORD dwDriverID, HDRVR hDriver, UINT uiMessage, LPARAM lParam1, LPARAM lParam2) {
	return ICERR_UNSUPPORTED;
}

