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

#include "FrameSubset.h"
#include <vd2/system/error.h>
#include <vd2/system/fraction.h>

FrameSubset::FrameSubset() {
	invalidateCache();
}

FrameSubset::FrameSubset(sint64 length) {
	addRange(0, length, false, 0);
}

FrameSubset::FrameSubset(const FrameSubset& src)
	: mTimeline(src.mTimeline)
{
	invalidateCache();
}

FrameSubset::~FrameSubset() {
}

FrameSubset& FrameSubset::operator=(const FrameSubset& src) {
	mTimeline = src.mTimeline;
	invalidateCache();
	return *this;
}

void FrameSubset::clear() {
	mTimeline.clear();
	invalidateCache();
}

void FrameSubset::addFrom(FrameSubset& src) {
	for(iterator it(src.begin()), itEnd(src.end()); it!=itEnd; ++it)
		addRangeMerge(it->start, it->len, it->bMask, it->source);
}

FrameSubset::iterator FrameSubset::addRange(sint64 start, sint64 len, bool bMask, int source) {
	mTimeline.push_back(FrameSubsetNode(start, len, bMask, source));
	invalidateCache();

	iterator it(mTimeline.end());
	--it;

	return it;
}

void FrameSubset::addRangeMerge(sint64 start, sint64 len, bool bMask, int source) {
	tTimeline::iterator it(begin()), itEnd(end());

	while(it != itEnd) {
		if (start + len < it->start) {				// Isolated -- insert
			mTimeline.insert(it, FrameSubsetNode(start, len, bMask, source));
			invalidateCache();
			return;
		} else if (start + len >= it->start && start <= it->start+it->len) {		// Overlap!

			if (start+len > it->start+it->len) {	// < A [ B ] > or [ B <] A > cases
				// If the types are compatible, accumulate.  Otherwise, write out
				// the head portion if it exists, and trim to the tail.

				if (bMask != it->bMask) {
					if (start < it->start)
						mTimeline.insert(it, FrameSubsetNode(start, it->start - start, bMask, source));

					len -= (it->start + it->len - start);
					start = it->start + it->len;
				} else {
					if (it->start < start) {
						len += (start - it->start);
						start = it->start;
					}

					it = mTimeline.erase(it);
					continue;
				}
			} else {									// < A [> B ], <A | B>, or [ <A> B ] cases

				// Check the types.  If the types are compatible, great -- merge
				// the blocks and be done.  If the types are different, trim the
				// new block.

				if (bMask != it->bMask) {
					len = it->start - start;

					if (len > 0)
						mTimeline.insert(it, FrameSubsetNode(start, len, bMask, source));

					invalidateCache();
					return;
				} else if (it->start > start) {
					it->len += (it->start - start);
					it->start = start;
				}
#ifdef _DEBUG
				goto check_list;
#else
				invalidateCache();
				return;
#endif
			}
		}

		++it;
	}

	// List is empty or element falls after last element

	addRange(start, len, bMask, source);
	invalidateCache();

#ifdef _DEBUG
check_list:
	sint64 lastpt = -1;
	bool bLastWasMasked;

	for(it = begin(); it!=itEnd; ++it) {
		if (it->start <= lastpt && bLastWasMasked == it->bMask) {
			throw MyError("addRangeMerge: FAILED!!  %d <= %d\n", it->start, lastpt);
		}

		lastpt = it->start + it->len;
		bLastWasMasked = it->bMask;
	}

#endif
}

sint64 FrameSubset::getTotalFrames() const {
	sint64 iFrames = 0;

	for(const_iterator it(mTimeline.begin()), itEnd(mTimeline.end()); it!=itEnd; ++it)
		iFrames += it->len;

	return iFrames;
}

sint64 FrameSubset::lookupFrame(sint64 frame, bool& bMasked, int& source) const {
	sint64 len = 1;

	return lookupRange(frame, len, bMasked, source);
}

sint64 FrameSubset::revLookupFrame(sint64 frame, bool& bMasked) const {
	sint64 iSrcFrame = 0;

	for(const_iterator it(begin()), itEnd(end()); it!=itEnd; ++it) {
		if (frame >= it->start && frame < it->start+it->len) {
			bMasked = it->bMask;
			return iSrcFrame + (frame - it->start);
		}

		iSrcFrame += it->len;
	}

	return -1;
}

