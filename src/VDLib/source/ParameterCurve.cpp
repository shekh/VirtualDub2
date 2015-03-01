//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2006 Avery Lee
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
#include <vd2/VDLib/ParameterCurve.h>

VDParameterCurve::VDParameterCurve()
	: mMinVal(-1e+30f)
	, mMaxVal(1e+30f)
	, mRefCount(0)
{
}

VDParameterCurve::~VDParameterCurve() {
}

int VDParameterCurve::AddRef() {
	return ++mRefCount;
}

int VDParameterCurve::Release() {
	int rv = --mRefCount;
	if (!rv)
		delete this;
	return rv;
}

VDParameterCurvePoint VDParameterCurve::operator()(double x) const {
	VDParameterCurvePoint interpPt;
	interpPt.mX = x;
	interpPt.mY = 0;
	interpPt.mbLinear = false;

	// check for totally bogus empty case
	if (mPoints.empty())
		return interpPt;

	// find enclosing points
	PointList::const_iterator it2(UpperBound(x));

	// check if requested sample is prior to first point
	if (it2 == mPoints.begin()) {
		interpPt.mY = it2->mY;
		interpPt.mbLinear = it2->mbLinear;
		return interpPt;
	}

	// compute lower point
	PointList::const_iterator it1(it2);
	--it1;

	// check if requested sample is after last point
	const Point& p1 = *it1;
	if (it2 == mPoints.end()) {
		interpPt.mY = p1.mY;
		interpPt.mbLinear = p1.mbLinear;
		return interpPt;
	}

	// interpolate
	const Point& p2 = *it2;

	if (p1.mbLinear || fabs(p1.mX - p2.mX) < 0.01f) {
		double t = 0.0f;
		double xdel = p2.mX - p1.mX;
		double ydel = p2.mY - p1.mY;

		if (fabs(xdel) > 1e-5)
			t = (x - p1.mX) / xdel;

		interpPt.mY = p1.mY + ydel*t;
		interpPt.mbLinear = true;
	} else {
		// grab two outer points
		PointList::const_iterator it0(it1);
		if (it0 != mPoints.begin())
			--it0;

		PointList::const_iterator it3(it2);
		++it3;
		if (it3 == mPoints.end())
			--it3;

		const Point& p0 = *it0;
		const Point& p3 = *it3;

		double x0 = p0.mX;
		double x1 = p1.mX;
		double x2 = p2.mX;
		double x3 = p3.mX;

		double y0 = p0.mY;
		double y1 = p1.mY;
		double y2 = p2.mY;
		double y3 = p3.mY;

		if (fabs(x0 - x1) < 0.01f)
			x0 = x1 - 0.01f;

		if (fabs(x2 - x3) < 0.01f)
			x3 = x2 + 0.01f;

		// Rebias points by p0. Not only does this simplify the solve, but it also reduces numerical
		// accuracy issues.

		double dx2 = x2 - x1;
		double dy2 = y2 - y1;
		double m1 = (y2-y0)/(x2-x0);
		double m2 = (y3-y1)/(x3-x1);

		double YA = (m2 + m1 - dy2/dx2*2)/dx2/dx2;
		double YB = (3*dy2/dx2 - 2*m1 - m2)/dx2;
		double YC = m1;
		double YD = y1;

		double t = x - x1;

		interpPt.mY = ((YA*t+YB)*t+YC)*t+YD;
		if (interpPt.mY < mMinVal)
			interpPt.mY = mMinVal;
		if (interpPt.mY > mMaxVal)
			interpPt.mY = mMaxVal;
		interpPt.mbLinear = false;
	}

	return interpPt;
}

VDParameterCurve::PointList::iterator VDParameterCurve::GetNearestPointX(double x) {
	return mPoints.begin() + (static_cast<const VDParameterCurve *>(this)->GetNearestPointX(x) - mPoints.begin());
}

VDParameterCurve::PointList::const_iterator VDParameterCurve::GetNearestPointX(double x) const {
	PointList::const_iterator it1(LowerBound(x));

	if (it1 == mPoints.end())
		return it1;

	PointList::const_iterator it2(it1);
	++it2;

	if (it2 != mPoints.end()) {
		const Point& pt1 = *it1;
		const Point& pt2 = *it2;

		if ((pt2.mX - x) < (x - pt1.mX))
			it1 = it2;
	}

	return it1;
}

VDParameterCurve::PointList::iterator VDParameterCurve::GetNearestPoint2D(double x, double y, double xRadius, double yScale) {
	return mPoints.begin() + (static_cast<const VDParameterCurve *>(this)->GetNearestPoint2D(x, y, xRadius, yScale) - mPoints.begin());	
}

VDParameterCurve::PointList::const_iterator VDParameterCurve::GetNearestPoint2D(double x, double y, double xRadius, double yScale) const {
	PointList::const_iterator it1(LowerBound(x - xRadius));
	PointList::const_iterator it2(LowerBound(x + xRadius));

	if (it2 != mPoints.end())
		++it2;

	PointList::const_iterator itBest(mPoints.end());
	double bestDist2 = xRadius * xRadius;
	for(; it1!=it2; ++it1) {
		const Point& pt = *it1;

		double distX = (pt.mX - x);
		double distY = (pt.mY - y) * yScale;
		double dist2 = distX*distX + distY*distY;

		if (dist2 < bestDist2) {
			bestDist2 = dist2;
			itBest = it1;
		}
	}

	return itBest;
}

VDParameterCurve::PointList::iterator VDParameterCurve::LowerBound(double x) {
	return mPoints.begin() + (static_cast<const VDParameterCurve *>(this)->LowerBound(x) - mPoints.begin());
}

VDParameterCurve::PointList::const_iterator VDParameterCurve::LowerBound(double x) const {
	int lo = 0;
	if (!mPoints.empty()) {
		int hi = (uint32)mPoints.size() - 1;

		while(lo < hi) {
			uint32 mid = (lo + hi) >> 1;
			double xmid = mPoints[mid].mX;

			if (xmid < x)
				lo = mid+1;
			else if (xmid >= x)
				hi = mid;
		}

		if (mPoints[lo].mX < x)
			++lo;
	}

	return mPoints.begin() + lo;
}

VDParameterCurve::PointList::iterator VDParameterCurve::UpperBound(double x) {
	return mPoints.begin() + (static_cast<const VDParameterCurve *>(this)->UpperBound(x) - mPoints.begin());
}

VDParameterCurve::PointList::const_iterator VDParameterCurve::UpperBound(double x) const {
	int lo = 0;
	if (!mPoints.empty()) {
		int hi = (uint32)mPoints.size() - 1;

		while(lo < hi) {
			uint32 mid = (lo + hi) >> 1;
			double xmid = mPoints[mid].mX;

			if (xmid <= x)
				lo = mid+1;
			else if (xmid > x)
				hi = mid;
		}

		if (mPoints[lo].mX <= x)
			++lo;
	}

	return mPoints.begin() + lo;
}
