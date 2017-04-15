//	VDXFrame - Helper library for VirtualDub plugins
//	Copyright (C) 2008 Avery Lee
//
//	The plugin headers in the VirtualDub plugin SDK are licensed differently
//	differently than VirtualDub and the Plugin SDK themselves.  This
//	particular file is thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#ifndef f_VD2_VDXFRAME_VIDEOFILTERENTRY_H
#define f_VD2_VDXFRAME_VIDEOFILTERENTRY_H

struct VDXFilterModule;
struct VDXFilterFunctions;
struct FilterModInitFunctions;
struct VDXFilterDefinition2;

///////////////////////////////////////////////////////////////////////////////
/// Video filter declaration macros
///
/// To declare video filters, use the following pattern:
///
///	VDX_DECLARE_VIDEOFILTERS_BEGIN()
///		VDX_DECLARE_VIDEOFILTER(definitionSymbolName)
///	VDX_DECLARE_VIDEOFILTERS_END()
///
/// Each entry points to a variable of type VDXFilterDefinition. Note that these must
/// be declared as _non-const_ for compatibility with older versions of VirtualDub.
/// Video filters declared this way are automatically registered by the module init
/// routine.
///
#define VDX_DECLARE_VIDEOFILTERS_BEGIN()		VDXFilterDefinition2 *VDXGetVideoFilterDefinition(int index) {
#define VDX_DECLARE_VIDEOFILTER(defVarName)			if (!index--) { extern VDXFilterDefinition2 defVarName; return &defVarName; }
#define VDX_DECLARE_VIDEOFILTERS_END()				return NULL;	\
												}
#define VDX_DECLARE_VFMODULE()

int VDXVideoFilterModuleInit2(struct VDXFilterModule *fm, const VDXFilterFunctions *ff, int vdfd_ver);
int VDXVideoFilterModuleInitFilterMod(struct VDXFilterModule *fm, const FilterModInitFunctions *ff, int vdfd_ver, int mod_ver);

#endif
