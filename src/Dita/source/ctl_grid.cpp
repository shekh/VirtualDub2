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

#include <stdafx.h>
#include <vd2/Dita/basetypes.h>
#include <vector>

class VDUIGrid : public VDUIWindow, public IVDUIGrid {
public:
	VDUIGrid();
	~VDUIGrid();

	void *AsInterface(uint32 id);

	bool Create(IVDUIParameters *pParams);
	void AddChild(IVDUIWindow *pWindow);
	void AddChild(IVDUIWindow *pWin, int col, int row, int colspan, int rowspan);
	void RemoveChild(IVDUIWindow *pWindow);
	void PreLayoutBase(const VDUILayoutSpecs&);
	void PostLayoutBase(const vduirect&);

	void SetRow(int row, int minsize=-1, int maxsize=-1, int affinity=-1);
	void SetColumn(int col, int minsize=-1, int maxsize=-1, int affinity=-1);
	void NextRow();

protected:
	int mnFillCount;
	int mSpacing;

	int mX, mY;
	bool mbVerticalTravel;

	class Axis {
	public:
		Axis();

		void Extend(int cell);
		void SetBehavior(int cell, int minsize, int maxsize, int affinity);
		void BeginLayout();
		int GetSpanMax(int start, int end, int limit) const;
		int GetSpanWidth(int start, int end) const { return mEntries[end-1].end - mEntries[start].start; }
		void ApplyMinimum(int start, int end, int minval);
		int GetMinimumSum() const;
		int GetCount() const { return mEntries.size()-1; }
		int GetStart(int i) const { return mEntries[i].start; }
		int GetEnd(int i) const { return mEntries[i].end; }
		void Layout(int pos, int width, int pad);

	protected:
		struct Entry {
			int minsize;
			int maxsize;
			int maxsizesum;
			int affinity;
			int affinitysum;
			int mincursize;
			int start, end;

			Entry() : minsize(0), maxsize(INT_MAX), affinity(0) {}
		};

		typedef std::vector<Entry> tEntries;
		tEntries	mEntries;
	};

	Axis	mRows;
	Axis	mCols;

	struct GridItem {
		IVDUIWindow *mpWin;
		vduirect	mPos;
	};
	typedef std::vector<GridItem> tItems;

	tItems		mItems;
};

IVDUIWindow *VDCreateUIGrid() { return new VDUIGrid; }

VDUIGrid::VDUIGrid() {
	mAlignX = nsVDUI::kFill;
	mAlignY = nsVDUI::kFill;
	mSpacing = 0;
	mX = 0;
	mY = 0;
	mbVerticalTravel = false;
}

VDUIGrid::~VDUIGrid() {
}

void *VDUIGrid::AsInterface(uint32 id) {
	switch(id) {
	case IVDUIGrid::kTypeID: return static_cast<IVDUIGrid *>(this);
	}

	return VDUIWindow::AsInterface(id);
}

bool VDUIGrid::Create(IVDUIParameters *pParams) {
	mSpacing = pParams->GetI(nsVDUI::kUIParam_Spacing, 0);
	mbVerticalTravel = pParams->GetB(nsVDUI::kUIParam_IsVertical, false);
	return VDUIWindow::Create(pParams);
}

void VDUIGrid::AddChild(IVDUIWindow *pWindow) {
	AddChild(pWindow, -1, -1, 1, 1);
}

void VDUIGrid::AddChild(IVDUIWindow *pWin, int col, int row, int colspan, int rowspan) {
	VDASSERT(rowspan>=1 && colspan>=1);

	if (col >= 0)
		mX = col;

	if (row >= 0)
		mY = row;

	tChildren::iterator it(std::find(mChildren.begin(), mChildren.end(), pWin));

	if (it == mChildren.end()) {
		mChildren.push_back(pWin);
		pWin->AddRef();
		pWin->SetParent(this);

		GridItem item;
		
		item.mpWin = pWin;
		item.mPos = vduirect(mX, mY, mX+colspan, mY+rowspan);
		mItems.push_back(item);

		mCols.Extend(mX);
		mRows.Extend(mY);

		if (mbVerticalTravel)
			mY += rowspan;
		else
			mX += colspan;
	}
}

