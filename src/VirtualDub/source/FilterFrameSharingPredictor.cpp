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

#include "stdafx.h"
#include "FilterFrameSharingPredictor.h"

VDFilterFrameSharingPredictor::VDFilterFrameSharingPredictor() {
	Clear();
}

void VDFilterFrameSharingPredictor::Clear() {
	LRUEntry dummy = { -1, true };
	for(int i=0; i<8; ++i)
		mLRU[i] = dummy;

	mShareCount = 8;
}

void VDFilterFrameSharingPredictor::OnRequest(sint64 frame) {
	for(int i=0; i<8; ++i) {
		LRUEntry& e = mLRU[i];

		if (e.mFrame == frame) {
			if (!e.mbShared) {
				e.mbShared = true;
				++mShareCount;
			}

			if (i) {
				const LRUEntry t(e);

				memmove(mLRU + 1, mLRU, i*sizeof(mLRU[0]));
				mLRU[0] = t;
			}

			return;
		}
	}

	if (mLRU[7].mbShared)
		--mShareCount;

	memmove(mLRU + 1, mLRU, 7*sizeof(mLRU[0]));
	mLRU[0].mFrame = frame;
	mLRU[0].mbShared = false;
}
