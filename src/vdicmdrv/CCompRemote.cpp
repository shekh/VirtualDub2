#include <crtdbg.h>

#include "resource.h"
#include "vdserver.h"

#include "CCompRemote.h"

extern HINSTANCE g_hInst;

static HINSTANCE g_hInstVDSVRLNK;
static FARPROC g_fpGetDubServerInterface;
static IVDubServerLink *g_ivdsl;

CCompRemoteDriver videoDriverRemote;

CCompRemoteDriver::~CCompRemoteDriver() {
}

BOOL CCompRemoteDriver::Load(HDRVR hDriver) {
	if (g_hInstVDSVRLNK = LoadLibrary("vdsvrlnk.dll")) {
		if (g_fpGetDubServerInterface = GetProcAddress(g_hInstVDSVRLNK, "GetDubServerInterface")) {
			g_ivdsl = ((IVDubServerLink *(__cdecl *)())g_fpGetDubServerInterface)();

			return TRUE;
		}
		FreeLibrary(g_hInstVDSVRLNK);
		g_hInstVDSVRLNK = NULL;
	}
	return FALSE;
}

void CCompRemoteDriver::Free(HDRVR hDriver) {
	if (g_hInstVDSVRLNK) {
		FreeLibrary(g_hInstVDSVRLNK);
		g_hInstVDSVRLNK = NULL;
	}
}

DWORD CCompRemoteDriver::Open(HDRVR hDriver, char *szDescription, LPVIDEO_OPEN_PARMS lpVideoOpenParms) {
	return (DWORD)new CCompRemote();
}

void CCompRemoteDriver::Disable(HDRVR hDriver) {
}

void CCompRemoteDriver::Enable(HDRVR hDriver) {
}

LPARAM CCompRemoteDriver::Default(DWORD dwDriverID, HDRVR hDriver, UINT uiMessage, LPARAM lParam1, LPARAM lParam2) {
	return DefDriverProc(dwDriverID, hDriver, uiMessage, lParam1, lParam2);
}

/////////////////////////////////////////////////////////////

CCompRemote::CCompRemote() {
	ivdac = NULL;
}

CCompRemote::~CCompRemote() {
	if (ivdac) g_ivdsl->FrameServerDisconnect(ivdac);
}

BOOL CCompRemote::AboutProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_COMMAND:
			if (LOWORD(lParam) == IDCANCEL) {
				EndDialog(hDlg, 0);
				return TRUE;
			}
			break;
	}
	return FALSE;
}

LRESULT CCompRemote::About(HWND hwnd) {
	if (hwnd != (HWND)-1L) {
		DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUT), hwnd, (DLGPROC)CCompRemote::AboutProc);
	}

	return ICERR_OK;
}

LRESULT CCompRemote::Compress(ICCOMPRESS *icc, DWORD cbSize) {
	return ICERR_UNSUPPORTED;
}

LRESULT CCompRemote::CompressFramesInfo(ICCOMPRESSFRAMES *icf, DWORD cbSize) {
	return ICERR_UNSUPPORTED;
}

LRESULT CCompRemote::CompressGetFormat(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput) {
	return ICERR_UNSUPPORTED;
}

LRESULT CCompRemote::CompressGetSize(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput) {
	return 16;
}

LRESULT CCompRemote::CompressQuery(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput) {
	return ICERR_UNSUPPORTED;
}

LRESULT CCompRemote::DecompressGetFormat(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput) {
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
	bmihOutput->biSizeImage		= ((bmihInput->biWidth*3+3)&-4)*abs(bmihInput->biHeight);
	bmihOutput->biXPelsPerMeter	= bmihInput->biXPelsPerMeter;
	bmihOutput->biYPelsPerMeter	= bmihInput->biYPelsPerMeter;
	bmihOutput->biClrUsed		= 0;
	bmihOutput->biClrImportant	= 0;

	return ICERR_OK;
}