void VDUIGrid::RemoveChild(IVDUIWindow *pWindow) {
	tItems::iterator it(mItems.begin()), itEnd(mItems.end());

	for(; it!=itEnd; ++it) {
		GridItem& item = *it;

		if (item.mpWin == pWindow) {
			mItems.erase(it);
			pWindow->SetParent(NULL);
			break;
		}
	}

	VDUIWindow::RemoveChild(pWindow);
}

void VDUIGrid::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	vduisize pad = mpBase->MapUnitsToPixels(vduisize(mSpacing, mSpacing));

	mLayoutSpecs.minsize.w = 0;
	mLayoutSpecs.minsize.h = 0;

	const int rows = mRows.GetCount();
	const int cols = mCols.GetCount();

	mRows.BeginLayout();
	mCols.BeginLayout();

	// phase I: constraint gathering
	tItems::const_iterator itItem(mItems.begin()), itItemEnd(mItems.end());
	for(; itItem != itItemEnd; ++itItem) {
		const GridItem& item = *itItem;
		const vduirect& cells = item.mPos;

		VDUILayoutSpecs cellConstraints;

		cellConstraints.minsize.w = mCols.GetSpanMax(cells.left, cells.right, parentConstraints.minsize.w);
		cellConstraints.minsize.h = mRows.GetSpanMax(cells.top, cells.bottom, parentConstraints.minsize.h);

		item.mpWin->PreLayout(cellConstraints);

		const VDUILayoutSpecs& specs = item.mpWin->GetLayoutSpecs();
		const int minw = specs.minsize.w;
		const int minh = specs.minsize.h;

		mCols.ApplyMinimum(cells.left, cells.right, minw);
		mRows.ApplyMinimum(cells.top, cells.bottom, minh);
	}

	mLayoutSpecs.minsize.w = pad.w*(cols-1) + mCols.GetMinimumSum();
	mLayoutSpecs.minsize.h = pad.h*(rows-1) + mRows.GetMinimumSum();
}

void VDUIGrid::PostLayoutBase(const vduirect& target) {
	vduisize pad = mpBase->MapUnitsToPixels(vduisize(mSpacing, mSpacing));

	// phase II: layout columns
	mCols.Layout(target.left, target.width(), pad.w);

	tItems::const_iterator itItem(mItems.begin()), itItemEnd(mItems.end());
	for(; itItem != itItemEnd; ++itItem) {
		const GridItem& item = *itItem;
		const vduirect& cells = item.mPos;

		VDUILayoutSpecs cellConstraints;

		cellConstraints.minsize.w = mCols.GetSpanWidth(cells.left, cells.right);
		cellConstraints.minsize.h = target.height();

		item.mpWin->PreLayout(cellConstraints);

		const VDUILayoutSpecs& specs = item.mpWin->GetLayoutSpecs();
		const int minh = specs.minsize.h;

		mRows.ApplyMinimum(cells.top, cells.bottom, minh);
	}

	// phase III: final layout
	mRows.Layout(target.top, target.height(), pad.h);

	itItem = mItems.begin();
	itItemEnd = mItems.end();
	for(; itItem != itItemEnd; ++itItem) {
		const GridItem& item = *itItem;
		const vduirect& cells = item.mPos;

		item.mpWin->PostLayout(vduirect(mCols.GetStart(cells.left), mRows.GetStart(cells.top), mCols.GetEnd(cells.right-1), mRows.GetEnd(cells.bottom-1)));
	}
}

void VDUIGrid::SetRow(int row, int minsize, int maxsize, int affinity) {
	mRows.SetBehavior(row, minsize, maxsize, affinity);
}

