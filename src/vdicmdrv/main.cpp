#include <crtdbg.h>

#include <windows.h>
#include <mmsystem.h>
#include <vfw.h>

//#include <msviddrv.h>

#include "IVideoDriver.h"
#include "IVideoCompressor.h"

#include "CCompRemote.h"

HMODULE g_hInst;

#define BOGUS_DRIVER_ID     1

IVideoDriver *videoDrivers[] = {

    // add other procedures here...

	&videoDriverRemote,

    NULL              // FENCE: must be last
};

/***************************************************************************
 ***************************************************************************/

class DriverPtrTranslator {
public:
	DriverPtrTranslator *next, *prev;
	DWORD id16;
	IVideoCompressor *id32;
};

DriverPtrTranslator *active_opens = NULL;
DWORD xlat_last_id = 2;

DriverPtrTranslator *find_xlator_by_driverptr(IVideoCompressor *ivc) {
	DriverPtrTranslator *ptr = active_opens;

	while(ptr) {
		if (ivc == ptr->id32) return ptr;

		ptr = ptr->next;
	}

	return NULL;
}

DriverPtrTranslator *find_xlator_by_id16(DWORD id16) {
	DriverPtrTranslator *ptr = active_opens;

	while(ptr) {
		if (id16 == ptr->id16) return ptr;

		ptr = ptr->next;
	}

	return NULL;
}

IVideoCompressor *id_to_driverptr(DWORD id16) {
	DriverPtrTranslator *dpt = find_xlator_by_id16(id16);

	return dpt ? dpt->id32 : NULL;
}

DriverPtrTranslator *create_translation() {
	DWORD start_id = xlat_last_id;
	DWORD cur_id = start_id;

	do {
		if (cur_id != BOGUS_DRIVER_ID && cur_id) {
			if (!find_xlator_by_id16(cur_id)) {
				DriverPtrTranslator *dpt;

				if (!(dpt = new DriverPtrTranslator)) return 0;

				dpt->prev = NULL;
				dpt->next = active_opens;
				dpt->id16 = cur_id;

				start_id = (cur_id+1) & 0xffff;

				if (active_opens) active_opens->prev = dpt;
				active_opens = dpt;

				return dpt;
			}
		}

		cur_id = (cur_id+1) & 0xffff;
	} while(cur_id != start_id);

	return 0;
}

void remove_translation(DriverPtrTranslator *dpt) {
	if (dpt == active_opens) active_opens = dpt->next;
	if (dpt->prev) dpt->prev->next = dpt->next;
	if (dpt->next) dpt->next->prev = dpt->prev;

	delete dpt;
}

////////////////////////////////////////////////////////////

#define DRIVERMESSAGE(x) x,#x