static void *DibXY(BITMAPINFOHEADER *bih, void *ptr, long x, long y) {
	return (void *)(
			(char *)ptr
			+ x*(bih->biBitCount/8)
			+ y*4*((bih->biBitCount*bih->biWidth+31)/32)
		);
}

LRESULT CCompRemote::DecompressBegin(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput) {
	LRESULT lres;
	BITMAPINFOHEADER bmih;

	if (ICERR_OK != (lres = DecompressQuery(lpbiInput, lpbiOutput)))
		return lres;

	if (ivdac) g_ivdsl->FrameServerDisconnect(ivdac);

	_RPT0(0,"Attempting to connect to frameserver...\n");

	ivdac = g_ivdsl->FrameServerConnect("VCM");

	if (!ivdac) return ICERR_BADFORMAT;

	_RPT0(0,"Connected.\n");

	if ((lres=ivdac->readFormat(&bmih, FALSE))<0 || bmih.biWidth != lpbiOutput->bmiHeader.biWidth || bmih.biHeight != lpbiOutput->bmiHeader.biHeight) {

		if (lres) _RPT1(0,"Failed to read format (%d)\n", lres);
		else _RPT4(0,"Formats not same: %dx%d vs. %dx%d\n",
			bmih.biWidth,
			bmih.biHeight,
			lpbiOutput->bmiHeader.biWidth,
			lpbiOutput->bmiHeader.biHeight
			);

		g_ivdsl->FrameServerDisconnect(ivdac);
		ivdac = NULL;
		return ICERR_BADFORMAT;
	}

	dwWidth		= bmih.biWidth;
	dwHeight	= bmih.biHeight;

	return ICERR_OK;
}

LRESULT CCompRemote::DecompressEnd() {
	if (ivdac) {
		g_ivdsl->FrameServerDisconnect(ivdac);
		ivdac = NULL;
	}

	return ICERR_OK;
}

LRESULT CCompRemote::DecompressExBegin(ICDECOMPRESSEX *icdex, DWORD cbSize) {
	return DecompressBegin((BITMAPINFO *)icdex->lpbiSrc, (BITMAPINFO *)icdex->lpbiDst);
}

LRESULT CCompRemote::DecompressExEnd() {
	return DecompressEnd();
}

