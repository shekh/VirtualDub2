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

#ifndef f_LEVELCONTROL_H
#define f_LEVELCONTROL_H

#include <windows.h>

#define VIDEOLEVELCONTROLCLASS (g_szLevelControlName)

#ifndef f_LEVELCONTROL_CPP
extern const char g_szLevelControlName[];
#endif

#define VLCM_SETTABCOUNT		(WM_USER+0x100)
#define	VLCM_SETTABCOLOR		(WM_USER+0x101)
#define VLCM_SETTABPOS			(WM_USER+0x102)
#define	VLCM_GETTABPOS			(WM_USER+0x103)
#define VLCM_MOVETABPOS			(WM_USER+0x104)
#define	VLCM_SETGRADIENT		(WM_USER+0x105)

#define VLCN_TABCHANGE			(2)

typedef struct NMVLTABCHANGE {
	NMHDR	hdr;
	int		iTab;
	int		iNewPos;
} NMVLTABCHANGE;

ATOM RegisterLevelControl();

#endif
