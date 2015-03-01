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

#include <stdarg.h>
#include <malloc.h>
#include <windows.h>

#include <ctype.h>

#ifdef _MSC_VER
	#include <vd2/system/win32/intrin.h>
#endif

#include "resource.h"
#include <vd2/system/debug.h>
#include <vd2/system/error.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/filesys.h>
#include <vd2/system/protscope.h>
#include <vd2/system/refcount.h>
#include <vd2/system/VDString.h>
#include <vd2/system/vdalloc.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/VDLib/Dialog.h>
#include "plugins.h"

#include "filter.h"
#include "filters.h"
#include <vd2/plugin/vdplugin.h>

VDFilterChainDesc g_filterChain;
FilterSystem	filters;

///////////////////////////////////////////////////////////////////////////
//
//	FilterDefinitionInstance
//
///////////////////////////////////////////////////////////////////////////

FilterDefinitionInstance::FilterDefinitionInstance(VDExternalModule *pfm)
	: mpExtModule(pfm)
	, mAPIVersion(0)
	, mRefCount(0)
	, mbHasStaticAbout(false)
	, mbHasStaticConfigure(false)
{
}

FilterDefinitionInstance::~FilterDefinitionInstance() {
	VDASSERT(mRefCount==0);
}

void FilterDefinitionInstance::Assign(const FilterDefinition& def, int len) {
	memset(&mDef, 0, sizeof mDef);
	memset(&mFilterModDef, 0, sizeof mFilterModDef);
	memcpy(&mDef, &def, std::min<size_t>(sizeof mDef, len));

	mName			= def.name;
	mAuthor			= def.maker ? def.maker : "(internal)";
	mDescription	= def.desc;

	if (mpExtModule)
		mDef._module = const_cast<VDXFilterModule *>(&mpExtModule->GetFilterModuleInfo());
	else
		mDef._module = NULL;

	mAPIVersion = mpExtModule ? mpExtModule->GetVideoFilterAPIVersion() : VIRTUALDUB_FILTERDEF_VERSION;

	if (mAPIVersion >= 16) {
		mDef.stringProc = NULL;
		mDef.copyProc = NULL;
	} else {
		mDef.mSourceCountLowMinus1 = 0;
		mDef.mSourceCountHighMinus1 = 0;
	}

	mbHasStaticAbout = (mDef.mpStaticAboutProc != NULL);
	mbHasStaticConfigure = (mDef.mpStaticConfigureProc != NULL);
}

void FilterDefinitionInstance::AssignFilterMod(const FilterModDefinition& def, int len) {
	memset(&mFilterModDef, 0, sizeof mFilterModDef);
	memcpy(&mFilterModDef, &def, std::min<size_t>(sizeof mFilterModDef, len));
}

void FilterDefinitionInstance::Deactivate() {
	memset(&mDef, 0, sizeof mDef);
	memset(&mFilterModDef, 0, sizeof mFilterModDef);
}

const FilterDefinition& FilterDefinitionInstance::Attach() {
	VDASSERT(mAPIVersion);

	if (mpExtModule)
		mpExtModule->Lock();

	++mRefCount;

	return mDef;
}

void FilterDefinitionInstance::Detach() {
	VDASSERT(mRefCount.dec() >= 0);

	if (mpExtModule)
		mpExtModule->Unlock();
}

///////////////////////////////////////////////////////////////////////////
//
//	Filter global functions
//
///////////////////////////////////////////////////////////////////////////

static ListAlloc<FilterDefinitionInstance>	g_filterDefs;

FilterDefinitionInstance *FilterBaseAdd(VDXFilterModule *fm, FilterDefinition *pfd, int fd_len) {
	VDExternalModule *pExtModule = VDGetExternalModuleByFilterModule(fm);

	if (pExtModule) {
		List2<FilterDefinitionInstance>::fwit it2(g_filterDefs.begin());

		for(; it2; ++it2) {
			FilterDefinitionInstance& fdi = *it2;

			if (fdi.GetModule() == pExtModule && fdi.GetName() == pfd->name) {
				fdi.Assign(*pfd, fd_len);
				return &fdi;
			}
		}

		vdautoptr<FilterDefinitionInstance> pfdi(new FilterDefinitionInstance(pExtModule));
		pfdi->Assign(*pfd, fd_len);

		FilterDefinitionInstance* fdi = pfdi;
		g_filterDefs.AddTail(pfdi.release());

		return fdi;
	}

	return NULL;
}

FilterDefinition *FilterAdd(VDXFilterModule *fm, FilterDefinition *pfd, int fd_len) {
	FilterDefinitionInstance* fdi = FilterBaseAdd(fm,pfd,fd_len);
	if(fdi) return const_cast<FilterDefinition *>(&fdi->GetDef());
	return NULL;
}

FilterDefinition *FilterModAdd(VDXFilterModule *fm, FilterDefinition *pfd, int fd_len, FilterModDefinition *pfm, int fm_len) {
	FilterDefinitionInstance* fdi = FilterBaseAdd(fm,pfd,fd_len);
	if(fdi){
		fdi->AssignFilterMod(*pfm, fm_len);
		return const_cast<FilterDefinition *>(&fdi->GetDef());
	}
	return NULL;
}

void FilterAddBuiltin(const FilterDefinition *pfd) {
	VDASSERT(!pfd->stringProc || pfd->stringProc2);
	VDASSERT(!pfd->copyProc || pfd->copyProc2);

	vdautoptr<FilterDefinitionInstance> fdi(new FilterDefinitionInstance(NULL));
	fdi->Assign(*pfd, sizeof(FilterDefinition));

	g_filterDefs.AddTail(fdi.release());
}

void FilterRemove(FilterDefinition *fd) {
	// These calls are now ignored.
}

void VDFilterRemoveAll(VDExternalModule *module) {
	List2<FilterDefinitionInstance>::fwit it(g_filterDefs.begin());

	for(; it; ++it) {
		FilterDefinitionInstance& fd = *it;

		if (fd.GetModule() == module)
			fd.Deactivate();
	}
}

void FilterEnumerateFilters(std::list<FilterBlurb>& blurbs) {
	List2<FilterDefinitionInstance>::fwit it(g_filterDefs.begin());

	for(; it; ++it) {
		FilterDefinitionInstance& fd = *it;

		blurbs.push_back(FilterBlurb());
		FilterBlurb& fb = blurbs.back();

		fb.key			= &fd;
		fb.name			= fd.GetName();
		fb.author		= fd.GetAuthor();
		fb.description	= fd.GetDescription();
	}
}

void VDEnumerateFilters(vdfastvector<FilterDefinitionInstance *>& defs) {
	List2<FilterDefinitionInstance>::fwit it(g_filterDefs.begin());

	for(; it; ++it) {
		FilterDefinitionInstance& fd = *it;

		defs.push_back(&fd);
	}
}