LRESULT CCompRemote::DecompressEx(ICDECOMPRESSEX *icdex, DWORD cbSize) {
	BITMAPINFOHEADER *bmihInput		= icdex->lpbiSrc;
	BITMAPINFOHEADER *bmihOutput	= icdex->lpbiDst;
	LRESULT res;
	long w0s, h0s, w0d, h0d;

	/////////////////////

#if 0
	_RPT1(0,"\tdwFlags: %p\n",icdex->dwFlags);
	_RPT1(0,"\tlpbiSrc: %p\n",icdex->lpbiSrc);
	_RPT1(0,"\t\tbiSize:          %ld\n", icdex->lpbiSrc->biSize);
	_RPT1(0,"\t\tbiWidth:         %ld\n", icdex->lpbiSrc->biWidth);
	_RPT1(0,"\t\tbiHeight:        %ld\n", icdex->lpbiSrc->biHeight);
	_RPT1(0,"\t\tbiPlanes:        %ld\n", icdex->lpbiSrc->biPlanes);
	_RPT1(0,"\t\tbiBitCount:      %ld\n", icdex->lpbiSrc->biBitCount);
	_RPT1(0,"\t\tbiCompression:   %ld\n", icdex->lpbiSrc->biCompression);
	_RPT1(0,"\t\tbiSizeImage:     %ld\n", icdex->lpbiSrc->biSizeImage);
	_RPT1(0,"\t\tbiXPelsPerMeter: %ld\n", icdex->lpbiSrc->biXPelsPerMeter);
	_RPT1(0,"\t\tbiYPelsPerMeter: %ld\n", icdex->lpbiSrc->biYPelsPerMeter);
	_RPT1(0,"\t\tbiClrUsed:       %ld\n", icdex->lpbiSrc->biClrUsed);
	_RPT1(0,"\t\tbiClrImportant:  %ld\n", icdex->lpbiSrc->biClrImportant);
	_RPT1(0,"\tlpSrc:   %p\n",icdex->lpSrc);
	_RPT1(0,"\tlpbiDst: %p\n",icdex->lpbiDst);
	_RPT1(0,"\t\tbiSize:          %ld\n", icdex->lpbiDst->biSize);
	_RPT1(0,"\t\tbiWidth:         %ld\n", icdex->lpbiDst->biWidth);
	_RPT1(0,"\t\tbiHeight:        %ld\n", icdex->lpbiDst->biHeight);
	_RPT1(0,"\t\tbiPlanes:        %ld\n", icdex->lpbiDst->biPlanes);
	_RPT1(0,"\t\tbiBitCount:      %ld\n", icdex->lpbiDst->biBitCount);
	_RPT1(0,"\t\tbiCompression:   %ld\n", icdex->lpbiDst->biCompression);
	_RPT1(0,"\t\tbiSizeImage:     %ld\n", icdex->lpbiDst->biSizeImage);
	_RPT1(0,"\t\tbiXPelsPerMeter: %ld\n", icdex->lpbiDst->biXPelsPerMeter);
	_RPT1(0,"\t\tbiYPelsPerMeter: %ld\n", icdex->lpbiDst->biYPelsPerMeter);
	_RPT1(0,"\t\tbiClrUsed:       %ld\n", icdex->lpbiDst->biClrUsed);
	_RPT1(0,"\t\tbiClrImportant:  %ld\n", icdex->lpbiDst->biClrImportant);
	_RPT1(0,"\tlpDst:   %p\n",icdex->lpDst);
	_RPT4(0,"\tSource:  (%d,%d), size %dx%d\n", icdex->xSrc, icdex->ySrc, icdex->dxSrc, icdex->dySrc);
	_RPT4(0,"\tDest:    (%d,%d), size %dx%d\n", icdex->xDst, icdex->yDst, icdex->dxDst, icdex->dyDst);
	return ICERR_OK;
#endif

	/////////////////////

	if (!ivdac) {
		if (ICERR_OK != (res = DecompressExBegin(icdex, cbSize)))
			return res;
	} else {
		if (ICERR_OK != (res = DecompressExQuery(icdex, cbSize)))
			return res;
	}

	if (icdex->dwFlags & (ICDECOMPRESS_UPDATE))
		return ICERR_BADPARAM;

	w0s = icdex->dxSrc==-1 ? bmihInput->biWidth  : icdex->dxSrc;
	h0s = icdex->dySrc==-1 ? bmihInput->biHeight : icdex->dySrc;
	w0d = icdex->dxDst==-1 ? bmihOutput->biWidth  : icdex->dxDst;
	h0d = icdex->dyDst==-1 ? bmihOutput->biHeight : icdex->dyDst;

	if (w0s != w0d || h0s != h0d)
		return ICERR_BADPARAM;

	if (w0s != bmihInput->biWidth || h0s != bmihInput->biHeight)
		return ICERR_BADPARAM;

	if (icdex->xSrc != 0 || icdex->ySrc != 0)
		return ICERR_OK;

	if (icdex->xDst != 0 || icdex->yDst != 0)
		return ICERR_OK;

	if (VDSRVERR_OK != ivdac->readVideo(*(long *)icdex->lpSrc, icdex->lpDst))
		return ICERR_ERROR;

	return ICERR_OK;
}

