/**
 * Debugmode Frameserver
 * Copyright (C) 2002-2009 Satish Kumar, All Rights Reserved
 * http://www.debugmode.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 * http://www.gnu.org/copyleft/gpl.html
 */

#include "stdafx.h"
#include "dfsc.h"
#include <crtdbg.h>
#include <STDIO.H>
#include <TCHAR.H>
#include <vd2/Riza/bitmap.h>

class DfscInstance {
public:
  bool decompressing;

  DfscData* vars;
  HANDLE varFile;
  HANDLE videoEncSem, videoEncEvent, videoDecEvent;
  int curCmpIndex;

public:
  DfscInstance();
  BOOL QueryAbout();
  DWORD About(HWND hwnd);

  BOOL QueryConfigure();
  DWORD Configure(HWND hwnd);

  DWORD GetState(LPVOID pv, DWORD dwSize);
  DWORD SetState(LPVOID pv, DWORD dwSize);

  DWORD GetInfo(ICINFO* icinfo, DWORD dwSize);

  DWORD CompressFramesInfo(ICCOMPRESSFRAMES* icif);
  DWORD CompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD CompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD CompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD CompressGetSize(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD Compress(ICCOMPRESS* icinfo, DWORD dwSize);
  DWORD CompressEnd();

  void ConvertRGB24toYUY2(const unsigned char* src, unsigned char* dst, int width, int height);

  DWORD DecompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD DecompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD DecompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD Decompress(ICDECOMPRESS* icinfo, DWORD dwSize);
  DWORD DecompressGetPalette(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut);
  DWORD DecompressEnd();
};

///////////////////////////////////////////////////////////////////////////

VDVideoDecompressorDFSC::VDVideoDecompressorDFSC()
{
  mFormat = 0;
  pinst = 0;
}

VDVideoDecompressorDFSC::~VDVideoDecompressorDFSC() {
  delete pinst;
}

void VDVideoDecompressorDFSC::Init(const void *srcFormat, uint32 srcFormatSize) {
  const VDAVIBitmapInfoHeader *bih = (const VDAVIBitmapInfoHeader *)srcFormat;
  mSrcFormat.assign(bih, srcFormatSize);
  pinst = new DfscInstance();
  BITMAPINFOHEADER f0;
  pinst->DecompressGetFormat((BITMAPINFOHEADER *)srcFormat, &f0);
  mDstFormat.assign((VDAVIBitmapInfoHeader*)&f0,sizeof(f0));
  mFormat = VDBitmapFormatToPixmapFormat((VDAVIBitmapInfoHeader&)f0);
}

bool VDVideoDecompressorDFSC::QueryTargetFormat(int format) {
  return format==mFormat;
}

bool VDVideoDecompressorDFSC::QueryTargetFormat(const void *format) {
  return false;
}

bool VDVideoDecompressorDFSC::SetTargetFormat(int format) {
  return format==0 || format==mFormat;
}

bool VDVideoDecompressorDFSC::SetTargetFormat(const void *format) {
  return false;
}

void VDVideoDecompressorDFSC::Start() {
  if (!mFormat)
    throw MyError("Cannot find compatible target format for video decompression.");
}

void VDVideoDecompressorDFSC::Stop() {
}

void VDVideoDecompressorDFSC::DecompressFrame(void *dst, const void *src, uint32 srcSize, bool keyframe, bool preroll) {
  VDAVIBitmapInfoHeader *pSrcFormat = mSrcFormat.data();
  VDAVIBitmapInfoHeader *pDstFormat = mDstFormat.data();

  DWORD dwFlags = 0;

  if (!keyframe)
    dwFlags |= ICDECOMPRESS_NOTKEYFRAME;

  if (preroll)
    dwFlags |= ICDECOMPRESS_PREROLL;

  DWORD dwOldSize = pSrcFormat->biSizeImage;
  pSrcFormat->biSizeImage = srcSize;
  ICDECOMPRESS ic;
  ic.lpbiInput = (BITMAPINFOHEADER*)pSrcFormat;
  ic.lpbiOutput = (BITMAPINFOHEADER*)pDstFormat;
  ic.lpInput = (void*)src;
  ic.lpOutput = dst;
  ic.dwFlags = dwFlags;
  ic.ckid = 0;
  pinst->Decompress(&ic, sizeof(ic));

  pSrcFormat->biSizeImage = dwOldSize;
}

const void *VDVideoDecompressorDFSC::GetRawCodecHandlePtr() {
  return NULL;
}

const wchar_t *VDVideoDecompressorDFSC::GetName() {
  return L"Internal DebugMode decoder";
}

// ------------ Defines ---------------------------

TCHAR szDescription[] = TEXT("DebugMode FSVFWC (internal use)");
TCHAR szName[] = TEXT("DebugMode FSVFWC (internal use)");

#define VERSION         0x00010000   // 1.0

/********************************************************************
********************************************************************/

DfscInstance::DfscInstance() {
  varFile = 0;
  vars = 0;
  curCmpIndex = 0;
}

BOOL DfscInstance::QueryAbout() {
  return TRUE;
}

DWORD DfscInstance::About(HWND hwnd) {
  MessageBox(NULL, _T(
          "DebugMode FrameServer VFW Codec\nCopyright 2000-2004 Satish Kumar. S\n\nhttp://www.debugmode.com/"),
      _T("About"), MB_OK);
  return ICERR_OK;
}

BOOL DfscInstance::QueryConfigure() {
  return FALSE;
}

DWORD DfscInstance::Configure(HWND hwnd) {
  return ICERR_OK;
}

/********************************************************************
********************************************************************/

DWORD DfscInstance::GetState(LPVOID pv, DWORD dwSize) {
  return 0;
}

DWORD DfscInstance::SetState(LPVOID pv, DWORD dwSize) {
  return 0;
}

DWORD DfscInstance::GetInfo(ICINFO* icinfo, DWORD dwSize) {
  if (icinfo == NULL)
    return sizeof(ICINFO);

  if (dwSize < sizeof(ICINFO))
    return 0;

  icinfo->dwSize = sizeof(ICINFO);
  icinfo->fccType = ICTYPE_VIDEO;
  icinfo->fccHandler = FOURCC_DFSC;
  icinfo->dwFlags = 0;

  icinfo->dwVersion = VERSION;
  icinfo->dwVersionICM = ICVERSION;
  MultiByteToWideChar(CP_ACP, 0, szName, -1, icinfo->szName, sizeof(icinfo->szName) / sizeof(WCHAR));
  MultiByteToWideChar(CP_ACP, 0, szDescription, -1, icinfo->szDescription, sizeof(icinfo->szDescription) / sizeof(WCHAR));

  return sizeof(ICINFO);
}

/********************************************************************
********************************************************************/

DWORD DfscInstance::CompressFramesInfo(ICCOMPRESSFRAMES* icif) {
  return ICERR_OK;
}

DWORD DfscInstance::CompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
  return ICERR_OK;
}