sint64 FrameSubset::lookupRange(sint64 start, sint64& len, bool& bMasked, int& source) const {
	sint64 offset;
	const_iterator it = findNode(offset, start);

	if (it == end()) return -1;

	source = it->source;
	bMasked = it->bMask;
	if (it->bMask) {
		len = 1;

		while(it->bMask) {
			if (it == begin()) {
				// First range is masked... this is bad.  Oh well, just
				// return the first frame in the first range.

				return it->start;
			}

			--it;
		}

		return it->start + it->len - 1;
	} else {
		len = it->len - offset;
		return it->start + offset;
	}
}

void FrameSubset::deleteInputRange(sint64 start, sint64 len) {
	for(iterator it(begin()), itEnd(end()); it != itEnd; ++it) {
		if (it->start >= start+len)
			break;

		if (it->start + it->len >= start && it->start < start+len) {
			bool bSectionBeforeDelete = it->start < start;
			bool bSectionAfterDelete = it->start + it->len > start + len;

			if (bSectionAfterDelete) {
				if (bSectionBeforeDelete)
					mTimeline.insert(it, FrameSubsetNode(it->start, start - it->start, it->bMask, it->source));

				it->len = (it->start + it->len) - (start+len);
				it->start = start+len;
				break;
			} else {
				if (bSectionBeforeDelete)			// before only
					it->len = start - it->start;
				else {								// absorbed
					it = mTimeline.erase(it);
					continue;
				}
			}
		}

		++it;
	}
	invalidateCache();
}

void FrameSubset::deleteRange(sint64 start, sint64 len) {
	sint64 offset;
	iterator it = findNode(offset, start), itEnd = end();

	while(it != itEnd && len>0) {
		VDASSERT(offset >= 0);
		if (len+offset < it->len) {
			if (offset)
				mTimeline.insert(it, FrameSubsetNode(it->start, offset, it->bMask, it->source));

			it->start += (offset+len);
			it->len -= (offset+len);

			VDASSERT(it->len > 0);

			break;
		} else {
			if (offset) {
				len -= it->len - offset;
				it->len = offset;
				offset = 0;
				++it;
			} else {
				len -= it->len;
				it = mTimeline.erase(it);
				continue;
			}
		}
	}
	invalidateCache();
}

void FrameSubset::setRange(sint64 start, sint64 len, bool bMask, int source) {
	sint64 offset;
	iterator it = findNode(offset, start), itEnd(end());

	while(it != itEnd && len>0) {
		FrameSubsetNode& fsn = *it;

		// Check if the operation ends before the end of this segment.
		if (len+offset < fsn.len) {
			if (fsn.bMask != bMask) {
				// insert pre-segment
				if (offset)
					mTimeline.insert(it, FrameSubsetNode(fsn.start, offset, fsn.bMask, fsn.source));

				// insert changed segment
				FrameSubsetNode temp(fsn.start + offset, len, bMask, source);

				if (it != mTimeline.begin()) {
					iterator it2(it);
					--it2;

					if (it2->CanMergeBefore(temp)) {
						it2->len += len;
						goto no_insert_required;
					}
				}

				mTimeline.insert(it, temp);

no_insert_required:
				// adjust post-segment
				fsn.start += (offset+len);
				fsn.len -= (offset+len);
			}

			break;
		}

		if (offset) {
			len -= (fsn.len-offset);

			if (fsn.bMask != bMask) {
				mTimeline.insert(it, FrameSubsetNode(fsn.start, offset, fsn.bMask, source));

				fsn.start += offset;
				fsn.len -= offset;
				fsn.bMask = bMask;
			}

			offset = 0;
		} else {
			fsn.bMask = bMask;
			len -= fsn.len;

			// check for a possible merge beforehand
			if (it != mTimeline.begin()) {
				iterator it2(it);
				--it2;

				if (it2->CanMergeBefore(fsn)) {
					fsn.len += it2->len;
					fsn.start -= it2->len;
					mTimeline.erase(it2);
				}
			}
		}

		// check for a possible merge after
		if (++it == itEnd)
			break;

		if (fsn.CanMergeBefore(*it)) {
			fsn.len += it->len;
			len -= it->len;				// Note: may cause len<0. That's OK because the mask type is already correct for the second node.
			it = mTimeline.erase(it);
		}

		continue;
	}
	invalidateCache();
}

void FrameSubset::clip(sint64 start, sint64 len) {
	deleteRange(0, start);
	deleteRange(len, 0x7FFFFFFF - len);
}

void FrameSubset::offset(sint64 off) {
	for(iterator it = begin(), itEnd = end(); it != itEnd; ++it)
		it->start += off;
	invalidateCache();
}

