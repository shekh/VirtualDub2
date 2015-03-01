//	VirtualDub - Video processing and capture application
//	Internal filter library
//	Copyright (C) 1998-2011 Avery Lee
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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef f_VD2_VDFILTERS_SINGLEVALUEDIALOG_H
#define f_VD2_VDFILTERS_SINGLEVALUEDIALOG_H

#include <vd2/plugin/vdvideofilt.h>

bool VDFilterGetSingleValue(VDXHWND hWnd, sint32 cVal, sint32 *result, sint32 lMin, sint32 lMax, const char *title, IVDXFilterPreview2 *ifp2, void (*pUpdateFunction)(long value, void *data), void *pUpdateFunctionData);

#endif	// f_VD2_VDFILTERS_SINGLEVALUEDIALOG_H