DWORD DfscInstance::CompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
  if (!lpbiOut)
    return sizeof(BITMAPINFOHEADER);

  *lpbiOut = *lpbiIn;
  lpbiOut->biCompression = FOURCC_DFSC;
  return ICERR_OK;
}

DWORD DfscInstance::CompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
  CompressEnd();
  curCmpIndex = 0;
  return ICERR_OK;
}

DWORD DfscInstance::CompressGetSize(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
  return 4;
}

DWORD DfscInstance::Compress(ICCOMPRESS* icinfo, DWORD dwSize) {
  if (icinfo->lpckid)
    *icinfo->lpckid = FOURCC_DFSC;
  *icinfo->lpdwFlags = AVIIF_KEYFRAME;

  *(DWORD*)icinfo->lpOutput = curCmpIndex++;
  icinfo->lpbiOutput->biSizeImage = 4;
  return ICERR_OK;
}

DWORD DfscInstance::CompressEnd() {
  return ICERR_OK;
}

/********************************************************************
********************************************************************/

DWORD DfscInstance::DecompressQuery(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
  if (lpbiIn->biCompression != FOURCC_DFSC)
    return ICERR_BADFORMAT;

  if (lpbiOut) {
    char st[32];
    int outHeight = lpbiOut->biHeight;
    bool isYUY2 = (lpbiIn->biBitCount == 16);
    DWORD inComp = BI_RGB;
    WORD inBitCount = lpbiIn->biBitCount;
    if (isYUY2) {
      outHeight = abs(outHeight);
      inComp = MAKEFOURCC('Y', 'U', 'Y', '2');
      inBitCount = lpbiOut->biBitCount;
    }
    if (inBitCount != lpbiOut->biBitCount ||
        inComp != lpbiOut->biCompression ||
        lpbiIn->biHeight != outHeight ||
        lpbiIn->biWidth != lpbiOut->biWidth ||
        lpbiIn->biPlanes != lpbiOut->biPlanes) {
      sprintf(st, "0x%X - 0x%X\n", lpbiIn->biBitCount, lpbiOut->biBitCount);
      OutputDebugString(st);
      sprintf(st, "0x%X - 0x%X\n", lpbiIn->biHeight, lpbiOut->biHeight);
      OutputDebugString(st);
      sprintf(st, "0x%X - 0x%X\n", lpbiIn->biWidth, lpbiOut->biWidth);
      OutputDebugString(st);
      sprintf(st, "0x%X - 0x%X\n", lpbiIn->biPlanes, lpbiOut->biPlanes);
      OutputDebugString(st);
      return ICERR_BADFORMAT;
    }
  }
  return ICERR_OK;
}

