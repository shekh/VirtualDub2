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

#ifndef f_MRULIST_H
#define f_MRULIST_H

#include <vector>
#include <vd2/system/VDString.h>

class MRUList {
private:
	std::vector<char>			mKey;
	std::vector<VDStringW>		mFiles;
	const char		*mpKeyName;
	int		mMaxCount;
	bool			mbDirty;

public:
	MRUList(int max_files, char *key);
	~MRUList();

	VDStringW operator[](int i);

	void set_capacity(int max_files);

	void add(const wchar_t *file);
	void move_to_top(int index);
	void clear();
	void clear_history();
	void load();
	void flush();
};

#endif
