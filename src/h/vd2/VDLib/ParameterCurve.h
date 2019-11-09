#ifndef f_VD2_VDLIB_PARAMETERCURVE_H
#define f_VD2_VDLIB_PARAMETERCURVE_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/vectors.h>

struct VDParameterCurvePoint {
	double		mX;
	double		mY;
	bool		mbLinear;
	bool		mbLinkedTangents;
};

class VDParameterCurve {
public:
	typedef VDParameterCurvePoint Point;
	typedef vdfastvector<Point> PointList;

	VDParameterCurve();
	VDParameterCurve(const VDParameterCurve& a);
	~VDParameterCurve();

	int AddRef();
	int Release();

	float GetYMin() const { return mMinVal; }
	float GetYMax() const { return mMaxVal; }

	void SetYRange(float minVal, float maxVal) { mMinVal = minVal; mMaxVal = maxVal; }

	PointList& Points() { return mPoints; }
	const PointList& Points() const { return mPoints; }

	PointList::iterator Begin() { return mPoints.begin(); }
	PointList::const_iterator Begin() const { return mPoints.begin(); }

	PointList::iterator End() { return mPoints.end(); }
	PointList::const_iterator End() const { return mPoints.end(); }

	VDParameterCurvePoint operator()(double x) const;

	PointList::iterator			LowerBound(double x);
	PointList::const_iterator	LowerBound(double x) const;
	PointList::iterator			UpperBound(double x);
	PointList::const_iterator	UpperBound(double x) const;
	PointList::iterator			GetNearestPointX(double x);
	PointList::const_iterator	GetNearestPointX(double x) const;
	PointList::iterator			GetNearestPoint2D(double x, double y, double xRadius, double yScale);
	PointList::const_iterator	GetNearestPoint2D(double x, double y, double xRadius, double yScale) const;

protected:
	PointList mPoints;

	float	mMinVal;
	float	mMaxVal;

	int mRefCount;
};

#endif
