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

#include <windows.h>

#include <map>

#include "vdserver.h"

#include "AudioSource.h"
#include "VideoSource.h"
#include <vd2/system/error.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/strutil.h>
#include <vd2/Dita/services.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "FrameSubset.h"
#include "FilterFrameVideoSource.h"

#include "filters.h"
#include "dub.h"
#include "DubUtils.h"
#include "gui.h"
#include "audio.h"
#include "command.h"
#include "prefs.h"
#include "project.h"
#include "server.h"
#include "resource.h"
#include "uiframe.h"

extern HINSTANCE g_hInst;
extern HWND g_hWnd;

extern VDProject *g_project;

extern wchar_t g_szInputAVIFile[MAX_PATH];

extern bool VDPreferencesGetFilterAccelEnabled();
extern sint32 VDPreferencesGetFilterThreadCount();

///////////////////////////////////////////////////////////////////////////

enum {
	kFileDialog_Signpost		= 'sign'
};

//////////////////////////////////////////////////////////////////////////

class FrameserverSession {
private:
	HANDLE hArena;

public:
	FrameserverSession *next, *prev;

	char *arena;
	long arena_size;
	DWORD id;

	FrameserverSession();
	DWORD Init(LONG arena_size, DWORD session_id);
	~FrameserverSession();
};

FrameserverSession::FrameserverSession() {
	next = prev = NULL;
	hArena = INVALID_HANDLE_VALUE;
}

DWORD FrameserverSession::Init(LONG arena_size, DWORD session_id) {
	char buf[16];

	wsprintf(buf, "VDUBF%08lx", session_id);

	if (INVALID_HANDLE_VALUE == (hArena = OpenFileMapping(FILE_MAP_WRITE, FALSE, buf)))
		return NULL;

	if (!(arena = (char *)MapViewOfFile(hArena, FILE_MAP_WRITE, 0, 0, arena_size)))
		return NULL;

	this->id = (DWORD)this;
	this->arena_size = arena_size;

	return this->id;
}

FrameserverSession::~FrameserverSession() {
	if (arena) UnmapViewOfFile(arena);
	if (hArena != INVALID_HANDLE_VALUE) CloseHandle(hArena);
}

///////////////////////////////////

class Frameserver : public vdrefcounted<IVDUIFrameClient> {
private:
	DubOptions			*opt;
	HWND				hwnd;
	AudioSource			*const aSrc;
	IVDVideoSource		*const vSrc;

	bool			mbExit;

	DubAudioStreamInfo	aInfo;
	DubVideoStreamInfo	vInfo;
	FrameSubset			audioset;
	long				lVideoSamples;
	long				lAudioSamples;
	VDRenderFrameMap	mVideoFrameMap;
	vdrefptr<VDFilterFrameVideoSource>	mpVideoFrameSource;
	VDPixmapLayout		mFrameLayout;
	uint32				mFrameSize;

	long			lRequestCount, lFrameCount, lAudioSegCount;

	HWND			hwndStatus;

	typedef std::map<uint32, FrameserverSession *> tSessions;
	tSessions	mSessions;

	char *lpszFsname;

	VDUIFrame		*mpUIFrame;

	FrameSubset		mSubset;

public:
	Frameserver(IVDVideoSource *video, AudioSource *audio, HWND hwndParent, DubOptions *xopt, const FrameSubset& server);
	~Frameserver();

	void Detach();
	LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void Go(IVDubServerLink *ivdsl, char *name);

