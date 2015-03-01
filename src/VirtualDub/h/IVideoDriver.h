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

#ifndef _f_IVIDEODRIVER_H
#define _f_IVIDEODRIVER_H

#include <windows.h>
#include <vfw.h>
//#include "msviddrv.h"

// From msviddrv.h (Video for Windows 1.1e SDK)

typedef struct tag_video_open_parms {
    DWORD               dwSize;         // sizeof(VIDEO_OPEN_PARMS)
    FOURCC              fccType;        // 'vcap'
    FOURCC              fccComp;        // unused
    DWORD               dwVersion;      // version of msvideo opening you
    DWORD               dwFlags;        // channel type
    DWORD               dwError;        // if open fails, this is why
} VIDEO_OPEN_PARMS, FAR * LPVIDEO_OPEN_PARMS;

class IVideoDriver {
public:
	virtual ~IVideoDriver() {};

	virtual BOOL	Load(HDRVR hDriver)																	= 0;
	virtual void	Free(HDRVR hDriver)																	= 0;
	virtual DWORD	Open(HDRVR hDriver, char *szDescription, LPVIDEO_OPEN_PARMS lpVideoOpenParms)	= 0;
	virtual void	Disable(HDRVR hDriver)																= 0;
	virtual void	Enable(HDRVR hDriver)																= 0;
	virtual LRESULT	Default(DWORD dwDriverID, HDRVR hDriver, UINT uiMessage, LPARAM lParam1, LPARAM lParam2) = 0;
};

#endif
