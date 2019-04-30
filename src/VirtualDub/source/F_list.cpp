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
#include "filter.h"
#include "filters.h"
#include <vd2/plugin/vdvideofilt.h>
#include <vd2/VDXFrame/VideoFilter.h>
#include <vd2/VDFilters/VFList.h>

extern const VDXFilterDefinition
#ifdef _DEBUG
	filterDef_debugerror,
	filterDef_debugcrop,
	filterDef_showinfo,
#endif
	filterDef_curves,
	filterDef_resize,
	filterDef_canvas,
	filterDef_fill,
	filterDef_test;

extern FilterDefinition
	filterDef_levels,
	filterDef_logo,
	filterDef_convolute,
	filterDef_emboss
	;

static const FilterDefinition *const builtin_filters[]={
	&filterDef_fill,
	&filterDef_resize,
	&filterDef_canvas,
	&filterDef_levels,
	&filterDef_logo,
//	&filterDef_curves,
	&filterDef_convolute,
	&filterDef_emboss,

#ifdef _DEBUG
	&filterDef_debugerror,
	&filterDef_debugcrop,
	&filterDef_showinfo,
	&filterDef_test,
#endif

	NULL
};

void InitBuiltinFilters() {
	const FilterDefinition *cur, *const *cpp;

	VDXVideoFilter::SetAPIVersion(VIRTUALDUB_FILTERDEF_VERSION);

	cpp = builtin_filters;
	while(cur = *cpp++)
		FilterAddBuiltin(cur);

	cpp = VDVFGetList();
	while(cur = *cpp++)
		FilterAddBuiltin(cur);
}
