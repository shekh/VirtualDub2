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

#include "yuvcodec.h"
#include "VBitmap.h"

//#define MY_FOURCC 'cqcv'
#define MY_FOURCC 'BUDV'

YUVCodecDriver g_YUVCodecDriver;

YUVCodecDriver::~YUVCodecDriver() {
}

BOOL YUVCodecDriver::Load(HDRVR hDriver) {
	return TRUE;
}

void YUVCodecDriver::Free(HDRVR hDriver) {
}

DWORD YUVCodecDriver::Open(HDRVR hDriver, char *szDescription, LPVIDEO_OPEN_PARMS lpVideoOpenParms) {
	return (DWORD)new YUVCodec();
}

void YUVCodecDriver::Disable(HDRVR hDriver) {
}

void YUVCodecDriver::Enable(HDRVR hDriver) {
}

LPARAM YUVCodecDriver::Default(DWORD dwDriverID, HDRVR hDriver, UINT uiMessage, LPARAM lParam1, LPARAM lParam2) {
	return DefDriverProc(dwDriverID, hDriver, uiMessage, lParam1, lParam2);
}

/////////////////////////////////////////////////////////////

YUVCodec::YUVCodec() {
}

YUVCodec::~YUVCodec() {
}

LRESULT YUVCodec::Compress(ICCOMPRESS *icc, DWORD cbSize) {
	return ICERR_UNSUPPORTED;
}

LRESULT YUVCodec::CompressFramesInfo(ICCOMPRESSFRAMES *icf, DWORD cbSize) {
	return ICERR_UNSUPPORTED;
}

LRESULT YUVCodec::CompressGetFormat(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput) {
	return ICERR_UNSUPPORTED;
}

LRESULT YUVCodec::CompressGetSize(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput) {
	return ICERR_UNSUPPORTED;
}

LRESULT YUVCodec::CompressQuery(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput) {
	return ICERR_UNSUPPORTED;
}

LRESULT YUVCodec::DecompressGetFormat(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput) {
	BITMAPINFOHEADER *bmihInput		= &lpbiInput->bmiHeader;
	BITMAPINFOHEADER *bmihOutput	= &lpbiOutput->bmiHeader;
	LRESULT res;

	if (!lpbiOutput)
		return sizeof(BITMAPINFOHEADER);

	if (ICERR_OK != (res = DecompressQuery(lpbiInput, 0)))
		return res;

	bmihOutput->biSize			= sizeof(BITMAPINFOHEADER);
	bmihOutput->biWidth			= bmihInput->biWidth;
	bmihOutput->biHeight		= bmihInput->biHeight;
	bmihOutput->biPlanes		= 1;
	bmihOutput->biBitCount		= 24;
	bmihOutput->biCompression	= BI_RGB;
	bmihOutput->biSizeImage		= ((bmihInput->biWidth*3+3)&-4)*bmihInput->biHeight;
	bmihOutput->biXPelsPerMeter	= bmihInput->biXPelsPerMeter;
	bmihOutput->biYPelsPerMeter	= bmihInput->biYPelsPerMeter;
	bmihOutput->biClrUsed		= 0;
	bmihOutput->biClrImportant	= 0;

	return ICERR_OK;
}

LRESULT YUVCodec::DecompressEx(ICDECOMPRESSEX *icdex, DWORD cbSize) {
	BITMAPINFOHEADER *bmihInput		= icdex->lpbiSrc;
	BITMAPINFOHEADER *bmihOutput	= icdex->lpbiDst;
	LRESULT res;
	long w0s, h0s, w0d, h0d;

	/////////////////////

	if (ICERR_OK != (res = DecompressExQuery(icdex, cbSize)))
		return res;

	if (icdex->dwFlags & (ICDECOMPRESS_UPDATE))
		return ICERR_BADPARAM;

	w0s = icdex->dxSrc==-1 ? bmihInput->biWidth  : icdex->dxSrc;
	h0s = icdex->dySrc==-1 ? bmihInput->biHeight : icdex->dySrc;
	w0d = icdex->dxDst==-1 ? bmihOutput->biWidth  : icdex->dxDst;
	h0d = icdex->dyDst==-1 ? bmihOutput->biHeight : icdex->dyDst;

	if (w0s != w0d || h0s != h0d)
		return ICERR_BADPARAM;

	if (w0s > bmihInput->biWidth || h0s > bmihInput->biHeight)
		return ICERR_BADPARAM;

	if (w0s <= 0 || h0s <= 0)
		return ICERR_OK;

	VBitmap src(icdex->lpSrc, bmihInput);
	VBitmap(icdex->lpDst, bmihOutput).BitBltFromYUY2(icdex->xDst, icdex->yDst, &src, icdex->xSrc, icdex->ySrc, w0s, h0s);
//	VBitmap(icdex->lpDst, bmihOutput).BitBltFromYUY2Fullscale(icdex->xDst, icdex->yDst, &VBitmap(icdex->lpSrc, bmihInput), icdex->xSrc, icdex->ySrc, w0s, h0s);

	return ICERR_OK;
}

LRESULT YUVCodec::DecompressExQuery(ICDECOMPRESSEX *icdex, DWORD cbSize) {
	BITMAPINFOHEADER *bmihInput		= icdex->lpbiSrc;

	// We only accept YUY2!

	if (bmihInput->biPlanes != 1) return ICERR_BADFORMAT;
	if (bmihInput->biBitCount != 16) return ICERR_BADFORMAT;
	if (bmihInput->biCompression != '2YUY') return ICERR_BADFORMAT;

	if (icdex->lpbiDst) {
		BITMAPINFOHEADER *bmihOutput	= icdex->lpbiDst;

		// YUY2 can decompress to RGB16, RGB24, and RGB32.

		if (bmihOutput->biPlanes != 1) return ICERR_BADFORMAT;
		if (bmihOutput->biCompression != BI_RGB) return ICERR_BADFORMAT;
		if (bmihOutput->biBitCount != 16 && bmihOutput->biBitCount != 24 && bmihOutput->biBitCount != 32) return ICERR_BADFORMAT;
		if (bmihOutput->biWidth != bmihInput->biWidth) return ICERR_BADFORMAT;
		if (bmihOutput->biHeight != bmihInput->biHeight) return ICERR_BADFORMAT;
	}

	_RPT0(0,"--------------Accepted!\n");

	return ICERR_OK;
}

LRESULT YUVCodec::GetInfo(ICINFO *lpicinfo, DWORD cbSize) {
	if (cbSize < sizeof(ICINFO)) return 0;

	lpicinfo->dwSize		= sizeof(ICINFO);
	lpicinfo->fccType		= ICTYPE_VIDEO;
	lpicinfo->fccHandler	= MY_FOURCC;
	lpicinfo->dwFlags		= 0;
	lpicinfo->dwVersion		= 1;
	lpicinfo->dwVersionICM	= ICVERSION;
	wcscpy(lpicinfo->szName, L"VirtualDubYUV");
	wcscpy(lpicinfo->szDescription, L"VirtualDub YUV driver");

	return sizeof(ICINFO);
}

