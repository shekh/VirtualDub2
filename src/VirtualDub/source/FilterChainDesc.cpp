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

#include <stdafx.h>
#include <FilterChainDesc.h>

VDFilterChainEntry::VDFilterChainEntry() {
}

VDFilterChainDesc::VDFilterChainDesc() {
}

VDFilterChainDesc::VDFilterChainDesc(const VDFilterChainDesc& src) {
	for(Entries::const_iterator it(mEntries.begin()), itEnd(mEntries.end());
		it != itEnd;
		++it)
	{
		VDFilterChainEntry *ent = new VDFilterChainEntry(**it);

		ent->AddRef();
		mEntries.push_back(ent);
	}
}

VDFilterChainDesc::~VDFilterChainDesc() {
	Clear();
}

bool VDFilterChainDesc::IsEmpty() const {
	return mEntries.empty();
}

void VDFilterChainDesc::Clear() {
	while(!mEntries.empty()) {
		VDFilterChainEntry *ent = mEntries.back();
		mEntries.pop_back();

		ent->Release();
	}
}

void VDFilterChainDesc::AddEntry(VDFilterChainEntry *ent) {
	mEntries.push_back(ent);
	ent->AddRef();
}