void FrameSubset::trimInputRange(sint64 limit) {
	iterator it(mTimeline.begin()), itEnd(mTimeline.end());
	
	while(it != itEnd) {
		FrameSubsetNode& fsn = *it;

		// check if this range is entirely off the end
		if (fsn.start >= limit)
			it = mTimeline.erase(it);
		else {
			// check if this range is partially beyond the end
			if (fsn.start + fsn.len > limit)
				fsn.len = limit - fsn.start;

			++it;
		}
	}

	invalidateCache();
}

void FrameSubset::assign(const FrameSubset& src, sint64 start, sint64 len) {
	mTimeline = src.mTimeline;
	invalidateCache();
	clip(start, len);
}

void FrameSubset::insert(iterator it, const FrameSubset& src) {
	if (src.empty())
		return;

	const_iterator itSrc(src.begin()), itSrcEnd(src.end());

	iterator itFront = mTimeline.insert(it, *itSrc);
	iterator itBack = itFront;
	++itSrc;

	for(; itSrc != itSrcEnd; ++itSrc)
		itBack = mTimeline.insert(++itBack, *itSrc);

	// check for merge in front

	if (itFront != begin()) {
		iterator itBeforeFront = itFront;
		--itBeforeFront;
		if (itBeforeFront->bMask == itFront->bMask && itBeforeFront->start + itBeforeFront->len == itFront->start) {
			itFront->len += itBeforeFront->len;
			itFront->start -= itBeforeFront->len;
			mTimeline.erase(itBeforeFront);
		}
	}

	// check for merge in back
	iterator itAfterBack = itBack;
	++itAfterBack;
	if (itAfterBack != end()) {
		if (itBack->bMask != itAfterBack->bMask && itBack->start + itBack->len == itAfterBack->start) {
			itBack->len += itAfterBack->len;
			mTimeline.erase(itAfterBack);
		}
	}

	invalidateCache();
}

void FrameSubset::insert(sint64 insertionPoint, const FrameSubset& src) {
	sint64 offset = 0;
	FrameSubset::iterator it;
	
	if (insertionPoint < 0)
		it = begin();
	else
		it = findNode(offset, insertionPoint);

	if (it != end() && offset > 0) {
		mTimeline.insert(it, FrameSubsetNode(it->start, offset, it->bMask, it->source));
		it->start += offset;
		it->len -= offset;
	}

	insert(it, src);
	invalidateCache();
}

FrameSubset::const_iterator FrameSubset::findNode(sint64& poffset, sint64 iDstFrame) const {
	return const_cast<FrameSubset *>(this)->findNode(poffset, iDstFrame);
}

FrameSubset::iterator FrameSubset::findNode(sint64& poffset, sint64 iDstFrame) {
	if (iDstFrame<0)
		return end();

	iterator it(begin()), itEnd(end());

	if (iDstFrame >= mCachedPosition) {
		iDstFrame -= mCachedPosition;
		it = mCachedIterator;
	} else {
		mCachedPosition = 0;
	}

	for(; it!=itEnd && iDstFrame >= 0; ++it) {
		if (iDstFrame < it->len) {
			poffset = iDstFrame;
			mCachedIterator = it;
			return it;
		}

		iDstFrame -= it->len;
		mCachedPosition += it->len;
	}

	mCachedIterator = it;
	poffset = 0;
	return end();
}

void FrameSubset::swap(FrameSubset& x) {
	mTimeline.swap(x.mTimeline);
	invalidateCache();
	x.invalidateCache();
}

void FrameSubset::dump() {
#ifdef _DEBUG
	VDDEBUG("Frame subset dump:\n");
	for(const_iterator it(begin()), itEnd(end()); it!=itEnd; ++it) {
		VDDEBUG("   start: %6I64d   len:%4I64d   bMask:%d\n", it->start, it->len, it->bMask);
	}
#endif
}

void FrameSubset::rescale(const VDFraction& oldRate, sint64 oldLength, const VDFraction& newRate, sint64 newLength) {
	double rateFactor = newRate.asDouble() / oldRate.asDouble();

	tTimeline tmp;
	mTimeline.swap(tmp);
	invalidateCache();

	for(tTimeline::const_iterator it(tmp.begin()), itEnd(tmp.end()); it!=itEnd; ++it) {
		const FrameSubsetNode& fsn = *it;

		sint64 start = fsn.start;
		sint64 len = fsn.len;

		sint64 oldEnd = start + len;
		sint64 newStart = VDCeilToInt64((double)start * rateFactor - 0.5);
		sint64 newEnd = VDCeilToInt64((double)(start + len) * rateFactor - 0.5);

		if (newEnd > newLength)
			newEnd = newLength;
		else if (newEnd < newLength && oldEnd == oldLength)
			newEnd = newLength;

		if (newEnd > newStart)
			addRange(newStart, newEnd - newStart, fsn.bMask, fsn.source);
	}
}