	static INT_PTR APIENTRY StatusDlgProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	INT_PTR APIENTRY StatusDlgProc2( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	FrameserverSession *SessionLookup(LPARAM lParam);
	LRESULT SessionOpen(LPARAM mmapID, WPARAM arena_len);
	LRESULT SessionClose(LPARAM lParam);
	LRESULT SessionStreamInfo(LPARAM lParam, WPARAM stream);
	LRESULT SessionFormat(LPARAM lParam, WPARAM stream);
	LRESULT SessionFrame(LPARAM lParam, WPARAM sample);
	LRESULT SessionAudio(LPARAM lParam, WPARAM lStart);
	LRESULT SessionAudioInfo(LPARAM lParam, WPARAM lStart);
};

Frameserver::Frameserver(IVDVideoSource *video, AudioSource *audio, HWND hwndParent, DubOptions *xopt, const FrameSubset& subset)
	: aSrc(audio)
	, vSrc(video)
	, mSubset(subset)
{
	opt				= xopt;
	hwnd			= hwndParent;

	lFrameCount = lRequestCount = lAudioSegCount = 0;
}

Frameserver::~Frameserver() {
	for(tSessions::iterator it(mSessions.begin()), itEnd(mSessions.end()); it!=itEnd; ++it) {
		FrameserverSession *pSession = (*it).second;

		delete pSession;
	}

	mSessions.clear();

	filters.DeinitFilters();
	filters.DeallocateBuffers();
}

void Frameserver::Detach() {}

void Frameserver::Go(IVDubServerLink *ivdsl, char *name) {
	int server_index = -1;

	lpszFsname = name;
	
	// prepare the sources...
	if (!vSrc->setTargetFormat(g_dubOpts.video.mInputFormat))
		if (!vSrc->setTargetFormat(nsVDPixmap::kPixFormat_XRGB8888))
			if (!vSrc->setTargetFormat(nsVDPixmap::kPixFormat_RGB888))
				if (!vSrc->setTargetFormat(nsVDPixmap::kPixFormat_XRGB1555))
					if (!vSrc->setTargetFormat(nsVDPixmap::kPixFormat_Pal8))
						throw MyError("The decompression codec cannot decompress to an RGB format. This is very unusual. Check that any \"Force YUY2\" options are not enabled in the codec's properties.");

	IVDStreamSource *pVSS = vSrc->asStream();
	FrameSubset videoset(mSubset);

	const VDFraction frameRateTimeline(g_project->GetTimelineFrameRate());
	VDPosition startFrame;
	VDPosition endFrame;
	VDConvertSelectionTimesToFrames(*opt, mSubset, frameRateTimeline, startFrame, endFrame);
	InitVideoStreamValuesStatic(vInfo, vSrc, aSrc, opt, &mSubset, &startFrame, &endFrame);

	const VDPixmap& px = vSrc->getTargetFormat();

	const VDFraction& srcFAR = vSrc->getPixelAspectRatio();
	filters.prepareLinearChain(&g_filterChain, px.w, px.h, px.format, vInfo.mFrameRatePreFilter, -1, srcFAR);

	mpVideoFrameSource = new VDFilterFrameVideoSource;
	mpVideoFrameSource->Init(vSrc, filters.GetInputLayout());

	filters.SetVisualAccelDebugEnabled(false);
	filters.SetAccelEnabled(VDPreferencesGetFilterAccelEnabled());
	filters.SetAsyncThreadCount(VDPreferencesGetFilterThreadCount());
	filters.initLinearChain(NULL, 0, &g_filterChain, mpVideoFrameSource, px.w, px.h, px.format, px.palette, vInfo.mFrameRatePreFilter, -1, srcFAR);

	filters.ReadyFilters();

	InitVideoStreamValuesStatic2(vInfo, opt, &filters, frameRateTimeline);

	InitAudioStreamValuesStatic(aInfo, aSrc, opt);

	vdfastvector<IVDVideoSource *> vsrcs(1, vSrc);
	mVideoFrameMap.Init(vsrcs, vInfo.start_src, vInfo.mFrameRateTimeline / vInfo.mFrameRate, &mSubset, vInfo.end_dst, opt->video.mbUseSmartRendering, opt->video.mode == DubVideoOptions::M_NONE, opt->video.mbPreserveEmptyFrames, &filters, false, false);

	if (opt->audio.fEndAudio)
		videoset.deleteRange(endFrame, videoset.getTotalFrames());

	if (opt->audio.fStartAudio)
		videoset.deleteRange(0, startFrame);

	VDDEBUG("Video subset:\n");
	videoset.dump();

	if (aSrc)
		AudioTranslateVideoSubset(audioset, videoset, vInfo.mFrameRateTimeline, aSrc->getWaveFormat(), !opt->audio.fEndAudio && (videoset.empty() || videoset.back().end() == pVSS->getEnd()) ? aSrc->getEnd() : 0, NULL);

	VDDEBUG("Audio subset:\n");
	audioset.dump();

	if (aSrc) {
		audioset.offset(aSrc->msToSamples(-opt->audio.offset));
		lAudioSamples = VDClampToUint32(audioset.getTotalFrames());
	} else
		lAudioSamples = 0;

	lVideoSamples = VDClampToUint32(mVideoFrameMap.size());

	vSrc->streamBegin(true, false);

	const VDPixmapLayout& outputLayout = filters.GetOutputLayout();

	mFrameSize = VDPixmapCreateLinearLayout(mFrameLayout, nsVDPixmap::kPixFormat_RGB888, outputLayout.w, outputLayout.h, 4);
	VDPixmapLayoutFlipV(mFrameLayout);

	if (aSrc)
		aSrc->streamBegin(true, false);

	// usurp the window

	VDUIFrame *pFrame = VDUIFrame::GetFrame(hwnd);
	mpUIFrame = pFrame;
	pFrame->Attach(this);

	guiSetTitle(hwnd, IDS_TITLE_FRAMESERVER);

	// create dialog box

	mbExit = false;

	if (hwndStatus = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_SERVER), hwnd, Frameserver::StatusDlgProc, (LPARAM)this)) {

		// hide the main window

		ShowWindow(hwnd, SW_HIDE);

		// create the frameserver

		server_index = ivdsl->CreateFrameServer(name, hwnd);

		if (server_index>=0) {

			// kick us into high priority

			SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

			// enter window loop

			{
				MSG msg;

				while(!mbExit) {
					BOOL result = GetMessage(&msg, NULL, 0, 0);

					if (result == (BOOL)-1)
						break;

					if (!result) {
						PostQuitMessage(msg.wParam);
						break;
					}

					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}

			// return to normal priority

			SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

			ivdsl->DestroyFrameServer(server_index);
		}

		if (IsWindow(hwndStatus)) DestroyWindow(hwndStatus);

		// show the main window

		ShowWindow(hwnd, SW_SHOW);
	}

	// unsubclass
	pFrame->Detach();

	if (vSrc) {
		IVDStreamSource *pVSS = vSrc->asStream();
		pVSS->streamEnd();
	}

	if (server_index<0) throw MyError("Couldn't create frameserver");
}

