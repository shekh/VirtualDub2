//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2004 Avery Lee
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

#include "DubSource.h"
#include "VideoSource.h"
#include "timeline.h"

VDTimeline::VDTimeline() {
}

VDTimeline::~VDTimeline() {
}

void VDTimeline::SetFromSource() {
	mSubset.clear();
	mSubset.insert(mSubset.begin(), FrameSubsetNode(mpTiming->GetStart(), mpTiming->GetLength(), false, 0));
}

VDPosition VDTimeline::GetNearestKey(VDPosition pos) {
	if (pos <= 0)
		return 0;

	sint64 offset;
	FrameSubset::iterator it(mSubset.findNode(offset, pos)), itBegin(mSubset.begin()), itEnd(mSubset.end());

	do {
		if (it!=itEnd) {
			const FrameSubsetNode& fsn0 = *it;

			if (!fsn0.bMask) {
				if (mpTiming)
					pos = mpTiming->GetNearestKey(fsn0.start + offset) - fsn0.start;
				else
					pos = offset;

				if (pos >= 0)
					break;
			}
		}

		while(it != itBegin) {
			--it;
			const FrameSubsetNode& fsn = *it;

			if (!fsn.bMask) {
				if (mpTiming)
					pos = mpTiming->GetNearestKey(fsn.start + fsn.len - 1) - fsn.start;
				else
					pos = fsn.len - 1;

				if (pos >= 0)
					break;
			}

			pos = 0;
		}
	} while(false);

	while(it != itBegin) {
		--it;
		const FrameSubsetNode& fsn2 = *it;

		pos += fsn2.len;
	}

	return pos;
}

VDPosition VDTimeline::GetNearestKeyNext(VDPosition pos) {
	VDPosition newpos = GetNearestKey(pos);

	if (newpos < pos) {
		VDPosition newpos2 = GetNextKey(pos);

		if (newpos2 > pos)
			return newpos2;
	}

	return newpos;
}

VDPosition VDTimeline::GetPrevKey(VDPosition pos) {
	if (pos <= 0)
		return -1;

	if (!mpTiming)
		return pos - 1;

	sint64 offset;
	FrameSubset::iterator it(mSubset.findNode(offset, pos)), itBegin(mSubset.begin()), itEnd(mSubset.end());

	do {
		if (it!=itEnd) {
			const FrameSubsetNode& fsn0 = *it;

			if (!fsn0.bMask) {
				pos = mpTiming->GetPrevKey(fsn0.start + offset) - fsn0.start;

				if (pos >= 0)
					break;
			}
		}

		while(it != itBegin) {
			--it;
			const FrameSubsetNode& fsn = *it;

			if (!fsn.bMask) {
				pos = mpTiming->GetNearestKey(fsn.start + fsn.len - 1) - fsn.start;

				if (pos >= 0)
					break;
			}

			pos = 0;
		}
	} while(false);

	while(it != itBegin) {
		--it;
		const FrameSubsetNode& fsn2 = *it;

		pos += fsn2.len;
	}

	return pos;
}

VDPosition VDTimeline::GetNextKey(VDPosition pos) {
	if (pos >= mSubset.getTotalFrames() - 1)
		return -1;

	if (!mpTiming)
		return pos + 1;

	else {
		sint64 offset;
		FrameSubset::iterator it(mSubset.findNode(offset, pos)), itBegin(mSubset.begin()), itEnd(mSubset.end());

		do {
			if (it==itEnd) {
				VDASSERT(false);
				return -1;
			}

			const FrameSubsetNode& fsn0 = *it;

			if (!fsn0.bMask) {
				pos = mpTiming->GetNextKey(fsn0.start + offset) - fsn0.start;

				if (pos >= 0 && pos < fsn0.len)
					break;

				pos = 0;
			}

			for(;;) {
				if (++it == itEnd)
					return -1;

				const FrameSubsetNode& fsn = *it;

				if (!fsn.bMask) {
					pos = 0;
					if (mpTiming->IsKey(fsn.start))
						break;

					pos = mpTiming->GetNextKey(fsn.start) - fsn.start;

					if (pos >= 0 && pos < fsn.len)
						break;
				}

				pos = 0;
			}
		} while(false);

		while(it != itBegin) {
			--it;
			const FrameSubsetNode& fsn2 = *it;

			pos += fsn2.len;
		}
	}

	return pos;
}

