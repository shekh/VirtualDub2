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

#ifndef f_FILTERS_H
#define f_FILTERS_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <list>
#include <vd2/system/list.h>
#include <vd2/system/VDString.h>
#include "FilterChainDesc.h"
#include "FilterSystem.h"
#include "filter.h"

//////////////////

class VDTimeline;
struct VDXFilterDefinition;
class VDExternalModule;

///////////////////

class FilterDefinitionInstance : public ListNode2<FilterDefinitionInstance> {
public:

	FilterDefinitionInstance(VDExternalModule *pfm);
	~FilterDefinitionInstance();

	void Assign(const FilterDefinition& def, int len);
	void AssignFilterMod(const FilterModDefinition& def, int len);
	void Deactivate();

	const FilterDefinition& Attach();
	void Detach();

	int	GetAPIVersion() const { return mAPIVersion; }

	const FilterDefinition& GetDef() const { return mDef; }
	const FilterModDefinition& GetFilterModDef() const { return mFilterModDef; }
	VDExternalModule	*GetModule() const { return mpExtModule; }

	const VDStringA&	GetName() const { return mName; }
	const VDStringA&	GetAuthor() const { return mAuthor; }
	const VDStringA&	GetDescription() const { return mDescription; }

	bool				HasStaticAbout() const { return mbHasStaticAbout; }
	bool				HasStaticConfigure() const { return mbHasStaticConfigure; }

protected:
	VDExternalModule	*mpExtModule;
	int					mAPIVersion;
	VDXFilterDefinition	mDef;
	FilterModDefinition	mFilterModDef;
	VDAtomicInt			mRefCount;
	VDStringA			mName;
	VDStringA			mAuthor;
	VDStringA			mDescription;
	bool				mbHasStaticAbout;
	bool				mbHasStaticConfigure;
};

//////////

extern VDFilterChainDesc	g_filterChain;

extern FilterSystem	filters;

FilterDefinitionInstance *FilterBaseAdd(VDXFilterModule *fm, VDXFilterDefinition *pfd, int fd_len);
FilterDefinition *FilterAdd(VDXFilterModule *fm, VDXFilterDefinition *pfd, int fd_len);
FilterDefinition *FilterModAdd(VDXFilterModule *fm, VDXFilterDefinition *pfd, int fd_len, FilterModDefinition *pfm, int fm_len);
void				FilterAddBuiltin(const VDXFilterDefinition *pfd);
void				FilterRemove(VDXFilterDefinition *fd);
void				VDFilterRemoveAll(VDExternalModule *module);

struct FilterBlurb {
	FilterDefinitionInstance	*key;
	VDStringA					name;
	VDStringA					author;
	VDStringA					description;
	VDStringW					module;
	bool						hide;
};

void				FilterEnumerateFilters(std::list<FilterBlurb>& blurbs);
void				VDEnumerateFilters(vdfastvector<FilterDefinitionInstance *>& defs);

#endif
