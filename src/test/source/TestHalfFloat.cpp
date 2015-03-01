#include <vd2/system/halffloat.h>
#include "test.h"

#define TEST_FLOAT_TO_HALF(floatpat, halfpat)	\
	f = (floatpat); h = VDConvertFloatToHalf((const float *)&f); TEST_ASSERT(h == (halfpat));	\
	f = (floatpat) | 0x80000000; h = VDConvertFloatToHalf((const float *)&f); TEST_ASSERT(h == ((halfpat) | 0x8000))

#define TEST_HALF_TO_FLOAT(halfpat, floatpat)	\
	VDConvertHalfToFloat((halfpat), (float *)&f); TEST_ASSERT(f == (floatpat));	\
	VDConvertHalfToFloat((halfpat) | 0x8000, (float *)&f); TEST_ASSERT(f == ((floatpat) | 0x80000000))

DEFINE_TEST(HalfFloat) {
	uint32 f;
	uint16 h;

	///////////////////////////////////////////////////////////////////////
	// float to half conversions
	//

	// 0.0
	TEST_FLOAT_TO_HALF(0x00000000, 0x0000);

	// smallest denormalized half
	TEST_FLOAT_TO_HALF(0x33800000, 0x0001);

	// largest denormalized half
	TEST_FLOAT_TO_HALF(0x387fa000, 0x03fe);
	TEST_FLOAT_TO_HALF(0x387fa001, 0x03ff);
	TEST_FLOAT_TO_HALF(0x387fc000, 0x03ff);
	TEST_FLOAT_TO_HALF(0x387fdfff, 0x03ff);
	TEST_FLOAT_TO_HALF(0x387fe000, 0x0400);

	// smallest normalized half
	TEST_FLOAT_TO_HALF(0x38800000, 0x0400);

	// 0.5
	TEST_FLOAT_TO_HALF(0x3f000000, 0x3800);

	// 1.0
	TEST_FLOAT_TO_HALF(0x3f800000, 0x3c00);		// exact, 1.0
	TEST_FLOAT_TO_HALF(0x3f800fff, 0x3c00);		// round down
	TEST_FLOAT_TO_HALF(0x3f801000, 0x3c00);		// round down to nearest even
	TEST_FLOAT_TO_HALF(0x3f801001, 0x3c01);		// round up
	TEST_FLOAT_TO_HALF(0x3f802000, 0x3c01);		// exact
	TEST_FLOAT_TO_HALF(0x3f802fff, 0x3c01);		// round down
	TEST_FLOAT_TO_HALF(0x3f803000, 0x3c02);		// round up to nearest even
	TEST_FLOAT_TO_HALF(0x3f803001, 0x3c02);		// round up
	TEST_FLOAT_TO_HALF(0x3f804000, 0x3c02);		// exact

	// 2.0
	TEST_FLOAT_TO_HALF(0x40000000, 0x4000);

	// pi
	TEST_FLOAT_TO_HALF(0x40490fdb, 0x4248);

	// infinity
	TEST_FLOAT_TO_HALF(0x7f800000, 0x7c00);

	///////////////////////////////////////////////////////////////////////
	// half to float conversions
	//

	// 0.0
	TEST_HALF_TO_FLOAT(0x0000, 0x00000000);

	// smallest denormalized half
	TEST_HALF_TO_FLOAT(0x0001, 0x33800000);

	// largest denormalized half
	TEST_HALF_TO_FLOAT(0x03ff, 0x387fc000);

	// smallest normalized half
	TEST_HALF_TO_FLOAT(0x0400, 0x38800000);

	// 0.5
	TEST_HALF_TO_FLOAT(0x3800, 0x3f000000);

	// 1.0
	TEST_HALF_TO_FLOAT(0x3c00, 0x3f800000);

	// 2.0
	TEST_HALF_TO_FLOAT(0x4000, 0x40000000);

	// pi
	TEST_HALF_TO_FLOAT(0x4248, 0x40490000);

	// largest normalized half (65504)
	TEST_HALF_TO_FLOAT(0x7bff, 0x477fe000);

	// infinity
	TEST_HALF_TO_FLOAT(0x7c00, 0x7f800000);

	return 0;
}
