//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2008 Avery Lee
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

#ifndef f_VD2_VERSION_H
#define f_VD2_VERSION_H

#ifdef _MSC_VER
	#pragma once
#endif

#ifndef f_VD2_SYSTEM_VDTYPES_H
	#include <vd2/system/vdtypes.h>
#endif

extern "C" unsigned long version_num;

uint32 VDGetBuildNumber();

#define VD_WIDE_STRING_CONST2(name) L ## name
#define VD_WIDE_STRING_CONST(name) VD_WIDE_STRING_CONST2(name)

#define VD_PROGRAM_NAMEA		"VirtualDub2"
#define VD_PROGRAM_NAMEW		VD_WIDE_STRING_CONST(VD_PROGRAM_NAMEA)

#define VD_PROGRAM_VERSIONA		""
#define VD_PROGRAM_VERSIONW		VD_WIDE_STRING_CONST(VD_PROGRAM_VERSIONA)

#define VD_PROGRAM_SPECIAL_BUILDA	""
#define VD_PROGRAM_SPECIAL_BUILDW	VD_WIDE_STRING_CONST(VD_PROGRAM_SPECIAL_BUILDA)

#ifdef _DEBUG
	#define VD_PROGRAM_GENERIC_CONFIGA	"debug"

	#ifdef _M_AMD64
		#define VD_PROGRAM_CONFIGA			"debug-AMD64"
	#else
		#define VD_PROGRAM_CONFIGA			"debug"
	#endif
#else
	#define VD_PROGRAM_GENERIC_CONFIGA	"release"

	#ifdef _M_AMD64
		#define VD_PROGRAM_CONFIGA			"release-AMD64"
	#else
		#define VD_PROGRAM_CONFIGA			"release"
	#endif
#endif

#define VD_PROGRAM_GENERIC_CONFIGW	VD_WIDE_STRING_CONST(VD_PROGRAM_GENERIC_CONFIGA)
#define VD_PROGRAM_CONFIGW			VD_WIDE_STRING_CONST(VD_PROGRAM_CONFIGA)

#if defined(_M_AMD64)
	#define VD_PROGRAM_PLATFORM_NAMEW		L"AMD64"
	#define VD_PROGRAM_EXEFILE_NAMEA		"VirtualDub64.exe"
	#define VD_PROGRAM_CLIEXE_NAMEA			"vdub64.exe"
#else
	#define VD_PROGRAM_PLATFORM_NAMEW		L"80x86"
	#define VD_PROGRAM_EXEFILE_NAMEA		"VirtualDub.exe"
	#define VD_PROGRAM_CLIEXE_NAMEA			"vdub.exe"
#endif

#endif
