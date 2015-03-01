//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2009 Avery Lee
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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

#ifndef f_VD2_FILTERFRAMESHARINGPREDICTOR_H
#define f_VD2_FILTERFRAMESHARINGPREDICTOR_H

class VDFilterFrameSharingPredictor {
public:
	VDFilterFrameSharingPredictor();

	void Clear();
	void OnRequest(sint64 frame);

	bool IsSharingPredicted(sint64 frame) const {
		return mShareCount > 0;
	}

protected:
	struct LRUEntry {
		sint64	mFrame;
		bool	mbShared;
	};

	uint32		mShareCount;
	LRUEntry	mLRU[8];
};

#endif
