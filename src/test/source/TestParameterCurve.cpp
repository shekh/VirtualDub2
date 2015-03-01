#include <vd2/system/vdstl.h>
#include <vd2/VDLib/ParameterCurve.h>
#include "test.h"

DEFINE_TEST(ParameterCurve) {
	int e = 0;

	VDParameterCurve curve;

	VDParameterCurvePoint pt;
	pt.mX = 1.0f;
	pt.mY = 0.0f;
	pt.mbLinear = false;
	pt.mbLinkedTangents = false;

	VDParameterCurve::PointList& ptlist = curve.Points();
	ptlist.push_back(pt);

	TEST_ASSERT(curve.LowerBound(0.0f) == curve.Begin());
	TEST_ASSERT(curve.LowerBound(1.0f) == curve.Begin());
	TEST_ASSERT(curve.LowerBound(2.0f) == curve.End());

	TEST_ASSERT(curve.UpperBound(0.0f) == curve.Begin());
	TEST_ASSERT(curve.UpperBound(1.0f) == curve.End());
	TEST_ASSERT(curve.UpperBound(2.0f) == curve.End());

	pt.mX = 2.0f;
	ptlist.push_back(pt);

	TEST_ASSERT(curve.LowerBound(0.0f) == curve.Begin());
	TEST_ASSERT(curve.LowerBound(1.0f) == curve.Begin());
	TEST_ASSERT(curve.LowerBound(2.0f) == curve.Begin()+1);
	TEST_ASSERT(curve.LowerBound(3.0f) == curve.End());

	TEST_ASSERT(curve.UpperBound(0.0f) == curve.Begin());
	TEST_ASSERT(curve.UpperBound(1.0f) == curve.Begin()+1);
	TEST_ASSERT(curve.UpperBound(2.0f) == curve.End());
	TEST_ASSERT(curve.UpperBound(3.0f) == curve.End());

	pt.mX = 3.0f;
	ptlist.push_back(pt);

	TEST_ASSERT(curve.LowerBound(0.0f) == curve.Begin());
	TEST_ASSERT(curve.LowerBound(1.0f) == curve.Begin());
	TEST_ASSERT(curve.LowerBound(2.0f) == curve.Begin()+1);
	TEST_ASSERT(curve.LowerBound(3.0f) == curve.Begin()+2);
	TEST_ASSERT(curve.LowerBound(4.0f) == curve.End());

	TEST_ASSERT(curve.UpperBound(0.0f) == curve.Begin());
	TEST_ASSERT(curve.UpperBound(1.0f) == curve.Begin()+1);
	TEST_ASSERT(curve.UpperBound(2.0f) == curve.Begin()+2);
	TEST_ASSERT(curve.UpperBound(3.0f) == curve.End());
	TEST_ASSERT(curve.UpperBound(4.0f) == curve.End());

	return e;
}