///////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
class FrameSubsetClassVerifier {
public:
	void check(FrameSubset& fs, int test, ...) {
		va_list val;
		FrameSubset::iterator pfsn = fs.begin();

		va_start(val, test);
		while(pfsn != fs.end()) {
			if (pfsn->start != va_arg(val, int)) {
				fs.dump();
				throw MyError("fail test #%dA", test);
			}
			if (pfsn->len != va_arg(val, int)) {
				fs.dump();
				throw MyError("fail test #%dB", test);
			}

			++pfsn;
		}
		if (va_arg(val, int) != -1) {
			fs.dump();
			throw MyError("fail test #%dC", test);
		}

		va_end(val);

	}

	FrameSubsetClassVerifier() {
		_RPT0(0,"Verifying class: FrameSubset\n");
		try {
			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false, 0);
				fs.addRangeMerge(30, 10, false, 0);
				fs.addRangeMerge(50, 10, false, 0);
				check(fs, 1, 10, 10, 30, 10, 50, 10, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false, 0);
				fs.addRangeMerge(20, 10, false, 0);
				fs.addRangeMerge(30, 10, false, 0);
				check(fs, 2, 10, 30, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false, 0);
				fs.addRangeMerge(20, 10, false, 0);
				fs.addRangeMerge(50, 10, false, 0);
				check(fs, 3, 10, 20, 50, 10, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false, 0);
				fs.addRangeMerge(40, 10, false, 0);
				fs.addRangeMerge(50, 10, false, 0);
				check(fs, 4, 10, 10, 40, 20, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false, 0);
				fs.addRangeMerge(15, 10, false, 0);
				fs.addRangeMerge(50, 10, false, 0);
				check(fs, 5, 10, 15, 50, 10, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false, 0);
				fs.addRangeMerge(45, 10, false, 0);
				fs.addRangeMerge(50, 10, false, 0);
				check(fs, 6, 10, 10, 45, 15, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false, 0);
				fs.addRangeMerge(15, 30, false, 0);
				fs.addRangeMerge(50, 10, false, 0);
				check(fs, 7, 10, 35, 50, 10, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false, 0);
				fs.addRangeMerge(8, 48, false, 0);
				fs.addRangeMerge(50, 10, false, 0);
				check(fs, 8, 8, 52, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false, 0);
				fs.addRangeMerge(8, 100, false, 0);
				fs.addRangeMerge(50, 10, false, 0);
				check(fs, 9, 8, 100, -1);
			}

			{
				FrameSubset fs;

				fs.addRange(110, 20, false, 0);
				fs.addRange(100, 20, false, 0);
				fs.trimInputRange(130);
				check(fs, 10, 110, 20, 100, 20, -1);
				fs.trimInputRange(120);
				check(fs, 11, 110, 10, 100, 20, -1);
				fs.trimInputRange(110);
				check(fs, 12, 100, 10, -1);
				fs.trimInputRange(100);
				check(fs, 13, -1);
			}

			// addRange() tests
			{
				FrameSubset fs;

				fs.addRange(100, 30, false, 0);

				fs.setRange(10, 10, true, 0);
				check(fs, __LINE__, 100, 10, 110, 10, 120, 10, -1);
			}
			{
				FrameSubset fs;

				fs.addRange(100, 10, false, 0);
				fs.addRange(120, 10, false, 0);

				fs.setRange(5, 10, true, 0);
				check(fs, __LINE__, 100, 5, 105, 5, 120, 5, 125, 5, -1);
			}
			{
				FrameSubset fs;

				fs.addRange(100, 10, false, 0);
				fs.addRange(110, 10, true, 0);
				fs.addRange(120, 10, false, 0);

				fs.setRange(10, 10, false, 0);
				check(fs, __LINE__, 100, 30, -1);
			}

		} catch(const MyError& e) {
			VDDEBUG("%s", e.gets());
			VDBREAK;
		}
	}
} g_ClassVerifyFrameSubset;
#endif