void VDUIGrid::SetColumn(int col, int minsize, int maxsize, int affinity) {
	mCols.SetBehavior(col, minsize, maxsize, affinity);
}

void VDUIGrid::NextRow() {
	if (mbVerticalTravel) {
		++mX;
		mY = 0;
	} else {
		++mY;
		mX = 0;
	}
}

///////////////////////////////////////////////////////////////////////////

VDUIGrid::Axis::Axis() {
	Extend(0);
}

void VDUIGrid::Axis::Extend(int cell) {
	VDASSERT(cell >= 0);

	if ((size_t)(cell+1) >= mEntries.size())		// need sentinel at end too
		mEntries.resize(cell+2);
}

void VDUIGrid::Axis::SetBehavior(int cell, int minsize, int maxsize, int affinity) {
	Extend(cell);

	Entry& ent = mEntries[cell];

	if (minsize >= 0)
		ent.minsize = minsize;

	if (maxsize >= 0)
		ent.maxsize = maxsize;

	if (ent.maxsize < ent.minsize)
		ent.maxsize = ent.minsize;

	if (affinity >= 0)
		ent.affinity = affinity;
}

void VDUIGrid::Axis::BeginLayout() {
	tEntries::iterator it(mEntries.begin()), itEnd(mEntries.end());

	int affinitysum = 0;
	int maxsizesum = 0;
	for(; it!=itEnd; ++it) {
		Entry& ent = *it;

		ent.mincursize = ent.minsize;
		ent.affinitysum = affinitysum;
		ent.maxsizesum = maxsizesum;
		affinitysum += ent.affinity;

		if (ent.maxsize == INT_MAX)
			++maxsizesum;
		else
			maxsizesum += ent.maxsize << 12;
	}
}

int VDUIGrid::Axis::GetSpanMax(int start, int end, int limit) const {
	VDASSERT(start >= 0);
	VDASSERT((size_t)end < mEntries.size());

	int diff = mEntries[end].maxsizesum - mEntries[start].maxsizesum;

	if (diff & 0xfff)
		return limit;

	diff >>= 12;

	if (diff > limit)
		diff = limit;

	return diff;
}

void VDUIGrid::Axis::ApplyMinimum(int start, int end, int minval) {
	VDASSERT(start >= 0);
	VDASSERT((size_t)end < mEntries.size());

	// fast path
	if (end == start+1) {
		int& curmin = mEntries[start].mincursize;

		if (curmin < minval)
			curmin = minval;

		return;
	}

	const int affsum = mEntries[end].affinitysum - mEntries[start].affinitysum;
	int affleft = affsum ? affsum : end-start;

	for(int i=start; i<end; ++i) {
		Entry& ent = mEntries[i];
		int affinity = affsum ? ent.affinity : 1;
		int minslice = affinity ? (minval * affinity + affleft - 1) / affleft : 0;

		if (ent.mincursize < minslice)
			ent.mincursize = minslice;

		minval -= minslice;
	}
}

int VDUIGrid::Axis::GetMinimumSum() const {
	const int ents = mEntries.size();
	int sum = 0;

	for(int i=0; i<ents-1; ++i)
		sum += mEntries[i].mincursize;

	return sum;
}

void VDUIGrid::Axis::Layout(int pos, int width, int pad) {
	const int n = mEntries.size() - 1;
	const int affsum = mEntries[n].affinitysum - mEntries[0].affinitysum;
	int affleft = affsum ? affsum : n;
	int slackleft = width - pad*(n-1) - GetMinimumSum();

	if (slackleft < 0)
		slackleft = 0;

	for(int i=0; i<n; ++i) {
		Entry& ent = mEntries[i];
		const int affinity = affsum ? ent.affinity : 1;
		const int minslice = affinity ? (slackleft * affinity + affleft - 1) / affleft : 0;

		ent.start = pos;
		pos += ent.mincursize + minslice;
		ent.end = pos;
		pos += pad;

		affleft -= affinity;
		slackleft -= minslice;
	}
}