LRESULT Frameserver::WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {

	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;

	case WM_DESTROY:                  // message: window being destroyed
		mbExit = true;
		break;

	case VDSRVM_BIGGEST:
		{
			uint32 size = sizeof(AVISTREAMINFO);

			if (vSrc) {
				if (size < sizeof(BITMAPINFOHEADER))
					size = sizeof(BITMAPINFOHEADER);

				if (size < mFrameSize)
					size = mFrameSize;
			}

			if (aSrc) {
				uint32 dataRate = aSrc->getWaveFormat()->mDataRate;
				if (size < dataRate)
					size = dataRate;

				uint32  formatSize = aSrc->getFormatLen();
				if (size < formatSize)
					size = aSrc->getFormatLen();
			}

			if (size < 65536)
				size = 65536;

			VDDEBUG("VDSRVM_BIGGEST: allocate a frame of size %ld bytes\n", size);
			return size;
		}

	case VDSRVM_OPEN:
		++lRequestCount;
		VDDEBUG("VDSRVM_OPEN(arena size %ld, mmap ID %08lx)\n", wParam, lParam);
		return SessionOpen(lParam, wParam);

	case VDSRVM_CLOSE:
		++lRequestCount;
		VDDEBUG("[session %08lx] VDSRVM_CLOSE()\n", lParam);
		return SessionClose(lParam);

	case VDSRVM_REQ_STREAMINFO:
		++lRequestCount;
		VDDEBUG("[session %08lx] VDSRVM_REQ_STREAMINFO(stream %d)\n", lParam, wParam);
		return SessionStreamInfo(lParam, wParam);

	case VDSRVM_REQ_FORMAT:
		++lRequestCount;
		VDDEBUG("[session %08lx] VDSRVM_REQ_FORMAT(stream %d)\n", lParam, wParam);
		return SessionFormat(lParam, wParam);

	case VDSRVM_REQ_FRAME:
		++lFrameCount;
		VDDEBUG("[session %08lx] VDSRVM_REQ_FRAME(sample %ld)\n", lParam, wParam);
		return SessionFrame(lParam, wParam);

	case VDSRVM_REQ_AUDIO:
		++lAudioSegCount;
		return SessionAudio(lParam, wParam);

	case VDSRVM_REQ_AUDIOINFO:
		++lAudioSegCount;
		VDDEBUG("[session %08lx] VDSRVM_REQ_AUDIOINFO(sample %ld)\n", lParam, wParam);
		return SessionAudioInfo(lParam, wParam);

	default:
		return mpUIFrame->DefProc(hWnd, message, wParam, lParam);
    }
    return (0);
}