struct DriveMessageTranslation {
	UINT msg;
	char *name;
} driverMessages[]={
	DRIVERMESSAGE(DRV_LOAD),
	DRIVERMESSAGE(DRV_FREE),
	DRIVERMESSAGE(DRV_OPEN),
	DRIVERMESSAGE(DRV_CLOSE),
	DRIVERMESSAGE(DRV_QUERYCONFIGURE),
	DRIVERMESSAGE(DRV_CONFIGURE),
	DRIVERMESSAGE(DRV_DISABLE),
	DRIVERMESSAGE(DRV_ENABLE),
	DRIVERMESSAGE(DRV_INSTALL),
	DRIVERMESSAGE(DRV_REMOVE),
	DRIVERMESSAGE(ICM_ABOUT),
	DRIVERMESSAGE(ICM_COMPRESS),
	DRIVERMESSAGE(ICM_COMPRESS_BEGIN),
	DRIVERMESSAGE(ICM_COMPRESS_FRAMES_INFO),
	DRIVERMESSAGE(ICM_COMPRESS_GET_FORMAT),
	DRIVERMESSAGE(ICM_COMPRESS_GET_SIZE),
	DRIVERMESSAGE(ICM_COMPRESS_QUERY),
	DRIVERMESSAGE(ICM_CONFIGURE),
	DRIVERMESSAGE(ICM_DECOMPRESS),
	DRIVERMESSAGE(ICM_DECOMPRESS_BEGIN),
	DRIVERMESSAGE(ICM_DECOMPRESS_END),
	DRIVERMESSAGE(ICM_DECOMPRESS_GET_FORMAT),
	DRIVERMESSAGE(ICM_DECOMPRESS_GET_PALETTE),
	DRIVERMESSAGE(ICM_DECOMPRESS_QUERY),
	DRIVERMESSAGE(ICM_DECOMPRESS_SET_PALETTE),
	DRIVERMESSAGE(ICM_DECOMPRESSEX),
	DRIVERMESSAGE(ICM_DECOMPRESSEX_BEGIN),
	DRIVERMESSAGE(ICM_DECOMPRESSEX_QUERY),
	DRIVERMESSAGE(ICM_DRAW),
	DRIVERMESSAGE(ICM_DRAW_BEGIN),
	DRIVERMESSAGE(ICM_DRAW_CHANGEPALETTE),
	DRIVERMESSAGE(ICM_DRAW_END),
	DRIVERMESSAGE(ICM_DRAW_FLUSH),
	DRIVERMESSAGE(ICM_DRAW_GETTIME),
	DRIVERMESSAGE(ICM_DRAW_QUERY),
	DRIVERMESSAGE(ICM_DRAW_REALIZE),
	DRIVERMESSAGE(ICM_DRAW_RENDERBUFFER),
	DRIVERMESSAGE(ICM_DRAW_SETTIME),
	DRIVERMESSAGE(ICM_DRAW_START),
	DRIVERMESSAGE(ICM_DRAW_START_PLAY),
	DRIVERMESSAGE(ICM_DRAW_STOP),
	DRIVERMESSAGE(ICM_DRAW_STOP_PLAY),
	DRIVERMESSAGE(ICM_DRAW_SUGGESTFORMAT),
	DRIVERMESSAGE(ICM_DRAW_WINDOW),
	DRIVERMESSAGE(ICM_GET),
	DRIVERMESSAGE(ICM_GETBUFFERSWANTED),
	DRIVERMESSAGE(ICM_GETDEFAULTKEYFRAMERATE),
	DRIVERMESSAGE(ICM_GETDEFAULTQUALITY),
	DRIVERMESSAGE(ICM_GETINFO),
	DRIVERMESSAGE(ICM_GETQUALITY),
	DRIVERMESSAGE(ICM_GETSTATE),
	DRIVERMESSAGE(ICM_SET_STATUS_PROC),
	DRIVERMESSAGE(ICM_SETQUALITY),
	DRIVERMESSAGE(ICM_SETSTATE),
};

char *TranslateDriverMessage(UINT msg) {
	static char buf[12];

	for(int i=0; i<(sizeof driverMessages/sizeof driverMessages[0]); i++)
		if (driverMessages[i].msg == msg)
			return driverMessages[i].name;

	wsprintf(buf, "%08lx", msg);

	return buf;
}

/////////////////////

extern "C" __declspec(dllexport) LRESULT CALLBACK DriverProc(DWORD dwDriverID, HDRVR hDriver, UINT uiMessage, LPARAM lParam1, LPARAM lParam2);

