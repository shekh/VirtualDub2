//	VirtualDub - Video processing and capture application
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
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef f_FILTERCHAINDESC_H
#define f_FILTERCHAINDESC_H

#include <vd2/system/refcount.h>
#include "FilterInstance.h"

class FilterInstance;
class IVDPixmapViewDialog;

struct VDFilterChainEntry : public vdrefcount {
	VDFilterChainEntry();

	vdrefptr<FilterInstance> mpInstance;
	vdvector<VDStringA> mSources;
	VDStringA	mOutputName;
	vdrefptr<IVDPixmapViewDialog> mpView;
};

class VDFilterChainDesc {
	VDFilterChainDesc& operator=(const VDFilterChainDesc&);
public:
	VDFilterChainDesc();
	VDFilterChainDesc(const VDFilterChainDesc&);
	~VDFilterChainDesc();

	bool IsEmpty() const;

	void Clear();
	void AddEntry(VDFilterChainEntry *ent);

	typedef vdfastvector<VDFilterChainEntry *> Entries;
	Entries mEntries;
};

#endif