///////////////////////

INT_PTR CALLBACK Frameserver::StatusDlgProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	return ((Frameserver *)GetWindowLongPtr(hWnd, DWLP_USER))->StatusDlgProc2(hWnd, message, wParam, lParam);
}

INT_PTR CALLBACK Frameserver::StatusDlgProc2( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
	case WM_INITDIALOG:
		SetWindowLongPtr(hWnd, DWLP_USER, lParam);
		SetDlgItemText(hWnd, IDC_STATIC_FSNAME, ((Frameserver *)lParam)->lpszFsname);
		SetTimer(hWnd,1,1000,NULL);

		{
			HKEY hkey;
			HIC hic;
			BOOL fAVIFile = FALSE, fVCM = FALSE;

			if (RegOpenKeyEx(HKEY_CLASSES_ROOT, "CLSID\\{894288E0-0948-11D2-8109-004845000EB5}\\InProcServer32\\AVIFile", 0, KEY_QUERY_VALUE, &hkey)==ERROR_SUCCESS) {
				RegCloseKey(hkey);
				fAVIFile = TRUE;
			}

			if (hic = ICOpen('CDIV', 'TSDV', ICMODE_DECOMPRESS)) {
				ICClose(hic);
				fVCM = TRUE;
			}

			if (fAVIFile && fVCM)
				SetDlgItemText(hWnd, IDC_STATIC_FCINSTALLED, "AVIFile and VCM");
			else if (fAVIFile)
				SetDlgItemText(hWnd, IDC_STATIC_FCINSTALLED, "AVIFile only");
			else if (fVCM)
				SetDlgItemText(hWnd, IDC_STATIC_FCINSTALLED, "VCM only");
		}
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) != IDOK) break;
	case WM_CLOSE:
		mbExit = true;
		return TRUE;
	case WM_TIMER:
		SetDlgItemInt(hWnd, IDC_STATIC_REQCOUNT, lRequestCount, FALSE);
		SetDlgItemInt(hWnd, IDC_STATIC_FRAMECNT, lFrameCount, FALSE);
		SetDlgItemInt(hWnd, IDC_STATIC_AUDIOSEGS, lAudioSegCount, FALSE);
		return TRUE;
    }
    return FALSE;
}

////////////////////////////////////////////////////////

FrameserverSession *Frameserver::SessionLookup(LPARAM lParam) {
	tSessions::const_iterator it(mSessions.find(lParam));

	if (it != mSessions.end())
		return (*it).second;

	VDDEBUG("Session lookup failed on %08lx\n", lParam);

	return NULL;
}

LRESULT Frameserver::SessionOpen(LPARAM mmapID, WPARAM arena_len) {
	FrameserverSession *fs;
	DWORD id;

	if (fs = new FrameserverSession()) {
		if (id = fs->Init(arena_len, mmapID)) {
			mSessions[id] = fs;
			return id;
		}
		delete fs;
	}

	return NULL;
}

LRESULT Frameserver::SessionClose(LPARAM lParam) {
	FrameserverSession *fs = SessionLookup(lParam);

	if (!fs) return VDSRVERR_BADSESSION;

	delete fs;

	mSessions.erase(lParam);

	return VDSRVERR_OK;
}