LRESULT CCompRemote::DecompressExQuery(ICDECOMPRESSEX *icdex, DWORD cbSize) {
	BITMAPINFOHEADER *bmihInput		= icdex->lpbiSrc;

#if 1
	_RPT1(0,"\tlpbiSrc: %p\n",icdex->lpbiSrc);
	_RPT1(0,"\t\tbiSize:          %ld\n", icdex->lpbiSrc->biSize);
	_RPT1(0,"\t\tbiWidth:         %ld\n", icdex->lpbiSrc->biWidth);
	_RPT1(0,"\t\tbiHeight:        %ld\n", icdex->lpbiSrc->biHeight);
	_RPT1(0,"\t\tbiPlanes:        %ld\n", icdex->lpbiSrc->biPlanes);
	_RPT1(0,"\t\tbiBitCount:      %ld\n", icdex->lpbiSrc->biBitCount);
	_RPT1(0,"\t\tbiCompression:   %ld\n", icdex->lpbiSrc->biCompression);
	_RPT1(0,"\t\tbiSizeImage:     %ld\n", icdex->lpbiSrc->biSizeImage);
	_RPT1(0,"\t\tbiXPelsPerMeter: %ld\n", icdex->lpbiSrc->biXPelsPerMeter);
	_RPT1(0,"\t\tbiYPelsPerMeter: %ld\n", icdex->lpbiSrc->biYPelsPerMeter);
	_RPT1(0,"\t\tbiClrUsed:       %ld\n", icdex->lpbiSrc->biClrUsed);
	_RPT1(0,"\t\tbiClrImportant:  %ld\n", icdex->lpbiSrc->biClrImportant);
#endif

	if (bmihInput->biCompression != 'TSDV') return ICERR_BADFORMAT;

	if (icdex->lpbiDst) {
		BITMAPINFOHEADER *bmihOutput	= icdex->lpbiDst;

#if 1
		_RPT1(0,"\tlpbiDst: %p\n",icdex->lpbiDst);
		_RPT1(0,"\t\tbiSize:          %ld\n", icdex->lpbiDst->biSize);
		_RPT1(0,"\t\tbiWidth:         %ld\n", icdex->lpbiDst->biWidth);
		_RPT1(0,"\t\tbiHeight:        %ld\n", icdex->lpbiDst->biHeight);
		_RPT1(0,"\t\tbiPlanes:        %ld\n", icdex->lpbiDst->biPlanes);
		_RPT1(0,"\t\tbiBitCount:      %ld\n", icdex->lpbiDst->biBitCount);
		_RPT1(0,"\t\tbiCompression:   %ld\n", icdex->lpbiDst->biCompression);
		_RPT1(0,"\t\tbiSizeImage:     %ld\n", icdex->lpbiDst->biSizeImage);
		_RPT1(0,"\t\tbiXPelsPerMeter: %ld\n", icdex->lpbiDst->biXPelsPerMeter);
		_RPT1(0,"\t\tbiYPelsPerMeter: %ld\n", icdex->lpbiDst->biYPelsPerMeter);
		_RPT1(0,"\t\tbiClrUsed:       %ld\n", icdex->lpbiDst->biClrUsed);
		_RPT1(0,"\t\tbiClrImportant:  %ld\n", icdex->lpbiDst->biClrImportant);
#endif

		if (bmihOutput->biPlanes != 1) return ICERR_BADFORMAT;
		if (bmihOutput->biCompression != BI_RGB) return ICERR_BADFORMAT;
		if (bmihOutput->biBitCount != 24) return ICERR_BADFORMAT;
		if (bmihOutput->biWidth != bmihInput->biWidth) return ICERR_BADFORMAT;
		if (bmihOutput->biHeight != bmihInput->biHeight) return ICERR_BADFORMAT;
	}

	_RPT0(0,"--------------Accepted!\n");

	return ICERR_OK;
}

LRESULT CCompRemote::GetInfo(ICINFO *lpicinfo, DWORD cbSize) {
	if (cbSize < sizeof(ICINFO)) return 0;

	lpicinfo->dwSize		= sizeof(ICINFO);
	lpicinfo->fccType		= ICTYPE_VIDEO;
	lpicinfo->fccHandler	= 'TSDV';
	lpicinfo->dwFlags		= 0;
	lpicinfo->dwVersion		= 1;
	lpicinfo->dwVersionICM	= ICVERSION;
	wcscpy(lpicinfo->szName, L"vdremote");
	wcscpy(lpicinfo->szDescription, L"VirtualDub remote frameclient");

	return sizeof(ICINFO);
}