DWORD DfscInstance::DecompressGetFormat(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
  if (lpbiOut == NULL)
    return sizeof(BITMAPINFOHEADER);

  *lpbiOut = *lpbiIn;
  lpbiOut->biSize = sizeof(BITMAPINFOHEADER);
  lpbiOut->biPlanes = 1;
  lpbiOut->biSizeImage = lpbiIn->biWidth * abs(lpbiIn->biHeight) * (lpbiIn->biBitCount / 8);
  lpbiOut->biCompression = BI_RGB;
  if (lpbiIn->biBitCount == 16) {
    lpbiOut->biCompression = MAKEFOURCC('Y', 'U', 'Y', '2');
    lpbiOut->biBitCount = 32;
    lpbiOut->biSizeImage /= 2;
  }

  return ICERR_OK;
}

DWORD DfscInstance::DecompressBegin(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
  int outHeight = lpbiOut->biHeight;
  bool isYUY2 = (lpbiIn->biBitCount == 16);
  DWORD inComp = BI_RGB;
  WORD inBitCount = lpbiIn->biBitCount;

  if (isYUY2) {
    outHeight = abs(outHeight);
    inComp = MAKEFOURCC('Y', 'U', 'Y', '2');
    inBitCount = lpbiOut->biBitCount;
  }
  if (inBitCount != lpbiOut->biBitCount ||
      inComp != lpbiOut->biCompression ||
      lpbiIn->biHeight != outHeight ||
      lpbiIn->biWidth != lpbiOut->biWidth ||
      lpbiIn->biPlanes != lpbiOut->biPlanes) {
    return ICERR_BADFORMAT;
  }
  DecompressEnd();

  decompressing = true;

  return ICERR_OK;
}

DWORD DfscInstance::Decompress(ICDECOMPRESS* icinfo, DWORD dwSize) {
  {
    if (!decompressing) {
      DWORD retval = DecompressBegin(icinfo->lpbiInput, icinfo->lpbiOutput);
      if (retval != ICERR_OK)
        return retval;
    }

    if (!vars) {
      varFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(DfscData), "DfscNetData");
      if (GetLastError() != ERROR_ALREADY_EXISTS) {
        CloseHandle(varFile);
        varFile = NULL;
      }
      if (!varFile) {
        DWORD stream = ((DWORD*)icinfo->lpInput)[1];
        char str[64] = "DfscData";
        _ultoa(stream, str + strlen(str), 10);
        varFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(DfscData), str);
        if (GetLastError() != ERROR_ALREADY_EXISTS) {
          CloseHandle(varFile);
          varFile = NULL;
        }
      }
      if (!varFile)
        return ICERR_ABORT;

      vars = (DfscData*)MapViewOfFile(varFile, FILE_MAP_WRITE, 0, 0, 0);
      if (!vars)
        return ICERR_BADFORMAT;

      videoEncSem = CreateSemaphore(NULL, 1, 1, vars->videoEncSemName);
      videoEncEvent = CreateEvent(NULL, FALSE, FALSE, vars->videoEncEventName);
      videoDecEvent = CreateEvent(NULL, FALSE, FALSE, vars->videoDecEventName);
    }

    WaitForSingleObject(videoEncSem, 10000);
    {
      if (vars->encStatus == 1)         // encoder closed
        return ICERR_ABORT;

      icinfo->lpbiOutput->biSizeImage = vars->videoBytesRead;

      vars->videoFrameIndex = ((DWORD*)icinfo->lpInput)[0];
      SetEvent(videoEncEvent);
      // OutputDebugString("Waiting for video...");

      if (WaitForSingleObject(videoDecEvent, INFINITE) != WAIT_OBJECT_0)
        return ICERR_OK;            // some error.
      if (vars->encStatus == 1)         // encoder closed
        return ICERR_ABORT;
      // OutputDebugString("got video...");
      memcpy(icinfo->lpOutput, ((LPBYTE)vars) + vars->videooffset, vars->videoBytesRead);
    }
    ReleaseSemaphore(videoEncSem, 1, NULL);
  }
  return ICERR_OK;
}

DWORD DfscInstance::DecompressGetPalette(LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
  return ICERR_BADFORMAT;
}

DWORD DfscInstance::DecompressEnd() {
  decompressing = false;
  // return ICERR_OK;
  if (varFile) {
    vars->decStatus = 1;        // decoder closing.

    UnmapViewOfFile(vars);
    CloseHandle(varFile);
    vars = NULL;
    varFile = NULL;
    CloseHandle(videoEncSem);
    CloseHandle(videoEncEvent);
    CloseHandle(videoDecEvent);
  }

  return ICERR_OK;
}