LRESULT Frameserver::SessionStreamInfo(LPARAM lParam, WPARAM stream) {
	FrameserverSession *fs = SessionLookup(lParam);

	if (!fs) return VDSRVERR_BADSESSION;

	if (stream<0 || stream>2) return VDSRVERR_NOSTREAM;

	if (stream==0) {
		AVISTREAMINFO *lpasi = (AVISTREAMINFO *)(fs->arena+8);

		if (!vSrc) return VDSRVERR_NOSTREAM;

		*(long *)(fs->arena+0) = 0;										//vSrc->lSampleFirst;
		*(long *)(fs->arena+4) = lVideoSamples;			//vSrc->lSampleLast;

		IVDStreamSource *pVSS = vSrc->asStream();

		memset(lpasi, 0, sizeof *lpasi);
		memcpy(lpasi, &pVSS->getStreamInfo(), sizeof(VDAVIStreamInfo));

		lpasi->fccHandler	= ' BID';
		lpasi->dwLength		= *(long *)(fs->arena+4);
		lpasi->dwRate		= vInfo.mFrameRate.getHi();
		lpasi->dwScale		= vInfo.mFrameRate.getLo();

		const VDPixmapLayout& output = filters.GetOutputLayout();
		SetRect(&lpasi->rcFrame, 0, 0, output.w, output.h);

		lpasi->dwSuggestedBufferSize = mFrameSize;

	} else {
		if (!aSrc) return VDSRVERR_NOSTREAM;

		*(long *)(fs->arena+0) = 0;
		*(long *)(fs->arena+4) = lAudioSamples;
		memcpy(fs->arena+8, &aSrc->getStreamInfo(), sizeof(AVISTREAMINFO));

		((AVISTREAMINFO *)(fs->arena+8))->dwLength = lAudioSamples;
	}

	return VDSRVERR_OK;
}

LRESULT Frameserver::SessionFormat(LPARAM lParam, WPARAM stream) {
	FrameserverSession *fs = SessionLookup(lParam);
	DubSource *ds;
	long len;

	if (!fs) return VDSRVERR_BADSESSION;

	if (stream<0 || stream>2) return VDSRVERR_NOSTREAM;

	ds = stream ? (DubSource *)aSrc : (DubSource *)vSrc;

	if (!ds) return VDSRVERR_NOSTREAM;

	if (stream) {
		len = aSrc->getFormatLen();

		if (len > fs->arena_size) return VDSRVERR_TOOBIG;

		memcpy(fs->arena, aSrc->getFormat(), len);
	} else {
		BITMAPINFOHEADER *bmih;

		len = sizeof(BITMAPINFOHEADER);
		if (len > fs->arena_size) return VDSRVERR_TOOBIG;

		memcpy(fs->arena, vSrc->getDecompressedFormat(), len);

		const VDPixmapLayout& output = filters.GetOutputLayout();
		bmih = (BITMAPINFOHEADER *)fs->arena;
//		bmih->biSize		= sizeof(BITMAPINFOHEADER);
		bmih->biWidth		= output.w;
		bmih->biHeight		= output.h;
		bmih->biPlanes		= 1;
		bmih->biCompression	= BI_RGB;
		bmih->biBitCount	= 24;
		bmih->biSizeImage	= ((bmih->biWidth*3+3)&-4)*abs(bmih->biHeight);
		bmih->biClrUsed		= 0;
		bmih->biClrImportant= 0;
	}

	return len;
}

