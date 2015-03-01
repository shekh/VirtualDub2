#include <crtdbg.h>

#include <windows.h>


namespace Windows95 {

#include <svrapi.h>

};


#include <lmcons.h>
#include <lmserver.h>
#include <lmapibuf.h>
#include <vfw.h>

#define BUILDING_VDSVRLNK_DLL
#include "vdserver.h"

typedef struct VDubPostedFrameserver {
	LONG hwndServer;
	int active_connects;
	char name[56];
} VDubPostedFrameserver;

#define MAXIMUM_FRAMESERVERS (256)

typedef struct VDubSharedHeap {
	char	comp_name[16];
	int		total_frameservers;
	int		last_alloc;
	DWORD	next_mmapID;
	VDubPostedFrameserver fs_table[MAXIMUM_FRAMESERVERS];
} VDubSharedHeap;

////////////////////////////////////////////

VDubSharedHeap *heap;
HANDLE			hHeap;
HANDLE			hMutex;

////////////////////////////////////////////

typedef	int (APIENTRY *LPNETSERVERGETINFO95)(
				const char FAR *     pszServer,
				short                sLevel,
				char FAR *           pbBuffer,
				unsigned short       cbBuffer,
				unsigned short FAR * pcbTotalAvail);

typedef	NET_API_STATUS (APIENTRY *LPNETSERVERGETINFONT)(
				LPWSTR				servername,
				DWORD				level,
				LPBYTE				*bufptr);

static void InitComputerAlias() {
	union {
		struct Windows95::server_info_50 si_95;
		char buf[2048];
	} si;
	unsigned short needed_size;
	HMODULE hDll = NULL;
	FARPROC fpNetServerGetInfo = NULL;

	// For Windows 95, NetServerGetInfo() is in SVRAPI.DLL
	// For Windows NT, NetServerGetInfo() is in NETAPI32.DLL
	//
	// Rather than test for the OS as Microsoft suggests, we simply
	// try SVRAPI.DLL first, and then NETAPI32.  If both fail, then
	// fake a computer name with 'ANON' and then 8 hex digits of the
	// current tick value.

	if (hDll = LoadLibrary("svrapi.dll")) {
		fpNetServerGetInfo = GetProcAddress(hDll, "NetServerGetInfo");

		if (fpNetServerGetInfo && !((LPNETSERVERGETINFO95)fpNetServerGetInfo)(NULL, 50, (char *)&si, sizeof si, &needed_size)) {
			strncpy(heap->comp_name, si.si_95.sv50_name, sizeof heap->comp_name);
			heap->comp_name[sizeof heap->comp_name-1] = 0;

			FreeLibrary(hDll);
			return;
		}

		FreeLibrary(hDll);
	}

	// Okay, that didn't work -- try Windows NT/2000.

	if (!fpNetServerGetInfo && (hDll = LoadLibrary("netapi32.dll"))) {
		SERVER_INFO_100 *psi100;

		fpNetServerGetInfo = GetProcAddress(hDll, "NetServerGetInfo");
		if (fpNetServerGetInfo && !((LPNETSERVERGETINFONT)fpNetServerGetInfo)(NULL, 100, (LPBYTE *)&psi100)) {

			WideCharToMultiByte(CP_ACP, 0,  (LPWSTR)psi100->sv100_name, -1, heap->comp_name, sizeof heap->comp_name, NULL, NULL);
//			strncpy(heap->comp_name, psi100->sv100_name, sizeof heap->comp_name);
//			heap->comp_name[sizeof heap->comp_name-1] = 0;

			void *lpNetApiBufferFree;

			if (lpNetApiBufferFree = GetProcAddress(hDll, "NetApiBufferFree"))
				((NET_API_STATUS (APIENTRY *)(LPVOID ))lpNetApiBufferFree)(psi100);

			FreeLibrary(hDll);
			return;
		}

		FreeLibrary(hDll);
	}

	// Ugh.

	wsprintf(heap->comp_name, "Anon%08lx", GetTickCount());
}

