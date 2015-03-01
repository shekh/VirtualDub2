#ifndef f_VDSERVER_H
#define f_VDSERVER_H

#include <windows.h>
#include <vfw.h>

#ifdef BUILDING_VDSVRLNK_DLL
#define DLLFUNC extern "C" __declspec(dllexport)
#else
#define DLLFUNC extern "C" __declspec(dllimport)
#endif

class IVDubAnimConnection {
public:
	virtual BOOL hasAudio() = 0;
	virtual BOOL readStreamInfo(AVISTREAMINFO *lpsi, BOOL fAudio, long *lpFirst, long *lpLast) = 0;
	virtual int readFormat(void *ptr, BOOL fAudio) = 0;
	virtual int readVideo(long lSample, void *lpBuffer) = 0;
	virtual int readAudio(long lSample, long lCount, void *lpBuffer, long cbBuffer, long *lplBytes, long *lplSamples) = 0;	
};

class IVDubServerLink {
public:
	virtual void GetComputerName(char *buf) = 0;
	virtual IVDubAnimConnection *FrameServerConnect(char *fs_name) = 0;
	virtual void FrameServerDisconnect(IVDubAnimConnection *) = 0;
	virtual int CreateFrameServer(char *name, HWND hwndServer) = 0;
	virtual void DestroyFrameServer(int handle) = 0;
};

DLLFUNC IVDubServerLink * __cdecl GetDubServerInterface();


enum {
	VDSRVM_BIGGEST			= 0x7f00,
		//	wParam:
		//	lParam:
		//
		//	Return: arena size required to hold largest frame

	VDSRVM_OPEN				= 0x7f01,
		//	wParam: length of arena
		//	lParam: 32-bit memory map identifier
		//
		//	Return:	32-bit session ID or NULL on failure
		//
		//	The memory will be mapped in as VDUBFxxxxxxxx

	VDSRVM_CLOSE			= 0x7f02,
		//	wParam:
		//	lParam:	32-bit session ID
		//
		//	Return:	0 on success
		//			-4 if session ID is unknown

	VDSRVM_REQ_STREAMINFO	= 0x7f03,
		//	wParam: 0 for video stream, 1 for audio stream
		//	lParam: 32-bit session ID
		//
		//	Return: 0 on success, -1 on failure, -2 if stream doesn't exist
		//
		//			arena+0: first sample
		//			arena+4: last sample+1
		//			arena+8: AVISTREAMINFO

	VDSRVM_REQ_FORMAT		= 0x7f04,
		//	wParam:	0 for video stream, 1 for audio stream
		//	lParam: 32-bit session ID
		//
		//	Return:	length of format on success
		//			-1 on failure
		//			-2 if stream doesn't exist
		//			-3 if format is too long for shared arena
		//			-4 if session ID is unknown
		//
		//	Format is placed at bottom of shared arena

	VDSRVM_REQ_FRAME		= 0x7f05,
		//	wParam:	requested sample #
		//	lParam: 32-bit session ID
		//
		//	Return:	0 on success
		//			-1 on failure
		//			-2 if stream doesn't exist
		//			-3 if frame is too big for shared arena
		//			-4 if session ID is unknown

	VDSRVM_REQ_AUDIO		= 0x7f06,
		//	wParam:	requested sample # to start at
		//	lParam: 32-bit session ID
		//	arena+0: number of samples to read
		//
		//	Return: 0 on success
		//				arena+0: number of bytes actually read
		//				arena+4: number of samples actually read
		//				arena+8: data
		//			-1 on failure
		//			-2 if stream doesn't exist
		//			-3 if a sample is too big for shared arena
		//			-4 if session ID is unknown

	VDSRVM_REQ_AUDIOINFO	= 0x7f07,	// VirtualDub 1.3b or higher
		//	wParam: requested sample # to start at
		//	lParam: 32-bit session ID
		//	arena+0: number of samples to check
		//
		//	Return: 0 on success
		//				arena+0: number of bytes actually read
		//				arena+4: number of samples actually read
		//			-1 on failure
		//			-2 if stream doesn't exist
		//			-3 if a sample is too big for shared arena
		//			-4 if session ID is unknown
};

enum {
	VDSRVERR_OK			= 0,
	VDSRVERR_FAILED		= -1,
	VDSRVERR_NOSTREAM	= -2,
	VDSRVERR_TOOBIG		= -3,
	VDSRVERR_BADSESSION	= -4
};

#endif
