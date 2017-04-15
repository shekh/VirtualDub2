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

#include "stdafx.h"
#include <vd2/VDXFrame/VideoFilterEntry.h>
#include <vd2/VDXFrame/VideoFilter.h>


VDXFilterDefinition2 *VDXGetVideoFilterDefinition(int index);

int VDXVideoFilterModuleInit2(struct VDXFilterModule *fm, const VDXFilterFunctions *ff, int vdfd_ver) {
  for(int i=0; ; ++i){
    VDXFilterDefinition2* def = VDXGetVideoFilterDefinition(i);
    if(!def) break;
    ff->addFilter(fm, def, sizeof(VDXFilterDefinition));
  }

  VDXVideoFilter::SetAPIVersion(vdfd_ver);
  
  return 0;
}

int VDXVideoFilterModuleInitFilterMod(struct VDXFilterModule *fm, const FilterModInitFunctions *ff, int vdfd_ver, int mod_ver) {
  for(int i=0; ; ++i){
    VDXFilterDefinition2* def = VDXGetVideoFilterDefinition(i);
    if(!def) break;
    ff->addFilter(fm, def, sizeof(VDXFilterDefinition), &def->filterMod, sizeof(FilterModDefinition));
  }

  VDXVideoFilter::SetAPIVersion(vdfd_ver);
  VDXVideoFilter::SetFilterModVersion(mod_ver);
  
  return 0;
}