static BOOL InitSharedSpace() {
	BOOL fInit;

	//////////

	hHeap = CreateFileMapping(	INVALID_HANDLE_VALUE,
								NULL,
								PAGE_READWRITE,
								0,
								sizeof(VDubSharedHeap),
								"VirtualDub Server System Window");

	if (hHeap == NULL) return FALSE;

	fInit = (GetLastError() != ERROR_ALREADY_EXISTS);

	_RPT0(0,"shared heap created\n");

	heap = (VDubSharedHeap *)MapViewOfFile(
								hHeap,
								FILE_MAP_WRITE,
								0,
								0,
								0);

	if (!heap) {
		CloseHandle(hHeap);
		return FALSE;
	}

	_RPT0(0,"heap mapped into subspace... preparing to go to warp.\n");

	if (!(hMutex = CreateMutex(NULL, FALSE, "VirtualDub Server System Mutex")))
		return FALSE;

	_RPT0(0,"mutex obtained\n");

	if (!fInit) return TRUE;

	memset(heap, 0, sizeof VDubSharedHeap);

	InitComputerAlias();

	return TRUE;
}

// Are you a Ranma 1/2 fan?

static void ranko() {
	_RPT1(0,"server: lock mutex %p\n", hMutex);
	WaitForSingleObject(hMutex, INFINITE);
}

static void ranma() {
	_RPT1(0,"server: unlock mutex %p\n", hMutex);
	if (!ReleaseMutex(hMutex)) {
		_RPT0(0,"Hey!  I didn't own the mutex!\n");
	}
}

////////////////////////////////////////////

//extern "C" __declspec(dllexport) BOOL WINAPI DllMain(HANDLE hModule, ULONG ulReason, LPVOID lpReserved);

HMODULE ghModule;

BOOL APIENTRY DllMain(HANDLE hModule, ULONG ulReason, LPVOID lpReserved) {

	switch(ulReason) {
	case DLL_PROCESS_ATTACH:
	    ghModule = (HMODULE)hModule;

		_RPT0(0,"Process attach\n");

		return InitSharedSpace();

	case DLL_PROCESS_DETACH:
		UnmapViewOfFile(heap);
		CloseHandle(hHeap);
		break;
	}

    return TRUE;
}

/////////////////////////////////////////////

class CVDubAnimConnection : public IVDubAnimConnection {
private:
	VDubPostedFrameserver *frameserver;
	HANDLE hArena;
	char *arena;
	long lArenaSize;
	DWORD dwSessionID;
	long lFrameSize;

public:
	CVDubAnimConnection(VDubPostedFrameserver *);
	~CVDubAnimConnection();

	BOOL init();

	BOOL hasAudio();
	BOOL readStreamInfo(AVISTREAMINFO *lpsi, BOOL fAudio, long *lpFirst, long *lpLast);
	int readFormat(void *ptr, BOOL fAudio);
	int readVideo(long lSample, void *lpBuffer);
	int readAudio(long lSample, long lCount, void *lpBuffer, long cbBuffer, long *lplBytes, long *lplSamples);
};


///////////////////////////////////////////

CVDubAnimConnection::CVDubAnimConnection(VDubPostedFrameserver *vdpf) {
	frameserver = vdpf;
	dwSessionID = 0;
	hArena = INVALID_HANDLE_VALUE;
	arena = NULL;
}

CVDubAnimConnection::~CVDubAnimConnection() {
	if (dwSessionID) SendMessage((HWND)LongToHandle(frameserver->hwndServer), VDSRVM_CLOSE, 0, dwSessionID);

	if (arena) UnmapViewOfFile(arena);
	if (hArena) CloseHandle(hArena);

	--frameserver->active_connects;
}

