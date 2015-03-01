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

#include <vd2/system/error.h>
#include <vd2/system/registry.h>

#include "oshelper.h"

#include "MRUList.h"

MRUList::MRUList(int max_files, char *key_name)
	: mKey(max_files, 0)
	, mFiles(max_files)
	, mMaxCount(max_files)
	, mpKeyName(key_name)
	, mbDirty(false)
{
}

MRUList::~MRUList() {
	flush();
}

void MRUList::set_capacity(int max_files) {
	mMaxCount = max_files;
	mFiles.resize(max_files);
	mKey.resize(max_files, 0);
}

void MRUList::add(const wchar_t *file) {
	if (!mMaxCount)
		return;

	int index;

	// Does this file already exist?  If not, move it to the top.

	for(index=0; index<mMaxCount; index++) {
		int i = mKey[index];

		if (!i)
			continue;

		i -= 'a';

		if (!_wcsicmp(mFiles[i].c_str(), file)) {
			move_to_top(index);
			return;
		}
	}

	// Add file to list
	std::rotate(mKey.begin(), mKey.end()-1, mKey.end());

	if (mKey.front()) {
		index = mKey.front() - 'a';
	} else {
		index=0;
		while(!mFiles[index].empty())
			++index;
	}

	mFiles[index] = file;
	mKey.front() = (char)(index + 'a');

	mbDirty = true;
}

VDStringW MRUList::operator[](int i) {
	VDStringW s;
	if ((unsigned)i < mMaxCount && mKey[i]) {
		i = mKey[i] - 'a';

		s = mFiles[i];
	}
	return s;
}

void MRUList::move_to_top(int index) {
	if (index >= mMaxCount)
		return;

	// Move file to top of list
	if (index)
		std::rotate(mKey.begin(), mKey.begin()+index, mKey.begin()+index+1);
}

void MRUList::clear() {
	mKey.clear();
	mKey.resize(mMaxCount, 0);
	mFiles.clear();
	mFiles.resize(mMaxCount);

	mbDirty = true;
}

void MRUList::clear_history() {
	VDRegistryAppKey key(mpKeyName);

	VDRegistryKeyIterator it(key);

	while(const char *name = it.Next()) {
		key.removeValue(name);
	}

	clear();
}

void MRUList::load() {
	clear();

	VDRegistryAppKey key(mpKeyName);
	if (!key.isReady())
		return;

	VDStringA s;
	if (!key.getString("MRUList", s))
		return;

	int nItems = std::min<int>(mMaxCount, s.length());

	mKey.resize(mMaxCount, 0);

	for(int i=0; i<nItems; i++) {
		char name[2]={s[i], 0};

		if (!name[0])
			break;

		if (!key.getString(name, mFiles[i]))
			break;

		mKey[i] = (char)('a'+i);
	}
}

void MRUList::flush() {
	if (!mbDirty)
		return;

	VDRegistryAppKey key(mpKeyName);

	mKey.resize(mMaxCount+1, 0);
	key.setString("MRUList", &mKey[0]);
	mKey.resize(mMaxCount);

	for(int i=0; i<mMaxCount && mKey[i]; i++) {
		char name[2] = {mKey[i], 0};

		key.setString(name, mFiles[name[0] - 'a'].c_str());
	}

	mbDirty = false;
}
