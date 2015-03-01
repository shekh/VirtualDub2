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

#ifndef f_FRAMESUBSET_H
#define f_FRAMESUBSET_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <list>
#include <vd2/system/list.h>

class VDFraction;

struct FrameSubsetNode {
public:
	sint64 start;
	sint64 len;
	bool bMask;			// if set, all frames map to the previous frame
	int source;

	FrameSubsetNode() {}
	FrameSubsetNode(sint64 _s, sint64 _l, bool _bMask, int source_) : start(_s), len(_l), bMask(_bMask), source(source_) {}

	sint64 end() const { return start+len; }

	bool CanMergeBefore(const FrameSubsetNode& fsn) const {
		return start+len == fsn.start && bMask == fsn.bMask && source == fsn.source;
	}
};

class FrameSubset {
	typedef std::list<FrameSubsetNode> tTimeline;
public:
	typedef tTimeline::value_type			value_type;
	typedef tTimeline::reference			reference;
	typedef tTimeline::iterator				iterator;
	typedef tTimeline::const_reference		const_reference;
	typedef tTimeline::const_iterator		const_iterator;

	FrameSubset();
	FrameSubset(sint64 length);
	FrameSubset(const FrameSubset&);
	~FrameSubset();

	FrameSubset& operator=(const FrameSubset&);

	void clear();
	void addFrom(FrameSubset&);

	sint64 getTotalFrames() const;
	iterator addRange(sint64 start, sint64 len, bool bMask, int source);
	void addRangeMerge(sint64 start, sint64 len, bool bMask, int source);
	sint64 lookupFrame(sint64 frame) const {
		bool b;
		int source;

		return lookupFrame(frame, b, source);
	}
	sint64 lookupFrame(sint64 frame, bool& bMasked, int& source) const;
	sint64 revLookupFrame(sint64 frame, bool& bMasked) const;
	sint64 lookupRange(sint64 start, sint64& len) const {
		bool b;
		int source;

		return lookupRange(start, len, b, source);
	}
	sint64 lookupRange(sint64 start, sint64& len, bool& bMasked, int& source) const;
	void deleteInputRange(sint64 start, sint64 len);	// in source coordinates
	void deleteRange(sint64 start, sint64 len);	// in translated coordinates
	void setRange(sint64 start, sint64 len, bool bMask, int source);	// translated coordinates
	void clip(sint64 start, sint64 len);
	void offset(sint64 off);
	void trimInputRange(sint64 limit);

	////////////////////

	bool					empty() const		{ return mTimeline.empty(); }
	iterator				begin()				{ return mTimeline.begin(); }
	const_iterator			begin() const		{ return mTimeline.begin(); }
	iterator				end()				{ return mTimeline.end(); }
	const_iterator			end() const			{ return mTimeline.end(); }
	reference				front()				{ return mTimeline.front(); }
	const_reference			front() const		{ return mTimeline.front(); }
	reference				back()				{ return mTimeline.back(); }
	const_reference			back() const		{ return mTimeline.back(); }

	void assign(const FrameSubset& src, sint64 start, sint64 len);
	iterator erase(iterator it) {
		it = mTimeline.erase(it);
		invalidateCache();
		return it;
	}
	iterator erase(iterator it1, iterator it2) {
		it1 = mTimeline.erase(it1, it2);
		invalidateCache();
		return it1;
	}
	void insert(iterator it, const value_type& v) {
		FrameSubset tmp;
		tmp.mTimeline.push_back(v);
		tmp.invalidateCache();
		insert(it, tmp);
	}
	void insert(iterator it, const FrameSubset& src);
	void insert(sint64 insertionPoint, const FrameSubset& src);

	iterator findNode(sint64& poffset, sint64 iDstFrame);
	const_iterator findNode(sint64& poffset, sint64 iDstFrame) const;

	void swap(FrameSubset& x);

	void dump();

	void invalidateCache() {
		mCachedIterator = mTimeline.begin();
		mCachedPosition = 0;
	}

	void rescale(const VDFraction& oldRate, sint64 oldLength, const VDFraction& newRate, sint64 newLength);

protected:

	tTimeline				mTimeline;
	mutable iterator		mCachedIterator;
	mutable VDPosition		mCachedPosition;
};

#endif