LRESULT Frameserver::SessionFrame(LPARAM lParam, WPARAM original_frame) {
	FrameserverSession *fs = SessionLookup(lParam);

	if (!fs)
		return VDSRVERR_BADSESSION;

	try {
		const VDPixmapLayout& output = filters.GetOutputLayout();
		if (fs->arena_size < ((output.w*3+3)&-4)*output.h)
			return VDSRVERR_TOOBIG;

		VDPosition pos = mVideoFrameMap[original_frame].mSourceFrame;

		if (pos < 0)
			return VDSRVERR_FAILED;

		vdrefptr<IVDFilterFrameClientRequest> creq;
		filters.RequestFrame(pos, 0, ~creq);

		while(!creq->IsCompleted()) {
			if (filters.Run(NULL, false) == FilterSystem::kRunResult_Running)
				continue;

			switch(mpVideoFrameSource->RunRequests(NULL)) {
				case IVDFilterFrameSource::kRunResult_Running:
				case IVDFilterFrameSource::kRunResult_IdleWasActive:
				case IVDFilterFrameSource::kRunResult_BlockedWasActive:
					continue;
			}

			filters.Block();
		}

		VDPixmap pxdst(VDPixmapFromLayout(mFrameLayout, fs->arena));

		VDFilterFrameBuffer *buf = creq->GetResultBuffer();
		VDPixmap px = VDPixmapFromLayout(filters.GetOutputLayout(), (void *)buf->LockRead());
		px.info = buf->info;
		VDPixmapBlt(pxdst, px);
		buf->Unlock();
	} catch(const MyError&) {
		return VDSRVERR_FAILED;
	}

	return VDSRVERR_OK;
}

LRESULT Frameserver::SessionAudio(LPARAM lParam, WPARAM lStart) {
	FrameserverSession *fs = SessionLookup(lParam);
	if (!fs) return VDSRVERR_BADSESSION;

	LONG lCount = *(LONG *)fs->arena;
	LONG cbBuffer = *(LONG *)(fs->arena+4);

	if (cbBuffer > fs->arena_size - 8) cbBuffer = fs->arena_size - 8;

	VDDEBUG("[session %08lx] VDSRVM_REQ_AUDIO(sample %ld, count %d, cbBuffer %ld)\n", lParam, lCount, lStart, cbBuffer);

	// Do not return an error on an attempt to read beyond the end of
	// the audio stream -- this causes Panasonic to error.

	if (lStart >= lAudioSamples) {
		memset(fs->arena, 0, 8);
		return VDSRVERR_OK;
	}

	if (lStart+lCount > lAudioSamples)
		lCount = lAudioSamples;

	// Read subsets.

	long lTotalBytes = 0, lTotalSamples = 0;
	uint32 lActualBytes, lActualSamples = 1;
	char *pDest = (char *)(fs->arena + 8);

	try {
		while(lCount>0 && lActualSamples>0) {
			sint64 start, len;

			// Translate range.

			start = audioset.lookupRange(lStart, len);

			if (len > lCount)
				len = lCount;

			if (start < aSrc->getStart()) {
				start = aSrc->getStart();
				len = 1;
			}

			if (start >= aSrc->getEnd()) {
				start = aSrc->getEnd() - 1;
				len = 1;
			}

			// Attempt read.

			switch(aSrc->read(start, VDClampToSint32(len), pDest, cbBuffer, &lActualBytes, &lActualSamples)) {
			case AVIERR_OK:
				break;
			case AVIERR_BUFFERTOOSMALL:
				if (!lTotalSamples)
					return VDSRVERR_TOOBIG;
				goto out_of_space;
			default:
				return VDSRVERR_FAILED;
			}

			lCount -= lActualSamples;
			lStart += lActualSamples;
			cbBuffer -= lActualBytes;
			pDest += lActualBytes;
			lTotalSamples += lActualSamples;
			lTotalBytes += lActualBytes;
		}
out_of_space:
		;

	} catch(const MyError&) {
		return VDSRVERR_FAILED;
	}

	*(LONG *)(fs->arena + 0) = lTotalBytes;
	*(LONG *)(fs->arena + 4) = lTotalSamples;

	return VDSRVERR_OK;
}

LRESULT Frameserver::SessionAudioInfo(LPARAM lParam, WPARAM lStart) {
	FrameserverSession *fs = SessionLookup(lParam);
	if (!fs) return VDSRVERR_BADSESSION;

	LONG lCount = *(LONG *)fs->arena;
	//LONG cbBuffer = *(LONG *)(fs->arena+4);	Currently ignored.

	if (lStart < 0)
		return VDSRVERR_FAILED;

	if (lStart + lCount > lAudioSamples)
		lCount = lAudioSamples - lStart;

	if (lCount < 0)
		lCount = 0;

	*(LONG *)(fs->arena + 0) = aSrc->getWaveFormat()->mBlockSize * lCount;
	*(LONG *)(fs->arena + 4) = lCount;

	return VDSRVERR_OK;
}