BOOL CVDubAnimConnection::init() {
	LONG lArenaSize;
	char buf[16];
	DWORD mmapID;

	// find out how big of an arena we need

	lArenaSize = SendMessage((HWND)LongToHandle(frameserver->hwndServer), VDSRVM_BIGGEST, 0, 0);

	if (!lArenaSize) return FALSE;

	// create a name for us

	ranko();
	wsprintf(buf, "VDUBF%08lx", mmapID = heap->next_mmapID++);
	ranma();

	// create a shared arena and map a window for us

	hArena = CreateFileMapping(
			INVALID_HANDLE_VALUE,
			NULL,
			PAGE_READWRITE,
			0,
			lArenaSize,
			buf);

	if (!hArena) return FALSE;

	_RPT0(0,"Have arena.\n");

	arena = (char *)MapViewOfFile(hArena, FILE_MAP_WRITE, 0, 0, lArenaSize);

	if (!arena) return FALSE;

	_RPT0(0,"Arena mapped.\n");

	// hail the server

	dwSessionID = SendMessage((HWND)LongToHandle(frameserver->hwndServer), VDSRVM_OPEN, lArenaSize, mmapID);

	if (!dwSessionID) return FALSE;		// no response, Captain

	_RPT0(0,"Server responded.\n");

	// on screen!  get me the video format!

	if (SendMessage((HWND)LongToHandle(frameserver->hwndServer), VDSRVM_REQ_FORMAT, 0, dwSessionID) <= 0)
		return FALSE;

	{
		BITMAPINFOHEADER *bmih = (BITMAPINFOHEADER *)arena;

		lFrameSize = bmih->biSizeImage;
	}

	_RPT0(0,"Connect!\n");

	return TRUE;
}

BOOL CVDubAnimConnection::hasAudio() {
	return VDSRVERR_OK == SendMessage((HWND)LongToHandle(frameserver->hwndServer), VDSRVM_REQ_STREAMINFO, 1, dwSessionID);
}

BOOL CVDubAnimConnection::readStreamInfo(AVISTREAMINFO *lpsi, BOOL fAudio, long *lpFirst, long *lpLast) {
	if (VDSRVERR_OK == SendMessage((HWND)LongToHandle(frameserver->hwndServer), VDSRVM_REQ_STREAMINFO, !!fAudio, dwSessionID)) {
		if (lpsi) memcpy(lpsi, arena+8, sizeof(AVISTREAMINFO));
		if (lpFirst) *lpFirst = *(long *)(arena+0);
		if (lpLast) *lpLast = *(long *)(arena+4);
		return TRUE;
	}
	return FALSE;
}

int CVDubAnimConnection::readFormat(void *ptr, BOOL fAudio) {
	int err;

	err = SendMessage((HWND)LongToHandle(frameserver->hwndServer), VDSRVM_REQ_FORMAT, !!fAudio, dwSessionID);

	if (err<0) return err;

	if (ptr) memcpy(ptr, arena, err);

	return err;
}

int CVDubAnimConnection::readVideo(long lSample, void *lpBuffer) {
	int err;

	_RPT0(0,"Sending message...\n");
	if (VDSRVERR_OK != (err = SendMessage((HWND)LongToHandle(frameserver->hwndServer), VDSRVM_REQ_FRAME, lSample, dwSessionID)))
		return err;

	_RPT2(0,"Copying %ld bytes to user buffer from arena %p\n", lFrameSize, arena);
	memcpy(lpBuffer, arena, lFrameSize);
	_RPT0(0,"Copy completed.\n");

	return VDSRVERR_OK;
}

int CVDubAnimConnection::readAudio(long lSample, long lCount, void *lpBuffer, long cbBuffer, long *lplBytes, long *lplSamples) {
	int err;

	*(long *)(arena+0) = lCount;
	*(long *)(arena+4) = cbBuffer;

	if (VDSRVERR_OK != (err = SendMessage((HWND)LongToHandle(frameserver->hwndServer), lpBuffer?VDSRVM_REQ_AUDIO:VDSRVM_REQ_AUDIOINFO, lSample, dwSessionID)))
		return err;

	if (lplSamples) *lplSamples = *(long *)(arena + 4);
	if (lplBytes) *lplBytes = *(long *)(arena + 0);

	if (lpBuffer) memcpy(lpBuffer, arena+8, *lplBytes);

	return VDSRVERR_OK;
}

