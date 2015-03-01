#ifndef _f_CVIDEOCOMPRESSOR_H
#define _f_CVIDEOCOMPRESSOR_H

#include <windows.h>
#include <vfw.h>

#include "IVideoCompressor.h"

class CVideoCompressor : public IVideoCompressor {
public:
	virtual ~CVideoCompressor();

	virtual LRESULT About				(HWND hwnd)											;
	virtual LRESULT CompressBegin		(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)		;
	virtual LRESULT CompressEnd			()													;
	virtual LRESULT Configure			(HWND hwnd)											;
	virtual LRESULT Decompress			(ICDECOMPRESS *icd, DWORD cbSize)					;
	virtual LRESULT DecompressBegin		(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)		;
	virtual LRESULT DecompressEnd		()													;
	virtual LRESULT DecompressGetPalette(BITMAPINFOHEADER *lpbiInput, BITMAPINFOHEADER *lpbiOutput)		;
	virtual LRESULT DecompressQuery		(BITMAPINFO *lpbiInput, BITMAPINFO *lpbiOutput)		;
	virtual LRESULT DecompressSetPalette(BITMAPINFOHEADER *lpbiPalette)						;
	virtual LRESULT DecompressExBegin	(ICDECOMPRESSEX *icdex, DWORD cbSize)				;
	virtual LRESULT DecompressExEnd		()													;
	virtual LRESULT Draw				(ICDRAW *icdraw, DWORD cbSize)						;
	virtual LRESULT DrawBegin			(ICDRAWBEGIN *icdrwBgn, DWORD cbSize)				;
	virtual LRESULT DrawChangePalette	(BITMAPINFO *lpbiInput)								;
	virtual LRESULT DrawEnd				()													;
	virtual LRESULT DrawFlush			()													;
	virtual LRESULT DrawGetPalette		()													;
	virtual LRESULT DrawGetTime			(DWORD *lplTime)									;
	virtual LRESULT DrawQuery			(BITMAPINFO *lpbiInput)								;
	virtual LRESULT DrawRealize			(HDC hdc, BOOL fBackground)							;
	virtual LRESULT DrawRenderBuffer	()													;
	virtual LRESULT DrawSetTime			(DWORD lpTime)										;
	virtual LRESULT DrawStart			()													;
	virtual LRESULT DrawStartPlay		(DWORD lFrom, DWORD lTo)							;
	virtual LRESULT DrawStop			()													;
	virtual LRESULT DrawStopPlay		()													;
	virtual LRESULT DrawSuggestFormat	(ICDRAWSUGGEST *icdrwSuggest, DWORD cbSize)			;
	virtual LRESULT DrawWindow			(RECT *prc)											;
	virtual LRESULT Get					(LPVOID pv, DWORD cbSize)							;
	virtual LRESULT GetBuffersWanted	(DWORD *lpdwBuffers)								;
	virtual LRESULT GetDefaultKeyFrameRate(DWORD *lpdwICValue)								;
	virtual LRESULT GetDefaultQuality	(DWORD *lpdwICValue)								;
	virtual LRESULT GetQuality			(DWORD *lpdwICValue)								;
	virtual LRESULT GetState			(LPVOID pv, DWORD cbSize)							;
	virtual LRESULT SetStatusProc		(ICSETSTATUSPROC *icsetstatusProc, DWORD cbSize)	;
	virtual LRESULT SetQuality			(DWORD *lpdwICValue)								;
	virtual LRESULT SetState			(LPVOID pv, DWORD cbSize)							;

	virtual LRESULT Default				(DWORD dwDriverID, HDRVR hDriver, UINT uiMessage, LPARAM lParam1, LPARAM lParam2);
};

#endif
