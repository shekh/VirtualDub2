#ifndef _f_CCOMPREMOTE_H
#define _f_CCOMPREMOTE_H

#include "IVideoDriver.h"
#include "CVideoCompressor.h"

class IVDubAnimConnection;
class IVDubServerLink;

class CCompRemoteDriver : public IVideoDriver {
public:
	virtual ~CCompRemoteDriver();

	BOOL	Load(HDRVR hDriver);
	void	Free(HDRVR hDriver);
	DWORD	Open(HDRVR hDriver, char *szDescription, LPVIDEO_OPEN_PARMS lpVideoOpenParms);
	void	Disable(HDRVR hDriver);
	void	Enable(HDRVR hDriver);
	LRESULT	Default(DWORD dwDriverID, HDRVR hDriver, UINT uiMessage, LPARAM lParam1, LPARAM lParam2);
};

class CCompRemote : public CVideoCompressor {
private:
	IVDubAnimConnection *ivdac;
	DWORD dwWidth, dwHeight;

	static BOOL AboutProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

public:
	CCompRemote();
	~CCompRemote();

	LRESULT About(HWND hwnd);
	LRESULT Compress			(ICCOMPRESS *icc, DWORD cbSize);
	LRESULT CompressFramesInfo	(ICCOMPRESSFRAMES *icf, DWORD cbSize);
	LRESULT CompressGetFormat	(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput);
	LRESULT CompressGetSize		(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput);
	LRESULT CompressQuery		(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput);
	LRESULT DecompressBegin		(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput);
	LRESULT DecompressEnd		();
	LRESULT DecompressExBegin	(ICDECOMPRESSEX *icdex, DWORD cbSize);
	LRESULT DecompressExEnd		();
	LRESULT DecompressGetFormat	(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput);
	LRESULT DecompressEx		(ICDECOMPRESSEX *icdex, DWORD cbSize);
	LRESULT DecompressExQuery	(ICDECOMPRESSEX *icdex, DWORD cbSize);
	LRESULT GetInfo				(ICINFO *lpicinfo, DWORD cbSize);
};

extern CCompRemoteDriver videoDriverRemote;

#endif