//////////////////////////////////////////////////////////////////////////

extern vdrefptr<AudioSource> inputAudio;
extern vdrefptr<IVDVideoSource> inputVideo;

static HMODULE hmodServer;
static IVDubServerLink *ivdsl;

static BOOL InitServerDLL() {
#ifdef _M_AMD64
	hmodServer = LoadLibrary("vdsvrlnk64.dll");
#else
	hmodServer = LoadLibrary("vdsvrlnk.dll");
#endif

	VDDEBUG("VDSVRLNK handle: %p\n", hmodServer);

	if (hmodServer) {
		FARPROC fp;

		if (!(fp = GetProcAddress(hmodServer, "GetDubServerInterface")))
			return FALSE;

		ivdsl = ((IVDubServerLink *(*)(void))fp)();

		return TRUE;
	}

	return FALSE;
}

INT_PTR CALLBACK FrameServerSetupDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_INITDIALOG:
		{
			char buf[32];

			ivdsl->GetComputerName(buf);
			strcat(buf,"/");

			SetDlgItemText(hDlg, IDC_COMPUTER_NAME, buf);
		}
		SetDlgItemText(hDlg, IDC_FSNAME, VDTextWToA(VDFileSplitPath(g_szInputAVIFile)).c_str());
		SetWindowLongPtr(hDlg, DWLP_USER, lParam);
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
			SendDlgItemMessage(hDlg, IDC_FSNAME, WM_GETTEXT, 128, GetWindowLongPtr(hDlg, DWLP_USER));
			EndDialog(hDlg, TRUE);
			break;
		case IDCANCEL:
			EndDialog(hDlg, FALSE);
			break;
		}
		break;
	}

	return FALSE;
}

void ActivateFrameServerDialog(HWND hwnd, const char *server) {
	static wchar_t fileFilters[]=
		L"VirtualDub AVIFile signpost (*.vdr,*.avi)\0"		L"*.vdr;*.avi\0"
		L"All files\0"										L"*.*\0"
		;

	char szServerName[128];

	if (!InitServerDLL()) return;

	if (server && *server) {
		ivdsl->GetComputerName(szServerName);
		vdstrlcpy(szServerName, server, 128);
	} else {
		if (!DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SERVER_SETUP), hwnd, FrameServerSetupDlgProc, (LPARAM)szServerName))
			return;
	}

	try {
		vdrefptr<Frameserver> fs(new Frameserver(inputVideo, inputAudio, hwnd, &g_dubOpts, g_project->GetTimeline().GetSubset()));

		if (!server || !*server) {
			const VDStringW fname(VDGetSaveFileName(kFileDialog_Signpost, (VDGUIHandle)hwnd, L"Save .VDR signpost for AVIFile handler", fileFilters, g_prefs.main.fAttachExtension ? L"vdr" : NULL, 0, 0));

			if (!fname.empty()) {
				long buf[5];
				char sname[128];
				int slen;

				ivdsl->GetComputerName(sname);
				strcat(sname,"/");
				strcat(sname,szServerName);
				slen = strlen(sname);
				slen += slen&1;

				buf[0] = 'FFIR';
				buf[1] = slen+12;
				buf[2] = 'MRDV';
				buf[3] = 'HTAP';
				buf[4] = slen;

				VDFile file(fname.c_str(), nsVDFile::kWrite | nsVDFile::kDenyRead | nsVDFile::kCreateAlways);

				file.write(buf, 20);
				file.write(sname, strlen(sname));
				if (strlen(sname) & 1)
					file.write("", 1);

				file.close();
			}
		}

		VDDEBUG("Attempting to initialize frameserver...\n");

		fs->Go(ivdsl, szServerName);

		VDDEBUG("Frameserver exit.\n");

	} catch(const MyError& e) {
		e.post(hwnd, "Frameserver error");
	}
}