VDPosition VDTimeline::GetPrevDrop(VDPosition pos) {
	if (!mpTiming)
		return -1;

	while(--pos >= 0) {
		VDPosition srcPos = mSubset.lookupFrame(pos);

		if (mpTiming->IsNullSample(srcPos))
			return pos;
	}

	return pos;
}

VDPosition VDTimeline::GetNextDrop(VDPosition pos) {
	if (!mpTiming)
		return -1;

	const VDPosition len = mSubset.getTotalFrames();

	while(++pos < len) {
		VDPosition srcPos = mSubset.lookupFrame(pos);

		if (mpTiming->IsNullSample(srcPos))
			return pos;
	}

	return -1;
}

VDPosition VDTimeline::GetPrevEdit(VDPosition pos) {
	sint64 offset;

	FrameSubset::iterator pfsn = mSubset.findNode(offset, pos);

	if (pfsn == mSubset.end()) {
		if (pos >= 0) {
			if (pfsn != mSubset.begin()) {
				--pfsn;
				return mSubset.getTotalFrames() - pfsn->len;
			}
		}
		return -1;
	}
	
	if (offset)
		return pos - offset;

	if (pfsn != mSubset.begin()) {
		--pfsn;
		return pos - pfsn->len;
	}

	return -1;
}

VDPosition VDTimeline::GetNextEdit(VDPosition pos) {
	sint64 offset;

	FrameSubset::iterator pfsn = mSubset.findNode(offset, pos);

	if (pfsn == mSubset.end())
		return -1;

	pos -= offset;
	pos += pfsn->len;

	++pfsn;

	if (pfsn == mSubset.end())
		return -1;

	return pos;
}

VDPosition VDTimeline::TimelineToSourceFrame(VDPosition pos) {
	return mSubset.lookupFrame(pos);
}

void VDTimeline::Rescale(const VDFraction& oldRate, sint64 oldLength, const VDFraction& newRate, sint64 newLength) {
	mSubset.rescale(oldRate, oldLength, newRate, newLength);
}

///////////////////////////////////////////////////////////////////////////////

class VDTimelineTimingSourceVS : public vdrefcounted<IVDTimelineTimingSource> {
public:
	VDTimelineTimingSourceVS(IVDVideoSource *pVS);
	~VDTimelineTimingSourceVS();

	sint64 GetStart();
	sint64 GetLength();
	const VDFraction GetRate();
	sint64 GetPrevKey(sint64 pos);
	sint64 GetNextKey(sint64 pos);
	sint64 GetNearestKey(sint64 pos);
	bool IsKey(sint64 pos);
	bool IsNullSample(sint64 pos);

protected:
	vdrefptr<IVDVideoSource> mpVS;
	IVDStreamSource *mpSS;
};

VDTimelineTimingSourceVS::VDTimelineTimingSourceVS(IVDVideoSource *pVS)
	: mpVS(pVS)
	, mpSS(pVS->asStream())
{
}

VDTimelineTimingSourceVS::~VDTimelineTimingSourceVS() {
}

sint64 VDTimelineTimingSourceVS::GetStart() {
	return mpSS->getStart();
}

sint64 VDTimelineTimingSourceVS::GetLength() {
	return mpSS->getLength();
}

const VDFraction VDTimelineTimingSourceVS::GetRate() {
	return mpSS->getRate();
}

sint64 VDTimelineTimingSourceVS::GetPrevKey(sint64 pos) {
	return mpVS->prevKey(pos);
}

sint64 VDTimelineTimingSourceVS::GetNextKey(sint64 pos) {
	return mpVS->nextKey(pos);
}

sint64 VDTimelineTimingSourceVS::GetNearestKey(sint64 pos) {
	return mpVS->nearestKey(pos);
}

bool VDTimelineTimingSourceVS::IsKey(sint64 pos) {
	return mpVS->isKey(pos);
}

bool VDTimelineTimingSourceVS::IsNullSample(sint64 pos) {
	int err;
	uint32 lBytes, lSamples;

	err = mpSS->read(pos, 1, NULL, 0, &lBytes, &lSamples);
	if (err != IVDStreamSource::kOK)
		return false;

	return lBytes == 0;
}

void VDCreateTimelineTimingSourceVS(IVDVideoSource *pVS, IVDTimelineTimingSource **ppTS) {
	*ppTS = new VDTimelineTimingSourceVS(pVS);
	(*ppTS)->AddRef();
}