__declspec(dllexport) LRESULT CALLBACK DriverProc(DWORD dwDriverID, HDRVR hDriver, UINT uiMessage, LPARAM lParam1, LPARAM lParam2)
{
    IVideoCompressor *pi;
    int	    i;
    LRESULT dw;

    if ( (dwDriverID == BOGUS_DRIVER_ID) || (dwDriverID == 0))
        pi = NULL;
    else
        pi = (IVideoCompressor *)id_to_driverptr(dwDriverID);

    _RPT4(0,"Driver %08lx, Message %s(%08lx, %08lx)\n", dwDriverID, TranslateDriverMessage(uiMessage), lParam1, lParam2);

    switch (uiMessage)
    {
        case DRV_LOAD:

			for(i=0; videoDrivers[i]; i++)
                if (!videoDrivers[i]->Load(hDriver))
                    return 0L;
	    
            return (LRESULT)1L;

        case DRV_FREE:

            // put global de-initialization here...

	    // Pass to all driver procs
            for(i=0; videoDrivers[i]; i++)
                videoDrivers[i]->Free(hDriver);
	    
            return (LRESULT)1L;

        case DRV_OPEN:
             
            /*
               Sent to the driver when it is opened. 

               dwDriverID is 0L.
               
               lParam1 is a far pointer to a zero-terminated string
               containing the name used to open the driver.
               
               lParam2 is passed through from the drvOpen call. It is
               NULL if this open is from the Drivers Applet in control.exe
               It is LPVIDEO_OPEN_PARMS otherwise.
                
               Return 0L to fail the open.
             */

            //
            //  if we were opened without an open structure then just
            //  return a phony (non zero) id so the OpenDriver() will
            //  work.
            //

            if (lParam2 == 0)
                return BOGUS_DRIVER_ID;

            // else, ask all procs if they like input type
			{
				DriverPtrTranslator *dpt = create_translation();

				for (i=0; videoDrivers[i]; i++) {
					if (dw = (DWORD)videoDrivers[i]->Open(hDriver, (char *)lParam1, (LPVIDEO_OPEN_PARMS)lParam2)) {
						_RPT2(0,"DRV_OPEN: Driver %d returned %p\n", i, dw);

						dpt->id32 = (IVideoCompressor *)dw;

						return dpt->id16;	// they did, return
					}
				}

				remove_translation(dpt);
			}
	    
			return 0L;

		case DRV_CLOSE:

			if (pi) {
				DriverPtrTranslator *dpt;

				if (dpt = find_xlator_by_id16(dwDriverID))
					remove_translation(dpt);

				delete pi;
			}

			return 0L;

        case DRV_QUERYCONFIGURE:
            return (LRESULT)0L;

		case DRV_CONFIGURE:	    
            return DRV_OK;

        /*********************************************************************

            standard driver messages

        *********************************************************************/

        case DRV_DISABLE:
			for(i=0; videoDrivers[i]; i++)
				videoDrivers[i]->Disable(hDriver);

			return TRUE;

        case DRV_ENABLE:
            for(i=0; videoDrivers[i]; i++)
                videoDrivers[i]->Enable(hDriver);

            return TRUE;

        case DRV_INSTALL:
        case DRV_REMOVE:
            return (LRESULT)DRV_OK;

        default:
            if (pi && uiMessage>=DRV_USER) switch(uiMessage) {
				case ICM_ABOUT:						return pi->About((HWND)lParam1);
				case ICM_COMPRESS:					return pi->Compress((ICCOMPRESS *)lParam1, (DWORD)lParam2);
				case ICM_COMPRESS_BEGIN:			return pi->CompressBegin((LPBITMAPINFO)lParam1, (LPBITMAPINFO)lParam2);
				case ICM_COMPRESS_FRAMES_INFO:		return pi->CompressFramesInfo((ICCOMPRESSFRAMES *)lParam1, (DWORD)lParam2);
				case ICM_COMPRESS_GET_FORMAT:		return pi->CompressGetFormat((LPBITMAPINFO)lParam1, (LPBITMAPINFO)lParam2);
				case ICM_COMPRESS_GET_SIZE:			return pi->CompressGetSize((LPBITMAPINFO)lParam1, (LPBITMAPINFO)lParam2);
				case ICM_COMPRESS_QUERY:			return pi->CompressQuery((LPBITMAPINFO)lParam1, (LPBITMAPINFO)lParam2);
				case ICM_CONFIGURE:					return pi->Configure((HWND)lParam1);
				case ICM_DECOMPRESS:				return pi->Decompress((ICDECOMPRESS *)lParam1, (DWORD)lParam2);
				case ICM_DECOMPRESS_BEGIN:			return pi->DecompressBegin((LPBITMAPINFO)lParam1, (LPBITMAPINFO)lParam2);
				case ICM_DECOMPRESS_END:			return pi->DecompressEnd();
				case ICM_DECOMPRESS_GET_FORMAT:		return pi->DecompressGetFormat((LPBITMAPINFO)lParam1, (LPBITMAPINFO)lParam2);
				case ICM_DECOMPRESS_GET_PALETTE:	return pi->DecompressGetPalette((LPBITMAPINFOHEADER)lParam1, (LPBITMAPINFOHEADER)lParam2);
				case ICM_DECOMPRESS_QUERY:			return pi->DecompressQuery((LPBITMAPINFO)lParam1, (LPBITMAPINFO)lParam2);
				case ICM_DECOMPRESS_SET_PALETTE:	return pi->DecompressSetPalette((LPBITMAPINFOHEADER)lParam1);
				case ICM_DECOMPRESSEX:				return pi->DecompressEx((ICDECOMPRESSEX *)lParam1, (DWORD)lParam2);
				case ICM_DECOMPRESSEX_BEGIN:		return pi->DecompressExBegin((ICDECOMPRESSEX *)lParam1, (DWORD)lParam2);
				case ICM_DECOMPRESSEX_QUERY:		return pi->DecompressExQuery((ICDECOMPRESSEX *)lParam1, (DWORD)lParam2);
				case ICM_DRAW:						return pi->Draw((ICDRAW *)lParam1, (DWORD)lParam2);
				case ICM_DRAW_BEGIN:				return pi->DrawBegin((ICDRAWBEGIN *)lParam1, (DWORD)lParam2);
				case ICM_DRAW_CHANGEPALETTE:		return pi->DrawChangePalette((BITMAPINFO *)lParam1);
				case ICM_DRAW_END:					return pi->DrawEnd();
				case ICM_DRAW_FLUSH:				return pi->DrawFlush();
				case ICM_DRAW_GETTIME:				return pi->DrawGetTime((DWORD *)lParam1);
				case ICM_DRAW_QUERY:				return pi->DrawQuery((BITMAPINFO *)lParam1);
				case ICM_DRAW_REALIZE:				return pi->DrawRealize((HDC)lParam1, (BOOL)lParam2);
				case ICM_DRAW_RENDERBUFFER:			return pi->DrawRenderBuffer();
				case ICM_DRAW_SETTIME:				return pi->DrawSetTime((DWORD)lParam1);
				case ICM_DRAW_START:				return pi->DrawStart();
				case ICM_DRAW_START_PLAY:			return pi->DrawStartPlay((DWORD)lParam1, (DWORD)lParam2);
				case ICM_DRAW_STOP:					return pi->DrawStop();
				case ICM_DRAW_STOP_PLAY:			return pi->DrawStopPlay();
				case ICM_DRAW_SUGGESTFORMAT:		return pi->DrawSuggestFormat((ICDRAWSUGGEST *)lParam1, (DWORD)lParam2);
				case ICM_DRAW_WINDOW:				return pi->DrawWindow((RECT *)lParam1);
				case ICM_GET:						return pi->Get((LPVOID)lParam1, (DWORD)lParam2);
				case ICM_GETBUFFERSWANTED:			return pi->GetBuffersWanted((DWORD *)lParam1);
				case ICM_GETDEFAULTKEYFRAMERATE:	return pi->GetDefaultKeyFrameRate((DWORD *)lParam1);
				case ICM_GETDEFAULTQUALITY:			return pi->GetDefaultQuality((DWORD *)lParam1);
				case ICM_GETINFO:					return pi->GetInfo((ICINFO *)lParam1, (DWORD)lParam2);
				case ICM_GETQUALITY:				return pi->GetQuality((DWORD *)lParam1);
				case ICM_GETSTATE:					return pi->GetState((LPVOID)lParam1, (DWORD)lParam2);
				case ICM_SET_STATUS_PROC:			return pi->SetStatusProc((ICSETSTATUSPROC *)lParam1, (DWORD)lParam2);
				case ICM_SETQUALITY:				return pi->SetQuality((DWORD *)lParam1);
				case ICM_SETSTATE:					return pi->SetState((LPVOID)lParam1, (DWORD)lParam2);
				default:
					return pi->Default(dwDriverID, hDriver, uiMessage, lParam1, lParam2);
			} else
                return DefDriverProc(dwDriverID, hDriver, uiMessage, lParam1, lParam2);
    }
}

BOOL WINAPI DllMain(HINSTANCE hInst, ULONG ulReason, LPVOID lpReserved) {

	switch(ulReason) {
	case DLL_PROCESS_ATTACH:
	    g_hInst = hInst;
		break;
	}

    return TRUE;
}