/////////////////////////////////////////////////////////////////////////////
//
//		CVDubServerLink
//
/////////////////////////////////////////////////////////////////////////////

class CVDubServerLink : public IVDubServerLink {
public:
	void					GetComputerName(char *buf);
	IVDubAnimConnection	*	FrameServerConnect(char *fs_name);
	void					FrameServerDisconnect(IVDubAnimConnection *);
	int						CreateFrameServer(char *name, HWND hwndServer);
	void					DestroyFrameServer(int handle);
};

static CVDubServerLink i_dubserver;

void CVDubServerLink::GetComputerName(char *buf) {
	lstrcpy(buf, heap->comp_name);
}

IVDubAnimConnection *CVDubServerLink::FrameServerConnect(char *fs_name) {
	int i;
	char c,*s;
	CVDubAnimConnection *cvdac;

	// look for a slash (/) that delimits the computer name

	_RPT1(0,"Looking for server [%s]\n",fs_name);

	s = fs_name;
	while(c=*s++) if (c=='/') {
		char *s1, *s2, d;

		// we found the slash... now compare that to our computer name

		s1 = fs_name;
		s2 = heap->comp_name;

		do {
			c = *s1++;
			d = *s2++;
		} while(c==d && c!='/' && d);

		if (c!='/' || d)
			return NULL;	// not our computer, and we don't do remote yet

		// yay!  it's us!

		break;
	}

	if (!c) s=fs_name;

	// look through the list and see if we can spot the server

	_RPT1(0,"Computer name ok, looking for server [%s]\n", s);

	ranko();

	for(i=0; i<MAXIMUM_FRAMESERVERS; i++) {
		if (heap->fs_table[i].hwndServer && !_strnicmp(heap->fs_table[i].name, s, sizeof heap->fs_table[i].name - 1))
			break;
	}

	if (i >= MAXIMUM_FRAMESERVERS) {
		_RPT0(0,"Computer not found\n");
		ranma();
		return NULL;
	}

	// hey!  we spotted the server!  now lock it before it goes
	// away!

	_RPT0(0,"Server found\n");

	++heap->fs_table[i].active_connects;

	// allocate a CVDubAnimConnection

	if (cvdac = new CVDubAnimConnection(&heap->fs_table[i])) {
		// try to initialize it

		if (!cvdac->init()) {
			delete cvdac;
			cvdac = NULL;
		}
	} else --heap->fs_table[i].active_connects;

	ranma();

	return cvdac;
}

void CVDubServerLink::FrameServerDisconnect(IVDubAnimConnection *idac) {
	delete (CVDubAnimConnection *)idac;
}

int CVDubServerLink::CreateFrameServer(char *name, HWND hwndServer) {
	int i;

	if (heap->total_frameservers >= MAXIMUM_FRAMESERVERS)
		return -1;

	ranko();

	i = heap->last_alloc;
	while(heap->fs_table[i].active_connects || heap->fs_table[i].hwndServer)
		i = (i+1) % MAXIMUM_FRAMESERVERS;

	heap->fs_table[i].hwndServer = HandleToLong(hwndServer);
	strncpy(heap->fs_table[i].name, name, sizeof heap->fs_table[i].name);

	heap->fs_table[i].name[(sizeof heap->fs_table[i].name)-1] = 0;

	heap->last_alloc = i;

	ranma();

	return i;
}

void CVDubServerLink::DestroyFrameServer(int handle) {
	if (handle<0 || handle>=MAXIMUM_FRAMESERVERS)
		return;

	ranko();

	heap->fs_table[handle].hwndServer = NULL;

	ranma();
}

extern "C" __declspec(dllexport) IVDubServerLink *__cdecl GetDubServerInterface() {
	return &i_dubserver;
}
